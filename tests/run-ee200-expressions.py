#!/usr/bin/env python3
"""Check EE200 assembler expression operators added upstream."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
assembler = root / 'build/out/toolchain/bin/asee200'
linker = root / 'build/out/toolchain/bin/ldee200'
with tempfile.TemporaryDirectory(prefix='cpu6-expressions-') as temp:
    work = Path(temp)
    source = work / 'expr.s'
    source.write_text('''.setcpu 6
.code
.export start
start:
.byte 1|2&4
.byte 1|2+4
.word 0x1234&0x00ff|0x0800
.byte 3*4&15
.byte 3*4 + 1
.byte 3 * 4 + 1
.byte 24 / 3 * 2 - 1
.byte 1 | 2 & 4
.byte [3 * 4 + 1] * 2 - 5
.word 0x1234 & 0x00ff | 0x0800
.byte 3*4\t+\t1 ; comment after a spaced expression
''')
    obj, image = work / 'expr.o', work / 'expr.bin'
    subprocess.run([assembler, '-o', obj, source], check=True)
    subprocess.run([linker, '-b', '-C', '0', '-o', image, obj], check=True)
    if image.read_bytes() != bytes.fromhex('01 07 08 34 0c 0d 0d 0f 01 15 08 34 0d'):
        raise SystemExit('incorrect EE200 expression result or precedence')
    invalid = work / 'invalid.s'
    invalid.write_text('.setcpu 6\n.code\nstart:\n.word start&0xff\n')
    result = subprocess.run([assembler, '-o', work/'invalid.o', invalid],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if result.returncode == 0 or 'must be absolute' not in result.stdout:
        raise SystemExit('assembler accepted a non-absolute bitwise expression')
print('PASS: EE200 bitwise expression operators and precedence')
