#!/usr/bin/env python3
"""Direct object updates: word carries/borrows, signed shifts and volatile guard."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
work=Path(tempfile.mkdtemp(prefix='sdcc-inplace-',dir=root/'build/out/tests'))
print('Results:',work,flush=True)
source='#include <stdint.h>\n#include <stdio.h>\n#include <int48.h>\nstatic int fail(const char *s,unsigned i){puts(s);putchar(48+i/10);putchar(48+i%10);return 0;}\n'
checks=[]
for bits in (32,48):
    mask=(1<<bits)-1;sign=1<<(bits-1);typ=f'uint{bits}_t';styp=f'int{bits}_t'
    values=[0,1,2,65534,65535,65536,65537,0xffff0000,0xffffffff,0x100000000,sign-1,sign,mask-1,mask]
    values=sorted(set(n&mask for n in values))
    def lit(n):
        n &=mask
        return '0x%xUL'%n if bits==32 else '(((uint48_t)0x%xUL<<16)|0x%xU)'%(n>>16,n&65535)
    source+=f'static {typ} g{bits};static {styp} s{bits};\n'
    for amount in (1,2,65535):
        source+=f'void plus{bits}_{amount}(void){{g{bits}+={amount}U;}}\n'
        source+=f'void minus{bits}_{amount}(void){{g{bits}-={amount}U;}}\n'
    source+=f'void shl{bits}(void){{g{bits}<<=1;}}\nvoid shr{bits}(void){{g{bits}>>=1;}}\nvoid sar{bits}(void){{s{bits}>>=1;}}\n'
    source+=f'struct row{bits}{{{typ} value,plus[3],minus[3],left,right;{styp} signed_right;}};\nstatic const struct row{bits} rows{bits}[]={{\n'
    for n in values:
        signed=n if n<sign else n-(mask+1)
        source+='{'+lit(n)+',{'+','.join(lit(n+c) for c in (1,2,65535))+'},{'+','.join(lit(n-c) for c in (1,2,65535))+'},'+','.join(map(lit,[n<<1,n>>1,signed>>1]))+'},\n'
    source+='};\n'
    source+=f'int check{bits}(void){{unsigned i;for(i=0;i<sizeof(rows{bits})/sizeof(rows{bits}[0]);i++){{const struct row{bits}*p=rows{bits}+i;\n'
    for index,amount in enumerate((1,2,65535)):
        source+=f'g{bits}=p->value;plus{bits}_{amount}();if(g{bits}!=p->plus[{index}])return fail("plus{bits}_{amount}",i);\n'
        source+=f'g{bits}=p->value;minus{bits}_{amount}();if(g{bits}!=p->minus[{index}])return fail("minus{bits}_{amount}",i);\n'
    source+=f'g{bits}=p->value;shl{bits}();if(g{bits}!=p->left)return fail("shl{bits}",i);g{bits}=p->value;shr{bits}();if(g{bits}!=p->right)return fail("shr{bits}",i);s{bits}=p->value;sar{bits}();if(s{bits}!=p->signed_right)return fail("sar{bits}",i);\n}}return 1;}}\n'
    checks.append(f'check{bits}()')
source+='''static volatile uint32_t vol;
void volatile_update(void){vol+=1;}
int main(void){if(!('''+'&&'.join(checks)+''')){puts("FAIL inplace wide");return 1;}
vol=0xffffffffUL;volatile_update();if(vol) return 1;
puts("PASS inplace wide carry, borrow, shifts and volatile");return 0;}
'''
(work/'program.c').write_text(source)
manifest=[]
for name,flags in [('speed',['-O1']),('disabled',['-O1','--sdcc-option=--cpu6-no-inplace-wide']),('stack',['-O0']),('no-peep',['-O1','--no-peep']),('size',['-O1','--sdcc-option=--opt-code-size'])]:
    binary=work/(name+'.bin')
    with (work/(name+'-build.log')).open('w') as log:
        subprocess.run([root/'run/cpu6-bare-sdcc',*flags,'--native-int48','--sdcc-option=--cpu6-no-auto-inline',work/'program.c','--save-temps',work/name,'-o',binary],stdout=log,stderr=subprocess.STDOUT,check=True)
    asm=(work/name/'unit2.s').read_text()
    def body(n):return re.search(r'^_'+n+r':\n(.*?)(?=^_\w+:|\Z)',asm,re.M|re.S).group(1)
    for bits in (32,48):
        for fn in ('plus'+str(bits)+'_1','minus'+str(bits)+'_65535','shl'+str(bits),'sar'+str(bits)):
            assert ('direct nonvolatile multiword update' in body(fn))==(name not in ('disabled','size')),(name,fn)
    assert 'direct nonvolatile multiword update' not in body('volatile_update')
    output=subprocess.check_output([root/'tests/centurion-cli','-A','2000','-T','100000000','-b',binary],text=True,timeout=120)
    assert 'PASS inplace wide' in output and 'FAIL' not in output,(name,output)
    manifest.append(binary.name);print('PASS instruction core:',name,flush=True)
(work/'manifest.json').write_text(json.dumps(manifest))
with (work/'microcode.log').open('w') as log:
    subprocess.run(['node',root/'tests/compiler-audit-core.js',work,work/'manifest.json'],env=dict(os.environ,CPU6_MICROSTEP_LIMIT='300000000'),stdout=log,stderr=subprocess.STDOUT,check=True,timeout=240)
print('PASS direct object updates on both CPU cores')
(root/'build/out/tests/latest-sdcc-inplace.txt').write_text(str(work)+'\n')
