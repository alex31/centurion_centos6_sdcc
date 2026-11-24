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

# Signed byte promotion must retain the arithmetic flags when they are live.
sext = ['clrb bh', 'orib bl,bl', 'bp Lpositive', 'dcrb bh', 'Lpositive:']
for value in ('0','127','-1','-128','0xff80'):
    check(['ldb '+value, *sext, 'rsr'], 'Lpositive:\nrsr')
    check(['ldb '+value, *sext, 'rsr'], 'srrb bh', False)
check(['ldb 128', *sext, 'rsr'], 'srrb bh,7')
check(['ldbb (x)', *sext, 'bl Ldone', 'Ldone:', 'rsr'], 'clrb bh\nldbb (x)\nbp')
check(['ldbb (b)', *sext, 'bl Ldone', 'Ldone:', 'rsr'], 'orib bl,bl')
check(['!ldbb (x)', *sext, 'bl Ldone', 'Ldone:', 'rsr'], 'orib bl,bl')
check([*sext, 'stbb (x)', 'ldb 0', 'rsr'], 'srrb bh', False)
check([*sext, 'bm Ldone', 'Ldone:', 'rsr'], 'orib bl,bl')

equal = [*sext, 'sub 65410,b', 'bz Ldone','Ldone:', 'ldb 7', 'rsr']
check(equal, 'ldab <65410')
check(['bz Lpositive', *equal], 'ldab <65410', False)
check([*sext, 'sub 128,b', 'ldb 7', 'rsr'], 'ldab <128', False)
check([*sext, 'sub 2,b', 'bl Ldone', 'Ldone:', 'ldb 7', 'rsr'], 'ldab <2', False)
check([*sext, 'sub 2,b', 'rsr'], 'ldab <2', False)
check([*sext, 'sub 2,b', 'stab (x)', 'ldb 7', 'rsr'], 'ldab <2', False)

for op in ('add','and','ori','ore'):
    check(['ldb (_value)', f'{op} x,b', 'rsr'], f'{op} x,(_value),b')
    check(['xfr y,b', f'{op} 31,b', 'rsr'], f'{op} y,31,b')
check(['xfr y,b', 'sub 31,b', 'rsr'], 'sub y,31,b')
check(['xfr y,b', 'sub x,31,b', 'rsr'], 'sub y,x,31,b', False)
check(['ldb 4(s)', 'add b,b', 'rsr'], 'add b,4(s),b', False)
check(['ldb (s+)', 'ldb (b)', 'rsr'], 'ldb @(s+)', False)
check(['ldb @4(s)', 'ldb (b)', 'rsr'], 'ldb @@4(s)', False)
check(['!ldb 4(s)', 'ldb (b)', 'rsr'], 'ldb @4(s)', False)
check(['ldb 4(s)', 'ldb (b)', 'rsr'], 'ldb @4(s)')
check(['ldb 4(s)', 'ldbb (b)', 'clrb bh', 'rsr'], 'ldbb @4(s)')
check(['ldb 4(s)', 'ldbb (b)', 'rsr'], 'ldbb @4(s)', False)
check(['xfr x,b', 'xfr b,y', 'ldb 4', 'rsr'], 'xfr x,y')
check(['xfr x,b', '@xfr b,y', 'ldb 4', 'rsr'], 'xfr x,y', False)
check(['ldb 2(s)', '@ldb (b)', 'rsr'], 'ldb @2(s)', False)
check(['stb (-s)', 'lda (__hireg)', '@sta (-s)', 'rsr'], 'push ah,bl', False)

check(['inr s,2', 'stb (-s)', 'rsr'], 'stb (s)')
check(['inr s,2', 'stb (-s)', 'bf Ldone', 'Ldone:', 'rsr'], 'inr s,2')
check(['stb (-s)', 'lda (__hireg)', 'sta (-s)', 'rsr'], 'push ah,bl')
check(['stb (-s)', '!lda (__hireg)', 'sta (-s)', 'rsr'], 'push ah,bl', False)
for reg in 'xyz':
    push = 'stx (-s)' if reg == 'x' else f'push {reg}h,{reg}l'
    check([f'xfr {reg},b','stb (-s)','ldb 7','rsr'],push)
    check([f'xfr {reg},b','stb (-s)','rsr'],push,False)

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

# Run a complete kernel through both profiles, then check the actual linked
# lengths and results. Only its assembly changes between the two binaries.
kernel = ['_probe:', 'stx (-s)', 'push yh,yl', 'ldx _slot', 'ldb 2',
          'lda (x)', 'aab', 'stb (_sum)', 'ldb 0(x)', 'xfr b,y', 'ldb 3',
          'stb (x)', 'xfr y,a', 'sta (x)', 'lda 7', 'ldb (_sum)', 'pop yh,yl',
          'ldx (s+)', 'rsr']
(work/'kernel.in').write_text('\n'.join(kernel)+'\n')
(work/'kernel.c').write_text("""#include <stdio.h>
unsigned slot, sum, probe(void);
int main(void) {
    slot=9;
    if (probe()!=11 || slot!=9 || sum!=11) { puts("FAIL compact addressing"); return 1; }
    puts("PASS compact addressing"); return 0;
}
""")
sizes = {}
for profile, args in [('speed', []), ('size', ['--size'])]:
    asm = work/(profile+'.s')
    subprocess.run([work/'driver',work/'kernel.in',asm,*args],check=True)
    text = asm.read_text()
    for pattern in ['add (x),b', 'ldw y,0(x)', 'stw y,(x)']:
        assert (pattern in text) == (profile=='speed'), (profile, text)
    asm.write_text('.setcpu 6\n.code\n.export _probe\n'+text)
    binary = work/(profile+'.bin')
    subprocess.run([root/'run/cpu6-bare-sdcc','-O1',work/'kernel.c',asm,'-o',binary], check=True)
    result = subprocess.check_output([root/'tests/centurion-cli','-A','2000','-b',binary],text=True,timeout=10)
    assert 'PASS compact addressing' in result, result
    sizes[profile] = binary.stat().st_size
assert sizes['size'] < sizes['speed'], sizes
print('Compact addressing kernel executable bytes:', sizes, flush=True)

# In a read/modify/write fold, the old B and reloaded B must denote the
# same address. The CPU6 memory unary form also cannot index through A.
for address in ('(b)', '2(b)', '(a)'):
    check(['ldb '+address, 'inr b', 'stb '+address, 'ldb 0', 'rsr'], 'inr '+address, False)

for first, last in [('!ldb (x)', 'stb (x)'), ('ldb (x)', '!stb (x)')]:
    check([first, 'inr b', last, 'ldb 0', 'rsr'], 'inr (x)', False)

def rotate(lines):
    (work/'input.s').write_text('\n'.join(lines)+'\n')
    subprocess.run([work/'driver', work/'input.s', work/'output.s', '--loops'], check=True)
    return (work/'output.s').read_text().splitlines()

# B is dead inside the countdown but carries the final zero to the caller.
countdown = ['Lhead:', 'xfr x,b', 'bz Lexit', 'ldb 9', 'dcr x', 'bra Lhead', 'Lexit:']
assert rotate(countdown + ['rsr']).count('xfr x,b') == 2
assert rotate(countdown + ['ldb 0', 'rsr']).count('xfr x,b') == 1

# Hoisting CLR ahead of RLR would destroy the incoming link on the first trip.
body = ['rlr x', 'clr y', 'stw y,(_out)', 'add x,x', 'dcr z']
rotated = rotate(['Lhead:', 'ori z,z', 'bz Lexit', *body, 'bra Lhead', 'Lexit:', 'rsr'])
assert rotated.index('rlr x') < rotated.index('clr y'), rotated

# An otherwise profitable rotation would push a separate short branch out of
# range. Exercise both directions and a smaller case that fits.
loop = ['Lhead:', 'lda 37', 'bz Lexit', 'nop', 'bra Lhead', 'Lexit:']
forward = ['bz Lfar', *loop, *['nop']*117, 'Lfar:', 'rsr']
backward = ['Lfar:', *['nop']*116, *loop, 'bz Lfar', 'rsr']
assert 'jz Lfar' in rotate(forward)
assert 'jz Lfar' in rotate(backward)
close = ['bz Lfar', *loop, *['nop']*100, 'Lfar:', 'rsr']
assert 'bz Lfar' in rotate(close)
# A branch shortened before the extended ALU folds spans 128 bytes afterward.
check(['bz Lskip', 'jmp Lend', 'Lskip:', *(['lda (x)', 'aab']*4),
       *['nop']*112, 'Lend:', 'rsr'], 'jnz Lend')

# Check every emitted pseudo against the EE200 assembler's actual bytes.
# Its JLE/JGT spellings correspond to opcodes BGZ/BLE respectively.
for n, op in enumerate(('bl', 'bnl', 'bf', 'bnf', 'bz', 'bnz', 'bm', 'bp', 'bgz', 'ble')):
    lines = rotate([op+' Lfar', *['nop']*128, 'Lfar:', 'rsr'])
    assert lines[0] != op+' Lfar'
    (work/'branch.s').write_text('\n'.join(['.setcpu 6', '.code', *lines])+'\n')
    tools = root/'build/out/toolchain/bin'
    subprocess.run([tools/'asee200', '-o', work/'branch.o', work/'branch.s'], check=True)
    subprocess.run([tools/'ldee200', '-b', '-C', '0', '-o', work/'branch.bin', work/'branch.o'], check=True)
    assert (work/'branch.bin').read_bytes()[:5] == bytes([(0x10+n)^1, 3, 0x71, 0, 133]), lines[0]

(work/'results.json').write_text(json.dumps({'result':'PASS','cases':len(tests),'tests':tests},indent=2)+'\n')
print('PASS',len(tests),'actual peephole engine cases')
(root/'build/out/tests/latest-autoindex-rules.txt').write_text(str(work)+'\n')
