// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "finch.hpp"

#include <algorithm>
#include <bit>
#include <stdexcept>
#include <utility>

namespace centurion
{
namespace
{

constexpr auto crc_table = [] {
    std::array<std::uint16_t, 256> table {};
    for (unsigned i = 0; i < table.size(); ++i) {
        auto value = static_cast<std::uint16_t>(i << 8);
        for (unsigned bit = 0; bit < 8; ++bit) {
            const unsigned polynomial = (value & 0x8000) ? 0x1021 : 0;
            value = static_cast<std::uint16_t>((value << 1) ^ polynomial);
        }
        table[i] = std::byteswap(value);
    }
    return table;
}();

std::uint16_t crc_add(std::uint16_t crc, std::uint8_t byte) noexcept
{
    return (crc >> 8) ^ crc_table[static_cast<std::uint8_t>(crc ^ byte)];
}

constexpr std::array unit_select { -1, 0, 1, -1, 2, -1, -1, -1, 3, -1, -1, -1, -1, -1, -1, -1 };

}

FinchImage::FinchImage(std::string filename, std::vector<std::uint8_t> bytes, bool protect)
    : name(std::move(filename))
    , data(std::move(bytes))
    , write_protected(protect)
{
    if (data.empty() || data.size() % platter_size != 0 || data.size() > 4 * platter_size)
        throw std::invalid_argument("Expected a raw Finch image with one to four platters");
}

Finch::Finch(Backplane& bus, Cpu6& cpu, const Firmware& firmware)
    : cpu_(cpu)
{
    load_microcode(firmware);
    bus.attach(*this, 0x3f800, 2);
    cpu.attach_dma(*this);
    bus.clock_every_microstep(*this);
}

void Finch::load_microcode(const Firmware& firmware)
{
    constexpr std::array names { "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8" };
    std::array<std::span<const std::uint8_t>, names.size()> parts;
    for (unsigned i = 0; i < names.size(); ++i) {
        parts[i] = firmware.table(names[i]);
        if (parts[i].size() != rom_.size())
            throw std::invalid_argument("Each Finch microcode ROM must contain 1024 bytes");
    }
    const auto timing = firmware.table("tsm");
    if (timing.size() != timing_rom_.size())
        throw std::invalid_argument("The Finch timing ROM must contain 256 bytes");
    std::ranges::copy(timing, timing_rom_.begin());

    for (unsigned i = 0; i < rom_.size(); ++i) {
        const auto m0 = parts[0][i], m1 = parts[1][i], m2 = parts[2][i], m4 = parts[4][i];
        const auto m5 = parts[5][i], m6 = parts[6][i], m7 = parts[7][i];
        auto& op = rom_[i];
        op.shift_q = m0 & 7;
        op.shift_ram = (m0 >> 4) & 7;
        op.source = m1 & 15;
        op.destination = m1 >> 4;
        op.constant = parts[3][i];
        op.flag_source = (m2 >> 1) & 7;
        op.test_source = m2 >> 5;
        op.carry_source = (m7 >> 4) & 7;
        op.sequence_control = m7 & 7;
        op.alu_a = m4 & 15;
        op.alu_b = m4 >> 4;
        op.alu_source = m5 & 7;
        op.alu_function = (m5 >> 4) & 7;
        op.alu_destination = m6 & 7;
        op.branch_high = ((m7 << 2) & 0x200) | ((m7 << 5) & 0x100);
        op.read_gate = m0 & 8;
        op.enable_crc = m0 & 128;
        op.flag_direct = m2 & 1;
        op.test_direct = m2 & 16;
        op.increment_address = m5 & 8;
        op.start_dma = !(m5 & 128);
        op.crc_out = m6 & 16;
        op.reset_crc = m6 & 32;
        op.bridge_data = m6 & 128;
    }
}

void Finch::mount(FinchImage disk) { image_.emplace(std::move(disk)); }

void Finch::reset()
{
    address_ = interrupt_priority_ = 0;
    busy_ = to_host_ = from_host_ = dma_read_ = dma_active_ = false;
    sequencer_.pc = 0;
}

std::uint8_t Finch::read(unsigned address)
{
    if (address == 0) {
        to_host_ = false;
        return system_data_;
    }
    return (busy_ << 3) | (status_2_ << 2) | (from_host_ << 1) | to_host_;
}

void Finch::write(unsigned address, std::uint8_t value)
{
    if (address == 0) {
        system_data_ = value;
        from_host_ = true;
    } else {
        reset();
    }
}

void Finch::step_dma(Cpu6& cpu)
{
    if (std::exchange(dma_cycle_, false)) {
        if (dma_read_) {
            system_data_ = cpu.dma_read();
            from_host_ = true;
        } else {
            cpu.dma_write(system_data_);
            to_host_ = false;
        }
    }
    if (!dma_active_)
        cpu.end_dma();
}

void Finch::update_track_base() noexcept
{
    const auto head = ((drive_control_2_ >> 5) & 2) | ((drive_control_2_ >> 3) & 1);
    track_base_ = track_ * FinchImage::track_size + head * FinchImage::platter_size;
}

void Finch::advance_rotation()
{
    if (++rotation_ == FinchImage::track_size * 8) {
        rotation_ = 0;
        index_ = true;
    } else if (rotation_ == 10) {
        index_ = false;
    }

    const auto phase = rotation_ & 7;
    const bool start = index_ || sector_byte_ == 0xfff;
    if (phase == 0)
        sector_byte_ = start ? sector_reload_ : sector_byte_ + 1;
    else if (phase == 4)
        sector_start_ = start ? 2 : sector_start_ - (sector_start_ != 0);
}

std::uint8_t Finch::read_track_byte(unsigned offset) const noexcept
{
    const auto index = track_base_ + offset;
    return index < image_->data.size() ? image_->data[index] : 0;
}

void Finch::write_track_byte(unsigned offset, std::uint8_t value) noexcept
{
    const auto index = track_base_ + offset;
    if (!image_->write_protected && index < image_->data.size())
        image_->data[index] = value;
}

void Finch::tick(unsigned)
{
    advance_rotation();
    const auto previous = timing_state_ & 127;
    if (selected_unit_ == 1 && image_) {
        const auto& op = rom_[sequencer_.pc];
        const auto byte = rotation_ >> 3, phase = rotation_ & 7;
        const auto mode = (drive_control_1_ & 0x40) | (drive_control_2_ & 0x10);
        if (phase == 0 && op.read_gate && mode == 0x40) {
            const auto next = byte + 1 == FinchImage::track_size ? 0 : byte + 1;
            if (read_track_byte(next) & 128)
                data_edge_ = 0;
        }
        if (phase == 7 && mode == 0x10) {
            auto data = std::exchange(data_to_disk_, 0);
            if (crc_out_)
                data = static_cast<std::uint8_t>(crc_);
            if (crc_enabled_)
                crc_ = crc_add(crc_, data);
            write_track_byte(byte, data);
        }
        timing_state_ = timing_rom_[previous | data_edge_];
        data_edge_ = 128;
        const auto rising = (previous ^ 127) & timing_state_;
        if (mode == 0x40 && (rising & 32)) {
            data_from_disk_ = read_track_byte(byte);
            if (crc_enabled_)
                crc_ = crc_add(crc_, data_from_disk_);
        }
        if (phase == 4) {
            crc_enabled_ = op.enable_crc;
            crc_out_ = op.crc_out;
        }
    } else {
        timing_state_ = timing_rom_[previous | data_edge_];
        data_edge_ = 128;
    }
    if (++timing_timeout_ == 255) {
        timing_timeout_ = 0;
        timing_state_ = 127;
    }
    if (!(previous & 3) && (timing_state_ & 3)) {
        micro_step();
        timing_timeout_ = 0;
    }
}

void Finch::Alu::resolve(const MicroInstruction& op, std::uint8_t input, bool carry_in)
{
    const auto a = registers[op.alu_a], b = registers[op.alu_b];
    unsigned r = 0, s = 0;
    switch (op.alu_source) {
    case 0:
        r = a;
        s = q;
        break;
    case 1:
        r = a;
        s = b;
        break;
    case 2: s = q; break;
    case 3: s = b; break;
    case 4: s = a; break;
    case 5:
        r = input;
        s = a;
        break;
    case 6:
        r = input;
        s = q;
        break;
    case 7: r = input; break;
    }
    switch (op.alu_function) {
    case 1:
    case 5:
    case 7: r ^= 255; break;
    case 2: s ^= 255; break;
    }
    unsigned value = 0;
    switch (op.alu_function) {
    case 0:
    case 1:
    case 2:
        value = r + s + carry_in;
        carry = value > 255;
        half_carry = (r & 15) + (s & 15) + carry_in > 15;
        overflow = carry != ((r & 127) + (s & 127) + carry_in > 127);
        break;
    case 3:
        value = r | s;
        half_carry = (value & 15) != 15 || carry_in;
        overflow = carry = value != 255 || carry_in;
        break;
    case 4:
    case 5:
        value = r & s;
        half_carry = (value & 15) != 0 || carry_in;
        overflow = carry = value != 0 || carry_in;
        break;
    case 6:
    case 7: value = r ^ s; break;
    }
    result = static_cast<std::uint8_t>(value);
    zero = result == 0;
    sign = result & 128;
    output = op.alu_destination == 2 ? a : result;
    shift_right = !(op.alu_destination & 2);
    if (op.alu_destination > 3) {
        if (shift_right) {
            q_low = q & 1;
            ram_low = result & 1;
        } else {
            q_high = q & 128;
            ram_high = result & 128;
        }
    }
}

void Finch::Alu::commit(const MicroInstruction& op)
{
    auto& b = registers[op.alu_b];
    switch (op.alu_destination) {
    case 0: q = result; break;
    case 2:
    case 3: b = result; break;
    case 4: q = (q >> 1) | (q_high << 7); [[fallthrough]];
    case 5: b = (result >> 1) | (ram_high << 7); break;
    case 6: q = static_cast<std::uint8_t>((q << 1) | q_low); [[fallthrough]];
    case 7: b = static_cast<std::uint8_t>((result << 1) | ram_low); break;
    }
}

void Finch::Sequencer::commit(unsigned control, unsigned branch, bool test) noexcept
{
    unsigned next = pc + 1;
    switch (control) {
    case 0: next += test; break;
    case 2:
        sp = (sp - 1) & 3;
        if (test)
            next = stack[sp];
        break;
    case 3:
        sp = (sp - 1) & 3;
        next = stack[sp];
        break;
    case 4:
        if (test) {
            stack[sp] = next;
            sp = (sp + 1) & 3;
            next = branch;
        }
        break;
    case 5:
        stack[sp] = pc;
        sp = (sp + 1) & 3;
        break;
    case 6:
        if (test)
            next = branch;
        break;
    case 7: next = 0; break;
    }
    pc = next & 0x3ff;
}

std::uint8_t Finch::read_source(const MicroInstruction& op)
{
    switch (op.source) {
    case 0: return (crc_ == 0 ? 0 : 0x20) | (drive_control_1_ & 0xcf);
    case 1: return 0xfc | (drive_control_2_ & 3);
    case 2: {
        if (selected_unit_ != 1)
            return 0;
        const auto status = image_ ? (2 * image_->platters() - 1) : 0;
        const bool write_allowed = !image_ || !image_->write_protected;
        return (index_ << 7) | ((track_ == 0) << 6) | (write_allowed << 4) | ((sector_start_ != 0) << 3)
            | status;
    }
    case 3:
    case 4: return 255;
    case 6: return data_from_disk_;
    case 8: return static_cast<std::uint8_t>(~address_);
    case 9: return static_cast<std::uint8_t>(~ram_[address_]);
    case 10: from_host_ = false; return static_cast<std::uint8_t>(~system_data_);
    case 11: return 255 ^ ((cpu_.dma_finished() << 2) | (from_host_ << 1) | to_host_);
    case 12: return 255 ^ ((address_ >> 8) & 15);
    case 15: return op.constant;
    default: return 0;
    }
}

void Finch::write_destination(const MicroInstruction& op, std::uint8_t input, std::uint8_t output)
{
    if (op.increment_address) {
        if (op.destination == 8) {
            if ((address_ & 255) == 255)
                address_ = (address_ + 0x100) & 0xfff;
        } else if (op.destination == 11) {
            address_ = ((address_ + 1) & 255) | (address_ & 0xf00);
        } else {
            address_ = (address_ + 1) & 0xfff;
        }
    }
    switch (op.destination) {
    case 0: {
        const auto rising = output & ~drive_control_1_;
        drive_control_1_ = output;
        selected_unit_ = unit_select[output & 15];
        if (rising & 0x40)
            update_track_base();
        break;
    }
    case 1: {
        const auto rising = output & ~drive_control_2_;
        if (rising & 1)
            track_ = (output & 2) ? std::min(track_ + 1, FinchImage::cylinders - 1) : track_ - (track_ != 0);
        if (rising & 4)
            track_ = 0;
        if (rising & 0x15)
            update_track_base(); // Head selection is sampled from the previous latch.
        drive_control_2_ = output;
        break;
    }
    case 2: sector_reload_ = (sector_reload_ & 0xf00) | output; break;
    case 3: data_not_equal_ |= input != data_from_disk_; break;
    case 4: data_to_disk_ = output; break;
    case 6: sector_reload_ = (sector_reload_ & 255) | ((output & 15) << 8); break;
    case 7: timing_state_ = 0x87 ^ (output & 6); break;
    case 8: address_ = (address_ & 0xf00) | output; break;
    case 9: ram_[address_] = output; break;
    case 10:
        system_data_ = output;
        to_host_ = true;
        break;
    case 11: address_ = (address_ & 255) | ((output & 15) << 8); break;
    case 12:
        dma_active_ = input & 1;
        dma_read_ = input & 2;
        status_2_ = input & 4;
        busy_ = input & 8;
        break;
    case 13: interrupt_priority_ = output & 15; break;
    }
}

void Finch::micro_step(bool sample_only)
{
    const auto& op = rom_[sequencer_.pc];
    const auto input = read_source(op);
    const bool previous_flag = flag_;
    const bool carry_in = op.carry_source == 1 || (op.carry_source == 4 && !previous_flag)
        || (op.carry_source == 5 && previous_flag);
    alu_.resolve(op, input, carry_in);
    switch (op.carry_source) {
    case 2: alu_.resolve(op, input, alu_.shift_right ? alu_.ram_low : alu_.ram_high); break;
    case 3: alu_.resolve(op, input, alu_.shift_right ? alu_.q_low : alu_.q_high); break;
    case 6: alu_.resolve(op, input, alu_.carry); break;
    case 7: alu_.resolve(op, input, alu_.sign); break;
    }
    const std::uint8_t output = op.bridge_data ? input ^ 255 : alu_.output;
    if (sample_only)
        return;

    dma_cycle_ |= op.start_dma;
    if (op.reset_crc) {
        data_not_equal_ = false;
        crc_ = 0;
    }
    const bool ram_out = alu_.shift_right ? alu_.ram_low : alu_.ram_high;
    const bool q_out = alu_.shift_right ? alu_.q_low : alu_.q_high;
    const std::array flag_sources { true,       previous_flag, ram_out,   q_out,
                                    alu_.carry, alu_.overflow, alu_.sign, alu_.half_carry };
    flag_ = flag_sources[op.flag_source] == op.flag_direct;
    const std::array shifts { false,          true,          ram_out,    q_out,
                              !previous_flag, previous_flag, alu_.carry, alu_.sign };
    if (alu_.shift_right) {
        alu_.q_high = shifts[op.shift_q];
        alu_.ram_high = shifts[op.shift_ram];
    } else {
        alu_.q_low = shifts[op.shift_q];
        alu_.ram_low = shifts[op.shift_ram];
    }
    const std::array tests { true,          previous_flag, alu_.zero,       alu_.carry,
                             alu_.overflow, alu_.sign,     data_not_equal_, false };
    const bool branch = tests[op.test_source] == op.test_direct;
    write_destination(op, input, output);
    if (!dma_requested && (dma_active_ || dma_cycle_))
        cpu_.request_dma(*this);
    alu_.commit(op);
    sequencer_.commit(op.sequence_control, output | op.branch_high, branch);
}

void Finch::sample_signals() { micro_step(true); }

}
