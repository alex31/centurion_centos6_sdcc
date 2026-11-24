"""CPU6 link-time inlining over separately compiled assembly units.

Only compiler-described functions with verified entry/exit shapes are imported. The
assembler remains the final authority on instruction encoding/relocation.
This is deliberately independent of C parsing and translation-unit scopes.
"""
from collections import Counter
from dataclasses import dataclass
import json
import re

TOKEN = re.compile(r'[A-Za-z_?][A-Za-z_?0-9]*')
BEGIN = re.compile(r'^; CPU6 function (\S+) begin$')
END = re.compile(r'^; CPU6 function (\S+) end$')
ABI = re.compile(r'^; CPU6 ABI (\w+) frame=(\d+) saved=(\d+) variadic=(\d+) asm=(\d+)$')
# EE200's relaxable aliases are paired by opcode (0x10 through 0x19).
BRANCHES = {'bl': 'jc', 'bnl': 'jcc', 'bf': 'jn', 'bnf': 'jnc', 'bz': 'jz',
            'bnz': 'jnz', 'bm': 'jlt', 'bp': 'jge', 'bgz': 'jle', 'ble': 'jgt'}
CONDITIONAL = set(BRANCHES) | set(BRANCHES.values())
SAFE = set('lda ldb ldx ldw sta stb stx stw ldab ldbb stab stbb xfr xfrb xab xax xay xaz '
           'cla clr clrb iva ivr ivrb ina inx dca dcx inr inrb dcr dcrb '
           'aab sab nab add sub and ori ore addb subb andb orib oreb '
           'slr srr rlr rrr slrb srrb rlrb rrrb sla sra sl rl sf rf nop '
           'mul div push pop jmp bra jsr .byte'.split()) | CONDITIONAL


def instruction(line):
    text = line.split(';', 1)[0].strip()
    if not text or text.endswith(':'):
        return '', ''
    parts = text.split(None, 1)
    return parts[0].lower(), parts[1].strip() if len(parts) > 1 else ''


@dataclass
class Function:
    unit: int
    name: str
    start: int
    stop: int
    lines: list
    abi: tuple | None


def functions(unit, lines):
    result = []
    start = name = None
    for index, line in enumerate(lines):
        begin, end = BEGIN.match(line), END.match(line)
        if begin:
            if start is not None:
                raise ValueError('nested CPU6 function markers')
            start, name = index, begin[1]
        if end and start is not None:
            if end[1] != name:
                raise ValueError('mismatched CPU6 function markers')
            body = lines[start:index + 1]
            metadata = next((ABI.match(item) for item in body if ABI.match(item)), None)
            result.append(Function(unit, name, start, index + 1, body,
                                   metadata.groups() if metadata else None))
            start = name = None
    if start is not None:
        raise ValueError('unterminated CPU6 function')
    return result


def candidate(function, private):
    if not function.abi or function.abi[0] not in ('stack', 'register') or function.abi[-1] not in ('0', '2'):
        return None, 'requires compiler ABI metadata and known instruction effects'
    try:
        entry = function.lines.index(function.name + ':')
    except ValueError:
        return None, 'missing entry label'
    lines = [line for line in function.lines[entry + 1:] if line and not line.startswith(';')]
    if len(lines) < 3:
        return None, 'unrecognized prologue'
    first_op = instruction(lines[0])
    last_ops = [instruction(line) for line in lines[-2:]]
    valid_frame = (
        (first_op == ('stx', '(-s)') and last_ops == [('ldx', '(s+)'), ('rsr', '')]) or
        (first_op == ('push', 'xh,yl') and last_ops == [('pop', 'xh,yl'), ('rsr', '')]) or
        (first_op == ('push', 'xh,zl') and last_ops == [('pop', 'xh,zl'), ('rsr', '')])
    )
    if not valid_frame:
        return None, 'unrecognized prologue'
    body = lines[1:-2]
    if sum(bool(instruction(line)[0]) for line in body) > 48:
        return None, 'function exceeds instruction budget'
    labels = {line[:-1] for line in body if line.endswith(':')}
    compact = function.abi[:4] == ('stack', '0', '2', '0') and first_op == ('stx', '(-s)')
    for line in body:
        op, args = instruction(line)
        if not op:
            continue
        if op not in SAFE:
            return None, 'directive or unsupported instruction'
        if (op in CONDITIONAL or op in ('jmp', 'bra')) and args not in labels:
            return None, 'external or indirect branch'
        if set(TOKEN.findall(args)) & (private - labels):
            return None, 'reference to translation-unit private data'
        # Nontrivial stack uses select the frame-preserving expansion. Keeping
        # both original saved words preserves addresses even across argument
        # pushes, register calls, wide temporaries and pointers to locals.
        stripped = re.sub(r'(?<![\w])(?:[4-9]|[1-9][0-9]+)\(s\)', '', args)
        if 's' in TOKEN.findall(stripped) or re.search(r'\bs[lh]\b', args):
            compact = False
        # Register-range pushes can put saved registers at apparent argument
        # offsets. Only rebase offsets when S stays fixed throughout the body.
        if op in ('jsr', '.byte', 'push', 'pop'):
            compact = False
        if op in ('mul', 'div'):
            operands = [part.strip() for part in args.split(',')]
            if len(operands) != 2 or operands[-1] not in ('a', 'b'):
                compact = False
    return {'body': body if compact else lines[:-1],
            'mode': 'compact' if compact else 'frame-preserving'}, None


def size_bound(line):
    """Conservative byte bound; unknown assembly forces long branches."""
    op, args = instruction(line)
    if not op:
        return 0
    if op in CONDITIONAL:
        return 5  # inverted condition over an absolute jump
    if op in ('bra', 'jmp', 'jsr'):
        return 3
    if op in ('ldab', 'ldbb', 'stab', 'stbb', 'lda', 'ldb', 'ldx', 'sta', 'stb', 'stx'):
        return 3
    if op in ('push', 'pop', 'xfr', 'xfrb'):
        return 2
    if op in ('aab', 'sab', 'nab', 'xab', 'xax', 'xay', 'xaz', 'cla', 'iva',
              'ina', 'inx', 'dca', 'dcx', 'sla', 'sra', 'sl', 'rl', 'sf', 'rf', 'nop', 'rsr'):
        return 1
    if op == '.byte':
        return len(args.split(','))
    if op == '.word':
        return 2 * len(args.split(','))
    return 4 if op in SAFE else 65536


def relax(lines):
    # BRA already relaxes in the assembler. Keep raw short conditionals when
    # they fit even if every intervening branch takes its longest encoding.
    positions, labels, pos = [], {}, 0
    for line in lines:
        positions.append(pos)
        if line.endswith(':'):
            labels[line[:-1]] = pos
        pos += size_bound(line)
    for index, line in enumerate(lines):
        op, target = instruction(line)
        if op in BRANCHES:
            distance = labels.get(target, 1 << 30) - positions[index] - 2
            if not -128 <= distance <= 127:
                lines[index] = '\t' + BRANCHES[op] + ' ' + target
    return lines


def optimize(paths, report_path, no_inline=()):
    units = [path.read_text().splitlines() for path in paths]
    groups = [functions(index, lines) for index, lines in enumerate(units)]
    all_functions = [fn for group in groups for fn in group]
    counts = Counter(fn.name for fn in all_functions)
    # Directives can carry indentation; labels have compiler-normalized spelling.
    exported = [{line.strip().split()[1] for line in lines if line.strip().startswith('.export ')} for lines in units]
    private = [{line[:-1] for line in lines if line.endswith(':')} - public
               for lines, public in zip(units, exported)]
    available, rejected = {}, {}
    # An explicitly mapped function must retain its own body and page. This
    # also covers declarations in a different translation unit from its body.
    mapped={m[1][:16] for lines in units for line in lines
            if (m:=re.match(r'^; CPU6 MMU D (\S+) ',line))}
    for fn in all_functions:
        if fn.name not in exported[fn.unit] or counts[fn.name] != 1:
            continue
        if fn.name in no_inline:
            rejected[fn.name] = 'kept out of line for overlay size'
            continue
        if fn.name[:16] in mapped:
            rejected[fn.name]='MMU placement must be preserved'
            continue
        description, reason = candidate(fn, private[fn.unit])
        if description is None:
            rejected[fn.name] = reason
        else:
            available[fn.name] = (fn, description)
    used_names = set(TOKEN.findall('\n'.join('\n'.join(lines) for lines in units)))
    sequence = 0
    def fresh():
        nonlocal sequence
        while True:
            sequence += 1
            name = 'LTI%08x' % sequence
            if name not in used_names:
                used_names.add(name)
                return name
    inlined = []
    for unit, group in enumerate(groups):
        replacements = {}
        for caller in group:
            if not caller.abi or caller.abi[-1] not in ('0', '2'):
                continue
            size_mode = '; CPU6 optimize for size' in caller.lines
            output, budget, sites = [], 96, 0
            for line in caller.lines:
                op, target = instruction(line)
                if op != 'jsr' or target not in available or target == caller.name:
                    output.append(line)
                    continue
                callee, description = available[target]
                body, mode = description['body'], description['mode']
                cost = sum(bool(instruction(item)[0]) for item in body)
                if callee.unit == caller.unit or cost > budget or sites >= 8:
                    output.append(line)
                    continue
                labels = {item[:-1]: fresh() for item in body if item.endswith(':')}
                # JSR/RSR preserve caller X implicitly. Imported bodies that
                # mention X need an explicit save; the others need neither.
                preserve_x = mode == 'frame-preserving' or any(
                    re.search(r'\b(?:x|xl|xh|ldx|stx|inx|dcx|xax)\b', item) for item in body)
                removed = 2 if preserve_x else 4
                expansion = []
                if preserve_x:
                    expansion.append('\tstx (-s)')
                for item in body:
                    item = TOKEN.sub(lambda match: labels.get(match[0], match[0]), item)
                    if mode == 'compact':
                        item = re.sub(r'(?<![\w])([0-9]+)\(s\)',
                                      lambda match: str(int(match[1]) - removed) + '(s)', item)
                    expansion.append(item)
                if preserve_x:
                    expansion.append('\tldx (s+)')
                # An external JSR occupies three bytes. Its target may still
                # be required by an address reference or another overlay.
                if size_mode and sum(size_bound(item) for item in expansion) > 3:
                    output.append(line)
                    continue
                output += ['; CPU6 LTO inline ' + target, *expansion, '; CPU6 LTO inline end']
                budget -= cost
                sites += 1
                inlined.append({'caller': caller.name, 'callee': target, 'callerUnit': unit,
                                'calleeUnit': callee.unit, 'instructions': cost, 'preserveX': preserve_x,
                                'mode': mode})
            if sites:
                replacements[caller.start] = (caller.stop, relax(output))
        if replacements:
            output, index = [], 0
            while index < len(units[unit]):
                if index in replacements:
                    stop, lines = replacements[index]
                    output.extend(lines)
                    index = stop
                else:
                    output.append(units[unit][index])
                    index += 1
            paths[unit].write_text('\n'.join(output) + '\n')
    report_path.write_text(json.dumps({'inlined': inlined, 'rejected': rejected,
                                     'eligible': sorted(available)}, indent=2) + '\n')
    return inlined
