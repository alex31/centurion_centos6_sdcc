#ifndef CPU6_OS6_H
#define CPU6_OS6_H
#include <stdint.h>

/* Cooperate with CENTOS during long calculations. */
void os6_checkpoint(void);

enum {
    OS6_JCL_OK = 0,
    OS6_JCL_TRUNCATED = 1,
    OS6_JCL_ARGUMENT = -1,
    OS6_JCL_IO = -2,
    OS6_JCL_TIMEOUT = -3
};

/* Submit a native JCL stream on the dedicated job console and capture SYSLOG.
   *size includes space for NUL on entry and counts copied bytes on return.
   A zero capacity permits a null output pointer. See doc/jcl.md for setup. */
int system_jcl(const char *job, char *output, uint16_t *size);
#endif
