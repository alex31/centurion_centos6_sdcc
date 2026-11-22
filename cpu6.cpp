// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "cpu6.hpp"
#include "cpu6-compiled.hpp"
#include "cpu6-clock.inl"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace centurion
{

Cpu6::Cpu6(Backplane& bus, const Firmware& firmware)
    : bus_(bus)
{
    load_microcode(firmware);
    compiled_ = CompiledCore::matches(microcode_words_);
    const auto mapping = firmware.table("map");
    if (mapping.size() != map_rom_.size())
        throw std::runtime_error("The CPU6 mapping ROM must contain 256 bytes");
    std::ranges::copy(mapping, map_rom_.begin());
}

void Cpu6::load_microcode(const Firmware& firmware)
{
    constexpr std::array names { "e", "j", "m", "l", "h", "f", "k" };
    std::array<std::span<const std::uint8_t>, 7> roms;
    for (unsigned i = 0; i < roms.size(); ++i) {
        roms[i] = firmware.table(names[i]);
        if (roms[i].size() != microcode_.size())
            throw std::runtime_error("Each CPU6 microcode ROM must contain 2048 bytes");
    }
    for (unsigned address = 0; address < microcode_.size(); ++address) {
        auto& word = microcode_words_[address];
        for (unsigned chip = 0; chip < word.size(); ++chip)
            word[chip] = roms[chip][address];
        microcode_[address] = decode_microinstruction(word);
    }
}

void Cpu6::reset()
{
    sequence_ = {};
    micro_pc_ = 0;
    result = 0;
    swap = 0;
    interrupt_level_ = 0;
    sysctl = 0;
    busctl = 0;
    resetting = true;
    sys_write_latch = false;
    sys_read_in = false;
    parity_ = false;
    dma_12 = 0;
    timer_period_ = timer_countdown_ = 83333;
    bus_.reset();
    select_dma(false, 0);
    while (dma_current)
        end_dma();
}

void Cpu6::set_pc(std::uint16_t address)
{
    if (!at_instruction_boundary())
        throw std::logic_error("The CPU6 is between instruction boundaries");
    memaddr = address;
    map_memory_address();
}

unsigned Cpu6::condition_codes() const noexcept
{
    return (ccr_v << 7) | (ccr_m << 6) | (ccr_f << 5) | (ccr_l << 4);
}

Cpu6::MicroState Cpu6::micro_state() const noexcept
{
    return {
        workaddr,       result,         rir,          rdr,
        swap,           alu_flag,       busctl,       sysctl,
        pagetb[pindex], sequence_.hold, sequence_.pc, sequence_.sp,
        alu_q_,         memdata_out,    memdata_in,   sequence_.stack,
        alu_registers_,
    };
}

unsigned Cpu6::reg(unsigned index) const
{
    if (index >= 8)
        throw std::out_of_range("Invalid register index");
    const auto base = interrupt_level_ * 16 + index * 2;
    return (regfile[base] << 8) | regfile[base + 1];
}

void Cpu6::set_timer(Timer mode) noexcept
{
    timer_ = mode;
    update_timer_period();
}

void Cpu6::timer_edge()
{
    timer_countdown_ = timer_period_;
    if (!(sysctl & 0x20) || timer_ == Timer::Disabled)
        return;
    if (timer_ == Timer::Emulated) {
        rtc = true;
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now >= next_rtc_) {
        using namespace std::chrono_literals;
        rtc = true;
        if (++rtc_phase_ == 15) {
            rtc_phase_ = 0;
            next_rtc_ += 12ms;
        } else {
            next_rtc_ += 17ms;
        }
        if (now > next_rtc_ + 1s)
            next_rtc_ = now;
    }
}

void Cpu6::attach_dma(DmaDevice& device)
{
    if (device.interrupt_mask_ || dma_devices_.size() >= std::numeric_limits<std::uint32_t>::digits)
        throw std::logic_error("Cannot attach DMA device");
    device.interrupt_mask_ = std::uint32_t { 1 } << dma_devices_.size();
    dma_devices_.push_back(&device);
}

void Cpu6::dma_interrupt(const DmaDevice& device, bool on)
{
    dma_12 = (dma_12 & ~device.interrupt_mask_) | (on ? device.interrupt_mask_ : 0);
}

void Cpu6::request_dma(DmaDevice& device)
{
    device.dma_requested = true;
    dma_pending();
}

void Cpu6::select_dma(bool enable, unsigned channel)
{
    for (auto* device : dma_devices_)
        device->select_dma(enable, channel);
}

bool Cpu6::dma_pending()
{
    if (!dma_current) {
        for (auto* device : dma_devices_) {
            if (device->dma_requested) {
                dma_current = device;
                break;
            }
        }
    }
    return dma_current != nullptr;
}

void Cpu6::end_dma()
{
    if (dma_current)
        dma_current->dma_requested = false;
    dma_current = nullptr;
    dma_pending();
}

void Cpu6::advance_dma_address() noexcept
{
    const auto delta = (busctl & 8) ? 1 : -1;
    workaddr = static_cast<std::uint16_t>(workaddr + delta);
    memaddr = static_cast<std::uint16_t>(memaddr + delta);
    map_memory_address();
}

std::uint8_t Cpu6::dma_read()
{
    const auto value = bus_.read(physaddr, memfault_);
    advance_dma_address();
    return value;
}

void Cpu6::dma_write(std::uint8_t value)
{
    bus_.write(physaddr, value, parity_);
    advance_dma_address();
}

void Cpu6::run(std::uint64_t cycles)
{
    if (compiled_ && cycles) {
        CompiledCore::run(*this, cycles);
        return;
    }
    for (std::uint64_t i = 0; i < cycles; ++i)
        step();
}

}
