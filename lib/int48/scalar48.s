	.setcpu 6
	.export __cpu6_mid48
	.bss
; Additional return/accumulator word for the opt-in SDCC 48-bit scalar ABI.
; The other words are __hireg (high) and B (low). Link only when referenced.
__cpu6_mid48:
	.ds 2
