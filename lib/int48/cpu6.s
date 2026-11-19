	.setcpu 6
	.code
	.export _u48_double
	.export _u48_div2
	.export _u48_mul
	.export _i48_mul
	.export _u48_divraw

; In-place multiply by two, modulo 2^48. Stores preserve the link flag.
; CPU6 function _u48_double begin
_u48_double:
	stx (-s)
	ldx 4(s)
	lda 4(x)
	sla
	sta 4(x)
	lda 2(x)
	rlr a
	sta 2(x)
	lda (x)
	rlr a
	sta (x)
	ldx (s+)
	rsr

; CPU6 function _u48_double end
; In-place unsigned division by two. Return the remainder in B.
; CPU6 function _u48_div2 begin
_u48_div2:
	stx (-s)
	ldx 4(s)
	lda (x)
	rl
	rrr a
	sta (x)
	lda 2(x)
	rrr a
	sta 2(x)
	lda 4(x)
	rrr a
	sta 4(x)
	ldb 0
	bnl div2_done
	inr b
div2_done:
	ldx (s+)
	rsr

; CPU6 function _u48_div2 end
; The low 48 bits of a product need six native 16-bit partial products.
; Signed and unsigned wrapping multiplication have identical bit results.
; CPU6 function _i48_mul begin
_i48_mul:
_u48_mul:
	stx (-s)
	xfr y,a
	sta (-s)
	xfr z,a
	sta (-s)
	lda 65530
	add a,s
	lda 16(s)
	xfr a,y
	lda 18(s)
	xfr a,z
	lda 4(y)
	ldb 4(z)
	mul b,a
	sta 2(s)
	stb 4(s)
	cla
	sta 0(s)
	; (a.mid * b.lo) << 16
	lda 2(y)
	ldb 4(z)
	mul b,a
	xfr a,x
	lda 2(s)
	aab
	stb 2(s)
	bnl cross1_no_carry
	inr x
cross1_no_carry:
	lda 0(s)
	add x,a
	sta 0(s)
	; (a.lo * b.mid) << 16
	lda 4(y)
	ldb 2(z)
	mul b,a
	xfr a,x
	lda 2(s)
	aab
	stb 2(s)
	bnl cross2_no_carry
	inr x
cross2_no_carry:
	lda 0(s)
	add x,a
	sta 0(s)
	; The three high-word terms only need their low 16 bits.
	lda 0(y)
	ldb 4(z)
	mul a,b
	lda 0(s)
	aab
	stb 0(s)
	lda 2(y)
	ldb 2(z)
	mul a,b
	lda 0(s)
	aab
	stb 0(s)
	lda 4(y)
	ldb 0(z)
	mul a,b
	lda 0(s)
	aab
	stb 0(s)
	; Delay output until all inputs have been read, allowing out == a/b.
	ldx 14(s)
	lda 0(s)
	sta (x)
	lda 2(s)
	sta 2(x)
	lda 4(s)
	sta 4(x)
	lda 6
	add a,s
	lda (s+)
	xfr a,z
	lda (s+)
	xfr a,y
	ldx (s+)
	rsr

; CPU6 function _i48_mul end
; Internal unsigned division with nonzero divisor and non-null outputs.
; Zero-extend each six-byte operand to eight bytes, so signed microcoded DRM
; also handles uint48_t values whose bit 47 is set. Scratch is private to S.
; Stack layout: numerator 0..7, divisor 8..15, remainder 16..23, saved X 24.
; CPU6 function _u48_divraw begin
_u48_divraw:
	stx (-s)
	lda 65512
	add a,s
	cla
	sta 0(s)
	sta 8(s)
	ldx 32(s)
	lda (x)
	sta 2(s)
	lda 2(x)
	sta 4(s)
	lda 4(x)
	sta 6(s)
	ldx 34(s)
	lda (x)
	sta 10(s)
	lda 2(x)
	sta 12(s)
	lda 4(x)
	sta 14(s)
	lda 16
	add s,a
	ldb 8
	add s,b
	xfr s,x
	rf
	; DRM 8-byte [B] into 8-byte [X], remainder at [A].
	.byte 0x46,0x77,0x7a,0x24
	ldx 28(s)
	lda 2(s)
	sta (x)
	lda 4(s)
	sta 2(x)
	lda 6(s)
	sta 4(x)
	ldx 30(s)
	lda 18(s)
	sta (x)
	lda 20(s)
	sta 2(x)
	lda 22(s)
	sta 4(x)
	lda 24
	add a,s
	ldx (s+)
	rsr
; CPU6 function _u48_divraw end
