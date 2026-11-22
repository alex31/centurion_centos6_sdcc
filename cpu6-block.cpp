// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "cpu6-compiled.hpp"
#include "embedded-microcode.hpp"

#ifdef CPU6_EMBEDDED
#include "cpu6-cycle.inl"

namespace centurion
{
#define CPU6_ADDRESS(n) extern template void CompiledCore::execute<n>(Cpu6&, std::uint64_t);
#include "addresses.inc"
#undef CPU6_ADDRESS

template <unsigned Address> void CompiledCore::execute(Cpu6& cpu, std::uint64_t remaining)
{
    constexpr auto op = decode_microinstruction(detail::embedded[Address]);
    cpu.execute_cycle(op, Address);
    if (--remaining == 0)
        return;
    // The branch OR inputs are inactive in this mode. Use a direct jump only
    // when all three slices select the ROM literal, or K9 forces that selection.
    constexpr bool direct = op.seq0_si == 3 && op.seq1_si_k6dc == 3 && op.seq2_si_k6b == 3;
    if constexpr (op.seq_branch && (direct || op.k9_en)) {
        constexpr unsigned target = op.seq_d & 0x7ff;
        // A device can reset the CPU on the clock edge. Honor the latched PC.
        if (cpu.micro_pc_ == target) {
            [[gnu::musttail]] return execute<target>(cpu, remaining);
        }
    }
    [[gnu::musttail]] return handlers[cpu.micro_pc_](cpu, remaining);
}

#define CPU6_BLOCK CPU6_BUILD_BLOCK
#define CPU6_ADDRESS(n) template void CompiledCore::execute<n>(Cpu6&, std::uint64_t);
#include "addresses.inc"
#undef CPU6_ADDRESS
}
#endif
