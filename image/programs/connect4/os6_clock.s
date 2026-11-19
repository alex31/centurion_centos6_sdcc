	.setcpu 6
	.code
	.export _c4_clock_read

; SVC 27 stores the CPU5/CPU6 system time in tenths of a second since midnight.
_c4_clock_read:
	stx (-s)
	ldb 4(s)
	stb (__clock_dest)
	.byte 0x66,0x1b
__clock_dest:
	.word 0
	ldx (s+)
	rsr
