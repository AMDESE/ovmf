/** @file
  Common header file for SEV-ES #VC Exception Handler Support.

  Copyright (c) 2020, Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _AMD_SEV_VC_COMMON_H_
#define _AMD_SEV_VC_COMMON_H_

#include <Protocol/DebugSupport.h>
#include <Register/Amd/Ghcb.h>

UINTN
DoVcException (
  EFI_SYSTEM_CONTEXT  Context
  );

UINTN
DoVcCommon (
  GHCB                *Ghcb,
  EFI_SYSTEM_CONTEXT  Context
  );

#endif
