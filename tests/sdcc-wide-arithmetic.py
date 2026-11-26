#!/usr/bin/env python3
"""Independent 32-bit vectors, conditional comparisons and ABI checks."""
import json
import os
from pathlib import Path
import random
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
work = Path(tempfile.mkdtemp(prefix='sdcc-wide-tests-', dir=root / 'build/out/tests'))
print('Results:', work, flush=True)
rng = random.Random(0x323248)
mask = (1 << 32) - 1
corners = [0, 1, 2, 47, 65535, 65536, 65537, 0x7fffffff, 0x80000000,
           0xffff0000, 0xfffffffe, mask]
pairs = [(a, b) for a in corners for b in corners if b]
pairs += [(rng.getrandbits(32), rng.getrandbits(32) or 1) for _ in range(40)]
def flags(a, b):
    return sum((1 << i) for i, v in enumerate([a == b, a != b, a < b, a <= b, a > b, a >= b]) if v)
def signed(n):
    return n if n < (1 << 31) else n - (1 << 32)
def literal(n):
    return '0x%08xUL' % (n & mask)

source = '''#include <stdint.h>
#include <stdio.h>
static volatile uint32_t observed;
static volatile unsigned numerator = 60000U, denominator = 301U;
static uint32_t widened_division(void) {
    return (uint32_t)numerator / ((uint32_t)denominator | 1UL);
}
static uint32_t divide_u(uint32_t a, uint32_t b) { return a / b; }
static uint32_t remainder_u(uint32_t a, uint32_t b) { return a % b; }
static int32_t divide_s(int32_t a, int32_t b) { return a / b; }
static int32_t remainder_s(int32_t a, int32_t b) { return a % b; }
static unsigned compare_u(uint32_t a, uint32_t b) {
    unsigned r = 0;
    if (a == b) r |= 1;
    if (a != b) r |= 2;
    if (a < b) r |= 4;
    if (a <= b) r |= 8;
    if (a > b) r |= 16;
    if (a >= b) r |= 32;
    return r;
}
static unsigned compare_s(int32_t a, int32_t b) {
    unsigned r = 0;
    if (a == b) r |= 1;
    if (a != b) r |= 2;
    if (a < b) r |= 4;
    if (a <= b) r |= 8;
    if (a > b) r |= 16;
    if (a >= b) r |= 32;
    return r;
}
static uint32_t increment(uint32_t a) { return a + 1UL; }
static uint32_t add_two(uint32_t a) { return a + 2UL; }
static uint32_t subtract_one(uint32_t a) { return a - 1UL; }
static uint32_t subtract_large(uint32_t a) { return a - 65535UL; }
static uint32_t add(uint32_t a, uint32_t b) { return a + b; }
static uint32_t subtract(uint32_t a, uint32_t b) { return a - b; }
static uint32_t multiply(uint32_t a, uint32_t b) { return a * b; }
static int mixed(int32_t a) { return a < -1L; }
static unsigned compare_constants(uint32_t a) {
    unsigned r = 0;
    if (a == 0x00010000UL) r |= 1;
    if (a != 0xffff0000UL) r |= 2;
    if (a < 0x80000000UL) r |= 4;
    if (a <= 0x00010000UL) r |= 8;
    if (a > 0x0000ffffUL) r |= 16;
    if (a >= 0xfffffffeUL) r |= 32;
    if ((int32_t)a == -1L) r |= 64;
    return r;
}
struct unsigned_case { uint32_t a,b,q,r,sum,difference,product,inc,add2,dec,sub65535; unsigned comparison,constants; };
static const struct unsigned_case unsigned_cases[] = {
'''
for a,b in pairs:
    constants = sum(1 << i for i, match in enumerate((
        a == 0x10000, a != 0xffff0000, a < 0x80000000, a <= 0x10000,
        a > 0xffff, a >= 0xfffffffe, signed(a) == -1)) if match)
    source += '{' + ','.join(map(literal, [a,b,a//b,a%b,(a+b)&mask,(a-b)&mask,(a*b)&mask,
                                         (a+1)&mask,(a+2)&mask,(a-1)&mask,(a-65535)&mask])) + f',{flags(a,b)},{constants}}},\n'
source += '};\nstruct signed_case { int32_t a,b,q,r; unsigned comparison; };\nstatic const struct signed_case signed_cases[] = {\n'
for ua,ub in pairs:
    a,b = signed(ua),signed(ub)
    if a == -(1 << 31) and b == -1: continue
    q = abs(a)//abs(b) * (-1 if (a<0) != (b<0) else 1)
    r = a-q*b
    source += '{' + ','.join(map(literal, [a,b,q,r])) + ',' + str(flags(a,b)) + '},\n'
source += '''};
int main(void) {
    unsigned i;
    if (widened_division() != 199UL) { puts("FAIL narrowed division"); return 4; }
    for (i=0; i<sizeof(unsigned_cases)/sizeof(unsigned_cases[0]); i++) {
        const struct unsigned_case *p = &unsigned_cases[i];
        uint32_t a = p->a, b = p->b;
        if (divide_u(a,b) != p->q || remainder_u(a,b) != p->r ||
            add(a,b) != p->sum || subtract(a,b) != p->difference ||
            multiply(a,b) != p->product || compare_u(a,b) != p->comparison ||
            compare_constants(a) != p->constants ||
            increment(a) != p->inc || add_two(a) != p->add2 ||
            subtract_one(a) != p->dec || subtract_large(a) != p->sub65535) {
            puts("FAIL unsigned wide arithmetic"); return 1;
        }
        observed = a;
        if (observed != a) { puts("FAIL volatile long"); return 2; }
    }
    for (i=0; i<sizeof(signed_cases)/sizeof(signed_cases[0]); i++) {
        const struct signed_case *p = &signed_cases[i];
        if (divide_s(p->a,p->b) != p->q || remainder_s(p->a,p->b) != p->r ||
            compare_s(p->a,p->b) != p->comparison || mixed(p->a) != (p->a < -1L)) {
            puts("FAIL signed wide arithmetic"); return 3;
        }
    }
    puts("PASS wide arithmetic"); return 0;
}
'''
(work / 'program.c').write_text(source)
manifest = []
for name, options in [('speed', ['-O1']), ('stack', ['-O0']),
                      ('size', ['-O1', '--sdcc-option=--opt-code-size']),
                      ('no-peep', ['-O1', '--no-peep']),
                      ('software', ['-O1', '--sdcc-option=--cpu6-no-wide-native'])]:
    binary = work / (name + '.bin')
    with (work / (name + '-build.log')).open('w') as log:
        subprocess.run([root / 'run/cpu6-bare-sdcc', *options, '--origin', '0200',
                        work / 'program.c', '--save-temps', work / name, '-o', binary],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    output = subprocess.check_output([root / 'tests/centurion-cli', '-A', '0200',
                                     '-T', '100000000', '-b', binary], text=True, timeout=120)
    (work / (name + '.log')).write_text(output)
    assert 'PASS wide arithmetic' in output and 'FAIL' not in output and 'System halted' in output, name
    manifest.append(binary.name)
    print('PASS instruction core:', name, flush=True)
(work / 'manifest.json').write_text(json.dumps(manifest))
with (work / 'microcode.log').open('w') as log:
    subprocess.run(['node', root / 'tests/compiler-audit-core.js', work, work / 'manifest.json'],
                   env=dict(os.environ, CPU6_BARE_ORIGIN='0200', CPU6_MICROSTEP_LIMIT='300000000'),
                   stdout=log, stderr=subprocess.STDOUT, check=True, timeout=300)
print('PASS all wide-arithmetic variants on both CPU cores')
(root / 'build/out/tests/latest-sdcc-wide-tests.txt').write_text(str(work) + '\n')
