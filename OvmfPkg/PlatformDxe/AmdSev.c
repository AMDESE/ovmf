/**@file
  Detect KVM hypervisor support for SEV live migration and if 
  detected, setup a new UEFI enviroment variable indicating 
  OVMF support for SEV live migration.

  Copyright (c) 2020, Advanced Micro Devices. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
//
// The package level header files this module uses
//

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/MemEncryptLib.h>

/**
  Figures out if we are running inside KVM HVM and 
  KVM HVM supports SEV Live Migration feature.

  @retval TRUE   KVM was detected and Live Migration supported
  @retval FALSE  KVM was not detected or Live Migration not supported

**/
BOOLEAN
KvmDetectSevLiveMigrationFeature(
  VOID
  )
{
  UINT8 Signature[13];
  UINT32 mKvmLeaf = 0;
  UINT32 RegEax, RegEbx, RegEcx, RegEdx;

  Signature[12] = '\0';
  for (mKvmLeaf = 0x40000000; mKvmLeaf < 0x40010000; mKvmLeaf += 0x100) {
    AsmCpuid (mKvmLeaf,
              NULL,
              (UINT32 *) &Signature[0],
              (UINT32 *) &Signature[4],
              (UINT32 *) &Signature[8]);

    if (!AsciiStrCmp ((CHAR8 *) Signature, "KVMKVMKVM\0\0\0")) {
   DEBUG ((
    DEBUG_ERROR,
    "%a: KVM Detected, signature = %s\n",
    __FUNCTION__,
    Signature
    ));

    RegEax = 0x40000001;
    RegEcx = 0;
      AsmCpuid (0x40000001, &RegEax, &RegEbx, &RegEcx, &RegEdx);
      if (RegEax & (1 << 14)) {
   DEBUG ((
    DEBUG_ERROR,
    "%a: Live Migration feature supported\n",
    __FUNCTION__
    ));
      	return TRUE;
     }
    }
  }

  return FALSE;
}

/**

  Function checks if SEV Live Migration support is available, if present then it sets
  a UEFI enviroment variable to be queried later using Runtime services.

  **/
VOID
AmdSevSetConfig(
  VOID
  )
{
  EFI_STATUS Status;
  BOOLEAN SevLiveMigrationEnabled;

  SevLiveMigrationEnabled = KvmDetectSevLiveMigrationFeature();

  if (SevLiveMigrationEnabled) {
   Status = gRT->SetVariable (
		L"SevLiveMigrationEnabled",
                &gMemEncryptGuid,
		EFI_VARIABLE_NON_VOLATILE |
                EFI_VARIABLE_BOOTSERVICE_ACCESS |
	       	EFI_VARIABLE_RUNTIME_ACCESS,
                sizeof (BOOLEAN),
                &SevLiveMigrationEnabled
               );

   DEBUG ((
    DEBUG_ERROR,
    "%a: Setting SevLiveMigrationEnabled variable, status = %lx\n",
    __FUNCTION__,
    Status
    ));
  }
}
