
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Register/Amd/Msr.h>
#include "CpuExceptionCommon.h"
#include "AMDSevVcCommon.h"

UINTN
DoVcException(
  EFI_SYSTEM_CONTEXT_X64 *Regs
  )
{
  MSR_SEV_ES_GHCB_REGISTER  Msr;
  GHCB                      *Ghcb;

  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);
  ASSERT(!Msr.Bits.GhcbNegotiateBit);

  Ghcb = (GHCB *) Msr.GhcbPhysicalAddress;

  return DoVcCommon (Ghcb, Regs);
}
