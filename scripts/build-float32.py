#!/usr/bin/env python3
"""Build the explicitly enabled CPU6 binary32 runtime as independent members."""
from pathlib import Path
import argparse,os
import shutil
import subprocess
import tempfile
from cpu6_archive import assemble_members
from cpu6_provenance import compiler_info,input_hashes,write_manifest

root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile',choices=['speed','size'],default='speed')
p.add_argument('--destination',type=Path,default=root/'build/out/toolchain/lib/cpu6')
args=p.parse_args();destination=args.destination.resolve();destination.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='cpu6-float32-',dir=root/'build') as temporary:
    work=Path(temporary);objects=[]
    for index,source in enumerate(sorted((root/'lib/float32').glob('*.c'))):
        assembly=work/(source.stem+'.s')
        subprocess.run([root/'build/out/toolchain/sdcc-cpu6/bin/sdcc','-mcpu6','--cpu6-float32','-S',
                        '--std=c99','--opt-code-'+args.profile,'-DCPU6_FLOAT_ABI_THUNKS','-I'+str(root/'baremetal/include'),
                        '-I'+str(root/'lib/float32'),'-o',assembly,source],check=True)
        objects+=assemble_members(assembly,work,root/'build/out/toolchain/bin/asee200','f32%02d'%index)
    objects+=assemble_members(root/'lib/float32/abi-cpu6.s',work,root/'build/out/toolchain/bin/asee200','f32abi')
    objects+=assemble_members(root/'lib/float32/significand-cpu6.s',work,root/'build/out/toolchain/bin/asee200','f32sig')
    archive=work/'libfloat32-sdcc.a'
    subprocess.run(['ar','qc',archive,*objects],check=True)
    with tempfile.TemporaryDirectory(prefix='.stage-',dir=destination) as stage:
        target=Path(stage)/archive.name;shutil.copy2(archive,target);os.replace(target,destination/archive.name)
    write_manifest(destination/archive.name,compiler_info(root/'build/out/toolchain/sdcc-cpu6/bin/sdcc'),
                   ['-mcpu6','--cpu6-float32','--std=c99','--opt-code-'+args.profile,'-DCPU6_FLOAT_ABI_THUNKS'],
                   input_hashes([*list((root/'lib/float32').glob('*.[chs]')),*list((root/'baremetal/include').glob('*.h'))]),profile=args.profile)
print('Built',destination/'libfloat32-sdcc.a')
