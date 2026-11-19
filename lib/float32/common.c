#include "core.h"

uint32_t f32_jam(uint32_t value,unsigned distance)
{
    if (!distance) return value;
    if (distance >= 32) return value != 0;
    return (value >> distance) | ((value & ((1UL << distance)-1UL)) != 0);
}

/* Significand has three rounding bits, with sticky in bit zero, and its
   normal hidden bit at bit 26. Exponent is the encoded exponent before
   gradual underflow. Only this function performs the final rounding. */
uint32_t f32_pack(unsigned sign,int exponent,uint32_t significand)
{
    uint32_t mantissa;
    unsigned rounding;
    if (exponent < 1) {
        significand=f32_jam(significand,(unsigned)(1-exponent));
        exponent=1;
    }
    rounding=(unsigned)(significand & 7U);
    mantissa=significand >> 3;
    if (rounding>4 || (rounding==4 && (mantissa&1U))) mantissa++;
    if (mantissa>=0x01000000UL) {mantissa>>=1;exponent++;}
    if (exponent>=255) return ((uint32_t)sign<<31)|F32_INF;
    if (mantissa<F32_HIDDEN) exponent=0;
    return ((uint32_t)sign<<31)|((uint32_t)exponent<<23)|(mantissa&F32_FRAC);
}

/* Call only for finite, nonzero magnitudes. Normalize subnormals without
   narrowing their exponent or flushing their significant bits. */
int f32_unpack(uint32_t bits,uint32_t *significand)
{
    int exponent=(int)((bits>>23)&255U);
    uint32_t mantissa=bits&F32_FRAC;
    if (exponent) mantissa|=F32_HIDDEN;
    else {
        exponent=1;
        while (mantissa<F32_HIDDEN) {mantissa<<=1;exponent--;}
    }
    *significand=mantissa;
    return exponent;
}
