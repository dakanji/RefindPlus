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
 * Modifications copyright (c) 2012-2021 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2023 Dayo Akanji (sf.net/u/dakanji/profile)
 * Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "lib.h"
#include "icns.h"
#include "screenmgt.h"
#include "../include/refit_call_wrapper.h"
#include "../include/RemovableMedia.h"
#include "gpt.h"
#include "config.h"
#include "apple.h"
#include "scan.h"
#include "mystrings.h"

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
#define HFSPLUS_MAGIC1                   0x2B48
#define HFSPLUS_MAGIC2                   0x5848
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
EFI_HANDLE                  SelfImageHandle;

CHAR16                     *StrSelfUUID         = NULL;
CHAR16                     *SelfDirPath         = NULL;

EFI_LOADED_IMAGE_PROTOCOL  *SelfLoadedImage;

EFI_FILE_PROTOCOL          *SelfRootDir;
EFI_FILE_PROTOCOL          *SelfDir;
EFI_FILE_PROTOCOL          *gVarsDir             = NULL;

REFIT_VOLUME               *SelfVolume           = NULL;
REFIT_VOLUME              **Volumes              = NULL;
REFIT_VOLUME              **RecoveryVolumes      = NULL;
REFIT_VOLUME              **SkipApfsVolumes      = NULL;
REFIT_VOLUME              **PreBootVolumes       = NULL;
REFIT_VOLUME              **SystemVolumes        = NULL;
REFIT_VOLUME              **DataVolumes          = NULL;
REFIT_VOLUME              **HfsRecovery          = NULL;

UINTN                       RecoveryVolumesCount    = 0;
UINTN                       SkipApfsVolumesCount    = 0;
UINTN                       PreBootVolumesCount     = 0;
UINTN                       SystemVolumesCount      = 0;
UINTN                       DataVolumesCount        = 0;
UINTN                       HfsRecoveryCount        = 0;
UINTN                       VolumesCount            = 0;

UINT64                      ReadWriteCreate     = EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE;

BOOLEAN                     FoundExternalDisk   = FALSE;
BOOLEAN                     DoneHeadings        = FALSE;
BOOLEAN                     ScannedOnce         = FALSE;
BOOLEAN                     SkipSpacing         = FALSE;
BOOLEAN                     SelfVolSet          = FALSE;
BOOLEAN                     SelfVolRun          = FALSE;
BOOLEAN                     MediaCheck          = FALSE;
BOOLEAN                     SingleAPFS          =  TRUE;
BOOLEAN                     ValidAPFS           =  TRUE;
BOOLEAN                     ScanMBR             = FALSE;

#if REFIT_DEBUG > 0
BOOLEAN                     FirstVolume         =  TRUE;
BOOLEAN                     FoundMBR            = FALSE;
#endif

EFI_GUID                    GuidESP             =              ESP_GUID_VALUE;
EFI_GUID                    GuidHFS             =              HFS_GUID_VALUE;
EFI_GUID                    GuidAPFS            =             APFS_GUID_VALUE;
EFI_GUID                    GuidNull            =             NULL_GUID_VALUE;
EFI_GUID                    GuidLinux           =            LINUX_GUID_VALUE;
EFI_GUID                    GuidHomeGPT         =         GPT_HOME_GUID_VALUE;
EFI_GUID                    GuidServerGPT       =       GPT_SERVER_GUID_VALUE;
EFI_GUID                    GuidBasicData       =       BASIC_DATA_GUID_VALUE;
EFI_GUID                    GuidApplTvRec       =      APPLE_TV_RECOVERY_GUID;
EFI_GUID                    GuidMacRaidOn       =      MAC_RAID_ON_GUID_VALUE;
EFI_GUID                    GuidMacRaidOff      =     MAC_RAID_OFF_GUID_VALUE;
EFI_GUID                    GuidReservedMS      =    MSFT_RESERVED_GUID_VALUE;
EFI_GUID                    GuidWindowsRE       = WIN_RECOVERY_ENV_GUID_VALUE;
EFI_GUID                    GuidRecoveryHD      =  MAC_RECOVERY_HD_GUID_VALUE;
EFI_GUID                    GuidContainerHFS    =    CONTAINER_HFS_GUID_VALUE;


extern EFI_GUID             RefindPlusOldGuid;
extern EFI_GUID             RefindPlusGuid;
extern BOOLEAN              ScanningLoaders;

#if REFIT_DEBUG > 0
extern BOOLEAN              LogNewLine;
#endif


// Maximum size for disk sectors
#define SECTOR_SIZE 4096

// Number of bytes to read from a partition to determine its filesystem type
// and identify its boot loader, and hence probable BIOS-mode OS installation
// 68 KiB -- ReiserFS superblock begins at 64 KiB
#define SAMPLE_SIZE 69632

#define PARTITION_TABLE_TXT    L"Found MBR Partition Table"
#define LEGACY_CODE_TXT        L"Found Legacy Boot Code"
#define ENTRY_BELOW_TXT        L" on Entry Below:"
#define AND_JOIN_TXT           L" and "
#define UNKNOWN_OS             L"Unknown OS"

#define NAME_FIX(Name) FindSubStr ((*Volume)->VolName, Name)) VolumeName = Name


BOOLEAN egIsGraphicsModeEnabled (VOID);


// DA-TAG: Stash here for later use
//         Allows getting user input
// NOTE: Currently Disabled by 'if 0'
#if 0
static
UINTN GetUserInput (
    IN  CHAR16   *prompt,
    OUT BOOLEAN  *bool_out
) {
    EFI_STATUS          Status;
    UINTN               Index;
    EFI_INPUT_KEY       Key;

    ReadAllKeyStrokes(); // Remove buffered key strokes

    Print(prompt);

    do {
        REFIT_CALL_3_WRAPPER(
            gBS->WaitForEvent, 1,
            &gST->ConIn->WaitForKey, &Index
        );

        Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &Key);
        if (EFI_ERROR(Status) && Status != EFI_NOT_READY) {
            return 1;
        }
    } while (EFI_ERROR(Status));

    if (Key.UnicodeChar == 'y' || Key.UnicodeChar == 'Y') {
        Print(L"Yes\n");
        *bool_out = TRUE;
    }
    else {
        Print(L"No\n");
        *bool_out = FALSE;
    }

    ReadAllKeyStrokes();
    return 0;
} // UINTN GetUserInput()
#endif

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
// out if this is not present.
VOID CleanUpPathNameSlashes (
    IN OUT CHAR16 *PathName
) {
    UINTN Dest;
    UINTN Source;

    if (PathName == NULL ||
        PathName[0] == '\0'
    ) {
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
            // Regular character; copy it straight.
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
} // CleanUpPathNameSlashes()

// Splits an EFI device path into device and filename components.
// For instance, if InString is PciRoot(0x0)/Pci(0x1f,0x2)/Ata(ABC,XYZ,0x0)/HD(2,GPT,GUID_STR)/\bzImage-3.5.1.efi,
// this function will truncate that input to PciRoot(0x0)/Pci(0x1f,0x2)/Ata(ABC,XYZ,0x0)/HD(2,GPT,GUID_STR)
// and return bzImage-3.5.1.efi as its return value.
// It does this by searching for the last ")" character in InString, copying everything
// after that string (after some cleanup) as the return value, and truncating the original
// input value.
// If InString contains no ")" character, this function leaves the original input string
// unmodified and also returns that string. If InString is NULL, this function returns NULL.
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
} // static CHAR16* SplitDeviceString()

//
// Library initialization and de-initialization
//

static
EFI_STATUS FinishInitRefitLib (VOID) {
    EFI_STATUS  Status;

    if (SelfVolume && SelfVolume->DeviceHandle != SelfLoadedImage->DeviceHandle) {
        SelfLoadedImage->DeviceHandle = SelfVolume->DeviceHandle;
    }

    if (SelfRootDir == NULL) {
        SelfRootDir = LibOpenRoot (SelfLoadedImage->DeviceHandle);
        if (SelfRootDir == NULL) {
            CheckError (EFI_LOAD_ERROR, L"While (Re)Opening Installation Volume");

            return EFI_LOAD_ERROR;
        }
    }

    Status = REFIT_CALL_5_WRAPPER(
        SelfRootDir->Open, SelfRootDir,
        &SelfDir, SelfDirPath,
        EFI_FILE_MODE_READ, 0
    );
    if (CheckFatalError (Status, L"While Opening Installation Directory")) {
        return EFI_LOAD_ERROR;
    }

    return EFI_SUCCESS;
} // static EFI_STATUS FinishInitRefitLib()

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
    if (CheckFatalError (Status, L"While Getting a LoadedImageProtocol Handle")) {
        return EFI_LOAD_ERROR;
    }

    // Find the current directory
    DevicePathAsString = DevicePathToStr (SelfLoadedImage->FilePath);
    GlobalConfig.SelfDevicePath = FileDevicePath (
        SelfLoadedImage->DeviceHandle,
        DevicePathAsString
    );

    CleanUpPathNameSlashes (DevicePathAsString);
    MY_FREE_POOL(SelfDirPath);
    Temp        = FindPath (DevicePathAsString);
    SelfDirPath = SplitDeviceString (Temp);

    MY_FREE_POOL(DevicePathAsString);
    MY_FREE_POOL(Temp);

    Status = FinishInitRefitLib();

    return Status;
} // InitRefitLib

static
VOID UninitVolume (
    IN OUT REFIT_VOLUME  **Volume
) {
    if (Volume && *Volume) {
        if ((*Volume)->RootDir != NULL) {
            REFIT_CALL_1_WRAPPER((*Volume)->RootDir->Close, (*Volume)->RootDir);
            (*Volume)->RootDir = NULL;
        }

        (*Volume)->BlockIO          = NULL;
        (*Volume)->DeviceHandle     = NULL;
        (*Volume)->WholeDiskBlockIO = NULL;
    }
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
} // static VOID UninitVolumes()

static
VOID ReinitVolume (
    IN OUT REFIT_VOLUME  **Volume
) {
    EFI_STATUS                 StatusA, StatusB;
    EFI_DEVICE_PATH_PROTOCOL  *RemainingDevicePath;
    EFI_HANDLE                 DeviceHandle;
    EFI_HANDLE                 WholeDiskHandle;

    if (Volume && *Volume) {
        if ((*Volume)->DevicePath != NULL) {
            // Get the handle for that path
            RemainingDevicePath = (*Volume)->DevicePath;
            StatusA = REFIT_CALL_3_WRAPPER(
                gBS->LocateDevicePath, &BlockIoProtocol,
                &RemainingDevicePath, &DeviceHandle
            );
            if (EFI_ERROR(StatusA)) {
                CheckError (StatusA, L"from LocateDevicePath");
            }
            else {
                (*Volume)->DeviceHandle = DeviceHandle;

                // Get the root directory
                (*Volume)->RootDir = LibOpenRoot ((*Volume)->DeviceHandle);
            }
        }

        if ((*Volume)->WholeDiskDevicePath != NULL) {
            // Get the handle for that path
            RemainingDevicePath = (*Volume)->WholeDiskDevicePath;
            StatusB = REFIT_CALL_3_WRAPPER(
                gBS->LocateDevicePath, &BlockIoProtocol,
                &RemainingDevicePath, &WholeDiskHandle
            );
            if (EFI_ERROR(StatusB)) {
                CheckError (StatusB, L"from LocateDevicePath");
            }
            else {
                // Get the BlockIO protocol
                StatusB = REFIT_CALL_3_WRAPPER(
                    gBS->HandleProtocol, WholeDiskHandle,
                    &BlockIoProtocol, (VOID **) &(*Volume)->WholeDiskBlockIO
                );
                if (EFI_ERROR(StatusB)) {
                    (*Volume)->WholeDiskBlockIO = NULL;
                    CheckError (StatusB, L"from HandleProtocol");
                }
            }
        }
    } // if Volume
} // static VOID ReinitVolume()

VOID ReinitVolumes (VOID) {
    REINIT_VOLUMES(RecoveryVolumes, RecoveryVolumesCount);
    REINIT_VOLUMES(SkipApfsVolumes, SkipApfsVolumesCount);
    REINIT_VOLUMES(PreBootVolumes,  PreBootVolumesCount);
    REINIT_VOLUMES(SystemVolumes,   SystemVolumesCount);
    REINIT_VOLUMES(HfsRecovery,     HfsRecoveryCount);
    REINIT_VOLUMES(DataVolumes,     DataVolumesCount);
    REINIT_VOLUMES(Volumes,         VolumesCount);
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
}

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
        L"Locate/Create Emulated NVRAM for RefindPlus-Specific Items ... In Installation Folder:- '%r'",
        Status
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
            L"Locate/Create Emulated NVRAM for RefindPlus-Specific Items ... In First Available ESP:- '%r'",
            Status
        );

        if (EFI_ERROR(Status)) {
            ALT_LOG(1, LOG_THREE_STAR_MID,
                L"Activate the 'use_nvram' Config Token to Use Hardware NVRAM Instead"
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

    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;
    BOOLEAN  HybridLogger = FALSE;
    MY_HYBRIDLOGGER_SET;
    #endif

    BufferSize = 0;
    TmpBuffer = NULL;
    if (!GlobalConfig.UseNvram &&
        (
            GuidsAreEqual (VendorGUID, &RefindPlusGuid) ||
            GuidsAreEqual (VendorGUID, &RefindPlusOldGuid)
        )
    ) {
        Status = FindVarsDir();
        if (!EFI_ERROR(Status)) {
            Status = egLoadFile (
                gVarsDir,
                VariableName,
                (UINT8 **) &TmpBuffer,
                &BufferSize
            );
        }
        else if (Status != EFI_NOT_FOUND) {
            #if REFIT_DEBUG > 0
            if (ScanningLoaders && LogNewLine) {
                LogNewLine = FALSE;
                LOG_MSG("\n");
            }

            MsgStr = PoolPrint (
                L"In Emulated NVRAM ... Could Not Read UEFI Variable:- '%s'",
                VariableName
            );
            ALT_LOG(1, LOG_THREE_STAR_MID, L"%s!!", MsgStr);
            LOG_MSG("** WARN: %s", MsgStr);
            LOG_MSG("\n");
            MY_FREE_POOL(MsgStr);

            MsgStr = StrDuplicate (
                L"Activate the 'use_nvram' Option to Silence this Warning"
            );
            ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
            LOG_MSG("         %s", MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
            #endif
        }

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"In Emulated NVRAM ... %r %s:- '%s'",
            Status, NVRAM_LOG_GET, VariableName
        );
        #endif

        if (!EFI_ERROR(Status)) {
            *VariableData = TmpBuffer;
            *VariableSize = (BufferSize) ? BufferSize : 0;
        }
        else {
            MY_FREE_POOL(TmpBuffer);
            *VariableData = NULL;
            *VariableSize = 0;
        }
    }
    else if (GlobalConfig.UseNvram ||
        !GuidsAreEqual (VendorGUID, &RefindPlusGuid)
    ) {
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
            if (!TmpBuffer) {
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
            L"In Hardware NVRAM ... %r %s:- '%s'",
            Status, NVRAM_LOG_GET, VariableName
        );
        #endif

        if (!EFI_ERROR(Status)) {
            *VariableData = TmpBuffer;
            *VariableSize = (BufferSize) ? BufferSize : 0;
        }
        else {
            MY_FREE_POOL(TmpBuffer);
            *VariableData = NULL;
            *VariableSize = 0;
        }
    }
    else {
        Status = EFI_LOAD_ERROR;

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_SEP, L"Program Coding Error in Fetching Variable From NVRAM!!");
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
    UINT32       AccessFlagsBase;
    VOID        *OldBuf;
    UINTN        OldSize;
    BOOLEAN      SettingMatch;

    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr;

    BOOLEAN  CheckMute = FALSE;
    BOOLEAN  HybridLogger = FALSE;
    MY_HYBRIDLOGGER_SET;
    #endif

    if (VariableSize > 0 &&
        VariableData != NULL &&
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
        MsgStr = L"Activate the 'use_nvram' Option to Silence this Warning";
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"In Emulated NVRAM ... %r %s:- '%s'",
            Status, NVRAM_LOG_SET, VariableName
        );

        if (EFI_ERROR(Status)) {
            ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);

            LOG_MSG("** WARN: Could Not Save to Emulated NVRAM:- '%s'", VariableName);
            LOG_MSG("\n");
            LOG_MSG("         %s", MsgStr);
            LOG_MSG("\n\n");
        }
        #endif
    }
    else {
        // Store the new value
        AccessFlagsBase = EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS;
        if (Persistent) {
            AccessFlagsBase |= EFI_VARIABLE_NON_VOLATILE;
        }
        Status = REFIT_CALL_5_WRAPPER(
            gRT->SetVariable, VariableName,
            VendorGUID, AccessFlagsBase,
            VariableSize, VariableData
        );

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"In Hardware NVRAM ... %r %s:- '%s'",
            Status, NVRAM_LOG_SET, VariableName
        );
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

        if (TmpListPtr) {
            *ListPtr = TmpListPtr;
        }
        else {
            Abort = TRUE;
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

        if (TmpListPtr) {
            *ListPtr = TmpListPtr;
        }
        else {
            Abort = TRUE;
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
    if (Volume && *Volume && ((*Volume)->VolName)) {
        if (0);
        else if (NAME_FIX(L"EFI System Partition"        );
        else if (NAME_FIX(L"Whole Disk Volume"           );
        else if (NAME_FIX(L"Unknown Volume"              );
        else if (NAME_FIX(L"NTFS Volume"                 );
        else if (NAME_FIX(L"HFS+ Volume"                 );
        else if (NAME_FIX(L"FAT Volume"                  );
        else if (NAME_FIX(L"XFS Volume"                  );
        else if (NAME_FIX(L"PreBoot"                     );
        else if (NAME_FIX(L"Ext4 Volume"                 );
        else if (NAME_FIX(L"Ext3 Volume"                 );
        else if (NAME_FIX(L"Ext2 Volume"                 );
        else if (NAME_FIX(L"BTRFS Volume"                );
        else if (NAME_FIX(L"ReiserFS Volume"             );
        else if (NAME_FIX(L"ISO-9660 Volume"             );
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

    FreeVolumes (&RecoveryVolumes, &RecoveryVolumesCount);
    FreeVolumes (&SkipApfsVolumes, &SkipApfsVolumesCount);
    FreeVolumes (&PreBootVolumes,  &PreBootVolumesCount );
    FreeVolumes (&SystemVolumes,   &SystemVolumesCount  );
    FreeVolumes (&HfsRecovery,     &HfsRecoveryCount    );
    FreeVolumes (&DataVolumes,     &DataVolumesCount    );
} // VOID FreeSyncVolumes()

VOID FreeVolumes (
    IN OUT REFIT_VOLUME  ***ListVolumes,
    IN OUT UINTN           *ListCount
) {
    UINTN i;

    if ((*ListCount > 0) && (**ListVolumes != NULL)) {
        for (i = 0; i < *ListCount; i++) {
            FreeVolume (&(*ListVolumes)[i]);
        }
        MY_FREE_POOL(*ListVolumes);
        *ListCount = 0;
    }
} // VOID FreeVolumes()

REFIT_VOLUME * CopyVolume (
    IN REFIT_VOLUME *VolumeToCopy
) {
    REFIT_VOLUME *Volume;

    if (!VolumeToCopy) {
        // Early Exit
        return NULL;
    }

    // UnInit VolumeToCopy
    UninitVolume (&VolumeToCopy);

      // Create New Volume based on VolumeToCopy (in 'UnInit' state)
    Volume = AllocateCopyPool (sizeof (REFIT_VOLUME), VolumeToCopy);
    if (Volume) {
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

        if (VolumeToCopy->MbrPartitionTable) {
            UINTN SizeMBR = 4 * 16;
            Volume->MbrPartitionTable = AllocatePool (SizeMBR);
            if (Volume->MbrPartitionTable) {
                CopyMem (Volume->MbrPartitionTable, VolumeToCopy->MbrPartitionTable, SizeMBR);
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
    if (!Volume || !(*Volume)) {
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
    CHAR16 *retval;

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
    else if (FindSubStr (Volume->VolName, L"Optical Disc Drive"        )) retval = L"ISO-9660 (Assumed)";
    else if (FindSubStr (Volume->VolName, L"APFS/FileVault Container"  )) retval = L"APFS (Assumed)"    ;
    else if (FindSubStr (Volume->VolName, L"Fusion/FileVault Container")) retval = L"HFS+ (Assumed)"    ;
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    if (!MyStriCmp (retval, L"Unknown")) {
        return retval;
    }

    if (0);
    else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidBasicData )) retval = L"NTFS (Assumed)";
    else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidReservedMS)) retval = L"NTFS (Assumed)";
    else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidMacRaidOff)) retval = L"Mac Raid (OFF)";
    else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidMacRaidOn )) retval = L"Mac Raid (ON)" ;

    return retval;
} // CHAR16 *FSTypeName()

// Sets the FsName field of Volume, based on data recorded in the partition's
// filesystem. This field may remain unchanged if there is no known filesystem
// or if the name field is empty.
static
VOID SetFilesystemName (
    IN OUT REFIT_VOLUME *Volume
) {
    EFI_FILE_SYSTEM_INFO *FileSystemInfoPtr;

    if (!Volume || !(Volume->RootDir)) {
        // Early Exit
        return;
    }

    FileSystemInfoPtr = LibFileSystemInfo (Volume->RootDir);
    if (!FileSystemInfoPtr) {
        // Early Exit
        return;
    }

    if (StrLen (FileSystemInfoPtr->VolumeLabel) > 0) {
        MY_FREE_POOL(Volume->FsName);
        Volume->FsName = StrDuplicate (FileSystemInfoPtr->VolumeLabel);
    }

    MY_FREE_POOL(FileSystemInfoPtr);
} // VOID *SetFilesystemName()

// Identify the filesystem type and record the filesystem's UUID/serial number,
// if possible. Expects a Buffer containing the first few (normally at least
// 4096) bytes of the filesystem. Sets the filesystem type code in Volume->FSType
// and the UUID/serial number in Volume->VolUuid. Note that the UUID value is
// recognized differently for each filesystem, and is currently supported only
// for NTFS, FAT, ext2/3/4fs, and ReiserFS (and for NTFS and FAT it is really a
// 64-bit or 32-bit serial number not a UUID or GUID). If the UUID can't be
// determined, it is set to 0. Also, the UUID is just read directly into memory;
// it is *NOT* valid when displayed by GuidAsString() or used in other
// GUID/UUID-manipulating functions. (As I write, it is being used merely to
// detect partitions that are part of a RAID 1 array.)
static
VOID SetFilesystemData (
    IN     UINT8        *Buffer,
    IN     UINTN         BufferSize,
    IN OUT REFIT_VOLUME *Volume
) {
    UINT32  *Ext2Compat;
    UINT32  *Ext2Incompat;
    UINT16  *Magic16;
    char    *MagicString;

    if ((Buffer != NULL) && (Volume != NULL)) {
        SetMem(&(Volume->VolUuid), sizeof(EFI_GUID), 0);

        // Default to Unknown FS TYpe
        Volume->FSType = FS_TYPE_UNKNOWN;

        if (BufferSize >= (1024 + 100)) {
            Magic16 = (UINT16*) (Buffer + 1024 + 56);
            if (*Magic16 == EXT2_SUPER_MAGIC) {
                // ext2/3/4
                Ext2Compat   = (UINT32*) (Buffer + 1024 + 92);
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
                    // none of these features. Assume it is ext2
                    Volume->FSType = FS_TYPE_EXT2;
                }

                CopyMem(&(Volume->VolUuid), Buffer + 1024 + 104, sizeof(EFI_GUID));
                return;
            }
        } // search for ext2/3/4 magic

        if (BufferSize >= (65536 + 100)) {
            MagicString = (char*) (Buffer + 65536 + 52);
            if ((CompareMem(MagicString, REISERFS_SUPER_MAGIC_STRING,     8) == 0) ||
                (CompareMem(MagicString, REISER2FS_SUPER_MAGIC_STRING,    9) == 0) ||
                (CompareMem(MagicString, REISER2FS_JR_SUPER_MAGIC_STRING, 9) == 0)
            ) {
                Volume->FSType = FS_TYPE_REISERFS;
                CopyMem(&(Volume->VolUuid), Buffer + 65536 + 84, sizeof(EFI_GUID));
                return;
            }
        } // search for ReiserFS magic

        if (BufferSize >= (65536 + 64 + 8)) {
            MagicString = (char*) (Buffer + 65536 + 64);
            if (CompareMem(MagicString, BTRFS_SIGNATURE, 8) == 0) {
                Volume->FSType = FS_TYPE_BTRFS;
                return;
            }
        } // search for Btrfs magic

        if (BufferSize >= 512) {
            MagicString = (char*) Buffer;
            if (CompareMem(MagicString, XFS_SIGNATURE, 4) == 0) {
                Volume->FSType = FS_TYPE_XFS;

                return;
            }
        } // search for XFS magic

        if (BufferSize >= (32768 + 4)) {
            MagicString = (char*) (Buffer + 32768);
            if (CompareMem (MagicString, JFS_SIGNATURE, 4) == 0) {
                Volume->FSType = FS_TYPE_JFS;
                return;
            }
        } // search for JFS magic

        if (BufferSize >= (1024 + 2)) {
            Magic16 = (UINT16*) (Buffer + 1024);
            if ((*Magic16 == HFSPLUS_MAGIC1) ||
                (*Magic16 == HFSPLUS_MAGIC2)
            ) {
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
                if (CompareMem(MagicString + 3, NTFS_SIGNATURE, 8) == 0) {
                    Volume->FSType = FS_TYPE_NTFS;
                    CopyMem(&(Volume->VolUuid), Buffer + 0x48, sizeof(UINT64));
                }
                else if (CompareMem(MagicString + 0x52, FAT32_SIGNATURE, 8) == 0) {
                    Volume->FSType = FS_TYPE_FAT32;
                    CopyMem(&(Volume->VolUuid), Buffer + 0x43, sizeof(UINT32));
                }
                else if (CompareMem(MagicString + 0x36, FAT16_SIGNATURE, 8) == 0) {
                    Volume->FSType = FS_TYPE_FAT16;
                    CopyMem(&(Volume->VolUuid), Buffer + 0x27, sizeof(UINT32));
                }
                else if (CompareMem(MagicString + 0x36, FAT12_SIGNATURE, 8) == 0) {
                    Volume->FSType = FS_TYPE_FAT12;
                    CopyMem(&(Volume->VolUuid), Buffer + 0x27, sizeof(UINT32));
                }
                else if (!Volume->BlockIO->Media->LogicalPartition) {
                    Volume->FSType = FS_TYPE_WHOLEDISK;
                }

                return;
            }
        } // search for FAT and NTFS magic

        // If no other filesystem is identified and block size is right, assume ISO-9660
        if (Volume->BlockIO->Media->BlockSize == 2048) {
            Volume->FSType = FS_TYPE_ISO9660;
            return;
        }

        // If no other filesystem is identified, assume APFS if on partition with APFS GUID
        if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidAPFS)) {
            Volume->FSType = FS_TYPE_APFS;
            return;
        }
    } // if ((Buffer != NULL) && (Volume != NULL))
} // UINT32 SetFilesystemData()

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
    if (EFI_ERROR(Status)) {
        // DA-TAG: Do not log as unable to filter
        return;
    }

    SetFilesystemData (Buffer, SAMPLE_SIZE, Volume);
    if (GlobalConfig.LegacyType != LEGACY_TYPE_MAC) {
        #if REFIT_DEBUG > 0
        if (SelfVolRun && ScanMBR) {
            if (Status == EFI_NO_MEDIA) {
                MediaCheck = TRUE;
            }
            ScannedOnce = FALSE;
            MsgStr = L"Could Not Read Boot Sector on Item Below";
            LOG_MSG("\n\n");
            LOG_MSG("** WARN: '%r' %s", Status, MsgStr);
            CheckError (Status, MsgStr);
        }
        #endif

        return;
    }

    if ((Buffer[0] != 0) &&
        (*((UINT16 *)(Buffer + 510)) == 0xaa55) &&
        (FindMem (Buffer, 512, "EXFAT", 5) == -1)
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
    else if (FindMem (Buffer, 512, "Geom\0Hard Disk\0Read\0 Error", 26) >= 0) {
        // GRUB
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"grub,linux";
        Volume->OSName       = L"Instance: Linux (Legacy)";
    }
    else if (
        (
            *((UINT32 *)(Buffer + 502)) == 0 &&
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
        (*((UINT16 *)(Buffer + 510)) == 0xaa55) &&
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
        FindMem (Buffer, 512,         "!Loading",            8) >= 0 ||
        FindMem (Buffer, SECTOR_SIZE, "/cdboot\0/CDBOOT\0", 16) >= 0
    ) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"openbsd";
        Volume->OSName       = L"Instance: OpenBSD (Legacy)";
    }
    else if (
        FindMem (Buffer, 512, "Not a bootxx image", 18) >= 0 ||
        *((UINT32 *)(Buffer + 1028)) == 0x7886b6d1
    ) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"netbsd";
        Volume->OSName       = L"Instance: NetBSD (Legacy)";
    }
    else if (FindMem (Buffer, SECTOR_SIZE, "NTLDR", 5) >= 0) {
        // Windows NT/200x/XP
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"win";
        Volume->OSName       = L"Instance: Windows (NT/XP)";
    }
    else if (FindMem (Buffer, SECTOR_SIZE, "BOOTMGR", 7) >= 0) {
        // Windows Vista/7/8/10
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"win8,win";
        Volume->OSName       = L"Instance: Windows (Legacy)";
    }
    else if (
        FindMem (Buffer, 512, "CPUBOOT SYS", 11) >= 0 ||
        FindMem (Buffer, 512, "KERNEL  SYS", 11) >= 0
    ) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"freedos";
        Volume->OSName       = L"Instance: FreeDOS (Legacy)";
    }
    else if (
        FindMem (Buffer, 512, "OS2LDR",  6) >= 0 ||
        FindMem (Buffer, 512, "OS2BOOT", 7) >= 0
    ) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"ecomstation";
        Volume->OSName       = L"Instance: eComStation (Legacy)";
    }
    else if (FindMem (Buffer, 512, "Be Boot Loader", 14) >= 0) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"beos";
        Volume->OSName       = L"Instance: BeOS (Legacy)";
    }
    else if (FindMem (Buffer, 512, "yT Boot Loader", 14) >= 0) {
        Volume->HasBootCode  = TRUE;
        Volume->OSIconName   = L"zeta,beos";
        Volume->OSName       = L"Instance: ZETA (Legacy)";
    }
    else if (
        FindMem (Buffer, 512, "\x04" "beos\x06" "system\x05" "zbeos", 18) >= 0 ||
        FindMem (Buffer, 512, "\x06" "system\x0c" "haiku_loader",     20) >= 0
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
        if (GlobalConfig.LegacyType == LEGACY_TYPE_MAC && Volume->FSType == FS_TYPE_NTFS) {
            Volume->HasBootCode = HasWindowsBiosBootFiles (Volume);
        }
        else if (FindMem (Buffer, 512, "Non-system disk", 15) >= 0) {
            // Dummy FAT boot sector (created by OS X's newfs_msdos)
            Volume->HasBootCode = FALSE;
        }
        else if (FindMem (Buffer, 512, "This is not a bootable disk", 27) >= 0) {
            // Dummy FAT boot sector (created by Linux's mkdosfs)
            Volume->HasBootCode = FALSE;
        }
        else if (FindMem (Buffer, 512, "Press any key to restart", 24) >= 0) {
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
    MbrTable = (MBR_PARTITION_INFO *)(Buffer + 446);
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
        if (!Volume->MbrPartitionTable) {
            *Bootable = Volume->HasBootCode = FALSE;
            return;
        }
        CopyMem (Volume->MbrPartitionTable, MbrTable, SizeMBR);

        #if REFIT_DEBUG > 0
        FoundMBR = TRUE;
        #endif
    }

    #if REFIT_DEBUG > 0
    if (DoneHeadings) {
        if (Volume->HasBootCode) {
            LogLineType = (SkipSpacing) ? LOG_LINE_SAME : LOG_LINE_SPECIAL;
            StrSpacer   = (SkipSpacing) ? AND_JOIN_TXT  : L"";
            ALT_LOG(1, LogLineType, L"%s%s", StrSpacer, LEGACY_CODE_TXT);
            SkipSpacing = (GlobalConfig.LogLevel > 0) ? TRUE : FALSE;
        }
        if (MbrTableFound) {
            LogLineType = (SkipSpacing) ? LOG_LINE_SAME : LOG_LINE_SPECIAL;
            StrSpacer   = (SkipSpacing) ? AND_JOIN_TXT  : L"";
            ALT_LOG(1, LogLineType, L"%s%s", StrSpacer, PARTITION_TABLE_TXT);
            SkipSpacing = (GlobalConfig.LogLevel > 0) ? TRUE : FALSE;
        }
    }
    #endif
} // VOID ScanVolumeBootcode()

// Set default volume badge icon based on /.VolumeBadge.{icns|png} file or disk kind
VOID SetVolumeBadgeIcon (
    IN OUT REFIT_VOLUME *Volume
) {
    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_BADGES) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Skipped ... VolumeBadge Config Setting:- 'Hidden'");
        #endif

        return;
    }

    if (Volume == NULL) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"NULL Volume!!");
        #endif

        return;
    }

    if (GlobalConfig.HelpIcon && Volume->VolBadgeImage == NULL) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Try Default Icon ... Config Setting is *NOT* Active:- 'decline_help_icon'"
        );
        #endif

        switch (Volume->DiskKind) {
            case DISK_KIND_INTERNAL: SET_BADGE_IMAGE(BUILTIN_ICON_VOL_INTERNAL); break;
            case DISK_KIND_EXTERNAL: SET_BADGE_IMAGE(BUILTIN_ICON_VOL_EXTERNAL); break;
            case DISK_KIND_OPTICAL:  SET_BADGE_IMAGE(BUILTIN_ICON_VOL_OPTICAL ); break;
            case DISK_KIND_NET:      SET_BADGE_IMAGE(BUILTIN_ICON_VOL_NET     ); break;
        } // switch
    }

    if (Volume->VolBadgeImage == NULL) {
        Volume->VolBadgeImage = egLoadIconAnyType (
            Volume->RootDir, L"", L".VolumeBadge",
            GlobalConfig.IconSizes[ICON_SIZE_BADGE]
        );
    }

    if (!GlobalConfig.HelpIcon && Volume->VolBadgeImage == NULL) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Try Default Icon");
        #endif

        switch (Volume->DiskKind) {
            case DISK_KIND_INTERNAL: SET_BADGE_IMAGE(BUILTIN_ICON_VOL_INTERNAL); break;
            case DISK_KIND_EXTERNAL: SET_BADGE_IMAGE(BUILTIN_ICON_VOL_EXTERNAL); break;
            case DISK_KIND_OPTICAL:  SET_BADGE_IMAGE(BUILTIN_ICON_VOL_OPTICAL ); break;
            case DISK_KIND_NET:      SET_BADGE_IMAGE(BUILTIN_ICON_VOL_NET     ); break;
        } // switch
    }
} // VOID SetVolumeBadgeIcon()

// Return a string representing the input size in IEEE-1541 units.
// The calling function is responsible for freeing the allocated memory.
static
CHAR16 * SizeInIEEEUnits (
    IN UINT64 SizeInBytes
) {
    UINTN   Index;
    UINTN   NumPrefixes;
    CHAR16 *Units;
    CHAR16 *Prefixes;
    CHAR16 *TheValue;
    UINT64  SizeInIeee;

    Prefixes = L" KMGTPEZ";
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

    if ((Volume == NULL) || (DevicePath == NULL)) {
        return;
    }

    if ((DevicePath->Type != MEDIA_DEVICE_PATH) ||
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
    if (!PartInfo) {
        return;
    }

    Volume->PartName = StrDuplicate (PartInfo->name);
    CopyMem (&(Volume->PartTypeGuid), PartInfo->type_guid, sizeof (EFI_GUID));

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


    FoundName = NULL;
    if ((Volume->FsName != NULL) &&
        (Volume->FsName[0] != L'\0') &&
        (StrLen (Volume->FsName) > 0)
    ) {
        FoundName = StrDuplicate (Volume->FsName);

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Set Name to Filesystem Name:- '%s'",
            FoundName
        );
        #endif
    }

    // Try to use the partition name if no filesystem name
    if ((FoundName == NULL) &&
        (Volume->PartName) &&
        (StrLen (Volume->PartName) > 0) &&
        !IsIn (Volume->PartName, IGNORE_PARTITION_NAMES)
    ) {
        FoundName = StrDuplicate (Volume->PartName);

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Set Name to Partition Name:- '%s'",
            FoundName
        );
        #endif
    }

    // 'FSTypeName' returns a constant ... Do not free 'TypeName'!
    TypeName = FSTypeName (Volume);

    // No filesystem or acceptable partition name ... use fs type and size
    if (FoundName == NULL) {
        FileSystemInfoPtr = (Volume->RootDir == NULL)
            ? NULL
            : LibFileSystemInfo (Volume->RootDir);

        if (FileSystemInfoPtr != NULL) {
            SISize    = SizeInIEEEUnits (FileSystemInfoPtr->VolumeSize);
            FoundName = PoolPrint (L"%s %s Volume", SISize, TypeName);

            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"Set Name to Filesystem Description:- '%s'",
                FoundName
            );
            #endif

            MY_FREE_POOL(SISize);
            MY_FREE_POOL(FileSystemInfoPtr);
        }
    }

    if (FoundName == NULL) {
        if (Volume->DiskKind == DISK_KIND_OPTICAL) {
            if (Volume->FSType == FS_TYPE_ISO9660) {
                FoundName = StrDuplicate (L"Optical ISO-9660 Image");
            }
            else {
                FoundName = StrDuplicate (L"Optical Disc Drive (Inactive)");
            }
        }
        else if (MediaCheck) {
            FoundName = StrDuplicate (L"Network Volume (Assumed)");
        }
        else {
            if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidAPFS)) {
                FoundName = StrDuplicate (L"APFS/FileVault Container");
            }
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidLinux)) {
                FoundName = StrDuplicate (L"Linux Volume");
            }
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidReservedMS)) {
                FoundName = StrDuplicate (L"Microsoft Reserved Partition");
            }
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidRecoveryHD)) {
                FoundName = StrDuplicate (L"Recovery HD");
            }
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidHomeGPT)) {
                FoundName = StrDuplicate (L"GPT Home Partition");
            }
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidServerGPT)) {
                FoundName = StrDuplicate (L"GPT Server Partition");
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
                if (MyStriCmp (TypeName, L"APFS")) {
                    FoundName = StrDuplicate (L"APFS Volume (Assumed)");
                }
                else {
                    FoundName = PoolPrint (L"%s Volume", TypeName);
                }
            }
        }

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Set Name to Generic Description:- '%s'",
            FoundName
        );
        #endif
    } // if FoundName == NULL

    // TODO: Above could be improved/extended, in case filesystem name is not found,
    //       such as:
    //         - use or add disk/partition number (e.g., "(hd0,2)")

    return FoundName;
} // CHAR16 * GetVolumeName()

// Return TRUE if NTFS boot files are found or if Volume is unreadable,
// FALSE otherwise. The idea is to weed out non-boot NTFS volumes from
// BIOS/legacy boot list on Macs. We cannot assume NTFS will be readable,
// so return TRUE if it is unreadable; but if it *IS* readable, return
// TRUE only if Windows boot files are found.
BOOLEAN HasWindowsBiosBootFiles (
    IN REFIT_VOLUME *Volume
) {
    if (!Volume->RootDir) {
        // Assume Present
        return TRUE;
    }

    // NTLDR   = Windows boot file: NT/200x/XP
    // Bootmgr = Windows boot file: Vista/7/8/10
    return (
        FileExists (Volume->RootDir, L"NTLDR") ||
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

        if (DevicePathType (DevicePath) == MEDIA_DEVICE_PATH &&
            DevicePathSubType (DevicePath) == MEDIA_CDROM_DP
        ) {
            // El Torito entry -> optical disk
            Volume->DiskKind = DISK_KIND_OPTICAL;
        }

        if (DevicePathType (DevicePath) == MESSAGING_DEVICE_PATH) {
            // Make a device path for the whole device
            PartialLength  = (UINT8 *) NextDevicePath - (UINT8 *) (Volume->DevicePath);
            DiskDevicePath = (EFI_DEVICE_PATH_PROTOCOL *) AllocatePool (
                PartialLength + sizeof (EFI_DEVICE_PATH)
            );

            CopyMem (
                DiskDevicePath,
                Volume->DevicePath,
                PartialLength
            );

            CopyMem (
                (UINT8 *) DiskDevicePath + PartialLength,
                EndDevicePath,
                sizeof (EFI_DEVICE_PATH)
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
                    ALT_LOG(1, LogLineType, L"%sCould Not Locate Device Path", StrSpacer);
                    SkipSpacing = (GlobalConfig.LogLevel > 0) ? TRUE : FALSE;
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
                        ALT_LOG(1, LogLineType, L"%sCould Not Get DiskDevicePath", StrSpacer);
                        SkipSpacing = (GlobalConfig.LogLevel > 0) ? TRUE : FALSE;
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
                        ALT_LOG(1, LogLineType, L"%sCould Not Get WholeDiskBlockIO", StrSpacer);
                        SkipSpacing = (GlobalConfig.LogLevel > 0) ? TRUE : FALSE;
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

    if (Volume->RootDir == NULL) {
        Volume->IsReadable = FALSE;
        return;
    }

    Volume->IsReadable = TRUE;
    if (GlobalConfig.LegacyType == LEGACY_TYPE_MAC
        && Volume->FSType == FS_TYPE_NTFS
        && Volume->HasBootCode
    ) {
        // VBR boot code found on NTFS, but volume is not actually bootable
        // on Mac unless there are actual boot file, so check for them.
        Volume->HasBootCode = HasWindowsBiosBootFiles (Volume);
    }
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
    UINT8               SectorBuffer[512];
    BOOLEAN             Bootable;
    REFIT_VOLUME       *Volume;
    MBR_PARTITION_INFO *EMbrTable;

    ExtBase = MbrEntry->StartLBA;
    for (ExtCurrent = ExtBase; ExtCurrent; ExtCurrent = NextExtCurrent) {
        // Read Current EMBR
        Status = REFIT_CALL_5_WRAPPER(
            WholeDiskVolume->BlockIO->ReadBlocks, WholeDiskVolume->BlockIO,
            WholeDiskVolume->BlockIO->Media->MediaId, ExtCurrent,
            512, SectorBuffer
        );

        if (EFI_ERROR(Status)) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            ALT_LOG(1, LOG_LINE_NORMAL, L"Error %d Reading Blocks From Disk", Status);
            #endif

            break;
        }

        if (*((UINT16 *) (SectorBuffer + 510)) != 0xaa55) {
            break;
        }

        EMbrTable = (MBR_PARTITION_INFO *) (SectorBuffer + 446);

        // Scan Logical Partitions in this EMBR
        NextExtCurrent = 0;
        LogicalPartitionIndex = 4;
        for (i = 0; i < 4; i++) {
            if ((EMbrTable[i].Flags != 0x00 && EMbrTable[i].Flags != 0x80) ||
                EMbrTable[i].StartLBA == 0 || EMbrTable[i].Size == 0
            ) {
                break;
            }

            if (IS_EXTENDED_PART_TYPE(EMbrTable[i].Type)) {
                // Set Next ExtCurrent
                NextExtCurrent = ExtBase + EMbrTable[i].StartLBA;
                break;
            }
            else {
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

                SetVolumeBadgeIcon (Volume);
                AddListElement (
                    (VOID ***) &Volumes,
                    &VolumesCount,
                    Volume
                );
            }
        } // for i
    } // for ExtCurrent = ExtBase
} // VOID ScanExtendedPartition()

// Check for Multi-Instance APFS Containers
static
VOID VetMultiInstanceAPFS (VOID) {
    EFI_STATUS                       Status;
    UINTN                              i, j;
    BOOLEAN                 ActiveContainer;
    APPLE_APFS_VOLUME_ROLE       VolumeRole;

    #if REFIT_DEBUG > 0
    BOOLEAN AppleRecovery;
    CHAR16 *MsgStrA;
    CHAR16 *MsgStrB = L"Disabled:- 'Recovery Tool for MacOS Versions on APFS'"  ;
    CHAR16 *MsgStrC = L"Disabled:- 'Synced APFS Volumes in DontScanVolumes'"    ;
    CHAR16 *MsgStrD = L"Disabled:- 'Hidden Tags for Synced AFPS Volumes'"       ;
    CHAR16 *MsgStrE = L"Disabled:- 'Apple Hardware Test on Synced AFPS Volumes'";

    // Check if configured to show Apple Recovery
    AppleRecovery = FALSE;
    for (i = 0; i < NUM_TOOLS; i++) {
        switch (GlobalConfig.ShowTools[i]) {
            case TAG_RECOVERY_APPLE: AppleRecovery = TRUE; break;
            default:                                    continue;
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
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
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
            if (Volumes[i]->VolName != NULL
                && StrLen (Volumes[i]->VolName) != 0
            ) {
                if (GuidsAreEqual
                    (
                        &(PreBootVolumes[j]->PartGuid),
                        &(Volumes[i]->PartGuid)
                    )
                ) {
                    VolumeRole = 0;
                    Status = RP_GetApfsVolumeInfo (
                        Volumes[i]->DeviceHandle,
                        NULL, NULL,
                        &VolumeRole
                    );
                    if (!EFI_ERROR(Status)) {
                        if (VolumeRole == APPLE_APFS_VOLUME_ROLE_SYSTEM ||
                            VolumeRole == APPLE_APFS_VOLUME_ROLE_UNDEFINED
                        ) {
                            if (!ActiveContainer) {
                                ActiveContainer = TRUE;
                            }
                            else {
                                SingleAPFS = FALSE;
                                break;
                            }
                        }
                    }
                } // if GuidsAreEqual
            } // if Volumes[i]->VolName != NULL
        } // for i = 0

        if (!SingleAPFS) {
            // DA-TAG: Multiple installations in a single APFS Container
            //         Misc features are disabled
            #if REFIT_DEBUG > 0
            MsgStrA = L"APFS Container with Multiple Mac OS Instances Found";
            LOG_MSG("INFO: %s", MsgStrA);

            ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s ... %s", MsgStrA, MsgStrE);
            LOG_MSG("%s      * %s", OffsetNext, MsgStrE);

            if (AppleRecovery) {
                // Log relevant warning if configured to show Apple Recovery
                ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s ... %s", MsgStrA, MsgStrB);
                LOG_MSG("%s      * %s", OffsetNext, MsgStrB);
            }

            // Log other warnings
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s ... %s", MsgStrA, MsgStrC);
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s ... %s", MsgStrA, MsgStrD);
            LOG_MSG("%s      * %s", OffsetNext, MsgStrC);
            LOG_MSG("%s      * %s", OffsetNext, MsgStrD);
            LOG_MSG("\n\n");
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
    UINTN   i, j;
    CHAR16 *CheckName;
    CHAR16 *TweakName;

    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;
    #endif

    if (PreBootVolumesCount == 0) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (
            L"Could Not Positively Identify APFS Partitions ... Disabling SyncAFPS"
        );
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
        LOG_MSG("\n\n");
        LOG_MSG("INFO: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        // Disable SyncAPFS if we do not have, or could not identify, any PreBoot volume
        GlobalConfig.SyncAPFS = FALSE;

        // Early Return
        return;
    }

    if (!ValidAPFS) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (
            L"Could Not Determine the VolUUID or PartGuid on One or More Key APFS Volumes ... Disabling SyncAFPS"
        );
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
        LOG_MSG("\n\n");
        LOG_MSG("INFO: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        // Disable SyncAPFS if we do not have, or could not identify, any PreBoot volume
        GlobalConfig.SyncAPFS = FALSE;

        // Early Return
        return;
    }

    #if REFIT_DEBUG > 0
    MsgStr = StrDuplicate (L"ReMap APFS Volume Groups");
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
    LOG_MSG("\n\n");
    LOG_MSG("%s:", MsgStr);
    MY_FREE_POOL(MsgStr);

    for (i = 0; i < SystemVolumesCount; i++) {
        MsgStr = PoolPrint (L"System Volume:- '%s'", SystemVolumes[i]->VolName);
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("%s  - %s", OffsetNext, MsgStr);
        MY_FREE_POOL(MsgStr);
    } // for
    #endif

    // Filter '- Data' string tag out of Volume Group name if present
    TweakName = CheckName = NULL;
    for (i = 0; i < DataVolumesCount; i++) {
        if (MyStrStr (DataVolumes[i]->VolName, L"- Data")) {
            for (j = 0; j < SystemVolumesCount; j++) {
                MY_FREE_POOL(TweakName);
                TweakName = SanitiseString (SystemVolumes[j]->VolName);

                MY_FREE_POOL(CheckName);
                CheckName = PoolPrint (L"%s - Data", TweakName);

                if (MyStriCmp (DataVolumes[i]->VolName, CheckName)) {
                    MY_FREE_POOL(DataVolumes[i]->VolName);
                    DataVolumes[i]->VolName = StrDuplicate (SystemVolumes[j]->VolName);

                    break;
                }

                // Check against raw name string if apporpriate
                if (!MyStriCmp (SystemVolumes[j]->VolName, TweakName)) {
                    MY_FREE_POOL(CheckName);
                    CheckName = PoolPrint (L"%s - Data", SystemVolumes[j]->VolName);

                    if (MyStriCmp (DataVolumes[i]->VolName, CheckName)) {
                        MY_FREE_POOL(DataVolumes[i]->VolName);
                        DataVolumes[i]->VolName = StrDuplicate (SystemVolumes[j]->VolName);

                        break;
                    }
                }
            } // for j = 0

            MY_FREE_POOL(TweakName);
            MY_FREE_POOL(CheckName);
        }
    } // for i = 0

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"ReMapped %d Volume Group%s",
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
    LOG_MSG("\n\n");
    MY_FREE_POOL(MsgStr);
    #endif

    // Check for Multi-Instance APFS Containers
    VetMultiInstanceAPFS();
} // VOID VetSyncAPFS()

#if REFIT_DEBUG > 0
static
CHAR16 * GetApfsRoleString (
    IN APPLE_APFS_VOLUME_ROLE VolumeRole
) {
    CHAR16 *retval;

    switch (VolumeRole) {
        case APPLE_APFS_VOLUME_ROLE_UNDEFINED:       retval = L"0x00 - Undefined"  ;       break;
        case APPLE_APFS_VOLUME_ROLE_SYSTEM:          retval = L"0x01 - System"     ;       break;
        case APPLE_APFS_VOLUME_ROLE_USER:            retval = L"0x02 - UserHome"   ;       break;
        case APPLE_APFS_VOLUME_ROLE_RECOVERY:        retval = L"0x04 - Recovery"   ;       break;
        case APPLE_APFS_VOLUME_ROLE_VM:              retval = L"0x08 - VM"         ;       break;
        case APPLE_APFS_VOLUME_ROLE_PREBOOT:         retval = L"0x10 - PreBoot"    ;       break;
        case APPLE_APFS_VOLUME_ROLE_INSTALLER:       retval = L"0x20 - Installer"  ;       break;
        case APPLE_APFS_VOLUME_ROLE_DATA:            retval = L"0x40 - Data"       ;       break;
        case APPLE_APFS_VOLUME_ROLE_UPDATE:          retval = L"0xC0 - Snapshot"   ;       break;
        case APFS_VOL_ROLE_XART:                     retval = L"0x0? - SecData"    ;       break;
        case APFS_VOL_ROLE_HARDWARE:                 retval = L"0x1? - Firmware"   ;       break;
        case APFS_VOL_ROLE_BACKUP:                   retval = L"0x2? - BackupTM"   ;       break;
        case APFS_VOL_ROLE_RESERVED_7:               retval = L"0x3? - Resrved07"  ;       break;
        case APFS_VOL_ROLE_RESERVED_8:               retval = L"0x4? - Resrved08"  ;       break;
        case APFS_VOL_ROLE_ENTERPRISE:               retval = L"0x5? - Enterprse"  ;       break;
        case APFS_VOL_ROLE_RESERVED_10:              retval = L"0x6? - Resrved10"  ;       break;
        case APFS_VOL_ROLE_PRELOGIN:                 retval = L"0x7? - PreLogin"   ;       break;
        default:                                     retval = L"0xFF - Unknown"    ;       break;
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
    UINTN                   HandleIndex;
    UINTN                   VolumeIndex;
    UINTN                   VolumeIndex2;
    UINTN                   PartitionIndex;
    UINTN                   HandleCount;
    UINT8                  *SectorBuffer1;
    UINT8                  *SectorBuffer2;
    CHAR16                 *RoleStr;
    CHAR16                 *PartType;
    CHAR16                 *TempUUID;
    CHAR16                 *VentoyName;
    BOOLEAN                 DupFlag;
    BOOLEAN                 CheckedAPFS;
    BOOLEAN                 FoundVentoy;
    EFI_GUID                VolumeGuid = NULL_GUID_VALUE;
    EFI_GUID               *UuidList;
    APPLE_APFS_VOLUME_ROLE  VolumeRole;

    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr;
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

    MsgStr = StrDuplicate (L"E X A M I N E   A V A I L A B L E   V O L U M E S");
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
    LOG_MSG("\n\n");
    LOG_MSG("%s", MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    if (SelfVolRun) {
        // Clear Volume Lists if not Scanning for Self Volume
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
        ALT_LOG(1, LOG_THREE_STAR_SEP, L"%s!!", MsgStr);
        LOG_MSG("\n\n");
        LOG_MSG("** %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        return;
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Found Handles for %d Volumes",
        HandleCount
    );
    #endif

    UuidList = AllocateZeroPool (sizeof (EFI_GUID) * HandleCount);
    if (UuidList == NULL) {
        #if REFIT_DEBUG > 0
        Status = EFI_BUFFER_TOO_SMALL;

        MsgStr = PoolPrint (
            L"In ScanVolumes ... '%r' While Allocating 'UuidList'",
            Status
        );
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        ALT_LOG(1, LOG_THREE_STAR_SEP, L"%s!!", MsgStr);
        LOG_MSG("\n\n");
        LOG_MSG("** WARN: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        return;
    }

    // First Pass: Collect information about all handles
    DoneHeadings = FALSE;
    ScannedOnce  = FALSE;
    SkipSpacing  = FALSE;
    CheckedAPFS  = FALSE;
    ValidAPFS    =  TRUE;

    #if REFIT_DEBUG > 0
    FoundMBR     = FALSE;
    FirstVolume  =  TRUE;
    #endif

    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
        #if REFIT_DEBUG > 0
        /* Exception for LOG_THREE_STAR_SEP */
        ALT_LOG(1, LOG_THREE_STAR_SEP, L"NEXT VOLUME");
        #endif

        Volume = AllocateZeroPool (sizeof (REFIT_VOLUME));
        if (Volume == NULL) {
            MY_FREE_POOL(UuidList);

            #if REFIT_DEBUG > 0
            Status = EFI_BUFFER_TOO_SMALL;

            MsgStr = PoolPrint (
                L"In ScanVolumes ... '%r' While Allocating 'Volumes'",
                Status
            );
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            ALT_LOG(1, LOG_THREE_STAR_SEP, L"%!!s", MsgStr);
            LOG_MSG("\n\n");
            LOG_MSG("** WARN: %s", MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
            #endif

            return;
        }

        Volume->DeviceHandle = Handles[HandleIndex];
        AddPartitionTable (Volume);
        ScanVolume (Volume);

        UuidList[HandleIndex] = Volume->VolUuid;
        // Deduplicate filesystem UUID so that we do not add duplicate entries for file systems
        // that are part of RAID mirrors. Do not deduplicate ESP partitions though, since unlike
        // normal file systems they are likely to all share the same volume UUID, and it is also
        // unlikely that they are part of software RAID mirrors.
        for (i = 0; i < HandleIndex; i++) {
            if (GlobalConfig.ScanAllESP) {
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
            else {
                DupFlag = (
                    (CompareMem (&(Volume->VolUuid), &(UuidList[i]), sizeof (EFI_GUID)) == 0) &&
                    (CompareMem (&(Volume->VolUuid), &GuidNull,      sizeof (EFI_GUID)) != 0)
                );
            }

            if (DupFlag) {
                // This is a duplicate filesystem item
                Volume->IsReadable = FALSE;
                break;
            }
        } // for

        if (SelfVolRun) {
            // Update/Create 'Volumes' List if not Scanning for Self Volume
            AddListElement (
                (VOID ***) &Volumes,
                &VolumesCount,
                Volume
            );
        }

        if (Volume->DeviceHandle == SelfLoadedImage->DeviceHandle) {
            SelfVolSet = TRUE;
            SelfVolume = CopyVolume (Volume);
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
                    "%-39s%-39s%-21s%-39s%-20s%s",
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
                                    LOG_MSG("%s", AND_JOIN_TXT);
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

            // 'FSTypeName' returns a constant ... Do not free 'PartType'!
            PartType = FSTypeName (Volume);
            if (0);
            else if (FindSubStr (PartType, L"NTFS"    )) Volume->FSType = FS_TYPE_NTFS   ;
            else if (FindSubStr (PartType, L"APFS"    )) Volume->FSType = FS_TYPE_APFS   ;
            else if (FindSubStr (PartType, L"HFS+"    )) Volume->FSType = FS_TYPE_HFSPLUS;
            else if (FindSubStr (PartType, L"ISO-9660")) Volume->FSType = FS_TYPE_ISO9660;

            RoleStr = NULL;
            VolumeRole = 0;
            if (0);
            else if (FindSubStr (Volume->VolName, L"APFS/FileVault"         )) RoleStr = L"0xCC - Container" ;
            else if (FindSubStr (Volume->VolName, L"Optical Disc Drive"     )) RoleStr = L" * Type Optical"  ;
            else if (FindSubStr (PartType,        L"Mac Raid"               )) RoleStr = L" * Part MacRaid"  ;
            else if (MyStriCmp (Volume->VolName,  L"Whole Disk Volume"      )) RoleStr = L" * Disk Physical" ;
            else if (MyStriCmp (Volume->VolName,  L"Unknown Volume"         )) RoleStr = L"?? Role Unknown"  ;
            else if (MyStriCmp (Volume->VolName,  L"BOOTCAMP"               )) RoleStr = L" * Mac BootCamp"  ;
            else if (MyStriCmp (Volume->VolName,  L"Boot OS X"              )) RoleStr = L" * Mac BootAssist";
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidESP       )) RoleStr = L" * Part SystemEFI";
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidLinux     )) RoleStr = L" * Part LinuxFS"  ;
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidHomeGPT   )) RoleStr = L" * Part HomeGPT"  ;
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidServerGPT )) RoleStr = L" * Part ServerGPT";
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidBasicData )) RoleStr = L" * Part BasicData";
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidWindowsRE )) RoleStr = L" * Win Recovery"  ;
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidRecoveryHD)) RoleStr = L" * HFS Recovery"  ;
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidReservedMS)) RoleStr = L" * Msft Reserved" ;
            else {
// DA-TAG: Limit to TianoCore
#ifndef __MAKEWITH_TIANO
                Status = EFI_NOT_STARTED;
#else
                Status = RP_GetApfsVolumeInfo (
                    Volume->DeviceHandle,
                    NULL,
                    &VolumeGuid,
                    &VolumeRole
                );
#endif

                if (!EFI_ERROR(Status)) {
                    if (!CheckedAPFS) {
                        CheckedAPFS = TRUE;
                        // DA-TAG: Do not scan any volumes called "Recovery" if APFS is present
                        //         Apply the same to 'Update' and 'VM'
                        MergeUniqueStrings (&GlobalConfig.DontScanVolumes, L"Recovery", L',');
                        MergeUniqueStrings (&GlobalConfig.DontScanVolumes, L"Update", L','  );
                        MergeUniqueStrings (&GlobalConfig.DontScanVolumes, L"VM", L','      );
                    }

                    PartType        = L"APFS";
                    Volume->FSType  = FS_TYPE_APFS;
                    Volume->VolUuid = VolumeGuid;
                    #if REFIT_DEBUG > 0
                    RoleStr         = GetApfsRoleString (VolumeRole);
                    #endif

                    // DA-TAG: Update FreeSyncVolumes() if expanding this
                    if (ValidAPFS) {
                        if (VolumeRole == APPLE_APFS_VOLUME_ROLE_RECOVERY) {
                            // Set as 'UnReadable' to boost load speed
                            Volume->IsReadable = FALSE;
                            // Create or add to a list representing APFS VolumeGroups
                            AddListElement (
                                (VOID ***) &RecoveryVolumes,
                                &RecoveryVolumesCount,
                                CopyVolume (Volume)
                            );

                            // Flag NULL VolUUID or PartGuid if found and not previously flagged
                            if (GuidsAreEqual (&GuidNull, &(Volume->VolUuid)) ||
                                GuidsAreEqual (&GuidNull, &(Volume->PartGuid))
                            ) {
                                ValidAPFS = FALSE;
                            }
                        }
                        else if (VolumeRole == APPLE_APFS_VOLUME_ROLE_DATA) {
                            // Set as 'UnReadable' to boost load speed
                            Volume->IsReadable = FALSE;
                            // Create or add to a list representing APFS VolumeGroups
                            AddListElement (
                                (VOID ***) &DataVolumes,
                                &DataVolumesCount,
                                CopyVolume (Volume)
                            );

                            // Flag NULL VolUUID or PartGuid if found and not previously flagged
                            if (GuidsAreEqual (&GuidNull, &(Volume->VolUuid)) ||
                                GuidsAreEqual (&GuidNull, &(Volume->PartGuid))
                            ) {
                                ValidAPFS = FALSE;
                            }
                        }
                        else if (VolumeRole == APPLE_APFS_VOLUME_ROLE_PREBOOT) {
                            // Create or add to a list of APFS PreBoot Volumes
                            AddListElement (
                                (VOID ***) &PreBootVolumes,
                                &PreBootVolumesCount,
                                CopyVolume (Volume)
                            );

                            // Flag NULL VolUUID or PartGuid if found and not previously flagged
                            if (GuidsAreEqual (&GuidNull, &(Volume->VolUuid)) ||
                                GuidsAreEqual (&GuidNull, &(Volume->PartGuid))
                            ) {
                                ValidAPFS = FALSE;
                            }
                        }
                        else if (
                            VolumeRole == APPLE_APFS_VOLUME_ROLE_SYSTEM ||
                            VolumeRole == APPLE_APFS_VOLUME_ROLE_UNDEFINED
                        ) {
                            // Create or add to a list of APFS System Volumes
                            AddListElement (
                                (VOID ***) &SystemVolumes,
                                &SystemVolumesCount,
                                CopyVolume (Volume)
                            );

                            if (!ShouldScan (Volume, MACOSX_LOADER_DIR)) {
                                // Create or add to a list of APFS Volumes not to be scanned
                                AddListElement (
                                    (VOID ***) &SkipApfsVolumes,
                                    &SkipApfsVolumesCount,
                                    CopyVolume (Volume)
                                );
                            }

                            // Flag NULL VolUUID or PartGuid if found and not previously flagged
                            if (GuidsAreEqual (&GuidNull, &(Volume->VolUuid)) ||
                                GuidsAreEqual (&GuidNull, &(Volume->PartGuid))
                            ) {
                                ValidAPFS = FALSE;
                            }
                        }
                        else {
                            // Set any other APFS  Volume as 'UnReadable' to boost load speed
                            Volume->IsReadable = FALSE;
                        }
                    } // if ValidAPFS
                } // if !EFI_ERROR(Status)
            } // if MyStriCmp Volume->VolName

            if (!RoleStr) {
                if (Volume->FSType == FS_TYPE_HFSPLUS) {
                    if (!GuidsAreEqual (&GuidHFS, &(Volume->PartTypeGuid))) {
                        #if REFIT_DEBUG > 0
                        PartType        = L"Unknown";
                        #endif
                        Volume->FSType  = FS_TYPE_UNKNOWN;
                    }
                    else {
                        RoleStr = (FileExists (Volume->RootDir, MACOSX_LOADER_PATH))
                            ? L" * HFS Boot macOS"
                            : L" * HFS Data/Other";
                    }
                }
                else if (Volume->FSType == FS_TYPE_NTFS) {
                    if (MyStriCmp (Volume->VolName, L"NTFS Volume")) {
                        RoleStr = L" * Win Volume";
                    }
                    else {
                        RoleStr = L" * Win Other";
                    }
                }
            }

            if (!RoleStr) {
                if (GlobalConfig.HandleVentoy) {
                    j = 0;
                    FoundVentoy = FALSE;
                    while (!FoundVentoy && (VentoyName = FindCommaDelimited (VENTOY_NAMES, j++)) != NULL) {
                        if (MyStriCmp (Volume->VolName, VentoyName)) {
                            FoundVentoy = TRUE;
                            RoleStr = L" * Part Ventoy";
                        }
                        MY_FREE_POOL(VentoyName);
                    } // while
                }

                if (!RoleStr) {
                    RoleStr = L"** Role Undefined";
                }
            }
            else if (FindSubStr (RoleStr, L"HFS Recovery")) {
                // Create or add to a list of bootable HFS+ volumes
                AddListElement (
                    (VOID ***) &HfsRecovery,
                    &HfsRecoveryCount,
                    CopyVolume (Volume)
                );
            }

            #if REFIT_DEBUG > 0
            // Allocate Pools for Log Details
            PartName     = StrDuplicate (PartType);
            PartGUID     = GuidAsString (&(Volume->PartGuid));
            PartTypeGUID = GuidAsString (&(Volume->PartTypeGuid));
            VolumeUUID   = GuidAsString (&(Volume->VolUuid));

            // Control PartName Length
            LimitStringLength (PartName, 18);

            MsgStr = PoolPrint (
                L"%-36s : %-36s : %-18s : %-36s : %-17s : %s",
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
        FirstVolume   = FALSE;
        #endif

        ScannedOnce = TRUE;
    } // for: first pass

    MY_FREE_POOL(UuidList);
    MY_FREE_POOL(Handles);

    if (!SelfVolSet || !SelfVolRun) {
        SelfVolRun = TRUE;

        return;
    }

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"%-39s%-39s%-21s%-39s%-20s%s",
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
        // check MBR partition table for extended partitions
        if (Volume->BlockIO != NULL && Volume->WholeDiskBlockIO != NULL &&
            Volume->BlockIO == Volume->WholeDiskBlockIO && Volume->BlockIOOffset == 0 &&
            Volume->MbrPartitionTable != NULL
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
        if (Volume->BlockIO != NULL && Volume->WholeDiskBlockIO != NULL &&
            Volume->BlockIO != Volume->WholeDiskBlockIO
        ) {
            for (VolumeIndex2 = 0; VolumeIndex2 < VolumesCount; VolumeIndex2++) {
                if (Volumes[VolumeIndex2]->BlockIO == Volume->WholeDiskBlockIO &&
                    Volumes[VolumeIndex2]->BlockIOOffset == 0
                ) {
                    WholeDiskVolume = Volumes[VolumeIndex2];
                }
            }
        }

        if (WholeDiskVolume != NULL &&
            WholeDiskVolume->MbrPartitionTable != NULL
        ) {
            // Check if this volume is one of the partitions in the table
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

                // Compare boot sector read through offset vs. directly
                Status = REFIT_CALL_5_WRAPPER(
                    Volume->BlockIO->ReadBlocks, Volume->BlockIO,
                    Volume->BlockIO->Media->MediaId, Volume->BlockIOOffset,
                    512, SectorBuffer1
                );
                if (EFI_ERROR(Status)) {
                    break;
                }
                Status = REFIT_CALL_5_WRAPPER(
                    Volume->WholeDiskBlockIO->ReadBlocks, Volume->WholeDiskBlockIO,
                    Volume->WholeDiskBlockIO->Media->MediaId, MbrTable[PartitionIndex].StartLBA,
                    512, SectorBuffer2
                );
                if (EFI_ERROR(Status)) {
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
            }

            MY_FREE_POOL(SectorBuffer1);
            MY_FREE_POOL(SectorBuffer2);
        }
    } // for

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"Enumerated %d Volume%s",
        VolumesCount, (VolumesCount == 1) ? L"" : L"s"
    );
    LOG_MSG("INFO: %s", MsgStr); // Skip Line Break
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    if (SelfVolRun && GlobalConfig.SyncAPFS) {
        VetSyncAPFS();
    }
    else {
        #if REFIT_DEBUG > 0
        // DA-TAG: if/else structure is important
        if (!SelfVolRun) {
            LOG_MSG("\n\n");
        }
        else {
            MsgStr = StrDuplicate (
                L"Skipped APFS Volume Group Sync ... Config Setting is Active:- 'disable_apfs_sync'"
            );
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
            LOG_MSG("\n\n");
            LOG_MSG("INFO: %s", MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
        }
        #endif
    }

    #if REFIT_DEBUG > 0
    MuteLogger = FALSE; /* Explicit For FB Infer */
    #endif
} // VOID ScanVolumes()

static
VOID GetVolumeBadgeIcons (VOID) {
    UINTN         i, VolumeIndex;
    BOOLEAN       FoundSysVol;
    REFIT_VOLUME *Volume;

    #if REFIT_DEBUG > 0
    UINTN    LogLineType;
    CHAR16  *MsgStr;
    BOOLEAN  LoopOnce;

    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Seek Volume Badges for Internal Volumes");
    #endif

    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"GetVolumeBadgeIcons";
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  A - START", FuncTag);

    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_BADGES) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Skipped Volume Badge Check ... Config Setting is Active:- 'HideUI Badges'"
        );
        #endif

        BREAD_CRUMB(L"%s:  A1 - END:- VOID - (Skipped Check)", FuncTag);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    if (!AllowGraphicsMode) {
        #if REFIT_DEBUG > 0
        MsgStr = (GlobalConfig.DirectBoot)
            ? StrDuplicate (L"'DirectBoot' is Active")
            : StrDuplicate (L"Screen is in Text Mode");
        ALT_LOG(1, LOG_LINE_NORMAL, L"Skipped Volume Badge Check ... %s", MsgStr);
        MY_FREE_POOL(MsgStr);
        #endif

        BREAD_CRUMB(L"%s:  A2 - END:- VOID - (DirectBoot or Text Mode is Active)", FuncTag);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    #if REFIT_DEBUG > 0
    LoopOnce = FALSE;
    #endif
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        Volume = Volumes[VolumeIndex];

        // Skip 'UnReadable' volumes
        if (!Volume->IsReadable) {
            continue;
        }

        // Skip volumes in 'DontScanVolumes' list
        if (IsIn (Volume->VolName, GlobalConfig.DontScanVolumes)) {
            continue;
        }

        if (GlobalConfig.SyncAPFS) {
            FoundSysVol = FALSE;
            for (i = 0; i < SystemVolumesCount; i++) {
                if (GuidsAreEqual (&(SystemVolumes[i]->VolUuid), &(Volume->VolUuid))) {
                    FoundSysVol = TRUE;
                    break;
                }
            }

            // Skip APFS system volumes when SyncAPFS is active
            if (FoundSysVol) {
                continue;
            }
        }

        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (
            L"Setting VolumeBadge for '%s'",
            Volume->VolName
        );

        LogLineType = (LoopOnce) ? LOG_STAR_HEAD_SEP : LOG_THREE_STAR_MID;
        ALT_LOG(1, LogLineType, L"%s", MsgStr);
        MY_FREE_POOL(MsgStr);
        #endif

        // Set volume badge icon
        SetVolumeBadgeIcon (Volume);

        #if REFIT_DEBUG > 0
        MsgStr = (Volume->VolBadgeImage == NULL)
            ? StrDuplicate (L"VolumeBadge Not Found")
            : StrDuplicate (L"VolumeBadge Found");
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        MY_FREE_POOL(MsgStr);

        LoopOnce = TRUE;
        #endif
    } // for

    BREAD_CRUMB(L"%s:  Z - END:- VOID", FuncTag);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID GetVolumeBadgeIcons()

VOID SetVolumeIcons (VOID) {
    UINTN         i, VolumeIndex;
    BOOLEAN       FoundSysVol;
    REFIT_VOLUME *Volume;

    #if REFIT_DEBUG > 0
    UINTN    LogLineType;
    CHAR16  *MsgStr;
    BOOLEAN  LoopOnce;
    #endif

    // Set volume badge icon
    GetVolumeBadgeIcons();

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP,
        L"Seek '.VolumeIcon' Icons for %s Volumes",
        (GlobalConfig.HiddenIconsExternal) ? L"Internal/External" : L"Internal"
    );
    #endif

    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"SetVolumeIcons";
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  A - START", FuncTag);

    if (GlobalConfig.HelpIcon || GlobalConfig.HiddenIconsIgnore) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Skipped '.VolumeIcon' Check ... Config Setting is %sActive:- '%s'",
            (GlobalConfig.HelpIcon) ? L"*NOT* " : L"",
            (GlobalConfig.HelpIcon) ? L"decline_help_icon" : L"hidden_icons_ignore"
        );
        #endif

        BREAD_CRUMB(L"%s:  A1 - END:- VOID - (Skipped Check)", FuncTag);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    if (!AllowGraphicsMode) {
        #if REFIT_DEBUG > 0
        MsgStr = (GlobalConfig.DirectBoot)
            ? StrDuplicate (L"'DirectBoot' is Active")
            : StrDuplicate (L"Screen is in Text Mode");
        ALT_LOG(1, LOG_LINE_NORMAL, L"Skipped '.VolumeIcon' Check ... %s", MsgStr);
        MY_FREE_POOL(MsgStr);
        #endif

        BREAD_CRUMB(L"%s:  A2 - END:- VOID - (DirectBoot or Text Mode is Active)", FuncTag);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    #if REFIT_DEBUG > 0
    LoopOnce = FALSE;
    #endif
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        Volume = Volumes[VolumeIndex];

        // Skip 'UnReadable' volumes
        if (!Volume->IsReadable) {
            continue;
        }

        // Skip volumes in 'DontScanVolumes' list
        if (IsIn (Volume->VolName, GlobalConfig.DontScanVolumes)) {
            continue;
        }

        if (GlobalConfig.SyncAPFS) {
            FoundSysVol = FALSE;
            for (i = 0; i < SystemVolumesCount; i++) {
                if (GuidsAreEqual (&(SystemVolumes[i]->VolUuid), &(Volume->VolUuid))) {
                    FoundSysVol = TRUE;
                    break;
                }
            }

            // Skip APFS system volumes when SyncAPFS is active
            if (FoundSysVol) {
                continue;
            }
        }

        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (
            L"Setting '.VolumeIcon' Icon for '%s'",
            Volume->VolName
        );

        LogLineType = (LoopOnce) ? LOG_STAR_HEAD_SEP : LOG_THREE_STAR_MID;
        ALT_LOG(1, LogLineType, L"%s", MsgStr);
        MY_FREE_POOL(MsgStr);
        #endif

        // Load custom volume icon for internal/external disks if present
        if (!Volume->VolIconImage) {
            if ((Volume->DiskKind == DISK_KIND_INTERNAL) ||
                (Volume->DiskKind == DISK_KIND_EXTERNAL && GlobalConfig.HiddenIconsExternal)
            ) {
                Volume->VolIconImage = egLoadIconAnyType (
                    Volume->RootDir,
                    L"",
                    L".VolumeIcon",
                    GlobalConfig.IconSizes[ICON_SIZE_BIG]
                );
            }
            else {
                #if REFIT_DEBUG > 0
                if (Volume->DiskKind == DISK_KIND_EXTERNAL) {
                    ALT_LOG(1, LOG_LINE_NORMAL,
                        L"Skipped External Volume: '%s' ... Config Setting is *NOT* Active:- 'hidden_icons_external'",
                        Volume->VolName
                    );
                }
                else {
                    ALT_LOG(1, LOG_LINE_NORMAL,
                        L"Skipped '%s' ... Not Internal Volume",
                        Volume->VolName
                    );
                }
                #endif
            }
        }
        else {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"Skipped '%s' ... Icon Already Set",
                Volume->VolName
            );
            #endif
        }

        #if REFIT_DEBUG > 0
        LoopOnce = TRUE;
        #endif
    } // for

    BREAD_CRUMB(L"%s:  Z - END:- VOID", FuncTag);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID SetVolumeIcons()

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
}

static
EFI_STATUS DirNextEntry (
    IN     EFI_FILE_PROTOCOL      *Directory,
    OUT    EFI_FILE_INFO         **DirEntry,
    IN     UINTN                   FilterMode
) {
    EFI_STATUS  Status = EFI_BAD_BUFFER_SIZE;
    UINTN       LastBufferSize;
    UINTN       BufferSize;
    VOID       *Buffer;
    INTN        IterCount;

    #if REFIT_DEBUG > 0
    BOOLEAN FirstRun;
    CHAR16  *MsgStr;
    #endif

    //#if REFIT_DEBUG > 1
    //CHAR16 *FuncTag = L"DirNextEntry";
    //#endif

    //LOG_SEP(L"X");
    //LOG_INCREMENT();
    //BREAD_CRUMB(L"%s:  1 - START", FuncTag);

    //BREAD_CRUMB(L"%s:  2", FuncTag);
    #if REFIT_DEBUG > 0
    FirstRun = TRUE;
    #endif
    for (;;) {
        //LOG_SEP(L"X");
        //BREAD_CRUMB(L"%s:  2a 1 - FOR LOOP:- START", FuncTag);
        *DirEntry = NULL;

        // Read next directory entry
        //BREAD_CRUMB(L"%s:  2a 2", FuncTag);
        LastBufferSize = BufferSize = 256;
        Buffer         = AllocatePool (BufferSize);
        if (Buffer == NULL) {
            //BREAD_CRUMB(L"%s:  2a 2a 1 - END:- return EFI_STATUS = 'Bad Buffer Size'", FuncTag);

            return EFI_BAD_BUFFER_SIZE;
        }

        //BREAD_CRUMB(L"%s:  2a 3", FuncTag);
        for (IterCount = 0; ; IterCount++) {
            //LOG_SEP(L"X");
            //BREAD_CRUMB(L"%s:  2a 3a 1 - FOR LOOP:- START", FuncTag);
            Status = REFIT_CALL_3_WRAPPER(
                Directory->Read, Directory,
                &BufferSize, Buffer
            );

            //BREAD_CRUMB(L"%s:  2a 3a 2", FuncTag);
            if (Status != EFI_BUFFER_TOO_SMALL || IterCount > 3) {
                //BREAD_CRUMB(L"%s:  2a 3a 2a 1", FuncTag);
                #if REFIT_DEBUG > 0
                if (!FirstRun) {
                    //BREAD_CRUMB(L"%s:  2a 3a 2a 1a 1", FuncTag);
                    if (IterCount > 3) {
                        //BREAD_CRUMB(L"%s:  2a 3a 2a 1a 1a 1", FuncTag);
                        MsgStr = StrDuplicate (L"IterCount > 3 ... Break");
                    }
                    else {
                        //BREAD_CRUMB(L"%s:  2a 3a 2a 1a 1b 1", FuncTag);
                        MsgStr = StrDuplicate (L"OK ... Break");
                    }
                    //BREAD_CRUMB(L"%s:  2a 3a 2a 1a 2", FuncTag);
                    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                    LOG_MSG(":- '%s'", MsgStr);
                    MY_FREE_POOL(MsgStr);
                }
                #endif

                //BREAD_CRUMB(L"%s:  2a 3a 2a 2 - FOR LOOP:- BREAK ... Status/IterCount", FuncTag);
                //LOG_SEP(L"X");

                break;
            }
            else {
                //BREAD_CRUMB(L"%s:  2a 3a 2b 1", FuncTag);
                #if REFIT_DEBUG > 0
                if (!FirstRun) {
                    //BREAD_CRUMB(L"%s:  2a 3a 2b 1a 1", FuncTag);
                    MsgStr = StrDuplicate (L"NOT OK!!");
                    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                    LOG_MSG(":- '%s'", MsgStr);
                    MY_FREE_POOL(MsgStr);
                }
                #endif
                //BREAD_CRUMB(L"%s:  2a 3a 2b 2", FuncTag);
            }

            //BREAD_CRUMB(L"%s:  2a 3a 3", FuncTag);
            #if REFIT_DEBUG > 0
            LOG_MSG("\n");
            #endif
            if (BufferSize <= LastBufferSize) {
                //BREAD_CRUMB(L"%s:  2a 3a 3a 1", FuncTag);
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
                //BREAD_CRUMB(L"%s:  2a 3a 3b 1", FuncTag);
                #if REFIT_DEBUG > 0
                MsgStr = PoolPrint (
                    L"Resizing DirEntry Buffer From %d to %d Bytes",
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

            //BREAD_CRUMB(L"%s:  2a 3a 4", FuncTag);
            Buffer = EfiReallocatePool (
                Buffer, LastBufferSize, BufferSize
            );
            LastBufferSize = BufferSize;

            //BREAD_CRUMB(L"%s:  2a 3a 5 - FOR LOOP:- END", FuncTag);
            //LOG_SEP(L"X");
        } // for IterCount = 0

        #if REFIT_DEBUG > 0
        FirstRun = TRUE;
        #endif

        //BREAD_CRUMB(L"%s:  2a 4", FuncTag);
        if (EFI_ERROR(Status)) {
            //BREAD_CRUMB(L"%s: 2a 4a 1 - FOR LOOP:- BREAK ... Status Error", FuncTag);
            //LOG_SEP(L"X");

            MY_FREE_POOL(Buffer);
            break;
        }

        // Check for End of Listing
        //BREAD_CRUMB(L"%s:  2a 5", FuncTag);
        if (BufferSize == 0) {
            //BREAD_CRUMB(L"%s: 2a 5a 1 - FOR LOOP:- BREAK ... End of Listing", FuncTag);
            //LOG_SEP(L"X");

            // End of Directory Listing
            MY_FREE_POOL(Buffer);
            break;
        }

        // Entry is ready to be returned
        //BREAD_CRUMB(L"%s:  2a 6", FuncTag);
        *DirEntry = (EFI_FILE_INFO *) Buffer;

        // Filter results
        //BREAD_CRUMB(L"%s:  2a 7", FuncTag);
        if (FilterMode == 1) {
            //BREAD_CRUMB(L"%s:  2a 7a 1", FuncTag);
            // Only return directories
            if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY)) {
                //BREAD_CRUMB(L"%s:  2a 7a 1a 1 - FOR LOOP:- BREAK ... EFI_FILE_DIRECTORY", FuncTag);
                //LOG_SEP(L"X");

                break;
            }
        }
        else if (FilterMode == 2) {
            //BREAD_CRUMB(L"%s:  2a 7b 1", FuncTag);
            // Only return files
            if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY) == 0) {
                //BREAD_CRUMB(L"%s:  2a 7b 1a 1 - FOR LOOP:- BREAK ... EFI_FILE_DIRECTORY == 0", FuncTag);
                //LOG_SEP(L"X");

                break;
            }
        }

        //BREAD_CRUMB(L"%s:  3a 8 - FOR LOOP:- END ... No Filter or Unknown Filter", FuncTag);
        //LOG_SEP(L"X");

        // No Filter or Unknown Filter -> Return Everything
    } // for ;;

    //BREAD_CRUMB(L"%s:  3 - END:- return EFI_STATUS Status = '%r'", FuncTag,
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

    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"DirIterOpen";
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  1 - START", FuncTag);


    BREAD_CRUMB(L"%s:  2", FuncTag);
    if (RelativePath == NULL) {
        BREAD_CRUMB(L"%s:  2a 1 - RelativePath == NULL", FuncTag);
        DirIter->LastStatus     = EFI_SUCCESS;
        DirIter->DirHandle      = BaseDir;
        DirIter->CloseDirHandle = FALSE;
    }
    else {
        BREAD_CRUMB(L"%s:  2b 1 - RelativePath != NULL", FuncTag);
        DirIter->LastStatus = REFIT_CALL_5_WRAPPER(
            BaseDir->Open, BaseDir,
            &(DirIter->DirHandle), RelativePath,
            EFI_FILE_MODE_READ, 0
        );
        DirIter->CloseDirHandle = EFI_ERROR(DirIter->LastStatus) ? FALSE : TRUE;
    }
    BREAD_CRUMB(L"%s:  3 - CloseDirHandle = '%s'", FuncTag,
        (DirIter->CloseDirHandle) ? L"TRUE" : L"FALSE"
    );

    BREAD_CRUMB(L"%s:  4 - END:- VOID", FuncTag);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID DirIterOpen()

#if defined(__MAKEWITH_TIANO)
EFI_UNICODE_COLLATION_PROTOCOL * OcUnicodeCollationEngInstallProtocol (IN BOOLEAN  Reinstall);
#endif

static
BOOLEAN RP_MetaiMatch (
    IN CHAR16 *String,
    IN CHAR16 *Pattern
) {
#if defined (__MAKEWITH_GNUEFI)
    return MetaiMatch (String, Pattern);
#elif defined(__MAKEWITH_TIANO)
    static EFI_UNICODE_COLLATION_PROTOCOL *UnicodeCollationEng = NULL;

    if (!UnicodeCollationEng) {
        UnicodeCollationEng = OcUnicodeCollationEngInstallProtocol (GlobalConfig.UnicodeCollation);
    }
    if (UnicodeCollationEng) {
        return UnicodeCollationEng->MetaiMatch (UnicodeCollationEng, String, Pattern);
    }

    // DA-TAG: Fallback on original inadequate upstream implementation
    //         Should not get here when support is present
    EFI_STATUS Status = REFIT_CALL_3_WRAPPER(
        gBS->LocateProtocol, &gEfiUnicodeCollation2ProtocolGuid,
        NULL, (VOID **) &UnicodeCollationEng
    );
    if (EFI_ERROR(Status)) {
        REFIT_CALL_3_WRAPPER(
            gBS->LocateProtocol, &gEfiUnicodeCollationProtocolGuid,
            NULL, (VOID **) &UnicodeCollationEng
        );
    }

    return FALSE;
#endif
} // static BOOLEAN RP_MetaiMatch()

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

    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"DirIterNext";
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  1 - START", FuncTag);

    BREAD_CRUMB(L"%s:  2", FuncTag);
    if (EFI_ERROR(DirIter->LastStatus)) {
        BREAD_CRUMB(L"%s:  2a 1 - END:- return BOOLEAN FALSE on DirIter->LastStatus Error", FuncTag);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Stop Iteration
        return FALSE;
    }

    BREAD_CRUMB(L"%s:  3", FuncTag);
    for (;;) {
        LOG_SEP(L"X");
        BREAD_CRUMB(L"%s:  3a 1 - FOR LOOP:- START", FuncTag);

        BREAD_CRUMB(L"%s:  3a 2", FuncTag);
        DirIter->LastStatus = DirNextEntry (
            DirIter->DirHandle,
            &LastFileInfo,
            FilterMode
        );

        BREAD_CRUMB(L"%s:  3a 3", FuncTag);
        if (EFI_ERROR(DirIter->LastStatus) || LastFileInfo == NULL) {
            BREAD_CRUMB(L"%s:  3a 3a 1 - END:- return BOOLEAN FALSE ... ERROR DirIter->LastStatus", FuncTag);
            LOG_DECREMENT();
            LOG_SEP(L"X");

            return FALSE;
        }

        BREAD_CRUMB(L"%s:  3a 4", FuncTag);
        if (FilePattern == NULL || LastFileInfo->Attribute & EFI_FILE_DIRECTORY) {
            BREAD_CRUMB(L"%s:  3a 4a 1 - END:- FilePattern == NULL ... return BOOLEAN TRUE", FuncTag);
            LOG_DECREMENT();
            LOG_SEP(L"X");

            *DirEntry = LastFileInfo;
            return TRUE;
        }

        BREAD_CRUMB(L"%s:  3a 5", FuncTag);
        i     =     0;
        Found = FALSE;
        while (!Found && (OnePattern = FindCommaDelimited (FilePattern, i++)) != NULL) {
            BREAD_CRUMB(L"%s:  3a 5a 1 - WHILE LOOP:- START ... Seek MetaiMatch Pattern", FuncTag);
            if (RP_MetaiMatch (LastFileInfo->FileName, OnePattern)) {
                BREAD_CRUMB(L"%s:  3a 5a 1a 1", FuncTag);
                Found = TRUE;
            }
            MY_FREE_POOL(OnePattern);
            BREAD_CRUMB(L"%s:  3a 5a 2 - WHILE LOOP:- END", FuncTag);
        } // while

        BREAD_CRUMB(L"%s:  3a 6", FuncTag);
        if (Found) {
            BREAD_CRUMB(L"%s:  3a 6 - END:- Found == TRUE ... return BOOLEAN TRUE", FuncTag);
            LOG_DECREMENT();
            LOG_SEP(L"X");

            *DirEntry = LastFileInfo;
            return TRUE;
        }
        BREAD_CRUMB(L"%s:  3a 7", FuncTag);
        MY_FREE_POOL(LastFileInfo);

        BREAD_CRUMB(L"%s:  3a 8 - FOR LOOP:- END", FuncTag);
        LOG_SEP(L"X");
    } // for
    BREAD_CRUMB(L"%s:  4 - END:- return BOOLEAN TRUE", FuncTag);
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return TRUE;
} // BOOLEAN DirIterNext()

EFI_STATUS DirIterClose (
    IN OUT REFIT_DIR_ITER *DirIter
) {
    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"DirIterClose";
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  1 - START", FuncTag);

    BREAD_CRUMB(L"%s:  2", FuncTag);
    if ((DirIter->CloseDirHandle) && (DirIter->DirHandle->Close)) {
        //BREAD_CRUMB(L"%s:  2a 1", FuncTag);
        REFIT_CALL_1_WRAPPER(DirIter->DirHandle->Close, DirIter->DirHandle);
    }

    BREAD_CRUMB(L"%s:  3 - END:- return EFI_STATUS DirIter->LastStatus = '%r'", FuncTag,
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
    if (Copy == NULL) {
        return NULL;
    }

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
}

// Takes an input pathname (*Path) and returns the part of the filename from
// the final dot onwards, converted to lowercase. If the filename includes
// no dots, or if the input is NULL, returns an empty (but allocated) string.
// The calling function is responsible for freeing the memory associated with
// the return value.
CHAR16 * FindExtension (
    IN CHAR16 *Path
) {
    INTN       i;
    CHAR16    *Extension;
    BOOLEAN    FoundSlash;
    BOOLEAN    Found;

    if (!Path) {
        return NULL;
    }

    Extension = AllocateZeroPool (sizeof (CHAR16));
    if (!Extension) {
        return NULL;
    }

    i = StrLen (Path);
    FoundSlash = Found = FALSE;
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
    }

    return (Extension);
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

    if (!Path) {
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

    return (Found);
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

    if (!FullPath) {
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

    return (PathOnly);
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

    if (!loadpath || !DeviceVolume || !loader) {
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

    MY_FREE_POOL(*VolName);
    MY_FREE_POOL(*Path);
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

    i = 0;
    Found = FALSE;
    while (!Found && (i < VolumesCount)) {
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
    EFI_GUID TargetVolGuid = NULL_GUID_VALUE;

    if ((Volume == NULL) || (Description == NULL)) {
        return FALSE;
    }

    if (!IsGuid (Description)) {
        return (
            MyStriCmp (Description, Volume->FsName)   ||
            MyStriCmp (Description, Volume->VolName)  ||
            MyStriCmp (Description, Volume->PartName)
        );
    }

    TargetVolGuid = StringAsGuid (Description);
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
    CHAR16    *OneElement;
    CHAR16    *TargetPath;
    CHAR16    *TargetVolName;
    CHAR16    *TargetFilename;
    BOOLEAN    Found;

    if (!Filename || !List) {
        return FALSE;
    }

    TargetPath = TargetVolName = TargetFilename = NULL;

    i = 0;
    Found = FALSE;
    while (!Found && (OneElement = FindCommaDelimited (List, i++))) {
        Found = TRUE;
        SplitPathName (OneElement, &TargetVolName, &TargetPath, &TargetFilename);
        if ((TargetVolName  != NULL && !VolumeMatchesDescription (Volume, TargetVolName)) ||
            (TargetPath     != NULL && !MyStriCmp (TargetPath, Directory)) ||
            (TargetFilename != NULL && !MyStriCmp (TargetFilename, Filename))
        ) {
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
        return (FALSE);
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

    while (*TheList) {
        NextItem = (*TheList)->Next;
        MY_FREE_POOL(*TheList);
        *TheList = NextItem;
    }
} // EraseUin32List()
