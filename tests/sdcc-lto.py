#!/usr/bin/env python3
"""Cross-unit inlining with ABI, symbol scope, retained addresses and fallbacks."""
import json
from pathlib import Path
import tempfile
from cpu6_roadmap_support import ROOT, execute

work=Path(tempfile.mkdtemp(prefix='sdcc-lto-',dir=ROOT/'build/out/tests'))
print('Results:',work,flush=True)
(work/'api.h').write_text('''#include <stdint.h>
#include <cpu6abi.h>
uint16_t mix(uint16_t a,uint16_t b);
uint16_t twice(uint16_t a);
uint16_t branchy(uint16_t a);
uint16_t next_private(uint16_t a);
uint16_t other_private(uint16_t a);
uint16_t recursion(uint16_t n);
uint16_t variadic(uint16_t first,...);
uint16_t regcall(uint16_t a,uint16_t b) CPU6_REGCALL;
uint32_t wide_xor(uint32_t a);
uint16_t touch(uint16_t *p,uint16_t v);
uint16_t local_work(uint16_t v);
int16_t char_mix(int8_t a,uint8_t b);
uint16_t saved_word(void);
uint16_t constant_one(void);
uint16_t read_shared(void);
''')
# The word at 4(S) belongs to the register range while the push is active.
(work/'registers.s').write_text('''    .setcpu 6
    .code
    .export _saved_word
; CPU6 function _saved_word begin
; CPU6 ABI stack frame=0 saved=2 variadic=0 asm=0
_saved_word:
    stx (-s)
    push ah,xl
    lda 0x1234
    sta 4(s)
    pop ah,xl
    xfr x,b
    ldx (s+)
    rsr
; CPU6 function _saved_word end
''')
(work/'callee.c').write_text('''#include "api.h"
static uint16_t state=100;
volatile uint16_t shared=42;
uint16_t read_shared(void){return shared;}
uint16_t constant_one(void){return 1U;}
uint16_t mix(uint16_t a,uint16_t b){return ((a<<3)^b)+37U;}
uint16_t twice(uint16_t a){uint16_t t=a*3U;return t^(t<<4);}
uint16_t branchy(uint16_t a){if(a&1U)return a+13U;return a^0x55aaU;}
uint16_t next_private(uint16_t a){state+=a;return state;}
uint16_t recursion(uint16_t n){if(!n)return 7;return recursion(n-1)+3U;}
uint16_t variadic(uint16_t first,...){return first+9U;}
uint16_t regcall(uint16_t a,uint16_t b) CPU6_REGCALL {return a^b;}
uint32_t wide_xor(uint32_t a){return a^0x12345678UL;}
uint16_t touch(uint16_t *p,uint16_t v){*p+=v;return *p;}
uint16_t local_work(uint16_t v){uint16_t local=v;touch(&local,7);return local^v;}
int16_t char_mix(int8_t a,uint8_t b){return a+b;}
''')
(work/'other.c').write_text('''#include "api.h"
static uint16_t state=500;
uint16_t other_private(uint16_t a){state+=a;return state;}
''')
value=0x1234
for i in range(4096):value=((((value<<3)&65535)^i)+37)&65535
checks=[]
for a in [0,1,2,127,255,32767,32768,65535]:
    t=a*3&65535
    checks += [(f'twice({a}U)',(t^(t<<4))&65535),
               (f'branchy({a}U)',((a+13) if a&1 else (a^0x55aa))&65535)]
caller='''#include <stdio.h>
#include "api.h"
static uint16_t state=900;
static volatile uint16_t shared=17;
uint16_t check_scope(void){return shared+read_shared();}
static uint16_t (*volatile indirect)(uint16_t,uint16_t)=mix;
uint16_t check_reg(uint16_t a,uint16_t b){return regcall(a,b)+1U;}
uint32_t check_wide(uint32_t a){return wide_xor(a)+1UL;}
uint16_t check_twice(uint16_t a){return twice(a)+1U;}
uint16_t check_recursion(uint16_t n){return recursion(n)+1U;}
uint16_t check_keep(uint16_t a,uint16_t b){uint16_t keep=a+103U;return twice(b)^keep;}
uint16_t check_local(uint16_t a){return local_work(a)+1U;}
uint16_t check_saved(void){return saved_word()+1U;}
int16_t check_char(int8_t a,uint8_t b){return char_mix(a,b)-1;}
uint16_t branch_block(uint16_t a){uint16_t t=0;if(a&1){t=mix(a,1);t=mix(t,2);t=mix(t,3);t=mix(t,4);t=mix(t,5);}return t;}
int main(void){uint16_t i,v=0x1234U,local=32760U;
for(i=0;i<4096;i++)v=mix(v,i);
'''
checks += [('v',value),('indirect(0x1234U,7U)',(((0x1234<<3)^7)+37)&65535),
           ('next_private(7)',107),('other_private(9)',509),('next_private(13)',120),
           ('other_private(11)',520),('state',900),('recursion(8)',31),
           ('variadic(11,19,23)',20),('regcall(0xabcdU,0x5555U)',0xfe98),
           ('touch(&local,19)',32779),('local',32779)]
checks += [('check_reg(0xabcdU,0x5555U)',0xfe99),('check_twice(171)',(513^(513<<4))+1),
           ('check_recursion(7)',29)]
for a,b in [(0,0),(32700,123),(65535,21845)]:
    t=b*3&65535
    checks.append((f'check_keep({a}U,{b}U)',((t^(t<<4))^((a+103)&65535))&65535))
for a in [0,1,32767,65535]:
    checks.append((f'check_local({a}U)',((a^((a+7)&65535))+1)&65535))
    value=0
    if a&1:
        value=a
        for b in range(1,6):value=(((value<<3)^b)+37)&65535
    checks.append((f'branch_block({a}U)',value))
checks.extend([('check_char(-128,255)',126), ('check_saved()',0x1235)])
checks.extend([('constant_one()',1), ('check_scope()',59)])
for index,(expression,expected) in enumerate(checks):
    caller+=f'if({expression}!={expected}U){{puts("FAIL LTO {index}");return 1;}}\n'
caller+='''if(wide_xor(0xabcdef19UL)!=0xb9f9b961UL){puts("FAIL LTO wide");return 1;}
if(check_wide(0xabcdef19UL)!=0xb9f9b962UL){puts("FAIL LTO wide frame");return 1;}
puts("PASS LTO cross-unit calls and ABI");return 0;}
'''
(work/'caller.c').write_text(caller)
rows=execute(work,[work/'caller.c',work/'callee.c',work/'other.c',work/'registers.s'],[
    ('normal',['-O1']),('lto',['-O1','--lto']),
    ('lto-no-ab',['-O1','--lto','--sdcc-option=--cpu6-no-ab-allocation']),
    ('lto-stack',['-O0','--lto']),('size',['-Os']),('lto-size',['-Os','--lto'])],
    common=['--sdcc-option=--cpu6-no-auto-inline'])
report=json.loads((work/'lto/lto-report.json').read_text())
assert any(row['callee']=='_mix' for row in report['inlined']),report
assert any(row['callee']=='_saved_word' and row['mode']=='frame-preserving'
           for row in report['inlined']),report
assert not any(row['callee'] in ('_next_private','_other_private')
               for row in report['inlined']),report
assert any(row['mode']=='frame-preserving' for row in report['inlined']),report
assert {'_mix','_twice','_regcall','_wide_xor','_recursion','_local_work','_char_mix'} <= {
    row['callee'] for row in report['inlined']},report
size_inline = json.loads((work/'lto-size/lto-report.json').read_text())['inlined']
assert any(row['callee'] == '_constant_one' for row in size_inline),size_inline
assert all(row['callee'] != '_mix' for row in size_inline),size_inline
assert (work/'lto-size.bin').stat().st_size <= (work/'size.bin').stat().st_size
print('Measured LTO gain: %.2f%%' % (100*(1-rows[1]['ticks']/rows[0]['ticks'])),flush=True)
(ROOT/'build/out/tests/latest-sdcc-lto.txt').write_text(str(work)+'\n')
