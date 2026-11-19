"""Native type A text: high-bit ASCII records wholly within 400-byte sectors."""
def encode(source):
    output=bytearray()
    for line in source.splitlines():
        raw=line.encode('ascii')
        if not raw:continue
        if len(raw)>398 or any(c<32 and c!=9 or c>126 for c in raw):
            raise ValueError('CENTOS text records need at most 398 printable ASCII characters')
        record=bytes(c|128 for c in raw)+b'\x8d'
        if len(record)+1>400-len(output)%400:
            output.append(0x8d)
            output.extend(b'\0'*(-len(output)%400))
        output.extend(record)
    if len(output)%400==399:output.append(0x8d)
    output.extend(b'\x84\x8d')
    output.extend(b'\0'*(-len(output)%400))
    return bytes(output)

def decode(raw):
    text=bytes(c&127 for c in raw).split(b'\x04',1)[0]
    return text.replace(b'\0',b'').replace(b'\r',b'\n').decode('ascii')
