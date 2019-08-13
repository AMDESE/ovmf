/** @file
  MSR Definitions.

  Provides defines for Machine Specific Registers(MSR) indexes. Data structures
  are provided for MSRs that contain one or more bit fields.  If the MSR value
  returned is a single 32-bit or 64-bit value, then a data structure is not
  provided for that MSR.

  Copyright (c) 2017, Advanced Micro Devices. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
  AMD64 Architecture Programming Manaul volume 2, March 2017, Sections 15.34

**/

#ifndef __FAM17_MSR_H__
#define __FAM17_MSR_H__

/**
  Secure Encrypted Virtualization - Encrypted State (SEV-ES) GHCB register

**/
#define MSR_SEV_ES_GHCB                    0xc0010130

/**
  MSR information returned for #MSR_SEV_ES_GHCB
**/
typedef union {
  struct {
    UINT32  GhcbNegotiateBit:1;

    UINT32  Reserved:31;
  } Bits;

  struct {
    UINT8   Reserved[3];
    UINT8   SevEncryptionBitPos;
    UINT16  SevEsProtocolMin;
    UINT16  SevEsProtocolMax;
  } GhcbProtocol;

  VOID    *Ghcb;

  UINT64  GhcbPhysicalAddress;
} MSR_SEV_ES_GHCB_REGISTER;

/**
  Secure Encrypted Virtualization (SEV) status register

**/
#define MSR_SEV_STATUS                     0xc0010131

/**
  MSR information returned for #MSR_SEV_STATUS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Secure Encrypted Virtualization (Sev) is enabled
    ///
    UINT32  SevBit:1;

    ///
    /// [Bit 1] Secure Encrypted Virtualization Encrypted State (SevEs) is enabled
    ///
    UINT32  SevEsBit:1;

    UINT32  Reserved:30;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SEV_STATUS_REGISTER;

#endif
