// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once

#include "cpu6.hpp"

#include <optional>
#include <string>
#include <vector>

namespace centurion
{

struct FinchImage {
    static constexpr unsigned track_size = 13'440;
    static constexpr unsigned cylinders = 605;
    static constexpr unsigned platter_size = track_size * cylinders;

    explicit FinchImage(std::string name, std::vector<std::uint8_t> data, bool write_protected = false);
    [[nodiscard]] unsigned platters() const noexcept { return data.size() / platter_size; }

    std::string name;
    std::vector<std::uint8_t> data;
    bool write_protected;
};

class Finch final : public Device, public DmaDevice
{
public:
    Finch(Backplane& bus, Cpu6& cpu, const Firmware& firmware);

    std::uint8_t read(unsigned address) override;
    void write(unsigned address, std::uint8_t value) override;
    void reset() override;
    void tick(unsigned increment) override;
    void select_dma(bool, unsigned) override { }
    void step_dma(Cpu6& cpu) override;

    void mount(FinchImage image);
    void eject() noexcept { image_.reset(); }
    [[nodiscard]] const FinchImage* image() const noexcept { return image_ ? &*image_ : nullptr; }

    // The JS server samples the Finch bus once at the end of a CPU slice.
    void sample_signals();

private:
    struct MicroInstruction {
        std::uint8_t shift_q, shift_ram, source, destination, constant;
        std::uint8_t flag_source, test_source, carry_source, sequence_control;
        std::uint8_t alu_a, alu_b, alu_source, alu_function, alu_destination;
        std::uint16_t branch_high;
        bool read_gate, enable_crc, flag_direct, test_direct;
        bool increment_address, start_dma, crc_out, reset_crc, bridge_data;
    };

    struct Alu {
        std::array<std::uint8_t, 16> registers {};
        std::uint8_t q {}, result {}, output {};
        bool carry {}, half_carry {}, overflow {}, sign {}, zero {};
        bool ram_low {}, ram_high {}, q_low {}, q_high {}, shift_right {};

        void resolve(const MicroInstruction& op, std::uint8_t input, bool carry_in);
        void commit(const MicroInstruction& op);
    };

    struct Sequencer {
        std::array<std::uint16_t, 4> stack {};
        std::uint16_t pc {};
        std::uint8_t sp {};

        void commit(unsigned control, unsigned branch, bool test) noexcept;
    };

    void load_microcode(const Firmware& firmware);
    void update_track_base() noexcept;
    void advance_rotation();
    [[nodiscard]] std::uint8_t read_track_byte(unsigned offset) const noexcept;
    void write_track_byte(unsigned offset, std::uint8_t value) noexcept;
    std::uint8_t read_source(const MicroInstruction& op);
    void write_destination(const MicroInstruction& op, std::uint8_t input, std::uint8_t output);
    void micro_step(bool sample_only = false);

    Cpu6& cpu_;
    std::array<MicroInstruction, 1024> rom_ {};
    std::array<std::uint8_t, 256> timing_rom_ {};
    std::array<std::uint8_t, 4096> ram_ {};
    Alu alu_;
    Sequencer sequencer_;
    std::optional<FinchImage> image_;
    int selected_unit_ { -1 };
    unsigned track_ { 7 }, rotation_ {}, track_base_ {};
    unsigned sector_start_ {}, sector_byte_ {};
    std::uint16_t address_ {}, sector_reload_ {}, crc_ {};
    std::uint8_t system_data_ {}, drive_control_1_ {}, drive_control_2_ {};
    std::uint8_t data_to_disk_ {}, data_from_disk_ {}, interrupt_priority_ {};
    std::uint8_t timing_state_ { 255 }, timing_timeout_ {}, data_edge_ { 128 };
    bool index_ {}, crc_enabled_ {}, crc_out_ {}, flag_ {}, data_not_equal_ {};
    bool to_host_ {}, from_host_ {}, status_2_ {}, busy_ {};
    bool dma_read_ {}, dma_active_ {}, dma_cycle_ {};
};

}
