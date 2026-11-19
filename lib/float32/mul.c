#include "core.h"

uint32_t f32_mul(uint32_t a,uint32_t b)
{
    uint32_t aa=F32_ABS(a),bb=F32_ABS(b),ma,mb,low,high,cross,part,significand;
    unsigned sign=(unsigned)((a^b)>>31);
    int exponent;
    if (aa>F32_INF || bb>F32_INF) return F32_NAN;
    if (aa==F32_INF || bb==F32_INF)
        return !aa || !bb ? F32_NAN : ((uint32_t)sign<<31)|F32_INF;
    if (!aa || !bb) return (uint32_t)sign<<31;
    if (aa==0x3f800000UL) return b^(a&F32_SIGN);
    if (bb==0x3f800000UL) return a^(b&F32_SIGN);
    exponent=f32_unpack(a,&ma)+f32_unpack(b,&mb)-127;
    /* Exact 24x24 -> 48-bit product, represented by two 32-bit words. */
    low=(ma&65535UL)*(mb&65535UL);
    cross=(ma>>16)*(mb&65535UL)+(mb>>16)*(ma&65535UL);
    high=(ma>>16)*(mb>>16)+(cross>>16);
    part=cross<<16;
    low+=part;if (low<part) high++;
    /* Both normalization distances are fixed. Using constant shifts
       avoids three variable-width operations on each product. */
    if (high&0x8000UL) {
        exponent++;
        significand=(high<<11)|(low>>21)|((low&0x1fffffUL)!=0);
    } else {
        significand=(high<<12)|(low>>20)|((low&0xfffffUL)!=0);
    }
    return f32_pack(sign,exponent,significand);
}
