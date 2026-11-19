"""Read and populate existing root files on a padded CENTOS Hawk disk."""
from dataclasses import dataclass

STRIDE = 512
PAYLOAD = 400
HAWK_BYTES = 6651904
ROOT_SECTOR = 16
ENTRY_BYTES = 16


@dataclass(frozen=True)
class RootEntry:
    kind: int
    allocation_sector: int
    allocation_index: int


def read_u16(data, offset):
    return (data[offset] << 8) | data[offset + 1]


def root_entry(data, name):
    if len(data) != HAWK_BYTES:
        raise ValueError("expected a 6,651,904-byte Hawk image")
    expected = bytes(byte | 0x80 for byte in name.ljust(10).encode("ascii"))
    header = ROOT_SECTOR * STRIDE
    for sector in range(ROOT_SECTOR, ROOT_SECTOR + 100):
        start = sector * STRIDE
        first = ENTRY_BYTES if start == header else 0
        for offset in range(start + first, start + PAYLOAD, ENTRY_BYTES):
            if data[offset:offset + 2] == b"\x84\x8d":
                return None
            if data[offset:offset + 10] == expected:
                return RootEntry(data[offset + 13] & 0x0f,
                                 read_u16(data, offset + 11), data[offset + 10])
    return None


def allocated_sectors(data, entry):
    directory = ROOT_SECTOR * STRIDE
    table = read_u16(data, directory + 14) * STRIDE
    record = table + entry.allocation_sector * STRIDE + entry.allocation_index * 3
    if record + 6 > len(data) or data[record + 4] > 20:
        raise ValueError("invalid CENTOS allocation header")
    cluster_size = 1 << data[record + 4]
    page = record + 6
    index = 0
    seen = set()
    sectors = []
    while index < 155:
        offset = page + index * 3
        if offset in seen or offset + 3 > len(data):
            raise ValueError("invalid CENTOS allocation chain")
        seen.add(offset)
        if data[offset] == 0xff:
            link = data[offset + 1]
            if link == 0xff:
                return sectors
            page = table + (link ^ 0xff) * STRIDE
            index = data[offset + 2]
            continue
        start = read_u16(data, offset + 1)
        end = start + cluster_size
        if end > len(data) // STRIDE:
            raise ValueError("CENTOS allocation extends beyond the disk")
        sectors.extend(range(start, end))
        index += 1
    return sectors



def write_root_file(image, name, kind, data):
    if not data or len(data) % PAYLOAD:
        raise ValueError('payload must contain whole 400-byte sectors')
    entry = root_entry(image, name)
    file_types = {'BIN': 1, 'TXT': 2, 'EXF': 4}
    if entry is None or entry.kind != file_types[kind]:
        raise ValueError(f'a root {kind} named {name} is required')
    sectors = allocated_sectors(image, entry)
    count = len(data) // PAYLOAD
    if count > len(sectors):
        raise ValueError(f'{name} needs {count} sectors, file only has {len(sectors)}')
    if len(set(sectors)) != len(sectors):
        raise ValueError(f'{name} has a duplicate sector in its allocation chain')
    for index, sector in enumerate(sectors[:count]):
        offset = sector * STRIDE
        start = index * PAYLOAD
        image[offset:offset + PAYLOAD] = data[start:start + PAYLOAD]
    return count
