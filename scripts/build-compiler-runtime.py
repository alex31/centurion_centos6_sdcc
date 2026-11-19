#!/usr/bin/env python3
"""Build lazy SDCC-generated helper calls not supplied by the FCC runtime."""
import argparse,os
from pathlib import Path
import shutil
import subprocess
import tempfile
from cpu6_archive import assemble_members
from cpu6_provenance import compiler_info,input_hashes,write_manifest
root = Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile',choices=['speed','size'],default='speed')
p.add_argument('--destination',type=Path,default=root/'build/out/toolchain/lib/cpu6')
args=p.parse_args();destination=args.destination.resolve()
destination.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='cpu6-compiler-runtime-') as temporary:
    work = Path(temporary)
    objects = []
    for index, source in enumerate(sorted((root / 'lib/compiler').glob('*.c'))):
        assembly = work / (source.stem + '.s')
        subprocess.run([root / 'build/out/toolchain/sdcc-cpu6/bin/sdcc', '-mcpu6', '-S', '--std=c99',
                        '--opt-code-'+args.profile, '-o', assembly, source], check=True)
        objects += assemble_members(assembly, work, root / 'build/out/toolchain/bin/asee200', 'crt%d' % index)
    archive = work / 'libcompiler-sdcc.a'
    subprocess.run(['ar', 'qc', archive, *objects], check=True)
    with tempfile.TemporaryDirectory(prefix='.stage-', dir=destination) as stage:
        staged = Path(stage) / archive.name
        shutil.copyfile(archive, staged)
        os.replace(staged, destination / archive.name)
    write_manifest(destination/archive.name,compiler_info(root/'build/out/toolchain/sdcc-cpu6/bin/sdcc'),
                   ['-mcpu6','--std=c99','--opt-code-'+args.profile],
                   input_hashes((root/'lib/compiler').glob('*.c')),profile=args.profile)
print('Built', destination / archive.name)
