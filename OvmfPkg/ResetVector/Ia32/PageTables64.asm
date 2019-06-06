;------------------------------------------------------------------------------
; @file
; Sets the CR3 register for 64-bit paging
;
; Copyright (c) 2008 - 2013, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
;------------------------------------------------------------------------------

BITS    32

%define PAGE_PRESENT            0x01
%define PAGE_READ_WRITE         0x02
%define PAGE_USER_SUPERVISOR    0x04
%define PAGE_WRITE_THROUGH      0x08
%define PAGE_CACHE_DISABLE     0x010
%define PAGE_ACCESSED          0x020
%define PAGE_DIRTY             0x040
%define PAGE_PAT               0x080
%define PAGE_GLOBAL           0x0100
%define PAGE_2M_MBO            0x080
%define PAGE_2M_PAT          0x01000

%define PAGE_2M_PDE_ATTR (PAGE_2M_MBO + \
                          PAGE_ACCESSED + \
                          PAGE_DIRTY + \
                          PAGE_READ_WRITE + \
                          PAGE_PRESENT)

%define PAGE_PDP_ATTR (PAGE_ACCESSED + \
                       PAGE_READ_WRITE + \
                       PAGE_PRESENT)

; Check if Secure Encrypted Virtualization (SEV) feature is enabled
;
; If SEV is enabled then EAX will be at least 32
; If SEV is disabled then EAX will be zero.
;
CheckSevFeature:
    ;
    ; Set up exception handlers to check for SEV-ES
    ;   Load temporary RAM stack based on PCDs
    ;   Establish exception handlers
    ;
    mov       esp, SEV_TOP_OF_STACK
    mov       eax, ADDR_OF(idtr)
    lidt      [cs:eax]

    ; Check if we have a valid (0x8000_001F) CPUID leaf
    mov       eax, 0x80000000
    cpuid

    ; This check should fail on Intel or Non SEV AMD CPUs. In future if
    ; Intel CPUs supports this CPUID leaf then we are guranteed to have exact
    ; same bit definition.
    cmp       eax, 0x8000001f
    jl        NoSev

    ; Check for memory encryption feature:
    ;  CPUID  Fn8000_001F[EAX] - Bit 1
    ;
    mov       eax,  0x8000001f
    cpuid
    bt        eax, 1
    jnc       NoSev

    ; Check if memory encryption is enabled
    ;  MSR_0xC0010131 - Bit 0 (SEV enabled)
    mov       ecx, 0xc0010131
    rdmsr
    bt        eax, 0
    jnc       NoSev

    ; Get pte bit position to enable memory encryption
    ; CPUID Fn8000_001F[EBX] - Bits 5:0
    ;
    mov       eax, ebx
    and       eax, 0x3f
    jmp       SevExit

NoSev:
    xor       eax, eax

SevExit:
    ;
    ; Clear exception handlers and stack
    ;
    push      eax
    mov       eax, ADDR_OF(idtr_clear)
    lidt      [cs:eax]
    pop       eax
    mov       esp, 0

    OneTimeCallRet CheckSevFeature

;
; Modified:  EAX, EBX, ECX, EDX
;
SetCr3ForPageTables64:

    OneTimeCall   CheckSevFeature
    xor     edx, edx
    test    eax, eax
    jz      SevNotActive

    ; If SEV is enabled, C-bit is always above 31
    sub     eax, 32
    bts     edx, eax

SevNotActive:

    ;
    ; For OVMF, build some initial page tables at
    ; PcdOvmfSecPageTablesBase - (PcdOvmfSecPageTablesBase + 0x6000).
    ;
    ; This range should match with PcdOvmfSecPageTablesSize which is
    ; declared in the FDF files.
    ;
    ; At the end of PEI, the pages tables will be rebuilt into a
    ; more permanent location by DxeIpl.
    ;

    mov     ecx, 6 * 0x1000 / 4
    xor     eax, eax
clearPageTablesMemoryLoop:
    mov     dword[ecx * 4 + PT_ADDR (0) - 4], eax
    loop    clearPageTablesMemoryLoop

    ;
    ; Top level Page Directory Pointers (1 * 512GB entry)
    ;
    mov     dword[PT_ADDR (0)], PT_ADDR (0x1000) + PAGE_PDP_ATTR
    mov     dword[PT_ADDR (4)], edx

    ;
    ; Next level Page Directory Pointers (4 * 1GB entries => 4GB)
    ;
    mov     dword[PT_ADDR (0x1000)], PT_ADDR (0x2000) + PAGE_PDP_ATTR
    mov     dword[PT_ADDR (0x1004)], edx
    mov     dword[PT_ADDR (0x1008)], PT_ADDR (0x3000) + PAGE_PDP_ATTR
    mov     dword[PT_ADDR (0x100C)], edx
    mov     dword[PT_ADDR (0x1010)], PT_ADDR (0x4000) + PAGE_PDP_ATTR
    mov     dword[PT_ADDR (0x1014)], edx
    mov     dword[PT_ADDR (0x1018)], PT_ADDR (0x5000) + PAGE_PDP_ATTR
    mov     dword[PT_ADDR (0x101C)], edx

    ;
    ; Page Table Entries (2048 * 2MB entries => 4GB)
    ;
    mov     ecx, 0x800
pageTableEntriesLoop:
    mov     eax, ecx
    dec     eax
    shl     eax, 21
    add     eax, PAGE_2M_PDE_ATTR
    mov     [ecx * 8 + PT_ADDR (0x2000 - 8)], eax
    mov     [(ecx * 8 + PT_ADDR (0x2000 - 8)) + 4], edx
    loop    pageTableEntriesLoop

    ;
    ; Set CR3 now that the paging structures are available
    ;
    mov     eax, PT_ADDR (0)
    mov     cr3, eax

    OneTimeCallRet SetCr3ForPageTables64

SevEsIdtCommon:
    hlt
    jmp     SevEsIdtCommon
    iret

SevEsIdtVmmComm:
    ;
    ; If we're here, then we are an SEV-ES guest and this
    ; was triggered by a CPUID instruction
    ;
    pop     ecx                     ; Error code
    cmp     ecx, 0x72               ; Be sure it was CPUID
    jne     SevEsIdtCommon

    ;
    ; Set up local variable room on the stack
    ;   CPUID function      : + 28
    ;   CPUID register      : + 24
    ;   GHCB MSR (EAX)      : + 20
    ;   GHCB MSR (EDX)      : + 16
    ;   CPUID result (EDX)  : + 12
    ;   CPUID result (ECX)  : + 8
    ;   CPUID result (EBX)  : + 4
    ;   CPUID result (EAX)  : + 0
    sub     esp, 32

    ; Save CPUID function and initial register request
    mov     [esp + 28], eax
    xor     eax, eax
    mov     [esp + 24], eax

    ; Save current GHCB MSR value
    mov     ecx, 0xc0010130
    rdmsr
    mov     [esp + 20], eax
    mov     [esp + 16], edx

NextReg:
    ;
    ; Setup GHCB MSR
    ;   GHCB_MSR[63:32] = CPUID function
    ;   GHCB_MSR[31:30] = CPUID register
    ;   GHCB_MSR[11:0]  = CPUID request protocol
    ;
    mov     eax, [esp + 24]
    cmp     eax, 4
    jge     VmmDone

    shl     eax, 30
    or      eax, 0x004
    mov     edx, [esp + 28]
    mov     ecx, 0xc0010130
    wrmsr

    ; Issue VMGEXIT (rep; vmmcall)
    db      0xf3
    db      0x0f
    db      0x01
    db      0xd9

    ;
    ; Read GHCB MSR
    ;   GHCB_MSR[63:32] = CPUID register value
    ;   GHCB_MSR[31:30] = CPUID register
    ;   GHCB_MSR[11:0]  = CPUID response protocol
    ;
    mov     ecx, 0xc0010130
    rdmsr
    mov     ecx, eax
    and     ecx, 0xfff
    cmp     ecx, 0x005
    jne     SevEsIdtCommon

    ; Save returned value
    shr     eax, 30
    and     eax, 0x3
    shl     eax, 2
    mov     ecx, esp
    add     ecx, eax
    mov     [ecx], edx

    ; Next register
    inc     word [esp + 24]

    jmp     NextReg

VmmDone:
    ;
    ; At this point we have all CPUID register values. Restore the GHCB MSR,
    ; set the return register values and return.
    ;
    mov     eax, [esp + 20]
    mov     edx, [esp + 16]
    mov     ecx, 0xc0010130
    wrmsr

    mov     eax, [esp + 0]
    mov     ebx, [esp + 4]
    mov     ecx, [esp + 8]
    mov     edx, [esp + 12]

    add     esp, 32
    add     word [esp], 2           ; Skip over the CPUID instruction
    iret

ALIGN   2

idtr:
    dw      IDT_END - IDT_BASE - 1  ; Limit
    dd      ADDR_OF(IDT_BASE)       ; Base

idtr_clear:
    dw      0                       ; Limit
    dd      0                       ; Base

ALIGN   16

;
; The Interrupt Descriptor Table (IDT)
;   This will be used to determine if SEV-ES is enabled.  Upon execution
;   of the CPUID instruction, a VMM Communication Exception will occur.
;   This will tell us if SEV-ES is enabled.  We can use the current value
;   of the GHCB MSR to determine the SEV attributes.
;
IDT_BASE:
;
; Vectors 0 - 28
;
%rep 29
    dw      (ADDR_OF(SevEsIdtCommon) & 0xffff)   ; Offset low bits 15..0
    dw      0x10                                 ; Selector
    db      0                                    ; Reserved
    db      0x8E                                 ; Gate Type (IA32_IDT_GATE_TYPE_INTERRUPT_32)
    dw      (ADDR_OF(SevEsIdtCommon) >> 16)      ; Offset high bits 31..16
%endrep
;
; Vector 29 (VMM Communication Exception)
;
    dw      (ADDR_OF(SevEsIdtVmmComm) & 0xffff)  ; Offset low bits 15..0
    dw      0x10                                 ; Selector
    db      0                                    ; Reserved
    db      0x8E                                 ; Gate Type (IA32_IDT_GATE_TYPE_INTERRUPT_32)
    dw      (ADDR_OF(SevEsIdtVmmComm) >> 16)     ; Offset high bits 31..16
;
; Vectors 30 - 31
;
%rep 2
    dw      (ADDR_OF(SevEsIdtCommon) & 0xffff)   ; Offset low bits 15..0
    dw      0x10                                 ; Selector
    db      0                                    ; Reserved
    db      0x8E                                 ; Gate Type (IA32_IDT_GATE_TYPE_INTERRUPT_32)
    dw      (ADDR_OF(SevEsIdtCommon) >> 16)      ; Offset high bits 31..16
%endrep
IDT_END:
