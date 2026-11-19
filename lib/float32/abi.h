#ifndef CPU6_F32_ABI_H
#define CPU6_F32_ABI_H
#include "core.h"
uint32_t __c6_fbits(float value);
float __c6_fvalue(uint32_t value);
float __c6_sin(float value);
float __c6_cos(float value);
float __c6_tan(float value);
float __c6_atan(float value);
float __c6_exp(float value);
float __c6_log(float value);
float __c6_poly(float value,const float *coefficients,unsigned count);
unsigned __c6_reduce(uint32_t magnitude,float *residual);
#endif
