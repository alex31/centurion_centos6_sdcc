// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once

#include "backplane.hpp"

#include <array>
#include <functional>

namespace centurion
{

class Mux final : public Device
{
public:
    using Transmit = std::function<void(std::uint8_t)>;

    std::uint8_t read(unsigned address) override;
    void write(unsigned address, std::uint8_t value) override;
    void reset() override;
    bool signals_irq_changes() const noexcept override { return true; }
    void acknowledge() override;

    void bind(unsigned port, Transmit transmit, bool cts = true);
    void observe_writes(unsigned port, Transmit observer);
    void set_cts(unsigned port, bool asserted);
    void receive(unsigned port, std::uint8_t byte);
    [[nodiscard]] bool can_receive(unsigned port) const { return !ports_.at(port).read_busy; }
    [[nodiscard]] bool rts(unsigned port) const { return ports_.at(port).rts; }
    [[nodiscard]] int baud(unsigned port) const { return ports_.at(port).baud; }
    [[nodiscard]] int frame_bits(unsigned port) const { return ports_.at(port).frame_cost / 50; }
    void advance_transmit();

private:
    struct Port {
        Transmit transmit, observer;
        int baud {}, transmit_credit {}, frame_cost { 400 };
        unsigned character_width {};
        std::uint8_t read_buffer {}, write_buffer {};
        bool write_busy {}, read_busy {}, overrun {}, rts { true }, cts { true };
    };

    void check_transmit(unsigned index);
    void interrupt() noexcept;
    std::array<Port, 4> ports_ {};
    unsigned level_ {}, transmit_interrupts_ {};
    bool enabled_ {}, pending_ {}, cause_ {};
};

}
