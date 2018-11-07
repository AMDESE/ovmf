
#ifndef _AMD_SEV_VC_COMMON_H_
#define _AMD_SEV_VC_COMMON_H_

#include <Protocol/DebugSupport.h>

UINTN
DoVcException(
  EFI_SYSTEM_CONTEXT_X64 *Regs
  );

#endif
