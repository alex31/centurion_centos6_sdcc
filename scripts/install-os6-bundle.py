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
from centos_disk import root_entry
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
work=Path(tempfile.mkdtemp(prefix='os6-bundle-',dir=ROOT/'build/out'));print('Results:',work,flush=True)
original=hashlib.sha256(seed_data).hexdigest()
allocate=work/'allocate.js'
allocate.write_text('''const {OS6Session}=require(process.env.BUNDLE_SESSION);
(async()=>{const s=new OS6Session(process.env.BUNDLE_SEED);try{await s.boot();
for(const [name,type,size] of JSON.parse(process.env.BUNDLE_FILES)){
const reply=await s.command(`.NEW ${name} ON 1 '${type}' ${size}S`,'DONE');
if(/ABORT|ERROR|CANCELLED|JX\\s*\\d/.test(reply))throw Error(reply);console.log("Allocated",name,type,size);}
s.saveNew(process.env.BUNDLE_OUT);}finally{s.stop();}})()
.then(()=>process.exit(0),e=>{console.error(e);process.exit(1)});''')
disk=work/'allocated.img'
with (work/'allocate.log').open('w') as log:
 subprocess.run(['node',allocate],stdout=log,stderr=subprocess.STDOUT,check=True,timeout=240,
  env=dict(os.environ,BUNDLE_SESSION=str(ROOT/'scripts/os6-session.js'),BUNDLE_SEED=str(seed),BUNDLE_FILES=json.dumps(alloc),BUNDLE_OUT=str(disk)))
for i,(name,p,kind) in enumerate(files):
 target=output if i==len(files)-1 else work/('disk%d.img'%i)
 with (work/(name+'.log')).open('w') as log:
  subprocess.run(['python3',ROOT/'scripts/inject-os6-exf.py',disk,name,p,'--type',kind,'-o',target],stdout=log,stderr=subprocess.STDOUT,check=True)
 disk=target
assert hashlib.sha256(seed.read_bytes()).hexdigest()==original
(work/'result.json').write_text(json.dumps({'image':str(output),'sourceSha256':original,'sourceUnchanged':True,
 'files':{name:{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for name,p,kind in files},'allocations':alloc},indent=2)+'\n')
print('Installed:',output,flush=True)
