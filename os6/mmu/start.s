	.setcpu 6
	.code
	.export __mmu_start
; .LOAD BASIC enters at logical 0x1000 regardless of the CBIN entry field.
; The linker must keep this trampoline at relative offset zero.
__mmu_start:
	jmp _mmu_entry
