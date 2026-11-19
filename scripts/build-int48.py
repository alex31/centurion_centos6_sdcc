#!/usr/bin/env python3
"""Build the common six-byte integer library for FCC and/or SDCC."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from cpu6_archive import assemble_members
from cpu6_provenance import compiler_info,input_hashes,sha256,write_manifest

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', choices=('fcc', 'sdcc', 'both'), default='sdcc')
parser.add_argument('--profile',choices=['speed','size'],default='speed')
parser.add_argument('--destination',type=Path,default=root/'build/out/toolchain/lib/cpu6')
args = parser.parse_args()
destination = args.destination.resolve()
destination.mkdir(parents=True, exist_ok=True)
for compiler in (('fcc', 'sdcc') if args.compiler == 'both' else (args.compiler,)):
    with tempfile.TemporaryDirectory(prefix='cpu6-int48-') as td:
        work = Path(td)
        shutil.copy2(root / 'lib/int48/int48.c', work / 'int48.c')
        shutil.copy2(root / 'lib/int48/cpu6.s', work / 'native.s')
        shutil.copy2(root / 'lib/int48/scalar48.s', work / 'scalar48.s')
        includes = ['-I' + str(root / 'baremetal/include'), '-I' + str(root / 'lib/int48')]
        if compiler == 'fcc':
            subprocess.run([root / 'build/out/toolchain/bin/fcc', '-mwrx6', '-O1', '-DINT48_USE_ASM',
                            *includes, '-c', work / 'int48.c'], cwd=work, check=True)
        else:
            subprocess.run([root / 'build/out/toolchain/sdcc-cpu6/bin/sdcc', '-mcpu6', '-S', '--std=c99',
                            '--opt-code-'+args.profile, '-DINT48_USE_ASM', *includes,
                            '-o', work / 'int48.s', work / 'int48.c'], cwd=work, check=True)
        if compiler == 'sdcc':
            objects = assemble_members(work / 'int48.s', work, root / 'build/out/toolchain/bin/asee200', 'i48')
            objects += assemble_members(work / 'native.s', work, root / 'build/out/toolchain/bin/asee200', 'n48')
        else:
            subprocess.run([root / 'build/out/toolchain/bin/asee200', '-o', work / 'native.o', work / 'native.s'], check=True)
            objects = [work / 'int48.o', work / 'native.o']
        subprocess.run([root / 'build/out/toolchain/bin/asee200', '-o', work / 'scalar48.o', work / 'scalar48.s'], check=True)
        archive = work / ('libint48-' + compiler + '.a')
        subprocess.run(['ar', 'qc', archive, *objects, work / 'scalar48.o'], check=True)
        with tempfile.TemporaryDirectory(prefix='.stage-', dir=destination) as stage:
            target = Path(stage) / archive.name
            shutil.copy2(archive, target)
            os.replace(target, destination / archive.name)
        cc=root/'build/out/toolchain'/('sdcc-cpu6/bin/sdcc' if compiler=='sdcc' else 'bin/fcc')
        info=compiler_info(cc) if compiler=='sdcc' else {'path':str(cc),'sha256':sha256(cc),'family':'FCC'}
        write_manifest(destination/archive.name,info,
                       ['--opt-code-'+args.profile,'-DINT48_USE_ASM'] if compiler=='sdcc' else ['-mwrx6','-O1','-DINT48_USE_ASM'],
                       input_hashes([*list((root/'lib/int48').glob('*.[csh]')),*list((root/'baremetal/include').glob('*.h'))]),profile=args.profile)
    print('Built', destination / archive.name)
