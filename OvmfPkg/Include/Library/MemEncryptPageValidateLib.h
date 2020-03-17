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
  This function validate the memory region specified the BaseAddress and NumPage.

  The caller must ensure that page table is in correct state asking for the memory
  state changes.

  @param[in]  BaseAddress             The physical address that is the start
                                      address of a memory region.
  @param[in]  NumPages                The number of pages from start memory
                                      region.

  @retval RETURN_SUCCESS              The attributes were cleared for the
                                      memory region.
  @retval RETURN_INVALID_PARAMETER    Number of pages is zero.
**/
RETURN_STATUS
EFIAPI
MemEncryptPageValidate (
  IN PHYSICAL_ADDRESS         BaseAddress,
  IN UINTN                    NumPages
  );

/**
  This function unvalidate the memory region specified the BaseAddress and NumPage.

  The caller must ensure that page table is in correct state asking for the memory
  state changes.

  @param[in]  BaseAddress             The physical address that is the start
                                      address of a memory region.
  @param[in]  NumPages                The number of pages from start memory
                                      region.

  @retval RETURN_SUCCESS              The attributes were cleared for the
                                      memory region.
  @retval RETURN_INVALID_PARAMETER    Number of pages is zero.
**/
RETURN_STATUS
EFIAPI
MemEncryptPageUnvalidate (
  IN PHYSICAL_ADDRESS         BaseAddress,
  IN UINTN                    NumPages
  );

/**
  This function issues the MemOp request for the memory region specified the
  BaseAddress and NumPage.

  The caller must ensure that page table is in correct state asking for the memory
  state changes.

  @param[in]  BaseAddress             The physical address that is the start
                                      address of a memory region.
  @param[in]  NumPages                The number of pages from start memory
                                      region.
  @param[in]  Type                    Memory operation command

  @retval RETURN_SUCCESS              The attributes were cleared for the
                                      memory region.
  @retval RETURN_INVALID_PARAMETER    Number of pages is zero.
**/
RETURN_STATUS
EFIAPI
MemEncryptMemOpRequest (
  IN PHYSICAL_ADDRESS         BaseAddress,
  IN UINTN                    NumPages,
  IN MEM_OP_REQ               Type
  );
#endif // _MEM_ENCRYPT_SEV_SNP_LIB_H_
