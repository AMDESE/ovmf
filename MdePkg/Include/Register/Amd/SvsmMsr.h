/** @file
  MSR Definitions.

  Provides defines for Machine Specific Registers(MSR) indexes. Data structures
  are provided for MSRs that contain one or more bit fields.  If the MSR value
  returned is a single 32-bit or 64-bit value, then a data structure is not
  provided for that MSR.

  Copyright (c) 2023, Advanced Micro Devices. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SVSM_MSR_H__
#define __SVSM_MSR_H__

/**
  Secure VM Service Module CAA register

**/
#define MSR_SVSM_CAA  0xc001f000

/**
  MSR information returned for #MSR_SVSM_CAA
**/
typedef union {
  struct {
    UINT32  Lower32Bits;
    UINT32  Upper32Bits;
  } Bits;

  UINT64    Uint64;
} MSR_SVSM_CAA_REGISTER;

#endif
