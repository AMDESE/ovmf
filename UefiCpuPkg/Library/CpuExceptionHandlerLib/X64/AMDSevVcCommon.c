
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include "AMDSevVcCommon.h"

#define CR4_OSXSAVE (1 << 18)

typedef enum {
  LongMode64Bit        = 0,
  LongModeCompat32Bit,
  LongModeCompat16Bit,
} SEV_ES_INSTRUCTION_MODE;

typedef enum {
  Size8Bits            = 0,
  Size16Bits,
  Size32Bits,
  Size64Bits,
} SEV_ES_INSTRUCTION_SIZE;

typedef enum {
  SegmentEs            = 0,
  SegmentCs,
  SegmentSs,
  SegmentDs,
  SegmentFs,
  SegmentGs,
} SEV_ES_INSTRUCTION_SEGMENT;

typedef enum {
  RepNone              = 0,
  RepZ,
  RepNZ,
} SEV_ES_INSTRUCTION_REP;

typedef union {
  struct {
    UINT8  B:1;
    UINT8  X:1;
    UINT8  R:1;
    UINT8  W:1;
    UINT8  REX:4;
  } Bits;

  UINT8  Uint8;
} SEV_ES_INSTRUCTION_REX_PREFIX;

typedef union {
  struct {
    UINT8  Rm:3;
    UINT8  Reg:3;
    UINT8  Mod:2;
  } Bits;

  UINT8  Uint8;
} SEV_ES_INSTRUCTION_MODRM;

typedef union {
  struct {
    UINT8  Base:3;
    UINT8  Index:3;
    UINT8  Scale:2;
  } Bits;

  UINT8  Uint8;
} SEV_ES_INSTRUCTION_SIB;

typedef struct {
  struct {
    UINT8  Rm;
    UINT8  Reg;
    UINT8  Mod;
  } ModRm;

  struct {
    UINT8  Base;
    UINT8  Index;
    UINT8  Scale;
  } Sib;

  INTN  RegData;
  INTN  RmData;
} SEV_ES_INSTRUCTION_OPCODE_EXT;

typedef struct {
  GHCB                           *Ghcb;

  SEV_ES_INSTRUCTION_MODE        Mode;
  SEV_ES_INSTRUCTION_SIZE        DataSize;
  SEV_ES_INSTRUCTION_SIZE        AddrSize;
  BOOLEAN                        SegmentSpecified;
  SEV_ES_INSTRUCTION_SEGMENT     Segment;
  SEV_ES_INSTRUCTION_REP         RepMode;

  UINT8                          *Begin;
  UINT8                          *End;

  UINT8                          *Prefixes;
  UINT8                          *OpCodes;
  UINT8                          *Displacement;
  UINT8                          *Immediate;

  SEV_ES_INSTRUCTION_REX_PREFIX  RexPrefix;

  BOOLEAN                        ModRmPresent;
  SEV_ES_INSTRUCTION_MODRM       ModRm;

  BOOLEAN                        SibPresent;
  SEV_ES_INSTRUCTION_SIB         Sib;

  UINT8                          PrefixSize;
  UINT8                          OpCodeSize;
  UINT8                          DisplacementSize;
  UINT8                          ImmediateSize;

  SEV_ES_INSTRUCTION_OPCODE_EXT  Ext;
} SEV_ES_INSTRUCTION_DATA;

typedef
UINTN
(*NAE_EXIT) (
  GHCB                     *Ghcb,
  EFI_SYSTEM_CONTEXT_X64   *Regs,
  SEV_ES_INSTRUCTION_DATA  *InstructionData
  );

STATIC
INT64 *
GetRegisterPointer (
  EFI_SYSTEM_CONTEXT_X64   *Regs,
  UINT8                    Register
  )
{
  UINT64 *Reg;

  switch (Register) {
  case 0:
    Reg = &Regs->Rax;
    break;
  case 1:
    Reg = &Regs->Rcx;
    break;
  case 2:
    Reg = &Regs->Rdx;
    break;
  case 3:
    Reg = &Regs->Rbx;
    break;
  case 4:
    Reg = &Regs->Rsp;
    break;
  case 5:
    Reg = &Regs->Rbp;
    break;
  case 6:
    Reg = &Regs->Rsi;
    break;
  case 7:
    Reg = &Regs->Rdi;
    break;
  case 8:
    Reg = &Regs->R8;
    break;
  case 9:
    Reg = &Regs->R9;
    break;
  case 10:
    Reg = &Regs->R10;
    break;
  case 11:
    Reg = &Regs->R11;
    break;
  case 12:
    Reg = &Regs->R12;
    break;
  case 13:
    Reg = &Regs->R13;
    break;
  case 14:
    Reg = &Regs->R14;
    break;
  case 15:
    Reg = &Regs->R15;
    break;
  default:
    Reg = NULL;
  }
  ASSERT (Reg);

  return (INT64 *) Reg;
}

STATIC
VOID
UpdateForDisplacement (
  SEV_ES_INSTRUCTION_DATA  *InstructionData,
  UINTN                    Size
  )
{
  InstructionData->DisplacementSize = Size;
  InstructionData->Immediate += Size;
  InstructionData->End += Size;
}

STATIC
BOOLEAN
IsRipRelative (
  SEV_ES_INSTRUCTION_DATA  *InstructionData
  )
{
  SEV_ES_INSTRUCTION_OPCODE_EXT  *Ext = &InstructionData->Ext;

  return ((InstructionData == LongMode64Bit) &&
          (Ext->ModRm.Mod == 0) &&
          (Ext->ModRm.Rm == 5)  &&
          (InstructionData->SibPresent == FALSE));
}

STATIC
UINTN
GetEffectiveMemoryAddress (
  EFI_SYSTEM_CONTEXT_X64   *Regs,
  SEV_ES_INSTRUCTION_DATA  *InstructionData
  )
{
  SEV_ES_INSTRUCTION_OPCODE_EXT  *Ext = &InstructionData->Ext;
  INTN                           EffectiveAddress = 0;

  if (IsRipRelative (InstructionData)) {
    /* RIP-relative displacement is a 32-bit signed value */
    INT32 RipRelative = *(INT32 *) InstructionData->Displacement;

    UpdateForDisplacement (InstructionData, 4);
    return (UINTN) ((INTN) Regs->Rip + RipRelative);
  }

  switch (Ext->ModRm.Mod) {
  case 1:
    UpdateForDisplacement (InstructionData, 1);
    EffectiveAddress += (INT8) (*(INT8 *) (InstructionData->Displacement));
    break;
  case 2:
    switch (InstructionData->AddrSize) {
    case Size16Bits:
      UpdateForDisplacement (InstructionData, 2);
      EffectiveAddress += (INT16) (*(INT16 *) (InstructionData->Displacement));
      break;
    default:
      UpdateForDisplacement (InstructionData, 4);
      EffectiveAddress += (INT32) (*(INT32 *) (InstructionData->Displacement));
      break;
    }
    break;
  }

  if (InstructionData->SibPresent) {
    if (Ext->Sib.Index != 4) {
      EffectiveAddress += (*GetRegisterPointer (Regs, Ext->Sib.Index) << Ext->Sib.Scale);
    }

    if ((Ext->Sib.Base != 5) || Ext->ModRm.Mod) {
      EffectiveAddress += *GetRegisterPointer (Regs, Ext->Sib.Base);
    } else {
      UpdateForDisplacement (InstructionData, 4);
      EffectiveAddress += (INT32) (*(INT32 *) (InstructionData->Displacement));
    }
  } else {
    EffectiveAddress += *GetRegisterPointer (Regs, Ext->ModRm.Rm);
  }

  return (UINTN) EffectiveAddress;
}

STATIC
VOID
DecodeModRm (
  EFI_SYSTEM_CONTEXT_X64   *Regs,
  SEV_ES_INSTRUCTION_DATA  *InstructionData
  )
{
  SEV_ES_INSTRUCTION_REX_PREFIX  *RexPrefix = &InstructionData->RexPrefix;
  SEV_ES_INSTRUCTION_OPCODE_EXT  *Ext = &InstructionData->Ext;
  SEV_ES_INSTRUCTION_MODRM       *ModRm = &InstructionData->ModRm;
  SEV_ES_INSTRUCTION_SIB         *Sib = &InstructionData->Sib;

  InstructionData->ModRmPresent = TRUE;
  ModRm->Uint8 = *(InstructionData->End);

  InstructionData->Displacement++;
  InstructionData->Immediate++;
  InstructionData->End++;

  Ext->ModRm.Mod = ModRm->Bits.Mod;
  Ext->ModRm.Reg = (RexPrefix->Bits.R << 3) | ModRm->Bits.Reg;
  Ext->ModRm.Rm  = (RexPrefix->Bits.B << 3) | ModRm->Bits.Rm;

  Ext->RegData = *GetRegisterPointer (Regs, Ext->ModRm.Reg);

  if (Ext->ModRm.Mod == 3) {
    Ext->RmData = *GetRegisterPointer (Regs, Ext->ModRm.Rm);
  } else {
    if (ModRm->Bits.Rm == 4) {
      InstructionData->SibPresent = TRUE;
      Sib->Uint8 = *(InstructionData->End);

      InstructionData->Displacement++;
      InstructionData->Immediate++;
      InstructionData->End++;

      Ext->Sib.Scale = Sib->Bits.Scale;
      Ext->Sib.Index = (RexPrefix->Bits.X << 3) | Sib->Bits.Index;
      Ext->Sib.Base  = (RexPrefix->Bits.B << 3) | Sib->Bits.Base;
    }

    Ext->RmData = GetEffectiveMemoryAddress (Regs, InstructionData);
  }
}

STATIC
VOID
DecodePrefixes (
  EFI_SYSTEM_CONTEXT_X64   *Regs,
  SEV_ES_INSTRUCTION_DATA  *InstructionData
  )
{
  SEV_ES_INSTRUCTION_MODE  Mode;
  SEV_ES_INSTRUCTION_SIZE  ModeDataSize;
  SEV_ES_INSTRUCTION_SIZE  ModeAddrSize;
  UINT8                    *Byte;

  /*TODO: Determine current mode - 64-bit for now */
  Mode = LongMode64Bit;
  ModeDataSize = Size32Bits;
  ModeAddrSize = Size64Bits;

  InstructionData->Mode = Mode;
  InstructionData->DataSize = ModeDataSize;
  InstructionData->AddrSize = ModeAddrSize;

  InstructionData->Prefixes = InstructionData->Begin;

  Byte = InstructionData->Prefixes;
  for ( ; ; Byte++, InstructionData->PrefixSize++) {
    switch (*Byte) {
    case 0x26:
    case 0x2E:
    case 0x36:
    case 0x3E:
      if (Mode != LongMode64Bit) {
        InstructionData->SegmentSpecified = TRUE;
        InstructionData->Segment = (*Byte >> 3) & 3;
      }
      break;

    case 0x40 ... 0x4F:
      InstructionData->RexPrefix.Uint8 = *Byte;
      if (*Byte & 0x08)
        InstructionData->DataSize = Size64Bits;
      break;

    case 0x64:
      InstructionData->SegmentSpecified = TRUE;
      InstructionData->Segment = *Byte & 7;
      break;

    case 0x66:
      if (!InstructionData->RexPrefix.Uint8) {
        InstructionData->DataSize =
          (Mode == LongMode64Bit)       ? Size16Bits :
          (Mode == LongModeCompat32Bit) ? Size16Bits :
          (Mode == LongModeCompat16Bit) ? Size32Bits : 0;
      }
      break;

    case 0x67:
      InstructionData->AddrSize =
        (Mode == LongMode64Bit)       ? Size32Bits :
        (Mode == LongModeCompat32Bit) ? Size16Bits :
        (Mode == LongModeCompat16Bit) ? Size32Bits : 0;
      break;

    case 0xF0:
      break;

    case 0xF2:
      InstructionData->RepMode = RepZ;
      break;

    case 0xF3:
      InstructionData->RepMode = RepNZ;
      break;

    default:
      InstructionData->OpCodes = Byte;
      InstructionData->OpCodeSize = (*Byte == 0x0F) ? 2 : 1;

      InstructionData->End = Byte + InstructionData->OpCodeSize;
      InstructionData->Displacement = InstructionData->End;
      InstructionData->Immediate = InstructionData->End;
      return;
    }
  }
}

STATIC
UINT64
InstructionLength (
  SEV_ES_INSTRUCTION_DATA  *InstructionData
  )
{
  return (UINT64) (InstructionData->End - InstructionData->Begin);
}

STATIC
VOID
InitInstructionData (
  SEV_ES_INSTRUCTION_DATA  *InstructionData,
  GHCB                     *Ghcb,
  EFI_SYSTEM_CONTEXT_X64   *Regs
  )
{
  SetMem (InstructionData, sizeof (*InstructionData), 0);
  InstructionData->Ghcb = Ghcb;
  InstructionData->Begin = (UINT8 *) Regs->Rip;
  InstructionData->End = (UINT8 *) Regs->Rip;

  DecodePrefixes (Regs, InstructionData);
}

STATIC
UINT64
MmioExit (
  GHCB                     *Ghcb,
  EFI_SYSTEM_CONTEXT_X64   *Regs,
  SEV_ES_INSTRUCTION_DATA  *InstructionData
  )
{
  UINT64  ExitInfo1, ExitInfo2;
  UINTN   Status;
  UINTN   Bytes;
  INTN    *Register;

  Bytes = 0;

  switch (*(InstructionData->OpCodes)) {
  /* MMIO write */
  case 0x88:
    Bytes = 1;
  case 0x89:
    DecodeModRm (Regs, InstructionData);
    Bytes = (Bytes) ? Bytes
                    : (InstructionData->DataSize == Size16Bits) ? 2
                    : (InstructionData->DataSize == Size32Bits) ? 4
                    : (InstructionData->DataSize == Size64Bits) ? 8
                    : 0;

    if (InstructionData->Ext.ModRm.Mod == 3) {
      /* NPF on two register operands??? */
      VmgExit (Ghcb, SvmExitUnsupported, SvmExitNpf, 0);
      ASSERT (0);
    }

    ExitInfo1 = InstructionData->Ext.RmData;
    ExitInfo2 = Bytes;
    CopyMem (Ghcb->SharedBuffer, &InstructionData->Ext.RegData, Bytes);

    Ghcb->SaveArea.SwScratch = (UINT64) Ghcb->SharedBuffer;
    Status = VmgExit (Ghcb, SvmExitMmioWrite, ExitInfo1, ExitInfo2);
    if (Status) {
      return Status;
    }
    break;

  /* MMIO read */
  case 0x8A:
    Bytes = 1;
  case 0x8B:
    DecodeModRm (Regs, InstructionData);
    Bytes = (Bytes) ? Bytes
                    : (InstructionData->DataSize == Size16Bits) ? 2
                    : (InstructionData->DataSize == Size32Bits) ? 4
                    : (InstructionData->DataSize == Size64Bits) ? 8
                    : 0;
    if (InstructionData->Ext.ModRm.Mod == 3) {
      /* NPF on two register operands??? */
      VmgExit (Ghcb, SvmExitUnsupported, SvmExitNpf, 0);
      ASSERT (0);
    }

    ExitInfo1 = InstructionData->Ext.RmData;
    ExitInfo2 = Bytes;

    Ghcb->SaveArea.SwScratch = (UINT64) Ghcb->SharedBuffer;
    Status = VmgExit (Ghcb, SvmExitMmioRead, ExitInfo1, ExitInfo2);
    if (Status) {
      return Status;
    }

    Register = GetRegisterPointer (Regs, InstructionData->Ext.ModRm.Reg);
    if (Bytes == 4) {
      /* Zero-extend for 32-bit operation */
      *Register = 0;
    }
    CopyMem (Register, Ghcb->SharedBuffer, Bytes);
    break;

  default:
    Status = GP_EXCEPTION;
    ASSERT (0);
  }

  return Status;
}

STATIC
UINTN
UnsupportedExit (
  GHCB                     *Ghcb,
  EFI_SYSTEM_CONTEXT_X64   *Regs,
  SEV_ES_INSTRUCTION_DATA  *InstructionData
  )
{
  UINTN  Status;

  Status = VmgExit (Ghcb, SvmExitUnsupported, Regs->ExceptionData, 0);
  ASSERT (0);

  return Status;
}

STATIC
UINTN
MsrExit (
  GHCB                     *Ghcb,
  EFI_SYSTEM_CONTEXT_X64   *Regs,
  SEV_ES_INSTRUCTION_DATA  *InstructionData
  )
{
  UINT64  ExitInfo1;
  UINTN   Status;

  ExitInfo1 = 0;

  switch (*(InstructionData->OpCodes + 1)) {
  case 0x30: // WRMSR
    ExitInfo1 = 1;
    Ghcb->SaveArea.Rax = Regs->Rax;
    GhcbSetRegValid (Ghcb, GhcbRax);
    Ghcb->SaveArea.Rdx = Regs->Rdx;
    GhcbSetRegValid (Ghcb, GhcbRdx);
    /* Fallthrough */
  case 0x32: // RDMSR
    Ghcb->SaveArea.Rcx = Regs->Rcx;
    GhcbSetRegValid (Ghcb, GhcbRcx);
    break;
  default:
    VmgExit (Ghcb, SvmExitUnsupported, SvmExitMsr, 0);
    ASSERT (0);
  }

  Status = VmgExit (Ghcb, SvmExitMsr, ExitInfo1, 0);
  if (Status) {
    return Status;
  }

  if (!ExitInfo1) {
    if (!GhcbIsRegValid (Ghcb, GhcbRax) ||
        !GhcbIsRegValid (Ghcb, GhcbRdx)) {
      VmgExit (Ghcb, SvmExitUnsupported, SvmExitMsr, 0);
      ASSERT (0);
    }
    Regs->Rax = Ghcb->SaveArea.Rax;
    Regs->Rdx = Ghcb->SaveArea.Rdx;
  }

  return 0;
}

#define IOIO_TYPE_STR  (1 << 2)
#define IOIO_TYPE_IN   1
#define IOIO_TYPE_INS  (IOIO_TYPE_IN | IOIO_TYPE_STR)
#define IOIO_TYPE_OUT  0
#define IOIO_TYPE_OUTS (IOIO_TYPE_OUT | IOIO_TYPE_STR)

#define IOIO_REP       (1 << 3)

#define IOIO_ADDR_64   (1 << 9)
#define IOIO_ADDR_32   (1 << 8)
#define IOIO_ADDR_16   (1 << 7)

#define IOIO_DATA_32   (1 << 6)
#define IOIO_DATA_16   (1 << 5)
#define IOIO_DATA_8    (1 << 4)

#define IOIO_SEG_ES    (0 << 10)
#define IOIO_SEG_DS    (3 << 10)

STATIC
UINT64
IoioExitInfo (
  EFI_SYSTEM_CONTEXT_X64   *Regs,
  SEV_ES_INSTRUCTION_DATA  *InstructionData
  )
{
  UINT64  ExitInfo = 0;

  switch (*(InstructionData->OpCodes)) {
  case 0x6C: /* INSB / INS mem8, DX */
    ExitInfo |= IOIO_TYPE_INS;
    ExitInfo |= IOIO_DATA_8;
    ExitInfo |= IOIO_SEG_ES;
    ExitInfo |= ((Regs->Rdx & 0xffff) << 16);
    break;
  case 0x6D: /* INSW / INS mem16, DX / INSD / INS mem32, DX */
    ExitInfo |= IOIO_TYPE_INS;
    ExitInfo |= (InstructionData->DataSize == Size16Bits) ? IOIO_DATA_16
                                                          : IOIO_DATA_32;
    ExitInfo |= IOIO_SEG_ES;
    ExitInfo |= ((Regs->Rdx & 0xffff) << 16);
    break;

  case 0x6E: /* OUTSB / OUTS DX, mem8 */
    ExitInfo |= IOIO_TYPE_OUTS;
    ExitInfo |= IOIO_DATA_8;
    ExitInfo |= IOIO_SEG_DS;
    ExitInfo |= ((Regs->Rdx & 0xffff) << 16);
    break;
  case 0x6F: /* OUTSW / OUTS DX, mem16 / OUTSD / OUTS DX, mem32 */
    ExitInfo |= IOIO_TYPE_OUTS;
    ExitInfo |= (InstructionData->DataSize == Size16Bits) ? IOIO_DATA_16
                                                          : IOIO_DATA_32;
    ExitInfo |= IOIO_SEG_DS;
    ExitInfo |= ((Regs->Rdx & 0xffff) << 16);
    break;

  case 0xE4: /* IN AL, imm8 */
    InstructionData->ImmediateSize = 1;
    InstructionData->End++;
    ExitInfo |= IOIO_TYPE_IN;
    ExitInfo |= IOIO_DATA_8;
    ExitInfo |= ((*(InstructionData->OpCodes + 1)) << 16);
    break;
  case 0xE5: /* IN AX, imm8 / IN EAX, imm8 */
    InstructionData->ImmediateSize = 1;
    InstructionData->End++;
    ExitInfo |= IOIO_TYPE_IN;
    ExitInfo |= (InstructionData->DataSize == Size16Bits) ? IOIO_DATA_16
                                                          : IOIO_DATA_32;
    ExitInfo |= ((*(InstructionData->OpCodes + 1)) << 16);
    break;

  case 0xEC: /* IN AL, DX */
    ExitInfo |= IOIO_TYPE_IN;
    ExitInfo |= IOIO_DATA_8;
    ExitInfo |= ((Regs->Rdx & 0xffff) << 16);
    break;
  case 0xED: /* IN AX, DX / IN EAX, DX */
    ExitInfo |= IOIO_TYPE_IN;
    ExitInfo |= (InstructionData->DataSize == Size16Bits) ? IOIO_DATA_16
                                                          : IOIO_DATA_32;
    ExitInfo |= ((Regs->Rdx & 0xffff) << 16);
    break;

  case 0xE6: /* OUT imm8, AL */
    InstructionData->ImmediateSize = 1;
    InstructionData->End++;
    ExitInfo |= IOIO_TYPE_OUT;
    ExitInfo |= IOIO_DATA_8;
    ExitInfo |= ((*(InstructionData->OpCodes + 1)) << 16) | IOIO_TYPE_OUT;
    break;
  case 0xE7: /* OUT imm8, AX / OUT imm8, EAX */
    InstructionData->ImmediateSize = 1;
    InstructionData->End++;
    ExitInfo |= IOIO_TYPE_OUT;
    ExitInfo |= (InstructionData->DataSize == Size16Bits) ? IOIO_DATA_16
                                                          : IOIO_DATA_32;
    ExitInfo |= ((*(InstructionData->OpCodes + 1)) << 16) | IOIO_TYPE_OUT;
    break;

  case 0xEE: /* OUT DX, AL */
    ExitInfo |= IOIO_TYPE_OUT;
    ExitInfo |= IOIO_DATA_8;
    ExitInfo |= ((Regs->Rdx & 0xffff) << 16);
    break;
  case 0xEF: /* OUT DX, AX / OUT DX, EAX */
    ExitInfo |= IOIO_TYPE_OUT;
    ExitInfo |= (InstructionData->DataSize == Size16Bits) ? IOIO_DATA_16
                                                          : IOIO_DATA_32;
    ExitInfo |= ((Regs->Rdx & 0xffff) << 16);
    break;

  default:
    return 0;
  }

  switch (InstructionData->AddrSize) {
  case Size16Bits:
    ExitInfo |= IOIO_ADDR_16;
    break;

  case Size32Bits:
    ExitInfo |= IOIO_ADDR_32;
    break;

  case Size64Bits:
    ExitInfo |= IOIO_ADDR_64;
    break;

  default:
    break;
  }

  if (InstructionData->RepMode) {
    ExitInfo |= IOIO_REP;
  }

  return ExitInfo;
}

STATIC
UINTN
IoioExit (
  GHCB                     *Ghcb,
  EFI_SYSTEM_CONTEXT_X64   *Regs,
  SEV_ES_INSTRUCTION_DATA  *InstructionData
  )
{
  UINT64   ExitInfo1, ExitInfo2;
  UINTN    Status;
  BOOLEAN  String;

  ExitInfo1 = IoioExitInfo (Regs, InstructionData);
  if (!ExitInfo1) {
    VmgExit (Ghcb, SvmExitUnsupported, SvmExitIoioProt, 0);
    ASSERT (0);
  }

  if (!(ExitInfo1 & IOIO_TYPE_IN)) {
    Ghcb->SaveArea.Rax = Regs->Rax;
    GhcbSetRegValid (Ghcb, GhcbRax);
  }

  //FIXME: This is likely needed for the merging cases (size<32 bits)
  //       Pass in zero and perform merge here (only for non-string)
  Ghcb->SaveArea.Rax = Regs->Rax;
  GhcbSetRegValid (Ghcb, GhcbRax);

  String = (ExitInfo1 & IOIO_TYPE_STR) ? TRUE : FALSE;
  if (String) {
    UINTN  IoBytes, VmgExitBytes;
    UINTN  GhcbCount, OpCount;

    Status = 0;

    IoBytes = (ExitInfo1 >> 4) & 0x7;
    GhcbCount = sizeof (Ghcb->SharedBuffer) / IoBytes;

    OpCount = (ExitInfo1 & IOIO_REP) ? Regs->Rcx : 1;
    while (OpCount) {
      ExitInfo2 = MIN (OpCount, GhcbCount);
      VmgExitBytes = ExitInfo2 * IoBytes;

      if (!(ExitInfo1 & IOIO_TYPE_IN)) {
        CopyMem (Ghcb->SharedBuffer, (VOID *) Regs->Rsi, VmgExitBytes);
        Regs->Rsi += VmgExitBytes;
      }

      Ghcb->SaveArea.SwScratch = (UINT64) Ghcb->SharedBuffer;
      Status = VmgExit (Ghcb, SvmExitIoioProt, ExitInfo1, ExitInfo2);
      if (Status) {
        return Status;
      }

      if (ExitInfo1 & IOIO_TYPE_IN) {
        CopyMem ((VOID *) Regs->Rdi, Ghcb->SharedBuffer, VmgExitBytes);
        Regs->Rdi += VmgExitBytes;
      }

      if (ExitInfo1 & IOIO_REP) {
        Regs->Rcx -= ExitInfo2;
      }

      OpCount -= ExitInfo2;
    }
  } else {
    Status = VmgExit (Ghcb, SvmExitIoioProt, ExitInfo1, 0);
    if (Status) {
      return Status;
    }

    if (ExitInfo1 & IOIO_TYPE_IN) {
      if (!GhcbIsRegValid (Ghcb, GhcbRax)) {
        VmgExit (Ghcb, SvmExitUnsupported, SvmExitIoioProt, 0);
        ASSERT (0);
      }
      Regs->Rax = Ghcb->SaveArea.Rax;
    }
  }

  return 0;
}

STATIC
UINTN
CpuidExit (
  GHCB                     *Ghcb,
  EFI_SYSTEM_CONTEXT_X64   *Regs,
  SEV_ES_INSTRUCTION_DATA  *InstructionData
  )
{
  UINTN  Status;

  Ghcb->SaveArea.Rax = Regs->Rax;
  GhcbSetRegValid (Ghcb, GhcbRax);
  Ghcb->SaveArea.Rcx = Regs->Rcx;
  GhcbSetRegValid (Ghcb, GhcbRcx);
  if (Regs->Rax == 0x0000000d) {
    Ghcb->SaveArea.XCr0 = (AsmReadCr4 () & CR4_OSXSAVE) ? AsmXGetBv (0) : 1;
    GhcbSetRegValid (Ghcb, GhcbXCr0);
  }

  Status = VmgExit (Ghcb, SvmExitCpuid, 0, 0);
  if (Status) {
    return Status;
  }

  if (!GhcbIsRegValid (Ghcb, GhcbRax) ||
      !GhcbIsRegValid (Ghcb, GhcbRbx) ||
      !GhcbIsRegValid (Ghcb, GhcbRcx) ||
      !GhcbIsRegValid (Ghcb, GhcbRdx)) {
    VmgExit (Ghcb, SvmExitUnsupported, SvmExitCpuid, 0);
    ASSERT (0);
  }
  Regs->Rax = Ghcb->SaveArea.Rax;
  Regs->Rbx = Ghcb->SaveArea.Rbx;
  Regs->Rcx = Ghcb->SaveArea.Rcx;
  Regs->Rdx = Ghcb->SaveArea.Rdx;

  return 0;
}

UINTN
DoVcCommon (
  GHCB                *Ghcb,
  EFI_SYSTEM_CONTEXT  Context
  )
{
  EFI_SYSTEM_CONTEXT_X64   *Regs = Context.SystemContextX64;
  SEV_ES_INSTRUCTION_DATA  InstructionData;
  NAE_EXIT                 NaeExit;
  UINTN                    ExitCode;
  UINTN                    Status;

  VmgInit (Ghcb);

  ExitCode = Regs->ExceptionData;
  switch (ExitCode) {
  case SvmExitCpuid:
    NaeExit = CpuidExit;
    break;

  case SvmExitIoioProt:
    NaeExit = IoioExit;
    break;

  case SvmExitMsr:
    NaeExit = MsrExit;
    break;

  case SvmExitNpf:
    NaeExit = MmioExit;
    break;

  default:
    NaeExit = UnsupportedExit;
  }

  InitInstructionData (&InstructionData, Ghcb, Regs);

  Status = NaeExit (Ghcb, Regs, &InstructionData);
  if (!Status) {
    Regs->Rip += InstructionLength(&InstructionData);
  }

  VmgDone (Ghcb);

  return 0;
}
