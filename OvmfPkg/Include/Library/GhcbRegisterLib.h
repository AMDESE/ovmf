/** @file

  Declarations of utility functions used for GHCB GPA registration.

  Copyright (C) 2020, AMD Inc, All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _GHCB_REGISTER_LIB_H_
#define _GHCB_REGISTER_LIB_H_


/**

  This function can be used to register the GPA.

  @param[in]  Address           The physical address that need to be registered.

  @retval EFI_SUCCESS           The physical address is succesfully registered.
  @retval EFI_ERROR             Failed to register the physical address.

**/
EFI_STATUS
EFIAPI
GhcbGPARegister (
  IN  EFI_PHYSICAL_ADDRESS   Address
  );

#endif // _GHCB_REGISTER_LIB_H_
