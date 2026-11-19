#include <stdint.h>
#include <stdio.h>
#include <int48.h>

uint16_t read_stack(void);
static uint16_t minimum = 65535U;
#ifndef EXPECT_TAIL
#define DEPTH 12U
#else
#define DEPTH 5001U
#endif
static void observe(void) { uint16_t s=read_stack(); if(s<minimum)minimum=s; }
uint32_t chain_b(uint16_t n,uint32_t a,uint32_t b);
uint32_t chain_a(uint16_t n,uint32_t a,uint32_t b) {
    observe(); if(!n)return a+3*b;
    return chain_b(n-1,b,a);
}
uint32_t chain_b(uint16_t n,uint32_t a,uint32_t b) {
    observe(); if(!n)return a+3*b;
    return chain_a(n-1,b,a);
}
uint32_t accumulate(uint16_t n,uint32_t sum) {
    observe(); if(!n)return sum;
    return accumulate(n-1,sum+n);
}
uint48_t wide_state(uint16_t n,uint48_t a,uint48_t b) {
    observe(); if(!n)return a;
    return wide_state(n-1,b,a);
}
uint32_t receive(uint8_t tag,uint32_t a,uint16_t b);
uint32_t forward(uint8_t tag,uint32_t a,uint16_t b) { return receive(tag+1,a+2,b+3); }
uint32_t receive(uint8_t tag,uint32_t a,uint16_t b) {return a+b+tag;}
uint16_t one(uint16_t n);
uint16_t fewer(uint16_t a,uint16_t b,uint16_t c) {return one(a+b+c);}
uint16_t one(uint16_t n) {return n+7;}
uint16_t two(uint16_t a,uint16_t b);
uint16_t more(uint16_t n) {return two(n,n+1);}
uint16_t two(uint16_t a,uint16_t b) {return a+b;}
uint16_t read_local(const uint16_t *p);
uint16_t escape(uint16_t n) {uint16_t local=n+9;return read_local(&local);}
uint16_t read_local(const uint16_t *p) {return *p;}
uint32_t indirect(uint32_t(*f)(uint8_t,uint32_t,uint16_t),uint32_t n) {return f(5,n,7);}
uint16_t changed_return(uint16_t n) {return (uint16_t)receive(1,n,2);}
int main(void) {
    uint16_t baseline=read_stack();
    uint48_t a=((uint48_t)0x1234U<<32)|0x56789abcUL;
    uint48_t b=((uint48_t)0xabcdU<<32)|0x01234567UL;
    uint32_t expected=(uint32_t)DEPTH*(DEPTH+1U)/2U+17U;
    if(chain_a(DEPTH,0x12345678UL,0x98765432UL)!=
       ((DEPTH&1)?0x98765432UL+3*0x12345678UL:0x12345678UL+3*0x98765432UL))goto fail;
    if(accumulate(DEPTH,17)!=expected || wide_state(DEPTH,a,b)!=((DEPTH&1)?b:a))goto fail;
    if(forward(254,0x12345678UL,65530U)!=0x12355776UL)goto fail;
    if(fewer(10,20,30)!=67 || more(10)!=21 || escape(10)!=19)goto fail;
    if(indirect(receive,100)!=112 || changed_return(100)!=103)goto fail;
    if(read_stack()!=baseline)goto fail;
#ifdef EXPECT_TAIL
    if(baseline-minimum>200U)goto fail;
#endif
    puts("PASS terminal calls, value recursion, wide state and stack identity");return 0;
fail:puts("FAIL terminal calls");return 1;
}
