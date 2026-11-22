// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once
#include "cpu6-clock.inl"

namespace centurion
{
[[gnu::always_inline]] inline void Cpu6::execute_cycle(const MicroInstruction& mci, unsigned address)
{

    const auto intena = (busctl & 1) != 0;
    const auto sysint = reqlevel;
    const auto dmaint = intena && (interrupt_level_ < 2) && (dma_12 != 0);
    const auto count_up = (busctl & 8) != 0;

    if (address == 0x101) {
        last_pc_ = memaddr;
    }

    unsigned jump_to_subroutine = 0;
    if (mci.k9_en) {
        switch (mci.k9_sel) {
        case 0: /* TODO f12b */ jump_to_subroutine = 1; break;
        case 1: /* rir.0 nor 4 */ jump_to_subroutine = ((rir | (rir >> 4)) & 1) ^ 1; break;
        case 2: /* rir.0 */ jump_to_subroutine = rir & 1; break;
        case 3: /* mmio/reg */ jump_to_subroutine = is_regmmio; break;
        case 4: /* REG/pbit */ jump_to_subroutine = is_mem; break;
        case 5: /* DMA P2.13 */ jump_to_subroutine = dma_pending(); break;
        case 6: /* TODO B15A */ jump_to_subroutine = b15a ^ 1; break;
        case 7: /* TODO F13->H13A any INT */
            jump_to_subroutine = dma_pending();
            if (dmaint || (intena && bus_.interrupt_pending(interrupt_level_))) {
                jump_to_subroutine = 1;
            }
            if (rtc && ((sysctl & 0x4) != 0)) {
                jump_to_subroutine = 1;
            }
            break;
        }
    }
    // branch and condition selectors
    unsigned seq_or = 0;

    // seqencer control
    unsigned seq0_s = mci.seq0_si;
    unsigned seq1_s = mci.seq1_si_k6dc;
    unsigned seq2_s = mci.seq2_si_k6b;
    unsigned seq_fc = mci.seq_fci;
    if (jump_to_subroutine) {
        seq0_s = 3;
        seq1_s = 3;
        seq2_s = 3;
        seq_fc &= 2;
    }

    std::uint8_t datapath = 0;
    // Data Path Control
    switch (mci.dp_sel) {
    case 0:
    case 4: // R_SWP ⇒ DP
        datapath = swap;
        break;
    case 1:
    case 5: // R_REG(d13) ⇒ DP
        datapath = rdr;
        break;
    case 2:
    case 6: // R_ADM(rawH) ⇒ DP
        datapath = (memaddr >> 8) ^ 0xf0;
        break;
    case 3:
    case 7: // R_ADL ⇒ DP
        datapath = static_cast<std::uint8_t>(memaddr);
        break;
    case 8: // R_AddrPH ⇒ DP
        datapath = pgaddr;
        break;
    case 9: // CCR:sense ⇒ DP
        // sense_switch inverted on bus
        datapath = ((ccr_l << 4) | (ccr_f << 5) | (ccr_m << 6) | (ccr_v << 7) | sense_switch)
            ^ 0xf0 /* sense sw */;
        break;
    case 10: // R_SysDLatch ⇒ DP
        datapath = memdata_in;
        break;
    case 11: // R_H14 ⇒ DP // TODO rest of H14? beware invertions
        datapath = (interrupt_level_ << 4) | 4 | (resetting ? 2 : 0) | notpgbit | (dmaint ? 8 : 0);
        if (resetting)
            resetting = false;
        break;
    case 12: // R_DLI (int7..4:dipsw3..1[RHI]:RTINT) ⇒ DP
        datapath = ((sysint & 15) << 4) | dswitch | ((rtc && ((sysctl & 0x4) != 0)) ? 1 : 0);
        break;
    case 13: // RCnst MCv2 ⇒ DP
        datapath = mci.mpconst_m6;
        break;
        // __ncE N/C
        // __ncF N/C
    }

    unsigned alu_result = 0;
    unsigned carry_in = 0;
    unsigned carry_flag = 0; // bit 1<<3

    switch (mci.x_f6h6) {
    case 0: carry_in = 0; break;
    case 1: carry_in = 1; break;
    case 2: carry_in = (alu_flag >> 3) & 1; break;
    case 3: carry_in = 0; break;
    }
    // perform logic flow

    unsigned val_r = 0;
    unsigned val_s = 0;
    switch (mci.aluc_s) {
    case 0:
        val_r = alu_registers_[mci.aluc_a];
        val_s = alu_q_;
        break;
    case 1:
        val_r = alu_registers_[mci.aluc_a];
        val_s = alu_registers_[mci.aluc_b];
        break;
    case 2:
        val_r = 0;
        val_s = alu_q_;
        break;
    case 3:
        val_r = 0;
        val_s = alu_registers_[mci.aluc_b];
        break;
    case 4:
        val_r = 0;
        val_s = alu_registers_[mci.aluc_a];
        break;
    case 5:
        val_r = datapath;
        val_s = alu_registers_[mci.aluc_a];
        break;
    case 6:
        val_r = datapath;
        val_s = alu_q_;
        break;
    case 7:
        val_r = datapath;
        val_s = 0;
        break;
    }
    val_r ^= mci.invert_r;
    val_s ^= mci.invert_s;
    switch (mci.alu_op) { // [+ & | ⊻]
    case AluOperation::Add: // subtraction is just addition with inverted inputs
        alu_result = val_r + val_s + carry_in;
        carry_flag = (alu_result > 255) ? 8 : 0;
        break;
    case AluOperation::Or: alu_result = val_r | val_s; break;
    case AluOperation::And: alu_result = val_r & val_s; break;
    case AluOperation::Xor: alu_result = val_r ^ val_s; break;
    }
    alu_result &= 255;

    std::uint8_t data_f;
    if (mci.x_h11 == 6) { // Select MAPROM to F
        data_f = map_rom_[datapath];
    } else if (mci.aluc_d == 2) {
        data_f = alu_registers_[mci.aluc_a]; // select A to the bus output
    } else {
        data_f = alu_result;
    }
    unsigned ram0 = 0;
    unsigned ram7 = 0;
    unsigned q0 = 0;
    unsigned q7 = 0;
    if (mci.aluc_d > 3) {
        if ((mci.aluc_d & 2) == 0) {
            q0 = alu_q_ & 1;
            ram0 = alu_result & 1;
            // >> 3.h.0 >> 3.l.0 >>
            switch (mci.x_f6h6) { // h6.Za
            case 0: ram7 = alu_result & 128; break;
            case 1: ram7 = ((alu_flag << 4) & 128); break;
            case 2: ram7 = q0 << 7; break;
            case 3: ram7 = carry_flag << 4; break;
            }
            q7 = ram0 << 7;
        } else {
            q7 = alu_q_ & 128;
            ram7 = alu_result & 128;
            // << 3.h.0 << 3.l.0 <<
            switch (mci.x_f6h6) { // h6.Zb
            case 0: break;
            case 1: q0 = ((alu_flag >> 3) & 1); break;
            case 2: q0 = alu_result >> 7; break;
            case 3: q0 = 1; break;
            }
            ram0 = q7 >> 7;
        }
    }
    // uses ALU flags, so must be after comb logic resolve
    if (mci.seq_branch == 0) {
        switch (mci.j13_sel) {
        case 0: // |1 IF ALUF.S, |2 IF ALUF.Z
            seq_or |= ((alu_flag & 2) != 0 ? 1 : 0);
            seq_or |= ((alu_flag & 1) != 0 ? 2 : 0);
            break;
        case 1: // |1 IF ALUF.H, |2 IF ALUF.V
            seq_or |= ((alu_flag & 16) != 0 ? 1 : 0);
            seq_or |= ((alu_flag & 4) != 0 ? 2 : 0);
            break;
        case 2: // |1 IF ~pbit, |2 IF ~MMIO
            seq_or |= notpgbit | (is_mmio << 1);
            break;
        case 3: break; // nop
        }

        switch (mci.k13_sel) { // mind the strange order of OR2/3 (|4 |8)
        case 0: // |4 IF INT_EN, |8 IF CCR.Carry (Link)
            if (intena)
                seq_or |= 4;
            if (ccr_l != 0)
                seq_or |= 8;
            break;
        case 1: // |4 IF DMA_INT, |8 IF INT_REQ
            if (!dmaint)
                seq_or |= 4;
            if (!(intena && bus_.interrupt_pending(interrupt_level_)))
                seq_or |= 8;
            break;
        case 2: // |4 IF MEM_INT, |8 IF DMA_REQ
            // seq_or |= 4; // TODO typically low
            if (!dma_pending())
                seq_or |= 8;
            break;
        case 3: break; // nop
        }
    }

    unsigned rindex;
    if (mci.x_c14_sel) {
        rindex = (rir & 15) | (interrupt_level_ << 4);
    } else {
        rindex = rir;
    }
    // The low address line is represented in its non-inverted form here.
    rindex |= mci.rir0;

    ////////////////////////////// rising clock edge /////////////////////////////

    const auto prevwork = workaddr;
    const auto prevflag = alu_flag;
    switch (mci.x_k11) {
    case 0: break; // nop
    case 1: dma_end_ = false; break; // L13A_SET
    case 2: // ?K11_2
        if ((mci.busctlset == 128) && ((sysctl & 128) == 0)) {
            reqlevel = bus_.acknowledge_interrupt(interrupt_level_);
        }
        if ((mci.busctlmask == 127) && ((sysctl & 128) == 128)) {
            reqlevel = 0;
        }
        sysctl = (sysctl & mci.busctlmask) | mci.busctlset;
        if (mci.busctla == 5)
            update_timer_period();
        if (mci.busctla < 2)
            select_dma((busctl & 128) != 0, sysctl & 3);
        break;
    case 3: // BusCtl
        busctl = (busctl & mci.busctlmask) | mci.busctlset;
        if (mci.busctla == 7)
            select_dma((busctl & 128) != 0, sysctl & 3);
        parity_ = (busctl >> 5) & 1;
        break;
    case 4: // REG_WR
        regfile[rindex] = result;
        break;
    case 5: // PTR_WR
        pagetb[pindex] = result;
        map_memory_address();
        break;
    case 6: // WADL_WR
        if (mci.x_e6 == 5) {
            workaddr = (workaddr & 0xff00) | (memaddr & 0xff);
        } else {
            workaddr = (workaddr & 0xff00) | result;
        }
        break;
    case 7: // BusD_WR
        memdata_out = data_f;
        sys_write_latch = true;
        break;
    }

    tick_timer();

    rdr = regfile[rindex];

    switch (mci.x_e7) {
    case 0: break; // nop
    case 1: break; // TODO: BusReady
    case 2: // ALUFlag_LD
    {
        unsigned overflow_flag = 0; /* bit 1<<2 */
        unsigned half_carry_flag = 0; // bit 1<<4
        switch (mci.alu_op) { // [+ & | ⊻]
        case AluOperation::Add:
            half_carry_flag = (((val_r & 15) + (val_s & 15) + carry_in) > 15) ? 1 << 4 : 0;
            if (((val_r & 127) + (val_s & 127) + carry_in) > 127) {
                overflow_flag = (carry_flag ^ 8) >> 1;
            } else {
                overflow_flag = carry_flag >> 1;
            }
            break;
        case AluOperation::Or:
            half_carry_flag = (alu_result & 15) == 15 ? carry_in << 4 : 16;
            if (alu_result == 255) {
                overflow_flag = carry_in << 2;
                carry_flag = carry_in << 3;
            } else {
                overflow_flag = 4;
                carry_flag = 8;
            }
            break;
        case AluOperation::And:
            half_carry_flag = (alu_result & 15) != 0 ? 16 : (carry_in << 4);
            if (alu_result != 0) {
                overflow_flag = 4;
                carry_flag = 8;
            } else {
                overflow_flag = carry_in << 2;
                carry_flag = carry_in << 3;
            }
            break;
        case AluOperation::Xor: break;
        }
        alu_flag = ((alu_result == 0) ? 1 : 0) | (alu_result > 127 ? 2 : 0) | overflow_flag | carry_flag
            | half_carry_flag | ((prevflag & 1) << 5);
        break;
    }
    case 3: // DataRD
        if (sys_write_latch) {
            memdata_in = memdata_out;
        } else if (sys_read_in) {
            memfault_ = false;
            memdata_in = bus_.read(physaddr, memfault_);
            if ((busctl & 0x40) != 0) {
                b15a = memfault_ ^ 1;
            } else {
                b15a = 1;
            }
        }
        sys_write_latch = false;
        sys_read_in = false;
        break;
    }

    const auto next_address = sequence_.clock(seq0_s, seq1_s, seq2_s, mci.seq_d, seq_or, seq_fc);

    if (((busctl & 20) == 16) && dma_current) {
        if (!dma_current->dma_requested) {
            end_dma();
        } else {
            dma_current->step_dma(*this);
        }
        if (workaddr == 0xffff)
            dma_end_ = true;
    }
    switch (mci.x_h11) {
    case 0: break; // nop
    case 1: // RD_START /* TODO */
        sys_read_in = true;
        break;
    case 2: // WT_START /* TODO */
        bus_.write(physaddr, memdata_out, parity_);
        break;
    case 3: // WorkAddr_LDH
        if (mci.x_e6 == 5) {
            workaddr = (workaddr & 0x00ff) | (memaddr & 0xff00);
        } else {
            workaddr = (workaddr & 0x00ff) | (result << 8);
        }
        break;
    case 4: // WorkAddr_Count
        workaddr = static_cast<std::uint16_t>(workaddr + (count_up ? 1 : -1));
        break;
    case 5: // MemAddr_Count
        memaddr = static_cast<std::uint16_t>(memaddr + (count_up ? 1 : -1));
        map_memory_address();
        break;
    case 6: break; // F=MapROM - handled farther above
    case 7: // NibSwap
        swap = static_cast<std::uint8_t>((datapath << 4) | (datapath >> 4));
        break;
    }

    switch (mci.x_e6) {
    // no case 0
    case 1: result = data_f; break; // F⇒Result
    case 2: rir = data_f; break; // F⇒RIdx
    case 3: interrupt_level_ = data_f >> 4; break; // F⇒Level
    case 4:
        pta = data_f & 7;
        map_memory_address();
        break; // F⇒PTA
    case 5:
        memaddr = prevwork;
        map_memory_address();
        break; // ASwap
    case 6: // SEQ_RE
        sequence_.hold = data_f;
        break;
    case 7: // conditions register
        switch (mci.j12_sel) {
        case 0: break;
        case 1:
            ccr_v = prevflag & 1;
            ccr_m = (prevflag >> 1) & 1;
            break;
        case 2:
            ccr_v = (result >> 7) & 1;
            ccr_m = (result >> 6) & 1;
            break;
        case 3:
            ccr_v = (prevflag & (prevflag >> 5)) & 1;
            ccr_m = (prevflag >> 1) & 1;
            break;
        }
        if (mci.j11_en == 0) {
            switch (mci.j11_sel | (prevflag & 0x4)) {
            case 0: ccr_f = (result >> 5) & 1; break;
            case 1: ccr_f = 1; break;
            case 2: break;
            case 3: ccr_f = 0; break;
            case 4: ccr_f = (result >> 5) & 1; break;
            case 5: ccr_f = 1; break;
            case 6: break;
            case 7: ccr_f = 1; break;
            }
        } else {
            ccr_f = 0;
        }
        if (mci.j10_en == 0) {
            switch (mci.j10_sel) {
            case 0: break;
            case 1: ccr_l = ccr_l ^ 1; break;
            case 2: ccr_l = (prevflag >> 3) & 1; break;
            case 3: ccr_l = 1; break;
            case 4: ccr_l = (result >> 4) & 1; break;
            case 5: ccr_l = ram7 >> 7; break;
            case 6: ccr_l = ram0; break;
            case 7: ccr_l = q0; break;
            }
        } else {
            ccr_l = 0;
        }
        break;
    }

    // save results to register(s)
    switch (mci.aluc_d) {
    case 0: alu_q_ = alu_result; break; // Q=
    case 1: break; // nop
    case 2: alu_registers_[mci.aluc_b] = alu_result; break; // B=
    case 3: alu_registers_[mci.aluc_b] = alu_result; break; // B=
    case 4: // B=F>>1 Q>>=1
        alu_registers_[mci.aluc_b] = (alu_result >> 1) | ram7;
        alu_q_ = (alu_q_ >> 1) | q7;
        break;
    case 5: // B=F>>1
        alu_registers_[mci.aluc_b] = (alu_result >> 1) | ram7;
        break;
    case 6: // B=F<<1 Q<<=1
        alu_registers_[mci.aluc_b] = static_cast<std::uint8_t>((alu_result << 1) | ram0);
        alu_q_ = static_cast<std::uint8_t>((alu_q_ << 1) | q0);
        break;
    case 7: // B=F<<1
        alu_registers_[mci.aluc_b] = static_cast<std::uint8_t>((alu_result << 1) | ram0);
        break;
    }
    micro_pc_ = next_address & 0x7ff;

    ++microsteps_;
    if (bus_.has_fast_clocks())
        bus_.tick_fast();
}

}
