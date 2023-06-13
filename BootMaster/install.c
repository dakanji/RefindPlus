/*
 * BootMaster/install.c
 * Functions related to installation of RefindPlus and management of EFI boot order
 *
 * Copyright (c) 2020-2023 by Roderick W. Smith
 *
 * Distributed under the terms of the GNU General Public License (GPL)
 * version 3 (GPLv3), a copy of which must be distributed with this source
 * code or binaries made from it.
 *
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2023 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "icns.h"
#include "lib.h"
#include "screenmgt.h"
#include "install.h"
#include "scan.h"
#include "menu.h"
#include "mystrings.h"
#include "../include/refit_call_wrapper.h"
#include "../include/Handle.h"

// A linked-list data structure intended to hold a list of all the ESPs
// on the computer.
typedef struct _esp_list {
    REFIT_VOLUME     *Volume;  // Holds pointer to existing volume structure; DO NOT FREE!
    struct _esp_list *NextESP;
} ESP_LIST;


/***********************
 *
 * Functions related to management of ESP data.
 *
 ***********************/

// Delete the linked-list ESP_LIST data structure passed as an argument.
static
VOID DeleteESPList (
    ESP_LIST *AllESPs
) {
    ESP_LIST *Temp;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Deleting List of ESPs");
    #endif

    while (AllESPs != NULL) {
        Temp = AllESPs;
        AllESPs = AllESPs->NextESP;
        MY_FREE_POOL(Temp);
    } // while
} // VOID DeleteESPList()

// Return a list of all ESP volumes (ESP_LIST *) on internal disks EXCEPT
// for the current ESP. ESPs are identified by GUID type codes, which means
// that non-FAT partitions marked as ESPs may be returned as valid; and FAT
// partitions that are not marked as ESPs will not be returned.
static
ESP_LIST * FindAllESPs (VOID) {
    ESP_LIST *AllESPs;
    ESP_LIST *NewESP;
    UINTN     VolumeIndex;
    EFI_GUID  ESPGuid = ESP_GUID_VALUE;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Searching for ESPs");
    #endif

    AllESPs = NULL;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_INTERNAL
            && (
                Volumes[VolumeIndex]->FSType == FS_TYPE_FAT12 ||
                Volumes[VolumeIndex]->FSType == FS_TYPE_FAT16 ||
                Volumes[VolumeIndex]->FSType == FS_TYPE_FAT32
            )
            && GuidsAreEqual (&(Volumes[VolumeIndex]->PartTypeGuid), &ESPGuid)
            && !GuidsAreEqual (&(Volumes[VolumeIndex]->PartGuid), &SelfVolume->PartGuid)
        ) {
            NewESP = AllocateZeroPool (sizeof (ESP_LIST));

            if (NewESP != NULL) {
                NewESP->Volume  = Volumes[VolumeIndex];
                NewESP->NextESP = AllESPs;
                AllESPs         = NewESP;
            }
        }
    } // for

    return AllESPs;
} // static ESP_LIST * FindAllESPs()

/***********************
 *
 * Functions related to user interaction.
 *
 ***********************/


// Prompt the user to pick one ESP from among the provided list of ESPs.
// Returns a pointer to a REFIT_VOLUME describing the selected ESP. Note
// that the function uses the partition's unique GUID value to locate the
// REFIT_VOLUME, so if these values are not unique (as, for instance,
// after some types of disk cloning operations), the returned value may
// not be accurate.
static
REFIT_VOLUME * PickOneESP (
    ESP_LIST *AllESPs
) {
    UINTN              i;
    CHAR16            *FsName;
    CHAR16            *VolName;
    CHAR16            *GuidStr;
    CHAR16            *PartName;
    ESP_LIST          *CurrentESP;
    REFIT_VOLUME      *ChosenVolume;
    REFIT_MENU_ENTRY  *MenuEntryItem;
    REFIT_MENU_SCREEN *InstallMenu;

    INTN               DefaultEntry;
    UINTN              MenuExit;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenOption;


    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Prompt User to Select an ESP for Installation");
    #endif

    if (!AllESPs) {
        DisplaySimpleMessage (L"No Eligible ESPs Found", NULL);

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"No Eligible ESPs Found");
        #endif

        // Early Return
        return NULL;
    }

    InstallMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (InstallMenu == NULL) {
        // Early Return
        return NULL;
    }

    InstallMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_INSTALL                    );
    InstallMenu->Title      = StrDuplicate (L"Install RefindPlus"                       );
    InstallMenu->Hint1      = StrDuplicate (L"Select a destination and press 'Enter' or");
    InstallMenu->Hint2      = StrDuplicate (RETURN_MAIN_SCREEN_HINT                     );

    AddMenuInfoLine (
        InstallMenu,
        L"Select a Partition and Press 'Enter' to Install RefindPlus",
        FALSE
    );

    i = 1;
    CurrentESP = AllESPs;
    while (CurrentESP != NULL) {
        MenuEntryItem = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
        if (MenuEntryItem == NULL) {
            FreeMenuScreen (&InstallMenu);

            // Early Return
            return NULL;
        }

        GuidStr  = GuidAsString (&(CurrentESP->Volume->PartGuid));
        PartName = CurrentESP->Volume->PartName;
        FsName   = CurrentESP->Volume->FsName;
        VolName  = CurrentESP->Volume->VolName;

        if (PartName && (StrLen (PartName) > 0) &&
            FsName && (StrLen (FsName) > 0) &&
            !MyStriCmp (FsName, PartName)
        ) {
            MenuEntryItem->Title = PoolPrint (L"%s - '%s', AKA '%s'", GuidStr, PartName, FsName);
        }
        else if (FsName && (StrLen (FsName) > 0)) {
            MenuEntryItem->Title = PoolPrint (L"%s - '%s'", GuidStr, FsName);
        }
        else if (PartName && (StrLen (PartName) > 0)) {
            MenuEntryItem->Title = PoolPrint (L"%s - '%s'", GuidStr, PartName);
        }
        else if (VolName && (StrLen (VolName) > 0)) {
            MenuEntryItem->Title = PoolPrint (L"%s - '%s'", GuidStr, VolName);
        }
        else {
            MenuEntryItem->Title = PoolPrint (L"%s - No Name", GuidStr);
        }

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Adding '%s' to UI List of ESPs");
        #endif

        MenuEntryItem->Tag = TAG_RETURN;
        MenuEntryItem->Row = i++;

        AddMenuEntry (InstallMenu, MenuEntryItem);

        CurrentESP = CurrentESP->NextESP;

        MY_FREE_POOL(GuidStr);
    } // while

    do {
        ChosenVolume = NULL;
        if (!GetReturnMenuEntry (&InstallMenu)) {
            break;
        }

        DefaultEntry = 9999; // Use the Max Index
        Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
        MenuExit = RunGenericMenu (InstallMenu, Style, &DefaultEntry, &ChosenOption);

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Returned '%d' (%s) From RunGenericMenu Call on '%s' in 'PickOneESP'",
            MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
        );
        #endif

        if (ChosenOption->Tag == TAG_RETURN) {
            break;
        }

        if (MenuExit == MENU_EXIT_ENTER) {
            CHAR16 *Temp;
            CurrentESP = AllESPs;
            while (CurrentESP != NULL) {
                Temp = GuidAsString (&(CurrentESP->Volume->PartGuid));
                if (MyStrStr (ChosenOption->Title, Temp)) {
                    ChosenVolume = CurrentESP->Volume;
                }
                CurrentESP = CurrentESP->NextESP;
                MY_FREE_POOL(Temp);
            } // while
        }
    } while (0); // This 'loop' only runs once

    FreeMenuScreen (&InstallMenu);

    return ChosenVolume;
} // REFIT_VOLUME *PickOneESP()

/***********************
 *
 * Functions related to manipulation files on disk.
 *
 ***********************/

static
EFI_STATUS RenameFile (
    EFI_FILE_PROTOCOL  *BaseDir,
    CHAR16             *OldName,
    CHAR16             *NewName
) {
    EFI_STATUS          Status;
    UINTN               NewInfoSize;
    EFI_FILE_INFO      *Buffer;
    EFI_FILE_INFO      *NewInfo;
    EFI_FILE_PROTOCOL  *FilePtr;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Trying to Rename '%s' to '%s'", OldName, NewName);
    #endif

    Status = REFIT_CALL_5_WRAPPER(
        BaseDir->Open, BaseDir,
        &FilePtr, OldName,
        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0
    );
    if (EFI_ERROR(Status)) {
        // Early Return
        return Status;
    }

    do {
        NewInfo = NULL;
        Buffer = LibFileInfo (FilePtr);
        if (Buffer == NULL) {
            Status = EFI_BUFFER_TOO_SMALL;

            break;
        }

        NewInfoSize = sizeof (EFI_FILE_INFO) + StrSize (NewName);
        NewInfo = (EFI_FILE_INFO *) AllocateZeroPool (NewInfoSize);
        if (NewInfo == NULL) {
            Status = EFI_OUT_OF_RESOURCES;

            break;
        }

        CopyMem (NewInfo, Buffer, sizeof (EFI_FILE_INFO));
        NewInfo->FileName[0] = 0;
        StrCat (NewInfo->FileName, NewName);

        Status = REFIT_CALL_4_WRAPPER(
            BaseDir->SetInfo, FilePtr,
            &gEfiFileInfoGuid, NewInfoSize, (VOID *) NewInfo
        );
    } while (0); // This 'loop' only runs once

    REFIT_CALL_1_WRAPPER(FilePtr->Close, FilePtr);

    MY_FREE_POOL(NewInfo);
    MY_FREE_POOL(FilePtr);
    MY_FREE_POOL(Buffer);

    return Status;
} // EFI_STATUS RenameFile()

// Rename *FileName to add a "-old" extension, but only if that file does not
// already exist. Called on the icons directory to preserve it in case the
// user wants icons stored there that have been supplanted by new icons.
static
EFI_STATUS BackupOldFile (
    EFI_FILE_PROTOCOL   *BaseDir,
    CHAR16              *FileName
) {
    EFI_STATUS           Status;
    CHAR16              *NewName;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Backing '%s' Up", FileName);
    #endif

    if ((BaseDir == NULL) || (FileName == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    NewName = PoolPrint (L"%s-old", FileName);

    Status = (FileExists (BaseDir, NewName))
        ? EFI_SUCCESS
        : RenameFile (BaseDir, FileName, NewName);

    MY_FREE_POOL(NewName);

    return Status;
} // static EFI_STATUS BackupOldFile()

// Create directories in which RefindPlus will reside.
static
EFI_STATUS CreateDirectories (
    EFI_FILE_PROTOCOL  *BaseDir
) {
    EFI_STATUS          Status;
    UINTN               i;
    CHAR16             *FileName;
    EFI_FILE_PROTOCOL  *TheDir;

    i = 0;
    TheDir = NULL;
    FileName = NULL;
    Status = EFI_SUCCESS;
    while (!EFI_ERROR(Status) &&
        (FileName = FindCommaDelimited (INST_DIRECTORIES, i++)) != NULL
    ) {
        REFIT_CALL_5_WRAPPER(
            BaseDir->Open, BaseDir,
            &TheDir, FileName,
            ReadWriteCreate, EFI_FILE_DIRECTORY
        );

        Status = REFIT_CALL_1_WRAPPER(TheDir->Close, TheDir);

        MY_FREE_POOL(FileName);
        MY_FREE_POOL(TheDir);
    } // while

    return Status;
} // CreateDirectories()

static
EFI_STATUS CopyOneFile (
    EFI_FILE_PROTOCOL *SourceDir,
    CHAR16            *SourceName,
    EFI_FILE_PROTOCOL *DestDir,
    CHAR16            *DestName
) {
    EFI_STATUS          Status;
    UINTN              *Buffer;
    UINTN               FileSize;
    EFI_FILE_INFO      *FileInfo;
    EFI_FILE_PROTOCOL  *SourceFile;
    EFI_FILE_PROTOCOL  *DestFile;

    // Read the original file.
    SourceFile = NULL;
    Status = REFIT_CALL_5_WRAPPER(
        SourceDir->Open, SourceDir,
        &SourceFile, SourceName,
        EFI_FILE_MODE_READ, 0
    );
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Error:- '%r' When Opening SourceDir in 'CopyOneFile'",
            Status
        );
        #endif

        // Early Return
        return Status;
    }

    FileInfo = LibFileInfo (SourceFile);
    if (FileInfo == NULL) {
        REFIT_CALL_1_WRAPPER(SourceFile->Close, SourceFile);

        // Early Return
        return EFI_NO_RESPONSE;
    }

    FileSize = FileInfo->FileSize;
    MY_FREE_POOL(FileInfo);

    Buffer = AllocateZeroPool (FileSize);
    if (Buffer == NULL) {
        // Early Return
        return EFI_OUT_OF_RESOURCES;
    }

    Status = REFIT_CALL_3_WRAPPER(
        SourceFile->Read, SourceFile,
        &FileSize, Buffer
    );
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Error:- '%r' When Reading SourceFile in 'CopyOneFile'",
            Status
        );
        #endif

        // Early Return
        return Status;
    }

    REFIT_CALL_1_WRAPPER(SourceFile->Close, SourceFile);

    // Write the file to a new location.
    DestFile = NULL;
    Status = REFIT_CALL_5_WRAPPER(
        DestDir->Open, DestDir,
        &DestFile, DestName,
        ReadWriteCreate, 0
    );
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Error:- '%r' When Opening DestDir in 'CopyOneFile'",
            Status
        );
        #endif

        // Early Return
        return Status;
    }

    Status = REFIT_CALL_3_WRAPPER(
        DestFile->Write, DestFile,
        &FileSize, Buffer
    );
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Error:- '%r' When Writing to DestDir in 'CopyOneFile'",
            Status
        );
        #endif

        // Early Return
        return Status;
    }

    Status = REFIT_CALL_1_WRAPPER(DestFile->Close, DestFile);

    MY_FREE_POOL(SourceFile);
    MY_FREE_POOL(DestFile);
    MY_FREE_POOL(Buffer);

    #if REFIT_DEBUG > 0
    if (EFI_ERROR(Status)) {
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Error:- '%r' When Closing DestDir in 'CopyOneFile'",
            Status
        );
    }
    #endif

    return Status;
} // EFI_STATUS CopyOneFile()

// Copy a single directory (non-recursively)
static
EFI_STATUS CopyDirectory (
    EFI_FILE_PROTOCOL *SourceDirPtr,
    CHAR16            *SourceDirName,
    EFI_FILE_PROTOCOL *DestDirPtr,
    CHAR16            *DestDirName
) {
    EFI_STATUS         Status;
    CHAR16            *DestFileName;
    CHAR16            *SourceFileName;
    EFI_FILE_INFO     *DirEntry;
    REFIT_DIR_ITER     DirIter;

    DirIterOpen (SourceDirPtr, SourceDirName, &DirIter);

    Status = EFI_SUCCESS;
    while (!EFI_ERROR(Status) && DirIterNext (&DirIter, 2, NULL, &DirEntry)) {
        SourceFileName = PoolPrint (L"%s\\%s", SourceDirName, DirEntry->FileName);
        DestFileName   = PoolPrint (L"%s\\%s", DestDirName, DirEntry->FileName);
        Status = CopyOneFile (
            SourceDirPtr, SourceFileName,
            DestDirPtr, DestFileName
        );

        MY_FREE_POOL(DestFileName);
        MY_FREE_POOL(SourceFileName);
        MY_FREE_POOL(DirEntry);
    } // while

    return Status;
} // EFI_STATUS CopyDirectory()

// Copy Linux drivers for detected filesystems, but not for undetected filesystems.
// Note: Does NOT copy HFS+ driver on Apple hardware even if HFS+ is detected;
// but it DOES copy the HFS+ driver on non-Apple hardware if HFS+ is detected,
// even though HFS+ is not technically a Linux filesystem, since HFS+ CAN be used
// as a Linux /boot partition. That is weird, but it does work.
static
EFI_STATUS CopyDrivers (
    EFI_FILE_PROTOCOL *SourceDirPtr,
    CHAR16            *SourceDirName,
    EFI_FILE_PROTOCOL *DestDirPtr,
    CHAR16            *DestDirName
) {
    EFI_STATUS         Status;
    EFI_STATUS         WorstStatus;
    UINTN              i;
    CHAR16            *DestFileName;
    CHAR16            *SourceFileName;
    CHAR16            *DriverName;
    BOOLEAN            DriverCopied[NUM_FS_TYPES];


    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Scanning %d Volumes for Identifiable Filesystems",
        VolumesCount
    );
    #endif

    DriverCopied[FS_TYPE_UNKNOWN]   = FALSE;
    DriverCopied[FS_TYPE_WHOLEDISK] = FALSE;
    DriverCopied[FS_TYPE_FAT12]     = FALSE;
    DriverCopied[FS_TYPE_FAT16]     = FALSE;
    DriverCopied[FS_TYPE_FAT32]     = FALSE;
    DriverCopied[FS_TYPE_EXFAT]     = FALSE;
    DriverCopied[FS_TYPE_NTFS]      = FALSE;
    DriverCopied[FS_TYPE_EXT2]      = FALSE;
    DriverCopied[FS_TYPE_EXT3]      = FALSE;
    DriverCopied[FS_TYPE_EXT4]      = FALSE;
    DriverCopied[FS_TYPE_HFSPLUS]   = FALSE;
    DriverCopied[FS_TYPE_REISERFS]  = FALSE;
    DriverCopied[FS_TYPE_BTRFS]     = FALSE;
    DriverCopied[FS_TYPE_XFS]       = FALSE;
    DriverCopied[FS_TYPE_JFS]       = FALSE;
    DriverCopied[FS_TYPE_ISO9660]   = FALSE;
    DriverCopied[FS_TYPE_APFS]      = FALSE;


    // Initial Worst Status
    WorstStatus = EFI_SUCCESS;

    for (i = 0; i < VolumesCount; i++) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Looking for Driver for Volume # %d, '%s'",
            i, Volumes[i]->VolName
        );
        #endif

        DriverName = NULL;
        switch (Volumes[i]->FSType) {
            case FS_TYPE_BTRFS:
                if (DriverCopied[FS_TYPE_BTRFS] == FALSE) {
                    DriverName = L"btrfs";
                    DriverCopied[FS_TYPE_BTRFS] = TRUE;
                }

            break;
            case FS_TYPE_EXT2:
                if (DriverCopied[FS_TYPE_EXT2] == FALSE) {
                    DriverName = L"ext2";
                    DriverCopied[FS_TYPE_EXT2] = TRUE;
                    DriverCopied[FS_TYPE_EXT3] = TRUE;
                }

            break;
            case FS_TYPE_EXT3:
                if (DriverCopied[FS_TYPE_EXT3] == FALSE) {
                    DriverName = L"ext2";
                    DriverCopied[FS_TYPE_EXT2] = TRUE;
                    DriverCopied[FS_TYPE_EXT3] = TRUE;
                }

            break;
            case FS_TYPE_EXT4:
                if (DriverCopied[FS_TYPE_EXT4] == FALSE) {
                    DriverName = L"ext4";
                    DriverCopied[FS_TYPE_EXT4] = TRUE;
                }

            break;
            case FS_TYPE_REISERFS:
                if (DriverCopied[FS_TYPE_REISERFS] == FALSE) {
                    DriverName = L"reiserfs";
                    DriverCopied[FS_TYPE_REISERFS] = TRUE;
                }

            break;
            case FS_TYPE_HFSPLUS:
                if ((DriverCopied[FS_TYPE_HFSPLUS] == FALSE) &&
                    (!AppleFirmware)
                ) {
                    DriverName = L"hfs";
                    DriverCopied[FS_TYPE_HFSPLUS] = TRUE;
                }

            break;
            default: DriverName = NULL;
        } // switch

        if (DriverName) {
            SourceFileName = PoolPrint (
                L"%s\\%s%s",
                SourceDirName, DriverName,
                INST_PLATFORM_EXTENSION
            );
            DestFileName = PoolPrint (
                L"%s\\%s%s",
                DestDirName, DriverName,
                INST_PLATFORM_EXTENSION
            );

            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL, L"Trying to Copy Driver for %s", DriverName);
            #endif

            Status = CopyOneFile (
                SourceDirPtr, SourceFileName,
                DestDirPtr, DestFileName
            );
            if (EFI_ERROR(Status)) {
                WorstStatus = Status;
            }

            MY_FREE_POOL(SourceFileName);
            MY_FREE_POOL(DestFileName);
        } // if DriverNam
    } // for

    return WorstStatus;
} // EFI_STATUS CopyDrivers()

// Copy all the files from the source to *TargetDir
static
EFI_STATUS CopyFiles (
    EFI_FILE_PROTOCOL *TargetDir
) {
    EFI_STATUS         Status;
    EFI_STATUS         WorstStatus;
    CHAR16            *SourceFile;
    CHAR16            *SourceDir;
    CHAR16            *ConfFile;
    CHAR16            *RefindPlusName;
    CHAR16            *TargetDriversDir;
    CHAR16            *SourceDriversDir;
    REFIT_VOLUME      *SourceVolume;

    SourceFile   = NULL;
    SourceVolume = NULL; // Do not free

    FindVolumeAndFilename (
        GlobalConfig.SelfDevicePath,
        &SourceVolume, &SourceFile
    );
    SourceDir = FindPath (SourceFile);

    // Begin by copying RefindPlus itself.
    RefindPlusName = PoolPrint (L"EFI\\refindplus\\%s", INST_REFINDPLUS_NAME);
    Status = WorstStatus = CopyOneFile (
        SourceVolume->RootDir, SourceFile,
        TargetDir, RefindPlusName
    );
    MY_FREE_POOL(SourceFile);
    MY_FREE_POOL(RefindPlusName);
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Error When Copying RefindPlus Binary ... Installation has Failed!!"
        );
        #endif

        MY_FREE_POOL(SourceDir);

        return EFI_ABORTED;
    }

    // Now copy the config file -- but:
    //  - Copy config.conf-sample, not config.conf, if it is available, to
    //    avoid picking up live-disk-specific customizations.
    //  - Do not overwrite an existing config.conf at the target; instead,
    //    copy to config.conf-sample if config.conf is present.
    ConfFile = PoolPrint (L"%s\\config.conf-sample", SourceDir);
    if (FileExists (SourceVolume->RootDir, ConfFile)) {
        SourceFile = PoolPrint (L"%s", ConfFile);
    }
    else {
        SourceFile = PoolPrint (L"%s\\config.conf", SourceDir);
    }
    MY_FREE_POOL(ConfFile);

    // Note: CopyOneFile() logs errors if they occur
    if (FileExists (TargetDir, L"\\EFI\\refindplus\\config.conf")) {
        Status = CopyOneFile (
            SourceVolume->RootDir,
            SourceFile,
            TargetDir,
            L"EFI\\refindplus\\config.conf-sample"
        );
    }
    else {
        Status = CopyOneFile (
            SourceVolume->RootDir,
            SourceFile,
            TargetDir,
            L"EFI\\refindplus\\config.conf"
        );
    }

    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Error When Copying Config File:- '%d'", Status);
        #endif

        WorstStatus = Status;
    }
    MY_FREE_POOL(SourceFile);

    // Now copy icons.
    SourceFile = PoolPrint (L"%s\\icons", SourceDir);
    Status     = CopyDirectory (
        SourceVolume->RootDir,
        SourceFile,
        TargetDir,
        L"EFI\\refindplus\\icons"
    );

    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Error When Copying Icons:- '%d'", Status);
        #endif

        WorstStatus = Status;
    }
    MY_FREE_POOL(SourceFile);

    // Now copy drivers.
    SourceDriversDir = PoolPrint (L"%s\\%s", SourceDir, INST_DRIVERS_SUBDIR);
    TargetDriversDir = PoolPrint (L"EFI\\refindplus\\%s", INST_DRIVERS_SUBDIR);

    Status = CopyDrivers (
        SourceVolume->RootDir,
        SourceDriversDir,
        TargetDir,
        TargetDriversDir
    );

    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Error When Copying Drivers:- '%d'", Status);
        #endif

        WorstStatus = Status;
    }

    MY_FREE_POOL(SourceDir);
    MY_FREE_POOL(SourceDriversDir);
    MY_FREE_POOL(TargetDriversDir);

    return WorstStatus;
} // EFI_STATUS CopyFiles()

// Create the BOOT.CSV file used by the fallback.efi/fbx86.efi program.
// Success is not critical, so we do not return a Status value.
static
VOID CreateFallbackCSV (
    EFI_FILE_PROTOCOL *TargetDir
) {
    EFI_STATUS         Status;
    UINTN              FileSize;
    CHAR16            *Contents;
    EFI_FILE_PROTOCOL *FilePtr;

    Status = REFIT_CALL_5_WRAPPER(
        TargetDir->Open, TargetDir,
        &FilePtr, L"\\EFI\\refindplus\\BOOT.CSV",
        ReadWriteCreate, 0
    );

    if (!EFI_ERROR(Status)) {
        Contents = PoolPrint (
            L"%s, RefindPlus Boot Manager,,This is the Boot Entry for RefindPlus\n",
            INST_REFINDPLUS_NAME
        );

        if (Contents) {
            FileSize = StrSize (Contents);
            Status = REFIT_CALL_3_WRAPPER(
                FilePtr->Write, FilePtr,
                &FileSize, Contents
            );

            if (!EFI_ERROR(Status)) {
                REFIT_CALL_1_WRAPPER(FilePtr->Close, FilePtr);
            }
            MY_FREE_POOL(FilePtr);
        }

        MY_FREE_POOL(Contents);
    }

    #if REFIT_DEBUG > 0
    if (EFI_ERROR(Status)) {
        ALT_LOG(1, LOG_LINE_NORMAL, L"Error When Writing 'BOOT.CSV' File:- '%r'", Status);
    }
    #endif
} // VOID CreateFallbackCSV()

 static
 BOOLEAN CopyRefindPlusFiles (
     EFI_FILE_PROTOCOL *TargetDir
 ) {
    EFI_STATUS Status;
    EFI_STATUS Status2;

    if (!FileExists (TargetDir, L"\\EFI\\refindplus\\icons")) {
        // Treat absense as success
        Status = EFI_SUCCESS;
    }
    else {
        Status = BackupOldFile (TargetDir, L"\\EFI\\refindplus\\icons");

        #if REFIT_DEBUG > 0
        if (EFI_ERROR(Status)) {
            ALT_LOG(1, LOG_LINE_NORMAL, L"Error When Backing Icons Up");
        }
        #endif
    }

    if (!EFI_ERROR(Status)) {
        Status = CreateDirectories (TargetDir);

        #if REFIT_DEBUG > 0
        if (EFI_ERROR(Status)) {
            ALT_LOG(1, LOG_LINE_NORMAL, L"Error When Creating Target Directory");
        }
        #endif
    }
    if (!EFI_ERROR(Status)) {
        // Check status and log if it is an error; but do not pass on the
        // result code unless it is EFI_ABORTED, since it may not be a
        // critical error.
        Status2 = CopyFiles (TargetDir);
        if (EFI_ERROR(Status2)) {
            if (Status2 == EFI_ABORTED) {
               Status = EFI_ABORTED;
            }
            else {
               DisplaySimpleMessage (L"Warning", L"Error When Copying Some Files");
            }
        }
    }
    CreateFallbackCSV (TargetDir);

    return Status;
} // BOOLEAN CopyRefindPlusFiles()

/***********************
 *
 * Functions related to manipulation of EFI NVRAM boot variables.
 *
 ***********************/

// Find a Boot#### number that will boot the new RefindPlus installation. This
// function must be passed:
// - *Entry -- A new entry that is been constructed, but not yet stored in NVRAM
// - Size -- The size of the new entry
// - *AlreadyExists -- A BOOLEAN for returning whether the returned boot number
//   already exists (TRUE) vs. must be created (FALSE)
//
// The main return value can be EITHER of:
// - An existing entry that is identical to the newly-constructed one, in which
//   case *AlreadyExists is set to TRUE and the calling function should NOT
//   create a new entry; or
// - A number corresponding to an unused entry that the calling function can
//   use for a new entry, in which case *AlreadyExists is set to FALSE.
//
// BUG: Note that this function WILL MISS duplicate entries that occur after
// the first unused entry. For instance, if Boot0000 through Boot0004 exist
// but are not exact duplicates, and if Boot0005 is empty, and if Boot0006 is
// an exact duplicate, then this function will return 5, resulting in two
// identical entries (Boot0005 and Boot0006). This is done because scanning
// all possible entries (0 through 0xffff) takes a few seconds, and because
// a single duplicate is not a big deal. (If RefindPlus is re-installed via this
// feature again, this function will return "5," so no additional duplicate
// entry will be created. A third duplicate might be created if some other
// process were to delete an entry between Boot0000 and Boot0004, though.)
//
// Also, this function looks for EXACT duplicates. An entry might not be an
// exact duplicate but would still launch the same program. For instance, it
// might have a different description or have a different (but equivalent) EFI
// device path structure. In this case, the function will skip the equivalent-
// but-not-identical entry and the boot list will end up with two (or more)
// functionally equivalent entries.
static
UINTN FindBootNum (
    EFI_DEVICE_PATH_PROTOCOL *Entry,
    UINTN                     Size,
    BOOLEAN                  *AlreadyExists
) {
    EFI_STATUS      Status;
    UINTN           VarSize, i, j;
    CHAR16         *VarName;
    CHAR16         *Contents = NULL;

    *AlreadyExists = FALSE;
    Contents = NULL;
    i = 0;
    do {
        VarName = PoolPrint (L"Boot%04x", i++);
        Status = EfivarGetRaw (
            &GlobalGuid, VarName,
            (VOID **) &Contents, &VarSize
        );
        if (!EFI_ERROR(Status) &&
            VarSize == Size &&
            CompareMem (Contents, Entry, VarSize) == 0
        ) {
            *AlreadyExists = TRUE;
        }
        MY_FREE_POOL(VarName);
    } while (!EFI_ERROR(Status) && *AlreadyExists == FALSE);

    if (i > 0x10000) {
        // Somehow ALL boot entries are occupied! VERY unlikely!
        // In desperation, the program will overwrite the last one.
        i = 0x10000;
    }

    j = (i - 1);

    return j;
} // UINTN FindBootNum()

// Construct an NVRAM entry, but do NOT write it to NVRAM. The entry
// consists of:
// - A 32-bit options flag, which holds the LOAD_OPTION_ACTIVE value
// - A 16-bit number specifying the size of the device path
// - A label/description for the entry
// - The device path data in binary form
// - Any arguments to be passed to the program. This function does NOT
//   create arguments.
EFI_STATUS ConstructBootEntry (
    EFI_HANDLE  *TargetVolume,
    CHAR16      *Loader,
    CHAR16      *Label,
    CHAR8      **Entry,
    UINTN       *Size
) {
    EFI_STATUS                 Status;
    UINTN                      DevPathSize;
    CHAR8                     *Working;
    EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

    DevicePath  = FileDevicePath (TargetVolume, Loader);
    DevPathSize = DevicePathSize (DevicePath);
    *Size       = sizeof (UINT32) + sizeof (UINT16) + StrSize (Label) + DevPathSize + 2;
    *Entry      = Working = AllocateZeroPool (*Size);

    if (!DevicePath || !(*Entry)) {
        Status = EFI_OUT_OF_RESOURCES;
    }
    else {
        Status = EFI_SUCCESS;

        *(UINT32 *) Working = LOAD_OPTION_ACTIVE;
        Working += sizeof (UINT32);

        *(UINT16 *) Working = DevPathSize;
        Working += sizeof (UINT16);

        StrCpy ((CHAR16 *) Working, Label);
        Working += StrSize (Label);

        CopyMem (Working, DevicePath, DevPathSize);
        // If support for arguments is required in the future, uncomment
        // the lines below and adjust Size computation above appropriately.
        // Working += DevPathSize;
        // StrCpy ((CHAR16 *)Working, Arguments);
    }
    MY_FREE_POOL(DevicePath);

    return Status;
} // EFI_STATUS ConstructBootEntry()

// Set BootNum as first in the boot order. This function also eliminates any
// duplicates of BootNum in the boot order list (but NOT duplicates among
// the Boot#### variables).
static
EFI_STATUS SetBootDefault (
    UINTN BootNum
) {
    EFI_STATUS  Status;
    UINTN       i, j;
    UINTN       VarSize;
    UINTN       ListSize;
    UINT16     *BootOrder;
    UINT16     *NewBootOrder;
    BOOLEAN     IsAlreadyFirst;

    Status = EfivarGetRaw (
        &GlobalGuid, L"BootOrder",
        (VOID **) &BootOrder, &VarSize
    );
    if (!EFI_ERROR(Status)) {
        IsAlreadyFirst = FALSE;
        ListSize = VarSize / sizeof (UINT16);
        for (i = 0; i < ListSize; i++) {
            if (BootOrder[i] == BootNum) {
                if (i == 0) {
                    IsAlreadyFirst = TRUE;
                }
            }
        } // for

        if (!IsAlreadyFirst) {
            NewBootOrder = AllocateZeroPool ((ListSize + 1) * sizeof (UINT16));
            if (NewBootOrder) {
                NewBootOrder[0] = BootNum;

                j = 1;
                for (i = 0; i < ListSize; i++) {
                    if (BootOrder[i] != BootNum) {
                        NewBootOrder[j++] = BootOrder[i];
                    }
                } // for

                Status = EfivarSetRaw (
                   &GlobalGuid, L"BootOrder",
                   NewBootOrder, j * sizeof (UINT16), TRUE
                );

                MY_FREE_POOL(NewBootOrder);
            }
        }
        MY_FREE_POOL(BootOrder);
    }

    return Status;
} // EFI_STATUS SetBootDefault()

// Create an NVRAM entry for the newly-installed RefindPlus and make it the default.
// (If an entry that is identical to the one this function would create already
// exists, it may be used instead; see the comments before the FindBootNum()
// function for details and caveats.)
static
EFI_STATUS CreateNvramEntry (
    EFI_HANDLE DeviceHandle
) {
    EFI_STATUS                 Status;
    CHAR16                    *VarName, *ProgName;
    UINTN                      Size, BootNum;
    BOOLEAN                    AlreadyExists;
    EFI_DEVICE_PATH_PROTOCOL  *Entry;

    ProgName = PoolPrint (L"\\EFI\\refindplus\\%s", INST_REFINDPLUS_NAME);
    Status = ConstructBootEntry (
        DeviceHandle, ProgName,
        L"RefindPlus Boot Manager",
        (CHAR8**) &Entry, &Size
    );
    MY_FREE_POOL(ProgName);

    if (EFI_ERROR(Status)) {
        BootNum = 0;
        AlreadyExists = TRUE;
    }
    else {
        AlreadyExists = FALSE;
        BootNum = FindBootNum (Entry, Size, &AlreadyExists);
    }

    if (EFI_ERROR(Status) || AlreadyExists != FALSE) {
        VarName = NULL;
    }
    else {
        VarName = PoolPrint (L"Boot%04x", BootNum);
        Status  = EfivarSetRaw (
            &GlobalGuid, VarName,
            Entry, Size, TRUE
        );
        MY_FREE_POOL(VarName);
    }
    MY_FREE_POOL(Entry);

    if (!EFI_ERROR(Status)) {
        Status = SetBootDefault (BootNum);
    }

    return Status;
} // VOID CreateNvramEntry()

/***********************
 *
 * The main RefindPlus-installation function.
 *
 ***********************/

// Install RefindPlus to an ESP that the user specifies, create an NVRAM entry for
// that installation, and set it as the default boot option.
VOID InstallRefindPlus (VOID) {
    EFI_STATUS     Status;
    CHAR16        *MsgStr;
    ESP_LIST      *AllESPs;
    REFIT_VOLUME  *SelectedESP; // Do not free

    AllESPs     = FindAllESPs();
    SelectedESP = PickOneESP (AllESPs);

    if (!SelectedESP) {
        return;
    }

    MsgStr = L"Installing RefindPlus to an ESP";
    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    #endif

    Status = CopyRefindPlusFiles (SelectedESP->RootDir);
    if (!EFI_ERROR(Status)) {
        Status = CreateNvramEntry (SelectedESP->DeviceHandle);
    }

    if (EFI_ERROR(Status)) {
        MsgStr = L"Problems Encountered During Installation!!";
        DisplaySimpleMessage (
            L"Warning", MsgStr
        );

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("%s    ** %s", OffsetNext, MsgStr);
        LOG_MSG("\n\n");
        #endif

        return;
    }

    MsgStr = L"RefindPlus Successfully Installed";
    DisplaySimpleMessage (MsgStr, NULL);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("%s    * %s", OffsetNext, MsgStr);
    LOG_MSG("\n\n");
    #endif

    DeleteESPList (AllESPs);
} // VOID InstallRefindPlus()

/***********************
 *
 * Functions related to the management of the boot order.
 *
 ***********************/

// Create a list of Boot entries matching the BootOrder list.
BOOT_ENTRY_LIST * FindBootOrderEntries (VOID) {
    EFI_STATUS        Status;
    UINTN             i;
    UINTN             VarSize;
    UINTN             ListSize;
    UINT16           *BootOrder;
    CHAR16           *VarName;
    CHAR16           *Contents;
    BOOT_ENTRY_LIST  *L;
    BOOT_ENTRY_LIST  *ListEnd;
    BOOT_ENTRY_LIST  *ListStart;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Fetch Boot Order Variables:");
    #endif

    BootOrder = NULL;
    Status = EfivarGetRaw (
        &GlobalGuid, L"BootOrder",
        (VOID **) &BootOrder, &VarSize
    );
    if (EFI_ERROR(Status)) {
        return NULL;
    }

    Contents  = NULL;
    ListStart = ListEnd = NULL;
    ListSize  = VarSize / sizeof (UINT16);

    for (i = 0; i < ListSize; i++) {
        VarName = PoolPrint (L"Boot%04x", BootOrder[i]);
        if (!VarName) {
            break;
        }

        Status  = EfivarGetRaw (
            &GlobalGuid, VarName,
            (VOID **) &Contents, &VarSize
        );

        if (!EFI_ERROR(Status)) {
            L = AllocateZeroPool (sizeof (BOOT_ENTRY_LIST));
            if (L) {
               L->BootEntry.BootNum = BootOrder[i];
               L->BootEntry.Options = (UINT32) Contents[0];
               L->BootEntry.Size    = (UINT16) Contents[2];
               L->BootEntry.Label   = StrDuplicate ((CHAR16*) &(Contents[3]));
               L->BootEntry.DevPath = AllocatePool (L->BootEntry.Size);
               CopyMem (
                   L->BootEntry.DevPath,
                   (EFI_DEVICE_PATH*) &Contents[3 + StrSize (L->BootEntry.Label)/2],
                   L->BootEntry.Size
               );
               L->NextBootEntry = NULL;

               if (ListStart == NULL) {
                   ListStart = L;
               }
               else {
                   ListEnd->NextBootEntry = L;
               }
               ListEnd = L;
            }
        }

        MY_FREE_POOL(VarName);
        MY_FREE_POOL(Contents);
    } // for

    MY_FREE_POOL(BootOrder);

    return ListStart;
} // BOOT_ENTRY_LIST * FindBootOrderEntries()

// Delete a linked-list BOOT_ENTRY_LIST data structure
VOID DeleteBootOrderEntries (
    BOOT_ENTRY_LIST *Entries
) {
    BOOT_ENTRY_LIST *Current;

    while (Entries != NULL) {
        Current = Entries;
        MY_FREE_POOL(Current->BootEntry.Label);
        MY_FREE_POOL(Current->BootEntry.DevPath);
        Entries = Entries->NextBootEntry;
        MY_FREE_POOL(Current);
    } // while
} // VOID DeleteBootOrderEntries()

static
UINTN ConfirmBootOptionOperation (
    UINTN   Operation,
    CHAR16 *BootOptionString
) {
    CHAR16            *CheckString;
    BOOLEAN            RetVal;
    REFIT_MENU_SCREEN *ConfirmBootOptionMenu;

    INTN               DefaultEntry;
    UINTN              MenuExit;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenOption;

    if (Operation != EFI_BOOT_OPTION_DELETE &&
        Operation != EFI_BOOT_OPTION_MAKE_DEFAULT
    ) {
        // Early Return
        return EFI_BOOT_OPTION_DO_NOTHING;
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Creating 'Confirm Boot Option Operation' Screen");
    #endif

    ConfirmBootOptionMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (!ConfirmBootOptionMenu) {
        // Early Return ... Fail
        return EFI_BOOT_OPTION_DO_NOTHING;
    }

    ConfirmBootOptionMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_BOOTORDER      );
    ConfirmBootOptionMenu->Title      = StrDuplicate (L"Confirm Boot Option Operation");
    ConfirmBootOptionMenu->Hint1      = StrDuplicate (SELECT_OPTION_HINT              );
    ConfirmBootOptionMenu->Hint2      = StrDuplicate (RETURN_MAIN_SCREEN_HINT         );

    AddMenuInfoLine (ConfirmBootOptionMenu, PoolPrint (L"%s", BootOptionString), TRUE);

    CheckString = NULL;
    if (Operation == EFI_BOOT_OPTION_MAKE_DEFAULT) {
        CheckString = L"Set This Boot Option as Default?";
    }
    else if (Operation == EFI_BOOT_OPTION_DELETE) {
        CheckString = L"Delete This Boot Option?";
    }
    AddMenuInfoLine (ConfirmBootOptionMenu, CheckString, FALSE);

    RetVal = GetYesNoMenuEntry (&ConfirmBootOptionMenu);
    if (!RetVal) {
        FreeMenuScreen (&ConfirmBootOptionMenu);

        // Early Return
        return EFI_BOOT_OPTION_DO_NOTHING;
    }

    DefaultEntry = 1;
    Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
    MenuExit = RunGenericMenu (ConfirmBootOptionMenu, Style, &DefaultEntry, &ChosenOption);

    #if REFIT_DEBUG > 0
    ALT_LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) From RunGenericMenu Call on '%s' in 'ConfirmBootOptionOperation'",
        MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
    );
    #endif

    if (MenuExit != MENU_EXIT_ENTER || ChosenOption->Tag != TAG_YES) {
        Operation = EFI_BOOT_OPTION_DO_NOTHING;
    }

    FreeMenuScreen (&ConfirmBootOptionMenu);

    return Operation;
} // UINTN ConfirmBootOptionOperation()


// Enable the user to pick one boot option to move to the top of the boot
// order list (via Enter) or delete (via Delete or '-'). This function does
// not actually call those options, though; that is left to the calling
// function.
// Returns the operation (EFI_BOOT_OPTION_MAKE_DEFAULT, EFI_BOOT_OPTION_DELETE,
// or EFI_BOOT_OPTION_DO_NOTHING).
// Input variables:
//  - *Entries: Linked-list set of boot entries. Unmodified.
//  - *BootOrderNum: Returns the Boot#### number to be promoted or deleted.
static
UINTN PickOneBootOption (
    IN     BOOT_ENTRY_LIST *Entries,
    IN OUT UINTN           *BootOrderNum
) {
    UINTN                Operation;
    CHAR16              *Filename;
    REFIT_VOLUME        *Volume;
    REFIT_MENU_ENTRY    *MenuEntryItem;
    REFIT_MENU_SCREEN   *PickBootOptionMenu;

    INTN               DefaultEntry;
    UINTN              MenuExit;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenOption;

    if (!Entries) {
        DisplaySimpleMessage (L"Firmware BootOrder List is Not Available", NULL);

        // Early Return
        return EFI_BOOT_OPTION_DO_NOTHING;
    }

    PickBootOptionMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (PickBootOptionMenu == NULL) {
        // Early Return
        return EFI_BOOT_OPTION_DO_NOTHING;
    }

    PickBootOptionMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_BOOTORDER);
    PickBootOptionMenu->Title      = StrDuplicate (L"Manage Firmware Boot Order");
    PickBootOptionMenu->Hint1      = StrDuplicate (
        L"Select an option and press 'Enter' to make it the default. Press '-' or"
    );
    PickBootOptionMenu->Hint2      = PoolPrint (
        L"'Delete' to delete it, or %s", RETURN_MAIN_SCREEN_HINT
    );

    AddMenuInfoLine (
        PickBootOptionMenu,
        L"Promote or Remove Firmware BootOrder Variables",
        FALSE
    );

    Volume = NULL;
    Filename = NULL;
    do {
        MenuEntryItem = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
        FindVolumeAndFilename (Entries->BootEntry.DevPath, &Volume, &Filename);
        if ((Filename != NULL) && (StrLen (Filename) > 0)) {
            if ((Volume != NULL) && (Volume->VolName != NULL)) {
                MenuEntryItem->Title = PoolPrint (
                    L"Boot%04x - %s - %s on %s",
                    Entries->BootEntry.BootNum,
                    Entries->BootEntry.Label,
                    Filename, Volume->VolName);
            }
            else {
                MenuEntryItem->Title = PoolPrint (
                    L"Boot%04x - %s - %s",
                    Entries->BootEntry.BootNum,
                    Entries->BootEntry.Label,
                    Filename
                );
            }
        }
        else {
            MenuEntryItem->Title = PoolPrint (
                L"Boot%04x - %s",
                Entries->BootEntry.BootNum,
                Entries->BootEntry.Label
            );
        }

        // NB: Using the 'Row' field to hold the 'Boot####' value
        MenuEntryItem->Row = Entries->BootEntry.BootNum;
        AddMenuEntry (PickBootOptionMenu, MenuEntryItem);

        // DA-TAG: Dereference 'Volume' ... Do not free
        MY_SOFT_FREE(Volume);
        MY_FREE_POOL(Filename);

        Entries = Entries->NextBootEntry;
    } while (Entries != NULL);

    do {
        Operation = EFI_BOOT_OPTION_DO_NOTHING;
        if (!GetReturnMenuEntry (&PickBootOptionMenu)) {
            break;
        }

        DefaultEntry = 9999; // Use the Max Index
        Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
        MenuExit = RunGenericMenu (PickBootOptionMenu, Style, &DefaultEntry, &ChosenOption);

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Returned '%d' (%s) From RunGenericMenu Call on '%s' in 'PickOneBootOption'",
            MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
        );
        #endif

        if (ChosenOption->Tag == TAG_RETURN) {
            break;
        }

        if (MenuExit == MENU_EXIT_ENTER) {
            Operation = EFI_BOOT_OPTION_MAKE_DEFAULT;
            *BootOrderNum = ChosenOption->Row;
        }
        else if (MenuExit == MENU_EXIT_HIDE) {
            Operation = EFI_BOOT_OPTION_DELETE;
            *BootOrderNum = ChosenOption->Row;
        }

        Operation = ConfirmBootOptionOperation (Operation, ChosenOption->Title);
    } while (0); // This 'loop' only runs once

    FreeMenuScreen (&PickBootOptionMenu);

    return Operation;
} // REFIT_VOLUME *PickOneBootOption()

static
EFI_STATUS DeleteInvalidBootEntries (VOID) {
    EFI_STATUS  Status;
    UINTN       i, j;
    UINTN       VarSize;
    UINTN       ListSize;
    CHAR8      *Contents;
    UINT16     *BootOrder;
    UINT16     *NewBootOrder;
    CHAR16     *VarName;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Deleting Invalid Boot Entries From Internal BootOrder List");
    #endif

    Status = EfivarGetRaw (
        &GlobalGuid, L"BootOrder",
        (VOID **) &BootOrder, &VarSize
    );
    if (!EFI_ERROR(Status)) {
        ListSize = VarSize / sizeof (UINT16);
        NewBootOrder = AllocateZeroPool (VarSize);

        j = 0;
        for (i = 0; i < ListSize; i++) {
            VarName = PoolPrint (L"Boot%04x", BootOrder[i]);
            Status  = EfivarGetRaw (
                &GlobalGuid, VarName,
                (VOID **) &Contents, &VarSize
            );
            if (!EFI_ERROR(Status)) {
                NewBootOrder[j++] = BootOrder[i];
                MY_FREE_POOL(Contents);
            }
            MY_FREE_POOL(VarName);
        } // for

        Status = EfivarSetRaw (
            &GlobalGuid, L"BootOrder",
            NewBootOrder, j * sizeof (UINT16), TRUE
        );

        MY_FREE_POOL(NewBootOrder);
        MY_FREE_POOL(BootOrder);
    }

    return Status;
} // EFI_STATUS DeleteInvalidBootEntries()

VOID ManageBootorder (VOID) {
    UINTN            BootNum;
    UINTN            Operation;
    CHAR16          *Name;
    CHAR16          *Message;
    BOOT_ENTRY_LIST *Entries;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Creating 'Manage Boot Order' Screen");
    #endif

    BootNum   = 0;
    Entries   = FindBootOrderEntries();
    Operation = PickOneBootOption (Entries, &BootNum);

    if (Operation == EFI_BOOT_OPTION_DELETE) {
        Name = PoolPrint (L"Boot%04x", BootNum);
        EfivarSetRaw (
            &GlobalGuid, Name,
            NULL, 0, TRUE
        );
        DeleteInvalidBootEntries();
        Message = PoolPrint (L"Boot%04x has been Deleted.", BootNum);
        DisplaySimpleMessage (Message, NULL);
        MY_FREE_POOL(Name);
        MY_FREE_POOL(Message);
    }

    if (Operation == EFI_BOOT_OPTION_MAKE_DEFAULT) {
        SetBootDefault (BootNum);
        Message = PoolPrint (L"Boot%04x is Now the Default EFI Boot Option.", BootNum);
        DisplaySimpleMessage (Message, NULL);
        MY_FREE_POOL(Message);
    }

    DeleteBootOrderEntries (Entries);
} // VOID ManageBootorder()
