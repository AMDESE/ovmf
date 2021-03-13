#include <Uefi/UefiBaseType.h>

#include "../SnpPageStateChange.h"

/**
 The function is used to set the page state when SEV-SNP is active. The page state
 transition consist of changing the page ownership in the RMP table, and using the
 PVALIDATE instruction to update the Validated bit in RMP table.

 */
VOID
SevSnpValidateSystemRamInternal (
  IN EFI_PHYSICAL_ADDRESS             BaseAddress,
  IN UINTN                            NumPages
  )
{
}
