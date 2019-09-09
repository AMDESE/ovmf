/** @file
  VMGEXIT Support Library.

  Copyright (c) 2019, AMD Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Register/Amd/Ghcb.h>
#include <Library/BaseMemoryLib.h>

STATIC
UINTN
VmgExitErrorCheck (
  GHCB                *Ghcb
  )
{
  GHCB_EXIT_INFO   ExitInfo;
  UINTN            Reason, Action;

  if (!Ghcb->SaveArea.SwExitInfo1) {
    return 0;
  }

  ExitInfo.Uint64 = Ghcb->SaveArea.SwExitInfo1;
  Action = ExitInfo.Elements.Lower32Bits;
  if (Action == 1) {
    Reason = ExitInfo.Elements.Upper32Bits;

    switch (Reason) {
    case UD_EXCEPTION:
    case GP_EXCEPTION:
      return Reason;
    }
  }

  ASSERT (0);
  return GP_EXCEPTION;
}

UINTN
EFIAPI
VmgExit (
  GHCB                *Ghcb,
  UINT64              ExitCode,
  UINT64              ExitInfo1,
  UINT64              ExitInfo2
  )
{
  Ghcb->SaveArea.SwExitCode = ExitCode;
  Ghcb->SaveArea.SwExitInfo1 = ExitInfo1;
  Ghcb->SaveArea.SwExitInfo2 = ExitInfo2;
  AsmVmgExit ();

  return VmgExitErrorCheck (Ghcb);
}

VOID
EFIAPI
VmgInit (
  GHCB                *Ghcb
  )
{
  SetMem (&Ghcb->SaveArea, sizeof (Ghcb->SaveArea), 0);
}

VOID
EFIAPI
VmgDone (
  GHCB                *Ghcb
  )
{
}

