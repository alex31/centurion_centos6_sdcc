#!/usr/bin/env python3
"""Deep mutual/value recursion and argument-area reuse, with safe fallbacks."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
work=Path(tempfile.mkdtemp(prefix='sdcc-terminal-',dir=root/'build/out/tests'))
print('Results:',work,flush=True)
manifest=[]
for name,flags in [('speed',['-O1','-DEXPECT_TAIL']),('disabled',['-O1','--sdcc-option=--cpu6-no-tail-calls','--sdcc-option=--cpu6-no-tail-recursion']),
                   ('stack',['-O0']),('size',['-O1','-DEXPECT_TAIL','--sdcc-option=--opt-code-size']),('no-peep',['-O1','-DEXPECT_TAIL','--no-peep'])]:
    binary=work/(name+'.bin'); enabled=name not in ('disabled','stack')
    with (work/(name+'-build.log')).open('w') as log:
        subprocess.run([root/'run/cpu6-bare-sdcc',*flags,'--native-int48','--sdcc-option=--cpu6-no-auto-inline',
                        root/'tests/cpu6-terminal-calls.c',root/'tests/cpu6-read-stack.s',
                        '--save-temps',work/name,'-o',binary],stdout=log,stderr=subprocess.STDOUT,check=True)
    asm=(work/name/'unit2.s').read_text()
    def body(name):return re.search(r'^_'+name+r':\n(.*?)(?=^_\w+:|\Z)',asm,re.M|re.S).group(1)
    for caller,callee in [('chain_a','chain_b'),('chain_b','chain_a'),('forward','receive'),('fewer','one')]:
        assert ('jmp _'+callee in body(caller))==enabled,(name,caller)
    for fn in ['accumulate','wide_state']:
        assert ('jsr _'+fn in body(fn))!=enabled,(name,fn)
    for caller,callee in [('more','two'),('escape','read_local'),('changed_return','receive')]:
        assert 'jsr _'+callee in body(caller),(name,caller)
    output=subprocess.check_output([root/'tests/centurion-cli','-A','2000','-T','200000000','-b',binary],text=True,timeout=120)
    (work/(name+'.log')).write_text(output)
    assert 'PASS terminal calls' in output and 'FAIL' not in output and 'System halted' in output,(name,output)
    manifest.append(binary.name);print('PASS instruction core:',name,flush=True)
(work/'manifest.json').write_text(json.dumps(manifest))
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node',root/'tests/compiler-audit-core.js',work,work/'manifest.json'],
                   env=dict(os.environ,CPU6_MICROSTEP_LIMIT='500000000'),stdout=log,stderr=subprocess.STDOUT,check=True,timeout=300)
print('PASS terminal-call variants on both CPU cores')
(root/'build/out/tests/latest-sdcc-terminal.txt').write_text(str(work)+'\n')
