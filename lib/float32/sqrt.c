#include "core.h"

uint32_t f32_sqrt(uint32_t a)
{
    uint32_t magnitude=F32_ABS(a),mantissa,high,low,remainder=0,root=0,trial,pair;
    int exponent;
    unsigned i,shift;
    if (magnitude>F32_INF) return F32_NAN;
    if (!magnitude) return a;
    if (a&F32_SIGN) return F32_NAN;
    if (magnitude==F32_INF) return a;
    exponent=f32_unpack(a,&mantissa)-127;
    if (exponent&1) {mantissa<<=1;exponent--;}
    high=mantissa>>3;low=mantissa<<29;
    /* Restoring square root of a 54-bit radicand. The remainder and root
       fit in 32 bits throughout; high/low supply two input bits per step. */
    for (i=27;i;i--) {
        shift=2*(i-1);
        pair=shift>=32 ? (high>>(shift-32))&3U : (low>>shift)&3U;
        remainder=(remainder<<2)|pair;
        root<<=1;trial=(root<<1)|1U;
        if (remainder>=trial) {remainder-=trial;root++;}
    }
    if (remainder) root|=1;
    return f32_pack(0,exponent/2+127,root);
}
