
#ifndef _AMD_SEV_VC_COMMON_H_
#define _AMD_SEV_VC_COMMON_H_

#include <Protocol/DebugSupport.h>
#include <Register/Amd/Ghcb.h>

UINTN
DoVcException(
  EFI_SYSTEM_CONTEXT  Context
  );

UINTN
DoVcCommon(
  GHCB                *Ghcb,
  EFI_SYSTEM_CONTEXT  Context
  );

#endif
