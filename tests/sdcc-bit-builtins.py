#!/usr/bin/env python3
"""Compare GCC bit builtins with Python integer oracles on both CPU6 models."""
from pathlib import Path
import random
import subprocess
import tempfile

from cpu6_roadmap_support import ROOT, execute


TYPES = {
    8: ('unsigned char', 'signed char'),
    16: ('unsigned int', 'int'),
    32: ('unsigned long', 'long'),
    48: ('unsigned _BitInt(48)', '_BitInt(48)'),
    64: ('unsigned long long', 'long long'),
}


def vector_source(bits):
    unsigned, signed = TYPES[bits]
    mask = (1 << bits) - 1
    rng = random.Random(0xc600 + bits)
    values = [0, mask, 1, mask-1, mask//2, mask//2+1, mask//2+2]
    values += [1 << n for n in range(1, bits, 5)]
    values += [rng.getrandbits(bits) for _ in range(12)]

    def literal(x):
        return f'(U)0x{x:x}ULL'

    rows = []
    for i, x in enumerate(values):
        count = (0, 1, bits-1, bits, bits+1, 8, 17, 255)[i % 8]
        k = count % bits
        inv = x ^ mask
        clz = lambda value: bits - value.bit_length()
        ctz = lambda value: (value & -value).bit_length()-1 if value else bits
        ceil = 1 if x < 2 else (1 << (x-1).bit_length()) & mask
        floor = 1 << (x.bit_length()-1) if x else 0
        left = ((x << k) | (x >> (bits-k))) & mask
        right = ((x >> k) | (x << (bits-k))) & mask
        left1 = ((x << 1) | (x >> (bits - 1))) & mask
        right9 = ((x >> (9 % bits)) | (x << (bits - 9 % bits))) & mask
        fields = [literal(v) for v in (x, left, right, ceil, floor, left1, right9)]
        fields += list(map(str, [count, x.bit_count(), clz(x), ctz(x), clz(inv), ctz(inv)]))
        rows.append('    {'+', '.join(fields)+'}')
    checks = [
        ('popcountg(x)', 'r->ones'), ('parityg(x)', '(r->ones & 1)'),
        ('clzg(x, -73)', '(x ? r->lz : -73)'), ('ctzg(x, -91)', '(x ? r->tz : -91)'),
        ('ffsg((S)x)', '(x ? r->tz+1 : 0)'),
        ('clrsbg((S)x)', '(((S)x < 0 ? r->lo : r->lz) - 1)'),
        ('stdc_bit_ceil(x)', 'r->ceil'), ('stdc_bit_floor(x)', 'r->floor'),
        ('stdc_bit_width(x)', f'({bits} - r->lz)'),
        ('stdc_count_ones(x)', 'r->ones'), ('stdc_count_zeros(x)', f'({bits} - r->ones)'),
        ('stdc_has_single_bit(x)', '(r->ones == 1)'),
        ('stdc_leading_zeros(x)', 'r->lz'), ('stdc_trailing_zeros(x)', 'r->tz'),
        ('stdc_leading_ones(x)', 'r->lo'), ('stdc_trailing_ones(x)', 'r->to'),
        ('stdc_first_leading_one(x)', '(x ? r->lz+1 : 0)'),
        ('stdc_first_trailing_one(x)', '(x ? r->tz+1 : 0)'),
        ('stdc_first_leading_zero(x)', f'(r->lo == {bits} ? 0 : r->lo+1)'),
        ('stdc_first_trailing_zero(x)', f'(r->to == {bits} ? 0 : r->to+1)'),
        ('stdc_rotate_left(x,r->count)', 'r->left'), ('stdc_rotate_right(x,r->count)', 'r->right'),
        ('stdc_rotate_left(x,1)', 'r->left1'), ('stdc_rotate_right(x,9)', 'r->right9'),
    ]
    # Classic forms convert the argument to int, long or long long first.
    if bits in (16,32,64):
        suffix = {16:'',32:'l',64:'ll'}[bits]
        for name, expected in [('popcount','r->ones'),('parity','(r->ones & 1)'),
                               ('clz','r->lz'),('ctz','r->tz'),('ffs','(x ? r->tz+1 : 0)'),
                               ('clrsb','(((S)x < 0 ? r->lo : r->lz) - 1)')]:
            arg = '(S)x' if name in ('ffs','clrsb') else 'x'
            checks.append((name+suffix+'('+arg+')', expected))
    statements=[]
    for name, expected in checks:
        condition = f'__builtin_{name} != {expected}'
        if name in ('clz(x)','clzl(x)','clzll(x)','ctz(x)','ctzl(x)','ctzll(x)'):
            condition = f'x && ({condition})'
        statements.append(f'        if ({condition}) {{ puts("FAIL {name}"); return 1; }}')
    return f"""#include <stdio.h>
typedef {unsigned} U;
typedef {signed} S;
struct row {{ U x,left,right,ceil,floor,left1,right9; unsigned char count,ones,lz,tz,lo,to; }};
static const struct row rows[] = {{
""" + ',\n'.join(rows) + """
};
int main(void) {
    unsigned i;
    for (i=0; i<sizeof(rows)/sizeof(rows[0]); ++i) {
        const struct row *r=&rows[i];
        U x=r->x;
""" + '\n'.join(statements) + """
    }
    puts("PASS bit builtin vectors"); return 0;
}
"""


def check_argument_types(work):
    errors = [
        ('__builtin_popcountg(1)', 'unsigned integer'),
        ('__builtin_ffsg(1U)', 'signed integer'),
        ('__builtin_stdc_bit_floor((void *)0)', 'unsigned integer'),
        ('__builtin_stdc_rotate_left(1U)', 'argument count'),
        ('__builtin_ctzg(1U, 0L)', 'int fallback'),
        ('__builtin_bswap16(1U, 2U)', 'argument count'),
    ]
    for expr, message in errors:
        source = work / 'invalid.c'
        source.write_text(f'int f(void) {{ return {expr}; }}\n')
        attempt = subprocess.run([ROOT/'run/sdcc', '--std=c99', '-S', source, '-o', work/'invalid.s'],
                                 capture_output=True, text=True)
        assert attempt.returncode and message in attempt.stderr, (expr, attempt.stderr)


def check_linked_helpers(work):
    programs = {
        'plain': 'int main(void) { return 0; }',
        'include': '#include <cpu6bits.h>\nint main(void) { return 0; }',
        'folded': 'int main(void) { return __builtin_popcount(0U); }',
        'dead': 'int spare(unsigned x) { return __builtin_popcount(x); }\nint main(void) { return 0; }',
        'used': 'volatile unsigned x;\nint main(void) { return __builtin_popcount(x); }',
    }
    for gc in ('--gc-sections', '--no-gc-sections'):
        binaries = {}
        for name, code in programs.items():
            source = work / (name + '.c')
            source.write_text(code + '\n')
            output = work / (name + gc + '.bin')
            link_map = output.with_suffix('.map')
            subprocess.run([ROOT/'run/cpu6-bare-sdcc', gc, source, '--map', link_map, '-o', output],
                           check=True)
            binaries[name] = output.read_bytes()
            symbols = {line.split()[-1] for line in link_map.read_text().splitlines() if line.strip()}
            helpers = {s for s in symbols if s.startswith(('___c6_', '__bc_', '__cpu6_hi64', '___cpu6_rot'))}
            live = name == 'used' or (name == 'dead' and gc == '--no-gc-sections')
            assert helpers == ({'___c6_pop16', '__bc_pop16'} if live else set()), (name, gc, helpers)
        assert binaries['plain'] == binaries['include'] == binaries['folded']
        if gc == '--gc-sections':
            assert binaries['plain'] == binaries['dead']


def main():
    work=Path(tempfile.mkdtemp(prefix='sdcc-bit-builtins-',dir=ROOT/'build/out/tests'))
    print('Results:',work,flush=True)
    for bits in TYPES:
        target=work/str(bits)
        target.mkdir()
        source=target/'program.c'
        source.write_text(vector_source(bits))
        modes=[('speed',['-O1']),('size',['-Os','--gc-sections'])]
        if bits==16: modes.append(('stack',['-O0']))
        execute(target,[source],modes,common=['--native-int48'] if bits==48 else ())
    # 2^32 is 16 modulo 48, but narrowing it to a CPU6 word yields zero.
    wide_count = work/'wide-count'
    wide_count.mkdir()
    source = wide_count/'count.c'
    source.write_text("""#include <stdio.h>
#include <int48.h>
static volatile unsigned long long count = 0x100000000ULL;
static volatile uint48_t bit = 1;
int main(void) {
    if (__builtin_stdc_rotate_left(bit,count) != 65536UL) return 1;
    if (__builtin_stdc_rotate_right(bit,count) != ((uint48_t)1 << 32)) return 2;
    puts("PASS 64-bit rotation count on a 48-bit value");
    return 0;
}
""")
    execute(wide_count, [source], [('speed', ['-O1'])], common=['--native-int48'])
    target=work/'contracts'
    target.mkdir()
    execute(target,[ROOT/'tests/cpu6-bit-builtins.c'],[('speed',['-O1']),('size',['-Os','--gc-sections'])],
            common=['--sdcc-option=--std=c11'])
    check_argument_types(work)
    constexpr = work/'constexpr.c'
    constexpr.write_text('constexpr unsigned x = 0x1234;\n'
                         'static_assert(__builtin_popcount(x) == 5);\n'
                         'constexpr unsigned w = __builtin_stdc_bit_width(x);\n'
                         'static_assert(w == 13);\n')
    subprocess.run([ROOT/'run/sdcc', '--std=c23', '-S', constexpr, '-o', work/'constexpr.s'],
                   check=True)
    check_linked_helpers(work)
    print('PASS bit builtins, types, single evaluation, and unused archive members',flush=True)


if __name__=='__main__':
    main()
