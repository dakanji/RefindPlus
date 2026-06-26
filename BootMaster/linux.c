/*
 * BootMaster/linux.c
 * Code related specifically to Linux loaders
 *
 * Copyright (c) 2006-2010 Christoph Pfisterer
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
 * Modifications for rEFInd Copyright (c) 2012-2020 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 *
 */
/**
** Modified for RefindPlus
** Copyright (c) 2020-2026 Dayo Akanji (sf.net/u/dakanji/profile)
** Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
**
** Modifications distributed under the preceding terms.
**/

#include "global.h"
#include "config.h"
#include "linux.h"
#include "scan.h"
#include "lib.h"
#include "menu.h"
#include "mystrings.h"

// Locate an initrd or initramfs file that matches the kernel specified by LoaderPath.
// The matching file has a name that begins with "init" and includes the same version
// number string as is found in LoaderPath -- but not a longer version number string.
// For instance, if LoaderPath is \EFI\kernels\bzImage-3.3.0.efi, and if \EFI\kernels
// has a file called initramfs-3.3.0.img, this function will return the string
// '\EFI\kernels\initramfs-3.3.0.img'. If the directory ALSO contains the file
// initramfs-3.3.0-rc7.img or initramfs-13.3.0.img, those files will NOT match.
// If more than one initrd file matches the extracted version string, the one
// that matches more characters AFTER (actually, from the start of) the version
// string is used.
// If more than one initrd file matches the extracted version string AND they match
// the same amount of characters, the initrd file with the shortest file name is used.
// If no matching init file can be found, returns NULL.
CHAR16 * FindInitrd (
    IN CHAR16       *LoaderPath,
    IN REFIT_VOLUME *Volume
) {
    #if REFIT_DEBUG > 0
    CHAR16              *VolName; // Do *NOT* Free
    #endif

    UINTN                TempCount;
    UINTN                SharedChars;
    UINTN                MaxSharedChars;
    CHAR16              *Path;
    CHAR16              *OurPath;
    CHAR16              *FileName;
    CHAR16              *InitrdName;
    CHAR16              *KernelPostNum;
    CHAR16              *InitrdPostNum;
    CHAR16              *KernelVersion;
    CHAR16              *InitrdVersion;
    BOOLEAN              CheckIter;
    STRING_LIST         *InitrdNames;
    STRING_LIST         *FinalInitrdName;
    STRING_LIST         *MaxSharedInitrd;
    STRING_LIST         *CurrentInitrdName;
    EFI_FILE_INFO       *DirEntry;
    REFIT_DIR_ITER       DirIter;


    OurPath = (
        LoaderPath[0] == L'\\'
    ) ? StrDuplicate (
        LoaderPath
    ) : PoolPrint (
        L"\\%s", LoaderPath
    );

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Locate/Match Linux Initrd File For Loader:- '%s'",
        OurPath
    );
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    BREAD_CRUMB(L"%a:  2", __func__);
    FileName = Basename (
        OurPath
    );

    BREAD_CRUMB(L"%a:  3", __func__);
    KernelVersion = FindNumbers (FileName);

    BREAD_CRUMB(L"%a:  4", __func__);
    Path = FindPath (
        OurPath
    );

    // Add trailing backslash to root directory (necessary on some systems).
    // NB: Limit to the root directory as on some systems, trailing backslashes
    // on anything else apart from the root directory may result in issues.
    if (StrLen (Path) == 0) {
        BREAD_CRUMB(L"%a:  4a 1", __func__);
        MergeStrings (
            &Path,
            L"\\", 0
        );
    }

    BREAD_CRUMB(L"%a:  5", __func__);
    #if REFIT_DEBUG > 0
    VolName = Volume->VolName;
    ALT_LOG(1, LOG_THREE_STAR_MID, L"Path                  : %s", (Path          != NULL) ? Path          : L"NULL");
    ALT_LOG(1, LOG_THREE_STAR_MID, L"Volume                : %s", (VolName       != NULL) ? VolName       : L"NULL");
    ALT_LOG(1, LOG_THREE_STAR_MID, L"FileName              : %s", (FileName      != NULL) ? FileName      : L"NULL");
    ALT_LOG(1, LOG_THREE_STAR_MID, L"Kernel Version String : %s", (KernelVersion != NULL) ? KernelVersion : L"NULL");
    #endif

    BREAD_CRUMB(L"%a:  6", __func__);
    DirIterOpen (
        Volume->RootDir,
        Path, &DirIter
    );

    // Add trailing backslash now if not added earlier.
    // For consistency in building 'InitrdName' later.
    BREAD_CRUMB(L"%a:  7", __func__);
    TempCount = StrLen (
        Path
    );
    if (TempCount > 0) {
        BREAD_CRUMB(L"%a:  7a 1", __func__);
        if (Path[TempCount - 1] != L'\\') {
            BREAD_CRUMB(L"%a:  7a 1a 1", __func__);
            MergeStrings (
                &Path,
                L"\\", 0
            );
        }
    }

    BREAD_CRUMB(L"%a:  8", __func__);
    InitrdNames = FinalInitrdName = CurrentInitrdName = NULL;
    while (1) {
        CheckIter = DirIterNext (
            &DirIter, FILTER_FILE,
            L"init*,booster*", &DirEntry
        );
        if (!CheckIter) break;

        BREAD_CRUMB(L"%a:  8a 0", __func__);
        InitrdVersion = FindNumbers (DirEntry->FileName);

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Validate 'KernelVersion = %s' on 'DirEntry = %s' with 'InitrdVersion = %s'",
            (KernelVersion      != NULL) ? KernelVersion      : L"NULL",
            (DirEntry->FileName != NULL) ? DirEntry->FileName : L"NULL",
            (InitrdVersion      != NULL) ? InitrdVersion      : L"NULL"
        );
        #endif

        LOG_SEP(L"X");
        BREAD_CRUMB(L"%a:  8a 1 - WHILE LOOP:- START", __func__);

        BREAD_CRUMB(L"%a:  8a 2", __func__);
        if ((
                KernelVersion != NULL &&
                MyStriCmp (InitrdVersion, KernelVersion)
            ) || (
                KernelVersion == NULL &&
                InitrdVersion == NULL
            )
        ) {
            BREAD_CRUMB(L"%a:  8a 2a 1", __func__);
            CurrentInitrdName = AllocateZeroPool (
                sizeof (STRING_LIST)
            );

            BREAD_CRUMB(L"%a:  8a 2a 2", __func__);
            if (InitrdNames == NULL) {
                BREAD_CRUMB(L"%a:  8a 2a 2a 1", __func__);
                InitrdNames = FinalInitrdName = CurrentInitrdName;
            }

            BREAD_CRUMB(L"%a:  8a 2a 3", __func__);
            if (CurrentInitrdName != NULL) {
                BREAD_CRUMB(L"%a:  8a 2a 3a 1", __func__);
                CurrentInitrdName->Value = PoolPrint (
                    L"%s%s",
                    Path,
                    DirEntry->FileName
                );

                BREAD_CRUMB(L"%a:  8a 2a 3a 2 - CurrentInitrdName = '%s'", __func__,
                    (CurrentInitrdName->Value) ? CurrentInitrdName->Value : L"NULL"
                );
                if (CurrentInitrdName != FinalInitrdName) {
                    BREAD_CRUMB(L"%a:  8a 2a 3a 2a 1", __func__);
                    FinalInitrdName->Next = CurrentInitrdName;
                    FinalInitrdName       = CurrentInitrdName;
                }
                BREAD_CRUMB(L"%a:  8a 2a 3a 3", __func__);
            }
            BREAD_CRUMB(L"%a:  8a 2a 4", __func__);
        }
        BREAD_CRUMB(L"%a:  8a 2a 5", __func__);
        MY_FREE_POOL(InitrdVersion);
        MY_FREE_POOL(DirEntry);

        BREAD_CRUMB(L"%a:  8a 3 - WHILE LOOP:- END", __func__);
        LOG_SEP(L"X");
    } // while {Infinite}

    BREAD_CRUMB(L"%a:  9", __func__);
    InitrdName = NULL;
    if (InitrdNames != NULL) {
        BREAD_CRUMB(L"%a:  9a 1", __func__);
        if (InitrdNames->Next == NULL) {
            BREAD_CRUMB(L"%a:  9a 1a 1", __func__);
            InitrdName = StrDuplicate (
                InitrdNames->Value
            );
        }
        else {
            BREAD_CRUMB(L"%a:  9a 1b 1", __func__);
            MaxSharedChars  = 0;
            MaxSharedInitrd = CurrentInitrdName = InitrdNames;

            BREAD_CRUMB(L"%a:  9a 1b 2", __func__);
            while (CurrentInitrdName != NULL) {
                LOG_SEP(L"X");
                BREAD_CRUMB(L"%a:  9a 1b 2a 1 - WHILE LOOP:- START", __func__);

                BREAD_CRUMB(L"%a:  9a 1b 2a 2", __func__);
                KernelPostNum = MyStrStr (
                    OurPath, KernelVersion
                );

                BREAD_CRUMB(L"%a:  9a 1b 2a 3", __func__);
                InitrdPostNum = MyStrStr (
                    CurrentInitrdName->Value,
                    KernelVersion
                );

                BREAD_CRUMB(L"%a:  9a 1b 2a 4", __func__);
                SharedChars = NumCharsInCommon (
                    KernelPostNum,
                    InitrdPostNum
                );

                BREAD_CRUMB(L"%a:  9a 1b 2a 5", __func__);
                if ((SharedChars > MaxSharedChars) ||
                    (
                        SharedChars == MaxSharedChars
                        && StrLen (CurrentInitrdName->Value) < StrLen (MaxSharedInitrd->Value)
                    )
                ) {
                    BREAD_CRUMB(L"%a:  9a 1b 2a 5a 1", __func__);
                    MaxSharedChars = SharedChars;
                    MaxSharedInitrd = CurrentInitrdName;
                }

                BREAD_CRUMB(L"%a:  9a 1b 2a 6", __func__);
                // DA-TAG: Investigate This
                //         Compute number of shared characters and compare with max.
                CurrentInitrdName = CurrentInitrdName->Next;
            } // while

            BREAD_CRUMB(L"%a:  9a 1b 3", __func__);
            if (MaxSharedInitrd != NULL) {
                BREAD_CRUMB(L"%a:  9a 1b 3a 1", __func__);
                InitrdName = StrDuplicate (
                    MaxSharedInitrd->Value
                );
                BREAD_CRUMB(L"%a:  9a 1b 3a 2", __func__);
            }
            BREAD_CRUMB(L"%a:  9a 1b 4", __func__);
        } // if/else InitrdNames->Next == NULL

        BREAD_CRUMB(L"%a:  9a 2", __func__);
    } // if

    BREAD_CRUMB(L"%a:  10", __func__);
    DeleteStringList (
        InitrdNames
    );

    BREAD_CRUMB(L"%a:  11", __func__);
    MY_FREE_POOL(Path);
    MY_FREE_POOL(OurPath);
    MY_FREE_POOL(FileName);
    MY_FREE_POOL(KernelVersion);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_MID,
        L"Identified Linux Initrd File:- '%s'",
        (InitrdName != NULL) ? InitrdName : L"NONE"
    );
    #endif

    BREAD_CRUMB(L"%a:  12 - END:- return CHAR16 *InitrdName = '%s'", __func__,
        (InitrdName != NULL) ? InitrdName : L"NULL"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return InitrdName;
} // static CHAR16 * FindInitrd()


static
VOID AddMenuEntrySpacer (
    IN OUT REFIT_MENU_SCREEN **Screen
) {
    REFIT_MENU_ENTRY *MenuEntrySpacer;


    if (Screen == NULL || *Screen == NULL) {
        // Early Return
        return;
    }

    MenuEntrySpacer = AllocateZeroPool (
        sizeof (REFIT_MENU_ENTRY)
    );
    if (MenuEntrySpacer == NULL) {
        // Early Return
        return;
    }

    MenuEntrySpacer->Title = StrDuplicate (GEN_TAG);
    MenuEntrySpacer->Tag = TAG_SPACER;
    AddMenuEntry (*Screen, MenuEntrySpacer);
} // static VOID AddMenuEntrySpacer()

// Adds InitrdPath to Options, but only if Options does not already include an
// initrd= line or a `%v` variable. Done to enable overriding the default initrd
// selection in a refindplus_linux.conf or refind_linux.conf file's options list.
// If a `%v` substring/variable is found in Options, it is replaced with the
// initrd version string to allow more complex customisation of initrd options.
//
// Returns a pointer to a new string.
// The calling function is responsible for freeing allocated memory.
CHAR16 * AddInitrdToOptions (
    CHAR16 *Options,
    CHAR16 *InitrdPath
) {
    CHAR16 *NewOptions;
    CHAR16 *InitrdVersion;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    if (Options == NULL) {
        BREAD_CRUMB(L"%a:  1a 1", __func__);
        NewOptions = NULL;
    }
    else {
        BREAD_CRUMB(L"%a:  1b 1", __func__);
        NewOptions = StrDuplicate (
            Options
        );
    }

    BREAD_CRUMB(L"%a:  2", __func__);
    if (InitrdPath == NULL) {
        BREAD_CRUMB(L"%a:  2a 1 - END:- return CHAR16 *NewOptions = '%s' ... NULL 'InitrdPath' Input", __func__,
            (NewOptions != NULL) ? NewOptions : L"NULL"
        );
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return NewOptions;
    }

    BREAD_CRUMB(L"%a:  3", __func__);
    if (NewOptions != NULL && FindSubStr (NewOptions, L"%v")) {
        BREAD_CRUMB(L"%a:  3a 1", __func__);
        InitrdVersion = FindNumbers (InitrdPath);

        BREAD_CRUMB(L"%a:  3a 2", __func__);
        ReplaceSubstring (
            &NewOptions,
            L"%v", InitrdVersion
        );

        BREAD_CRUMB(L"%a:  3a 3", __func__);
        MY_FREE_POOL(InitrdVersion);
    }
    else {
        BREAD_CRUMB(L"%a:  3b 1", __func__);
        if (NewOptions == NULL || !FindSubStr (NewOptions, L"initrd=")) {
            BREAD_CRUMB(L"%a:  3b 1a 1", __func__);
            MergeStrings (
                &NewOptions,
                L"initrd=", L' '
            );

            BREAD_CRUMB(L"%a:  3b 1a 2", __func__);
            MergeStrings (
                &NewOptions,
                InitrdPath, 0
            );
        }
        BREAD_CRUMB(L"%a:  3b 2", __func__);
    }

    BREAD_CRUMB(L"%a:  4 - END:- return CHAR16 *NewOptions = '%s'", __func__,
        (NewOptions != NULL) ? NewOptions : L"NULL"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return NewOptions;
} // CHAR16 *AddInitrdToOptions()

// Returns options for a Linux kernel. Reads them from an options file in the
// kernel's directory; and if present, adds an initrd= option for an initial
// RAM disk file with the same version number as the kernel file.
CHAR16 * GetMainLinuxOptions (
    IN CHAR16       *LoaderPath,
    IN REFIT_VOLUME *Volume
) {
    #if REFIT_DEBUG > 0
    BOOLEAN  CheckMute = FALSE;
    #endif

    CHAR16  *Options;
    CHAR16  *InitrdName;
    CHAR16  *FullOptions;
    CHAR16  *KernelVersion;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    Options = GetFirstOptionsFromFile (
        LoaderPath, Volume
    );

    BREAD_CRUMB(L"%a:  2", __func__);
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_SET;
    #endif
    InitrdName = FindInitrd (
        LoaderPath, Volume
    );
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    BREAD_CRUMB(L"%a:  3", __func__);
    if (InitrdName != NULL) {
        BREAD_CRUMB(L"%a:  3a 1", __func__);
        KernelVersion = FindNumbers (InitrdName);

        BREAD_CRUMB(L"%a:  3a 2", __func__);
        if (Options != NULL) {
            BREAD_CRUMB(L"%a:  3a 2a 1", __func__);
            ReplaceSubstring (
                &Options,
                KERNEL_VERSION,
                KernelVersion
            );
            BREAD_CRUMB(L"%a:  3a 2a 2", __func__);
        }
        MY_FREE_POOL(KernelVersion);
    }

    BREAD_CRUMB(L"%a:  4", __func__);
    FullOptions = NULL;
    if (InitrdName != NULL || Options != NULL) {
        BREAD_CRUMB(L"%a:  4a 1", __func__);
        FullOptions = AddInitrdToOptions (
            Options, InitrdName
        );
        BREAD_CRUMB(L"%a:  4a 2", __func__);
    }

    BREAD_CRUMB(L"%a:  5", __func__);
    MY_FREE_POOL(Options);
    MY_FREE_POOL(InitrdName);

    BREAD_CRUMB(L"%a:  6 - END:- return CHAR16 *FullOptions = '%s'", __func__,
        (FullOptions != NULL) ? FullOptions : L"NULL"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return FullOptions;
} // CHAR16 * GetMainLinuxOptions()

static
VOID ParseHelper (
    CHAR16       **ReturnTag,
    REFIT_VOLUME  *Volume,
    CHAR16        *FileName,
    CHAR16        *TargetType,
    BOOLEAN        FirstOnly
) {
    EFI_STATUS    Status;
    UINTN         FileSize;
    UINTN         TokenCount;
    CHAR16      **TokenList;
    CHAR16       *TempName;
    BOOLEAN       Depart;
    REFIT_FILE   *File;


    if (Volume   == NULL ||
        FileName == NULL ||
        !FileExists (Volume->RootDir, FileName)
    ) {
        return;
    }

    File = AllocateZeroPool (
        sizeof (REFIT_FILE)
    );
    if (File == NULL) {
        return;
    }

    FileSize = 0;
    TempName = NULL;
    Depart   = FALSE;

    Status = RefitReadFile (
        Volume->RootDir, FileName,
        File, &FileSize
    );
    if (!EFI_ERROR(Status)) {
        while (1) {
            TokenCount = ReadTokenLine (
                File, &TokenList
            );
            if (TokenCount == 0) {
                // Flag to Exit Loop
                Depart = TRUE;
            }
            else {
                if (TokenCount > 1 &&
                    MyStriCmp (TokenList[0], TargetType)
                ) {
                    Depart = TRUE;

                    MY_FREE_POOL(TempName);
                    TempName = StrDuplicate (
                        TokenList[1]
                    );
                    MergeUniqueWords (
                        ReturnTag,
                        TempName, L','
                    );
                }
            }

            FreeTokenLine (
                &TokenList,
                &TokenCount
            );

            if (Depart) break;
        } // while {Infinite}
    }

    MY_FREE_FILE(File);

    if (!FirstOnly) {
        ToLower (*ReturnTag);
        MY_FREE_POOL(TempName);
    }

    if (TempName == NULL) {
        return;
    }

    // Capitalise First Letter
    if (TempName[0] >= L'a' &&
        TempName[0] <= L'z'
    ) {
        TempName[0] = TempName[0] - L'a' + L'A';
    }

    MY_FREE_POOL(*ReturnTag);
    *ReturnTag = TempName;
} // static VOID ParseHelper()

// Read the specified file and add values of "ID", "NAME", or "DISTRIB_ID"
// tokens to the "ReturnTag" list. Intended for adding Linux distribution
// clues gleaned from the "/etc/os-release" and "/etc/lsb-release" files.
static
VOID ParseReleaseFile (
    CHAR16       **ReturnTag,
    REFIT_VOLUME  *Volume,
    CHAR16        *FileName,
    BOOLEAN        FirstOnly
) {
    if (Volume   == NULL ||
        FileName == NULL ||
        !FileExists (Volume->RootDir, FileName)
    ) {
        return;
    }

    ParseHelper (
        ReturnTag, Volume,
        FileName, L"ID",
        FirstOnly
    );

    if (!FirstOnly  ||
        *ReturnTag == NULL
    ) {
        ParseHelper (
            ReturnTag, Volume,
            FileName, L"DISTRIB_ID",
            FirstOnly
        );
    }

    if (!FirstOnly  ||
        *ReturnTag == NULL
    ) {
        ParseHelper (
            ReturnTag, Volume,
            FileName, L"NAME",
            FirstOnly
        );
    }

    if (!FirstOnly  ||
        *ReturnTag == NULL
    ) {
        ParseHelper (
            ReturnTag, Volume,
            FileName, L"ID_LIKE",
            FirstOnly
        );
    }
} // static VOID ParseReleaseFile()

VOID HandleParseCall (
    CHAR16       **ReturnTag,
    REFIT_VOLUME  *Volume,
    BOOLEAN        FirstOnly,
    BOOLEAN        VolumeTags
) {
    CHAR16        *VolTempName;
    CHAR16        *RelFileType;


    if (Volume == NULL) {
        return;
    }

    if (FileExists (Volume->RootDir, L"etc\\os-release")) {
        RelFileType = L"etc\\os-release";
    }
    else {
        RelFileType = L"usr\\lib\\os-release";
    }
    ParseReleaseFile (
        ReturnTag, Volume,
        RelFileType, FirstOnly
    );

    if (!FirstOnly || *ReturnTag == NULL) {
        ParseReleaseFile (
            ReturnTag, Volume,
            L"etc\\lsb-release", FirstOnly
        );
    }

    // DA-TAG: Strip Misc Unwanted Out
    DeleteItemFromCsvList (L"os",    ReturnTag);
    DeleteItemFromCsvList (L"gnu",   ReturnTag);
    DeleteItemFromCsvList (L"linux", ReturnTag);

    if (VolumeTags) {
        VolTempName = *ReturnTag;
        if (IsStriStr (VolTempName, L"Opensuse")) {
            *ReturnTag = StrDuplicate (
                L"OpenSUSE Volume"
            );
        }
        else {
            *ReturnTag = PoolPrint (
                L"%s Volume", VolTempName
            );
        }
        MY_FREE_POOL(VolTempName);
    }
} // VOID HandleParseCall()

// Try to guess Linux distribution name and add to ReturnTag list
VOID GuessLinuxDistribution (
    CHAR16       **ReturnTag,
    REFIT_VOLUME  *Volume,
    CHAR16        *LoaderPath,
    BOOLEAN        FirstOnly
) {
    UINTN          i;
    CHAR16        *LinuxName;
    CHAR16        *ShowName;  // Do *NOT* Free
    BOOLEAN        Found;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    BREAD_CRUMB(L"%a:  2 - Input List = '%s'", __func__,
        (*ReturnTag != NULL) ? *ReturnTag : L"NULL"
    );

    // /etc/os-release or /etc/lsb-release on Linux root fs may have clues
    BREAD_CRUMB(L"%a:  3", __func__);
    HandleParseCall (
        ReturnTag, Volume,
        FirstOnly, FALSE
    );

    BREAD_CRUMB(L"%a:  4", __func__);
    if (FirstOnly && *ReturnTag != NULL) {
        BREAD_CRUMB(L"%a:  6a 1 - END:- ReturnTagList = %s", __func__, *ReturnTag);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return;
    }

    // Search for clues in kernel filename
    BREAD_CRUMB(L"%a:  5", __func__);
    if (FindSubStr (LoaderPath, L".fc")) {
        BREAD_CRUMB(L"%a:  5a 1 - Fedora Loader", __func__);
        if (FirstOnly) {
            BREAD_CRUMB(L"%a:  5a 1a 1", __func__);
            *ReturnTag = StrDuplicate (
                L"Fedora"
            );
        }
        else {
            BREAD_CRUMB(L"%a:  5a 1b 1", __func__);
            MergeUniqueStrings (
                ReturnTag,
                L"fedora", L','
            );
            BREAD_CRUMB(L"%a:  5a 1b 2", __func__);
        }
        BREAD_CRUMB(L"%a:  5a 2", __func__);
    }
    else if (FindSubStr (LoaderPath, L".el")) {
        BREAD_CRUMB(L"%a:  5b 1 - RedHat Loader", __func__);
        if (FirstOnly) {
            BREAD_CRUMB(L"%a:  5b 1a 1", __func__);
            *ReturnTag = StrDuplicate (
                L"RedHat"
            );
        }
        else {
            BREAD_CRUMB(L"%a:  5b 1b 1", __func__);
            MergeUniqueStrings (
                ReturnTag,
                L"redhat", L','
            );
            BREAD_CRUMB(L"%a:  5b 1b 2", __func__);
        }
        BREAD_CRUMB(L"%a:  5b 2", __func__);
    }
    else {
        BREAD_CRUMB(L"%a:  5c 1 - General Check", __func__);
        Found = FALSE;

        i = 0;
        while (!Found) {
            LinuxName = FindCommaDelimited (
                MAIN_LINUX_DISTROS, i++
            );
            if (LinuxName == NULL) break;

            ShowName = GetShowName (
                LinuxName
            );
            if (FindSubStr (LoaderPath, ShowName)) {
                Found = TRUE;

                if (FirstOnly) {
                    *ReturnTag = StrDuplicate (
                        ShowName
                    );
                }
                else {
                    MergeUniqueStrings (
                        ReturnTag,
                        ShowName, L','
                    );
                }
            }

            if (!Found && FindSubStr (LoaderPath, LinuxName)) {
                Found = TRUE;

                if (FirstOnly) {
                    *ReturnTag = StrDuplicate (
                        LinuxName
                    );
                }
                else {
                    MergeUniqueStrings (
                        ReturnTag,
                        LinuxName, L','
                    );
                }
            }

            MY_FREE_POOL(LinuxName);
        } // while
        BREAD_CRUMB(L"%a:  5c 2", __func__);
    }

    BREAD_CRUMB(L"%a:  6 - END:- ReturnTagList = %s", __func__,
        (*ReturnTag) ? *ReturnTag : L"NULL"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID GuessLinuxDistribution()

// Add Linux kernel as submenu entry for pre-existing kernel entry
VOID AddKernelToSubmenu (
    LOADER_ENTRY *TargetLoader,
    CHAR16       *FileName,
    REFIT_VOLUME *Volume
) {
    #if REFIT_DEBUG > 0
    BOOLEAN  CheckMute = FALSE;
    #endif

    REFIT_FILE          *File;
    CHAR16             **TokenList = NULL;
    CHAR16              *Path;
    CHAR16              *VolName;
    CHAR16              *KernFile;
    CHAR16              *InitrdName;
    CHAR16              *ActualLoader;
    CHAR16              *KernelVersion;
    CHAR16              *BootTypeTag;
    CHAR16              *OutputData;
    REFIT_MENU_SCREEN   *SubScreen;
    LOADER_ENTRY        *SubEntry;
    UINTN                TokenCount;


    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_STAR_HEAD_SEPX, L"Add Linux Kernel as SubMenu Entry");
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    File = ReadLinuxOptionsFile (
        TargetLoader->LoaderPath,
        Volume
    );

    BREAD_CRUMB(L"%a:  2", __func__);
    if (File == NULL) {
        BREAD_CRUMB(L"%a:  2a 1 - END:- ReadLinuxOptionsFile FAILED", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early RETURN
        return;
    }

    BREAD_CRUMB(L"%a:  3", __func__);
    SubScreen = TargetLoader->me.SubScreen;

    BREAD_CRUMB(L"%a:  4", __func__);
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_SET;
    #endif
    AddMenuEntrySpacer (&SubScreen);
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    BREAD_CRUMB(L"%a:  5", __func__);
    InitrdName = FindInitrd (FileName, Volume);

    BREAD_CRUMB(L"%a:  6", __func__);
    KernelVersion = FindNumbers (FileName);

    BREAD_CRUMB(L"%a:  7", __func__);
    ActualLoader = StrDuplicate (FileName);
    CleanUpPathNameSlashes (ActualLoader);

    BREAD_CRUMB(L"%a:  8", __func__);
    Path = VolName = BootTypeTag = NULL;
    while (1) {
        TokenCount = ReadTokenLine (File, &TokenList);
        if (TokenCount < 2) {
            FreeTokenLine (&TokenList, &TokenCount);
            break;
        }

        LOG_SEP(L"X");
        BREAD_CRUMB(L"%a:  8a 1 - WHILE LOOP:- START", __func__);
        ReplaceSubstring (
            &(TokenList[1]),
            KERNEL_VERSION,
            KernelVersion
        );

        BREAD_CRUMB(L"%a:  8a 2", __func__);
        SubEntry = CopyLoaderEntry (TargetLoader);

        BREAD_CRUMB(L"%a:  8a 3", __func__);
        if (SubEntry == NULL) {
            BREAD_CRUMB(L"%a:  8a 3a 1 - WHILE LOOP:- BREAK", __func__);
            LOG_SEP(L"X");

            FreeTokenLine (&TokenList, &TokenCount);
            continue;
        }

        BREAD_CRUMB(L"%a:  8a 4", __func__);
        BootTypeTag = (
            TokenList[0] != NULL
        ) ? CapitalisedCase (
            TokenList[0], TRUE
        ) : StrDuplicate (
            L"Boot Linux"
        );

        BREAD_CRUMB(L"%a:  8a 5", __func__);
        SplitPathName (
            FileName, &VolName,
            &Path, &KernFile
        );

        BREAD_CRUMB(L"%a:  8a 6", __func__);
        OutputData = PoolPrint (
            L"%s : %s",
            BootTypeTag, KernFile
        );

        BREAD_CRUMB(L"%a:  8a 7", __func__);
        LimitStringLength (OutputData, MAX_LINE_LENGTH);

        BREAD_CRUMB(L"%a:  8a 8", __func__);
        SubEntry->me.Title = OutputData;

        BREAD_CRUMB(L"%a:  8a 9", __func__);
        // DA-TAG: Sets 'LoadOptions' to 'TokenList[1]'
        //         Adds 'InitrdName' if available and needed
        MY_FREE_POOL(SubEntry->LoadOptions);
        SubEntry->LoadOptions = AddInitrdToOptions (
            TokenList[1], InitrdName
        );

        BREAD_CRUMB(L"%a:  8a 10", __func__);
        SubEntry->Volume = Volume;
        MY_FREE_POOL(SubEntry->LoaderPath);
        SubEntry->LoaderPath = ActualLoader;
        SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;
        AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);

        BREAD_CRUMB(L"%a:  8a 11", __func__);
        FreeTokenLine (&TokenList, &TokenCount);
        MY_FREE_POOL(BootTypeTag);
        MY_FREE_POOL(KernFile);
        MY_FREE_POOL(VolName);
        MY_FREE_POOL(Path);

        BREAD_CRUMB(L"%a:  8a 12 - WHILE LOOP:- END", __func__);
        LOG_SEP(L"X");
    } // while {Infinite}

    BREAD_CRUMB(L"%a:  9", __func__);
    MY_FREE_POOL(KernelVersion);
    MY_FREE_POOL(InitrdName);
    MY_FREE_FILE(File);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_END,
        L"Added Linux Kernel SubMenu Entry to %s",
        TargetLoader->Title
    );
    #endif

    BREAD_CRUMB(L"%a:  10 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID AddKernelToSubmenu()

// Returns TRUE if a file with the same name as the original but with
// ".efi.signed" is also present in the same directory. Ubuntu is using
// this filename as a signed version of the original unsigned kernel, and
// there is no point in cluttering the display with two kernels that will
// behave identically on non-SB systems, or when one will fail when SB
// is active.
// CAUTION: *FullName MUST be properly cleaned up (via CleanUpPathNameSlashes())
BOOLEAN HasSignedCounterpart (
    IN REFIT_VOLUME *Volume,
    IN CHAR16       *FullName
) {
    CHAR16  *NewFile;
    BOOLEAN  retval;


    NewFile = NULL;
    MergeStrings (&NewFile, FullName, 0);
    MergeStrings (&NewFile, L".efi.signed", 0);

    retval = FALSE;
    if (NewFile != NULL) {
        if (FileExists(Volume->RootDir, NewFile)) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"Found Signed Counterpart to '%s'",
                FullName
            );
            #endif

            retval = TRUE;
        }
        MY_FREE_POOL(NewFile);
    } // if

    return retval;
} // BOOLEAN HasSignedCounterpart()
