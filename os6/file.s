	.setcpu 6
	.code
	.export _os6_assign_raw
	.export _os6_file_open_raw
	.export _os6_file_read_raw
	.export _os6_file_write_raw
	.export _os6_file_end_raw
	.export _os6_file_close_raw

; SVC 5/14 is the dynamic .USE operation used by APLIB's DOTUSE.
_os6_assign_raw:
	stx (-s)
	ldb 4(s)
	stb (__assign_name)
	ldb 6(s)
	stb (__assign_rcb)
	ldb 8(s)
	stbb (__assign_unit)
	ldb 10(s)
	stbb (__assign_flags)
	.byte 0x66,0x05,0x0e
__assign_name:
	.word 0
__assign_rcb:
	.word 0
__assign_unit:
	.byte 0
__assign_flags:
	.byte 0
	xfr a,b
	ldx (s+)
	rsr

_os6_file_open_raw:
	stx (-s)
	ldb 4(s)
	stb (__open_rcb)
	ldb 6(s)
	stbb (__open_mode)
	.byte 0x66,0x39
__open_mode:
	.byte 0
__open_rcb:
	.word 0
	.byte 0
	bra status

_os6_file_write_raw:
	stx (-s)
	ldb 4(s)
	stb (__write_rcb)
	ldb 6(s)
	stb (__write_record)
	.byte 0x66,0x38
__write_rcb:
	.word 0
__write_record:
	.word 0
	bra status

_os6_file_read_raw:
	stx (-s)
	ldb 4(s)
	stb (__read_rcb)
	ldb 6(s)
	stb (__read_record)
	.byte 0x66,0x37
__read_rcb:
	.word 0
__read_record:
	.word 0
	bra status

_os6_file_end_raw:
	stx (-s)
	ldb 4(s)
	stb (__end_rcb)
	.byte 0x66,0x3a,0x0b
__end_rcb:
	.word 0
	bra status

_os6_file_close_raw:
	stx (-s)
	ldb 4(s)
	stb (__close_rcb)
	.byte 0x66,0x3a,0x0a
__close_rcb:
	.word 0
status:
	ldb 4(s)
	ldbb (b)
	clrb bh
	ldx (s+)
	rsr
