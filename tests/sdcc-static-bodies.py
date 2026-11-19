#!/usr/bin/env python3
"""Drop unused bodies while preserving function-pointer and call references."""
import json
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
work=Path(tempfile.mkdtemp(prefix='sdcc-static-',dir=root/'build/out/tests'))
print('Results:',work,flush=True)
(work/'program.c').write_text('''#include <stdio.h>
static inline unsigned inc(unsigned a){return a+1;}
static unsigned small(unsigned a){return a+3;}
static unsigned unused(unsigned a){while(a<100)a=a*3+7;return a;}
static unsigned first(unsigned a){return a+11;}
static unsigned second(unsigned a){return a+17;}
static unsigned recur(unsigned a){return a ? recur(a-1)+1 : 0;}
typedef unsigned(*function)(unsigned);
static function table[]={first,second};
static volatile unsigned selector;
int main(void){unsigned value=inc(10)+small(20);selector=1;
 if(value!=34||table[selector](5)!=22||recur(7)!=7){puts("FAIL static bodies");return 1;}
 puts("PASS static bodies and data references");return 0;}
''')
manifest=[]
for name,flags in [('speed',[]),('keep',['--sdcc-option=--cpu6-keep-static-functions']),('no-inline',['--sdcc-option=--cpu6-no-auto-inline'])]:
    binary=work/(name+'.bin')
    with (work/(name+'-build.log')).open('w') as log:
        subprocess.run([root/'run/cpu6-bare-sdcc','-O1',*flags,work/'program.c','--save-temps',work/name,'-o',binary],stdout=log,stderr=subprocess.STDOUT,check=True)
    asm=(work/name/'unit2.s').read_text()
    assert ('\n_unused:' in asm)==(name=='keep'),name
    assert ('\n_inc:' in asm)==(name=='keep'),name
    assert ('\n_small:' in asm)==(name!='speed'),name
    for fn in ('first','second','recur'):assert '\n_'+fn+':' in asm,(name,fn)
    output=subprocess.check_output([root/'tests/centurion-cli','-A','2000','-T','10000000','-b',binary],text=True)
    assert 'PASS static bodies' in output and 'FAIL' not in output,(name,output)
    manifest.append(binary.name);print('PASS instruction core:',name,flush=True)
(work/'manifest.json').write_text(json.dumps(manifest))
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node',root/'tests/compiler-audit-core.js',work,work/'manifest.json'],stdout=log,stderr=subprocess.STDOUT,check=True,timeout=60)
print('PASS static-body selection on both CPU cores')
(root/'build/out/tests/latest-sdcc-static.txt').write_text(str(work)+'\n')
