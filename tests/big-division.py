#!/usr/bin/env python3
"""Compare signed D/DRM (1..8-byte operands) against the microcoded core."""
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
work = Path(tempfile.mkdtemp(prefix='big-division-', dir=root / 'build/out/tests'))
print('Results:', work, flush=True)
rows, descriptions = [], []
rng = random.Random(0x46777a24)
mask = (1 << 64) - 1
for source_size in range(1, 9):
    for dest_size in range(1, 9):
        source_limit, dest_limit = 1 << (8 * source_size - 1), 1 << (8 * dest_size - 1)
        for op in (0x6a, 0x7a):
            pairs = [(n, d) for n in (0, 1, -1, dest_limit - 1, -dest_limit, -dest_limit + 1)
                     for d in (0, 1, -1, source_limit - 1, -source_limit, -source_limit + 1)]
            pairs += [(rng.randrange(-dest_limit, dest_limit), rng.randrange(-source_limit, source_limit))
                      for _ in range(32)]
            for n, d in pairs:
                for flags in (0, 0x50, 0xa0, 0xf0):
                    rows.append(struct.pack('>4BQQ', source_size, dest_size, op, flags, n & mask, d & mask))
                    descriptions.append([source_size, dest_size, op, flags, n, d])
source = work / 'cases.bin'
source.write_bytes(b''.join(rows))
binary = work / 'instruction-core'
subprocess.run(['cc', '-O2', '-ffunction-sections', '-fdata-sections',
                root / 'tests/big-division-host.c', '-Wl,--gc-sections', '-o', binary], check=True)
with source.open('rb') as inp, (work / 'instruction.bin').open('wb') as out:
    subprocess.run([binary], stdin=inp, stdout=out, check=True)
with (work / 'microcode.log').open('w') as log:
    subprocess.run(['node', root / 'tests/big-division-core.js', source, work / 'microcode.bin'],
                   stdout=log, stderr=subprocess.STDOUT, check=True, timeout=240)
a, b = (work / 'instruction.bin').read_bytes(), (work / 'microcode.bin').read_bytes()
assert len(a) == len(b) == len(rows) * 44
for i, description in enumerate(descriptions):
    assert a[i * 44:(i + 1) * 44] == b[i * 44:(i + 1) * 44], (i, description,
              a[i * 44:(i + 1) * 44].hex(), b[i * 44:(i + 1) * 44].hex())
(work / 'results.json').write_text(json.dumps({'result': 'PASS', 'cases': len(rows),
    'inputSha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'outputSha256': hashlib.sha256(a).hexdigest()}, indent=2) + '\n')
(root / 'build/out/tests/latest-big-division-tests.txt').write_text(str(work) + '\n')
print('PASS:', len(rows), 'signed D/DRM full-state comparisons')
