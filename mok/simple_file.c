/*
 * Copyright 2012 <James.Bottomley@HansenPartnership.com>
 *
 * see COPYING file
 */
 /*
  * Modified for RefindPlus
  * Copyright (c) 2024 Dayo Akanji (sf.net/u/dakanji/profile)
  *
  * Modifications distributed under the preceding terms.
  */

#include <global.h>
#include "../include/refit_call_wrapper.h"

#include "simple_file.h"
//#include "execute.h"    /* for generate_path() */

static EFI_GUID IMAGE_PROTOCOL     = LOADED_IMAGE_PROTOCOL;
static EFI_GUID SIMPLE_FS_PROTOCOL = SIMPLE_FILE_SYSTEM_PROTOCOL;
static EFI_GUID FILE_INFO          = EFI_FILE_INFO_ID;

EFI_STATUS simple_file_open_by_handle (
    EFI_HANDLE          Device,
    CHAR16             *NameStr,
    EFI_FILE_PROTOCOL **file,
    UINT64              mode
) {
   EFI_STATUS                       Status;
   EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *drive;
   EFI_FILE_PROTOCOL               *root;

   Status = uefi_call_wrapper(gBS->HandleProtocol, 3, Device,
                   &SIMPLE_FS_PROTOCOL, (VOID **) &drive);

   if (Status != EFI_SUCCESS) {
      Print (L"Unable to find simple file protocol\n");
      goto error;
   }

   Status = uefi_call_wrapper(drive->OpenVolume, 2, drive, &root);

   if (Status != EFI_SUCCESS) {
      Print (L"Failed to open drive volume\n");
      goto error;
   }

   Status = uefi_call_wrapper(root->Open, 5, root, file, NameStr,
                   mode, 0);

 error:
   return Status;
}

// generate_path() from shim by Matthew J. Garrett
static
EFI_STATUS generate_path (
    CHAR16                     *NameStr,
    EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage,
    EFI_DEVICE_PATH_PROTOCOL  **path,
    CHAR16                    **PathName
) {
    INTN        i;
    UINTN       PathLen;
    UINTN       DestSize;
    CHAR16     *FoundStr;
    CHAR16     *DevPathStr;

    FoundStr = NULL;
    DevPathStr = DevicePathToStr (LoadedImage->FilePath);
    for (i = 0; i < StrLen (DevPathStr); i++) {
        if (DevPathStr[i] == '/') {
            DevPathStr[i] = '\\';
        }
        if (DevPathStr[i] == '\\') {
            FoundStr = &DevPathStr[i];
        }
    } // for

    if (FoundStr == NULL) {
        PathLen = 0;
    }
    else {
        while (*(FoundStr - 1) == '\\') {
            --FoundStr;
        }

        *FoundStr = '\0';
        PathLen = StrLen (DevPathStr);
    }

    if (NameStr[0] != '\\') {
        PathLen++;
    }

    DestSize  = StrLen (NameStr) + PathLen + 1;
    *PathName = AllocatePool (sizeof (CHAR16) * DestSize);
    if (*PathName == NULL) {
        Print (L"Failed to Allocate Path Buffer\n");
        FreePool (DevPathStr);

        return EFI_OUT_OF_RESOURCES;
    }

    StrCpyS (*PathName, DestSize, DevPathStr);

    if (NameStr[0] != '\\') {
        StrCat (*PathName, L"\\");
    }
    StrCat (*PathName, NameStr);

    *path = FileDevicePath (LoadedImage->DeviceHandle, *PathName);
    FreePool (DevPathStr);

    return EFI_SUCCESS;
} // generate_path()

EFI_STATUS simple_file_open (
    EFI_HANDLE          image,
    CHAR16             *NameStr,
    EFI_FILE_PROTOCOL **file,
    UINT64              mode
) {
   EFI_STATUS                 Status;
   EFI_HANDLE                 Device;
   EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
   EFI_DEVICE_PATH_PROTOCOL  *LoadPath;
   CHAR16                    *PathName;

   Status = uefi_call_wrapper(
       gBS->HandleProtocol, 3, image,
       &IMAGE_PROTOCOL, (VOID **) &LoadedImage
   );

   if (Status != EFI_SUCCESS) {
       return simple_file_open_by_handle (image, NameStr, file, mode);
   }

   PathName = NULL;
   LoadPath = NULL;
   Status = generate_path (NameStr, LoadedImage, &LoadPath, &PathName);

   if (Status != EFI_SUCCESS) {
      Print (L"Unable to generate load path for %s\n", NameStr);
      return Status;
   }

   Device = LoadedImage->DeviceHandle;

   Status = simple_file_open_by_handle (Device, PathName, file, mode);

   FreePool (PathName);
   FreePool (LoadPath);

   return Status;
}

EFI_STATUS simple_file_read_all (
    EFI_FILE_PROTOCOL  *file,
    UINTN              *size,
    void              **buffer
) {
   EFI_STATUS Status;
   EFI_FILE_INFO *fi;
   char buf[1024];

   *size = sizeof (buf);
   fi = (void *)buf;


   Status = uefi_call_wrapper(
       file->GetInfo, 4, file, &FILE_INFO, size, fi
   );
   if (Status != EFI_SUCCESS) {
      Print(L"Failed to get file info\n");
      return Status;
   }

   *size = fi->FileSize;

   *buffer = AllocatePool (*size);
   if (*buffer == NULL) {
      Print(L"Failed to allocate buffer of size %d\n", *size);
      return EFI_OUT_OF_RESOURCES;
   }
   Status = uefi_call_wrapper(file->Read, 3, file, size, *buffer);

   return Status;
}

VOID simple_file_close (
    EFI_FILE_PROTOCOL *file
) {
   uefi_call_wrapper(file->Close, 1, file);
}
