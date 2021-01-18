/** @file

  Secure Encrypted Virtualization (SEV) library helper function

  Copyright (c) 2020, Advanced Micro Devices, Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemEncryptSevLib.h>
#include <Library/PcdLib.h>
#include <Register/Amd/Cpuid.h>
#include <Register/Amd/Msr.h>
#include <Register/Cpuid.h>
#include <Uefi/UefiBaseType.h>

#include "MemEncryptSnpPageState.h"

/**
  Reads and sets the status of SEV features.

  **/
STATIC
UINT32
EFIAPI
InternalMemEncryptSevStatus (
  VOID
  )
{
  UINT32                            RegEax;
  CPUID_MEMORY_ENCRYPTION_INFO_EAX  Eax;
  BOOLEAN                           ReadSevMsr;
  SEC_SEV_ES_WORK_AREA              *SevEsWorkArea;

  ReadSevMsr = FALSE;

  SevEsWorkArea = (SEC_SEV_ES_WORK_AREA *) FixedPcdGet32 (PcdSevEsWorkAreaBase);
  if (SevEsWorkArea != NULL && SevEsWorkArea->EncryptionMask != 0) {
    //
    // The MSR has been read before, so it is safe to read it again and avoid
    // having to validate the CPUID information.
    //
    ReadSevMsr = TRUE;
  } else {
    //
    // Check if memory encryption leaf exist
    //
    AsmCpuid (CPUID_EXTENDED_FUNCTION, &RegEax, NULL, NULL, NULL);
    if (RegEax >= CPUID_MEMORY_ENCRYPTION_INFO) {
      //
      // CPUID Fn8000_001F[EAX] Bit 1 (Sev supported)
      //
      AsmCpuid (CPUID_MEMORY_ENCRYPTION_INFO, &Eax.Uint32, NULL, NULL, NULL);

      if (Eax.Bits.SevBit) {
        ReadSevMsr = TRUE;
      }
    }
  }

  return ReadSevMsr ? AsmReadMsr32 (MSR_SEV_STATUS) : 0;
}

/**
  Returns a boolean to indicate whether SEV-SNP is enabled.

  @retval TRUE           SEV-SNP is enabled
  @retval FALSE          SEV-SNP is not enabled
**/
BOOLEAN
EFIAPI
MemEncryptSevSnpIsEnabled (
  VOID
  )
{
  MSR_SEV_STATUS_REGISTER           Msr;

  Msr.Uint32 = InternalMemEncryptSevStatus ();

  return Msr.Bits.SevSnpBit ? TRUE : FALSE;
}

/**
  Returns a boolean to indicate whether SEV-ES is enabled.

  @retval TRUE           SEV-ES is enabled
  @retval FALSE          SEV-ES is not enabled
**/
BOOLEAN
EFIAPI
MemEncryptSevEsIsEnabled (
  VOID
  )
{
  MSR_SEV_STATUS_REGISTER           Msr;

  Msr.Uint32 = InternalMemEncryptSevStatus ();

  return Msr.Bits.SevEsBit ? TRUE : FALSE;
}

/**
  Returns a boolean to indicate whether SEV is enabled.

  @retval TRUE           SEV is enabled
  @retval FALSE          SEV is not enabled
**/
BOOLEAN
EFIAPI
MemEncryptSevIsEnabled (
  VOID
  )
{
  MSR_SEV_STATUS_REGISTER           Msr;

  Msr.Uint32 = InternalMemEncryptSevStatus ();

  return Msr.Bits.SevBit ? TRUE : FALSE;
}

/**
  Returns the SEV encryption mask.

  @return  The SEV pagtable encryption mask
**/
UINT64
EFIAPI
MemEncryptSevGetEncryptionMask (
  VOID
  )
{
  CPUID_MEMORY_ENCRYPTION_INFO_EBX  Ebx;
  SEC_SEV_ES_WORK_AREA              *SevEsWorkArea;
  UINT64                            EncryptionMask;

  SevEsWorkArea = (SEC_SEV_ES_WORK_AREA *) FixedPcdGet32 (PcdSevEsWorkAreaBase);
  if (SevEsWorkArea != NULL) {
    EncryptionMask = SevEsWorkArea->EncryptionMask;
  } else {
    //
    // CPUID Fn8000_001F[EBX] Bit 0:5 (memory encryption bit position)
    //
    AsmCpuid (CPUID_MEMORY_ENCRYPTION_INFO, NULL, &Ebx.Uint32, NULL, NULL);
    EncryptionMask = LShiftU64 (1, Ebx.Bits.PtePosBits);
  }

  return EncryptionMask;
}

/**
  Locate the page range that covers the initial (pre-SMBASE-relocation) SMRAM
  Save State Map.

  @param[out] BaseAddress     The base address of the lowest-address page that
                              covers the initial SMRAM Save State Map.

  @param[out] NumberOfPages   The number of pages in the page range that covers
                              the initial SMRAM Save State Map.

  @retval RETURN_SUCCESS      BaseAddress and NumberOfPages have been set on
                              output.

  @retval RETURN_UNSUPPORTED  SMM is unavailable.
**/
RETURN_STATUS
EFIAPI
MemEncryptSevLocateInitialSmramSaveStateMapPages (
  OUT UINTN *BaseAddress,
  OUT UINTN *NumberOfPages
  )
{
  return RETURN_UNSUPPORTED;
}

/**
 Save the memory range in the SEV-ES workarea Ranges list so that others can
 retrieve it.

 @param[in]  BasesAddress     The base address of the memory to be added.

 @param[in]  NumPages         Number of pages.

 @param[in]  Type             Memory type
**/
STATIC
VOID
AddRangeToList (
  PHYSICAL_ADDRESS      BaseAddress,
  UINTN                 NumPages,
  MEM_OP_REQ            Type
  )
{
  SEC_SEV_ES_WORK_AREA       *SevEsWorkArea;
  SNP_PAGE_STATE_RANGE       *Range;
  UINTN                      Index;

  //
  // Save the updates in WorkArea so that it can be retrived in PeiPhase
  //
  SevEsWorkArea = (SEC_SEV_ES_WORK_AREA *) FixedPcdGet32 (PcdSevEsWorkAreaBase);
  Index = SevEsWorkArea->NumSnpPageStateRanges;
  Range = (SNP_PAGE_STATE_RANGE *)&SevEsWorkArea->SnpPageStateRanges[Index];
  Range->Start = BaseAddress;
  Range->End = BaseAddress + (EFI_PAGE_SIZE * NumPages);
  Range->Validated = Type == MemoryTypeShared ? FALSE : TRUE;
  SevEsWorkArea->NumSnpPageStateRanges++;
}

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
  )
{
  if (!MemEncryptSevSnpIsEnabled ()) {
    return EFI_SUCCESS;
  }

  return PvalidateInternal (BaseAddress, NumPages, Type);
}

/**
  This function issues the page state change request for the memory region specified the
  BaseAddress and NumPage. If Validate flags is trued then it also validates the memory
  after changing the page state.

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
MemEncryptSnpSetPageState (
  IN PHYSICAL_ADDRESS         BaseAddress,
  IN UINTN                    NumPages,
  IN MEM_OP_REQ               Type,
  IN BOOLEAN                  Pvalidate
  )
{
  EFI_STATUS                  Status;

  if (!MemEncryptSevSnpIsEnabled ()) {
    return EFI_SUCCESS;
  }

  //
  // If the page state need to be set to shared then first validate the memory
  // range before requesting to page state change.
  //
  if (Pvalidate && (Type == MemoryTypeShared)) {
    Status = MemEncryptPvalidate (BaseAddress, NumPages, Type);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  //
  // Request the page state change.
  //
  switch (Type) {
    case MemoryTypePrivate:
    case MemoryTypeShared:
          Status = SetPageStateInternal (BaseAddress, NumPages, Type);
          break;
    default: return RETURN_UNSUPPORTED;
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Now that pages are added in the RMP table, validate it.
  //
  if (Pvalidate && (Type == MemoryTypePrivate)) {
    Status = MemEncryptPvalidate (BaseAddress, NumPages, Type);

    //
    // Save the address range in the workarea
    //
    AddRangeToList (BaseAddress, NumPages, Type);
  }

  return Status;
}

RETURN_STATUS
EFIAPI
MemEncryptSevSecInit (
  VOID
  )
{
  UINTN       NumPages;

  if (!MemEncryptSevSnpIsEnabled ()) {
    return FALSE;
  }

  //
  // The memory range is pre-validated during the VMM launch sequence.
  //
  NumPages = EFI_SIZE_TO_PAGES(FixedPcdGet32 (PcdOvmfSnpLaunchValidatedEnd));
  AddRangeToList (FixedPcdGet32 (PcdOvmfSnpCpuidBase), NumPages, MemoryTypePrivate);

  return EFI_SUCCESS;
}
