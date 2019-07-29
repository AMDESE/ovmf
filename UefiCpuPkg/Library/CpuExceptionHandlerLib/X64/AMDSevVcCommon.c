
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include "AMDSevVcCommon.h"

UINTN
DoVcCommon (
  GHCB                *Ghcb,
  EFI_SYSTEM_CONTEXT  Context
  )
{
  EFI_SYSTEM_CONTEXT_X64  *Regs = Context.SystemContextX64;
  UINTN                   ExitCode;
  UINTN                   Status;

  VmgInit (Ghcb);

  ExitCode = Regs->ExceptionData;
  switch (ExitCode) {
  default:
    Status = VmgExit (Ghcb, SvmExitUnsupported, ExitCode, 0);
  }

  VmgDone (Ghcb);

  return Status;
}
