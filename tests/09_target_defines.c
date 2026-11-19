#if !defined(__cpu6__)
#error CPU6 target must define __cpu6__
#endif
#if defined(__cpu4__)
#error CPU6 target must not define __cpu4__
#endif

int main(void)
{
    return 7;
}
