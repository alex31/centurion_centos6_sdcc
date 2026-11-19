#!/usr/bin/env python3
"""Generated CPU6 integer regressions, with Python arithmetic as the oracle.

Runs at -O0/-O1 on the instruction emulator and the independent microcoded
core. Generated sources, binaries and logs are retained for reproduction.
No disk image or running emulator is used.
"""
from pathlib import Path
import argparse
import subprocess
import random
import json
import runpy
import sys

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--work', type=Path, default=root/'build/out/tests/compiler-audit')
parser.add_argument('--filter', help='comma-separated probe names')
parser.add_argument('--compiler', type=Path, default=root/'run/cpu6-bare-sdcc',
                    help='SDCC bare-metal compiler wrapper')
args = parser.parse_args()
compiler = args.compiler.resolve()
work = args.work.resolve()
work.mkdir(parents=True, exist_ok=True)

rng=random.Random(0xc006)
head='#include <stdio.h>\nstatic int fail(const char *s, unsigned int i) { puts(s); putchar("0123456789abcdef"[(i>>4)&15]); putchar("0123456789abcdef"[i&15]); putchar(10); return 1; }\n'
probes={}
for bits in (16,32):
 typ='unsigned int' if bits==16 else 'unsigned long';suffix='U' if bits==16 else 'UL';mask=(1<<bits)-1
 fmt=lambda n:f'0x{n&mask:x}{suffix}'
 cases=[(a,b,k) for a,b,k in [(0,1,0),(mask,1,1),(1,mask,bits-1),(mask,mask,8),(1<<(bits-1),3,bits-1),(0xffff,0x101,7)]]
 cases += [(rng.getrandbits(bits),rng.getrandbits(bits) or 1,rng.randrange(bits)) for _ in range(58)]
 rows=[]
 for a,b,k in cases:
  vals=[a,b,a+b,a-b,a*b,a//b,a%b,a&b,a|b,a^b,a<<k,a>>k]
  rows.append('{'+','.join(fmt(n) for n in vals)+f',{k},{int(a<b)}}}')
 s=head+f'typedef {typ} U;\nstruct V {{ U a,b,add,sub,mul,div,rem,band,bor,bxor,left,right; unsigned int count,less; }};\nstatic struct V v[]={{'+','.join(rows)+'};\n'
 operations=[('add','+'),('sub','-'),('mul','*'),('div','/'),('rem','%'),('band','&'),('bor','|'),('bxor','^')]
 for name,op in operations:s+=f'static U f_{name}(U a,U b){{return a{op}b;}}\n'
 s+='static U f_left(U a,unsigned int n){return a<<n;}\nstatic U f_right(U a,unsigned int n){return a>>n;}\n'
 s+='int main(void){ unsigned int i; for(i=0;i<sizeof(v)/sizeof(v[0]);i++){\n'
 for name,op in operations:s+=f'if(f_{name}(v[i].a,v[i].b)!=v[i].{name}) return fail("FAIL u{bits} {name}",i);\n'
 s+=f'if(f_left(v[i].a,v[i].count)!=v[i].left) return fail("FAIL u{bits} left",i);\n'
 s+=f'if(f_right(v[i].a,v[i].count)!=v[i].right) return fail("FAIL u{bits} right",i);\n'
 s+=f'if((v[i].a<v[i].b)!=v[i].less) return fail("FAIL u{bits} less",i);\n'
 s+='}puts("PASS");return 0;}\n';probes[f'random_u{bits}']=s
for bits in (8,16,32):
 typ={8:'unsigned char',16:'unsigned int',32:'unsigned long'}[bits];suf='UL' if bits==32 else 'U';mask=(1<<bits)-1
 for name,op in [('add','+'),('sub','-'),('mul','*'),('div','/'),('rem','%'),('and','&'),('or','|'),('xor','^'),('shl','<<'),('shr','>>')]:
  cases=[(0x5a,3),(mask,1),(mask,mask),(1<<(bits-1),3),(0,1)]
  if op in ('<<','>>'):cases=[(mask,n) for n in (0,1,7,bits-1)]
  if bits==8 and op=='*':cases=[(a%128,b%128 or 1) for a,b in cases]
  rows=[]
  for a,b in cases:
   v={'add':lambda:a+b,'sub':lambda:a-b,'mul':lambda:a*b,'div':lambda:a//b,'rem':lambda:a%b,'and':lambda:a&b,'or':lambda:a|b,'xor':lambda:a^b,'shl':lambda:a<<b,'shr':lambda:a>>b}[name]()&mask
   rows.append('{'+','.join(f'0x{x:x}{suf}' for x in (a,b,v))+'}')
  s=head+f'typedef {typ} U;\nstruct V {{U a,b,result;}};\nstatic struct V v[]={{'+','.join(rows)+'};\n'
  s+=f'static U update(U *p,U b){{return *p {op}= b;}}\n'
  s+='int main(void){ unsigned int i; U x,y; for(i=0;i<sizeof(v)/sizeof(v[0]);i++){x=v[i].a;y=update(&x,v[i].b);'
  s+=f'if(x!=v[i].result) return fail("FAIL u{bits} {op}= memory",i); if(y!=v[i].result) return fail("FAIL u{bits} {op}= value",i);'
  s+='}puts("PASS");return 0;}\n';probes[f'compound_u{bits}_{name}']=s
for bits in (8,16,32):
 typ={8:'unsigned char',16:'unsigned int',32:'unsigned long'}[bits];suf='UL' if bits==32 else 'U';mask=(1<<bits)-1
 for name,op in [('inc','++'),('dec','--')]:
  vals=[0,1,255,256,65535,65536,mask]
  s=head+f'typedef {typ} U;\nstatic U v[]={{'+','.join(f'0x{v&mask:x}{suf}' for v in vals)+'};\n'
  s+=f'static U update(U *p){{return (*p){op};}}\n'
  s+='int main(void){ unsigned int i; U x,y,e; for(i=0;i<sizeof(v)/sizeof(v[0]);i++){x=v[i];'
  s+=f'e=(U)(x {"+" if op=="++" else "-"} 1{suf});y=update(&x); if(y!=v[i]||x!=e)return fail("FAIL u{bits} {op}",i);'
  s+='}puts("PASS");return 0;}\n';probes[f'post_u{bits}_{name}']=s
probes['casts']=head+'''static long f(unsigned int x){return (long)(int)x;}
static unsigned long g(int x){return (unsigned long)x;}
static long h(unsigned char x){return (long)(signed char)x;}
int main(void){if(f(0xffffU)!=-1L)return fail("FAIL cast u16-s16-s32",0); if(g(-1)!=0xffffffffUL)return fail("FAIL cast s16-u32",0);if(h(255)!=-1L)return fail("FAIL cast u8-s8-s32",0);puts("PASS");return 0;}
'''
# Loaded by the main audit generator; Python arithmetic supplies the oracle.
for bits in (16,32):
 typ='int' if bits==16 else 'long'; suf='' if bits==16 else 'L'; lo=-(1<<(bits-1)); hi=(1<<(bits-1))-1
 def fmt(x):
  return f'(-{hi}{suf}-1{suf})' if x==lo else f'{x}{suf}'
 cases=[(lo,1),(lo,3),(hi,-3),(-1,hi),(0,-1),(-655%hi,13)]
 cases += [(rng.randint(lo,hi),rng.randint(lo,hi) or 1) for _ in range(58)]
 rows=[]
 for a,b in cases:
  q=(abs(a)//abs(b))*(-1 if (a<0)!=(b<0) else 1);rem=a-q*b
  flags=(a<b)|((a<=b)<<1)|((a>b)<<2)|((a>=b)<<3)|((a==b)<<4)|((a!=b)<<5)
  rows.append('{'+','.join(fmt(x) for x in (a,b,q,rem))+','+str(flags)+'}')
 s=head+f'typedef {typ} S; struct V {{S a,b,q,r;unsigned int flags;}}; static struct V v[]={{'+','.join(rows)+'};\n'
 s+='static S quotient(S a,S b){return a/b;} static S remainder(S a,S b){return a%b;}\n'
 s+='static unsigned int compare(S a,S b){return (a<b)|((a<=b)<<1)|((a>b)<<2)|((a>=b)<<3)|((a==b)<<4)|((a!=b)<<5);}\n'
 s+='int main(void){unsigned int i;for(i=0;i<sizeof(v)/sizeof(v[0]);i++){'
 for fun,field in [('quotient','q'),('remainder','r'),('compare','flags')]: s+=f'if({fun}(v[i].a,v[i].b)!=v[i].{field})return fail("FAIL s{bits} {fun}",i);'
 s+='}puts("PASS");return 0;}';probes[f'random_s{bits}']=s
probes['nested_long']=head+'''static unsigned long f(unsigned long x){return x+0x12345678UL;}
static unsigned long g(unsigned long x){return x^0x55aa33ccUL;}
static unsigned long combine(unsigned long x,unsigned long y){return (f(x)+g(y))^(g(x)-f(y));}
int main(void){unsigned long x=0xdeadbeefUL,y=0x1234fedcUL;
if(combine(x,y)!=0xa1f06880UL)return fail("FAIL nested long",0);puts("PASS");return 0;}
'''
x,y=0xdeadbeef,0x1234fedc
f=lambda v:(v+0x12345678)&0xffffffff;g=lambda v:v^0x55aa33cc
z=((f(x)+g(y))^(g(x)-f(y)))&0xffffffff
probes['nested_long']=probes['nested_long'].replace('0xa1f06880UL',f'0x{z:x}UL')
probes['function_pointer']=head+'''static long f(int a,long b,int c){return b+a-c;}
static int apply(long (*p)(int,long,int)){return p(-23,0x12345678L,11)==0x12345656L;}
int main(void){if(!apply(f))return fail("FAIL function pointer",0);puts("PASS");return 0;}
'''
probes['large_frame']=head+'''static int run(int n){unsigned char a[260];int i; for(i=0;i<260;i++)a[i]=(unsigned char)(i+n); for(i=0;i<260;i++)if(a[i]!=(unsigned char)(i+n))return 1;return a[259];}
int main(void){if(run(5)!=8)return fail("FAIL large frame",0);puts("PASS");return 0;}
'''
for bits in (16,32):
 typ='unsigned int' if bits==16 else 'unsigned long';suf='U' if bits==16 else 'UL'; top=0xffff if bits==16 else 0xffffffff
 s=head+f'static int choose({typ} x){{switch(x){{case 0:return 1;case 1:return 2;case 258:return 3;case 32768{suf}:return 4;case 0x{top:x}{suf}:return 5;default:return 6;}}}}\n'
 s+=f'int main(void){{if(choose(0)!=1||choose(1)!=2||choose(258)!=3||choose(32768{suf})!=4||choose(0x{top:x}{suf})!=5||choose(12)!=6)return fail("FAIL switch {bits}",0);puts("PASS");return 0;}}'
 probes[f'switch{bits}']=s

probes["promotions"] = "#include <stdio.h>\n" + (root/"tests/15_compound_promotions.c").read_text().replace("return 7;",'puts("PASS"); return 0;')

for bits in (8,16,32):
 typ={8:'signed char',16:'int',32:'long'}[bits];suf='L' if bits==32 else ''
 for name,op in [('mul','*'),('div','/'),('rem','%')]:
  rows=[]
  for a,b in [(-12,3),(12,-3),(-12,-3),(13,-4),(-13,4),(0,1)]:
   q=abs(a)//abs(b)*(-1 if (a<0)!=(b<0) else 1)
   e={'mul':a*b,'div':q,'rem':a-q*b}[name]
   rows.append('{'+','.join(str(x)+suf for x in (a,b,e))+'}')
  probes[f'compound_s{bits}_{name}']=head+f'typedef {typ} S; struct V {{S a,b,e;}};static struct V v[]={{'+','.join(rows)+'};'+f'S update(S *p,S n){{return *p {op}= n;}}'+f'int main(void){{unsigned int i;S x,y;for(i=0;i<6;i++){{x=v[i].a;y=update(&x,v[i].b);if(x!=v[i].e||y!=v[i].e)return fail("FAIL signed {op}=",i);}}puts("PASS");return 0;}}'
probes['byte_guard']=head+'''static unsigned char a[]={0xa5,13,0x5a};
static unsigned char f(unsigned char *p,unsigned char n){return *p *= n;}
int main(void){if(f(a+1,7)!=91||a[0]!=0xa5||a[1]!=91||a[2]!=0x5a)return fail("FAIL byte guard",0);puts("PASS");return 0;}
'''
probes['constant_shifts']=head
for n in range(16):
 probes['constant_shifts']+=f'static unsigned int f{n}(unsigned int x){{return x>>{n};}}\n'
probes['constant_shifts']+='int main(void){unsigned int x;for(x=0;x<65535U;x+=4369U){'
for n in range(16):
 probes['constant_shifts']+=f'if(f{n}(x)!=x/(1U<<{n}))return fail("FAIL constant shift",{n});'
probes['constant_shifts']+='}puts("PASS");return 0;}'

probes["conditional"] = "#include <stdio.h>\n" + (root/"tests/16_conditional_types.c").read_text().replace("return 7;",'puts("PASS"); return 0;')

probes['runtime_abi'] = ''  # The assembly caller is generated separately.
branch_pairs = [(a, b) for a in (-32768, -1, 0, 1, 32767)
                for b in (-32768, -1, 0, 1, 32767)]
branch_rows = []
for a, b in branch_pairs:
    flags = (a < b) | ((a <= b) << 1) | ((a > b) << 2) | ((a >= b) << 3)
    flags |= ((a == b) << 4) | ((a != b) << 5)
    branch_rows.append('{' + ','.join('(-32767-1)' if n == -32768 else str(n)
                                    for n in (a, b, flags)) + '}')
probes['signed_branches'] = head + 'struct V { int a,b,flags; }; static struct V v[]={'
probes['signed_branches'] += ','.join(branch_rows) + '};\n'
for index, op in enumerate(('<', '<=', '>', '>=', '==', '!=')):
    probes['signed_branches'] += (f'static int f{index}(int a,int b) {{ '
                                 f'if(a {op} b) return 13; return -7; }}\n')
probes['signed_branches'] += 'int main(void) { unsigned int i; for(i=0;i<25;i++) {'
for index in range(6):
    probes['signed_branches'] += (f'if(f{index}(v[i].a,v[i].b) != '
                                 f'((v[i].flags & {1 << index}) ? 13 : -7)) '
                                 f'return fail("FAIL signed branch {index}",i);')
probes['signed_branches'] += '} puts("PASS"); return 0; }'
probes['fixed_width_types'] = head + '''#include <stdint.h>
static int32_t extend(int8_t x) { return x; }
static uint32_t multiply(uint32_t a, uint32_t b) { return a * b; }
int main(void) {
    int8_t s8 = -128;
    uint8_t u8 = 255;
    int16_t s16 = -32767 - 1;
    uint16_t u16 = 65535U;
    int32_t s32 = -2147483647L - 1L;
    uint32_t u32 = 4294967295UL;
    if (sizeof(s8) != 1 || sizeof(u8) != 1 ||
        sizeof(s16) != 2 || sizeof(u16) != 2 ||
        sizeof(s32) != 4 || sizeof(u32) != 4) return fail("FAIL widths", 0);
    if (s8 >= 0 || s16 >= 0 || s32 >= 0 || u8 != 255 ||
        u16 != 65535U || u32 != 4294967295UL) return fail("FAIL signs", 0);
    if (extend(s8) != -128L || multiply(65536UL, 65535UL) != 0xffff0000UL)
        return fail("FAIL fixed-width arithmetic", 0);
    puts("PASS");
    return 0;
}
'''
build_abi = runpy.run_path(str(root/'tests/runtime-abi.py'))['build']
results=[]
for name,source in probes.items():
 if args.filter and name not in args.filter.split(","): continue
 path=work/(name+'.c');path.write_text(source)
 for opt in (0,1):
  binary=work/f'{name}-O{opt}.bin';log=work/f'{name}-O{opt}.log'
  if name == 'runtime_abi':
   try:
    build_abi(work, opt, args.compiler.absolute())
    c = subprocess.CompletedProcess([], 0, '', '')
   except subprocess.CalledProcessError as error:
    c = subprocess.CompletedProcess(error.cmd, error.returncode,
                                    error.stdout.decode(), error.stderr.decode())
  else:
   c=subprocess.run([args.compiler.absolute(),f'-O{opt}',path,'-o',binary],capture_output=True,text=True,timeout=30)
  record={'name':name,'opt':opt,'source':str(path),'binary':str(binary)}
  if c.returncode:record.update(result='COMPILE_FAIL',output=c.stdout+c.stderr)
  else:
   try:
    r=subprocess.run([root/'tests/centurion-cli','-T','20000000','-A','2000','-b',binary],capture_output=True,text=True,timeout=30)
    out=r.stdout+r.stderr
    record.update(result='PASS' if r.returncode==0 and '\nPASS' in out and 'System halted' in out else 'FAIL',output=out,size=binary.stat().st_size)
   except subprocess.TimeoutExpired as e: record.update(result='TIMEOUT',output=str(e))
  log.write_text(record['output']);results.append(record)
  print(name,f'-O{opt}',record['result'],record.get('size',''),flush=True)
(work/'results.json').write_text(json.dumps(results,indent=2))

if not results:
    raise SystemExit('No matching probes')
manifest = work/'manifest.json'
manifest.write_text(json.dumps([Path(r['binary']).name for r in results if r['result'] != 'COMPILE_FAIL']))
with (work/'microcode.log').open('w') as log:
    core = subprocess.run(['node', root/'tests/compiler-audit-core.js', work, manifest],
                          stdout=log, stderr=subprocess.STDOUT, timeout=300)
passed = sum(r['result'] == 'PASS' for r in results)
print(f'Instruction core: {passed}/{len(results)} PASS')
if core.returncode == 0:
    micro = json.loads((work/'system-results.json').read_text())
    print(f'Microcoded core: {len(micro)}/{len(micro)} PASS')
else:
    print(f'Microcoded core FAILED: see {work}/microcode.log')
print(f'Results: {work}')
sys.exit(0 if passed == len(results) and core.returncode == 0 else 1)
