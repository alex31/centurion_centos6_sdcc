#define CPU6_NATIVE_ADAPTER_ONLY
#include "native-arithmetic-host.c"

static uint64_t input64(const uint8_t *p)
{
    uint64_t n = 0; unsigned i;
    for (i = 0; i < 8; i++) n = (n << 8) | p[i];
    return n;
}
int main(void)
{
    uint8_t in[20], out[44];
    cpu6_init();
    while (fread(in, sizeof(in), 1, stdin) == 1) {
        unsigned source_size = in[0], dest_size = in[1], i;
        for (i = 0; i < 8; i++) {
            mem_write8(0x5000 + i, 0xa5); mem_write8(0x5020 + i, 0xa5); mem_write8(0x5040 + i, 0x55);
        }
        bignum_write(0x5000, dest_size, input64(in + 4));
        bignum_write(0x5020, source_size, input64(in + 12));
        regpair_write(A,0x5040); regpair_write(B,0x5020); regpair_write(X,0x5000);
        regpair_write(Y,0x1111); regpair_write(Z,0x2222); regpair_write(S,0xdf00);
        regpair_write(C,0x3333); regpair_write(P,0x4444);
        alu_out = in[3]; pc = 0x200;
        mem_write8(0x200,0x46); mem_write8(0x201,(source_size-1)*16+dest_size-1);
        mem_write8(0x202,in[2]); mem_write8(0x203,0x24);
        cpu6_execute_one(0);
        for (i=0;i<8;i++) {out[i]=mem_read8(0x5000+i);out[8+i]=mem_read8(0x5020+i);out[16+i]=mem_read8(0x5040+i);}
        for (i=0;i<8;i++) {uint16_t v=regpair_read(i*2);out[24+i*2]=v>>8;out[25+i*2]=v;}
        out[40]=alu_out;out[41]=cpu_ipl;out[42]=pc>>8;out[43]=pc;
        if (fwrite(out,sizeof(out),1,stdout)!=1) return 1;
    }
    return ferror(stdin)||ferror(stdout);
}
