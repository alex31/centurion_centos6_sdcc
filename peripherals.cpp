// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "peripherals.hpp"

#include <cstdio>

namespace centurion
{

std::uint8_t DiagnosticCard::read(unsigned address)
{
    if (address == 16)
        return switches_;
    if (address == 6 || address == 7) {
        blank_ = address == 7;
    } else if (address >= 8 && address < 16) {
        const auto point = 1u << ((address - 8) / 2);
        if (address & 1)
            points_ &= ~point;
        else
            points_ |= point;
    }
    return 0;
}

void DiagnosticCard::write(unsigned address, std::uint8_t value)
{
    if (address == 16)
        display_ = value;
    read(address);
}

std::uint8_t Printer::read(unsigned address) { return address == 1 ? 31 : 0; }

void Printer::write(unsigned address, std::uint8_t value)
{
    if (address != 0)
        return;
    const auto character = value & 127;
    if (character == 12)
        text_ += "\xe2\x87\x8a\n";
    else
        text_ += static_cast<char>(character);
}

void TapeRegisters::reset() { control_ = data_ = 0; }

std::uint8_t TapeRegisters::read(unsigned address)
{
    switch (address) {
    case 0: return data_;
    case 1: return 0x2d;
    case 2: return 0xb3;
    default:
        if (trace_)
            std::fprintf(stderr, "Tape R %02X\n", address);
        return 0;
    }
}

void TapeRegisters::write(unsigned address, std::uint8_t value)
{
    if (address == 0)
        data_ = value;
    else if (address == 1)
        control_ = value;
    if (trace_)
        std::fprintf(stderr, "Tape W %02X %02X (data %02X control %02X)\n", address,
                     static_cast<unsigned>(value), static_cast<unsigned>(data_),
                     static_cast<unsigned>(control_));
}

std::uint8_t IoTrace::read(unsigned address)
{
    if (trace_)
        std::fprintf(stderr, "IOTrace-R %02X\n", address);
    return 0;
}

void IoTrace::write(unsigned address, std::uint8_t value)
{
    if (trace_)
        std::fprintf(stderr, "IOTrace-W %02X %02X\n", address, static_cast<unsigned>(value));
}

}
