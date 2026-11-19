#include "abi.h"
static const float coefficients[]={
 2.48015873015873015873e-5f,1.98412698412698412698e-4f,
 1.38888888888888888889e-3f,8.33333333333333333333e-3f,
 4.16666666666666666667e-2f,1.66666666666666666667e-1f,0.5f,1.0f,1.0f};
float __c6_exp(float value)
{
    uint32_t bits=__c6_fbits(value),magnitude=F32_ABS(bits);
    int exponent;
    float n,r,y;
    if (magnitude>F32_INF) return __c6_fvalue(F32_NAN);
    if (value>=89.0f) return __c6_fvalue(F32_INF);
    if (value<=-104.0f) return 0.0f;
    y=value*1.44269504088896340736f;
    exponent=(int)(y+(value<0.0f?-0.5f:0.5f));
    n=(float)exponent;
    r=(value-n*0.693145751953125f)-n*1.428606765330187045e-6f;
    y=__c6_poly(r,coefficients,9);
    return __c6_fvalue(f32_scale(__c6_fbits(y),exponent));
}
