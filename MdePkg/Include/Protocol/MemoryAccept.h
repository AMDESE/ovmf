/** @file
  The file provides the protocol to provide interface to accept memory.
  Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
**/

#ifndef __MEMORY_ACCEPT_H___
#define __MEMORY_ACCEPT_H___

#define EFI_MEMORY_ACCEPT_PROTOCOL_GUID \
  { 0x38c74800, 0x5590, 0x4db4, { 0xa0, 0xf3, 0x67, 0x5d, 0x9b, 0x8e, 0x80, 0x26 } };

typedef struct _EFI_MEMORY_ACCEPT_PROTOCOL EFI_MEMORY_ACCEPT_PROTOCOL;

/**
  @param This                   A pointer to a MEMORY_ACCEPT_PROTOCOL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_ACCEPT_MEMORY)(
  IN  EFI_MEMORY_ACCEPT_PROTOCOL    *This,
  IN  EFI_PHYSICAL_ADDRESS          StartAddress,
  IN  UINTN                         Size
);

///
/// The MEMORY_ACCEPT_PROTOCOL provides the ability for memory services
/// to accept memory.
///
struct _EFI_MEMORY_ACCEPT_PROTOCOL {
  EFI_ACCEPT_MEMORY   AcceptMemory;
};

extern EFI_GUID gEfiMemoryAcceptProtocolGuid;

#endif
