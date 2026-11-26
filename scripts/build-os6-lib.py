#!/usr/bin/env python3
"""Rebuild CENTOS C support with SDCC; retain an explicit FCC comparison build."""
import argparse,os,shutil,subprocess,sys,tempfile
from pathlib import Path
from cpu6_archive import assemble_members
from cpu6_object import attach
from cpu6_provenance import compiler_info,input_hashes,sha256,write_manifest
root=Path(__file__).resolve().parents[1];tools=root/'build/out/toolchain'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--printf-sdcc-only',action='store_true',help='compatibility: rebuild only SDCC printf and overlay support')
p.add_argument('--compiler',choices=['sdcc','fcc','both'],default='sdcc')
p.add_argument('--profile',choices=['speed','size'],default='speed')
p.add_argument('--destination',type=Path,default=tools/'lib/os6')
a=p.parse_args();destination=a.destination.resolve();destination.mkdir(parents=True,exist_ok=True)
if a.printf_sdcc_only and a.compiler!='sdcc':p.error('--printf-sdcc-only needs SDCC')
sources=[('crt0.s','crt0'),('console.s','putchar'),('console.c','puts'),('printf.c','printf'),
 ('console-write.s','console-write'),('file.c','file'),('file.s','fileasm'),
 ('input.c','input'),('input.s','inputasm'),('checkpoint.s','checkpoint'),('exf.c','exf'),('exf.s','exfasm'),
 ('exit.c','exit'),('terminate.s','terminate'),('time.c','time'),('clock.s','clock'),
 ('getchar.c','getchar'),('terminal-record.s','terminal-record'),
 ('terminal-key.s','terminal-key'),('curses.c','curses')]
headers=[*(root/'os6/include').glob('*.h'),*(root/'baremetal/include').glob('*.h')]
for family in (['fcc','sdcc'] if a.compiler=='both' else [a.compiler]):
 compiler=tools/('sdcc-cpu6/bin/sdcc' if family=='sdcc' else 'bin/fcc')
 if not compiler.is_file():raise SystemExit('Build '+family+' before its CENTOS runtime')
 info=compiler_info(compiler) if family=='sdcc' else {'path':str(compiler),'sha256':sha256(compiler),'family':'FCC'}
 flags=['-mcpu6','-S','--std=c99','--opt-code-'+a.profile] if family=='sdcc' else ['-mwrx6','-O1','-c']
 flags+=['-I'+str(root/'os6/include'),'-I'+str(root/'baremetal/include')]
 with tempfile.TemporaryDirectory(prefix='cpu6-os6-runtime-',dir=root/'build') as temporary:
  work=Path(temporary);members=[];formatter=[];commands=[]
  for index,(name,stem) in enumerate(sources):
   if a.printf_sdcc_only and name not in ('printf.c','console-write.s'):continue
   source=root/'os6'/name;assembly=work/(stem+'.s');obj=work/(stem+'.o')
   if source.suffix=='.c':
    if family=='sdcc':
     command=[compiler,*flags,'-o',assembly,source];commands.append(list(map(str,command)));subprocess.run(command,check=True)
     if stem=='puts':
      subprocess.run([tools/'bin/asee200','-o',obj,assembly],check=True);attach(obj,assembly.read_text());objects=[obj]
     else:objects=assemble_members(assembly,work,tools/'bin/asee200','os%d'%index)
    else:
     local=work/(stem+'.c');shutil.copy2(source,local)
     command=[compiler,*flags,local];commands.append(list(map(str,command)));subprocess.run(command,cwd=work,check=True);objects=[obj]
   else:
    command=[tools/'bin/asee200','-o',obj,source];commands.append(list(map(str,command)));subprocess.run(command,check=True);attach(obj,source.read_text());objects=[obj]
   if stem in ('crt0','putchar','puts'):
    filename=stem+('-fcc' if family=='fcc' and stem=='puts' else '')+'.o';target=destination/filename
    shutil.copy2(objects[0],target)
    write_manifest(target,info if source.suffix=='.c' else {'assembler':str(tools/'bin/asee200'),'sha256':sha256(tools/'bin/asee200')},flags if source.suffix=='.c' else [],input_hashes([source,*headers]),profile=a.profile,sourceLanguage=source.suffix[1:])
   else:members+=objects
   if stem in ('printf','console-write'):formatter+=objects
  def archive(filename,objects):
   archive=work/filename;subprocess.run(['ar','qc',archive,*objects],check=True)
   target=destination/filename;staged=destination/(filename+'.new');shutil.copy2(archive,staged);os.replace(staged,target)
   write_manifest(target,info,flags,input_hashes([root/'os6'/s for s,_ in sources]+headers),profile=a.profile,commands=commands)
   print('Built',target,flush=True)
  if not a.printf_sdcc_only:archive('libos6-fcc.a' if family=='fcc' else 'libos6.a',members)
  if family=='sdcc':archive('libprintf-sdcc.a',formatter)
if a.compiler!='fcc':subprocess.run([sys.executable,root/'scripts/build-overlay.py','--profile',a.profile,'--destination',destination],check=True)
