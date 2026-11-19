	.setcpu 6
	.code
	.export _os6_ov_raw
; Native CPL LOAD, option 2. Its byte parameter count is zero. Keep all C
; callee-saved registers, not just the return link; result remains in B.
_os6_ov_raw:
	stx (-s)
	xfr y,a
	sta (-s)
	xfr z,a
	sta (-s)
	ldb 8(s)
	stb (ov_name)
	ldb 10(s)
	stb (ov_slot)
	.byte 0x66,0x05,0x09
ov_name:
	.word 0
ov_slot:
	.word 0
	.word 2
	.byte 0
	lda (s+)
	xfr a,z
	lda (s+)
	xfr a,y
	ldx (s+)
	rsr
