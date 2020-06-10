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
#include <Library/BaseMemoryLib.h>

#include <Register/Amd/Ghcb.h>
#include <Register/Amd/Msr.h>

#define GHCB_INFO_MASK            0xfff
#define PAGES_PER_2MB             512
#define MEM_OP_MAX_NPAGES         4095
#define RMP_PAGE_SIZE_4K          0
#define RMP_PAGE_SIZE_2M          1
#define RMP_PAGES_COUNT(x, y)     (((y == RMP_PAGE_SIZE_4K) ? x : x * PAGES_PER_2MB))
#define IS_LARGE_PAGE(x)          ((x & 0x1fffff) == 0)
#define ROUND_UP(x, y)            (((x) + (y - 1)) & ~(y - 1))
#define ROUND_UP_2M(x)            ROUND_UP(x, 512)

/* pvalidate instruction error code */
#define PVALIDATE_FAIL_SIZEMISMATCH 6

/**
 The function calculates the size for a given number of RMP pages.
 */
STATIC
UINTN
CalRMPPagesToSize (
  UINTN       NumPages,
  UINT8       RmpPageSize
  )
{
  return EFI_PAGE_SIZE * RMP_PAGES_COUNT(NumPages, RmpPageSize);
}

/**
 The function call PVALIDATE instruction to validate the pages.
 */
STATIC
INTN
AsmPvalidate (
  IN EFI_PHYSICAL_ADDRESS         Start,
  IN UINTN                        NumPages,
  IN BOOLEAN                      RmpPageSize,
  IN BOOLEAN                      Validate
  )
{
  EFI_PHYSICAL_ADDRESS    End;

  End = Start + CalRMPPagesToSize(NumPages, RmpPageSize);


  while (Start < End) {
    INTN   Return;

    asm volatile (".byte 0xF2,0x0F,0x01,0xFF\n"
                  : "=a" (Return)
                  : "a"(Start), "c"(RmpPageSize), "d"(Validate) : "memory");

    // If we fail to validate due to size mismatch then try with small page size
    if ((Return == PVALIDATE_FAIL_SIZEMISMATCH) && IS_LARGE_PAGE(Start)) {
      Return = AsmPvalidate(Start, PAGES_PER_2MB, RMP_PAGE_SIZE_4K, Validate);
    }

    if (Return) {
      return EFI_SECURITY_VIOLATION;
    }

    Start += CalRMPPagesToSize(1, RmpPageSize);
  }

  return EFI_SUCCESS;
}

/**
 Maps the our MEM_OP_REQ type to GHCB Protocal MEM_OP command.
 */
STATIC
UINTN
MemoryTypeToGhcbProtoCmd (
  IN MEM_OP_REQ             Type
  )
{
  UINTN Cmd;

  switch (Type) {
    case MemoryTypeShared: Cmd = GHCB_INFO_SNP_MEM_OP_SHARED; break;
    case MemoryTypePrivate: Cmd = GHCB_INFO_SNP_MEM_OP_PRIVATE; break;
    default: ASSERT(0);
  }

  return Cmd;
}

/**
 Maps the our MEM_OP_REQ type to GHCB MEM_OP command.
 */
STATIC
UINTN
MemoryTypeToGhcbCmd (
  IN MEM_OP_REQ             Type
  )
{
  UINTN Cmd;

  switch (Type) {
    case MemoryTypeShared: Cmd = MEM_OP_TYPE_SHARED; break;
    case MemoryTypePrivate: Cmd = MEM_OP_TYPE_PRIVATE; break;
    default: ASSERT(0);
  }

  return Cmd;
}


/**
 The function builds the MEM_OP VMGEXIT command buffer for a specified address
 range and returns the remaining number of pages.
 */
STATIC
UINTN
BuildMemOpCmdBuf (
  EFI_PHYSICAL_ADDRESS      Start,
  UINTN                     NumOfPages,
  IN MEM_OP_REQ             Type,
  UINT8                     *Output,
  UINTN                     Length,
  EFI_PHYSICAL_ADDRESS      *NextStart
  )
{
  GHCB_MEM_OP_HDR           *Hdr;
  GHCB_MEM_OP               *Info;
  UINTN                     i, MaxNumEntries, Pages;

  //
  // Calculate the maximum number of entries
  //
  MaxNumEntries = (Length - sizeof (*Hdr)) / sizeof (*Info);

  SetMem (Output, Length, 0);

  Hdr = (GHCB_MEM_OP_HDR *) Output;
  Info = (GHCB_MEM_OP *) (Output + sizeof (*Hdr));

  //
  // Populate the entries
  //
  for (i = 0; NumOfPages && i < MaxNumEntries; i++) {
    Info->Data.GuestFrameNumber = Start >> EFI_PAGE_SHIFT;

    if (0 && (NumOfPages >= PAGES_PER_2MB)) {
      //
      // If remaining number of pages is greater than 512 and address is GFN
      // is aligned to large page then request a 2MB RMP entry.
      //
      if (IS_LARGE_PAGE(Start)) {
        Info->Data.RmpPageSize = RMP_PAGE_SIZE_2M;

        // Calculate the maximum number of large pages we can process in one go
        Info->Data.NumOfPages = MIN (NumOfPages / PAGES_PER_2MB, MEM_OP_MAX_NPAGES);
      } else {
        //
        // The number of pages are greater than 512 but the start address is not
        // 2MB aligned. Request to add the address range as a 4K.
        //
        Info->Data.RmpPageSize = RMP_PAGE_SIZE_4K;

        //
        // Round up to the next 2MB address and calculate the minimum number of pages
        // that need to be added as a 4K in the RMP table.
        Pages = ROUND_UP_2M(Info->Data.GuestFrameNumber) - Info->Data.GuestFrameNumber;
        Info->Data.NumOfPages = MIN (Pages, MEM_OP_MAX_NPAGES);
      }
    } else {
      //
      // We have less than 512 pages to work with, add all the pages as a 4K.
      Info->Data.RmpPageSize = RMP_PAGE_SIZE_4K;

      // Calculate the maximum number of the pages we can process in one go
      Info->Data.NumOfPages = MIN (NumOfPages, MEM_OP_MAX_NPAGES);
    }

    Info->Data.Type = MemoryTypeToGhcbCmd (Type);

    Hdr->Data.NumElements++;
    NumOfPages -= RMP_PAGES_COUNT(Info->Data.NumOfPages, Info->Data.RmpPageSize);
    Start += CalRMPPagesToSize(Info->Data.NumOfPages, Info->Data.RmpPageSize);
    Info++;
  }

  *NextStart = Start;
  return NumOfPages;
}

/**
 The function validates the memory range specified page range.
 */
STATIC
EFI_STATUS
MemEncryptPvalidateInternal (
  EFI_PHYSICAL_ADDRESS      Start,
  UINTN                     NumOfPages,
  IN MEM_OP_REQ             Type
  )
{
  GHCB                      Ghcb;
  GHCB_MEM_OP_HDR           *Hdr;
  MSR_SEV_ES_GHCB_REGISTER  Msr;
  GHCB_MEM_OP               *Info;
  EFI_PHYSICAL_ADDRESS      NextStart;
  UINTN                     i;
  EFI_STATUS                Status;
  BOOLEAN                   Validate;

  //
  // map Mem
  if (Type == MemoryTypePrivate) {
    Validate = TRUE;
  } else {
    Validate = FALSE;
  }

  //
  // If we don't have a valid GHCB then RMPUPDATEs were forced to 4K page size
  // so all the validation will happen as a 4K.
  //
  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);
  if ((Msr.GhcbPhysicalAddress & GHCB_INFO_MASK) != 0) {
    return AsmPvalidate (Start, NumOfPages, RMP_PAGE_SIZE_4K, Validate);
  }

  while (NumOfPages) {
    // Populate the Memop command entries in the shared buffer
    NumOfPages = BuildMemOpCmdBuf (Start, NumOfPages, Type, Ghcb.SharedBuffer,
                      sizeof Ghcb.SharedBuffer, &NextStart);

    // Issue pvalidate instruction
    Hdr = (GHCB_MEM_OP_HDR *) Ghcb.SharedBuffer;
    Info = (GHCB_MEM_OP *) ((UINT8 *) Ghcb.SharedBuffer + sizeof (*Hdr));
    for (i = 0; i < Hdr->Data.NumElements; i++, Info++) {
      Status = AsmPvalidate(Info->Data.GuestFrameNumber << EFI_PAGE_SHIFT,
                              Info->Data.NumOfPages,
                              Info->Data.RmpPageSize,
                              Validate);
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }

    Start = NextStart;
  }

  return EFI_SUCCESS;
}

/**
 The function uses the MEM_OP protocol method to request the RMPUPDATE for a
 specified memory range.
 */
STATIC
RETURN_STATUS
MemEncryptGhcbProtoRmpupdate (
  IN EFI_PHYSICAL_ADDRESS         Start,
  IN UINTN                        NumPages,
  IN MEM_OP_REQ                   Type
  )
{
  EFI_PHYSICAL_ADDRESS        End;
  MSR_SEV_ES_GHCB_REGISTER    Msr;

  End = Start + (NumPages * EFI_PAGE_SIZE);

  while (Start < End) {
    Msr.GhcbSnpMemOp.GuestFrameNumber = Start >> EFI_PAGE_SHIFT;
    Msr.GhcbSnpMemOp.RmpPageSize = RMP_PAGE_SIZE_4K; // hardcode to 4K
    Msr.GhcbSnpMemOp.Function = MemoryTypeToGhcbProtoCmd(Type);

    //
    // Use the GHCB MSR Protocol to update the page Information
    //
    AsmWriteMsr64 (MSR_SEV_ES_GHCB, Msr.GhcbPhysicalAddress);

    AsmVmgExit ();
    Start += EFI_PAGE_SIZE;
  }

  return EFI_SUCCESS;
}

/**
 The function causes a VMGEXIT to add the requested page range into the RMP table.
 */
STATIC
RETURN_STATUS
MemEncryptRmpUpdateInternal (
  IN EFI_PHYSICAL_ADDRESS         BaseAddress,
  IN UINTN                        NumPages,
  IN MEM_OP_REQ                   Type
  )
{
  EFI_STATUS                Status;
  GHCB                      *Ghcb;
  MSR_SEV_ES_GHCB_REGISTER  Msr;
  GHCB_MEM_OP_HDR           *Hdr;
  EFI_PHYSICAL_ADDRESS      NextStart;
  UINTN                     NumElements;

  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);
  Ghcb = Msr.Ghcb;

  //
  // If we don't have a valid GHCB then use the MEM_OP protocol
  //
  if ((Msr.GhcbPhysicalAddress & GHCB_INFO_MASK) != 0) {
    return MemEncryptGhcbProtoRmpupdate (BaseAddress, NumPages, Type);
  }

  //
  // NOTE: Do not use the DEBUG statement inside the loop as it can cause
  // a #VC and messup the GHCB block prepared for the MEM_OP VMGEXIT.
  //
  while (NumPages) {
    //
    // Initialize the GHCB and setup scratch sw to point to shared buffer
    //
    VmgInit (Ghcb);
    Hdr = (GHCB_MEM_OP_HDR *) Ghcb->SharedBuffer;

    // Populate the Memop command entries in the shared buffer
    NumPages = BuildMemOpCmdBuf (BaseAddress, NumPages, Type, Ghcb->SharedBuffer,
                      sizeof Ghcb->SharedBuffer, &NextStart);

    Ghcb->SaveArea.SwScratch = (UINT64) Ghcb->SharedBuffer;
    NumElements = Hdr->Data.NumElements;

    // 
    // Issue the VMGEXIT and retry if hypervisor failed to process all
    // the elements at once.
    //
    do {
      Hdr->Data.NumElements = NumElements;

      Status = VmgExit (Ghcb, SVM_EXIT_MEM_OP, 0, 0);
      if (EFI_ERROR (Status)) {
        return Status;
      }
    } while (Hdr->Data.NumElements);

    BaseAddress = NextStart;

    VmgDone (Ghcb);
  }

  return EFI_SUCCESS;
}

/**
 This function issues the PVALIDATE instruction for the memory range specified
 in the BaseAddress and NumPages.

  @param[in]  BaseAddress             The physical address that is the start
                                      address of a memory region.
  @param[in]  NumPages                The number of pages from start memory
                                      region.
  @param[in]  Validate                0 or 1 (0 validate, 1 unvalidate)

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
  )
{
  return MemEncryptPvalidateInternal (BaseAddress, NumPages, Type);
}
  
RETURN_STATUS
EFIAPI
MemEncryptRmpupdate (
  IN PHYSICAL_ADDRESS         BaseAddress,
  IN UINTN                    NumPages,
  IN MEM_OP_REQ               Type,
  IN BOOLEAN                  Pvalidate
  )
{
  EFI_STATUS      Status;

  switch (Type) {
    case MemoryTypePrivate:
    case MemoryTypeShared:
          Status = MemEncryptRmpUpdateInternal (BaseAddress, NumPages, Type);
          break;
    default: return RETURN_UNSUPPORTED;
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Pvalidate) {
    Status = MemEncryptPvalidate (BaseAddress, NumPages, Type);
  }

  return Status;
}
