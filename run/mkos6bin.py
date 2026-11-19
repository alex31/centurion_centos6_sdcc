#!/usr/bin/env python3
"""Encode a flat binary in the Centurion CBIN record format.

The module also encodes relocated multi-section OS6 programs. A flat binary
from this command does not by itself include the OS6 startup and metadata.
"""
import argparse
from pathlib import Path

SECTOR = 400
MAX_DATA = 120


def record(address, payload, kind=0):
    body = bytes((kind, len(payload), address >> 8, address & 255)) + payload
    return body + bytes((-sum(body) & 255,))


def make_cbin(data, load, entry, fixups=()):
    if not data or load < 0x100 or load + len(data) > 0x10000:
        raise ValueError('nonempty data must fit in the 16-bit range from 0x0100')
    return make_cbin_sections([(load, data)], entry, fixups)


def make_cbin_sections(sections, entry, fixups=()):
    sections = sorted((addr, bytes(data)) for addr, data in sections)
    if not sections or any(not data or addr < 0 or addr + len(data) > 0x10000
                           for addr, data in sections):
        raise ValueError('sections must be nonempty and fit in 16-bit address space')
    for (addr, data), (next_addr, _) in zip(sections, sections[1:]):
        if addr + len(data) > next_addr:
            raise ValueError('sections overlap')
    if not any(addr <= entry < addr + len(data) for addr, data in sections):
        raise ValueError('entry must lie within a section')
    fixups = list(fixups)
    if len(set(fixups)) != len(fixups):
        raise ValueError('duplicate relocation addresses')
    if any(not any(base <= addr < base + len(data) - 1 for base, data in sections)
           for addr in fixups):
        raise ValueError('relocation word lies outside the binary')
    output = bytearray()

    def flush():
        output.append(0x80)
        output.extend(bytes((-len(output)) % SECTOR))

    for load, data in sections:
        pos = 0
        while pos < len(data):
            remaining = SECTOR - len(output) % SECTOR
            # Retain space for the sector-end marker.
            if remaining < 7:
                flush()
                remaining = SECTOR
            count = min(MAX_DATA, len(data) - pos, remaining - 6)
            output.extend(record(load + pos, data[pos:pos + count]))
            pos += count
    pos = 0
    while pos < len(fixups):
        remaining = SECTOR - len(output) % SECTOR
        if remaining < 8:
            flush()
            remaining = SECTOR
        count = min(MAX_DATA // 2, len(fixups) - pos, (remaining - 6) // 2)
        words = b''.join(addr.to_bytes(2, 'big') for addr in fixups[pos:pos + count])
        output.extend(record(0, words, kind=1))
        pos += count
    if SECTOR - len(output) % SECTOR < 6:
        flush()
    output.extend(record(entry, b''))
    output.append(0x84)
    output.extend(bytes((-len(output)) % SECTOR))
    return bytes(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path, help='flat binary')
    parser.add_argument('-o', '--output', required=True, type=Path)
    parser.add_argument('--load', type=lambda x: int(x, 0), default=0x2000)
    parser.add_argument('--entry', type=lambda x: int(x, 0), default=None)
    parser.add_argument('--relocs', type=Path, help='linker -R file containing word relocations')
    args = parser.parse_args()
    entry = args.load if args.entry is None else args.entry
    try:
        fixups = []
        if args.relocs:
            records = args.relocs.read_bytes()
            if len(records) % 3 or any(records[i] != 1 for i in range(0, len(records), 3)):
                raise ValueError('linker relocations must be 3-byte big-endian word records')
            fixups = [int.from_bytes(records[i + 1:i + 3], 'big') for i in range(0, len(records), 3)]
        args.output.write_bytes(make_cbin(args.input.read_bytes(), args.load, entry, fixups))
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
