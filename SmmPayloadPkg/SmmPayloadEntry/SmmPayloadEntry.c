/** @file

  Copyright (c) 2014 - 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SmmPayloadEntry.h"
#include <UniversalPayload/SerialPortInfo.h>
#include <UniversalPayload/AcpiTable.h>
#include <Guid/MpInformation.h>

#pragma pack(1)
typedef struct {
  UINT64                     NumberOfProcessors;
  UINT64                     NumberOfEnabledProcessors;
  EFI_PROCESSOR_INFORMATION  ProcessorInfoBuffer[64];
} MY_MP_INFORMATION_HOB_DATA;
#pragma pack()

#define MEMORY_ATTRIBUTE_MASK  (EFI_RESOURCE_ATTRIBUTE_PRESENT             |        \
                                       EFI_RESOURCE_ATTRIBUTE_INITIALIZED         | \
                                       EFI_RESOURCE_ATTRIBUTE_TESTED              | \
                                       EFI_RESOURCE_ATTRIBUTE_READ_PROTECTED      | \
                                       EFI_RESOURCE_ATTRIBUTE_WRITE_PROTECTED     | \
                                       EFI_RESOURCE_ATTRIBUTE_EXECUTION_PROTECTED | \
                                       EFI_RESOURCE_ATTRIBUTE_READ_ONLY_PROTECTED | \
                                       EFI_RESOURCE_ATTRIBUTE_16_BIT_IO           | \
                                       EFI_RESOURCE_ATTRIBUTE_32_BIT_IO           | \
                                       EFI_RESOURCE_ATTRIBUTE_64_BIT_IO           | \
                                       EFI_RESOURCE_ATTRIBUTE_PERSISTENT          )

#define TESTED_MEMORY_ATTRIBUTES  (EFI_RESOURCE_ATTRIBUTE_PRESENT     |     \
                                       EFI_RESOURCE_ATTRIBUTE_INITIALIZED | \
                                       EFI_RESOURCE_ATTRIBUTE_TESTED      )

/**
  Found the Resource Descriptor HOB that contains a range (Base, Top)

  @param[in] HobList    Hob start address
  @param[in] Base       Memory start address
  @param[in] Top        Memory end address.

  @retval     The pointer to the Resource Descriptor HOB.
**/
EFI_HOB_RESOURCE_DESCRIPTOR *
FindResourceDescriptorByRange (
  IN VOID                  *HobList,
  IN EFI_PHYSICAL_ADDRESS  Base,
  IN EFI_PHYSICAL_ADDRESS  Top
  )
{
  EFI_PEI_HOB_POINTERS         Hob;
  EFI_HOB_RESOURCE_DESCRIPTOR  *ResourceHob;

  for (Hob.Raw = (UINT8 *)HobList; !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
    //
    // Skip all HOBs except Resource Descriptor HOBs
    //
    if (GET_HOB_TYPE (Hob) != EFI_HOB_TYPE_RESOURCE_DESCRIPTOR) {
      continue;
    }

    //
    // Skip Resource Descriptor HOBs that do not describe tested system memory
    //
    ResourceHob = Hob.ResourceDescriptor;
    if (ResourceHob->ResourceType != EFI_RESOURCE_SYSTEM_MEMORY) {
      continue;
    }

    if ((ResourceHob->ResourceAttribute & MEMORY_ATTRIBUTE_MASK) != TESTED_MEMORY_ATTRIBUTES) {
      continue;
    }

    //
    // Skip Resource Descriptor HOBs that do not contain the PHIT range EfiFreeMemoryBottom..EfiFreeMemoryTop
    //
    if (Base < ResourceHob->PhysicalStart) {
      continue;
    }

    if (Top > (ResourceHob->PhysicalStart + ResourceHob->ResourceLength)) {
      continue;
    }

    return ResourceHob;
  }

  return NULL;
}

/**
  Find the highest below 4G memory resource descriptor, except the input Resource Descriptor.

  @param[in] HobList                 Hob start address
  @param[in] MinimalNeededSize       Minimal needed size.
  @param[in] ExceptResourceHob       Ignore this Resource Descriptor.

  @retval     The pointer to the Resource Descriptor HOB.
**/
EFI_HOB_RESOURCE_DESCRIPTOR *
FindAnotherHighestBelow4GResourceDescriptor (
  IN VOID                         *HobList,
  IN UINTN                        MinimalNeededSize,
  IN EFI_HOB_RESOURCE_DESCRIPTOR  *ExceptResourceHob
  )
{
  EFI_PEI_HOB_POINTERS         Hob;
  EFI_HOB_RESOURCE_DESCRIPTOR  *ResourceHob;
  EFI_HOB_RESOURCE_DESCRIPTOR  *ReturnResourceHob;

  ReturnResourceHob = NULL;

  for (Hob.Raw = (UINT8 *)HobList; !END_OF_HOB_LIST (Hob); Hob.Raw = GET_NEXT_HOB (Hob)) {
    //
    // Skip all HOBs except Resource Descriptor HOBs
    //
    if (GET_HOB_TYPE (Hob) != EFI_HOB_TYPE_RESOURCE_DESCRIPTOR) {
      continue;
    }

    //
    // Skip Resource Descriptor HOBs that do not describe tested system memory
    //
    ResourceHob = Hob.ResourceDescriptor;
    if (ResourceHob->ResourceType != EFI_RESOURCE_SYSTEM_MEMORY) {
      continue;
    }

    if ((ResourceHob->ResourceAttribute & MEMORY_ATTRIBUTE_MASK) != TESTED_MEMORY_ATTRIBUTES) {
      continue;
    }

    //
    // Skip if the Resource Descriptor HOB equals to ExceptResourceHob
    //
    if (ResourceHob == ExceptResourceHob) {
      continue;
    }

    //
    // Skip Resource Descriptor HOBs that are beyond 4G
    //
    if ((ResourceHob->PhysicalStart + ResourceHob->ResourceLength) > BASE_4GB) {
      continue;
    }

    //
    // Skip Resource Descriptor HOBs that are too small
    //
    if (ResourceHob->ResourceLength < MinimalNeededSize) {
      continue;
    }

    //
    // Return the topest Resource Descriptor
    //
    if (ReturnResourceHob == NULL) {
      ReturnResourceHob = ResourceHob;
    } else {
      if (ReturnResourceHob->PhysicalStart < ResourceHob->PhysicalStart) {
        ReturnResourceHob = ResourceHob;
      }
    }
  }

  return ReturnResourceHob;
}


/**
  Add HOB into HOB list

  @param[in]  Hob    The HOB to be added into the HOB list.
**/
VOID
AddNewHob (
  IN EFI_PEI_HOB_POINTERS  *Hob
  )
{
  EFI_PEI_HOB_POINTERS  NewHob;

  if (Hob->Raw == NULL) {
    return;
  }

  NewHob.Header = CreateHob (Hob->Header->HobType, Hob->Header->HobLength);

  if (NewHob.Header != NULL) {
    CopyMem (NewHob.Header + 1, Hob->Header + 1, Hob->Header->HobLength - sizeof (EFI_HOB_GENERIC_HEADER));
  }
}



/**
  Get Package ID/Core ID/Thread ID of a processor.

  The algorithm assumes the target system has symmetry across physical
  package  boundaries with respect to the number of logical processors
  per package,  number of cores per package.

  @param[in]  InitialApicId  Initial APIC ID of the target logical processor.
  @param[out]  Package       Returns the processor package ID.
  @param[out]  Core          Returns the processor core ID.
  @param[out]  Thread        Returns the processor thread ID.
**/
VOID
EFIAPI
GetCpuLocationByApicId (
  IN  UINT32  InitialApicId,
  OUT UINT32  *Package  OPTIONAL,
  OUT UINT32  *Core    OPTIONAL,
  OUT UINT32  *Thread  OPTIONAL
  )
{
  BOOLEAN                             TopologyLeafSupported;
  CPUID_VERSION_INFO_EBX              VersionInfoEbx;
  CPUID_VERSION_INFO_EDX              VersionInfoEdx;
  CPUID_CACHE_PARAMS_EAX              CacheParamsEax;
  CPUID_EXTENDED_TOPOLOGY_EAX         ExtendedTopologyEax;
  CPUID_EXTENDED_TOPOLOGY_EBX         ExtendedTopologyEbx;
  CPUID_EXTENDED_TOPOLOGY_ECX         ExtendedTopologyEcx;
  UINT32                              MaxStandardCpuIdIndex;
  UINT32                              MaxExtendedCpuIdIndex;
  UINT32                              SubIndex;
  UINTN                               LevelType;
  UINT32                              MaxLogicProcessorsPerPackage;
  UINT32                              MaxCoresPerPackage;
  UINTN                               ThreadBits;
  UINTN                               CoreBits;

  //
  // Check if the processor is capable of supporting more than one logical processor.
  //
  AsmCpuid (CPUID_VERSION_INFO, NULL, NULL, NULL, &VersionInfoEdx.Uint32);
  if (VersionInfoEdx.Bits.HTT == 0) {
    if (Thread != NULL) {
      *Thread = 0;
    }

    if (Core != NULL) {
      *Core = 0;
    }

    if (Package != NULL) {
      *Package = 0;
    }

    return;
  }

  //
  // Assume three-level mapping of APIC ID: Package|Core|Thread.
  //
  ThreadBits = 0;
  CoreBits   = 0;

  //
  // Get max index of CPUID
  //
  AsmCpuid (CPUID_SIGNATURE, &MaxStandardCpuIdIndex, NULL, NULL, NULL);
  AsmCpuid (CPUID_EXTENDED_FUNCTION, &MaxExtendedCpuIdIndex, NULL, NULL, NULL);

  //
  // If the extended topology enumeration leaf is available, it
  // is the preferred mechanism for enumerating topology.
  //
  TopologyLeafSupported = FALSE;
  if (MaxStandardCpuIdIndex >= CPUID_EXTENDED_TOPOLOGY) {
    AsmCpuidEx (
      CPUID_EXTENDED_TOPOLOGY,
      0,
      &ExtendedTopologyEax.Uint32,
      &ExtendedTopologyEbx.Uint32,
      &ExtendedTopologyEcx.Uint32,
      NULL
      );
    //
    // If CPUID.(EAX=0BH, ECX=0H):EBX returns zero and maximum input value for
    // basic CPUID information is greater than 0BH, then CPUID.0BH leaf is not
    // supported on that processor.
    //
    if (ExtendedTopologyEbx.Uint32 != 0) {
      TopologyLeafSupported = TRUE;

      //
      // Sub-leaf index 0 (ECX= 0 as input) provides enumeration parameters to extract
      // the SMT sub-field of x2APIC ID.
      //
      LevelType = ExtendedTopologyEcx.Bits.LevelType;
      ASSERT (LevelType == CPUID_EXTENDED_TOPOLOGY_LEVEL_TYPE_SMT);
      ThreadBits = ExtendedTopologyEax.Bits.ApicIdShift;

      //
      // Software must not assume any "level type" encoding
      // value to be related to any sub-leaf index, except sub-leaf 0.
      //
      SubIndex = 1;
      do {
        AsmCpuidEx (
          CPUID_EXTENDED_TOPOLOGY,
          SubIndex,
          &ExtendedTopologyEax.Uint32,
          NULL,
          &ExtendedTopologyEcx.Uint32,
          NULL
          );
        LevelType = ExtendedTopologyEcx.Bits.LevelType;
        if (LevelType == CPUID_EXTENDED_TOPOLOGY_LEVEL_TYPE_CORE) {
          CoreBits = ExtendedTopologyEax.Bits.ApicIdShift - ThreadBits;
          break;
        }

        SubIndex++;
      } while (LevelType != CPUID_EXTENDED_TOPOLOGY_LEVEL_TYPE_INVALID);
    }
  }

  if (!TopologyLeafSupported) {
    //
    // Get logical processor count
    //
    AsmCpuid (CPUID_VERSION_INFO, NULL, &VersionInfoEbx.Uint32, NULL, NULL);
    MaxLogicProcessorsPerPackage = VersionInfoEbx.Bits.MaximumAddressableIdsForLogicalProcessors;

    //
    // Assume single-core processor
    //
    MaxCoresPerPackage = 1;

    //
    // Check for topology extensions on AMD processor
    //
    {
      //
      // Extract core count based on CACHE information
      //
      if (MaxStandardCpuIdIndex >= CPUID_CACHE_PARAMS) {
        AsmCpuidEx (CPUID_CACHE_PARAMS, 0, &CacheParamsEax.Uint32, NULL, NULL, NULL);
        if (CacheParamsEax.Uint32 != 0) {
          MaxCoresPerPackage = CacheParamsEax.Bits.MaximumAddressableIdsForLogicalProcessors + 1;
        }
      }
    }

    ThreadBits = (UINTN)(HighBitSet32 (MaxLogicProcessorsPerPackage / MaxCoresPerPackage - 1) + 1);
    CoreBits   = (UINTN)(HighBitSet32 (MaxCoresPerPackage - 1) + 1);
  }

  if (Thread != NULL) {
    *Thread = InitialApicId & ((1 << ThreadBits) - 1);
  }

  if (Core != NULL) {
    *Core = (InitialApicId >> ThreadBits) & ((1 << CoreBits) - 1);
  }

  if (Package != NULL) {
    *Package = (InitialApicId >> (ThreadBits + CoreBits));
  }
}

/**
  The Entry point of the MP CPU PEIM

  This function is the Entry point of the MP CPU PEIM which will install the MpServicePpi

  @param  FileHandle  Handle of the file being invoked.
  @param  PeiServices Describes the list of possible PEI Services.

  @retval EFI_SUCCESS   MpServicePpi is installed successfully.

**/
EFI_STATUS
EFIAPI
BuildSmmMpInfoHob (
  IN   EFI_PEI_HOB_POINTERS  Hob
  )
{
  SYS_CPU_TASK_HOB             *SysCpuTaskHob;
  SYS_CPU_INFO                 *SysCpuInfo;
  SYS_CPU_TASK                 *SysCpuTask;
  MY_MP_INFORMATION_HOB_DATA    MpInformationData;
  UINTN                         Index;
  EFI_PROCESSOR_INFORMATION    *ProcessorInfo;
  UINT32                        Package;
  UINT32                        Core;
  UINT32                        Thread;

  ZeroMem (&MpInformationData, sizeof(MpInformationData));

  SysCpuTaskHob = (SYS_CPU_TASK_HOB *) GET_GUID_HOB_DATA (Hob.Raw);
  ASSERT (SysCpuTaskHob != NULL);

  SysCpuTask = (SYS_CPU_TASK *) SysCpuTaskHob->SysCpuTask;
  SysCpuInfo = (SYS_CPU_INFO *) SysCpuTaskHob->SysCpuInfo;

  MpInformationData.NumberOfProcessors = SysCpuInfo->CpuCount;
  MpInformationData.NumberOfEnabledProcessors = SysCpuInfo->CpuCount;
  for (Index = 0; Index < SysCpuInfo->CpuCount; Index++) {
    DEBUG((EFI_D_ERROR, "Processor %2x: APICID %08x\n", Index, SysCpuInfo->CpuInfo[Index].ApicId));
    ProcessorInfo = &MpInformationData.ProcessorInfoBuffer[Index];
    ProcessorInfo->ProcessorId = SysCpuInfo->CpuInfo[Index].ApicId;
    ProcessorInfo->StatusFlag  = 3;
    if (Index == 0) {
      ProcessorInfo->StatusFlag |= 4;
    }

    GetCpuLocationByApicId (SysCpuInfo->CpuInfo[Index].ApicId, &Package, &Core, &Thread);
    ProcessorInfo->Location.Package = Package;
    ProcessorInfo->Location.Core    = Core;
    ProcessorInfo->Location.Thread  = Thread;
  }

  BuildGuidDataHob (
    &gMpInformationHobGuid,
    (VOID *)&MpInformationData,
    sizeof(MpInformationData)
    );

  return EFI_SUCCESS;
}



/**
  Performs platform specific initialization required for the CPU to access
  the hardware associated with a SerialPortLib instance.  This function does
  not initialize the serial port hardware itself.  Instead, it initializes
  hardware devices that are required for the CPU to access the serial port
  hardware.  This function may be called more than once.

  @retval RETURN_SUCCESS       The platform specific initialization succeeded.
  @retval RETURN_DEVICE_ERROR  The platform specific initialization could not be completed.

**/
STATIC
RETURN_STATUS
EFIAPI
PlatformHookSerialPortInitialize (
  IN CONST VOID      *HobStart
  )
{
  RETURN_STATUS                       Status;
  UNIVERSAL_PAYLOAD_SERIAL_PORT_INFO  *SerialPortInfo;
  UINT8                               *GuidHob;
  UNIVERSAL_PAYLOAD_GENERIC_HEADER    *GenericHeader;

  GuidHob = GetNextGuidHob (&gUniversalPayloadSerialPortInfoGuid, HobStart);
  if (GuidHob == NULL) {
    return EFI_NOT_FOUND;
  }

  GenericHeader = (UNIVERSAL_PAYLOAD_GENERIC_HEADER *)GET_GUID_HOB_DATA (GuidHob);
  if ((sizeof (UNIVERSAL_PAYLOAD_GENERIC_HEADER) > GET_GUID_HOB_DATA_SIZE (GuidHob)) || (GenericHeader->Length > GET_GUID_HOB_DATA_SIZE (GuidHob))) {
    return EFI_NOT_FOUND;
  }

  if (GenericHeader->Revision == UNIVERSAL_PAYLOAD_SERIAL_PORT_INFO_REVISION) {
    SerialPortInfo = (UNIVERSAL_PAYLOAD_SERIAL_PORT_INFO *)GET_GUID_HOB_DATA (GuidHob);
    if (GenericHeader->Length < UNIVERSAL_PAYLOAD_SIZEOF_THROUGH_FIELD (UNIVERSAL_PAYLOAD_SERIAL_PORT_INFO, RegisterBase)) {
      //
      // Return if can't find the Serial Port Info Hob with enough length
      //
      return EFI_NOT_FOUND;
    }

    Status = PcdSetBoolS (PcdSerialUseMmio, SerialPortInfo->UseMmio);
    if (RETURN_ERROR (Status)) {
      return Status;
    }

    Status = PcdSet64S (PcdSerialRegisterBase, SerialPortInfo->RegisterBase);
    if (RETURN_ERROR (Status)) {
      return Status;
    }

    Status = PcdSet32S (PcdSerialRegisterStride, SerialPortInfo->RegisterStride);
    if (RETURN_ERROR (Status)) {
      return Status;
    }

    Status = PcdSet32S (PcdSerialBaudRate, SerialPortInfo->BaudRate);
    if (RETURN_ERROR (Status)) {
      return Status;
    }

    return RETURN_SUCCESS;
  }

  return EFI_NOT_FOUND;
}


/**
  Entry point to the C language phase of UEFI payload.

  @param[in]   BootloaderParameter    The starting address of bootloader parameter block.

  @retval      It will not return if SUCCESS, and return error when passing bootloader parameter.
**/
EFI_STATUS
EFIAPI
_ModuleEntryPoint (
  IN UINTN  BootloaderParameter
  )
{
  EFI_PEI_HOB_POINTERS          Hob;
  UINTN                         MinimalNeededSize;
  EFI_PHYSICAL_ADDRESS          FreeMemoryBottom;
  EFI_PHYSICAL_ADDRESS          FreeMemoryTop;
  EFI_PHYSICAL_ADDRESS          MemoryBottom;
  EFI_PHYSICAL_ADDRESS          MemoryTop;
  EFI_HOB_RESOURCE_DESCRIPTOR   *PhitResourceHob;
  EFI_HOB_RESOURCE_DESCRIPTOR   *ResourceHob;
  BOOLEAN                       Required;
  UNIVERSAL_PAYLOAD_ACPI_TABLE  *TableInfo;

  Hob.Raw           = (UINT8 *)BootloaderParameter;

  PlatformHookSerialPortInitialize (Hob.Raw);

  // Call constructor for all libraries
  ProcessLibraryConstructorList ();

  DEBUG ((DEBUG_INFO, "Entering SMM Payload...\n"));


  MinimalNeededSize = SIZE_256KB;

  ASSERT (Hob.Raw != NULL);
  ASSERT ((UINTN)Hob.HandoffInformationTable->EfiFreeMemoryTop == Hob.HandoffInformationTable->EfiFreeMemoryTop);
  ASSERT ((UINTN)Hob.HandoffInformationTable->EfiMemoryTop == Hob.HandoffInformationTable->EfiMemoryTop);
  ASSERT ((UINTN)Hob.HandoffInformationTable->EfiFreeMemoryBottom == Hob.HandoffInformationTable->EfiFreeMemoryBottom);
  ASSERT ((UINTN)Hob.HandoffInformationTable->EfiMemoryBottom == Hob.HandoffInformationTable->EfiMemoryBottom);

  //
  // Try to find Resource Descriptor HOB that contains Hob range EfiMemoryBottom..EfiMemoryTop
  //
  PhitResourceHob = FindResourceDescriptorByRange (Hob.Raw, Hob.HandoffInformationTable->EfiMemoryBottom, Hob.HandoffInformationTable->EfiMemoryTop);
  if (PhitResourceHob == NULL) {
    //
    // Boot loader's Phit Hob is not in an available Resource Descriptor, find another Resource Descriptor for new Phit Hob
    //
    ResourceHob = FindAnotherHighestBelow4GResourceDescriptor (Hob.Raw, MinimalNeededSize, NULL);
    if (ResourceHob == NULL) {
      return EFI_NOT_FOUND;
    }

    MemoryBottom     = ResourceHob->PhysicalStart + ResourceHob->ResourceLength - MinimalNeededSize;
    FreeMemoryBottom = MemoryBottom;
    FreeMemoryTop    = ResourceHob->PhysicalStart + ResourceHob->ResourceLength;
    MemoryTop        = FreeMemoryTop;
  } else if (PhitResourceHob->PhysicalStart + PhitResourceHob->ResourceLength - Hob.HandoffInformationTable->EfiMemoryTop >= MinimalNeededSize) {
    //
    // New availiable Memory range in new hob is right above memory top in old hob.
    //
    MemoryBottom     = Hob.HandoffInformationTable->EfiFreeMemoryTop;
    FreeMemoryBottom = Hob.HandoffInformationTable->EfiMemoryTop;
    FreeMemoryTop    = FreeMemoryBottom + MinimalNeededSize;
    MemoryTop        = FreeMemoryTop;
  } else if (Hob.HandoffInformationTable->EfiMemoryBottom - PhitResourceHob->PhysicalStart >= MinimalNeededSize) {
    //
    // New availiable Memory range in new hob is right below memory bottom in old hob.
    //
    MemoryBottom     = Hob.HandoffInformationTable->EfiMemoryBottom - MinimalNeededSize;
    FreeMemoryBottom = MemoryBottom;
    FreeMemoryTop    = Hob.HandoffInformationTable->EfiMemoryBottom;
    MemoryTop        = Hob.HandoffInformationTable->EfiMemoryTop;
  } else {
    //
    // In the Resource Descriptor HOB contains boot loader Hob, there is no enough free memory size for payload hob
    // Find another Resource Descriptor Hob
    //
    ResourceHob = FindAnotherHighestBelow4GResourceDescriptor (Hob.Raw, MinimalNeededSize, PhitResourceHob);
    if (ResourceHob == NULL) {
      return EFI_NOT_FOUND;
    }

    MemoryBottom     = ResourceHob->PhysicalStart + ResourceHob->ResourceLength - MinimalNeededSize;
    FreeMemoryBottom = MemoryBottom;
    FreeMemoryTop    = ResourceHob->PhysicalStart + ResourceHob->ResourceLength;
    MemoryTop        = FreeMemoryTop;
  }

  HobConstructor ((VOID *)(UINTN)MemoryBottom, (VOID *)(UINTN)MemoryTop, (VOID *)(UINTN)FreeMemoryBottom, (VOID *)(UINTN)FreeMemoryTop);

  //
  // Since payload created new Hob, move all hobs except PHIT from boot loader hob list.
  //
  while (!END_OF_HOB_LIST (Hob)) {
    Required = FALSE;
    if (Hob.Header->HobType == EFI_HOB_TYPE_GUID_EXTENSION) {
      if (CompareGuid (&Hob.Guid->Name, &gUniversalPayloadSerialPortInfoGuid)) {
        Required = TRUE;
      } else if (CompareGuid (&Hob.Guid->Name, &gEfiSmmSmramMemoryGuid)) {
        Required = TRUE;
      } else if (CompareGuid (&Hob.Guid->Name, &gSmmRegisterInfoGuid)) {
        Required = TRUE;
      } else if (CompareGuid (&Hob.Guid->Name, &gLoaderMpCpuTaskInfoGuid)) {
        BuildSmmMpInfoHob (Hob);
      } else if (CompareGuid (&Hob.Guid->Name, &gUniversalPayloadAcpiTableGuid)) {
        TableInfo = (UNIVERSAL_PAYLOAD_ACPI_TABLE *)GET_GUID_HOB_DATA (Hob);
        BuildHobFromAcpi (TableInfo->Rsdp);
      }
    }
    if (Required) {
      AddNewHob (&Hob);
    }
    Hob.Raw = GET_NEXT_HOB (Hob);
  }

  SmmIplEntry (NULL, NULL);

  DEBUG ((DEBUG_INFO, "Exiting SMM Payload...\n"));

  return EFI_SUCCESS;
}
