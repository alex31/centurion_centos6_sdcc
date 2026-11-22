// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once

#include <array>
#include <cstdint>

namespace centurion
{
enum class AluOperation : std::uint8_t { Add, Or, And, Xor };
using MicroWord = std::array<std::uint8_t, 7>;
// Names follow the selectors and latches in the CPU6 microcode listing.
struct MicroInstruction {
    std::uint8_t dp_sel, x_e6, x_k11, x_h11, x_e7;
    std::uint8_t k9_en, k9_sel, mpconst_m6, j12_sel, j11_en, j11_sel;
    std::uint8_t j13_sel, j10_en, j10_sel, k13_sel;
    std::uint16_t seq_d;
    std::uint8_t seq_fci, seq0_si;
    std::uint8_t seq2_si_k6b, seq1_si_k6dc, seq_branch, rir0, x_f6h6, x_c14_sel;
    std::uint8_t aluc_s, aluc_d, busctla, busctlset, busctlmask, aluc_b, aluc_a;
    AluOperation alu_op;
    std::uint8_t invert_r, invert_s;
};

struct MicroSequencer {
    std::uint16_t pc {}, hold {};
    std::array<std::uint16_t, 4> stack {};
    std::uint8_t sp {};

    constexpr unsigned clock(unsigned low, unsigned middle, unsigned high, unsigned direct, unsigned branch,
                             unsigned control) noexcept
    {
        // The three four-bit counters share one carry chain and one stack control.
        const auto source = [&](unsigned selector) -> unsigned {
            switch (selector) {
            case 0: return pc;
            case 1: return hold;
            case 2: return stack[sp];
            default: return direct;
            }
        };
        unsigned next_address = source(low);
        if (low != middle || low != high)
            next_address = (next_address & 0x00f) | (source(middle) & 0x0f0) | (source(high) & 0xf00);
        next_address |= branch;
        if (control == 0) {
            sp = (sp + 1) & 3;
        } else if (control == 2) {
            sp = (sp - 1) & 3;
            stack[sp] = pc;
        }
        pc = (next_address + 1) & 0xfff;
        return next_address;
    }
};

constexpr MicroInstruction decode_microinstruction(const MicroWord& word) noexcept
{
    const auto [v0, v1, v2, v3, v4, v5, v6] = word;
    MicroInstruction insn {};
    insn.dp_sel = v0 & 15;
    insn.x_e6 = (v0 >> 4) & 7;
    insn.x_k11 = (v0 >> 7) | ((v1 & 3) << 1);
    insn.x_h11 = (v1 >> 2) & 7;
    insn.x_e7 = (v1 >> 5) & 3;
    insn.k9_en = (v1 & 128) == 0;
    insn.k9_sel = v2 & 7;
    insn.mpconst_m6 = v2 ^ 255;
    insn.j12_sel = v2 & 3;
    insn.j11_en = (v2 >> 2) & 1;
    insn.j11_sel = (v2 >> 3) & 3;
    insn.j13_sel = (v2 >> 4) & 3;
    insn.j10_en = (v2 >> 5) & 1;
    insn.j10_sel = ((v2 >> 6) & 3) | ((v3 & 1) << 2);
    insn.k13_sel = (v2 >> 6) & 3;
    insn.seq_d = ((v3 & 7) << 8) | v2;
    insn.seq_fci = (v3 >> 3) & 3;
    insn.seq0_si = 3 ^ ((v3 >> 5) & 3);
    const auto seq12_s0i = (v3 >> 7) & 1;
    insn.seq2_si_k6b = 3 ^ (seq12_s0i | ((v4 & 1) << 1));
    insn.seq1_si_k6dc = 3 ^ (seq12_s0i | (2 ^ (2 & (v6 >> 5) & ((v4 << 1) ^ 2))));
    insn.seq_branch = (v4 >> 1) & 1;
    insn.rir0 = (v6 >> 5) & 1;
    insn.x_f6h6 = (v6 >> 3) & 3;
    insn.x_c14_sel = v6 >> 7;
    insn.aluc_s = (v4 >> 2) & 7;
    const auto function = v4 >> 5;
    constexpr std::array operations { AluOperation::Add, AluOperation::Add, AluOperation::Add,
                                      AluOperation::Or,  AluOperation::And, AluOperation::And,
                                      AluOperation::Xor, AluOperation::Xor };
    insn.alu_op = operations[function];
    insn.invert_r = (function == 1 || function == 5 || function == 7) ? 255 : 0;
    insn.invert_s = function == 2 ? 255 : 0;
    insn.aluc_d = v5 & 7;
    insn.busctla = (v5 >> 4) & 7;
    insn.busctlset = ((v5 >> 3) & 1) << insn.busctla;
    insn.busctlmask = (1 << insn.busctla) ^ 255;
    insn.aluc_b = (v5 >> 3) & 15;
    insn.aluc_a = ((v6 << 1) | (v5 >> 7)) & 15;
    return insn;
}
}
