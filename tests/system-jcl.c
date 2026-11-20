#include <os6.h>
#include <stdio.h>

static const char directory_job[] = ".JOB JCLTEST ABORT=CANCEL\n.DIR 1 (S)\n.END";
static const char paused_job[] =
    ".JOB JCLTEST ABORT=CANCEL\n"
    "/ First log record fills the first sector with the next records. /\n"
    "/ Second log record continues on the dedicated CENTOS SYSLOG. /\n"
    "/ Third log record continues on the dedicated CENTOS SYSLOG. /\n"
    "/ Fourth log record continues on the dedicated CENTOS SYSLOG. /\n"
    "/ Fifth log record continues on the dedicated CENTOS SYSLOG. /\n"
    "/ Sixth log record continues on the dedicated CENTOS SYSLOG. /\n"
    "/ Seventh log record continues on the dedicated CENTOS SYSLOG. /\n"
    "/ JCL TEST WAITING /\n"
    ".PAUSE\n"
    ".DIR 1 (S)\n"
    ".END\n";

static char result[1024];

static int contains(const char *text, const char *part)
{
    while (*text) {
        const char *a = text++, *b = part;
        while (*b && *a == *b) { ++a; ++b; }
        if (!*b) return 1;
    }
    return 0;
}

int main(void)
{
    char small[9] = {0};
    uint16_t size = sizeof(result);
    int status = system_jcl(paused_job, result, &size);
    if (status != OS6_JCL_OK || size <= 400 || size >= sizeof(result) || result[size] ||
        !contains(result, "FIRST LOG RECORD") || !contains(result, "SEVENTH LOG RECORD") ||
        !contains(result, "DISK  1-")) {
        printf("FAIL full capture %d %u\n", status, size);
        return 1;
    }
    puts("PASS full JCL output");

    small[8] = '!';
    size = 8;
    status = system_jcl(directory_job, small, &size);
    if (status != OS6_JCL_TRUNCATED || size != 7 || small[7] || small[8] != '!' ||
        !contains(small, "DISK")) {
        printf("FAIL truncated capture %d %u\n", status, size);
        return 2;
    }
    puts("PASS truncated JCL output");

    size = 0;
    status = system_jcl(directory_job, 0, &size);
    if (status != OS6_JCL_TRUNCATED || size) {
        printf("FAIL zero-capacity capture %d %u\n", status, size);
        return 3;
    }
    puts("PASS zero-capacity JCL output");
    return 0;
}
