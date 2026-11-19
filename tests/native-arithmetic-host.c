/* Test adapter around the real instruction core, not a second arithmetic model. */
#include "../swext/centurion-c/cpu6.c"

static uint8_t test_memory[0x80000];
uint8_t mem_read8(uint32_t address) { return test_memory[address & 0x7ffff]; }
uint8_t mem_read8_debug(uint32_t address) { return mem_read8(address); }
uint16_t mem_read16_debug(uint32_t address) { return mem_read8(address) * 256U + mem_read8(address + 1); }
void mem_write8(uint32_t address, uint8_t value) { test_memory[address & 0x7ffff] = value; }
void mem_write8_debug(uint32_t address, uint8_t value) { mem_write8(address, value); }
void mem_write16_debug(uint32_t address, uint16_t value) { mem_write8(address, value >> 8); mem_write8(address + 1, value); }
void halt_system(void) { halted = 1; }
void advance_time(uint64_t nanoseconds) { (void)nanoseconds; }
void disassemble(unsigned opcode) { (void)opcode; }

#ifndef CPU6_NATIVE_ADAPTER_ONLY
/* Fixed binary records: opcode, mode, flags, level, 8 register words,
   instruction extension, memory address/value. All words are big endian.
   Output: 8 register words, flags, level, PC, memory word (22 bytes). */
static uint16_t word(const uint8_t *p) { return p[0] * 256U + p[1]; }
static void putword(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = v; }

int main(void)
{
    uint8_t input[26], output[22];
    cpu6_init();
    while (fread(input, sizeof(input), 1, stdin) == 1) {
        cpu_ipl = input[3] & 15;
        cpu_mmu = 0;
        int_enable = 0;
        pending_ipl_mask = 0;
        halted = 0;
        for (unsigned i = 0; i < 8; i++) regpair_write(i * 2, word(input + 4 + i * 2));
        alu_out = input[2];
        pc = 0x0200;
        mem_write8(pc, input[0]); mem_write8(pc + 1, input[1]);
        mem_write8(pc + 2, input[20]); mem_write8(pc + 3, input[21]);
        uint16_t address = word(input + 22);
        mmu_mem_write16(address, word(input + 24));
        cpu6_execute_one(0);
        for (unsigned i = 0; i < 8; i++) putword(output + i * 2, regpair_read(i * 2));
        output[16] = alu_out; output[17] = cpu_ipl;
        putword(output + 18, pc); putword(output + 20, mmu_mem_read16(address));
        if (fwrite(output, sizeof(output), 1, stdout) != 1) return 1;
    }
    return ferror(stdin) || ferror(stdout);
}
#endif
