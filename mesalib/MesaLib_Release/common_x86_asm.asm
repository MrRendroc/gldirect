











































































































































































































































































































































































































































































































































































































































































































































































































































































	
	
	
	
	
	




































































































































































































































































































































































































































































































































































































































































































































	
	
	
	
	
	
















































































































































































































































































































































































































































	SECTION .text

ALIGN 4
GLOBAL __mesa_x86_has_cpuid
__mesa_x86_has_cpuid:

	

	pushfd
	pop EAX
	mov ECX, EAX
	xor EAX, 0x00200000
	push EAX
	popfd
	pushfd
	pop EAX

	
	cmp EAX, ECX
	setne AL
	xor EAX, 0xff

	ret


ALIGN 4
GLOBAL __mesa_x86_cpuid
__mesa_x86_cpuid:

	mov EAX, dword [ESP + 4]		
	push EDI
	push EBX

	cpuid

	mov EDI, dword [ESP + 16]	
	mov dword [EDI], EAX
	mov EDI, dword [ESP + 20]	
	mov dword [EDI], EBX
	mov EDI, dword [ESP + 24]	
	mov dword [EDI], ECX
	mov EDI, dword [ESP + 28]	
	mov dword [EDI], EDX

	pop EBX
	pop EDI
	ret

ALIGN 4
GLOBAL __mesa_x86_cpuid_eax
__mesa_x86_cpuid_eax:

	mov EAX, dword [ESP + 4]		
	push EBX

	cpuid

	pop EBX
	ret

ALIGN 4
GLOBAL __mesa_x86_cpuid_ebx
__mesa_x86_cpuid_ebx:

	mov EAX, dword [ESP + 4]		
	push EBX

	cpuid
	mov EAX, EBX			

	pop EBX
	ret

ALIGN 4
GLOBAL __mesa_x86_cpuid_ecx
__mesa_x86_cpuid_ecx:

	mov EAX, dword [ESP + 4]		
	push EBX

	cpuid
	mov EAX, ECX			

	pop EBX
	ret

ALIGN 4
GLOBAL __mesa_x86_cpuid_edx
__mesa_x86_cpuid_edx:

	mov EAX, dword [ESP + 4]		
	push EBX

	cpuid
	mov EAX, EDX			

	pop EBX
	ret



























































