/*
 * refind/lib.c
 * General library functions
 *
 * Copyright (c) 2006-2009 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Modifications copyright (c) 2012-2020 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 *
 */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "global.h"
#include "lib.h"
#include "icns.h"
#include "screen.h"
#include "../include/refit_call_wrapper.h"
#include "../include/RemovableMedia.h"
#include "gpt.h"
#include "config.h"
#include "mystrings.h"

#ifdef __MAKEWITH_GNUEFI
#define EfiReallocatePool ReallocatePool
#else
#define LibLocateHandle gBS->LocateHandleBuffer
#define DevicePathProtocol gEfiDevicePathProtocolGuid
#define BlockIoProtocol gEfiBlockIoProtocolGuid
#define LibFileSystemInfo EfiLibFileSystemInfo
#define LibOpenRoot EfiLibOpenRoot
EFI_DEVICE_PATH EndDevicePath[] = {
   {END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, {END_DEVICE_PATH_LENGTH, 0}}
};
#endif

// "Magic" signatures for various filesystems
#define FAT_MAGIC                        0xAA55
#define EXT2_SUPER_MAGIC                 0xEF53
#define HFSPLUS_MAGIC1                   0x2B48
#define HFSPLUS_MAGIC2                   0x5848
#define REISERFS_SUPER_MAGIC_STRING      "ReIsErFs"
#define REISER2FS_SUPER_MAGIC_STRING     "ReIsEr2Fs"
#define REISER2FS_JR_SUPER_MAGIC_STRING  "ReIsEr3Fs"
#define BTRFS_SIGNATURE                  "_BHRfS_M"
#define XFS_SIGNATURE                    "XFSB"
#define NTFS_SIGNATURE                   "NTFS    "
#define FAT12_SIGNATURE                  "FAT12   "
#define FAT16_SIGNATURE                  "FAT16   "
#define FAT32_SIGNATURE                  "FAT32   "

#if defined (EFIX64)
EFI_GUID gFreedesktopRootGuid = { 0x4f68bce3, 0xe8cd, 0x4db1, { 0x96, 0xe7, 0xfb, 0xca, 0xf9, 0x84, 0xb7, 0x09 }};
#elif defined (EFI32)
EFI_GUID gFreedesktopRootGuid = { 0x44479540, 0xf297, 0x41b2, { 0x9a, 0xf7, 0xd1, 0x31, 0xd5, 0xf0, 0x45, 0x8a }};
#elif defined (EFIAARCH64)
EFI_GUID gFreedesktopRootGuid = { 0xb921b045, 0x1df0, 0x41c3, { 0xaf, 0x44, 0x4c, 0x6f, 0x28, 0x0d, 0x3f, 0xae }};
#else
// Below is GUID for ARM32
EFI_GUID gFreedesktopRootGuid = { 0x69dad710, 0x2ce4, 0x4e3c, { 0xb1, 0x6c, 0x21, 0xa1, 0xd4, 0x9a, 0xbe, 0xd3 }};
#endif

// variables

EFI_HANDLE       SelfImageHandle;
EFI_LOADED_IMAGE *SelfLoadedImage;
EFI_FILE         *SelfRootDir;
EFI_FILE         *SelfDir;
CHAR16           *SelfDirPath;

REFIT_VOLUME     *SelfVolume    = NULL;
REFIT_VOLUME     **Volumes      = NULL;
UINTN            VolumesCount   = 0;
BOOLEAN          MediaCheck     = FALSE;
BOOLEAN          ScannedOnce    = FALSE;
BOOLEAN          SelfVolSet     = FALSE;
BOOLEAN          SelfVolRun     = FALSE;
extern EFI_GUID RefindGuid;

// Maximum size for disk sectors
#define SECTOR_SIZE 4096

// Number of bytes to read from a partition to determine its filesystem type
// and identify its boot loader, and hence probable BIOS-mode OS installation
#define SAMPLE_SIZE 69632 /* 68 KiB -- ReiserFS superblock begins at 64 KiB */



BOOLEAN egIsGraphicsModeEnabled (VOID);

//
// Pathname manipulations
//

// Converts forward slashes to backslashes, removes duplicate slashes, and
// removes slashes from both the start and end of the pathname.
// Necessary because some (buggy?) EFI implementations produce "\/" strings
// in pathnames, because some user inputs can produce duplicate directory
// separators, and because we want consistent start and end slashes for
// directory comparisons. A special case: If the PathName refers to root,
// but is non-empty, return "\", since some firmware implementations flake
// out if this isn't present.
VOID
CleanUpPathNameSlashes (
    IN OUT CHAR16 *PathName
) {
    UINTN Source = 0, Dest = 0;

    if ((PathName == NULL) || (PathName[0] == '\0')) {
        return;
    }

    while (PathName[Source] != '\0') {
        if ((PathName[Source] == L'/') || (PathName[Source] == L'\\')) {
            if (Dest == 0) {
                // Skip slash if to first position
                Source++;
            }
            else {
                PathName[Dest] = L'\\';
                do {
                    // Skip subsequent slashes
                    Source++;
                } while ((PathName[Source] == L'/') || (PathName[Source] == L'\\'));
                Dest++;
            } // if/else
        }
        else {
            // Regular character; copy it straight....
            PathName[Dest] = PathName[Source];
            Source++;
            Dest++;
        } // if/else
    } // while()
    if ((Dest > 0) && (PathName[Dest - 1] == L'\\')) {
        Dest--;
    }
    PathName[Dest] = L'\0';
    if (PathName[0] == L'\0') {
        PathName[0] = L'\\';
        PathName[1] = L'\0';
    }
} // CleanUpPathNameSlashes()

// Splits an EFI device path into device and filename components. For instance, if InString is
// PciRoot (0x0)/Pci (0x1f,0x2)/Ata (Secondary,Master,0x0)/HD (2,GPT,8314ae90-ada3-48e9-9c3b-09a88f80d921,0x96028,0xfa000)/\bzImage-3.5.1.efi,
// this function will truncate that input to
// PciRoot (0x0)/Pci (0x1f,0x2)/Ata (Secondary,Master,0x0)/HD (2,GPT,8314ae90-ada3-48e9-9c3b-09a88f80d921,0x96028,0xfa000)
// and return bzImage-3.5.1.efi as its return value.
// It does this by searching for the last ")" character in InString, copying everything
// after that string (after some cleanup) as the return value, and truncating the original
// input value.
// If InString contains no ")" character, this function leaves the original input string
// unmodified and also returns that string. If InString is NULL, this function returns NULL.
CHAR16*
SplitDeviceString (
    IN OUT CHAR16 *InString
) {
    INTN i;
    CHAR16 *FileName = NULL;
    BOOLEAN Found = FALSE;

    if (InString != NULL) {
        i = StrLen (InString) - 1;
        while ((i >= 0) && (!Found)) {
            if (InString[i] == L')') {
                Found = TRUE;
                FileName = StrDuplicate (&InString[i + 1]);
                CleanUpPathNameSlashes (FileName);
                InString[i + 1] = '\0';
            } // if
            i--;
        } // while
        if (FileName == NULL) {
            FileName = StrDuplicate (InString);
        }
    } // if

    return FileName;
} // static CHAR16* SplitDeviceString()

//
// Library initialization and de-initialization
//

static
EFI_STATUS
FinishInitRefitLib (
    VOID
) {
    EFI_STATUS  Status;

    if (SelfRootDir == NULL) {
        SelfRootDir = LibOpenRoot (SelfLoadedImage->DeviceHandle);
        if (SelfRootDir == NULL) {
            CheckError (EFI_LOAD_ERROR, L"while (re)opening our installation volume");
            return EFI_LOAD_ERROR;
        }
    }

    Status = refit_call5_wrapper(SelfRootDir->Open, SelfRootDir, &SelfDir, SelfDirPath, EFI_FILE_MODE_READ, 0);
    if (CheckFatalError (Status, L"while opening our installation directory")) {
        return EFI_LOAD_ERROR;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
InitRefitLib (
    IN EFI_HANDLE ImageHandle
) {
    EFI_STATUS  Status;
    CHAR16      *DevicePathAsString, *Temp = NULL;

    SelfImageHandle = ImageHandle;
    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        SelfImageHandle,
        &LoadedImageProtocol,
        (VOID **) &SelfLoadedImage
    );

    if (CheckFatalError (Status, L"While Getting a LoadedImageProtocol Handle")) {
        return EFI_LOAD_ERROR;
    }

    // find the current directory
    DevicePathAsString = DevicePathToStr (SelfLoadedImage->FilePath);
    GlobalConfig.SelfDevicePath = FileDevicePath (
        SelfLoadedImage->DeviceHandle,
        DevicePathAsString
    );
    CleanUpPathNameSlashes (DevicePathAsString);
    MyFreePool (SelfDirPath);
    Temp = FindPath (DevicePathAsString);
    SelfDirPath = SplitDeviceString (Temp);
    MyFreePool (DevicePathAsString);
    MyFreePool (Temp);

    return FinishInitRefitLib();
}

static
VOID
UninitVolumes (
    VOID
) {
    REFIT_VOLUME  *Volume;
    UINTN         VolumeIndex;

    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        Volume = Volumes[VolumeIndex];

        if (Volume->RootDir != NULL) {
            refit_call1_wrapper(Volume->RootDir->Close, Volume->RootDir);
            Volume->RootDir = NULL;
        }

        Volume->DeviceHandle = NULL;
        Volume->BlockIO = NULL;
        Volume->WholeDiskBlockIO = NULL;
    }
} /* VOID UninitVolumes() */

VOID
ReinitVolumes (
    VOID
) {
    EFI_STATUS       Status;
    REFIT_VOLUME     *Volume;
    UINTN            VolumeIndex;
    EFI_DEVICE_PATH  *RemainingDevicePath;
    EFI_HANDLE       DeviceHandle;
    EFI_HANDLE       WholeDiskHandle;

    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        Volume = Volumes[VolumeIndex];

        if (Volume->DevicePath != NULL) {
            // get the handle for that path
            RemainingDevicePath = Volume->DevicePath;
            Status = refit_call3_wrapper(
                gBS->LocateDevicePath,
                &BlockIoProtocol,
                &RemainingDevicePath,
                &DeviceHandle
            );

            if (!EFI_ERROR (Status)) {
                Volume->DeviceHandle = DeviceHandle;

                // get the root directory
                Volume->RootDir = LibOpenRoot (Volume->DeviceHandle);

            }
            else {
                CheckError (Status, L"from LocateDevicePath");
            }
        }

        if (Volume->WholeDiskDevicePath != NULL) {
            // get the handle for that path
            RemainingDevicePath = Volume->WholeDiskDevicePath;
            Status = refit_call3_wrapper(
                gBS->LocateDevicePath,
                &BlockIoProtocol,
                &RemainingDevicePath,
                &WholeDiskHandle
            );

            if (!EFI_ERROR (Status)) {
                // get the BlockIO protocol
                Status = refit_call3_wrapper(
                    gBS->HandleProtocol,
                    WholeDiskHandle,
                    &BlockIoProtocol,
                    (VOID **) &Volume->WholeDiskBlockIO
                );
                if (EFI_ERROR (Status)) {
                    Volume->WholeDiskBlockIO = NULL;
                    CheckError (Status, L"from HandleProtocol");
                }
            }
            else {
                CheckError (Status, L"from LocateDevicePath");
            }
        }
    }
} /* VOID ReinitVolumes (VOID) */

// called before running external programs to close open file handles
VOID
UninitRefitLib (
    VOID
) {
    // This piece of code was made to correspond to weirdness in ReinitRefitLib().
    // See the comment on it there.
    if (SelfRootDir == SelfVolume->RootDir) {
        SelfRootDir=0;
    }

    UninitVolumes();

    if (SelfDir != NULL) {
        refit_call1_wrapper(SelfDir->Close, SelfDir);
        SelfDir = NULL;
    }

    if (SelfRootDir != NULL) {
       refit_call1_wrapper(SelfRootDir->Close, SelfRootDir);
       SelfRootDir = NULL;
    }
} /* VOID UninitRefitLib() */

// called after running external programs to re-open file handles
EFI_STATUS
ReinitRefitLib (
    VOID
) {
    ReinitVolumes();

    if ((gST->Hdr.Revision >> 16) == 1) {
       // Below two lines were in rEFIt, but seem to cause system crashes or
       // reboots when launching OSes after returning from programs on most
       // systems. OTOH, my Mac Mini produces errors about "(re)opening our
       // installation volume" (see the next function) when returning from
       // programs when these two lines are removed, and it often crashes
       // when returning from a program or when launching a second program
       // with these lines removed. Therefore, the preceding if() statement
       // executes these lines only on EFIs with a major version number of 1
       // (which Macs have) and not with 2 (which UEFI PCs have). My selection
       // of hardware on which to test is limited, though, so this may be the
       // wrong test, or there may be a better way to fix this problem.
       // TODO: Figure out cause of above weirdness and fix it more
       // reliably!
       if (SelfVolume != NULL && SelfVolume->RootDir != NULL) {
           SelfRootDir = SelfVolume->RootDir;
       }
    } // if

    return FinishInitRefitLib();
}

//
// EFI variable read and write functions
//

// Retrieve a raw EFI variable, either from NVRAM or from a disk file under
// rEFInd's "vars" subdirectory, depending on GlobalConfig.UseNvram.
// Returns EFI status
EFI_STATUS
EfivarGetRaw (
    EFI_GUID  *vendor,
    CHAR16    *name,
    CHAR8     **buffer,
    UINTN     *size
) {
    UINTN       l;
    UINT8       *buf          = NULL;
    EFI_FILE    *VarsDir      = NULL;
    BOOLEAN     ReadFromNvram = TRUE;
    EFI_STATUS  Status;

    if (!GlobalConfig.UseNvram && GuidsAreEqual (vendor, &RefindGuid)) {
        Status = refit_call5_wrapper(
            SelfDir->Open,
            SelfDir,
            &VarsDir,
            L"vars",
            EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
            EFI_FILE_DIRECTORY
        );
        if (Status == EFI_SUCCESS) {
            Status = egLoadFile (VarsDir, name, &buf, size);
            ReadFromNvram = FALSE;
        }
        else if (Status == EFI_WRITE_PROTECTED) {
            // Override use_nvram
            GlobalConfig.UseNvram = TRUE;

            #if REFIT_DEBUG > 0
            MsgLog ("** WARN: Could Not Read '%s' from Emulated NVRAM\n", name);
            MsgLog ("         Set 'use_nvram' to 'true' to Silence this Warning\n\n");
            #endif
        }
        else {
            #if REFIT_DEBUG > 0
            if (Status != EFI_NOT_FOUND) {
                MsgLog ("** WARN: Could Not Read '%s' NVRAM Variable\n", name);
            }
            #endif

            MyFreePool (VarsDir);

            return Status;
        }
        MyFreePool (VarsDir);
    }

    if (GlobalConfig.UseNvram || ! GuidsAreEqual (vendor, &RefindGuid)) {
        l = sizeof (CHAR16 *) * EFI_MAXIMUM_VARIABLE_SIZE;
        buf = AllocatePool (l);
        if (!buf) {
            *buffer = NULL;
            return EFI_OUT_OF_RESOURCES;
        }
        Status = refit_call5_wrapper(gRT->GetVariable, name, vendor, NULL, &l, buf);
    }
    if (EFI_ERROR (Status) == EFI_SUCCESS) {
        *buffer = (CHAR8*) buf;
        if ((size) && ReadFromNvram) {
            *size = l;
        }
    }
    else {
        MyFreePool (buf);
        *buffer = NULL;
    }
    return Status;
} // EFI_STATUS EfivarGetRaw()

// Set an EFI variable, either to NVRAM or to a disk file under rEFInd's
// "vars" subdirectory, depending on GlobalConfig.UseNvram.
// Returns EFI status
EFI_STATUS
EfivarSetRaw (
    EFI_GUID  *vendor,
    CHAR16    *name,
    CHAR8     *buf,
    UINTN     size,
    BOOLEAN   persistent
) {
    UINT32      flags;
    EFI_FILE    *VarsDir = NULL;
    EFI_STATUS  Status;

    if (!GlobalConfig.UseNvram && GuidsAreEqual (vendor, &RefindGuid)) {
        Status = refit_call5_wrapper(
            SelfDir->Open,
            SelfDir,
            &VarsDir,
            L"vars",
            EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
            EFI_FILE_DIRECTORY
        );
        if (Status == EFI_SUCCESS) {
            Status = egSaveFile (VarsDir, name, (UINT8 *) buf, size);
        }
        else if (Status == EFI_WRITE_PROTECTED) {
            // Override use_nvram
            GlobalConfig.UseNvram = TRUE;

            #if REFIT_DEBUG > 0
            MsgLog ("** WARN: Could Not Write '%s' to Emulated NVRAM\n", name);
            MsgLog ("         Set 'use_nvram' to 'true' to Silence this Warning\n\n");
            #endif
        }
        else {
            #if REFIT_DEBUG > 0
            MsgLog ("** WARN: Could Not Save '%s' NVRAM Variable\n", name);
            #endif

            return Status;
        }
        MyFreePool (VarsDir);
    }

    if (GlobalConfig.UseNvram || ! GuidsAreEqual (vendor, &RefindGuid)) {
        flags = EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS;
        if (persistent) {
            flags |= EFI_VARIABLE_NON_VOLATILE;
        }

        Status = refit_call5_wrapper(gRT->SetVariable, name, vendor, flags, size, buf);
    }
    return Status;
} // EFI_STATUS EfivarSetRaw()

//
// list functions
//

VOID
AddListElement (
    IN OUT VOID ***ListPtr,
    IN OUT UINTN *ElementCount,
    IN VOID *NewElement
) {
    UINTN AllocateCount;

    if ((*ElementCount & 15) == 0) {
        AllocateCount = *ElementCount + 16;

        if (*ElementCount == 0) {
            *ListPtr = AllocatePool (sizeof (VOID *) * AllocateCount);
        }
        else {
            *ListPtr = EfiReallocatePool (
                *ListPtr,
                sizeof (VOID *) * (*ElementCount),
                sizeof (VOID *) * AllocateCount
            );
        }
    }
    (*ListPtr)[*ElementCount] = NewElement;
    (*ElementCount)++;
} /* VOID AddListElement() */

VOID
FreeList (
    IN OUT VOID ***ListPtr,
    IN OUT UINTN *ElementCount
) {
    UINTN i;

    if ((*ElementCount > 0) && (**ListPtr != NULL)) {
        for (i = 0; i < *ElementCount; i++) {
            // TODO: call a user-provided routine for each element here
            MyFreePool ((*ListPtr)[i]);
        }
        MyFreePool (*ListPtr);
    }
} // VOID FreeList()

//
// volume functions
//

// Return a pointer to a string containing a filesystem type name. If the
// filesystem type is unknown, a blank (but non-null) string is returned.
// The returned variable is a constant that should NOT be freed.
static
CHAR16 *
FSTypeName (
    IN UINT32 TypeCode
) {
   CHAR16 *retval = NULL;

   switch (TypeCode) {
      case FS_TYPE_WHOLEDISK:
         retval = L"whole disk";
         break;
      case FS_TYPE_FAT:
         retval = L"FAT";
         break;
      case FS_TYPE_HFSPLUS:
         retval = L"HFS+";
         break;
      case FS_TYPE_EXT2:
         retval = L"ext2";
         break;
      case FS_TYPE_EXT3:
         retval = L"ext3";
         break;
      case FS_TYPE_EXT4:
         retval = L"ext4";
         break;
      case FS_TYPE_REISERFS:
         retval = L"ReiserFS";
         break;
      case FS_TYPE_BTRFS:
         retval = L"Btrfs";
         break;
      case FS_TYPE_XFS:
         retval = L"XFS";
         break;
      case FS_TYPE_ISO9660:
         retval = L"ISO-9660";
         break;
      case FS_TYPE_NTFS:
         retval = L"NTFS";
         break;
      default:
         retval = L"";
         break;
   } // switch
   return retval;
} // CHAR16 *FSTypeName()

// Identify the filesystem type and record the filesystem's UUID/serial number,
// if possible. Expects a Buffer containing the first few (normally at least
// 4096) bytes of the filesystem. Sets the filesystem type code in Volume->FSType
// and the UUID/serial number in Volume->VolUuid. Note that the UUID value is
// recognized differently for each filesystem, and is currently supported only
// for NTFS, FAT, ext2/3/4fs, and ReiserFS (and for NTFS and FAT it's really a
// 64-bit or 32-bit serial number not a UUID or GUID). If the UUID can't be
// determined, it's set to 0. Also, the UUID is just read directly into memory;
// it is *NOT* valid when displayed by GuidAsString() or used in other
// GUID/UUID-manipulating functions. (As I write, it's being used merely to
// detect partitions that are part of a RAID 1 array.)
static
VOID
SetFilesystemData (
    IN UINT8 *Buffer,
    IN UINTN BufferSize,
    IN OUT REFIT_VOLUME *Volume
) {
   UINT32  *Ext2Incompat, *Ext2Compat;
   UINT16  *Magic16;
   char    *MagicString;

   if ((Buffer != NULL) && (Volume != NULL)) {
      SetMem (&(Volume->VolUuid), sizeof (EFI_GUID), 0);
      Volume->FSType = FS_TYPE_UNKNOWN;

      if (BufferSize >= (1024 + 100)) {
         Magic16 = (UINT16*) (Buffer + 1024 + 56);
         if (*Magic16 == EXT2_SUPER_MAGIC) {
             // ext2/3/4
            Ext2Compat = (UINT32*) (Buffer + 1024 + 92);
            Ext2Incompat = (UINT32*) (Buffer + 1024 + 96);
            if ((*Ext2Incompat & 0x0040) || (*Ext2Incompat & 0x0200)) {
                // check for extents or flex_bg
               Volume->FSType = FS_TYPE_EXT4;
            }
            else if (*Ext2Compat & 0x0004) {
                // check for journal
               Volume->FSType = FS_TYPE_EXT3;
            }
            else {
                // none of these features; presume it's ext2...
               Volume->FSType = FS_TYPE_EXT2;
            }
            CopyMem (&(Volume->VolUuid), Buffer + 1024 + 120, sizeof (EFI_GUID));
            return;
         }
      } // search for ext2/3/4 magic

      if (BufferSize >= (65536 + 100)) {
         MagicString = (char*) (Buffer + 65536 + 52);
         if ((CompareMem (MagicString, REISERFS_SUPER_MAGIC_STRING, 8) == 0) ||
             (CompareMem (MagicString, REISER2FS_SUPER_MAGIC_STRING, 9) == 0) ||
             (CompareMem (MagicString, REISER2FS_JR_SUPER_MAGIC_STRING, 9) == 0)
         ) {
            Volume->FSType = FS_TYPE_REISERFS;
            CopyMem (&(Volume->VolUuid), Buffer + 65536 + 84, sizeof (EFI_GUID));
            return;
         } // if
      } // search for ReiserFS magic

      if (BufferSize >= (65536 + 64 + 8)) {
         MagicString = (char*) (Buffer + 65536 + 64);
         if (CompareMem (MagicString, BTRFS_SIGNATURE, 8) == 0) {
            Volume->FSType = FS_TYPE_BTRFS;
            return;
         } // if
      } // search for Btrfs magic

      if (BufferSize >= 512) {
         MagicString = (char*) Buffer;
         if (CompareMem (MagicString, XFS_SIGNATURE, 4) == 0) {
            Volume->FSType = FS_TYPE_XFS;
            return;
         }
      } // search for XFS magic

      if (BufferSize >= (1024 + 2)) {
         Magic16 = (UINT16*) (Buffer + 1024);
         if ((*Magic16 == HFSPLUS_MAGIC1) || (*Magic16 == HFSPLUS_MAGIC2)) {
            Volume->FSType = FS_TYPE_HFSPLUS;
            return;
         }
      } // search for HFS+ magic

      if (BufferSize >= 512) {
         // Search for NTFS, FAT, and MBR/EBR.
         // These all have 0xAA55 at the end of the first sector, so we must
         // also search for NTFS, FAT12, FAT16, and FAT32 signatures to
         // figure out where to look for the filesystem serial numbers.
         Magic16 = (UINT16*) (Buffer + 510);
         if (*Magic16 == FAT_MAGIC) {
            MagicString = (char*) Buffer;
            if (CompareMem (MagicString + 3, NTFS_SIGNATURE, 8) == 0) {
               Volume->FSType = FS_TYPE_NTFS;
               CopyMem (&(Volume->VolUuid), Buffer + 0x48, sizeof (UINT64));
            }
            else if ((CompareMem (MagicString + 0x36, FAT12_SIGNATURE, 8) == 0) ||
                       (CompareMem (MagicString + 0x36, FAT16_SIGNATURE, 8) == 0)) {
                Volume->FSType = FS_TYPE_FAT;
                CopyMem (&(Volume->VolUuid), Buffer + 0x27, sizeof (UINT32));
            }
            else if (CompareMem (MagicString + 0x52, FAT32_SIGNATURE, 8) == 0) {
                Volume->FSType = FS_TYPE_FAT;
                CopyMem (&(Volume->VolUuid), Buffer + 0x43, sizeof (UINT32));
            }
            else if (!Volume->BlockIO->Media->LogicalPartition) {
                Volume->FSType = FS_TYPE_WHOLEDISK;
            } // if/else

            return;
         } // if
      } // search for FAT and NTFS magic

      // If no other filesystem is identified and block size is right, assume
      // it's ISO-9660....
      if (Volume->BlockIO->Media->BlockSize == 2048) {
          Volume->FSType = FS_TYPE_ISO9660;
          return;
      }
   } // if ((Buffer != NULL) && (Volume != NULL))
} // UINT32 SetFilesystemData()

static
VOID
ScanVolumeBootcode (
    REFIT_VOLUME  *Volume,
    BOOLEAN       *Bootable
) {
    EFI_STATUS          Status;
    UINT8               Buffer[SAMPLE_SIZE];
    UINTN               i;
    MBR_PARTITION_INFO  *MbrTable;
    BOOLEAN             MbrTableFound = FALSE;

    Volume->HasBootCode = FALSE;
    Volume->OSIconName  = NULL;
    Volume->OSName      = NULL;
    *Bootable           = FALSE;
    MediaCheck          = FALSE;

    if (Volume->BlockIO == NULL) {
        return;
    }
    if (Volume->BlockIO->Media->BlockSize > SAMPLE_SIZE) {
        return;   // our buffer is too small...
    }

    // look at the boot sector (this is used for both hard disks and El Torito images!)
    Status = refit_call5_wrapper(
        Volume->BlockIO->ReadBlocks,
        Volume->BlockIO,
        Volume->BlockIO->Media->MediaId,
        Volume->BlockIOOffset,
        SAMPLE_SIZE,
        Buffer
    );
    if (!EFI_ERROR (Status)) {
        SetFilesystemData (Buffer, SAMPLE_SIZE, Volume);
    }
    if ((Status == EFI_SUCCESS) && (GlobalConfig.LegacyType == LEGACY_TYPE_MAC)) {
        if ((*((UINT16 *)(Buffer + 510)) == 0xaa55 && Buffer[0] != 0) &&
            (FindMem (Buffer, 512, "EXFAT", 5) == -1)
        ) {
            *Bootable = TRUE;
            Volume->HasBootCode = TRUE;
        }

        // detect specific boot codes
        if (CompareMem (Buffer + 2, "LILO", 4) == 0 ||
            CompareMem (Buffer + 6, "LILO", 4) == 0 ||
            CompareMem (Buffer + 3, "SYSLINUX", 8) == 0 ||
            FindMem (Buffer, SECTOR_SIZE, "ISOLINUX", 8) >= 0
        ) {
            Volume->HasBootCode = TRUE;
            Volume->OSIconName = L"linux";
            Volume->OSName = L"Linux (Legacy)";
        }
        // GRUB
        else if (FindMem (Buffer, 512, "Geom\0Hard Disk\0Read\0 Error", 26) >= 0) {
            Volume->HasBootCode = TRUE;
            Volume->OSIconName = L"grub,linux";
            Volume->OSName = L"Linux (Legacy)";
        }
        else if ((*((UINT32 *)(Buffer + 502)) == 0 &&
            *((UINT32 *)(Buffer + 506)) == 50000 &&
            *((UINT16 *)(Buffer + 510)) == 0xaa55) ||
            FindMem (Buffer, SECTOR_SIZE, "Starting the BTX loader", 23) >= 0
        ) {
            Volume->HasBootCode = TRUE;
            Volume->OSIconName = L"freebsd";
            Volume->OSName = L"FreeBSD (Legacy)";
        }
        // If more differentiation needed, also search for
        // "Invalid partition table" &/or "Missing boot loader".
        else if ((*((UINT16 *)(Buffer + 510)) == 0xaa55) &&
            (FindMem (Buffer, SECTOR_SIZE, "Boot loader too large", 21) >= 0) &&
            (FindMem (Buffer, SECTOR_SIZE, "I/O error loading boot loader", 29) >= 0)
        ) {
            Volume->HasBootCode = TRUE;
            Volume->OSIconName = L"freebsd";
            Volume->OSName = L"FreeBSD (Legacy)";
        }
        else if (FindMem (Buffer, 512, "!Loading", 8) >= 0 ||
            FindMem (Buffer, SECTOR_SIZE, "/cdboot\0/CDBOOT\0", 16) >= 0
        ) {
            Volume->HasBootCode = TRUE;
            Volume->OSIconName = L"openbsd";
            Volume->OSName = L"OpenBSD (Legacy)";
        }
        else if (FindMem (Buffer, 512, "Not a bootxx image", 18) >= 0 ||
            *((UINT32 *)(Buffer + 1028)) == 0x7886b6d1
        ) {
            Volume->HasBootCode = TRUE;
            Volume->OSIconName = L"netbsd";
            Volume->OSName = L"NetBSD (Legacy)";
        }
        // Windows NT/200x/XP
        else if (FindMem (Buffer, SECTOR_SIZE, "NTLDR", 5) >= 0) {
            Volume->HasBootCode = TRUE;
            Volume->OSIconName = L"win";
            Volume->OSName = L"Windows (NT/XP)";
        }
        // Windows Vista/7/8/10
        else if (FindMem (Buffer, SECTOR_SIZE, "BOOTMGR", 7) && ((*((UINT16 *)(Buffer + 450)) != 0xeefe) >= 0) {
            Volume->HasBootCode = TRUE;
            Volume->OSIconName = L"win8,win";
            Volume->OSName = L"Windows (Legacy)";
        }
        else if (FindMem (Buffer, 512, "CPUBOOT SYS", 11) >= 0 ||
            FindMem (Buffer, 512, "KERNEL  SYS", 11) >= 0
        ) {
            Volume->HasBootCode = TRUE;
            Volume->OSIconName = L"freedos";
            Volume->OSName = L"FreeDOS (Legacy)";
        }
        else if (FindMem (Buffer, 512, "OS2LDR", 6) >= 0 ||
            FindMem (Buffer, 512, "OS2BOOT", 7) >= 0
        ) {
            Volume->HasBootCode = TRUE;
            Volume->OSIconName = L"ecomstation";
            Volume->OSName = L"eComStation (Legacy)";
        }
        else if (FindMem (Buffer, 512, "Be Boot Loader", 14) >= 0) {
            Volume->HasBootCode = TRUE;
            Volume->OSIconName = L"beos";
            Volume->OSName = L"BeOS (Legacy)";
        }
        else if (FindMem (Buffer, 512, "yT Boot Loader", 14) >= 0) {
            Volume->HasBootCode = TRUE;
            Volume->OSIconName = L"zeta,beos";
            Volume->OSName = L"ZETA (Legacy)";
        }
        else if (FindMem (Buffer, 512, "\x04" "beos\x06" "system\x05" "zbeos", 18) >= 0 ||
            FindMem (Buffer, 512, "\x06" "system\x0c" "haiku_loader", 20) >= 0
        ) {
            Volume->HasBootCode = TRUE;
            Volume->OSIconName = L"haiku,beos";
            Volume->OSName = L"Haiku (Legacy)";
        }

        // NOTE: If you add an operating system with a name that starts with 'W' or 'L', you
        //  need to fix AddLegacyEntry in refind/legacy.c.

        // dummy FAT boot sector (created by OS X's newfs_msdos)
        if (FindMem (Buffer, 512, "Non-system disk", 15) >= 0) {
            Volume->HasBootCode = FALSE;
        }

        // dummy FAT boot sector (created by Linux's mkdosfs)
        if (FindMem (Buffer, 512, "This is Not a Bootable Disk", 27) >= 0) {
            Volume->HasBootCode = FALSE;
        }

        // dummy FAT boot sector (created by Windows)
        if (FindMem (Buffer, 512, "Press Any Key to Restart", 24) >= 0) {
            Volume->HasBootCode = FALSE;
        }

        // check for MBR partition table
        if (*((UINT16 *)(Buffer + 510)) == 0xaa55) {
            MbrTable = (MBR_PARTITION_INFO *)(Buffer + 446);
            for (i = 0; i < 4; i++) {
                if (MbrTable[i].StartLBA && MbrTable[i].Size) {
                    MbrTableFound = TRUE;
                }
            }
            for (i = 0; i < 4; i++) {
                if (MbrTable[i].Flags != 0x00 && MbrTable[i].Flags != 0x80) {
                    MbrTableFound = FALSE;
                }
            }
            if (MbrTableFound) {
                Volume->MbrPartitionTable = AllocatePool (4 * 16);
                CopyMem (Volume->MbrPartitionTable, MbrTable, 4 * 16);
            }
        }
    }
    else {
        #if REFIT_DEBUG > 0
        if (SelfVolRun) {
            if (Status == EFI_NO_MEDIA) {
                MediaCheck = TRUE;
            }
            ScannedOnce = FALSE;

            MsgLog("\n");
            CheckError (Status, L"Found While Reading Boot Sector on Volume Below");
        }
        #endif
    }
} /* VOID ScanVolumeBootcode() */

// Set default volume badge icon based on /.VolumeBadge.{icns|png} file or disk kind
VOID
SetVolumeBadgeIcon (
    REFIT_VOLUME *Volume
) {
   if (Volume == NULL) {
       return;
   }
   if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_BADGES) {
       return;
   }

   if (Volume->VolBadgeImage == NULL) {
      Volume->VolBadgeImage = egLoadIconAnyType (
          Volume->RootDir, L"", L".VolumeBadge",
          GlobalConfig.IconSizes[ICON_SIZE_BADGE]
      );
   }

   if (Volume->VolBadgeImage == NULL) {
      switch (Volume->DiskKind) {
          case DISK_KIND_INTERNAL:
             Volume->VolBadgeImage = BuiltinIcon (BUILTIN_ICON_VOL_INTERNAL);
             break;
          case DISK_KIND_EXTERNAL:
             Volume->VolBadgeImage = BuiltinIcon (BUILTIN_ICON_VOL_EXTERNAL);
             break;
          case DISK_KIND_OPTICAL:
             Volume->VolBadgeImage = BuiltinIcon (BUILTIN_ICON_VOL_OPTICAL);
             break;
          case DISK_KIND_NET:
             Volume->VolBadgeImage = BuiltinIcon (BUILTIN_ICON_VOL_NET);
             break;
      } // switch()
   }
} // VOID SetVolumeBadgeIcon()

// Return a string representing the input size in IEEE-1541 units.
// The calling function is responsible for freeing the allocated memory.
static
CHAR16 *
SizeInIEEEUnits (
    UINT64 SizeInBytes
) {
    UINT64 SizeInIeee;
    UINTN Index = 0, NumPrefixes;
    CHAR16 *Units = NULL, *Prefixes = L" KMGTPEZ";
    CHAR16 *TheValue;

    TheValue = AllocateZeroPool (sizeof (CHAR16) * 256);
    if (TheValue != NULL) {
        NumPrefixes = StrLen (Prefixes);
        SizeInIeee = SizeInBytes;
        while ((SizeInIeee > 1024) && (Index < (NumPrefixes - 1))) {
            Index++;
            SizeInIeee /= 1024;
        } // while
        if (Prefixes[Index] == ' ') {
            Units = StrDuplicate (L"-byte");
        }
        else {
            Units = StrDuplicate (L"  iB");
            Units[1] = Prefixes[Index];
        } // if/else
        SPrint (TheValue, 255, L"%ld%s", SizeInIeee, Units);
    } // if
    MyFreePool (Units);
    return TheValue;
} // CHAR16 *SizeInIEEEUnits()

// Return a name for the volume. Ideally this should be the label for the
// filesystem or volume, but this function falls back to describing the
// filesystem by size (200 MiB, etc.) and/or type (ext2, HFS+, etc.), if
// this information can be extracted.
// The calling function is responsible for freeing the memory allocated
// for the name string.
CHAR16
*GetVolumeName (
    IN REFIT_VOLUME *Volume
) {
    EFI_FILE_SYSTEM_INFO  *FileSystemInfoPtr = NULL;
    CHAR16                *FoundName = NULL;
    CHAR16                *SISize, *TypeName;

    if (Volume->RootDir != NULL) {
        FileSystemInfoPtr = LibFileSystemInfo (Volume->RootDir);
     }

    if ((FileSystemInfoPtr != NULL) &&
        (FileSystemInfoPtr->VolumeLabel != NULL) &&
        (StrLen (FileSystemInfoPtr->VolumeLabel) > 0)
    ) {
        FoundName = StrDuplicate (FileSystemInfoPtr->VolumeLabel);
    }

    // If no filesystem name, try to use the partition name....
    if ((FoundName == NULL) && (Volume->PartName != NULL) &&
        (StrLen (Volume->PartName) > 0) &&
        !IsIn (Volume->PartName, IGNORE_PARTITION_NAMES)
    ) {
        FoundName = StrDuplicate (Volume->PartName);
    } // if use partition name

    // NOTE: Don't free TypeName; function returns constant
    TypeName = FSTypeName (Volume->FSType);

    // No filesystem or acceptable partition name, so use fs type and size
    if ((FoundName == NULL) && (FileSystemInfoPtr != NULL)) {
        FoundName = AllocateZeroPool (sizeof (CHAR16) * 256);
        if (FoundName != NULL) {
            SISize = SizeInIEEEUnits (FileSystemInfoPtr->VolumeSize);
            SPrint (FoundName, 255, L"%s[%s] Volume", TypeName, SISize);
            MyFreePool (SISize);
        } // if allocated memory OK
    } // if (FoundName == NULL)

    MyFreePool (FileSystemInfoPtr);

    if (FoundName == NULL) {
        FoundName = AllocateZeroPool (sizeof (CHAR16) * 256);
        if (FoundName != NULL) {
            if (StrLen (TypeName) > 0) {
                SPrint (FoundName, 255, L"%s Volume", TypeName);
            }
            else if (MediaCheck) {
                SPrint (FoundName, 255, L"Disc/Network Volume (Assumed)");
            }
            else if (MyStriCmp (L"Apple", gST->FirmwareVendor)) {
                SPrint (FoundName, 255, L"APFS Container (Assumed)");
            }
            else {
                SPrint (FoundName, 255, L"Unknown Volume");
            }
        } // if allocated memory OK
    } // if

    // TODO: Above could be improved/extended, in case filesystem name is not found,
    // such as:
    //  - use or add disk/partition number (e.g., "(hd0,2)")

    // Desperate fallback name....
    if (FoundName == NULL) {
        FoundName = StrDuplicate (L"Unknown Volume");
    }

    return FoundName;
} // static CHAR16 *GetVolumeName()

// Determine the unique GUID, type code GUID, and name of the volume and store them.
static
VOID
SetPartGuidAndName (
    REFIT_VOLUME *Volume,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath
) {
    HARDDRIVE_DEVICE_PATH    *HdDevicePath;
    GPT_ENTRY                *PartInfo;

    if ((Volume == NULL) || (DevicePath == NULL))
        return;

    if ((DevicePath->Type == MEDIA_DEVICE_PATH) &&
        (DevicePath->SubType == MEDIA_HARDDRIVE_DP)
    ) {
        HdDevicePath = (HARDDRIVE_DEVICE_PATH*) DevicePath;
        if (HdDevicePath->SignatureType == SIGNATURE_TYPE_GUID) {
            Volume->PartGuid = *((EFI_GUID*) HdDevicePath->Signature);
            PartInfo = FindPartWithGuid (&(Volume->PartGuid));
            if (PartInfo) {
                Volume->PartName = StrDuplicate (PartInfo->name);
                CopyMem (&(Volume->PartTypeGuid), PartInfo->type_guid, sizeof (EFI_GUID));

                if (GuidsAreEqual (&(Volume->PartTypeGuid), &gFreedesktopRootGuid) &&
                    ((PartInfo->attributes & GPT_NO_AUTOMOUNT) == 0)
                ) {
                    GlobalConfig.DiscoveredRoot = Volume;
                } // if (GUIDs match && automounting OK)

                Volume->IsMarkedReadOnly = ((PartInfo->attributes & GPT_READ_ONLY) > 0);
                MyFreePool (PartInfo);
            } // if (PartInfo exists)
        }
        else {
            // TODO: Better to assign a random GUID to MBR partitions, but I couldn't
            // find an EFI function to do this. The below GUID is just one that I
            // generated in Linux.
            Volume->PartGuid = StringAsGuid (L"92a6c61f-7130-49b9-b05c-8d7e7b039127");
        } // if/else (GPT disk)
    } // if (disk device)
} // VOID SetPartGuid()

// Return TRUE if NTFS boot files are found or if Volume is unreadable,
// FALSE otherwise. The idea is to weed out non-boot NTFS volumes from
// BIOS/legacy boot list on Macs. We can't assume NTFS will be readable,
// so return TRUE if it's unreadable; but if it IS readable, return
// TRUE only if Windows boot files are found.
static
BOOLEAN
HasWindowsBiosBootFiles (
    REFIT_VOLUME *Volume
) {
    BOOLEAN FilesFound = TRUE;

    if (Volume->RootDir != NULL) {
        FilesFound = FileExists (Volume->RootDir, L"NTLDR") ||  // Windows NT/200x/XP boot file
                     FileExists (Volume->RootDir, L"bootmgr");  // Windows Vista/7/8 boot file
    } // if
    return FilesFound;
} // static VOID HasWindowsBiosBootFiles()

VOID
ScanVolume (
    REFIT_VOLUME *Volume
) {
    EFI_STATUS       Status;
    EFI_DEVICE_PATH  *DevicePath;
    EFI_DEVICE_PATH  *NextDevicePath;
    EFI_DEVICE_PATH  *DiskDevicePath;
    EFI_DEVICE_PATH  *RemainingDevicePath;
    EFI_HANDLE       WholeDiskHandle;
    UINTN            PartialLength;
    BOOLEAN          Bootable;
    const CHAR16     *ShowScreenStr = NULL;

    // get device path
    Volume->DevicePath = DuplicateDevicePath (
        DevicePathFromHandle (Volume->DeviceHandle)
    );
    if (Volume->DevicePath != NULL) {
        #if REFIT_DEBUG >= 2
        DumpHex (1, 0, DevicePathSize (Volume->DevicePath), Volume->DevicePath);
        #endif
    }

    Volume->DiskKind = DISK_KIND_INTERNAL;  // default

    // get block i/o
    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        Volume->DeviceHandle,
        &BlockIoProtocol,
        (VOID **) &(Volume->BlockIO)
    );
    if (EFI_ERROR (Status)) {
        Volume->BlockIO = NULL;

        SwitchToText (FALSE);

        ShowScreenStr = L"ERROR: Cannot get BlockIO Protocol";

        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
        PrintUglyText ((CHAR16 *) ShowScreenStr, NEXTLINE);
        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

        #if REFIT_DEBUG > 0
        MsgLog ("%s\n\n", ShowScreenStr);
        #endif

        HaltForKey();
        SwitchToGraphics();
    }
    else if (Volume->BlockIO->Media->BlockSize == 2048) {
            Volume->DiskKind = DISK_KIND_OPTICAL;
    }

    // scan for bootcode and MBR table
    Bootable = FALSE;
    ScanVolumeBootcode (Volume, &Bootable);

    // detect device type
    DevicePath = Volume->DevicePath;
    while (DevicePath != NULL && !IsDevicePathEndType (DevicePath)) {
        NextDevicePath = NextDevicePathNode (DevicePath);

        if (DevicePathType (DevicePath) == MEDIA_DEVICE_PATH) {
           SetPartGuidAndName (Volume, DevicePath);
        }
        if (DevicePathType (DevicePath) == MESSAGING_DEVICE_PATH &&
            (DevicePathSubType (DevicePath) == MSG_USB_DP ||
            DevicePathSubType (DevicePath) == MSG_USB_CLASS_DP ||
            DevicePathSubType (DevicePath) == MSG_1394_DP ||
            DevicePathSubType (DevicePath) == MSG_FIBRECHANNEL_DP)
        ) {
            Volume->DiskKind = DISK_KIND_EXTERNAL;    // USB/FireWire/FC device -> external
        }
        if (DevicePathType (DevicePath) == MEDIA_DEVICE_PATH &&
            DevicePathSubType (DevicePath) == MEDIA_CDROM_DP
        ) {
            Volume->DiskKind = DISK_KIND_OPTICAL;     // El Torito entry -> optical disk
            Bootable = TRUE;
        }

//         if (DevicePathType (DevicePath) == MEDIA_DEVICE_PATH &&
//              DevicePathSubType (DevicePath) == MEDIA_VENDOR_DP
//          ) {
//             Volume->IsAppleLegacy = TRUE;  // legacy BIOS device entry
//             // TODO: also check for Boot Camp GUID
//             Bootable = FALSE; // this handle's BlockIO is just an alias for the whole device
//         }

        if (DevicePathType (DevicePath) == MESSAGING_DEVICE_PATH) {
            // make a device path for the whole device
            PartialLength = (UINT8 *)NextDevicePath - (UINT8 *)(Volume->DevicePath);
            DiskDevicePath = (EFI_DEVICE_PATH *)AllocatePool (PartialLength +
                sizeof (EFI_DEVICE_PATH)
            );
            CopyMem (DiskDevicePath, Volume->DevicePath, PartialLength);
            CopyMem (
                (UINT8 *)DiskDevicePath + PartialLength,
                EndDevicePath,
                sizeof (EFI_DEVICE_PATH)
            );

            // get the handle for that path
            RemainingDevicePath = DiskDevicePath;
            Status = refit_call3_wrapper(
                gBS->LocateDevicePath,
                &BlockIoProtocol,
                &RemainingDevicePath,
                &WholeDiskHandle
            );
            MyFreePool (DiskDevicePath);

            if (!EFI_ERROR (Status)) {
                //Print (
                //    L"  - original handle: %08x - disk handle: %08x\n",
                //    (UINT32)DeviceHandle,
                //    (UINT32)WholeDiskHandle
                //);

                // get the device path for later
                Status = refit_call3_wrapper(
                    gBS->HandleProtocol,
                    WholeDiskHandle,
                    &DevicePathProtocol,
                    (VOID **) &DiskDevicePath
                );
                if (!EFI_ERROR (Status)) {
                    Volume->WholeDiskDevicePath = DuplicateDevicePath (DiskDevicePath);
                }

                // look at the BlockIO protocol
                Status = refit_call3_wrapper(
                    gBS->HandleProtocol,
                    WholeDiskHandle,
                    &BlockIoProtocol,
                    (VOID **) &Volume->WholeDiskBlockIO
                );
                if (!EFI_ERROR (Status)) {
                    // check the media block size
                    if (Volume->WholeDiskBlockIO->Media->BlockSize == 2048) {
                        Volume->DiskKind = DISK_KIND_OPTICAL;
                    }
                }
                else {
                    Volume->WholeDiskBlockIO = NULL;
                    //CheckError (Status, L"from HandleProtocol");
                }
            } //else
            //CheckError (Status, L"from LocateDevicePath");
        }

        DevicePath = NextDevicePath;
    } // while

    if (!Bootable) {
        if (Volume->HasBootCode) {
            #if REFIT_DEBUG > 0
            MsgLog ("  Volume Considered Non-Bootable, but Boot Code is Present\n");
            #endif
        }
        Volume->HasBootCode = FALSE;
    }

    // open the root directory of the volume
    Volume->RootDir = LibOpenRoot (Volume->DeviceHandle);

    Volume->VolName = GetVolumeName (Volume);

    if (Volume->RootDir == NULL) {
        Volume->IsReadable = FALSE;
        return;
    }
    else {
        Volume->IsReadable = TRUE;
        if ((GlobalConfig.LegacyType == LEGACY_TYPE_MAC) &&
            (Volume->FSType == FS_TYPE_NTFS) &&
            Volume->HasBootCode
        ) {
            // VBR boot code found on NTFS, but volume is not actually bootable
            // unless there are actual boot file, so check for them....
            Volume->HasBootCode = HasWindowsBiosBootFiles (Volume);
        }
    } // if/else
} // ScanVolume()

static
VOID
ScanExtendedPartition (
    REFIT_VOLUME *WholeDiskVolume,
    MBR_PARTITION_INFO *MbrEntry
) {
    EFI_STATUS          Status;
    REFIT_VOLUME        *Volume;
    UINT32              ExtBase;
    UINT32              ExtCurrent;
    UINT32              NextExtCurrent;
    UINTN               i;
    UINTN               LogicalPartitionIndex = 4;
    UINT8               SectorBuffer[512];
    BOOLEAN             Bootable;
    MBR_PARTITION_INFO  *EMbrTable;

    ExtBase = MbrEntry->StartLBA;

    for (ExtCurrent = ExtBase; ExtCurrent; ExtCurrent = NextExtCurrent) {
        // read current EMBR
        Status = refit_call5_wrapper(
            WholeDiskVolume->BlockIO->ReadBlocks,
            WholeDiskVolume->BlockIO,
            WholeDiskVolume->BlockIO->Media->MediaId,
            ExtCurrent,
            512,
            SectorBuffer
        );
        if (EFI_ERROR (Status)) {
            break;
        }
        if (*((UINT16 *) (SectorBuffer + 510)) != 0xaa55) {
            break;
        }
        EMbrTable = (MBR_PARTITION_INFO *) (SectorBuffer + 446);

        // scan logical partitions in this EMBR
        NextExtCurrent = 0;
        for (i = 0; i < 4; i++) {
            if ((EMbrTable[i].Flags != 0x00 && EMbrTable[i].Flags != 0x80) ||
                EMbrTable[i].StartLBA == 0 || EMbrTable[i].Size == 0
            ) {
                break;
            }
            if (IS_EXTENDED_PART_TYPE (EMbrTable[i].Type)) {
                // set next ExtCurrent
                NextExtCurrent = ExtBase + EMbrTable[i].StartLBA;
                break;
            }
            else {
                // found a logical partition
                Volume = AllocateZeroPool (sizeof (REFIT_VOLUME));
                Volume->DiskKind = WholeDiskVolume->DiskKind;
                Volume->IsMbrPartition = TRUE;
                Volume->MbrPartitionIndex = LogicalPartitionIndex++;
                Volume->VolName = AllocateZeroPool (256 * sizeof (UINT16));
                SPrint (
                    Volume->VolName,
                    255,
                    L"Partition %d",
                    Volume->MbrPartitionIndex + 1
                );
                Volume->BlockIO = WholeDiskVolume->BlockIO;
                Volume->BlockIOOffset = ExtCurrent + EMbrTable[i].StartLBA;
                Volume->WholeDiskBlockIO = WholeDiskVolume->BlockIO;

                Bootable = FALSE;
                ScanVolumeBootcode (Volume, &Bootable);
                if (!Bootable) {
                    Volume->HasBootCode = FALSE;
                }
                SetVolumeBadgeIcon (Volume);
                AddListElement ((VOID ***) &Volumes, &VolumesCount, Volume);
            } // if/else
        } // for
    } // for
} /* VOID ScanExtendedPartition() */

VOID
ScanVolumes (
    VOID
) {
    EFI_STATUS         Status;
    EFI_HANDLE         *Handles;
    REFIT_VOLUME       *Volume, *WholeDiskVolume;
    MBR_PARTITION_INFO *MbrTable;
    UINTN              HandleCount = 0;
    UINTN              HandleIndex;
    UINTN              VolumeIndex, VolumeIndex2;
    UINTN              PartitionIndex;
    UINTN              SectorSum, i;
    UINT8              *SectorBuffer1, *SectorBuffer2;
    EFI_GUID           *UuidList;
    EFI_GUID           NullUuid = NULL_GUID_VALUE;
    const CHAR16       *ShowScreenStr = NULL;

    MyFreePool (Volumes);
    Volumes = NULL;
    VolumesCount = 0;
    ForgetPartitionTables();

    // get all filesystem handles
    Status = LibLocateHandle (
        ByProtocol,
        &BlockIoProtocol,
        NULL,
        &HandleCount,
        &Handles
    );
    if (Status == EFI_NOT_FOUND) {
        return;  // no filesystems. strange, but true...
    }
    if (CheckError (Status, L"Found While Listing All File Systems")) {
        return;
    }
    UuidList = AllocateZeroPool (sizeof (EFI_GUID) * HandleCount);

    // first pass: collect information about all handles
    ScannedOnce = FALSE;
    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
        Volume = AllocateZeroPool (sizeof (REFIT_VOLUME));
        Volume->DeviceHandle = Handles[HandleIndex];
        AddPartitionTable (Volume);
        ScanVolume (Volume);
        if (UuidList) {
           UuidList[HandleIndex] = Volume->VolUuid;
           for (i = 0; i < HandleIndex; i++) {
              if ((CompareMem (&(Volume->VolUuid), &(UuidList[i]), sizeof (EFI_GUID)) == 0) &&
                  (CompareMem (&(Volume->VolUuid), &NullUuid, sizeof (EFI_GUID)) != 0)
              ) {
                  // Duplicate filesystem UUID
                  Volume->IsReadable = FALSE;
              } // if
           } // for
        } // if

        AddListElement ((VOID ***) &Volumes, &VolumesCount, Volume);

        if (Volume->DeviceHandle == SelfLoadedImage->DeviceHandle) {
            SelfVolSet = TRUE;
            SelfVolume = Volume;
        }

        #if REFIT_DEBUG > 0
        if (SelfVolRun) {
            if (ScannedOnce) {
                MsgLog ("\n");
            }
            CHAR16 *VolDesc = Volume->VolName;
            if (MyStrStr (VolDesc, L"whole disk Volume") != NULL) {
                VolDesc = L"Whole Disk Volume";
            }
            else if (MyStrStr (VolDesc, L"Unknown Volume") != NULL) {
                VolDesc = L"Unknown Volume";
            }
            else if (MyStrStr (VolDesc, L"HFS+ Volume") != NULL) {
                VolDesc = L"HFS+ Volume";
            }
            else if (MyStrStr (VolDesc, L"NTFS Volume") != NULL) {
                VolDesc = L"NTFS Volume";
            }
            else if (MyStrStr (VolDesc, L"FAT Volume") != NULL) {
                VolDesc = L"FAT Volume";
            }
            else if (MyStrStr (VolDesc, L"ext2 Volume") != NULL) {
                VolDesc = L"Ext2 Volume";
            }
            else if (MyStrStr (VolDesc, L"ext3 Volume") != NULL) {
                VolDesc = L"Ext3 Volume";
            }
            else if (MyStrStr (VolDesc, L"ext4 Volume") != NULL) {
                VolDesc = L"Ext4 Volume";
            }
            else if (MyStrStr (VolDesc, L"ReiserFS Volume") != NULL) {
                VolDesc = L"ReiserFS Volume";
            }
            else if (MyStrStr (VolDesc, L"Btrfs Volume") != NULL) {
                VolDesc = L"BTRFS Volume";
            }
            else if (MyStrStr (VolDesc, L"XFS Volume") != NULL) {
                VolDesc = L"XFS Volume";
            }
            else if (MyStrStr (VolDesc, L"ISO-9660 Volume") != NULL) {
                VolDesc = L"ISO-9660 Volume";
            }

            MsgLog ("Add to Collection:- '%s'", VolDesc);
            ScannedOnce = TRUE;
        }
        #endif
    } // for: first pass
    MyFreePool (UuidList);
    MyFreePool (Handles);

    if (!SelfVolSet) {
        SwitchToText (FALSE);

        ShowScreenStr = L"** WARN: Could not Set Volume";

        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
        PrintUglyText ((CHAR16 *) ShowScreenStr, NEXTLINE);
        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

        #if REFIT_DEBUG > 0
        MsgLog ("%s\n\n", ShowScreenStr);
        #endif

        PauseForKey();
        SwitchToGraphics();
    }
    else if (!SelfVolRun) {
        #if REFIT_DEBUG > 0
        MsgLog ("INFO: Self Volume:- '%s'\n\n", SelfVolume->VolName);
        #endif
    }
    else {
        #if REFIT_DEBUG > 0
        MsgLog ("\n\n");
        #endif
    }
    SelfVolRun = TRUE;

    // second pass: relate partitions and whole disk devices
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        Volume = Volumes[VolumeIndex];
        // check MBR partition table for extended partitions
        if (Volume->BlockIO != NULL && Volume->WholeDiskBlockIO != NULL &&
            Volume->BlockIO == Volume->WholeDiskBlockIO && Volume->BlockIOOffset == 0 &&
            Volume->MbrPartitionTable != NULL) {
            MbrTable = Volume->MbrPartitionTable;
            for (PartitionIndex = 0; PartitionIndex < 4; PartitionIndex++) {
                if (IS_EXTENDED_PART_TYPE (MbrTable[PartitionIndex].Type)) {
                   ScanExtendedPartition (Volume, MbrTable + PartitionIndex);
                }
            }
        }

        // search for corresponding whole disk volume entry
        WholeDiskVolume = NULL;
        if (Volume->BlockIO != NULL && Volume->WholeDiskBlockIO != NULL &&
            Volume->BlockIO != Volume->WholeDiskBlockIO) {
            for (VolumeIndex2 = 0; VolumeIndex2 < VolumesCount; VolumeIndex2++) {
                if (Volumes[VolumeIndex2]->BlockIO == Volume->WholeDiskBlockIO &&
                    Volumes[VolumeIndex2]->BlockIOOffset == 0) {
                    WholeDiskVolume = Volumes[VolumeIndex2];
                }
            }
        }

        if (WholeDiskVolume != NULL && WholeDiskVolume->MbrPartitionTable != NULL) {
            // check if this volume is one of the partitions in the table
            MbrTable = WholeDiskVolume->MbrPartitionTable;
            SectorBuffer1 = AllocatePool (512);
            SectorBuffer2 = AllocatePool (512);
            for (PartitionIndex = 0; PartitionIndex < 4; PartitionIndex++) {
                // check size
                if ((UINT64)(MbrTable[PartitionIndex].Size) !=
                    Volume->BlockIO->Media->LastBlock + 1
                ) {
                    continue;
                }

                // compare boot sector read through offset vs. directly
                Status = refit_call5_wrapper(
                    Volume->BlockIO->ReadBlocks,
                    Volume->BlockIO,
                    Volume->BlockIO->Media->MediaId,
                    Volume->BlockIOOffset,
                    512,
                    SectorBuffer1
                );
                if (EFI_ERROR (Status)) {
                    break;
                }
                Status = refit_call5_wrapper(
                    Volume->WholeDiskBlockIO->ReadBlocks,
                    Volume->WholeDiskBlockIO,
                    Volume->WholeDiskBlockIO->Media->MediaId,
                    MbrTable[PartitionIndex].StartLBA,
                    512,
                    SectorBuffer2
                );
                if (EFI_ERROR (Status)) {
                    break;
                }
                if (CompareMem (SectorBuffer1, SectorBuffer2, 512) != 0) {
                    continue;
                }

                SectorSum = 0;
                for (i = 0; i < 512; i++) {
                    SectorSum += SectorBuffer1[i];
                }
                if (SectorSum < 1000) {
                    continue;
                }

                // TODO: mark entry as non-bootable if it is an extended partition

                // now we're reasonably sure the association is correct...
                Volume->IsMbrPartition = TRUE;
                Volume->MbrPartitionIndex = PartitionIndex;
                if (Volume->VolName == NULL) {
                    Volume->VolName = AllocateZeroPool (sizeof (CHAR16) * 256);
                    SPrint (Volume->VolName, 255, L"Partition %d", PartitionIndex + 1);
                }
                break;
            }

            MyFreePool (SectorBuffer1);
            MyFreePool (SectorBuffer2);
        }
    } // for
} /* VOID ScanVolumes() */

VOID
SetVolumeIcons (
    VOID
) {
    UINTN        i;
    UINTN        k;
    UINTN        LogVal;
    REFIT_VOLUME *Volume;

    for (i = 0; i < VolumesCount; i++) {
        k = i + 1;
        // Limit logged value to 99
        if (k > 999) {
            LogVal = 999;
        }
        else {
            LogVal = k;
        }

        Volume = Volumes[i];

        // Set volume icon based on .VolumeBadge icon or disk kind
        SetVolumeBadgeIcon (Volume);
        if (Volumes[i]->DiskKind == DISK_KIND_INTERNAL) {
            // get custom volume icons if present
            if (!Volume->VolIconImage) {
                Volume->VolIconImage = egLoadIconAnyType (
                    Volume->RootDir,
                    L"",
                    L".VolumeIcon",
                    GlobalConfig.IconSizes[ICON_SIZE_BIG]
                );
            }
        }
    } // for
} // VOID SetVolumeIcons()

//
// file and dir functions
//

BOOLEAN
FileExists (
    IN EFI_FILE *BaseDir,
    IN CHAR16   *RelativePath
) {
    EFI_STATUS      Status;
    EFI_FILE_HANDLE TestFile;

    if (BaseDir != NULL) {
        Status = refit_call5_wrapper(
            BaseDir->Open, BaseDir,
            &TestFile,
            RelativePath,
            EFI_FILE_MODE_READ,
            0
        );
        if (Status == EFI_SUCCESS) {
            refit_call1_wrapper(TestFile->Close, TestFile);
            return TRUE;
        }
    }
    return FALSE;
}

EFI_STATUS
DirNextEntry (
    IN EFI_FILE *Directory,
    IN OUT EFI_FILE_INFO **DirEntry,
    IN UINTN FilterMode
) {
    EFI_STATUS Status;
    VOID       *Buffer;
    UINTN      LastBufferSize;
    UINTN      BufferSize;
    INTN       IterCount;

    for (;;) {
        // free pointer from last call
        MyFreePool (*DirEntry);
        *DirEntry = NULL;

        // read next directory entry
        LastBufferSize = BufferSize = 256;
        Buffer = AllocatePool (BufferSize);

        for (IterCount = 0; ; IterCount++) {
            Status = refit_call3_wrapper(
                Directory->Read, Directory,
                &BufferSize,
                Buffer
            );

            if (Status != EFI_BUFFER_TOO_SMALL || IterCount >= 4) {
                break;
            }

            if (BufferSize <= LastBufferSize) {
                BufferSize = LastBufferSize * 2;
            }
            Buffer = EfiReallocatePool (Buffer, LastBufferSize, BufferSize);
            LastBufferSize = BufferSize;
        }

        if (EFI_ERROR (Status)) {
            MyFreePool (Buffer);
            Buffer = NULL;
            break;
        }

        // check for end of listing

        if (BufferSize == 0) {
            // end of directory listing
            MyFreePool (Buffer);
            Buffer = NULL;
            break;
        }

        // entry is ready to be returned
        *DirEntry = (EFI_FILE_INFO *)Buffer;

        // filter results
        if (FilterMode == 1) {
            // only return directories
            if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY)) {
                break;
            }
        }
        else if (FilterMode == 2) {
            // only return files
            if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY) == 0) {
                break;
            }
        }
        else {
            // no filter or unknown filter -> return everything
            break;
        }
    }

    return Status;
}

VOID
DirIterOpen (
    IN EFI_FILE *BaseDir,
    IN CHAR16 *RelativePath OPTIONAL,
    OUT REFIT_DIR_ITER *DirIter
) {
    if (RelativePath == NULL) {
        DirIter->LastStatus = EFI_SUCCESS;
        DirIter->DirHandle = BaseDir;
        DirIter->CloseDirHandle = FALSE;
    }
    else {
        DirIter->LastStatus = refit_call5_wrapper(
            BaseDir->Open,
            BaseDir,
            &(DirIter->DirHandle),
            RelativePath,
            EFI_FILE_MODE_READ,
            0
        );
        DirIter->CloseDirHandle = EFI_ERROR (DirIter->LastStatus) ? FALSE : TRUE;
    }
    DirIter->LastFileInfo = NULL;
}

#ifndef __MAKEWITH_GNUEFI
EFI_UNICODE_COLLATION_PROTOCOL *mUnicodeCollation = NULL;

static
EFI_STATUS
InitializeUnicodeCollationProtocol (
    VOID
) {
   EFI_STATUS  Status;

   if (mUnicodeCollation != NULL) {
      return EFI_SUCCESS;
   }

   //
   // BUGBUG: Proper impelmentation is to locate all Unicode Collation Protocol
   // instances first and then select one which support English language.
   // Current implementation just pick the first instance.
   //
   Status = refit_call3_wrapper(
       gBS->LocateProtocol,
       &gEfiUnicodeCollation2ProtocolGuid,
       NULL,
       (VOID **) &mUnicodeCollation
   );
   if (EFI_ERROR (Status)) {
       Status = refit_call3_wrapper(
           gBS->LocateProtocol,
           &gEfiUnicodeCollationProtocolGuid,
           NULL,
           (VOID **) &mUnicodeCollation
       );
   }

   return Status;
}

static
BOOLEAN
MetaiMatch (
    IN CHAR16 *String,
    IN CHAR16 *Pattern
) {
   if (!mUnicodeCollation) {
      InitializeUnicodeCollationProtocol();
   }
   if (mUnicodeCollation) {
       return mUnicodeCollation->MetaiMatch (mUnicodeCollation, String, Pattern);
   }

   return FALSE; // Should not happen
}

#endif

BOOLEAN
DirIterNext (
    IN OUT REFIT_DIR_ITER *DirIter,
    IN UINTN FilterMode,
    IN CHAR16 *FilePattern OPTIONAL,
    OUT EFI_FILE_INFO **DirEntry
) {
    BOOLEAN KeepGoing = TRUE;
    UINTN   i;
    CHAR16  *OnePattern;

    MyFreePool (DirIter->LastFileInfo);
    DirIter->LastFileInfo = NULL;

    if (EFI_ERROR (DirIter->LastStatus)) {
        return FALSE;   // stop iteration
    }

    do {
        DirIter->LastStatus = DirNextEntry (
            DirIter->DirHandle,
            &(DirIter->LastFileInfo),
            FilterMode
        );
        if (EFI_ERROR (DirIter->LastStatus)) {
            return FALSE;
        }
        if (DirIter->LastFileInfo == NULL)  {
            // end of listing
            return FALSE;
        }
        if (FilePattern != NULL) {
            if ((DirIter->LastFileInfo->Attribute & EFI_FILE_DIRECTORY)) {
                KeepGoing = FALSE;
            }

            i = 0;
            while (KeepGoing && (OnePattern = FindCommaDelimited (FilePattern, i++)) != NULL) {
               if (MetaiMatch (DirIter->LastFileInfo->FileName, OnePattern)) {
                   KeepGoing = FALSE;
               }
               MyFreePool (OnePattern);
            } // while
            // else continue loop
        }
        else {
            break;
        }
   } while (KeepGoing && FilePattern);

    *DirEntry = DirIter->LastFileInfo;
    return TRUE;
}

EFI_STATUS
DirIterClose (
    IN OUT REFIT_DIR_ITER *DirIter
) {
    MyFreePool (DirIter->LastFileInfo);
    DirIter->LastFileInfo = NULL;
    if ((DirIter->CloseDirHandle) && (DirIter->DirHandle->Close)) {
        refit_call1_wrapper(DirIter->DirHandle->Close, DirIter->DirHandle);
    }

    return DirIter->LastStatus;
}

//
// file name manipulation
//

// Returns the filename portion (minus path name) of the
// specified file
CHAR16 *
Basename (
    IN CHAR16 *Path
) {
    CHAR16  *FileName;
    UINTN   i;

    FileName = Path;

    if (Path != NULL) {
        for (i = StrLen (Path); i > 0; i--) {
            if (Path[i-1] == '\\' || Path[i-1] == '/') {
                FileName = Path + i;
                break;
            }
        }
    }

    return FileName;
}

// Remove the .efi extension from FileName -- for instance, if FileName is
// "fred.efi", returns "fred". If the filename contains no .efi extension,
// returns a copy of the original input.
CHAR16 *
StripEfiExtension (
    IN CHAR16 *FileName
) {
    UINTN  Length;
    CHAR16 *Copy = NULL;

    if ((FileName != NULL) && ((Copy = StrDuplicate (FileName)) != NULL)) {
        Length = StrLen (Copy);
        if ((Length >= 4) && MyStriCmp (&Copy[Length - 4], L".efi")) {
            Copy[Length - 4] = 0;
        } // if
    } // if

    return Copy;
} // CHAR16 *StripExtension()

//
// memory string search
//

INTN
FindMem (
    IN VOID *Buffer,
    IN UINTN BufferLength,
    IN VOID *SearchString,
    IN UINTN SearchStringLength
) {
    UINT8 *BufferPtr;
    UINTN Offset;

    BufferPtr = Buffer;
    BufferLength -= SearchStringLength;
    for (Offset = 0; Offset < BufferLength; Offset++, BufferPtr++) {
        if (CompareMem (BufferPtr, SearchString, SearchStringLength) == 0) {
            return (INTN)Offset;
        }
    }

    return -1;
}

// Takes an input pathname (*Path) and returns the part of the filename from
// the final dot onwards, converted to lowercase. If the filename includes
// no dots, or if the input is NULL, returns an empty (but allocated) string.
// The calling function is responsible for freeing the memory associated with
// the return value.
CHAR16 *
FindExtension (
    IN CHAR16 *Path
) {
    CHAR16     *Extension;
    BOOLEAN    Found = FALSE, FoundSlash = FALSE;
    INTN       i;

    Extension = AllocateZeroPool (sizeof (CHAR16));
    if (Path) {
        i = StrLen (Path);
        while ((!Found) && (!FoundSlash) && (i >= 0)) {
            if (Path[i] == L'.') {
                Found = TRUE;
            }
            else if ((Path[i] == L'/') || (Path[i] == L'\\')) {
                FoundSlash = TRUE;
            }
            if (!Found) {
                i--;
            }
        } // while
        if (Found) {
            MergeStrings (&Extension, &Path[i], 0);
            ToLower (Extension);
        } // if (Found)
    } // if

    return (Extension);
} // CHAR16 *FindExtension()

// Takes an input pathname (*Path) and locates the final directory component
// of that name. For instance, if the input path is 'EFI\foo\bar.efi', this
// function returns the string 'foo'.
// Assumes the pathname is separated with backslashes.
CHAR16 *
FindLastDirName (
    IN CHAR16 *Path
) {
    UINTN i;
    UINTN PathLength;
    UINTN CopyLength;
    UINTN EndOfElement   = 0;
    UINTN StartOfElement = 0;
    CHAR16 *Found        = NULL;

    if (Path == NULL) {
        return NULL;
    }

    PathLength = StrLen (Path);
    // Find start & end of target element
    for (i = 0; i < PathLength; i++) {
        if (Path[i] == '\\') {
            StartOfElement = EndOfElement;
            EndOfElement   = i;
        } // if
    } // for
    // Extract the target element
    if (EndOfElement > 0) {
        while ((StartOfElement < PathLength) && (Path[StartOfElement] == '\\')) {
            StartOfElement++;
        } // while

        EndOfElement--;
        if (EndOfElement >= StartOfElement) {
            CopyLength = EndOfElement - StartOfElement + 1;
            Found = StrDuplicate (&Path[StartOfElement]);
            if (Found != NULL)
                Found[CopyLength] = 0;
        } // if (EndOfElement >= StartOfElement)
    } // if (EndOfElement > 0)

    return (Found);
} // CHAR16 *FindLastDirName()

// Returns the directory portion of a pathname. For instance,
// if FullPath is 'EFI\foo\bar.efi', this function returns the
// string 'EFI\foo'. The calling function is responsible for
// freeing the returned string's memory.
CHAR16 *
FindPath (
    IN CHAR16* FullPath
) {
   UINTN i, LastBackslash = 0;
   CHAR16 *PathOnly = NULL;

   if (FullPath != NULL) {
      for (i = 0; i < StrLen (FullPath); i++) {
         if (FullPath[i] == '\\') {
             LastBackslash = i;
         }
      } // for
      PathOnly = StrDuplicate (FullPath);
      if (PathOnly != NULL) {
          PathOnly[LastBackslash] = 0;
      }
   } // if

   return (PathOnly);
}

// Takes an input loadpath, splits it into disk and filename components, finds a matching
// DeviceVolume, and returns that and the filename (*loader).
VOID
FindVolumeAndFilename (
    IN EFI_DEVICE_PATH *loadpath,
    OUT REFIT_VOLUME **DeviceVolume,
    OUT CHAR16 **loader
) {
    CHAR16 *DeviceString, *VolumeDeviceString, *Temp;
    UINTN i = 0;
    BOOLEAN Found = FALSE;

    if (!loadpath || !DeviceVolume || !loader) {
        return;
    }

    MyFreePool (*loader);
    MyFreePool (*DeviceVolume);
    *DeviceVolume = NULL;
    DeviceString = DevicePathToStr (loadpath);
    *loader = SplitDeviceString (DeviceString);

    while ((i < VolumesCount) && (!Found)) {
        if (Volumes[i]->DevicePath == NULL) {
            i++;
            continue;
        }
        VolumeDeviceString = DevicePathToStr (Volumes[i]->DevicePath);
        Temp = SplitDeviceString (VolumeDeviceString);
        if (MyStrStr (VolumeDeviceString, DeviceString)) {
            Found = TRUE;
            *DeviceVolume = Volumes[i];
        }
        MyFreePool (Temp);
        MyFreePool (VolumeDeviceString);
        i++;
    } // while

    MyFreePool (DeviceString);
} // VOID FindVolumeAndFilename()

// Splits a volume/filename string (e.g., "fs0:\EFI\BOOT") into separate
// volume and filename components (e.g., "fs0" and "\EFI\BOOT"), returning
// the filename component in the original *Path variable and the split-off
// volume component in the *VolName variable.
// Returns TRUE if both components are found, FALSE otherwise.
BOOLEAN
SplitVolumeAndFilename (
    IN OUT CHAR16 **Path,
    OUT CHAR16 **VolName
) {
    UINTN i = 0, Length;
    CHAR16 *Filename;

    if (*Path == NULL) {
        return FALSE;
    }

    MyFreePool (*VolName);
    *VolName = NULL;

    Length = StrLen (*Path);
    while ((i < Length) && ((*Path)[i] != L':')) {
        i++;
    } // while

    if (i < Length) {
        Filename = StrDuplicate ((*Path) + i + 1);
        (*Path)[i] = 0;
        *VolName = *Path;
        *Path = Filename;
        return TRUE;
    }
    else {
        return FALSE;
    }
} // BOOLEAN SplitVolumeAndFilename()

// Take an input path name, which may include a volume specification and/or
// a path, and return separate volume, path, and file names. For instance,
// "BIGVOL:\EFI\ubuntu\grubx64.efi" will return a VolName of "BIGVOL", a Path
// of "EFI\ubuntu", and a Filename of "grubx64.efi". If an element is missing,
// the returned pointer is NULL. The calling function is responsible for
// freeing the allocated memory.
VOID
SplitPathName (
    CHAR16 *InPath,
    CHAR16 **VolName,
    CHAR16 **Path,
    CHAR16 **Filename
) {
    CHAR16 *Temp = NULL;

    MyFreePool (*VolName);
    MyFreePool (*Path);
    MyFreePool (*Filename);
    *VolName = *Path = *Filename = NULL;

    Temp = StrDuplicate (InPath);
    SplitVolumeAndFilename (&Temp, VolName); // VolName is NULL or has volume; Temp has rest of path
    CleanUpPathNameSlashes (Temp);
    *Path = FindPath (Temp); // *Path has path (may be 0-length); Temp unchanged.
    *Filename = StrDuplicate (Temp + StrLen (*Path));
    CleanUpPathNameSlashes (*Filename);
    if (StrLen (*Path) == 0) {
        FreePool (*Path);
        *Path = NULL;
    }
    if (StrLen (*Filename) == 0) {
        FreePool (*Filename);
        *Filename = NULL;
    }
    MyFreePool (Temp);
} // VOID SplitPathName()

// Finds a volume with the specified Identifier (a filesystem label, a
// partition name, or a partition GUID). If found, sets *Volume to point
// to that volume. If not, leaves it unchanged.
// Returns TRUE if a match was found, FALSE if not.
BOOLEAN
FindVolume (
    REFIT_VOLUME **Volume,
    CHAR16 *Identifier
) {
    UINTN     i = 0;
    BOOLEAN   Found = FALSE;

    while ((i < VolumesCount) && (!Found)) {
        if (VolumeMatchesDescription (Volumes[i], Identifier)) {
            *Volume = Volumes[i];
            Found = TRUE;
        } // if
        i++;
    } // while()

    return (Found);
} // static VOID FindVolume()

// Returns TRUE if Description matches Volume's VolName, PartName, or (once
// transformed) PartGuid fields, FALSE otherwise (or if either pointer is NULL)
BOOLEAN
VolumeMatchesDescription (
    REFIT_VOLUME *Volume,
    CHAR16 *Description
) {
    EFI_GUID TargetVolGuid = NULL_GUID_VALUE;

    if ((Volume == NULL) || (Description == NULL)) {
        return FALSE;
    }
    if (IsGuid (Description)) {
        TargetVolGuid = StringAsGuid (Description);
        return GuidsAreEqual (&TargetVolGuid, &(Volume->PartGuid));
    }
    else {
        return (MyStriCmp (Description, Volume->VolName) || MyStriCmp (Description, Volume->PartName));
    }
} // BOOLEAN VolumeMatchesDescription()

// Returns TRUE if specified Volume, Directory, and Filename correspond to an
// element in the comma-delimited List, FALSE otherwise. Note that Directory and
// Filename must *NOT* include a volume or path specification (that's part of
// the Volume variable), but the List elements may. Performs comparison
// case-insensitively.
BOOLEAN
FilenameIn (
    REFIT_VOLUME *Volume,
    CHAR16 *Directory,
    CHAR16 *Filename,
    CHAR16 *List
) {

    CHAR16    *OneElement;
    CHAR16    *TargetVolName   = NULL;
    CHAR16    *TargetPath      = NULL;
    CHAR16    *TargetFilename  = NULL;
    UINTN     i                = 0;
    BOOLEAN   Found            = FALSE;

    if (Filename && List) {
        while (!Found && (OneElement = FindCommaDelimited (List, i++))) {
            Found = TRUE;
            SplitPathName (OneElement, &TargetVolName, &TargetPath, &TargetFilename);
            if (((TargetVolName != NULL) &&
                (!VolumeMatchesDescription (Volume, TargetVolName))) ||
                ((TargetPath != NULL) && (!MyStriCmp (TargetPath, Directory))) ||
                ((TargetFilename != NULL) && (!MyStriCmp (TargetFilename, Filename)))
            ) {
                Found = FALSE;
            } // if
            MyFreePool (OneElement);
        } // while
        MyFreePool (TargetVolName);
        MyFreePool (TargetPath);
        MyFreePool (TargetFilename);
    } // if

    return Found;
} // BOOLEAN FilenameIn()

// Implement FreePool the way it should have been done to begin with, so that
// it doesn't throw an ASSERT message if fed a NULL pointer....
VOID MyFreePool (IN VOID *Pointer) {
    if (Pointer != NULL) {
        FreePool (Pointer);
    }
}

static EFI_GUID AppleRemovableMediaGuid = APPLE_REMOVABLE_MEDIA_PROTOCOL_GUID;

// Eject all removable media.
// Returns TRUE if any media were ejected, FALSE otherwise.
BOOLEAN
EjectMedia (
    VOID
) {
    EFI_STATUS                      Status;
    UINTN                           HandleIndex;
    UINTN                           HandleCount = 0;
    UINTN                           Ejected     = 0;
    EFI_HANDLE                      *Handles;
    EFI_HANDLE                      Handle;
    APPLE_REMOVABLE_MEDIA_PROTOCOL  *Ejectable;

    Status = LibLocateHandle (
        ByProtocol,
        &AppleRemovableMediaGuid,
        NULL,
        &HandleCount,
        &Handles
    );
    if (EFI_ERROR (Status) || HandleCount == 0) {
        return (FALSE); // probably not an Apple system
    }

    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
        Handle = Handles[HandleIndex];
        Status = refit_call3_wrapper(
            gBS->HandleProtocol,
            Handle,
            &AppleRemovableMediaGuid,
            (VOID **) &Ejectable
        );
        if (EFI_ERROR (Status)) {
            continue;
        }
        Status = refit_call1_wrapper(Ejectable->Eject, Ejectable);
        if (!EFI_ERROR (Status)) {
            Ejected++;
        }
    }
    MyFreePool (Handles);

    return (Ejected > 0);
} // VOID EjectMedia()

// Returns TRUE if the two GUIDs are equal, FALSE otherwise
BOOLEAN
GuidsAreEqual (
    EFI_GUID *Guid1,
    EFI_GUID *Guid2
) {
    return (CompareMem (Guid1, Guid2, 16) == 0);
} // BOOLEAN GuidsAreEqual()

// Erase linked-list of UINT32 values....
VOID
EraseUint32List (
    UINT32_LIST **TheList
) {
    UINT32_LIST *NextItem;

    while (*TheList) {
        NextItem = (*TheList)->Next;
        MyFreePool (*TheList);
        *TheList = NextItem;
    } // while
} // EraseUin32List()
