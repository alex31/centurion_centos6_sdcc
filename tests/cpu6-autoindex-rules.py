#!/usr/bin/env python3
"""Exercise real peephole matching, alias/flag/volatile guards and offsets."""
from pathlib import Path
import json
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
build = root / 'build/out/sdcc'
source = root / 'swext/sdcc/sdcc'
work = Path(tempfile.mkdtemp(prefix='autoindex-rules-', dir=root/'build/out/tests'))
print('Results:', work, flush=True)
subprocess.run(['objcopy','--redefine-sym','main=sdcc_cli_main',build/'src/SDCCmain.o',work/'cli.o'],check=True)
subprocess.run(['cc','-std=gnu11','-I'+str(source/'src'),'-I'+str(build/'src'),'-I'+str(build),
                '-I'+str(source/'support/util'),'-c',root/'tests/cpu6-peep-engine.c','-o',work/'driver.o'],check=True)
objects=[p for p in sorted((build/'src').glob('*.o')) if p.name != 'SDCCmain.o']
subprocess.run(['g++','-o',work/'driver',work/'driver.o',work/'cli.o',*objects,build/'src/cpu6/port.a','-lm'],check=True)
tests=[]
def check(lines, expected, present=True):
    number=len(tests)
    (work/'input.s').write_text('\n'.join(lines)+'\n')
    subprocess.run([work/'driver',work/'input.s',work/'output.s'],check=True)
    output=(work/'output.s').read_text()
    assert (expected in output)==present, (lines,expected,present,output)
    tests.append({'input':lines,'expected':expected,'present':present,'output':output})

operations=[('ldab','al',1),('ldbb','bl',1),('stab','al',1),('stbb','bl',1),
            ('lda','a',2),('ldb','b',2),('sta','a',2),('stb','b',2),('ldx','x',2),('stx','x',2)]
for op,value,size in operations:
    for base in 'abxyz':
        alias=base==value[0]
        for offset in ['', '0', '-128', '-1', '1', '127']:
            address=offset+'('+base+')'
            for pre in (False,True):
                updates=[('dcr ' if pre else 'inr ')+base]*size
                access=op+' '+address
                window=updates+[access] if pre else [access]+updates
                normalized='' if offset=='0' else offset
                expected=op+' '+normalized+('(-'+base+')' if pre else '('+base+'+)')
                check(window+['rsr'],expected,not alias)
        if not alias:
            for flag in ['bz','bm','bl','bf']:
                check([op+' ('+base+')']+['inr '+base]*size+[flag+' Ldone','Ldone:','rsr'],
                      op+' ('+base+'+)',False)
            check(['!'+op+' ('+base+')']+['inr '+base]*size+['rsr'],op+' ('+base+'+)',False)
            check(['@'+op+' ('+base+')']+['inr '+base]*size+['rsr'],op+' ('+base+'+)',False)
            for offset in ['128','-129','_symbol']:
                check([op+' '+offset+'('+base+')']+['inr '+base]*size+['rsr'],
                      op+' '+offset+'('+base+'+)',False)
for load,store,size in [('ldab','stab',1),('ldbb','stbb',1),('lda','sta',2),('ldb','stb',2)]:
    check([load+' 12(x)',store+' 0(s)']+['inr x']*size+['rsr'],load+' 12(x+)')
    check([load+' 12(x)',store+' (y)']+['inr x']*size+['rsr'],load+' 12(x+)',False)
    check([load+' 12(x)','!'+store+' 0(s)']+['inr x']*size+['rsr'],load+' 12(x+)',False)
    check([load+' (s)']+['inr s']*size+['rsr'],load+' (s+)',False)

# Byte cleanup must preserve a live high half, live flags and volatile tags.
check(['clrb bh','stbb (x)','ldb 0','rsr'],'clrb bh',False)
check(['clrb bh','stb (_out)','ldb 0','rsr'],'clrb bh')
check(['clrb bh','stbb (x)','bl Ldone','Ldone:','ldb 0','rsr'],'clrb bh')
check(['addb al,bl','orib bl,bl','rsr'],'orib bl,bl',False)
check(['mul a,b','orib bl,bl','rsr'],'orib bl,bl')
check(['!ldbb (x)','orib bl,bl','inr x','rsr'],'(x+)',False)

# Index-register arithmetic can bypass a dead A or B without changing flags.
for register in 'xyz':
    for operand in 'axyz':
        code = [f'xfr {register},b',f'add {operand},b',f'xfr b,{register}']
        check(code+['bl Ldone','Ldone:','ldb 0','rsr'],f'add {operand},{register}')
        check(code+['rsr'],f'add {operand},{register}',False)
    code = [f'xfr {register},a','ldb _limit','sab']
    check(code+['bf Ldone','Ldone:','rsr'],f'sub {register},b')
    check(code+['sta (_saved)','rsr'],f'sub {register},b',False)
check(['xfr y,b','add b,b','xfr b,y','ldb 0','rsr'],'add b,y',False)
for source in ['(a)', '(x+)', '(-x)']:
    check(['xfr x,a','ldb '+source,'sab','rsr'],'sub x,b',False)
check(['xfr x,a','!ldb (_limit)','sab','rsr'],'sub x,b',False)

(work/'results.json').write_text(json.dumps({'result':'PASS','cases':len(tests),'tests':tests},indent=2)+'\n')
print('PASS',len(tests),'actual peephole engine cases')
(root/'build/out/tests/latest-autoindex-rules.txt').write_text(str(work)+'\n')
