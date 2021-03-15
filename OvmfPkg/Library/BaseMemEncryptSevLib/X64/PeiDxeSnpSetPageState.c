/** @file

  SEV-SNP Page Validation functions.

  Copyright (c) 2020 - 2021, AMD Incorporated. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>
#include <Library/MemEncryptSevLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>

#include "PeiSnpPageStateChange.h"
#include "SnpPageStateTrack.h"

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
  SEC_SEV_ES_WORK_AREA       *SevEsWorkArea;
  SNP_VALIDATED_RANGE        *RootNode, *Range;

  //
  // Get the Page State tracker root node. The information will be used to lookup
  // the address in the page state tracker.
  //
  SevEsWorkArea = (SEC_SEV_ES_WORK_AREA *) FixedPcdGet32 (PcdSevEsWorkAreaBase);
  RootNode = (SNP_VALIDATED_RANGE *) SevEsWorkArea->SnpSystemRamValidatedRootAddress;

  //
  // Check if the region is validated during the System RAM validation process.
  // If region is not validated then do nothing. This typically will happen if
  // we are getting called to make the page state change for the MMIO region.
  // The MMIO regions fall within reserved memory type and does not require
  // page state changes.
  //
  Range = FindOverlapRange (RootNode, PhysicalAddress, PhysicalAddress + Length);
  if (Range == NULL) {
    DEBUG ((EFI_D_INFO, "%a:%a %Lx - %Lx is not RAM, skipping it.\n",
          gEfiCallerBaseName,
          __FUNCTION__,
          PhysicalAddress,
          PhysicalAddress + Length));
    return;
  }

  SetPageStateInternal (PhysicalAddress, EFI_SIZE_TO_PAGES (Length), SevSnpPageShared, FALSE);
}
