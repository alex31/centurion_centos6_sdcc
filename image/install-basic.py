#!/usr/bin/env python3
"""Install Stefan BASIC and the 101 BASIC Computer Games collection."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory

from basic_games import ARCHIVE, prepare_games

ROOT = Path(__file__).resolve().parents[1]
BASIC = ROOT / 'swext/stefan-basic/centos'
sys.path.insert(0, str(ROOT / 'scripts'))
from centos_text import encode


def install(directory, seed, output, check_games):
    if output.exists() or output == seed:
        raise ValueError('BASIC installation requires a new output image')
    output.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix='.basic-install-', dir=output.parent) as temporary:
        work = Path(temporary)
        originals, playable = prepare_games(work)
        base = work / 'basic.img'
        example = ROOT / 'image/programs/life1d/life1d.bas'
        command = [sys.executable, BASIC / 'install.py', directory,
                   '--seed', seed, '--example', f'LIFE1D={example}', '-o', base]
        if check_games:
            command.append('--check-games')
        for name, path in playable.items():
            command.extend(['--example', f'{name}={path}'])
        subprocess.run(command, check=True, cwd=ROOT)

        archive = work / 'catalog.img'
        command = [sys.executable, ROOT / 'scripts/install-os6-bundle.py',
                   base, '--library', ARCHIVE, '-o', archive]
        for name, source in originals.items():
            path = work / f'{name}.txt'
            path.write_bytes(encode(source))
            command.extend(['--job', f'{ARCHIVE}.{name}={path}'])
        subprocess.run(command, check=True, cwd=ROOT)
        subprocess.run(['node', ROOT / 'tests/basic-games.js', archive, *playable],
                       check=True, cwd=ROOT)

        report = json.loads(base.with_suffix('.json').read_text())
        report.update(image=str(output), imageSha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
                      gameArchive=ARCHIVE, originalListings=list(originals), playableGames=list(playable))
        pending = work / 'report.json'
        pending.write_text(json.dumps(report, indent=2) + '\n')
        os.link(archive, output)
        pending.replace(output.with_suffix('.json'))
    print(f'Installed {len(originals)} listings in {ARCHIVE} and {len(playable)} tested ports in SBASIC')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--seed', type=Path, required=True)
    parser.add_argument('-o', '--output', type=Path, required=True)
    parser.add_argument('--check-games', action='store_true')
    args = parser.parse_args()
    try:
        install(args.directory.resolve(), args.seed.resolve(strict=True),
                args.output.resolve(), args.check_games)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
