#!/usr/bin/env python3
"""Exercise inline semantics and the opt-out on both independent CPU cores."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
work = Path(tempfile.mkdtemp(prefix='sdcc-inline-tests-', dir=root/'build/out/tests'))
print('Results:', work, flush=True)
source = root/'tests/cpu6-inline.c'
subprocess.run(['cc', '-std=c99', '-O1', '-fsanitize=address,undefined', source, '-o', work/'host'], check=True)
subprocess.run([work/'host'], check=True)
manifest = []
for name, flags in [('speed', ['-O1']), ('disabled', ['-O1', '--sdcc-option=--cpu6-no-auto-inline']),
                    ('stack', ['-O0']), ('no-peep', ['-O1', '--no-peep']),
                    ('size', ['-O1', '--sdcc-option=--opt-code-size'])]:
    binary = work/(name+'.bin')
    with (work/(name+'-build.log')).open('w') as log:
        subprocess.run([root/'run/cpu6-bare-sdcc', *flags, source, '--save-temps', work/name, '-o', binary],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    asm = (work/name/'unit2.s').read_text()
    def body(function):
        return re.search(r'^_'+function+r':\n(.*?)(?=^_\w+:|\Z)', asm, re.M|re.S).group(1)
    # Explicit inline ignores the heuristic budget and its opt-out switch.
    assert not re.search(r'\bjsr ', body('invoke_forced')), name
    has_call = bool(re.search(r'\bjsr ', body('invoke_leaf')))
    assert has_call == (name in ('disabled', 'size')), (name, body('invoke_leaf'))
    # Out-of-line static identity and indirect dispatch must survive.
    assert re.search(r'\b(?:jsr|jmp) _tick', body('third')), name
    assert 'jsr _target' not in body('indirect'), name
    output = subprocess.check_output([root/'tests/centurion-cli', '-A', '2000', '-T', '100000000', '-b', binary], text=True, timeout=120)
    (work/(name+'.log')).write_text(output)
    assert 'PASS inline' in output and 'FAIL' not in output and 'System halted' in output, (name, output)
    manifest.append(binary.name)
    print('PASS instruction core:', name, flush=True)
(work/'manifest.json').write_text(json.dumps(manifest))
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node', root/'tests/compiler-audit-core.js', work, work/'manifest.json'],
                   env=dict(os.environ, CPU6_MICROSTEP_LIMIT='300000000'), stdout=log, stderr=subprocess.STDOUT, check=True, timeout=300)
print('PASS inline configurations on both CPU cores')
(root/'build/out/tests/latest-sdcc-inline-tests.txt').write_text(str(work)+'\n')
