#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source="$root/swext/centurion-js"
output="$root/build/out/emulator"
node -e 'if (+process.versions.node.split(".")[0] < 20) process.exit(1)' || {
    printf '%s\n' 'Node.js 20 or newer is required.' >&2
    exit 1
}
mkdir -p "$output" "$output/server/disks"
cp "$source/package.json" "$source/package-lock.json" "$output/"
npm --prefix "$output" ci --include=dev --ignore-scripts --no-audit --no-fund
node "$source/build.cjs" "$output"
