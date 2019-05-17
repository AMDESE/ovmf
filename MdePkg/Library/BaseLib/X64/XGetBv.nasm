;------------------------------------------------------------------------------
;
; Copyright (c) 2018, Advanced Micro Device, Inc. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
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
; VOID
; EFIAPI
; AsmXGetBv (
;   IN UINT32 Index
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmXGetBv)
ASM_PFX(AsmXGetBv):
    xgetbv
    shl     rdx, 0x20
    or      rax, rdx
    ret
