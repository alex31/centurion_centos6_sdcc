	.setcpu 6
	.code
	.export _mmu_raw
	.export _mmu_present

; Only CENTOS changes mappings. The user stack, caller and gate occupy the
; same upper-half pages in maps 4 and 6. Save every C callee-saved register.
_mmu_raw:
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

; @GBKV block 2, two bytes at offset 0x39: native BASIC monitor entry.
; Both bytes are readable in CENTOS 7.13's protection table at 0x2cf2.
_mmu_present:
	stx (-s)
	xfr y,a
	sta (-s)
	xfr z,a
	sta (-s)
	.byte 0x66,0x5c,2,2,0x39
	.word mmu_value
	ldb (mmu_value)
	lda (s+)
	xfr a,z
	lda (s+)
	xfr a,y
	ldx (s+)
	rsr
mmu_value:
	.word 0
