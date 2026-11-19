#!/usr/bin/env python3
"""Build the SDCC examples and assemble a bootable CENTOS application image."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/out/image'
DEFAULT_SEED = OUT / 'CENTOS_13.IMG'
BASIC = ROOT / 'swext/stefan-basic/centos'
SDK = ROOT / 'run/cpu6-os6-sdcc'
DEFAULT_OUTPUT = OUT / 'centos-apps.img'
sys.path[:0] = [str(ROOT / 'tests'), str(ROOT / 'scripts')]
from regression_cases import regression_cases
from centos_text import encode
SEED_BYTES = 6651904


def ensure_seed(seed):
    if not seed.is_file():
        raise FileNotFoundError(
            f'CENTOS seed image not found: {seed}. Supply your local copy with '
            'SEED=/path/to/CENTOS_13.IMG.'
        )
    if seed.stat().st_size != SEED_BYTES:
        raise ValueError('The application image requires a 512-byte-stride CENTOS seed')


def build_programs(work):
    examples = ROOT / 'image/programs'
    chess = examples / 'chess'
    connect4 = examples / 'connect4'
    sieve = examples / 'sieve'
    life = examples / 'life1d'
    engine = ROOT / 'swext/mcu-max/src'
    kermit = ROOT / 'swext/ekermit'
    programs = {
        'XCHESS': [
            '--stack-size', '4096', '-DMCUMAX_GAME', '-I', engine,
            chess / 'chess.c', chess / 'play.c', chess / 'engine.c',
            engine / 'mcu-max.c', connect4 / 'os6_clock.s',
        ],
        'XCONNECT4': [connect4 / 'connect4.c', connect4 / 'game.c', connect4 / 'os6_clock.s'],
        'XSIEVE': [sieve / 'sieve_optimise.c', sieve / 'clock.s'],
        'XKERMIT': [
            '-DNO_AT', '-DNO_CTRLC', '-DNO_SSW', '-DNO_SCAN', '-DOBUFLEN=256',
            '-DFN_MAX=16', '-DP_PKTLEN=512', '-DNODEBUG', '-I', kermit,
            kermit / 'os6-receiver.c', kermit / 'os6-transport.c',
            kermit / 'os6-serial.s', kermit / 'kermit.c',
        ],
        'XLIFE1D': [life / name for name in ('life1d.c', 'evolve.c', 'writeln.s', 'clock.s')],
    }
    files = []
    for name, sources in programs.items():
        executable = work / (name.lower() + '.exf')
        subprocess.run([SDK, '-O1', '--lto', '--gc-sections', *sources,
                        '--map', executable.with_suffix('.map'), '-o', executable],
                       check=True, cwd=ROOT)
        files.append(f'{name}={executable}')
    return files



def build_tests(work):
    directory = work / 'tests'
    directory.mkdir()
    files = []
    names = []
    mapping = ['# CENTOS regression programs', '']
    for index, (case, sources) in enumerate(regression_cases(directory), 1):
        for opt in (0, 1):
            name = f'T{index:02d}O{opt}'
            executable = directory / (name + '.exf')
            subprocess.run([SDK, f'-O{opt}', *sources, '-o', executable], check=True, cwd=ROOT)
            files.append(f'{name}={executable}')
            names.append(name)
            mapping.append(f'- {name} · {case} -O{opt}')
    (work / 'tests.md').write_text('\n'.join(mapping) + '\n')
    job = directory / 'RUNTESTS.txt'
    commands = '\n'.join('.RUN ' + name for name in names)
    job.write_bytes(encode('.JOB RUNTESTS ABORT=CANCEL\n' + commands + '\n.END\nS.CON\n'))
    return files, job, len(names)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seed', type=Path, default=DEFAULT_SEED)
    parser.add_argument('--output', type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    seed = args.seed.resolve()
    output = args.output.parent.resolve() / args.output.name
    if output == seed or (output.exists() and not (output == DEFAULT_OUTPUT and output.is_symlink())):
        parser.error('the output must be a new image (set APP_IMAGE to choose its path)')
    OUT.mkdir(parents=True, exist_ok=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    ensure_seed(seed)
    work_root = OUT / 'work'
    work_root.mkdir(exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='run-', dir=work_root))
    print(f'Application build directory: {work}', flush=True)
    env = dict(os.environ, CENTURION_IMAGE_WORK=str(work))

    def run(*command):
        subprocess.run(command, check=True, cwd=ROOT, env=env)

    programs = build_programs(work)
    tests, job, test_count = build_tests(work)
    basic_artifacts = work / 'basic'
    run(sys.executable, BASIC / 'build.py', '--commands', '--vm-math', '--profile', 'size',
        '--no-auto-inline', '--stack-size', '1536',
        '-o', basic_artifacts / 'BASIC.exf')
    games = work / 'programs.img'
    life = work / 'LIFE1D.txt'
    life.write_bytes(encode((ROOT / 'image/programs/life1d/life1d.bas').read_text()))
    run(sys.executable, ROOT / 'scripts/install-os6-bundle.py', seed, *programs, *tests,
        '--job', f'RUNTESTS={job}', '--job', f'LIFE1D={life}', '-o', games)
    final_image = work / 'centos-apps.img'
    run(sys.executable, BASIC / 'install.py', basic_artifacts, '--seed', games,
        '--check-games', '-o', final_image)
    run('node', ROOT / 'tests/apps-image.js', final_image, str(test_count))
    run('node', ROOT / 'tests/basic-life.js', final_image, '1', work / 'basic-life.json')
    run('node', ROOT / 'tests/games-vt100.js', final_image)
    with tempfile.TemporaryDirectory(prefix='.image-', dir=output.parent) as directory:
        pending = Path(directory) / output.name
        if output == DEFAULT_OUTPUT:
            pending.symlink_to(final_image.relative_to(OUT))
            os.replace(pending, output)
        else:
            shutil.copyfile(final_image, pending)
            os.link(pending, output)
    print(f'Bootable application image: {output}')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
