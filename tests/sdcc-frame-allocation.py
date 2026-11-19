#!/usr/bin/env python3
"""Large-frame hot window, CFG branches, calls and register preservation."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
work=Path(tempfile.mkdtemp(prefix='sdcc-frame-',dir=root/'build/out/tests'))
print('Results:',work,flush=True)
source='''#include <stdint.h>
#include <stdio.h>
uint16_t callee(uint16_t a);
uint16_t large_frame(uint16_t seed) {
'''
source+=''.join(f' volatile uint16_t v{i}=seed+{i}U;\n' for i in range(80))
source+=''' uint16_t i,total=0;
 for(i=0;i<128;i++) {
  total+=(v0^i)+v1+v2+v3+v4+v5+v6+v7;
  if(i&1)total^=callee(v0+i);else total+=callee(v1-i);
 }
 return total+v79;
}
uint16_t callee(uint16_t a) {return a*17U+3U;}
struct row {uint16_t input,expected;};
static const struct row cases[]={
'''
for seed in [0,1,19,255,32767,65500]:
    total=0
    for i in range(128):
        total=(total+((seed^i)+sum((seed+n)&65535 for n in range(1,8))))&65535
        if i&1:total^=(((seed+i)&65535)*17+3)&65535
        else:total=(total+((((seed+1-i)&65535)*17+3)&65535))&65535
    expected=(total+seed+79)&65535
    source+='{%dU,%dU},\n'%(seed,expected)
source+='''};
int main(void) {unsigned i;for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++)
 if(large_frame(cases[i].input)!=cases[i].expected){puts("FAIL frame allocation");return 1;}
 puts("PASS frame allocation and preserved registers");return 0;}
'''
(work/'program.c').write_text(source)
manifest=[]
for name,flags in [('speed',['-O1']),('no-frame',['-O1','--sdcc-option=--cpu6-no-frame-base']),
                   ('simple',['-O1','--sdcc-option=--cpu6-simple-allocation']),
                   ('stack',['-O0']),('no-peep',['-O1','--no-peep'])]:
    binary=work/(name+'.bin')
    with (work/(name+'-build.log')).open('w') as log:
        subprocess.run([root/'run/cpu6-bare-sdcc',*flags,'--sdcc-option=--cpu6-no-auto-inline',
                        work/'program.c','--save-temps',work/name,'-o',binary],stdout=log,stderr=subprocess.STDOUT,check=True)
    asm=(work/name/'unit2.s').read_text()
    assert ('CPU6 fixed frame window' in asm)==(name not in ('no-frame','stack')),name
    output=subprocess.check_output([root/'tests/centurion-cli','-A','2000','-T','100000000','-b',binary],text=True,timeout=120)
    (work/(name+'.log')).write_text(output)
    assert 'PASS frame allocation' in output and 'FAIL' not in output,(name,output)
    manifest.append(binary.name);print('PASS instruction core:',name,flush=True)
(work/'manifest.json').write_text(json.dumps(manifest))
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node',root/'tests/compiler-audit-core.js',work,work/'manifest.json'],
                   env=dict(os.environ,CPU6_MICROSTEP_LIMIT='200000000'),stdout=log,stderr=subprocess.STDOUT,check=True,timeout=240)
print('PASS frame/allocator variants on both CPU cores')
(root/'build/out/tests/latest-sdcc-frame.txt').write_text(str(work)+'\n')
