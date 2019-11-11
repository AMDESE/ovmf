/** @file
  PEI and DXE SEV-ES #VC Exception Handler functons.

  Copyright (c) 2019, Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Register/Amd/Msr.h>
#include "CpuExceptionCommon.h"
#include "AMDSevVcCommon.h"

UINTN
DoVcException (
  EFI_SYSTEM_CONTEXT  Context
  )
{
  MSR_SEV_ES_GHCB_REGISTER  Msr;
  GHCB                      *Ghcb;

  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);
  ASSERT(Msr.Bits.GhcbNegotiateBit == FALSE);

  Ghcb = Msr.Ghcb;

  return DoVcCommon (Ghcb, Context);
}
