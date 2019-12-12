/** @file
  X64 SEV-ES #VC Exception Handler functons.

  Copyright (c) 2019, Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/VmgExitLib.h>
#include "AMDSevVcCommon.h"

UINTN
DoVcCommon (
  GHCB                *Ghcb,
  EFI_SYSTEM_CONTEXT  Context
  )
{
  EFI_SYSTEM_CONTEXT_X64   *Regs = Context.SystemContextX64;
  UINTN                    ExitCode;
  UINTN                    Status;

  VmgInit (Ghcb);

  ExitCode = Regs->ExceptionData;
  switch (ExitCode) {
  default:
    Status = VmgExit (Ghcb, SvmExitUnsupported, ExitCode, 0);
  }

  VmgDone (Ghcb);

  return Status;
}
