			area transfer, code
Transfer    PROC
			
			EXPORT Transfer
			EXTERN draw_scan
			EXTERN scan_data
			
			PUSH   {r0-r9}
			LDR    r0, =draw_scan
			LDR    r1, =scan_data
Trans
			LDM    r0!, {r2-r9}
			STM    r1!, {r2-r9}
			TST    r0, #0xFF
			BNE    Trans
			
			;Put the registers back
			POP    {r0-r9}
			BX     lr
			ENDP
			END