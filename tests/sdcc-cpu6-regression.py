#!/usr/bin/env python3
"""Run the existing numbered tests plus SDCC bool/flags checks on both cores."""
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
work = root / 'build/out/tests/sdcc-cpu6/regression'
work.mkdir(parents=True, exist_ok=True)
driver = work / 'driver.c'
driver.write_text('''#include <stdio.h>
int case_main(void);
int main(void) {
    if (case_main() != 7) { puts("FAIL numbered regression"); return 1; }
    puts("PASS numbered regression"); return 0;
}
''')
manifest = []
cases = [(path.stem, [path], True) for path in sorted((root / 'tests').glob('[0-9][0-9]_*.c'))]
cases += [('bool', [root / 'tests/sdcc-cpu6-bool.c'], False),
          ('address', [root / 'tests/sdcc-cpu6-address.c'], False),
          ('indexed', [root / 'tests/sdcc-cpu6-indexed.c'], False),
          ('switch-tables', [root / 'tests/sdcc-cpu6-switch.c'], False),
          ('carry', [root / 'tests/sdcc-cpu6-carry.c'], False),
          ('flags', [root / 'tests/ee200-flags.c', root / 'tests/ee200-flags.s'], False)]
for name, sources, numbered in cases:
    if numbered:
        renamed = work / (name + '.c')
        renamed.write_text('#define main case_main\n' + sources[0].read_text())
        sources = [renamed, driver]
    for opt in (0, 1):
        binary = work / f'{name}-O{opt}.bin'
        with binary.with_suffix('.log').open('w') as log:
            subprocess.run([root / 'run/cpu6-bare-sdcc', f'-O{opt}', *sources, '-o', binary],
                           stdout=log, stderr=subprocess.STDOUT, check=True)
            output = subprocess.check_output([root / 'tests/centurion-cli', '-T', '3000000',
                                              '-A', '2000', '-b', binary], text=True)
            log.write(output)
            assert 'PASS' in output and 'FAIL' not in output and 'System halted' in output, output
        manifest.append(binary.name)
        print(f'PASS instruction core: {name} -O{opt}', flush=True)
(work / 'manifest.json').write_text(json.dumps(manifest))
with (work / 'microcode.log').open('w') as log:
    subprocess.run(['node', root / 'tests/compiler-audit-core.js', work, work / 'manifest.json'],
                   stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
print(f'PASS: {len(manifest)} executions on each core')
