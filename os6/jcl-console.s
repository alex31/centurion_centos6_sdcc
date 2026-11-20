	.setcpu 6
	.code
	.export _os6_jcl_submit
	.export _os6_jcl_clock

; SYS001 is the caller's transmit line to the independent JCL console.
_os6_jcl_submit:
	stx (-s)
	.byte 0x66,0x39,3
	.word jcl_crt
	.byte 0
	ldab (jcl_crt)
	bnz jcl_failed
	; WRITEN keeps the terminating CR in the field. CENTOS sends CR/LF.
	.byte 0x66,0x31
	.word jcl_crt,jcl_format,jcl_command,0
	ldbb (jcl_crt)
	clrb bh
	stb (-s)
	.byte 0x66,0x3a,0x0a
	.word jcl_crt
	ldb (s+)
	bnz jcl_done
jcl_failed:
	ldbb (jcl_crt)
	clrb bh
jcl_done:
	ldx (s+)
	rsr

_os6_jcl_clock:
	stx (-s)
	ldb 4(s)
	stb (jcl_clock_target)
	.byte 0x66,0x1b
jcl_clock_target:
	.word 0
	ldx (s+)
	rsr

	.data
jcl_crt:
	.byte 0,1
	.ds 11
	.byte 2
	.ds 16
jcl_format:
	.byte 2
	.word 16
	.byte 0
jcl_command:
	.byte 0xCA,0xC3,0xCC,0xD3,0xC4,0xCB,0xAE,0xD2,0xC5,0xD1,0xA0,0xCF,0xCE,0xA0,0xB1,0x8D
