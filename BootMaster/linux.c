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
 * Modifications copyright (c) 2012-2020 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 *
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 * Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "config.h"
#include "lib.h"
#include "menu.h"
#include "mystrings.h"
#include "linux.h"
#include "scan.h"

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
    UINTN                SharedChars       = 0;
    UINTN                MaxSharedChars    = 0;
    CHAR16              *Path              = NULL;
    CHAR16              *FileName          = NULL;
    CHAR16              *InitrdName        = NULL;
    CHAR16              *KernelPostNum     = NULL;
    CHAR16              *InitrdPostNum     = NULL;
    CHAR16              *KernelVersion     = NULL;
    CHAR16              *InitrdVersion     = NULL;
    STRING_LIST         *InitrdNames       = NULL;
    STRING_LIST         *FinalInitrdName   = NULL;
    STRING_LIST         *MaxSharedInitrd   = NULL;
    STRING_LIST         *CurrentInitrdName = NULL;
    EFI_FILE_INFO       *DirEntry;
    REFIT_DIR_ITER       DirIter;

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL,
        L"Search for Initrd Matching '%s' in '%s'",
        LoaderPath, Volume->VolName
    );
    #endif

    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 1 - START");
    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 2");
    FileName = Basename (LoaderPath);

    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 3");
    KernelVersion = FindNumbers (FileName);

    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 4");
    Path  = FindPath (LoaderPath);

    // Add trailing backslash for root directory; necessary on some systems, but must
    // NOT be added to all directories, since on other systems, a trailing backslash on
    // anything but the root directory causes them to flake out!
    if (StrLen (Path) == 0) {
        LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 4a 1");
        MergeStrings (&Path, L"\\", 0);

        LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 4a 2");
    }

    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 5");
    #if REFIT_DEBUG > 0
    LOG(3, LOG_THREE_STAR_MID, L"Path                  : '%s'", Path);
    LOG(3, LOG_THREE_STAR_MID, L"FileName              : '%s'", FileName);
    LOG(3, LOG_THREE_STAR_MID, L"Kernel Version String : '%s'", KernelVersion);
    #endif

    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 6");
    DirIterOpen (Volume->RootDir, Path, &DirIter);

    // Now add a trailing backslash if it was NOT added earlier, for consistency in
    // building the InitrdName later
    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 7");
    if ((StrLen (Path) > 0) && (Path[StrLen (Path) - 1] != L'\\')) {
        LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 7a 1");
        MergeStrings(&Path, L"\\", 0);
        LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 7a 2");
    }

    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8");
    while (DirIterNext (&DirIter, 2, L"init*,booster*", &DirEntry)) {
        LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 0");
        InitrdVersion = FindNumbers (DirEntry->FileName);

        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
            L"Checking 'KernelVersion = %s' Against 'InitrdVersion = %s' from '%s'",
            KernelVersion,
            InitrdVersion ? InitrdVersion : L"NULL",
            DirEntry->FileName
        );
        #endif

        LOG(5, LOG_BLANK_LINE_SEP, L"X");
        LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 1 - START WHILE LOOP");


        LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 2");
        if (((KernelVersion != NULL) && (MyStriCmp (InitrdVersion, KernelVersion))) ||
            ((KernelVersion == NULL) && (InitrdVersion == NULL))
        ) {
            LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 2a 1");
            CurrentInitrdName = AllocateZeroPool (sizeof(STRING_LIST));

            LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 2a 2");
            if (InitrdNames == NULL) {
                LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 2a 2a 1");
                InitrdNames = FinalInitrdName = CurrentInitrdName;
                LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 2a 2a 2");
            }

            LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 2a 3");
            if (CurrentInitrdName) {
                LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 2a 3a 1");
                CurrentInitrdName->Value = PoolPrint (L"%s%s", Path, DirEntry->FileName);

                LOG(5, LOG_LINE_FORENSIC,
                    L"In FindInitrd ... 8a 2a 3a 2 - CurrentInitrdName = '%s'",
                    CurrentInitrdName->Value ? CurrentInitrdName->Value : L"NULL"
                );
                if (CurrentInitrdName != FinalInitrdName) {
                    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 2a 3a 2a 1");
                    FinalInitrdName->Next = CurrentInitrdName;
                    FinalInitrdName       = CurrentInitrdName;
                    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 2a 3a 2a 2");
                }
                LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 2a 3a 3");
            }
            LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 2a 4");
        }
        LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 2a 5");
        MY_FREE_POOL(InitrdVersion);

        LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 8a 3 - END WHILE LOOP");
        LOG(5, LOG_BLANK_LINE_SEP, L"X");
    } // while

    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9");
    if (InitrdNames) {
        LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9a 1");
        if (InitrdNames->Next == NULL) {
            LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9a 1a 1");
            InitrdName = StrDuplicate (InitrdNames -> Value);
            LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9a 1a 2");
        }
        else {
            LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 1");
            MaxSharedInitrd = CurrentInitrdName = InitrdNames;
            MaxSharedChars = 0;

            LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 2");
            while (CurrentInitrdName != NULL) {
                LOG(5, LOG_BLANK_LINE_SEP, L"X");
                LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 2a 1 - START WHILE LOOP");

                LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 2a 2");
                KernelPostNum = MyStrStr (LoaderPath, KernelVersion);

                LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 2a 3");
                InitrdPostNum = MyStrStr (CurrentInitrdName->Value, KernelVersion);

                LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 2a 4");
                SharedChars = NumCharsInCommon (KernelPostNum, InitrdPostNum);

                LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 2a 5");
                if ((SharedChars > MaxSharedChars) ||
                    (
                        SharedChars == MaxSharedChars
                        && StrLen (CurrentInitrdName->Value) < StrLen (MaxSharedInitrd->Value)
                    )
                ) {
                    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 2a 5a 1");
                    MaxSharedChars = SharedChars;
                    MaxSharedInitrd = CurrentInitrdName;
                    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 2a 5a 2");
                }

                LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 2a 6");
                // TODO: Compute number of shared characters & compare with max.
                CurrentInitrdName = CurrentInitrdName->Next;
                LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 2a 7");
            }

            LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 3");
            if (MaxSharedInitrd) {
                LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 3a 1");
                InitrdName = StrDuplicate (MaxSharedInitrd->Value);
                LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 3a 2");
            }
            LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 4");
        } // if/else InitrdNames->Next == NULL
        LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 9b 4");
    } // if
    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 10");
    DeleteStringList(InitrdNames);

    LOG(5, LOG_LINE_FORENSIC, L"In FindInitrd ... 11");
    MY_FREE_POOL(Path);
    MY_FREE_POOL(FileName);
    MY_FREE_POOL(KernelVersion);

    LOG(5, LOG_LINE_FORENSIC,
        L"In FindInitrd ... 12 - END:- return CHAR16 *InitrdName = '%s'",
        InitrdName ? InitrdName : L"NULL"
    );
    LOG(5, LOG_BLANK_LINE_SEP, L"X");

    #if REFIT_DEBUG > 0
    LOG(3, LOG_THREE_STAR_MID, L"Located Initrd:- '%s'", InitrdName ? InitrdName : L"NULL");
    #endif
    return InitrdName;
} // static CHAR16 * FindInitrd()


// Adds InitrdPath to Options, but only if Options does not already include an
// initrd= line or a `%v` variable. Done to enable overriding the default initrd
// selection in a refind_linux.conf file's options list.
// If a `%v` substring/variable is found in Options, it is replaced with the
// initrd version string. This is available to allow for more complex customisation
// of initrd options.
// Returns a pointer to a new string. The calling function is responsible for
// freeing its memory.
CHAR16 * AddInitrdToOptions (
    CHAR16 *Options,
    CHAR16 *InitrdPath
) {
    CHAR16 *NewOptions = NULL;

    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    LOG(5, LOG_LINE_FORENSIC, L"In AddInitrdToOptions ... 1 - START");
    if (Options != NULL) {
        LOG(5, LOG_LINE_FORENSIC, L"In AddInitrdToOptions ... 1a 1");
        NewOptions = StrDuplicate (Options);

        LOG(5, LOG_LINE_FORENSIC, L"In AddInitrdToOptions ... 1a 2");
    }

    LOG(5, LOG_LINE_FORENSIC, L"In AddInitrdToOptions ... 2");
    if (InitrdPath != NULL) {
        LOG(5, LOG_LINE_FORENSIC, L"In AddInitrdToOptions ... 2a 1");
        if (StriSubCmp (L"%v", Options)) {
            LOG(5, LOG_LINE_FORENSIC, L"In AddInitrdToOptions ... 2a 1a 1");
            CHAR16 *InitrdVersion = FindNumbers (InitrdPath);

            LOG(5, LOG_LINE_FORENSIC, L"In AddInitrdToOptions ... 2a 1a 2");
            ReplaceSubstring (&NewOptions, L"%v", InitrdVersion);

            LOG(5, LOG_LINE_FORENSIC, L"In AddInitrdToOptions ... 2a 1a 3");
            MY_FREE_POOL(InitrdVersion);

            LOG(5, LOG_LINE_FORENSIC, L"In AddInitrdToOptions ... 2a 1a 4");
        }
        else if (!StriSubCmp (L"initrd=", Options)) {
            LOG(5, LOG_LINE_FORENSIC, L"In AddInitrdToOptions ... 2a 1b 1");
            MergeStrings (&NewOptions, L"initrd=", L' ');

            LOG(5, LOG_LINE_FORENSIC, L"In AddInitrdToOptions ... 2a 1b 2");
            MergeStrings (&NewOptions, InitrdPath, 0);

            LOG(5, LOG_LINE_FORENSIC, L"In AddInitrdToOptions ... 2a 1b 3");
        }
        LOG(5, LOG_LINE_FORENSIC, L"In AddInitrdToOptions ... 2a 2");
    }

    LOG(5, LOG_LINE_FORENSIC,
        L"In AddInitrdToOptions ... 3 - END:- return CHAR16 *NewOptions = '%s'",
        NewOptions ? NewOptions : L"NULL"
    );
    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    return NewOptions;
} // CHAR16 *AddInitrdToOptions()

// Returns options for a Linux kernel. Reads them from an options file in the
// kernel's directory; and if present, adds an initrd= option for an initial
// RAM disk file with the same version number as the kernel file.
CHAR16 * GetMainLinuxOptions (
    IN CHAR16       *LoaderPath,
    IN REFIT_VOLUME *Volume
) {
    CHAR16 *Options = NULL, *InitrdName, *FullOptions = NULL, *KernelVersion;

    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    LOG(5, LOG_LINE_FORENSIC, L"In GetMainLinuxOptions ... 1 - START");
    Options = GetFirstOptionsFromFile (LoaderPath, Volume);

    LOG(5, LOG_LINE_FORENSIC, L"In GetMainLinuxOptions ... 2");
    InitrdName = FindInitrd (LoaderPath, Volume);

    LOG(5, LOG_LINE_FORENSIC, L"In GetMainLinuxOptions ... 3");
    KernelVersion = FindNumbers (InitrdName);

    LOG(5, LOG_LINE_FORENSIC, L"In GetMainLinuxOptions ... 4");
    ReplaceSubstring (&Options, KERNEL_VERSION, KernelVersion);

    LOG(5, LOG_LINE_FORENSIC, L"In GetMainLinuxOptions ... 5");
    FullOptions = AddInitrdToOptions (Options, InitrdName);

    LOG(5, LOG_LINE_FORENSIC, L"In GetMainLinuxOptions ... 6");
    MY_FREE_POOL(Options);
    MY_FREE_POOL(InitrdName);
    MY_FREE_POOL(KernelVersion);

    LOG(5, LOG_LINE_FORENSIC,
        L"In GetMainLinuxOptions ... 7 - END:- return CHAR16 *FullOptions = '%s'",
        FullOptions ? FullOptions : L"NULL"
    );
    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    return FullOptions;
} // static CHAR16 * GetMainLinuxOptions()

// Read the specified file and add values of "ID", "NAME", or "DISTRIB_ID" tokens to
// OSIconName list. Intended for adding Linux distribution clues gleaned from
// /etc/lsb-release and /etc/os-release files.
static
VOID ParseReleaseFile (
    CHAR16       **OSIconName,
    REFIT_VOLUME  *Volume,
    CHAR16        *FileName
) {
    UINTN         FileSize   = 0;
    UINTN         TokenCount = 0;
    CHAR16      **TokenList;
    REFIT_FILE    File;

    if ((Volume == NULL) || (FileName == NULL) ||
        (OSIconName == NULL) || (*OSIconName == NULL)
    ) {
        return;
    }

    if (FileExists (Volume->RootDir, FileName) &&
        (RefitReadFile (Volume->RootDir, FileName, &File, &FileSize) == EFI_SUCCESS)
    ) {
        do {
            TokenCount = ReadTokenLine (&File, &TokenList);
            if ((TokenCount > 1) &&
                (
                    MyStriCmp (TokenList[0], L"ID") ||
                    MyStriCmp (TokenList[0], L"NAME") ||
                    MyStriCmp (TokenList[0], L"DISTRIB_ID")
                )
            ) {
                MergeUniqueWords (OSIconName, TokenList[1], L',');
            }

            FreeTokenLine (&TokenList, &TokenCount);
        } while (TokenCount > 0);
        MY_FREE_POOL(File.Buffer);

    } // if
} // VOID ParseReleaseFile()

// Try to guess the name of the Linux distribution & add that name to
// OSIconName list.
VOID GuessLinuxDistribution (
    CHAR16       **OSIconName,
    REFIT_VOLUME  *Volume,
    CHAR16        *LoaderPath
) {
    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    LOG(5, LOG_LINE_FORENSIC, L"In GuessLinuxDistribution ... 1 - START");

    // If on Linux root fs, /etc/os-release or /etc/lsb-release file probably has clues.
    LOG(5, LOG_LINE_FORENSIC, L"In GuessLinuxDistribution ... 2");
    ParseReleaseFile (OSIconName, Volume, L"etc\\lsb-release");

    LOG(5, LOG_LINE_FORENSIC, L"In GuessLinuxDistribution ... 3");
    ParseReleaseFile (OSIconName, Volume, L"etc\\os-release");

    // Search for clues in the kernel's filename.
    LOG(5, LOG_LINE_FORENSIC, L"In GuessLinuxDistribution ... 4");
    if (StriSubCmp (L".fc", LoaderPath)) {
        LOG(5, LOG_LINE_FORENSIC, L"In GuessLinuxDistribution ... 4a 1");
        MergeStrings (OSIconName, L"fedora", L',');

        LOG(5, LOG_LINE_FORENSIC, L"In GuessLinuxDistribution ... 4a 2");
    }

    LOG(5, LOG_LINE_FORENSIC, L"In GuessLinuxDistribution ... 5");
    if (StriSubCmp (L".el", LoaderPath)) {
        LOG(5, LOG_LINE_FORENSIC, L"In GuessLinuxDistribution ... 5a 1");
        MergeStrings (OSIconName, L"redhat", L',');

        LOG(5, LOG_LINE_FORENSIC, L"In GuessLinuxDistribution ... 5a 2");
    }

    LOG(5, LOG_LINE_FORENSIC, L"In GuessLinuxDistribution ... 6 - END:- VOID");
    LOG(5, LOG_BLANK_LINE_SEP, L"X");
} // VOID GuessLinuxDistribution()

// Add a Linux kernel as a submenu entry for another (pre-existing) Linux kernel entry.
VOID AddKernelToSubmenu (
    LOADER_ENTRY *TargetLoader,
    CHAR16       *FileName,
    REFIT_VOLUME *Volume
) {
    REFIT_FILE          *File;
    CHAR16             **TokenList = NULL, *InitrdName, *SubmenuName = NULL, *VolName = NULL;
    CHAR16              *Path = NULL, *Title, *KernelVersion;
    REFIT_MENU_SCREEN   *SubScreen;
    LOADER_ENTRY        *SubEntry;
    UINTN                TokenCount;

    #if REFIT_DEBUG > 0
    LOG(2, LOG_THREE_STAR_SEP, L"Adding Linux Kernel as SubMenu Entry");
    #endif

    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 1 - START");
    File = ReadLinuxOptionsFile (TargetLoader->LoaderPath, Volume);

    LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2");
    if (File == NULL) {
        LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2a 1");

        #if REFIT_DEBUG > 0
        LOG(3, LOG_THREE_STAR_END, L"ReadLinuxOptionsFile FAILED!!");
        #endif

        LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2a 2");
    }
    else {
        LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 1");
        SubScreen     = TargetLoader->me.SubScreen;

        LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 2");
        InitrdName    = FindInitrd (FileName, Volume);

        LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 3");
        KernelVersion = FindNumbers (FileName);

        LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4");
        while ((TokenCount = ReadTokenLine (File, &TokenList)) > 1) {
            LOG(5, LOG_BLANK_LINE_SEP, L"X");
            LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 1 - START WHILE LOOP");
            ReplaceSubstring (&(TokenList[1]), KERNEL_VERSION, KernelVersion);

            LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 2");
            SubEntry = InitializeLoaderEntry (TargetLoader);

            LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 3");
            // DA-TAG: InitializeLoaderEntry can return NULL
            if (SubEntry != NULL) {
                LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 3a 1");
                SplitPathName (FileName, &VolName, &Path, &SubmenuName);

                LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 3a 2");
                MergeStrings (&SubmenuName, L": ", '\0');

                LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 3a 3");
                MergeStrings (
                    &SubmenuName,
                    TokenList[0] ? StrDuplicate (TokenList[0]) : StrDuplicate (L"Boot Linux"),
                    '\0'
                );

                LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 3a 4");
                MY_FREE_POOL(SubEntry->LoaderPath);
                MY_FREE_POOL(SubEntry->LoadOptions);

                LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 3a 5");
                Title = StrDuplicate (SubmenuName);

                LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 3a 6");
                LimitStringLength (Title, MAX_LINE_LENGTH);

                LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 3a 7");
                SubEntry->me.Title    = Title;

                LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 3a 8");
                SubEntry->LoadOptions = AddInitrdToOptions (TokenList[1], InitrdName);

                LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 3a 9");
                SubEntry->LoaderPath  = StrDuplicate (FileName);

                LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 3a 10");
                CleanUpPathNameSlashes (SubEntry->LoaderPath);

                LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 3a 11");
                SubEntry->Volume = Volume;
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;

                LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 3a 12");
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);

                LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 3a 13");
            }
            LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 4");
            FreeTokenLine (&TokenList, &TokenCount);

            LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 4a 5 - END WHILE LOOP");
            LOG(5, LOG_BLANK_LINE_SEP, L"X");
        } // while

        LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 5");
        FreeTokenLine (&TokenList, &TokenCount);

        LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 6");
        MY_FREE_POOL(VolName);
        MY_FREE_POOL(Path);
        MY_FREE_POOL(SubmenuName);
        MY_FREE_POOL(InitrdName);
        MY_FREE_POOL(File);
        MY_FREE_POOL(KernelVersion);
        LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 2b 7");
    }
    LOG(5, LOG_LINE_FORENSIC, L"In AddKernelToSubmenu ... 3 - END:- VOID");

    #if REFIT_DEBUG > 0
    LOG(4, LOG_THREE_STAR_MID, L"Added Linux Kernel as SubMenu Entry");
    #endif
} // static VOID AddKernelToSubmenu()

// Returns TRUE if a file with the same name as the original but with
// ".efi.signed" is also present in the same directory. Ubuntu is using
// this filename as a signed version of the original unsigned kernel, and
// there is no point in cluttering the display with two kernels that will
// behave identically on non-SB systems, or when one will fail when SB
// is active.
// CAUTION: *FullName MUST be properly cleaned up (via CleanUpPathNameSlashes())
BOOLEAN HasSignedCounterpart(IN REFIT_VOLUME *Volume, IN CHAR16 *FullName) {
    CHAR16 *NewFile = NULL;
    BOOLEAN retval = FALSE;

    MergeStrings(&NewFile, FullName, 0);
    MergeStrings(&NewFile, L".efi.signed", 0);
    if (NewFile != NULL) {
        if (FileExists(Volume->RootDir, NewFile)) {
            #if REFIT_DEBUG > 0
            LOG(3, LOG_LINE_NORMAL, L"Found signed counterpart to '%s'", FullName);
            #endif

            retval = TRUE;
        }
        MY_FREE_POOL(NewFile);
    } // if

    return retval;
} // BOOLEAN HasSignedCounterpart()
