#!/usr/bin/env python3
"""Right-operand forwarding, with independent word/byte/wide arithmetic oracles."""
import random
import tempfile
from pathlib import Path
from cpu6_roadmap_support import ROOT,execute

work=Path(tempfile.mkdtemp(prefix='sdcc-rhs-',dir=ROOT/'build/out/tests'))
print('Results:',work,flush=True)
rng=random.Random(0xc006b)
cases=[(a,b,n) for a,b,n in [(0,0,0),(65535,65535,0),(32768,1,15),(1,32768,1),(255,65535,8)]]
cases += [(rng.randrange(65536),rng.randrange(65536),rng.randrange(16)) for _ in range(43)]
source='''#include <stdint.h>
#include <stdio.h>
#include <cpu6abi.h>
typedef uint16_t U;
U add(U a,U b,U n){return a+((b>>n)^0xa55aU);}
U sub(U a,U b,U n){return a-((b>>n)^0xa55aU);}
U mul(U a,U b,U n){return a*((b>>n)^0xa55aU);}
U divv(U a,U b,U n){return a/(((b>>n)^0xa55aU)|1U);}
U rem(U a,U b,U n){return a%(((b>>n)^0xa55aU)|1U);}
U band(U a,U b,U n){return a&((b>>n)^0xa55aU);}
U bxor(U a,U b,U n){return a^((b>>n)^0xa55aU);}
U cmp(U a,U b,U n){return a<((b>>n)^0xa55aU);}
U byte_left(uint8_t a,U b){return a-(b^0xa55aU);}
uint32_t wide_left(uint32_t a,U b){return a+(b^0xa55aU);}
U shift_rhs(U a,U b){return a<<(b&15U);}
U reused(U a,U b){U v=b^0xa55aU;return (a+v)^v;}
U reg(U a,U b) CPU6_REGCALL {return a^b;}
U reg_result(U a,U b){return a-(reg(b,0xa55aU));}
struct row{U a,b,n,add,sub,mul,divv,rem,band,bxor,cmp,byte_left,shift_rhs,reused;uint32_t wide_left;};
static const struct row cases[]={
'''
for a,b,n in cases:
    z=(b>>n)^0xa55a;v=b^0xa55a
    values=[a,b,n,(a+z)&65535,(a-z)&65535,a*z&65535,a//(z|1),a%(z|1),a&z,a^z,int(a<z),
            ((a&255)-v)&65535,a<<(b&15)&65535,((a+v)&65535)^v]
    source+='{'+','.join(str(value)+'U' for value in values)+','+str(0x12340000+a+v)+'UL},\n'
source+='''};
int main(void){unsigned i;for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
'''
for name in ['add','sub','mul','divv','rem','band','bxor','cmp']:
    source+=f'if({name}(cases[i].a,cases[i].b,cases[i].n)!=cases[i].{name}){{puts("FAIL RHS {name}");return 1;}}\n'
for name in ['byte_left','shift_rhs','reused']:
    source+=f'if({name}(cases[i].a,cases[i].b)!=cases[i].{name}){{puts("FAIL RHS {name}");return 1;}}\n'
source+='''if(wide_left(0x12340000UL+cases[i].a,cases[i].b)!=cases[i].wide_left){puts("FAIL RHS wide");return 1;}
if(reg_result(cases[i].a,cases[i].b)!=(U)(cases[i].a-(cases[i].b^0xa55aU))){puts("FAIL RHS regcall");return 1;}
}puts("PASS RHS arithmetic, comparisons and fallbacks");return 0;}
'''
(work/'program.c').write_text(source)
execute(work,[work/'program.c'],[
 ('on',['-O1']),('off',['-O1','--sdcc-option=--cpu6-no-rhs-forwarding']),
 ('no-peep',['-O1','--no-peep']),('stack',['-O0']),
 ('size',['-O1','--sdcc-option=--opt-code-size'])],common=['--sdcc-option=--cpu6-no-auto-inline'])
(ROOT/'build/out/tests/latest-sdcc-rhs.txt').write_text(str(work)+'\n')
