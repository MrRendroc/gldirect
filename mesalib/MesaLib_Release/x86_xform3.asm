
































































































































































































































































































































































































































































































































































































































































































































































































































































	
	
	
	
	
	




































































































































































































































































































































































































































































































































































































































































































































	
	
	
	
	
	
































































































































































































































































































































































































































	SECTION .text






























ALIGN 16
GLOBAL __mesa_x86_transform_points3_general
__mesa_x86_transform_points3_general:


	push ESI
	push EDI

	mov ESI, dword [ESP + 8+12]
	mov EDI, dword [ESP + 8+4]

	mov EDX, dword [ESP + 8+8]
	mov ECX, dword [ESI + 8]

	test ECX, ECX
	jz near x86_p3_gr_done

	mov EAX, dword [ESI + 12]
	or dword [EDI + 20], 0xf

	mov dword [EDI + 8], ECX
	mov dword [EDI + 16], 4

	shl ECX, 4
	mov ESI, dword [ESI + 4]

	mov EDI, dword [EDI + 4]
	add ECX, EDI

ALIGN 16
x86_p3_gr_loop:

	fld dword [ESI + 0]			
	fmul dword [EDX + 0]
	fld dword [ESI + 0]			
	fmul dword [EDX + 4]
	fld dword [ESI + 0]			
	fmul dword [EDX + 8]
	fld dword [ESI + 0]			
	fmul dword [EDX + 12]

	fld dword [ESI + 4]			
	fmul dword [EDX + 16]
	fld dword [ESI + 4]			
	fmul dword [EDX + 20]
	fld dword [ESI + 4]			
	fmul dword [EDX + 24]
	fld dword [ESI + 4]			
	fmul dword [EDX + 28]

	fxch st3			
	faddp st7, st0		
	fxch st1			
	faddp st5, st0		
	faddp st3, st0		
	faddp st1, st0		

	fld dword [ESI + 8]			
	fmul dword [EDX + 32]
	fld dword [ESI + 8]			
	fmul dword [EDX + 36]
	fld dword [ESI + 8]			
	fmul dword [EDX + 40]
	fld dword [ESI + 8]			
	fmul dword [EDX + 44]

	fxch st3			
	faddp st7, st0		
	fxch st1			
	faddp st5, st0		
	faddp st3, st0		
	faddp st1, st0		

	fxch st3			
	fadd dword [EDX + 48]
	fxch st2			
	fadd dword [EDX + 52]
	fxch st1			
	fadd dword [EDX + 56]
	fxch st3			
	fadd dword [EDX + 60]

	fxch st2			
	fstp dword [EDI + 0]		
	fstp dword [EDI + 4]		
	fxch st1			
	fstp dword [EDI + 8]		
	fstp dword [EDI + 12]		

x86_p3_gr_skip:

	add EDI, 16
	add ESI, EAX
	cmp EDI, ECX
	jne near x86_p3_gr_loop

x86_p3_gr_done:

	pop EDI
	pop ESI
	ret





ALIGN 16
GLOBAL __mesa_x86_transform_points3_perspective
__mesa_x86_transform_points3_perspective:


	push ESI
	push EDI
	push EBX

	mov ESI, dword [ESP + 12+12]
	mov EDI, dword [ESP + 12+4]

	mov EDX, dword [ESP + 12+8]
	mov ECX, dword [ESI + 8]

	test ECX, ECX
	jz near x86_p3_pr_done

	mov EAX, dword [ESI + 12]
	or dword [EDI + 20], 0xf

	mov dword [EDI + 8], ECX
	mov dword [EDI + 16], 4

	shl ECX, 4
	mov ESI, dword [ESI + 4]

	mov EDI, dword [EDI + 4]
	add ECX, EDI

ALIGN 16
x86_p3_pr_loop:

	fld dword [ESI + 0]			
	fmul dword [EDX + 0]

	fld dword [ESI + 4]			
	fmul dword [EDX + 20]

	fld dword [ESI + 8]			
	fmul dword [EDX + 32]
	fld dword [ESI + 8]			
	fmul dword [EDX + 36]
	fld dword [ESI + 8]			
	fmul dword [EDX + 40]

	fxch st2			
	faddp st4, st0		
	faddp st2, st0		
	fld dword [EDX + 56]		
	fxch st1			
	faddp st1, st0		

	mov EBX, dword [ESI + 8]
	xor EBX, -2147483648

	fxch st2			
	fstp dword [EDI + 0]		
	fstp dword [EDI + 4]		
	fstp dword [EDI + 8]		
	mov dword [EDI + 12], EBX

x86_p3_pr_skip:

	add EDI, 16
	add ESI, EAX
	cmp EDI, ECX
	jne near x86_p3_pr_loop

x86_p3_pr_done:

	pop EBX
	pop EDI
	pop ESI
	ret





ALIGN 16
GLOBAL __mesa_x86_transform_points3_3d
__mesa_x86_transform_points3_3d:


	push ESI
	push EDI

	mov ESI, dword [ESP + 8+12]
	mov EDI, dword [ESP + 8+4]

	mov EDX, dword [ESP + 8+8]
	mov ECX, dword [ESI + 8]

	test ECX, ECX
	jz near x86_p3_3dr_done

	mov EAX, dword [ESI + 12]
	or dword [EDI + 20], 0x7

	mov dword [EDI + 8], ECX
	mov dword [EDI + 16], 3

	shl ECX, 4
	mov ESI, dword [ESI + 4]

	mov EDI, dword [EDI + 4]
	add ECX, EDI

ALIGN 16
x86_p3_3dr_loop:

	fld dword [ESI + 0]			
	fmul dword [EDX + 0]
	fld dword [ESI + 0]			
	fmul dword [EDX + 4]
	fld dword [ESI + 0]			
	fmul dword [EDX + 8]

	fld dword [ESI + 4]			
	fmul dword [EDX + 16]
	fld dword [ESI + 4]			
	fmul dword [EDX + 20]
	fld dword [ESI + 4]			
	fmul dword [EDX + 24]

	fxch st2			
	faddp st5, st0		
	faddp st3, st0		
	faddp st1, st0		

	fld dword [ESI + 8]			
	fmul dword [EDX + 32]
	fld dword [ESI + 8]			
	fmul dword [EDX + 36]
	fld dword [ESI + 8]			
	fmul dword [EDX + 40]

	fxch st2			
	faddp st5, st0		
	faddp st3, st0		
	faddp st1, st0		

	fxch st2			
	fadd dword [EDX + 48]
	fxch st1			
	fadd dword [EDX + 52]
	fxch st2			
	fadd dword [EDX + 56]

	fxch st1			
	fstp dword [EDI + 0]		
	fxch st1			
	fstp dword [EDI + 4]		
	fstp dword [EDI + 8]		

x86_p3_3dr_skip:

	add EDI, 16
	add ESI, EAX
	cmp EDI, ECX
	jne near x86_p3_3dr_loop

x86_p3_3dr_done:

	pop EDI
	pop ESI
	ret





ALIGN 16
GLOBAL __mesa_x86_transform_points3_3d_no_rot
__mesa_x86_transform_points3_3d_no_rot:


	push ESI
	push EDI

	mov ESI, dword [ESP + 8+12]
	mov EDI, dword [ESP + 8+4]


	mov EDX, dword [ESP + 8+8]
	mov ECX, dword [ESI + 8]

	test ECX, ECX
	jz near x86_p3_3dnrr_done

	mov EAX, dword [ESI + 12]
	or dword [EDI + 20], 0x7

	mov dword [EDI + 8], ECX
	mov dword [EDI + 16], 3

	shl ECX, 4
	mov ESI, dword [ESI + 4]

	mov EDI, dword [EDI + 4]
	add ECX, EDI

ALIGN 16
x86_p3_3dnrr_loop:

	fld dword [ESI + 0]			
	fmul dword [EDX + 0]

	fld dword [ESI + 4]			
	fmul dword [EDX + 20]

	fld dword [ESI + 8]			
	fmul dword [EDX + 40]

	fxch st2			
	fadd dword [EDX + 48]
	fld dword [EDX + 52]		
	fxch st2			
	faddp st2, st0		
	fld dword [EDX + 56]		
	fxch st3			
	faddp st3, st0		

	fstp dword [EDI + 0]		
	fstp dword [EDI + 4]		
	fstp dword [EDI + 8]		

x86_p3_3dnrr_skip:

	add EDI, 16
	add ESI, EAX
	cmp EDI, ECX
	jne near x86_p3_3dnrr_loop

x86_p3_3dnrr_done:

	pop EDI
	pop ESI
	ret





ALIGN 16
GLOBAL __mesa_x86_transform_points3_2d
__mesa_x86_transform_points3_2d:


	push ESI
	push EDI
	push EBX

	mov ESI, dword [ESP + 12+12]
	mov EDI, dword [ESP + 12+4]

	mov EDX, dword [ESP + 12+8]
	mov ECX, dword [ESI + 8]

	test ECX, ECX
	jz near x86_p3_2dr_done

	mov EAX, dword [ESI + 12]
	or dword [EDI + 20], 0x7

	mov dword [EDI + 8], ECX
	mov dword [EDI + 16], 3

	shl ECX, 4
	mov ESI, dword [ESI + 4]

	mov EDI, dword [EDI + 4]
	add ECX, EDI

ALIGN 16
x86_p3_2dr_loop:

	fld dword [ESI + 0]			
	fmul dword [EDX + 0]
	fld dword [ESI + 0]			
	fmul dword [EDX + 4]

	fld dword [ESI + 4]			
	fmul dword [EDX + 16]
	fld dword [ESI + 4]			
	fmul dword [EDX + 20]

	fxch st1			
	faddp st3, st0		
	faddp st1, st0		

	fxch st1			
	fadd dword [EDX + 48]
	fxch st1			
	fadd dword [EDX + 52]

	mov EBX, dword [ESI + 8]

	fxch st1			
	fstp dword [EDI + 0]		
	fstp dword [EDI + 4]		
	mov dword [EDI + 8], EBX

x86_p3_2dr_skip:

	add EDI, 16
	add ESI, EAX
	cmp EDI, ECX
	jne near x86_p3_2dr_loop

x86_p3_2dr_done:

	pop EBX
	pop EDI
	pop ESI
	ret





ALIGN 16
GLOBAL __mesa_x86_transform_points3_2d_no_rot
__mesa_x86_transform_points3_2d_no_rot:


	push ESI
	push EDI
	push EBX

	mov ESI, dword [ESP + 12+12]
	mov EDI, dword [ESP + 12+4]

	mov EDX, dword [ESP + 12+8]
	mov ECX, dword [ESI + 8]

	test ECX, ECX
	jz near x86_p3_2dnrr_done

	mov EAX, dword [ESI + 12]
	or dword [EDI + 20], 0x7

	mov dword [EDI + 8], ECX
	mov dword [EDI + 16], 3

	shl ECX, 4
	mov ESI, dword [ESI + 4]

	mov EDI, dword [EDI + 4]
	add ECX, EDI

ALIGN 16
x86_p3_2dnrr_loop:

	fld dword [ESI + 0]			
	fmul dword [EDX + 0]

	fld dword [ESI + 4]			
	fmul dword [EDX + 20]

	fxch st1			
	fadd dword [EDX + 48]
	fld dword [EDX + 52]		

	fxch st2			
	faddp st2, st0		

	mov EBX, dword [ESI + 8]

	fstp dword [EDI + 0]		
	fstp dword [EDI + 4]		
	mov dword [EDI + 8], EBX

x86_p3_2dnrr_skip:

	add EDI, 16
	add ESI, EAX
	cmp EDI, ECX
	jne near x86_p3_2dnrr_loop

x86_p3_2dnrr_done:

	pop EBX
	pop EDI
	pop ESI
	ret





ALIGN 16
GLOBAL __mesa_x86_transform_points3_identity
__mesa_x86_transform_points3_identity:


	push ESI
	push EDI
	push EBX
	push EBP

	mov ESI, dword [ESP + 16+12]
	mov EDI, dword [ESP + 16+4]

	mov EDX, dword [ESP + 16+8]
	mov ECX, dword [ESI + 8]

	test ECX, ECX
	jz near x86_p3_ir_done

	mov EAX, dword [ESI + 12]
	or dword [EDI + 20], 0x7

	mov dword [EDI + 8], ECX
	mov dword [EDI + 16], 3

	shl ECX, 4
	mov ESI, dword [ESI + 4]

	mov EDI, dword [EDI + 4]
	add ECX, EDI

	cmp EDI, ESI
	je near x86_p3_ir_done

ALIGN 16
x86_p3_ir_loop:


	mov EBX, dword [ESI + 0]
	mov EBP, dword [ESI + 4]
	mov EDX, dword [ESI + 8]

	mov dword [EDI + 0], EBX
	mov dword [EDI + 4], EBP
	mov dword [EDI + 8], EDX










x86_p3_ir_skip:

	add EDI, 16
	add ESI, EAX
	cmp EDI, ECX
	jne near x86_p3_ir_loop

x86_p3_ir_done:

	pop EBP
	pop EBX
	pop EDI
	pop ESI
	ret
