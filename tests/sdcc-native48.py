#!/usr/bin/env python3
"""Check native 48-bit C operators against Python integers on both CPU cores."""
import json
import os
from pathlib import Path
import random
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
work = Path(tempfile.mkdtemp(prefix='sdcc-native48-', dir=root / 'build/out/tests'))
print('Results:', work, flush=True)
mask, sign = (1 << 48) - 1, 1 << 47
rng = random.Random(0x48c23)

def literal(n):
    n &= mask
    return '(((uint48_t)0x%xUL << 16) | 0x%xU)' % (n >> 16, n & 65535)

def signed(n):
    return n - (1 << 48) if n & sign else n

def comparisons(a,b):
    return sum(1 << i for i,v in enumerate((a==b,a!=b,a<b,a<=b,a>b,a>=b)) if v)

pairs = [(0,1),(mask,1),(mask,mask),(sign,mask),(sign,2),(sign-1,sign),
         (65535,65536),(0x123456789abc,0xabcdef123456)]
pairs += [(mask,1 << bit) for bit in (0,1,15,16,17,31,32,33,46,47)]
pairs += [(rng.getrandbits(48),rng.getrandbits(48) or 1) for _ in range(22)]
rows = []
for i,(a,b) in enumerate(pairs):
    sa,sb = signed(a),signed(b)
    valid_signed = not (sa == -sign and sb == -1)
    sq = abs(sa)//abs(sb)*(-1 if (sa<0)!=(sb<0) else 1) if valid_signed else 0
    sr = sa-sq*sb if valid_signed else 0
    count = (0,1,15,16,17,31,32,33,46,47)[i % 10]
    values = [a,b,a+b,a-b,a*b,a//b,a%b,-a,~a,a&b,a|b,a^b,a<<count,a>>count,
              a<<1,a>>1,sq,sr,sa>>count,sa>>1]
    rows.append('{' + ','.join(map(literal,values)) + ',' +
                ','.join(map(str,(count,comparisons(a,b),comparisons(sa,sb),int(valid_signed)))) +
                ',%dUL,%dU}' % (a & 0xffffffff, a & 65535))
source = '''#include <stdint.h>
#include <stdio.h>
#include <int48.h>
struct vector {
    uint48_t a,b,add,sub,mul,q,r,neg,inv,and_,or_,xor_,left,right,double_,half,sq,sr,sright,shalf;
    unsigned count,ucmp,scmp,valid_signed;
    uint32_t low32;
    uint16_t low16;
};
static const struct vector vectors[] = {
''' + ',\n'.join(rows) + '''};
static volatile uint48_t observed;
static uint48_t add(uint48_t a,uint48_t b) { return a+b; }
static uint48_t (*volatile call_target)(uint48_t,uint48_t)=add;
static uint48_t sub(uint48_t a,uint48_t b) { return a-b; }
static uint48_t mul(uint48_t a,uint48_t b) { return a*b; }
static uint48_t divu(uint48_t a,uint48_t b) { return a/b; }
static uint48_t remu(uint48_t a,uint48_t b) { return a%b; }
static int48_t divs(int48_t a,int48_t b) { return a/b; }
static int48_t rems(int48_t a,int48_t b) { return a%b; }
static uint48_t left(uint48_t a,unsigned n) { return a<<n; }
static uint48_t right(uint48_t a,unsigned n) { return a>>n; }
static int48_t sright(int48_t a,unsigned n) { return a>>n; }
static uint48_t twice(uint48_t a) { return a<<1; }
static uint48_t half(uint48_t a) { return a>>1; }
static int48_t shalf(int48_t a) { return a>>1; }
static unsigned compare_u(uint48_t a,uint48_t b) {
    unsigned result=0;
    if(a==b) result|=1; if(a!=b) result|=2;
    if(a<b) result|=4; if(a<=b) result|=8;
    if(a>b) result|=16; if(a>=b) result|=32;
    return result;
}
static unsigned compare_s(int48_t a,int48_t b) {
    unsigned result=0;
    if(a==b) result|=1; if(a!=b) result|=2;
    if(a<b) result|=4; if(a<=b) result|=8;
    if(a>b) result|=16; if(a>=b) result|=32;
    return result;
}
static int equal(uint48_t a,uint48_t b) { return a==b; }
static uint48_t widen_u(uint32_t a) { return a; }
static int48_t widen_s(int32_t a) { return a; }
static uint48_t nested(uint48_t a,uint48_t b) { return add(sub(a,b),b); }
static uint48_t update(uint48_t *a,uint48_t b) { return *a+=b; }
static unsigned condition(uint48_t a) { return a ? 1 : 0; }
static uint48_t recurse(unsigned n,uint48_t value) {
    if(!n) return value;
    return recurse(n-1,value+3U);
}
static uint48_t large_frame(uint48_t value) {
    volatile unsigned char padding[180];
    padding[0]=7; padding[179]=11;
    value=add(value,padding[0]);
    return value+padding[179];
}
#define CHECK(x) do { if(!(x)) { puts("FAIL native48"); putchar('0'+i/10);putchar('0'+i%10);putchar(':');putchar('0'+stage/10);putchar('0'+stage%10);return 1; } } while(0)
int main(void) {
    unsigned i=0,stage=0;
    uint48_t a,b,out;
    char text[17];
    CHECK(sizeof(uint48_t)==6 && sizeof(int48_t)==6);
    for(i=0;i<sizeof(vectors)/sizeof(vectors[0]);i++) {
        const struct vector *p=&vectors[i];
        a=p->a;b=p->b;
        stage=1; CHECK(add(a,b)==p->add && sub(a,b)==p->sub && mul(a,b)==p->mul);
        stage=2; CHECK(divu(a,b)==p->q && remu(a,b)==p->r);
        stage=3; CHECK(-a==p->neg && ~a==p->inv && (a&b)==p->and_ && (a|b)==p->or_ && (a^b)==p->xor_);
        stage=4; CHECK(left(a,p->count)==p->left && right(a,p->count)==p->right);
        stage=5; CHECK(twice(a)==p->double_ && half(a)==p->half);
        stage=6; CHECK(compare_u(a,b)==p->ucmp && compare_s((int48_t)a,(int48_t)b)==p->scmp);
        if(p->valid_signed) {stage=7;CHECK((uint48_t)divs((int48_t)a,(int48_t)b)==p->sq && (uint48_t)rems((int48_t)a,(int48_t)b)==p->sr);}
        stage=8; CHECK((uint48_t)sright((int48_t)a,p->count)==p->sright && (uint48_t)shalf((int48_t)a)==p->shalf);
        stage=9; CHECK(equal(a,a) && nested(a,b)==a && condition(a)==(p->a!=0));
        stage=10; observed=a; CHECK(observed==a); out=a; CHECK(update(&out,b)==p->add && out==p->add);
        stage=11; u48_add(&out,&a,&b); CHECK(out==p->add);
        stage=12; CHECK((uint16_t)a==p->low16 && (uint32_t)a==p->low32);
    }
    i=0; stage=13;
    CHECK(widen_u(0xffffffffUL)==(uint48_t)0xffffffffUL && widen_s(-3L)==(int48_t)-3);
    stage=14; CHECK(call_target(123U,456U)==579U && recurse(10,100U)==130U && large_frame(100U)==118U);
    a=((uint48_t)0xffffffffUL<<16)|65535U;
    u48_format_dec(text,&a);
    CHECK(text[0]=='2' && text[14]=='5' && text[15]==0);
    puts("PASS native48 operators, calls and library ABI");return 0;
}
'''
(work / 'program.c').write_text(source)
manifest = []
for name,flags in [('speed',['-O1']),('stack',['-O0']),
                   ('library',['-O1','--sdcc-option=--cpu6-no-inline48']),
                   ('no-peep',['-O1','--no-peep']),('size',['-O1','--sdcc-option=--opt-code-size'])]:
    binary=work/(name+'.bin')
    with (work/(name+'-build.log')).open('w') as log:
        subprocess.run([root/'run/cpu6-bare-sdcc','--native-int48','--origin','0200',*flags,
                        work/'program.c','--save-temps',work/name,'-o',binary],
                       stdout=log,stderr=subprocess.STDOUT,check=True)
    output=subprocess.check_output([root/'tests/centurion-cli','-A','0200','-T','100000000','-b',binary],text=True,timeout=120)
    (work/(name+'.log')).write_text(output)
    assert 'PASS native48' in output and 'FAIL' not in output and 'System halted' in output, name
    print('PASS instruction core:',name,flush=True)
    manifest.append(binary.name)
(work/'manifest.json').write_text(json.dumps(manifest))
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node',root/'tests/compiler-audit-core.js',work,work/'manifest.json'],
                   env=dict(os.environ,CPU6_BARE_ORIGIN='0200',CPU6_MICROSTEP_LIMIT='300000000'),
                   stdout=log,stderr=subprocess.STDOUT,check=True,timeout=300)
print('PASS native48 operators on both CPU cores')
(root/'build/out/tests/latest-sdcc-native48-tests.txt').write_text(str(work)+'\n')
