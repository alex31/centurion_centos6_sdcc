	.setcpu 6
	.code
	.export _os6_checkpoint
; Yield to CENTOS during lengthy CPU computations. The bundled CPL sieve
; uses the same SVC 7 within its long loops.
_os6_checkpoint:
	stx (-s)
	.byte 0x66,0x07
	ldx (s+)
	rsr
