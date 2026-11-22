// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "runtime.hpp"

#include <algorithm>
#include <stdexcept>

namespace centurion
{

Runtime::Runtime(boost::asio::io_context& io, const Firmware& firmware)
    : machine(firmware)
    , terminals { Terminal(machine.mux, 0), Terminal(machine.mux, 1) }
    , cpu_timer_(io)
    , terminal_timer_(io)
    , pacer_(Clock::now())
    , sample_time_(Clock::now())
{
    machine.mux.observe_writes(0, [this](std::uint8_t byte) { terminals[0].receive(byte); });
    machine.mux.observe_writes(1, [this](std::uint8_t byte) {
        if (raw_output_)
            raw_output_(byte & 127);
        else
            terminals[1].receive(byte);
    });
    reset();
    schedule_terminals();
}

void Runtime::reset_timing()
{
    sample_time_ = Clock::now();
    sample_cycles_ = machine.cpu.microsteps();
    speed_factor_.reset();
    pacer_ = Pacer(sample_time_);
}

void Runtime::start()
{
    if (running_)
        return;
    running_ = true;
    reset_timing();
    schedule_cpu();
}

void Runtime::stop()
{
    running_ = false;
    ++scheduled_cpu_;
    cpu_timer_.cancel();
    reset_timing();
}

void Runtime::reset()
{
    machine.reset();
    for (auto& terminal : terminals)
        terminal.reset();
    raw_input_.clear();
    serial_wait_ = 0;
    reset_timing();
    if (running_)
        schedule_cpu();
}

void Runtime::step()
{
    tick_raw_serial(1);
    machine.step();
}

void Runtime::preview_step()
{
    // The historical half-step request samples the next microinstruction before the CPU clock edge.
    machine.hawk.tick(1);
    tick_raw_serial(1);
}

void Runtime::set_speed(Speed speed)
{
    if (speed == Speed::Custom)
        throw std::invalid_argument("Custom speed requires a cycle rate");
    if (speed == speed_)
        return;
    speed_ = speed;
    rate_ = 100'000;
    reset_timing();
    if (running_)
        schedule_cpu();
}

void Runtime::set_rate(unsigned cycles)
{
    if (cycles == 0 || cycles > 150'000)
        throw std::invalid_argument("Unsupported cycle rate");
    rate_ = cycles;
    speed_ = Speed::Custom;
    reset_timing();
    if (running_)
        schedule_cpu();
}

void Runtime::schedule_cpu()
{
    if (speed_ == Speed::Realtime)
        cpu_timer_.expires_at(pacer_.deadline());
    else if (speed_ == Speed::Custom)
        cpu_timer_.expires_after(Pacer::period);
    else
        cpu_timer_.expires_after(Clock::duration::zero());
    cpu_timer_.async_wait([this, schedule = ++scheduled_cpu_](const boost::system::error_code& error) {
        if (!error && running_ && schedule == scheduled_cpu_) {
            run_cpu();
            schedule_cpu();
        }
    });
}

void Runtime::run_cpu()
{
    const auto started = Clock::now();
    if (speed_ == Speed::Maximum && started - sample_time_ >= std::chrono::seconds(1)) {
        const auto elapsed = std::chrono::duration<double>(started - sample_time_).count();
        const auto cycles = machine.cpu.microsteps();
        speed_factor_ = (cycles - sample_cycles_) / (elapsed * Machine::nominal_clock);
        sample_time_ = started;
        sample_cycles_ = cycles;
    }
    const auto cycles = speed_ == Speed::Realtime ? pacer_.cycles_due(started) : rate_;
    if (raw_output_) {
        machine.run_slice(cycles, [this](unsigned count) { tick_raw_serial(count); });
    } else {
        machine.run_slice(cycles);
    }
}

void Runtime::schedule_terminals()
{
    terminal_timer_.expires_after(Pacer::period);
    terminal_timer_.async_wait([this](const boost::system::error_code& error) {
        if (error)
            return;
        for (auto& terminal : terminals)
            terminal.tick();
        schedule_terminals();
    });
}

void Runtime::claim_raw_serial(Mux::Transmit output)
{
    raw_input_.clear();
    serial_wait_ = 0;
    terminals[1].set_available(false);
    raw_output_ = std::move(output);
}

void Runtime::release_raw_serial()
{
    raw_output_ = {};
    raw_input_.clear();
    serial_wait_ = 0;
    terminals[1].set_available(true);
}

bool Runtime::queue_raw_serial(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() > 8192 - raw_input_.size())
        return false;
    raw_input_.insert(raw_input_.end(), bytes.begin(), bytes.end());
    return true;
}

void Runtime::tick_raw_serial(unsigned cycles)
{
    serial_wait_ -= std::min(serial_wait_, cycles);
    const auto baud = machine.mux.baud(1);
    if (raw_input_.empty() || serial_wait_ || !machine.mux.can_receive(1) || baud <= 0)
        return;
    machine.mux.receive(1, raw_input_.front() & 127);
    raw_input_.pop_front();
    const auto frame = Machine::nominal_clock * machine.mux.frame_bits(1);
    serial_wait_ = (frame + baud - 1) / baud;
}

}
