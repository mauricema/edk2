/** @file
  HOB Library implementation for Standalone MM Core.

Copyright (c) 2006 - 2014, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.<BR>
Copyright (c) 2018, Linaro, Ltd. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiMm.h>

#include <Library/HobLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MmServicesTableLib.h>

//
// Cache copy of HobList pointer.
//
VOID *gHobList = NULL;

/**
  The constructor function caches the pointer to HOB list.

  The constructor function gets the start address of HOB list from system configuration table.
  It will ASSERT() if that operation fails and it will always return EFI_SUCCESS.

  @param  ImageHandle     The firmware allocated handle for the image.
  @param  MmSystemTable   A pointer to the MM System Table.

  @retval EFI_SUCCESS     The constructor successfully gets HobList.
  @retval Other value     The constructor can't get HobList.

**/
EFI_STATUS
EFIAPI
HobListLibConstructor (
  IN EFI_HANDLE             ImageHandle,
  IN EFI_MM_SYSTEM_TABLE    *MmSystemTable
  )
{
  UINTN       Index;

  gMmst = MmSystemTable;
  for (Index = 0; Index < gMmst->NumberOfTableEntries; Index++) {
    if (CompareGuid (&gEfiHobListGuid, &gMmst->MmConfigurationTable[Index].VendorGuid)) {
      gHobList = gMmst->MmConfigurationTable[Index].VendorTable;
      break;
    }
  }
  return EFI_SUCCESS;
}

/**
  Returns the pointer to the HOB list.

  This function returns the pointer to first HOB in the list.
  If the pointer to the HOB list is NULL, then ASSERT().

  @return The pointer to the HOB list.

**/
VOID *
EFIAPI
GetHobList (
  VOID
  )
{
  UINTN       Index;

  if (gHobList == NULL) {
    for (Index = 0; Index < gMmst->NumberOfTableEntries; Index++) {
      if (CompareGuid (&gEfiHobListGuid, &gMmst->MmConfigurationTable[Index].VendorGuid)) {
        gHobList = gMmst->MmConfigurationTable[Index].VendorTable;
        break;
      }
    }
  }
  ASSERT (gHobList != NULL);
  return gHobList;
}

