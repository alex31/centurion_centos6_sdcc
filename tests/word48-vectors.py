#!/usr/bin/env python3
"""Verify the shared 48-bit sieve arithmetic against Python on both CPU cores."""
from pathlib import Path
import json
import random
import subprocess

root = Path(__file__).resolve().parent.parent
work = root / 'build/out/tests/sdcc-cpu6/word48-tests'
work.mkdir(parents=True, exist_ok=True)
rng = random.Random(0x4806)
mask = (1 << 48) - 1
cases = [(0, 1), (mask, 1), (mask, 65535), (mask, 1 << 46),
         ((1 << 47) - 1, (1 << 47) - 1)]
cases += [(rng.getrandbits(48), rng.getrandbits(47) or 1) for _ in range(27)]
rows = []
for a, b in cases:
    row = []
    for value in (a, b, a // b, (a * 2) & mask, (a - b) & mask, a >> 1):
        row += [(value >> shift) & 65535 for shift in (32, 16, 0)]
    row.append(a & 1)
    rows.append('{' + ','.join(f'0x{x:x}U' for x in row) + '}')
source = '''#include <stdio.h>
#include "word48.h"
struct V { uint16_t ah,am,al,bh,bm,bl,qh,qm,ql,dh,dm,dl,sh,sm,sl,rh,rm,rl,bit; };
static struct V cases[] = {''' + ','.join(rows) + '''};
int main(void) {
    unsigned int i, bit;
    Word48 a,b,value;
    if (sizeof(Word48) != 6) return 1;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        a.hi=cases[i].ah; a.mid=cases[i].am; a.lo=cases[i].al;
        b.hi=cases[i].bh; b.mid=cases[i].bm; b.lo=cases[i].bl;
        word48_divide(&value,&a,&b);
        if(value.hi!=cases[i].qh || value.mid!=cases[i].qm || value.lo!=cases[i].ql) {
            puts("FAIL divide48"); return 2;
        }
        word48_copy(&value,&a); word48_double(&value);
        if(value.hi!=cases[i].dh || value.mid!=cases[i].dm || value.lo!=cases[i].dl) {
            puts("FAIL double48"); return 3;
        }
        word48_copy(&value,&a); word48_subtract(&value,&b);
        if(value.hi!=cases[i].sh || value.mid!=cases[i].sm || value.lo!=cases[i].sl) {
            puts("FAIL subtract48"); return 4;
        }
        word48_copy(&value,&a); bit=word48_popbit(&value);
        if(value.hi!=cases[i].rh || value.mid!=cases[i].rm || value.lo!=cases[i].rl || bit!=cases[i].bit) {
            puts("FAIL popbit48"); return 5;
        }
    }
    puts("PASS 48-bit arithmetic"); return 0;
}
'''
(work / 'vectors.c').write_text(source)
helper = root / 'tests/word48.c'
include = '-I' + str(helper.parent)
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-O2', include,
                work / 'vectors.c', helper, root / 'lib/int48/int48.c', '-o', work / 'host'], check=True)
subprocess.run([work / 'host'], check=True)
files = []
for opt in (0, 1):
    binary = work / f'sdcc-O{opt}.bin'
    with (work / f'sdcc-O{opt}.log').open('w') as log:
        subprocess.run([root / 'run/cpu6-bare-sdcc', f'-O{opt}', include,
                        work / 'vectors.c', helper, '-o', binary],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
        output = subprocess.check_output([root / 'tests/centurion-cli', '-T', '20000000',
                                           '-A', '2000', '-b', binary], text=True)
        log.write(output)
        assert 'PASS 48-bit arithmetic' in output and 'System halted' in output
    files.append(binary.name)
    print('PASS SDCC', f'-O{opt}')
(work / 'manifest.json').write_text(json.dumps(files))
with (work / 'microcode.log').open('w') as log:
    subprocess.run(['node', root / 'tests/compiler-audit-core.js', work, work / 'manifest.json'],
                   stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
print('PASS 48-bit arithmetic on both CPU cores')
