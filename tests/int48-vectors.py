#!/usr/bin/env python3
"""Independent Python integer vectors for the public signed/unsigned 48-bit API."""
import json
import os
from pathlib import Path
import random
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
work = Path(tempfile.mkdtemp(prefix='int48-tests-', dir=root / 'build/out/tests'))
print('Results:', work, flush=True)
mask, sign = (1 << 48) - 1, 1 << 47
rng = random.Random(0x480048)


def words(n):
    n &= mask
    return '{' + ','.join(str((n >> shift) & 65535) + 'U' for shift in (32, 16, 0)) + '}'


def signed(n):
    return n - (1 << 48) if n & sign else n


pairs = [(0, 0), (1, 0), (mask, 1), (mask, mask), (sign, mask), (sign, 2),
         (sign - 1, sign), (0xffff0000ffff, 0x100010001)]
pairs += [(mask, 1 << bit) for bit in range(48)]
pairs += [(rng.randrange(1 << 48), rng.randrange(1 << 48) or 1) for _ in range(24)]
rows = []
for i, (a, b) in enumerate(pairs):
    sa, sb = signed(a), signed(b)
    q, r = divmod(a, b) if b else (0, 0)
    overflow = sa == -sign and sb == -1
    sq = abs(sa) // abs(sb) * (-1 if (sa < 0) != (sb < 0) else 1) if sb and not overflow else 0
    sr = sa - sq * sb if sb and not overflow else 0
    count = [0, 1, 15, 16, 17, 31, 32, 33, 47, 48, 63][i % 11]
    values = [a, b, a + b, a - b, a * b, q, r, -a, a << count, a >> count, sq, sr, sa >> count]
    rows.append('{' + ','.join(words(v) for v in values) + ',' +
                ','.join(map(str, (count, 0 if b else 1, 1 if not b else 2 if overflow else 0,
                                  (a > b) - (a < b), (sa > sb) - (sa < sb)))) +
                ',"' + str(a) + '","' + str(sa) + '"}')
source = '''#include <stdint.h>
#include <stdio.h>
#include <int48.h>
struct V {
    uint48_t a,b,add,sub,mul,q,r,neg,left,right;
    int48_t sq,sr,arithmetic;
    unsigned int count;
    int unsigned_status,signed_status,ucmp,scmp;
    char unsigned_text[17],signed_text[17];
};
static const struct V vectors[] = {
''' + ',\n'.join(rows) + '''};
#define SAME(a,b) ((a).hi==(b).hi && (a).mid==(b).mid && (a).lo==(b).lo)
#define CHECK(test) do { if (!(test)) { puts("FAIL int48 vector"); putchar('0'+i/10); putchar('0'+i%10); return 1; } } while (0)
static int text_equal(const char *a,const char *b) {
    while (*a && *a==*b) { a++; b++; } return *a==*b;
}
int main(void) {
    static const uint48_t sentinel = {0x1357,0x2468,0x369c};
    uint48_t a,b,out,q,r;
    int48_t sa,sb,so,sq,sr;
    unsigned char text[19];
    unsigned int i,limit=sizeof(vectors)/sizeof(vectors[0]);
    int status;
#ifdef INT48_TEST_SHORT
    limit=8;
#endif
    CHECK(sizeof(uint48_t)==6 && sizeof(int48_t)==6);
    for(i=0;i<limit;i++) {
        u48_copy(&a,&vectors[i].a);u48_copy(&b,&vectors[i].b);
        sa.hi=a.hi;sa.mid=a.mid;sa.lo=a.lo;sb.hi=b.hi;sb.mid=b.mid;sb.lo=b.lo;
        u48_add(&out,&a,&b);CHECK(SAME(out,vectors[i].add));
        i48_add(&so,&sa,&sb);CHECK(SAME(so,vectors[i].add));
        u48_sub(&out,&a,&b);CHECK(SAME(out,vectors[i].sub));
        i48_sub(&so,&sa,&sb);CHECK(SAME(so,vectors[i].sub));
        u48_mul(&out,&a,&b);CHECK(SAME(out,vectors[i].mul));
        i48_mul(&so,&sa,&sb);CHECK(SAME(so,vectors[i].mul));
        u48_neg(&out,&a);CHECK(SAME(out,vectors[i].neg));
        i48_neg(&so,&sa);CHECK(SAME(so,vectors[i].neg));
        CHECK(u48_cmp(&a,&b)==vectors[i].ucmp && i48_cmp(&sa,&sb)==vectors[i].scmp);
        u48_copy(&q,&sentinel);u48_copy(&r,&sentinel);
        status=u48_divmod(&q,&r,&a,&b);CHECK(status==vectors[i].unsigned_status);
        if(!status) {CHECK(SAME(q,vectors[i].q));CHECK(SAME(r,vectors[i].r));}
        else {CHECK(SAME(q,sentinel));CHECK(SAME(r,sentinel));}
        sq.hi=sr.hi=sentinel.hi;sq.mid=sr.mid=sentinel.mid;sq.lo=sr.lo=sentinel.lo;
        status=i48_divmod(&sq,&sr,&sa,&sb);CHECK(status==vectors[i].signed_status);
        if(!status) {CHECK(SAME(sq,vectors[i].sq));CHECK(SAME(sr,vectors[i].sr));}
        else {CHECK(SAME(sq,sentinel));CHECK(SAME(sr,sentinel));}
        u48_shl(&out,&a,vectors[i].count);CHECK(SAME(out,vectors[i].left));
        i48_shl(&so,&sa,vectors[i].count);CHECK(SAME(so,vectors[i].left));
        u48_shr(&out,&a,vectors[i].count);CHECK(SAME(out,vectors[i].right));
        i48_shr(&so,&sa,vectors[i].count);CHECK(SAME(so,vectors[i].arithmetic));
        /* Exact input/output aliases must preserve both source operands. */
        u48_copy(&out,&a);u48_add(&out,&out,&b);CHECK(SAME(out,vectors[i].add));
        u48_copy(&out,&b);u48_sub(&out,&a,&out);CHECK(SAME(out,vectors[i].sub));
        u48_copy(&out,&a);u48_mul(&out,&out,&b);CHECK(SAME(out,vectors[i].mul));
        if(b.hi||b.mid||b.lo) {
            status=u48_divmod(&a,&b,&a,&b);CHECK(status==0);
            CHECK(SAME(a,vectors[i].q));CHECK(SAME(b,vectors[i].r));
            CHECK(u48_divmod(&out,&out,&vectors[i].a,&vectors[i].b)==INT48_ALIAS);
        }
        text[0]=0x5a;text[18]=0xa5;
        u48_format_dec((char *)(text+1),&vectors[i].a);
        CHECK(text_equal((char *)(text+1),vectors[i].unsigned_text));
        CHECK(text[0]==0x5a && text[18]==0xa5);
        i48_format_dec((char *)(text+1),&sa);
        CHECK(text_equal((char *)(text+1),vectors[i].signed_text));
        CHECK(text[0]==0x5a && text[18]==0xa5);
    }
    puts("PASS int48 arithmetic, signed division, aliases, shifts and formatting");return 0;
}
'''
program = work / 'vectors.c'
program.write_text(source)
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-O2', '-I' + str(root / 'lib/int48'),
                program, root / 'lib/int48/int48.c', '-o', work / 'host'], check=True)
subprocess.run([work / 'host'], check=True)
manifest = []
for opt in (0, 1):
    name = f'sdcc-O{opt}'
    binary = work / (name + '.bin')
    with (work / (name + '-build.log')).open('w') as log:
        subprocess.run([root / 'run/cpu6-bare-sdcc', '--origin', '0200',
                        f'-O{opt}', program, '-o', binary],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    output = subprocess.check_output([root / 'tests/centurion-cli', '-T', '100000000',
                                      '-A', '0200', '-b', binary], text=True)
    (work / (name + '.log')).write_text(output)
    assert 'PASS int48' in output and 'FAIL' not in output, name
    print('PASS instruction core:', name, flush=True)
    manifest.append(binary.name)
(work / 'manifest.json').write_text(json.dumps(manifest))
with (work / 'microcode.log').open('w') as log:
    subprocess.run(['node', root / 'tests/compiler-audit-core.js', work, work / 'manifest.json'],
                   env=dict(os.environ,CPU6_BARE_ORIGIN='0200',CPU6_MICROSTEP_LIMIT='300000000'),
                   stdout=log, stderr=subprocess.STDOUT, check=True, timeout=240)
print('PASS int48 public API on both CPU cores')
(root / 'build/out/tests/latest-int48-tests.txt').write_text(str(work) + '\n')
