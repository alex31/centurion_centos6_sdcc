// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "backplane.hpp"

#include <algorithm>
#include <stdexcept>

namespace centurion
{

Backplane::Backplane(const Firmware& firmware)
{
    const auto boot = firmware.table("bpl_romc");
    if (boot.size() != boot_rom_.size())
        throw std::runtime_error("The bootstrap ROM must contain 512 bytes");
    // Socket pin order maps A5 through A8 differently from the backplane.
    constexpr std::array pin_order { 0, 1, 2, 3, 4, 8, 5, 6, 7 };
    for (unsigned address = 0; address < boot_rom_.size(); ++address) {
        const auto inverted = address ^ 0x1ff;
        unsigned index = 0;
        for (unsigned bit = 0; bit < pin_order.size(); ++bit)
            index |= ((inverted >> pin_order[bit]) & 1) << bit;
        boot_rom_[address] = boot[index];
    }

    constexpr std::array names { "f1", "f2", "f3", "f4" };
    for (unsigned i = 0; i < names.size(); ++i) {
        const auto rom = firmware.table(names[i]);
        if (rom.size() != 0x800)
            throw std::runtime_error("Each diagnostic ROM must contain 2048 bytes");
        std::ranges::copy(rom, diagnostic_rom_.begin() + i * 0x800);
    }

    // CenRE's built-in register rotation demo is initially present in RAM.
    constexpr auto rotation_demo = std::to_array<std::uint8_t>({
        0x60, 0xaa, 0xaa, 0x60, 0xaa, 0xaa, 0x55, 0x40, 0x36, 0,    0x36, 0,    0x36, 0,    0x36,
        0,    0x36, 0,    0x36, 0,    0x36, 0,    0x36, 0,    0x36, 0,    0x36, 0,    0x36, 0,
        0x36, 0,    0x36, 0,    0x36, 0,    0x36, 0,    0x36, 0,    0x71, 0x01, 0x03,
    });
    load_ram(0x100, rotation_demo);
}

void Backplane::attach(Device& device, unsigned base, unsigned size, Irq irq)
{
    if (base < io_base || base >= 0x40000 || size == 0 || size > 0x40000 - base)
        throw std::out_of_range("Invalid device address range");
    auto slots = std::span(mappings_).subspan(base - io_base, size);
    if (std::ranges::any_of(slots, [](auto slot) { return slot.device != nullptr; }))
        throw std::logic_error("Overlapping backplane devices");
    if (std::ranges::find(devices_, &device) == devices_.end())
        devices_.push_back(&device);
    if (irq == Irq::Wired && std::ranges::find(interrupt_devices_, &device) == interrupt_devices_.end()) {
        interrupt_devices_.push_back(&device);
        if (device.signals_irq_changes())
            device.irq_bus_ = this;
        else
            has_polled_irq_ = true;
        update_interrupts();
    }
    std::ranges::fill(slots, Mapping { &device, base });
}

std::size_t Backplane::clock_every_microstep(Device& device, bool active)
{
    const auto slot = fast_devices_.size();
    fast_devices_.push_back({ &device, active });
    enable_fast_clock(slot, active);
    return slot;
}

void Backplane::enable_fast_clock(std::size_t slot, bool active) noexcept
{
    fast_devices_[slot].enabled = active;
    if (active)
        clock_end_ = std::max(clock_end_, slot + 1);
    else
        while (clock_end_ && !fast_devices_[clock_end_ - 1].enabled)
            --clock_end_;
}

void Backplane::load_ram(unsigned base, std::span<const std::uint8_t> bytes)
{
    if (base > ram_.size() || bytes.size() > ram_.size() - base)
        throw std::out_of_range("Program does not fit in RAM");
    std::ranges::copy(bytes, ram_.begin() + base);
    std::ranges::transform(bytes, parity_.begin() + base, odd_parity);
}

void Device::drive_irq(unsigned level) noexcept
{
    if (irq_level_ == level)
        return;
    irq_level_ = level;
    if (irq_bus_)
        irq_bus_->update_interrupts();
}

void Backplane::update_interrupts() noexcept
{
    max_irq_ = 0;
    for (const auto* device : interrupt_devices_)
        if (device->irq_bus_ == this)
            max_irq_ = std::max(max_irq_, device->irq_level_);
}

bool Backplane::poll_interrupts(unsigned level) const
{
    return std::ranges::any_of(interrupt_devices_,
                               [level](const auto* device) { return device->interrupt_level() > level; });
}

unsigned Backplane::acknowledge_interrupt(unsigned level)
{
    for (auto* device : interrupt_devices_) {
        const auto requested = device->interrupt_level();
        if (requested > level) {
            device->acknowledge();
            return requested;
        }
    }
    return 0;
}

void Backplane::reset()
{
    for (auto* device : devices_)
        device->reset();
}

void Backplane::tick_fast()
{
    for (std::size_t i = 0; i < clock_end_; ++i)
        if (fast_devices_[i].enabled)
            fast_devices_[i].device->tick(5);
}

std::uint8_t Backplane::read_mapped(unsigned address)
{
    if (address >= io_base && address < 0x40000) {
        const auto [device, base] = mappings_[address - io_base];
        if (device)
            return device->read(address - base);
        if (address >= 0x3fc00 && address < 0x3fe00)
            return boot_rom_[address - 0x3fc00];
    }
    if (diagnostic_roms_ && address >= 0x8000 && address < 0xa000)
        return diagnostic_rom_[address - 0x8000];
    if (diagnostic_roms_ && address >= 0xb800 && address < 0xc000)
        return diagnostic_ram_[address - 0xb800];
    return 0;
}

void Backplane::write_mapped(unsigned address, std::uint8_t value)
{
    if (address >= io_base && address < 0x40000) {
        const auto [device, base] = mappings_[address - io_base];
        if (device)
            device->write(address - base, value);
    } else if (diagnostic_roms_ && address >= 0xb800 && address < 0xc000) {
        diagnostic_ram_[address - 0xb800] = value;
    }
}

}
