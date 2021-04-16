/** @file
  SEV Secret boot time HOB placement

  Copyright (C) 2020 James Bottomley, IBM Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <PiPei.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/MemEncryptSevLib.h>

EFI_STATUS
EFIAPI
InitializeSecretPei (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  UINTN   Type;

  //
  // The secret page should be mapped encrypted by the guest OS and must not
  // be treated as a system RAM. Mark it as ACPI NVS so that guest OS maps it
  // encrypted.
  //
  if (MemEncryptSevSnpIsEnabled ()) {
    Type = EfiACPIMemoryNVS;
  } else {
    Type = EfiBootServicesData;
  }

  BuildMemoryAllocationHob (
    PcdGet32 (PcdSevLaunchSecretBase),
    PcdGet32 (PcdSevLaunchSecretSize),
    Type
    );

  return EFI_SUCCESS;
}
