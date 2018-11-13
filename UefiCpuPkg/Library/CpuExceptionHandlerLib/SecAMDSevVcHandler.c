
#include <Library/BaseLib.h>
#include <Register/Amd/Msr.h>
#include "CpuExceptionCommon.h"
#include "AMDSevVcCommon.h"


#define GHCB_INIT 0x807000

UINTN
DoVcException(
  EFI_SYSTEM_CONTEXT_X64 *Regs
  )
{
  MSR_SEV_ES_GHCB_REGISTER  Msr;
  GHCB                      *Ghcb;

  Msr.GhcbPhysicalAddress = AsmReadMsr64 (MSR_SEV_ES_GHCB);
  if (Msr.Bits.GhcbNegotiateBit) {
    Msr.GhcbPhysicalAddress = GHCB_INIT;
    AsmWriteMsr64(MSR_SEV_ES_GHCB, Msr.GhcbPhysicalAddress);

    SetMem ((VOID *) Msr.GhcbPhysicalAddress, sizeof (*Ghcb), 0);
  }

  Ghcb = (GHCB *) Msr.GhcbPhysicalAddress;

  return DoVcCommon(Ghcb, Regs);
}
