; Harbour Walls for Veranda: the hot loops in assembly
;
; sprites_put: the buoys' sprite attributes (sprites 8-19, bytes 2-7)
; from the engine's bx, by, bkind, nballs, anchor
;
; (the flood fill is fill.s)

.setcpu "65C02"
.export _sprites_put
.import _bx, _by, _bkind, _nballs, _anchor
.importzp tmp1, tmp2

VERA_ADDR_L = $9F20
VERA_ADDR_M = $9F21
VERA_ADDR_H = $9F22
VERA_DATA0  = $9F23
VERA_CTRL   = $9F25

SPR_FIRST = 8
MAXBALLS  = 12
FIELD_PX  = 3 * 8               ; the field's top on the screen
PAL_FIRST = 2

; the test builds' bytes for the mouse scripts' "wait" (test.c sets them;
; here so that the map names their addresses)
.export _hw_sync, _hw_ready, _hw_lives, _hw_state, _hw_lvl
.segment "BSS"
_hw_sync:	.res 1
_hw_ready:	.res 1
_hw_lives:	.res 1
_hw_state:	.res 1
_hw_lvl:	.res 1

.segment "CODE"

_sprites_put:
	stz VERA_CTRL
	lda #$11                        ; $1xxxx, increment 1
	sta VERA_ADDR_H
	lda _anchor
	ora _anchor+1
	beq @free
	lda #3                          ; anchored: palette 3 + PAL_FIRST
@free:	sta tmp2                        ; 0: by class
	ldx #0                          ; buoy
@buoy:	txa                             ; attribute 2 of sprite SPR_FIRST + x
	asl
	asl
	asl
	clc
	adc #<($FC00 + 8 * SPR_FIRST + 2)
	sta VERA_ADDR_L
	lda #>($FC00 + 8 * SPR_FIRST + 2)
	adc #0
	sta VERA_ADDR_M
	cpx _nballs
	bcs @off
	txa
	asl
	tay
	lda _bx,y
	sta VERA_DATA0
	lda _bx+1,y
	sta VERA_DATA0
	lda _by,y
	clc
	adc #FIELD_PX
	sta VERA_DATA0
	lda _by+1,y
	adc #0
	sta VERA_DATA0
	lda #$0C                        ; in front of both layers
	sta VERA_DATA0
	lda tmp2
	bne @pal
	lda _bkind,x
@pal:	ora #$50 + PAL_FIRST            ; 16 x 16
	sta VERA_DATA0
	bra @next
@off:	lda VERA_DATA0                  ; (x, y as they are)
	lda VERA_DATA0
	lda VERA_DATA0
	lda VERA_DATA0
	stz VERA_DATA0                  ; off
@next:	inx
	cpx #MAXBALLS
	bne @buoy
	rts
