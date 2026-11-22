// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "cmd.hpp"

namespace centurion
{
namespace
{
enum StatusCode : std::uint8_t {
    Success = 0,
    InvalidCommand = 0x12,
    DmaLag = 0x23,
    InvalidAddress = 0x26,
    InvalidLength = 0x29,
};
}

Cmd::Cmd(Backplane& bus, Cpu6& cpu)
    : cpu_(cpu)
    , bus_(bus)
    , clock_origin_(cpu.microsteps())
{
    bus.attach(*this, 0x3f808, 2);
    cpu.attach_dma(*this);
    clock_slot_ = bus.clock_every_microstep(*this, false);
}

void Cmd::reset()
{
    phase_ = Phase::Idle;
    input_pending_ = output_pending_ = dma_requested = false;
    input_ = output_ = 0;
    bus_.enable_fast_clock(clock_slot_, false);
}

std::uint8_t Cmd::read(unsigned address)
{
    if (address == 1)
        return (phase_ != Phase::Idle ? 8 : 0) | (input_pending_ ? 2 : 0) | (output_pending_ ? 1 : 0);
    output_pending_ = false;
    return output_;
}

void Cmd::write(unsigned address, std::uint8_t value)
{
    if (address == 0) {
        input_ = value;
        input_pending_ = true;
        consume_input();
        bus_.enable_fast_clock(clock_slot_, input_pending_ || phase_ == Phase::Delay);
        return;
    }
    if (phase_ == Phase::Transfer) {
        output_ = DmaLag;
        output_pending_ = true;
        dma_requested = false;
        phase_ = Phase::Cancel;
    } else if (phase_ == Phase::Argument || phase_ == Phase::ArgumentReceived) {
        phase_ = Phase::Cancel;
    }
}

void Cmd::consume_input()
{
    if (phase_ == Phase::Idle) {
        input_pending_ = false;
        start(input_);
    } else if (phase_ == Phase::Argument) {
        input_pending_ = false;
        arguments_[argument_count_++] = input_;
        phase_ = Phase::ArgumentReceived;
    }
}

void Cmd::start(std::uint8_t command)
{
    switch (command) {
    case 0:
        phase_ = Phase::Delay;
        // The free-running divider wraps on tick 41 (0, 5, ..., 200, 0).
        // Preserve its phase through idle periods and warm resets.
        delay_ticks_ = 41 - (cpu_.microsteps() - clock_origin_) % 41;
        break;
    case 0x43:
        transfer_address_ = 0x11;
        remaining_ = 178;
        direction_ = Direction::ToCard;
        start_transfer();
        break;
    case 0x46:
    case 0x47:
        argument_count_ = 0;
        direction_ = command == 0x46 ? Direction::ToCard : Direction::ToCpu;
        phase_ = Phase::Argument;
        break;
    default: finish(InvalidCommand); break;
    }
}

void Cmd::tick(unsigned)
{
    if (phase_ == Phase::Delay && --delay_ticks_ == 0)
        finish(Success);
    if (input_pending_)
        consume_input();
    if (!input_pending_ && phase_ != Phase::Delay)
        bus_.enable_fast_clock(clock_slot_, false);
}

void Cmd::finish_slice()
{
    // Promise continuations in CenRE run between host CPU slices.
    if (phase_ == Phase::ArgumentReceived) {
        if (argument_count_ < arguments_.size()) {
            phase_ = Phase::Argument;
        } else {
            transfer_address_ = (arguments_[0] << 8) | arguments_[1];
            remaining_ = (arguments_[2] << 8) | arguments_[3];
            if (transfer_address_ >= ram_.size())
                finish(InvalidAddress);
            else if (remaining_ >= ram_.size())
                finish(InvalidLength);
            else
                start_transfer();
        }
    }
    if (phase_ == Phase::Finish) {
        output_ = completion_;
        output_pending_ = true;
        phase_ = Phase::Idle;
    } else if (phase_ == Phase::Cancel) {
        phase_ = Phase::Idle;
    }
}

void Cmd::start_transfer()
{
    phase_ = Phase::Transfer;
    cpu_.request_dma(*this);
}

void Cmd::step_dma(Cpu6& cpu)
{
    if (remaining_ == 0 || cpu.dma_finished()) {
        cpu.end_dma();
        finish(Success);
        return;
    }
    auto& byte = ram_[transfer_address_];
    if (direction_ == Direction::ToCard)
        byte = cpu.dma_read();
    else
        cpu.dma_write(byte);
    transfer_address_ = (transfer_address_ + 1) & 0xfff;
    --remaining_;
}

void Cmd::finish(std::uint8_t result)
{
    completion_ = result;
    phase_ = Phase::Finish;
}

}
