"""Build an assembly caller that checks runtime helper register/stack contracts."""
from pathlib import Path
import subprocess


def build(work, opt, compiler):
    directory = work / f'abi-O{opt}'
    directory.mkdir(exist_ok=True)
    helpers = []
    for suffix in ('', 'c', 'l'):
        for operation in ('mul', 'div', 'rem', 'shl', 'shr'):
            helpers.append((f'__{operation}eq{suffix}', 'pointer'))
        for operation in ('div', 'rem', 'shr'):
            helpers.append((f'__{operation}equ{suffix}', 'pointer'))
    for name in ('andeql', 'oreql', 'xoreql', 'pluseql', 'minuseql', 'postincl', 'postdecl'):
        helpers.append(('__' + name, 'pointer'))
    for name in ('bandl', 'orl', 'xorl', 'plusl', 'minusl', 'mull', 'divl', 'reml', 'divul', 'remul'):
        helpers.append(('__' + name, 'long'))
    for name in ('mul', 'div', 'rem', 'divu', 'remu'):
        helpers.append(('__' + name, 'word'))

    assembly = ['\t.setcpu 6', '\t.code', '\t.export _abi', '_abi:',
                '\tstx (-s)', '\txfr y,a', '\tsta (-s)', '\txfr z,a', '\tsta (-s)',
                '\txfr s,a', '\tsta (saved_s)']
    for index, (helper, operand) in enumerate(helpers, 1):
        assembly += [f'; {index}: {helper}', f'\tlda {index}', '\tsta (test_number)',
                     '\tlda 0x1234', '\tsta (target)', '\tlda 0x5678', '\tsta (target+2)']
        if operand == 'pointer':
            assembly += ['\tlda target', '\tsta (-s)']
        else:
            assembly += ['\tlda 0x5678', '\tsta (-s)']
            if operand == 'long':
                assembly += ['\tlda 0x1234', '\tsta (-s)']
        assembly += ['\tlda 0', '\tsta (__hireg)', '\tldb 3',
                     '\tldx 0x4567', '\tlda 0x89ab', '\txfr a,y',
                     '\tlda 0xcdef', '\txfr a,z', f'\tjsr {helper}']
        for reg, expected in [('x', '0x4567'), ('y', '0x89ab'), ('z', '0xcdef'), ('s', '(saved_s)')]:
            assembly += [f'\txfr {reg},b', f'\tlda {expected}', '\tsab',
                         f'\tbz ok_{index}_{reg}', '\tjmp failed', f'ok_{index}_{reg}:']
    assembly += ['\tldb 0', '\tbra done', 'failed:', '\tldb (test_number)',
                 'done:', '\tlda (saved_s)', '\txfr a,s', '\tlda (s+)', '\txfr a,z',
                 '\tlda (s+)', '\txfr a,y', '\tldx (s+)', '\trsr', '\t.data',
                 'saved_s:', '\t.word 0', 'test_number:', '\t.word 0',
                 'target:', '\t.word 0,0']
    (directory / 'abi.s').write_text('\n'.join(assembly) + '\n')
    (directory / 'driver.c').write_text('''#include <stdio.h>
int abi(void);
int main(void) {
    int n = abi();
    if (n) {
        puts("FAIL ABI helper");
        putchar("0123456789abcdef"[(n >> 4) & 15]);
        putchar("0123456789abcdef"[n & 15]);
        putchar(10);
        return n;
    }
    puts("PASS");
    return 0;
}
''')
    output = work / f'runtime_abi-O{opt}.bin'
    subprocess.run([compiler, f'-O{opt}', directory / 'driver.c',
                    directory / 'abi.s', '-o', output],
                   check=True, capture_output=True)
    return output
