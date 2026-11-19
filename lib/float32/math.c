#include "abi.h"
float sinf(float a){return __c6_sin(a);}
float cosf(float a){return __c6_cos(a);}
float tanf(float a){return __c6_tan(a);}
float atanf(float a){return __c6_atan(a);}
float expf(float a){return __c6_exp(a);}
float logf(float a){return __c6_log(a);}
float sqrtf(float a){return __c6_fvalue(f32_sqrt(__c6_fbits(a)));}
float fabsf(float a){return __c6_fvalue(F32_ABS(__c6_fbits(a)));}
float copysignf(float a,float b){return __c6_fvalue(F32_ABS(__c6_fbits(a))|(__c6_fbits(b)&F32_SIGN));}
float truncf(float a){return __c6_fvalue(f32_integral(__c6_fbits(a),0));}
float floorf(float a){return __c6_fvalue(f32_integral(__c6_fbits(a),1));}
float ceilf(float a){return __c6_fvalue(f32_integral(__c6_fbits(a),2));}
int __c6_isnan(float a){return F32_ABS(__c6_fbits(a))>F32_INF;}
int __c6_isinf(float a){return F32_ABS(__c6_fbits(a))==F32_INF;}
int __c6_isfinite(float a){return F32_ABS(__c6_fbits(a))<F32_INF;}
int __c6_signbit(float a){return (int)(__c6_fbits(a)>>31);}
