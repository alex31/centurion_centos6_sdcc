#!/usr/bin/env python3
"""Native arithmetic through SDCC: edge cases, code selection and signed sweeps."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--quick', action='store_true', help='omit the complete signed sweeps')
args = parser.parse_args()
work = Path(tempfile.mkdtemp(prefix='sdcc-native-tests-', dir=root / 'build/out/tests'))
print('Results:', work, flush=True)
manifest = []
variants = {'stack': ['-O0'], 'speed': ['-O1'], 'size': ['-O1', '--sdcc-option=--opt-code-size'],
            'no-peep': ['-O1', '--no-peep']}
for name, flags in variants.items():
    for suffix, source, extra in [('edges', 'os6-native-arithmetic.c', []),
                                   ('signed', 'sdcc-native-arithmetic.c', ['-DCPU6_NATIVE_SWEEP_MAX=256U'])]:
        binary = work / f'{name}-{suffix}.bin'
        temps = work / f'{name}-{suffix}'
        subprocess.run([root / 'run/cpu6-bare-sdcc', *flags, *extra, root / 'tests' / source,
                        '-o', binary, '--save-temps', temps], check=True)
        assembly = (temps / 'unit2.s').read_text()
        assert '\tdiv ' in assembly, binary
        if not (name == 'size' and suffix == 'edges'):
            assert '\tmul ' in assembly, binary
        manifest.append(binary.name)
if not args.quick:
    binary = work / 'signed-exhaustive.bin'
    subprocess.run([root / 'run/cpu6-bare-sdcc', '-O1', root / 'tests/sdcc-native-arithmetic.c',
                    '-o', binary], check=True)
    manifest.append(binary.name)
for name in manifest:
    output = subprocess.check_output([root / 'tests/centurion-cli', '-T', '1000000000',
                                      '-A', '2000', '-b', work / name], text=True, timeout=300)
    (work / (name + '.log')).write_text(output)
    assert 'PASS' in output and 'FAIL' not in output and 'System halted' in output, name
    print('PASS instruction core:', name, flush=True)
(work / 'manifest.json').write_text(json.dumps(manifest))
with (work / 'microcode.log').open('w') as log:
    subprocess.run(['node', root / 'tests/compiler-audit-core.js', work, work / 'manifest.json'],
                   env=dict(os.environ, CPU6_MICROSTEP_LIMIT='3000000000'),
                   stdout=log, stderr=subprocess.STDOUT, check=True, timeout=600)
print('PASS all SDCC variants on both CPU cores')
(root / 'build/out/tests/latest-sdcc-native-tests.txt').write_text(str(work) + '\n')
