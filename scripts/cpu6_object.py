"""Self-contained CPU6 object IR, backward-compatible with the EE200 prefix.

The legacy object bytes are unchanged. A bounded, checksummed assembly-IR trailer
allows link-time pruning/reassembly after the original .c/.s files are gone.
"""
import hashlib
import json
from pathlib import Path
import struct
import zlib

MAGIC=b'CPU6IR1\0'
MAX_IR=16*1024*1024

def unpack(data):
    if not data.endswith(MAGIC):return data,None
    if len(data)<12:raise ValueError('truncated CPU6 IR trailer')
    size=struct.unpack_from('<I',data,len(data)-12)[0]
    if size>MAX_IR or size>len(data)-12:raise ValueError('invalid CPU6 IR trailer size')
    start=len(data)-12-size;raw=data[:start]
    decoder=zlib.decompressobj()
    try:
        payload=decoder.decompress(data[start:start+size],MAX_IR+1)
    except zlib.error as error:
        raise ValueError('invalid compressed CPU6 IR payload') from error
    if len(payload)>MAX_IR or not decoder.eof or decoder.unused_data:
        raise ValueError('invalid or oversized CPU6 IR payload')
    metadata=json.loads(payload)
    if not isinstance(metadata, dict) or metadata.get('version')!=1 or not isinstance(metadata.get('assembly'),str):
        raise ValueError('unsupported CPU6 IR version')
    if hashlib.sha256(raw).hexdigest()!=metadata.get('objectSha256'):
        raise ValueError('CPU6 object changed after its IR was attached')
    return raw,metadata

def attach(path,assembly):
    path=Path(path);raw,_=unpack(path.read_bytes())
    symbols(raw) # Validate the legacy object before recording IR.
    metadata={'version':1,'assembly':str(assembly),'objectSha256':hashlib.sha256(raw).hexdigest()}
    encoded=json.dumps(metadata,separators=(',',':')).encode()
    if len(encoded)>MAX_IR:raise ValueError('CPU6 IR payload too large')
    payload=zlib.compress(encoded,9)
    if len(payload)>MAX_IR:raise ValueError('CPU6 IR payload too large')
    path.write_bytes(raw+payload+struct.pack('<I',len(payload))+MAGIC)

def symbols(data):
    """Read classic 16-bit EE200 public/undefined symbols, not debug locals."""
    if len(data)<108 or data[:2]!=b'\x1b\x3d' or data[2]!=11:
        raise ValueError('expected a 16-bit EE200 object')
    sizes=struct.unpack_from('<15H',data,68)
    begin,end=struct.unpack_from('<II',data,100)
    if not 108<=begin<=end<=len(data) or (end-begin)%19:
        raise ValueError('invalid EE200 symbol table')
    defined=set();undefined=set()
    for offset in range(begin,end,19):
        kind=data[offset]
        name=data[offset+1:offset+17].split(b'\0',1)[0].decode('ascii')
        (undefined if kind&0x80 else defined).add(name)
    return defined,undefined,sizes

def archive_members(path):
    """Read ordinary GNU/BSD ar; ignore indices, preserve member order."""
    data=Path(path).read_bytes()
    if not data.startswith(b'!<arch>\n'):
        raise ValueError('expected an ordinary ar archive (thin archives are unsupported)')
    offset=8;names=b''
    while offset<len(data):
        header=data[offset:offset+60]
        if len(header)!=60 or header[58:]!=b'`\n':raise ValueError('invalid ar member header')
        length=int(header[48:58]);name=header[:16].decode('ascii').rstrip()
        if length < 0:raise ValueError('negative ar member size')
        start=offset+60;body=data[start:start+length]
        if len(body)!=length:raise ValueError('truncated ar member')
        offset=start+length+(length&1)
        if name=='//':names=body;continue
        if name in ('/','/SYM64/') or name.startswith('__.SYMDEF'):continue
        if name.startswith('#1/'):
            n=int(name[3:])
            if not 0 < n <= length:raise ValueError('invalid BSD ar long name')
            name=body[:n].rstrip(b'\0').decode();body=body[n:]
        elif name.startswith('/'):
            n=int(name[1:]);end=names.find(b'/\n',n)
            if n<0 or end<=n:raise ValueError('invalid GNU ar long name')
            name=names[n:end].decode()
        else:name=name.rstrip('/')
        if body[:2]==b'\x1b\x3d':yield name,body
