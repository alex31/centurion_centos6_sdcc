// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once

#include "machine.hpp"

#include <algorithm>
#include <chrono>

namespace centurion
{

class Pacer
{
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto period = std::chrono::milliseconds(20);

    explicit Pacer(Clock::time_point now)
        : previous_(now)
        , deadline_(now + period)
    {
    }

    [[nodiscard]] Clock::time_point deadline() const noexcept { return deadline_; }

    unsigned cycles_due(Clock::time_point now) noexcept
    {
        constexpr auto cycle_time = std::chrono::nanoseconds(1'000'000'000 / Machine::nominal_clock);
        constexpr auto max_credit = std::chrono::duration_cast<Clock::duration>(2 * period);
        const auto credit = std::min(remainder_ + now - previous_, max_credit);
        const auto batches = credit / (cycle_time * Machine::hardware_batch);
        const auto cycles = static_cast<unsigned>(batches) * Machine::hardware_batch;
        remainder_ = credit - cycle_time * cycles;
        previous_ = now;
        // Keep the phase tied to the monotonic clock when a host wake-up is late.
        deadline_ += period * std::max(Clock::duration::rep { 1 }, (now - deadline_) / period + 1);
        return cycles;
    }

private:
    Clock::time_point previous_, deadline_;
    Clock::duration remainder_ {};
};

}
