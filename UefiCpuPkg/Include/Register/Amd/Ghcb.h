
#ifndef __GHCB_H__
#define __GHCB_H__

#include <Protocol/DebugSupport.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#define UD_EXCEPTION  6
#define GP_EXCEPTION 13

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
  UINT64                 X87Dp;
  UINT32                 MxCsr;
  UINT16                 X87Ftw;
  UINT16                 X87Fsw;
  UINT16                 X87Fcw;
  UINT16                 X87Fop;
  UINT16                 X87Ds;
  UINT16                 X87Cs;
  UINT64                 X87Rip;
  UINT8                  FpregX87[80];
  UINT8                  FpregXmm[256];
  UINT8                  FpregYmm[256];
  UINT8                  Reserved1[3472];
  UINT8                  ValidBitmap[64];
} __attribute__ ((__packed__)) __attribute__ ((aligned(SIZE_4KB))) GHCB_X87_STATE;

typedef struct {
  UINT16                 Selector;
  UINT16                 Attrib;
  UINT32                 Limit;
  UINT64                 Base;
} __attribute__ ((__packed__)) GHCB_SEGMENT;

typedef struct {
  GHCB_SEGMENT           Es;
  GHCB_SEGMENT           Cs;
  GHCB_SEGMENT           Ss;
  GHCB_SEGMENT           Ds;
  GHCB_SEGMENT           Fs;
  GHCB_SEGMENT           Gs;
  GHCB_SEGMENT           Gdtr;
  GHCB_SEGMENT           Ldtr;
  GHCB_SEGMENT           Idtr;
  GHCB_SEGMENT           Tr;

  UINT8                  Reserved1[43];
  UINT8                  Cpl;
  UINT8                  Reserved2[4];
  UINT64                 Efer;
  UINT8                  Reserved3[112];
  UINT64                 Cr4;
  UINT64                 Cr3;
  UINT64                 Cr0;
  UINT64                 Dr7;
  UINT64                 Dr6;
  UINT64                 Rflags;
  UINT64                 Rip;
  UINT8                  Reserved4[88];
  UINT64                 Rsp;
  UINT8                  Reserved5[24];
  UINT64                 Rax;
  UINT64                 Star;
  UINT64                 LStar;
  UINT64                 CStar;
  UINT64                 SfMask;
  UINT64                 KernelGsBase;
  UINT64                 SysEnterCs;
  UINT64                 SysEnterEsp;
  UINT64                 SysEnterEip;
  UINT64                 Cr2;
  UINT8                  Reserved6[32];
  UINT64                 GPat;
  UINT64                 DbgCtl;
  UINT64                 BrFrom;
  UINT64                 BrTo;
  UINT64                 LastExcpFrom;
  UINT64                 LastExcpTo;
  UINT8                  Reserved7[112];
  UINT64                 Rcx;
  UINT64                 Rdx;
  UINT64                 Rbx;
  UINT64                 Reserved8;
  UINT64                 Rbp;
  UINT64                 Rsi;
  UINT64                 Rdi;
  UINT64                 R8;
  UINT64                 R9;
  UINT64                 R10;
  UINT64                 R11;
  UINT64                 R12;
  UINT64                 R13;
  UINT64                 R14;
  UINT64                 R15;
  UINT64                 Zero;
  UINT64                 ChecksumPointer;
  UINT64                 SwExitCode;
  UINT64                 SwExitInfo1;
  UINT64                 SwExitInfo2;
  UINT64                 SwScratch;
  UINT8                  Reserved9[56];
  UINT64                 XCr0;
  UINT8                  ValidBitmap[16];
  UINT64                 X87StateGpa;
} __attribute__ ((__packed__)) GHCB_SAVE_AREA;

typedef struct {
  GHCB_SAVE_AREA         SaveArea;
  UINT8                  Reserved10[1014];
  UINT16                 ProtocolVersion;
  UINT8                  SharedBuffer[2048];
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
