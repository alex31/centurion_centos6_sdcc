"""Prepare local build inputs for the constexpr CPU6 decoder."""
import base64
import json
from pathlib import Path
import sys
from tempfile import NamedTemporaryFile

directory, firmware, flags = sys.argv[1:]
out = Path(directory)
out.mkdir(parents=True, exist_ok=True)


def update(name, contents):
    path = out / name
    if not path.exists() or path.read_bytes() != contents:
        with NamedTemporaryFile(dir=out, delete=False) as stream:
            stream.write(contents)
        Path(stream.name).replace(path)


def cpu6_rom(path):
    pack = json.loads(path.read_text())
    if pack['format'] != 'cenre-firmware-v1':
        raise ValueError('unsupported firmware pack format')
    # One microinstruction per address, in the same chip order as Cpu6::load_microcode.
    chips = []
    for name in ('e', 'j', 'm', 'l', 'h', 'f', 'k'):
        encoded = pack['roms'][name]
        chip = base64.b64decode(encoded, validate=True)
        if len(chip) != 2048 or base64.b64encode(chip).decode('ascii') != encoded:
            raise ValueError(f'invalid 2048-byte ROM {name}')
        chips.append(chip)
    return b''.join(bytes(word) for word in zip(*chips, strict=True))


rom = b''
if firmware and (pack := Path(firmware)).is_file():
    try:
        rom = cpu6_rom(pack)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f'{pack}: {error}. Building the generic CPU6 decoder.', file=sys.stderr)

# An empty file selects the generic decoder and invalidates an earlier specialized build.
update('cpu6-control-store.rom', rom)
update('build-flags', (flags + '\n').encode())
# Extern instantiations prevent a direct jump from instantiating a second copy
# of a handler belonging to another translation unit.
blocks = []
for block in range(8):
    entries = [f'CPU6_ADDRESS({address})' for address in range(block * 256, (block + 1) * 256)]
    blocks.extend([f'#if !defined(CPU6_BLOCK) || CPU6_BLOCK == {block}', *entries, '#endif'])
update('addresses.inc', ('\n'.join(blocks) + '\n').encode())
