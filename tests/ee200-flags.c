#include <stdio.h>
int cpu6_flags(void);
int main(void)
{
    int result = cpu6_flags();
    puts(result ? "FAIL: CPU6 word and load/clear flags" : "PASS: CPU6 word and load/clear flags");
    return result;
}
