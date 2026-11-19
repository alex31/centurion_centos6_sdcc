"""Check runtime helper results and register/stack contracts from assembly."""
from pathlib import Path
import subprocess


def build(work, opt, compiler):
    directory = work / f'abi-O{opt}'
    directory.mkdir(exist_ok=True)
    helpers = []
    for suffix, width in (('', 2), ('c', 1), ('l', 4)):
        for operation in ('mul', 'div', 'rem', 'shl', 'shr'):
            helpers.append((f'__{operation}eq{suffix}', 'pointer', width, operation))
        for operation in ('div', 'rem', 'shr'):
            helpers.append((f'__{operation}equ{suffix}', 'pointer', width, operation))
    for name, operation in [('andeql', 'and'), ('oreql', 'or'), ('xoreql', 'xor'),
                            ('pluseql', 'plus'), ('minuseql', 'minus'),
                            ('postincl', 'plus'), ('postdecl', 'minus')]:
        helpers.append(('__' + name, 'pointer', 4, operation))
    for operation in ('band', 'or', 'xor', 'plus', 'minus', 'mul', 'div', 'rem', 'divu', 'remu'):
        helpers.append(('__' + operation + 'l', 'value', 4, operation.rstrip('u')))
    for operation in ('mul', 'div', 'rem', 'divu', 'remu'):
        helpers.append(('__' + operation, 'value', 2, operation.rstrip('u')))

    def result(operation, left, width):
        results = {'mul': left * 3, 'div': left // 3, 'rem': left % 3, 'shl': left << 3,
                   'shr': left >> 3, 'and': left & 3, 'band': left & 3, 'or': left | 3,
                   'xor': left ^ 3, 'plus': left + 3, 'minus': left - 3}
        return results[operation] & ((1 << (8 * width)) - 1)

    def check_word(index, part, expected, load=None):
        if load:
            assembly.append(load)
        label = f'ok_{index}_{part}'
        assembly.extend([f'\tlda {expected}', '\tsab', f'\tbz {label}', '\tjmp failed', label+':'])

    assembly = ['\t.setcpu 6', '\t.code', '\t.export _abi', '_abi:',
                '\tstx (-s)', '\txfr y,a', '\tsta (-s)', '\txfr z,a', '\tsta (-s)',
                '\txfr s,a', '\tsta (saved_s)']
    for index, (helper, operand, width, operation) in enumerate(helpers, 1):
        assembly += [f'; {index}: {helper}', f'\tlda {index}', '\tsta (test_number)',
                     '\tlda 0x1234', '\tsta (target)', '\tlda 0x5678', '\tsta (target+2)']
        if operand == 'pointer':
            assembly += ['\tlda target', '\tsta (-s)']
        else:
            assembly += ['\tlda 0x5678', '\tsta (-s)']
            if width == 4:
                assembly += ['\tlda 0x1234', '\tsta (-s)']
        assembly += ['\tlda 0', '\tsta (__hireg)', '\tldb 3',
                     '\tldx 0x4567', '\tlda 0x89ab', '\txfr a,y',
                     '\tlda 0xcdef', '\txfr a,z', f'\tjsr {helper}']
        left = {1: 0x12, 2: 0x1234 if operand == 'pointer' else 0x5678, 4: 0x12345678}[width]
        updated = result(operation, left, width)
        returned = left if helper.startswith('__post') else updated
        check_word(index, 'result', returned & 65535, '\tclrb bh' if width == 1 else None)
        if width == 4:
            check_word(index, 'result_high', returned >> 16, '\tldb (__hireg)')
        if operand == 'pointer':
            if width == 1:
                assembly += ['\tldbb (target)', '\tclrb bh']
                check_word(index, 'target', updated)
            else:
                check_word(index, 'target', updated & 65535, '\tldb (target+2)' if width == 4 else '\tldb (target)')
            if width == 4:
                check_word(index, 'target_high', updated >> 16, '\tldb (target)')
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
