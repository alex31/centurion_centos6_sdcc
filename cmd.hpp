// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once

#include "cpu6.hpp"

namespace centurion
{

// The experimental CMD card implements the command buffer and DMA transfers.
class Cmd final : public Device, public DmaDevice
{
public:
    Cmd(Backplane& bus, Cpu6& cpu);
    std::uint8_t read(unsigned address) override;
    void write(unsigned address, std::uint8_t value) override;
    void reset() override;
    void tick(unsigned increment) override;
    void select_dma(bool, unsigned) override { }
    void step_dma(Cpu6& cpu) override;
    void finish_slice();

private:
    enum class Phase { Idle, Delay, Argument, ArgumentReceived, Transfer, Finish, Cancel };
    enum class Direction { ToCard, ToCpu };

    void consume_input();
    void start(std::uint8_t command);
    void start_transfer();
    void finish(std::uint8_t result);

    Cpu6& cpu_;
    Backplane& bus_;
    std::size_t clock_slot_ {};
    std::uint64_t clock_origin_ {};
    std::array<std::uint8_t, 4096> ram_ {};
    std::array<std::uint8_t, 4> arguments_ {};
    unsigned argument_count_ {}, delay_ticks_ {};
    std::uint16_t transfer_address_ {}, remaining_ {};
    std::uint8_t input_ {}, output_ {}, completion_ {};
    bool input_pending_ {}, output_pending_ {};
    Phase phase_ { Phase::Idle };
    Direction direction_ { Direction::ToCard };
};

}
