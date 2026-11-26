#ifndef CPU6_OS6_TIME_H
#define CPU6_OS6_TIME_H

#ifndef NULL
#define NULL ((void *)0)
#endif

/* Seconds since 1970-01-01 in the system clock's time basis. */
typedef unsigned long time_t;
time_t time(time_t *result);

#endif
