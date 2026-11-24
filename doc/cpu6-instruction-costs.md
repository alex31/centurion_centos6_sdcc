# CPU6 instruction costs for SDCC

Each candidate instruction sequence has an encoded size in bytes and an
execution time in CPU6 microcycles. The assembler chooses the encoding
from the operands. For example, `xfr a,b` occupies one byte and takes eight
microcycles, while `xfr b,a` occupies two and takes fifteen. The comparisons
below use normal instruction fetch from RAM with no interrupt or DMA service.
[EE200 instruction encoder](../swext/fuzix-bintools/as1-ee200.c)

## Reproduce the measurements

Import the firmware, then build the JavaScript emulator and run the probe.
[Build instructions](../README.md#build)

```sh
make emulator
node tests/cpu6-instruction-costs.js build/out/cpu6-instruction-costs.tsv
```

The TSV records each encoding, disassembled operands, input conditions,
encoded size, microcycles and final PC or branch outcome. The probe resets
the microcoded CPU for each trial, prepares its registers and RAM, and
counts steps between ordinary instruction boundaries. It checks instruction
lengths against the next PC for sequential instructions and exercises both
outcomes of all 16 conditional branches. A cross-check on the native
microcoded core reproduced 57 sequential instruction timings. The separate
C instruction emulator has partial timing estimates. [Probe](../tests/cpu6-instruction-costs.js)
· [C instruction emulator](../swext/centurion-c/cpu6.c)

## Primary opcode table

The row gives the high hex digit and the column the low digit. Each entry is
the observed minimum and maximum over the sampled operands. Actual latency
can also depend on operand values, the block length, and hardware or MMU
state. `00` halts. The seven remaining dashes are reserved in the bundled
disassembler. The probe uses the assembler's `RIM` mnemonic for `0B`.
[Disassembler](../swext/centurion-js/src/cen.ts)
· [Assembler mnemonics](../swext/fuzix-bintools/as6-ee200.c)

| High / low | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | — | 4 | 5 | 5 | 8 | 8 | 5 | 5 | 5 | 21 | 30 | 43 | 6 | 9 | 22725 | 42 |
| 1 | 9–16 | 10–17 | 9–16 | 10–17 | 9–16 | 10–17 | 9–16 | 10–17 | 10–17 | 9–16 | 10–17 | 10–17 | 10–17 | 10–17 | 9–16 | 10–17 |
| 2 | 12 | 12 | 11 | 12 | 14–44 | 14–62 | 18–63 | 18–108 | 6 | 6 | 5 | 6 | 9 | 6 | 67–340 | 14–21 |
| 3 | 13–35 | 13–35 | 12–34 | 13–36 | 17–85 | 20–118 | 21–104 | 22–150 | 7 | 7 | 6 | 7 | 10 | 8 | 10 | 10 |
| 4 | 11 | 11 | 11 | 11 | 11 | 11 | 80–34513 | 72–735 | 8 | 8 | 8 | 7 | 7 | 7 | 7 | 7 |
| 5 | 15–29 | 15–29 | 15–29 | 15–29 | 15–29 | 15–29 | 5 | 5 | 9 | 9 | 9 | 8 | 8 | 8 | 8 | 8 |
| 6 | 13 | 22 | 32 | 23 | 33 | 23–38 | 59 | 69–735 | 15 | 23 | 33 | 24 | 34 | 24–39 | 19 | 18 |
| 7 | — | 13 | 23 | 14 | 24 | 14–29 | 8 | 111–124 | 18–183 | 34 | 44 | 35 | 45 | 35–50 | 25–115 | 22–97 |
| 8 | 8 | 18 | 28 | 19 | 29 | 19–34 | 8 | — | 13 | 15 | 15 | 15 | 15 | 15 | 15 | 15 |
| 9 | 12 | 22 | 32 | 23 | 33 | 23–38 | 8 | — | 17 | 19 | 19 | 19 | 19 | 19 | 19 | 19 |
| A | 10 | 18 | 28 | 19 | 29 | 19–34 | 8 | — | 13 | 15 | 15 | 15 | 15 | 15 | 15 | 15 |
| B | 14 | 22 | 32 | 23 | 33 | 23–38 | 10 | — | 17 | 19 | 19 | 19 | 19 | 19 | 19 | 19 |
| C | 8 | 18 | 28 | 19 | 29 | 19–34 | 10 | — | 13 | 15 | 15 | 15 | 15 | 15 | 15 | 15 |
| D | 12 | 22 | 32 | 23 | 33 | 23–38 | 13–28 | 13 | 17 | 19 | 19 | 19 | 19 | 19 | 19 | 19 |
| E | 11 | 18 | 28 | 19 | 29 | 19–34 | 16 | — | 13 | 15 | 15 | 15 | 15 | 15 | 15 | 15 |
| F | 15 | 22 | 32 | 23 | 33 | 23–38 | 35–51 | 41–2336 | 17 | 19 | 19 | 19 | 19 | 19 | 19 | 19 |

The 11,126 trials cover all primary opcode bytes, the valid base registers,
displacement, indirection and update fields for ordinary indexed instructions,
all register descriptors for the byte and word ALU, and each assembler-legal
push/pop range. The extended page, DMA, block and multiple-precision families
use their named suboperations. The block and multiple-precision samples use
representative address and length combinations. The TSV marks the control
transfers seen for page suboperation 5. The 18-cycle DIV observation is
the zero-divisor path. The `F7` samples use A = 1, 2, 16 and 256.

## Costs that affect code generation

| SDCC assembler form | Bytes | Microcycles | Decision |
| --- | ---: | ---: | --- |
| `add a,b` (AAB) / `add b,b` | 1 / 2 | 9 / 15 | Reuse the implicit operand pair when it already holds the values. |
| `sla` / `slr b,1` | 1 / 2 | 8 / 20 | Use the A accumulator form for a one-bit shift when A holds the value. |
| `inr b,1` through `inr b,16` | 2 | 13 | A single INR covers the whole literal range at the same cost. |
| `inr (address)` / `inr 2(x)` | 4 / 4 | 34 / 35 | Consider an in-memory update when the register result is dead. |
| `lda (a)` / `lda (x)` / `lda 2(x)` | 1 / 1 / 3 | 17 / 19 / 27 | Both base choice and displacement affect the cost. |
| `lda (x+)` / `lda (-x)` | 2 / 2 | 25 / 24 | Combine the load with an existing pointer update of the correct width. |
| `lda (x)` followed by `xfr a,y` / `ldw y,(x)` | 2 / 4 | 27 / 29 | The compact pair wins when clobbering A is allowed. |
| `lda (x)` followed by AAB / `add 0(x),b` | 2 / 4 | 28 / 29 | The fold becomes worthwhile with displacement or a different register assignment. |
| `bz` fall-through / taken | 2 | 9 / 16 | Account for the direction of the frequent path. |
| `bnz` fall-through / taken | 2 | 10 / 17 | Inverting a branch changes both paths' costs. |
| `jmp label` / `bra label` | 3 / 2 | 13 / 14 | Relative relaxation trades one cycle for one byte. |
| `jsr label` / `bsr label` / `rsr` | 3 / 2 / 1 | 34 / 35 / 21 | Include return and register saves when comparing call sequences. |
| `push yh,zl` / `pop yh,zl` | 2 / 2 | 43 / 37 | Saving contiguous Y and Z shares the range instruction's fixed cost. |
| `mul 3,b` | 4 | 116 | A replacement shift/add chain must also fit the four-byte budget if size must be preserved. |

The backend's `cpu6instructionSize` estimates branch spans and uses a
conservative upper bound for unknown assembly. The switch dispatch costs in
`cpu6_port.jumptableCost` are byte estimates for full sequences. For a
word already loadable into A, multiplication by 2ⁿ ± 1 can use XAB, a
shift and AAB or SAB. The four-byte sequence takes at most 107 microcycles
for the supported shifts, including 8 for XAB and 9 for AAB or SAB.
Immediate MUL takes 116 microcycles and occupies four bytes.
[Branch relaxation](../swext/sdcc/sdcc/src/cpu6/peep.c)
· [Port costs](../swext/sdcc/sdcc/src/cpu6/main.c)
· [Instruction selection](../swext/sdcc/sdcc/src/cpu6/gen.c)

## Encoding and branch-span checks

The survey exercises each named suboperation in the disassembler's page
family (`2E`, selectors 0–5), DMA family (`2F`, 0–9), multi-precision
family (`46`, 0–9) and both block families (`47` and `67`, 0–9). It
includes all four word/byte and read/write selectors of `F6`. The TSV
includes a sample of each inter-level register transfer (`D7`, `E6`).
The suboperation names describe encodings. Privileged and I/O timings
refer to the specific reset configuration used by the probe.

A separate check compared the backend's span estimates with 215,573
assembled instruction lines from the BASIC build, wide-arithmetic tests,
sieve and targeted assembly fixtures. Every estimate covered the actual
encoded length. This includes relaxable `bra`, spaced autoindex operands and
assembler symbols such as `cf` that are also internal peephole flag names.

For a relative target outside the signed displacement range, the bundled
assembler expands `bra` to the three-byte absolute jump. Its `bsr`
pseudo-instruction also expands to opcode `71` (JMP), which loses the
return link. SDCC emits `jsr` for calls to distant functions. Handwritten
assembly should likewise use `jsr` for those calls.
[Assembler branch relaxation](../swext/fuzix-bintools/as1-ee200.c)
