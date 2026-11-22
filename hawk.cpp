// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "hawk.hpp"

#include <stdexcept>
#include <utility>

namespace centurion
{
namespace
{

constexpr unsigned sector_size = 400;
constexpr unsigned sector_count = 12992;

}

HawkImage::HawkImage(std::string filename, std::vector<std::uint8_t> bytes, bool protect)
    : name(std::move(filename))
    , data(std::move(bytes))
    , stride(0)
    , write_protected(protect)
{
    if (data.size() == sector_size * sector_count)
        stride = sector_size;
    else if (data.size() == 512 * sector_count)
        stride = 512;
    else
        throw std::invalid_argument("Expected a 400-byte or 512-byte stride Hawk image");
}

Hawk::Hawk(Backplane& bus, Cpu6& cpu)
    : cpu_(cpu)
{
    bus.attach(*this, 0x3f140, 16);
    cpu.attach_dma(*this);
}

void Hawk::mount(unsigned unit, HawkImage image) { units_.at(unit).image.emplace(std::move(image)); }

void Hawk::eject(unsigned unit) { units_.at(unit).image.reset(); }

const HawkImage* Hawk::image(unsigned unit) const
{
    const auto& image = units_.at(unit).image;
    return image ? &*image : nullptr;
}

Hawk::Unit* Hawk::selected_unit() noexcept
{
    const auto index = drive_ & 7;
    return index < units_.size() ? &units_[index] : nullptr;
}

void Hawk::reset()
{
    command_ = -1;
    format_permit_ = seeking_ = seek_done_ = busy_ = address_error_ = false;
    busy_time_ = write_mask_ = drive_ = 0;
    interrupt_enabled_ = interrupt_pending_ = false;
    update_interrupt();
}

void Hawk::update_interrupt()
{
    cpu_.dma_interrupt(*this, dma_selected_ && interrupt_enabled_ && interrupt_pending_);
}

void Hawk::select_dma(bool enable, unsigned channel)
{
    const bool selected = enable && channel == 0;
    if (std::exchange(dma_selected_, selected) != selected)
        cpu_.dma_interrupt(*this, interrupt_pending_ && interrupt_enabled_);
}

void Hawk::tick(unsigned increment)
{
    if (busy_) {
        busy_timeout_ += increment;
        if (command_ < 2 || command_ == 4 || command_ == 5) {
            if (auto* unit = selected_unit(); unit && unit->image)
                busy_timeout_ = 0;
        }
        if (busy_timeout_ > 300000) {
            busy_ = false;
            timeout_error_ = true;
        }
    }
    // CenRE advances the seek/completion latch once per hardware batch.
    if (!busy_time_ || --busy_time_ != 0)
        return;
    interrupt_pending_ |= std::exchange(busy_, false);
    if (command_ == 2 || command_ == 3) {
        seeking_ = false;
        seek_done_ = interrupt_pending_ = true;
    }
    update_interrupt();
    command_ = -1;
}

std::uint8_t Hawk::read(unsigned address)
{
    switch (address) {
    case 0: return drive_ | 0xf0;
    case 1: return address_ >> 8;
    case 2: return address_ & 255;
    case 3: return write_mask_;
    case 4:
        return (timeout_error_ ? 128 : 0) | (check_error_ ? 64 : 0) | (address_error_ ? 32 : 0)
            | (format_error_ ? 16 : 0) | busy_;
    case 5:
        if (auto* unit = selected_unit()) {
            return (!unit->image || unit->image->write_protected ? 128 : 0)
                | ((write_mask_ & (1 << (drive_ & 7))) ? 64 : 0) | (seeking_ ? 0 : 32)
                | (unit->image ? 16 : 0) | seek_done_;
        }
        return seek_done_;
    case 8:
    case 9:
    case 10: return 0;
    default: return 255;
    }
}

void Hawk::start_transfer(Transfer operation)
{
    busy_time_ = 0;
    sector_remaining_ = sector_size;
    transfer_ = operation;
    cpu_.request_dma(*this);
}

void Hawk::finish_transfer(unsigned delay)
{
    cpu_.end_dma();
    busy_time_ = delay;
}

void Hawk::step_dma(Cpu6& cpu)
{
    auto* unit = selected_unit();
    if (!unit || !unit->image)
        return;
    auto& image = *unit->image;
    if (cpu.dma_finished()) {
        finish_transfer(10);
        return;
    }
    if (sector_remaining_ == 0) {
        sector_remaining_ = sector_size;
        address_ = (address_ & 0xff00) | ((address_ + 1) & 255);
    }
    if (sector_remaining_ == sector_size) {
        if ((address_ & 0xffe0) != (unit->address & 0xffe0) || address_ >= sector_count) {
            address_error_ = true;
            finish_transfer(200);
            return;
        }
        sector_base_ = address_ * image.stride;
        sector_crc_ = 0;
        if (image.stride > sector_size) {
            const auto flag = image.data[sector_base_ + 402];
            if (flag == 0x41 || flag == 0x46) {
                address_error_ = flag == 0x41;
                format_error_ = flag == 0x46;
                finish_transfer(20);
                return;
            }
        }
    }

    auto& destination = image.data[sector_base_ + sector_size - sector_remaining_];
    const unsigned data = transfer_ == Transfer::Read ? destination : cpu.dma_read();
    switch (transfer_) {
    case Transfer::Verify: check_error_ |= data != destination; break;
    case Transfer::Read: cpu.dma_write(data); break;
    case Transfer::Write: destination = data; break;
    case Transfer::ProtectedWrite: break;
    }
    if (transfer_ != Transfer::ProtectedWrite)
        sector_crc_ ^= destination << ((sector_remaining_ & 1) == 0 ? 8 : 0);
    if (--sector_remaining_ == 0 && image.stride > sector_size) {
        image.data[sector_base_ + 400] = sector_crc_ >> 8;
        image.data[sector_base_ + 401] = sector_crc_ & 255;
    }
}

void Hawk::start_command(std::uint8_t command)
{
    address_error_ = format_error_ = check_error_ = timeout_error_ = false;
    busy_timeout_ = 0;
    if (!(command & 2))
        busy_ = true;
    auto* unit = selected_unit();
    if (!unit)
        return;
    seek_done_ = false;
    command_ = command;
    if (command != 5)
        format_permit_ = false;
    switch (command & 7) {
    case 0: start_transfer(Transfer::Read); break;
    case 1: {
        const bool writable
            = (write_mask_ & (1 << (drive_ & 7))) && unit->image && !unit->image->write_protected;
        start_transfer(writable ? Transfer::Write : Transfer::ProtectedWrite);
        break;
    }
    case 2:
        unit->address = address_;
        seeking_ = true;
        busy_time_ = 40;
        break;
    case 3:
        unit->address = address_ = 0;
        seeking_ = true;
        busy_time_ = 200;
        break;
    case 4: start_transfer(Transfer::Verify); break;
    case 5:
        if (format_permit_)
            start_transfer(Transfer::Write);
        else
            busy_time_ = 10;
        break;
    case 6: format_permit_ = true; break;
    }
}

void Hawk::write(unsigned address, std::uint8_t value)
{
    switch (address) {
    case 0: drive_ = value & 15; break;
    case 1: address_ = (address_ & 255) | (value << 8); break;
    case 2: address_ = (address_ & 0xff00) | value; break;
    case 3: write_mask_ = value; break;
    case 8: start_command(value); break;
    case 11:
        reset();
        drive_ = value & 15;
        break;
    case 12:
        interrupt_pending_ = true;
        update_interrupt();
        break;
    case 13:
        interrupt_enabled_ = false;
        update_interrupt();
        break;
    case 14:
        interrupt_enabled_ = true;
        update_interrupt();
        break;
    case 15:
        interrupt_pending_ = false;
        update_interrupt();
        break;
    }
}

}
