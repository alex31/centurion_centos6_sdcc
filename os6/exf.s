	.setcpu 6
	.code
	.export _os6_exf_call

; B points to the RCB for the unprotected logical I/O service.
_os6_exf_call:
	stx (-s)
	ldb 4(s)
	.byte 0x66,0x18
	ldb 4(s)
	ldbb (b)
	clrb bh
	ldx (s+)
	rsr
