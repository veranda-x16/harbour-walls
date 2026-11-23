; Harbour Walls for Veranda: the capture's flood fill (src/fill.c's
; fill_run, in assembly; the host tests check the C, the gates check that
; the X16's game equals the host's step by step).
;
; u8 __fastcall__ fill_run(u16 budget): from the seeds on the stack
; (fsx, fsy, fsp) mark every free card (grid value below C_WALL: sea,
; crates, growing arms, not marked yet) reachable; after about budget
; cards return 0, when done 1. A seed that finds the stack full sets
; fill_lostf; fill_rescan (C) finds such places again when it runs dry.

.setcpu "65C02"
.export _fill_run, _paint_scan
.import _prow, _grid, _fsx, _fsy, _fsp, _fill_lostf, _fill_rescan
.importzp ptr1, ptr2

GW      = 80
GH      = 53
C_WALL  = 4
MARK    = $80
FSTACK  = 255

.segment "RODATA"
rowlo:	.repeat GH, i
	.lobytes _grid + i * GW
	.endrep
rowhi:	.repeat GH, i
	.hibytes _grid + i * GW
	.endrep

.segment "BSS"
budget:	.res 2
cx:	.res 1
cy:	.res 1
xl:	.res 1
xr:	.res 1
in:	.res 1
sr:	.res 1                  ; the seeds' row

.segment "CODE"

_fill_run:
	sta budget
	stx budget+1
@loop:	ldx _fsp
	bne @pop
	lda _fill_lostf
	beq @done
	jsr _fill_rescan
	bra @loop
@done:	lda #1
	ldx #0
	rts
@pop:	dex
	stx _fsp
	ldy _fsy,x
	sty cy
	lda _fsx,x
	sta cx
	lda rowlo,y
	sta ptr1
	lda rowhi,y
	sta ptr1+1
	ldy cx
	lda (ptr1),y
	cmp #C_WALL
	bcs @again              ; solid, or marked already
@left:	dey
	lda (ptr1),y
	cmp #C_WALL
	bcc @left
	iny
	sty xl
	ldy cx
@right:	iny
	lda (ptr1),y
	cmp #C_WALL
	bcc @right
	dey
	sty xr
@mark:	lda (ptr1),y            ; xr down to xl
	ora #MARK
	sta (ptr1),y
	dey
	cpy xl
	bcs @mark
	ldy cy
	dey
	jsr seeds
	ldy cy
	iny
	jsr seeds
	; budget -= xr - xl + 1; done for now when it runs out
	lda xr
	sec
	sbc xl
	sec                     ; + 1
	adc #0
	sta cx                  ; the run's length
	lda budget+1
	bne @sub
	lda cx
	cmp budget
	bcs @out                ; budget <= length
@sub:	lda budget
	sec
	sbc cx
	sta budget
	bcs @again
	dec budget+1
@again:	jmp @loop
@out:	lda #0
	tax
	rts

; the runs of free cards in row y, xl..xr: a seed each
seeds:	sty sr
	lda rowlo,y
	sta ptr2
	lda rowhi,y
	sta ptr2+1
	stz in
	ldy xl
@s:	lda (ptr2),y
	cmp #C_WALL
	bcs @solid
	lda in
	bne @next
	inc in
	ldx _fsp                ; push (y, row)
	cpx #FSTACK
	beq @full
	tya
	sta _fsx,x
	lda sr
	sta _fsy,x
	inc _fsp
	bra @next
@full:	lda #1
	sta _fill_lostf
	bra @next
@solid:	stz in
@next:	cpy xr
	iny
	bcc @s
	rts

; u8 __fastcall__ paint_scan(u8 x): in row prow from x on, the marks
; cleared up to the next free card not marked (sea, a crate: below
; C_ARMA); its column, or GW - 1
C_ARMA = 2
_paint_scan:
	tay
	lda _prow
	sta ptr1
	lda _prow+1
	sta ptr1+1
@l:	cpy #GW - 1
	bcs @end
	lda (ptr1),y
	bmi @mark
	cmp #C_ARMA
	bcc @end
	iny
	bra @l
@mark:	and #$7F
	sta (ptr1),y
	iny
	bra @l
@end:	tya
	ldx #0
	rts
