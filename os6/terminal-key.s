	.setcpu 6
	.code
	.export _os6_terminal_key

; Use the native CRT single-key logical I/O mode, as the BASIC editor does.
_os6_terminal_key:
	stx (-s)
	cla
	sta (__key_record)
	ldb __key_op
	.byte 0x66,0x00
	.byte 0x66,0x08
	.word __key_op
	ldb (__key_record)
	bz __key_eof
	ldbb (__key_record+2)
	clrb bh
	bra __key_return
__key_eof:
	ldb 0xffff
__key_return:
	ldx (s+)
	rsr

	.data
__key_op:
	.byte 0,0x81,0x08,1
	.word 1,__key_record,0
	.bss
__key_record:
	.ds 4
