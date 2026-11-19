	.setcpu 6
	.code
	.export ___mp_enter
	.export ___mp_probe
	.export ___mp_task
___mp_enter:
	stx (-s)
	xfr y,a
	sta (-s)
	xfr z,a
	sta (-s)
	ldb 8(s)
	stb (-s)
	stx (-s)
	ldx mmu_return
	.byte 0x66,0x6a
mmu_return:
	ldx (s+)
	xfr b,a
	ldb mmu_resume
	.byte 0x66,0x1a
mmu_resume:
	xfr a,b
	lda (s+)
	xfr a,z
	lda (s+)
	xfr a,y
	ldx (s+)
	rsr
___mp_probe:
	stx (-s)
	xfr y,a
	sta (-s)
	xfr z,a
	sta (-s)
	.byte 0x66,0x5c,2,2,0x39
	.word mmu_value
	ldb (mmu_value)
	bra mmu_done
___mp_task:
	stx (-s)
	xfr y,a
	sta (-s)
	xfr z,a
	sta (-s)
	.byte 0x66,0x5c,0,1,1
	.word mmu_value
	ldbb (mmu_value)
	clrb bh
mmu_done:
	lda (s+)
	xfr a,z
	lda (s+)
	xfr a,y
	ldx (s+)
	rsr
mmu_value:
	.word 0
