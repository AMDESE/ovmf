
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Register/Amd/Msr.h>
#include "CpuExceptionCommon.h"
#include "AMDSevVcCommon.h"

UINTN
DoVcException (
  EFI_SYSTEM_CONTEXT  Context
  )
{
  MSR_SEV_ES_GHCB_REGISTER  Msr;
  GHCB                      *Ghcb;

  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);
  ASSERT(!Msr.Bits.GhcbNegotiateBit);

  Ghcb = Msr.Ghcb;

  return DoVcCommon (Ghcb, Context);
}
