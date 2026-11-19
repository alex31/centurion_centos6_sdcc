	.setcpu 6
	.code
	.export _cpu6_flags
_cpu6_flags:
	stx (-s)
	; Word increment must test the whole word, including bit 15.
	ldb 255
	inr b
	bz failed
	bm failed
	ldb 32767
	inr b
	bp failed
	bnf failed
	ldb 65535
	inr b
	bnz failed
	; Loading zero preserves carry and overflow; CLR clears both.
	lda 32767
	ldb 1
	aab
	sl
	lda 0
	bnl failed
	bnf failed
	cla
	bl failed
	bf failed
	; Word memory loads and stores also report N/Z for the full word.
	ldb (flag_words)
	bp failed
	bz failed
	ldx flag_words
	ldb 2(x)
	bm failed
	bz failed
	ldb 4(x)
	bnz failed
	ldb 32768
	stb (flag_scratch)
	bp failed
	bz failed
	ldb 0
	ldx (s+)
	rsr
failed:
	ldb 1
	ldx (s+)
	rsr
	.data
flag_words:
	.word 32768,256,0
flag_scratch:
	.word 0
