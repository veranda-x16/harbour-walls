; Harbour Walls for Veranda: the field's cards as map entries (the
; kernel's tile mode, Veranda's vr_pic: DECISIONS V105; tools/mkpic.py's
; pictures)
;
; map_run: the cards mr_x .. mr_x + mr_w - 1 of the screen's card rows
; mr_y .. mr_y + mr_h - 1, each tile mr_base + (y & 3) * 4 + (x & 3) (a
; kind's picture: 4 x 4 tiles, below 256) in palette row mr_row, straight
; into layer 0's map ($1AC00 + 256 y + 2 x). The game draws only while no
; menu or box covers a card (the kernel's covers write the map then).

.setcpu "65C02"
.export _map_run, _mr_x, _mr_y, _mr_w, _mr_h, _mr_base, _mr_row

VERA_ADDR_L = $9F20
VERA_ADDR_M = $9F21
VERA_ADDR_H = $9F22
VERA_DATA0  = $9F23
VERA_CTRL   = $9F25

.segment "BSS"
_mr_x:	.res 1
_mr_y:	.res 1
_mr_w:	.res 1
_mr_h:	.res 1
_mr_base: .res 1
_mr_row: .res 1                 ; the row << 4
t0:	.res 1

.segment "CODE"

_map_run:
	lda _mr_w
	beq @r
	lda _mr_h
	beq @r
@row:	stz VERA_CTRL
	lda _mr_x
	asl
	sta VERA_ADDR_L
	lda _mr_y
	clc
	adc #$AC
	sta VERA_ADDR_M
	lda #$11                ; $1xxxx, increment 1
	sta VERA_ADDR_H
	lda _mr_y               ; the picture's row: base + (y & 3) * 4
	and #3
	asl
	asl
	clc
	adc _mr_base
	sta t0
	lda _mr_x
	and #3
	tay
	ldx _mr_w
@c:	tya
	clc
	adc t0
	sta VERA_DATA0
	lda _mr_row
	sta VERA_DATA0
	iny
	tya
	and #3
	tay
	dex
	bne @c
	inc _mr_y
	dec _mr_h
	bne @row
@r:	rts
