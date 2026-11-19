	.setcpu 6
	.code
	.export _os6_readline_raw
_os6_readline_raw:
	stx (-s)
	.byte 0x66,0x34
	.word __oscrt, __osfmt75, __osline, 0
	ldb __osline
	ldx (s+)
	rsr
	.bss
__osline:
	.ds 76
