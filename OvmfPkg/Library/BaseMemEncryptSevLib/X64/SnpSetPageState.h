/** @file

  SEV-SNP Page Validation functions.

  Copyright (c) 2020 - 2021, AMD Incorporated. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PEI_DXE_SNP_PAGE_STATE_INTERNAL_H_
#define PEI_DXE_SNP_PAGE_STATE_INTERNAL_H_

#include "../SnpPageStateChange.h"

VOID
SnpSetMemoryPrivate (
  IN  PHYSICAL_ADDRESS      PhysicalAddress,
  IN  UINTN                 Length
  );

VOID
SnpSetMemoryShared (
  IN  PHYSICAL_ADDRESS      PhysicalAddress,
  IN  UINTN                 Length
  );
#endif
