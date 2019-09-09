/** @file
  Public header file for the VMGEXIT Support library class.

  This library class defines some routines used when invoking the VMGEXIT
  instruction in support of SEV-ES.

  Copyright (c) 2019, AMD Inc. All rights reserved.<BR>
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

#endif
