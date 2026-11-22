// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once

#include "cpu6.hpp"

#include <optional>
#include <string>
#include <vector>

namespace centurion
{

struct HawkImage {
    explicit HawkImage(std::string name, std::vector<std::uint8_t> data, bool write_protected = false);

    std::string name;
    std::vector<std::uint8_t> data;
    unsigned stride;
    bool write_protected;
};

class Hawk final : public Device, public DmaDevice
{
public:
    Hawk(Backplane& bus, Cpu6& cpu);

    std::uint8_t read(unsigned address) override;
    void write(unsigned address, std::uint8_t value) override;
    void reset() override;
    void tick(unsigned increment) override;
    void select_dma(bool enable, unsigned channel) override;
    void step_dma(Cpu6& cpu) override;

    void mount(unsigned unit, HawkImage image);
    void eject(unsigned unit);
    [[nodiscard]] const HawkImage* image(unsigned unit) const;

private:
    struct Unit {
        std::optional<HawkImage> image;
        unsigned address {};
    };
    enum class Transfer { Verify, Read, Write, ProtectedWrite };

    [[nodiscard]] Unit* selected_unit() noexcept;
    void start_command(std::uint8_t command);
    void start_transfer(Transfer operation);
    void finish_transfer(unsigned delay);
    void update_interrupt();

    Cpu6& cpu_;
    std::array<Unit, 2> units_ {};
    unsigned drive_ {}, address_ {}, write_mask_ {};
    unsigned busy_time_ {}, busy_timeout_ {};
    unsigned sector_remaining_ {}, sector_base_ {}, sector_crc_ {};
    int command_ { -1 };
    Transfer transfer_ { Transfer::Verify };
    bool busy_ {}, seeking_ {}, seek_done_ {}, format_permit_ {};
    bool address_error_ {}, format_error_ {}, check_error_ {}, timeout_error_ {};
    bool dma_selected_ {}, interrupt_enabled_ {}, interrupt_pending_ {};
};

}
