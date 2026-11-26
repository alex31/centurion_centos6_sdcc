	.setcpu 6
	.code
	.export _os6_terminate
_os6_terminate:
	ldb 2(s)
	jmp __os6_finish
