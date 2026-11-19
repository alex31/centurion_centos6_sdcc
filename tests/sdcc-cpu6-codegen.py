#!/usr/bin/env python3
"""Check volatile accesses, branch range, zero tests and safe CPU6 tail calls."""
from pathlib import Path
import json
import re
import subprocess

root = Path(__file__).resolve().parent.parent
work = root / 'build/out/tests/sdcc-cpu6/codegen-tests'
work.mkdir(parents=True, exist_ok=True)
(work / 'targets.c').write_text('''int *escaped;
int target_zero(void) { return 41; }
int target_argument(int x) { return x + 1; }
int target_escape(void) { return *escaped; }
''')
source = '''#include <stdio.h>
extern int *escaped;
int target_zero(void);
int target_argument(int);
int target_escape(void);
volatile unsigned int observed;
int tail_direct(void) { return target_zero(); }
int tail_indirect(int (*p)(void)) { return p(); }
int tail_frame(int x) {
    volatile int keep = x;
    if (keep < 0) return 7;
    return target_zero();
}
int must_keep_frame(void) {
    int local = 73;
    escaped = &local;
    return target_escape();
}
int tail_argument(int x) { return target_argument(x); }
unsigned int readback(unsigned int x) { observed = x; return observed; }
int word_condition(unsigned int a, unsigned int b) {
    if (a + b) return 7;
    return 3;
}
int byte_condition(unsigned int a, unsigned int b) {
    if ((unsigned char)(a + b)) return 7;
    return 3;
}
int long_jump(int yes) {
    if (yes) {
'''
source += ''.join(f'observed = {i};\n' for i in range(1, 121))
source += '''return 41;
    }
    return 17;
}
int main(void) {
    if (tail_direct() != 41 || tail_indirect(target_zero) != 41 ||
        tail_frame(2) != 41 || tail_frame(-2) != 7 || must_keep_frame() != 73 ||
        tail_argument(12) != 13 || readback(65535U) != 65535U ||
        word_condition(65535U, 1U) != 3 || word_condition(255U, 1U) != 7 ||
        byte_condition(255U, 1U) != 3 || byte_condition(255U, 0U) != 7 ||
        long_jump(0) != 17 || long_jump(1) != 41 || observed != 120) {
        puts("FAIL code generation");
        return 1;
    }
    puts("PASS code generation");
    return 0;
}
'''
(work / 'program.c').write_text(source)
manifest = []
sizes = {}
for name, flags in [('stack', ['-O0']), ('optimized', ['-O1']), ('no-peep', ['-O1', '--no-peep'])]:
    binary = work / (name + '.bin')
    temps = work / name
    with (work / (name + '.log')).open('w') as log:
        subprocess.run([root / 'run/cpu6-bare-sdcc', *flags, work / 'program.c',
                        work / 'targets.c', '-o', binary, '--save-temps', temps],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
        output = subprocess.check_output([root / 'tests/centurion-cli', '-T', '2000000',
                                          '-A', '2000', '-b', binary], text=True)
        log.write(output)
        assert 'PASS code generation' in output and 'System halted' in output, output
    sizes[name] = binary.stat().st_size
    manifest.append(binary.name)
    if name == 'optimized':
        assembly = (temps / 'unit2.s').read_text()
        def body(function):
            return re.search(r'^_' + function + r':\n(.*?)(?=^_\w+:|\Z)',
                             assembly, re.M | re.S).group(1)
        assert 'jmp _target_zero' in body('tail_direct')
        assert 'jsr _target_escape' in body('must_keep_frame')
        assert 'jmp _target_argument' in body('tail_argument')
        assert re.search(r'\bldb\s+\(_observed', body('readback'))
        assert re.search(r'\bstb\s+\(_observed', body('readback'))
    print('PASS CPU6 code generation:', name)
assert sizes['optimized'] < sizes['no-peep'], sizes
(work / 'manifest.json').write_text(json.dumps(manifest))
with (work / 'microcode.log').open('w') as log:
    subprocess.run(['node', root / 'tests/compiler-audit-core.js', work, work / 'manifest.json'],
                   stdout=log, stderr=subprocess.STDOUT, check=True, timeout=60)
print('PASS microcoded core for all code generation variants')
print('Binary sizes:', sizes)
