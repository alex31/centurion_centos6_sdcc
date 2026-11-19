	.setcpu 6
	.code
	.export _mmu_yield
; Native CENTOS yield, with no pointers into the lower module window.
_mmu_yield:
	stx (-s)
	xfr y,a
	sta (-s)
	xfr z,a
	sta (-s)
	.byte 0x66,7
	lda (s+)
	xfr a,z
	lda (s+)
	xfr a,y
	ldx (s+)
	rsr
