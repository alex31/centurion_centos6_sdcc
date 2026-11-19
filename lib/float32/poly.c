#include "abi.h"
/* Shared Horner evaluator: code size does not grow with polynomial degree. */
float __c6_poly(float value,const float *coefficients,unsigned count)
{
    float result;
    if (!count) return 0.0f;
    result=*coefficients++;
    while (--count) result=result*value+*coefficients++;
    return result;
}
