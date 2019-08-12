/**@file
  Initialize Secure Encrypted Virtualization (SEV) support

  Copyright (c) 2017, Advanced Micro Devices. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
//
// The package level header files this module uses
//
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemEncryptSevLib.h>
#include <Library/PcdLib.h>
#include <PiPei.h>
#include <Register/Amd/Cpuid.h>
#include <Register/Cpuid.h>
#include <Register/Amd/Msr.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#include "Platform.h"

/**

  Initialize SEV-ES support if running an SEV-ES guest.

  **/
STATIC
VOID
AmdSevEsInitialize (
  VOID
  )
{
  VOID              *GhcbBase;
  PHYSICAL_ADDRESS  GhcbBasePa;
  UINTN             GhcbPageCount;
  RETURN_STATUS     DecryptStatus, PcdStatus;
  IA32_DESCRIPTOR   Gdtr;
  VOID              *Gdt;

  if (!MemEncryptSevEsIsEnabled ()) {
    return;
  }

  GhcbPageCount = mMaxCpuCount * 2;

  //
  // Allocate GHCB pages.
  //
  GhcbBase = AllocatePages (GhcbPageCount);
  ASSERT (GhcbBase);

  GhcbBasePa = (PHYSICAL_ADDRESS)(UINTN) GhcbBase;

  DecryptStatus = MemEncryptSevClearPageEncMask (
    0,
    GhcbBasePa,
    GhcbPageCount,
    TRUE
    );
  ASSERT_RETURN_ERROR (DecryptStatus);

  BuildMemoryAllocationHob (
    GhcbBasePa,
    EFI_PAGES_TO_SIZE (GhcbPageCount),
    EfiBootServicesData
    );

  SetMem (GhcbBase, GhcbPageCount * SIZE_4KB, 0);

  PcdStatus = PcdSet64S (PcdGhcbBase, (UINT64)GhcbBasePa);
  ASSERT_RETURN_ERROR (PcdStatus);
  PcdStatus = PcdSet64S (PcdGhcbSize, (UINT64)EFI_PAGES_TO_SIZE (GhcbPageCount));
  ASSERT_RETURN_ERROR (PcdStatus);
  PcdStatus = PcdSet32S (PcdCpuSevEsActive, 1);
  ASSERT_RETURN_ERROR (PcdStatus);

  DEBUG ((EFI_D_INFO, "SEV-ES is enabled, %u GHCB pages allocated starting at 0x%lx\n", GhcbPageCount, GhcbBase));

  AsmWriteMsr64 (MSR_SEV_ES_GHCB, (UINT64)GhcbBasePa);

  //
  // The SEV support will clear the C-bit from the non-RAM areas. Since
  // the GDT initially lives in that area and it will be read when a #VC
  // exception happens, it needs to be moved to RAM for an SEV-ES guest.
  //
  AsmReadGdtr (&Gdtr);

  Gdt = AllocatePool (Gdtr.Limit + 1);
  ASSERT (Gdt);

  CopyMem (Gdt, (VOID *) Gdtr.Base, Gdtr.Limit + 1);
  Gdtr.Base = (UINTN) Gdt;
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
  CPUID_MEMORY_ENCRYPTION_INFO_EBX  Ebx;
  UINT64                            EncryptionMask;
  RETURN_STATUS                     PcdStatus;

  //
  // Check if SEV is enabled
  //
  if (!MemEncryptSevIsEnabled ()) {
    return;
  }

  //
  // CPUID Fn8000_001F[EBX] Bit 0:5 (memory encryption bit position)
  //
  AsmCpuid (CPUID_MEMORY_ENCRYPTION_INFO, NULL, &Ebx.Uint32, NULL, NULL);
  EncryptionMask = LShiftU64 (1, Ebx.Bits.PtePosBits);

  //
  // Set Memory Encryption Mask PCD
  //
  PcdStatus = PcdSet64S (PcdPteMemoryEncryptionAddressOrMask, EncryptionMask);
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
    RETURN_STATUS LocateMapStatus;
    UINTN         MapPagesBase;
    UINTN         MapPagesCount;

    LocateMapStatus = MemEncryptSevLocateInitialSmramSaveStateMapPages (
                        &MapPagesBase,
                        &MapPagesCount
                        );
    ASSERT_RETURN_ERROR (LocateMapStatus);

    BuildMemoryAllocationHob (
      MapPagesBase,                      // BaseAddress
      EFI_PAGES_TO_SIZE (MapPagesCount), // Length
      EfiBootServicesData                // MemoryType
      );
  }

  //
  // Check and perform SEV-ES initialization if required.
  //
  AmdSevEsInitialize ();
}
