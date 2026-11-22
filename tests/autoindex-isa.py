#!/usr/bin/env python3
"""Validate auto-index encodings and full CPU states against microcode."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
work = Path(tempfile.mkdtemp(prefix='autoindex-isa-', dir=root / 'build/out/tests'))
print('Results:', work, flush=True)
operations = [('ldab',0x85,1),('ldbb',0xc5,1),('stab',0xa5,1),('stbb',0xe5,1),
              ('lda',0x95,2),('ldb',0xd5,2),('sta',0xb5,2),('stb',0xf5,2),
              ('ldx',0x65,2),('stx',0x6d,2)]
registers = ['a','b','x','y','z','s']
offsets = [None,-128,-127,-2,-1,0,1,2,126,127]
assembly = ['.setcpu 6','.code']
expected = bytearray()
records = bytearray()
descriptions = []
for mnemonic,opcode,size in operations:
    for index,base in enumerate(registers):
        for predec in (False,True):
            for offset in offsets:
                address = ('(-' + base + ')') if predec else ('(' + base + '+)')
                if offset is not None: address = str(offset) + address
                assembly.append(mnemonic + ' ' + address)
                mode = index * 32 + (2 if predec else 1) + (8 if offset is not None else 0)
                encoded = bytes([opcode,mode]) + (bytes([offset & 255]) if offset is not None else b'')
                expected += encoded
                for flags in range(16):
                    for level in (0,3,15):
                        for value in [0,0x007f,0x0080,0x7fff,0x8000,0xabcd,0xff00,0xffff]:
                            regs = [0x1357,0x2468,0x4321,0x5555,0x6666,0xd000,0x7777,0x8888]
                            regs[index] = 0x5000
                            effective = 0x5000 - (size if predec else 0) + (offset or 0)
                            records += struct.pack('>BBBB8HBBHH',opcode,mode,flags << 4,level,*regs,
                                                   (offset or 0) & 255,0,effective,value)
                            descriptions.append((mnemonic,address,flags,level,value))
        # Indirect accesses step over a pointer even for a byte operation.
        # The pointer targets the known instruction-extension bytes in RAM.
        for pointer_mode, operand in [(0, f'({base})'), (1, f'({base}+)'), (2, f'(-{base})')]:
            for offset in (None, -3, 5):
                address = '@' + (str(offset) if offset is not None else '') + operand
                assembly.append(mnemonic + ' ' + address)
                mode = index * 32 + 4 + pointer_mode + (8 if offset is not None else 0)
                expected.extend([opcode, mode])
                if offset is not None:
                    expected.append(offset & 255)
                regs = [0x1357,0x2468,0x4321,0x5555,0x6666,0xd000,0x7777,0x8888]
                regs[index] = 0x5000
                effective = 0x5000 - (2 if pointer_mode == 2 else 0) + (offset or 0)
                records += struct.pack('>BBBB8HBBHH', opcode, mode, 0xf0, 0, *regs,
                                       (offset or 0) & 255, 0xa5, effective, 0x0202)
                descriptions.append((mnemonic, address, 15, 0, 0x0202))
(work/'all.s').write_text('\n'.join(assembly)+'\n')
subprocess.run([root/'build/out/toolchain/bin/asee200','-o',work/'all.o',work/'all.s'],check=True)
subprocess.run([root/'build/out/toolchain/bin/ldee200','-b','-C','0','-o',work/'all.bin',work/'all.o'],check=True)
assert (work/'all.bin').read_bytes()==expected, 'auto-index encoding differs'
(work/'input.bin').write_bytes(records)
subprocess.run(['cc','-O2','-std=gnu11','-ffunction-sections','-fdata-sections',
                root/'tests/native-arithmetic-host.c','-Wl,--gc-sections','-o',work/'host'],check=True)
with (work/'input.bin').open('rb') as inp, (work/'instruction.bin').open('wb') as out:
    subprocess.run([work/'host'],stdin=inp,stdout=out,check=True)
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node',root/'tests/native-arithmetic-core.js',work/'input.bin',work/'microcode.bin'],
                   stdout=log,stderr=subprocess.STDOUT,check=True,timeout=240)
a,b=(work/'instruction.bin').read_bytes(),(work/'microcode.bin').read_bytes()
assert len(a)==len(b)==len(descriptions)*22
failures=[]
for i,case in enumerate(descriptions):
    if a[i*22:(i+1)*22]!=b[i*22:(i+1)*22]:
        failures.append({'case':case,'instruction':a[i*22:(i+1)*22].hex(),'microcode':b[i*22:(i+1)*22].hex()})
result={'cases':len(descriptions),'encodings':len(assembly)-2,'failures':len(failures),
        'firstFailures':failures[:20], 'inputSha256':hashlib.sha256(records).hexdigest()}
(work/'results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2),flush=True)
assert not failures
(root/'build/out/tests/latest-autoindex-isa.txt').write_text(str(work)+'\n')
