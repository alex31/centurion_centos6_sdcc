	.setcpu 6
	.common
	.export __osctx
	.export __oscrt
	.export __osfmt1
	.export __osfmt75
	.export __osctxp

; This layout reproduces the startup data used by the working XHELLO CPL
; application. The linker places COMMON at logical address 0x0087.
__osctl1:
	.ds 4
__osctl2:
	.ds 1
__osctx:
	.word __osctx+7
	.word 0,0,0
	.byte 0x50
	.word 0
	.ds 80
	.word 0
__oscrt:
	.byte 0,0x81
	.ds 11
	.byte 2
	.ds 16
	.ds 75
	.byte 0,0,0
__osfmt75:
	.byte 2
	.word 75
	.byte 0
__osfmt1:
	.byte 2
	.word 1
	.byte 0
__osctxp:
	.word __osctx

	.bss
	.export _os6_exit_flush
_os6_exit_flush:
	.ds 2
; Keep the C stack out of the inherited low-memory OS/CPU context. Calls in
; nontrivial programs can otherwise overwrite those reserved locations.
__osstack:
	.ds 1024
__osstack_end:

	.code
	.export start
	.export __os6_finish
start:
	lda __osctx
	xfr a,s
	; Register the application context, then open SYSIPT as CRT.
	.byte 0x66,0x5d,0,2,0x19
	.word __osctxp
	.byte 0x66,0x39,3
	.word __oscrt
	.byte 0
	; The executable does not contain BSS; initialize it before main.
	ldx __bss_size
	bz ready
	ldb __bss
	cla
clear_bss:
	stab (b+)
	dcx
	bnz clear_bss
ready:
	lda __osstack_end
	xfr a,s
	jsr _main
__os6_finish:
	stbb (__exit_code)
	; Buffered printf installs a callback only when batching is enabled.
	ldb (_os6_exit_flush)
	bz close_crt
	jsr (b)
close_crt:
	; CLOSE CRT and STOP using the low byte returned by main.
	.byte 0x66,0x3a,0x0a
	.word __oscrt
	.byte 0x66,0x0a
__exit_code:
	.byte 0
