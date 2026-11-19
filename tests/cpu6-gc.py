#!/usr/bin/env python3
"""Prune code/data inside built objects; preserve callbacks, roots and asm refs."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import sys

root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'scripts'))
from cpu6_object import unpack
work=Path(tempfile.mkdtemp(prefix='cpu6-gc-',dir=root/'build/out/tests'))
print('Results:',work,flush=True)
source='''#include <stdio.h>
volatile int used_data=17;
volatile int cookie=93;
int dead_data[511];
const char dead_text[512]="discard this unreferenced initialized object";
int live(void){return used_data+2;}
int handler(void){return cookie;}
#ifdef DEAD_UNDEFINED
extern int deliberately_missing(void);
int dead_fn(void){return deliberately_missing();}
#else
int dead_fn(void){return dead_data[0]+dead_text[0];}
#endif
int (*volatile calls[])(void)={live};
int (*vectors[])(void)={handler};
int (*dead_tab[])(void)={dead_fn};
#ifdef ASM_CALL
extern int asm_get(void);
#endif
int main(void){
 if(calls[0]()!=19){puts("FAIL callback");return 1;}
#ifdef ASM_CALL
 if(asm_get()!=93){puts("FAIL assembly vector");return 2;}
#endif
 puts("PASS GC roots callbacks and data");return 0;}
'''
(work/'program.c').write_text(source)
(work/'asm.s').write_text('''.setcpu 6
.code
.export _asm_get
_asm_get:
 stx (-s)
 ldb (_vectors)
 jsr (b)
 ldx (s+)
 rsr
''')
for name,flags in [('ordinary',[]),('missing',['-DDEAD_UNDEFINED']),('assembly',['-DASM_CALL'])]:
    subprocess.run([root/'run/cpu6-bare-sdcc','-c','-O1','--sdcc-option=--cpu6-no-auto-inline',
                    *flags,work/'program.c','-o',work/(name+'.o')],check=True)
subprocess.run([root/'run/cpu6-bare-sdcc','-c',work/'asm.s','-o',work/'asm.o'],check=True)
# No original C or assembly is available when the saved objects are linked.
(work/'source.json').write_text(json.dumps({'source':source,'sha256':hashlib.sha256(source.encode()).hexdigest()},indent=2)+'\n')
(work/'program.c').unlink();(work/'asm.s').unlink()
relocated=work/'relocated';relocated.mkdir()
for obj in work.glob('*.o'):shutil.copy2(obj,relocated/obj.name);obj.unlink()
subprocess.run(['ar','qc',work/'bundle.a',relocated/'ordinary.o'],check=True)
rows=[];manifest=[]
def symbols(path):
    return {s[2]:int(s[0],16) for line in path.read_text().splitlines() if len(s:=line.split())==3}
for name,flags,inputs in [
 ('full',[],[relocated/'ordinary.o']),
 ('gc',['--gc-sections'],[relocated/'ordinary.o']),
 ('kept',['--gc-sections','--gc-keep=_vectors'],[relocated/'ordinary.o']),
 ('missing-dead',['--gc-sections'],[relocated/'missing.o']),
 ('archive',['--gc-sections'],[work/'bundle.a']),
 ('asm',['--gc-sections'],[relocated/'assembly.o',relocated/'asm.o'])]:
    binary=work/(name+'.bin');mapping=work/(name+'.map')
    with (work/(name+'-build.log')).open('w') as log:
        subprocess.run([root/'run/cpu6-bare-sdcc',*flags,*inputs,'--save-temps',work/name,
                        '--map',mapping,'-o',binary],stdout=log,stderr=subprocess.STDOUT,check=True)
    syms=symbols(mapping)
    if name=='full':
        assert all(n in syms for n in ['_dead_fn','_dead_data','_dead_text','_dead_tab','_vectors','_handler'])
    else:
        assert not any(n in syms for n in ['_dead_fn','_dead_data','_dead_text','_dead_tab'])
    if name in ('kept','asm'):
        assert all(n in syms for n in ['_vectors','_handler','_cookie'])
        raw=binary.read_bytes();offset=syms['_vectors']-0x2000
        assert int.from_bytes(raw[offset:offset+2],'big')==syms['_handler']
    elif name!='full':assert '_vectors' not in syms and '_handler' not in syms
    assert '_calls' in syms and '_live' in syms and '_used_data' in syms
    output=subprocess.check_output([root/'tests/centurion-cli','-A','2000','-T','100000000','-b',binary],text=True,timeout=120)
    assert 'PASS GC' in output and 'FAIL' not in output,(name,output)
    (work/(name+'.log')).write_text(output)
    rows.append({'variant':name,'bytes':binary.stat().st_size,'code':syms['__code_size'],
                 'data':syms['__data_size'],'bss':syms['__bss_size']})
    manifest.append(binary.name);print('PASS',name,rows[-1],flush=True)
assert rows[1]['code']<rows[0]['code'] and rows[1]['data']<rows[0]['data'] and rows[1]['bss']<rows[0]['bss']
p=subprocess.run([root/'run/cpu6-bare-sdcc','--gc-sections','--gc-keep=_dead_fn',relocated/'missing.o',
                  '-o',work/'must-fail.bin'],capture_output=True,text=True)
assert p.returncode and '_deliberately_mi' in p.stderr,p.stderr
(work/'missing-live.log').write_text(p.stderr)
# A plain link must honor explicit archive roots too, without enabling GC.
(work/'root.c').write_text('int external_root(void){return 731;}\n')
subprocess.run([root/'run/cpu6-bare-sdcc','-c',work/'root.c','-o',work/'root.o'],check=True)
subprocess.run(['ar','qc',work/'root.a',work/'root.o'],check=True)
(work/'root.c').unlink();(work/'root.o').unlink()
subprocess.run([root/'run/cpu6-bare-sdcc','--gc-keep=_external_root',relocated/'ordinary.o',
                work/'root.a','--map',work/'no-gc-root.map','-o',work/'no-gc-root.bin'],check=True)
assert '_external_root' in symbols(work/'no-gc-root.map')
# Editing the legacy bytes must not silently leave a different retained IR.
damaged=bytearray((relocated/'ordinary.o').read_bytes());damaged[108]^=1
try:
    unpack(damaged)
except ValueError as error:
    assert 'changed after' in str(error),error
else:raise AssertionError('stale object IR accepted')
(work/'manifest.json').write_text(json.dumps(manifest))
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node',root/'tests/compiler-audit-core.js',work,work/'manifest.json'],
                   stdout=log,stderr=subprocess.STDOUT,check=True,env=dict(os.environ,CPU6_MICROSTEP_LIMIT='100000000'),timeout=300)
(work/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
(root/'build/out/tests/latest-cpu6-gc.txt').write_text(str(work)+'\n')
