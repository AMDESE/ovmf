/** @file
  SEC SEV-ES #VC Exception Handler functons.

  Copyright (c) 2019, Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Register/Amd/Msr.h>
#include "CpuExceptionCommon.h"
#include "AMDSevVcCommon.h"


UINTN
DoVcException(
  EFI_SYSTEM_CONTEXT  Context
  )
{
  MSR_SEV_ES_GHCB_REGISTER  Msr;
  GHCB                      *Ghcb;

  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);
  Ghcb = Msr.Ghcb;

  if (Msr.Bits.GhcbNegotiateBit) {
    if (Msr.GhcbProtocol.SevEsProtocolMin > Msr.GhcbProtocol.SevEsProtocolMax) {
      ASSERT (0);
      return GP_EXCEPTION;
    }

    if ((Msr.GhcbProtocol.SevEsProtocolMin > GHCB_VERSION_MAX) ||
        (Msr.GhcbProtocol.SevEsProtocolMax < GHCB_VERSION_MIN)) {
      ASSERT (0);
      return GP_EXCEPTION;
    }

    Msr.GhcbPhysicalAddress = FixedPcdGet32 (PcdSecGhcbBase);
    AsmWriteMsr64(MSR_SEV_ES_GHCB, Msr.GhcbPhysicalAddress);

    Ghcb = Msr.Ghcb;
    SetMem (Ghcb, sizeof (*Ghcb), 0);

    /* Set the version to the maximum that can be supported */
    Ghcb->ProtocolVersion = MIN (Msr.GhcbProtocol.SevEsProtocolMax, GHCB_VERSION_MAX);
    Ghcb->GhcbUsage = GHCB_STANDARD_USAGE;
  }

  return DoVcCommon(Ghcb, Context);
}
