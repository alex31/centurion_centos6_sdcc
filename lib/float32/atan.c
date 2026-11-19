#include "abi.h"
static const float coefficients[]={
 0.04761904761904761905f,-0.05263157894736842105f,
 0.05882352941176470588f,-0.06666666666666666667f,
 0.07692307692307692308f,-0.09090909090909090909f,
 0.11111111111111111111f,-0.14285714285714285714f,
 0.2f,-0.33333333333333333333f};
float __c6_atan(float value)
{
    uint32_t bits=__c6_fbits(value),magnitude=F32_ABS(bits);
    float x,r,z;
    unsigned inverse,shifted;
    if (magnitude>F32_INF) return __c6_fvalue(F32_NAN);
    if (magnitude<=0x39800000UL) return value;
    if (magnitude==F32_INF) return __c6_fvalue((bits&F32_SIGN)|0x3fc90fdbUL);
    x=__c6_fvalue(magnitude);
    inverse=magnitude>0x3f800000UL;
    if (inverse) x=1.0f/x;
    shifted=x>0.41421356237309504880f;
    if (shifted) x=(x-1.0f)/(x+1.0f);
    z=x*x;r=x+(x*z)*__c6_poly(z,coefficients,10);
    if (shifted) r=0.78539816339744830962f+r;
    if (inverse) r=1.57079632679489661923f-r;
    return __c6_fvalue(__c6_fbits(r)^(bits&F32_SIGN));
}
