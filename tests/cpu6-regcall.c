#include <stdio.h>
#include "cpu6-regcall.h"
static volatile uint16_t observed;
static reg_function table[2]={sum,second};
uint16_t read_stack(void);
int main(void) {
    uint16_t i,a,b,baseline=read_stack();
    for(i=0;i<80;i++) {
        a=i*1009U;b=65535U-i*151U;
        if(sum(a,b)!=(uint16_t)(a*3U+b*7U)||first(a,b)!=a||second(a,b)!=b)goto fail;
        if(indirect(table[i&1],a,b)!=((i&1)?b:(uint16_t)(a*3U+b*7U)))goto fail;
        if(large_receive(a,b)!=(uint16_t)(a*3U+b*7U+(uint8_t)a+(uint8_t)b))goto fail;
        if(mixed(-123,a,0x76543210UL,245)!=0x76543210UL+a+122U)goto fail;
        if(wide_first(0xffff0000UL,a)!=0xffff0000UL+a)goto fail;
        if(stack_function(a,b)!=(uint16_t)(a+b+11U))goto fail;
        if(stack_forward(a,b)!=(uint16_t)(a*3U+b*7U+21U))goto fail;
    }
    observed=47;
    if(sum(observed,observed)!=470)goto fail;
    a=2;b=5;
    if(sum(a++,b++)!=41||a!=3||b!=6)goto fail;
    if(sum(sum(2,3),sum(4,5))!=410)goto fail;
    if(recursive(12,7)!=85||read_stack()!=baseline)goto fail;
    if(variadic(3,5U,17U,29U)!=51)goto fail;
    puts("PASS register calls, indirect calls, mixed arguments and the stack ABI");return 0;
fail:puts("FAIL register calls");return 1;
}
