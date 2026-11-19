#include "abi.h"
#define bits __c6_fbits
#define number __c6_fvalue

#ifndef CPU6_FLOAT_ABI_THUNKS
uint32_t bits(float value)
{ union {float f;uint32_t u;} v;v.f=value;return v.u; }
float number(uint32_t value)
{ union {float f;uint32_t u;} v;v.u=value;return v.f; }

float __fsadd(float a,float b){return number(f32_add(bits(a),bits(b)));}
float __fsmul(float a,float b){return number(f32_mul(bits(a),bits(b)));}
float __fsdiv(float a,float b){return number(f32_div(bits(a),bits(b)));}
_Bool __fseq(float a,float b){return f32_eq(bits(a),bits(b));}
_Bool __fslt(float a,float b){return f32_lt(bits(a),bits(b));}
_Bool __fsle(float a,float b){return f32_le(bits(a),bits(b));}
float __ulong2fs(unsigned long a){return number(f32_fromu(a));}
float __slong2fs(long a){return number(f32_fromi(a));}
unsigned long __fs2ulong(float a){return f32_tou(bits(a));}
long __fs2slong(float a){return f32_toi(bits(a));}
#endif
float __fssub(float a,float b){return number(f32_add(bits(a),bits(b)^F32_SIGN));}
_Bool __fsneq(float a,float b){return !f32_eq(bits(a),bits(b));}
float __uchar2fs(unsigned char a){return number(f32_fromu(a));}
float __schar2fs(signed char a){return number(f32_fromi(a));}
float __uint2fs(unsigned int a){return number(f32_fromu(a));}
float __sint2fs(int a){return number(f32_fromi(a));}
unsigned char __fs2uchar(float a){return (unsigned char)f32_tou(bits(a));}
signed char __fs2schar(float a){return (signed char)f32_toi(bits(a));}
unsigned int __fs2uint(float a){return (unsigned int)f32_tou(bits(a));}
int __fs2sint(float a){return (int)f32_toi(bits(a));}
