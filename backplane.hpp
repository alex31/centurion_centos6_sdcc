// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once

#include "firmware.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace centurion
{

class Backplane;

class Device
{
public:
    virtual ~Device() = default;
    virtual std::uint8_t read(unsigned address) = 0;
    virtual void write(unsigned address, std::uint8_t value) = 0;
    virtual void reset() { }
    virtual unsigned interrupt_level() const { return irq_level_; }
    virtual bool signals_irq_changes() const noexcept { return false; }
    virtual void acknowledge() { }
    virtual void tick(unsigned) { }

protected:
    void drive_irq(unsigned level) noexcept;

private:
    friend class Backplane;
    Backplane* irq_bus_ {};
    unsigned irq_level_ {};
};

class Backplane
{
public:
    enum class Irq { Unwired, Wired };

    explicit Backplane(const Firmware& firmware);

    void attach(Device& device, unsigned base, unsigned size, Irq irq = Irq::Unwired);
    std::size_t clock_every_microstep(Device& device, bool active = true);
    void enable_fast_clock(std::size_t slot, bool active) noexcept;
    void insert_diagnostic_roms(bool present) noexcept { diagnostic_roms_ = present; }
    [[nodiscard]] bool diagnostic_roms_installed() const noexcept { return diagnostic_roms_; }
    void load_ram(unsigned base, std::span<const std::uint8_t> bytes);
    [[nodiscard]] std::span<const std::uint8_t> ram() const noexcept { return ram_; }

    [[nodiscard]] bool interrupt_pending(unsigned level) const
    {
        return max_irq_ > level || (has_polled_irq_ && poll_interrupts(level));
    }
    unsigned acknowledge_interrupt(unsigned level);
    void reset();
    bool has_fast_clocks() const noexcept { return clock_end_ != 0; }
    void tick_fast();

    std::uint8_t read(unsigned address, bool& parity_fault)
    {
        if (address < ram_.size() && (!diagnostic_roms_ || address < 0x8000 || address >= 0xc000)) {
            const auto value = ram_[address];
            parity_fault = parity_[address] != odd_parity(value);
            return value;
        }
        return read_mapped(address);
    }

    void write(unsigned address, std::uint8_t value, bool invert_parity = false)
    {
        if (address < ram_.size() && (!diagnostic_roms_ || address < 0x8000 || address >= 0xc000)) {
            ram_[address] = value;
            parity_[address] = odd_parity(value) ^ invert_parity;
            return;
        }
        write_mapped(address, value);
    }

private:
    struct Mapping {
        Device* device {};
        unsigned base {};
    };

    friend class Device;
    static std::uint8_t odd_parity(std::uint8_t value) noexcept { return __builtin_parity(value); }
    void update_interrupts() noexcept;
    bool poll_interrupts(unsigned level) const;
    std::uint8_t read_mapped(unsigned address);
    void write_mapped(unsigned address, std::uint8_t value);

    static constexpr unsigned io_base = 0x3f000;
    std::size_t clock_end_ {};
    unsigned max_irq_ {};
    bool has_polled_irq_ {};
    bool diagnostic_roms_ {};
    std::array<std::uint8_t, io_base> ram_ {};
    std::array<std::uint8_t, io_base> parity_ {};
    std::array<std::uint8_t, 0x200> boot_rom_ {};
    std::array<std::uint8_t, 0x2000> diagnostic_rom_ {};
    std::array<std::uint8_t, 0x800> diagnostic_ram_ {};
    std::array<Mapping, 0x1000> mappings_ {};
    std::vector<Device*> devices_;
    std::vector<Device*> interrupt_devices_;
    struct FastClock {
        Device* device;
        bool enabled;
    };
    std::vector<FastClock> fast_devices_;
};

}
