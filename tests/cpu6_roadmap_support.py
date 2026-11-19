"""Reproducible execution of identical CPU6 workloads on both CPU cores."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def execute(work, sources, variants, *, common=(), limit=200_000_000, extra_inputs=()):
    # Honour the same supported load-origin option as the compiler and the
    # microcode runner. Large diagnostic corpora can use the full bare RAM.
    origin=os.environ.get('CPU6_BARE_ORIGIN','2000')
    tracked = [*sources, *extra_inputs, ROOT / 'build/out/toolchain/sdcc-cpu6/bin/sdcc', ROOT / 'run/cpu6-sdcc',
               ROOT / 'scripts/cpu6_lto.py', *sorted((ROOT / 'build/out/toolchain/lib').rglob('*.a'))]
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked}
    rows = []
    for name, flags in variants:
        binary = work / (name + '.bin')
        command = [ROOT / 'run/cpu6-bare-sdcc', *common, *flags, *sources,
                   '--save-temps', work / name, '-o', binary]
        with (work / (name + '-build.log')).open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        result = subprocess.run([ROOT / 'tests/centurion-cli', '-A', origin,
                                 '-T', str(limit), '-b', binary], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                check=True, timeout=180)
        (work / (name + '-instruction.log')).write_text(result.stdout)
        assert 'PASS' in result.stdout and 'FAIL' not in result.stdout, (name, result.stdout)
        rows.append({'file': binary.name, 'bytes': binary.stat().st_size,
                     'sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                     'flags': list(common) + flags})
        print('PASS instruction core:', name, flush=True)
    manifest = work / 'manifest.json'
    manifest.write_text(json.dumps([row['file'] for row in rows]))
    with (work / 'microcode.log').open('w') as log:
        subprocess.run(['node', ROOT / 'tests/compiler-audit-core.js', work, manifest],
                       env=dict(os.environ, CPU6_MICROSTEP_LIMIT=str(limit)),
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=600)
    results = json.loads((work / 'system-results.json').read_text())
    for row, metadata in zip(results, rows):
        assert row['file'] == metadata['file'] and row['result'] == 'PASS', row
        row.update(metadata)
        print(row['file'], row['ticks'], 'microsteps,', row['bytes'], 'bytes', flush=True)
    report = {'sources': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
              'loadAddress':int(origin,16),
              'compiler': hashlib.sha256((ROOT / 'build/out/toolchain/sdcc-cpu6/bin/sdcc').read_bytes()).hexdigest(),
              'inputHashes': hashes,
              'results': results}
    assert hashes == {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked}, \
        'source, compiler, wrapper or runtime changed during comparison'
    (work / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    return results
