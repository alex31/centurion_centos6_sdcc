#include "core.h"

uint32_t f32_sqrt(uint32_t a)
{
    uint32_t magnitude=F32_ABS(a),mantissa,radicand,remainder=0,root=0,trial;
    int exponent;
    unsigned i;
    if (magnitude>F32_INF) return F32_NAN;
    if (!magnitude) return a;
    if (a&F32_SIGN) return F32_NAN;
    if (magnitude==F32_INF) return a;
    exponent=f32_unpack(a,&mantissa)-127;
    if (exponent&1) {mantissa<<=1;exponent--;}
    /* The radicand is mantissa followed by 29 zero bits. Consume its
       54 bits from the high end, refilling the stream with zeros. */
    radicand=mantissa<<7;
    for (i=27;i;i--) {
        remainder=(remainder<<2)|(radicand>>30);
        radicand<<=2;
        root<<=1;trial=(root<<1)|1U;
        if (remainder>=trial) {remainder-=trial;root++;}
    }
    if (remainder) root|=1;
    return f32_pack(0,exponent/2+127,root);
}
