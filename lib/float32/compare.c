#include "core.h"

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
    return f32_eq(a,b)||f32_lt(a,b);
}
