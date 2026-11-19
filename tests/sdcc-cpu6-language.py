#!/usr/bin/env python3
"""CPU6 C language cases for aliases, aggregate copies and bit-field diagnostics."""
from pathlib import Path
import subprocess
import tempfile

from cpu6_roadmap_support import ROOT, execute


def main():
    tests = ROOT / 'tests'
    sdcc = ROOT / 'swext/sdcc/sdcc'
    upstream = sdcc / 'support/regression'
    output = ROOT / 'build/out/tests'
    output.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='sdcc-cpu6-language-', dir=output))
    print('Results:', work, flush=True)

    variants = [('O0', ['-O0']), ('O1', ['-O1']), ('Os', ['-Os'])]
    for variant, flags in variants:
        for case, options in [('read', ['-DREAD_FIELD']), ('write', []),
                              ('write-static', ['-DWRITE_STATIC_FIELD'])]:
            name = f'bitfield-{case}-{variant}'
            result = subprocess.run([ROOT / 'run/cpu6-bare-sdcc', *flags, *options,
                                     '-c', tests / 'cpu6-bitfield.c', '-o', work / f'{name}.o'],
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            log = work / f'{name}.log'
            log.write_text(result.stdout)
            if result.returncode == 0 or 'cpu6: unsupported bitfield access' not in result.stdout:
                raise SystemExit(f'{name}: expected the CPU6 bit-field diagnostic. See {log}')
            print('PASS compiler diagnostic:', name, flush=True)

    # The upstream framework uses __code for separate code space on other ports.
    framework = upstream / 'fwk/include'
    execute(work, [tests / 'cpu6-language.c', tests / 'sdcc-stale-use.c',
                   upstream / 'tests/gcc-torture-execute-struct-cpy-1.c',
                   upstream / 'tests/union-volatile.c'], variants,
            common=['-I', str(framework), '-I', str(sdcc / 'device/include'), '-D__code='],
            extra_inputs=[tests / 'sdcc-stale-use.h', framework / 'testfwk.h'])


if __name__ == '__main__':
    main()
