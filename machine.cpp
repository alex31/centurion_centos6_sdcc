// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "machine.hpp"

#include <algorithm>

namespace centurion
{

Machine::Machine(const Firmware& firmware)
    : bus(firmware)
    , cpu(bus, firmware)
    , hawk(bus, cpu)
    , finch(bus, cpu, firmware)
    , front_panel_(cpu)
    , cmd_(bus, cpu)
{
    bus.attach(mux, 0x3f200, 0x100, Backplane::Irq::Wired);
    bus.attach(front_panel_, 0x3f000, 60);
    bus.attach(printer, 0x3f0e0, 16);
    bus.attach(diagnostic, 0x3f100, 32);
    bus.attach(tape, 0x3f700, 16);
    bus.attach(io_trace, 0x3f710, 240);
}

void Machine::reset()
{
    cpu.reset();
    cpu.guard_early_boot(true);
    cpu.set_sense_switches(10);
}

void Machine::step()
{
    hawk.tick(1);
    cpu.run(1);
    cmd_.finish_slice();
}

void Machine::run_slice(unsigned cycles, const std::function<void(unsigned)>& hardware_tick)
{
    mux.advance_transmit();
    const auto batch = cycles <= 10'000 ? 1 : hardware_batch;
    while (cycles) {
        const auto count = std::min(cycles, batch);
        hawk.tick(count);
        if (hardware_tick)
            hardware_tick(count);
        cpu.run(count);
        cycles -= count;
    }
    finch.sample_signals();
    cmd_.finish_slice();
}

std::uint8_t Machine::FrontPanel::read(unsigned address)
{
    if (address == 0)
        return 0x80 | cpu_.sense_switches();
    return address < 32 ? 0x80 : 0;
}

}
