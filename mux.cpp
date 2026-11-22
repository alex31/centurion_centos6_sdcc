// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "mux.hpp"

#include <algorithm>
#include <bit>
#include <utility>

namespace centurion
{
namespace
{

constexpr std::array baud_rates { 0, 75, 300, 1200, 2400, 4800, 9600, 19200 };
constexpr std::array masks { 0x1f, 0x3f, 0x7f, 0xff };

}

void Mux::reset()
{
    enabled_ = pending_ = cause_ = false;
    level_ = 0;
    drive_irq(0);
    for (auto& port : ports_) {
        port.write_busy = port.read_busy = port.overrun = false;
        port.rts = true;
        port.baud = 0;
        port.character_width = 0; // Reset configures a five-bit UART.
        port.frame_cost = 400;
    }
}

void Mux::acknowledge()
{
    pending_ = false;
    drive_irq(0);
}

void Mux::interrupt() noexcept
{
    pending_ = cause_ = true;
    drive_irq(enabled_ ? level_ : 0);
}

void Mux::bind(unsigned port, Transmit transmit, bool cts)
{
    ports_.at(port).transmit = std::move(transmit);
    set_cts(port, cts);
}

void Mux::set_cts(unsigned port, bool asserted) { ports_.at(port).cts = asserted; }

void Mux::observe_writes(unsigned port, Transmit observer) { ports_.at(port).observer = std::move(observer); }

void Mux::receive(unsigned index, std::uint8_t byte)
{
    auto& port = ports_.at(index);
    port.overrun |= port.read_busy;
    port.read_buffer = byte & masks[port.character_width];
    port.read_busy = true;
    interrupt();
}

void Mux::check_transmit(unsigned index)
{
    auto& port = ports_[index];
    if (!port.write_busy || port.transmit_credit <= 0)
        return;
    port.transmit_credit -= port.frame_cost;
    port.write_busy = false;
    if (port.transmit)
        port.transmit(port.write_buffer & masks[port.character_width]);
    transmit_interrupts_ |= 1u << index;
    interrupt();
}

void Mux::advance_transmit()
{
    for (unsigned index = 0; index < ports_.size(); ++index) {
        auto& port = ports_[index];
        port.transmit_credit = std::min(port.transmit_credit + port.baud, port.baud);
        check_transmit(index);
    }
}

std::uint8_t Mux::read(unsigned address)
{
    if (address < 8) {
        auto& port = ports_[address >> 1];
        if (transmit_interrupts_)
            interrupt();
        if (!(address & 1)) {
            return (port.write_busy ? 0 : 2) | (port.read_busy ? 1 : 0) | (port.overrun ? 16 : 0)
                | (port.cts ? 32 : 0);
        }
        port.read_busy = port.overrun = false;
        return std::exchange(port.read_buffer, 0);
    }
    if (address != 15 || !std::exchange(cause_, false))
        return 0;
    for (unsigned index = 0; index < ports_.size(); ++index) {
        if (ports_[index].read_busy)
            return index << 1;
    }
    if (!transmit_interrupts_)
        return 0;
    const auto index = std::countr_zero(transmit_interrupts_);
    transmit_interrupts_ &= transmit_interrupts_ - 1;
    return (index << 1) | 1;
}

void Mux::write(unsigned address, std::uint8_t value)
{
    if (address < 8) {
        const auto index = address >> 1;
        auto& port = ports_[index];
        if (address & 1) {
            if (port.observer)
                port.observer(value);
            port.write_busy = true;
            port.write_buffer = value;
            check_transmit(index);
        } else {
            port.baud = baud_rates[value >> 5];
            port.character_width = (value >> 1) & 3;
            const auto stop = (value >> 3) & 1;
            const auto parity_bit = (value & 16) == 0;
            port.frame_cost = (7 + stop + port.character_width + parity_bit) * 50;
        }
        return;
    }

    switch (address) {
    case 8: ports_[(value >> 1) & 3].rts = !(value & 1); break;
    case 10: level_ = value & 15; break;
    case 12:
        transmit_interrupts_ |= value & 15;
        interrupt();
        break;
    case 13: enabled_ = false; break;
    case 14: enabled_ = true; break;
    case 15: reset(); break;
    }
    drive_irq(enabled_ && pending_ ? level_ : 0);
}

}
