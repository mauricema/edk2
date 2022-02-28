/** @file
  Provide constructor and GetTick for Base instance of ACPI Timer Library

  Copyright (C) 2014, Gabriel L. Somlo <somlo@cmu.edu>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/HobLib.h>
#include <Library/PciLib.h>
#include <Guid/AcpiBoardInfoGuid.h>

//
// Cached ACPI Timer IO Address
//
STATIC UINT32 mAcpiTimerIoAddr;

/**
  The constructor function caches the ACPI tick counter address, and,
  if necessary, enables ACPI IO space.

  @retval EFI_SUCCESS   The constructor always returns RETURN_SUCCESS.

**/
RETURN_STATUS
EFIAPI
AcpiTimerLibConstructor (
  VOID
  )
{
  EFI_HOB_GUID_TYPE  *GuidHob;
  ACPI_BOARD_INFO    *pAcpiBoardInfo;

  //
  // Find the acpi board information guid hob
  //
  GuidHob = GetFirstGuidHob (&gUefiAcpiBoardInfoGuid);
  ASSERT (GuidHob != NULL);

  pAcpiBoardInfo = (ACPI_BOARD_INFO *)GET_GUID_HOB_DATA (GuidHob);

  mAcpiTimerIoAddr = (UINT32)pAcpiBoardInfo->PmTimerRegBase;

  return RETURN_SUCCESS;
}

/**
  Internal function to read the current tick counter of ACPI.

  Read the current ACPI tick counter using the counter address cached
  by this instance's constructor.

  @return The tick counter read.

**/
UINT32
InternalAcpiGetTimerTick (
  VOID
  )
{
  //
  //   Return the current ACPI timer value.
  //
  return IoRead32 (mAcpiTimerIoAddr);
}
