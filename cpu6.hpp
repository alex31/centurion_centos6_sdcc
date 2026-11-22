// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once

#include "backplane.hpp"
#include "microinstruction.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace centurion
{

class Cpu6;

class DmaDevice
{
public:
    virtual ~DmaDevice() = default;
    virtual void select_dma(bool enable, unsigned channel) = 0;
    virtual void step_dma(Cpu6& cpu) = 0;
    bool dma_requested {};

private:
    friend class Cpu6;
    std::uint32_t interrupt_mask_ {};
};

class Cpu6
{
public:
    enum class Timer { Emulated, HostClock, Disabled };

    Cpu6(Backplane& bus, const Firmware& firmware);
    void reset();
    void step();
    void run(std::uint64_t cycles);

    void set_pc(std::uint16_t address);
    [[nodiscard]] bool at_instruction_boundary() const noexcept
    {
        return micro_pc_ == 0x100 || micro_pc_ == 0x101;
    }
    [[nodiscard]] unsigned pc() const noexcept { return at_instruction_boundary() ? memaddr : last_pc_; }
    [[nodiscard]] unsigned micro_pc() const noexcept { return micro_pc_; }
    [[nodiscard]] unsigned level() const noexcept { return interrupt_level_; }
    [[nodiscard]] unsigned map() const noexcept { return pta; }
    [[nodiscard]] unsigned physical_address() const noexcept { return physaddr; }
    [[nodiscard]] unsigned memory_address() const noexcept { return memaddr; }
    [[nodiscard]] unsigned condition_codes() const noexcept;
    [[nodiscard]] bool parity_fault() const noexcept { return memfault_; }
    [[nodiscard]] unsigned reg(unsigned index) const;
    [[nodiscard]] const auto& register_ram() const noexcept { return regfile; }
    [[nodiscard]] const auto& page_ram() const noexcept { return pagetb; }
    [[nodiscard]] std::uint64_t microsteps() const noexcept { return microsteps_; }
    [[nodiscard]] const auto& microinstruction() const noexcept { return microcode_words_[micro_pc_]; }

    struct MicroState {
        unsigned work_address, result, register_index, register_data, nibble_swap, alu_flags;
        unsigned bus_control, system_control, page_entry, sequence_hold, sequence_pointer;
        unsigned sequence_stack_position, alu_q, memory_data_out, memory_data_in;
        std::array<std::uint16_t, 4> sequence_stack;
        std::array<std::uint8_t, 16> alu_registers;
        bool operator==(const MicroState&) const = default;
    };

    [[nodiscard]] MicroState micro_state() const noexcept;

    void set_sense_switches(unsigned value) noexcept { sense_switch = value & 15; }
    [[nodiscard]] unsigned sense_switches() const noexcept { return sense_switch; }
    void set_diagnostic_switches(unsigned value) noexcept { dswitch = value & 14; }
    void set_timer(Timer mode) noexcept;
    [[nodiscard]] bool uses_compiled_microcode() const noexcept { return compiled_; }
    void guard_early_boot(bool on) noexcept { boot_guard_ = on; }

    void attach_dma(DmaDevice& device);
    void request_dma(DmaDevice& device);
    void dma_interrupt(const DmaDevice& device, bool on);
    [[nodiscard]] bool dma_finished() const noexcept { return dma_end_; }
    std::uint8_t dma_read();
    void dma_write(std::uint8_t value);
    void end_dma();

private:
    void load_microcode(const Firmware& firmware);
    inline void map_memory_address() noexcept;
    friend struct CompiledCore;
    inline void execute_cycle(const MicroInstruction& mci, unsigned address);
    inline void update_timer_period() noexcept;
    inline void tick_timer();
    void timer_edge();
    void select_dma(bool enable, unsigned channel);
    bool dma_pending();
    void advance_dma_address() noexcept;

    Backplane& bus_;
    std::uint8_t alu_q_ {};
    MicroSequencer sequence_ {};

    std::uint16_t micro_pc_ {}, last_pc_ {};
    std::uint16_t workaddr {}, memaddr {};
    std::uint32_t physaddr {};
    std::uint8_t alu_flag {}, ccr_f {}, ccr_l {}, ccr_m {}, ccr_v {};
    std::uint8_t memdata_in {}, memdata_out {}, busctl {}, sysctl {};
    std::uint8_t b15a { 1 }, swap {}, result {}, interrupt_level_ {}, reqlevel {};
    std::uint8_t rir {}, rdr {}, pta {}, pindex {}, pgaddr {};
    std::uint8_t is_mmio {}, is_regmmio {}, notpgbit {}, is_mem {};
    std::uint8_t sense_switch { 10 }, dswitch {};
    bool resetting { true }, sys_write_latch {}, sys_read_in {};
    bool parity_ {}, memfault_ {}, dma_end_ {};

    std::uint64_t microsteps_ {};
    int timer_period_ { 83333 }, timer_countdown_ { 83333 };
    unsigned rtc_phase_ {};
    bool compiled_ {};
    bool rtc {}, boot_guard_ {};
    std::chrono::steady_clock::time_point next_rtc_ {};
    Timer timer_ { Timer::Emulated };

    DmaDevice* dma_current {};
    std::uint32_t dma_12 {};
    std::array<std::uint8_t, 16> alu_registers_ {};
    std::array<std::uint8_t, 256> regfile {}, pagetb {}, map_rom_ {};
    std::array<MicroInstruction, 2048> microcode_ {};
    std::array<MicroWord, 2048> microcode_words_ {};
    std::vector<DmaDevice*> dma_devices_;
};

}
