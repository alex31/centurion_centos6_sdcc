#!/usr/bin/env python3
"""Differential CPU6 arithmetic tests: real C decoder versus independent microcode.

Default: complete 16-bit operand sweeps against boundary divisors/multipliers,
all 8-bit pairs, all mode/flag/context combinations, and deterministic random
cases. This is not the full 2**32 Cartesian product of 16-bit operands.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--quick', action='store_true', help='omit the complete 16-bit sweeps')
parser.add_argument('--keep', action='store_true', help='retain matching raw inputs and outputs')
parser.add_argument('--cartesian16', action='store_true', help='test the complete word-pair space (very long)')
parser.add_argument('--first-right', type=int, default=0, help='first right operand for --cartesian16')
parser.add_argument('--last-right', type=int, default=65535, help='last right operand for --cartesian16')
args = parser.parse_args()
if not 0 <= args.first_right <= args.last_right <= 65535 or args.quick and args.cartesian16:
    parser.error('invalid operand range or conflicting modes')
work = Path(tempfile.mkdtemp(prefix='native-arithmetic-', dir=root / 'build/out/tests'))
print('Results:', work, flush=True)
host = work / 'instruction-core'
subprocess.run(['cc', '-O2', '-ffunction-sections', '-fdata-sections',
                root / 'tests/native-arithmetic-host.c', '-Wl,--gc-sections', '-o', host], check=True)
record = struct.Struct('>4B11H')
decode = struct.Struct('>8HBBHH')
results = []
rng = random.Random(0x77407840)


def pack(op, mode, flags, level, regs, extension=0x5000, memory=7):
    # Keep the memory operand at ordinary RAM address 0x5000 even when its
    # index register contains a boundary value. Source/destination overlaps
    # remain intact, including C/P and the context-specific register banks.
    if mode & 0x11 == 0x11:
        extension = (0x5000 - regs[mode >> 5]) & 65535
    return record.pack(op, mode, flags, level, *regs, extension, 0x5000, memory)


def run(name, rows):
    source = work / (name + '.input')
    c_output, mc_output = work / (name + '.host'), work / (name + '.micro')
    source.write_bytes(b''.join(rows))
    count = source.stat().st_size // record.size
    began = time.monotonic()
    with source.open('rb') as data, c_output.open('wb') as output:
        subprocess.run([host], stdin=data, stdout=output, check=True, timeout=120)
    with (work / (name + '.log')).open('w') as log:
        subprocess.run(['node', root / 'tests/native-arithmetic-core.js', source, mc_output],
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=300)
    c_bytes, mc_bytes = c_output.read_bytes(), mc_output.read_bytes()
    assert len(c_bytes) == len(mc_bytes) == count * decode.size
    if c_bytes != mc_bytes:
        for offset in range(0, len(c_bytes), decode.size):
            if c_bytes[offset:offset + decode.size] != mc_bytes[offset:offset + decode.size]:
                index = offset // decode.size
                incoming = record.unpack_from(source.read_bytes(), index * record.size)
                raise AssertionError((name, index, 'input', incoming,
                                      'instruction', decode.unpack_from(c_bytes, offset),
                                      'microcode', decode.unpack_from(mc_bytes, offset)))
    results.append({'phase': name, 'cases': count, 'seconds': time.monotonic() - began,
                    'inputSha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                    'outputSha256': hashlib.sha256(mc_bytes).hexdigest(), 'result': 'PASS'})
    (work / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print(f'PASS {name}: {count:,} full-state comparisons ({results[-1]["seconds"]:.2f} s)', flush=True)
    if not args.keep:
        for file in (source, c_output, mc_output):
            file.unlink()


def unary_flags():
    # Exercise the incoming link and overflow independently. The eight-bit
    # and sixteen-bit boundaries differ for both decrement and negate.
    values = [0, 1, 15, 0x7f, 0x80, 0x8f, 0xff, 0x7fff, 0x8000, 0x800f, 0xffff]
    for value in values:
        regs = [0x3456, value, 0x1234, 0, 0, 0xd000, 0, 0]
        for imm in range(16):
            for flags in range(0, 256, 16):
                for op in (0x20, 0x21, 0x22, 0x23):  # BL
                    yield pack(op, 0x30 | imm, flags, 0, regs)
                for op in (0x30, 0x31, 0x32, 0x33):  # B or a memory word
                    yield pack(op, 0x20 | imm, flags, 0, regs)
                    yield record.pack(op, 0x10 | imm, flags, 0, *regs, 0x5000, 0x5000, value)


def word_alu_modes():
    samples = random.Random(0x5055d6)
    for op in (*range(0x50, 0x56), 0xd6):
        for mode in range(256):
            for sample in range(16):
                regs = [samples.randrange(65536) for _ in range(8)]
                extension = samples.randrange(65536) if mode & 0x11 == 0x10 else 0x5000
                yield pack(op, mode, sample << 4, sample, regs, extension, samples.randrange(65536))


def mode_cases():
    for op in (0x77, 0x78):
        for mode in range(256):
            for level in range(16):
                for flags in range(0, 256, 16):
                    yield pack(op, mode, flags, level, [13, 17, 19, 23, 29, 31, 37, 41])


def corners():
    values = [0, 1, 2, 3, 7, 127, 128, 255, 256, 257, 32767, 32768, 32769, 65534, 65535]
    for op in (0x77, 0x78):
        for mode in (0x40, 0x42):
            for a in values:
                for b in values:
                    for flags in range(0, 256, 16):
                        regs = [a, a, b, 0x1234, 0x5678, 0x9abc, 0xdef0, 0x3210]
                        yield pack(op, mode, flags, 0, regs)


def random_modes():
    for op in (0x77, 0x78):
        for mode in range(256):
            for iteration in range(128):
                regs = [rng.randrange(65536) for _ in range(8)]
                extension = rng.randrange(65536) if mode & 0x11 == 0x10 else 0x5000
                yield pack(op, mode, (iteration & 15) << 4, iteration & 15, regs,
                           extension, rng.randrange(65536))


def pairs8():
    for op in (0x77, 0x78):
        for mode in (0x40, 0x42):
            for a in range(256):
                for b in range(256):
                    yield pack(op, mode, ((a + b) & 15) << 4, 0,
                               [a, a, b, 0x1234, 0x5678, 0x9abc, 0xdef0, 0x3210])


def sweep(fixed, reverse):
    for op in (0x77, 0x78):
        for mode in (0x40, 0x42):
            for value in range(65536):
                a, b = (fixed, value) if reverse else (value, fixed)
                yield pack(op, mode, (value & 15) << 4, 0,
                           [a, a, b, 0x1234, 0x5678, 0x9abc, 0xdef0, 0x3210])


run('unary-flags', unary_flags())
run('word-alu-modes', word_alu_modes())
run('modes-flags-contexts', mode_cases())
run('corner-flags', corners())
run('random-modes', random_modes())
run('all-byte-pairs', pairs8())
if args.cartesian16:
    for fixed in range(args.first_right, args.last_right + 1):
        run(f'cartesian-{fixed:04x}', sweep(fixed, False))
elif not args.quick:
    for fixed in (0, 1, 2, 3, 7, 47, 255, 256, 257, 32767, 32768, 32769, 65534, 65535):
        for reverse in (False, True):
            run(f'words-{fixed:04x}-' + ('right' if reverse else 'left'), sweep(fixed, reverse))
total = sum(row['cases'] for row in results)
print(f'PASS: {total:,} full-state comparisons; registers, flags, PC and operand memory agree')
(root / 'build/out/tests/latest-native-arithmetic-tests.txt').write_text(str(work) + '\n')
