/** @file

  AMD SEV helper function.

  Copyright (c) 2021, AMD Incorporated. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "MpLib.h"
#include <Library/VmgExitLib.h>
#include <Register/Amd/Fam17Msr.h>
#include <Register/Amd/Ghcb.h>

/**
  Create an SEV-SNP AP save area (VMSA) for use in running the vCPU.

  @param[in]  CpuMpData        Pointer to CPU MP Data
  @param[in]  CpuData          Pointer to CPU AP Data
  @param[in]  ApicId           APIC ID of the vCPU
**/
VOID
SevSnpCreateSaveArea (
  IN CPU_MP_DATA  *CpuMpData,
  IN CPU_AP_DATA  *CpuData,
  UINT32          ApicId
  )
{
  SEV_ES_SAVE_AREA          *SaveArea;
  UINTN                     PageCount;
  IA32_CR0                  ApCr0;
  IA32_CR0                  ResetCr0;
  IA32_CR4                  ApCr4;
  IA32_CR4                  ResetCr4;
  UINTN                     StartIp;
  UINT8                     SipiVector;
  UINT64                    VmgExitStatus;
  MSR_SEV_ES_GHCB_REGISTER  Msr;
  GHCB                      *Ghcb;
  BOOLEAN                   InterruptState;
  UINT64                    ExitInfo1;
  UINT64                    ExitInfo2;
  EFI_STATUS                VmsaStatus;

  //
  // Allocate the memory for the SEV-ES Save Area (and possibly the CAA)
  // and initialize it.
  //
  PageCount = VmgExitSvsmPresent () ? 2 : 1;
  SaveArea = AllocateReservedPages (PageCount);
  if (!SaveArea) {
    return;
  }

  ZeroMem (SaveArea, EFI_PAGE_SIZE * PageCount);

  //
  // Propogate the CR0.NW and CR0.CD setting to the AP
  //
  ResetCr0.UintN = 0x00000010;
  ApCr0.UintN    = CpuData->VolatileRegisters.Cr0;
  if (ApCr0.Bits.NW) {
    ResetCr0.Bits.NW = 1;
  }

  if (ApCr0.Bits.CD) {
    ResetCr0.Bits.CD = 1;
  }

  //
  // Propagate the CR4.MCE setting to the AP
  //
  ResetCr4.UintN = 0;
  ApCr4.UintN    = CpuData->VolatileRegisters.Cr4;
  if (ApCr4.Bits.MCE) {
    ResetCr4.Bits.MCE = 1;
  }

  //
  // Convert the start IP into a SIPI Vector
  //
  StartIp    = CpuMpData->MpCpuExchangeInfo->BufferStart;
  SipiVector = (UINT8)(StartIp >> 12);

  //
  // Set the CS:RIP value based on the start IP
  //
  SaveArea->Cs.Base                    = SipiVector << 12;
  SaveArea->Cs.Selector                = SipiVector << 8;
  SaveArea->Cs.Limit                   = 0xFFFF;
  SaveArea->Cs.Attributes.Bits.Present = 1;
  SaveArea->Cs.Attributes.Bits.Sbit    = 1;
  SaveArea->Cs.Attributes.Bits.Type    = SEV_ES_RESET_CODE_SEGMENT_TYPE;
  SaveArea->Rip                        = StartIp & 0xFFF;

  //
  // Set the remaining values as defined in APM for INIT
  //
  SaveArea->Ds.Limit                   = 0xFFFF;
  SaveArea->Ds.Attributes.Bits.Present = 1;
  SaveArea->Ds.Attributes.Bits.Sbit    = 1;
  SaveArea->Ds.Attributes.Bits.Type    = SEV_ES_RESET_DATA_SEGMENT_TYPE;
  SaveArea->Es                         = SaveArea->Ds;
  SaveArea->Fs                         = SaveArea->Ds;
  SaveArea->Gs                         = SaveArea->Ds;
  SaveArea->Ss                         = SaveArea->Ds;

  SaveArea->Gdtr.Limit                   = 0xFFFF;
  SaveArea->Ldtr.Limit                   = 0xFFFF;
  SaveArea->Ldtr.Attributes.Bits.Present = 1;
  SaveArea->Ldtr.Attributes.Bits.Type    = SEV_ES_RESET_LDT_TYPE;
  SaveArea->Idtr.Limit                   = 0xFFFF;
  SaveArea->Tr.Limit                     = 0xFFFF;
  SaveArea->Ldtr.Attributes.Bits.Present = 1;
  SaveArea->Ldtr.Attributes.Bits.Type    = SEV_ES_RESET_TSS_TYPE;

  SaveArea->Efer   = 0x1000;
  SaveArea->Cr4    = ResetCr4.UintN;
  SaveArea->Cr0    = ResetCr0.UintN;
  SaveArea->Dr7    = 0x0400;
  SaveArea->Dr6    = 0xFFFF0FF0;
  SaveArea->Rflags = 0x0002;
  SaveArea->GPat   = 0x0007040600070406ULL;
  SaveArea->XCr0   = 0x0001;
  SaveArea->Mxcsr  = 0x1F80;
  SaveArea->X87Ftw = 0x5555;
  SaveArea->X87Fcw = 0x0040;

  //
  // Set the SEV-SNP specific fields for the save area:
  //   VMPL - based on current mode
  //   SEV_FEATURES - equivalent to the SEV_STATUS MSR right shifted 2 bits
  //
  SaveArea->Vmpl        = VmgExitGetVmpl ();
  SaveArea->SevFeatures = AsmReadMsr64 (MSR_SEV_STATUS) >> 2;

  //
  // Turn the page into a recognized VMSA page
  //
  VmsaStatus = VmgExitVmsaRmpAdjust (SaveArea, ApicId, TRUE);
  ASSERT_EFI_ERROR (VmsaStatus);

  ExitInfo1  = (UINT64)ApicId << 32;
  ExitInfo1 |= (UINT64)SaveArea->Vmpl << 16;
  ExitInfo1 |= SVM_VMGEXIT_SNP_AP_CREATE;
  ExitInfo2  = (UINT64)(UINTN)SaveArea;

  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);
  Ghcb                    = Msr.Ghcb;

  VmgInit (Ghcb, &InterruptState);
  Ghcb->SaveArea.Rax = SaveArea->SevFeatures;
  VmgSetOffsetValid (Ghcb, GhcbRax);
  VmgExitStatus = VmgExit (
                    Ghcb,
                    SVM_EXIT_SNP_AP_CREATION,
                    ExitInfo1,
                    ExitInfo2
                    );
  VmgDone (Ghcb, InterruptState);

  ASSERT (VmgExitStatus == 0);
  if (VmgExitStatus != 0) {
    VmsaStatus = VmgExitVmsaRmpAdjust (SaveArea, ApicId, FALSE);
    if (EFI_ERROR (VmsaStatus)) {
      DEBUG ((DEBUG_INFO, "SEV-SNP: RMPADJUST failed, leaking VMSA page\n"));
    } else {
      FreePages (SaveArea, PageCount);
    }

    SaveArea = NULL;
  }

  if (CpuData->SevEsSaveArea) {
    VmsaStatus = VmgExitVmsaRmpAdjust (CpuData->SevEsSaveArea, ApicId, FALSE);
    if (EFI_ERROR (VmsaStatus)) {
      DEBUG ((DEBUG_INFO, "SEV-SNP: RMPADJUST failed, leaking VMSA page\n"));
    } else {
      FreePages (CpuData->SevEsSaveArea, PageCount);
    }
  }

  CpuData->SevEsSaveArea = SaveArea;
}

/**
  Create SEV-SNP APs.

  @param[in]  CpuMpData        Pointer to CPU MP Data
  @param[in]  ProcessorNumber  The handle number of specified processor
                               (-1 for all APs)
**/
VOID
SevSnpCreateAP (
  IN CPU_MP_DATA  *CpuMpData,
  IN INTN         ProcessorNumber
  )
{
  CPU_INFO_IN_HOB  *CpuInfoInHob;
  CPU_AP_DATA      *CpuData;
  UINTN            Index;
  UINTN            MaxIndex;
  UINT32           ApicId;
  GHCB_APIC_IDS    *GhcbApicIds;

  ASSERT (CpuMpData->MpCpuExchangeInfo->BufferStart < 0x100000);

  CpuInfoInHob = (CPU_INFO_IN_HOB *)(UINTN)CpuMpData->CpuInfoInHob;

  if (ProcessorNumber < 0) {
    GhcbApicIds = (GHCB_APIC_IDS *)(UINTN)PcdGet64 (PcdSevSnpApicIds);

    if (CpuMpData->InitFlag == ApInitConfig) {
      //
      // APs have not been started, so CpuCount is not "known" yet.
      // Use the retrieved APIC IDs to start the APs and fill out the
      // MpLib CPU information properly.
      //
      ASSERT(GhcbApicIds != NULL);

      MaxIndex = MIN (
                   GhcbApicIds->NumEntries,
                   PcdGet32 (PcdCpuMaxLogicalProcessorNumber)
                   );
    } else {
      //
      // APs have been previously started.
      //
      MaxIndex = CpuMpData->CpuCount;
    }

    for (Index = 0; Index < MaxIndex; Index++) {
      if (Index != CpuMpData->BspNumber) {
        CpuData = &CpuMpData->CpuData[Index];

        if (CpuMpData->InitFlag == ApInitConfig) {
          ApicId  = GhcbApicIds->ApicIds[Index];

          //
          // For the first boot, use the BSP register information.
          //
          CopyMem (
            &CpuData->VolatileRegisters,
            &CpuMpData->CpuData[0].VolatileRegisters,
            sizeof (CpuData->VolatileRegisters)
            );
        } else {
          ApicId  = CpuInfoInHob[Index].ApicId;
        }

        SevSnpCreateSaveArea (CpuMpData, CpuData, ApicId);
      }
    }
  } else {
    Index   = (UINTN)ProcessorNumber;
    CpuData = &CpuMpData->CpuData[Index];
    ApicId  = CpuInfoInHob[ProcessorNumber].ApicId,
    SevSnpCreateSaveArea (CpuMpData, CpuData, ApicId);
  }
}

/**
  Determine if the SEV-SNP AP Create protocol should be used.

  @param[in]  CpuMpData  Pointer to CPU MP Data

  @retval     TRUE       Use SEV-SNP AP Create protocol
  @retval     FALSE      Do not use SEV-SNP AP Create protocol
**/
BOOLEAN
SevSnpUseCreateAP (
  IN  CPU_MP_DATA  *CpuMpData
  )
{
  //
  // The AP Create protocol is used for an SEV-SNP guest if
  //   - The initial configuration has been performed already or
  //   - PcdSevSnpApicIds is non-zero.
  //
  if (!CpuMpData->SevSnpIsEnabled) {
    return FALSE;
  }

  if (CpuMpData->InitFlag == ApInitConfig &&
      PcdGet64 (PcdSevSnpApicIds) == 0) {
    return FALSE;
  }

  return TRUE;
}
