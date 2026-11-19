#include "abi.h"

/* floor((2/pi)*2^256), little-endian. Enough guard precision for every
   binary32 exponent. Generated independently using Decimal Machin series. */
static const uint8_t two_over_pi[32]={
 0x61,0xc5,0xbb,0xde,0xab,0x63,0x51,0xfe,0x41,0x90,0x43,0x3c,0x99,0x95,0x62,0xdb,
 0xc0,0xdd,0x34,0xf5,0xd1,0x57,0x27,0xfc,0x29,0x15,0x44,0x4e,0x6e,0x83,0xf9,0xa2};

static unsigned product_bit(const uint8_t *product,int bit)
{
    if (bit<0 || bit>=280) return 0;
    return (product[(unsigned)bit>>3]>>(bit&7))&1U;
}

unsigned __c6_reduce(uint32_t magnitude,float *residual)
{
    uint8_t product[35];
    uint32_t mantissa=(magnitude&F32_FRAC)|F32_HIDDEN,significand=0;
    unsigned i,j,limb,carry,value,quadrant,negative,bytes;
    int exponent=(int)(magnitude>>23)-127,point=279-exponent,top,bit;
    for (i=0;i<35;i++) product[i]=0;
    for (i=0;i<3;i++) {
        limb=(unsigned)((mantissa>>(i*8))&255U);carry=0;
        for (j=0;j<32;j++) {
            value=(unsigned)product[i+j]+limb*(unsigned)two_over_pi[j]+carry;
            product[i+j]=(uint8_t)value;carry=value>>8;
        }
        product[i+32]=(uint8_t)carry;
    }
    quadrant=product_bit(product,point)|(product_bit(product,point+1)<<1);
    negative=product_bit(product,point-1);
    if (negative) quadrant=(quadrant+1)&3U;
    bytes=(unsigned)(point+7)>>3;
    if (negative) {
        carry=1;
        for (i=0;i<bytes;i++) {
            value=(unsigned)(product[i]^255U)+carry;
            product[i]=(uint8_t)value;carry=value>>8;
        }
    }
    if (point&7) product[bytes-1]&=(uint8_t)((1U<<(point&7))-1U);
    for (i=bytes;i<35;i++) product[i]=0;
    top=point-1;
    while (top>=0 && !product_bit(product,top)) top--;
    if (top<0) {*residual=0.0f;return quadrant;}
    for (i=0;i<27;i++) significand=(significand<<1)|product_bit(product,top-(int)i);
    for (bit=top-27;bit>=0;bit--) if (product_bit(product,bit)) {significand|=1;break;}
    *residual=__c6_fvalue(f32_pack(negative,top-point+127,significand))*1.57079632679489661923f;
    return quadrant;
}
