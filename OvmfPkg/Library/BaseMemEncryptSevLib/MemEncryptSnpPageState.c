#include "MemEncryptSnpPageState.h"

#define GHCB_INFO_MASK          0xfff
#define IS_ALIGNED(x, y)        ((((x) & (y - 1)) == 0))
#define EFI_LARGE_PAGE          (EFI_PAGE_SIZE * 512)

/**
 The function return boolean indicating GHCB is in protocol mode.
 */
STATIC
BOOLEAN
GhcbIsProtoMode (
  VOID
  )
{
  MSR_SEV_ES_GHCB_REGISTER        Msr;

  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);

  if ((Msr.GhcbPhysicalAddress & GHCB_INFO_MASK) != 0) {
    return TRUE;
  }

  return FALSE;
}

/**
 Maps the our MEM_OP_REQ type to GHCB Protocal Page operation command.
 */
STATIC
UINTN
MemoryTypeToGhcbProtoOp (
  IN MEM_OP_REQ             Type
  )
{
  UINTN Cmd;

  switch (Type) {
    case MemoryTypeShared: Cmd = SNP_PAGE_STATE_SHARED; break;
    case MemoryTypePrivate: Cmd = SNP_PAGE_STATE_PRIVATE; break;
    default: ASSERT(0);
  }

  return Cmd;
}

/**
 Maps the our MEM_OP_REQ type to GHCB MEM_OP command.
 */
STATIC
UINTN
MemoryTypeToGhcbOp (
  IN MEM_OP_REQ             Type
  )
{
  UINTN Cmd;

  switch (Type) {
    case MemoryTypeShared: Cmd = SNP_PAGE_STATE_SHARED; break;
    case MemoryTypePrivate: Cmd = SNP_PAGE_STATE_PRIVATE; break;
    default: ASSERT(0);
  }

  return Cmd;
}


STATIC
VOID
SnpPageStateProtocolCheck (
  VOID
  )
{
  MSR_SEV_ES_GHCB_REGISTER  Msr;

  //
  // Read the page state response code.
  //
  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);
  if (Msr.SnpPageStateChangeResponse.Function == GHCB_INFO_SNP_PAGE_STATE_CHANGE_RESPONSE) {
    if (Msr.SnpPageStateChangeResponse.ErrorCode == 0) {
      return;
    }
  }

  //
  // Use the GHCB MSR Protocol to request termination by the hypervisor
  //
  Msr.GhcbPhysicalAddress = 0;
  Msr.GhcbTerminate.Function = GHCB_INFO_TERMINATE_REQUEST;
  Msr.GhcbTerminate.ReasonCodeSet = GHCB_TERMINATE_GHCB;
  Msr.GhcbTerminate.ReasonCode = GHCB_TERMINATE_GHCB_GENERAL;
  AsmWriteMsr64 (MSR_SEV_ES_GHCB, Msr.GhcbPhysicalAddress);

  AsmVmgExit ();

  ASSERT (FALSE);
  CpuDeadLoop ();
}

STATIC
RETURN_STATUS
ChangePageStateProto (
  IN EFI_PHYSICAL_ADDRESS         BaseAddress,
  IN UINTN                        NumPages,
  IN MEM_OP_REQ                   Type
  )
{
  EFI_PHYSICAL_ADDRESS            EndAddress, NextAddress;
  MSR_SEV_ES_GHCB_REGISTER        Msr;
  MSR_SEV_ES_GHCB_REGISTER        CurrentMsr;

  //
  // Save the current MSR Value
  //
  CurrentMsr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);

  EndAddress = BaseAddress + (NumPages * EFI_PAGE_SIZE);

  for (; EndAddress > BaseAddress; BaseAddress = NextAddress) {
    Msr.GhcbPhysicalAddress = 0;
    Msr.SnpPageStateChangeRequest.GuestFrameNumber = BaseAddress >> EFI_PAGE_SHIFT;
    Msr.SnpPageStateChangeRequest.Operation = MemoryTypeToGhcbProtoOp(Type);
    Msr.SnpPageStateChangeRequest.Function = GHCB_INFO_SNP_PAGE_STATE_CHANGE_REQUEST;

    AsmWriteMsr64 (MSR_SEV_ES_GHCB, Msr.GhcbPhysicalAddress);

    AsmVmgExit ();

    SnpPageStateProtocolCheck ();

    NextAddress = BaseAddress + EFI_PAGE_SIZE;
  }

  //
  // Restore the MSR
  //
  AsmWriteMsr64 (MSR_SEV_ES_GHCB, CurrentMsr.GhcbPhysicalAddress);

  return EFI_SUCCESS;
}

/**
 The function request setting the page state in the RMP table through the VMGEXIT.
 */
RETURN_STATUS
SetPageStateInternal (
  IN EFI_PHYSICAL_ADDRESS         BaseAddress,
  IN UINTN                        NumPages,
  IN MEM_OP_REQ                   Type
  )
{
  UINTN                           i;
  EFI_STATUS                      Status;
  GHCB                            *Ghcb;
  EFI_PHYSICAL_ADDRESS            NextAddress, EndAddress;
  MSR_SEV_ES_GHCB_REGISTER        Msr;
  BOOLEAN                         InterruptState;

  //
  // If we don't have a valid GHCB then use the GHCB protocol to change page state.
  //
  if (GhcbIsProtoMode ()) {
    return ChangePageStateProto (BaseAddress, NumPages, Type);
  }

  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);
  Ghcb = Msr.Ghcb;

  EndAddress = BaseAddress + EFI_PAGES_TO_SIZE (NumPages);

  for (; EndAddress > BaseAddress; BaseAddress = NextAddress) {
    SNP_PAGE_STATE_CHANGE_INFO      *Info;
    UINTN                            RmpPageSize;

    //
    // Initialize the GHCB and setup scratch sw to point to shared buffer.
    //
    VmgInit (Ghcb, &InterruptState);
    Info = (SNP_PAGE_STATE_CHANGE_INFO *) Ghcb->SharedBuffer;

    SetMem (Info, sizeof (*Info), 0);

    //
    // Build page state change buffer
    //
    for (i = 0; (EndAddress > BaseAddress) && i < SNP_PAGE_STATE_MAX_ENTRY;
          BaseAddress = NextAddress, i++) {
      //
      // Is this a 2MB aligned page ? Check whether we can do a 2MB validation.
      //
      if (IS_ALIGNED (BaseAddress, EFI_LARGE_PAGE) &&
          ((EndAddress - BaseAddress) >> EFI_PAGE_SHIFT) >= 512) {
        RmpPageSize = PVALIDATE_PAGE_SIZE_2M;
        NextAddress = BaseAddress + EFI_LARGE_PAGE;
      } else {
        RmpPageSize = PVALIDATE_PAGE_SIZE_4K;
        NextAddress = BaseAddress + EFI_PAGE_SIZE;
      }

      Info->Entry[i].GuestFrameNumber = BaseAddress >> EFI_PAGE_SHIFT;
      Info->Entry[i].PageSize = RmpPageSize;
      Info->Entry[i].Op = MemoryTypeToGhcbOp (Type);
      Info->Entry[i].CurrentPage = 0;
    }

    Info->Header.CurrentEntry = 0;
    Info->Header.EndEntry = i - 1;

    Ghcb->SaveArea.SwScratch = (UINT64) Ghcb->SharedBuffer;
    VmgSetOffsetValid (Ghcb, GhcbSwScratch);

    // 
    // Issue the VMGEXIT and retry if hypervisor failed to process all the entries.
    //
    while (Info->Header.CurrentEntry <= Info->Header.EndEntry) {
      Status = VmgExit (Ghcb, SVM_EXIT_SNP_PAGE_STATE_CHANGE, 0, 0);
      if (EFI_ERROR (Status)) {
        break;
      }
    }

    VmgDone (Ghcb, InterruptState);
  }

  return Status;
}

RETURN_STATUS
PvalidateInternal (
  IN PHYSICAL_ADDRESS               BaseAddress,
  IN UINTN                          NumPages,
  IN MEM_OP_REQ                     Type
  )
{
  BOOLEAN                         Validate, RmpPageSize;
  IA32_EFLAGS32                   EFlags;
  UINTN                           i, Return;
  EFI_PHYSICAL_ADDRESS            EndAddress, NextAddress;

  if (Type == MemoryTypePrivate) {
    Validate = TRUE;
  } else {
    Validate = FALSE;
  }

  EndAddress = BaseAddress + EFI_PAGES_TO_SIZE (NumPages);
  for (; EndAddress > BaseAddress; BaseAddress = NextAddress) {

    //
    // Is this a 2MB aligned page ? Check whether we can do a 2MB validation.
    //
    if (IS_ALIGNED (BaseAddress, EFI_LARGE_PAGE) &&
        ((EndAddress - BaseAddress) >> EFI_PAGE_SHIFT) >= 512) {
      RmpPageSize = PVALIDATE_PAGE_SIZE_2M;
      NextAddress = BaseAddress + EFI_LARGE_PAGE;
    } else {
      RmpPageSize = PVALIDATE_PAGE_SIZE_4K;
      NextAddress = BaseAddress + EFI_PAGE_SIZE;
    }

    Return = AsmPvalidate (RmpPageSize, Validate, BaseAddress, &EFlags);

    //
    // Check the rFlags.CF to verify that PVALIDATE updated the RMP
    // entry. If there was a no change in the RMP entry then we are
    // double validating the memory and it can lead to a security
    // compromise. Abort the boot if we detect the double validation
    // senario.
    //
    if (EFlags.Bits.CF) {
      DEBUG ((EFI_D_INFO, "*** Double validation detected for GPA=0x%Lx "
             "PageSize=%d Validate=%d\n", BaseAddress, RmpPageSize, Validate));
      ASSERT (FALSE);
      CpuDeadLoop ();
    }

    //
    // If we fail to validate due to size mismatch then try with the
    // smaller page size. This senario will occur if we requested to
    // add RMP entry as a 2MB but the hypervisor backed the page as
    // a 4K.
    //
    if ((Return == PVALIDATE_RET_FAIL_SIZEMISMATCH) &&
        (RmpPageSize == PVALIDATE_PAGE_SIZE_2M)) {
      for (i = 0; i < 512; i++) {

        Return = AsmPvalidate (PVALIDATE_PAGE_SIZE_4K, Validate,
                               BaseAddress + (i * EFI_PAGE_SIZE), &EFlags);

        if (EFlags.Bits.CF) {
          DEBUG ((EFI_D_INFO, "*** Double validation detected for GPA=0x%Lx "
                 "PageSize=%d Validate=%d\n", BaseAddress, RmpPageSize, Validate));
          ASSERT (FALSE);
          CpuDeadLoop ();
        }
      }
    }

    if (Return) {
      return EFI_SECURITY_VIOLATION;
    }

  }

  return EFI_SUCCESS;
}
