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
  UINT64                   Status;
  UINTN                    ExitCode, VcRet;

  VmgInit (Ghcb);

  ExitCode = Regs->ExceptionData;
  switch (ExitCode) {
  default:
    Status = VmgExit (Ghcb, SvmExitUnsupported, ExitCode, 0);
    if (Status == 0) {
      Regs->ExceptionData = 0;
      VcRet = GP_EXCEPTION;
    } else {
      GHCB_EVENT_INJECTION  Event;

      Event.Uint64 = Status;
      if (Event.Elements.ErrorCodeValid) {
        Regs->ExceptionData = Event.Elements.ErrorCode;
      } else {
        Regs->ExceptionData = 0;
      }

      VcRet = Event.Elements.Vector;
    }
  }

  VmgDone (Ghcb);

  return VcRet;
}
