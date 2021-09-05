/** @file
Copyright (C) 2020, vit9696. All rights reserved.

Modified 2021, Dayo Akanji. (sf.net/u/dakanji/profile)

  All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "RP_ApfsInternal.h"
#include "RP_ApfsLib.h"
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/PartitionInfo.h>

#include "../../include/refit_call_wrapper.h"

EFI_STATUS RP_ApfsConnectParentDevice (
    VOID
) {
  EFI_STATUS       Status;
  EFI_STATUS       XStatus;
  UINTN            HandleCount;
  EFI_HANDLE       *HandleBuffer;
  UINTN            Index;

  HandleCount = 0;
  Status = REFIT_CALL_5_WRAPPER(
      gBS->LocateHandleBuffer,
      ByProtocol,
      &gEfiBlockIoProtocolGuid,
      NULL,
      &HandleCount,
      &HandleBuffer
  );

  if (!EFI_ERROR(Status)) {
      Status = EFI_NOT_FOUND;

      for (Index = 0; Index < HandleCount; ++Index) {
          XStatus = RP_ApfsConnectHandle (HandleBuffer[Index]);
          if (XStatus == EFI_SUCCESS || XStatus == EFI_ALREADY_STARTED) {
              if (EFI_ERROR(Status)) {
                  Status = XStatus;
              }
          }
      }

      FreePool (HandleBuffer);
  }

  return Status;
}

EFI_STATUS RP_ApfsConnectDevices (
    VOID
) {
    EFI_STATUS  Status;
    VOID        *PartitionInfoInterface;


    REFIT_CALL_3_WRAPPER(
        gBS->LocateProtocol,
        &gEfiPartitionInfoProtocolGuid,
        NULL,
        &PartitionInfoInterface
    );
    Status = RP_ApfsConnectParentDevice();

    return Status;
}
