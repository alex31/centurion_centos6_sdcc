#!/usr/bin/env python3
"""Check CPU6 inline assembly and C operands on both emulator cores."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
installed = root / 'build/out/toolchain/sdcc-cpu6/bin/sdcc'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', type=Path, default=installed)
args = parser.parse_args()
compiler = args.compiler.resolve()
env = dict(os.environ, PATH=str(installed.parent) + os.pathsep + os.environ['PATH'])

source = r'''
#include <stdio.h>
volatile unsigned observed;

static inline unsigned next_word(unsigned v) {
    return __cpu6_asm16("inr b", v);
}

static unsigned subtract(unsigned first, unsigned second) {
    return __cpu6_asm16_2("sab", first, second);
}

int main(void) {
    unsigned char data[4] = {42, 0x12, 0x34, 60};
    unsigned a, b, sum;
    observed = 4;
    a = observed;
    b = a + 27;
    __asm
        lda 29
        sta (_observed)
    __endasm;
    sum = __cpu6_asm16_2("aab", a++, b++);
    if (sum != 35 || a != 5 || b != 32 || observed != 29) goto fail;
    if (subtract(b, a) != 27 || next_word(a) != 6 || next_word(b) != 33)
        goto fail;
    if (__cpu6_asm16("inr b\ninr b", next_word(a)) != 8) goto fail;
    if (__cpu6_asm16("ldb (b)", (unsigned)(data + 1)) != 0x1234) goto fail;
    __cpu6_asm16_2("stb (a)", (unsigned)(data + 1), 0x80fe);
    if (data[0] != 42 || data[1] != 0x80 || data[2] != 0xfe || data[3] != 60)
        goto fail;
    __cpu6_asm16_2("stb (a)", (unsigned)&observed, 41);
    if (observed != 41) goto fail;
    puts("PASS CPU6 inline assembly");
    return 0;
fail:
    puts("FAIL CPU6 inline assembly");
    return 1;
}
'''

with tempfile.TemporaryDirectory(prefix='cpu6-inline-asm-') as temp:
    work = Path(temp)
    program = work / 'probe.c'
    assembly = work / 'probe.s'
    base = [compiler, '-mcpu6', '-S', '--std=c99', '-I' + str(root / 'baremetal/include')]
    program.write_text(source)
    binaries = []
    for name, flags in [('stack', ['--cpu6-stack-only']), ('speed', ['--opt-code-speed']),
                        ('size', ['--opt-code-size']), ('no-peep', ['--no-peep'])]:
        subprocess.run([*base, *flags, program, '-o', assembly], env=env, check=True)
        code = assembly.read_text()
        assert 'jsr ___cpu6_asm' not in code and re.search(r'\bldb\s+\(b\)', code), code
        binary = work / (name + '.bin')
        subprocess.run([root / 'run/cpu6-bare-sdcc', assembly, '-o', binary], check=True)
        output = subprocess.check_output([root / 'tests/centurion-cli', '-T', '2000000',
                                          '-A', '2000', '-b', binary], text=True)
        assert 'PASS CPU6 inline assembly' in output and 'System halted' in output, output
        binaries.append(binary.name)

    cases = [
        ('__cpu6_asm16(text, 7)', 'requires a string literal'),
        (r'__cpu6_asm16("nop\0nop", 7)', 'requires a string literal'),
        ('__cpu6_asm16("xfr b,x", 7)', 'requires straight-line A/B'),
        ('__cpu6_asm16("stb (_observed)", 7)', 'requires straight-line A/B'),
        ('__cpu6_asm16("jmp (b)", 7)', 'requires straight-line A/B'),
    ]
    for expression, diagnostic in cases:
        program.write_text(f'unsigned probe(const char *text) {{ return {expression}; }}\n')
        invalid = subprocess.run([*base, program, '-o', assembly], env=env,
                                 text=True, capture_output=True)
        assert invalid.returncode and diagnostic in invalid.stderr, invalid.stderr

    manifest = work / 'manifest.json'
    manifest.write_text(json.dumps(binaries))
    output = subprocess.check_output(['node', root / 'tests/compiler-audit-core.js', work, manifest],
                                     text=True, timeout=60)
    assert output.count(' PASS ') == len(binaries), output

print('PASS CPU6 inline assembly, operands, memory and diagnostics on both cores (four profiles)')
