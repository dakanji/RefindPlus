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

#include "RpApfsInternal.h"
#include "RpApfsLib.h"
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/PartitionInfo.h>

#include "../../include/refit_call_wrapper.h"

EFI_STATUS
RpApfsConnectParentDevice (
    VOID
) {
  EFI_STATUS       Status;
  EFI_STATUS       XStatus;
  UINTN            HandleCount;
  EFI_HANDLE       *HandleBuffer;
  EFI_DEVICE_PATH  *ParentDevicePath = NULL;
  EFI_DEVICE_PATH  *ChildDevicePath;
  UINTN            Index;
  UINTN            PrefixLength = 0;

  HandleCount = 0;
  Status = refit_call5_wrapper(
      gBS->LocateHandleBuffer,
      ByProtocol,
      &gEfiBlockIoProtocolGuid,
      NULL,
      &HandleCount,
      &HandleBuffer
  );

  if (!EFI_ERROR (Status)) {
    Status = EFI_NOT_FOUND;

    for (Index = 0; Index < HandleCount; ++Index) {
      if (ParentDevicePath != NULL && PrefixLength > 0) {
        XStatus = gBS->HandleProtocol (
          HandleBuffer[Index],
          &gEfiDevicePathProtocolGuid,
          (VOID **) &ChildDevicePath
          );
        if (EFI_ERROR (XStatus)) {
          DEBUG ((DEBUG_INFO, "No child device path - %r\n", XStatus));
          continue;
        }

        if (CompareMem (ParentDevicePath, ChildDevicePath, PrefixLength) == 0) {
          DEBUG ((DEBUG_INFO, "Matched device path\n"));
        } else {
          continue;
        }
      }

      XStatus = RpApfsConnectHandle (HandleBuffer[Index]);
      if (!EFI_ERROR (XStatus)) {
        Status = XStatus;
      }
    }

    FreePool (HandleBuffer);
  } else {
    DEBUG ((DEBUG_INFO, "BlockIo buffer error - %r\n", Status));
  }

  return Status;
}

EFI_STATUS
RpApfsConnectDevices (
    VOID
) {
    EFI_STATUS  Status;
    VOID        *PartitionInfoInterface;

    Status = gBS->LocateProtocol (
        &gEfiPartitionInfoProtocolGuid,
        NULL,
        &PartitionInfoInterface
    );
    Status = RpApfsConnectParentDevice ();

    return Status;
}
