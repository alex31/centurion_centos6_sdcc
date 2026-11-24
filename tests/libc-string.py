#!/usr/bin/env python3
"""Check the shared string API and lazy extraction on CPU6."""
from pathlib import Path
import subprocess
import tempfile

from cpu6_roadmap_support import ROOT, execute

work = Path(tempfile.mkdtemp(prefix='libc-string-', dir=ROOT/'build/out/tests'))
print('Results:', work, flush=True)
execute(work, [ROOT/'tests/libc-string.c'],
        [('plain', ['-O1', '--no-gc-sections']), ('small', ['-Os', '--gc-sections'])])

# Header-only consumers keep the same binary. A strlen caller should not
# pull in the tokenizer's code or state, even without linker GC.
for gc in ('--gc-sections', '--no-gc-sections'):
    binaries = {}
    for name, source in {
        'plain': 'int main(void) { return 0; }',
        'header': '#include <string.h>\nint main(void) { return 0; }',
        'length': '#include <string.h>\nchar text[20];\nint main(void) { return strlen(text); }',
        'fields': '#include <string.h>\nchar text[] = "a,b";\nint main(void) { char *p = text; return *strsep(&p, ","); }',
    }.items():
        src = work/(name + '.c')
        src.write_text(source + '\n')
        output = work/(name + gc + '.bin')
        link_map = output.with_suffix('.map')
        subprocess.run([ROOT/'run/cpu6-bare-sdcc', gc, src, '--map', link_map, '-o', output], check=True)
        symbols = {line.split()[-1] for line in link_map.read_text().splitlines() if line.strip()}
        assert '_strtok' not in symbols and '_strtok_r' not in symbols, symbols
        assert ('_strlen' in symbols) == (name == 'length'), symbols
        assert ('_strsep' in symbols) == (name == 'fields'), symbols
        binaries[name] = output.read_bytes()
    assert binaries['plain'] == binaries['header'], gc
print('PASS string library selection')
