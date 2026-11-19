	.setcpu 6
	.code
	.export _life_write_line

; CENTOS WRITE emits the encoded field and its line terminator in one call.
_life_write_line:
	stx (-s)
	ldb 4(s)
	stb (row_data)
	ldb 6(s)
	stb (row_width)
	.byte 0x66,0x35
	.word __oscrt, row_format
row_data:
	.word 0,0
	ldx (s+)
	rsr

	.data
row_format:
	.byte 2
row_width:
	.word 0
	.byte 0
