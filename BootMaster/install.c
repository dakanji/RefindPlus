/*
 * BootMaster/install.c
 * Functions related to installation of RefindPlus and management of EFI boot order
 *
 * Copyright (c) 2020-2021 by Roderick W. Smith
 *
 * Distributed under the terms of the GNU General Public License (GPL)
 * version 3 (GPLv3), a copy of which must be distributed with this source
 * code or binaries made from it.
 *
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
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


extern REFIT_MENU_ENTRY  MenuEntryYes;
extern REFIT_MENU_ENTRY  MenuEntryNo;


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
    LOG(3, LOG_LINE_NORMAL, L"Deleting List of ESPs");
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
    ESP_LIST *AllESPs = NULL;
    ESP_LIST *NewESP;
    UINTN     VolumeIndex;
    EFI_GUID  ESPGuid = ESP_GUID_VALUE;

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL, L"Searching for ESPs");
    #endif

    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_INTERNAL &&
            GuidsAreEqual (&(Volumes[VolumeIndex]->PartTypeGuid), &ESPGuid) &&
            (Volumes[VolumeIndex]->FSType == FS_TYPE_FAT) &&
            !GuidsAreEqual (&(Volumes[VolumeIndex]->PartGuid), &SelfVolume->PartGuid)) {
            NewESP = AllocateZeroPool (sizeof (ESP_LIST));

            if (NewESP != NULL) {
                NewESP->Volume = Volumes[VolumeIndex];
                NewESP->NextESP = AllESPs;
                AllESPs = NewESP;
            }
        }
    } // for

    return AllESPs;
} // ESP_LIST * FindAllESPs()

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
    ESP_LIST            *CurrentESP;
    REFIT_VOLUME        *ChosenVolume  = NULL;
    CHAR16              *Temp          = NULL, *GuidStr, *PartName, *FsName, *VolName;
    INTN                 DefaultEntry  = 0, MenuExit, i = 1;
    MENU_STYLE_FUNC      Style         = TextMenuStyle;
    REFIT_MENU_ENTRY    *ChosenOption, *MenuEntryItem = NULL;

    REFIT_MENU_ENTRY    *TempMenuEntry = CopyMenuEntry (&MenuEntryReturn);
    TempMenuEntry->Image               = BuiltinIcon (BUILTIN_ICON_FUNC_INSTALL);
    CHAR16 *MenuInfo = L"Select a partition and press 'Enter' to install RefindPlus";
    REFIT_MENU_SCREEN   InstallMenu = { L"Install RefindPlus", NULL, 0, &MenuInfo, 0, &TempMenuEntry, 0, NULL,
                                        L"Select a destination and press 'Enter' or",
                                        L"press 'Esc' to return to the main menu without changes" };

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL, L"Prompting user to select an ESP for installation");
    #endif

    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    if (AllESPs) {
        CurrentESP = AllESPs;
        AddMenuInfoLine (&InstallMenu, MenuInfo);
        while (CurrentESP != NULL) {
            MenuEntryItem = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
            GuidStr       = GuidAsString (&(CurrentESP->Volume->PartGuid));
            PartName      = CurrentESP->Volume->PartName;
            FsName        = CurrentESP->Volume->FsName;
            VolName       = CurrentESP->Volume->VolName;
            if (PartName && (StrLen (PartName) > 0) &&
                FsName && (StrLen (FsName) > 0) &&
                !MyStriCmp (FsName, PartName)
            ) {
                Temp = PoolPrint (L"%s - '%s', AKA '%s'", GuidStr, PartName, FsName);
            }
            else if (FsName && (StrLen (FsName) > 0)) {
                Temp = PoolPrint (L"%s - '%s'", GuidStr, FsName);
            }
            else if (PartName && (StrLen (PartName) > 0)) {
                Temp = PoolPrint (L"%s - '%s'", GuidStr, PartName);
            }
            else if (VolName && (StrLen (VolName) > 0)) {
                Temp = PoolPrint (L"%s - '%s'", GuidStr, VolName);
            }
            else {
                Temp = PoolPrint (L"%s - No Name", GuidStr);
            }

            #if REFIT_DEBUG > 0
            LOG(3, LOG_LINE_NORMAL, L"Adding '%s' to UI List of ESPs");
            #endif

            MY_FREE_POOL(GuidStr);
            MenuEntryItem->Title = Temp;
            MenuEntryItem->Tag = TAG_RETURN;
            MenuEntryItem->Row = i++;
            AddMenuEntry (&InstallMenu, MenuEntryItem);
            CurrentESP = CurrentESP->NextESP;
        } // while
        MenuExit = RunGenericMenu (&InstallMenu, Style, &DefaultEntry, &ChosenOption);

        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
            L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'PickOneESP'",
            MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
        );
        #endif


        if (MenuExit == MENU_EXIT_ENTER) {
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
    }
    else {
        DisplaySimpleMessage (L"Information", L"No Eligible ESPs Found");

        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL, L"No ESPs found");
        #endif
    }
    MY_FREE_POOL(TempMenuEntry);

    return ChosenVolume;
} // REFIT_VOLUME *PickOneESP()

/***********************
 *
 * Functions related to manipulation files on disk.
 *
 ***********************/

static
EFI_STATUS RenameFile (
    IN EFI_FILE *BaseDir,
    CHAR16      *OldName,
    CHAR16      *NewName
) {
    EFI_STATUS     Status;
    EFI_FILE      *FilePtr;
    EFI_FILE_INFO *NewInfo, *Buffer = NULL;
    UINTN          NewInfoSize;

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL, L"Trying to Rename '%s' to '%s'", OldName, NewName);
    #endif

    Status = REFIT_CALL_5_WRAPPER(BaseDir->Open, BaseDir, &FilePtr, OldName,
                                  EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    if (Status == EFI_SUCCESS) {
        Buffer = LibFileInfo (FilePtr);
        if (Buffer == NULL) {
            REFIT_CALL_1_WRAPPER(FilePtr->Close, FilePtr);
            return FALSE;
        }
    }

    if (Status == EFI_SUCCESS) {
        NewInfoSize = sizeof (EFI_FILE_INFO) + StrSize (NewName);
        NewInfo = (EFI_FILE_INFO *) AllocateZeroPool (NewInfoSize);

        if (NewInfo != NULL) {
            CopyMem (NewInfo, Buffer, sizeof (EFI_FILE_INFO));
            NewInfo->FileName[0] = 0;
            StrCat (NewInfo->FileName, NewName);
            Status = REFIT_CALL_4_WRAPPER(
                BaseDir->SetInfo,
                FilePtr,
                &gEfiFileInfoGuid,
                NewInfoSize,
                (VOID *) NewInfo
            );

            MY_FREE_POOL(NewInfo);
            MY_FREE_POOL(FilePtr);
            MY_FREE_POOL(Buffer);
        }
        else {
            Status = EFI_BUFFER_TOO_SMALL;
        }
    }
    REFIT_CALL_1_WRAPPER(FilePtr->Close, FilePtr);

    return Status;
} // EFI_STATUS RenameFile()

// Rename *FileName to add a "-old" extension, but only if that file does not
// already exist. Called on the icons directory to preserve it in case the
// user wants icons stored there that have been supplanted by new icons.
static
EFI_STATUS BackupOldFile (
    IN EFI_FILE *BaseDir,
    CHAR16      *FileName
) {
    EFI_STATUS          Status = EFI_SUCCESS;
    CHAR16              *NewName;

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL, L"Backing '%s' Up", FileName);
    #endif

    if ((BaseDir == NULL) || (FileName == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    NewName = PoolPrint (L"%s-old", FileName);
    if (!FileExists (BaseDir, NewName)) {
        Status = RenameFile (BaseDir, FileName, NewName);
    }
    MY_FREE_POOL(NewName);

    return (Status);
} // EFI_STATUS BackupOldFile()

// Create directories in which RefindPlus will reside.
static
EFI_STATUS CreateDirectories (
    IN EFI_FILE *BaseDir
) {
    EFI_STATUS  Status   = EFI_SUCCESS;
    EFI_FILE   *TheDir   = NULL;
    CHAR16     *FileName = NULL;
    UINTN       i        = 0;

    while (Status == EFI_SUCCESS &&
        (FileName = FindCommaDelimited (INST_DIRECTORIES, i++)) != NULL
    ) {
        REFIT_CALL_5_WRAPPER(
            BaseDir->Open,
            BaseDir,
            &TheDir,
            FileName,
            EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
            EFI_FILE_DIRECTORY
        );

        Status = REFIT_CALL_1_WRAPPER(TheDir->Close, TheDir);

        MY_FREE_POOL(FileName);
        MY_FREE_POOL(TheDir);
    } // while

    return (Status);
} // CreateDirectories()

static
EFI_STATUS CopyOneFile (
    IN EFI_FILE *SourceDir,
    IN CHAR16   *SourceName,
    IN EFI_FILE *DestDir,
    IN CHAR16   *DestName
) {
    EFI_FILE           *SourceFile = NULL, *DestFile = NULL;
    UINTN              FileSize = 0, Status;
    EFI_FILE_INFO      *FileInfo = NULL;
    UINTN              *Buffer = NULL;

    // Read the original file.
    Status = REFIT_CALL_5_WRAPPER(
        SourceDir->Open, SourceDir,
        &SourceFile, SourceName,
        EFI_FILE_MODE_READ, 0
    );

    if (Status == EFI_SUCCESS) {
        FileInfo = LibFileInfo (SourceFile);
        if (FileInfo == NULL) {
            REFIT_CALL_1_WRAPPER(SourceFile->Close, SourceFile);
            return EFI_NO_RESPONSE;
        }
        FileSize = FileInfo->FileSize;
        MY_FREE_POOL(FileInfo);
    }

    if (Status == EFI_SUCCESS) {
        Buffer = AllocateZeroPool (FileSize);
        if (Buffer == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
        }
    }
    if (Status == EFI_SUCCESS) {
        Status = REFIT_CALL_3_WRAPPER(
            SourceFile->Read, SourceFile,
            &FileSize, Buffer
        );
    }

    if (Status == EFI_SUCCESS) {
        REFIT_CALL_1_WRAPPER(SourceFile->Close, SourceFile);
    }

    // Write the file to a new location.
    if (Status == EFI_SUCCESS) {
        Status = REFIT_CALL_5_WRAPPER(
            DestDir->Open, DestDir,
            &DestFile, DestName,
            EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0
        );
    }

    if (Status == EFI_SUCCESS) {
        Status = REFIT_CALL_3_WRAPPER(
            DestFile->Write, DestFile,
            &FileSize, Buffer
        );
    }
    if (Status == EFI_SUCCESS) {
        Status = REFIT_CALL_1_WRAPPER(DestFile->Close, DestFile);
    }

    MY_FREE_POOL(SourceFile);
    MY_FREE_POOL(DestFile);
    MY_FREE_POOL(Buffer);

    #if REFIT_DEBUG > 0
    if (EFI_ERROR(Status)) {
        LOG(3, LOG_LINE_NORMAL,
            L"Error:- '%r' When Copying '%s' to '%s'",
            Status, SourceName, DestName
        );
    }
    #endif

    return (Status);
} // EFI_STATUS CopyOneFile()

// Copy a single directory (non-recursively)
static
EFI_STATUS CopyDirectory (
    IN EFI_FILE *SourceDirPtr,
    IN CHAR16   *SourceDirName,
    IN EFI_FILE *DestDirPtr,
    IN CHAR16   *DestDirName
) {
    REFIT_DIR_ITER  DirIter;
    EFI_FILE_INFO   *DirEntry;
    CHAR16          *DestFileName = NULL, *SourceFileName = NULL;
    EFI_STATUS      Status = EFI_SUCCESS;

    DirIterOpen (SourceDirPtr, SourceDirName, &DirIter);
    while (Status == EFI_SUCCESS && DirIterNext (&DirIter, 2, NULL, &DirEntry)) {
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

    return (Status);
} // EFI_STATUS CopyDirectory()

// Copy Linux drivers for detected filesystems, but not for undetected filesystems.
// Note: Does NOT copy HFS+ driver on Apple hardware even if HFS+ is detected;
// but it DOES copy the HFS+ driver on non-Apple hardware if HFS+ is detected,
// even though HFS+ is not technically a Linux filesystem, since HFS+ CAN be used
// as a Linux /boot partition. That is weird, but it does work.
static
EFI_STATUS CopyDrivers (
    IN EFI_FILE *SourceDirPtr,
    IN CHAR16   *SourceDirName,
    IN EFI_FILE *DestDirPtr,
    IN CHAR16   *DestDirName
) {
    CHAR16         *DestFileName   = NULL;
    CHAR16         *SourceFileName = NULL;
    CHAR16         *DriverName     = NULL; // Note: Assign to string constants ... do not free.
    EFI_STATUS      Status         = EFI_SUCCESS;
    EFI_STATUS      WorstStatus    = EFI_SUCCESS;
    BOOLEAN         DriverCopied[NUM_FS_TYPES];
    UINTN           i;

    for (i = 0; i < NUM_FS_TYPES; i++) {
        DriverCopied[i] = FALSE;
    }

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL,
        L"Scanning %d Volumes for Identifiable Filesystems",
        VolumesCount
    );
    #endif

    for (i = 0; i < VolumesCount; i++) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
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
                    (!MyStriCmp (L"Apple", ST->FirmwareVendor))
                ) {
                    DriverName = L"hfs";
                    DriverCopied[FS_TYPE_HFSPLUS] = TRUE;
                }

                break;
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
            LOG(3, LOG_LINE_NORMAL, L"Trying to Copy Driver for %s", DriverName);
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

    return (WorstStatus);
} // EFI_STATUS CopyDrivers()

// Copy all the files from the source to *TargetDir
static
EFI_STATUS CopyFiles (
    IN EFI_FILE *TargetDir
) {
    REFIT_VOLUME    *SourceVolume = NULL; // Do not free
    CHAR16          *SourceFile   = NULL, *SourceDir, *ConfFile;
    CHAR16          *SourceDriversDir, *TargetDriversDir, *RefindPlusName;
    EFI_STATUS       Status;
    EFI_STATUS       WorstStatus = EFI_SUCCESS;

    FindVolumeAndFilename (
        GlobalConfig.SelfDevicePath,
        &SourceVolume, &SourceFile
    );
    SourceDir = FindPath (SourceFile);

    // Begin by copying RefindPlus itself.
    RefindPlusName = PoolPrint (L"EFI\\refindplus\\%s", INST_REFINDPLUS_NAME);
    Status         = CopyOneFile (
        SourceVolume->RootDir, SourceFile,
        TargetDir, RefindPlusName
    );

    MY_FREE_POOL(SourceFile);
    MY_FREE_POOL(RefindPlusName);
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
            L"Error When Copying RefindPlus Binary ... Installation has Failed!!"
        );
        #endif

        Status = WorstStatus = EFI_ABORTED;
    }

    if (Status == EFI_SUCCESS) {
        // Now copy the config file -- but:
        //  - Copy config.conf-sample, not config.conf, if it is available, to
        //    avoid picking up live-disk-specific customizations.
        //  - Do not overwrite an existing config.conf at the target; instead,
        //    copy to config.conf-sample if config.conf is present.
        ConfFile = PoolPrint (L"%s\\config.conf-sample", SourceDir);
        if (FileExists (SourceVolume->RootDir, ConfFile)) {
            StrCpy (SourceFile, ConfFile);
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
            LOG(3, LOG_LINE_NORMAL, L"Error When Copying Drivers:- '%d'", Status);
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
            WorstStatus = Status;
        }

        MY_FREE_POOL(SourceDriversDir);
        MY_FREE_POOL(TargetDriversDir);
    }
    MY_FREE_POOL(SourceDir);

    return (WorstStatus);
} // EFI_STATUS CopyFiles()

// Create the BOOT.CSV file used by the fallback.efi/fbx86.efi program.
// Success is not critical, so we do not return a Status value.
static
VOID CreateFallbackCSV (
    IN EFI_FILE *TargetDir
) {
    CHAR16   *Contents = NULL;
    UINTN     FileSize, Status;
    EFI_FILE *FilePtr;

    Status = REFIT_CALL_5_WRAPPER(
        TargetDir->Open, TargetDir,
        &FilePtr, L"\\EFI\\refindplus\\BOOT.CSV",
        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0
    );

    if (Status == EFI_SUCCESS) {
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

            if (Status == EFI_SUCCESS) {
                REFIT_CALL_1_WRAPPER(FilePtr->Close, FilePtr);
            }
            MY_FREE_POOL(FilePtr);
        }

        MY_FREE_POOL(Contents);
    }

    #if REFIT_DEBUG > 0
    if (EFI_ERROR(Status)) {
        LOG(3, LOG_LINE_NORMAL, L"Error When Writing 'BOOT.CSV' File:- '%r'", Status);
    }
    #endif
} // VOID CreateFallbackCSV()

 static
 BOOLEAN CopyRefindPlusFiles (
     IN EFI_FILE *TargetDir
 ) {
    EFI_STATUS Status = EFI_SUCCESS, Status2;

    if (FileExists (TargetDir, L"\\EFI\\refindplus\\icons")) {
        Status = BackupOldFile (TargetDir, L"\\EFI\\refindplus\\icons");

        #if REFIT_DEBUG > 0
        if (EFI_ERROR(Status)) {
            LOG(3, LOG_LINE_NORMAL, L"Error When Backing Icons Up");
        }
        #endif
    }
    if (Status == EFI_SUCCESS) {
        Status = CreateDirectories (TargetDir);

        #if REFIT_DEBUG > 0
        if (EFI_ERROR(Status)) {
            LOG(3, LOG_LINE_NORMAL, L"Error When Creating Target Directory");
        }
        #endif
    }
    if (Status == EFI_SUCCESS) {
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
    EFI_DEVICE_PATH *Entry,
    UINTN            Size,
    BOOLEAN         *AlreadyExists
) {
    UINTN   i = 0, VarSize, Status;
    CHAR16  *VarName, *Contents = NULL;

    *AlreadyExists = FALSE;
    do {
        VarName = PoolPrint (L"Boot%04x", i++);
        Status = EfivarGetRaw (
            &GlobalGuid, VarName,
            (VOID **) &Contents, &VarSize
        );
        if ((Status == EFI_SUCCESS) &&
            (VarSize == Size) &&
            (CompareMem (Contents, Entry, VarSize) == 0)
        ) {
            *AlreadyExists = TRUE;
        }
        MY_FREE_POOL(VarName);
    } while ((Status == EFI_SUCCESS) && (*AlreadyExists == FALSE));

    if (i > 0x10000) {
        // Somehow ALL boot entries are occupied! VERY unlikely!
        // In desperation, the program will overwrite the last one.
        i = 0x10000;
    }

    return (i - 1);
} // UINTN FindBootNum()

// Construct an NVRAM entry, but do NOT write it to NVRAM. The entry
// consists of:
// - A 32-bit options flag, which holds the LOAD_OPTION_ACTIVE value
// - A 16-bit number specifying the size of the device path
// - A label/description for the entry
// - The device path data in binary form
// - Any arguments to be passed to the program. This function does NOT
//   create arguments.
static
EFI_STATUS ConstructBootEntry (
    EFI_HANDLE  *TargetVolume,
    CHAR16      *Loader,
    CHAR16      *Label,
    CHAR8      **Entry,
    UINTN       *Size
) {
    EFI_DEVICE_PATH *DevicePath;
     UINTN           Status = EFI_SUCCESS, DevPathSize;
     CHAR8           *Working;

    DevicePath  = FileDevicePath (TargetVolume, Loader);
    DevPathSize = DevicePathSize (DevicePath);
    *Size       = sizeof (UINT32) + sizeof (UINT16) + StrSize (Label) + DevPathSize + 2;
    *Entry      = Working = AllocateZeroPool (*Size);

    if (DevicePath && *Entry) {
        *(UINT32 *) Working = LOAD_OPTION_ACTIVE;
        Working += sizeof (UINT32);

        *(UINT16 *) Working = DevPathSize;
        Working += sizeof (UINT16);

        StrCpy ((CHAR16 *) Working, Label);
        Working += StrSize (Label);

        CopyMem (Working, DevicePath, DevPathSize);
        // If support for arguments is required in the future, uncomment
        // the below two lines and adjust Size computation above appropriately.
        // Working += DevPathSize;
        // StrCpy ((CHAR16 *)Working, Arguments);
    }
    else {
        Status = EFI_OUT_OF_RESOURCES;
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
    UINTN    Status, VarSize, ListSize, i, j;
    UINT16   *BootOrder, *NewBootOrder;
    BOOLEAN  IsAlreadyFirst = FALSE;

    Status = EfivarGetRaw (
        &GlobalGuid, L"BootOrder",
        (VOID **) &BootOrder, &VarSize
    );
    if (Status == EFI_SUCCESS) {
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
                   &GlobalGuid,
                   L"BootOrder",
                   NewBootOrder,
                   j * sizeof (UINT16),
                   TRUE
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
    CHAR16           *VarName = NULL, *ProgName;
    UINTN            Status, Size, BootNum = 0;
    EFI_DEVICE_PATH  *Entry;
    BOOLEAN          AlreadyExists = FALSE;

    ProgName = PoolPrint (L"\\EFI\\refindplus\\%s", INST_REFINDPLUS_NAME);
    Status = ConstructBootEntry (
        DeviceHandle, ProgName,
        L"RefindPlus Boot Manager",
        (CHAR8**) &Entry, &Size
    );
    MY_FREE_POOL(ProgName);

    if (Status == EFI_SUCCESS) {
        BootNum = FindBootNum (Entry, Size, &AlreadyExists);
    }

    if ((Status == EFI_SUCCESS) && (AlreadyExists == FALSE)) {
        VarName = PoolPrint (L"Boot%04x", BootNum);
        Status  = EfivarSetRaw (&GlobalGuid, VarName, Entry, Size, TRUE);
        MY_FREE_POOL(VarName);
    }
    MY_FREE_POOL(Entry);

    if (Status == EFI_SUCCESS) {
        Status = SetBootDefault (BootNum);
    }

    return (Status);
} // VOID CreateNvramEntry()

/***********************
 *
 * The main RefindPlus-installation function.
 *
 ***********************/

// Install RefindPlus to an ESP that the user specifies, create an NVRAM entry for
// that installation, and set it as the default boot option.
VOID InstallRefindPlus (VOID) {
    ESP_LIST      *AllESPs;
    REFIT_VOLUME  *SelectedESP; // Do not free
    UINTN         Status;

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL, L"Installing RefindPlus to an ESP");
    #endif

    AllESPs     = FindAllESPs();
    SelectedESP = PickOneESP (AllESPs);

    if (SelectedESP) {
        Status = CopyRefindPlusFiles (SelectedESP->RootDir);

        if (Status == EFI_SUCCESS) {
            Status = CreateNvramEntry (SelectedESP->DeviceHandle);
        }

        if (Status == EFI_SUCCESS) {
            DisplaySimpleMessage (
                L"Information", L"RefindPlus Successfully Installed"
            );
        }
        else {
            DisplaySimpleMessage (
                L"Warning", L"Problems Encountered During Installation!!"
            );
        }
    }

    DeleteESPList (AllESPs);
} // VOID InstallRefindPlus()

/***********************
 *
 * Functions related to the management of the boot order.
 *
 ***********************/

// Create a list of Boot entries matching the BootOrder list.
BOOT_ENTRY_LIST * FindBootOrderEntries (VOID) {
    UINTN             Status = EFI_SUCCESS, i;
    UINT16           *BootOrder = NULL;
    UINTN             VarSize, ListSize;
    CHAR16           *VarName = NULL;
    CHAR16           *Contents = NULL;
    BOOT_ENTRY_LIST  *L, *ListStart = NULL, *ListEnd = NULL; // return value; do not free

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL, L"Locating Boot Order Entries");
    #endif

    Status = EfivarGetRaw (
        &GlobalGuid, L"BootOrder", (VOID **)
        &BootOrder, &VarSize
    );

    if (Status != EFI_SUCCESS) {
        return NULL;
    }

    ListSize = VarSize / sizeof (UINT16);
    for (i = 0; i < ListSize; i++) {
        VarName = PoolPrint (L"Boot%04x", BootOrder[i]);
        Status  = EfivarGetRaw (
            &GlobalGuid, VarName,
            (VOID **) &Contents, &VarSize
        );

        if (Status == EFI_SUCCESS) {
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
    INTN               DefaultEntry = 1;
    UINTN              MenuExit;
    CHAR16            *CheckString = NULL;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenOption;

    if (Operation == EFI_BOOT_OPTION_MAKE_DEFAULT || Operation == EFI_BOOT_OPTION_DELETE) {
        #if REFIT_DEBUG > 0
        LOG(1, LOG_LINE_THIN_SEP, L"Showing Confirm Boot Option Operation Screen");
        #endif

        REFIT_MENU_SCREEN  ConfirmBootOptionMenu  = {
            NULL, NULL, 0, NULL, 0, NULL, 0, NULL,
            L"Select an Option and Press 'Enter' or",
            L"Press 'Esc' to Return to Main Menu (Without Changes)"
        };
        ConfirmBootOptionMenu.TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_BOOTORDER);
        ConfirmBootOptionMenu.Title = L"Confirm Boot Option Operation";

        if (AllowGraphicsMode) {
            Style = GraphicsMenuStyle;
        }
        else {
            Style = TextMenuStyle;
        }

        AddMenuInfoLine (&ConfirmBootOptionMenu, PoolPrint (L"%s", BootOptionString));
        if (Operation == EFI_BOOT_OPTION_MAKE_DEFAULT) {
            CheckString = L"Set This Boot Option as Default?";
        }
        else if (Operation == EFI_BOOT_OPTION_DELETE) {
            CheckString = L"Delete This Boot Option?";
        }
        AddMenuInfoLine (&ConfirmBootOptionMenu, CheckString);

        AddMenuEntry (&ConfirmBootOptionMenu, &MenuEntryYes);
        AddMenuEntry (&ConfirmBootOptionMenu, &MenuEntryNo);

        MenuExit = RunGenericMenu (&ConfirmBootOptionMenu, Style, &DefaultEntry, &ChosenOption);

        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
            L"Returned '%d' (%s) from RunGenericMenu Call on 'Selection = %s' in 'ConfirmBootOptionOperation'",
            MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
        );
        #endif

        if (MyStriCmp (ChosenOption->Title, L"No") || (MenuExit != MENU_EXIT_ENTER)) {
            Operation = EFI_BOOT_OPTION_DO_NOTHING;
        }
    }

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
    IN BOOT_ENTRY_LIST *Entries,
    IN OUT UINTN       *BootOrderNum
) {
    CHAR16              *Temp          = NULL;
    CHAR16              *Filename      = NULL;
    INTN                 DefaultEntry  = 0;
    INTN                 MenuExit;
    UINTN                Operation     = EFI_BOOT_OPTION_DO_NOTHING;
    REFIT_VOLUME        *Volume        = NULL;
    MENU_STYLE_FUNC      Style         = TextMenuStyle;
    REFIT_MENU_ENTRY    *ChosenOption  = NULL;
    REFIT_MENU_ENTRY    *MenuEntryItem = NULL;

    REFIT_MENU_ENTRY    *TempMenuEntry = CopyMenuEntry (&MenuEntryReturn);
    TempMenuEntry->Image               = BuiltinIcon (BUILTIN_ICON_FUNC_BOOTORDER);
    CHAR16 *MenuInfo = L"Promote or Remove Firmware BootOrder Variables";
    REFIT_MENU_SCREEN    Menu = { L"Manage Firmware Boot Order", NULL, 0, &MenuInfo, 0, &TempMenuEntry, 0, NULL,
                                  L"Select an option and press 'Enter' to make it the default, press '-' or",
                                  L"'Delete' to delete it, or 'Esc' to return to Main Menu (without changes)" };
    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    if (!Entries) {
        DisplaySimpleMessage (L"Information", L"Firmware BootOrder List is Unavailable!!");
    }
    else {
        AddMenuInfoLine (&Menu, MenuInfo);
        do {
            MenuEntryItem = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
            FindVolumeAndFilename (Entries->BootEntry.DevPath, &Volume, &Filename);
            if ((Filename != NULL) && (StrLen (Filename) > 0)) {
                if ((Volume != NULL) && (Volume->VolName != NULL)) {
                    Temp = PoolPrint (
                        L"Boot%04x - %s - %s on %s",
                        Entries->BootEntry.BootNum,
                        Entries->BootEntry.Label,
                        Filename, Volume->VolName);
                }
                else {
                    Temp = PoolPrint (
                        L"Boot%04x - %s - %s",
                        Entries->BootEntry.BootNum,
                        Entries->BootEntry.Label,
                        Filename
                    );
                }
            }
            else {
                Temp = PoolPrint (
                    L"Boot%04x - %s",
                    Entries->BootEntry.BootNum,
                    Entries->BootEntry.Label
                );
            }

            MenuEntryItem->Title = StrDuplicate (Temp);
            MenuEntryItem->Row   = Entries->BootEntry.BootNum; // Not really the row but the Boot#### number
            AddMenuEntry (&Menu, MenuEntryItem);

            // Do not free 'Volume' ... just set to NULL
            Volume = NULL;
            MY_FREE_POOL(Temp);
            MY_FREE_POOL(Filename);

            Entries = Entries->NextBootEntry;
        } while (Entries != NULL);

        MenuExit = RunGenericMenu (&Menu, Style, &DefaultEntry, &ChosenOption);
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
            L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'PickOneBootOption'",
            MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
        );
        #endif

        if (MenuExit == MENU_EXIT_ENTER) {
            Operation = EFI_BOOT_OPTION_MAKE_DEFAULT;
            *BootOrderNum = ChosenOption->Row;
        }
        else if (MenuExit == MENU_EXIT_HIDE) {
            Operation = EFI_BOOT_OPTION_DELETE;
            *BootOrderNum = ChosenOption->Row;
        }

        Operation = ConfirmBootOptionOperation (Operation, ChosenOption->Title);

        MY_FREE_POOL(MenuEntryItem);
    }
    MY_FREE_POOL(TempMenuEntry);

    return Operation;
} // REFIT_VOLUME *PickOneBootOption()

static
EFI_STATUS DeleteInvalidBootEntries (VOID) {
    UINTN    Status, VarSize, ListSize, i, j = 0;
    UINT16   *BootOrder, *NewBootOrder;
    CHAR8    *Contents;
    CHAR16   *VarName;

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL, L"Deleting Invalid Boot Entries from Internal BootOrder List");
    #endif

    Status = EfivarGetRaw (
        &GlobalGuid, L"BootOrder",
        (VOID **) &BootOrder, &VarSize
    );
    if (Status == EFI_SUCCESS) {
        ListSize = VarSize / sizeof (UINT16);
        NewBootOrder = AllocateZeroPool (VarSize);

        for (i = 0; i < ListSize; i++) {
            VarName = PoolPrint (L"Boot%04x", BootOrder[i]);
            Status  = EfivarGetRaw (&GlobalGuid, VarName, (VOID **) &Contents, &VarSize);
            MY_FREE_POOL(VarName);

            if (Status == EFI_SUCCESS) {
                NewBootOrder[j++] = BootOrder[i];
                MY_FREE_POOL(Contents);
            }
        } // for

        Status = EfivarSetRaw (
            &GlobalGuid,
            L"BootOrder",
            NewBootOrder,
            j * sizeof (UINT16),
            TRUE
        );

        MY_FREE_POOL(NewBootOrder);
        MY_FREE_POOL(BootOrder);
    }

    return Status;
} // EFI_STATUS DeleteInvalidBootEntries()

VOID ManageBootorder (VOID) {
    BOOT_ENTRY_LIST *Entries;
    UINTN           BootNum = 0, Operation;
    CHAR16          *Name, *Message;

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_THIN_SEP, L"Showing Manage Boot Order Screen");
    #endif

    Entries   = FindBootOrderEntries();
    Operation = PickOneBootOption (Entries, &BootNum);

    if (Operation == EFI_BOOT_OPTION_DELETE) {
        Name = PoolPrint (L"Boot%04x", BootNum);
        EfivarSetRaw (&GlobalGuid, Name, NULL, 0, TRUE);
        DeleteInvalidBootEntries();
        Message = PoolPrint (L"Boot%04x has been Deleted.", BootNum);
        DisplaySimpleMessage (L"Information", Message);
        MY_FREE_POOL(Name);
        MY_FREE_POOL(Message);
    }

    if (Operation == EFI_BOOT_OPTION_MAKE_DEFAULT) {
        SetBootDefault (BootNum);
        Message = PoolPrint (L"Boot%04x is Now the Default EFI Boot Option.", BootNum);
        DisplaySimpleMessage (L"Information", Message);
        MY_FREE_POOL(Message);
    }

    DeleteBootOrderEntries (Entries);
} // VOID ManageBootorder()
