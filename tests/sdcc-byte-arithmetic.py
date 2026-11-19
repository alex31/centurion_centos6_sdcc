#!/usr/bin/env python3
"""Python-derived byte results, promotions, volatile widths and frame cases."""
import json
import os
from pathlib import Path
import random
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
work=Path(tempfile.mkdtemp(prefix='sdcc-byte-tests-',dir=root/'build/out/tests'))
print('Results:',work,flush=True)
rng=random.Random(0x816)
edges=[0,1,2,15,16,63,127,128,129,254,255]
pairs=[(a,b) for a in edges for b in edges]
pairs += [(rng.randrange(256),rng.randrange(256)) for _ in range(40)]
rows=[]
for n,(a,b) in enumerate(pairs):
    w=[0,255,256,257,32767,32768,65535][n%7]
    total=(a+b)&255
    signed_total=total if total<128 else total-256
    fields=[a,b,total,(a-b)&255,a&b,a|b,a^b,w,(w+b)&255,(w|b)&255,(w^a)&255,
            signed_total,7 if total else 3, int(((w+b)&65535)!=0),(w+b+3)&255]
    rows.append('{'+','.join(str(x)+('U' if x>=0 else '') for x in fields)+'}')
source='''#include <stdint.h>
#include <stdio.h>
struct row { uint8_t a,b,add,sub,and_,or_,xor_; uint16_t word; uint8_t mixed,lowor,volatile_result;
             int signed_add; unsigned branch,truth; uint8_t large; };
static const struct row rows[]={
'''+',\n'.join(rows)+'''};
static volatile uint16_t volatile_word;
static volatile uint8_t volatile_byte;
uint8_t add8(uint8_t a,uint8_t b) {return a+b;}
uint8_t sub8(uint8_t a,uint8_t b) {return a-b;}
uint8_t and8(uint8_t a,uint8_t b) {return a&b;}
uint8_t or8(uint8_t a,uint8_t b) {return a|b;}
uint8_t xor8(uint8_t a,uint8_t b) {return a^b;}
uint8_t mixed(uint16_t a,uint8_t b) {return (uint8_t)(a+b);}
uint8_t low_or(uint16_t a,uint16_t b) {return (uint8_t)(a|b);}
int signed_add(int8_t a,int8_t b) {return (int8_t)(a+b);}
unsigned byte_branch(uint8_t a,uint8_t b) {if((uint8_t)(a+b))return 7;return 3;}
_Bool word_truth(uint16_t a,uint8_t b) {return (uint16_t)(a+b);}
uint8_t read_volatile(void) {return (uint8_t)(volatile_word ^ volatile_byte);}
void modify(uint8_t *p,uint16_t mask) {*p=(uint8_t)(*p^mask);}
uint8_t large_frame(uint16_t a,uint8_t b) {
    volatile uint8_t padding[160];
    padding[0]=1;padding[159]=2;
    return (uint8_t)(a+b+padding[0]+padding[159]);
}
int main(void) {
    unsigned i,stage;
    uint8_t guard[3];
    for(i=0;i<sizeof(rows)/sizeof(rows[0]);i++) {
        const struct row *p=&rows[i];
        stage=1;
        if(add8(p->a,p->b)!=p->add || sub8(p->a,p->b)!=p->sub ||
           and8(p->a,p->b)!=p->and_ || or8(p->a,p->b)!=p->or_ || xor8(p->a,p->b)!=p->xor_)goto fail;
        stage=2;
        if(mixed(p->word,p->b)!=p->mixed || low_or(p->word,p->b)!=p->lowor ||
           signed_add((int8_t)p->a,(int8_t)p->b)!=p->signed_add)goto fail;
        stage=3;
        if(byte_branch(p->a,p->b)!=p->branch || word_truth(p->word,p->b)!=p->truth)goto fail;
        stage=4;
        volatile_word=p->word;volatile_byte=p->a;
        if(read_volatile()!=p->volatile_result)goto fail;
        stage=5;
        guard[0]=0xa5;guard[1]=p->a;guard[2]=0x5a;
        modify(guard+1,p->word);
        if(guard[0]!=0xa5 || guard[1]!=p->volatile_result || guard[2]!=0x5a)goto fail;
        stage=6;
        if(large_frame(p->word,p->b)!=p->large)goto fail;
    }
    puts("PASS byte arithmetic, promotions, volatile widths and frames");return 0;
fail:
    puts("FAIL byte arithmetic");putchar('0'+stage);putchar(':');putchar('0'+i/100);putchar('0'+i/10%10);putchar('0'+i%10);return 1;
}
'''
(work/'program.c').write_text(source)
subprocess.run(['cc','-std=c99','-O1','-fsanitize=address,undefined',work/'program.c','-o',work/'host'],check=True)
subprocess.run([work/'host'],check=True)
manifest=[]
for name,flags in [('speed',['-O1']),('disabled',['-O1','--sdcc-option=--cpu6-no-byte-arithmetic']),
                   ('stack',['-O0']),('no-peep',['-O1','--no-peep']),('size',['-O1','--sdcc-option=--opt-code-size'])]:
    binary=work/(name+'.bin')
    with (work/(name+'-build.log')).open('w') as log:
        subprocess.run([root/'run/cpu6-bare-sdcc',*flags,work/'program.c','--save-temps',work/name,'-o',binary],
                       stdout=log,stderr=subprocess.STDOUT,check=True)
    asm=(work/name/'unit2.s').read_text()
    def body(function):return re.search(r'^_'+function+r':\n(.*?)(?=^_\w+:|\Z)',asm,re.M|re.S).group(1)
    assert ('addb al,bl' in body('add8')) == (name!='disabled'),name
    assert ('andb al,bl' in body('and8')) == (name!='disabled'),name
    # The declared volatile word must be read in full, not as a low byte.
    v=body('read_volatile')
    assert re.search(r'\bld[ab] \(_volatile_word(?:\+0)?\)',v),v
    assert not re.search(r'\bld[ab]b \(_volatile_word',v),v
    output=subprocess.check_output([root/'tests/centurion-cli','-A','2000','-T','100000000','-b',binary],text=True,timeout=120)
    (work/(name+'.log')).write_text(output)
    assert 'PASS byte arithmetic' in output and 'FAIL' not in output and 'System halted' in output,name
    manifest.append(binary.name)
    print('PASS instruction core:',name,flush=True)
(work/'manifest.json').write_text(json.dumps(manifest))
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node',root/'tests/compiler-audit-core.js',work,work/'manifest.json'],
                   env=dict(os.environ,CPU6_MICROSTEP_LIMIT='300000000'),stdout=log,stderr=subprocess.STDOUT,check=True,timeout=300)
print('PASS byte-arithmetic configurations on both CPU cores')
(root/'build/out/tests/latest-sdcc-byte-tests.txt').write_text(str(work)+'\n')
