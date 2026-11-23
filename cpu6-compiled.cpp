// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "cpu6-compiled.hpp"
#include "embedded-microcode.hpp"

#include <algorithm>

namespace centurion
{
bool CompiledCore::matches(const std::array<MicroWord, 2048>& words) noexcept
{
#ifdef CPU6_EMBEDDED
    return words == detail::embedded;
#else
    (void)words;
    return false;
#endif
}

#ifdef CPU6_EMBEDDED
const std::array<CompiledCore::Handler, 2048> CompiledCore::handlers {
#define CPU6_ADDRESS(n) &execute<n>,
#include "addresses.inc"
#undef CPU6_ADDRESS
};
#endif

void CompiledCore::run(Cpu6& cpu, std::uint64_t cycles)
{
#ifdef CPU6_EMBEDDED
#if __has_cpp_attribute(gnu::musttail)
    handlers[cpu.micro_pc_](cpu, cycles);
#else
    // Bound stack usage when the compiler does not guarantee tail calls.
    constexpr std::uint64_t max_chain = 256;
    while (cycles) {
        const auto count = std::min(cycles, max_chain);
        handlers[cpu.micro_pc_](cpu, count);
        cycles -= count;
    }
#endif
#else
    while (cycles--)
        cpu.step();
#endif
}
}
