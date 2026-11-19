#!/usr/bin/env python3
"""Run the extended SDCC CPU6 regression suites and keep their logs."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TESTS = ROOT / "tests"
OUTPUT = ROOT / "build/out/tests"

SUITES = (
    "compiler-audit.py",
    "sdcc-cpu6-language.py",
    "sdcc-cpu6-codegen.py",
    "sdcc-cpu6-constants.py",
    "sdcc-byte-arithmetic.py",
    "sdcc-wide-arithmetic.py",
    "sdcc-native-arithmetic.py",
    "sdcc-divmod.py",
    "sdcc-long-shifts.py",
    "sdcc-native48.py",
    "word48-vectors.py",
    "int48-vectors.py",
    "sdcc-float32.py",
    "sdcc-autoindex.py",
    "sdcc-frame-allocation.py",
    "sdcc-regcall.py",
    "sdcc-inline-asm.py",
    "sdcc-inline.py",
    "sdcc-nonleaf-inline.py",
    "sdcc-inplace-wide.py",
    "sdcc-tail-recursion.py",
    "sdcc-terminal-calls.py",
    "sdcc-static-bodies.py",
    "sdcc-lto.py",
    "sdcc-loop-strength.py",
    "sdcc-rhs-forwarding.py",
    "cpu6-gc.py",
    "cpu6-archive.py",
    "archive-rescan.py",
    "run-cpu6-peep.sh",
    "cpu6-autoindex-rules.py",
    "cpu6-ab-allocation.py",
    "run-ee200-expressions.py",
    "run-ee200-relocations.py",
    "ee200-native-arithmetic.py",
    "autoindex-isa.py",
    "byte-alu-isa.py",
    "big-division.py",
    "native-arithmetic.py",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    logs = Path(tempfile.mkdtemp(prefix="suites-", dir=OUTPUT))
    print(f"Suite logs: {logs}", flush=True)

    def run(name):
        script = TESTS / name
        interpreter = "sh" if script.suffix == ".sh" else sys.executable
        log_path = logs / (script.stem + ".log")
        print(f"RUN  {name}", flush=True)
        with log_path.open("w") as log:
            result = subprocess.run([interpreter, script], cwd=ROOT, stdout=log,
                                    stderr=subprocess.STDOUT)
        if result.returncode:
            print(f"FAIL {name} (exit {result.returncode}, {log_path})", flush=True)
            return False
        print(f"PASS {name}", flush=True)
        return True

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        passed = sum(pool.map(run, SUITES))
    print(f"{passed}/{len(SUITES)} extended suites passed")
    return 0 if passed == len(SUITES) else 1


if __name__ == "__main__":
    sys.exit(main())
