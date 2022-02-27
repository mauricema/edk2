/** @file
Implementation of MP CPU driver for PEI phase.

This PEIM is to expose the MpService Ppi

  Copyright (c) 2012 - 2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

#include <Ppi/MpServices.h>
#include <Ppi/MasterBootMode.h>
#include <Ppi/SecPlatformInformation.h>

#include <Guid/MpInformation.h>

#include <Library/DebugLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/BaseLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/SynchronizationLib.h>



/**
  This function will get CPU count from system,
  and build Hob to save the data.

  @param MaxCpuCount  The MaxCpuCount could be supported by system
**/
VOID
CountCpuNumber (
  IN UINTN                      MaxCpuCount
  )
{
  UINTN                      Index;
  //EFI_STATUS                 Status;
  MY_MP_INFORMATION_HOB_DATA MpInformationData;
  UINTN                      NumberOfProcessors;
  UINTN                      NumberOfEnabledProcessors;
  EFI_PROCESSOR_INFORMATION  ProcessorInfoBuffer;


  DEBUG((EFI_D_ERROR, "PeiGetNumberOfProcessors - NumberOfProcessors - %x\n", NumberOfProcessors));
  DEBUG((EFI_D_ERROR, "PeiGetNumberOfProcessors - NumberOfEnabledProcessors - %x\n", NumberOfEnabledProcessors));

  //
  // Record MP information
  //
  MpInformationData.NumberOfProcessors        = NumberOfProcessors;
  MpInformationData.NumberOfEnabledProcessors = NumberOfEnabledProcessors;
  for (Index = 0; Index < NumberOfProcessors; Index++) {

    DEBUG((EFI_D_ERROR, "PeiGetProcessorInfo - Index - %x\n", Index));
    DEBUG((EFI_D_ERROR, "PeiGetProcessorInfo - ProcessorId      - %016lx\n", ProcessorInfoBuffer.ProcessorId));
    DEBUG((EFI_D_ERROR, "PeiGetProcessorInfo - StatusFlag       - %08x\n", ProcessorInfoBuffer.StatusFlag));
    DEBUG((EFI_D_ERROR, "PeiGetProcessorInfo - Location.Package - %08x\n", ProcessorInfoBuffer.Location.Package));
    DEBUG((EFI_D_ERROR, "PeiGetProcessorInfo - Location.Core    - %08x\n", ProcessorInfoBuffer.Location.Core));
    DEBUG((EFI_D_ERROR, "PeiGetProcessorInfo - Location.Thread  - %08x\n", ProcessorInfoBuffer.Location.Thread));
    CopyMem (&MpInformationData.ProcessorInfoBuffer[Index], &ProcessorInfoBuffer, sizeof(ProcessorInfoBuffer));
  }

  BuildGuidDataHob (
    &gMpInformationHobGuid,
    (VOID *)&MpInformationData,
    sizeof(MpInformationData)
    );
}

