/* Binary32 round-to-nearest-even core using only 8/16/32-bit integers. */
#ifndef CPU6_F32_CORE_H
#define CPU6_F32_CORE_H
#include <stdint.h>
#define f32_jam __c6_fjam
#define f32_pack __c6_fpack
#define f32_unpack __c6_funpack
#define f32_add __c6_fadd
#define f32_mul __c6_fmul
#define f32_div __c6_fdiv
#define f32_fromu __c6_ffromu
#define f32_fromi __c6_ffromi
#define f32_tou __c6_ftou
#define f32_toi __c6_ftoi
#define f32_eq __c6_feq
#define f32_lt __c6_flt
#define f32_le __c6_fle
#define f32_sqrt __c6_fsqrt
#define f32_integral __c6_fint
#define f32_scale __c6_fscale
#define F32_SIGN 0x80000000UL
#define F32_INF 0x7f800000UL
#define F32_NAN 0x7fc00000UL
#define F32_FRAC 0x007fffffUL
#define F32_HIDDEN 0x00800000UL
#define F32_ABS(a) ((a)&0x7fffffffUL)
uint32_t f32_jam(uint32_t value, unsigned distance);
uint32_t f32_pack(unsigned sign,int exponent,uint32_t significand);
int f32_unpack(uint32_t bits,uint32_t *significand);
uint32_t f32_add(uint32_t a,uint32_t b);
uint32_t f32_mul(uint32_t a,uint32_t b);
uint32_t f32_div(uint32_t a,uint32_t b);
uint32_t f32_fromu(uint32_t value);
uint32_t f32_fromi(int32_t value);
uint32_t f32_tou(uint32_t bits);
int32_t f32_toi(uint32_t bits);
unsigned char f32_eq(uint32_t a,uint32_t b);
unsigned char f32_lt(uint32_t a,uint32_t b);
unsigned char f32_le(uint32_t a,uint32_t b);
uint32_t f32_sqrt(uint32_t a);
uint32_t f32_integral(uint32_t a,unsigned mode);
uint32_t f32_scale(uint32_t a,int power);
#endif
