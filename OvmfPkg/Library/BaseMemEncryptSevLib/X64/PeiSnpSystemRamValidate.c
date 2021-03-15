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
  SEC_SEV_ES_WORK_AREA    *SevEsWorkArea;
  SNP_VALIDATED_RANGE     *RootNode;

  EndAddress = BaseAddress + EFI_PAGES_TO_SIZE (NumPages);

  // The Root of SNP_VALIDATED_RANGE is saved in the EsWorkArea.
  SevEsWorkArea = (SEC_SEV_ES_WORK_AREA *) FixedPcdGet32 (PcdSevEsWorkAreaBase);
  RootNode = (SNP_VALIDATED_RANGE *) SevEsWorkArea->SnpSystemRamValidatedRootAddress;

  //
  // If the Root is NULL then its the first call. Lets initialize the List before
  // we process the request.
  //
  if (RootNode == NULL) {
    RootNode = SetPageStateChangeInitialize ();

    //
    // Save the RootNode in the workarea
    //
    SevEsWorkArea->SnpSystemRamValidatedRootAddress = (UINT64) (UINTN) RootNode;
  }

  //
  // The page table used in PEI can address up to 4GB memory. If we are asked to validate
  // a range above the 4GB, then create an identity mapping so that the PVALIDATE instruction
  //
  //
  
  if (BaseAddress >= SIZE_4GB) {
    Status = InternalMemEncryptSevCreateIdentityMap1G (0, BaseAddress,
                  EFI_PAGES_TO_SIZE (NumPages));
    if (EFI_ERROR (Status)) {
      ASSERT (FALSE);
    }
  }

  //
  // Check if the range is already validated
  //
  Range = FindOverlapRange (RootNode, BaseAddress, EndAddress);

  //
  // Range is not validated
  if (Range == NULL) {
    SetPageStateInternal (BaseAddress, NumPages, SevSnpPagePrivate, TRUE);
    AddRangeToIntervalTree (RootNode, BaseAddress, EndAddress);
    return;
  }

  //
  // The input range overlaps with the pre-validated range. Calculate the non-overlap
  // region and validate them.
  //
  if (BaseAddress < Range->StartAddress) {
    NumPages = EFI_SIZE_TO_PAGES (Range->StartAddress - BaseAddress);
    SetPageStateInternal (BaseAddress, NumPages, SevSnpPagePrivate, TRUE);
    AddRangeToIntervalTree (RootNode, BaseAddress, Range->StartAddress);
  }

  if (EndAddress > Range->EndAddress) {
    NumPages = EFI_SIZE_TO_PAGES (EndAddress - Range->EndAddress);
    SetPageStateInternal (Range->EndAddress, NumPages, SevSnpPagePrivate, TRUE);
    AddRangeToIntervalTree (RootNode, Range->StartAddress, EndAddress);
  }
}
