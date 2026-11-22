"""Compare the Finch sector test on both machines, including its completion time."""

from pathlib import Path
import subprocess
import sys
import tempfile


def run(command):
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE)
    print(result.stdout, end='', flush=True)
    result.check_returncode()
    last = result.stdout.splitlines()[-1]
    if not last.startswith('PASS: Finch ') or ' Microsteps ' not in last:
        raise ValueError(f'Missing Finch cycle count: {last}')
    return int(last.rsplit(' ', 1)[-1])


def compare(native_probe, firmware, spec, program):
    reference = Path(__file__).with_name('finch.js')
    with tempfile.TemporaryDirectory(prefix='finch-compare-') as directory:
        native_image, reference_image = (Path(directory) / name for name in ('native.ffi', 'reference.ffi'))
        for options in ([], ['--late-mount']):
            native_cycles = run([native_probe, firmware, spec, program, native_image, *options])
            reference_cycles = run(['node', reference, program, reference_image, *options])
            subprocess.run(['cmp', native_image, reference_image], check=True)
            if native_cycles != reference_cycles:
                raise ValueError(f'Finch timing differs: native {native_cycles}, JS {reference_cycles}')


if __name__ == '__main__':
    compare(*sys.argv[1:])
