#ifndef CPU6_REGCALL_TEST_H
#define CPU6_REGCALL_TEST_H
#include <stdint.h>
#include <cpu6abi.h>
uint16_t sum(uint16_t a,uint16_t b) CPU6_REGCALL;
uint16_t first(uint16_t a,uint16_t b) CPU6_REGCALL;
uint16_t second(uint16_t a,uint16_t b) CPU6_REGCALL;
uint32_t mixed(int8_t a,uint16_t b,uint32_t c,uint8_t d) CPU6_REGCALL;
uint32_t wide_first(uint32_t a,uint16_t b) CPU6_REGCALL;
uint16_t recursive(uint16_t n,uint16_t total) CPU6_REGCALL;
uint16_t large_receive(uint16_t a,uint16_t b) CPU6_REGCALL;
typedef uint16_t (*reg_function)(uint16_t,uint16_t) CPU6_REGCALL;
uint16_t indirect(reg_function f,uint16_t a,uint16_t b);
uint16_t stack_function(uint16_t a,uint16_t b) CPU6_STACKCALL;
uint16_t stack_forward(uint16_t a,uint16_t b) CPU6_STACKCALL;
uint16_t stack_bridge(uint16_t a,uint16_t b) CPU6_STACKCALL;
uint16_t variadic(uint16_t n,...) CPU6_REGCALL;
#endif
