#include "core.h"

uint32_t f32_fromu(uint32_t value)
{
    int exponent=158;
    if (!value) return 0;
    while (!(value&F32_SIGN)) {value<<=1;exponent--;}
    return f32_pack(0,exponent,f32_jam(value,5));
}
uint32_t f32_fromi(int32_t value)
{
    uint32_t magnitude=(uint32_t)value;
    if (value<0) return f32_fromu(0UL-magnitude)|F32_SIGN;
    return f32_fromu(magnitude);
}
uint32_t f32_tou(uint32_t bits)
{
    int exponent=(int)((bits>>23)&255U)-127;
    uint32_t mantissa=(bits&F32_FRAC)|F32_HIDDEN;
    if (F32_ABS(bits)>F32_INF || exponent<0 || (bits&F32_SIGN)) return 0;
    if (exponent>31) return 0xffffffffUL;
    return exponent<=23 ? mantissa>>(23-exponent) : mantissa<<(exponent-23);
}
int32_t f32_toi(uint32_t bits)
{
    int exponent=(int)((bits>>23)&255U)-127;
    uint32_t magnitude;
    if (F32_ABS(bits)>F32_INF || exponent<0) return 0;
    if (exponent>=31) return bits&F32_SIGN ? (int32_t)0x80000000UL : 0x7fffffffL;
    magnitude=f32_tou(F32_ABS(bits));
    return bits&F32_SIGN ? -(int32_t)magnitude : (int32_t)magnitude;
}
