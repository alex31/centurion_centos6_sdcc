	.setcpu 6
	.discard
	.export __mmu_start
	.export ___mp_claim
	.export ___mp_release
	.export __mmu_owner
	.export __mmu_lock
__mmu_start:
	jmp ___mp_dispatch
; This word is compiler runtime state, never reset as a C application object.
; A partition may restart after an OS abort; another active partition is denied.
__mmu_owner:
	.word 0
__mmu_lock:
	.word 1
___mp_claim:
	stx (-s)
	lda (__mmu_owner)
	ldb 4(s)
	sub a,b
	bz mmu_owned
	cla
; One CPU6 instruction changes the memory word and its condition flags.
; Failed contenders roll back their decrement before reporting busy.
	.byte 0x31,0x10
	.word __mmu_lock
	bz mmu_acquire
	.byte 0x30,0x10
	.word __mmu_lock
	ldb 0
	ldx (s+)
	rsr
mmu_acquire:
	ldb 4(s)
	stb (__mmu_owner)
mmu_owned:
	ldb 1
	ldx (s+)
	rsr
___mp_release:
	cla
	sta (__mmu_owner)
	.byte 0x30,0x10
	.word __mmu_lock
	rsr
