"""Reachability GC for self-contained CPU6 objects and ordinary ar archives.

Marked C functions and individual data definitions are link units. Surviving
assembly IR is reassembled, preserving relocations, aliases and expressions.
Legacy objects and unrecognized assembly stay at whole-object granularity.
"""
import argparse
from dataclasses import dataclass,field
import json
from pathlib import Path
import re
import subprocess
import tempfile
from cpu6_object import unpack,symbols,archive_members

ROOT=Path(__file__).resolve().parents[1]
BEGIN=re.compile(r'^; CPU6 function (\w+) begin$')
END=re.compile(r'^; CPU6 function (\w+) end$')
LABEL=re.compile(r'^([A-Za-z_]\w*):\s*(.*)$')
IDENT=re.compile(r'\b[A-Za-z_]\w*\b')
STRINGS=re.compile(r'"(?:[^"\\]|\\.)*"|\'(?:[^\'\\]|\\.)*\'|;.*$')
SEGMENTS={'code','data','bss','literal'}
INTERNAL={name[:16] for segment in ['code','data','bss','literal','zp','discard','common','commondata','buffers']
          for name in ['__'+segment,'__'+segment+'_size']}|{'__end'}

def clean(line):
    return STRINGS.sub(lambda m:'' if m[0].startswith(';') else '""',line).strip()

def names(line):
    text=clean(line);label=LABEL.match(text)
    if label:text=label[2]
    parts=text.split(None,1)
    return {name[:16] for name in IDENT.findall(parts[1])} if len(parts)==2 else set()

@dataclass(eq=False)
class Atom:
    unit: object
    kind: str
    segment: str
    lines: list
    defined: set=field(default_factory=set)
    refs: set=field(default_factory=set)

@dataclass(eq=False)
class Unit:
    name: str
    raw: bytes
    metadata: dict|None
    library: bool
    exports: set
    imports: set
    absolute: bool
    atoms: list=field(default_factory=list)
    local: dict=field(default_factory=dict)
    opaque: str|None=None

def opaque(unit,reason):
    unit.opaque=reason
    atom=Atom(unit,'object','',[],set(unit.exports),set(unit.imports))
    unit.atoms=[atom];unit.local={name:atom for name in atom.defined}

def atomize(unit):
    if unit.metadata is None:return opaque(unit,'legacy object without retained IR')
    text=unit.metadata['assembly']
    if unit.absolute:return opaque(unit,'absolute-address object')
    # asm=2 is restricted to compiler-validated position-independent instructions.
    if re.search(r'^; CPU6 ABI .* asm=1\s*$',text,re.M):
        return opaque(unit,'inline assembly may share private layout')
    segment='code';current=None;function=None;has_data=False;settings=set()
    for line in text.splitlines():
        stripped=line.strip();begin=BEGIN.match(stripped);end=END.match(stripped)
        if begin:
            if function or (current and not has_data):return opaque(unit,'ambiguous function boundary')
            current=Atom(unit,'function','code',[line]);unit.atoms.append(current)
            function=begin[1];has_data=False;segment='code';continue
        if end:
            if function!=end[1]:raise ValueError('unbalanced function IR in '+unit.name)
            current.lines.append(line);current=None;function=None;continue
        if function:
            current.lines.append(line)
            match=LABEL.match(clean(line))
            if match:current.defined.add(match[1][:16])
            current.refs|=names(line)
            continue
        statement=clean(line)
        if not statement:continue
        parts=statement.split(None,1);op=parts[0].lower()
        if op=='.setcpu':settings.add(statement.lower());continue
        if op=='.export':continue
        if op.startswith('.') and op[1:] in SEGMENTS:
            if current and not has_data:return opaque(unit,'unbound data alias')
            current=None;has_data=False;segment=op[1:];continue
        match=LABEL.match(statement)
        if match:
            if current is None or has_data:
                current=Atom(unit,'data',segment,[]);unit.atoms.append(current);has_data=False
            current.defined.add(match[1][:16]);current.lines.append(line)
            statement=match[2]
            if not statement:continue
            op=statement.split(None,1)[0].lower()
        elif current:current.lines.append(line)
        if op not in ('.byte','.word','.ascii','.ds') or current is None:
            return opaque(unit,'unmarked code or unsupported assembly directive')
        has_data=True;current.refs|=names(line)
    if function:raise ValueError('unterminated function IR in '+unit.name)
    if current and not has_data:return opaque(unit,'trailing data alias')
    if len(settings)>1:return opaque(unit,'multiple instruction-set settings')
    for atom in unit.atoms:
        for name in atom.defined:
            if name in unit.local and unit.local[name] is not atom:
                return opaque(unit,'overlapping local symbol names')
            unit.local[name]=atom
    if not unit.exports<=unit.local.keys():return opaque(unit,'exports not described by IR')
    # Expressions involving two symbols in one operand can encode a layout
    # range. Preserve the original object instead of guessing its boundaries.
    for atom in unit.atoms:
        for line in atom.lines:
            operands=clean(line).split(None,1)
            if len(operands)<2:continue
            for operand in operands[1].split(','):
                referenced={unit.local[name[:16]] for name in IDENT.findall(operand) if name[:16] in unit.local}
                if len(referenced)>1:return opaque(unit,'cross-unit layout expression')

def read_units(paths):
    units=[]
    for path in paths:
        data=path.read_bytes();library=data.startswith((b'!<arch>\n',b'!<thin>\n'))
        members=archive_members(path) if library else [(None,data)]
        for member,blob in members:
            raw,metadata=unpack(blob);defined,undefined,sizes=symbols(raw)
            unit=Unit(str(path)+(f'({member})' if member else ''),raw,metadata,library,
                      defined,undefined,bool(sizes[0]))
            atomize(unit);units.append(unit)
    return units

def select(units,roots):
    definitions={}
    for unit in units:
        for name in unit.exports:definitions.setdefault(name,[]).append(unit.local[name])
    for name,choices in definitions.items():
        explicit=[atom for atom in choices if not atom.unit.library]
        if len(explicit)>1:raise ValueError('multiple definitions of '+name)
        choices.sort(key=lambda atom:atom.unit.library)
    live=set();pending=[]
    def require(name):
        name=name[:16]
        if name in INTERNAL:return
        if name not in definitions:raise ValueError('unresolved live symbol: '+name)
        pending.append(definitions[name][0])
    for name in roots:require(name)
    # Absolute input storage can represent runtime context or fixed vectors.
    for unit in units:
        if unit.absolute and not unit.library:pending.extend(unit.atoms)
    while pending:
        atom=pending.pop()
        if atom in live:continue
        live.add(atom)
        for name in atom.refs:
            if name in atom.unit.local:pending.append(atom.unit.local[name])
            elif name in atom.unit.imports or name in definitions or name in INTERNAL:require(name)
    return live

def render(unit,live):
    kept=[atom for atom in unit.atoms if atom in live]
    exported=sorted(name for name in unit.exports if unit.local[name] in live)
    # Preserve the compiler's instruction-set selection, with CPU6 default.
    settings=re.findall(r'^\s*\.setcpu\s+\S+\s*$',unit.metadata['assembly'],re.M)
    output=[settings[0].strip() if settings else '.setcpu 6']
    output+=['.export '+name for name in exported]
    for atom in kept:output+=['.'+atom.segment,*atom.lines]
    return '\n'.join(output)+'\n'

ARGUMENTS=set('uomfRABCDLOSXZ89')
SWITCHES=set('rbvtsi')
def separate(arguments):
    options=[];paths=[];roots=[];i=0;files=False
    while i<len(arguments):
        arg=arguments[i];i+=1
        if arg=='--':files=True;continue
        if not files and arg.startswith('-'):
            if len(arg) == 1:
                raise ValueError('unsupported linker option '+arg)
            for pos, key in enumerate(arg[1:], start=1):
                if key not in ARGUMENTS | SWITCHES:
                    raise ValueError('unsupported linker option -'+key)
                if key == 'r':
                    raise ValueError('GC currently requires a final link, not -r')
                options.append('-'+key)
                if key in ARGUMENTS:
                    value = arg[pos+1:]
                    if not value:
                        if i == len(arguments):raise ValueError('missing argument for '+arg)
                        value = arguments[i]
                        i += 1
                    options.append(value)
                    if key == 'u':roots.append(value)
                    break
        else:paths.append(Path(arg).resolve(strict=True))
    return options,paths,roots

def link(arguments,entry='start',keep=(),report=None):
    options,paths,undefined_roots=separate(arguments)
    units=read_units(paths);roots=[entry,*keep,*undefined_roots];live=select(units,roots)
    records=[]
    with tempfile.TemporaryDirectory(prefix='cpu6-gc-') as temporary:
        work=Path(temporary);objects=[]
        for index,unit in enumerate(units):
            kept=[atom for atom in unit.atoms if atom in live]
            records.append({'input':unit.name,'opaqueReason':unit.opaque,
                'kept':[sorted(atom.defined) for atom in kept],
                'removed':[sorted(atom.defined) for atom in unit.atoms if atom not in live]})
            if not kept:continue
            obj=work/('unit%04d.o'%index)
            if unit.opaque:obj.write_bytes(unit.raw)
            else:
                source=obj.with_suffix('.s');source.write_text(render(unit,live))
                subprocess.run([ROOT/'build/out/toolchain/bin/asee200','-o',obj,source],check=True)
            objects.append(obj)
        subprocess.run([ROOT/'build/out/toolchain/bin/ldee200',*options,*objects],check=True)
    if report:
        Path(report).write_text(json.dumps({'roots':roots,'units':records,'retainedUnits':sum(atom in live for u in units for atom in u.atoms),
            'discardedUnits':sum(atom not in live for u in units for atom in u.atoms)},indent=2)+'\n')

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gc-sections',action='store_true')
    parser.add_argument('--gc-keep',action='append',default=[])
    parser.add_argument('--entry',default='start')
    parser.add_argument('--gc-report',type=Path)
    options,arguments=parser.parse_known_args(argv)
    if options.gc_sections:link(arguments,options.entry,options.gc_keep,options.gc_report)
    else:
        forced=[item for name in options.gc_keep for item in ['-u',name]]
        subprocess.run([ROOT/'build/out/toolchain/bin/ldee200',*forced,*arguments],check=True)
