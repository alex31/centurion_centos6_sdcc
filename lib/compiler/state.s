    .setcpu 6
    .bss
    .export __hireg

; SPDX-License-Identifier: GPL-2.0-or-later
; High word of a 32-bit value passed or returned through the scalar ABI.
__hireg:
    .ds 2
