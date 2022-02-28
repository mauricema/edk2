/** @file
  Software SMI handler implementation for bootloader.

  Copyright (c) 2011 - 2020, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/HobLib.h>
#include <Library/BaseMemoryLib.h>

#include <Protocol/SmmCpu.h>
#include <Library/MmServicesTableLib.h>
#include <Guid/AcpiBoardInfoGuid.h>
#include <Guid/SmmRegisterInfoGuid.h>

typedef struct {
  UINT8     EosBitOffset;
  UINT8     ApmBitOffset;
  UINT32    SmiEosAddr;
  UINT32    SmiApmStsAddr;
} SMM_PCH_REGISTER;

SMM_PCH_REGISTER       mSmiPchReg;

EFI_SMM_CPU_PROTOCOL   *mSmmCpuProtocol;

UINT8                  mSmiTriggerRegister = 0xB2;
UINT8                  mSmiDataRegister    = 0xB3;
UINT32                 mAcpiBase;

/**
  Software SMI callback for restoring SMRR base and mask in S3 path.

  @param[in]      DispatchHandle  The unique handle assigned to this handler by SmiHandlerRegister().
  @param[in]      Context         Points to an optional handler context which was specified when the
                                  handler was registered.
  @param[in, out] CommBuffer      A pointer to a collection of data in memory that will
                                  be conveyed from a non-SMM environment into an SMM environment.
  @param[in, out] CommBufferSize  The size of the CommBuffer.

  @retval EFI_SUCCESS             The interrupt was handled successfully.

**/
EFI_STATUS
EFIAPI
StandaloneMmSwSmiHandler (
  IN EFI_HANDLE                  DispatchHandle,
  IN CONST VOID                  *Context,
  IN OUT VOID                    *CommBuffer,
  IN OUT UINTN                   *CommBufferSize
  )
{

  EFI_STATUS                      Status;
  UINTN                           Index;
  EFI_SMM_SAVE_STATE_IO_INFO      IoInfo;


  //
  // Clear SMI APM status
  //
  DEBUG ((DEBUG_INFO, "Sw SMI\n"));

  if (IoRead32 (mSmiPchReg.SmiApmStsAddr) & (1 << mSmiPchReg.ApmBitOffset)) {

    DEBUG ((DEBUG_INFO, "Sw SMI Root Handler\n"));

    //
    // Try to find which CPU trigger SWSMI
    //
    for (Index = 0; Index < gMmst->NumberOfCpus; Index++) {
      Status = mSmmCpuProtocol->ReadSaveState (
                                  mSmmCpuProtocol,
                                  sizeof(IoInfo),
                                  EFI_SMM_SAVE_STATE_REGISTER_IO,
                                  Index,
                                  &IoInfo
                                  );
      if (EFI_ERROR (Status)) {
        continue;
      }
      if (IoInfo.IoPort == mSmiTriggerRegister) {
        //
        // Great! Find it.
        //
        DEBUG ((DEBUG_INFO, "CPU index = 0x%x/0x%x\n", Index, gMmst->NumberOfCpus));
        DEBUG ((DEBUG_INFO, "SW SMI Data %x\n", IoRead8 (mSmiTriggerRegister)));
        IoWrite8 (mSmiTriggerRegister, IoRead8 (mSmiTriggerRegister) + 1);
        break;
      }
    }

    //
    // Clear SMI APM status
    //
    IoOr32 (mSmiPchReg.SmiApmStsAddr, 1 << mSmiPchReg.ApmBitOffset);
  }

  //
  // Set EOS bit
  //
  IoOr32 (mSmiPchReg.SmiEosAddr, 1 << mSmiPchReg.EosBitOffset);

  return EFI_SUCCESS;
}


/**
  Get specified SMI register based on given register ID

  @param[in]  SmmRegister  SMI related register array from bootloader
  @param[in]  Id           The register ID to get.

  @retval NULL             The register is not found or the format is not expected.
  @return smi register

**/
PLD_GENERIC_REGISTER *
GetSmmCtrlRegById (
  IN PLD_SMM_REGISTERS  *SmmRegister,
  IN UINT32             Id
  )
{
  UINT32                Index;
  PLD_GENERIC_REGISTER  *PldReg;

  PldReg = NULL;
  for (Index = 0; Index < SmmRegister->Count; Index++) {
    if (SmmRegister->Registers[Index].Id == Id) {
      PldReg = &SmmRegister->Registers[Index];
      break;
    }
  }

  if (PldReg == NULL) {
    DEBUG ((DEBUG_INFO, "Register %d not found.\n", Id));
    return NULL;
  }

  //
  // Checking the register if it is expected.
  //
  if ((PldReg->Address.AccessSize       != EFI_ACPI_3_0_DWORD) ||
      (PldReg->Address.Address          == 0) ||
      (PldReg->Address.RegisterBitWidth != 1) ||
      (PldReg->Address.AddressSpaceId   != EFI_ACPI_3_0_SYSTEM_IO) ||
      (PldReg->Value != 1))
  {
    DEBUG ((DEBUG_INFO, "Unexpected SMM register.\n"));
    DEBUG ((DEBUG_INFO, "AddressSpaceId= 0x%x\n", PldReg->Address.AddressSpaceId));
    DEBUG ((DEBUG_INFO, "RegBitWidth   = 0x%x\n", PldReg->Address.RegisterBitWidth));
    DEBUG ((DEBUG_INFO, "RegBitOffset  = 0x%x\n", PldReg->Address.RegisterBitOffset));
    DEBUG ((DEBUG_INFO, "AccessSize    = 0x%x\n", PldReg->Address.AccessSize));
    DEBUG ((DEBUG_INFO, "Address       = 0x%lx\n", PldReg->Address.Address));
    return NULL;
  }

  return PldReg;
}


/**
  The driver's entry point.

  It install callbacks for bootloader sw smi

  @param[in] ImageHandle  The firmware allocated handle for the EFI image.
  @param[in] SystemTable  A pointer to the EFI System Table.

  @retval EFI_SUCCESS     The entry point is executed successfully.
  @retval Others          Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
InitializeStandaloneMmSwSmiHandler (
  IN EFI_HANDLE             ImageHandle,
  IN EFI_MM_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS                     Status;
  EFI_HANDLE                     DispatchHandle;

  EFI_HOB_GUID_TYPE              *GuidHob;
  ACPI_BOARD_INFO                *AcpiBoardInfo;
  PLD_SMM_REGISTERS              *SmmRegister;
  PLD_GENERIC_REGISTER           *SmiEosReg;
  PLD_GENERIC_REGISTER           *SmiApmStsReg;
  UINT32                          SmiEn;

  //
  // Find the acpi board information guid hob
  //
  GuidHob = GetFirstGuidHob (&gUefiAcpiBoardInfoGuid);
  ASSERT (GuidHob != NULL);

  AcpiBoardInfo = (ACPI_BOARD_INFO *)GET_GUID_HOB_DATA (GuidHob);

  mAcpiBase = (UINT32)AcpiBoardInfo->PmCtrlRegBase;

  //
  // Find SMM info
  //
  GuidHob = GetFirstGuidHob (&gSmmRegisterInfoGuid);
  if (GuidHob == NULL) {
    DEBUG ((DEBUG_ERROR, "SMM HOB not found.\n"));
    return EFI_UNSUPPORTED;
  }

  SmmRegister = (PLD_SMM_REGISTERS *)GET_GUID_HOB_DATA (GuidHob);
  SmiEosReg   = GetSmmCtrlRegById (SmmRegister, REGISTER_ID_SMI_EOS);
  if (SmiEosReg == NULL) {
    DEBUG ((DEBUG_ERROR, "SMI EOS reg not found.\n"));
    return EFI_NOT_FOUND;
  }

  mSmiPchReg.SmiEosAddr   = (UINT32)SmiEosReg->Address.Address;
  mSmiPchReg.EosBitOffset = SmiEosReg->Address.RegisterBitOffset;

  SmiApmStsReg = GetSmmCtrlRegById (SmmRegister, REGISTER_ID_SMI_APM_STS);
  if (SmiApmStsReg == NULL) {
    DEBUG ((DEBUG_ERROR, "SMI APM status reg not found.\n"));
    return EFI_NOT_FOUND;
  }

  mSmiPchReg.SmiApmStsAddr = (UINT32)SmiApmStsReg->Address.Address;
  mSmiPchReg.ApmBitOffset  = SmiApmStsReg->Address.RegisterBitOffset;

  //
  // Locate PI SMM CPU protocol
  //
  Status = SystemTable->MmLocateProtocol (
                    &gEfiSmmCpuProtocolGuid,
                    NULL,
                    (VOID **)&mSmmCpuProtocol
                    );
  ASSERT_EFI_ERROR (Status);


  // register a root SW SMI handler
  Status = SystemTable->MmiHandlerRegister (
                    StandaloneMmSwSmiHandler,
                    NULL,
                    &DispatchHandle
                    );

  //
  // Set SMI_EN bits
  //
  SmiEn = (1 << mSmiPchReg.EosBitOffset) | (1 << mSmiPchReg.ApmBitOffset) | BIT0;
  IoOr32 (mSmiPchReg.SmiEosAddr, SmiEn);

  return Status;

}

