;------------------------------------------------------------------------------
;
; Copyright (c) 2019, Advanced Micro Device, Inc. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   XGetBv.Asm
;
; Abstract:
;
;   AsmXgetBv function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; AsmXGetBv (
;   IN UINT32  Index
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmXGetBv)
ASM_PFX(AsmXGetBv):
    xgetbv
    shl     rdx, 0x20
    or      rax, rdx
    ret

