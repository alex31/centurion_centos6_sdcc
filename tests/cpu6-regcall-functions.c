#include "cpu6-regcall.h"
#include <stdarg.h>
uint16_t sum(uint16_t a,uint16_t b) CPU6_REGCALL {return a*3U+b*7U;}
uint16_t first(uint16_t a,uint16_t b) CPU6_REGCALL {(void)b;return a;}
uint16_t second(uint16_t a,uint16_t b) CPU6_REGCALL {(void)a;return b;}
uint32_t mixed(int8_t a,uint16_t b,uint32_t c,uint8_t d) CPU6_REGCALL {return c+b+a+d;}
uint32_t wide_first(uint32_t a,uint16_t b) CPU6_REGCALL {return a+b;}
uint16_t recursive(uint16_t n,uint16_t total) CPU6_REGCALL {
    if(!n)return total;
    return recursive(n-1,total+n);
}
uint16_t large_receive(uint16_t a,uint16_t b) CPU6_REGCALL {
    volatile unsigned char bytes[180];
    bytes[0]=a;bytes[179]=b;
    return sum(a,b)+bytes[0]+bytes[179];
}
uint16_t indirect(reg_function f,uint16_t a,uint16_t b) {return f(a,b);}
uint16_t stack_bridge(uint16_t a,uint16_t b) CPU6_STACKCALL {return sum(a,b)+21;}
uint16_t variadic(uint16_t n,...) CPU6_REGCALL {
    va_list ap;uint16_t total=0;
    va_start(ap,n);while(n--)total+=va_arg(ap,unsigned int);va_end(ap);return total;
}
