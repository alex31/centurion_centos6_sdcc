// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once

#include "backplane.hpp"

#include <string>

namespace centurion
{

class DiagnosticCard final : public Device
{
public:
    std::uint8_t read(unsigned address) override;
    void write(unsigned address, std::uint8_t value) override;

    void set_switches(std::uint8_t value) noexcept { switches_ = value & 15; }
    [[nodiscard]] std::uint8_t switches() const noexcept { return switches_; }
    [[nodiscard]] std::uint8_t display() const noexcept { return display_; }
    [[nodiscard]] std::uint8_t points() const noexcept { return points_; }
    [[nodiscard]] bool blank() const noexcept { return blank_; }

private:
    std::uint8_t switches_ { 0x0d }, display_ {}, points_ {};
    bool blank_ {};
};

class Printer final : public Device
{
public:
    std::uint8_t read(unsigned address) override;
    void write(unsigned address, std::uint8_t value) override;
    [[nodiscard]] const std::string& transcript() const noexcept { return text_; }

private:
    std::string text_;
};

class TapeRegisters final : public Device
{
public:
    std::uint8_t read(unsigned address) override;
    void write(unsigned address, std::uint8_t value) override;
    void reset() override;

    void trace_io(bool on) noexcept { trace_ = on; }

private:
    std::uint8_t data_ {}, control_ {};
    bool trace_ {};
};

class IoTrace final : public Device
{
public:
    std::uint8_t read(unsigned address) override;
    void write(unsigned address, std::uint8_t value) override;
    void trace_io(bool on) noexcept { trace_ = on; }

private:
    bool trace_ {};
};

}
