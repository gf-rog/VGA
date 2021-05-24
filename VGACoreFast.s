			AREA test, CODE
VGA			PROC
			EXPORT VGA

			EXTERN scan_data
			EXTERN draw_scan	;Scanline data
			EXTERN palette
			
col			EQU 0x400

			;Push all the registers
			PUSH   {r1-r9}
			
_chars		RN r2				;r2 - character address
_palette	RN r3							;r3 - palette address
			LDR    _chars, =Chars
			LDR    _palette, =palette
			
_char_cols	RN r4							;r4 - character colors
			;_char_cols as temp register
			AND    _char_cols, r0, #7		;first 3 bits - vertical character offset
			ADD    _chars, _char_cols
			AND    r0, #0xfffffff8
			;Screen pointer in r0
			;Scanline pointer in r1
			
		
RepeatChar
_char_pix	RN r5							;r5 - character pixels
_pix_mask	RN r6							;r6 - pixel mask
			;_char_pix as temp register
			LDRB   _char_cols, [r0, #col]	;Get character color from screen
			LDRB   _char_pix, [r0], #0x01	;Get character offset from screen, next
			
			LDRB   _char_pix, [_chars, _char_pix, LSL #3]	;Load pixels
			MOV    _pix_mask, #0x80				;Pixel mask

_col_off2	RN r7		;r7 - 2nd color offset
_curr_col	RN r8		;r8 - current pixel color
			LSR    _col_off2, _char_cols, #4
			AND    _col_off2, #0x0F
			AND    _char_cols, #0x0F
			LDRB   _char_cols, [_palette, _char_cols]
			LDRB   _col_off2, [_palette, _col_off2]
RepeatPixel
			;_char_cols - 1st color, _col_off2 - 2nd color
			MOV    _curr_col, _char_cols
			TST    _char_pix, _pix_mask		;Get pixel value
			MOVEQ  _curr_col, _col_off2
			STRB   _curr_col, [r1], #1		;Store to draw, next
			
			LSRS   _pix_mask, #1
			BNE    RepeatPixel				;If not all 8 pixels, next
			
			TST    r1, #0xFF
			BNE    RepeatChar				;If not all characters, next
			
			;Put the registers back
			POP    {r1-r9}
			BX     lr
			ENDP
				
			LTORG
Chars						;Characters used
			INCBIN chars.bin
			END