/** @file
  File defines the Sec routines for the AMD SEV

  Copyright (c) 2021, Advanced Micro Devices, Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemEncryptSevLib.h>
#include <Library/BaseMemoryLib.h>
#include <Register/Amd/Ghcb.h>
#include <Register/Amd/Msr.h>

#include "AmdSev.h"

/**
  Handle an SEV-ES/GHCB protocol check failure.

  Notify the hypervisor using the VMGEXIT instruction that the SEV-ES guest
  wishes to be terminated.

  @param[in] ReasonCode  Reason code to provide to the hypervisor for the
                         termination request.

**/
VOID
SevEsProtocolFailure (
  IN UINT8  ReasonCode
  )
{
  MSR_SEV_ES_GHCB_REGISTER  Msr;

  //
  // Use the GHCB MSR Protocol to request termination by the hypervisor
  //
  Msr.GhcbPhysicalAddress         = 0;
  Msr.GhcbTerminate.Function      = GHCB_INFO_TERMINATE_REQUEST;
  Msr.GhcbTerminate.ReasonCodeSet = GHCB_TERMINATE_GHCB;
  Msr.GhcbTerminate.ReasonCode    = ReasonCode;
  AsmWriteMsr64 (MSR_SEV_ES_GHCB, Msr.GhcbPhysicalAddress);

  AsmVmgExit ();

  ASSERT (FALSE);
  CpuDeadLoop ();
}

/**
  Validate the SEV-ES/GHCB protocol level.

  Verify that the level of SEV-ES/GHCB protocol supported by the hypervisor
  and the guest intersect. If they don't intersect, request termination.

**/
VOID
SevEsProtocolCheck (
  VOID
  )
{
  MSR_SEV_ES_GHCB_REGISTER  Msr;
  GHCB                      *Ghcb;

  //
  // Use the GHCB MSR Protocol to obtain the GHCB SEV-ES Information for
  // protocol checking
  //
  Msr.GhcbPhysicalAddress = 0;
  Msr.GhcbInfo.Function   = GHCB_INFO_SEV_INFO_GET;
  AsmWriteMsr64 (MSR_SEV_ES_GHCB, Msr.GhcbPhysicalAddress);

  AsmVmgExit ();

  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);

  if (Msr.GhcbInfo.Function != GHCB_INFO_SEV_INFO) {
    SevEsProtocolFailure (GHCB_TERMINATE_GHCB_GENERAL);
  }

  if (Msr.GhcbProtocol.SevEsProtocolMin > Msr.GhcbProtocol.SevEsProtocolMax) {
    SevEsProtocolFailure (GHCB_TERMINATE_GHCB_PROTOCOL);
  }

  if ((Msr.GhcbProtocol.SevEsProtocolMin > GHCB_VERSION_MAX) ||
      (Msr.GhcbProtocol.SevEsProtocolMax < GHCB_VERSION_MIN))
  {
    SevEsProtocolFailure (GHCB_TERMINATE_GHCB_PROTOCOL);
  }

  //
  // SEV-ES protocol checking succeeded, set the initial GHCB address
  //
  Msr.GhcbPhysicalAddress = FixedPcdGet32 (PcdOvmfSecGhcbBase);
  AsmWriteMsr64 (MSR_SEV_ES_GHCB, Msr.GhcbPhysicalAddress);

  Ghcb = Msr.Ghcb;
  SetMem (Ghcb, FixedPcdGet32 (PcdOvmfSecGhcbSize), 0);

  //
  // Set the version to the maximum that can be supported
  //
  Ghcb->ProtocolVersion = MIN (Msr.GhcbProtocol.SevEsProtocolMax, GHCB_VERSION_MAX);
  Ghcb->GhcbUsage       = GHCB_STANDARD_USAGE;
}

/**
 Determine if the SEV is active.

 During the early booting, GuestType is set in the work area. Verify that it
 is an SEV guest.

 @retval TRUE   SEV is enabled
 @retval FALSE  SEV is not enabled

**/
BOOLEAN
IsSevGuest (
  VOID
  )
{
  OVMF_WORK_AREA  *WorkArea;

  //
  // Ensure that the size of the Confidential Computing work area header
  // is same as what is provided through a fixed PCD.
  //
  ASSERT (
    (UINTN)FixedPcdGet32 (PcdOvmfConfidentialComputingWorkAreaHeader) ==
    sizeof (CONFIDENTIAL_COMPUTING_WORK_AREA_HEADER)
    );

  WorkArea = (OVMF_WORK_AREA *)FixedPcdGet32 (PcdOvmfWorkAreaBase);

  return ((WorkArea != NULL) && (WorkArea->Header.GuestType == GUEST_TYPE_AMD_SEV));
}

/**
  Determine if SEV-ES is active.

  During early booting, SEV-ES support code will set a flag to indicate that
  SEV-ES is enabled. Return the value of this flag as an indicator that SEV-ES
  is enabled.

  @retval TRUE   SEV-ES is enabled
  @retval FALSE  SEV-ES is not enabled

**/
BOOLEAN
SevEsIsEnabled (
  VOID
  )
{
  SEC_SEV_ES_WORK_AREA  *SevEsWorkArea;

  if (!IsSevGuest ()) {
    return FALSE;
  }

  SevEsWorkArea = (SEC_SEV_ES_WORK_AREA *)FixedPcdGet32 (PcdSevEsWorkAreaBase);

  return (SevEsWorkArea->SevEsEnabled != 0);
}
