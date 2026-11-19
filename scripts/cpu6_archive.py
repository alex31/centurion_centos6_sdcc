"""Split marked CPU6 assembly without losing within-unit C optimization.

Compiler-generated markers include entry/return labels and all local branches.
Private cross-member symbols receive archive-specific exported names; public
C names and the stack ABI remain unchanged. Shared data has its own member.
"""
import hashlib
from pathlib import Path
import re
import subprocess
from cpu6_object import attach

def assemble_members(source, directory, assembler, namespace):
    source, directory = Path(source), Path(directory)
    text = source.read_text()
    pattern = re.compile(r'^; CPU6 function (\w+) begin\n(.*?)^; CPU6 function \1 end\n', re.M|re.S)
    matches = list(pattern.finditer(text))
    if not matches:
        # A general toolchain rebuild may still find an older installed SDCC.
        # Keep its complete object until build-sdcc installs the marked output.
        if '; CPU6 function ' in text:
            raise ValueError('unbalanced CPU6 function markers: '+str(source))
        obj = directory/(namespace+'_whole.o')
        subprocess.run([assembler,'-o',obj,source],check=True)
        attach(obj, text)
        return [obj]
    shared = pattern.sub('', text)
    if re.search(r'^; CPU6 function .* (?:begin|end)$', shared, re.M):
        raise ValueError('unbalanced CPU6 function markers: '+str(source))
    exports = set(re.findall(r'^\s*\.export\s+(\w+)\s*$', text, re.M))
    def definitions(body): return set(re.findall(r'^(\w+):', body, re.M))
    shared_names = definitions(shared)
    entries = {match[1] for match in matches}
    private = (shared_names | entries) - exports
    renamed = {name: '__r_'+hashlib.sha256((namespace+':'+name).encode()).hexdigest()[:10] for name in private}
    assert len(set(renamed.values())) == len(renamed)
    # Do not rewrite quoted assembler string data.
    tokens = re.compile(r'"(?:[^"\\]|\\.)*"|\b[A-Za-z_][A-Za-z_0-9]*\b')
    def rename(body): return tokens.sub(lambda m:renamed.get(m[0],m[0]),body)
    shared = re.sub(r'^\s*\.export[^\n]*\n', '', shared, flags=re.M)
    chunks = []
    if shared_names:
        chunks.append(('data', shared, shared_names))
    for index,match in enumerate(matches):
        names = (definitions(match[2]) & exports) | {match[1]}
        chunks.append(('f%03d'%index, '\t.code\n'+match[0], names))
    objects = []
    for suffix,body,names in chunks:
        asm = directory/(namespace+'_'+suffix+'.s')
        header = '\t.setcpu 6\n'+''.join('\t.export '+name+'\n' for name in sorted(names))
        asm.write_text(rename(header+body))
        obj = asm.with_suffix('.o')
        subprocess.run([assembler,'-o',obj,asm],check=True)
        attach(obj, asm.read_text())
        objects.append(obj)
    return objects
