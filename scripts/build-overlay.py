#!/usr/bin/env python3
"""Build the resident SDCC interface to CENTOS's existing CPL LOAD service."""
import argparse,os
from pathlib import Path
import shutil
import subprocess
import tempfile
from cpu6_archive import assemble_members
from cpu6_object import attach
from cpu6_provenance import compiler_info,input_hashes,write_manifest

root = Path(__file__).resolve().parents[1]
tools = root / 'build/out/toolchain'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile',choices=['speed','size'],default='speed')
p.add_argument('--destination',type=Path,default=tools/'lib/os6')
args=p.parse_args();destination=args.destination.resolve()
destination.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='cpu6-overlay-lib-') as temporary:
    work = Path(temporary)
    assembly = work / 'overlay.s'
    subprocess.run([tools / 'sdcc-cpu6/bin/sdcc', '-mcpu6', '-S', '--std=c99',
                    '--opt-code-'+args.profile, '-I' + str(root / 'baremetal/include'),
                    '-I' + str(root / 'os6/include'), '-o', assembly,
                    root / 'os6/overlay.c'], check=True)
    objects = assemble_members(assembly, work, tools / 'bin/asee200', 'ov')
    raw = work / 'load.o'
    source = root / 'os6/overlay-load.s'
    subprocess.run([tools / 'bin/asee200', '-o', raw, source], check=True)
    attach(raw, source.read_text())
    archive = work / 'liboverlay-sdcc.a'
    subprocess.run(['ar', 'qc', archive, *objects, raw], check=True)
    with tempfile.TemporaryDirectory(prefix='.stage-', dir=destination) as stage:
        staged = Path(stage) / archive.name
        shutil.copyfile(archive, staged)
        os.replace(staged, destination / archive.name)
    write_manifest(destination/archive.name,compiler_info(tools/'sdcc-cpu6/bin/sdcc'),
                   ['-mcpu6','--std=c99','--opt-code-'+args.profile],
                   input_hashes([root/'os6/overlay.c',source,*list((root/'os6/include').glob('*.h'))]),profile=args.profile)
print('Built', destination / 'liboverlay-sdcc.a')
