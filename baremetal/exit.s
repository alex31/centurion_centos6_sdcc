	.setcpu 6
	.code
	.export _exit
_exit:
	hlt
	bra _exit
