#!/usr/bin/env python3
"""Exercise terminal void state calls, argument snapshots and rejected cases."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
work = Path(tempfile.mkdtemp(prefix='sdcc-tail-tests-', dir=root / 'build/out/tests'))
print('Results:', work, flush=True)
large = work / 'large.c'
source = '#include <stdint.h>\nextern volatile uint16_t large_seen;\nvoid observe_stack(void);\n'
source += 'void large_state(uint16_t n,uint16_t a,uint16_t b) {\n'
source += ''.join(f'volatile uint16_t v{i} = a + {i}U;\n' for i in range(80))
source += '''observe_stack();
if (!n) { large_seen = v0 * 3U + b; return; }
large_state(n - 1,b,a);
}
'''
large.write_text(source)
many = work / 'many.c'
parameters = ',\n'.join(f'uint16_t a{i}' for i in range(66))
rotated = ','.join(['a65'] + [f'a{i}' for i in range(65)])
arguments = ','.join(str(i + 1) for i in range(66))
many.write_text('''#include <stdint.h>
#include <stdio.h>
void observe_stack(void);
static volatile uint16_t many_result;
''' + 'void many_state(uint16_t n,\n' + parameters + ') {\nobserve_stack();\n'
    + 'if (!n) { many_result = a0 + a65; return; }\n'
    + 'many_state(n-1,' + rotated + ');\n}\n'
    + 'int many_state_test(void) {\n'
    + '#ifdef EXPECT_TAIL\n' + 'many_state(67,' + arguments + ');\n'
    + 'return many_result == 131;\n'
    + '#else\n' + 'many_state(2,' + arguments + ');\n'
    + 'return many_result == 129;\n'
    + '#endif\n}\n')
manifest = []
variants = [('stack', ['-O0'], False), ('disabled', ['-O1', '--sdcc-option=--cpu6-no-tail-recursion'], False),
            ('optimized', ['-O1'], True), ('no-peep', ['-O1', '--no-peep'], True),
            ('size', ['-O1', '--sdcc-option=--opt-code-size'], True)]
for name, flags, optimized in variants:
    binary = work / (name + '.bin')
    temps = work / name
    if optimized: flags += ['-DEXPECT_TAIL', '-DTAIL_DEEP=40001U', '-DTAIL_WALK=1024U']
    with (work / (name + '-build.log')).open('w') as log:
        subprocess.run([root / 'run/cpu6-bare-sdcc', *flags,
                        root / 'tests/cpu6-tail-recursion.c', large, many, root / 'tests/cpu6-read-stack.s',
                        '-o', binary, '--save-temps', temps],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    assembly = (temps / 'unit2.s').read_text()
    def body(function):
        return re.search(r'^_' + function + r':\n(.*?)(?=^_\w+:|\Z)', assembly, re.M | re.S).group(1)
    for function in ['rotate_state', 'mixed_state', 'pointer_state', 'evaluated_state']:
        assert ('jsr _' + function in body(function)) != optimized, (name, function)
    for function in ['escaping_local', 'escaping_parameter', 'nonterminal', 'variadic_state']:
        assert 'jsr _' + function in body(function), (name, function)
    for unit, function in [('unit3.s', 'large_state'), ('unit4.s', 'many_state')]:
        assembly = (temps / unit).read_text()
        assert ('jsr _' + function in body(function)) != optimized, (name, function)
    output = subprocess.check_output([root / 'tests/centurion-cli', '-T', '100000000', '-A', '2000', '-b', binary], text=True)
    (work / (name + '.log')).write_text(output)
    assert 'PASS tail recursion' in output and 'FAIL' not in output and 'System halted' in output, (name, output)
    print('PASS instruction core:', name, flush=True)
    manifest.append(binary.name)
(work / 'manifest.json').write_text(json.dumps(manifest))
with (work / 'microcode.log').open('w') as log:
    subprocess.run(['node', root / 'tests/compiler-audit-core.js', work, work / 'manifest.json'],
                   env=dict(os.environ, CPU6_MICROSTEP_LIMIT='200000000'),
                   stdout=log, stderr=subprocess.STDOUT, check=True, timeout=240)
rows = json.loads((work / 'system-results.json').read_text())
assert all('FAIL' not in row['serial'] for row in rows)
print('PASS all tail-recursion variants on both CPU cores')
(root / 'build/out/tests/latest-sdcc-tail-tests.txt').write_text(str(work) + '\n')
