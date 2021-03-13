/** @file

  SEV-SNP Page Validation functions.

  Copyright (c) 2020 - 2021, AMD Incorporated. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>

#include "../SnpPageStateChange.h"

VOID
SevSnpValidateSystemRam (
  IN EFI_PHYSICAL_ADDRESS             BaseAddress,
  IN UINTN                            NumPages
  )
{
  SetPageStateInternal (BaseAddress, NumPages, SevSnpPagePrivate, TRUE);
}
