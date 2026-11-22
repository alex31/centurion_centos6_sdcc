// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "cpu6-cycle.inl"

namespace centurion
{
void Cpu6::step() { execute_cycle(microcode_[micro_pc_], micro_pc_); }
}
