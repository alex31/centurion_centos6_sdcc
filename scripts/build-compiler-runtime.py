#!/usr/bin/env python3
"""Build the native CPU6 compiler helpers as a lazy archive."""
import argparse,os,sys
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
runtime = root / 'lib/compiler'
sources = sorted(runtime.glob('*.c')) + sorted(runtime.glob('*.s'))
generator = runtime / 'compound.py'
bit_runtime = runtime / 'bits'
bit_generator = bit_runtime / 'bitops.py'
bit_sources = sorted(bit_runtime.glob('*.s'))
strings = root / 'lib/string'
string_header = root / 'baremetal/include/string.h'
stdlib = sorted((root / 'lib/stdlib').glob('*.c'))
stdlib_header = root / 'baremetal/include/stdlib.h'
errno_header = stdlib_header.with_name('errno.h')
compiler = root / 'build/out/toolchain/sdcc-cpu6/bin/sdcc'
assembler = root / 'build/out/toolchain/bin/asee200'
flags = ['-mcpu6', '--std=c99', '--opt-code-' + args.profile]
with tempfile.TemporaryDirectory(prefix='cpu6-compiler-runtime-') as temporary:
    work = Path(temporary)
    objects = []
    for index, source in enumerate(sources):
        assembly = source
        if source.suffix == '.c':
            assembly = work / (source.stem + '.s')
            subprocess.run([compiler, '-S', *flags, '-o', assembly, source], check=True)
        objects += assemble_members(assembly, work, assembler, 'crt%d' % index)
    compound = work / 'compound.s'
    subprocess.run([sys.executable, generator, compound], check=True)
    objects += assemble_members(compound, work, assembler, 'compound')
    # Keep the existing archive member order and private-symbol namespaces.
    # Bit operations are pulled independently by the ordinary archive linker.
    for source in bit_sources:
        objects += assemble_members(source, work, assembler, source.stem)
    bitops = work / 'bitops.s'
    subprocess.run([sys.executable, bit_generator, bitops], check=True)
    objects += assemble_members(bitops, work, assembler, 'bitops')
    string_asm = work / 'string.s'
    subprocess.run([compiler, '-S', *flags, '--cpu6-no-auto-inline',
                    '-I' + str(string_header.parent), '-o', string_asm,
                    strings / 'string.c'], check=True)
    objects += assemble_members(string_asm, work, assembler, 'str')
    objects += assemble_members(strings / 'memory.s', work, assembler, 'strmem')
    for source in stdlib:
        stdlib_asm = work / (source.stem + '.s')
        subprocess.run([compiler, '-S', *flags, '--cpu6-no-auto-inline',
                        '-I' + str(stdlib_header.parent), '-o', stdlib_asm, source], check=True)
        objects += assemble_members(stdlib_asm, work, assembler, source.stem)
    archive = work / 'libcompiler-sdcc.a'
    subprocess.run(['ar', 'qc', archive, *objects], check=True)
    with tempfile.TemporaryDirectory(prefix='.stage-', dir=destination) as stage:
        staged = Path(stage) / archive.name
        shutil.copyfile(archive, staged)
        os.replace(staged, destination / archive.name)
    write_manifest(destination/archive.name, compiler_info(compiler),
                   flags, input_hashes([*sources, generator, *bit_sources, bit_generator,
                                        strings / 'string.c', strings / 'memory.s', string_header,
                                        *stdlib, stdlib_header, errno_header,
                                        stdlib_header.with_name('stdint.h')]), profile=args.profile)
print('Built', destination / archive.name)
