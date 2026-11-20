#!/usr/bin/env python3
"""Build the optional hardware drivers for CPU6 bare-metal programs."""
from pathlib import Path
import subprocess
import tempfile

from cpu6_object import attach
from cpu6_provenance import compiler_info, input_hashes, write_manifest

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / 'build/out/toolchain'
BAREMETAL = ROOT / 'baremetal'


def main():
    compiler = TOOLS / 'sdcc-cpu6/bin/sdcc'
    assembler = TOOLS / 'bin/asee200'
    destination = TOOLS / 'lib/cpu6'
    destination.mkdir(parents=True, exist_ok=True)
    source = BAREMETAL / 'finch.c'
    dma = BAREMETAL / 'finch_dma.s'
    includes = BAREMETAL / 'include'
    flags = ['-mcpu6', '--std=c99', '--opt-code-size', '-I' + str(includes)]
    with tempfile.TemporaryDirectory(prefix='.baremetal-', dir=destination) as directory:
        work = Path(directory)
        assembly = work / 'finch.s'
        subprocess.run([compiler, '-S', *flags, '-o', assembly, source], check=True)
        objects = []
        for unit in (assembly, dma):
            obj = work / (unit.stem + '.o')
            subprocess.run([assembler, '-o', obj, unit], check=True)
            attach(obj, unit.read_text())
            objects.append(obj)
        archive = work / 'libbaremetal-sdcc.a'
        subprocess.run(['ar', 'qc', archive, *objects], check=True)
        output = destination / archive.name
        archive.replace(output)
    write_manifest(output, compiler_info(compiler), flags,
                   input_hashes([source, dma, *sorted(includes.glob('*.h'))]), profile='size')
    print('Built', output)


if __name__ == '__main__':
    main()
