#!/usr/bin/env python3
"""IEEE binary32 values, conversions, ABI, diagnostics and math on both cores."""
import argparse
import ctypes
import json
import math
import os
from pathlib import Path
import random
import struct
import subprocess
import tempfile
from cpu6_roadmap_support import ROOT,execute

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--part',choices=['arithmetic','math','diagnostics'],action='append')
parser.add_argument('--skip-stack',action='store_true',help='omit the larger -O0 ABI diagnostic when it exceeds bare memory')
args=parser.parse_args();parts=args.part or ['arithmetic','math','diagnostics']
# Leave room for the unoptimized vector program below the diagnostic ROM.
os.environ.setdefault('CPU6_BARE_ORIGIN','1800')
work=Path(tempfile.mkdtemp(prefix='sdcc-float32-',dir=ROOT/'build/out/tests'))
print('Results:',work,flush=True)
runtime=[p for p in sorted((ROOT/'lib/float32').glob('*.c')) if p.name!='math.c']
subprocess.run(['cc','-std=c99','-O2','-shared','-fPIC','-fno-fast-math','-ffp-contract=off','-fno-builtin',
                '-I'+str(ROOT/'lib/float32'),ROOT/'tests/float32-oracle.c',*runtime,'-lm','-o',work/'oracle.so'],check=True)
native=ctypes.CDLL(str(work/'oracle.so'));native.oracle.argtypes=[ctypes.c_uint,ctypes.c_uint32,ctypes.c_uint32];native.oracle.restype=ctypes.c_uint32
def oracle(op,a,b=0):return int(native.oracle(op,a,b))
def literal(value):return '0x%08xUL'%value
head='''#include <stdio.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <cpu6abi.h>
union word {float f;uint32_t u;};
static int same(uint32_t a,uint32_t b){
 if((b&0x7fffffffUL)>0x7f800000UL)
  return (a&0x7fffffffUL)>0x7f800000UL && (!(b&0x00400000UL)||(a&0x00400000UL));
 return a==b;
}
static int fail(const char *s,unsigned n){puts(s);putchar("0123456789abcdef"[(n>>4)&15]);putchar("0123456789abcdef"[n&15]);putchar(10);return 1;}
'''
edge=[0,0x80000000,1,0x80000001,0x7fffff,0x800000,0x800001,0x3f800000,0x3f800001,
      0x33800000,0x3f000000,0x40000000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,
      0x7fc00000,0x7f800001,0xffc12345,0x4f000000,0xcf000000,0x4f7fffff,0x4f800000]
rng=random.Random(0xf32c006)
if 'arithmetic' in parts:
    directory=work/'arithmetic';directory.mkdir()
    pairs=[(a,edge[(i*7+3)%len(edge)]) for i,a in enumerate(edge)]
    pairs += [(0,0),(0x80000000,0x80000000),(0,0x80000000),(0x7f800000,0xff800000),
              (0x3f800000,0x33800000),(0x3f800001,0x33800000),(0x800000,0x3f000000)]
    pairs += [(0x447a1000,0xc0000000),(0xc47a1000,0x3f000000),(0xbf800000,1),
              (0x80000001,0x80000001),(0x800001,0x40000000),(0x7f7fffff,0x3f000000)]
    pairs += [(0x3fc00001,0x3fc00001),(0x3f900001,0x3f900001)]
    pairs += [(0x3f80ffff,0x3fffffff),(0x3f808000,0x3ffffffe),(0x3f807fff,0x3f810000)]
    pairs += [(0x7f000001,0x00800003),(0x80800001,0x00000005)]
    pairs += [(rng.getrandbits(32),rng.getrandbits(32)) for _ in range(34)]
    source=head+'''float add(float a,float b){return a+b;} float sub(float a,float b){return a-b;}
float mul(float a,float b){return a*b;} float divv(float a,float b){return a/b;}
float addzero(float a){return a+0.0f;} float mulzero(float a){return a*0.0f;}
float selfsub(float a){return a-a;} float selfdiv(float a){return a/a;}
float assoc(float a){return (a+16777216.0f)-16777216.0f;}
unsigned relation(float a,float b){return (a==b)|((a!=b)<<1)|((a<b)<<2)|((a<=b)<<3)|((a>b)<<4)|((a>=b)<<5)|((!(a<b))<<6)|((!(a>=b))<<7);}
float recurse(float a,unsigned n){if(!n)return a;return recurse(a+0.5f,n-1);}
float mixed(unsigned tag,float a,unsigned n) CPU6_REGCALL {return a+(float)tag+(float)n;}
static float (*volatile callbacks[])(float,float)={add,mul};
struct row {uint32_t a,b,add,sub,mul,divv,fromi,fromu,addzero,mulzero,selfsub,selfdiv,assoc;unsigned relations;};
static const volatile struct row rows[]={
'''
    for a,b in pairs:
        values=[a,b,*[oracle(op,a,b) for op in range(4)],oracle(11,a),oracle(12,a),
                *[oracle(op,a) for op in range(17,22)]]
        source+='{'+','.join(map(literal,values))+','+str(oracle(16,a,b))+'U},\n'
    source+='};\nstruct conversion{uint32_t bits,want;unsigned mask;};\nstatic const volatile struct conversion conversions[]={\n'
    conversion_types=[('int8_t',-128,127),('uint8_t',0,255),('int16_t',-32768,32767),('uint16_t',0,65535),('int32_t',-2147483648,2147483647),('uint32_t',0,4294967295)]
    for number in [-2147483648.0,-65535.75,-32768.0,-128.5,-1.9,-0.9,-0.0,0.9,1.9,127.9,255.9,32767.9,65535.0,2147483520.0,4294967040.0]:
        bits=struct.unpack('<I',struct.pack('<f',number))[0];value=struct.unpack('<f',struct.pack('<I',bits))[0];truncated=int(value)
        mask=sum(1<<i for i,(_,low,high) in enumerate(conversion_types) if low<=truncated<=high)
        source+='{'+literal(bits)+','+literal(truncated&0xffffffff)+','+str(mask)+'U},\n'
    source+='''};
int main(void){unsigned i;union word a,b,r;_Bool truth;
if(sizeof(float)!=4||FLT_MANT_DIG!=24||FLT_HAS_SUBNORM!=1)return fail("FAIL format",0);
r.f=(16777216.0f+1.0f)-16777216.0f;if(r.u)return fail("FAIL constexpr",0);
r.f=-0.0f;if(r.u!=0x80000000UL)return fail("FAIL literal negative zero",0);
r.f=1.0f;if(((unsigned char *)&r.f)[0]!=0x3f)return fail("FAIL endian",0);
for(i=0;i<sizeof(rows)/sizeof(rows[0]);i++){
a.u=rows[i].a;b.u=rows[i].b;
'''
    for name in ['add','sub','mul','divv']:
        source+=f'r.f={name}(a.f,b.f);if(!same(r.u,rows[i].{name}))return fail("FAIL {name}",i);\n'
    for name in ['addzero','mulzero','selfsub','selfdiv','assoc']:
        source+=f'r.f={name}(a.f);if(!same(r.u,rows[i].{name}))return fail("FAIL {name}",i);\n'
    source+='''if(relation(a.f,b.f)!=rows[i].relations)return fail("FAIL relations",i);
r.f=-a.f;if(r.u!=(a.u^0x80000000UL))return fail("FAIL negate bits",i);
truth=(_Bool)a.f;if(truth!=((a.u&0x7fffffffUL)!=0)||(!a.f)!=!truth)return fail("FAIL truth",i);
r.f=(float)truth;if(r.u!=(truth?0x3f800000UL:0))return fail("FAIL bool to float",i);
r.f=(float)(int32_t)rows[i].a;if(!same(r.u,rows[i].fromi))return fail("FAIL from signed",i);
r.f=(float)rows[i].a;if(!same(r.u,rows[i].fromu))return fail("FAIL from unsigned",i);
r.f=callbacks[i&1](a.f,b.f);if(!same(r.u,(i&1)?rows[i].mul:rows[i].add))return fail("FAIL callback",i);
}
r.f=recurse(0.0f,20);if(r.u!=0x41200000UL)return fail("FAIL recurse",0);
r.f=mixed(7,1.5f,3);if(r.u!=0x41380000UL)return fail("FAIL register ABI",0);
'''
    source+='for(i=0;i<sizeof(conversions)/sizeof(conversions[0]);i++){a.u=conversions[i].bits;\n'
    for bit,(ctype,_,_) in enumerate(conversion_types):
        source+=f'if((conversions[i].mask&{1<<bit}U)&&(uint32_t)({ctype})a.f!=conversions[i].want)return fail("FAIL to {ctype}",i);\n'
    source+='}\n'
    source+='puts("PASS float32 arithmetic conversions ABI");return 0;}\n'
    (directory/'program.c').write_text(source)
    execute(directory,[directory/'program.c'],[
      ('speed',['-O1']),*([] if args.skip_stack else [('stack',['-O0'])]),('size',['-Os']),
      ('no-peep',['-O1','--no-peep']),('gc',['-O1','--gc-sections'])],common=['--float32'],limit=600_000_000)
if 'math' in parts:
    directory=work/'math';directory.mkdir()
    inputs=edge+[0x3fc90fda,0x3fc90fdb,0x3fc90fdc,0x40490fdb,0x3f490fdb,0x39800000,0x39800001]
    # All boundary cases plus a target-sized random sample; the host oracle
    # covers 100,000 additional values without the 32 KiB target ceiling.
    inputs += [rng.getrandbits(32) for _ in range(12)]
    source=head+'struct row{uint32_t a,sqrt,sin,cos,tan,floor,ceil,trunc,atan,exp,log;};\nstatic const volatile struct row rows[]={\n'
    for a in inputs:source+='{'+','.join(map(literal,[a,*[oracle(op,a) for op in [*range(4,11),22,23,24]]]))+'},\n'
    source+='''};
static const float special[]={INFINITY,NAN,FLT_TRUE_MIN,FLT_MIN,FLT_MAX};
int main(void){unsigned i;union word a,r;
if(!isinf(special[0])||!isnan(special[1])||!isfinite(special[4]))return fail("FAIL special initializers",0);
r.f=special[2];if(r.u!=1UL)return fail("FAIL true min",0);
for(i=0;i<sizeof(rows)/sizeof(rows[0]);i++){a.u=rows[i].a;
'''
    for name in ['sqrt','sin','cos','tan','floor','ceil','trunc','atan','exp','log']:
        source+=f'r.f={name}f(a.f);if(!same(r.u,rows[i].{name}))return fail("FAIL {name}f",i);\n'
    source+='''r.f=fabsf(a.f);if(r.u!=(a.u&0x7fffffffUL))return fail("FAIL fabsf",i);
r.f=copysignf(a.f,-1.0f);if(r.u!=(a.u|0x80000000UL))return fail("FAIL copysignf",i);
if(!!signbit(a.f)!=(unsigned)(a.u>>31))return fail("FAIL signbit",i);
}puts("PASS float32 math");return 0;}
'''
    (directory/'program.c').write_text(source)
    execute(directory,[directory/'program.c'],[('speed',['-O1']),('size',['-O1','--sdcc-option=--opt-code-size']),('gc',['-O1','--gc-sections'])],common=['--float32'],limit=800_000_000)
if 'diagnostics' in parts:
    checks=[('disabled','float f(float a){return a;}',[], 'requires --cpu6-float32'),
      ('double','double f(double a){return a;}',['--cpu6-float32'],'not double'),
      ('long-double','long double f(void){return 0;}',['--cpu6-float32'],'not double'),
      ('literal','float f(void){return 1.0;}',['--cpu6-float32'],'f suffix'),
      ('decimal-suffix','float f(void){return 1.0df;}',['--cpu6-float32'],'f suffix'),
      ('to48','_BitInt(48) f(float x){return x;}',['--cpu6-float32','--cpu6-bitint48'],'8/16/32-bit'),
      ('from48','float f(_BitInt(48) x){return x;}',['--cpu6-float32','--cpu6-bitint48'],'8/16/32-bit'),
      ('varargs','extern void f(int,...);void g(float x){f(1,x);}',['--cpu6-float32'],'typed parameter'),
      ('oldstyle','extern void f();void g(float x){f(x);}',['--cpu6-float32'],'typed parameter')]
    for name,source,flags,message in checks:
        path=work/(name+'.c');path.write_text(source)
        p=subprocess.run([ROOT/'build/out/toolchain/sdcc-cpu6/bin/sdcc','-mcpu6','-S','--std=c99',*flags,path,'-o',work/(name+'.s')],capture_output=True,text=True)
        (work/(name+'.log')).write_text(p.stdout+p.stderr)
        assert p.returncode and message in p.stderr,(name,p.returncode,p.stderr)
    print('PASS float32 diagnostics',flush=True)
(ROOT/'build/out/tests/latest-sdcc-float32.txt').write_text(str(work)+'\n')
