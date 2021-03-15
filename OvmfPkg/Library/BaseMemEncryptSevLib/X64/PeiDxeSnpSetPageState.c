/** @file

  SEV-SNP Page Validation functions.

  Copyright (c) 2020 - 2021, AMD Incorporated. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>

#include "PeiSnpPageStateChange.h"

VOID
SnpSetMemoryPrivate (
  IN  PHYSICAL_ADDRESS      PhysicalAddress,
  IN  UINTN                 Length
  )
{
  SetPageStateInternal (PhysicalAddress, EFI_SIZE_TO_PAGES (Length), SevSnpPagePrivate, FALSE);
}

VOID
SnpSetMemoryShared (
  IN  PHYSICAL_ADDRESS      PhysicalAddress,
  IN  UINTN                 Length
  )
{
  SetPageStateInternal (PhysicalAddress, EFI_SIZE_TO_PAGES (Length), SevSnpPageShared, FALSE);
}
