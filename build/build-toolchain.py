#!/usr/bin/env python3
"""Build SDCC, the EE200 host tools, and the CPU6 target libraries."""
import argparse
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/out"
TOOLS = OUT / "toolchain"
SDCC = ROOT / "swext/sdcc/sdcc"
BINTOOLS = ROOT / "swext/fuzix-bintools"


def build_bintools(cc, jobs, run):
    work = OUT / "bintools"
    work.mkdir(exist_ok=True)
    for source in [BINTOOLS / "Makefile", *BINTOOLS.glob("*.c"), *BINTOOLS.glob("*.h")]:
        target = work / source.name
        if not target.exists() or source.read_bytes() != target.read_bytes():
            shutil.copyfile(source, target)
    binaries = ["asee200", "ldee200", "nmee200", "osizeee200", "dumprelocsee200"]
    run(["make", f"-j{jobs}", f"CC={shlex.join(cc)}", "CFLAGS=-O2 -Wall -DENABLE_RESCAN=1",
         *binaries], work)
    dest = TOOLS / "bin"
    dest.mkdir(parents=True, exist_ok=True)
    for name in binaries:
        shutil.copy2(work / name, dest / name)


def build_sdcc(jobs, run, cc):
    work = OUT / "sdcc"
    work.mkdir(exist_ok=True)
    prefix = TOOLS / "sdcc-cpu6"
    configure = SDCC / "configure"
    ports = re.findall(r"^AC_DO_PORT(?:_ENABLER)?\((\w+),",
                       (SDCC / "configure.ac").read_text(), re.M)
    options = [f"--prefix={prefix}", "--without-ccache"]
    options += [f"--disable-{port}-port" for port in ports if port != "cpu6"]
    options += [f"--disable-{option}" for option in
                ("ucsim", "device-lib", "packihx", "sdcdb", "sdbinutils", "non-free")]
    status = work / "config.status"
    if not status.exists() or configure.stat().st_mtime > status.stat().st_mtime:
        run([configure, *options, f"CC={shlex.join(cc)}"], work)
    run(["make", f"-j{jobs}", "-C", "src"], work)

    # The preprocessor only needs libiberty from the binutils tree.
    library = work / "support/sdbinutils/libiberty"
    library.mkdir(parents=True, exist_ok=True)
    if not (library / "Makefile").exists():
        run([SDCC / "support/sdbinutils/libiberty/configure", "--disable-shared",
             f"CC={shlex.join(cc)}"], library)
    run(["make", f"-j{jobs}"], library)
    run(["make", f"-j{jobs}", "-C", "support/cpp"], work)
    run(["make", "-C", "support/cpp/gcc", "install-cpp", "install-common"], work)
    (prefix / "bin").mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".sdcc-", dir=prefix / "bin") as stage:
        staged = Path(stage) / "sdcc"
        shutil.copy2(work / "src/sdcc", staged)
        staged.replace(prefix / "bin/sdcc")
    run([prefix / "bin/sdcc", "--version"], ROOT)


def build_runtime(run):
    for script in ("build-int48.py", "build-os6-lib.py", "build-float32.py",
                   "build-compiler-runtime.py"):
        run([sys.executable, ROOT / "scripts" / script], ROOT)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--cc", default="cc")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    cc = shlex.split(args.cc)
    if not cc:
        parser.error("--cc must specify a compiler")
    for program in (cc[0], "make", "flex", "ar", "install"):
        if not shutil.which(program):
            parser.error(f"{program} is required")
    if not (shutil.which("bison") or shutil.which("byacc")):
        parser.error("GNU Bison or Berkeley Yacc is required")
    OUT.mkdir(parents=True, exist_ok=True)
    log_path = OUT / "toolchain.log"
    env = dict(os.environ)
    env.pop("MAKEFLAGS", None)
    env.pop("MFLAGS", None)

    with log_path.open("w") as log:
        def run(command, cwd):
            command = list(map(str, command))
            print(f"{cwd}$ {shlex.join(command)}", file=log, flush=True)
            subprocess.run(command, cwd=cwd, env=env, stdout=log,
                           stderr=subprocess.STDOUT, check=True)

        try:
            build_bintools(cc, args.jobs, run)
            build_sdcc(args.jobs, run, cc)
            build_runtime(run)
        except (OSError, subprocess.CalledProcessError) as error:
            print(f"Toolchain build failed. See {log_path}\n{error}", file=sys.stderr)
            return 1
    print(f"Built SDCC CPU6 and the target libraries. Log: {log_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
