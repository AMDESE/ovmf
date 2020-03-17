/** @file

  Secure Encrypted Virtualization (SEV) library helper function

  Copyright (c) 2020, AMD Incorporated. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/VmgExitLib.h>
#include <Library/MemEncryptPageValidateLib.h>

#include <Register/Amd/Ghcb.h>
#include <Register/Amd/Msr.h>

#define GHCB_INFO_MASK       0xfff

/**
 Function returns boolean indicating whether the full GHCB is active or not.

 */
STATIC
BOOLEAN
GhcbIsActive (
  VOID
  )
{
  MSR_SEV_ES_GHCB_REGISTER  Msr;

  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);

  return ((Msr.GhcbPhysicalAddress & GHCB_INFO_MASK) == 0);
}

STATIC
INTN
IssuePvalidate (
  IN EFI_PHYSICAL_ADDRESS         Start,
  IN UINTN                        NumPages,
  IN BOOLEAN                      RmpPageSize,
  IN BOOLEAN                      Validate
  )
{
  EFI_PHYSICAL_ADDRESS    End;

  End = Start + EFI_PAGES_TO_SIZE(NumPages);
  while (Start < End) {
    INTN   Return;

    asm volatile (".byte 0xF2,0x0F,0x01,0xFF\n"
                  : "=a" (Return)
                  : "a"(Start), "c"(RmpPageSize), "d"(Validate) : "memory");
    if (Return) {
      return EFI_SECURITY_VIOLATION;
    }

    Start += EFI_PAGE_SIZE;
  }

  return EFI_SUCCESS;
}

STATIC
UINTN
SnpMemTypeToGhcbInfoCmd (
  IN    UINTN MemType
  )
{
  UINTN Cmd;

  switch (MemType) {
    case MEM_OP_TYPE_SHARED: Cmd = GHCB_INFO_SNP_MEM_OP_SHARED; break;
    case MEM_OP_TYPE_PRIVATE: Cmd = GHCB_INFO_SNP_MEM_OP_PRIVATE; break;
    default: ASSERT(TRUE);
  }

  return Cmd;
}

STATIC
RETURN_STATUS
GhcbInfoSnpMemOperation (
  IN EFI_PHYSICAL_ADDRESS         Start,
  IN UINTN                        NumPages,
  IN UINTN                        Type
  )
{
  EFI_PHYSICAL_ADDRESS        End;
  MSR_SEV_ES_GHCB_REGISTER    Msr;

  End = Start + (NumPages * EFI_PAGE_SIZE);

  while (Start < End) {
    Msr.GhcbSnpMemOp.GuestFrameNumber = Start >> EFI_PAGE_SHIFT;
    Msr.GhcbSnpMemOp.RmpPageSize = 0;
    Msr.GhcbSnpMemOp.Function = SnpMemTypeToGhcbInfoCmd(Type);

    //
    // Use the GHCB MSR Protocol to update the page Information
    //
    AsmWriteMsr64 (MSR_SEV_ES_GHCB, Msr.GhcbPhysicalAddress);

    AsmVmgExit ();

    Start += EFI_PAGE_SIZE;
  }

  return EFI_SUCCESS;
}

STATIC
RETURN_STATUS
MemEncryptSevSnpShared (
  IN EFI_PHYSICAL_ADDRESS         BaseAddress,
  IN UINTN                        NumPages
  )
{
  //
  // Request the RMP table changes
  //
  if (GhcbIsActive ()) {
    return VmgSnpMemOperation (BaseAddress, NumPages, MEM_OP_TYPE_SHARED);
  } else {
    return GhcbInfoSnpMemOperation (BaseAddress, NumPages, MEM_OP_TYPE_SHARED);
  }
}

STATIC
RETURN_STATUS
MemEncryptSevSnpPrivate (
  IN EFI_PHYSICAL_ADDRESS         BaseAddress,
  IN UINTN                        NumPages
  )
{
  EFI_STATUS              Status;

  //
  // Request the RMP table changes
  //
  if (GhcbIsActive ()) {
    Status = VmgSnpMemOperation (BaseAddress, NumPages, MEM_OP_TYPE_PRIVATE);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  } else {
    Status = GhcbInfoSnpMemOperation (BaseAddress, NumPages, MEM_OP_TYPE_PRIVATE);
  }

  //
  // Issue PVALIDATE instruction to validate the existing page.
  //
  Status = IssuePvalidate (BaseAddress, NumPages, 0, 1);
  if (EFI_ERROR (Status)) {
    return EFI_SECURITY_VIOLATION;
  }

  return EFI_SUCCESS;
}
  
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
  )
{
  if (NumPages == 0) {
    return RETURN_INVALID_PARAMETER;
  }

  return MemEncryptSevSnpPrivate (BaseAddress, NumPages);
}

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
  )
{
  if (NumPages == 0) {
    return RETURN_INVALID_PARAMETER;
  }

  return MemEncryptSevSnpShared (BaseAddress, NumPages);
}

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
  )
{
  switch (Type) {
    case MemoryTypePrivate: return  MemEncryptSevSnpPrivate (BaseAddress, NumPages);
    case MemoryTypeShared:  return MemEncryptSevSnpShared (BaseAddress, NumPages);
    default: return RETURN_UNSUPPORTED;
  }

  return RETURN_UNSUPPORTED;
}
