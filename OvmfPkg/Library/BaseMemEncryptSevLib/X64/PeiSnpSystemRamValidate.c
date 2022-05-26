/** @file

  SEV-SNP Page Validation functions.

  Copyright (c) 2021 AMD Incorporated. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/MemEncryptSevLib.h>

#include "SnpPageStateChange.h"
#include "VirtualMemory.h"

// KEEP THIS SORTED and DISJOINT FOR CORRECTNESS.
// The unaccepted memory algorithm depends on overlapping ranges returning in
// increasing order. 0s are considered unused.
STATIC SEV_SNP_PRE_VALIDATED_RANGE mPreValidatedRange[] = {
  // The below address range was part of the SEV OVMF metadata, and range
  // should be pre-validated by the Hypervisor.
  {
    FixedPcdGet32 (PcdOvmfSecPageTablesBase),
    FixedPcdGet32 (PcdOvmfPeiMemFvBase),
  },
  // The below range is pre-validated by the Sec/SecMain.c
  {
    FixedPcdGet32 (PcdOvmfSecValidatedStart),
    FixedPcdGet32 (PcdOvmfSecValidatedEnd)
  },
  // This is reserved for the first 512KB of memory.
  {
    0, 0,
  },
  // This is reserved for the configurable low memory preaccepted range.
  {
    0, 0,
  },
  // This is reserved for PeiInstallMemory.
  {
    0, 0,
  },
};

STATIC
VOID
RemovePreValidatedRange(
  UINTN Index
  )
{
  UINTN i;
  UINTN End;

  End = ARRAY_SIZE(mPreValidatedRange) - 1;

  ASSERT(Index <= End);

  // Overwrite Index..End-1 with Index+1..End. This duplicates the final entry.
  for (i = Index + 1; i <= End; i++) {
    CopyMem(&mPreValidatedRange[i - 1], &mPreValidatedRange[i], sizeof(mPreValidatedRange[i]));
  }
  // Clear the duplicated final entry.
  SetMem(&mPreValidatedRange[ARRAY_SIZE(mPreValidatedRange) - 1],
         sizeof(mPreValidatedRange[0]), 0);
}

/**
  If two ranges are consecutive, collapse them to be one range.
 **/
STATIC
VOID
MergePreValidatedRanges(
  VOID
  )
{
  UINTN i;

  for (i = 0; i < ARRAY_SIZE(mPreValidatedRange) - 1 &&
              mPreValidatedRange[i].EndAddress != 0; i++) {
    if (mPreValidatedRange[i].EndAddress ==
        mPreValidatedRange[i + 1].StartAddress) {
      mPreValidatedRange[i].EndAddress = mPreValidatedRange[i + 1].EndAddress;
      RemovePreValidatedRange(i + 1);
    }
  }
}

STATIC
BOOLEAN
DetectPreValidatedSubsumedIndex (
  IN    PHYSICAL_ADDRESS StartAddress,
  IN    PHYSICAL_ADDRESS EndAddress,
  OUT   UINTN            *Index
  )
{
  UINTN i;

  //
  // Check if the specified address range exist in pre-validated array.
  //
  for (i = 0; i < ARRAY_SIZE(mPreValidatedRange) &&
              // Also stop at the start of unclaimed 0,0 entries.
              mPreValidatedRange[i].EndAddress != 0; i++) {
    if ((StartAddress <= mPreValidatedRange[i].StartAddress) &&
        (mPreValidatedRange[i].EndAddress <= EndAddress)) {
      *Index = i;
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
VOID
InsertPreValidatedRange (
  IN PHYSICAL_ADDRESS StartAddress,
  IN PHYSICAL_ADDRESS EndAddress
  )
{
  SEV_SNP_PRE_VALIDATED_RANGE Temp;
  UINTN FirstZeroEntry;
  UINTN i;

  FirstZeroEntry = 0;

  // Find the first entry that is {0, 0} to use as the insert location.
  for (i = 0; i < ARRAY_SIZE(mPreValidatedRange); i++) {
    if (mPreValidatedRange[i].StartAddress == 0 &&
        mPreValidatedRange[i].EndAddress == 0) {
        FirstZeroEntry = i;
        break;
    }
  }

  // The number of required prevalidated ranges should be known, so if we hit
  // this, mPreValidatedRange needs to be extended with another {0, 0} entry.
  ASSERT(i < ARRAY_SIZE(mPreValidatedRange));

  mPreValidatedRange[FirstZeroEntry].StartAddress = StartAddress;
  mPreValidatedRange[FirstZeroEntry].EndAddress = EndAddress;

  // Do a backward insert to maintain the sort.
  for (i = FirstZeroEntry; i > 0; i--) {
    if (mPreValidatedRange[i-1].StartAddress <= mPreValidatedRange[i].StartAddress) {
      break;
    }
    // Swap.
    CopyMem(&Temp, &mPreValidatedRange[i], sizeof(Temp));
    CopyMem(&mPreValidatedRange[i], &mPreValidatedRange[i-1], sizeof(Temp));
    CopyMem(&mPreValidatedRange[i-1], &Temp, sizeof(Temp));
  }
}

STATIC
VOID
AddPreValidatedRange(
  IN PHYSICAL_ADDRESS StartAddress,
  IN PHYSICAL_ADDRESS EndAddress
  )
{
  UINTN i;
  UINTN Index;

  // Clean up the ranges.
  MergePreValidatedRanges();

  // Grow the input range to subsume all intersecting ranges.
  for (i = 0; i < ARRAY_SIZE(mPreValidatedRange) &&
              mPreValidatedRange[i].EndAddress != 0; i++) {
    if ((mPreValidatedRange[i].StartAddress < EndAddress) &&
        (StartAddress < mPreValidatedRange[i].EndAddress)) {
      StartAddress = MIN(StartAddress, mPreValidatedRange[i].StartAddress);
      EndAddress = MAX(EndAddress, mPreValidatedRange[i].EndAddress);
    }
  }

  // Remove all subsumed ranges.
  for (i = 0; i < ARRAY_SIZE(mPreValidatedRange) &&
              mPreValidatedRange[i].EndAddress != 0; i++) {
    if (DetectPreValidatedSubsumedIndex(StartAddress, EndAddress, &Index)) {
      RemovePreValidatedRange(Index);
    }
  }

  // Insert the range at the appropriate index.
  InsertPreValidatedRange(StartAddress, EndAddress);
}

/**
  Compares the StartAddress-EndAddress range against pre-validated ranges to
  check for overlap.

  @param[in] StartAddress        The start of a range to check overlap against.
  @param[in] EndAddress          The end of a range to check overlap against.
  @param[out] OverlapRange       If there is overlap, will contain the range
                                 that overlapped.

  @retval TRUE   The StartAddress to EndAddress range overlaps with the range
                 written to OverlapRange.
  @retval FALSE  The StartAddress to EndAddress range does not overlap with any
                 of pre-validated ranges.
 **/
BOOLEAN
EFIAPI
MemEncryptDetectPreValidatedOverlap (
  IN    PHYSICAL_ADDRESS              StartAddress,
  IN    PHYSICAL_ADDRESS              EndAddress,
  OUT   SEV_SNP_PRE_VALIDATED_RANGE   *OverlapRange
  )
{
  UINTN i;
  //
  // Check if the specified address range exist in pre-validated array.
  //
  for (i = 0; i < ARRAY_SIZE(mPreValidatedRange) &&
              // Also stop at the start of unclaimed 0,0 entries.
              mPreValidatedRange[i].EndAddress != 0; i++) {
    if ((mPreValidatedRange[i].StartAddress < EndAddress) &&
        (mPreValidatedRange[i].EndAddress > StartAddress)) {
      CopyMem(OverlapRange, &mPreValidatedRange[i], sizeof(*OverlapRange));
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Pre-validate the system RAM when SEV-SNP is enabled in the guest VM.
  Registers the memory range as pre-validated.

  @param[in]  BaseAddress             Base address
  @param[in]  NumPages                Number of pages starting from the base address

**/
VOID
EFIAPI
MemEncryptSevSnpPreValidateSystemRam (
  IN PHYSICAL_ADDRESS  BaseAddress,
  IN UINTN             NumPages
  )
{
  PHYSICAL_ADDRESS              CurrentAddress;
  PHYSICAL_ADDRESS              EndAddress;
  SEV_SNP_PRE_VALIDATED_RANGE   OverlapRange;
  EFI_STATUS                    Status;

  if (!MemEncryptSevSnpIsEnabled ()) {
    return;
  }

  CurrentAddress = BaseAddress;
  EndAddress = BaseAddress + EFI_PAGES_TO_SIZE (NumPages);

  //
  // The page table used in PEI can address up to 4GB memory. If we are asked to
  // validate a range above the 4GB, then create an identity mapping so that the
  // PVALIDATE instruction can execute correctly. If the page table entry is not
  // present then PVALIDATE will #GP.
  //
  if (CurrentAddress >= SIZE_4GB) {
    Status = InternalMemEncryptSevCreateIdentityMap1G (
                0,
                CurrentAddress,
                EFI_PAGES_TO_SIZE (NumPages)
                );
    if (EFI_ERROR (Status)) {
      ASSERT (FALSE);
      CpuDeadLoop ();
    }
  }

  while (CurrentAddress < EndAddress) {
    //
    // Check if the range overlaps with the pre-validated ranges.
    //
    if (MemEncryptDetectPreValidatedOverlap (CurrentAddress, EndAddress, &OverlapRange)) {
      // Validate the non-overlap regions.
      if (CurrentAddress < OverlapRange.StartAddress) {
        NumPages = EFI_SIZE_TO_PAGES (OverlapRange.StartAddress - CurrentAddress);

        InternalSetPageState (CurrentAddress, NumPages, SevSnpPagePrivate, TRUE);
      }

      CurrentAddress = OverlapRange.EndAddress;
      continue;
    }

    // Validate the remaining pages.
    NumPages = EFI_SIZE_TO_PAGES (EndAddress - CurrentAddress);
    InternalSetPageState (CurrentAddress, NumPages, SevSnpPagePrivate, TRUE);
    CurrentAddress = EndAddress;
  }
  AddPreValidatedRange(BaseAddress, EndAddress);
}

/**
  Accept pages system RAM when SEV-SNP is enabled in the guest VM.

  @param[in]  BaseAddress             Base address
  @param[in]  NumPages                Number of pages starting from the base address

**/
VOID
EFIAPI
MemEncryptSnpAcceptPages (
  IN PHYSICAL_ADDRESS           BaseAddress,
  IN UINTN                      NumPages
  )
{
  ASSERT (FALSE);
}
