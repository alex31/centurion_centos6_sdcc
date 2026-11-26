#include <assert.h>
#include <stdint.h>

#include "cpu6.h"
#include "mux.h"
#include "scheduler.h"

volatile unsigned emulator_done;
static int64_t now;
static unsigned fired, level;

int64_t get_current_time(void) { return now; }
uint16_t cpu6_pc(void) { return 0; }
void cpu_assert_irq(unsigned ipl) { level = ipl; }
void cpu_deassert_irq(unsigned ipl) { (void)ipl; }
void mux_poll_fds(unsigned trace) { (void)trace; }

static void dispatch(struct event_t *event, int64_t late_ns)
{
    (void)event;
    assert(late_ns == 0);
    ++fired;
}

int main(void)
{
    struct event_t event = {.callback = dispatch, .name = "device"};
    event.delta_ns = 2;
    schedule_event(&event);
    event.delta_ns = 4;
    schedule_event(&event);
    assert(scheduler_next() == 4);
    now = 4;
    run_scheduler(now, 0);
    assert(fired == 1 && scheduler_next() == -1);

    mux_init();
    mux_write(MUX0_BASE + 10, 0xa3, 0);
    mux_write(MUX0_BASE + 12, 9, 0); /* Transmit interrupts for UARTs 0 and 3. */
    assert(mux_read(MUX0_BASE + 15, 0) == 255);
    mux_write(MUX0_BASE + 14, 1, 0);
    mux_poll(0);
    assert(level == 3 && mux_read(MUX0_BASE + 15, 0) == 1);
    mux_poll(0);
    assert(mux_read(MUX0_BASE + 15, 0) == 7);
    mux_poll(0);
    assert(mux_read(MUX0_BASE + 15, 0) == 255);
    return 0;
}
