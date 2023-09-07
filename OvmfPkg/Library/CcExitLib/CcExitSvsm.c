/** @file
  SVSM Support Library.

  Copyright (C) 2022, Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CcExitLib.h>
#include <Register/Amd/Msr.h>
#include <Register/Amd/Svsm.h>

#define PAGES_PER_2MB_ENTRY  512

/**
  Terminate the guest using the GHCB MSR protocol.

  Uses the GHCB MSR protocol to request that the guest be termiated.

**/
STATIC
VOID
SvsmTerminate (
  VOID
  )
{
  MSR_SEV_ES_GHCB_REGISTER  Msr;

  //
  // Use the GHCB MSR Protocol to request termination by the hypervisor
  //
  Msr.Uint64                      = 0;
  Msr.GhcbTerminate.Function      = GHCB_INFO_TERMINATE_REQUEST;
  Msr.GhcbTerminate.ReasonCodeSet = GHCB_TERMINATE_GHCB;
  Msr.GhcbTerminate.ReasonCode    = GHCB_TERMINATE_GHCB_GENERAL;
  AsmWriteMsr64 (MSR_SEV_ES_GHCB, Msr.Uint64);

  AsmVmgExit ();

  ASSERT (FALSE);
  CpuDeadLoop ();
}

/**
  Return the address of SVSM Call Area (CAA).

  Determines the address of the SVSM CAA.

  @return                         The address of the SVSM CAA

**/
STATIC
SVSM_CAA *
SvsmGetCaa (
  VOID
  )
{
  SVSM_INFORMATION  *SvsmInfo;

  SvsmInfo = (SVSM_INFORMATION *)(UINTN)PcdGet32 (PcdOvmfSnpSecretsBase);

  return CcExitSnpSvsmPresent () ? (SVSM_CAA *)SvsmInfo->SvsmCaa : NULL;
}

/**
  Issue an SVSM request.

  Invokes the SVSM to process a request on behalf of the guest.

  @param[in]  Caa     Pointer to the call area
  @param[in]  Rax     Contents to be set in RAX at VMGEXIT
  @param[in]  Rcx     Contents to be set in RCX at VMGEXIT
  @param[in]  Rdx     Contents to be set in RDX at VMGEXIT
  @param[in]  R8      Contents to be set in R8 at VMGEXIT

  @return             Contents of RAX upon return from VMGEXIT
**/
STATIC
UINTN
SvsmMsrProtocol (
  IN SVSM_CAA  *Caa,
  IN UINT64    Rax,
  IN UINT64    Rcx,
  IN UINT64    Rdx,
  IN UINT64    R8
  )
{
  MSR_SEV_ES_GHCB_REGISTER  Msr;
  UINT64                    CurrentMsr;
  UINT8                     Pending;
  BOOLEAN                   InterruptState;
  UINTN                     Ret;

  //
  // Be sure that an interrupt can't cause a #VC while the GHCB MSR protocol
  // is being used (#VC handler will ASSERT if lower 12-bits are not zero).
  //
  InterruptState = GetInterruptState ();
  if (InterruptState) {
    DisableInterrupts ();
  }

  Caa->SvsmCallPending = 1;

  CurrentMsr = AsmReadMsr64 (MSR_SEV_ES_GHCB);

  Msr.Uint64                  = 0;
  Msr.SnpVmplRequest.Function = GHCB_INFO_SNP_VMPL_REQUEST;
  Msr.SnpVmplRequest.Vmpl     = 0;
  AsmWriteMsr64 (MSR_SEV_ES_GHCB, Msr.Uint64);

  //
  // Guest memory is used for the guest-SVSM communication, so fence the
  // invocation of the VMGEXIT instruction to ensure VMSA accesses are
  // synchronized properly.
  //
  MemoryFence ();
  Ret = AsmVmgExitSvsm (Rcx, Rdx, R8, 0, Rax);
  MemoryFence ();

  Msr.Uint64 = AsmReadMsr64 (MSR_SEV_ES_GHCB);

  AsmWriteMsr64 (MSR_SEV_ES_GHCB, CurrentMsr);

  if (InterruptState) {
    EnableInterrupts ();
  }

  if (Msr.SnpVmplResponse.Function != GHCB_INFO_SNP_VMPL_RESPONSE ||
      Msr.SnpVmplResponse.ErrorCode != 0) {
    SvsmTerminate ();
  }

  Pending = 0;
  asm volatile ("xchgb %0, %1" : "+r" (Pending) : "m" (Caa->SvsmCallPending) : "memory");
  if (Pending != 0) {
    SvsmTerminate ();
  }

  return Ret;
}

/**
  Perform an RMPADJUST operation to alter the VMSA setting of a page.

  Add or remove the VMSA attribute for a page.

  @param[in]       Vmsa           Pointer to an SEV-ES save area page
  @param[in]       ApicId         APIC ID associated with the VMSA
  @param[in]       SetVmsa        Boolean indicator as to whether to set or
                                  or clear the VMSA setting for the page

  @retval  EFI_SUCCESS            RMPADJUST operation successful
  @retval  EFI_UNSUPPORTED        Operation is not supported
  @retval  EFI_INVALID_PARAMETER  RMPADJUST operation failed, an invalid
                                  parameter was supplied

**/
EFI_STATUS
EFIAPI
SvsmVmsaRmpAdjust (
  IN SEV_ES_SAVE_AREA  *Vmsa,
  IN UINT32            ApicId,
  IN BOOLEAN           SetVmsa
  )
{
  SVSM_CAA                *Caa;
  SVSM_FUNCTION           Function;
  UINT64                  VmsaGpa;
  UINT64                  CaaGpa;
  UINTN                   Ret;

  Caa = SvsmGetCaa ();

  Function.Protocol = 0;
  if (SetVmsa) {
    Function.CallId = 2;

    VmsaGpa = (UINT64)(UINTN)Vmsa;
    CaaGpa  = (UINT64)(UINTN)Vmsa + SIZE_4KB;
    Ret = SvsmMsrProtocol (Caa, Function.Uint64, VmsaGpa, CaaGpa, ApicId);
  } else {
    Function.CallId = 3;

    VmsaGpa = (UINT64)(UINTN)Vmsa;
    Ret = SvsmMsrProtocol (Caa, Function.Uint64, VmsaGpa, 0, 0);
  }

  return (Ret == 0) ? EFI_SUCCESS : EFI_INVALID_PARAMETER;
}

/**
  Perform an RMPADJUST operation to alter the VMSA setting of a page.

  Add or remove the VMSA attribute for a page.

  @param[in]       Vmsa           Pointer to an SEV-ES save area page
  @param[in]       ApicId         APIC ID associated with the VMSA
  @param[in]       SetVmsa        Boolean indicator as to whether to set or
                                  or clear the VMSA setting for the page

  @retval  EFI_SUCCESS            RMPADJUST operation successful
  @retval  EFI_UNSUPPORTED        Operation is not supported
  @retval  EFI_INVALID_PARAMETER  RMPADJUST operation failed, an invalid
                                  parameter was supplied

**/
EFI_STATUS
EFIAPI
BaseVmsaRmpAdjust (
  IN SEV_ES_SAVE_AREA  *Vmsa,
  IN UINT32            ApicId,
  IN BOOLEAN           SetVmsa
  )
{
  UINT64  Rdx;
  UINT32  Ret;

  //
  // The RMPADJUST instruction is used to set or clear the VMSA bit for a
  // page. The VMSA change is only made when running at VMPL0 and is ignored
  // otherwise. If too low a target VMPL is specified, the instruction can
  // succeed without changing the VMSA bit when not running at VMPL0. Using a
  // target VMPL level of 1, RMPADJUST will return a FAIL_PERMISSION error if
  // not running at VMPL0, thus ensuring that the VMSA bit is set appropriately
  // when no error is returned.
  //
  Rdx = 1;
  if (SetVmsa) {
    Rdx |= RMPADJUST_VMSA_PAGE_BIT;
  }

  Ret = AsmRmpAdjust ((UINT64)(UINTN)Vmsa, 0, Rdx);

  return (Ret == 0) ? EFI_SUCCESS : EFI_INVALID_PARAMETER;
}

/**
  Issue an SVSM request to perform the PVALIDATE instruction.

  Invokes the SVSM to process the PVALIDATE instruction on behalf of the
  guest to validate or invalidate the memory range specified.

  @param[in]       Info           Pointer to a page state change structure

**/
STATIC
VOID
SvsmPvalidate (
  IN SNP_PAGE_STATE_CHANGE_INFO  *Info
  )
{
  SVSM_CAA                *Caa;
  SVSM_PVALIDATE_REQUEST  *Request;
  SVSM_FUNCTION           Function;
  BOOLEAN                 Validate;
  UINTN                   Entry;
  UINTN                   EntryLimit;
  UINTN                   Index;
  UINTN                   EndIndex;
  UINT64                  Gfn;
  UINT64                  GfnEnd;
  UINTN                   Ret;

  Caa = SvsmGetCaa ();
  SetMem (Caa->SvsmBuffer, sizeof (Caa->SvsmBuffer), 0);

  Function.Protocol = 0;
  Function.CallId   = 1;

  Request    = (SVSM_PVALIDATE_REQUEST *)Caa->SvsmBuffer;
  EntryLimit = ((sizeof (Caa->SvsmBuffer) - sizeof (*Request)) /
                 sizeof (Request->Entry[0])) - 1;

  Entry    = 0;
  Index    = Info->Header.CurrentEntry;
  EndIndex = Info->Header.EndEntry;

  while (Index <= EndIndex) {
    Validate = Info->Entry[Index].Operation == SNP_PAGE_STATE_PRIVATE;

    Request->Header.Entries++;
    Request->Entry[Entry].PageSize = Info->Entry[Index].PageSize;
    Request->Entry[Entry].Action   = (Validate == TRUE) ? 1 : 0;
    Request->Entry[Entry].IgnoreCf = 0;
    Request->Entry[Entry].Address  = Info->Entry[Index].GuestFrameNumber;

    Entry++;
    if (Entry > EntryLimit || Index == EndIndex) {
      do {
        Ret = SvsmMsrProtocol (Caa, Function.Uint64, (UINT64)Request, 0, 0);
      } while (Ret == SVSM_ERR_INCOMPLETE || Ret == SVSM_ERR_BUSY);

      if (Ret == SVSM_ERR_PVALIDATE_FAIL_SIZE_MISMATCH &&
          Request->Entry[Request->Header.Next].PageSize != 0) {
        // Calculate the Index of the entry after the entry that failed
        // before clearing the buffer so that processing can continue
        // from that point
        Index = Index - (Entry - Request->Header.Next) + 2;

        // Obtain the failing GFN before clearing the buffer
        Gfn = Request->Entry[Request->Header.Next].Address;

        // Clear the buffer in prep for creating all new entries
        SetMem (Caa->SvsmBuffer, sizeof (Caa->SvsmBuffer), 0);
        Entry = 0;

        GfnEnd = Gfn + 511;
        for (; Gfn <= GfnEnd; Gfn++) {
          Request->Header.Entries++;
          Request->Entry[Entry].PageSize = 0;
          Request->Entry[Entry].Action   = (Validate == TRUE) ? 1 : 0;
          Request->Entry[Entry].IgnoreCf = 0;
          Request->Entry[Entry].Address  = Gfn;

          Entry++;
          if (Entry > EntryLimit || Gfn == GfnEnd) {
            do {
              Ret = SvsmMsrProtocol (Caa, Function.Uint64, (UINT64)Request, 0, 0);
            } while (Ret == SVSM_ERR_INCOMPLETE || Ret == SVSM_ERR_BUSY);

            ASSERT (Ret == 0);

            SetMem (Caa->SvsmBuffer, sizeof (Caa->SvsmBuffer), 0);
            Entry = 0;
          }
        }

        continue;
      }

      ASSERT (Ret == 0);

      SetMem (Caa->SvsmBuffer, sizeof (Caa->SvsmBuffer), 0);
      Entry = 0;
    }

    Index++;
  }
}

/**
  Perform the PVALIDATE instruction.

  Performs the PVALIDATE instruction to validate or invalidate the memory
  range specified.

  @param[in]       Info           Pointer to a page state change structure

**/
STATIC
VOID
BasePvalidate (
  IN SNP_PAGE_STATE_CHANGE_INFO  *Info
  )
{
  UINTN    Index;
  UINTN    EndIndex;
  UINTN    Address;
  UINTN    RmpPageSize;
  BOOLEAN  Validate;
  UINTN    Ret;

  Index    = Info->Header.CurrentEntry;
  EndIndex = Info->Header.EndEntry;
  while (Index <= EndIndex) {
    //
    // Get the address and the page size from the Info.
    //
    Address     = Info->Entry[Index].GuestFrameNumber << EFI_PAGE_SHIFT;
    RmpPageSize = Info->Entry[Index].PageSize;
    Validate    = Info->Entry[Index].Operation == SNP_PAGE_STATE_PRIVATE;


    Ret = AsmPvalidate (RmpPageSize, Validate, Address);

    //
    // If PVALIDATE of a 2M page fails due to a size mismatch, then retry
    // the full 2M range using a page size of 4K. This can occur if RMP entry
    // has a page size of 4K.
    //
    if (Ret == PVALIDATE_RET_SIZE_MISMATCH && RmpPageSize == PvalidatePageSize2MB) {
      UINTN  EndAddress;

      EndAddress = Address + (PAGES_PER_2MB_ENTRY * SIZE_4KB);
      while (Address < EndAddress) {
        Ret = AsmPvalidate (PvalidatePageSize4K, Validate, Address);
        if (Ret) {
          break;
        }

        Address += SIZE_4KB;
      }
    }

    //
    // If validation failed then do not continue.
    //
    if (Ret) {
      DEBUG ((
        DEBUG_ERROR,
        "%a:%a: Failed to %a address 0x%Lx Error code %d\n",
        gEfiCallerBaseName,
        __func__,
        Validate ? "Validate" : "Invalidate",
        Address,
        Ret
        ));

      SvsmTerminate ();
    }

    Index++;
  }
}

/**
  Report the presence of an Secure Virtual Services Module (SVSM).

  Determines the presence of an SVSM.

  @retval  TRUE                   An SVSM is present
  @retval  FALSE                  An SVSM is not present

**/
BOOLEAN
EFIAPI
CcExitSnpSvsmPresent (
  VOID
  )
{
  SVSM_INFORMATION  *SvsmInfo;

  SvsmInfo = (SVSM_INFORMATION *)(UINTN)PcdGet32 (PcdOvmfSnpSecretsBase);

  return (SvsmInfo != NULL && SvsmInfo->SvsmSize != 0);
}

/**
  Report the VMPL level at which the SEV-SNP guest is running.

  Determines the VMPL level at which the guest is running. If an SVSM is
  not present, then it must be VMPL0, otherwise return what is reported
  by the SVSM.

  @return                         The VMPL level

**/
UINT8
EFIAPI
CcExitSnpGetVmpl (
  VOID
  )
{
  SVSM_INFORMATION  *SvsmInfo;

  SvsmInfo = (SVSM_INFORMATION *)(UINTN)PcdGet32 (PcdOvmfSnpSecretsBase);

  return CcExitSnpSvsmPresent () ? SvsmInfo->SvsmGuestVmpl : 0;
}

/**
  Perform a PVALIDATE operation for the page ranges specified.

  Validate or rescind the validation of the specified pages.

  @param[in]       Info           Pointer to a page state change structure

**/
VOID
EFIAPI
CcExitSnpPvalidate (
  IN SNP_PAGE_STATE_CHANGE_INFO  *Info
  )
{
  if (CcExitSnpSvsmPresent ()) {
    SvsmPvalidate (Info);
  } else {
    BasePvalidate (Info);
  }
}

/**
  Perform an RMPADJUST operation to alter the VMSA setting of a page.

  Add or remove the VMSA attribute for a page.

  @param[in]       Vmsa           Pointer to an SEV-ES save area page
  @param[in]       ApicId         APIC ID associated with the VMSA
  @param[in]       SetVmsa        Boolean indicator as to whether to set or
                                  or clear the VMSA setting for the page

  @retval  EFI_SUCCESS            RMPADJUST operation successful
  @retval  EFI_UNSUPPORTED        Operation is not supported
  @retval  EFI_INVALID_PARAMETER  RMPADJUST operation failed, an invalid
                                  parameter was supplied

**/
EFI_STATUS
EFIAPI
CcExitSnpVmsaRmpAdjust (
  IN SEV_ES_SAVE_AREA  *Vmsa,
  IN UINT32            ApicId,
  IN BOOLEAN           SetVmsa
  )
{
  return CcExitSnpSvsmPresent () ? SvsmVmsaRmpAdjust (Vmsa, ApicId, SetVmsa)
                                 : BaseVmsaRmpAdjust (Vmsa, ApicId, SetVmsa);
}
