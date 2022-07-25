/** @file
  VMGEXIT Base Support Library.

  Copyright (C) 2020, Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Uefi.h>
#include <Library/VmgExitLib.h>

/**
  Perform VMGEXIT.

  Sets the necessary fields of the GHCB, invokes the VMGEXIT instruction and
  then handles the return actions.

  The base library function returns an error in the form of a
  GHCB_EVENT_INJECTION representing a GP_EXCEPTION.

  @param[in, out]  Ghcb       A pointer to the GHCB
  @param[in]       ExitCode   VMGEXIT code to be assigned to the SwExitCode
                              field of the GHCB.
  @param[in]       ExitInfo1  VMGEXIT information to be assigned to the
                              SwExitInfo1 field of the GHCB.
  @param[in]       ExitInfo2  VMGEXIT information to be assigned to the
                              SwExitInfo2 field of the GHCB.

  @retval  0                  VMGEXIT succeeded.
  @return                     Exception number to be propagated, VMGEXIT
                              processing did not succeed.

**/
UINT64
EFIAPI
VmgExit (
  IN OUT GHCB    *Ghcb,
  IN     UINT64  ExitCode,
  IN     UINT64  ExitInfo1,
  IN     UINT64  ExitInfo2
  )
{
  GHCB_EVENT_INJECTION  Event;

  Event.Uint64          = 0;
  Event.Elements.Vector = GP_EXCEPTION;
  Event.Elements.Type   = GHCB_EVENT_INJECTION_TYPE_EXCEPTION;
  Event.Elements.Valid  = 1;

  return Event.Uint64;
}

/**
  Perform pre-VMGEXIT initialization/preparation.

  Performs the necessary steps in preparation for invoking VMGEXIT. Must be
  called before setting any fields within the GHCB.

  @param[in, out]  Ghcb            A pointer to the GHCB
  @param[in, out]  InterruptState  A pointer to hold the current interrupt
                                   state, used for restoring in VmgDone ()

**/
VOID
EFIAPI
VmgInit (
  IN OUT GHCB     *Ghcb,
  IN OUT BOOLEAN  *InterruptState
  )
{
}

/**
  Perform post-VMGEXIT cleanup.

  Performs the necessary steps to cleanup after invoking VMGEXIT. Must be
  called after obtaining needed fields within the GHCB.

  @param[in, out]  Ghcb            A pointer to the GHCB
  @param[in]       InterruptState  An indicator to conditionally (re)enable
                                   interrupts

**/
VOID
EFIAPI
VmgDone (
  IN OUT GHCB     *Ghcb,
  IN     BOOLEAN  InterruptState
  )
{
}

/**
  Marks a field at the specified offset as valid in the GHCB.

  The ValidBitmap area represents the areas of the GHCB that have been marked
  valid. Set the bit in ValidBitmap for the input offset.

  @param[in, out] Ghcb    Pointer to the Guest-Hypervisor Communication Block
  @param[in]      Offset  Qword offset in the GHCB to mark valid

**/
VOID
EFIAPI
VmgSetOffsetValid (
  IN OUT GHCB           *Ghcb,
  IN     GHCB_REGISTER  Offset
  )
{
}

/**
  Checks if a specified offset is valid in the GHCB.

  The ValidBitmap area represents the areas of the GHCB that have been marked
  valid. Return whether the bit in the ValidBitmap is set for the input offset.

  @param[in]  Ghcb            A pointer to the GHCB
  @param[in]  Offset          Qword offset in the GHCB to mark valid

  @retval TRUE                Offset is marked valid in the GHCB
  @retval FALSE               Offset is not marked valid in the GHCB

**/
BOOLEAN
EFIAPI
VmgIsOffsetValid (
  IN GHCB           *Ghcb,
  IN GHCB_REGISTER  Offset
  )
{
  return FALSE;
}

/**
  Handle a #VC exception.

  Performs the necessary processing to handle a #VC exception.

  The base library function returns an error equal to VC_EXCEPTION,
  to be propagated to the standard exception handling stack.

  @param[in, out]  ExceptionType  Pointer to an EFI_EXCEPTION_TYPE to be set
                                  as value to use on error.
  @param[in, out]  SystemContext  Pointer to EFI_SYSTEM_CONTEXT

  @retval  EFI_SUCCESS            Exception handled
  @retval  EFI_UNSUPPORTED        #VC not supported, (new) exception value to
                                  propagate provided
  @retval  EFI_PROTOCOL_ERROR     #VC handling failed, (new) exception value to
                                  propagate provided

**/
EFI_STATUS
EFIAPI
VmgExitHandleVc (
  IN OUT EFI_EXCEPTION_TYPE  *ExceptionType,
  IN OUT EFI_SYSTEM_CONTEXT  SystemContext
  )
{
  *ExceptionType = VC_EXCEPTION;

  return EFI_UNSUPPORTED;
}

/**
  Report the presence of an Secure Virtual Services Module (SVSM).

  Determines the presence of an SVSM.

  @retval  TRUE                   An SVSM is present
  @retval  FALSE                  An SVSM is not present

**/
BOOLEAN
EFIAPI
VmgExitSvsmPresent (
  VOID
  )
{
  return FALSE;
}

/**
  Report the VMPL level at which the SEV-SNP guest is running.

  Determines the VMPL level at which the guest is running. If an SVSM is
  not present, then it must be VMPL0, otherwise return what is reported
  by the SVSM.

  @return                         The VMPL level

**/
UINT8
EFIAPI
VmgExitGetVmpl (
  VOID
  )
{
  return 0;
}

/**
  Perform a PVALIDATE operation for the page ranges specified.

  Validate or rescind the validation of the specified pages.

  @param[in]       Info           Pointer to a page state change structure
  @param[in]       Validate       Boolean indicator as to whether to validate
                                  or rescind the page validation status

**/
VOID
EFIAPI
VmgExitPvalidate (
  IN SNP_PAGE_STATE_CHANGE_INFO  *Info,
  IN BOOLEAN                     Validate
  )
{
}

/**
  Perform an RMPADJUST operation to alter the VMSA setting of a page.

  Add or remove the VMSA attribute for a page.

  @param[in]       Vmsa           Pointer to an SEV-ES save area page
  @param[in]       ApicId         APIC ID associated with the VMSA
  @param[in]       SetVmsa        Boolean indicator as to whether to set or
                                  or clear the VMSA setting for the page

  @retval  EFI_SUCCESS            RMPADJUST operation successful
  @retval  EFI_UNSUPPORTED        Operation is not supported
  @retval  EFI_INVALID_PARAMETER  RMPADJUST operation failed, an invalid
                                  parameter was supplied

**/
EFI_STATUS
EFIAPI
VmgExitVmsaRmpAdjust (
  IN SEV_ES_SAVE_AREA  *Vmsa,
  IN UINT32            ApicId,
  IN BOOLEAN           SetVmsa
  )
{
  return EFI_UNSUPPORTED;
}
