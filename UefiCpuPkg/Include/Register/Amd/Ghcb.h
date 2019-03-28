
#ifndef __GHCB_H__
#define __GHCB_H__

#include <Protocol/DebugSupport.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#define UD_EXCEPTION  6
#define GP_EXCEPTION 13

#define GHCB_VERSION_MIN     1
#define GHCB_VERSION_MAX     1

#define GHCB_STANDARD_USAGE  0

typedef enum {
  SvmExitDr7Write      = 0x37,
  SvmExitRdtsc         = 0x6E,
  SvmExitRdpmc,
  SvmExitCpuid         = 0x72,
  SvmExitInvd          = 0x76,
  SvmExitPause,
  SvmExitIoioProt      = 0x7B,
  SvmExitMsr,
  SvmExitVmmCall       = 0x81,
  SvmExitRdtscp        = 0x87,
  SvmExitWbinvd        = 0x89,
  SvmExitMonitor,
  SvmExitMwait,
  SvmExitNpf           = 0x400,

  // VMG special exits
  SvmExitMmioRead      = 0x80000001,
  SvmExitMmioWrite,
  SvmExitNmiComplete,
  SvmExitApResetHold,

  SvmExitUnsupported   = 0x8000FFFF,
} SVM_EXITCODE;

typedef enum {
  GhcbRflags           = 46,
  GhcbRip,
  GhcbRsp              = 59,
  GhcbRax              = 63,
  GhcbRcx              = 97,
  GhcbRdx,
  GhcbRbx,
  GhcbRbp              = 101,
  GhcbRsi,
  GhcbRdi,
  GhcbR8,
  GhcbR9,
  GhcbR10,
  GhcbR11,
  GhcbR12,
  GhcbR13,
  GhcbR14,
  GhcbR15,
  GhcbXCr0             = 125,
} GHCB_REGISTER;

typedef struct {
  UINT8                  Reserved1[203];
  UINT8                  Cpl;
  UINT8                  Reserved2[148];
  UINT64                 Dr7;
  UINT8                  Reserved3[144];
  UINT64                 Rax;
  UINT8                  Reserved4[264];
  UINT64                 Rcx;
  UINT64                 Rdx;
  UINT64                 Rbx;
  UINT8                  Reserved5[112];
  UINT64                 SwExitCode;
  UINT64                 SwExitInfo1;
  UINT64                 SwExitInfo2;
  UINT64                 SwScratch;
  UINT8                  Reserved6[56];
  UINT64                 XCr0;
  UINT8                  ValidBitmap[16];
  UINT64                 X87StateGpa;
  UINT8                  Reserved7[1016];
} __attribute__ ((__packed__)) GHCB_SAVE_AREA;

typedef struct {
  GHCB_SAVE_AREA         SaveArea;
  UINT8                  SharedBuffer[2032];
  UINT8                  Reserved1[10];
  UINT16                 ProtocolVersion;
  UINT32                 GhcbUsage;
} __attribute__ ((__packed__)) __attribute__ ((aligned(SIZE_4KB))) GHCB;

typedef union {
  struct {
    UINT32  Lower32Bits;
    UINT32  Upper32Bits;
  } Elements;

  UINT64    Uint64;
} GHCB_EXIT_INFO;

static inline
BOOLEAN
GhcbIsRegValid(
  GHCB                   *Ghcb,
  GHCB_REGISTER          Reg
  )
{
  UINT32  RegIndex = Reg / 8;
  UINT32  RegBit   = Reg & 0x07;

  return (Ghcb->SaveArea.ValidBitmap[RegIndex] & (1 << RegBit));
}

static inline
VOID
GhcbSetRegValid(
  GHCB                   *Ghcb,
  GHCB_REGISTER          Reg
  )
{
  UINT32  RegIndex = Reg / 8;
  UINT32  RegBit   = Reg & 0x07;

  Ghcb->SaveArea.ValidBitmap[RegIndex] |= (1 << RegBit);
}

static inline
VOID
VmgException(
  UINTN                  Exception
  )
{
  switch (Exception) {
  case UD_EXCEPTION:
  case GP_EXCEPTION:
    break;
  default:
    ASSERT (0);
  }
}

static inline
UINTN
VmgExit(
  GHCB                   *Ghcb,
  UINT64                 ExitCode,
  UINT64                 ExitInfo1,
  UINT64                 ExitInfo2
  )
{
  GHCB_EXIT_INFO   ExitInfo;
  UINTN            Reason, Action;

  Ghcb->SaveArea.SwExitCode = ExitCode;
  Ghcb->SaveArea.SwExitInfo1 = ExitInfo1;
  Ghcb->SaveArea.SwExitInfo2 = ExitInfo2;
  AsmVmgExit ();

  if (!Ghcb->SaveArea.SwExitInfo1) {
    return 0;
  }

  ExitInfo.Uint64 = Ghcb->SaveArea.SwExitInfo1;
  Reason = ExitInfo.Elements.Upper32Bits;
  Action = ExitInfo.Elements.Lower32Bits;
  switch (Action) {
  case 1:
    VmgException (Reason);
    break;
  default:
    ASSERT (0);
  }

  return Reason;
}

static inline
VOID
VmgInit(
  GHCB                   *Ghcb
  )
{
  SetMem (&Ghcb->SaveArea, sizeof (Ghcb->SaveArea), 0);
}

static inline
VOID
VmgDone(
  GHCB                   *Ghcb
  )
{
}
#endif
