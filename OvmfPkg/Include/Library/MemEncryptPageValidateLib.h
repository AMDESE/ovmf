/** @file

  Define Secure Encrypted Virtualization (SEV) base library helper function

  Copyright (c) 2020, AMD Incorporated. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _MEM_ENCRYPT_PAGE_VALIDATE_LIB_H_
#define _MEM_ENCRYPT_PAGE_VALIDATE_LIB_H_

#include <Base.h>

typedef enum {
  MemoryTypePrivate,
  MemoryTypeShared,

  MemoryTypeMax
} MEM_OP_REQ;

/**
 This function issues the PVALIDATE instruction for the memory range specified
 in the BaseAddress and NumPages.

  @param[in]  BaseAddress             The physical address that is the start
                                      address of a memory region.
  @param[in]  NumPages                The number of pages from start memory
                                      region.
  @param[in]  Type                    Memory operation command

  @retval RETURN_SUCCESS              The attributes were cleared for the
                                      memory region.
  @retval RETURN_INVALID_PARAMETER    Number of pages is zero.
  @return RETURN_SECRITY_VIOLATION    Pvalidate instruction failed.
  */
RETURN_STATUS
EFIAPI
MemEncryptPvalidate (
  IN PHYSICAL_ADDRESS         BaseAddress,
  IN UINTN                    NumPages,
  IN MEM_OP_REQ               Type
  );

/**
  This function issues the RMPUPDATE request for the memory region specified the
  BaseAddress and NumPage. If Validate flags is trued then it also issues
  the PVALIDATE instruction to change the page state after the RMPUPDATE.

  @param[in]  BaseAddress             The physical address that is the start
                                      address of a memory region.
  @param[in]  NumPages                The number of pages from start memory
                                      region.
  @param[in]  Type                    Memory operation command
  @param[in]  PValidate               Pvalidate the memory range

  @retval RETURN_SUCCESS              The attributes were cleared for the
                                      memory region.
  @retval RETURN_INVALID_PARAMETER    Number of pages is zero.
**/
RETURN_STATUS
EFIAPI
MemEncryptRmpupdate (
  IN PHYSICAL_ADDRESS         BaseAddress,
  IN UINTN                    NumPages,
  IN MEM_OP_REQ               Type,
  IN BOOLEAN                  Pvalidate
  );
#endif // _MEM_ENCRYPT_SEV_SNP_LIB_H_
