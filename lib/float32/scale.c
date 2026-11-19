#include "core.h"
uint32_t f32_scale(uint32_t a,int power)
{
    uint32_t magnitude=F32_ABS(a),mantissa;
    int exponent;
    if (magnitude>F32_INF) return F32_NAN;
    if (!magnitude || magnitude==F32_INF) return a;
    if (power>512) return (a&F32_SIGN)|F32_INF;
    if (power<-512) return a&F32_SIGN;
    exponent=f32_unpack(a,&mantissa);
    return f32_pack((unsigned)(a>>31),exponent+power,mantissa<<3);
}
