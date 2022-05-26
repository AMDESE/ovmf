/**@file
  Initialize Secure Encrypted Virtualization (SEV) support

  Copyright (c) 2017 - 2020, Advanced Micro Devices. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
//
// The package level header files this module uses
//
#include <IndustryStandard/Q35MchIch9.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemEncryptSevLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <PiPei.h>
#include <Register/Amd/Msr.h>
#include <Register/Intel/SmramSaveStateMap.h>
#include <Library/VmgExitLib.h>
#include <ConfidentialComputingGuestAttr.h>

#include "Platform.h"

#define MEGABYTE_SHIFT     20

UINT64  mSevSnpAcceptMemSize = 0;

STATIC
UINTN
GetAcceptSize (
  VOID
  )
{
  UINTN AcceptSize;
  AcceptSize = FixedPcdGet64 (PcdSevSnpAcceptPartialMemorySize);

  //
  // If specified accept size is equal to or less than zero, accept all of the memory.
  // Else transfer the size in megabyte to the number in byte.
  //
  if (AcceptSize <= 0) {
    AcceptSize = ~(UINT64) 0;
    return AcceptSize;
  }

  return AcceptSize << MEGABYTE_SHIFT;
}

STATIC
UINT64
GetHypervisorFeature (
  VOID
  );

/**
  Initialize SEV-SNP support if running as an SEV-SNP guest.

**/
STATIC
VOID
AmdSevSnpInitialize (
  VOID
  )
{
  EFI_PEI_HOB_POINTERS         Hob;
  EFI_HOB_RESOURCE_DESCRIPTOR  *ResourceHob;
  UINT64                       HvFeatures;
  EFI_STATUS                   PcdStatus;
  EFI_PHYSICAL_ADDRESS          PhysicalStart;
  EFI_PHYSICAL_ADDRESS          PhysicalEnd;
  UINT64                        ResourceLength;
  UINT64                        PeiPartialAccept;
  EFI_PHYSICAL_ADDRESS          PeiMemoryEnd;
  EFI_PHYSICAL_ADDRESS          PreAcceptedStart;
  EFI_PHYSICAL_ADDRESS          PreAcceptedEnd;
  UINT64                        AccumulateAccepted;

  if (!MemEncryptSevSnpIsEnabled ()) {
    return;
  }

  mSevSnpAcceptMemSize = GetAcceptSize ();

  // PEI free memory overlaps with system memory higher than the acceptmemsize,
  // and should be explicitly accepted. If partial acceptance overlaps with the
  // PEI region, then PeiPartialAccept is the amount over the memory base that
  // is accepted.
  PeiPartialAccept = 0;
  PeiMemoryEnd = mPeiMemoryBase + mPeiMemoryLength;
  //
  // Query the hypervisor feature using the VmgExit and set the value in the
  // hypervisor features PCD.
  //
  HvFeatures = GetHypervisorFeature ();
  PcdStatus  = PcdSet64S (PcdGhcbHypervisorFeatures, HvFeatures);
  ASSERT_RETURN_ERROR (PcdStatus);

  AccumulateAccepted = 0;
  PreAcceptedStart = ~(EFI_PHYSICAL_ADDRESS)0;
  PreAcceptedEnd = 0;
  //
  // Iterate through the system RAM and validate up to mSevSnpAcceptMemSize.
  //
  for (Hob.Raw = GetHobList (); !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
    if (Hob.Raw != NULL && GET_HOB_TYPE (Hob) == EFI_HOB_TYPE_RESOURCE_DESCRIPTOR &&
        AccumulateAccepted < mSevSnpAcceptMemSize) {
        ResourceHob = Hob.ResourceDescriptor;

        if (ResourceHob->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) {
          ResourceLength = Hob.ResourceDescriptor->ResourceLength;
          PhysicalStart = ResourceHob->PhysicalStart;
          PhysicalEnd = PhysicalStart + ResourceLength;

          if (AccumulateAccepted + ResourceLength > mSevSnpAcceptMemSize) {
          //
          // If the memory can't be accepted completely, accept the part of it
          // to meet the SEV_SNP_PARTIAL_ACCEPTED_MEM_SIZE.
          //
          ResourceLength = mSevSnpAcceptMemSize - AccumulateAccepted;
          PhysicalEnd = ResourceHob->PhysicalStart + ResourceLength;
        }

        // If this is the hob that contains PEI memory, account for what is
        // accepted above memorybase.
        if (PhysicalEnd > mPeiMemoryBase &&
            PhysicalStart < PeiMemoryEnd) {
          PeiPartialAccept = PhysicalEnd - mPeiMemoryBase;
        }

        MemEncryptSevSnpPreValidateSystemRam (
          ResourceHob->PhysicalStart,
          EFI_SIZE_TO_PAGES ((UINTN) ResourceLength)
          );
        AccumulateAccepted += ResourceLength;
      }
    }
  }

  // Now accept what hasn't been accepted of PEI memory.
  if (mPeiMemoryBase + PeiPartialAccept < PeiMemoryEnd) {
    MemEncryptSevSnpPreValidateSystemRam (
      mPeiMemoryBase + PeiPartialAccept,
      EFI_SIZE_TO_PAGES (mPeiMemoryLength - PeiPartialAccept)
      );
  }
}

/**
  Transfer the PEI HobList to the final HobList for Dxe
**/
VOID
AmdSevTransferHobs (
  VOID
  )
{
  EFI_PEI_HOB_POINTERS        Hob;
  EFI_RESOURCE_TYPE           ResourceType;
  EFI_RESOURCE_ATTRIBUTE_TYPE ResourceAttribute;
  EFI_RESOURCE_ATTRIBUTE_TYPE ResourceAttributeUnaccepted;
  EFI_PHYSICAL_ADDRESS        PhysicalStart;
  EFI_PHYSICAL_ADDRESS        PhysicalEnd;
  EFI_PHYSICAL_ADDRESS        PeiMemoryEnd;
  SEV_SNP_PRE_VALIDATED_RANGE OverlapRange;
  UINT64                      ResourceLength;

  if (!MemEncryptSevSnpIsEnabled ()) {
    return;
  }

  // PEI free memory overlaps with system memory higher than the acceptmemsize,
  // and should be explicitly accepted. If partial acceptance overlaps with the
  // PEI region, then PeiPartialAccept is the amount over the memory base that
  // is accepted.
  PeiMemoryEnd = mPeiMemoryBase + mPeiMemoryLength;

  for (Hob.Raw = GetHobList(); !END_OF_HOB_LIST (Hob);  Hob.Raw = GET_NEXT_HOB (Hob)) {
    if (Hob.Header->HobType == EFI_HOB_TYPE_RESOURCE_DESCRIPTOR) {
      ResourceAttribute = Hob.ResourceDescriptor->ResourceAttribute;
      ResourceLength    = Hob.ResourceDescriptor->ResourceLength;
      ResourceType      = Hob.ResourceDescriptor->ResourceType;
      PhysicalStart     = Hob.ResourceDescriptor->PhysicalStart;

      if (ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) {
        ResourceAttribute |= EFI_RESOURCE_ATTRIBUTE_PRESENT | EFI_RESOURCE_ATTRIBUTE_INITIALIZED;
        ResourceAttributeUnaccepted = ResourceAttribute &
           ~(EFI_RESOURCE_ATTRIBUTE_TESTED | EFI_RESOURCE_ATTRIBUTE_ENCRYPTED);
        PhysicalEnd = PhysicalStart + ResourceLength;

        // This HOB can overlap with multiple pre-accepted ranges. A HOB never
        // intersects and does not overlap with a pre-accepted range.
        //
        // We'll get each overlapped range in increasing order.
        //
        // For each range that it overlaps, there are three potential ranges to
        // consider:
        //   - Memory before the overlap: Known to be unaccepted.
        //   - The overlapped memory: Known accepted (system memory).
        //   - Memory after the overlap: Either not in the HOB or unaccepted
        //                               followed by unknown (never accepted).
        //
        // The memory after the overlap, if it is in the HOB, will be added as
        // SYSTEM_MEMORY in its entirety. This iteration will reach it later and
        // split and classify it appropriately.
        //
        if (!MemEncryptDetectPreValidatedOverlap(PhysicalStart, PhysicalEnd, &OverlapRange)) {
          ResourceType = EFI_RESOURCE_MEMORY_UNACCEPTED;
          ResourceAttribute = ResourceAttributeUnaccepted;
        } else {
          // TODO(dionnaglaze): Keep the pre-validated as EFI_RESOURCE_SYSTEM_MEMORY,
          // and all other parts of of HOB as unaccepted.
          if (PhysicalStart < OverlapRange.StartAddress) {
            BuildResourceDescriptorHob(
                EFI_RESOURCE_MEMORY_UNACCEPTED,
                ResourceAttributeUnaccepted,
                PhysicalStart,
                OverlapRange.StartAddress - PhysicalStart);
          }
          PhysicalStart = OverlapRange.StartAddress;
          ResourceLength = OverlapRange.EndAddress - OverlapRange.StartAddress;

          if (PhysicalEnd > OverlapRange.EndAddress) {
            // There is more memory in the HOB after the overlapped region. Add
            // another SYSTEM_MEMORY hob for processing later in the iteration.
            BuildResourceDescriptorHob(
                EFI_RESOURCE_SYSTEM_MEMORY,
                ResourceAttribute,
                OverlapRange.EndAddress,
                PhysicalEnd - OverlapRange.EndAddress);
          }
        }

        Hob.ResourceDescriptor->ResourceAttribute = ResourceAttribute;
        Hob.ResourceDescriptor->ResourceLength = ResourceLength;
        Hob.ResourceDescriptor->PhysicalStart = PhysicalStart;
        Hob.ResourceDescriptor->ResourceType = ResourceType;
       }
    }
  }
}

/**
  Handle an SEV-SNP/GHCB protocol check failure.

  Notify the hypervisor using the VMGEXIT instruction that the SEV-SNP guest
  wishes to be terminated.

  @param[in] ReasonCode  Reason code to provide to the hypervisor for the
                         termination request.

**/
STATIC
VOID
SevEsProtocolFailure (
  IN UINT8  ReasonCode
  )
{
  MSR_SEV_ES_GHCB_REGISTER  Msr;

  //
  // Use the GHCB MSR Protocol to request termination by the hypervisor
  //
  Msr.GhcbPhysicalAddress         = 0;
  Msr.GhcbTerminate.Function      = GHCB_INFO_TERMINATE_REQUEST;
  Msr.GhcbTerminate.ReasonCodeSet = GHCB_TERMINATE_GHCB;
  Msr.GhcbTerminate.ReasonCode    = ReasonCode;
  AsmWriteMsr64 (MSR_SEV_ES_GHCB, Msr.GhcbPhysicalAddress);

  AsmVmgExit ();

  ASSERT (FALSE);
  CpuDeadLoop ();
}

/**
 Get the hypervisor features bitmap

**/
STATIC
UINT64
GetHypervisorFeature (
  VOID
  )
{
  UINT64                    Status;
  GHCB                      *Ghcb;
  MSR_SEV_ES_GHCB_REGISTER  Msr;
  BOOLEAN                   InterruptState;
  UINT64                    Features;

  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);
  Ghcb                    = Msr.Ghcb;

  //
  // Initialize the GHCB
  //
  VmgInit (Ghcb, &InterruptState);

  //
  // Query the Hypervisor Features.
  //
  Status = VmgExit (Ghcb, SVM_EXIT_HYPERVISOR_FEATURES, 0, 0);
  if ((Status != 0)) {
    SevEsProtocolFailure (GHCB_TERMINATE_GHCB_GENERAL);
  }

  Features = Ghcb->SaveArea.SwExitInfo2;

  VmgDone (Ghcb, InterruptState);

  return Features;
}

/**

  This function can be used to register the GHCB GPA.

  @param[in]  Address           The physical address to be registered.

**/
STATIC
VOID
GhcbRegister (
  IN  EFI_PHYSICAL_ADDRESS  Address
  )
{
  MSR_SEV_ES_GHCB_REGISTER  Msr;
  MSR_SEV_ES_GHCB_REGISTER  CurrentMsr;

  //
  // Save the current MSR Value
  //
  CurrentMsr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);

  //
  // Use the GHCB MSR Protocol to request to register the GPA.
  //
  Msr.GhcbPhysicalAddress      = Address & ~EFI_PAGE_MASK;
  Msr.GhcbGpaRegister.Function = GHCB_INFO_GHCB_GPA_REGISTER_REQUEST;
  AsmWriteMsr64 (MSR_SEV_ES_GHCB, Msr.GhcbPhysicalAddress);

  AsmVmgExit ();

  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);

  //
  // If hypervisor responded with a different GPA than requested then fail.
  //
  if ((Msr.GhcbGpaRegister.Function != GHCB_INFO_GHCB_GPA_REGISTER_RESPONSE) ||
      ((Msr.GhcbPhysicalAddress & ~EFI_PAGE_MASK) != Address))
  {
    SevEsProtocolFailure (GHCB_TERMINATE_GHCB_GENERAL);
  }

  //
  // Restore the MSR
  //
  AsmWriteMsr64 (MSR_SEV_ES_GHCB, CurrentMsr.GhcbPhysicalAddress);
}

/**

  Initialize SEV-ES support if running as an SEV-ES guest.

  **/
STATIC
VOID
AmdSevEsInitialize (
  VOID
  )
{
  UINT8                *GhcbBase;
  PHYSICAL_ADDRESS     GhcbBasePa;
  UINTN                GhcbPageCount;
  UINT8                *GhcbBackupBase;
  UINT8                *GhcbBackupPages;
  UINTN                GhcbBackupPageCount;
  SEV_ES_PER_CPU_DATA  *SevEsData;
  UINTN                PageCount;
  RETURN_STATUS        PcdStatus, DecryptStatus;
  IA32_DESCRIPTOR      Gdtr;
  VOID                 *Gdt;

  if (!MemEncryptSevEsIsEnabled ()) {
    return;
  }

  PcdStatus = PcdSetBoolS (PcdSevEsIsEnabled, TRUE);
  ASSERT_RETURN_ERROR (PcdStatus);

  //
  // Allocate GHCB and per-CPU variable pages.
  //   Since the pages must survive across the UEFI to OS transition
  //   make them reserved.
  //
  GhcbPageCount = mMaxCpuCount * 2;
  GhcbBase      = AllocateReservedPages (GhcbPageCount);
  ASSERT (GhcbBase != NULL);

  GhcbBasePa = (PHYSICAL_ADDRESS)(UINTN)GhcbBase;

  //
  // Each vCPU gets two consecutive pages, the first is the GHCB and the
  // second is the per-CPU variable page. Loop through the allocation and
  // only clear the encryption mask for the GHCB pages.
  //
  for (PageCount = 0; PageCount < GhcbPageCount; PageCount += 2) {
    DecryptStatus = MemEncryptSevClearPageEncMask (
                      0,
                      GhcbBasePa + EFI_PAGES_TO_SIZE (PageCount),
                      1
                      );
    ASSERT_RETURN_ERROR (DecryptStatus);
  }

  ZeroMem (GhcbBase, EFI_PAGES_TO_SIZE (GhcbPageCount));

  PcdStatus = PcdSet64S (PcdGhcbBase, GhcbBasePa);
  ASSERT_RETURN_ERROR (PcdStatus);
  PcdStatus = PcdSet64S (PcdGhcbSize, EFI_PAGES_TO_SIZE (GhcbPageCount));
  ASSERT_RETURN_ERROR (PcdStatus);

  DEBUG ((
    DEBUG_INFO,
    "SEV-ES is enabled, %lu GHCB pages allocated starting at 0x%p\n",
    (UINT64)GhcbPageCount,
    GhcbBase
    ));

  //
  // Allocate #VC recursion backup pages. The number of backup pages needed is
  // one less than the maximum VC count.
  //
  GhcbBackupPageCount = mMaxCpuCount * (VMGEXIT_MAXIMUM_VC_COUNT - 1);
  GhcbBackupBase      = AllocatePages (GhcbBackupPageCount);
  ASSERT (GhcbBackupBase != NULL);

  GhcbBackupPages = GhcbBackupBase;
  for (PageCount = 1; PageCount < GhcbPageCount; PageCount += 2) {
    SevEsData =
      (SEV_ES_PER_CPU_DATA *)(GhcbBase + EFI_PAGES_TO_SIZE (PageCount));
    SevEsData->GhcbBackupPages = GhcbBackupPages;

    GhcbBackupPages += EFI_PAGE_SIZE * (VMGEXIT_MAXIMUM_VC_COUNT - 1);
  }

  DEBUG ((
    DEBUG_INFO,
    "SEV-ES is enabled, %lu GHCB backup pages allocated starting at 0x%p\n",
    (UINT64)GhcbBackupPageCount,
    GhcbBackupBase
    ));

  //
  // SEV-SNP guest requires that GHCB GPA must be registered before using it.
  //
  if (MemEncryptSevSnpIsEnabled ()) {
    GhcbRegister (GhcbBasePa);
  }

  AsmWriteMsr64 (MSR_SEV_ES_GHCB, GhcbBasePa);

  //
  // The SEV support will clear the C-bit from non-RAM areas.  The early GDT
  // lives in a non-RAM area, so when an exception occurs (like a #VC) the GDT
  // will be read as un-encrypted even though it was created before the C-bit
  // was cleared (encrypted). This will result in a failure to be able to
  // handle the exception.
  //
  AsmReadGdtr (&Gdtr);

  Gdt = AllocatePages (EFI_SIZE_TO_PAGES ((UINTN)Gdtr.Limit + 1));
  ASSERT (Gdt != NULL);

  CopyMem (Gdt, (VOID *)Gdtr.Base, Gdtr.Limit + 1);
  Gdtr.Base = (UINTN)Gdt;
  AsmWriteGdtr (&Gdtr);
}

/**

  Function checks if SEV support is available, if present then it sets
  the dynamic PcdPteMemoryEncryptionAddressOrMask with memory encryption mask.

  **/
VOID
AmdSevInitialize (
  VOID
  )
{
  UINT64         EncryptionMask;
  RETURN_STATUS  PcdStatus;

  //
  // Check if SEV is enabled
  //
  if (!MemEncryptSevIsEnabled ()) {
    return;
  }

  //
  // Check and perform SEV-SNP initialization if required. This need to be
  // done before the GHCB page is made shared in the AmdSevEsInitialize(). This
  // is because the system RAM must be validated before it is made shared.
  // The AmdSevSnpInitialize() validates the system RAM.
  //
  AmdSevSnpInitialize ();

  //
  // Set Memory Encryption Mask PCD
  //
  EncryptionMask = MemEncryptSevGetEncryptionMask ();
  PcdStatus      = PcdSet64S (PcdPteMemoryEncryptionAddressOrMask, EncryptionMask);
  ASSERT_RETURN_ERROR (PcdStatus);

  DEBUG ((DEBUG_INFO, "SEV is enabled (mask 0x%lx)\n", EncryptionMask));

  //
  // Set Pcd to Deny the execution of option ROM when security
  // violation.
  //
  PcdStatus = PcdSet32S (PcdOptionRomImageVerificationPolicy, 0x4);
  ASSERT_RETURN_ERROR (PcdStatus);

  //
  // When SMM is required, cover the pages containing the initial SMRAM Save
  // State Map with a memory allocation HOB:
  //
  // There's going to be a time interval between our decrypting those pages for
  // SMBASE relocation and re-encrypting the same pages after SMBASE
  // relocation. We shall ensure that the DXE phase stay away from those pages
  // until after re-encryption, in order to prevent an information leak to the
  // hypervisor.
  //
  if (FeaturePcdGet (PcdSmmSmramRequire) && (mBootMode != BOOT_ON_S3_RESUME)) {
    RETURN_STATUS  LocateMapStatus;
    UINTN          MapPagesBase;
    UINTN          MapPagesCount;

    LocateMapStatus = MemEncryptSevLocateInitialSmramSaveStateMapPages (
                        &MapPagesBase,
                        &MapPagesCount
                        );
    ASSERT_RETURN_ERROR (LocateMapStatus);

    if (mQ35SmramAtDefaultSmbase) {
      //
      // The initial SMRAM Save State Map has been covered as part of a larger
      // reserved memory allocation in InitializeRamRegions().
      //
      ASSERT (SMM_DEFAULT_SMBASE <= MapPagesBase);
      ASSERT (
        (MapPagesBase + EFI_PAGES_TO_SIZE (MapPagesCount) <=
         SMM_DEFAULT_SMBASE + MCH_DEFAULT_SMBASE_SIZE)
        );
    } else {
      BuildMemoryAllocationHob (
        MapPagesBase,                      // BaseAddress
        EFI_PAGES_TO_SIZE (MapPagesCount), // Length
        EfiBootServicesData                // MemoryType
        );
    }
  }

  //
  // Check and perform SEV-ES initialization if required.
  //
  AmdSevEsInitialize ();

  //
  // Set the Confidential computing attr PCD to communicate which SEV
  // technology is active.
  //
  if (MemEncryptSevSnpIsEnabled ()) {
    PcdStatus = PcdSet64S (PcdConfidentialComputingGuestAttr, CCAttrAmdSevSnp);
  } else if (MemEncryptSevEsIsEnabled ()) {
    PcdStatus = PcdSet64S (PcdConfidentialComputingGuestAttr, CCAttrAmdSevEs);
  } else {
    PcdStatus = PcdSet64S (PcdConfidentialComputingGuestAttr, CCAttrAmdSev);
  }

  ASSERT_RETURN_ERROR (PcdStatus);
}

/**
 The function performs SEV specific region initialization.

 **/
VOID
SevInitializeRam (
  VOID
  )
{
  if (MemEncryptSevSnpIsEnabled ()) {
    //
    // If SEV-SNP is enabled, reserve the Secrets and CPUID memory area.
    //
    // This memory range is given to the PSP by the hypervisor to populate
    // the information used during the SNP VM boots, and it need to persist
    // across the kexec boots. Mark it as EfiReservedMemoryType so that
    // the guest firmware and OS does not use it as a system memory.
    //
    BuildMemoryAllocationHob (
      (EFI_PHYSICAL_ADDRESS)(UINTN)PcdGet32 (PcdOvmfSnpSecretsBase),
      (UINT64)(UINTN)PcdGet32 (PcdOvmfSnpSecretsSize),
      EfiReservedMemoryType
      );
    BuildMemoryAllocationHob (
      (EFI_PHYSICAL_ADDRESS)(UINTN)PcdGet32 (PcdOvmfCpuidBase),
      (UINT64)(UINTN)PcdGet32 (PcdOvmfCpuidSize),
      EfiReservedMemoryType
      );
  }
}
