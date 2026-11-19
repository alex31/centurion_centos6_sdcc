# SPDX-License-Identifier: GPL-2.0-or-later
"""Emit the legacy pointer-operand ABI adapters for native CPU6 arithmetic."""
from pathlib import Path
import sys


def assignment(name, operation, width, signed=False, post=False):
    # After saving Y, the pointer follows the caller X at 4(S).
    code = ['xfr y,a', 'sta (-s)', 'lda 4(s)', 'xay']
    if width == 1:
        code += ['cla', 'ldab (y)']
        if signed:
            code += ['orib al,al', 'bp positive_'+name, 'dcrb ah', 'positive_'+name+':']
        code += ['sta (-s)']
    elif width == 2:
        code += ['lda (y)', 'sta (-s)']
    elif post:
        # Keep a copy of the old value below the operand for the call.
        code += ['lda (y)', 'sta (-s)', 'lda 2(y)', 'sta (-s)',
                 'sta (-s)', 'lda 4(s)', 'sta (-s)']
    else:
        code += ['lda 2(y)', 'sta (-s)', 'lda (y)', 'sta (-s)']
    code += ['jsr '+operation]
    if width == 1:
        code += ['stbb (y)']
    elif width == 2:
        code += ['stb (y)']
    else:
        code += ['stb 2(y)', 'lda (__hireg)', 'sta (y)']
    if post:
        code += ['ldb (s+)', 'lda (s+)', 'sta (__hireg)']
    code += ['lda (s+)', 'xay', 'lda (s)', 'sta 2(s)', 'inr s,2', 'rsr']
    return [
        '.export '+name,
        '; CPU6 function '+name+' begin',
        name+':',
        *['    '+line if not line.endswith(':') else line for line in code],
        '; CPU6 function '+name+' end',
        '',
    ]


def generate(path):
    text = ['.setcpu 6', '.code', '; SPDX-License-Identifier: GPL-2.0-or-later']
    for suffix, width in (('', 2), ('c', 1), ('l', 4)):
        for op in ('mul', 'div', 'rem', 'shl', 'shr'):
            callee = '__'+op+('l' if width == 4 else '')
            text += assignment('__'+op+'eq'+suffix, callee, width, op in ('div', 'rem', 'shr'))
        for op in ('div', 'rem', 'shr'):
            callee = '__'+op+'u'+('l' if width == 4 else '')
            text += assignment('__'+op+'equ'+suffix, callee, width)
    for name, callee in [('and', 'band'), ('or', 'or'), ('xor', 'xor'),
                         ('plus', 'plus'), ('minus', 'minus')]:
        text += assignment('__'+name+'eql', '__'+callee+'l', 4)
    text += assignment('__postincl', '__plusl', 4, post=True)
    text += assignment('__postdecl', '__minusl', 4, post=True)
    path.write_text('\n'.join(text))


if __name__ == '__main__':
    generate(Path(sys.argv[1]))
