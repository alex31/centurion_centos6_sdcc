#!/usr/bin/env python3
"""All constant 32-bit shift distances, independently computed in Python."""
import json
import os
from pathlib import Path
import random
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
work=Path(tempfile.mkdtemp(prefix='sdcc-long-shifts-',dir=root/'build/out/tests'))
print('Results:',work,flush=True)
rng=random.Random(0x5A1F7)
values=[0,1,255,256,65535,65536,0x7fffffff,0x80000000,0x80000001,0xffffffff]
values += [rng.getrandbits(32) for _ in range(10)]
source='#include <stdint.h>\n#include <stdio.h>\n'
for n in range(32):
    source+=f'uint32_t left{n}(uint32_t a) {{ return a << {n}; }}\n'
    source+=f'uint32_t right{n}(uint32_t a) {{ return a >> {n}; }}\n'
    source+=f'int32_t signed{n}(int32_t a) {{ return a >> {n}; }}\n'
source+='typedef uint32_t (*ufn)(uint32_t); typedef int32_t (*sfn)(int32_t);\n'
for name,typ,prefix in [('lefts','ufn','left'),('rights','ufn','right'),('signeds','sfn','signed')]:
    source+=f'static {typ} {name}[32]={{'+','.join(prefix+str(n) for n in range(32))+'};\n'
source+='struct vector {uint32_t value; uint32_t left[32],right[32]; int32_t signed_right[32];};\nstatic const struct vector vectors[]={\n'
for v in values:
    s=v if v<0x80000000 else v-0x100000000
    source+='{0x%xUL,'%v+','.join('{'+','.join('0x%xUL'%(x&0xffffffff) for x in arr)+'}'
                                    for arr in [[v<<n for n in range(32)],[v>>n for n in range(32)],[s>>n for n in range(32)]])+'},\n'
source+='''};
int main(void) {
 unsigned i,n;
 for(i=0;i<sizeof(vectors)/sizeof(vectors[0]);i++)for(n=0;n<32;n++) {
  if(lefts[n](vectors[i].value)!=vectors[i].left[n] ||
     rights[n](vectors[i].value)!=vectors[i].right[n] ||
     signeds[n]((int32_t)vectors[i].value)!=vectors[i].signed_right[n]) {
   puts("FAIL long shifts");putchar('0'+i/10);putchar('0'+i%10);putchar(':');putchar('0'+n/10);putchar('0'+n%10);return 1;
  }
 }
 puts("PASS all constant long shifts");return 0;
}
'''
(work/'program.c').write_text(source)
manifest=[]
for name,flags in [('speed',['-O1']),('disabled',['-O1','--sdcc-option=--cpu6-no-wide-shifts']),
                   ('stack',['-O0']),('size',['-O1','--sdcc-option=--opt-code-size']),('no-peep',['-O1','--no-peep'])]:
    binary=work/(name+'.bin')
    with (work/(name+'-build.log')).open('w') as log:
        subprocess.run([root/'run/cpu6-bare-sdcc',*flags,'--origin','0200',work/'program.c',
                        '--save-temps',work/name,'-o',binary],stdout=log,stderr=subprocess.STDOUT,check=True)
    asm=(work/name/'unit2.s').read_text()
    assert ('jsr __shll' in asm)==(name in ('disabled','size')),name
    output=subprocess.check_output([root/'tests/centurion-cli','-A','0200','-T','100000000','-b',binary],text=True,timeout=120)
    (work/(name+'.log')).write_text(output)
    assert 'PASS all constant long shifts' in output and 'FAIL' not in output and 'System halted' in output,(name,output)
    manifest.append(binary.name)
    print('PASS instruction core:',name,flush=True)
(work/'manifest.json').write_text(json.dumps(manifest))
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node',root/'tests/compiler-audit-core.js',work,work/'manifest.json'],
                   env=dict(os.environ,CPU6_BARE_ORIGIN='0200',CPU6_MICROSTEP_LIMIT='300000000'),stdout=log,stderr=subprocess.STDOUT,check=True,timeout=300)
print('PASS all constant 32-bit shift distances on both CPU cores')
(root/'build/out/tests/latest-sdcc-long-shifts.txt').write_text(str(work)+'\n')
