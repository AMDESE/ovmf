;------------------------------------------------------------------------------
;
; Copyright (C) 2022, Advanced Micro Devices, Inc. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   VmgExitSvsm.Asm
;
; Abstract:
;
;   AsmVmgExitSvsm function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINT32
; EFIAPI
; AsmVmgExitSvsm (
;   UINT64  Rcx,
;   UINT64  Rdx,
;   UINT64  R8,
;   UINT64  R9,
;   UINT64  Rax
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmVmgExitSvsm)
ASM_PFX(AsmVmgExitSvsm):
;
; Calling convention puts Rcx/Rdx/R8/R9 arguments in the proper registers already.
; Only Rax needs to be loaded (from the stack).
;
    mov     rax, [rsp + 40]     ; Get Rax parameter from the stack
    rep     vmmcall
    ret

