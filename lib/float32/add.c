#include "core.h"

#ifndef CPU6_FLOAT_ABI_THUNKS
uint32_t f32_add(uint32_t a,uint32_t b)
{
    uint32_t aa=F32_ABS(a),bb=F32_ABS(b),ma,mb,tmp;
    unsigned sign;
    int ea,eb;
    if (aa>F32_INF || bb>F32_INF) return F32_NAN;
    if (aa==F32_INF) return bb==F32_INF && ((a^b)&F32_SIGN) ? F32_NAN : a;
    if (bb==F32_INF) return b;
    if (!aa && !bb) return (a&b)&F32_SIGN;
    if (!aa) return b;
    if (!bb) return a;
    if (aa<bb) {tmp=a;a=b;b=tmp;tmp=aa;aa=bb;bb=tmp;}
    sign=(unsigned)(a>>31);
    ea=(int)(aa>>23);eb=(int)(bb>>23);
    ma=(aa&F32_FRAC)|(ea?F32_HIDDEN:0);
    mb=(bb&F32_FRAC)|(eb?F32_HIDDEN:0);
    if (!ea) ea=1;
    if (!eb) eb=1;
    ma<<=3;mb=f32_jam(mb<<3,(unsigned)(ea-eb));
    if (!((a^b)&F32_SIGN)) {
        ma+=mb;
        if (ma&0x08000000UL) {ma=f32_jam(ma,1);ea++;}
    } else {
        ma-=mb;
        if (!ma) return 0;
        while (ma<0x04000000UL && ea>1) {ma<<=1;ea--;}
    }
    return f32_pack(sign,ea,ma);
}
#endif
