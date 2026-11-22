// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once

#include "cmd.hpp"
#include "finch.hpp"
#include "hawk.hpp"
#include "mux.hpp"
#include "peripherals.hpp"

#include <functional>

namespace centurion
{

class Machine
{
public:
    static constexpr unsigned hardware_batch = 100;
    static constexpr unsigned nominal_clock = 5'000'000;

    explicit Machine(const Firmware& firmware);
    Machine(const Machine&) = delete;
    Machine& operator=(const Machine&) = delete;

    void reset();
    void step();
    void run_slice(unsigned cycles, const std::function<void(unsigned)>& hardware_tick = {});

    Backplane bus;
    Cpu6 cpu;
    Mux mux;
    Hawk hawk;
    Finch finch;
    DiagnosticCard diagnostic;
    Printer printer;
    TapeRegisters tape;
    IoTrace io_trace;

private:
    class FrontPanel final : public Device
    {
    public:
        explicit FrontPanel(const Cpu6& cpu)
            : cpu_(cpu)
        {
        }
        std::uint8_t read(unsigned address) override;
        void write(unsigned, std::uint8_t) override { }

    private:
        const Cpu6& cpu_;
    };

    FrontPanel front_panel_;
    Cmd cmd_;
};

}
