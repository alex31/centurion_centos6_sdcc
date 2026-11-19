#!/usr/bin/env python3
"""Explicit A/B argument ABI, cross-unit pointers and calls through the default stack ABI."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
work=Path(tempfile.mkdtemp(prefix='sdcc-regcall-',dir=root/'build/out/tests'))
print('Results:',work,flush=True)
manifest=[]
for name,flags in [('speed',['-O1']),('stack',['-O0']),('size',['-O1','--sdcc-option=--opt-code-size']),('no-peep',['-O1','--no-peep']),('inlining',['-O1'])]:
    binary=work/(name+'.bin')
    with (work/(name+'-build.log')).open('w') as log:
        subprocess.run([root/'run/cpu6-bare-sdcc',*flags,'-I'+str(root/'os6/include'),*(['--sdcc-option=--cpu6-no-auto-inline'] if name!='inlining' else []),
                        root/'tests/cpu6-regcall.c',root/'tests/cpu6-regcall-functions.c',
                        root/'tests/stack-abi.s',root/'tests/cpu6-read-stack.s','--save-temps',work/name,'-o',binary],
                       stdout=log,stderr=subprocess.STDOUT,check=True)
    output=subprocess.check_output([root/'tests/centurion-cli','-A','2000','-T','100000000','-b',binary],text=True,timeout=120)
    (work/(name+'.log')).write_text(output)
    assert 'PASS register calls' in output and 'FAIL' not in output,(name,output)
    manifest.append(binary.name);print('PASS instruction core:',name,flush=True)
(work/'manifest.json').write_text(json.dumps(manifest))
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node',root/'tests/compiler-audit-core.js',work,work/'manifest.json'],
                   env=dict(os.environ,CPU6_MICROSTEP_LIMIT='300000000'),stdout=log,stderr=subprocess.STDOUT,check=True,timeout=240)
(work/'bad.c').write_text('int f(int) __sdcccall(1); int f(int x) __sdcccall(0) {return x;}\n')
bad=subprocess.run([root/'build/out/toolchain/sdcc-cpu6/bin/sdcc','-mcpu6','-S',work/'bad.c','-o',work/'bad.s'],text=True,capture_output=True)
(work/'bad.log').write_text(bad.stdout+bad.stderr)
assert bad.returncode!=0,'incompatible ABI redeclaration accepted'
print('PASS register ABI on both cores and mismatch diagnostic')
(root/'build/out/tests/latest-sdcc-regcall.txt').write_text(str(work)+'\n')
