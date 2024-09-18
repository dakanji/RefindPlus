/*
 * BootMaster/lib.c
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
 * Modifications for rEFInd Copyright (c) 2012-2021 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2024 Dayo Akanji (sf.net/u/dakanji/profile)
 * Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "icns.h"
#include "gpt.h"
#include "lib.h"
#include "scan.h"
#include "apple.h"
#include "config.h"
#include "screenmgt.h"
#include "mystrings.h"
#include "../include/RemovableMedia.h"
#include "../include/refit_call_wrapper.h"

#ifdef __MAKEWITH_GNUEFI
#define EfiReallocatePool ReallocatePool
#else
#define LibLocateHandle gBS->LocateHandleBuffer
#define DevicePathProtocol gEfiDevicePathProtocolGuid
#define BlockIoProtocol gEfiBlockIoProtocolGuid
#define LibFileSystemInfo EfiLibFileSystemInfo
#define LibOpenRoot EfiLibOpenRoot
EFI_DEVICE_PATH_PROTOCOL EndDevicePath[] = {
   {END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, {END_DEVICE_PATH_LENGTH, 0}}
};
#endif

// "Magic" Signatures for Various Filesystems
#define FAT_MAGIC                        0xAA55
#define EXT2_SUPER_MAGIC                 0xEF53
#define HFSPLUS_MAGIC0                   0x4442     /* 'BD' in ASCII for HFS  */
#define HFSPLUS_MAGIC1                   0x2B48     /* 'H+' in ASCII for HFS+ */
#define HFSPLUS_MAGIC2                   0x5848     /* 'HX' in ASCII for HFSX */
#define REISERFS_SUPER_MAGIC_STRING      "ReIsErFs"
#define REISER2FS_SUPER_MAGIC_STRING     "ReIsEr2Fs"
#define REISER2FS_JR_SUPER_MAGIC_STRING  "ReIsEr3Fs"
#define BTRFS_SIGNATURE                  "_BHRfS_M"
#define XFS_SIGNATURE                    "XFSB"
#define JFS_SIGNATURE                    "JFS1"
#define NTFS_SIGNATURE                   "NTFS    "
#define FAT12_SIGNATURE                  "FAT12   "
#define FAT16_SIGNATURE                  "FAT16   "
#define FAT32_SIGNATURE                  "FAT32   "

#if defined (EFIX64)
EFI_GUID gFreedesktopRootGuid = {0x4f68bce3, 0xe8cd, 0x4db1, {0x96, 0xe7, 0xfb, 0xca, 0xf9, 0x84, 0xb7, 0x09}};
#elif defined (EFI32)
EFI_GUID gFreedesktopRootGuid = {0x44479540, 0xf297, 0x41b2, {0x9a, 0xf7, 0xd1, 0x31, 0xd5, 0xf0, 0x45, 0x8a}};
#elif defined (EFIAARCH64)
EFI_GUID gFreedesktopRootGuid = {0xb921b045, 0x1df0, 0x41c3, {0xaf, 0x44, 0x4c, 0x6f, 0x28, 0x0d, 0x3f, 0xae}};
#else //  GUID for ARM32 is Below
EFI_GUID gFreedesktopRootGuid = {0x69dad710, 0x2ce4, 0x4e3c, {0xb1, 0x6c, 0x21, 0xa1, 0xd4, 0x9a, 0xbe, 0xd3}};
#endif

// Macros
#define SET_BADGE_IMAGE(x) Volume->VolBadgeImage = BuiltinIcon (x)
#define UNINIT_VOLUMES(x, y)              \
    do {                                  \
        for (UINTN i = 0; i < y; i++) {   \
            UninitVolume (&x[i]);         \
        }                                 \
    } while (0)
#define REINIT_VOLUMES(x, y)              \
    do {                                  \
        for (UINTN i = 0; i < y; i++) {   \
            ReinitVolume (&x[i]);         \
        }                                 \
    } while (0)


// Variables
EFI_HANDLE                  SelfImageHandle         = NULL;

EFI_LOADED_IMAGE_PROTOCOL  *SelfLoadedImage         = NULL;

CHAR16                     *StrSelfUUID             = NULL;
CHAR16                     *SelfDirPath             = NULL;
CHAR16                     *SelfBaseName            = NULL;

EFI_FILE_PROTOCOL          *SelfRootDir             = NULL;
EFI_FILE_PROTOCOL          *SelfDir                 = NULL;
EFI_FILE_PROTOCOL          *gVarsDir                = NULL;

REFIT_VOLUME               *SelfVolume              = NULL;
REFIT_VOLUME              **Volumes                 = NULL;
REFIT_VOLUME              **RecoveryVolumes         = NULL;
REFIT_VOLUME              **SkipApfsVolumes         = NULL;
REFIT_VOLUME              **PreBootVolumes          = NULL;
REFIT_VOLUME              **SystemVolumes           = NULL;
REFIT_VOLUME              **DataVolumes             = NULL;
REFIT_VOLUME              **HfsRecovery             = NULL;

UINTN                       RecoveryVolumesCount    = 0;
UINTN                       SkipApfsVolumesCount    = 0;
UINTN                       PreBootVolumesCount     = 0;
UINTN                       SystemVolumesCount      = 0;
UINTN                       DataVolumesCount        = 0;
UINTN                       HfsRecoveryCount        = 0;
UINTN                       VolumesCount            = 0;

UINT64                      ReadWriteCreate         = EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE;

BOOLEAN                     FoundExternalDisk       = FALSE;
BOOLEAN                     DoneHeadings            = FALSE;
BOOLEAN                     SkipSpacing             = FALSE;
BOOLEAN                     UseButJoin              = FALSE;
BOOLEAN                     SelfVolSet              = FALSE;
BOOLEAN                     SelfVolRun              = FALSE;
BOOLEAN                     MediaCheck              = FALSE;
BOOLEAN                     SingleAPFS              =  TRUE;
BOOLEAN                     ValidAPFS               =  TRUE;
BOOLEAN                     ScanMBR                 = FALSE;

#if REFIT_DEBUG > 0
BOOLEAN                     FirstVolume             =  TRUE;
BOOLEAN                     ScannedOnce             = FALSE;
BOOLEAN                     FoundMBR                = FALSE;
#endif

EFI_GUID                    GuidESP                 =              ESP_GUID_VALUE;
EFI_GUID                    GuidHFS                 =              HFS_GUID_VALUE;
EFI_GUID                    GuidAPFS                =             APFS_GUID_VALUE;
EFI_GUID                    GuidNull                =             NULL_GUID_VALUE;
EFI_GUID                    GuidSwap                =             SWAP_GUID_VALUE;
EFI_GUID                    GuidLuks                =             LUKS_GUID_VALUE;
EFI_GUID                    GuidLinux               =            LINUX_GUID_VALUE;
EFI_GUID                    GuidBasicData           =       BASIC_DATA_GUID_VALUE;
EFI_GUID                    GuidApplTvRec           =      APPLE_TV_RECOVERY_GUID;
EFI_GUID                    GuidFlagAPFS            =      APFS_FINGER_PRINT_GUID;
EFI_GUID                    GuidMacRaidOn           =      MAC_RAID_ON_GUID_VALUE;
EFI_GUID                    GuidMacRaidOff          =     MAC_RAID_OFF_GUID_VALUE;
EFI_GUID                    GuidReservedMS          =    MSFT_RESERVED_GUID_VALUE;
EFI_GUID                    GuidWindowsRE           = WIN_RECOVERY_ENV_GUID_VALUE;
EFI_GUID                    GuidRecoveryHD          =  MAC_RECOVERY_HD_GUID_VALUE;
EFI_GUID                    GuidContainerHFS        =    CONTAINER_HFS_GUID_VALUE;


extern EFI_GUID             RefindPlusOldGuid;
extern EFI_GUID             RefindPlusGuid;
extern BOOLEAN              ScanningLoaders;

extern
UINTN OcFileDevicePathFullNameSize (
    IN const EFI_DEVICE_PATH_PROTOCOL  *DevicePath
);
extern
VOID OcFileDevicePathFullName (
    OUT CHAR16                      *PathName,
    IN  CONST FILEPATH_DEVICE_PATH  *FilePath,
    IN  UINTN                       PathNameSize
);

// Maximum size for disk sectors
#define SECTOR_SIZE 4096

// Default Buffer Size
#define BASE_SIZE    512

// Number of bytes to read from a partition to determine its filesystem type
// and identify its boot loader, and hence probable BIOS-mode OS installation
// 68 KiB -- ReiserFS superblock begins at 64 KiB
#define SAMPLE_SIZE 69632

#define PARTITION_TABLE_TXT    L"Found MBR Partition Table"
#define LEGACY_CODE_TXT        L"Found Legacy Boot Code"
#define ENTRY_BELOW_TXT        L" on Entry Below:"
#define AND_JOIN_TXT           L" and "
#define BUT_JOIN_TXT           L" but "
#define UNKNOWN_OS             L"Unknown OS"

#define NAME_FIX(Name) FindSubStr  ((*Volume)->VolName, Name)) VolumeName = Name
#define FAT_NAME(Name) MyStrBegins (Name, (*Volume)->VolName)) VolumeName = Name
#define FIX_FLAG(Name) MyStriCmp   ((*Volume)->VolName, Name)) VolumeName = Name


// DA-TAG: Stash here for later use
//         Allows getting user input
// NOTE: Currently Disabled by '#if 0'
#if 0
static
UINTN GetUserInput (
    IN  CHAR16   *prompt,
    OUT BOOLEAN  *bool_out
) {
    EFI_STATUS          Status;
    UINTN               Index;
    BOOLEAN             ErrorOut;
    EFI_INPUT_KEY       Key;


    ReadAllKeyStrokes(); // Remove buffered key strokes

    Print(prompt);

    ErrorOut = FALSE;
    do {
        REFIT_CALL_3_WRAPPER(
            gBS->WaitForEvent, 1,
            &gST->ConIn->WaitForKey, &Index
        );

        Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &Key);
        if (EFI_ERROR(Status) && Status != EFI_NOT_READY) {
            ErrorOut = TRUE;
            break;
        }
    } while (EFI_ERROR(Status));

    if (ErrorOut) {
        return 1;
    }

    if (Key.UnicodeChar == 'y' || Key.UnicodeChar == 'Y') {
        Print(L"Yes\n");
        *bool_out = TRUE;
    }
    else {
        Print(L"No\n");
        *bool_out = FALSE;
    }

    ReadAllKeyStrokes(); // Remove buffered key strokes

    return 0;
} // static UINTN GetUserInput()
#endif

static
BOOLEAN IsSystemVolume (
    IN REFIT_VOLUME *Volume
) {
    UINTN         i;
    BOOLEAN       FoundSysVol;


    FoundSysVol = FALSE;
    for (i = 0; i < SystemVolumesCount; i++) {
        if (GuidsAreEqual (
                &(Volume->VolUuid),
                &(SystemVolumes[i]->VolUuid)
            )
        ) {
            FoundSysVol = TRUE;
            break;
        }
    }

    return FoundSysVol;
} // static BOOLEAN IsSystemVolume()

static
EFI_STATUS FinishInitRefitLib (VOID) {
    EFI_STATUS  Status;

    #if REFIT_DEBUG > 0
    BOOLEAN  CheckMute = FALSE;
    #endif


    if (SelfVolume && SelfVolume->DeviceHandle != SelfLoadedImage->DeviceHandle) {
        SelfLoadedImage->DeviceHandle = SelfVolume->DeviceHandle;
    }

    if (SelfRootDir == NULL) {
        SelfRootDir = LibOpenRoot (SelfLoadedImage->DeviceHandle);
    }

    if (SelfRootDir == NULL) {
        Status = EFI_INVALID_PARAMETER;
    }
    else {
        Status = REFIT_CALL_5_WRAPPER(
            SelfRootDir->Open, SelfRootDir,
            &SelfDir, SelfDirPath,
            EFI_FILE_MODE_READ, 0
        );
        if (!EFI_ERROR(Status)) {
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_SET;
            #endif
            Status = FindVarsDir();
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_OFF;
            #endif
        }
    }

    if (EFI_ERROR(Status)) {
        CheckError (Status, L"While (Re)Opening Installation Volume/Directory");
    }

    return Status;
} // static EFI_STATUS FinishInitRefitLib()

static
VOID UninitVolume (
    IN OUT REFIT_VOLUME  **Volume
) {
    if (Volume == NULL || *Volume == NULL) {
        return;
    }

    if ((*Volume)->RootDir != NULL) {
        REFIT_CALL_1_WRAPPER((*Volume)->RootDir->Close, (*Volume)->RootDir);
        (*Volume)->RootDir = NULL;
    }

    (*Volume)->BlockIO          = NULL;
    (*Volume)->DeviceHandle     = NULL;
    (*Volume)->WholeDiskBlockIO = NULL;
} // static VOID UninitVolume()

static
VOID UninitVolumes (VOID) {
    UNINIT_VOLUMES(RecoveryVolumes, RecoveryVolumesCount);
    UNINIT_VOLUMES(SkipApfsVolumes, SkipApfsVolumesCount);
    UNINIT_VOLUMES(PreBootVolumes,  PreBootVolumesCount);
    UNINIT_VOLUMES(SystemVolumes,   SystemVolumesCount);
    UNINIT_VOLUMES(HfsRecovery,     HfsRecoveryCount);
    UNINIT_VOLUMES(DataVolumes,     DataVolumesCount);
    UNINIT_VOLUMES(Volumes,         VolumesCount);
    UninitVolume (&SelfVolume);
} // static VOID UninitVolumes()

static
VOID ReinitVolume (
    IN OUT REFIT_VOLUME  **Volume
) {
    EFI_STATUS                 Status;
    CHAR16                    *ErrStr;
    EFI_HANDLE                 DeviceHandle;
    EFI_HANDLE                 WholeDiskHandle;
    EFI_DEVICE_PATH_PROTOCOL  *RemainingDevicePath;


    if (Volume == NULL || *Volume == NULL) {
        return;
    }

    if ((*Volume)->DevicePath != NULL) {
        RemainingDevicePath = (*Volume)->DevicePath;
        Status = REFIT_CALL_3_WRAPPER(
            gBS->LocateDevicePath, &BlockIoProtocol,
            &RemainingDevicePath, &DeviceHandle
        );
        if (!EFI_ERROR(Status)) {
            (*Volume)->DeviceHandle = DeviceHandle;
            (*Volume)->RootDir = LibOpenRoot ((*Volume)->DeviceHandle);
        }
        else {
            ErrStr = PoolPrint (
                L"from LocateDevicePath for DeviceHandle in ReinitVolume:- '%s'",
                ((*Volume)->VolName != NULL) ? (*Volume)->VolName : L"Unnamed"
            );
            CheckError (Status, ErrStr);
            MY_FREE_POOL(ErrStr);
        }
    }

    if ((*Volume)->WholeDiskDevicePath != NULL) {
        RemainingDevicePath = (*Volume)->WholeDiskDevicePath;
        Status = REFIT_CALL_3_WRAPPER(
            gBS->LocateDevicePath, &BlockIoProtocol,
            &RemainingDevicePath, &WholeDiskHandle
        );
        if (!EFI_ERROR(Status)) {
            Status = REFIT_CALL_3_WRAPPER(
                gBS->HandleProtocol, WholeDiskHandle,
                &BlockIoProtocol, (VOID **) &(*Volume)->WholeDiskBlockIO
            );
            if (EFI_ERROR(Status)) {
                (*Volume)->WholeDiskBlockIO = NULL;
            }
        }
    }

    if ((*Volume)->DeviceHandle != NULL) {
        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, (*Volume)->DeviceHandle,
            &BlockIoProtocol, (VOID **) &((*Volume)->BlockIO)
        );
        if (EFI_ERROR(Status)) {
            (*Volume)->BlockIO = NULL;
        }
    }
} // static VOID ReinitVolume()


/**
 * The RefitGetBootPathName function below is derived from the OpenCore Project
 * Copyright (C) 2019, vit9696
 *
 * All rights reserved.
 *
 * This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the BSD License
 * which accompanies this distribution.  The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/
/**
 * Modified for RefindPlus
 * Copyright (c) 2024 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
**/
CHAR16 * RefitGetBootPathName (
    IN  EFI_DEVICE_PATH_PROTOCOL  *DevicePath
) {
    UINTN                            Len;
    UINTN                            PathNameSize;
    CHAR16                          *FilePathName;
    CHAR16                          *PathName;
    CHAR16                          *Slash;


    if (DevicePathType    (DevicePath) != MEDIA_DEVICE_PATH ||
        DevicePathSubType (DevicePath) != MEDIA_FILEPATH_DP
    ) {
        PathName = AllocateZeroPool (sizeof (L"\\"));
        if (PathName != NULL) {
            StrCpyS (PathName, sizeof (L"\\"), L"\\");
        }

        // Early Return ... Return Output ... Could be NULL
        return PathName;
    }

    PathNameSize = OcFileDevicePathFullNameSize (DevicePath);

    PathName = AllocateZeroPool (PathNameSize);
    if (PathName == NULL) {
        // Early Return ... Return NULL
        return NULL;
    }

    OcFileDevicePathFullName (
        PathName,
        (FILEPATH_DEVICE_PATH *) DevicePath,
        PathNameSize
    );

    Slash = MyStrStr (PathName, L"\\");

    if (Slash != NULL) {
        Len = StrLen (PathName);

        FilePathName = &PathName[Len - 1];

        while (*FilePathName != L'\\') {
            *FilePathName = L'\0';
            --FilePathName;
        }
    }

    return PathName;
} // CHAR16 * RefitGetBootPathName

// Converts forward slashes to backslashes, removes duplicate slashes, and
// removes slashes from both the start and end of the pathname.
// Necessary because some (buggy?) EFI implementations produce "\/" strings
// in pathnames, because some user inputs can produce duplicate directory
// separators, and because we want consistent start and end slashes for
// directory comparisons. A special case: If the PathName refers to root,
// but is non-empty, return "\", since some firmware implementations flake
// out if this is not present.
VOID CleanUpPathNameSlashes (
    IN OUT CHAR16 *PathName
) {
    UINTN Dest;
    UINTN Source;


    if (PathName == NULL || PathName[0] == '\0') {
        return;
    }

    Source = Dest = 0;
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
            }
        }
        else {
            // Regular character ... copy directly.
            PathName[Dest] = PathName[Source];
            Source++;
            Dest++;
        }
    } // while

    if ((Dest > 0) && (PathName[Dest - 1] == L'\\')) {
        Dest--;
    }

    PathName[Dest]   = L'\0';
    if (PathName[0] == L'\0') {
        PathName[0]  = L'\\';
        PathName[1]  = L'\0';
    }
} // VOID CleanUpPathNameSlashes()

// Splits an EFI device path into device and filename components.
// For instance, if InString is
// PciRoot(0x0)/Pci(0x1f,0x2)/Ata(ABC,XYZ,0x0)/HD(2,GPT,GUID_STR)/\bzImage-3.5.1.efi,
// this function will truncate that input to
// PciRoot(0x0)/Pci(0x1f,0x2)/Ata(ABC,XYZ,0x0)/HD(2,GPT,GUID_STR)
// and return bzImage-3.5.1.efi as its return value.
//
// It does this by searching for the last ")" character in InString,
// copying everything after that string (after some cleanup) as the return
// value, and truncating the original after that string (after some cleanup)
// as the return value, and truncating the original input value.
//
// If InString contains no ")" character, this function leaves the original
// input string unmodified and also returns that string. If InString is NULL,
// this function returns NULL.
CHAR16 * SplitDeviceString (
    IN OUT CHAR16 *InString
) {
    INTN     i;
    CHAR16  *FileName;
    BOOLEAN  Found;


    FileName = NULL;
    if (InString != NULL) {
        Found = FALSE;
        i = StrLen (InString) - 1;
        while ((i >= 0) && (!Found)) {
            if (InString[i] == L')') {
                Found    = TRUE;
                FileName = StrDuplicate (&InString[i + 1]);
                CleanUpPathNameSlashes (FileName);
                InString[i + 1] = '\0';
            }
            i--;
        } // while

        if (FileName == NULL) {
            FileName = StrDuplicate (InString);
        }
    }

    return FileName;
} // CHAR16 * SplitDeviceString()

EFI_STATUS InitRefitLib (
    IN EFI_HANDLE ImageHandle
) {
    EFI_STATUS   Status;
    CHAR16      *Temp;
    CHAR16      *DevicePathAsString;


    SelfImageHandle = ImageHandle;
    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol, SelfImageHandle,
        &LoadedImageProtocol, (VOID **) &SelfLoadedImage
    );
    if (EFI_ERROR(Status)) {
        CheckFatalError (Status, L"While Getting a LoadedImageProtocol Handle");
        return EFI_LOAD_ERROR;
    }

    // Find the current directory
    DevicePathAsString = DevicePathToStr (SelfLoadedImage->FilePath);
    GlobalConfig.SelfDevicePath = FileDevicePath (
        SelfLoadedImage->DeviceHandle,
        DevicePathAsString
    );

    CleanUpPathNameSlashes (DevicePathAsString);
    Temp = SplitDeviceString (DevicePathAsString);

    MY_FREE_POOL(SelfDirPath);
    SelfDirPath = FindPath (Temp);

    MY_FREE_POOL(SelfBaseName);
    SelfBaseName = Basename (Temp);

    MY_FREE_POOL(DevicePathAsString);
    MY_FREE_POOL(Temp);

    Status = FinishInitRefitLib();

    return Status;
} // EFI_STATUS InitRefitLib

VOID ReinitVolumes (VOID) {
    REINIT_VOLUMES(RecoveryVolumes, RecoveryVolumesCount);
    REINIT_VOLUMES(SkipApfsVolumes, SkipApfsVolumesCount);
    REINIT_VOLUMES(PreBootVolumes,  PreBootVolumesCount);
    REINIT_VOLUMES(SystemVolumes,   SystemVolumesCount);
    REINIT_VOLUMES(HfsRecovery,     HfsRecoveryCount);
    REINIT_VOLUMES(DataVolumes,     DataVolumesCount);
    REINIT_VOLUMES(Volumes,         VolumesCount);
    ReinitVolume (&SelfVolume);
} // VOID ReinitVolumes()

// Called before running external programs to close open file handles
VOID UninitRefitLib (VOID) {
    // This piece of code was made to correspond to weirdness in ReinitRefitLib().
    // See the comment on it there.
    if (SelfRootDir == SelfVolume->RootDir) {
        SelfRootDir = NULL;
    }

    UninitVolumes();

    if (SelfDir != NULL) {
        REFIT_CALL_1_WRAPPER(SelfDir->Close, SelfDir);
        SelfDir = NULL;
    }

    if (SelfRootDir != NULL) {
       REFIT_CALL_1_WRAPPER(SelfRootDir->Close, SelfRootDir);
       SelfRootDir = NULL;
    }

    if (gVarsDir != NULL) {
       REFIT_CALL_1_WRAPPER(gVarsDir->Close, gVarsDir);
       gVarsDir = NULL;
    }
} // VOID UninitRefitLib()

// Called after running external programs to re-open file handles
EFI_STATUS ReinitRefitLib (VOID) {
    EFI_STATUS Status;


    ReinitVolumes();
    Status = FinishInitRefitLib();

    return Status;
} // EFI_STATUS ReinitRefitLib()

//
// UEFI variable read and write functions
//

// Create a directory for holding RefindPlus variables, if they are stored on
// disk. This will be in the RefindPlus install directory if possible, or on the
// first ESP that RefindPlus can identify if not (this typically means RefindPlus
// is on a read-only filesystem, such as HFS+ on a Mac). If neither location can
// be used, the variable is not stored. Sets the pointer to the directory in the
// file-global gVarsDir variable and returns the status of the operation.
EFI_STATUS FindVarsDir (VOID) {
    EFI_STATUS       Status;
    CHAR16          *VarsFolder;
    EFI_FILE_HANDLE  EspRootDir;


    if (gVarsDir != NULL) {
        // Early Return
        return EFI_SUCCESS;
    }

    VarsFolder = L"vars";
    Status = REFIT_CALL_5_WRAPPER(
        SelfDir->Open, SelfDir,
        &gVarsDir, VarsFolder,
        ReadWriteCreate, EFI_FILE_DIRECTORY
    );

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Locate/Create %s for RefindPlus-Specific Items ... In Installation Folder:- '%r'",
        NVRAM_EMULATED, Status
    );
    #endif

    if (EFI_ERROR(Status)) {
        VarsFolder = L"refind-vars";
        Status = egFindESP (&EspRootDir);
        if (!EFI_ERROR(Status)) {
            Status = REFIT_CALL_5_WRAPPER(
                EspRootDir->Open, EspRootDir,
                &gVarsDir, VarsFolder,
                ReadWriteCreate, EFI_FILE_DIRECTORY
            );
        }

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Locate/Create %s for RefindPlus-Specific Items ... In First Available ESP:- '%r'",
            NVRAM_EMULATED, Status
        );

        if (EFI_ERROR(Status)) {
            ALT_LOG(1, LOG_THREE_STAR_MID,
                L"Activate The 'use_nvram' Config Token to Use Hardware %s Instead",
                NVRAM_TITLE
            );
        }
        #endif
    }

    return Status;
} // EFI_STATUS FindVarsDir()

// Retrieve a raw UEFI variable, either from NVRAM or from a disk file under
// RefindPlus' "vars" subdirectory, depending on GlobalConfig.UseNvram.
// Returns EFI status
EFI_STATUS EfivarGetRaw (
    IN  EFI_GUID  *VendorGUID,
    IN  CHAR16    *VariableName,
    OUT VOID     **VariableData,
    OUT UINTN     *VariableSize  OPTIONAL
) {
    EFI_STATUS   Status;
    UINTN        BufferSize;
    VOID        *TmpBuffer;
    BOOLEAN      TypeRP;


    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;
    BOOLEAN  HybridLogger = FALSE;
    MY_HYBRIDLOGGER_SET;
    #endif

    BufferSize = 0;
    TmpBuffer = NULL;
    TypeRP = (
        GuidsAreEqual (VendorGUID, &RefindPlusGuid) ||
        GuidsAreEqual (VendorGUID, &RefindPlusOldGuid)
    );

    if (TypeRP && !GlobalConfig.UseNvram) {
        Status = FindVarsDir();
        if (!EFI_ERROR(Status)) {
            Status = egLoadFile (
                gVarsDir,
                VariableName,
                (UINT8 **) &TmpBuffer,
                &BufferSize
            );
        }

        if (!EFI_ERROR(Status)) {
            *VariableData = TmpBuffer;
            *VariableSize = (BufferSize != 0) ? BufferSize : 0;
        }
        else {
            *VariableSize = 0;
            *VariableData = NULL;
            MY_FREE_POOL(TmpBuffer);

            #if REFIT_DEBUG > 0
            if (Status != EFI_NOT_FOUND) {
                MsgStr = PoolPrint (
                    L"%s %s:- '%r ... %s'",
                    NVRAM_LOG_GET, NVRAM_EMULATED,
                    Status, VariableName
                );
                ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
                LOG_MSG("\n");
                LOG_MSG("** WARN: %s", MsgStr);
                LOG_MSG("\n");
                LOG_MSG("         Set the 'use_nvram' Config Option to Silence This Warning");
                MY_FREE_POOL(MsgStr);
            }
            #endif
        }
    }
    else if (!TypeRP || GlobalConfig.UseNvram) {
        // Pass zero-size buffer in to find the required buffer size.
        Status = REFIT_CALL_5_WRAPPER(
            gRT->GetVariable, VariableName,
            VendorGUID, NULL,
            &BufferSize, TmpBuffer
        );

        // If the variable exists, the status should be EFI_BUFFER_TOO_SMALL and BufferSize updated.
        // Any other status means the variable does not exist.
        if (Status != EFI_BUFFER_TOO_SMALL) {
            Status = EFI_NOT_FOUND;
        }
        else {
            TmpBuffer = AllocateZeroPool (BufferSize);
            if (TmpBuffer == NULL) {
                Status = EFI_OUT_OF_RESOURCES;
            }
            else {
                // Retry with the correct buffer size.
                Status = REFIT_CALL_5_WRAPPER(
                    gRT->GetVariable, VariableName,
                    VendorGUID, NULL,
                    &BufferSize, TmpBuffer
                );
            }
        }

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"%s %s:- '%r ... %s'",
            NVRAM_LOG_GET, NVRAM_HARDWARE,
            Status, VariableName
        );
        #endif

        if (!EFI_ERROR(Status)) {
            *VariableData = TmpBuffer;
            *VariableSize = (BufferSize != 0) ? BufferSize : 0;
        }
        else {
            *VariableSize = 0;
            *VariableData = NULL;
            MY_FREE_POOL(TmpBuffer);
        }
    }
    else {
        Status = EFI_LOAD_ERROR;

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_THIN_SEP,
            L"Program Coding Error Getting Item from %s",
            NVRAM_TITLE
        );
        #endif
    }

    #if REFIT_DEBUG > 0
    MY_HYBRIDLOGGER_OFF;
    #endif

    return Status;
} // EFI_STATUS EfivarGetRaw()

// Set an UEFI variable. This is normally done to NVRAM; however, RefindPlus'
// variables (as determined by the *VendorGUID code) will be saved to a disk file IF
// GlobalConfig.UseNvram == FALSE.
// Returns EFI status
EFI_STATUS EfivarSetRaw (
    IN  EFI_GUID  *VendorGUID,
    IN  CHAR16    *VariableName,
    IN  VOID      *VariableData,
    IN  UINTN      VariableSize,
    IN  BOOLEAN    Persistent
) {
    EFI_STATUS   Status;
    UINT32       OurAccessFlag;
    VOID        *OldBuf;
    UINTN        OldSize;
    BOOLEAN      SettingMatch;

    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr;

    BOOLEAN  CheckMute = FALSE;
    BOOLEAN  HybridLogger = FALSE;
    MY_HYBRIDLOGGER_SET;
    #endif

    if (VariableSize > 0                       &&
        VariableData != NULL                    &&
        !MyStriCmp (VariableName, L"HiddenTags") &&
        !MyStriCmp (VariableName, L"HiddenTools") &&
        !MyStriCmp (VariableName, L"HiddenLegacy") &&
        !MyStriCmp (VariableName, L"HiddenFirmware")
    ) {
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif
        Status = EfivarGetRaw (
            VendorGUID, VariableName,
            &OldBuf, &OldSize
        );
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif

        if (!EFI_ERROR(Status)) {
            // Check for settings match
            SettingMatch = (
                VariableSize == OldSize &&
                CompareMem (VariableData, OldBuf, VariableSize) == 0
            );

            if (SettingMatch) {
                // Return if settings match

                #if REFIT_DEBUG > 0
                MY_HYBRIDLOGGER_OFF;
                #endif

                MY_FREE_POOL(OldBuf);

                // State to be logged by caller if required
                return EFI_ALREADY_STARTED;
            }
        }
        MY_FREE_POOL(OldBuf);
    }

    // Proceed ... settings do not match
    if (!GlobalConfig.UseNvram &&
        (
            GuidsAreEqual (VendorGUID, &RefindPlusGuid) ||
            GuidsAreEqual (VendorGUID, &RefindPlusOldGuid)
        )
    ) {
        Status = FindVarsDir();
        if (!EFI_ERROR(Status)) {
            // Clear the current value
            egSaveFile (
                gVarsDir, VariableName,
                NULL, 0
            );

            // Store the new value
            Status = egSaveFile (
                gVarsDir, VariableName,
                (UINT8 *) VariableData, VariableSize
            );
        }

        #if REFIT_DEBUG > 0
        if (VariableData != NULL && VariableSize != 0 && StrLen (VariableData) > 0) {
            ALT_LOG(1, LOG_THREE_STAR_MID,
                L"%s %s:- '%r ... %s'",
                NVRAM_LOG_SET, NVRAM_EMULATED,
                Status, VariableName
            );

            if (EFI_ERROR(Status)) {
                MsgStr = L"Activate the ‘use_nvram’ Option to Silence This Warning";
                ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
                LOG_MSG(
                    "** WARN: Could *NOT* Save to %s:- '%s'",
                    NVRAM_EMULATED, VariableName
                );
                LOG_MSG("\n");
                LOG_MSG("         %s", MsgStr);
                LOG_MSG("\n\n");
            }
        }
        #endif
    }
    else {
        // Store the new value
        OurAccessFlag = EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS;
        if (Persistent) {
            OurAccessFlag |= EFI_VARIABLE_NON_VOLATILE;
        }
        Status = REFIT_CALL_5_WRAPPER(
            gRT->SetVariable, VariableName,
            VendorGUID, OurAccessFlag,
            VariableSize, VariableData
        );

        #if REFIT_DEBUG > 0
        if (VariableData != NULL && VariableSize != 0 && StrLen (VariableData) > 0) {
            ALT_LOG(1, LOG_THREE_STAR_MID,
                L"%s %s:- '%r ... %s'%s",
                NVRAM_LOG_SET, NVRAM_HARDWARE,
                Status, VariableName,
                (!Persistent) ? L" (Volatile)" : L""
            );
        }
        #endif
    }

    #if REFIT_DEBUG > 0
    MY_HYBRIDLOGGER_OFF;
    #endif

    return Status;
} // EFI_STATUS EfivarSetRaw()

//
// list functions
//

VOID AddListElement (
    IN OUT VOID  ***ListPtr,
    IN OUT UINTN   *ElementCount,
    IN     VOID    *NewElement
) {
    VOID          *TmpListPtr;
    const UINTN    AllocatePointer = sizeof (VOID *) * (*ElementCount + 16);
    const UINTN    ElementPointer  = sizeof (VOID *) * (*ElementCount);
    BOOLEAN        Abort;


    Abort = FALSE;
    if (*ListPtr == NULL) {
        TmpListPtr = AllocatePool (AllocatePointer);
        if (TmpListPtr == NULL) {
            Abort = TRUE;
        }
        else {
            *ListPtr = TmpListPtr;
        }
    }
    else if ((*ElementCount & 15) == 0) {
        if (*ElementCount == 0) {
            // DA-TAG: Dereference *ListPtr ... Do not free
            MY_SOFT_FREE(*ListPtr);

            TmpListPtr = AllocatePool (AllocatePointer);
        }
        else {
            TmpListPtr = EfiReallocatePool (
                *ListPtr, ElementPointer, AllocatePointer
            );
        }

        if (TmpListPtr == NULL) {
            Abort = TRUE;
        }
        else {
            *ListPtr = TmpListPtr;
        }
    }

    if (!Abort) {
        (*ListPtr)[*ElementCount] = NewElement;
        (*ElementCount)++;
    }
} // VOID AddListElement()

VOID FreeList (
    IN OUT VOID  ***ListPtr,
    IN OUT UINTN   *ElementCount
) {
    UINTN i;

    if ((*ElementCount > 0) && (**ListPtr != NULL)) {
        for (i = 0; i < *ElementCount; i++) {
            // TODO: call a user-provided routine for each element here
            MY_FREE_POOL((*ListPtr)[i]);
        }
        MY_FREE_POOL(*ListPtr);
    }
} // VOID FreeList()

//
// volume functions
//

VOID SanitiseVolumeName (
    REFIT_VOLUME **Volume
) {
    CHAR16 *VolumeName;


    VolumeName = NULL;
    if (Volume != NULL &&
        *Volume != NULL &&
        (*Volume)->VolName != NULL
    ) {
        if (0);
        else if (NAME_FIX(L"EFI System Partition"        );
        else if (NAME_FIX(L"Whole Disk Volume"           );
        else if (NAME_FIX(L"Unknown Volume"              );
        else if (NAME_FIX(L"NTFS Volume"                 );
        else if (NAME_FIX(L"HFS+ Volume"                 );
        else if (FAT_NAME(L"FAT Volume"                  ); // Special Case
        else if (NAME_FIX(L"XFS Volume"                  );
        else if (NAME_FIX(L"PreBoot"                     );
        else if (FIX_FLAG(L"Swap"                        ); // Match Required
        else if (NAME_FIX(L"Ext4 Volume"                 );
        else if (NAME_FIX(L"Ext3 Volume"                 );
        else if (NAME_FIX(L"Ext2 Volume"                 );
        else if (FAT_NAME(L"ExFAT Volume"                ); // Special Case
        else if (NAME_FIX(L"BtrFS Volume"                );
        else if (NAME_FIX(L"ReiserFS Volume"             );
        else if (NAME_FIX(L"ISO-9660 Volume"             );
        else if (NAME_FIX(L"System Reserved"             );
        else if (NAME_FIX(L"Basic Data Partition"        );
        else if (NAME_FIX(L"Microsoft Reserved Partition");
    }

    if (VolumeName != NULL) {
        MY_FREE_POOL((*Volume)->VolName);
        (*Volume)->VolName = StrDuplicate (VolumeName);
    }
} // VOID SanitiseVolumeName()

// DA-TAG: Update UninitVolume() and ReinitVolume() if expanding this
VOID FreeSyncVolumes (VOID) {
    if (!GlobalConfig.SyncAPFS) {
        // Early Return
        return;
    }

    MY_FREE_POOL(RecoveryVolumes);
    MY_FREE_POOL(SkipApfsVolumes);
    MY_FREE_POOL(PreBootVolumes );
    MY_FREE_POOL(SystemVolumes  );
    MY_FREE_POOL(HfsRecovery    );
    MY_FREE_POOL(DataVolumes    );

    RecoveryVolumesCount      = 0;
    SkipApfsVolumesCount      = 0;
    PreBootVolumesCount       = 0;
    SystemVolumesCount        = 0;
    HfsRecoveryCount          = 0;
    DataVolumesCount          = 0;
} // VOID FreeSyncVolumes()

VOID FreeVolumes (
    IN OUT REFIT_VOLUME  ***ListVolumes,
    IN OUT UINTN           *ListCount
) {
    if ((*ListCount > 0) && (**ListVolumes != NULL)) {
        MY_FREE_POOL(*ListVolumes);
        *ListCount = 0;
    }
} // VOID FreeVolumes()

REFIT_VOLUME * CopyVolume (
    IN REFIT_VOLUME *VolumeToCopy
) {
    REFIT_VOLUME *Volume;
    UINTN         SizeMBR;


    if (VolumeToCopy == NULL) {
        // Early Exit
        return NULL;
    }

    // UnInit VolumeToCopy
    UninitVolume (&VolumeToCopy);

      // Create New Volume based on VolumeToCopy (in 'UnInit' state)
    Volume = AllocateCopyPool (sizeof (REFIT_VOLUME), VolumeToCopy);
    if (Volume != NULL) {
        Volume->FsName        = StrDuplicate (VolumeToCopy->FsName);
        Volume->VolName       = StrDuplicate (VolumeToCopy->VolName);
        Volume->PartName      = StrDuplicate (VolumeToCopy->PartName);
        Volume->VolIconImage  = egCopyImage (VolumeToCopy->VolIconImage);
        Volume->VolBadgeImage = egCopyImage (VolumeToCopy->VolBadgeImage);

        if (VolumeToCopy->DevicePath != NULL) {
            Volume->DevicePath = DuplicateDevicePath (VolumeToCopy->DevicePath);
        }

        if (VolumeToCopy->WholeDiskDevicePath != NULL) {
            Volume->WholeDiskDevicePath = DuplicateDevicePath (VolumeToCopy->WholeDiskDevicePath);
        }

        if (VolumeToCopy->MbrPartitionTable != NULL) {
            SizeMBR = 4 * 16;
            Volume->MbrPartitionTable = AllocatePool (SizeMBR);
            if (Volume->MbrPartitionTable != NULL) {
                REFIT_CALL_3_WRAPPER(
                    gBS->CopyMem,
                    Volume->MbrPartitionTable,
                    VolumeToCopy->MbrPartitionTable,
                    SizeMBR
                );
            }
        }

        // ReInit Volume
        ReinitVolume (&Volume);
    }

    // ReInit VolumeToCopy
    ReinitVolume (&VolumeToCopy);

    return Volume;
} // REFIT_VOLUME * CopyVolume()

VOID FreeVolume (
    REFIT_VOLUME **Volume
) {
    if (Volume == NULL || *Volume == NULL) {
        // Early Exit
        return;
    }

    // UnInit Volume
    UninitVolume (&(*Volume));

    // Free pool elements
    MY_FREE_POOL((*Volume)->FsName);
    MY_FREE_POOL((*Volume)->VolName);
    MY_FREE_POOL((*Volume)->PartName);
    MY_FREE_POOL((*Volume)->DevicePath);
    MY_FREE_POOL((*Volume)->MbrPartitionTable);
    MY_FREE_POOL((*Volume)->WholeDiskDevicePath);

    // Free image elements
    MY_FREE_IMAGE((*Volume)->VolIconImage);
    MY_FREE_IMAGE((*Volume)->VolBadgeImage);

    // Free whole volume
    MY_FREE_POOL(*Volume);
} // VOID FreeVolume()

// Return a pointer to a string containing a filesystem type name. If the
// filesystem type is unknown, a blank (but non-null) string is returned.
// The returned variable is a constant that should NOT be freed.
static
CHAR16 * FSTypeName (
    IN REFIT_VOLUME *Volume
) {
    UINTN    i;
    CHAR16  *retval;
    CHAR16  *VentoyName;
    BOOLEAN  FoundVentoy;

    #if REFIT_DEBUG > 0
    BOOLEAN  CheckMute = FALSE;
    #endif

    switch (Volume->FSType) {
        case FS_TYPE_WHOLEDISK: retval = L"Whole Disk";       break;
        case FS_TYPE_HFSPLUS:   retval = L"HFS+"      ;       break;
        case FS_TYPE_APFS:      retval = L"APFS"      ;       break;
        case FS_TYPE_NTFS:      retval = L"NTFS"      ;       break;
        case FS_TYPE_EXT4:      retval = L"Ext4"      ;       break;
        case FS_TYPE_EXT3:      retval = L"Ext3"      ;       break;
        case FS_TYPE_EXT2:      retval = L"Ext2"      ;       break;
        case FS_TYPE_FAT32:     retval = L"FAT-32"    ;       break;
        case FS_TYPE_FAT16:     retval = L"FAT-16"    ;       break;
        case FS_TYPE_FAT12:     retval = L"FAT-12"    ;       break;
        case FS_TYPE_EXFAT:     retval = L"ExFAT"     ;       break;
        case FS_TYPE_XFS:       retval = L"XFS"       ;       break;
        case FS_TYPE_JFS:       retval = L"JFS"       ;       break;
        case FS_TYPE_BTRFS:     retval = L"BtrFS"     ;       break;
        case FS_TYPE_ISO9660:   retval = L"ISO-9660"  ;       break;
        case FS_TYPE_REISERFS:  retval = L"ReiserFS"  ;       break;
        default:                retval = L"Unknown"   ;       break;
    } // switch

    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_SET;
    #endif
    if (0);
    else if (FindSubStr (Volume->VolName, L"Optical Disc"    )) retval = L"ISO-9660 (Assumed)";
    else if (FindSubStr (Volume->VolName, L"APFS/FileVault"  )) retval = L"APFS (Assumed)"    ;
    else if (FindSubStr (Volume->VolName, L"Fusion/FileVault")) retval = L"HFS+ (Assumed)"    ;
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    i = 0;
    FoundVentoy = FALSE;
    while (
        !FoundVentoy &&
        (VentoyName = FindCommaDelimited (VENTOY_NAMES, i++)) != NULL
    ) {
        if (MyStrBegins (VentoyName, Volume->VolName)) {
            GlobalConfig.HandleVentoy = FoundVentoy = TRUE;
        }

        MY_FREE_POOL(VentoyName);

        if (FoundVentoy && MyStriCmp (retval, L"Unknown")) {
            return L"ExFAT (Assumed)";
        }
    } // while

    if (!MyStriCmp (retval, L"Unknown")) {
        return retval;
    }

    if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidBasicData)) {
        retval = (FoundVentoy) ? L"ExFAT (Assumed)" : L"NTFS (Assumed)";
    }
    else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidReservedMS)) retval = L"NTFS (Assumed)"  ;
    else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidBasicData )) retval = L"NTFS (Assumed)"  ;
    else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidSwap      )) retval = L"Linux Swap"      ;
    else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidLuks      )) retval = L"LUKS Encrypted"  ;
    else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidMacRaidOn )) retval = L"Apple Raid (ON)" ;
    else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidMacRaidOff)) retval = L"Apple Raid (OFF)";

    return retval;
} // static CHAR16 * FSTypeName()

// Sets the FsName field of Volume, based on data recorded in the partition's
// filesystem. This field may remain unchanged if there is no known filesystem
// or if the name field is empty.
static
VOID SetFilesystemName (
    IN OUT REFIT_VOLUME *Volume
) {
    EFI_FILE_SYSTEM_INFO *FileSystemInfoPtr;


    if (Volume          == NULL ||
        Volume->RootDir == NULL
    ) {
        return;
    }

    FileSystemInfoPtr = LibFileSystemInfo (Volume->RootDir);
    if (FileSystemInfoPtr == NULL) {
        return;
    }

    if (StrLen (FileSystemInfoPtr->VolumeLabel) > 0) {
        MY_FREE_POOL(Volume->FsName);
        Volume->FsName = StrDuplicate (FileSystemInfoPtr->VolumeLabel);
    }

    MY_FREE_POOL(FileSystemInfoPtr);
} // static VOID SetFilesystemName()

// Identify filesystem type and record filesystem's UUID/serial number,
// if possible. Expects a Buffer containing the first few (normally 4096)
// bytes of the filesystem. Sets filesystem type code in "Volume->FSType"
// and the UUID/serial number in Volume->VolUuid. Note that the UUID value is
// recognised differently for each filesystem, and is currently supported only
// for HFS+, NTFS, FAT, ext2/3/4, and ReiserFS (for HFS+, NTFS, and FAT, it is
// really a 64-bit or 32-bit serial number and not a UUID/GUID). If the UUID
// cannot be gotten, it is set to 0. Also, the value is just read directly
// into memory and is NOT valid when displayed by GuidAsString() or used
// in other GUID/UUID-manipulating functions. It is currently only for
// handling partitions in RAID arrays 1 or in APFS Volume Groups.
static
VOID SetFilesystemData (
    IN     UINT8        *Buffer,
    IN     UINTN         BufferSize,
    IN OUT REFIT_VOLUME *Volume
) {
    EFI_GUID *GuidPathAPFS;
    UINT32   *Ext2Incompat;
    UINT32   *Ext2Compat;
    UINT16   *Magic16;
    char     *MagicString;


    if (Buffer == NULL ||
        Volume == NULL
    ) {
        return;
    }

    REFIT_CALL_3_WRAPPER(
        gBS->SetMem, &(Volume->VolUuid),
        sizeof (EFI_GUID), 0
    );

    // Default to "Unknown" Type
    Volume->FSType = FS_TYPE_UNKNOWN;

    if (BufferSize >= (1024 + 100)) {
        Magic16 = (UINT16 *) (Buffer + 1024 + 56);
        if (*Magic16 == EXT2_SUPER_MAGIC) {
            // Ext2/3/4
            Ext2Compat   = (UINT32 *) (Buffer + 1024 + 92);
            Ext2Incompat = (UINT32 *) (Buffer + 1024 + 96);

            if ((*Ext2Incompat & 0x0040) || (*Ext2Incompat & 0x0200)) {
                // Check for extents or flex_bg
                Volume->FSType = FS_TYPE_EXT4;
            }
            else if (*Ext2Compat & 0x0004) {
                // Check for journal
                Volume->FSType = FS_TYPE_EXT3;
            }
            else {
                // None of the features ... Assume Ext2
                Volume->FSType = FS_TYPE_EXT2;
            }

            REFIT_CALL_3_WRAPPER(
                gBS->CopyMem, &(Volume->VolUuid),
                Buffer + 1024 + 104, sizeof (EFI_GUID)
            );

            return;
        }
    } // Search for Ext2/3/4 magic

    if (BufferSize >= (65536 + 100)) {
        MagicString = (char *) (Buffer + 65536 + 52);
        if ((CompareMem (MagicString, REISERFS_SUPER_MAGIC_STRING,     8) == 0) ||
            (CompareMem (MagicString, REISER2FS_SUPER_MAGIC_STRING,    9) == 0) ||
            (CompareMem (MagicString, REISER2FS_JR_SUPER_MAGIC_STRING, 9) == 0)
        ) {
            Volume->FSType = FS_TYPE_REISERFS;
            REFIT_CALL_3_WRAPPER(
                gBS->CopyMem, &(Volume->VolUuid),
                Buffer + 65536 + 84, sizeof (EFI_GUID)
            );
            return;
        }
    } // Search for ReiserFS magic

    if (BufferSize >= (65536 + 64 + 8)) {
        MagicString = (char *) (Buffer + 65536 + 64);
        if (CompareMem (MagicString, BTRFS_SIGNATURE, 8) == 0) {
            Volume->FSType = FS_TYPE_BTRFS;
            return;
        }
    } // Search for BtrFS magic

    if (BufferSize >= BASE_SIZE) {
        MagicString = (char *) Buffer;
        if (CompareMem (MagicString, XFS_SIGNATURE, 4) == 0) {
            Volume->FSType = FS_TYPE_XFS;
            return;
        }
    } // Search for XFS magic

    if (BufferSize >= (32768 + 4)) {
        MagicString = (char *) (Buffer + 32768);
        if (CompareMem (MagicString, JFS_SIGNATURE, 4) == 0) {
            Volume->FSType = FS_TYPE_JFS;
            return;
        }
    } // Search for JFS magic

    if (BufferSize >= (1024 + 2)) {
        Magic16 = (UINT16 *) (Buffer + 1024);
        if ((*Magic16 == HFSPLUS_MAGIC0) ||
            (*Magic16 == HFSPLUS_MAGIC1) ||
            (*Magic16 == HFSPLUS_MAGIC2)
        ) {
            if (BufferSize >= (1024 + 112)) {
                REFIT_CALL_3_WRAPPER(
                    gBS->CopyMem, &(Volume->VolUuid),
                    Buffer + 1024 + 104, sizeof (UINT64)
                );
            }

            Volume->FSType = FS_TYPE_HFSPLUS;
            return;
        }
    } // Search for HFS+ magic

    if (BufferSize >= BASE_SIZE) {
        // Search for NTFS, FAT, and MBR/EBR.
        // These all have 0xAA55 at the end of the first sector, so we must
        // also search for NTFS, FAT12, FAT16, and FAT32 signatures to
        // figure out where to look for the filesystem serial numbers.
        Magic16 = (UINT16 *) (Buffer + 510);
        if (*Magic16 == FAT_MAGIC) {
            MagicString = (char *) Buffer;
            if (CompareMem (MagicString + 3, NTFS_SIGNATURE, 8) == 0) {
                Volume->FSType = FS_TYPE_NTFS;
                REFIT_CALL_3_WRAPPER(
                    gBS->CopyMem, &(Volume->VolUuid),
                    Buffer + 0x48, sizeof (UINT64)
                );
            }
            else if (CompareMem (MagicString + 0x52, FAT32_SIGNATURE, 8) == 0) {
                Volume->FSType = FS_TYPE_FAT32;
                REFIT_CALL_3_WRAPPER(
                    gBS->CopyMem, &(Volume->VolUuid),
                    Buffer + 0x43, sizeof (UINT32)
                );
            }
            else if (CompareMem (MagicString + 0x36, FAT16_SIGNATURE, 8) == 0) {
                Volume->FSType = FS_TYPE_FAT16;
                REFIT_CALL_3_WRAPPER(
                    gBS->CopyMem, &(Volume->VolUuid),
                    Buffer + 0x27, sizeof (UINT32)
                );
            }
            else if (CompareMem (MagicString + 0x36, FAT12_SIGNATURE, 8) == 0) {
                Volume->FSType = FS_TYPE_FAT12;
                REFIT_CALL_3_WRAPPER(
                    gBS->CopyMem, &(Volume->VolUuid),
                    Buffer + 0x27, sizeof (UINT32)
                );
            }
            else if (!Volume->BlockIO->Media->LogicalPartition) {
                Volume->FSType = FS_TYPE_WHOLEDISK;
            }
            else if (FindMem (Buffer, BASE_SIZE, "EXFAT", 5) >= 0) {
                Volume->FSType = FS_TYPE_EXFAT;
            }

            return;
        }
    } // Search for FAT and NTFS magic

    // DA-TAG: Assume APFS if DevicePath has APFS signature
    //         Adated from Clover 'ApfsSignatureUUID' check
    if (DevicePathType (Volume->DevicePath) == MEDIA_DEVICE_PATH &&
        DevicePathSubType (Volume->DevicePath) == MEDIA_VENDOR_DP
    ) {
        GuidPathAPFS = (EFI_GUID *) ((UINT8 *) Volume->DevicePath + 0x04);
        if (GuidsAreEqual (GuidPathAPFS, &GuidFlagAPFS)) {
            Volume->FSType = FS_TYPE_APFS;
            if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidNull)) {
                Volume->PartTypeGuid = GuidFlagAPFS;
            }
            return;
        }
    }

    // DA-TAG: Assume APFS if on partition with APFS GUID
    if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidAPFS)) {
        Volume->FSType = FS_TYPE_APFS;
        return;
    }

    // DA-TAG: Assume ISO-9660 if block size is right
    if (Volume->BlockIO->Media->BlockSize == 2048) {
        Volume->FSType = FS_TYPE_ISO9660;
    }
} // static VOID SetFilesystemData()

static
VOID ScanVolumeBootcode (
    IN OUT REFIT_VOLUME  *Volume,
    IN OUT BOOLEAN       *Bootable
) {
    EFI_STATUS           Status;
    UINTN                i, SizeMBR;
    UINT8                Buffer[SAMPLE_SIZE];
    BOOLEAN              MbrTableFound;
    MBR_PARTITION_INFO  *MbrTable;

    #if REFIT_DEBUG > 0
    UINTN    LogLineType;
    CHAR16  *StrSpacer;
    CHAR16  *MsgStr;
    #endif

    *Bootable           = FALSE;
    MediaCheck          = FALSE;
    Volume->HasBootCode = FALSE;
    Volume->OSIconName  = NULL;
    Volume->OSName      = NULL;

    if (Volume->BlockIO == NULL) {
        #if REFIT_DEBUG > 0
        if (SelfVolRun) {
            MsgStr = L"Found Invalid Volume BlockIO on Item Below";
            LOG_MSG("\n\n");
            LOG_MSG("** WARN:  %s", MsgStr);
            LOG_MSG("\n");
        }
        #endif

        return;
    }
    if (Volume->BlockIO->Media->BlockSize > SAMPLE_SIZE) {
        #if REFIT_DEBUG > 0
        if (SelfVolRun) {
            // Buffer is too small
            MsgStr = L"Found Invalid Boot Code Buffer Size on Item Below";
            LOG_MSG("\n\n");
            LOG_MSG("** WARN:  %s", MsgStr);
        }
        #endif

        return;
    }

    // Look at the boot sector (this is used for both hard disks and El Torito images!)
    Status = REFIT_CALL_5_WRAPPER(
        Volume->BlockIO->ReadBlocks, Volume->BlockIO,
        Volume->BlockIO->Media->MediaId, Volume->BlockIOOffset,
        SAMPLE_SIZE, Buffer
    );

    if (GlobalConfig.LegacyType != LEGACY_TYPE_MAC1) {
        #if REFIT_DEBUG > 0
        if (SelfVolRun && ScanMBR) {
            MsgStr = L"Could *NOT* Read Boot Sector on Item Below";
            LOG_MSG("\n\n");
            LOG_MSG("** WARN: '%r' %s", Status, MsgStr);

            ScannedOnce = FALSE;
            if (EFI_ERROR(Status)) {
                CheckError (Status, MsgStr);
                if (Status == EFI_NO_MEDIA) {
                    MediaCheck = TRUE;
                }
            }
        }
        #endif

        return;
    }

    // DA-TAG: Do not log if unable to filter
    if (EFI_ERROR(Status)) {
        return;
    }

    SetFilesystemData (Buffer, SAMPLE_SIZE, Volume);
    if ((Buffer[0] != 0)                        &&
        (*((UINT16 *)(Buffer + 510)) == 0xaa55) &&
        (FindMem (Buffer, BASE_SIZE, "EXFAT", 5) == -1)
    ) {
        *Bootable = Volume->HasBootCode = TRUE;
    }

    // Detect specific boot codes
    if (CompareMem (Buffer + 2, "LILO",           4) == 0 ||
        CompareMem (Buffer + 6, "LILO",           4) == 0 ||
        CompareMem (Buffer + 3, "SYSLINUX",       8) == 0 ||
        FindMem (Buffer, SECTOR_SIZE, "ISOLINUX", 8) >= 0
    ) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"linux";
        Volume->OSName       = L"Instance: Linux (Legacy)";
    }
    else if (
        FindMem (Buffer, BASE_SIZE, "Geom\0Hard Disk\0Read\0 Error", 26) >= 0
    ) {
        // GRUB
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"grub,linux";
        Volume->OSName       = L"Instance: Linux (Legacy)";
    }
    else if (
        (
            *((UINT32 *)(Buffer + 502)) == 0     &&
            *((UINT32 *)(Buffer + 506)) == 50000 &&
            *((UINT16 *)(Buffer + 510)) == 0xaa55
        ) || (
            FindMem (Buffer, SECTOR_SIZE, "Starting the BTX loader", 23) >= 0
        )
    ) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"freebsd";
        Volume->OSName       = L"Instance: FreeBSD (Legacy)";
    }
    else if (
        (*((UINT16 *)(Buffer + 510)) == 0xaa55)                                   &&
        (FindMem (Buffer, SECTOR_SIZE, "Boot loader too large",         21) >= 0) &&
        (FindMem (Buffer, SECTOR_SIZE, "I/O error loading boot loader", 29) >= 0)
    ) {
        // If more differentiation needed, also search for
        // "Invalid Partition Table" &/or "Missing boot loader".
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"freebsd";
        Volume->OSName       = L"Instance: FreeBSD (Legacy)";
    }
    else if (
        FindMem (Buffer, BASE_SIZE,   "!Loading",            8) >= 0 ||
        FindMem (Buffer, SECTOR_SIZE, "/cdboot\0/CDBOOT\0", 16) >= 0
    ) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"openbsd";
        Volume->OSName       = L"Instance: OpenBSD (Legacy)";
    }
    else if (
        FindMem (Buffer, BASE_SIZE, "Not a bootxx image", 18) >= 0 ||
        *((UINT32 *)(Buffer + 1028)) == 0x7886b6d1
    ) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"netbsd";
        Volume->OSName       = L"Instance: NetBSD (Legacy)";
    }
    else if (FindMem (Buffer, SECTOR_SIZE, "NTLDR", 5) >= 0) {
        // Windows NT/200x/XP
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"win8,win";
        Volume->OSName       = L"Instance: Windows (Legacy - NT/XP)";
    }
    else if (FindMem (Buffer, SECTOR_SIZE, "BOOTMGR", 7) >= 0) {
        // Windows Vista/7/8/10/11
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"win8,win";
        Volume->OSName       = L"Instance: Windows (Legacy)";
    }
    else if (
        FindMem (Buffer, BASE_SIZE, "CPUBOOT SYS", 11) >= 0 ||
        FindMem (Buffer, BASE_SIZE, "KERNEL  SYS", 11) >= 0
    ) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"freedos";
        Volume->OSName       = L"Instance: FreeDOS (Legacy)";
    }
    else if (
        FindMem (Buffer, BASE_SIZE, "OS2LDR",  6) >= 0 ||
        FindMem (Buffer, BASE_SIZE, "OS2BOOT", 7) >= 0
    ) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"ecomstation";
        Volume->OSName       = L"Instance: eComStation (Legacy)";
    }
    else if (FindMem (Buffer, BASE_SIZE, "Be Boot Loader", 14) >= 0) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"beos";
        Volume->OSName       = L"Instance: BeOS (Legacy)";
    }
    else if (FindMem (Buffer, BASE_SIZE, "yT Boot Loader", 14) >= 0) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"zeta,beos";
        Volume->OSName       = L"Instance: ZETA (Legacy)";
    }
    else if (
        FindMem (Buffer, BASE_SIZE, "\x04" "beos\x06" "system\x05" "zbeos", 18) >= 0 ||
        FindMem (Buffer, BASE_SIZE, "\x06" "system\x0c" "haiku_loader",     20) >= 0
    ) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"haiku,beos";
        Volume->OSName       = L"Instance: Haiku (Legacy)";
    } // CompareMem

    /**
     * NOTE: If you add an operating system with a name that starts with 'W' or 'L',
     *       you need to fix AddLegacyEntry in BootMaster/launch_legacy.c.
     *       DA-TAGGED
    **/

    if (Volume->HasBootCode) {
        // Verify Windows boot sector on Macs
        if (Volume->FSType          == FS_TYPE_NTFS  &&
            GlobalConfig.LegacyType == LEGACY_TYPE_MAC1
        ) {
            Volume->HasBootCode = HasWindowsBiosBootFiles (Volume);
        }
        else if (FindMem (Buffer, BASE_SIZE, "Non-system disk", 15) >= 0) {
            // Dummy FAT boot sector (created by OS X's newfs_msdos)
            Volume->HasBootCode = FALSE;
        }
        else if (FindMem (Buffer, BASE_SIZE, "This is not a bootable disk", 27) >= 0) {
            // Dummy FAT boot sector (created by Linux's mkdosfs)
            Volume->HasBootCode = FALSE;
        }
        else if (FindMem (Buffer, BASE_SIZE, "Press any key to restart", 24) >= 0) {
            // Dummy FAT boot sector (created by Windows)
            Volume->HasBootCode = FALSE;
        }
    }

    if (!Volume->HasBootCode) {
        *Bootable = FALSE;
        return;
    }

    // Check for MBR partition table
    if (*((UINT16 *)(Buffer + 510)) != 0xaa55) {
        *Bootable = Volume->HasBootCode = FALSE;
        return;
    }

    MbrTableFound = FALSE;
    MbrTable = (MBR_PARTITION_INFO *) (Buffer + 446);
    for (i = 0; i < 4; i++) {
        if (MbrTable[i].StartLBA && MbrTable[i].Size) {
            MbrTableFound = TRUE;
            break;
        }
    }
    for (i = 0; i < 4; i++) {
        if (MbrTable[i].Flags != 0x00 && MbrTable[i].Flags != 0x80) {
            MbrTableFound = FALSE;
            break;
        }
    }

    if (MbrTableFound) {
        SizeMBR = 4 * 16;
        Volume->MbrPartitionTable = AllocatePool (SizeMBR);
        if (Volume->MbrPartitionTable == NULL) {
            *Bootable = Volume->HasBootCode = FALSE;

            return;
        }

        REFIT_CALL_3_WRAPPER(
            gBS->CopyMem, Volume->MbrPartitionTable,
            MbrTable, SizeMBR
        );

        #if REFIT_DEBUG > 0
        FoundMBR = TRUE;
        #endif
    }

    #if REFIT_DEBUG > 0
    if (DoneHeadings && (MbrTableFound || Volume->HasBootCode)) {
        LogLineType = (SkipSpacing)
            ? LOG_LINE_SAME
            : LOG_LINE_SPECIAL;

        if (Volume->HasBootCode) {
            StrSpacer = (!SkipSpacing)
                ? L""
                : (UseButJoin)
                    ? BUT_JOIN_TXT
                    : AND_JOIN_TXT;
            ALT_LOG(1, LogLineType, L"%s%s", StrSpacer, LEGACY_CODE_TXT);
            UseButJoin = FALSE;
        }

        if (MbrTableFound) {
            if (Volume->HasBootCode) {
                StrSpacer   =  AND_JOIN_TXT;
                LogLineType = LOG_LINE_SAME;
            }
            else {
                StrSpacer = (!SkipSpacing)
                    ? L""
                    : (UseButJoin)
                        ? BUT_JOIN_TXT
                        : AND_JOIN_TXT;
            }
            ALT_LOG(1, LogLineType, L"%s%s", StrSpacer, PARTITION_TABLE_TXT);
            UseButJoin = FALSE;
        }

        SkipSpacing = (GlobalConfig.LogLevel > 0) ? TRUE : FALSE;
    }
    #endif
} // static VOID ScanVolumeBootcode()

static
VOID UpdateBadgeIcon (
    IN OUT REFIT_VOLUME *Volume
) {
    Volume->VolBadgeImage = egLoadIconAnyType (
        Volume->RootDir, L"", L".VolumeBadge",
        GlobalConfig.IconSizes[ICON_SIZE_BADGE]
    );
} // static VOID UpdateBadgeIcon()

// Set default volume badge icon based on '.VolumeBadge.{ext}' file or disk kind
VOID SetVolumeBadgeIcon (
    IN OUT REFIT_VOLUME *Volume
) {
    if (!AllowGraphicsMode) {
        // Early Return
        return;
    }

    if (Volume == NULL) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"Skip VolumeBadge ... NULL Volume"
        );
        #endif

        // Early Return
        return;
    }

    // Early Return ... Do not log
    if (Volume->VolBadgeImage != NULL) {
        return;
    }

    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_BADGES) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"Skip VolumeBadge ... Config Setting is Active:- 'hideui - badges or all'"
        );
        #endif

        return;
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Create VolumeBadge for Menu Item:- '%s'",
        Volume->VolName
    );
    #endif

    if (GlobalConfig.HiddenIconsPrefer) {
        UpdateBadgeIcon (Volume);
    }

    if (Volume->VolBadgeImage == NULL) {
        switch (Volume->DiskKind) {
            case DISK_KIND_INTERNAL: SET_BADGE_IMAGE(BUILTIN_ICON_VOL_INTERNAL); break;
            case DISK_KIND_EXTERNAL: SET_BADGE_IMAGE(BUILTIN_ICON_VOL_EXTERNAL); break;
            case DISK_KIND_OPTICAL:  SET_BADGE_IMAGE(BUILTIN_ICON_VOL_OPTICAL ); break;
            case DISK_KIND_NET:      SET_BADGE_IMAGE(BUILTIN_ICON_VOL_NET     ); break;
        } // switch
    }

    if (Volume->VolBadgeImage == NULL) {
        UpdateBadgeIcon (Volume);
    }
} // VOID SetVolumeBadgeIcon()

// Return a string representing the input size in IEEE-1541 units.
// The calling function is responsible for freeing the allocated memory.
static
CHAR16 * SizeInIEEEUnits (
    IN UINT64 SizeInBytes
) {
    UINTN         Index;
    UINTN         NumPrefixes;
    CHAR16       *Units;
    const CHAR16 *Prefixes = L" KMGTPEZ";
    CHAR16       *TheValue;
    UINT64        SizeInIeee;


    NumPrefixes = StrLen (Prefixes);
    SizeInIeee = SizeInBytes;
    Index = 0;
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
    }

    TheValue = PoolPrint (L"%ld%s", SizeInIeee, Units);
    MY_FREE_POOL(Units);

    return TheValue;
} // static CHAR16 * SizeInIEEEUnits()

// Determine the unique GUID, type code GUID, and name of the volume and store them.
static
VOID SetPartGuidAndName (
    IN OUT REFIT_VOLUME             *Volume,
    IN OUT EFI_DEVICE_PATH_PROTOCOL *DevicePath
) {
    HARDDRIVE_DEVICE_PATH    *HdDevicePath;
    GPT_ENTRY                *PartInfo;
    EFI_GUID                  GuidMBR = MBR_GUID_VALUE;


    if (Volume == NULL || DevicePath == NULL) {
        return;
    }

    if ((DevicePath->Type    != MEDIA_DEVICE_PATH) ||
        (DevicePath->SubType != MEDIA_HARDDRIVE_DP)
    ) {
        return;
    }

    HdDevicePath = (HARDDRIVE_DEVICE_PATH*) DevicePath;
    if (HdDevicePath->SignatureType != SIGNATURE_TYPE_GUID) {
        // DA-TAG: Investigate This
        //         Better to assign a random GUID to MBR partitions
        //         The GUID below was just generated in Linux for use in rEFInd.
        //         92a6c61f-7130-49b9-b05c-8d7e7b039127
        Volume->PartGuid = GuidMBR;

        return;
    }

    Volume->PartGuid = *((EFI_GUID*) HdDevicePath->Signature);
    PartInfo = FindPartWithGuid (&(Volume->PartGuid));
    if (PartInfo == NULL) {
        return;
    }

    Volume->PartName = StrDuplicate (PartInfo->name);
    REFIT_CALL_3_WRAPPER(
        gBS->CopyMem, &(Volume->PartTypeGuid),
        PartInfo->type_guid, sizeof (EFI_GUID)
    );

    if (GuidsAreEqual (&(Volume->PartTypeGuid), &gFreedesktopRootGuid) &&
        ((PartInfo->attributes & GPT_NO_AUTOMOUNT) == 0)
    ) {
        GlobalConfig.DiscoveredRoot = Volume;
    }

    Volume->IsMarkedReadOnly = ((PartInfo->attributes & GPT_READ_ONLY) > 0);

    MY_FREE_POOL(PartInfo);
} // static VOID SetPartGuidAndName()

// Return a name for the volume. Ideally this should be the label for the
// filesystem or partition, but this function falls back to describing the
// filesystem by size (200 MiB, etc.) and/or type (ext2, HFS+, etc.), if
// this information can be extracted.
// The calling function is responsible for freeing the memory allocated
// for the name string.
CHAR16 * GetVolumeName (
    IN REFIT_VOLUME *Volume
) {
    CHAR16                *SISize;
    CHAR16                *TypeName;
    CHAR16                *FoundName;
    EFI_FILE_SYSTEM_INFO  *FileSystemInfoPtr;


    // DA-TAG: Investigate This
    //         If filesystem name is not found, this could be improved/extended
    //         Such as: use or add disk/partition number (e.g., "(hd0,2)")
    if (Volume->FsName          == NULL  ||
        Volume->FsName[0]       == L'\0' ||
        StrLen (Volume->FsName) == 0
    ) {
        FoundName = NULL;
    }
    else {
        if (Volume->FSType == FS_TYPE_EXFAT &&
            MyStriCmp (Volume->FsName, L"FAT")
        ) {
            FoundName = StrDuplicate (L"ExFAT");
        }
        else {
            FoundName = StrDuplicate (Volume->FsName);
        }
    }
    if (FoundName != NULL) {
        return FoundName;
    }


    // Try to use the partition name if no filesystem name
    if (Volume->PartName != NULL &&
        StrLen (Volume->PartName) > 0 &&
        !IsListItem (Volume->PartName, IGNORE_PARTITION_NAMES)
    ) {
        FoundName = StrDuplicate (Volume->PartName);
    }
    if (FoundName != NULL) {
        return FoundName;
    }

    if (Volume->DiskKind == DISK_KIND_OPTICAL) {
        FoundName = (Volume->FSType == FS_TYPE_ISO9660)
            ? StrDuplicate (L"Optical ISO-9660 Image")
            : StrDuplicate (L"Optical Disc Drive");
    }
    else if (MediaCheck) {
        FoundName = StrDuplicate (L"Network Volume (Assumed)");
    }
    else {
        if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidAPFS)) {
            FoundName = StrDuplicate (L"APFS/FileVault Container");
        }
        else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidRecoveryHD)) {
            FoundName = StrDuplicate (L"Recovery HD");
        }
        else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidLinux)) {
            FoundName = StrDuplicate (L"Linux Volume");
        }
        else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidReservedMS)) {
            FoundName = StrDuplicate (L"Microsoft Reserved Partition");
        }
        else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidMacRaidOn)) {
            FoundName = StrDuplicate (L"Apple Raid Partition (Online)");
        }
        else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidMacRaidOff)) {
            FoundName = StrDuplicate (L"Apple Raid Partition (Offline)");
        }
        else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidContainerHFS)) {
            FoundName = StrDuplicate (L"Fusion/FileVault Container");
        }
        else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidApplTvRec)) {
            FoundName = StrDuplicate (L"AppleTV Recovery Partition");
        }
        else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidHFS)) {
            FoundName = StrDuplicate (L"Unidentified HFS+ Partition");
        }
        else {
            // 'FSTypeName' returns a constant ... Do not free 'TypeName'!
            TypeName = FSTypeName (Volume);

            if (MyStriCmp (TypeName, L"APFS")) {
                FoundName = StrDuplicate (L"APFS Volume (Assumed)");
            }
            else {
                // Try to use fs type and size as name
                FileSystemInfoPtr = (Volume->RootDir != NULL)
                    ? LibFileSystemInfo (Volume->RootDir) : NULL;
                if (FileSystemInfoPtr != NULL) {
                    SISize    = SizeInIEEEUnits (FileSystemInfoPtr->VolumeSize);
                    FoundName = PoolPrint (L"%s %s Volume", SISize, TypeName);
                    MY_FREE_POOL(SISize);
                    MY_FREE_POOL(FileSystemInfoPtr);
                }

                if (FoundName == NULL) {
                    FoundName = PoolPrint (L"%s Volume", TypeName);
                }
            }
        }
    }

    return FoundName;
} // CHAR16 * GetVolumeName()

BOOLEAN VolumeScanAllowed (
    IN REFIT_VOLUME *Volume,
    IN BOOLEAN       SkipVentoy,
    IN BOOLEAN       SkipRootDir
) {
    UINTN        i;
    CHAR16      *VolGuid;
    CHAR16      *VentoyName;
    BOOLEAN      FoundVentoy;
    BOOLEAN      ScanAllowed;


    if (Volume == NULL          ||
        Volume->VolName == NULL ||
        !Volume->IsReadable
    ) {
        return FALSE;
    }

    if (!SkipRootDir && Volume->RootDir == NULL) {
        return FALSE;
    }

    if (Volume->FSType == FS_TYPE_APFS) {
        if (GlobalConfig.SyncAPFS                    &&
            Volume->VolRole == APFS_VOLUME_ROLE_PREBOOT
        ) {
            return TRUE;
        }

        if (Volume->VolRole != APFS_VOLUME_ROLE_SYSTEM  &&
            Volume->VolRole != APFS_VOLUME_ROLE_PREBOOT &&
            Volume->VolRole != APFS_VOLUME_ROLE_UNDEFINED
        ) {
            return FALSE;
        }
    }

    if (Volume->FSType == FS_TYPE_HFSPLUS                    &&
        GuidsAreEqual (&(Volume->PartTypeGuid), &GuidRecoveryHD)
    ) {
        return FALSE;
    }

    if (Volume->FSType == FS_TYPE_NTFS) {
        if (MyStriCmp (Volume->VolName, L"System Reserved"             )  ||
            MyStriCmp (Volume->VolName, L"System Device Bay"           )  ||
            MyStriCmp (Volume->VolName, L"Microsoft Reserved Partition")
        ) {
            return FALSE;
        }
    }

    if (IsListItem (Volume->VolName,  GlobalConfig.DontScanVolumes) ||
        IsListItem (Volume->FsName,   GlobalConfig.DontScanVolumes) ||
        IsListItem (Volume->PartName, GlobalConfig.DontScanVolumes)
    ) {
        return FALSE;
    }

    if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidWindowsRE)) {
        return FALSE;
    }

    VolGuid = GuidAsString (&(Volume->PartGuid));
    if (IsListItem (VolGuid, GlobalConfig.DontScanVolumes)) {
        ScanAllowed = FALSE;
    }
    else {
        ScanAllowed = TRUE;

        if (!SkipVentoy) {
            i = 0;
            FoundVentoy = FALSE;
            while (
                !FoundVentoy              &&
                GlobalConfig.HandleVentoy &&
                (VentoyName = FindCommaDelimited (VENTOY_NAMES, i++)) != NULL
            ) {
                if (MyStrBegins (VentoyName, Volume->VolName) ||
                    MyStrBegins (VentoyName, Volume->FsName)  ||
                    MyStrBegins (VentoyName, Volume->PartName)
                ) {
                    FoundVentoy = TRUE;
                }
                MY_FREE_POOL(VentoyName);
            } // while

            if (FoundVentoy) {
                if (!FileExists (Volume->RootDir, FALLBACK_FULLNAME)) {
                    ScanAllowed = FALSE;
                }
            }
        }
    }
    MY_FREE_POOL(VolGuid);

    return ScanAllowed;
} // BOOLEAN VolumeScanAllowed()

// Return TRUE if NTFS boot files are found or if Volume is unreadable,
// FALSE otherwise. The idea is to weed out non-boot NTFS volumes from
// BIOS/legacy boot list on Macs. We cannot assume NTFS will be readable,
// so return TRUE if it is unreadable; but if it *IS* readable, return
// TRUE only if Windows boot files are found.
BOOLEAN HasWindowsBiosBootFiles (
    IN REFIT_VOLUME *Volume
) {
    if (Volume->RootDir == NULL) {
        return TRUE;
    }

    // NTLDR   = Windows boot file: NT/200x/XP
    // Bootmgr = Windows boot file: Vista/7/8/10
    return (
        FileExists (Volume->RootDir, L"NTLDR"  ) ||
        FileExists (Volume->RootDir, L"bootmgr")
    );
} // static VOID HasWindowsBiosBootFiles()

VOID ScanVolume (
    IN OUT REFIT_VOLUME *Volume
) {
    EFI_STATUS                 Status;
    EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
    EFI_DEVICE_PATH_PROTOCOL  *NextDevicePath;
    EFI_DEVICE_PATH_PROTOCOL  *DiskDevicePath;
    EFI_DEVICE_PATH_PROTOCOL  *RemainingDevicePath;
    EFI_HANDLE                 WholeDiskHandle;
    UINTN                      PartialLength;
    BOOLEAN                    Bootable;


    #if REFIT_DEBUG > 0
    UINTN    LogLineType;
    CHAR16  *StrSpacer;
    CHAR16  *MsgStr;
    BOOLEAN  HybridLogger = FALSE;
    MY_HYBRIDLOGGER_SET;
    #endif

    // Get device path
    Volume->DevicePath = DuplicateDevicePath (
        DevicePathFromHandle (Volume->DeviceHandle)
    );

    Volume->DiskKind = DISK_KIND_INTERNAL;  // default

    // Get block i/o
    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol, Volume->DeviceHandle,
        &BlockIoProtocol, (VOID **) &(Volume->BlockIO)
    );
    if (EFI_ERROR(Status)) {
        Volume->BlockIO = NULL;

        #if REFIT_DEBUG > 0
        LogLineType = (HybridLogger) ? LOG_LINE_SPECIAL : LOG_LINE_NORMAL;
        ALT_LOG(1, LogLineType, L"Cannot Get BlockIO Protocol in ScanVolume!!");
        #endif
    }
    else if (Volume->BlockIO->Media->BlockSize == 2048) {
        Volume->DiskKind = DISK_KIND_OPTICAL;
    }

    #if REFIT_DEBUG > 0
    UseButJoin = FALSE;
    #endif

    // Detect Device Type
    DevicePath = Volume->DevicePath;
    while (DevicePath != NULL && !IsDevicePathEndType (DevicePath)) {
        NextDevicePath = NextDevicePathNode (DevicePath);

        if (DevicePathType (DevicePath) == MEDIA_DEVICE_PATH) {
           SetPartGuidAndName (Volume, DevicePath);
        }

        if (DevicePathType (DevicePath) == MESSAGING_DEVICE_PATH &&
            (
                DevicePathSubType (DevicePath) == MSG_USB_DP       ||
                DevicePathSubType (DevicePath) == MSG_1394_DP      ||
                DevicePathSubType (DevicePath) == MSG_USB_CLASS_DP ||
                DevicePathSubType (DevicePath) == MSG_FIBRECHANNEL_DP
            )
        ) {
            // USB/FireWire/FC device -> external
            Volume->DiskKind = DISK_KIND_EXTERNAL;
            FoundExternalDisk = TRUE;
        }

        if (DevicePathType    (DevicePath) == MEDIA_DEVICE_PATH &&
            DevicePathSubType (DevicePath) == MEDIA_CDROM_DP
        ) {
            // El Torito entry -> optical disk
            Volume->DiskKind = DISK_KIND_OPTICAL;
        }

        if (DevicePathType (DevicePath) == MESSAGING_DEVICE_PATH) {
            // Make a device path for the whole device
            PartialLength  = (UINT8 *) NextDevicePath - (UINT8 *)(Volume->DevicePath);
            DiskDevicePath = (EFI_DEVICE_PATH_PROTOCOL *) AllocatePool (
                PartialLength + sizeof (EFI_DEVICE_PATH)
            );

            REFIT_CALL_3_WRAPPER(
                gBS->CopyMem, DiskDevicePath,
                Volume->DevicePath, PartialLength
            );

            REFIT_CALL_3_WRAPPER(
                gBS->CopyMem, (UINT8 *) DiskDevicePath + PartialLength,
                EndDevicePath, sizeof (EFI_DEVICE_PATH)
            );

            // Get the handle for that path
            RemainingDevicePath = DiskDevicePath;
            Status = REFIT_CALL_3_WRAPPER(
                gBS->LocateDevicePath, &BlockIoProtocol,
                &RemainingDevicePath, &WholeDiskHandle
            );
            MY_FREE_POOL(DiskDevicePath);
            if (EFI_ERROR(Status)) {
                #if REFIT_DEBUG > 0
                if (DoneHeadings) {
                    if (HybridLogger) {
                        LogLineType = (SkipSpacing) ? LOG_LINE_SAME : LOG_LINE_SPECIAL;
                        StrSpacer   = (SkipSpacing) ? L" ... "      : L"";
                    }
                    else {
                        StrSpacer   = L"";
                        LogLineType = LOG_LINE_NORMAL;
                    }
                    ALT_LOG(1, LogLineType, L"%sCould *NOT* Locate Device Path", StrSpacer);
                    SkipSpacing = (GlobalConfig.LogLevel > 0) ? TRUE : FALSE;
                    UseButJoin  = TRUE;
                }
                #endif
            }
            else {
                // Get the device path for later
                Status = REFIT_CALL_3_WRAPPER(
                    gBS->HandleProtocol, WholeDiskHandle,
                    &DevicePathProtocol, (VOID **) &DiskDevicePath
                );
                if (EFI_ERROR(Status)) {
                    #if REFIT_DEBUG > 0
                    if (DoneHeadings) {
                        if (HybridLogger) {
                            LogLineType = (SkipSpacing) ? LOG_LINE_SAME : LOG_LINE_SPECIAL;
                            StrSpacer   = (SkipSpacing) ? L" ... "      : L"";
                        }
                        else {
                            StrSpacer   = L"";
                            LogLineType = LOG_LINE_NORMAL;
                        }
                        ALT_LOG(1, LogLineType, L"%sCould *NOT* Get DiskDevicePath", StrSpacer);
                        SkipSpacing = (GlobalConfig.LogLevel > 0) ? TRUE : FALSE;
                        UseButJoin  = TRUE;
                    }
                    #endif
                }
                else {
                    Volume->WholeDiskDevicePath = DuplicateDevicePath (DiskDevicePath);
                }

                // Look at the BlockIO protocol
                Status = REFIT_CALL_3_WRAPPER(
                    gBS->HandleProtocol, WholeDiskHandle,
                    &BlockIoProtocol, (VOID **) &Volume->WholeDiskBlockIO
                );
                if (!EFI_ERROR(Status)) {
                    // Check the media block size
                    if (Volume->WholeDiskBlockIO->Media->BlockSize == 2048) {
                        Volume->DiskKind = DISK_KIND_OPTICAL;
                    }
                }
                else {
                    Volume->WholeDiskBlockIO = NULL;

                    #if REFIT_DEBUG > 0
                    if (DoneHeadings) {
                        if (HybridLogger) {
                            LogLineType = (SkipSpacing) ? LOG_LINE_SAME : LOG_LINE_SPECIAL;
                            StrSpacer   = (SkipSpacing) ? L" ... "      : L"";
                        }
                        else {
                            StrSpacer   = L"";
                            LogLineType = LOG_LINE_NORMAL;
                        }
                        ALT_LOG(1, LogLineType, L"%sCould *NOT* Get WholeDiskBlockIO", StrSpacer);
                        SkipSpacing = (GlobalConfig.LogLevel > 0) ? TRUE : FALSE;
                        UseButJoin  = TRUE;
                    }
                    #endif
                }
            } // if/else !EFI_ERROR(Status)
        } // if DevicePathType
        DevicePath = NextDevicePath;
    } // while

    // Scan for bootcode and MBR table
    Bootable = FALSE;
    ScanVolumeBootcode (Volume, &Bootable);
    if (Volume->DiskKind == DISK_KIND_OPTICAL) {
        Bootable = TRUE;
    }

    if (!Bootable) {
        if (Volume->HasBootCode) {
            #if REFIT_DEBUG > 0
            MsgStr = L"Considered Non-Bootable but Boot Code is Present";
            if (DoneHeadings) {
                if (HybridLogger) {
                    LogLineType = (SkipSpacing) ? LOG_LINE_SAME : LOG_LINE_SPECIAL;
                    StrSpacer   = (SkipSpacing) ? L" ... "      : L"";
                }
                else {
                    StrSpacer   = L"";
                    LogLineType = LOG_LINE_NORMAL;
                }
                ALT_LOG(1, LogLineType, L"%s%s!!", StrSpacer, MsgStr);
            }
            LOG_MSG("\n");
            LOG_MSG("** WARN: %s", MsgStr);
            SkipSpacing = (GlobalConfig.LogLevel > 0) ? TRUE : FALSE;
            #endif

            Volume->HasBootCode = FALSE;
        }
    }

    #if REFIT_DEBUG > 0
    MY_HYBRIDLOGGER_OFF;
    #endif

    // Open the root directory of the volume
    Volume->RootDir = LibOpenRoot (Volume->DeviceHandle);

    SetFilesystemName (Volume);
    Volume->VolName = GetVolumeName (Volume);
    SanitiseVolumeName (&Volume);

    if (Volume->HasBootCode                      &&
        Volume->FSType          == FS_TYPE_NTFS  &&
        GlobalConfig.LegacyType == LEGACY_TYPE_MAC1
    ) {
        // VBR boot code found on NTFS, but volume is not actually bootable
        // on Mac unless there are actual boot file, so check for them.
        Volume->HasBootCode = HasWindowsBiosBootFiles (Volume);
    }

    Volume->IsReadable = (Volume->HasBootCode || Volume->RootDir != NULL)
        ? TRUE : FALSE;
} // ScanVolume()

static
VOID ScanExtendedPartition (
    IN OUT REFIT_VOLUME       *WholeDiskVolume,
    IN     MBR_PARTITION_INFO *MbrEntry
) {
    EFI_STATUS          Status;
    UINT32              ExtBase;
    UINT32              ExtCurrent;
    UINT32              NextExtCurrent;
    UINTN               i;
    UINTN               LogicalPartitionIndex;
    UINT8               SectorBuffer[BASE_SIZE];
    BOOLEAN             Bootable;
    REFIT_VOLUME       *Volume;
    MBR_PARTITION_INFO *EMbrTable;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif


    ExtBase = MbrEntry->StartLBA;
    for (ExtCurrent = ExtBase; ExtCurrent; ExtCurrent = NextExtCurrent) {
        // Read Current EMBR
        Status = REFIT_CALL_5_WRAPPER(
            WholeDiskVolume->BlockIO->ReadBlocks, WholeDiskVolume->BlockIO,
            WholeDiskVolume->BlockIO->Media->MediaId, ExtCurrent,
            BASE_SIZE, SectorBuffer
        );

        if (EFI_ERROR(Status)) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            ALT_LOG(1, LOG_LINE_NORMAL, L"Error %d Reading Blocks from Disk", Status);
            #endif

            break;
        }

        if (*((UINT16 *)(SectorBuffer + 510)) != 0xaa55) {
            break;
        }

        EMbrTable = (MBR_PARTITION_INFO *) (SectorBuffer + 446);

        // Scan Logical Partitions in this EMBR
        NextExtCurrent = 0;
        LogicalPartitionIndex = 4;
        for (i = 0; i < 4; i++) {
            if (EMbrTable[i].Size     == 0 ||
                EMbrTable[i].StartLBA == 0 ||
                (
                    EMbrTable[i].Flags != 0x00 &&
                    EMbrTable[i].Flags != 0x80
                )
            ) {
                break;
            }

            if (IS_EXTENDED_PART_TYPE(EMbrTable[i].Type)) {
                // Set Next ExtCurrent
                NextExtCurrent = ExtBase + EMbrTable[i].StartLBA;
                break;
            }

            // Found Logical Partition
            Volume = AllocateZeroPool (sizeof (REFIT_VOLUME));
            Volume->DiskKind          = WholeDiskVolume->DiskKind;
            Volume->IsMbrPartition    = TRUE;
            Volume->MbrPartitionIndex = LogicalPartitionIndex++;

            Volume->VolName = PoolPrint (
                L"Partition %d",
                Volume->MbrPartitionIndex + 1
            );

            Volume->BlockIO          = WholeDiskVolume->BlockIO;
            Volume->BlockIOOffset    = ExtCurrent + EMbrTable[i].StartLBA;
            Volume->WholeDiskBlockIO = WholeDiskVolume->BlockIO;

            ScanMBR  = TRUE;
            Bootable = FALSE;
            ScanVolumeBootcode (Volume, &Bootable);
            if (!Bootable) {
                Volume->HasBootCode = FALSE;
            }
            ScanMBR = FALSE;

            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_SET;
            #endif
            SetVolumeBadgeIcon (Volume);
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_OFF;
            #endif

            AddListElement (
                (VOID ***) &Volumes,
                &VolumesCount, Volume
            );
        } // for i
    } // for ExtCurrent = ExtBase
} // VOID ScanExtendedPartition()

// Check for Multi-Instance APFS Containers
static
VOID VetMultiInstanceAPFS (VOID) {
    UINTN                              i, j;
    BOOLEAN                    GotSystemVol;
    BOOLEAN                 ActiveContainer;

    #if REFIT_DEBUG > 0
    BOOLEAN       AppleRecovery;
    CHAR16       *MsgStrA;
    const CHAR16 *MsgStrB = L"Disabled:- 'Every Instance (APFS) - Apple Recovery Tool'";
    const CHAR16 *MsgStrC = L"Disabled:- 'Synced Volumes (APFS) - Apple Hardware Test'";
    const CHAR16 *MsgStrD = L"Disabled:- 'Synced Volumes (APFS) - Hide/Unhide Entries'";

    // Check if configured to show Apple Recovery
    AppleRecovery = FALSE;
    for (i = 0; i < NUM_TOOLS; i++) {
        switch (GlobalConfig.ShowTools[i]) {
            case TAG_RECOVERY_MAC: AppleRecovery = TRUE; break;
            default:                                  continue;
        } // switch

        if (AppleRecovery) {
            break;
        }
    } // for
    #endif

    if (!GlobalConfig.SyncAPFS) {
        #if REFIT_DEBUG > 0
        if (AppleRecovery) {
            // Log warning if configured to show Apple Recovery
            MsgStrA = L"SyncAPFS is Inactive";
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s ... %s", MsgStrA, MsgStrB);
            LOG_MSG("\n\n");
            LOG_MSG("INFO: %s ... %s", MsgStrA, MsgStrB);
            LOG_MSG("\n\n");
        }
        #endif

        // Act as if we have Multi-Instance APFS
        SingleAPFS = FALSE;

        // Early Return
        return;
    }

    for (j = 0; j < PreBootVolumesCount; j++) {
        ActiveContainer = FALSE;

        for (i = 0; i < VolumesCount; i++) {
            GotSystemVol = (
                (
                    Volumes[i]->VolRole == APFS_VOLUME_ROLE_SYSTEM ||
                    Volumes[i]->VolRole == APFS_VOLUME_ROLE_UNDEFINED
                ) && (
                    Volumes[i]->VolName != NULL
                ) && (
                    StrLen (Volumes[i]->VolName) != 0
                ) && GuidsAreEqual (
                    &(PreBootVolumes[j]->PartGuid),
                    &(Volumes[i]->PartGuid)
                )
            );

            if (GotSystemVol) {
                if (!ActiveContainer) {
                    ActiveContainer = TRUE;
                }
                else {
                    SingleAPFS = FALSE;
                    break;
                }
            }
        } // for

        if (!SingleAPFS) {
            // DA-TAG: Multiple installations in a single APFS Container
            //         Misc features are disabled
            #if REFIT_DEBUG > 0
            MsgStrA = L"Found APFS Container(s) with Multiple macOS Instances";
            LOG_MSG("\n\n");
            LOG_MSG("INFO: %s", MsgStrA);

            if (AppleRecovery) {
                // Log relevant warning if configured to show Apple Recovery
                ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s ... %s", MsgStrA, MsgStrB);
                LOG_MSG("%s      * %s", OffsetNext, MsgStrB);
            }

            ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s ... %s", MsgStrA, MsgStrC);
            LOG_MSG("%s      * %s", OffsetNext, MsgStrC);

            if (GlobalConfig.HiddenTags) {
                // Log relevant warning if configured to hide tags
                ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s ... %s", MsgStrA, MsgStrD);
                LOG_MSG("%s      * %s", OffsetNext, MsgStrD);
            }
            #endif

            // Break out of loop
            break;
        }
    } // for j = 0
} // VOID VetMultiInstanceAPFS()

// Ensure SyncAPFS can be used.
// This is only run when SyncAPFS is active
static
VOID VetSyncAPFS (VOID) {
    UINTN    i, j;
    CHAR16  *CheckName;
    CHAR16  *TweakName;
    BOOLEAN  GotName;


    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;
    CHAR16 *TmpMsg;

    TmpMsg = L"S Y N C   A P F S   V O L U M E S";
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", TmpMsg);
    LOG_MSG("\n\n");
    LOG_MSG("%s", TmpMsg);
    LOG_MSG("\n");
    #endif

    if (PreBootVolumesCount == 0) {
        #if REFIT_DEBUG > 0
        TmpMsg = L"Activated 'disable_apfs_sync' ... Could *NOT* Identify APFS PreBoot Volumes";
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", TmpMsg);
        LOG_MSG("INFO: %s", TmpMsg);
        #endif

        // Disable SyncAPFS if we do not have, or could not identify, any PreBoot volume
        GlobalConfig.SyncAPFS = FALSE;

        // Early Return
        return;
    }

    if (!ValidAPFS) {
        #if REFIT_DEBUG > 0
        TmpMsg = L"Activated 'disable_apfs_sync' ... Could *NOT* Get VolUUID/PartGUID on One/More Key APFS Volumes";
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", TmpMsg);
        LOG_MSG("INFO: %s", TmpMsg);
        #endif

        // Disable SyncAPFS if not 'ValidAPFS'
        GlobalConfig.SyncAPFS = FALSE;

        // Early Return
        return;
    }

    #if REFIT_DEBUG > 0
    TmpMsg = L"ReMap Potential APFS Volume Groups";
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"%s", TmpMsg);
    LOG_MSG("%s:", TmpMsg);

    for (i = 0; i < SystemVolumesCount; i++) {
        MsgStr = PoolPrint (L"Potential System Volume:- '%s'", SystemVolumes[i]->VolName);
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("%s  - %s", OffsetNext, MsgStr);
        MY_FREE_POOL(MsgStr);
    } // for
    #endif

    // Filter '- Data' string tag out of Volume Group name if present
    for (i = 0; i < DataVolumesCount; i++) {
        if (MyStrStr (DataVolumes[i]->VolName, L"- Data")) {
            GotName = FALSE;

            for (j = 0; j < SystemVolumesCount; j++) {
                TweakName = SanitiseString (SystemVolumes[j]->VolName);
                CheckName = PoolPrint (L"%s - Data", TweakName);

                if (MyStriCmp (DataVolumes[i]->VolName, CheckName)) {
                    GotName = TRUE;
                    MY_FREE_POOL(DataVolumes[i]->VolName);
                    DataVolumes[i]->VolName = StrDuplicate (SystemVolumes[j]->VolName);
                }
                else if (!MyStriCmp (SystemVolumes[j]->VolName, TweakName)) {
                    // Check against raw name string if apporpriate
                    MY_FREE_POOL(CheckName);
                    CheckName = PoolPrint (L"%s - Data", SystemVolumes[j]->VolName);

                    if (MyStriCmp (DataVolumes[i]->VolName, CheckName)) {
                        GotName = TRUE;
                        MY_FREE_POOL(DataVolumes[i]->VolName);
                        DataVolumes[i]->VolName = StrDuplicate (SystemVolumes[j]->VolName);
                    }
                }

                MY_FREE_POOL(TweakName);
                MY_FREE_POOL(CheckName);

                if (GotName) {
                    break;
                }
            } // for j = 0
        }
    } // for i = 0

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"Processed %d Potential APFS Volume Group%s",
        SystemVolumesCount, (SystemVolumesCount == 1) ? L"" : L"s"
    );
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);

    if (SystemVolumesCount == 0) {
        LOG_MSG("%s%s", OffsetNext, MsgStr);
    }
    else {
        LOG_MSG("\n\n");
        LOG_MSG("INFO: %s", MsgStr);
    }
    MY_FREE_POOL(MsgStr);
    #endif

    // Check for Multi-Instance APFS Containers
    VetMultiInstanceAPFS();
} // VOID VetSyncAPFS()

#if REFIT_DEBUG > 0
static
CHAR16 * GetApfsRoleString (
    IN APFS_VOLUME_ROLE VolumeRole
) {
    CHAR16 *retval;


    switch (VolumeRole) {
        case APFS_VOLUME_ROLE_UNDEFINED: retval = L"0x00 - APFS Undefined"   ; break;
        case APFS_VOLUME_ROLE_SYSTEM:    retval = L"0x01 - APFS System"      ; break;
        case APFS_VOLUME_ROLE_USER:      retval = L"0x02 - APFS User Home"   ; break;
        case APFS_VOLUME_ROLE_RECOVERY:  retval = L"0x04 - APFS Recovery"    ; break;
        case APFS_VOLUME_ROLE_VM:        retval = L"0x08 - APFS VM"          ; break;
        case APFS_VOLUME_ROLE_PREBOOT:   retval = L"0x10 - APFS Pre-Boot"    ; break;
        case APFS_VOLUME_ROLE_INSTALLER: retval = L"0x20 - APFS Installer"   ; break;
        case APFS_VOLUME_ROLE_DATA:      retval = L"0x40 - APFS Data"        ; break;
        case APFS_VOLUME_ROLE_UPDATE:    retval = L"0xC0 - APFS Snapshot"    ; break;
        case APFS_VOL_ROLE_XART:         retval = L"0x0? - APFS Secured Data"; break;
        case APFS_VOL_ROLE_HARDWARE:     retval = L"0x1? - APFS FirmwareData"; break;
        case APFS_VOL_ROLE_BACKUP:       retval = L"0x2? - APFS Backup (TM)" ; break;
        case APFS_VOL_ROLE_RESERVED_7:   retval = L"0x3? - APFS Reserved 07" ; break;
        case APFS_VOL_ROLE_RESERVED_8:   retval = L"0x4? - APFS Reserved 08" ; break;
        case APFS_VOL_ROLE_ENTERPRISE:   retval = L"0x5? - APFS Enterprise"  ; break;
        case APFS_VOL_ROLE_RESERVED_10:  retval = L"0x6? - APFS Reserved 10" ; break;
        case APFS_VOL_ROLE_PRELOGIN:     retval = L"0x7? - APFS Pre-Login"   ; break;
        default:                         retval = L"0xFF - APFS Unknown Role"; break;
    } // switch

    return retval;
} // static CHAR16 * GetApfsRoleString()
#endif

VOID ScanVolumes (VOID) {
    EFI_STATUS              Status;
    EFI_HANDLE             *Handles;
    REFIT_VOLUME           *Volume;
    REFIT_VOLUME           *WholeDiskVolume;
    MBR_PARTITION_INFO     *MbrTable;
    UINTN                   i, j;
    UINTN                   SectorSum;
    UINTN                   HandleCount;
    UINTN                   HandleIndex;
    UINTN                   VolumeIndex;
    UINTN                   VolumeIndex2;
    UINTN                   PartitionIndex;
    UINT8                  *SectorBuffer1;
    UINT8                  *SectorBuffer2;
    CHAR16                 *VentoyName;
    CHAR16                 *TempUUID;
    CHAR16                 *PartType;
    CHAR16                 *RoleStr;
    BOOLEAN                 DupFlag;
    BOOLEAN                 FoundAPFS;
    BOOLEAN                 FoundVentoy;
    EFI_GUID                VolumeGuid = NULL_GUID_VALUE;
    EFI_GUID               *UuidList;
    APFS_VOLUME_ROLE        VolumeRole;

    #if REFIT_DEBUG > 0
    #if REFIT_DEBUG > 1
    BOOLEAN  CheckMute = FALSE;
    #endif

    CHAR16  *MsgStr;
    CHAR16  *TmpMsg;
    CHAR16  *PartName;
    CHAR16  *PartGUID;
    CHAR16  *VolumeUUID;
    CHAR16  *PartTypeGUID;

    const CHAR16 *ITEMVOLA = L"PARTITION TYPE GUID";
    const CHAR16 *ITEMVOLB = L"PARTITION GUID";
    const CHAR16 *ITEMVOLC = L"PARTITION TYPE";
    const CHAR16 *ITEMVOLD = L"VOLUME UUID";
    const CHAR16 *ITEMVOLE = L"VOLUME ROLE";
    const CHAR16 *ITEMVOLF = L"VOLUME NAME";

    TmpMsg = L"A S S E S S   D E T E C T E D   V O L U M E S";
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", TmpMsg);
    LOG_MSG("\n\n");
    LOG_MSG("%s", TmpMsg);
    #endif

    if (SelfVolRun) {
        // Clear Volume Lists if not scanning for Self Volume
        FreeVolumes (
            &Volumes,
            &VolumesCount
        );
        FreeSyncVolumes();
        ForgetPartitionTables();
    }

    // Get all filesystem handles
    HandleCount = 0;
    Status = LibLocateHandle (
        ByProtocol,
        &BlockIoProtocol, NULL,
        &HandleCount, &Handles
    );
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (
            L"In ScanVolumes ... '%r' While Listing File Systems (Fatal Error)",
            Status
        );
        ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s!!", MsgStr);
        LOG_MSG("\n\n");
        LOG_MSG("** %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        return;
    }

    UuidList = AllocateZeroPool (sizeof (EFI_GUID) * HandleCount);
    if (UuidList == NULL) {
        #if REFIT_DEBUG > 0
        Status = EFI_BUFFER_TOO_SMALL;

        MsgStr = PoolPrint (
            L"In ScanVolumes ... Allocate UuidList:- '%r'",
            Status
        );
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s!!", MsgStr);
        LOG_MSG("\n\n");
        LOG_MSG("** WARN: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        MY_FREE_POOL(Handles);

        return;
    }

    // First Pass: Collect information about all handles
    DoneHeadings = FALSE;
    SkipSpacing  = FALSE;
    FoundAPFS    = FALSE;
    ValidAPFS    =  TRUE;

    #if REFIT_DEBUG > 0
    FoundMBR     = FALSE;
    ScannedOnce  = FALSE;
    FirstVolume  =  TRUE;
    #endif

    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
        Volume = AllocateZeroPool (sizeof (REFIT_VOLUME));
        if (Volume == NULL) {
            MY_FREE_POOL(UuidList);
            MY_FREE_POOL(Handles);

            #if REFIT_DEBUG > 0
            Status = EFI_BUFFER_TOO_SMALL;

            MsgStr = PoolPrint (
                L"In ScanVolumes ... Allocate Volumes:- '%r'",
                Status
            );
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            ALT_LOG(1, LOG_LINE_THIN_SEP, L"%!!s", MsgStr);
            LOG_MSG("\n\n");
            LOG_MSG("** WARN: %s", MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
            #endif

            return;
        }

        Volume->VolRole = APFS_VOLUME_ROLE_UNKNOWN;
        Volume->DeviceHandle = Handles[HandleIndex];
        AddPartitionTable (Volume);
        ScanVolume (Volume);

        UuidList[HandleIndex] = Volume->VolUuid;
        // Deduplicate filesystem UUID so that we do not add duplicate entries for file systems
        // that are part of RAID mirrors. Do not deduplicate ESP partitions though, since unlike
        // normal file systems they are likely to all share the same volume UUID, and it is also
        // unlikely that they are part of software RAID mirrors.
        for (i = 0; i < HandleIndex; i++) {
            if (!GlobalConfig.ScanAllESP) {
                DupFlag = (
                    (CompareMem (&(Volume->VolUuid), &(UuidList[i]), sizeof (EFI_GUID)) == 0) &&
                    (CompareMem (&(Volume->VolUuid), &GuidNull,      sizeof (EFI_GUID)) != 0)
                );
            }
            else {
                DupFlag = (
                    (!GuidsAreEqual (&(Volume->PartTypeGuid), &GuidESP))                      &&
                    (CompareMem (&(Volume->VolUuid), &(UuidList[i]), sizeof (EFI_GUID)) == 0) &&
                    (CompareMem (&(Volume->VolUuid), &GuidNull,      sizeof (EFI_GUID)) != 0)
                );

                if (!DupFlag                                       &&
                    !GuidsAreEqual (&(Volume->VolUuid), &GuidNull) &&
                    GuidsAreEqual (&(Volume->PartTypeGuid), &GuidESP)
                ) {
                    TempUUID = GuidAsString (&Volume->VolUuid);
                    DupFlag  = MyStriCmp (StrSelfUUID, TempUUID);
                    MY_FREE_POOL(TempUUID);
                }
            }

            if (DupFlag) {
                // This is a duplicate filesystem item
                Volume->IsReadable = FALSE;
                break;
            }
        } // for

        if (SelfVolRun) {
            // Update/Create 'Volumes' List if not scanning for Self Volume
            AddListElement (
                (VOID ***) &Volumes,
                &VolumesCount, Volume
            );
        }

        if (Volume->DeviceHandle == SelfLoadedImage->DeviceHandle) {
            SelfVolume = CopyVolume (Volume);
            SelfVolSet = TRUE;
        }

        if (SelfVolRun) {
            #if REFIT_DEBUG > 0
            if (SkipSpacing) {
                LOG_MSG("%s", ENTRY_BELOW_TXT);
            }

            if (!DoneHeadings) {
                BRK_MOD("\n");
            }
            else if (ScannedOnce) {
                if (!SkipSpacing && (HandleIndex % 4) == 0 && (HandleCount - HandleIndex) > 2) {
                    if ((HandleIndex % 40) == 0 && (HandleCount - HandleIndex) > (20 + 2)) {
                        DoneHeadings = FALSE;
                        BRK_MOD("\n\n                   ");
                    }
                    else {
                        BRK_MOD("\n\n");
                    }
                }
                else {
                    BRK_MOD("\n");
                }
            }
            SkipSpacing = FALSE;

            if (!DoneHeadings) {
                LOG_MSG(
                    "%-39s%-39s%-21s%-39s%-27s%s",
                    ITEMVOLA, ITEMVOLB, ITEMVOLC, ITEMVOLD, ITEMVOLE, ITEMVOLF
                );
                LOG_MSG("\n");
                DoneHeadings = TRUE;

                if (FirstVolume) {
                    if (GlobalConfig.LogLevel > 0) {
                        if (FoundMBR || Volume->HasBootCode) {
                            if (Volume->HasBootCode) {
                                LOG_MSG(
                                    "%s for %s",
                                    LEGACY_CODE_TXT,
                                    (Volume->OSName) ? Volume->OSName : UNKNOWN_OS
                                );
                            }
                            if (FoundMBR) {
                                if (Volume->HasBootCode) {
                                    LOG_MSG("%s", (UseButJoin) ? BUT_JOIN_TXT : AND_JOIN_TXT);
                                }
                                LOG_MSG("%s", PARTITION_TABLE_TXT);
                            }
                            LOG_MSG("%s", ENTRY_BELOW_TXT);
                            LOG_MSG("\n");
                        }
                    }
                }
            }
            #endif

#if REFIT_DEBUG > 1
MY_MUTELOGGER_SET;
#endif

            // 'FSTypeName' returns a constant ... Do not free 'PartType'!
            PartType = FSTypeName (Volume);
            if (0);
            else if (FindSubStr (PartType, L"NTFS"    )) Volume->FSType = FS_TYPE_NTFS   ;
            else if (FindSubStr (PartType, L"APFS"    )) Volume->FSType = FS_TYPE_APFS   ;
            else if (FindSubStr (PartType, L"HFS+"    )) Volume->FSType = FS_TYPE_HFSPLUS;
            else if (FindSubStr (PartType, L"ISO-9660")) Volume->FSType = FS_TYPE_ISO9660;
            else if (FindSubStr (PartType, L"ExFAT"   )) Volume->FSType = FS_TYPE_EXFAT  ;

            RoleStr = NULL;
            VolumeRole = APFS_VOLUME_ROLE_UNKNOWN;
            if (0);
            else if (FindSubStr (Volume->VolName, L"APFS/FileVault"          )) RoleStr = L"?* Type Entity-Container";
            else if (MyStriCmp (Volume->VolName,  L"Whole Disk Volume"       )) RoleStr = L" * Type Entity-WholeDisk";
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidESP        )) RoleStr = L" * Part System EFI (ESP)";
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidLinux      )) RoleStr = L" * Part Linux Filesystem";
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidSwap       )) RoleStr = L" * Part Linux SwapVolume";
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidBasicData  )) RoleStr = L" * Type Volume-BasicData";
            else if (FindSubStr (Volume->VolName, L"Optical Disc"            )) RoleStr = L" * Type Entity-OpticDisk";
            else if (FindSubStr (PartType,        L"Apple Raid"              )) RoleStr = L" * Type Entity-AppleRAID";
            else if (AppleFirmware && MyStriCmp (Volume->VolName, L"BOOTCAMP")) RoleStr = L" * Part Windows BootCamp";
            else if (MyStriCmp (Volume->VolName,  L"Boot OS X"               )) RoleStr = L" * Part BootAssist (Mac)";
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidRecoveryHD )) RoleStr = L" * Part RecoveryHD (HFS)";
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidWindowsRE  )) RoleStr = L" * Part RecoveryHD (Win)";
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidReservedMS )) RoleStr = L" * Part ReservedHD (Win)";
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidLuks       )) RoleStr = L" * Type Encrypted Volume";
            else if (MyStriCmp (Volume->VolName,  L"Unknown Volume"          )) RoleStr = L"?? Role Unknown"         ;
            else {
                // DA-TAG: Limit to TianoCore
                #ifndef __MAKEWITH_TIANO
                Status = EFI_NOT_STARTED;
                #else
                Status = RefitGetApfsVolumeInfo (
                    Volume->DeviceHandle, NULL,
                    &VolumeGuid, &VolumeRole
                );
                #endif

                if (!EFI_ERROR(Status)) {
                    if (!FoundAPFS) {
                        FoundAPFS = TRUE;

                        // DA-TAG: Do not scan any volumes called "Recovery" if APFS is present
                        //         Apply the same to 'Update' and 'VM'
                        MergeUniqueStrings (&GlobalConfig.DontScanVolumes, L"Recovery", L',');
                        MergeUniqueStrings (&GlobalConfig.DontScanVolumes, L"Update",   L',');
                        MergeUniqueStrings (&GlobalConfig.DontScanVolumes, L"VM",       L',');
                    }

                    PartType        = L"APFS";
                    Volume->FSType  = FS_TYPE_APFS;
                    Volume->VolUuid = VolumeGuid;
                    Volume->VolRole = VolumeRole;

                    #if REFIT_DEBUG > 0
                    RoleStr = GetApfsRoleString (VolumeRole);
                    #endif

                    // DA-TAG: Update FreeSyncVolumes() if expanding this
                    if (ValidAPFS) {
                        if (Volume->VolRole == APFS_VOLUME_ROLE_RECOVERY) {
                            // Set as 'UnReadable' to boost load speed
                            Volume->IsReadable = FALSE;

                            // Create or add to a list of APFS Recovery volumes
                            AddListElement (
                                (VOID ***) &RecoveryVolumes,
                                &RecoveryVolumesCount, Volume
                            );
                        }
                        else if (Volume->VolRole == APFS_VOLUME_ROLE_DATA) {
                            // Set as 'UnReadable' to boost load speed
                            Volume->IsReadable = FALSE;

                            // Create or add to a list representing APFS VolumeGroups
                            AddListElement (
                                (VOID ***) &DataVolumes,
                                &DataVolumesCount, Volume
                            );
                        }
                        else if (Volume->VolRole == APFS_VOLUME_ROLE_PREBOOT) {
                            // Create or add to a list of APFS PreBoot Volumes
                            AddListElement (
                                (VOID ***) &PreBootVolumes,
                                &PreBootVolumesCount, Volume
                            );
                        }
                        else if (
                            Volume->VolRole == APFS_VOLUME_ROLE_SYSTEM ||
                            Volume->VolRole == APFS_VOLUME_ROLE_UNDEFINED
                        ) {
                            // Create or add to a list of APFS System Volumes
                            AddListElement (
                                (VOID ***) &SystemVolumes,
                                &SystemVolumesCount, Volume
                            );

                            if (!ShouldScan (Volume, MACOSX_LOADER_DIR)) {
                                // Create or add to a list of APFS Volumes not to be scanned
                                AddListElement (
                                    (VOID ***) &SkipApfsVolumes,
                                    &SkipApfsVolumesCount, Volume
                                );
                            }
                        }
                        else {
                            // Set any other APFS Volume as 'UnReadable' to boost load speed
                            Volume->IsReadable = FALSE;
                        }

                        // Flag NULL VolUUID or PartGuid if found and not previously flagged
                        if (Volume->IsReadable &&
                            (
                                GuidsAreEqual (&GuidNull, &(Volume->VolUuid )) ||
                                GuidsAreEqual (&GuidNull, &(Volume->PartGuid))
                            )
                        ) {
                            ValidAPFS = FALSE;
                        }
                    } // if ValidAPFS
                } // if !EFI_ERROR(Status)
            } // if MyStriCmp Volume->VolName

            if (RoleStr != NULL) {
                if (Volume->FSType == FS_TYPE_HFSPLUS &&
                    MyStriCmp (RoleStr, L" * Part RecoveryHD (HFS)")
                ) {
                    // Create or add to a list of HFS+ Recovery volumes
                    AddListElement (
                        (VOID ***) &HfsRecovery,
                        &HfsRecoveryCount, Volume
                    );
                }
            }
            else {
                if (Volume->FSType == FS_TYPE_NTFS) {
                    RoleStr = (MyStriCmp (Volume->VolName, L"System Reserved"))
                        ? L" * Part SysReserve (Win)"
                        : (MyStriCmp (Volume->VolName, L"NTFS Volume"))
                            ? L" * Type Windows (Other)"
                            : L" * Type Windows (Named)";
                }
                else if (Volume->FSType == FS_TYPE_HFSPLUS) {
                    if (GuidsAreEqual (&GuidHFS, &(Volume->PartTypeGuid))) {
                        RoleStr = (FileExists (Volume->RootDir, MACOSX_LOADER_PATH))
                            ? L" * Part macOS Boot (HFS)"
                            : L" * Part Other/Data (HFS)";
                    }
                    else {
                        Volume->FSType = FS_TYPE_UNKNOWN;

                        #if REFIT_DEBUG > 0
                        PartType = L"Unknown";
                        #endif
                    }
                }
            }

            if (RoleStr == NULL) {
                FoundVentoy = FALSE;
                j = 0;
                while (
                    !FoundVentoy              &&
                    GlobalConfig.HandleVentoy &&
                    (VentoyName = FindCommaDelimited (VENTOY_NAMES, j++)) != NULL
                ) {
                    if (MyStrBegins (VentoyName, Volume->VolName)) {
                        RoleStr = L" * Part Ventoy ISO Boot";
                        FoundVentoy = TRUE;
                    }

                    MY_FREE_POOL(VentoyName);
                } // while

                if (RoleStr == NULL) {
                    RoleStr = (Volume->FSType == FS_TYPE_EXFAT)
                        ? L" * Part Data Store"
                        : L"** Role Undefined";
                }
            }

#if REFIT_DEBUG > 1
MY_MUTELOGGER_OFF;
#endif

            #if REFIT_DEBUG > 0
            // Allocate Pools for Log Details
            PartName     = StrDuplicate (PartType);
            PartGUID     = GuidAsString (&(Volume->PartGuid));
            PartTypeGUID = GuidAsString (&(Volume->PartTypeGuid));
            VolumeUUID   = GuidAsString (&(Volume->VolUuid));

            // Control PartName Length
            LimitStringLength (PartName, 18);

            MsgStr = PoolPrint (
                L"%-36s : %-36s : %-18s : %-36s : %-24s : %s",
                PartTypeGUID, PartGUID, PartName,
                VolumeUUID, RoleStr, Volume->VolName
            );

            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("%s", MsgStr);
            MY_FREE_POOL(MsgStr);

            MY_FREE_POOL(PartName);
            MY_FREE_POOL(PartGUID);
            MY_FREE_POOL(PartTypeGUID);
            MY_FREE_POOL(VolumeUUID);
            #endif
        }

        #if REFIT_DEBUG > 0
        FoundMBR      = FALSE;
        UseButJoin    = FALSE;
        FirstVolume   = FALSE;
        ScannedOnce   =  TRUE;
        #endif
    } // for: first pass

    MY_FREE_POOL(UuidList);
    MY_FREE_POOL(Handles);

    if (!SelfVolSet || !SelfVolRun) {
        SelfVolRun = TRUE;

        return;
    }

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"%-39s%-39s%-21s%-39s%-27s%s",
        ITEMVOLA, ITEMVOLB, ITEMVOLC, ITEMVOLD, ITEMVOLE, ITEMVOLF
    );
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("%s", OffsetNext);
    LOG_MSG("%s", MsgStr);
    LOG_MSG("\n\n");
    MY_FREE_POOL(MsgStr);
    #endif

    // Second pass: relate partitions and whole disk devices
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        Volume = Volumes[VolumeIndex];
        // Check MBR partition table for extended partitions
        if (Volume->BlockIOOffset     ==    0 &&
            Volume->MbrPartitionTable != NULL &&
            Volume->WholeDiskBlockIO  != NULL &&
            Volume->WholeDiskBlockIO  == Volume->BlockIO
        ) {
            MbrTable = Volume->MbrPartitionTable;
            for (PartitionIndex = 0; PartitionIndex < 4; PartitionIndex++) {
                if (IS_EXTENDED_PART_TYPE(MbrTable[PartitionIndex].Type)) {
                    ScanExtendedPartition (Volume, MbrTable + PartitionIndex);
                }
            }
        }

        // Search for corresponding whole disk volume entry
        WholeDiskVolume = NULL;
        if (Volume->BlockIO          != NULL &&
            Volume->WholeDiskBlockIO != NULL &&
            Volume->WholeDiskBlockIO != Volume->BlockIO
        ) {
            for (VolumeIndex2 = 0; VolumeIndex2 < VolumesCount; VolumeIndex2++) {
                if (Volumes[VolumeIndex2]->BlockIOOffset == 0 &&
                    Volumes[VolumeIndex2]->BlockIO == Volume->WholeDiskBlockIO
                ) {
                    WholeDiskVolume = Volumes[VolumeIndex2];

                    break;
                }
            } // for
        }

        if (WholeDiskVolume && WholeDiskVolume->MbrPartitionTable) {
            // Check if this volume is one of the partitions in the table
            SectorBuffer1 = AllocatePool (BASE_SIZE);
            SectorBuffer2 = AllocatePool (BASE_SIZE);
            MbrTable = WholeDiskVolume->MbrPartitionTable;
            for (PartitionIndex = 0; PartitionIndex < 4; PartitionIndex++) {
                // check size
                if ((UINT64)(MbrTable[PartitionIndex].Size) !=
                    Volume->BlockIO->Media->LastBlock + 1
                ) {
                    continue;
                }

                // Compare boot sector read through offset vs. directly
                Status = REFIT_CALL_5_WRAPPER(
                    Volume->BlockIO->ReadBlocks, Volume->BlockIO,
                    Volume->BlockIO->Media->MediaId, Volume->BlockIOOffset,
                    BASE_SIZE, SectorBuffer1
                );
                if (EFI_ERROR(Status)) {
                    break;
                }

                Status = REFIT_CALL_5_WRAPPER(
                    Volume->WholeDiskBlockIO->ReadBlocks, Volume->WholeDiskBlockIO,
                    Volume->WholeDiskBlockIO->Media->MediaId, MbrTable[PartitionIndex].StartLBA,
                    BASE_SIZE, SectorBuffer2
                );
                if (EFI_ERROR(Status)) {
                    break;
                }

                if (CompareMem (SectorBuffer1, SectorBuffer2, BASE_SIZE) != 0) {
                    continue;
                }

                SectorSum = 0;
                for (i = 0; i < BASE_SIZE; i++) {
                    SectorSum += SectorBuffer1[i];
                }
                if (SectorSum < 1000) {
                    continue;
                }

                // DA-TAG: Investigate this
                //         Mark entry as non-bootable if it is an extended partition
                //
                // We are now reasonably sure the association is correct
                Volume->IsMbrPartition = TRUE;
                Volume->MbrPartitionIndex = PartitionIndex;
                if (Volume->VolName == NULL) {
                    Volume->VolName = PoolPrint (
                        L"Partition %d",
                        PartitionIndex + 1
                    );
                }

                break;
            } // for PartitionIndex

            MY_FREE_POOL(SectorBuffer1);
            MY_FREE_POOL(SectorBuffer2);
        }
    } // for VolumeIndex

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"Assessed %d Volume%s",
        VolumesCount, (VolumesCount == 1) ? L"" : L"s"
    );
    LOG_MSG("INFO: %s", MsgStr); // Skip Line Break
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    if (SelfVolRun && GlobalConfig.SyncAPFS) {
        VetSyncAPFS();
    }

    #if REFIT_DEBUG > 0
    MuteLogger = FALSE; /* Explicit For FB Infer */
    LOG_MSG("\n\n");
    #endif
} // VOID ScanVolumes()

VOID LoadVolumeBadgeIcon (
    IN OUT REFIT_VOLUME  **Volume
) {
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);

    if (!AllowGraphicsMode) {
        BREAD_CRUMB(L"%a:  A1 - END:- VOID - (Skip VolumeBadge Check ... DirectBoot or Text Mode is Active)", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_BADGES) {
        BREAD_CRUMB(L"%a:  A2 - END:- VOID - (Skip VolumeBadge Check ... 'HideUI Badges/All' is Active)", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    if ((*Volume)->VolBadgeImage != NULL) {
        BREAD_CRUMB(L"%a:  A3 - END:- VOID - (Skip VolumeBadge Check ... Image Already Set)", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    // Set volume badge icon
    SetVolumeBadgeIcon (*Volume);

    #if REFIT_DEBUG > 0
    if ((*Volume)->VolBadgeImage == NULL) {
        ALT_LOG(1, LOG_LINE_NORMAL, L"VolumeBadge *NOT* Found");
    }
    #endif

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID LoadVolumeBadgeIcon()

VOID LoadVolumeIcon (
    IN OUT REFIT_VOLUME  *Volume
) {
    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr;
    #endif

    CHAR16  *BootDirectoryName;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);

    if (!AllowGraphicsMode) {
        #if REFIT_DEBUG > 0
        // MsgStr OK below as first use then exit
        MsgStr = (GlobalConfig.DirectBoot) ? L"'DirectBoot' is Active" : L"Running in Text Mode";
        ALT_LOG(1, LOG_THREE_STAR_MID, L"Skip '.VolumeIcon' Check ... %s", MsgStr);
        #endif

        BREAD_CRUMB(L"%a:  A1 - END:- VOID - (DirectBoot or Text Mode is Active)", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    if (GlobalConfig.HiddenIconsIgnore) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"Skip '.VolumeIcon' Check ... Config Setting is Active:- 'hidden_icons_ignore'"
        );
        #endif

        BREAD_CRUMB(L"%a:  A2 - END:- VOID - (Skip Check ... HiddenIconsIgnore is Active)", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    // Skip APFS system volumes when SyncAPFS is active
    if (GlobalConfig.SyncAPFS && IsSystemVolume (Volume)) {
        // Early Return
        return;
    }

    // Load custom volume icon for internal/external disks if present
    if (Volume->VolIconImage == NULL) {
        if (Volume->DiskKind == DISK_KIND_INTERNAL ||
            (
                GlobalConfig.HiddenIconsExternal &&
                Volume->DiskKind == DISK_KIND_EXTERNAL
            )
        ) {
            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (
                L"Set '.VolumeIcon' Icon for '%s'",
                Volume->VolName
            );

            ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
            MY_FREE_POOL(MsgStr);
            #endif

            BootDirectoryName = RefitGetBootPathName (Volume->DevicePath);
            if (BootDirectoryName != NULL) {
                Volume->VolIconImage = egLoadIconAnyType (
                    Volume->RootDir,
                    BootDirectoryName,
                    L".VolumeIcon",
                    GlobalConfig.IconSizes[ICON_SIZE_BIG]
                );
                MY_FREE_POOL(BootDirectoryName);
            }

            if (Volume->VolIconImage == NULL) {
                Volume->VolIconImage = egLoadIconAnyType (
                    Volume->RootDir,
                    L"",
                    L".VolumeIcon",
                    GlobalConfig.IconSizes[ICON_SIZE_BIG]
                );
            }
        }
        else {
            #if REFIT_DEBUG > 0
            if (Volume->DiskKind != DISK_KIND_EXTERNAL) {
                ALT_LOG(1, LOG_THREE_STAR_MID,
                    L"Skip Setting '.VolumeIcon' Icon for '%s' ... Volume *IS NOT* 'Internal'",
                    Volume->VolName
                );
            }
            else {
                ALT_LOG(1, LOG_THREE_STAR_MID,
                    L"Skip Setting '.VolumeIcon' Icon for External Volume: '%s' ... Config Setting *IS NOT* Active:- 'hidden_icons_external'",
                    Volume->VolName
                );
            }
            #endif
        }
    }

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID LoadVolumeIcon()

//
// file and dir functions
//

BOOLEAN FileExists (
    IN EFI_FILE_PROTOCOL *BaseDir,
    IN CHAR16            *RelativePath
) {
    EFI_STATUS      Status;
    EFI_FILE_HANDLE TestFile;


    if (BaseDir != NULL) {
        Status = REFIT_CALL_5_WRAPPER(
            BaseDir->Open, BaseDir,
            &TestFile, RelativePath,
            EFI_FILE_MODE_READ, 0
        );
        if (!EFI_ERROR(Status)) {
            REFIT_CALL_1_WRAPPER(TestFile->Close, TestFile);

            return TRUE;
        }
    }

    return FALSE;
} // BOOLEAN FileExists()

static
EFI_STATUS DirNextEntry (
    IN     EFI_FILE_PROTOCOL      *Directory,
    OUT    EFI_FILE_INFO         **DirEntry,
    IN     UINTN                   FilterMode
) {
    EFI_STATUS  Status;
    UINTN       LastBufferSize;
    UINTN       BufferSize;
    VOID       *Buffer;
    INTN        IterCount;

    #if REFIT_DEBUG > 0
    BOOLEAN FirstRun;
    CHAR16  *MsgStr;
    CHAR16  *TmpMsg;
    #endif


    //LOG_SEP(L"X");
    //LOG_INCREMENT();
    //BREAD_CRUMB(L"%a:  1 - START", __func__);

    //BREAD_CRUMB(L"%a:  2", __func__);
    #if REFIT_DEBUG > 0
    FirstRun = TRUE;
    #endif
    for (;;) {
        //LOG_SEP(L"X");
        //BREAD_CRUMB(L"%a:  2a 1 - FOR LOOP:- START", __func__);
        *DirEntry = NULL;

        // Read next directory entry
        //BREAD_CRUMB(L"%a:  2a 2", __func__);
        LastBufferSize = BufferSize = 256;
        Buffer         = AllocatePool (BufferSize);
        if (Buffer == NULL) {
            //BREAD_CRUMB(L"%a:  2a 2a 1 - END:- return EFI_STATUS = 'Bad Buffer Size'", __func__);

            return EFI_BAD_BUFFER_SIZE;
        }

        //BREAD_CRUMB(L"%a:  2a 3", __func__);
        for (IterCount = 0; ; IterCount++) {
            //LOG_SEP(L"X");
            //BREAD_CRUMB(L"%a:  2a 3a 1 - FOR LOOP:- START", __func__);
            Status = REFIT_CALL_3_WRAPPER(
                Directory->Read, Directory,
                &BufferSize, Buffer
            );

            //BREAD_CRUMB(L"%a:  2a 3a 2", __func__);
            if (Status != EFI_BUFFER_TOO_SMALL || IterCount > 3) {
                //BREAD_CRUMB(L"%a:  2a 3a 2a 1", __func__);
                #if REFIT_DEBUG > 0
                if (!FirstRun) {
                    //BREAD_CRUMB(L"%a:  2a 3a 2a 1a 1", __func__);
                    if (IterCount > 3) {
                        //BREAD_CRUMB(L"%a:  2a 3a 2a 1a 1a 1", __func__);
                        TmpMsg = L"IterCount > 3";
                    }
                    else {
                        //BREAD_CRUMB(L"%a:  2a 3a 2a 1a 1b 1", __func__);
                        TmpMsg = L"OK";
                    }
                    //BREAD_CRUMB(L"%a:  2a 3a 2a 1a 2", __func__);
                    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", TmpMsg);
                    LOG_MSG(":- '%s ... Break'", TmpMsg);
                }
                #endif

                //BREAD_CRUMB(L"%a:  2a 3a 2a 2 - FOR LOOP:- BREAK ... Status/IterCount", __func__);
                //LOG_SEP(L"X");

                break;
            }
            else {
                //BREAD_CRUMB(L"%a:  2a 3a 2b 1", __func__);
                #if REFIT_DEBUG > 0
                if (!FirstRun) {
                    //BREAD_CRUMB(L"%a:  2a 3a 2b 1a 1", __func__);
                    TmpMsg = L"NOT OK!!";
                    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", TmpMsg);
                    LOG_MSG(":- '%s'", TmpMsg);
                }
                #endif
                //BREAD_CRUMB(L"%a:  2a 3a 2b 2", __func__);
            }

            //BREAD_CRUMB(L"%a:  2a 3a 3", __func__);
            #if REFIT_DEBUG > 0
            LOG_MSG("\n");
            #endif
            if (BufferSize <= LastBufferSize) {
                //BREAD_CRUMB(L"%a:  2a 3a 3a 1", __func__);
                #if REFIT_DEBUG > 0
                MsgStr = PoolPrint (
                    L"Bad Filesystem Driver Buffer Size Request %d (was %d) ... Using %d Instead",
                    BufferSize,
                    LastBufferSize,
                    LastBufferSize * 2
                );
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("%s", MsgStr);
                MY_FREE_POOL(MsgStr);
                #endif

                BufferSize = LastBufferSize * 2;
            }
            else {
                //BREAD_CRUMB(L"%a:  2a 3a 3b 1", __func__);
                #if REFIT_DEBUG > 0
                MsgStr = PoolPrint (
                    L"Resizing DirEntry Buffer from %d to %d Bytes",
                    LastBufferSize, BufferSize
                );
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("%s", MsgStr);
                MY_FREE_POOL(MsgStr);
                #endif
            }
            #if REFIT_DEBUG > 0
            FirstRun = FALSE;
            #endif

            //BREAD_CRUMB(L"%a:  2a 3a 4", __func__);
            Buffer = EfiReallocatePool (
                Buffer, LastBufferSize, BufferSize
            );
            LastBufferSize = BufferSize;

            //BREAD_CRUMB(L"%a:  2a 3a 5 - FOR LOOP:- END", __func__);
            //LOG_SEP(L"X");
        } // for IterCount = 0

        #if REFIT_DEBUG > 0
        FirstRun = TRUE;
        #endif

        //BREAD_CRUMB(L"%a:  2a 4", __func__);
        if (EFI_ERROR(Status)) {
            //BREAD_CRUMB(L"%a: 2a 4a 1 - FOR LOOP:- BREAK ... Status Error", __func__);
            //LOG_SEP(L"X");

            MY_FREE_POOL(Buffer);
            break;
        }

        // Check for End of Listing
        //BREAD_CRUMB(L"%a:  2a 5", __func__);
        if (BufferSize == 0) {
            //BREAD_CRUMB(L"%a: 2a 5a 1 - FOR LOOP:- BREAK ... End of Listing", __func__);
            //LOG_SEP(L"X");

            // End of Directory Listing
            MY_FREE_POOL(Buffer);
            break;
        }

        // Entry is ready to be returned
        //BREAD_CRUMB(L"%a:  2a 6", __func__);
        *DirEntry = (EFI_FILE_INFO *) Buffer;

        // Filter results
        //BREAD_CRUMB(L"%a:  2a 7", __func__);
        if (FilterMode == 1) {
            //BREAD_CRUMB(L"%a:  2a 7a 1", __func__);
            // Only return directories
            if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY)) {
                //BREAD_CRUMB(L"%a:  2a 7a 1a 1 - FOR LOOP:- BREAK ... EFI_FILE_DIRECTORY", __func__);
                //LOG_SEP(L"X");

                break;
            }
        }
        else if (FilterMode == 2) {
            //BREAD_CRUMB(L"%a:  2a 7b 1", __func__);
            // Only return files
            if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY) == 0) {
                //BREAD_CRUMB(L"%a:  2a 7b 1a 1 - FOR LOOP:- BREAK ... EFI_FILE_DIRECTORY == 0", __func__);
                //LOG_SEP(L"X");

                break;
            }
        }

        //BREAD_CRUMB(L"%a:  3a 8 - FOR LOOP:- END ... No Filter or Unknown Filter", __func__);
        //LOG_SEP(L"X");

        // No Filter or Unknown Filter -> Return Everything
    } // for ;;

    //BREAD_CRUMB(L"%a:  3 - END:- return EFI_STATUS Status = '%r'", __func__,
    //    Status
    //);
    //LOG_DECREMENT();
    //LOG_SEP(L"X");

    return Status;
} // EFI_STATUS DirNextEntry()

VOID DirIterOpen (
    IN  EFI_FILE_PROTOCOL       *BaseDir,
    IN  CHAR16                  *RelativePath OPTIONAL,
    OUT REFIT_DIR_ITER          *DirIter
) {
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    BREAD_CRUMB(L"%a:  2", __func__);
    if (RelativePath == NULL) {
        BREAD_CRUMB(L"%a:  2a 1 - RelativePath == NULL", __func__);
        DirIter->LastStatus     = EFI_SUCCESS;
        DirIter->DirHandle      = BaseDir;
        DirIter->CloseDirHandle = FALSE;
    }
    else {
        BREAD_CRUMB(L"%a:  2b 1 - RelativePath != NULL", __func__);
        DirIter->LastStatus = REFIT_CALL_5_WRAPPER(
            BaseDir->Open, BaseDir,
            &(DirIter->DirHandle), RelativePath,
            EFI_FILE_MODE_READ, 0
        );
        DirIter->CloseDirHandle = EFI_ERROR(DirIter->LastStatus) ? FALSE : TRUE;
    }
    BREAD_CRUMB(L"%a:  3 - CloseDirHandle = '%s'", __func__,
        (DirIter->CloseDirHandle) ? L"TRUE" : L"FALSE"
    );

    BREAD_CRUMB(L"%a:  4 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID DirIterOpen()

#if defined(__MAKEWITH_TIANO)
EFI_UNICODE_COLLATION_PROTOCOL * OcUnicodeCollationEngInstallProtocol (IN BOOLEAN  Reinstall);
#endif

BOOLEAN RefitMetaiMatch (
    IN CHAR16 *String,
    IN CHAR16 *Pattern
) {
#if defined (__MAKEWITH_GNUEFI)
    return MetaiMatch (String, Pattern);
#elif defined(__MAKEWITH_TIANO)
    EFI_STATUS Status;
    static EFI_UNICODE_COLLATION_PROTOCOL *UnicodeCollationEng = NULL;


    if (UnicodeCollationEng == NULL) {
        UnicodeCollationEng = OcUnicodeCollationEngInstallProtocol (GlobalConfig.UnicodeCollation);
    }
    if (UnicodeCollationEng != NULL) {
        return UnicodeCollationEng->MetaiMatch (UnicodeCollationEng, String, Pattern);
    }

    // DA-TAG: Fallback on original upstream implementation
    //         Should not get here when support is present
    Status = REFIT_CALL_3_WRAPPER(
        gBS->LocateProtocol, &gEfiUnicodeCollation2ProtocolGuid,
        NULL, (VOID **) &UnicodeCollationEng
    );
    if (EFI_ERROR(Status)) {
        REFIT_CALL_3_WRAPPER(
            gBS->LocateProtocol, &gEfiUnicodeCollationProtocolGuid,
            NULL, (VOID **) &UnicodeCollationEng
        );
    }
#endif

    return FALSE;
} // BOOLEAN RefitMetaiMatch()

BOOLEAN DirIterNext (
    IN  OUT REFIT_DIR_ITER  *DirIter,
    IN      UINTN            FilterMode,
    IN      CHAR16          *FilePattern OPTIONAL,
    OUT     EFI_FILE_INFO  **DirEntry
) {
    UINTN          i;
    BOOLEAN        Found;
    CHAR16        *OnePattern;
    EFI_FILE_INFO *LastFileInfo;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    BREAD_CRUMB(L"%a:  2", __func__);
    if (EFI_ERROR(DirIter->LastStatus)) {
        BREAD_CRUMB(L"%a:  2a 1 - END:- return BOOLEAN FALSE on DirIter->LastStatus Error", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Stop Iteration
        return FALSE;
    }

    BREAD_CRUMB(L"%a:  3", __func__);
    for (;;) {
        LOG_SEP(L"X");
        BREAD_CRUMB(L"%a:  3a 1 - FOR LOOP:- START", __func__);

        BREAD_CRUMB(L"%a:  3a 2", __func__);
        DirIter->LastStatus = DirNextEntry (
            DirIter->DirHandle,
            &LastFileInfo,
            FilterMode
        );

        BREAD_CRUMB(L"%a:  3a 3", __func__);
        if (EFI_ERROR(DirIter->LastStatus) || LastFileInfo == NULL) {
            BREAD_CRUMB(L"%a:  3a 3a 1 - FOR LOOP END:- return BOOLEAN FALSE ... ERROR DirIter->LastStatus", __func__);
            LOG_DECREMENT();
            LOG_SEP(L"X");

            return FALSE;
        }

        BREAD_CRUMB(L"%a:  3a 4", __func__);
        if (FilePattern == NULL || LastFileInfo->Attribute & EFI_FILE_DIRECTORY) {
            BREAD_CRUMB(L"%a:  3a 4a 1 - FOR LOOP END:- FilePattern == NULL ... return BOOLEAN TRUE", __func__);
            LOG_DECREMENT();
            LOG_SEP(L"X");

            *DirEntry = LastFileInfo;
            return TRUE;
        }

        BREAD_CRUMB(L"%a:  3a 5", __func__);
        i     =     0;
        Found = FALSE;
        while (
            !Found &&
            (OnePattern = FindCommaDelimited (FilePattern, i++)) != NULL
        ) {
            BREAD_CRUMB(L"%a:  3a 5a 1 - WHILE LOOP:- START ... Seek MetaiMatch Pattern", __func__);
            if (RefitMetaiMatch (LastFileInfo->FileName, OnePattern)) {
                BREAD_CRUMB(L"%a:  3a 5a 1a 1", __func__);
                Found = TRUE;
            }
            MY_FREE_POOL(OnePattern);
            BREAD_CRUMB(L"%a:  3a 5a 2 - WHILE LOOP:- END", __func__);
        } // while

        BREAD_CRUMB(L"%a:  3a 6", __func__);
        if (Found) {
            BREAD_CRUMB(L"%a:  3a 6a 1 - FOR LOOP END:- Found == TRUE ... return BOOLEAN TRUE", __func__);
            LOG_DECREMENT();
            LOG_SEP(L"X");

            *DirEntry = LastFileInfo;
            return TRUE;
        }
        BREAD_CRUMB(L"%a:  3a 7", __func__);
        MY_FREE_POOL(LastFileInfo);

        BREAD_CRUMB(L"%a:  3a 8 - FOR LOOP:- END", __func__);
        LOG_SEP(L"X");
    } // for
    BREAD_CRUMB(L"%a:  4 - END:- return BOOLEAN TRUE", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return TRUE;
} // BOOLEAN DirIterNext()

EFI_STATUS DirIterClose (
    IN OUT REFIT_DIR_ITER *DirIter
) {
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    BREAD_CRUMB(L"%a:  2", __func__);
    if ((DirIter->CloseDirHandle) && (DirIter->DirHandle->Close)) {
        //BREAD_CRUMB(L"%a:  2a 1", __func__);
        REFIT_CALL_1_WRAPPER(DirIter->DirHandle->Close, DirIter->DirHandle);
    }

    BREAD_CRUMB(L"%a:  3 - END:- return EFI_STATUS DirIter->LastStatus = '%r'", __func__,
        DirIter->LastStatus
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return DirIter->LastStatus;
} // EFI_STATUS DirIterClose()

//
// file name manipulation
//

// Returns the filename portion (minus path name) of the
// specified file
CHAR16 * Basename (
    IN CHAR16 *Path
) {
    CHAR16  *FileName;
    UINTN    i;


    FileName = Path;

    if (Path != NULL) {
        for (i = StrLen (Path); i > 0; i--) {
            if (Path[i-1] == '\\' || Path[i-1] == '/') {
                FileName = Path + i;
                break;
            }
        }
    }

    return StrDuplicate (FileName);
}

// Remove the .efi extension from FileName -- for instance, if FileName is
// "fred.efi", returns "fred". If the filename contains no .efi extension,
// returns a copy of the original input.
CHAR16 * StripEfiExtension (
    IN CHAR16 *FileName
) {
    UINTN   Length;
    CHAR16 *Copy;


    if (FileName == NULL) {
        return NULL;
    }

    Copy = StrDuplicate (FileName);
    Length = StrLen (Copy);
    if ((Length >= 4) && MyStriCmp (&Copy[Length - 4], L".efi")) {
        Copy[Length - 4] = 0;
    }

    return Copy;
} // CHAR16 *StripExtension()

//
// memory string search
//

INTN FindMem (
    IN VOID  *Buffer,
    IN UINTN  BufferLength,
    IN VOID  *SearchString,
    IN UINTN  SearchStringLength
) {
    UINT8 *BufferPtr;
    UINTN  Offset;


    BufferPtr = Buffer;
    BufferLength -= SearchStringLength;
    for (Offset = 0; Offset < BufferLength; Offset++, BufferPtr++) {
        if (CompareMem (BufferPtr, SearchString, SearchStringLength) == 0) {
            return (INTN) Offset;
        }
    }

    return -1;
} // INTN FindMem

// Takes an input pathname (*Path) and returns the part of the filename from
// the final dot onwards, converted to lowercase. Returns a NULL string when
// the input is NULL or does not include any dots.
// The caller is responsible for freeing memory allocated to the return value.
CHAR16 * FindExtension (
    IN CHAR16 *Path
) {
    INTN       i;
    CHAR16    *Extension;
    BOOLEAN    FoundSlash;
    BOOLEAN    Found;


    if (Path == NULL) {
        return NULL;
    }

    Extension = AllocateZeroPool (sizeof (CHAR16));
    if (Extension == NULL) {
        return NULL;
    }

    i = StrLen (Path);
    FoundSlash = Found = FALSE;
    while (!Found && !FoundSlash && i >= 0) {
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

    if (!Found) {
        MY_FREE_POOL(Extension);
    }
    else {
        MergeStrings (&Extension, &Path[i], 0);
        ToLower (Extension);
    }

    return Extension;
} // CHAR16 *FindExtension()

// Takes an input pathname (*Path) and locates the final directory component
// of that name. For instance, if the input path is 'EFI\foo\bar.efi', this
// function returns the string 'foo'.
// Assumes the pathname is separated with backslashes.
CHAR16 * FindLastDirName (
    IN CHAR16 *Path
) {
    UINTN   i;
    UINTN   PathLength;
    UINTN   CopyLength;
    UINTN   EndOfElement;
    UINTN   StartOfElement;
    CHAR16 *Found;


    if (Path == NULL) {
        return NULL;
    }

    PathLength = StrLen (Path);
    // Find start and end of target element
    EndOfElement = StartOfElement = 0;
    for (i = 0; i < PathLength; i++) {
        if (Path[i] == '\\') {
            StartOfElement = EndOfElement;
            EndOfElement   = i;
        }
    } // for

    // Extract the target element
    Found = NULL;
    if (EndOfElement > 0) {
        while ((StartOfElement < PathLength) && (Path[StartOfElement] == '\\')) {
            StartOfElement++;
        } // while

        EndOfElement--;
        if (EndOfElement >= StartOfElement) {
            CopyLength = EndOfElement - StartOfElement + 1;
            Found = StrDuplicate (&Path[StartOfElement]);
            if (Found != NULL) {
                Found[CopyLength] = 0;
            }
        }
    }

    return Found;
} // CHAR16 *FindLastDirName()

// Returns the directory portion of a pathname. For instance,
// if FullPath is 'EFI\foo\bar.efi', this function returns the
// string 'EFI\foo'. The calling function is responsible for
// freeing the returned string's memory.
CHAR16 * FindPath (
    IN CHAR16 *FullPath
) {
    UINTN   i;
    UINTN   LastBackslash;
    CHAR16 *PathOnly;


    if (FullPath == NULL) {
        return NULL;
    }

    LastBackslash = 0;
    for (i = 0; i < StrLen (FullPath); i++) {
        if (FullPath[i] == '\\') {
            LastBackslash = i;
        }
    }

    PathOnly = StrDuplicate (FullPath);
    if (PathOnly != NULL) {
        PathOnly[LastBackslash] = 0;
    }

    return PathOnly;
}

// Takes an input loadpath, splits it into disk and filename components, finds a matching
// DeviceVolume, and returns that and the filename (*loader).
VOID FindVolumeAndFilename (
    IN  EFI_DEVICE_PATH_PROTOCOL  *loadpath,
    OUT REFIT_VOLUME             **DeviceVolume,
    OUT CHAR16                   **loader
) {
    CHAR16 *DeviceString, *VolumeDeviceString, *Temp;
    UINTN i;
    BOOLEAN Found;


    if (loader       == NULL ||
        loadpath     == NULL ||
        DeviceVolume == NULL
    ) {
        return;
    }

    MY_FREE_POOL(*loader);
    MY_FREE_POOL(*DeviceVolume);

    DeviceString = DevicePathToStr (loadpath);
    *loader      = SplitDeviceString (DeviceString);

    i = 0;
    Found = FALSE;
    while (!Found && i < VolumesCount) {
        if (Volumes[i]->DevicePath == NULL) {
            i++;
            continue;
        }

        VolumeDeviceString = DevicePathToStr (Volumes[i]->DevicePath);
        Temp               = SplitDeviceString (VolumeDeviceString);

        if (MyStrStr (VolumeDeviceString, DeviceString)) {
            Found         = TRUE;
            *DeviceVolume = Volumes[i];
        }
        i++;

        MY_FREE_POOL(Temp);
        MY_FREE_POOL(VolumeDeviceString);
    } // while

    MY_FREE_POOL(DeviceString);
} // VOID FindVolumeAndFilename()

// Splits a volume/filename string (e.g., "fs0:\EFI\BOOT") into separate
// volume and filename components (e.g., "fs0" and "\EFI\BOOT"), returning
// the filename component in the original *Path variable and the split-off
// volume component in the *VolName variable.
// Returns TRUE if both components are found, FALSE otherwise.
BOOLEAN SplitVolumeAndFilename (
    IN  OUT CHAR16 **Path,
        OUT CHAR16 **VolName
) {
    UINTN   Length, i;
    CHAR16 *Filename;


    if (*Path == NULL) {
        return FALSE;
    }

    MY_FREE_POOL(*VolName);

    Length = StrLen (*Path);
    i = 0;
    while ((i < Length) && ((*Path)[i] != L':')) {
        i++;
    }

    if (i < Length) {
        Filename   =  StrDuplicate ((*Path) + i + 1);
        (*Path)[i] =  0;
        *VolName   = *Path;
        *Path      =  Filename;

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
VOID SplitPathName (
    IN     CHAR16  *InPath,
    IN OUT CHAR16 **VolName,
    IN OUT CHAR16 **Path,
    IN OUT CHAR16 **Filename
) {
    CHAR16 *Temp;


    MY_FREE_POOL(*Path);
    MY_FREE_POOL(*VolName);
    MY_FREE_POOL(*Filename);

    Temp = StrDuplicate (InPath);
    SplitVolumeAndFilename (&Temp, VolName); // VolName is NULL or has volume; Temp has rest of path
    CleanUpPathNameSlashes (Temp);

    *Path     = FindPath (Temp); // *Path has path (may be 0-length); Temp unchanged.
    *Filename = StrDuplicate (Temp + StrLen (*Path));

    CleanUpPathNameSlashes (*Filename);

    if (StrLen (*Path) == 0) {
        MY_FREE_POOL(*Path);
    }

    if (StrLen (*Filename) == 0) {
        MY_FREE_POOL(*Filename);
    }

    MY_FREE_POOL(Temp);
} // VOID SplitPathName()

// Finds a volume with the specified Identifier (a filesystem label, a
// partition name, or a partition GUID). If found, sets *Volume to point
// to that volume. If not, leaves it unchanged.
// Returns TRUE if a match was found, FALSE if not.
BOOLEAN FindVolume (
    IN REFIT_VOLUME **Volume,
    IN CHAR16        *Identifier
) {
    UINTN     i;
    BOOLEAN   Found;


    if (Identifier == NULL) {
        return FALSE;
    }

    i = 0;
    Found = FALSE;
    while (!Found && VolumesCount > i) {
        Found = VolumeMatchesDescription (Volumes[i], Identifier);
        if (Found) {
            *Volume = Volumes[i];
        }
        i++;
    }

    return Found;
} // BOOLEAN FindVolume()

// Returns TRUE if Description matches Volume's VolName, FsName, or (once
// transformed) PartGuid fields, FALSE otherwise (or if either pointer is NULL)
BOOLEAN VolumeMatchesDescription (
    IN REFIT_VOLUME *Volume,
    IN CHAR16       *Description
) {
    CHAR16   *FilteredDescription;
    EFI_GUID  TargetVolGuid = NULL_GUID_VALUE;


    if (Volume == NULL || Description == NULL) {
        return FALSE;
    }

    FilteredDescription = GetSubStrAfter (DEFAULT_STRING_DELIM, Description);

    if (!IsGuid (FilteredDescription)) {
        return (
            MyStriCmp (FilteredDescription, Volume->VolName)  ||
            MyStriCmp (FilteredDescription, Volume->FsName)   ||
            MyStriCmp (FilteredDescription, Volume->PartName)
        );
    }

    TargetVolGuid = StringAsGuid (FilteredDescription);
    FilteredDescription = NULL;

    return GuidsAreEqual (&TargetVolGuid, &(Volume->PartGuid));
} // BOOLEAN VolumeMatchesDescription()

// Returns TRUE if specified Volume, Directory, and Filename correspond to an
// element in the comma-delimited List, FALSE otherwise. Note that Directory and
// Filename must *NOT* include a volume or path specification (that is part of
// the Volume variable), but the List elements may. Performs comparison
// case-insensitively.
BOOLEAN FilenameIn (
    IN REFIT_VOLUME *Volume,
    IN CHAR16       *Directory,
    IN CHAR16       *Filename,
    IN CHAR16       *List
) {
    UINTN      i;
    CHAR16    *AnElement; // *DO NOT* Free
    CHAR16    *OneElement;
    CHAR16    *TargetPath;
    CHAR16    *TargetVolName;
    CHAR16    *TargetFilename;
    BOOLEAN    Found;


    if (Filename == NULL || List == NULL) {
        return FALSE;
    }

    TargetPath = TargetVolName = TargetFilename = NULL;

    i = 0;
    Found = FALSE;
    while (
        !Found &&
        (OneElement = FindCommaDelimited (List, i++)) != NULL
    ) {
        AnElement = GetSubStrAfter (DEFAULT_STRING_DELIM, OneElement);
        SplitPathName (AnElement, &TargetVolName, &TargetPath, &TargetFilename);

        if (TargetPath     == NULL &&
            TargetVolName  == NULL &&
            TargetFilename == NULL
        ) {
            return FALSE;
        }

        if (MyStriCmp (TargetPath, Directory)    &&
            MyStriCmp (TargetFilename, Filename) &&
            VolumeMatchesDescription (Volume, TargetVolName)
        ) {
            Found = TRUE;
        }
        else {
            Found = FALSE;
        }

        MY_FREE_POOL(OneElement);
        MY_FREE_POOL(TargetPath);
        MY_FREE_POOL(TargetVolName);
        MY_FREE_POOL(TargetFilename);
    } // while

    return Found;
} // BOOLEAN FilenameIn()

// Eject all removable media.
// Returns TRUE if any media were ejected, FALSE otherwise.
BOOLEAN EjectMedia (VOID) {
    EFI_STATUS                      Status;
    UINTN                           HandleIndex;
    EFI_GUID                        AppleRemovableMediaGuid = APPLE_REMOVABLE_MEDIA_PROTOCOL_GUID;
    UINTN                           HandleCount;
    UINTN                           Ejected;
    EFI_HANDLE                     *Handles;
    EFI_HANDLE                      Handle;
    APPLE_REMOVABLE_MEDIA_PROTOCOL *Ejectable;


    HandleCount = 0;
    Status = LibLocateHandle (
        ByProtocol,
        &AppleRemovableMediaGuid, NULL,
        &HandleCount, &Handles
    );
    if (EFI_ERROR(Status) || HandleCount == 0) {
        // probably not an Apple system
        return FALSE;
    }

    Ejected = 0;
    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
        Handle = Handles[HandleIndex];
        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, Handle,
            &AppleRemovableMediaGuid, (VOID **) &Ejectable
        );
        if (EFI_ERROR(Status)) {
            continue;
        }

        Status = REFIT_CALL_1_WRAPPER(Ejectable->Eject, Ejectable);
        if (!EFI_ERROR(Status)) {
            Ejected++;
        }
    }
    MY_FREE_POOL(Handles);

    return (Ejected > 0);
} // VOID EjectMedia()

// Returns TRUE if the two GUIDs are equal, FALSE otherwise
BOOLEAN GuidsAreEqual (
    IN EFI_GUID *Guid1,
    IN EFI_GUID *Guid2
) {
    return (CompareMem (Guid1, Guid2, 16) == 0);
} // BOOLEAN GuidsAreEqual()

// Erase linked-list of UINT32 values
VOID EraseUint32List (
    IN UINT32_LIST **TheList
) {
    UINT32_LIST *NextItem;


    while (*TheList != NULL) {
        NextItem = (*TheList)->Next;
        MY_FREE_POOL(*TheList);
        *TheList = NextItem;
    }
} // EraseUin32List()
