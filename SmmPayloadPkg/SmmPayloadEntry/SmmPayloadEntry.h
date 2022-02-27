/** @file

  Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SMM_PAYLOAD_ENTRY_H__
#define __SMM_PAYLOAD_ENTRY_H__

#include <PiPei.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Guid/MpCpuTaskInfoHob.h>

/**
  Auto-generated function that calls the library constructors for all of the module's
  dependent libraries.
**/
VOID
EFIAPI
ProcessLibraryConstructorList (
  VOID
  );


/**
  The Entry Point for SMM IPL

  Load SMM Core into SMRAM.

  @param  FileHandle  Handle of the file being invoked.
  @param  PeiServices Describes the list of possible PEI Services.

  @retval EFI_SUCCESS    The entry point is executed successfully.
  @retval Other          Some error occurred when executing this entry point.

**/
EFI_STATUS
EFIAPI
SmmIplEntry (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  );


/**
  Add a new HOB to the HOB List.

  @param HobType            Type of the new HOB.
  @param HobLength          Length of the new HOB to allocate.

  @return  NULL if there is no space to create a hob.
  @return  The address point to the new created hob.

**/
VOID *
EFIAPI
CreateHob (
  IN  UINT16  HobType,
  IN  UINT16  HobLength
  );



/**
  Build a Handoff Information Table HOB

  This function initialize a HOB region from EfiMemoryBegin to
  EfiMemoryTop. And EfiFreeMemoryBottom and EfiFreeMemoryTop should
  be inside the HOB region.

  @param[in] EfiMemoryBottom       Total memory start address
  @param[in] EfiMemoryTop          Total memory end address.
  @param[in] EfiFreeMemoryBottom   Free memory start address
  @param[in] EfiFreeMemoryTop      Free memory end address.

  @return   The pointer to the handoff HOB table.

**/
EFI_HOB_HANDOFF_INFO_TABLE *
EFIAPI
HobConstructor (
  IN VOID  *EfiMemoryBottom,
  IN VOID  *EfiMemoryTop,
  IN VOID  *EfiFreeMemoryBottom,
  IN VOID  *EfiFreeMemoryTop
  );

/**
  The Entry point of the MP CPU PEIM

  This function is the Entry point of the MP CPU PEIM which will install the MpServicePpi

  @param  FileHandle  Handle of the file being invoked.
  @param  PeiServices Describes the list of possible PEI Services.

  @retval EFI_SUCCESS   MpServicePpi is installed successfully.

**/
EFI_STATUS
EFIAPI
BuildMpInfoHob (
  IN   EFI_PEI_HOB_POINTERS  Hob
  );

#endif
