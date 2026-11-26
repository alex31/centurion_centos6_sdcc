#!/usr/bin/env python3
"""Check the CPU6 standard library through the bare-metal and CENTOS drivers."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from cpu6_roadmap_support import ROOT, execute


def check_centos(work, seed):
    sys.path.insert(0, str(ROOT / 'scripts'))
    from centos_text import encode

    driver = ROOT / 'run/cpu6-os6-sdcc'
    libc = work / 'libc.exf'
    hello = work / 'hello.exf'
    for source, output in ((ROOT / 'tests/libc-stdlib.c', libc),
                           (ROOT / 'examples/hello_os6.c', hello)):
        subprocess.run([driver, '-Os', '--gc-sections', source, '-o', output], check=True)
    job = work / 'libctest.txt'
    job.write_bytes(encode('.JOB LIBCTEST ABORT=CANCEL\n.RUN XLIBC\n'
                           '.SKIP 1 IF #C NE 23\n.RUN XHELLO6\n.END\nS.CON\n'))
    image = work / 'libc.img'
    subprocess.run([sys.executable, ROOT / 'scripts/install-os6-bundle.py', seed,
                    f'XLIBC={libc}', f'XHELLO6={hello}', '--job', f'LIBCTEST={job}',
                    '-o', image], check=True)
    subprocess.run(['node', ROOT / 'tests/libc-centos.js', image], check=True)


def check_bare_console(work, source):
    binary = work / 'console.bin'
    subprocess.run([ROOT / 'run/cpu6-bare-sdcc', '-Os', '--gc-sections',
                    '-DTEST_BARE_CONSOLE', source, '-o', binary], check=True)
    core = ROOT / 'build/out/test-c-emulator/centurion'
    reply = subprocess.run([core, '-R', '-A', os.environ.get('CPU6_BARE_ORIGIN', '2000'),
                            '-T', '20000000', '-b', binary],
                           input=b'A\xff', stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           check=True, timeout=30).stdout
    assert b'PASS stdlib' in reply and b'FAIL' not in reply and b'System halted' in reply, reply
    print('PASS: polled getchar and exit on bare metal')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--centos', type=Path, metavar='SEED')
    args = parser.parse_args()
    output = ROOT / 'build/out/tests'
    output.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='libc-stdlib-', dir=output))
    print('Results:', work, flush=True)
    if args.centos:
        check_centos(work, args.centos)
    else:
        source = ROOT / 'tests/libc-stdlib.c'
        execute(work, [source],
                [('plain', ['-O1', '--no-gc-sections']), ('small', ['-Os', '--gc-sections'])])
        check_bare_console(work, source)


if __name__ == '__main__':
    main()
