/** @file
  IA32 SEV-ES #VC Exception Handler functons.

  Copyright (c) 2019, Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include "AMDSevVcCommon.h"

UINTN
DoVcCommon (
  GHCB                *Ghcb,
  EFI_SYSTEM_CONTEXT  Context
  )
{
  return GP_EXCEPTION;
}
