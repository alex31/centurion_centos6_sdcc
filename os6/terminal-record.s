	.setcpu 6
	.code
	.export _os6_terminal_record

; The native CRT performs line editing and returns a counted record.
_os6_terminal_record:
	stx (-s)
	ldb 4(s)
	stb (__record_buffer)
	ldb 6(s)
	stb (__record_limit)
	ldb __record_op
	.byte 0x66,0x00
	.byte 0x66,0x08
	.word __record_op
	ldb (__record_buffer)
	ldb (b)
	ldx (s+)
	rsr

	.data
__record_op:
	.byte 0,0x81,0,1
__record_limit:
	.word 0
__record_buffer:
	.word 0,0
