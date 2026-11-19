#include <stdint.h>
#include <math.h>
#include "abi.h"
typedef union {float f;uint32_t u;} Word;
uint32_t oracle(unsigned op,uint32_t x,uint32_t y)
{
 Word a,b,r;a.u=x;b.u=y;volatile float av=a.f,bv=b.f,t;
 switch(op){
 case 0:r.f=av+bv;break;
 case 1:r.f=av-bv;break;
 case 2:r.f=av*bv;break;
 case 3:r.f=av/bv;break;
 case 4:r.f=sqrtf(av);break;
 case 5:r.f=__c6_sin(av);break;
 case 6:r.f=__c6_cos(av);break;
 case 7:r.f=__c6_tan(av);break;
 case 8:r.f=floorf(av);break;
 case 9:r.f=ceilf(av);break;
 case 10:r.f=truncf(av);break;
 case 11:r.f=(float)(int32_t)x;break;
 case 12:r.f=(float)x;break;
 case 13:return (uint32_t)(int32_t)av;
 case 14:return (uint32_t)av;
 case 15:r.f=-av;break;
 case 16:return (av==bv)|((av!=bv)<<1)|((av<bv)<<2)|((av<=bv)<<3)|((av>bv)<<4)|((av>=bv)<<5)|((!(av<bv))<<6)|((!(av>=bv))<<7);
 case 17:r.f=av+0.0f;break;
 case 18:r.f=av*0.0f;break;
 case 19:r.f=av-av;break;
 case 20:r.f=av/av;break;
 case 21:t=av+16777216.0f;r.f=t-16777216.0f;break;
 case 22:r.f=__c6_atan(av);break;
 case 23:r.f=__c6_exp(av);break;
 case 24:r.f=__c6_log(av);break;
 default:return 0;
 }
 return r.u;
}
