/** @file

  Provides a simple interval search tree implementation that will be used
  by the SnpValidateSystemRam() to keep track of the memory range validated
  during the SEC/PEI phases.

  Copyright (c) 2020 - 2021, AMD Incorporated. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/MemoryAllocationLib.h>

#include "SnpPageStateTrack.h"

STATIC
SNP_VALIDATED_RANGE *
AllocNewNode (
  IN  UINTN     StartAddress,
  IN  UINTN     EndAddress
  )
{
  SNP_VALIDATED_RANGE   *Node;

  Node = AllocatePool (sizeof (SNP_VALIDATED_RANGE));
  if (Node == NULL) {
    return NULL;
  }

  Node->StartAddress  = StartAddress;
  Node->EndAddress = EndAddress;
  Node->MaxAddress = Node->EndAddress;
  Node->Left = Node->Right = NULL;

  return Node;
}

STATIC
BOOLEAN
RangeIsOverlap (
  IN  SNP_VALIDATED_RANGE     *Node,
  IN  UINTN                   StartAddress,
  IN  UINTN                   EndAddress
  )
{
  if (Node->StartAddress < EndAddress && StartAddress < Node->EndAddress) {
    return TRUE;
  }

  return FALSE;
}


/**
 Function to find the overlapping range within the interval tree. If range is not
 found then NULL is returned.

 */
SNP_VALIDATED_RANGE *
FindOverlapRange (
  IN  SNP_VALIDATED_RANGE   *RootNode,
  IN  UINTN                 StartAddress,
  IN  UINTN                 EndAddress
  )
{
  // Tree is empty or no overlap found
  if (RootNode == NULL) {
    return NULL;
  }

  // Check with the range exist in the root node
  if (RangeIsOverlap(RootNode, StartAddress, EndAddress)) {
    return RootNode;
  }

  //
  // If the left child of root is present and the max of the left child is
  // greater than or equal to a given range then requested range will overlap
  // with left subtree
  //
  if (RootNode->Left != NULL && (RootNode->Left->MaxAddress >= StartAddress)) {
    return FindOverlapRange (RootNode->Left, StartAddress, EndAddress);
  }

  // The range can only overlap with the right subtree
  return FindOverlapRange (RootNode->Right, StartAddress, EndAddress);
}

/**
 Function to insert the validated range in the interval search tree.

 */
SNP_VALIDATED_RANGE *
AddRangeToIntervalTree (
  IN  SNP_VALIDATED_RANGE   *RootNode,
  IN  UINTN                 StartAddress,
  IN  UINTN                 EndAddress
  )
{
  // Tree is empty or we reached to the leaf
  if (RootNode == NULL) {
    return AllocNewNode (StartAddress, EndAddress);
  }

  // If the StartAddress is smaller then the BaseAddress then go to the left in the tree.
  if (StartAddress < RootNode->StartAddress) {
    RootNode->Left = AddRangeToIntervalTree (RootNode->Left, StartAddress, EndAddress);
  } else {
    RootNode->Right = AddRangeToIntervalTree (RootNode->Right, StartAddress, EndAddress);
  }

  // Update the max value of the ancestor if needed
  if (RootNode->MaxAddress < EndAddress) {
    RootNode->MaxAddress = EndAddress;
  }

  return RootNode;
}
