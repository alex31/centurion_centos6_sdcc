#!/usr/bin/env python3
"""Inline small call wrappers without capturing names or expanding cycles."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
work=Path(tempfile.mkdtemp(prefix='sdcc-nonleaf-',dir=root/'build/out/tests'))
print('Results:',work,flush=True)
(work/'callee.c').write_text('volatile unsigned calls; unsigned external(unsigned a){calls++;return a*3+7;}\n')
(work/'program.c').write_text('''#include <stdio.h>
extern volatile unsigned calls;
unsigned external(unsigned a);
static unsigned wrapper(unsigned n){n=external(n);return n^0x55U;}
unsigned usewrapper(unsigned n){return wrapper(n)+1;}
static unsigned fake(unsigned n){return n+99;}
unsigned scoped(unsigned(*external)(unsigned),unsigned n){(void)external;return wrapper(n);}
static inline unsigned recursive(unsigned n){return n ? recursive(n-1)+1 : 0;}
unsigned odd(unsigned);
unsigned even(unsigned n){return n ? odd(n-1)+1 : 0;}
unsigned odd(unsigned n){return n ? even(n-1)+1 : 0;}
int main(void){unsigned i;for(i=0;i<16;i++)if(usewrapper(i)!=((i*3+7)^0x55U)+1)goto fail;
 if(calls!=16||scoped(fake,5)!=(22^0x55U)||calls!=17||recursive(17)!=17||even(18)!=18||odd(19)!=19)goto fail;
 puts("PASS nonleaf inline, global binding and recursion guards");return 0;
fail:puts("FAIL nonleaf inline");return 1;}
''')
manifest=[]
for name,flags in [('speed',['-O1']),('leaf',['-O1','--sdcc-option=--cpu6-leaf-inline-only']),
                   ('disabled',['-O1','--sdcc-option=--cpu6-no-auto-inline']),('stack',['-O0']),
                   ('no-peep',['-O1','--no-peep']),('size',['-O1','--sdcc-option=--opt-code-size'])]:
    binary=work/(name+'.bin')
    with (work/(name+'-build.log')).open('w') as log:
        subprocess.run([root/'run/cpu6-bare-sdcc',*flags,work/'program.c',work/'callee.c',
                        '--save-temps',work/name,'-o',binary],stdout=log,stderr=subprocess.STDOUT,check=True,timeout=60)
    asm=(work/name/'unit2.s').read_text()
    body=re.search(r'^_usewrapper:\n(.*?)(?=^_\w+:|\Z)',asm,re.M|re.S).group(1)
    assert ('jsr _wrapper' in body)==(name in ('leaf','disabled','size')),(name,body)
    output=subprocess.check_output([root/'tests/centurion-cli','-A','2000','-T','10000000','-b',binary],text=True,timeout=60)
    assert 'PASS nonleaf inline' in output and 'FAIL' not in output,(name,output)
    manifest.append(binary.name);print('PASS instruction core:',name,flush=True)
(work/'manifest.json').write_text(json.dumps(manifest))
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node',root/'tests/compiler-audit-core.js',work,work/'manifest.json'],stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
print('PASS bounded nonleaf inline on both CPU cores')
(root/'build/out/tests/latest-sdcc-nonleaf.txt').write_text(str(work)+'\n')
