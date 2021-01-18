#ifndef SNP_PAGE_STATE_INTERNAL_H_
#define SNP_PAGE_STATE_INTERNAL_H_

#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemEncryptSevLib.h>
#include <Library/VmgExitLib.h>
#include <Register/Amd/Ghcb.h>
#include <Register/Amd/Msr.h>


RETURN_STATUS
SetPageStateInternal (
  IN EFI_PHYSICAL_ADDRESS         BaseAddress,
  IN UINTN                        NumPages,
  IN MEM_OP_REQ                   Type
  );

RETURN_STATUS
PvalidateInternal (
  IN PHYSICAL_ADDRESS               BaseAddress,
  IN UINTN                          NumPages,
  IN MEM_OP_REQ                     Type
  );

#endif
