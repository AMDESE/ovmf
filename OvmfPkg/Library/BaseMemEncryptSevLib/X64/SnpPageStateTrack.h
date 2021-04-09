/** @file


  Copyright (c) 2020 - 2021, AMD Incorporated. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SNP_PAGE_STATE_TRACK_INTERNAL_H_
#define SNP_PAGE_STATE_TRACK_INTERNAL_H_

#include <Uefi/UefiBaseType.h>

typedef struct SNP_VALIDATED_RANGE {
  UINT64    StartAddress, EndAddress;
  UINT64    MaxAddress;

  struct SNP_VALIDATED_RANGE  *Left, *Right;
} SNP_VALIDATED_RANGE;

SNP_VALIDATED_RANGE *
FindOverlapRange (
  IN  SNP_VALIDATED_RANGE   *RootNode,
  IN  UINTN                 StartAddress,
  IN  UINTN                 EndAddress
  );

SNP_VALIDATED_RANGE *
AddRangeToIntervalTree (
  IN  SNP_VALIDATED_RANGE   *RootNode,
  IN  UINTN                 StartAddress,
  IN  UINTN                 EndAddress
  );

#endif
