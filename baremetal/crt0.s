	.setcpu 6
	.code
	.export start
start:
	lda 0xdffe
	xfr a,s
	ldab 0xc5
	stab (0xf200)
	; Clear the linker's BSS, including the compiler's __hireg.
	ldx __bss_size
	bz ready
	ldb __bss
	cla
clear_bss:
	stab (b+)
	dcx
	bnz clear_bss
ready:
	jsr _main
	hlt
