"""Prepare local build inputs for the constexpr CPU6 decoder."""
import base64
import json
from pathlib import Path
import sys
from tempfile import NamedTemporaryFile

directory, firmware, flags, control_store_format = sys.argv[1:]
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


def cpp_rom(rom):
    if not rom:
        return b''
    lines = ['#define CPU6_EMBEDDED 1', 'inline constexpr std::uint8_t control_store_rom[] = {']
    for offset in range(0, len(rom), 7):
        lines.append('    ' + ', '.join(f'0x{byte:02x}' for byte in rom[offset:offset + 7]) + ',')
    lines.append('};\n')
    return '\n'.join(lines).encode('ascii')


rom = b''
if firmware and (pack := Path(firmware)).is_file():
    try:
        rom = cpu6_rom(pack)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f'{pack}: {error}. Building the generic CPU6 decoder.', file=sys.stderr)

# Empty inputs select the generic decoder and invalidate an earlier specialized build.
update('cpu6-control-store.rom', rom if control_store_format == 'embed' else b'')
update('cpu6-control-store.inc', cpp_rom(rom) if control_store_format == 'include' else b'')
update('build-flags', (flags + '\n').encode())
# Extern instantiations prevent a direct jump from instantiating a second copy
# of a handler belonging to another translation unit.
blocks = []
for block in range(8):
    entries = [f'CPU6_ADDRESS({address})' for address in range(block * 256, (block + 1) * 256)]
    blocks.extend([f'#if !defined(CPU6_BLOCK) || CPU6_BLOCK == {block}', *entries, '#endif'])
update('addresses.inc', ('\n'.join(blocks) + '\n').encode())
