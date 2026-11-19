#include "core.h"

/* mode 0 = truncation, 1 = floor, 2 = ceiling. */
uint32_t f32_integral(uint32_t a,unsigned mode)
{
    uint32_t magnitude=F32_ABS(a),mask;
    int exponent=(int)(magnitude>>23)-127;
    unsigned sign=(unsigned)(a>>31);
    if (magnitude>F32_INF) return F32_NAN;
    if (!magnitude || exponent>=23) return a;
    if (exponent<0) {
        if ((mode==1 && sign) || (mode==2 && !sign)) return ((uint32_t)sign<<31)|0x3f800000UL;
        return (uint32_t)sign<<31;
    }
    mask=(1UL<<(23-exponent))-1UL;
    if (!(magnitude&mask)) return a;
    magnitude&=~mask;
    if ((mode==1 && sign) || (mode==2 && !sign)) magnitude+=mask+1UL;
    return ((uint32_t)sign<<31)|magnitude;
}
