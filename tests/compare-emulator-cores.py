#!/usr/bin/env python3
"""Run the same guest binary through both microcoded cores."""

import argparse
import os
from pathlib import Path
import statistics
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/out'
SPEC = ROOT / 'swext/centurion-js/src/firmware-spec.json'


def run_probe(command, mode, cycles):
    run = subprocess.run(
        [*command, str(cycles), mode], cwd=ROOT, check=True, text=True,
        stdout=subprocess.PIPE,
    )
    states = [line for line in run.stdout.splitlines() if line.startswith('state ')]
    if not states:
        raise RuntimeError('Probe returned no CPU state')
    durations = [float(line.split()[1]) for line in run.stdout.splitlines()
                 if line.startswith('seconds ')]
    realtime = [tuple(map(float, line.split()[1:])) for line in run.stdout.splitlines()
                if line.startswith('realtime ')]
    return states, realtime if mode == 'realtime' else durations


def compare(reference_command, native_command, mode, cycles):
    reference, reference_times = run_probe(reference_command, mode, cycles)
    native, native_times = run_probe(native_command, mode, cycles)
    for index, (expected, actual) in enumerate(zip(reference, native), 1):
        if expected != actual:
            raise RuntimeError(f'CPU state differs at sample {index}\nJS  {expected}\nC++ {actual}')
    if len(reference) != len(native):
        raise RuntimeError(f'Expected {len(reference)} snapshots, got {len(native)}')
    return reference_times, native_times


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cycles', type=int, default=100_000_000)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--cpu', type=int, help='pin both probes to the same logical processor')
    parser.add_argument('--realtime-cycles', type=int, default=0,
                        help='also run the 5 MHz schedulers for this many microcycles')
    args = parser.parse_args()
    if args.rounds < 1 or args.cycles < 1 or args.realtime_cycles < 0:
        parser.error('cycles and rounds must be positive')

    firmware = Path(os.environ.get('CENTURION_FIRMWARE') or ROOT / 'build/inputs/firmware.json')
    pin = [] if args.cpu is None else ['taskset', '-c', str(args.cpu)]
    with tempfile.TemporaryDirectory(prefix='centurion-core-') as directory:
        binary = Path(directory) / 'life.bin'
        subprocess.run([
            ROOT / 'run/cpu6-bare-sdcc', '-O1', '--gc-sections', 'tests/emulator-life-probe.c',
            'image/programs/life1d/evolve.c', '-o', binary,
        ], cwd=ROOT, check=True)
        reference = [*pin, 'node', 'tests/js-emulator-probe.js', binary]
        native = [*pin, OUT / 'emulator-cpp/cpu6-probe', firmware.resolve(), SPEC, binary]
        subprocess.run([*native, '1000000', 'verify'], check=True)
        compare(reference, native, 'trace', 10_000)
        print('PASS: CPU state after each of 10000 LIFE microcycles and during reset', flush=True)

        js_times = []
        cpp_times = []
        for round_number in range(1, args.rounds + 1):
            js, cpp = compare(reference, native, 'bench', args.cycles)
            js_seconds, = js
            cpp_seconds, = cpp
            js_times.append(js_seconds)
            cpp_times.append(cpp_seconds)
            print(f'Round {round_number}: JS {js_seconds:.3f}s, C++ {cpp_seconds:.3f}s, '
                  'CPU and RAM match', flush=True)
        js_rate = args.cycles / statistics.median(js_times) / 1_000_000
        cpp_rate = args.cycles / statistics.median(cpp_times) / 1_000_000
        print(f'Median: JS {js_rate:.2f} M microcycles/s, C++ {cpp_rate:.2f} M microcycles/s, '
              f'{cpp_rate / js_rate:.2f}x')
        if args.realtime_cycles:
            js, cpp = compare(reference, native, 'realtime', args.realtime_cycles)
            for name, [(wall, cpu, wake_p95)] in [('JS', js), ('C++', cpp)]:
                print(f'5 MHz {name}: {wall:.3f}s wall, {100 * cpu / wall:.1f}% of one CPU, '
                      f'p95 wake-up lateness {wake_p95:.3f}ms')


if __name__ == '__main__':
    main()
