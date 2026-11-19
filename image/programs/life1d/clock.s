	.setcpu 6
	.code
	.export _life_clock_read

; CENTOS SVC 27 writes a 32-bit count of tenths since midnight.
_life_clock_read:
	stx (-s)
	ldb 4(s)
	stb (clock_target)
	.byte 0x66,0x1b
clock_target:
	.word 0
	ldx (s+)
	rsr
