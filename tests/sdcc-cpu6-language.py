#!/usr/bin/env python3
"""CPU6 C language cases for aliases, aggregate copies and packed bit-fields."""
from pathlib import Path
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
    # The upstream framework uses __code for separate code space on other ports.
    # Distinguish upstream function names in the assembler's 16-byte symbols.
    upstream_flags = ['-D__code=',
                      '-DtestBitfieldsSingleBitLiteral=test_bit_const',
                      '-DtestBitfieldsSingleBit=test_bit_var',
                      '-DtestBitfieldsMultibitLiteral=test_bits_const',
                      '-DtestBitfieldsMultibit=test_bits_var']
    framework = upstream / 'fwk/include'
    execute(work, [tests / 'cpu6-language.c', tests / 'sdcc-stale-use.c',
                   tests / 'cpu6-bitfield.c', upstream / 'tests/bitfields.c',
                   upstream / 'tests/gcc-torture-execute-struct-cpy-1.c',
                   upstream / 'tests/union-volatile.c'], variants,
            common=['--gc-sections', '-I', str(framework), '-I', str(sdcc / 'device/include'),
                    *upstream_flags],
            extra_inputs=[tests / 'sdcc-stale-use.h', framework / 'testfwk.h'])


if __name__ == '__main__':
    main()
