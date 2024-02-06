/** @file
  Utility routines used by boot maintenance modules.

Copyright (c) 2004 - 2009, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/
/**
 * Modified for RefindPlus
 * Copyright (c) 2020-2024 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
**/

#ifdef __MAKEWITH_TIANO
#include "Platform.h"
#else
#include "gnuefi-helper.h"
#endif

#include "../BootMaster/rp_funcs.h"
#include "../include/refit_call_wrapper.h"

/**

  Find the first instance of this Protocol
  in the system and return its interface.


  @param ProtocolGuid    Provides the protocol to search for
  @param Interface       On return, a pointer to the first interface
                         that matches ProtocolGuid

  @retval  EFI_SUCCESS      A protocol instance matching ProtocolGuid was found
  @retval  EFI_NOT_FOUND    No protocol instances were found that match ProtocolGuid

**/
EFI_STATUS EfiLibLocateProtocol (
    IN  EFI_GUID  *ProtocolGuid,
    OUT VOID     **Interface
) {
    EFI_STATUS  Status;

    Status = REFIT_CALL_3_WRAPPER(
        gBS->LocateProtocol, ProtocolGuid,
        NULL, (VOID **) Interface
    );

    return Status;
} // EFI_STATUS EfiLibLocateProtocol()

/**

  Function opens and returns a file handle to the root directory of a volume.

  @param DeviceHandle    A handle for a device

  @return A valid file handle or NULL is returned

**/
EFI_FILE_HANDLE EfiLibOpenRoot (
    IN EFI_HANDLE DeviceHandle
) {
    EFI_STATUS                       Status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Volume;
    EFI_FILE_HANDLE                  File;

    File = NULL;

    // File the file system interface to the device
    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol, DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid, (VOID **) &Volume
    );
    if (!EFI_ERROR(Status)) {
        // Open the root directory of the volume
        Status = REFIT_CALL_2_WRAPPER(Volume->OpenVolume, Volume, &File);
        if (EFI_ERROR(Status)) {
            CheckError (Status, L"While Opening the Root Directory of the Volume");
        }
    }

    // Done
    return EFI_ERROR(Status) ? NULL : File;
} // EFI_FILE_HANDLE EfiLibOpenRoot()

/**
  Duplicate a string.

  @param Src             The source.

  @return A new string which is duplicated copy of the source.
  @retval NULL If there is not enough memory.

**/
CHAR16 * EfiStrDuplicate (
    IN CHAR16   *Src
) {
    CHAR16 *Dest;
    UINTN   Size;

    // Do not deference Null pointers
    if (Src == NULL) {
        return NULL;
    }

    Size = StrSize (Src); //at least 2bytes
    Dest = AllocateZeroPool (Size);
    if (Dest != NULL) {
        CopyMem (Dest, Src, Size);
    }

    return Dest;
} // CHAR16 * EfiStrDuplicate()

/**

  Function gets the file information from an open file descriptor, and stores it
  in a buffer allocated from pool.

  @param FHand           File Handle.

  @return                A pointer to a buffer with file information or NULL is returned

**/
EFI_FILE_INFO * EfiLibFileInfo (
    IN EFI_FILE_HANDLE      FHand
) {
    EFI_STATUS     Status;
    EFI_FILE_INFO *FileInfo = NULL;
    UINTN          Size     = 0;

    Status = FHand->GetInfo (FHand, &gEfiFileInfoGuid, &Size, FileInfo);
    if (Status == EFI_BUFFER_TOO_SMALL) {
        FileInfo = AllocateZeroPool (Size);
        Status = FHand->GetInfo (FHand, &gEfiFileInfoGuid, &Size, FileInfo);
    }

    return EFI_ERROR(Status) ? NULL : FileInfo;
} // EFI_FILE_INFO * EfiLibFileInfo()

EFI_FILE_SYSTEM_INFO * EfiLibFileSystemInfo (
    IN EFI_FILE_HANDLE      FHand
) {
    EFI_STATUS            Status;
    EFI_FILE_SYSTEM_INFO *FileSystemInfo = NULL;
    UINTN                 Size = 0;

    Status = FHand->GetInfo (FHand, &gEfiFileSystemInfoGuid, &Size, FileSystemInfo);
    if (Status == EFI_BUFFER_TOO_SMALL) {
        FileSystemInfo = AllocateZeroPool (Size);
        Status = FHand->GetInfo (FHand, &gEfiFileSystemInfoGuid, &Size, FileSystemInfo);
    }

    return EFI_ERROR(Status) ? NULL : FileSystemInfo;
} // EFI_FILE_SYSTEM_INFO * EfiLibFileSystemInfo()

/**
  Adjusts the size of a previously allocated buffer.

  @param OldPool         - A pointer to the buffer whose size is being adjusted.
  @param OldSize         - The size of the current buffer.
  @param NewSize         - The size of the new buffer.

  @return   The newly allocated buffer.
  @retval   NULL  Allocation failed.

**/
VOID * EfiReallocatePool (
    IN VOID  *OldPool,
    IN UINTN  OldSize,
    IN UINTN  NewSize
) {
    VOID  *NewPool;

    NewPool = NULL;
    if (NewSize != 0) {
        NewPool = AllocateZeroPool (NewSize);
    }

    if (OldPool != NULL) {
        if (NewPool != NULL) {
            CopyMem (NewPool, OldPool, OldSize < NewSize ? OldSize : NewSize);
        }

        MY_FREE_POOL(OldPool);
    }

    return NewPool;
} // VOID * EfiReallocatePool()
