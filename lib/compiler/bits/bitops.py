# SPDX-License-Identifier: GPL-2.0-or-later
"""Emit only the width-dependent portions of the CPU6 bit runtime."""
from pathlib import Path
import sys


def member(name, body):
    lines = [f'.export {name}', f'; CPU6 function {name} begin', name + ':']
    lines += [line if line.endswith(':') else '    ' + line for line in body]
    return [*lines, f'; CPU6 function {name} end', '']


def load_word(bits, offset, reg='b'):
    if bits == 8:
        return [f'clr {reg}', f'ld{reg}b {offset + 1}(s)']
    return [f'ld{reg} {offset}(s)']


def result_slots(bits):
    """Return-state addresses in high-word-first order. The low word is B."""
    if bits == 64:
        return ['__cpu6_hi64', '__cpu6_hi64+2', '__hireg']
    if bits == 48:
        return ['__hireg', '__cpu6_mid48']
    if bits == 32:
        return ['__hireg']
    return []


def count_zeros(op, bits, words):
    # The high byte of the 8-bit stack argument is ABI padding. The common
    # nonzero-word leading scan counts it, so start the count eight lower.
    leading = op != 'ctz'
    bias = (-8 if leading and bits == 8 else 0)
    if op == 'clrsb':
        bias -= 1
    body = []
    if op == 'clrsb':
        # Normalize negative input by complementing the caller's by-value copy.
        sign_byte = 3 if bits == 8 else 2
        body += [f'ldab {sign_byte}(s)', 'bp scan']
        if bits == 8:
            body += ['ivab', 'stab 3(s)']
        else:
            body += [f'ivr {offset}(s)' for offset in words]
        body += ['scan:']
    body += [f'lda {bias & 0xffff}' if bias else 'cla']
    offsets = words if leading else list(reversed(words))
    for index, offset in enumerate(offsets):
        if index:
            body += ['inr a,16']
        body += load_word(bits, offset) + ['bnz found']
    if op == 'clrsb':
        body += [f'ldb {bits - 1}']
    else:
        body += [f'ldb {words[-1] + 2}(s)']  # explicit zero fallback
    return body + ['rsr', 'found:', 'jmp __bc_lz' if leading else 'jmp __bc_tz']


def population(bits, words):
    body = load_word(bits, words[0])
    if len(words) == 1:
        return body + ['jmp __bc_pop16']
    # Keep a running sum on the stack. Word loads after the push are
    # displaced by two bytes. The helper preserves all registers except A:B.
    body += ['jsr __bc_pop16', 'stb (-s)']
    for offset in words[1:]:
        body += [f'ldb {offset + 2}(s)', 'jsr __bc_pop16', 'add (s),b']
        if offset != words[-1]:
            body += ['stb (s)']
    return body + ['inr s,2', 'rsr']


def power_of_two(op, bits, words):
    slots = result_slots(bits)
    body = ['cla', *[f'sta ({slot})' for slot in slots]] if slots else []
    # The highest nonzero word selects the return lane. For ceil, double
    # floor only when some other bit of the input was set.
    for i, offset in enumerate(words):
        body += load_word(bits, offset) + [f'bnz lane{i}']
    body += ['ldb 1' if op == 'ceil' else 'clr b', 'rsr']
    for i, offset in enumerate(words):
        body += [f'lane{i}:', 'jsr __bc_floor16']
        if op == 'ceil':
            if bits == 8:
                body += ['ldab 3(s)', 'oreb bl,al']
            else:
                body += ['xfr b,a', f'ore {offset}(s),a']
                body += [f'ori {low}(s),a' for low in words[i + 1:]]
            body += [f'bz store{i}', 'slr b']
            if i:
                body += [f'bnl store{i}', f'inr ({slots[i - 1]})']
            if bits == 8:
                body += ['clrb bh']
            body += [f'store{i}:']
        if i < len(slots):
            body += [f'stb ({slots[i]})', 'clr b']
        body += ['rsr']
    return body


def permute(registers, distance, scratch, op):
    """Rotate named registers left by distance, emitting disjoint copy cycles."""
    n = len(registers)
    visited = set()
    body = []
    for start in range(n):
        if start in visited:
            continue
        body += [f'{op} {registers[start]},{scratch}']
        cursor = start
        while True:
            visited.add(cursor)
            source = (cursor + distance) % n
            if source == start:
                break
            body += [f'{op} {registers[source]},{registers[cursor]}']
            cursor = source
        body += [f'{op} {scratch},{registers[cursor]}']
    return body


def rotate(op, bits, words):
    left = op == 'rotl'
    registers = {
        8: ['b'], 16: ['b'], 32: ['y', 'b'],
        48: ['y', 'z', 'b'], 64: ['x', 'y', 'z', 'b'],
    }[bits]
    saved = {8: None, 16: None, 32: 'yh,yl', 48: 'yh,zl', 64: 'xh,zl'}[bits]
    frame = (len(registers) - 1) * 2
    count = words[-1] + 2 + frame
    body = [f'push {saved}'] if saved else []
    for reg, offset in zip(registers, words):
        if reg == 'b':
            body += load_word(bits, offset + frame)
        else:
            body += [f'ldw {reg},{offset + frame}(s)']
    # Decode count in chunks, so a variable rotation takes at most seven
    # carry-chain iterations, even for the 48- and 64-bit scalar ABIs.
    for shift in (32, 16, 8):
        if bits <= shift:
            continue
        body += [f'lda {count}(s)', f'and {shift},a', f'bz skip{shift}']
        if shift == 8:
            lanes = [reg + half for reg in registers for half in ('h','l')]
            body += permute(lanes, 1 if left else -1, 'al', 'xfrb')
        else:
            body += permute(registers, shift // 16 if left else -(shift // 16), 'a', 'xfr')
        body += [f'skip{shift}:']
    body += [f'lda {count}(s)', 'and 7,a', 'bz done', f'sta {count}(s)', 'bit:']
    if bits == 8:
        body += ['xfrb bl,al', 'slab' if left else 'srab', 'rlrb bl' if left else 'rrrb bl']
    else:
        seed = registers[0] if left else registers[-1]
        body += [f'xfr {seed},a', 'sla' if left else 'sra']
        for reg in (reversed(registers) if left else registers):
            body += [('rlr ' if left else 'rrr ') + reg]
    body += [f'dcr {count}(s)', 'bnz bit', 'done:']
    body += [f'stw {reg},({slot})' for reg, slot in zip(registers, result_slots(bits))]
    if saved:
        body += [f'pop {saved}']
    return body + ['rsr']


def rotate_field(left):
    # The field value is already zero-extended, width 1..15. Build its top
    # bit once, then use that boundary to wrap a bit in either direction.
    body = ['push yh,zl', 'ldw y,8(s)', 'ldb 6(s)', 'ori y,y', 'bz field_done',
            'lda 10(s)', 'dca', 'ldw z,1', 'ori a,a', 'bz field_bit',
            'field_mask:', 'slr z', 'dca', 'bnz field_mask', 'field_bit:']
    if left:
        body += ['xfr b,a', 'and z,a', 'bz field_no_wrap', 'slr b', 'inr b',
                 'bra field_next', 'field_no_wrap:', 'slr b']
    else:
        body += ['srr b', 'bnl field_next', 'ori z,b']
    body += ['field_next:', 'dcr y', 'bnz field_bit']
    if left:
        body += ['slr z', 'dcr z', 'and z,b']
    return body + ['field_done:', 'pop yh,zl', 'rsr']


def modulo_count():
    # The modulus is 1..64; the previous remainder and next byte together
    # fit in the hardware 16-bit dividend. Skip leading zero bytes.
    return ['push yh,zl', 'add s,6,y', 'ldw z,8', 'cla', 'mod_skip:',
            'ldab (y+)', 'bnz mod_divide', 'dcr z', 'bnz mod_skip', 'bra mod_done',
            'mod_divide:', 'ldb 14(s)', 'div b,a', 'dcr z', 'bz mod_done',
            'xfrb al,ah', 'ldab (y+)', 'bra mod_divide', 'mod_done:',
            'xab', 'pop yh,zl', 'rsr']


def generate(path):
    output = ['.setcpu 6', '.code']
    for bits in (8, 16, 32, 48, 64):
        words = list(range(2, max(bits, 16) // 8 + 2, 2))
        output += member(f'___c6_pop{bits}', population(bits, words))
        for op in ('clz', 'ctz', 'clrsb'):
            output += member(f'___c6_{op}{bits}', count_zeros(op, bits, words))
        for op in ('ceil', 'floor'):
            output += member(f'___c6_{op}{bits}', power_of_two(op, bits, words))
        for op in ('rotl', 'rotr'):
            output += member(f'___cpu6_{op}{bits}', rotate(op, bits, words))
    output += member('___c6_rotlfield', rotate_field(True))
    output += member('___c6_rotrfield', rotate_field(False))
    output += member('___c6_modcnt64', modulo_count())
    path.write_text('\n'.join(output))


if __name__ == '__main__':
    generate(Path(sys.argv[1]))
