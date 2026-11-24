    .setcpu 6
    .bss
    .export __cpu6_hi64

; The two most significant words of a 64-bit builtin operand or result.
; Its low 32 bits follow the existing __hireg:B convention.
__cpu6_hi64:
    .ds 4
