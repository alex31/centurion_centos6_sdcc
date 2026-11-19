#ifndef CPU6_ABI_H
#define CPU6_ABI_H
#ifndef __SDCC
#error "cpu6abi.h requires SDCC; FCC uses the default stack ABI"
#endif
/* Put the annotation on every declaration, definition and pointer type.
   Default/unannotated functions retain the FCC-compatible stack convention. */
#define CPU6_REGCALL __sdcccall(1)
#define CPU6_STACKCALL __sdcccall(0)
#endif
