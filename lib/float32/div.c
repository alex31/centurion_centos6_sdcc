#include "core.h"

uint32_t f32_div(uint32_t a,uint32_t b)
{
    uint32_t aa=F32_ABS(a),bb=F32_ABS(b),ma,mb,quotient=0;
    unsigned sign=(unsigned)((a^b)>>31),i;
    int exponent;
    if (aa>F32_INF || bb>F32_INF || (aa==F32_INF && bb==F32_INF) || (!aa&&!bb)) return F32_NAN;
    if (aa==F32_INF || !bb) return ((uint32_t)sign<<31)|F32_INF;
    if (!aa || bb==F32_INF) return (uint32_t)sign<<31;
    exponent=f32_unpack(a,&ma)-f32_unpack(b,&mb)+127;
    if (ma<mb) {ma<<=1;exponent--;}
    /* Restoring division produces the retained significand plus guard,
       round and sticky. No 64-bit integer operations are required. */
    for (i=0;i<27;i++) {
        quotient<<=1;
        if (ma>=mb) {ma-=mb;quotient|=1;}
        ma<<=1;
    }
    if (ma) quotient|=1;
    return f32_pack(sign,exponent,quotient);
}
