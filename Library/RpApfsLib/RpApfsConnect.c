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
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include "RpApfsLib.h"
#include <Library/OcBootManagementLib.h>
#include <Library/OcConsoleLib.h>
#include <Library/OcDriverConnectionLib.h>
#include <Library/OcGuardLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/OcVariable.h>
#include "ApfsUnsupportedBds.h"
#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

#include "../../include/refit_call_wrapper.h"

extern BOOLEAN SilenceAPFS;

LIST_ENTRY               mApfsPrivateDataList = INITIALIZE_LIST_HEAD_VARIABLE (mApfsPrivateDataList);
static EFI_SYSTEM_TABLE  *mNullSystemTable;

static
EFI_STATUS
ApfsRegisterPartition (
    IN  EFI_HANDLE             Handle,
    IN  EFI_BLOCK_IO_PROTOCOL  *BlockIo,
    IN  APFS_NX_SUPERBLOCK     *SuperBlock,
    OUT APFS_PRIVATE_DATA      **PrivateDataPointer
) {
    EFI_STATUS           Status;
    APFS_PRIVATE_DATA    *PrivateData;

    PrivateData = AllocateZeroPool (sizeof (*PrivateData));
    if (PrivateData == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    // File private data fields.
    PrivateData->Signature = APFS_PRIVATE_DATA_SIGNATURE;
    PrivateData->LocationInfo.ControllerHandle = Handle;
    CopyGuid (&PrivateData->LocationInfo.ContainerUuid, &SuperBlock->Uuid);

    PrivateData->BlockIo = BlockIo;
    PrivateData->ApfsBlockSize = SuperBlock->BlockSize;
    PrivateData->LbaMultiplier = PrivateData->ApfsBlockSize / PrivateData->BlockIo->Media->BlockSize;
    PrivateData->EfiJumpStart  = SuperBlock->EfiJumpStart;
    InternalApfsInitFusionData (SuperBlock, PrivateData);

    // Install boot record information.
    // Guaranties we never register twice.
    Status = refit_call4_wrapper(
        gBS->InstallMultipleProtocolInterfaces,
        &PrivateData->LocationInfo.ControllerHandle,
        &gApfsEfiBootRecordInfoProtocolGuid,
        &PrivateData->LocationInfo,
        NULL
    );

    if (EFI_ERROR (Status)) {
        FreePool (PrivateData);
        return Status;
    }

    // Save into the list and return.
    InsertTailList (&mApfsPrivateDataList, &PrivateData->Link);
    *PrivateDataPointer = PrivateData;

    return EFI_SUCCESS;
}

static
EFI_STATUS
ApfsStartDriver (
    IN APFS_PRIVATE_DATA  *PrivateData,
    IN VOID               *DriverBuffer,
    IN UINTN              DriverSize
) {
    EFI_STATUS                 Status;
    EFI_DEVICE_PATH_PROTOCOL   *DevicePath;
    EFI_HANDLE                 ImageHandle;
    EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage;

    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        PrivateData->LocationInfo.ControllerHandle,
        &gEfiDevicePathProtocolGuid,
        (VOID **) &DevicePath
    );

    if (EFI_ERROR (Status)) {
        DevicePath = NULL;
    }

    ImageHandle = NULL;
    Status = refit_call6_wrapper(
        gBS->LoadImage,
        FALSE,
        gImageHandle,
        DevicePath,
        DriverBuffer,
        DriverSize,
        &ImageHandle
    );

    if (EFI_ERROR (Status)) {
        return Status;
    }

    // Disable APFS verbose mode.
    if (SilenceAPFS) {
        Status = refit_call3_wrapper(
            gBS->HandleProtocol,
            ImageHandle,
            &gEfiLoadedImageProtocolGuid,
            (VOID *) &LoadedImage
        );

        if (!EFI_ERROR (Status)) {
            if (mNullSystemTable == NULL) {
                mNullSystemTable = AllocateNullTextOutSystemTable (gST);
            }
            if (mNullSystemTable != NULL) {
                LoadedImage->SystemTable = mNullSystemTable;
            }
        }
    }

    Status = refit_call3_wrapper(
        gBS->StartImage,
        ImageHandle,
        NULL,
        NULL
    );

    if (EFI_ERROR (Status)) {
        gBS->UnloadImage (ImageHandle);

        return Status;
    }

    // Unblock handles as some types of firmware, such as that on the HP EliteBook 840 G2,
    // may automatically lock all volumes without filesystem drivers upon
    // any attempt to connect them.
    // REF: https://github.com/acidanthera/bugtracker/issues/1128
    OcDisconnectDriversOnHandle (PrivateData->LocationInfo.ControllerHandle);

    // Recursively connect controller to get apfs.efi loaded.
    // We cannot use apfs.efi handle as it apparently creates new handles.
    // This follows ApfsJumpStart driver implementation.
    refit_call4_wrapper(
        gBS->ConnectController,
        PrivateData->LocationInfo.ControllerHandle,
        NULL,
        NULL,
        TRUE
    );

    return EFI_SUCCESS;
}

static
EFI_STATUS
ApfsConnectDevice (
    IN EFI_HANDLE             Handle,
    IN EFI_BLOCK_IO_PROTOCOL  *BlockIo
) {
    EFI_STATUS           Status;
    APFS_NX_SUPERBLOCK   *SuperBlock;
    APFS_PRIVATE_DATA    *PrivateData;
    VOID                 *DriverBuffer;
    UINTN                DriverSize;

    // This may yet not be APFS but some other file system ... verify
    Status = InternalApfsReadSuperBlock (BlockIo, &SuperBlock);
    if (EFI_ERROR (Status)) {
        return Status;
    }

    // We no longer need super block once we register ourselves.
    Status = ApfsRegisterPartition (Handle, BlockIo, SuperBlock, &PrivateData);
    FreePool (SuperBlock);
    if (EFI_ERROR (Status)) {
        return Status;
    }

    // We cannot load drivers if we have no fusion drive pair as they are not
    // guarantied to be located on each drive.
    if (!PrivateData->CanLoadDriver) {
        return EFI_NOT_READY;
    }

    Status = InternalApfsReadDriver (PrivateData, &DriverSize, &DriverBuffer);
    if (EFI_ERROR (Status)) {
        return Status;
    }

    Status = ApfsStartDriver (PrivateData, DriverBuffer, DriverSize);
    FreePool (DriverBuffer);

    return Status;
}

EFI_STATUS
RpApfsConnectHandle (
    IN EFI_HANDLE  Handle
) {
    EFI_STATUS             Status;
    VOID                   *TempProtocol;
    EFI_BLOCK_IO_PROTOCOL  *BlockIo;

    // We have a filesystem driver afer successful APFS or other connection.
    // Skip if the device is already connected.
    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        Handle,
        &gEfiSimpleFileSystemProtocolGuid,
        &TempProtocol
    );

    if (!EFI_ERROR (Status)) {
        return EFI_ALREADY_STARTED;
    }

    // Obtain Block I/O.
    // Ignore the 2nd revision as apfs.efi does not use it.
    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        Handle,
        &gEfiBlockIoProtocolGuid,
        (VOID **) &BlockIo
    );

    if (EFI_ERROR (Status)) {
        return EFI_UNSUPPORTED;
    }

    // Filter out handles:
    // - Without media.
    // - Which are not partitions (APFS containers).
    // - Which have non-POT block size.
    if (BlockIo->Media == NULL || !BlockIo->Media->LogicalPartition) {
        return EFI_UNSUPPORTED;
    }

    if (BlockIo->Media->BlockSize == 0 ||
        (BlockIo->Media->BlockSize & (BlockIo->Media->BlockSize - 1)) != 0
    ) {
        return EFI_UNSUPPORTED;
    }

    // If the handle is marked as unsupported, we should respect this.
    // TODO: Install this protocol on failure (not in ApfsJumpStart)?
    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        Handle,
        &gApfsUnsupportedBdsProtocolGuid,
        &TempProtocol
    );

    if (!EFI_ERROR (Status)) {
        return EFI_UNSUPPORTED;
    }

    // If we installed APFS EFI boot record, then this handle is already
    // handled, though potentially not connected.
    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        Handle,
        &gApfsEfiBootRecordInfoProtocolGuid,
        &TempProtocol
    );

    if (!EFI_ERROR (Status)) {
        return EFI_UNSUPPORTED;
    }

    // This is possibly APFS, try connecting.
    return ApfsConnectDevice (Handle, BlockIo);
}
