#include "abi.h"
static const float coefficients[]={
 0.11111111111111111111f,0.14285714285714285714f,0.2f,0.33333333333333333333f,1.0f};
float __c6_log(float value)
{
    uint32_t bits=__c6_fbits(value),magnitude=F32_ABS(bits),mantissa;
    int exponent;
    float m,z,r,n;
    if (magnitude>F32_INF) return __c6_fvalue(F32_NAN);
    if (!magnitude) return __c6_fvalue(F32_SIGN|F32_INF);
    if (bits&F32_SIGN) return __c6_fvalue(F32_NAN);
    if (magnitude==F32_INF) return value;
    exponent=f32_unpack(bits,&mantissa)-127;
    bits=0x3f800000UL|(mantissa&F32_FRAC);
    if (bits>0x3fb504f3UL) {bits-=0x00800000UL;exponent++;}
    m=__c6_fvalue(bits);z=(m-1.0f)/(m+1.0f);
    r=(2.0f*z)*__c6_poly(z*z,coefficients,5);
    n=(float)exponent;
    return (n*0.693145751953125f+r)+n*1.428606765330187045e-6f;
}
