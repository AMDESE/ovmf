/** @file
  VMGEXIT Support Library.

  Copyright (c) 2019, Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Register/Amd/Ghcb.h>
#include <Register/Amd/Msr.h>

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

UINTN
EFIAPI
VmgMmio (
  UINT8               *MmioAddress,
  UINT8               *Buffer,
  UINTN               Bytes,
  BOOLEAN             Write
  )
{
  UINT64                    MmioOp;
  UINT64                    ExitInfo1, ExitInfo2;
  UINTN                     Status;
  GHCB                      *Ghcb;
  MSR_SEV_ES_GHCB_REGISTER  Msr;

  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);
  Ghcb = Msr.Ghcb;

  if (Write) {
    MmioOp = SvmExitMmioWrite;
  } else {
    MmioOp = SvmExitMmioRead;
  }

  ExitInfo1 = (UINT64) MmioAddress;
  ExitInfo2 = Bytes;

  if (Write) {
    CopyMem (Ghcb->SharedBuffer, Buffer, Bytes);
  }

  Ghcb->SaveArea.SwScratch = (UINT64) Ghcb->SharedBuffer;
  Status = VmgExit (Ghcb, MmioOp, ExitInfo1, ExitInfo2);
  if (Status != 0) {
    return Status;
  }

  if (!Write) {
    CopyMem (Buffer, Ghcb->SharedBuffer, Bytes);
  }

  return 0;
}

VOID
EFIAPI
VmgMmioWrite (
  UINT8               *Dest,
  UINT8               *Src,
  UINTN                Bytes
  )
{
  VmgMmio (Dest, Src, Bytes, TRUE);
}

