// Shared by the runtime decoder and the ROM-specialized microcycles.
#pragma once
#include "cpu6.hpp"

namespace centurion
{
inline void Cpu6::map_memory_address() noexcept
{
    pindex = (memaddr >> 11) | (pta << 5);
    const unsigned page = pagetb[pindex], page_address = page & 127;
    pgaddr = page_address | ((page_address & 112) == 112 ? 128 : 0);
    is_mmio = (page & 253) != 253;
    const unsigned is_register = page_address != 0 || (memaddr & 1792) != 0;
    is_regmmio = (is_register & is_mmio) ^ 1;
    notpgbit = (page >> 7) ^ 1;
    is_mem = (is_register & notpgbit) ^ 1;
    physaddr = (page_address << 11) | (memaddr & 0x7ff);
}

inline void Cpu6::update_timer_period() noexcept
{
    const int period = timer_ == Timer::HostClock && (sysctl & 0x20) ? 2000 : 83333;
    // Preserve the elapsed portion, even when switching to a shorter period.
    timer_countdown_ += period - timer_period_;
    timer_period_ = period;
}

inline void Cpu6::tick_timer()
{
    if (boot_guard_ && memaddr < 0x0600) {
        timer_countdown_ = timer_period_;
        rtc = false;
    } else if (--timer_countdown_ <= 0) {
        timer_edge();
    }
    if (!(sysctl & 0x20))
        rtc = false;
}
}
