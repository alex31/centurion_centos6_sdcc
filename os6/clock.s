	.setcpu 6
	.code
	.export _os6_date_word
	.export _os6_clock_read

_os6_date_word:
	stx (-s)
	; DT returns the binary date in A.
	.byte 0x66,0x0b
	xab
	ldx (s+)
	rsr

_os6_clock_read:
	stx (-s)
	ldb 4(s)
	stb (__os6_clock_dest)
	; GTIME stores tenths of a second since midnight.
	.byte 0x66,0x1b
__os6_clock_dest:
	.word 0
	ldx (s+)
	rsr
