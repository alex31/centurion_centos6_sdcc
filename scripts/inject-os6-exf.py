#!/usr/bin/env python3
"""Fill an existing root file in a new copy of a 512-byte-stride Hawk image.

The file must first be allocated by CENTOS itself, so the volume's directory,
allocation maps and timestamps are not invented by this tool.
"""
import argparse
import pathlib
import re
import sys
from centos_disk import write_root_file


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
    result = bytearray(args.image.read_bytes())
    count = write_root_file(result, name, args.type, args.cbin.read_bytes())
    with args.output.open('xb') as out:
        out.write(result)
    print(f'{name}: {count} sectors written into new image {args.output}')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError) as error:
        print(f'inject-os6-exf: {error}', file=sys.stderr)
        sys.exit(1)
