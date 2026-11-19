#!/usr/bin/env python3
"""Verify private cross-member references, quoted data and omitted members."""
from pathlib import Path
import subprocess
import sys
import tempfile
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'scripts'))
from cpu6_archive import assemble_members
work=Path(tempfile.mkdtemp(prefix='cpu6-archive-',dir=root/'build/out/tests'))
source=work/'library.s'
source.write_text('''\t.setcpu 6
\t.export _entry
\t.export _unused
\t.data
_message:
\t.ascii "_helper"
\t.byte 0
; CPU6 function _entry begin
_entry:
\t.word _helper
; CPU6 function _entry end
; CPU6 function _helper begin
_helper:
\t.word _message
; CPU6 function _helper end
; CPU6 function _unused begin
_unused:
\t.ds 500
; CPU6 function _unused end
''')
objects=assemble_members(source,work,root/'build/out/toolchain/bin/asee200','test')
subprocess.run(['ar','qc',work/'library.a',*objects],check=True)
(work/'root.s').write_text('\t.setcpu 6\n\t.code\n\t.export start\nstart:\n\t.word _entry\n')
subprocess.run([root/'build/out/toolchain/bin/asee200','-o',work/'root.o',work/'root.s'],check=True)
subprocess.run([root/'build/out/toolchain/bin/ldee200','-b','-C','512','-o',work/'image.bin','-m',work/'image.map',work/'root.o',work/'library.a'],check=True)
image=(work/'image.bin').read_bytes()
def word(pos):return int.from_bytes(image[pos:pos+2],'big')
entry=word(512);helper=word(entry);message=word(helper)
assert image[message:message+8]==b'_helper\0'
assert '_unused' not in (work/'image.map').read_text()
assert len(image)<600
(work/'old.s').write_text('\t.setcpu 6\n\t.code\n\t.export _old\n_old:\n\trsr\n')
assert len(assemble_members(work/'old.s',work,root/'build/out/toolchain/bin/asee200','old'))==1
(work/'bad.s').write_text('; CPU6 function _bad begin\n_bad:\n\trsr\n')
try:
    assemble_members(work/'bad.s',work,root/'build/out/toolchain/bin/asee200','bad')
    raise AssertionError('incomplete function markers accepted')
except ValueError:
    pass
print('PASS split archive references, quoted data, unused exclusion and marker validation:',work)
