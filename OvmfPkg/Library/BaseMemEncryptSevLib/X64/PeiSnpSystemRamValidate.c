/** @file

  SEV-SNP Page Validation functions.

  Copyright (c) 2020 - 2021, AMD Incorporated. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemEncryptSevLib.h>
#include <Library/DebugLib.h>

#include "../SnpPageStateChange.h"
#include "SnpPageStateTrack.h"
#include "VirtualMemory.h"

STATIC SNP_VALIDATED_RANGE     *mRootNode;

STATIC
SNP_VALIDATED_RANGE *
SetPageStateChangeInitialize (
  VOID
  )
{
  UINTN                      StartAddress, EndAddress;
  SNP_VALIDATED_RANGE        *RootNode;

  //
  // The memory range from PcdOvmfSnpCpuidBase to PcdOvmfDecompressionScratchEnd is
  // prevalidated before we enter into the Pei phase. The pre-validation breakdown
  // looks like this:
  //
  //    SnpCpuidBase                                      (VMM)
  //    SnpSecretBase                                     (VMM)
  //    SnpLaunchValidatedStart - SnpLaunchValidatedEnd   (VMM)
  //    SnpLaunchValidatedEnd - DecompressionScratchEnd   (SecMain)
  //
  // Add the range in system ram region tracker interval tree. The interval tree will
  // used to check whether there is an overlap with the pre-validated region. We will
  // skip validating the pre-validated region.
  //
  StartAddress = (UINTN) PcdGet32 (PcdOvmfSnpCpuidBase);
  EndAddress = (UINTN) PcdGet32 (PcdOvmfDecompressionScratchEnd);

  RootNode = AddRangeToIntervalTree (NULL, StartAddress, EndAddress);
  if (RootNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to add range to interval tree\n"));
    ASSERT (FALSE);
  }

  return RootNode;
}

VOID
SevSnpValidateSystemRam (
  IN UINTN      BaseAddress,
  IN UINTN      NumPages
  )
{
  UINTN                   EndAddress;
  SNP_VALIDATED_RANGE     *Range;
  EFI_STATUS              Status;

  EndAddress = BaseAddress + EFI_PAGES_TO_SIZE (NumPages);

  //
  // The page table used in PEI can address up to 4GB memory. If we are asked to validate
  // a range above the 4GB, then create an identity mapping so that the PVALIDATE instruction
  // can execute correctly. If the page table entry is not present then PVALIDATE will
  // cause the #GP.
  //
  if (BaseAddress >= SIZE_4GB) {
    Status = InternalMemEncryptSevCreateIdentityMap1G (0, BaseAddress,
                  EFI_PAGES_TO_SIZE (NumPages));
    if (EFI_ERROR (Status)) {
      ASSERT (FALSE);
    }
  }

  //
  // If the Root is NULL then its the first call. Lets initialize the List before
  // we process the request.
  //
  if (mRootNode == NULL) {
    mRootNode = SetPageStateChangeInitialize ();
  }

  //
  // Check if the range is already validated
  //
  EndAddress = BaseAddress + EFI_PAGES_TO_SIZE(NumPages);
  Range = FindOverlapRange (mRootNode, BaseAddress, EndAddress);

  //
  // Range is not validated
  if (Range == NULL) {
    SetPageStateInternal (BaseAddress, NumPages, SevSnpPagePrivate, TRUE);
    AddRangeToIntervalTree (mRootNode, BaseAddress, EndAddress);
    return;
  }

  //
  // The input range overlaps with the pre-validated range. Calculate the non-overlap
  // region and validate them.
  //
  if (BaseAddress < Range->StartAddress) {
    NumPages = EFI_SIZE_TO_PAGES (Range->StartAddress - BaseAddress);
    SetPageStateInternal (BaseAddress, NumPages, SevSnpPagePrivate, TRUE);
    AddRangeToIntervalTree (mRootNode, BaseAddress, Range->StartAddress);
  }

  if (EndAddress > Range->EndAddress) {
    NumPages = EFI_SIZE_TO_PAGES (EndAddress - Range->EndAddress);
    SetPageStateInternal (Range->EndAddress, NumPages, SevSnpPagePrivate, TRUE);
    AddRangeToIntervalTree (mRootNode, Range->StartAddress, EndAddress);
  }
}
