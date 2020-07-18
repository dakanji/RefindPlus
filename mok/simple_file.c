/*
 * Copyright 2012 <James.Bottomley@HansenPartnership.com>
 *
 * see COPYING file
 */

#include <global.h>
#include "../include/refit_call_wrapper.h"

#include "simple_file.h"
//#include "execute.h"    /* for generate_path() */

static EFI_GUID IMAGE_PROTOCOL = LOADED_IMAGE_PROTOCOL;
static EFI_GUID SIMPLE_FS_PROTOCOL = SIMPLE_FILE_SYSTEM_PROTOCOL;
static EFI_GUID FILE_INFO = EFI_FILE_INFO_ID;

EFI_STATUS
simple_file_open_by_handle(EFI_HANDLE device, CHAR16 *name, EFI_FILE **file, UINT64 mode)
{
   EFI_STATUS efi_status;
   EFI_FILE_IO_INTERFACE *drive;
   EFI_FILE *root;

   efi_status = uefi_call_wrapper(BS->HandleProtocol, 3, device,
                   &SIMPLE_FS_PROTOCOL, (VOID**) &drive);

   if (efi_status != EFI_SUCCESS) {
      Print(L"Unable to find simple file protocol\n");
      goto error;
   }

   efi_status = uefi_call_wrapper(drive->OpenVolume, 2, drive, &root);

   if (efi_status != EFI_SUCCESS) {
      Print(L"Failed to open drive volume\n");
      goto error;
   }

   efi_status = uefi_call_wrapper(root->Open, 5, root, file, name,
                   mode, 0);

 error:
   return efi_status;
}

// generate_path() from shim by Matthew J. Garrett
static
EFI_STATUS
generate_path(CHAR16* name, EFI_LOADED_IMAGE *li, EFI_DEVICE_PATH **path, CHAR16 **PathName)
{
        unsigned int pathlen;
        EFI_STATUS efi_status = EFI_SUCCESS;
        CHAR16 *devpathstr = DevicePathToStr(li->FilePath),
                *found = NULL;
        int i;

        for (i = 0; i < StrLen(devpathstr); i++) {
                if (devpathstr[i] == '/')
                        devpathstr[i] = '\\';
                if (devpathstr[i] == '\\')
                        found = &devpathstr[i];
        }
        if (!found) {
                pathlen = 0;
        } else {
                while (*(found - 1) == '\\')
                        --found;
                *found = '\0';
                pathlen = StrLen(devpathstr);
        }

        if (name[0] != '\\')
                pathlen++;

        *PathName = AllocatePool((pathlen + 1 + StrLen(name))*sizeof(CHAR16));

        if (!*PathName) {
                Print(L"Failed to allocate path buffer\n");
                efi_status = EFI_OUT_OF_RESOURCES;
                goto error;
        }

        StrCpy(*PathName, devpathstr);

        if (name[0] != '\\')
                StrCat(*PathName, L"\\");
        StrCat(*PathName, name);

        *path = FileDevicePath(li->DeviceHandle, *PathName);

error:
        FreePool(devpathstr);

        return efi_status;
} // generate_path()

EFI_STATUS
simple_file_open(EFI_HANDLE image, CHAR16 *name, EFI_FILE **file, UINT64 mode)
{
   EFI_STATUS efi_status;
   EFI_HANDLE device;
   EFI_LOADED_IMAGE *li;
   EFI_DEVICE_PATH *loadpath = NULL;
   CHAR16 *PathName = NULL;

   efi_status = uefi_call_wrapper(BS->HandleProtocol, 3, image,
                   &IMAGE_PROTOCOL, (VOID**) &li);

   if (efi_status != EFI_SUCCESS)
      return simple_file_open_by_handle(image, name, file, mode);

   efi_status = generate_path(name, li, &loadpath, &PathName);

   if (efi_status != EFI_SUCCESS) {
      Print(L"Unable to generate load path for %s\n", name);
      return efi_status;
   }

   device = li->DeviceHandle;

   efi_status = simple_file_open_by_handle(device, PathName, file, mode);

   FreePool(PathName);
   FreePool(loadpath);

   return efi_status;
}

EFI_STATUS
simple_file_read_all(EFI_FILE *file, UINTN *size, void **buffer)
{
   EFI_STATUS efi_status;
   EFI_FILE_INFO *fi;
   char buf[1024];

   *size = sizeof(buf);
   fi = (void *)buf;


   efi_status = uefi_call_wrapper(file->GetInfo, 4, file, &FILE_INFO,
                   size, fi);
   if (efi_status != EFI_SUCCESS) {
      Print(L"Failed to get file info\n");
      return efi_status;
   }

   *size = fi->FileSize;

   *buffer = AllocatePool(*size);
   if (!*buffer) {
      Print(L"Failed to allocate buffer of size %d\n", *size);
      return EFI_OUT_OF_RESOURCES;
   }
   efi_status = uefi_call_wrapper(file->Read, 3, file, size, *buffer);

   return efi_status;
}

void
simple_file_close(EFI_FILE *file)
{
   uefi_call_wrapper(file->Close, 1, file);
}
