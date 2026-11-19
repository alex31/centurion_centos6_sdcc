/* Auto-indexed loops, boundary displacements, volatile accesses and pointers. */
#include <stdint.h>
#include <stdio.h>

static uint8_t bytes[512], output8[520];
static uint16_t words[80], output16[84];
static volatile uint8_t volatile8[80];
static volatile uint16_t volatile16[80];

uint16_t sum8(const uint8_t *p, unsigned n)
{ uint16_t sum=0; while(n--) sum += *p++; return sum; }
uint16_t sum16(const uint16_t *p, unsigned n)
{ uint16_t sum=0; while(n--) sum += *p++; return sum; }
void copy8(uint8_t *d, const uint8_t *s, unsigned n)
{ while(n--) *d++=*s++; }
void copy16(uint16_t *d, const uint16_t *s, unsigned n)
{ while(n--) *d++=*s++; }
void back8(uint8_t *d, const uint8_t *s, unsigned n)
{ while(n--) *--d=*--s; }
void back16(uint16_t *d, const uint16_t *s, unsigned n)
{ while(n--) *--d=*--s; }
uint16_t negative_offset(const uint8_t *p, unsigned n)
{ uint16_t sum=0; while(n--) { sum += p[-128]; p++; } return sum; }
uint16_t positive_offset(const uint8_t *p, unsigned n)
{ uint16_t sum=0; while(n--) { sum += p[127]; p++; } return sum; }
uint8_t *fill(uint8_t *p, unsigned n)
{ while(n--) *p++=7; return p; }
void write_volatile(volatile uint8_t *p, unsigned n)
{ while(n--) *p++=3; }
uint16_t read_volatile(const volatile uint16_t *p, unsigned n)
{ uint16_t sum=0; while(n--) sum += *p++; return sum; }

int main(void)
{
    unsigned i,n;
    for(i=0;i<512;i++) bytes[i]=(uint8_t)(i*37U+19U);
    for(i=0;i<80;i++) { words[i]=(uint16_t)(i*1003U+0x7f80U); volatile16[i]=i; }
    for(n=0;n<=64;n++) {
        uint16_t expected8=0,expected16=0,expected_positive=0;
        for(i=0;i<520;i++) output8[i]=0xa5;
        for(i=0;i<84;i++) output16[i]=0xa55aU;
        for(i=0;i<n;i++) { expected8+=bytes[i];expected16+=words[i];expected_positive+=bytes[i+255]; }
        if(sum8(bytes,n)!=expected8 || sum16(words,n)!=expected16 ||
           negative_offset(bytes+128,n)!=expected8 || positive_offset(bytes+128,n)!=expected_positive) {
            puts("FAIL autoindex sums");return 1;
        }
        copy8(output8+2,bytes,n); copy16(output16+2,words,n);
        for(i=0;i<n;i++) if(output8[i+2]!=bytes[i] || output16[i+2]!=words[i]) {
            puts("FAIL autoindex forward copy");return 2;
        }
        if(output8[1]!=0xa5 || output8[n+2]!=0xa5 || output16[1]!=0xa55aU || output16[n+2]!=0xa55aU) {
            puts("FAIL autoindex forward guard");return 3;
        }
        back8(output8+2+n,bytes+32+n,n); back16(output16+2+n,words+8+n,n);
        for(i=0;i<n;i++) if(output8[i+2]!=bytes[i+32] || output16[i+2]!=words[i+8]) {
            puts("FAIL autoindex reverse copy");return 4;
        }
        if(output8[1]!=0xa5 || output8[n+2]!=0xa5 || output16[1]!=0xa55aU || output16[n+2]!=0xa55aU) {
            puts("FAIL autoindex reverse guard");return 5;
        }
        if(fill(output8+2,n)!=output8+2+n) { puts("FAIL autoindex pointer result");return 6; }
        for(i=0;i<n;i++) if(output8[i+2]!=7) { puts("FAIL autoindex fill");return 7; }
        write_volatile(volatile8,n);
        for(i=0;i<n;i++) if(volatile8[i]!=3) { puts("FAIL autoindex volatile store");return 8; }
        if(read_volatile(volatile16,n)!=(uint16_t)(n*(n-1U)/2U)) {
            puts("FAIL autoindex volatile load");return 9;
        }
    }
    puts("PASS autoindex loops");return 0;
}
