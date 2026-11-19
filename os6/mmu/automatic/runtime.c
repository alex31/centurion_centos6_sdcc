#include <stdint.h>
#include <stdio.h>
#include <os6.h>

struct mmu_packet {
    uint16_t magic, id1, id2, task, status, result;
    uint16_t (*run)(void);
};
extern uint16_t __mp_probe(void);
extern uint16_t __mp_task(void);
extern uint16_t __mp_enter(struct mmu_packet *);
extern uint16_t __mp_claim(uint16_t);
extern void __mp_release(void);
extern const unsigned char __mp_initial[];
extern int main(void);

/* Generated into the lower module. Validate its identity before calling any
   address in the independently loaded resident image. */
uint16_t __mp_dispatch(struct mmu_packet *p) attribute((mmu_page=auto))
{
    uint16_t address = (uint16_t)p;
    if (address < 0x8000U || sizeof(*p) - 1U > 0xffffU - address) return 1;
    if (p->magic != 0x6d36U || p->id1 != MMU_ID1 || p->id2 != MMU_ID2) {
        p->status = 2;
        return 0;
    }
    if (!p->task || (uint16_t)p->run < 0x8000U) {p->status = 3; return 0;}
    if (!__mp_claim(p->task)) {p->status = 4; return 0;}
    p->status = 0;
    p->result = p->run();
    __mp_release();
    return 0;
}

/* Private initialization bytecode: target16, length16, mode8, optional data.
   Zero target terminates the stream. All data stays in the user partition. */
static uint16_t word(const unsigned char **cursor)
{
    uint16_t value = (uint16_t)(*cursor)[0] << 8 | (*cursor)[1];
    *cursor += 2;
    return value;
}

static uint16_t __mp_run(void)
{
    const unsigned char *cursor = __mp_initial;
    uint16_t target, count, budget = 256;
    unsigned char mode, *destination;
    while ((target = word(&cursor)) != 0) {
        count = word(&cursor);
        mode = *cursor++;
        destination = (unsigned char *)target;
        while (count--) {
            *destination++ = mode ? *cursor++ : 0;
            if (!--budget) {os6_checkpoint(); budget = 256;}
        }
    }
    return main();
}

int __mp_launch(void)
{
    struct mmu_packet packet;
    if (__mp_probe() != 0x1000U) {
        puts("MMU module absent");
        return 250;
    }
    packet.magic = 0x6d36U; packet.id1 = MMU_ID1; packet.id2 = MMU_ID2;
    packet.task = __mp_task() + 1U;
    packet.status = 5; packet.result = 251;
    packet.run = __mp_run;
    if (__mp_enter(&packet) || packet.status) {
        puts("MMU module mismatch or busy");
        return 251;
    }
    return packet.result;
}
