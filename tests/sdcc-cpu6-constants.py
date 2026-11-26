#!/usr/bin/env python3
"""Check constant word arithmetic against Python and an exhaustive /47 oracle."""
import json
import os
from pathlib import Path
import random
import subprocess

root = Path(__file__).resolve().parents[1]
work = root / 'build/out/tests/sdcc-cpu6/constant-tests'
work.mkdir(parents=True, exist_ok=True)
rng = random.Random(0xc00647)
divisors = [1, 2, 3, 5, 7, 10, 31, 47, 127, 128, 129, 255, 256, 257,
            32767, 32768, 32769, 65534, 65535]
manifest = []
# Narrow result types do not change the promoted left operand's shift width.
shift_source = work / 'narrow-result.c'
shift_source.write_text('unsigned char f(void) { return (unsigned char)(65535UL << 21); }\n'
                        'unsigned char g(unsigned long x) { return (unsigned char)(x << 21); }\n')
subprocess.run([root/'run/sdcc', '--Werror', '-S', shift_source, '-o', work/'narrow-result.s'], check=True)
for group in range(4):
    source = '#include <stdio.h>\n'
    names = []
    for divisor in divisors[group::4]:
        name = f'check_{divisor}'
        names.append(name)
        values = [0, 1, divisor - 1, divisor, min(65535, divisor + 1), 65535,
                  32767, 32768, 32769] + [rng.randrange(65536) for _ in range(24)]
        source += f'unsigned int divide_{divisor}(unsigned int x) {{ return x / {divisor}U; }}\n'
        source += f'unsigned int remain_{divisor}(unsigned int x) {{ return x % {divisor}U; }}\n'
        source += f'unsigned int times_{divisor}(unsigned int x) {{ return x * {divisor}U; }}\n'
        source += f'int {name}(void) {{\nstatic unsigned int rows[][4] = {{\n'
        source += ',\n'.join('{' + ','.join(str(n) + 'U' for n in
                                 [x, x // divisor, x % divisor, x * divisor & 65535]) + '}'
                              for x in values)
        source += '}; unsigned int i; for (i=0; i<sizeof(rows)/sizeof(rows[0]); i++) {\n'
        source += f'''if (divide_{divisor}(rows[i][0]) != rows[i][1] ||
remain_{divisor}(rows[i][0]) != rows[i][2] || times_{divisor}(rows[i][0]) != rows[i][3]) return 1;
}} return 0; }}
'''
    source += 'int main(void) { if (' + ' || '.join(n + '()' for n in names)
    source += ') { puts("FAIL constants"); return 1; } puts("PASS constants"); return 0; }\n'
    path = work / f'vectors{group}.c'
    path.write_text(source)
    for opt in (0, 1):
        binary = work / f'vectors{group}-O{opt}.bin'
        with binary.with_suffix('.log').open('w') as log:
            subprocess.run([root / 'run/cpu6-bare-sdcc', f'-O{opt}', path, '-o', binary],
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        manifest.append(binary.name)

for group in range(4):
    text = '#include <stdio.h>\n'
    checks = []
    for count in range(group * 4, group * 4 + 4):
        values = [0, 1, 127, 128, 255, 256, 32767, 32768, 65535]
        values += [rng.randrange(65536) for _ in range(16)]
        text += f'''unsigned int left{count}(unsigned int x) {{ return x << {count}; }}
unsigned int right{count}(unsigned int x) {{ return x >> {count}; }}
int signed{count}(int x) {{ return x >> {count}; }}
unsigned int byte{count}(unsigned char x) {{ return x >> {count}; }}
int check{count}(void) {{ static unsigned int rows[][5] = {{
'''
        rows = []
        for x in values:
            signed = x if x < 32768 else x - 65536
            rows.append('{' + ','.join(str(v) + 'U' for v in
                                      (x, x << count & 65535, x >> count,
                                       (signed >> count) & 65535, (x & 255) >> count)) + '}')
        text += ',\n'.join(rows)
        text += f'''}}; unsigned int i; for (i=0; i<sizeof(rows)/sizeof(rows[0]); i++) {{
if (left{count}(rows[i][0]) != rows[i][1] || right{count}(rows[i][0]) != rows[i][2] ||
(unsigned int)signed{count}((int)rows[i][0]) != rows[i][3] ||
byte{count}((unsigned char)rows[i][0]) != rows[i][4]) return 1;
}} return 0; }}
'''
        checks.append(f'check{count}()')
    text += 'int main(void) { if (' + ' || '.join(checks)
    text += ') { puts("FAIL shifts"); return 1; } puts("PASS shifts"); return 0; }\n'
    source = work / f'shifts{group}.c'
    source.write_text(text)
    for opt in (0, 1):
        binary = work / f'shifts{group}-O{opt}.bin'
        subprocess.run([root / 'run/cpu6-bare-sdcc', f'-O{opt}', source, '-o', binary], check=True)
        manifest.append(binary.name)

source = work / 'exhaustive.c'
source.write_text('''#include <stdio.h>
unsigned int divide47(unsigned int x) { return x / 47U; }
unsigned int remain47(unsigned int x) { return x % 47U; }
int main(void) {
    unsigned int value = 0, quotient = 0, remainder = 0;
    do {
        if (divide47(value) != quotient || remain47(value) != remainder) {
            puts("FAIL exhaustive divide by 47"); return 1;
        }
        remainder++;
        if (remainder == 47) { remainder = 0; quotient++; }
        value++;
    } while (value != 0);
    puts("PASS all 65536 dividends for /47 and %47"); return 0;
}
''')
binary = work / 'exhaustive.bin'
subprocess.run([root / 'run/cpu6-bare-sdcc', '-O1', source, '-o', binary], check=True)
manifest.append(binary.name)
for name in manifest:
    output = subprocess.check_output([root / 'tests/centurion-cli', '-T', '40000000',
                                      '-A', '2000', '-b', work / name], text=True)
    (work / (name + '.run.log')).write_text(output)
    assert 'PASS' in output and 'FAIL' not in output and 'System halted' in output, name
    print('PASS instruction core:', name, flush=True)
(work / 'manifest.json').write_text(json.dumps(manifest))
with (work / 'microcode.log').open('w') as log:
    subprocess.run(['node', root / 'tests/compiler-audit-core.js', work, work / 'manifest.json'],
                   env=dict(os.environ, CPU6_MICROSTEP_LIMIT='400000000'),
                   stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
print('PASS all constant arithmetic cases on both cores')
