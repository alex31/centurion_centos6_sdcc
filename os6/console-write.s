	.setcpu 6
	.code
	.export _os6_write_encoded

; Internal console sink: a nonempty field of encoded characters.
; WRITEN preserves the current line. Newlines use the existing putchar path.
_os6_write_encoded:
	stx (-s)
	ldb 4(s)
	stb (printf_data)
	ldb 6(s)
	stb (printf_length)
	.byte 0x66,0x31
	.word __oscrt, printf_format
printf_data:
	.word 0,0
	ldx (s+)
	rsr

	.data
printf_format:
	.byte 2
printf_length:
	.word 0
	.byte 0
