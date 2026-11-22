#!/usr/bin/env python3
"""Check every MUL/DIV mode byte, operand aliases, relocations and rejection."""
import argparse
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--assembler', type=Path, default=root / 'build/out/toolchain/bin/asee200')
args = parser.parse_args()
assembler = args.assembler.resolve()
linker = root / 'build/out/toolchain/bin/ldee200'
work = Path(tempfile.mkdtemp(prefix='ee200-native-', dir=root / 'build/out/tests'))
lines = ['.setcpu 6', '.code']
expected = bytearray()
registers = ['a', 'b', 'x', 'y', 'z', 's', 'c', 'p']
for mnemonic, opcode in [('mul', 0x77), ('div', 0x78)]:
    for src, source in enumerate(registers):
        for dst, dest in enumerate(registers):
            lines.extend([f'{mnemonic} {source},{dest}',
                          f'{mnemonic} {source},0x1234,{dest}',
                          f'{mnemonic} {source},(0x5678),{dest}',
                          f'{mnemonic} -2({source}),{dest}'])
            mode = src * 32 + dst * 2
            expected.extend([opcode, mode, opcode, mode | 16, 0x12, 0x34,
                             opcode, mode | 1, 0x56, 0x78,
                             opcode, mode | 17, 0xff, 0xfe])
    for index, reg in enumerate(registers):
        mode = index * 32 + index * 2
        lines.extend([f'{mnemonic} 47,{reg}', f'{mnemonic} (0x4321),{reg}',
                      f'{mnemonic} ({reg}),{reg}'])
        expected.extend([opcode, mode | 16, 0, 47, opcode, mode | 1, 0x43, 0x21,
                         opcode, mode | 17, 0, 0])
source, obj, binary = work / 'all.s', work / 'all.o', work / 'all.bin'
for mnemonic, opcode in [('add', 0x50), ('sub', 0x51), ('and', 0x52),
                         ('ori', 0x53), ('ore', 0x54)]:
    for index, reg in enumerate(registers):
        mode = index * 32 + 2   # source or index register, result in B
        lines.extend([f'{mnemonic} {reg},0x1234,b', f'{mnemonic} {reg},(0x5678),b',
                      f'{mnemonic} -2({reg}),b'])
        expected.extend([opcode, mode | 16, 0x12, 0x34, opcode, mode | 1, 0x56, 0x78,
                         opcode, mode | 17, 0xff, 0xfe])
for instruction, encoding in [
        ('ldw y,0x1234', '55 16 12 34'), ('stw y,(0x1234)', 'd6 07 12 34'),
        ('ldw z,-300(s)', '55 b9 fe d4'), ('stw z,-300(s)', 'd6 b9 fe d4'),
        ('inr (0x5678)', '30 10 56 78'), ('dcr (s),2', '31 b1 00 00'),
        ('push xh,zl', '7e 45'), ('pop zh,zl', '7f 81'), ('push bl,bl', '7e 30'),
        ('jmp @(b)', '75 24'), ('lda @-2(x+)', '95 4d fe'), ('ldb @(0x5678)', 'd2 56 78')]:
    lines.append(instruction)
    expected.extend(bytes.fromhex(encoding))
source.write_text('\n'.join(lines) + '\n')
subprocess.run([assembler, '-o', obj, source], check=True)
subprocess.run([linker, '-b', '-C', '0', '-o', binary, obj], check=True)
assert binary.read_bytes() == expected, 'opcode or operand encoding differs'

source.write_text('''.setcpu 6
.code
.export start
.export factor
start:
mul factor,b
div (factor),a
mul factor+4(x),b
.data
factor:
.word 7
''')
mapping, reloc = work / 'symbols.map', work / 'reloc.bin'
subprocess.run([assembler, '-o', obj, source], check=True)
subprocess.run([linker, '-b', '-C', '8192', '-m', mapping, '-R', reloc, '-o', binary, obj], check=True)
symbols = {s[2]: int(s[0], 16) for line in mapping.read_text().splitlines()
           if len(s := line.split()) == 3}
address = symbols['factor']
data = binary.read_bytes()[0x2000:0x200c]
assert data == (b'\x77\x32' + address.to_bytes(2, 'big') +
                b'\x78\x01' + address.to_bytes(2, 'big') +
                b'\x77\x53' + (address + 4).to_bytes(2, 'big'))
fixups = reloc.read_bytes()
assert {int.from_bytes(fixups[i+1:i+3], 'big') for i in range(0, len(fixups), 3)} == {0x2002, 0x2006, 0x200a}
for index, bad in enumerate(['mul al,b', 'div a,bl', 'mul (al),b', 'mul a,(x)',
                             'ldw a,b', 'stw z,0x1234', '.setcpu 4\nldw x,(0x1234)',
                             'div a,b,x', 'mul 7,7', '.setcpu 4\nmul a,b']):
    source.write_text('.setcpu 6\n.code\n' + bad + '\n')
    result = subprocess.run([assembler, '-o', obj, source], capture_output=True, text=True)
    (work / f'invalid{index}.log').write_text(result.stdout + result.stderr)
    assert result.returncode, f'accepted invalid input: {bad}'
print('PASS: native arithmetic, word loads/stores, register ranges, indirection, relocations and invalid operands')
print('Results:', work)
