"""Show progress while make builds the bootable CENTOS image."""
import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
PROGRESS = 'CENTURION_BUILD_PROGRESS'
STEPS = (
    'Prepare the SDCC toolchain and test emulator',
    'Compile CENTOS applications, regression programs, and BASIC',
    'Allocate disk files and install the CENTOS applications',
    'Install BASIC and test the bundled games',
    'Run the final CENTOS tests and write the bootable image',
)


def step_text(number):
    return f'[{number}/{len(STEPS)}] {STEPS[number - 1]}'


def announce(number):
    if os.environ.get(PROGRESS) == '1':
        print(step_text(number), flush=True)


def clear_line():
    if not (sys.stdout.isatty() and sys.stderr.isatty()):
        return None
    try:
        import curses
    except ImportError:
        return None
    try:
        curses.setupterm(fd=sys.stdout.fileno())
    except curses.error:
        return None
    erase = curses.tigetstr('el')
    return b'\r' + erase if erase else None


def run_in_terminal(command, env, erase):
    output = sys.stdout.buffer
    steps = {step_text(number).encode() for number in range(1, len(STEPS) + 1)}
    status = step_text(1).encode()

    def display(log=b''):
        columns = shutil.get_terminal_size().columns
        output.write(erase + log + status[:max(0, columns - 1)])
        output.flush()

    display()
    try:
        with subprocess.Popen(command, env=env, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT) as build:
            for line in build.stdout:
                message = line.rstrip(b'\r\n')
                if message in steps:
                    status = message
                    display()
                else:
                    display(line if line.endswith(b'\n') else line + b'\n')
            result = build.wait()
    finally:
        output.write(erase)
        output.flush()
    return result if result >= 0 else 128 - result


def main():
    print('Centurion build plan')
    for number in range(1, len(STEPS) + 1):
        print(f'  {step_text(number)}')
    # Rounded from the clean-build measurement on a Ryzen 9 5950X with -j4 JOBS=4.
    print('Estimated time from a clean checkout with the C++ emulator: '
          '4 to 20 minutes.\n', flush=True)

    command = ['make', '-C', str(ROOT), 'all', *sys.argv[1:]]
    env = dict(os.environ, **{PROGRESS: '1'})
    erase = clear_line()
    try:
        if erase is None:
            print(step_text(1), flush=True)
            os.execvpe(command[0], command, env)
        return run_in_terminal(command, env, erase)
    except KeyboardInterrupt:
        return 130
    except FileNotFoundError as error:
        print(f'dobuild: {error}', file=sys.stderr)
        return 127


if __name__ == '__main__':
    sys.exit(main())
