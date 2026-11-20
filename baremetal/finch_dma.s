    .setcpu 6
    .code
    .export _finch_dma_arm
    .export _finch_dma_stop

; Finch mode 3 lets the card select the direction and increments the address.
; The count is the one's complement of the byte length.
_finch_dma_arm:
    lda 2(s)
    stdma a
    lda 4(s)
    stdmac a
    dmamode 3
    dmaen
    rsr

_finch_dma_stop:
    .byte 0x2f,0x07 ; DDMA
    lddmac b
    rsr
