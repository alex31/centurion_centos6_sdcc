	.setcpu 6
	.code
	.export _stack_function
	.export _stack_forward

; JSR pushes the caller's X before the two arguments.
_stack_function:
	ldb 2(s)
	lda 4(s)
	aab
	lda 11
	aab
	rsr

; Preserve our return link and pass the arguments through the stack ABI.
_stack_forward:
	stx (-s)
	ldb 6(s)
	stb (-s)
	ldb 6(s)
	stb (-s)
	jsr _stack_bridge
	inr s,4
	ldx (s+)
	rsr
