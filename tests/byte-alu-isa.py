#!/usr/bin/env python3
"""All byte input pairs for the five emitted ALU operations on both cores."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
work=Path(tempfile.mkdtemp(prefix='byte-alu-isa-',dir=root/'build/out/tests'))
print('Results:',work,flush=True)
forms=[(0x40,'add'),(0x48,'add'),(0x41,'sub'),(0x49,'sub'),
       (0x42,'and'),(0x4a,'and'),(0x43,'or'),(0x44,'xor')]
corners=[0,1,0x7f,0x80,0xff]
records=bytearray();expected=[]
for opcode,operation in forms:
    cases=[(a,b,0) for a in range(256) for b in range(256)]
    cases += [(a,b,flags<<4) for a in corners for b in corners for flags in range(1,16)]
    for a,b,flags in cases:
        regs=[0xa500|a,0x5a00|b,0x4321,0x5555,0x6666,0xd000,0x7777,0x8888]
        records += struct.pack('>BBBB8HBBHH',opcode,0x13,flags,0,*regs,0,0,0x5000,0xcafe)
        value={'add':a+b,'sub':a-b,'and':a&b,'or':a|b,'xor':a^b}[operation]&255
        expected.append((regs[0],0x5a00|value))
(work/'input.bin').write_bytes(records)
subprocess.run(['cc','-O2','-std=gnu11','-ffunction-sections','-fdata-sections',
                root/'tests/native-arithmetic-host.c','-Wl,--gc-sections','-o',work/'host'],check=True)
with (work/'input.bin').open('rb') as inp,(work/'instruction.bin').open('wb') as out:
    subprocess.run([work/'host'],stdin=inp,stdout=out,check=True)
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node',root/'tests/native-arithmetic-core.js',work/'input.bin',work/'microcode.bin'],
                   stdout=log,stderr=subprocess.STDOUT,check=True,timeout=240)
a,b=(work/'instruction.bin').read_bytes(),(work/'microcode.bin').read_bytes()
assert len(a)==len(b)==len(expected)*22
failures=[]
for i,(left,right) in enumerate(expected):
    one,two=a[i*22:(i+1)*22],b[i*22:(i+1)*22]
    if one!=two or struct.unpack('>HH',one[:4])!=(left,right):
        failures.append({'case':i,'input':records[i*26:(i+1)*26].hex(),'instruction':one.hex(),'microcode':two.hex()})
result={'cases':len(expected),'fullBytePairsPerForm':65536,'forms':len(forms),
        'failures':len(failures),'firstFailures':failures[:12],
        'inputSha256':hashlib.sha256(records).hexdigest()}
(work/'results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2),flush=True)
assert not failures
(root/'build/out/tests/latest-byte-alu-isa.txt').write_text(str(work)+'\n')
