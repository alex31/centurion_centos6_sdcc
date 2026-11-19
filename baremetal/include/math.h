#ifndef CPU6_MATH_H
#define CPU6_MATH_H
#define INFINITY (1.0f/0.0f)
#define NAN (0.0f/0.0f)
#define HUGE_VALF INFINITY
float sinf(float);
float cosf(float);
float tanf(float);
float atanf(float);
float expf(float);
float logf(float);
float sqrtf(float);
float fabsf(float);
float copysignf(float,float);
float truncf(float);
float floorf(float);
float ceilf(float);
int __c6_isnan(float);
int __c6_isinf(float);
int __c6_isfinite(float);
int __c6_signbit(float);
#define isnan(x) __c6_isnan(x)
#define isinf(x) __c6_isinf(x)
#define isfinite(x) __c6_isfinite(x)
#define signbit(x) __c6_signbit(x)
#endif
