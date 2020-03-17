/** @file
  Public header file for the VMGEXIT Support library class.

  This library class defines some routines used when invoking the VMGEXIT
  instruction in support of SEV-ES.

  Copyright (c) 2019, Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __VMG_EXIT_LIB_H__
#define __VMG_EXIT_LIB_H__

#include <Register/Amd/Ghcb.h>


/**
  Perform VMGEXIT.

  Sets the necessary fields of the GHCB, invokes the VMGEXIT instruction and
  then handles the return actions.

  @param[in]  GHCB       A pointer to the GHCB
  @param[in]  ExitCode   VMGEXIT code to be assigned to the SwExitCode field of
                         the GHCB.
  @param[in]  ExitInfo1  VMGEXIT information to be assigned to the SwExitInfo1
                         field of the GHCB.
  @param[in]  ExitInfo2  VMGEXIT information to be assigned to the SwExitInfo2
                         field of the GHCB.

  @retval  0       VMGEXIT succeeded.
  @retval  Others  VMGEXIT processing did not succeed. Exception number to
                   be issued.

**/
UINTN
EFIAPI
VmgExit (
  GHCB                *Ghcb,
  UINT64              ExitCode,
  UINT64              ExitInfo1,
  UINT64              ExitInfo2
  );

/**
  Perform pre-VMGEXIT initialization/preparation.

  Performs the necessary steps in preparation for invoking VMGEXIT.

  @param[in]  GHCB       A pointer to the GHCB

**/
VOID
EFIAPI
VmgInit (
  GHCB                *Ghcb
  );

/**
  Perform post-VMGEXIT cleanup.

  Performs the necessary steps to cleanup after invoking VMGEXIT.

  @param[in]  GHCB       A pointer to the GHCB

**/
VOID
EFIAPI
VmgDone (
  GHCB                *Ghcb
  );

#define VMGMMIO_READ   False
#define VMGMMIO_WRITE  True

/**
  Perform MMIO write of a buffer to a non-MMIO marked range.

  Performs an MMIO write without taking a #VC. This is useful
  for Flash devices, which are marked read-only.

  @param[in]  UINT8      A pointer to the destination buffer
  @param[in]  UINTN      The immediate value to write
  @param[in]  UINTN      Number of bytes to write

**/
VOID
EFIAPI
VmgMmioWrite (
  UINT8               *Dest,
  UINT8               *Src,
  UINTN                Bytes
  );

/**
  Issue the GHCB set AP Jump Table VMGEXIT.

  Performs a VMGEXIT using the GHCB AP Jump Table exit code to save the
  AP Jump Table address with the hypervisor for retrieval at a later time.

  @param[in]  EFI_PHYSICAL_ADDRESS  Physical address of the AP Jump Table

**/
UINTN
EFIAPI
VmgExitSetAPJumpTable (
  EFI_PHYSICAL_ADDRESS  Address
  );

/**
  Perform the Memory operation for the specified address range.

  @param[in]  EFI_PHYSICAL_ADDRESS      Start address
  @param[in]  UINTN                     Number of pages
  @param[in]  UINT8                     Memory Operation type

**/
EFI_STATUS
EFIAPI
VmgSnpMemOperation (
  EFI_PHYSICAL_ADDRESS      Start,
  UINTN                     NumOfPages,
  UINTN                     Type
  );

#endif
