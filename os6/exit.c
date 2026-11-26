#include <stdlib.h>

void os6_terminate(int status);

void exit(int status)
{
    os6_terminate(status);
}
