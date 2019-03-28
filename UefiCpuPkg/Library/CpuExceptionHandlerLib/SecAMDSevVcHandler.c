
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
  Ghcb = (GHCB *) Msr.GhcbPhysicalAddress;

  if (Msr.Bits.GhcbNegotiateBit) {
    if (Msr.GhcbProtocol.SevEsProtocolMin > Msr.GhcbProtocol.SevEsProtocolMax) {
      ASSERT (0);
      return GP_EXCEPTION;
    }

    if ((Msr.GhcbProtocol.SevEsProtocolMin > GHCB_VERSION_MAX) ||
        (Msr.GhcbProtocol.SevEsProtocolMax < GHCB_VERSION_MIN)) {
      ASSERT (0);
      return GP_EXCEPTION;
    }

    Msr.GhcbPhysicalAddress = GHCB_INIT;
    AsmWriteMsr64(MSR_SEV_ES_GHCB, Msr.GhcbPhysicalAddress);

    Ghcb = (GHCB *) Msr.GhcbPhysicalAddress;
    SetMem (Ghcb, sizeof (*Ghcb), 0);

    /* Set the version to the maximum that can be supported */
    Ghcb->ProtocolVersion = MIN (Msr.GhcbProtocol.SevEsProtocolMax, GHCB_VERSION_MAX);
    Ghcb->GhcbUsage = GHCB_STANDARD_USAGE;
  }

  return DoVcCommon(Ghcb, Regs);
}
