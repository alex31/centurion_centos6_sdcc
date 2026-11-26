#include <stdint.h>
#include <time.h>

uint16_t os6_date_word(void);
void os6_clock_read(uint32_t *ticks);

time_t time(time_t *result)
{
    uint16_t date;
    uint32_t ticks;
    time_t value = (time_t)-1;

    /* Keep the date and clock samples on the same side of midnight. */
    do {
        date = os6_date_word();
        os6_clock_read(&ticks);
    } while (date != os6_date_word());

    if (date >= 1 && date <= 36525u && ticks < 864000UL) {
        /* DT counts from 1 on 01/01/00, with leap years every four years.
           Interpret 70..99 as 1970..1999 and 00..69 as 2000..2069. */
        unsigned int days = date >= 25569u ? date - 25569u : date - 1 + 10957u;
        value = days * 86400UL + ticks / 10UL;
    }
    if (result)
        *result = value;
    return value;
}
