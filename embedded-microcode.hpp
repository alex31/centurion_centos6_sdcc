// Only the seven control-store ROMs participate in specialization. The MAPROM
// and peripheral firmware are always read from the user's runtime pack.
#pragma once

#include "microinstruction.hpp"

#if __has_embed(<cpu6-control-store.rom>) == __STDC_EMBED_FOUND__
#define CPU6_EMBEDDED 1
#endif

namespace centurion::detail
{
#ifdef CPU6_EMBEDDED
consteval auto read_control_store()
{
    constexpr std::uint8_t rom[] = {
#embed <cpu6-control-store.rom>
    };
    std::array<MicroWord, 2048> store {};
    constexpr auto chips = std::tuple_size_v<MicroWord>;
    static_assert(sizeof rom == std::tuple_size_v<decltype(store)> * chips);
    for (std::size_t address = 0; address < store.size(); ++address)
        for (std::size_t chip = 0; chip < chips; ++chip)
            store[address][chip] = rom[address * chips + chip];
    return store;
}
inline constexpr auto embedded = read_control_store();
#endif
}
