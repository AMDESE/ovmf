
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include "AMDSevVcCommon.h"

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

  UINTN  RegData;
  UINTN  RmData;
} SEV_ES_INSTRUCTION_OPCODE_EXT;

typedef struct {
  GHCB                           *Ghcb;

  SEV_ES_INSTRUCTION_MODE        Mode;
  SEV_ES_INSTRUCTION_SIZE        DataSize;
  SEV_ES_INSTRUCTION_SIZE        AddrSize;
  BOOLEAN                        SegmentSpecified;
  SEV_ES_INSTRUCTION_SEGMENT     Segment;
  SEV_ES_INSTRUCTION_REP         RepMode;
  UINT8                          *OpCodes;
  UINT8                          *Displacement;

  SEV_ES_INSTRUCTION_REX_PREFIX  RexPrefix;

  BOOLEAN                        ModRmPresent;
  SEV_ES_INSTRUCTION_MODRM       ModRm;

  BOOLEAN                        SibPresent;
  SEV_ES_INSTRUCTION_SIB         Sib;

  BOOLEAN                        DisplacementSize;

  SEV_ES_INSTRUCTION_OPCODE_EXT  Ext;
} SEV_ES_INSTRUCTION_DATA;

static
VOID
InitInstructionData (
  SEV_ES_INSTRUCTION_DATA  *InstructionData,
  GHCB                     *Ghcb
  )
{
  SetMem (InstructionData, sizeof (*InstructionData), 0);
  InstructionData->Ghcb = Ghcb;
}

static
UINT64
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

  Byte = (UINT8 *) Regs->Rip;
  while (TRUE) {
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
      InstructionData->Displacement = Byte + 1;
      return (UINT64) Byte;
    }

    Byte++;
  }
}

static
UINT64
DecodeInstruction (
  EFI_SYSTEM_CONTEXT_X64   *Regs,
  SEV_ES_INSTRUCTION_DATA  *InstructionData
  )
{
  return DecodePrefixes (Regs, InstructionData);
}

static
VOID
AdvanceRip (
  EFI_SYSTEM_CONTEXT_X64   *Regs,
  UINTN                    Bytes
  )
{
  SEV_ES_INSTRUCTION_DATA  InstructionData;

  InitInstructionData (&InstructionData, NULL);

  Regs->Rip = DecodePrefixes (Regs, &InstructionData);
  Regs->Rip += Bytes;
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

static
UINT64
IoioExitInfo (
  EFI_SYSTEM_CONTEXT_X64   *Regs,
  SEV_ES_INSTRUCTION_DATA  *InstructionData
  )
{
  UINT64  ExitInfo = 0;

  switch (*(InstructionData->OpCodes)) {
  case 0xE4: /* IN AL, imm8 */
    ExitInfo |= IOIO_TYPE_IN;
    ExitInfo |= IOIO_DATA_8;
    ExitInfo |= ((*(InstructionData->OpCodes + 1)) << 16);
    break;
  case 0xE5: /* IN AX, imm8 / IN EAX, imm8 */
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
    ExitInfo |= IOIO_TYPE_OUT;
    ExitInfo |= IOIO_DATA_8;
    ExitInfo |= ((*(InstructionData->OpCodes + 1)) << 16) | IOIO_TYPE_OUT;
    break;
  case 0xE7: /* OUT imm8, AX / OUT imm8, EAX */
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

UINTN
DoVcCommon(
  GHCB                   *Ghcb,
  EFI_SYSTEM_CONTEXT_X64 *Regs
  )
{
  SEV_ES_INSTRUCTION_DATA  InstructionData;
  UINTN                    ExitCode;
  UINTN                    Status;
  UINT64                   ExitInfo1;
  UINT8                    *OpCode;
  UINTN                    InstructionLength;

  VmgInit (Ghcb);

  ExitCode = Regs->ExceptionData;
  switch (ExitCode) {
  case SvmExitIoioProt:
    InitInstructionData (&InstructionData, Ghcb);
    OpCode = (UINT8 *) DecodeInstruction (Regs, &InstructionData);

    ExitInfo1 = IoioExitInfo (Regs, &InstructionData);
    if (!ExitInfo1) {
      VmgExit (Ghcb, SvmExitUnsupported, ExitCode, 0);
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

    Status = VmgExit (Ghcb, ExitCode, ExitInfo1, 0);
    if (Status) {
      break;
    }

    if (ExitInfo1 & IOIO_TYPE_IN) {
      if (!GhcbIsRegValid (Ghcb, GhcbRax)) {
        VmgExit (Ghcb, SvmExitUnsupported, ExitCode, 0);
        ASSERT (0);
      }
      Regs->Rax = Ghcb->SaveArea.Rax;
    }

    switch (*OpCode) {
    case 0xE4: /* IN AL, imm8 */
    case 0xE5: /* IN AX, imm8 / IN EAX, imm8 */
    case 0xE6: /* OUT imm8, AL */
    case 0xE7: /* OUT imm8, AX / OUT imm8, EAX */
      InstructionLength = 2;
      break;
    default:
      InstructionLength = 1;
    }

    AdvanceRip (Regs, InstructionLength);
    break;

  default:
    Status = VmgExit (Ghcb, SvmExitUnsupported, ExitCode, 0);
  }

  VmgDone (Ghcb);

  return Status;
}
