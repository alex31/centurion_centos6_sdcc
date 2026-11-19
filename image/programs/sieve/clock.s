	.setcpu 6
	.code
	.export _sieve_clock_read
; CENTOS SVC 27 writes time of day in tenths of a second.
_sieve_clock_read:
	stx (-s)
	ldb 4(s)
	stb (clock_dest)
	.byte 0x66,0x1b
clock_dest:
	.word 0
	ldx (s+)
	rsr
