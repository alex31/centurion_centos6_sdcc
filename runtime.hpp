// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once

#include "pacer.hpp"
#include "terminal.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <chrono>
#include <deque>
#include <functional>
#include <optional>
#include <span>

namespace centurion
{

// The emulation and all clients are driven by the same event loop.
class Runtime
{
public:
    enum class Speed { Maximum, Realtime, Custom };
    using Clock = Pacer::Clock;

    Runtime(boost::asio::io_context& io, const Firmware& firmware);
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    void start();
    void stop();
    void step();
    void preview_step();
    void reset();
    void set_speed(Speed speed);
    void set_rate(unsigned cycles);
    [[nodiscard]] bool running() const noexcept { return running_; }
    [[nodiscard]] Speed speed() const noexcept { return speed_; }
    [[nodiscard]] unsigned rate() const noexcept { return rate_; }
    [[nodiscard]] std::optional<double> speed_factor() const noexcept { return speed_factor_; }

    void claim_raw_serial(Mux::Transmit output);
    void release_raw_serial();
    bool queue_raw_serial(std::span<const std::uint8_t> bytes);
    [[nodiscard]] bool raw_serial_in_use() const noexcept { return !!raw_output_; }

    Machine machine;
    std::array<Terminal, 2> terminals;

private:
    void schedule_cpu();
    void run_cpu();
    void schedule_terminals();
    void tick_raw_serial(unsigned cycles);
    void reset_timing();

    boost::asio::steady_timer cpu_timer_, terminal_timer_;
    Pacer pacer_;
    Clock::time_point sample_time_;
    std::uint64_t sample_cycles_ {};
    std::uint64_t scheduled_cpu_ {};
    std::optional<double> speed_factor_;
    std::deque<std::uint8_t> raw_input_;
    Mux::Transmit raw_output_;
    unsigned serial_wait_ {};
    unsigned rate_ { 100'000 };
    Speed speed_ { Speed::Maximum };
    bool running_ {};
};

}
