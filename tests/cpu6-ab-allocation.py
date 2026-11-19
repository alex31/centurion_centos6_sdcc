#!/usr/bin/env python3
"""Exercise actual A/B allocation guards, byte aliases, flags and ABI liveness."""
import json
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
build=root/'build/out/sdcc'
source=root/'swext/sdcc/sdcc'
work=Path(tempfile.mkdtemp(prefix='ab-allocation-',dir=root/'build/out/tests'))
print('Results:',work,flush=True)
subprocess.run(['objcopy','--redefine-sym','main=sdcc_cli_main',build/'src/SDCCmain.o',work/'cli.o'],check=True)
subprocess.run(['cc','-std=gnu11','-I'+str(source/'src'),'-I'+str(build/'src'),'-I'+str(build),
                '-I'+str(source/'support/util'),'-c',root/'tests/cpu6-peep-engine.c','-o',work/'driver.o'],check=True)
objects=[p for p in sorted((build/'src').glob('*.o')) if p.name!='SDCCmain.o']
subprocess.run(['g++','-o',work/'driver',work/'driver.o',work/'cli.o',*objects,build/'src/cpu6/port.a','-lm'],check=True)
tests=[]
def check(lines,changed):
    (work/'input.s').write_text('\n'.join(lines)+'\n')
    subprocess.run([work/'driver',work/'input.s',work/'before.s'],check=True)
    subprocess.run([work/'driver',work/'input.s',work/'after.s','--allocate-ab'],check=True)
    before,after=(work/'before.s').read_text(),(work/'after.s').read_text()
    assert (before!=after)==changed,(lines,changed,before,after)
    tests.append({'input':lines,'changed':changed,'before':before,'after':after})

for reg in 'xyz':
    kill='ldx 0' if reg=='x' else 'clr '+reg
    for accumulator in 'ab':
        other='b' if accumulator=='a' else 'a'
        copy=f'xfr {accumulator},{reg}'
        tail=[kill,'rsr']
        for unary in ('inr','dcr','ivr','slr','srr'):
            check([copy,f'{unary} {reg}',f'xfr {reg},{accumulator}',*tail],True)
        check([copy,f'ld{accumulator} ({reg})',*tail],True)
        for barrier in ('Ljoin:','@nop','jsr _opaque','mul a,b','div b,a'):
            check([copy,barrier,f'xfr {reg},{accumulator}',*tail],False)
        for branch in ('bz','bnz','bm','bp','bl','bnl','bf','bnf'):
            check([copy,branch+' Ljoin',f'inr {reg}','Ljoin:',f'xfr {reg},{accumulator}',*tail],False)
        # Both original scratch values remain live, including a byte alias.
        check([copy,f'inr {reg}',f'xfr {reg},{"z" if reg!="z" else "y"}',
               'sta (_asink)','stb (_bsink)',*tail],False)
        check([copy,f'st{other} (_sink)',f'clrb {accumulator}h',f'xfr {reg},{other}',*tail],False)
        # Unknown calls and external tail jumps may consume register arguments.
        check([copy,f'inr {reg}',f'xfr {reg},{"z" if reg!="z" else "y"}',
               kill,'jsr _regcallee','rsr'],False)
        check([copy,f'inr {reg}',f'xfr {reg},{"z" if reg!="z" else "y"}',
               kill,'jmp _regcallee'],False)

# Recolor into A when B is busy; the original A must be dead before overwrite.
check(['xfr b,x','ldb 9','xfr x,a','add a,b','ldx 0','rsr'],True)
check(['xfr b,x','sta (_livea)','ldb 9','xfr x,a','add a,b','ldx 0','rsr'],False)
(work/'results.json').write_text(json.dumps({'result':'PASS','cases':len(tests),'tests':tests},indent=2)+'\n')
print('PASS',len(tests),'A/B allocation engine cases',flush=True)
(root/'build/out/tests/latest-ab-allocation.txt').write_text(str(work)+'\n')
