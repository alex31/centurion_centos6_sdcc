#!/usr/bin/env python3
"""Install several named CBIN files into a NEW CENTOS image, without OS changes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
from centos_disk import allocated_sectors, root_entry, write_root_file
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('image',type=Path)
parser.add_argument('files',nargs='*',help='NAME=program.exf')
parser.add_argument('-o',required=True,type=Path)
parser.add_argument('--job',action='append',default=[],help='NAME=encoded-text-file, install a type A job')
parser.add_argument('--data-file',action='append',default=[],help='NAME:sectors[:A|B], preallocate a data file (B by default)')
args=parser.parse_args()
if not args.files and not args.job:parser.error("at least one executable or job is required")
seed=args.image.resolve(strict=True);output=args.o.resolve()
if output.exists() or output==seed:parser.error('output must be a new distinct image')
seed_data = seed.read_bytes()
files=[];alloc=[]
for item in args.files:
 name,filename=item.split('=',1);p=Path(filename).resolve(strict=True)
 if not re.fullmatch('[A-Z0-9_?@]{1,10}',name):parser.error('invalid name '+name)
 data=p.read_bytes()
 if not data or len(data)%400:parser.error('CBIN must contain whole 400-byte sectors')
 files.append((name,p,'EXF'));alloc.append([name,'E',len(data)//400])
for item in args.job:
 name,filename=item.split('=',1);p=Path(filename).resolve(strict=True)
 if not re.fullmatch('[A-Z][A-Z0-9]{0,9}',name):parser.error('invalid job name '+name)
 size=p.stat().st_size
 if not size or size%400:parser.error('job must contain encoded 400-byte sectors')
 files.append((name,p,'TXT'));alloc.append([name,'A',size//400])
for item in args.data_file:
 parts=item.split(':')
 if len(parts) not in (2,3):parser.error('invalid data allocation')
 name,size=parts[:2];kind=parts[2] if len(parts)==3 else 'B'
 if not re.fullmatch('[A-Z][A-Z0-9]{0,9}',name) or int(size)<1 or kind not in ('A','B'):parser.error('invalid data allocation')
 alloc.append([name,kind,int(size)])
if len({item[0] for item in alloc})!=len(alloc):parser.error('duplicate filename')
for name, _, _ in alloc:
 if root_entry(seed_data, name) is not None:parser.error('root file already exists: '+name)
work=Path(tempfile.mkdtemp(prefix='os6-bundle-',dir=os.environ.get('CENTURION_IMAGE_WORK', ROOT/'build/out')));print('Results:',work,flush=True)
original=hashlib.sha256(seed_data).hexdigest()
allocate=work/'allocate.js'
allocate.write_text(r'''const {OS6Session}=require(process.env.BUNDLE_SESSION);

async function allocate() {
    const session = new OS6Session(process.env.BUNDLE_SEED);
    try {
        await session.boot();
        for (const [name, type, size] of JSON.parse(process.env.BUNDLE_FILES)) {
            const command = ".NEW " + name + " ON 1 '" + type + "' " + size + "S";
            await session.command(command, /(?:^|\n)DONE\x07?\r?\n/);
            console.log('Allocated', name, type, size);
        }
        session.saveNew(process.env.BUNDLE_OUT);
    } finally {
        session.stop();
    }
}

allocate().then(() => process.exit(0), error => {
    console.error(error);
    process.exit(1);
});
''')
disk=work/'allocated.img'
with (work/'allocate.log').open('w') as log:
 subprocess.run(['node',allocate],stdout=log,stderr=subprocess.STDOUT,check=True,timeout=240,
  env=dict(os.environ,BUNDLE_SESSION=str(ROOT/'scripts/os6-session.js'),BUNDLE_SEED=str(seed),BUNDLE_FILES=json.dumps(alloc),BUNDLE_OUT=str(disk)))
result = bytearray(disk.read_bytes())
file_types = {'A': 2, 'B': 1, 'E': 4}
for name, file_type, size in alloc:
    entry = root_entry(result, name)
    if entry is None or entry.kind != file_types[file_type]:
        raise ValueError(f'CENTOS did not allocate {name} as type {file_type}')
    sectors = allocated_sectors(result, entry)
    if len(sectors) < size or len(set(sectors)) != len(sectors):
        raise ValueError(f'{name} has an invalid allocation for {size} sectors')
with (work/'install.log').open('w') as log:
 for name, path, kind in files:
  count = write_root_file(result, name, kind, path.read_bytes())
  print(f'{name}: {count} sectors written', file=log)
assert hashlib.sha256(seed.read_bytes()).hexdigest()==original
with tempfile.TemporaryDirectory(prefix='.image-', dir=output.parent) as directory:
 pending = Path(directory) / output.name
 pending.write_bytes(result)
 os.link(pending, output)
(work/'result.json').write_text(json.dumps({'image':str(output),'sourceSha256':original,'sourceUnchanged':True,
 'files':{name:{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for name,p,kind in files},'allocations':alloc},indent=2)+'\n')
print('Installed:',output,flush=True)
