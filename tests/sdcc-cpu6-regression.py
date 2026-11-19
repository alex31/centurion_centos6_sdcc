#!/usr/bin/env python3
"""Run the existing numbered tests plus SDCC bool/flags checks on both cores."""
import json
from pathlib import Path
import subprocess
from regression_cases import regression_cases

root = Path(__file__).resolve().parents[1]
work = root / 'build/out/tests/sdcc-cpu6/regression'
work.mkdir(parents=True, exist_ok=True)
manifest = []
for name, sources in regression_cases(work):
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
