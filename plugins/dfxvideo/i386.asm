; i386.asm  -  description
; -------------------
; begin                : Sun Nov 08 2001
; copyright            : (C) 2001 by Pete Bernert
; email                : BlackDove@addcom.de

; ported from inline gcc to nasm by linuzappz


; This program is free software; you can redistribute it and/or modify  *
; it under the terms of the GNU General Public License as published by  *
; the Free Software Foundation; either version 2 of the License, or     *
; (at your option) any later version. See also the license.txt file for *
; additional informations.                                              *


bits 32

section .text

%include "macros.inc"

NEWSYM i386_BGR24to16
	push ebp
	mov ebp, esp
	push ebx
	push edx
	
	mov eax, [ebp+8]            ; this can hold the G value
	mov ebx, eax                ; this can hold the R value
	mov edx, eax                ; this can hold the B value
	shr ebx, 3                  ; move the R value
	and edx, 00f80000h          ; mask the B value
	shr edx, 9                  ; move the B value
	and eax, 00f800h            ; mask the G value
	shr eax, 6                  ; move the G value
	and ebx, 0000001fh          ; mask the R value
	or  eax, ebx                ; add R to G value
	or  eax, edx                ; add B to RG value
	pop edx
	pop ebx
	mov esp, ebp
	pop ebp
	ret

NEWSYM i386_shl10idiv
	push ebp
	mov ebp, esp
	push ebx
	push edx

	mov eax, [ebp+8]
	mov ebx, [ebp+12]
	mov edx, eax
    shl eax, 10
	sar edx, 22
	idiv ebx

	pop edx
	pop ebx
	mov esp, ebp
	pop ebp
	ret
%ifidn __OUTPUT_FORMAT__,elf
section .note.GNU-stack noalloc noexec nowrite progbits
%endif

