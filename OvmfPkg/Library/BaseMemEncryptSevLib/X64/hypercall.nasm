DEFAULT REL
SECTION .text

; INTN
; EFIAPI
; SetMemoryEncDecHypercall3 (
;   IN INTN Arg1,
;   IN INTN Arg2,
;   IN INTN Arg3
;   );
global ASM_PFX(SetMemoryEncDecHypercall3)
ASM_PFX(SetMemoryEncDecHypercall3):
  ; Copy HypercallNumber to rax
  mov rax, 11
  ; Copy Arg1 to the register expected by KVM
  mov rbx, rcx
  ; Copy Arg2 to register expected by KVM
  mov rcx, rdx
  ; Copy Arg2 to register expected by KVM
  mov rdx, r8
  ; Call VMMCALL
  DB 0x0F, 0x01, 0xD9
  ret

