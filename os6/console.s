	.setcpu 6
	.code
	.export _putchar

_putchar:
	stx (-s)
	lda 10
	ldb 4(s)
	sab
	bz newline
	ldb 4(s)
	ldab 128
	orib al,bl
	stbb (__oschar)
	; WRITEN(CRT, C1) character
	.byte 0x66,0x31
	.word __oscrt, __osfmt1, __oschar, 0
	bra done
newline:
	ldbb 0xa0
	stbb (__oschar)
	; WRITE terminates the current output line.
	.byte 0x66,0x35
	.word __oscrt, __osfmt1, __oschar, 0
done:
	ldb 4(s)
	ldx (s+)
	rsr

	.data
__oschar:
	.byte 0,0
