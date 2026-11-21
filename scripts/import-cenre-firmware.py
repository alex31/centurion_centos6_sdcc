#!/usr/bin/env python3
"""Extract the firmware data from a local, firmware-bearing CenRE source file."""
import argparse
import base64
import binascii
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / 'swext/centurion-js/src/firmware-spec.json'
DEFAULT_OUTPUT = ROOT / 'build/out/emulator/firmware.json'
LITERAL = re.compile(r'(?:\bconst\s+(\w+)\s*=|\b(\w+)\s*:)\s*bload\(\s*`(.*?)`\s*\)', re.S)


def import_firmware(source, spec, output):
    text = source.read_text()
    lengths = json.loads(spec.read_text())
    roms = {}
    for match in LITERAL.finditer(text):
        name = match[1] or match[2]
        if name not in lengths:
            continue
        if name in roms:
            raise ValueError(f'Duplicate firmware table: {name}')
        encoded = ''.join(match[3].split())
        try:
            roms[name] = base64.b64decode(encoded, validate=True)
        except binascii.Error as error:
            raise ValueError(f'Invalid base64 in firmware table {name}') from error

    mapping = re.search(r'\bmap\s*:\s*new Uint8Array\(\[([^\]]*)\]\)', text, re.S)
    if mapping and 'map' not in roms:
        values = re.sub(r'//[^\n]*', '', mapping[1]).strip().rstrip(',')
        roms['map'] = bytes(int(token.strip(), 16) for token in values.split(','))

    if not roms and '__cenFirmware' in text:
        raise ValueError(
            f'{source} loads firmware from the host via __cenFirmware. '
            'Use the pinned upstream cen.ts with embedded firmware linked in README.md.'
        )

    for name, length in lengths.items():
        if name not in roms:
            raise ValueError(f'Missing firmware table: {name}')
        if len(roms[name]) != length:
            raise ValueError(f'Firmware table {name} must contain {length} bytes')
    data = {
        'format': 'cenre-firmware-v1',
        'roms': {name: base64.b64encode(roms[name]).decode('ascii') for name in lengths},
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('x') as file:
        json.dump(data, file, indent=2)
        file.write('\n')
    return len(data['roms'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--output', type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    try:
        count = import_firmware(args.source, SPEC, args.output)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f'Imported {count} firmware tables into {args.output}')


if __name__ == '__main__':
    main()
