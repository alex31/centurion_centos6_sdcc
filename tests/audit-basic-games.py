#!/usr/bin/env python3
"""Attempt to load and run each original 101 BASIC listing on a scratch disk."""
from concurrent.futures import ThreadPoolExecutor
import argparse
import json
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(ROOT / 'image'), str(ROOT / 'scripts')]
from basic_games import UPSTREAM
from centos_disk import PAYLOAD, STRIDE, allocated_sectors, file_entry
from centos_text import encode


def audit(image, report, jobs):
    files = sorted(UPSTREAM.glob('*.bas'))
    if not files:
        raise ValueError('Initialize the 101 BASIC games submodule')
    disk = image.read_bytes()
    slot = file_entry(disk, 'SBASIC.BDEMO')
    if slot is None or slot.kind != 2:
        raise ValueError('The image needs the native BASIC example BDEMO')
    sectors = allocated_sectors(disk, slot)
    with TemporaryDirectory(prefix='basic-games-audit-') as temporary:
        work = Path(temporary)
        for source in files:
            listing = encode(source.read_text(encoding='ascii'))
            if len(listing) > len(sectors) * PAYLOAD:
                raise ValueError(f'The BDEMO test file is too small for {source.name}')
            (work / source.stem).write_bytes(listing)
        jobs = min(jobs, len(files))

        def worker(index):
            names = [source.stem for source in files[index::jobs]]
            manifest = work / f'worker-{index}.json'
            output = work / f'worker-{index}.ndjson'
            manifest.write_text(json.dumps({
                'image': str(image), 'work': str(work), 'names': names,
                'sectors': sectors, 'stride': STRIDE, 'payload': PAYLOAD, 'output': str(output),
            }))
            subprocess.run(['node', ROOT / 'tests/audit-basic-games.js', manifest], check=True)
            return [json.loads(line) for line in output.read_text().splitlines()]

        with ThreadPoolExecutor(max_workers=jobs) as pool:
            results = sorted((row for rows in pool.map(worker, range(jobs)) for row in rows),
                             key=lambda row: row['name'])
    report.write_text(json.dumps(results, indent=2) + '\n')
    failed = [row['name'] for row in results if row['status'] == 'harness-error']
    if failed:
        raise ValueError('The emulator audit could not complete for ' + ', '.join(failed))
    if [row['name'] for row in results] != [source.stem for source in files]:
        raise ValueError('Audit report did not cover every original listing')
    print(f'Tested {len(results)} original listings. Report written to {report}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('report', type=Path)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    audit(args.image.resolve(strict=True), args.report.resolve(), args.jobs)


if __name__ == '__main__':
    main()
