#!/usr/bin/env python3
"""Independent quotient/remainder vectors and fusion barriers on both cores."""
import json
import os
from pathlib import Path
import random
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
work = Path(tempfile.mkdtemp(prefix='sdcc-divmod-', dir=root/'build/out/tests'))
print('Results:', work, flush=True)
rng = random.Random(0xD1F00D)
source = '#include <stdint.h>\n#include <stdio.h>\n'
checks = []
for bits in (16, 32):
    mask = (1 << bits) - 1
    edges = [0, 1, 2, 47, 255, 256, (1 << (bits-1))-1, 1 << (bits-1), mask-1, mask]
    pairs = [(a,b) for a in edges for b in edges if b]
    pairs += [(rng.randrange(mask+1), rng.randrange(1,mask+1)) for _ in range(24)]
    for signed in (False, True):
        name = ('s' if signed else 'u') + str(bits)
        typ = ('int' if signed else 'uint') + str(bits) + '_t'
        rows = []
        for a,b in pairs:
            if signed:
                a = a if a < (1 << (bits-1)) else a-(mask+1)
                b = b if b < (1 << (bits-1)) else b-(mask+1)
                if a == -(1 << (bits-1)) and b == -1: continue
            q = abs(a)//abs(b) * (-1 if (a<0)!=(b<0) else 1)
            rem = a-q*b
            rows.append('{'+','.join('(('+typ+')0x%xUL)'%(n&mask) for n in (a,b,q,rem))+'}')
        source += f'''static {typ} q_{name}, r_{name};
struct case_{name} {{ {typ} a,b,q,r; }};
static const struct case_{name} cases_{name}[]={{'''+','.join(rows)+'''};
'''
        source += f'''void pair_{name}({typ} a,{typ} b) {{ {typ} q=a/b,r=a%b; q_{name}=q;r_{name}=r; }}
void reverse_{name}({typ} a,{typ} b) {{ {typ} r=a%b,q=a/b; q_{name}=q;r_{name}=r; }}
int check_{name}(void) {{ unsigned i; for(i=0;i<sizeof(cases_{name})/sizeof(cases_{name}[0]);++i) {{
 const struct case_{name} *p=cases_{name}+i;
 pair_{name}(p->a,p->b); if(q_{name}!=p->q||r_{name}!=p->r) return 0;
 reverse_{name}(p->a,p->b); if(q_{name}!=p->q||r_{name}!=p->r) return 0;
}} return 1; }}
'''
        checks.append('check_'+name+'()')
source += '''static volatile uint32_t observed;
static uint32_t input;
void touch(void);
void volatile_pair(uint32_t b) { q_u32=observed/b; r_u32=observed%b; }
void alias_pair(uint32_t *p,uint32_t b) { uint32_t q=*p/b; *p=19; r_u32=*p%b; q_u32=q; }
void call_pair(uint32_t b) { uint32_t q=input/b; touch(); r_u32=input%b; q_u32=q; }
void touch(void) { input=23; }
void changed_pair(uint16_t a,uint16_t b) { q_u16=a/b; a+=3; r_u16=a%b; }
void large_pair(uint32_t a,uint32_t b) {
 volatile uint8_t padding[200]; uint32_t q,r;
 padding[0]=7; q=a/b; r=a%b; padding[199]=11;
 q_u32=q+padding[0]-7; r_u32=r+padding[199]-11;
}
int main(void) {
 if (!('''+' && '.join(checks)+''')) { puts("FAIL divmod vectors");return 1; }
 observed=123456789UL;volatile_pair(47);if(q_u32!=2626740UL||r_u32!=9)goto fail;
 input=101;alias_pair(&input,7);if(q_u32!=14||r_u32!=5||input!=19)goto fail;
 input=101;call_pair(7);if(q_u32!=14||r_u32!=2||input!=23)goto fail;
 changed_pair(101,7);if(q_u16!=14||r_u16!=6)goto fail;
 large_pair(0xffffffffUL,47);if(q_u32!=91382282UL||r_u32!=41)goto fail;
 puts("PASS divmod vectors, aliases, volatile, calls and large frames");return 0;
fail: puts("FAIL divmod barriers");return 1;
}
'''
(work/'program.c').write_text(source)
subprocess.run(['cc','-std=c99','-O1','-fsanitize=address,undefined',work/'program.c','-o',work/'host'],check=True)
subprocess.run([work/'host'],check=True)
manifest = []
for name,flags in [('speed',['-O1']),('disabled',['-O1','--sdcc-option=--cpu6-no-divmod']),
                   ('stack',['-O0']),('size',['-O1','--sdcc-option=--opt-code-size']),
                   ('no-peep',['-O1','--no-peep'])]:
    binary=work/(name+'.bin')
    with (work/(name+'-build.log')).open('w') as log:
        subprocess.run([root/'run/cpu6-bare-sdcc',*flags,'--sdcc-option=--cpu6-no-auto-inline',
                        '--origin','0200',work/'program.c','--save-temps',work/name,'-o',binary],
                       stdout=log,stderr=subprocess.STDOUT,check=True)
    asm=(work/name/'unit2.s').read_text()
    def body(function): return re.search(r'^_'+function+r':\n(.*?)(?=^_\w+:|\Z)',asm,re.M|re.S).group(1)
    for bits in (16,32):
        for signed in ('u','s'):
            for prefix in ('pair_','reverse_'):
                expected=name not in ('disabled','stack') and (bits==16 or name!='size')
                assert ('reuse paired quotient/remainder' in body(prefix+signed+str(bits)))==expected,(name,prefix,signed,bits)
    for function in ('volatile_pair','alias_pair','call_pair','changed_pair'):
        assert 'reuse paired quotient/remainder' not in body(function),(name,function)
    output=subprocess.check_output([root/'tests/centurion-cli','-A','0200','-T','100000000','-b',binary],text=True,timeout=120)
    (work/(name+'.log')).write_text(output)
    assert 'PASS divmod' in output and 'FAIL' not in output and 'System halted' in output,(name,output)
    manifest.append(binary.name)
    print('PASS instruction core:',name,flush=True)
(work/'manifest.json').write_text(json.dumps(manifest))
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node',root/'tests/compiler-audit-core.js',work,work/'manifest.json'],
                   env=dict(os.environ,CPU6_BARE_ORIGIN='0200',CPU6_MICROSTEP_LIMIT='300000000'),
                   stdout=log,stderr=subprocess.STDOUT,check=True,timeout=300)
print('PASS quotient/remainder fusion on both CPU cores')
(root/'build/out/tests/latest-sdcc-divmod.txt').write_text(str(work)+'\n')
