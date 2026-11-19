#!/usr/bin/env python3
"""Fill an existing root file in a new copy of a 512-byte-stride Hawk image.

The file must first be allocated by CENTOS itself, so the volume's directory,
allocation maps and timestamps are not invented by this tool.
"""
import argparse
import pathlib
import re
from centos_disk import HAWK_BYTES, PAYLOAD, STRIDE, allocated_sectors, root_entry


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=pathlib.Path)
    parser.add_argument('name', help='existing root EXF name')
    parser.add_argument('cbin', type=pathlib.Path)
    parser.add_argument('--type', choices=['EXF', 'TXT', 'BIN'], default='EXF')
    parser.add_argument('-o', '--output', required=True, type=pathlib.Path)
    args = parser.parse_args()
    name = args.name.upper()
    if not re.fullmatch(r'[A-Z0-9_?@]{1,10}', name):
        parser.error('the root filename must be 1..10 supported characters')
    if args.image.resolve() == args.output.resolve():
        parser.error('output must be a different image')
    source = args.image.read_bytes()
    if len(source) != HAWK_BYTES:
        parser.error('expected a 6 651 904-byte Hawk image')
    data = args.cbin.read_bytes()
    if not data or len(data) % PAYLOAD:
        parser.error('CBIN must contain whole 400-byte sectors')
    entry = root_entry(source, name)
    file_types = {"BIN": 1, "TXT": 2, "EXF": 4}
    if entry is None or entry.kind != file_types[args.type]:
        raise ValueError(f"a root {args.type} named {name} is required")
    sectors = allocated_sectors(source, entry)
    count = len(data) // PAYLOAD
    if count > len(sectors):
        raise ValueError(f"CBIN needs {count} sectors, file only has {len(sectors)}")
    if len(set(sectors)) != len(sectors):
        raise ValueError("duplicate sector in allocation chain")
    result = bytearray(source)
    for i, sector in enumerate(sectors[:count]):
        result[sector * STRIDE:sector * STRIDE + PAYLOAD] = data[i * PAYLOAD:(i + 1) * PAYLOAD]
    with args.output.open('xb') as out:
        out.write(result)
    print(f'{name}: {count} sectors written into new image {args.output}')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'inject-os6-exf: {error}', file=sys.stderr)
        sys.exit(1)
