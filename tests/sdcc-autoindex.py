#!/usr/bin/env python3
"""Run generated auto-index loops with guarded optimizations on and off."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
work=Path(tempfile.mkdtemp(prefix='sdcc-autoindex-',dir=root/'build/out/tests'))
print('Results:',work,flush=True)
manifest=[]
for name,flags in [('speed',['-O1']),('disabled',['-O1','--sdcc-option=--cpu6-no-autoindex']),
                   ('stack',['-O0']),('no-peep',['-O1','--no-peep']),
                   ('size',['-O1','--sdcc-option=--opt-code-size'])]:
    binary=work/(name+'.bin')
    with (work/(name+'-build.log')).open('w') as log:
        subprocess.run([root/'run/cpu6-bare-sdcc',*flags,root/'tests/cpu6-autoindex.c',
                        '--save-temps',work/name,'-o',binary],stdout=log,stderr=subprocess.STDOUT,check=True)
    asm=(work/name/'unit2.s').read_text()
    def body(function):
        return re.search(r'^_'+function+r':\n(.*?)(?=^_\w+:|\Z)',asm,re.M|re.S).group(1)
    if name in ['speed','size']:
        assert re.search(r'ldbb \([xyz]\+\)',body('copy8'))
        assert re.search(r'stbb \([xyz]\+\)',body('copy8'))
        assert re.search(r'ldb \([xyz]\+\)',body('copy16'))
        assert re.search(r'stb \([xyz]\+\)',body('copy16'))
        assert re.search(r'ldbb \(-[xyz]\)',body('back8'))
        assert re.search(r'ldb \(-[xyz]\)',body('back16'))
        # Nonadjacent arithmetic prevents these particular C offset loops
        # from fusing. Offset encodings/rules have separate exhaustive tests.
    for function in ['write_volatile','read_volatile']:
        assert not re.search(r'\([abxyz]\+\)|\(-[abxyz]\)',body(function)),(name,function)
    output=subprocess.check_output([root/'tests/centurion-cli','-A','2000','-T','100000000','-b',binary],text=True,timeout=120)
    (work/(name+'.log')).write_text(output)
    assert 'PASS autoindex loops' in output and 'FAIL' not in output and 'System halted' in output,name
    manifest.append(binary.name)
    print('PASS instruction core:',name,flush=True)
(work/'manifest.json').write_text(json.dumps(manifest))
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node',root/'tests/compiler-audit-core.js',work,work/'manifest.json'],
                   env=dict(os.environ,CPU6_MICROSTEP_LIMIT='300000000'),stdout=log,stderr=subprocess.STDOUT,check=True,timeout=300)
print('PASS autoindex loops on both CPU cores')
(root/'build/out/tests/latest-sdcc-autoindex-tests.txt').write_text(str(work)+'\n')
