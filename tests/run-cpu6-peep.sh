#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source="$root/swext/sdcc/sdcc"
work="$root/build/out/sdcc"
if [ ! -f "$work/src/SDCCmain.o" ] || [ ! -f "$work/src/cpu6/port.a" ]; then
    echo 'Build SDCC CPU6 first with make toolchain.' >&2
    exit 1
fi
scratch=$(mktemp -d "$work/peep-test.XXXXXX")
trap 'rm -f "$scratch/driver.o" "$scratch/peep.o" "$scratch/cli.o" "$scratch/peep-test"; rmdir "$scratch"' EXIT

# Use SDCC's real type and memory helpers, as the other peephole tests do.
objcopy --redefine-sym main=sdcc_cli_main "$work/src/SDCCmain.o" "$scratch/cli.o"
for unit in "$root/tests/cpu6-peep.c" "$source/src/cpu6/peep.c"; do
    case "$unit" in
        */cpu6-peep.c) object=driver ;;
        *) object=peep ;;
    esac
    cc -std=gnu11 -Wall -Wextra -I"$source/src" -I"$work/src" -I"$work" \
        -I"$source/support/util" -c "$unit" -o "$scratch/$object.o"
done
set -- "$scratch/driver.o" "$scratch/peep.o" "$scratch/cli.o"
for object in "$work/src/"*.o; do
    [ "$object" = "$work/src/SDCCmain.o" ] || set -- "$@" "$object"
done
c++ -o "$scratch/peep-test" "$@" "$work/src/cpu6/port.a" -lm
"$scratch/peep-test"
mv "$scratch/peep-test" "$work/peep-test"
