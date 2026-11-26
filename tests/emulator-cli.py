#!/usr/bin/env python3
"""Verify CPU6 emulator loader options and the CBIN entry record."""
import argparse
import os
import shlex
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def record(address, data):
    header = bytes([0, len(data)]) + address.to_bytes(2, 'big') + data
    return header + bytes([-sum(header) & 255])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('emulator', type=Path, nargs='?',
                        default=ROOT / 'build/out/test-c-emulator/centurion')
    emulator = parser.parse_args().emulator.resolve()
    with tempfile.TemporaryDirectory(prefix='centurion-cli-') as temp:
        work = Path(temp)
        env = dict(os.environ, CENTURION_ROM_DIR=temp)

        def run(*args):
            return subprocess.run([emulator, *args], cwd=work, env=env,
                                  stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=5)

        for option, value in [('-A', '10100'), ('-A', '0x00ff'), ('-E', '200xyz'),
                              ('-E', '-1'), ('-l', '65536'), ('-T', 'xyz')]:
            result = run(option, value)
            assert result.returncode and 'not valid' in result.stderr, result

        devices = ROOT / 'swext/centurion-c'
        probe = work / 'events'
        subprocess.run([*shlex.split(os.environ.get('CC', 'cc')), '-I', devices,
                        ROOT / 'tests/c-emulator-events.c', devices / 'mux.c',
                        devices / 'scheduler.c', '-o', probe], check=True)
        subprocess.run([probe], timeout=5, check=True)

        # A CBIN entry record spans no payload and follows a real sector boundary.
        (work / 'bootstrap_unscrambled.bin').write_bytes(bytes(512))
        first = record(0x0100, bytes(2)) + bytes([0x80])
        second = record(0x0100, b'') + bytes([0x84])
        (work / 'sample.cbin').write_bytes(first.ljust(400, b'\0') + second.ljust(400, b'\0'))
        for options, entry in [((), '0100'), (('-E', '0x101'), '0101')]:
            result = run(*options, '-T', '1', 'sample.cbin')
            assert result.returncode == 0 and 'entry at ' + entry in result.stdout, result
        (work / 'program.bin').write_bytes(bytes(65536))
        result = run('-bR', '-A', '0200', '-T', '1', 'program.bin')
        assert result.returncode == 0 and 'entry at 0200' in result.stdout, result
    print('PASS: MUX interrupts, rescheduled events, numeric options, raw binary size, and CBIN entry selection')


if __name__ == '__main__':
    main()
