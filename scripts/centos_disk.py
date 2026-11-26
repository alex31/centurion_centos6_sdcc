"""Read and populate existing files on a padded CENTOS Hawk disk."""
from dataclasses import dataclass

STRIDE = 512
PAYLOAD = 400
HAWK_BYTES = 6651904
ROOT_SECTOR = 16
ENTRY_BYTES = 16


@dataclass(frozen=True)
class FileEntry:
    kind: int
    allocation_sector: int
    allocation_index: int
    table: int
    volume: range | tuple[int, ...]


def read_u16(data, offset):
    return (data[offset] << 8) | data[offset + 1]


def _directory_entry(data, name, directory, table, volume):
    expected = bytes(byte | 0x80 for byte in name.ljust(10).encode("ascii"))
    for index, sector in enumerate(directory):
        start = sector * STRIDE
        first = ENTRY_BYTES if index == 0 else 0
        for offset in range(start + first, start + PAYLOAD, ENTRY_BYTES):
            if data[offset:offset + 2] == b"\x84\x8d":
                return None
            if data[offset:offset + 10] == expected:
                return FileEntry(data[offset + 13] & 0x0f,
                                 read_u16(data, offset + 11), data[offset + 10], table, volume)
    return None


def root_entry(data, name):
    if len(data) != HAWK_BYTES:
        raise ValueError("expected a 6,651,904-byte Hawk image")
    table = read_u16(data, ROOT_SECTOR * STRIDE + 14)
    return _directory_entry(data, name, range(ROOT_SECTOR, ROOT_SECTOR + 100),
                            table, range(HAWK_BYTES // STRIDE))


def file_entry(data, path):
    library, separator, member = path.partition('.')
    entry = root_entry(data, library)
    if not separator or entry is None:
        return entry
    if entry.kind != 5:
        raise ValueError(f'{library} is not a CENTOS library')
    volume = tuple(allocated_sectors(data, entry))
    if not volume or len(set(volume)) != len(volume):
        raise ValueError(f'{library} has an invalid allocation')
    table = read_u16(data, volume[0] * STRIDE + 14)
    directory = _directory_entry(data, '@PDIR0', volume[:1], table, volume)
    if directory is None or directory.kind != 0:
        raise ValueError(f'{library} has no library directory')
    return _directory_entry(data, member, allocated_sectors(data, directory), table, volume)


def allocated_sectors(data, entry):
    page = entry.table + entry.allocation_sector
    index = entry.allocation_index
    if page >= len(entry.volume) or index * 3 + 6 > PAYLOAD:
        raise ValueError("invalid CENTOS allocation header")
    record = entry.volume[page] * STRIDE + index * 3
    if data[record + 4] > 20:
        raise ValueError("invalid CENTOS allocation header")
    cluster_size = 1 << data[record + 4]
    index += 2
    seen = set()
    sectors = []
    allocated = set()
    while True:
        if page >= len(entry.volume) or index * 3 + 3 > PAYLOAD:
            raise ValueError("invalid CENTOS allocation chain")
        offset = entry.volume[page] * STRIDE + index * 3
        if offset in seen:
            raise ValueError("invalid CENTOS allocation chain")
        seen.add(offset)
        if data[offset] == 0xff:
            link = data[offset + 1]
            if link == 0xff:
                return sectors
            page = entry.table + (link ^ 0xff)
            index = data[offset + 2]
            continue
        start = read_u16(data, offset + 1)
        end = start + cluster_size
        if end > len(entry.volume):
            raise ValueError("CENTOS allocation extends beyond its volume")
        cluster = entry.volume[start:end]
        if not allocated.isdisjoint(cluster):
            raise ValueError("duplicate sector in CENTOS allocation chain")
        allocated.update(cluster)
        sectors.extend(cluster)
        index += 1


def write_file(image, name, kind, data):
    if not data or len(data) % PAYLOAD:
        raise ValueError('payload must contain whole 400-byte sectors')
    entry = file_entry(image, name)
    file_types = {'BIN': 1, 'TXT': 2, 'EXF': 4}
    if entry is None or entry.kind != file_types[kind]:
        raise ValueError(f'a {kind} named {name} is required')
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


def write_root_file(image, name, kind, data):
    if '.' in name:
        raise ValueError('a root filename is required')
    return write_file(image, name, kind, data)
