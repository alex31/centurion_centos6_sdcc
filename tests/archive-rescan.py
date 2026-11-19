#!/usr/bin/env python3
"""Verify backward archive dependencies, cycles and unused-member exclusion."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
work=Path(tempfile.mkdtemp(prefix='archive-rescan-',dir=root/'build/out/tests'))
for name,definition in [('root','start:\n\t.word _a'),('a','_a:\n\t.word _b'),
                        ('b','_b:\n\t.word _c'),('c','_c:\n\t.word _a'),
                        ('unused','_unused:\n\t.ds 500')]:
    symbol='start' if name=='root' else '_'+name
    (work/(name+'.s')).write_text('\t.setcpu 6\n\t.code\n\t.export '+symbol+'\n'+definition+'\n')
    subprocess.run([root/'build/out/toolchain/bin/asee200','-o',work/(name+'.o'),work/(name+'.s')],check=True)
subprocess.run(['ar','qc',work/'chain.a',*[work/(s+'.o') for s in ['unused','c','b','a']]],check=True)
subprocess.run([root/'build/out/toolchain/bin/ldee200','-b','-C','512','-o',work/'image.bin','-m',work/'image.map',work/'root.o',work/'chain.a'],check=True)
symbols={row[2]:int(row[0],16) for line in (work/'image.map').read_text().splitlines() if len(row:=line.split())==3}
image=(work/'image.bin').read_bytes()
for a,b in [('start','_a'),('_a','_b'),('_b','_c'),('_c','_a')]:
    assert int.from_bytes(image[symbols[a]:symbols[a]+2],'big')==symbols[b]
assert '_unused' not in symbols and len(image)==520
print('PASS archive rescans, cyclic references and unused member exclusion:',work)
