#include "abi.h"

static const float sine_coefficients[]={
 2.75573192239858906526e-6f,-1.98412698412698412698e-4f,
 8.33333333333333333333e-3f,-1.66666666666666666667e-1f};
static const float cosine_coefficients[]={
 -2.75573192239858906526e-7f,2.48015873015873015873e-5f,
 -1.38888888888888888889e-3f,4.16666666666666666667e-2f,-5.0e-1f};

static float sine_kernel(float x)
{
    float z=x*x;
    float p=__c6_poly(z,sine_coefficients,4);
    return x+(x*z)*p;
}
static float cosine_kernel(float x)
{
    float z=x*x;
    float p=__c6_poly(z,cosine_coefficients,5);
    return 1.0f+z*p;
}
float __c6_sin(float value)
{
    uint32_t bits=__c6_fbits(value),magnitude=F32_ABS(bits),result;
    unsigned quadrant=0;
    float r;
    if (magnitude>=F32_INF) return __c6_fvalue(F32_NAN);
    if (magnitude<=0x39800000UL) return value;
    if (magnitude<=0x3f000000UL) r=__c6_fvalue(magnitude);
    else quadrant=__c6_reduce(magnitude,&r);
    r=(quadrant&1)?cosine_kernel(r):sine_kernel(r);
    result=__c6_fbits(r);
    if (quadrant&2) result^=F32_SIGN;
    return __c6_fvalue(result^(bits&F32_SIGN));
}
float __c6_cos(float value)
{
    uint32_t magnitude=F32_ABS(__c6_fbits(value)),result;
    unsigned quadrant=0;
    float r;
    if (magnitude>=F32_INF) return __c6_fvalue(F32_NAN);
    if (magnitude<=0x39800000UL) return 1.0f;
    if (magnitude<=0x3f000000UL) r=__c6_fvalue(magnitude);
    else quadrant=__c6_reduce(magnitude,&r);
    quadrant=(quadrant+1)&3U;
    r=(quadrant&1)?cosine_kernel(r):sine_kernel(r);
    result=__c6_fbits(r);
    if (quadrant&2) result^=F32_SIGN;
    return __c6_fvalue(result);
}
float __c6_tan(float value)
{
    uint32_t bits=__c6_fbits(value),magnitude=F32_ABS(bits);
    unsigned quadrant=0;
    float r,s,c;
    if (magnitude>=F32_INF) return __c6_fvalue(F32_NAN);
    if (magnitude<=0x39800000UL) return value;
    if (magnitude<=0x3f000000UL) r=__c6_fvalue(magnitude);
    else quadrant=__c6_reduce(magnitude,&r);
    s=sine_kernel(r);c=cosine_kernel(r);
    r=(quadrant&1)?-c/s:s/c;
    return __c6_fvalue(__c6_fbits(r)^(bits&F32_SIGN));
}
