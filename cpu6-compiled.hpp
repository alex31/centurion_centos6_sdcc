// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once
#include "cpu6.hpp"

namespace centurion
{
struct CompiledCore {
    using Handler = void (*)(Cpu6&, std::uint64_t);
    static bool matches(const std::array<MicroWord, 2048>& words) noexcept;
    static void run(Cpu6& cpu, std::uint64_t cycles);
    template <unsigned Address> static void execute(Cpu6& cpu, std::uint64_t remaining);
    static const std::array<Handler, 2048> handlers;
};
}
