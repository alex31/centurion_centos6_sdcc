#!/usr/bin/env python3
"""Check absolute EE200 output and the relocations used by CENTOS CBIN."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
assembler = root / 'build/out/toolchain/bin/asee200'
linker = root / 'build/out/toolchain/bin/ldee200'


def run(*args, success=True):
    result = subprocess.run([str(arg) for arg in args], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    if (result.returncode == 0) != success:
        raise RuntimeError(f'{args}: exit {result.returncode}\n{result.stdout}')
    return result.stdout


with tempfile.TemporaryDirectory(prefix='cpu6-relocations-') as temporary:
    work = Path(temporary)
    code, data = work / 'code.s', work / 'data.s'
    code.write_text('''\
.setcpu 6
.code
.export start
start:
.word target
.word local
local:
.word 0x1234
.word 0x5678
''')
    data.write_text('''\
.setcpu 6
.data
.export target
target:
.word 0xabcd
''')
    for source in (code, data):
        run(assembler, '-o', source.with_suffix('.o'), source)
    image, relocs = work / 'image.bin', work / 'image.rel'
    run(linker, '-b', '-C', '512', '-R', relocs, '-o', image,
        code.with_suffix('.o'), data.with_suffix('.o'))
    linked = bytearray(image.read_bytes())
    if any(linked[:512]) or linked[512:] != bytes.fromhex('02 08 02 04 12 34 56 78 ab cd'):
        raise AssertionError('unexpected linked image')
    if relocs.read_bytes() != bytes.fromhex('01 02 00 01 02 02'):
        raise AssertionError('unexpected CBIN relocation records')
    for address in (512, 514):
        value = int.from_bytes(linked[address:address + 2], 'big') + 0x1200
        linked[address:address + 2] = value.to_bytes(2, 'big')
    if linked[512:] != bytes.fromhex('14 08 14 04 12 34 56 78 ab cd'):
        raise AssertionError('relocation produced incorrect pointers')

    byte_source = work / 'byte.s'
    byte_source.write_text('''\
.setcpu 6
.code
.export start
start:
.byte target
target:
.byte 0
''')
    byte_obj = byte_source.with_suffix('.o')
    run(assembler, '-o', byte_obj, byte_source)
    output = run(linker, '-b', '-C', '0', '-R', work / 'byte.rel',
                 '-o', work / 'byte.bin', byte_obj, success=False)
    if 'absolute-image relocation is not a big-endian word' not in output:
        raise AssertionError('one-byte relocation was not rejected as expected')
    plain = work / 'byte-plain.bin'
    run(linker, '-b', '-C', '0', '-o', plain, byte_obj)
    if plain.read_bytes() != bytes.fromhex('01 00'):
        raise AssertionError('linking without relocation export changed')

print('PASS: CBIN word fixups, invalid byte fixup and plain binary linking')
