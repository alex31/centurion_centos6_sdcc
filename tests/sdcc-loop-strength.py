#!/usr/bin/env python3
"""Word recurrences: wraparound, zero trips, exits, narrow IVs and table scans."""
from pathlib import Path
import tempfile
from cpu6_roadmap_support import ROOT, execute

work = Path(tempfile.mkdtemp(prefix='sdcc-loop-strength-', dir=ROOT / 'build/out/tests'))
print('Results:', work, flush=True)
source = r'''#include <stdint.h>
#include <stdio.h>
uint16_t scaled(uint16_t start, uint16_t end) {
 uint16_t i, s=0; for(i=start;i<end;i++) s += (i*37U) ^ (i+19U); return s;
}
uint16_t wrapping(uint16_t start, uint16_t stop) {
 uint16_t i, s=0; for(i=start;i!=stop;i+=7U) s ^= i*103U; return s;
}
uint16_t shifted(uint16_t start,uint16_t end) {
 uint16_t i,s=0; for(i=start;i<end;i++) s += (i<<5) ^ (i<<3); return s;
}
uint16_t narrow(uint8_t start,uint8_t stop) {
 uint8_t i; uint16_t s=0; for(i=start;i!=stop;i++) s ^= i*37U; return s;
}
uint16_t exits(uint16_t n) {
 uint16_t i,s=0; for(i=0;i<n;i++) {
  if(i==73)break; if(i&3)continue; s ^= i*103U;
 } return s+i;
}
uint16_t post(uint16_t n) {
 uint16_t i=0,s=0; while(i<n) s += (++i)*37U; return s;
}
uint16_t descending(uint16_t start) {
 uint16_t i,s=0; for(i=start;i>3;i-=3) s ^= i*57U; return s;
}
uint16_t late_use(uint16_t n) {
 uint16_t i=0,s=0,t=7; while(i<n) {t=i*37U; ++i; s^=t;} return s+t;
}
struct record {uint16_t value; uint8_t padding[5];};
static struct record table[256];
uint16_t scan(uint16_t n) {
 uint16_t i,s=0; for(i=0;i<n;i++)s+=table[i].value; return s;
}
int main(void) {
 uint16_t i,k,total=0;
'''
checks=[]
for start,end in [(0,0),(3,3),(9,2),(0,100),(19,171),(32700,32900),(65200,65535)]:
    checks += [(f'scaled({start}U,{end}U)', sum(((i*37)&65535)^((i+19)&65535) for i in range(start,end))&65535),
               (f'shifted({start}U,{end}U)',sum(((i<<5)&65535)^((i<<3)&65535) for i in range(start,end))&65535)]
for start,count in [(0,0),(0,64),(65400,128),(32700,100)]:
    expected=0
    for k in range(count):expected^=(((start+7*k)&65535)*103)&65535
    checks.append((f'wrapping({start}U,{(start+7*count)&65535}U)',expected))
for start,count in [(0,0),(1,7),(240,40),(128,200)]:
    expected=0
    for k in range(count):expected^=((start+k)&255)*37
    checks.append((f'narrow({start}U,{(start+count)&255}U)',expected))
for n in [0,1,17,73,200]:
    expected=0
    for i in range(min(n,73)):
        if i&3==0:expected^=i*103
    checks.append((f'exits({n}U)',(expected+min(n,73))&65535))
    checks.append((f'post({n}U)',sum(i*37 for i in range(1,n+1))&65535))
for start in [0,3,4,99,1000]:
    expected=0
    for i in range(start,3,-3):expected^=(i*57)&65535
    checks.append((f'descending({start}U)',expected))
for n in [0,1,31,100]:
    total=0; t=7
    for i in range(n):t=i*37; total^=t
    checks.append((f'late_use({n}U)',(total+t)&65535))
for index,(expression,expected) in enumerate(checks):
    source+=f' if({expression}!={expected}U){{puts("FAIL induction {index}");return 1;}}\n'
expected=sum((i*61+7)&65535 for i in range(256))*64&65535
source+=''' for(i=0;i<256;i++)table[i].value=i*61U+7U;
 for(k=0;k<64;k++)total+=scan(256);
'''
source+=f' if(total!={expected}U){{puts("FAIL record scan");return 1;}}\n'
source+=' puts("PASS induction and strided record scans");return 0;}\n'
(work/'program.c').write_text(source)
rows=execute(work,[work/'program.c'],[
    ('on',['-O1']),('off',['-O1','--sdcc-option=--cpu6-no-loop-strength']),
    ('no-peep',['-O1','--no-peep']),('stack',['-O0']),
    ('size',['-O1','--sdcc-option=--opt-code-size'])],
    common=['--sdcc-option=--cpu6-no-auto-inline'])
print('Measured total gain: %.2f%%' % (100*(1-rows[0]['ticks']/rows[1]['ticks'])),flush=True)
(ROOT/'build/out/tests/latest-sdcc-loop-strength.txt').write_text(str(work)+'\n')
