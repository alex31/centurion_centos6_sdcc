#include "core.h"

#ifndef CPU6_FLOAT_ABI_THUNKS

unsigned char f32_eq(uint32_t a,uint32_t b)
{
    if (F32_ABS(a)>F32_INF || F32_ABS(b)>F32_INF) return 0;
    return a==b || !(F32_ABS(a)|F32_ABS(b));
}
unsigned char f32_lt(uint32_t a,uint32_t b)
{
    if (F32_ABS(a)>F32_INF || F32_ABS(b)>F32_INF || !(F32_ABS(a)|F32_ABS(b))) return 0;
    if ((a^b)&F32_SIGN) return (unsigned char)(a>>31);
    return a&F32_SIGN ? a>b : a<b;
}
unsigned char f32_le(uint32_t a,uint32_t b)
{
    uint32_t aa=F32_ABS(a),bb=F32_ABS(b);
    if (aa>F32_INF || bb>F32_INF) return 0;
    if (!(aa|bb)) return 1;
    if ((a^b)&F32_SIGN) return (unsigned char)(a>>31);
    return a&F32_SIGN ? a>=b : a<=b;
}
#endif
