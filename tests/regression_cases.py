"""CPU6 regression sources shared by the raw and CENTOS executable builds."""
from pathlib import Path

TESTS = Path(__file__).resolve().parent


def regression_cases(work):
    driver = work / 'numbered-driver.c'
    driver.write_text('''#include <stdio.h>
int case_main(void);
int main(void) {
    if (case_main() != 7) { puts("FAIL numbered regression"); return 1; }
    puts("PASS numbered regression"); return 0;
}
''')
    cases = []
    for source in sorted(TESTS.glob('[0-9][0-9]_*.c')):
        renamed = work / source.name
        renamed.write_text('#define main case_main\n' + source.read_text())
        cases.append((source.stem, [renamed, driver]))
    cases += [
        ('bool', [TESTS / 'sdcc-cpu6-bool.c']),
        ('address', [TESTS / 'sdcc-cpu6-address.c']),
        ('indexed', [TESTS / 'sdcc-cpu6-indexed.c']),
        ('switch-tables', [TESTS / 'sdcc-cpu6-switch.c']),
        ('carry', [TESTS / 'sdcc-cpu6-carry.c']),
        ('flags', [TESTS / 'ee200-flags.c', TESTS / 'ee200-flags.s']),
    ]
    return cases
