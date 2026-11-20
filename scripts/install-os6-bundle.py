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
from centos_disk import allocated_sectors, file_entry, root_entry, write_file
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('image',type=Path)
parser.add_argument('files',nargs='*',help='NAME=program.exf')
parser.add_argument('-o',required=True,type=Path)
parser.add_argument('--job',action='append',default=[],help='NAME=encoded-text-file, install a type A job')
parser.add_argument('--data-file',action='append',default=[],help='NAME:sectors[:A|B], preallocate a data file (B by default)')
parser.add_argument('--library',help='create a native library for qualified LIB.NAME files')
args=parser.parse_args()
if not args.files and not args.job:parser.error("at least one executable or job is required")
if args.library and not re.fullmatch('[A-Z][A-Z0-9]{0,9}',args.library):parser.error('invalid library name')
seed=args.image.resolve(strict=True);output=args.o.resolve()
if output.exists() or output==seed:parser.error('output must be a new distinct image')
seed_data = seed.read_bytes()
files=[];alloc=[]

def valid_name(path, pattern):
 library, separator, member = path.rpartition('.')
 return (not separator or library == args.library) and re.fullmatch(pattern,member)

for item in args.files:
 name,filename=item.split('=',1);p=Path(filename).resolve(strict=True)
 if not valid_name(name,'[A-Z0-9_?@]{1,10}'):parser.error('invalid name '+name)
 data=p.read_bytes()
 if not data or len(data)%400:parser.error('CBIN must contain whole 400-byte sectors')
 files.append((name,p,'EXF'));alloc.append([name,'E',len(data)//400])
for item in args.job:
 name,filename=item.split('=',1);p=Path(filename).resolve(strict=True)
 if not valid_name(name,'[A-Z][A-Z0-9]{0,9}'):parser.error('invalid job name '+name)
 size=p.stat().st_size
 if not size or size%400:parser.error('job must contain encoded 400-byte sectors')
 files.append((name,p,'TXT'));alloc.append([name,'A',size//400])
for item in args.data_file:
 parts=item.split(':')
 if len(parts) not in (2,3):parser.error('invalid data allocation')
 name,size=parts[:2];kind=parts[2] if len(parts)==3 else 'B'
 if not valid_name(name,'[A-Z][A-Z0-9]{0,9}') or int(size)<1 or kind not in ('A','B'):parser.error('invalid data allocation')
 alloc.append([name,kind,int(size)])
root_names = [name for name, _, _ in alloc if '.' not in name]
if args.library:root_names.append(args.library)
if len({item[0] for item in alloc})!=len(alloc) or len(root_names)!=len(set(root_names)):
 parser.error('duplicate filename')
for name in root_names:
 if root_entry(seed_data, name) is not None:parser.error('root file already exists: '+name)

commands = []
if args.library:
 members = [size for name, _, size in alloc if '.' in name]
 if not members:parser.error('the library needs at least one member')
 # The allocator owns the contents of the new library. Reserve directory
 # entries and enough UAL records even if files use one-sector clusters.
 directory_size = (len(members) + 5 + 24) // 25
 table_size = (sum(members) + 4 * (len(members) + 3) + 119) // 120
 free_size = 2
 needed = sum(members) + directory_size + table_size + free_size + len(members) + 16
 # Avoid a one-sector file size increment for a large library in the root UAL.
 volume_size = (needed + 63) // 64 * 64
 commands += [f".NEW {args.library} ON 1 'L' {volume_size}S",
              f'.CLR {args.library}. ON 1 DSIZE={directory_size}S USIZE={table_size}S FSIZE={free_size}S']
commands.extend(f".NEW {name} ON 1 '{kind}' {size}S" for name,kind,size in alloc)
work=Path(tempfile.mkdtemp(prefix='os6-bundle-',dir=os.environ.get('CENTURION_IMAGE_WORK', ROOT/'build/out')));print('Results:',work,flush=True)
original=hashlib.sha256(seed_data).hexdigest()
allocate=work/'allocate.js'
allocate.write_text(r'''const {OS6Session}=require(process.env.BUNDLE_SESSION);

async function allocate() {
    const session = new OS6Session(process.env.BUNDLE_SEED);
    try {
        await session.boot();
        for (const command of JSON.parse(process.env.BUNDLE_COMMANDS)) {
            const output = await session.command(command, /(?:^|\n)(?:DONE\x07?|JX\s*\d.*|ABORT\s*\d.*)\r?\n/);
            if (!/(?:^|\n)DONE\x07?\r?\n/.test(output)) throw new Error(output);
            console.log(command);
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
  env=dict(os.environ,BUNDLE_SESSION=str(ROOT/'scripts/os6-session.js'),BUNDLE_SEED=str(seed),BUNDLE_COMMANDS=json.dumps(commands),BUNDLE_OUT=str(disk)))
result = bytearray(disk.read_bytes())
file_types = {'A': 2, 'B': 1, 'E': 4}
for name, file_type, size in alloc:
    entry = file_entry(result, name)
    if entry is None or entry.kind != file_types[file_type]:
        raise ValueError(f'CENTOS did not allocate {name} as type {file_type}')
    sectors = allocated_sectors(result, entry)
    if len(sectors) < size or len(set(sectors)) != len(sectors):
        raise ValueError(f'{name} has an invalid allocation for {size} sectors')
with (work/'install.log').open('w') as log:
 for name, path, kind in files:
  count = write_file(result, name, kind, path.read_bytes())
  print(f'{name}: {count} sectors written', file=log)
assert hashlib.sha256(seed.read_bytes()).hexdigest()==original
with tempfile.TemporaryDirectory(prefix='.image-', dir=output.parent) as directory:
 pending = Path(directory) / output.name
 pending.write_bytes(result)
 os.link(pending, output)
(work/'result.json').write_text(json.dumps({'image':str(output),'sourceSha256':original,'sourceUnchanged':True,
 'files':{name:{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for name,p,kind in files},'allocations':alloc},indent=2)+'\n')
print('Installed:',output,flush=True)
