/*
 * BootMaster/scan.c
 * Code related to scanning for boot loaders
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
 * Modifications for rEFInd Copyright (c) 2012-2023 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2024 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "config.h"
#include "lib.h"
#include "mok.h"
#include "icns.h"
#include "menu.h"
#include "scan.h"
#include "linux.h"
#include "apple.h"
#include "install.h"
#include "screenmgt.h"
#include "mystrings.h"
#include "launch_efi.h"
#include "launch_legacy.h"
#include "driver_support.h"
#include "security_policy.h"
#include "../include/refit_call_wrapper.h"

#define IPXE_NAME          L"\\efi\\tools\\ipxe.efi"
#define IPXE_DISCOVER_NAME L"\\efi\\tools\\ipxe_discover.efi"


EFI_GUID GlobalGuid = EFI_GLOBAL_VARIABLE;

#if REFIT_DEBUG > 0
static CHAR16  *Spacer   = L"                          ";

BOOLEAN  LogNewLine      = FALSE;
#endif

BOOLEAN  HasMacOS        = FALSE;
BOOLEAN  DisplayLoader   = FALSE;
BOOLEAN  ScanningLoaders = FALSE;

// Structure used to hold boot loader filenames and time stamps
// in a linked list ... For sorting entries within a directory.
struct
LOADER_LIST {
    CHAR16              *FileName;
    EFI_TIME             TimeStamp;
    struct LOADER_LIST  *NextEntry;
};


static
BOOLEAN IsInstallerMac (
    REFIT_VOLUME *Volume
) {
    BOOLEAN MacInstaller;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;

    MY_MUTELOGGER_SET;
    #endif
    MacInstaller = (
        FindSubStr (Volume->VolName, L"Install Mac OS") ||
        FindSubStr (Volume->VolName, L"Install macOS")  ||
        FindSubStr (Volume->VolName, L"Install OS X")   ||
        FindSubStr (Volume->VolName, L"OS X Install")   ||
        FindSubStr (Volume->VolName, L"macOS Install")  ||
        FindSubStr (Volume->VolName, L"Mac OS Install")
    );
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    return MacInstaller;
} // static BOOLEAN IsInstallerMac()

static
VOID VetCSR (VOID) {
    EFI_STATUS  Status;
    UINTN       CsrLength;
    UINT32     *ReturnValue;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;

    MY_MUTELOGGER_SET;
    #endif
    // Check the NVRAM for previously set values ... Expecting an error
    Status = EfivarGetRaw (
        &AppleBootGuid, L"csr-active-config",
        (VOID **) &ReturnValue, &CsrLength
    );
    if (!EFI_ERROR(Status)) {
        // No Error ... Clear the Variable (Basically Enable SIP/SSV)
        EfivarSetRaw (
            &AppleBootGuid, L"csr-active-config",
            NULL, 0, TRUE
        );
    }
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif
} // static VOID VetCSR()

static
CHAR16 * GetShowName (
    CHAR16 *LinuxName
) {
    CHAR16 *ShowName;


    // DA-TAG: *DO NOT* Free 'ShowName'
    if (MyStriCmp (LinuxName, L"LinuxMint")) {
        ShowName = L"Mint";
    }
    else if (MyStriCmp (LinuxName, L"Zorin")) {
        ShowName = L"ZorinOS";
    }
    else if (MyStriCmp (LinuxName, L"Elementary")) {
        ShowName = L"ElementaryOS";
    }
    else {
        ShowName = LinuxName;
    }

    return ShowName;
} // static CHAR16 * GetShowName()

// Creates a copy of a menu screen.
// Returns a pointer to the copy of the menu screen.
REFIT_MENU_SCREEN * CopyMenuScreen (
    REFIT_MENU_SCREEN *Entry
) {
    UINTN              i;
    REFIT_MENU_SCREEN *NewEntry;


    if (Entry == NULL) {
        return NULL;
    }

    NewEntry = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (NewEntry == NULL) {
        return NULL;
    }

    NewEntry->Title = (Entry->Title != NULL)
        ? StrDuplicate (Entry->Title) : NULL;

    if (Entry->TitleImage != NULL) {
        NewEntry->TitleImage = egCopyImage (Entry->TitleImage);
    }

    NewEntry->InfoLineCount = Entry->InfoLineCount;
    if (NewEntry->InfoLineCount > 0) {
        NewEntry->InfoLines = (CHAR16 **) AllocateZeroPool (
            Entry->InfoLineCount * sizeof (CHAR16 *)
        );
        for (i = 0; i < Entry->InfoLineCount && NewEntry->InfoLines; i++) {
            NewEntry->InfoLines[i] = (Entry->InfoLines[i] != NULL)
                ? StrDuplicate (Entry->InfoLines[i]) : NULL;
        } // for
    }

    NewEntry->EntryCount = Entry->EntryCount;
    if (NewEntry->EntryCount > 0) {
        NewEntry->Entries = (REFIT_MENU_ENTRY **) AllocateZeroPool (
            Entry->EntryCount * (sizeof (REFIT_MENU_ENTRY *))
        );
        for (i = 0; i < Entry->EntryCount && NewEntry->Entries; i++) {
            AddMenuEntryCopy (NewEntry, Entry->Entries[i]);
        } // for
    }

    NewEntry->TimeoutSeconds =  Entry->TimeoutSeconds;
    NewEntry->TimeoutText    = (Entry->TimeoutText != NULL)
        ? StrDuplicate (Entry->TimeoutText) : NULL;

    NewEntry->Hint1 = (Entry->Hint1 != NULL) ? StrDuplicate (Entry->Hint1) : NULL;
    NewEntry->Hint2 = (Entry->Hint2 != NULL) ? StrDuplicate (Entry->Hint2) : NULL;

    return NewEntry;
} // REFIT_MENU_SCREEN * CopyMenuScreen()

// Creates a copy of a menu entry. Intended to enable moving a stack-based
// menu entry (such as the ones for the "reboot" and "exit" functions)
// to the heap. This enables easier deletion of the whole set
// of menu entries when re-scanning.
// Returns a pointer to the copy of the menu entry.
REFIT_MENU_ENTRY * CopyMenuEntry (
    REFIT_MENU_ENTRY *Entry
) {
    REFIT_MENU_ENTRY *NewEntry;


    if (Entry == NULL) {
        // Early Return
        return NULL;
    }

    NewEntry = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
    if (NewEntry == NULL) {
        // Early Return
        return NULL;
    }

    NewEntry->Tag            =  Entry->Tag;
    NewEntry->Row            =  Entry->Row;
    NewEntry->ShortcutDigit  =  Entry->ShortcutDigit;
    NewEntry->ShortcutLetter =  Entry->ShortcutLetter;
    NewEntry->Title          = (Entry->Title      != NULL) ? StrDuplicate   (Entry->Title)      : NULL;
    NewEntry->Image          = (Entry->Image      != NULL) ? egCopyImage    (Entry->Image)      : NULL;
    NewEntry->BadgeImage     = (Entry->BadgeImage != NULL) ? egCopyImage    (Entry->BadgeImage) : NULL;
    NewEntry->SubScreen      = (Entry->SubScreen  != NULL) ? CopyMenuScreen (Entry->SubScreen)  : NULL;

    return NewEntry;
} // REFIT_MENU_ENTRY * CopyMenuEntry()

// Convenience function to create a copy of a loader entry.
// Returns a pointer to the copy of the entry.
LOADER_ENTRY * CopyLoaderEntry (
    IN LOADER_ENTRY *Entry
) {
    return InitializeLoaderEntry (Entry);
} // LOADER_ENTRY * CopyLoaderEntry()

// Creates a new LOADER_ENTRY data structure and populates
// it with default values from the specified Entry,
// or NULL values if Entry is unspecified (NULL).
// Returns a pointer to the new data structure,
// or NULL if it could not be allocated
LOADER_ENTRY * InitializeLoaderEntry (
    IN LOADER_ENTRY *Entry
) {
    LOADER_ENTRY *NewEntry;


    NewEntry = AllocateZeroPool (sizeof (LOADER_ENTRY));
    if (NewEntry == NULL) {
        // Early Return
        return NULL;
    }

    NewEntry->Enabled             =       TRUE;
    NewEntry->UseGraphicsMode     =      FALSE;
    NewEntry->EfiLoaderPath       =       NULL;
    NewEntry->me.Title            =       NULL;
    NewEntry->me.Tag              = TAG_LOADER;
    NewEntry->OSType              =          0;
    NewEntry->EfiBootNum          =          0;

    if (Entry == NULL) {
        NewEntry->Volume          =       NULL;
        NewEntry->LoaderPath      =       NULL;
        NewEntry->LoadOptions     =       NULL;
        NewEntry->InitrdPath      =       NULL;
        NewEntry->EfiLoaderPath   =       NULL;
    }
    else {
        NewEntry->EfiBootNum      =  Entry->EfiBootNum;
        NewEntry->UseGraphicsMode =  Entry->UseGraphicsMode;
        NewEntry->Volume          = (Entry->Volume        != NULL) ?                      Entry->Volume         : NULL;
        NewEntry->LoaderPath      = (Entry->LoaderPath    != NULL) ? StrDuplicate        (Entry->LoaderPath)    : NULL;
        NewEntry->InitrdPath      = (Entry->InitrdPath    != NULL) ? StrDuplicate        (Entry->InitrdPath)    : NULL;
        NewEntry->LoadOptions     = (Entry->LoadOptions   != NULL) ? StrDuplicate        (Entry->LoadOptions)   : NULL;
        NewEntry->EfiLoaderPath   = (Entry->EfiLoaderPath != NULL) ? DuplicateDevicePath (Entry->EfiLoaderPath) : NULL;
    }

    return NewEntry;
} // LOADER_ENTRY *InitializeLoaderEntry()

// Prepare a REFIT_MENU_SCREEN data structure for a subscreen entry. This sets up
// the default entry that launches the boot loader using the same options as the
// Main Entry. Subsequent options can be added by the calling function.
// If a subscreen already exists in the Entry passed to this function,
// it is left unchanged and a pointer to it is returned.
// Returns a pointer to the new subscreen data structure,
// or NULL if there were problems with allocating memory.
REFIT_MENU_SCREEN * InitializeSubScreen (
    IN LOADER_ENTRY *Entry
) {
    UINTN                   i;
    CHAR16                 *NameOS;
    CHAR16                 *TmpStr;
    CHAR16                 *TmpName;
    CHAR16                 *FileName;
    CHAR16                 *ShowName;
    CHAR16                 *LinuxName;
    CHAR16                 *SearchName;
    CHAR16                 *DisplayName;
    BOOLEAN                 CheckFlag;
    BOOLEAN                 Found;
    LOADER_ENTRY           *SubEntry;
    REFIT_MENU_SCREEN      *SubScreen;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif


    if (Entry->me.SubScreen != NULL) {
        // Existing subscreen ... Early Return ... Add new entry later.
        return CopyMenuScreen (Entry->me.SubScreen);
    }

    // No subscreen yet ... Initialize default entry
    SubScreen = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (SubScreen == NULL) {
        // Early Return
        return NULL;
    }

    if (GlobalConfig.SyncAPFS                           &&
        Entry->Volume->FSType  == FS_TYPE_APFS          &&
        Entry->Volume->VolRole == APFS_VOLUME_ROLE_PREBOOT
    ) {
        DisplayName = GetVolumeGroupName (
            Entry->LoaderPath,
            Entry->Volume
        );
    }
    else {
        DisplayName = NULL;
    }

    FileName = (Entry->LoaderPath != NULL) ? Basename (Entry->LoaderPath) : NULL;
    TmpStr   = (Entry->Title      != NULL) ? Entry->Title                 : FileName;
    TmpName  = (DisplayName       != NULL) ? DisplayName                  : Entry->Volume->VolName;
    SubScreen->Title = PoolPrint (
        L"Boot Options for %s%s%s%s%s",
        TmpStr,
        SetVolJoin (TmpStr, TRUE                          ),
        SetVolKind (TmpStr, TmpName, Entry->Volume->FSType),
        SetVolFlag (TmpStr, TmpName                       ),
        SetVolType (TmpStr, TmpName, Entry->Volume->FSType)
    );

    do {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"Build SubScreen:- '%s'",
            SubScreen->Title
        );
        #endif

        SubScreen->TitleImage = egCopyImage (Entry->me.Image);

        // Default entry
        SubEntry = CopyLoaderEntry (Entry);
        if (SubEntry == NULL) {
            break;
        }

        NameOS = NULL;

        // DA-TAG: Do not free 'NameOS'
        if (0);
        else if (Entry->OSType == 'M') {
            NameOS = (IsInstallerMac (Entry->Volume))
                ? L"Instance: macOS Installer"
                : L"Instance: macOS";
        }
        else if (Entry->OSType == 'W') {
            if (MyStrStr (SubScreen->Title, L"Legacy")) {
                NameOS = (MyStrStr (SubScreen->Title, L"Legacy -"))
                    ? L"Instance: Windows (Legacy - NT/XP)"
                    : L"Instance: Windows (Legacy)";
            }
            else if (MyStrStr (SubScreen->Title, L"UEFI")) {
                NameOS = L"Instance: Windows (UEFI)";
            }
            else {
                NameOS = L"Instance: Windows";
            }
        }
        else if (
            Entry->OSType == 'L' ||
            Entry->OSType == 'E' ||
            Entry->OSType == 'G'
        ) {
            Found = FALSE;

            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_SET;
            #endif
            CheckFlag = (
                GlobalConfig.HelpScan                      &&
                !FindSubStr (SubScreen->Title, L"vmlinuz") &&
                !FindSubStr (SubScreen->Title, L"bzImage") &&
                !FindSubStr (SubScreen->Title, L"Manual Stanza:")
            );
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_OFF;
            #endif

            if (CheckFlag) {
                i = 0;
                while (
                    !Found &&
                    (LinuxName = FindCommaDelimited (MAIN_LINUX_DISTROS, i++)) != NULL
                ) {
                    // DA-TAG: *DO NOT* Free 'ShowName'
                    ShowName = GetShowName (LinuxName);

                    SearchName = PoolPrint (L"- %s", ShowName);
                    if (FindSubStr (SubScreen->Title, SearchName)) {
                        MY_FREE_POOL(DisplayName);

                        if (Entry->OSType == 'L') {
                            DisplayName = PoolPrint (
                                L"Instance: Linux - %s%s",
                                ShowName,
                                (FindSubStr (Entry->LoaderPath, L"SystemD"))
                                    ? L" via SystemdBoot" : L""
                            );
                        }
                        else if (Entry->OSType == 'G') {
                            DisplayName = PoolPrint (
                                L"Instance: Linux - %s via Grub", ShowName
                            );
                        }
                        else {
                            DisplayName = PoolPrint (
                                L"Instance: Linux - %s via Elilo", ShowName
                            );
                        }

                        NameOS = DisplayName;
                        Found  =        TRUE;
                    }
                    MY_FREE_POOL(LinuxName);
                    MY_FREE_POOL(SearchName);
                } // while
            }

            if (!Found) {
                if (Entry->OSType == 'L') {
                    NameOS = L"Instance: Linux";
                }
                else if (Entry->OSType == 'G') {
                    NameOS = L"Instance: Grub";
                }
                else {
                    NameOS = L"Instance: Elilo";
                }
            }
        }
        else if (Entry->OSType == 'R'                    ) NameOS = L"Instance: rEFit Variant";
        else if (MyStrStr (SubScreen->Title, L"OpenCore")) NameOS = L"Instance: OpenCore"     ;
        else if (MyStrStr (SubScreen->Title, L"Clover"  )) NameOS = L"Instance: Clover"       ;

        if (NameOS == NULL) {
            NameOS = Entry->Title;
        }

        SubEntry->me.Title = PoolPrint (
            L"Load %s with Default Options",
            NameOS
        );

        if (SubEntry->InitrdPath) {
            SubEntry->LoadOptions = AddInitrdToOptions (
                SubEntry->LoadOptions,
                SubEntry->InitrdPath
            );
        }

        AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
    } while (0); // This 'loop' only runs once

    SubScreen->Hint1 = StrDuplicate (SUBSCREEN_HINT1);
    SubScreen->Hint2 = StrDuplicate (
        (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR)
            ? SUBSCREEN_HINT2_NO_EDITOR : SUBSCREEN_HINT2
    );

    MY_FREE_POOL(FileName);
    MY_FREE_POOL(DisplayName);

    return SubScreen;
} // REFIT_MENU_SCREEN *InitializeSubScreen()

VOID GenerateSubScreen (
    IN OUT LOADER_ENTRY     *Entry,
    IN     REFIT_VOLUME     *Volume,
    IN     BOOLEAN           GenerateReturn
) {
    UINTN                    i;
    UINTN                    TokenCount;
    CHAR16                  *InitrdName;
    CHAR16                  *KernelVersion;
    CHAR16                 **TokenList;
    BOOLEAN                  UseSystemAPFS;
    REFIT_FILE              *File;
    LOADER_ENTRY            *SubEntry;
    REFIT_VOLUME            *DiagnosticsVolume;
    REFIT_MENU_SCREEN       *SubScreen;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - MAIN START", __func__);

    // Create the submenu
    if (StrLen (Entry->Title) == 0) {
        MY_FREE_POOL(Entry->Title);
    }

    SubScreen = InitializeSubScreen (Entry);

    // InitializeSubScreen cannot return NULL; but guard against this regardless
    if (SubScreen != NULL) {
        // Loader specific submenu entries
        if (Entry->OSType == 'M') {        // Entries for macOS
            LOG_SEP(L"X");
            BREAD_CRUMB(L"%a:  A1 - OSType M:- START", __func__);

            #if defined (EFIX64)
            SubEntry = CopyLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Instance: macOS with a 64-bit Kernel");
                SubEntry->LoadOptions     = StrDuplicate (L"arch=x86_64");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }
            #endif

            #if defined(EFIX64) || defined(EFI32)
            SubEntry = CopyLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Instance: macOS with a 32-bit Kernel");
                SubEntry->LoadOptions     = StrDuplicate (L"arch=i386");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }
            #endif

            SubEntry = CopyLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Instance: macOS in Verbose Mode");
                SubEntry->LoadOptions     = StrDuplicate (L"-v");
                SubEntry->UseGraphicsMode = FALSE;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            #if defined (EFIX64)
            SubEntry = CopyLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Instance: macOS in Verbose Mode (64-bit)");
                SubEntry->LoadOptions     = StrDuplicate (L"-v arch=x86_64");
                SubEntry->UseGraphicsMode = FALSE;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }
            #endif

            #if defined(EFIX64) || defined(EFI32)
            SubEntry = CopyLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Instance: macOS in Verbose Mode (32-bit)");
                SubEntry->LoadOptions     = StrDuplicate (L"-v arch=i386");
                SubEntry->UseGraphicsMode = FALSE;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }
            #endif

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_SAFEMODE)) {
                SubEntry = CopyLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    SubEntry->me.Title        = StrDuplicate (L"Load Instance: macOS in Safe Mode (Laconic)");
                    SubEntry->LoadOptions     = StrDuplicate (L"-x");
                    SubEntry->UseGraphicsMode = FALSE;
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                }
                SubEntry = CopyLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    SubEntry->me.Title        = StrDuplicate (L"Load Instance: macOS in Safe Mode (Verbose)");
                    SubEntry->LoadOptions     = StrDuplicate (L"-v -x");
                    SubEntry->UseGraphicsMode = FALSE;
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                }
            } // if !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_SAFEMODE)

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_SINGLEUSER)) {
                SubEntry = CopyLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    SubEntry->me.Title        = StrDuplicate (L"Load Instance: macOS in Single User Mode (Laconic)");
                    SubEntry->LoadOptions     = StrDuplicate (L"-s");
                    SubEntry->UseGraphicsMode = FALSE;
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                }
                SubEntry = CopyLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    SubEntry->me.Title        = StrDuplicate (L"Load Instance: macOS in Single User Mode (Verbose)");
                    SubEntry->LoadOptions     = StrDuplicate (L"-v -s");
                    SubEntry->UseGraphicsMode = FALSE;
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                }
            } // if !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_SINGLEUSER)

            // Check for Apple hardware diagnostics
            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HWTEST)) {
                UseSystemAPFS = FALSE;
                // DA-TAG: Investigate This
                //         Is SingleAPFS really needed?
                //         Play safe and add it for now.
                //         SingleAPFS is TRUE by default.
                //         Including when no APFS present.
                if (SingleAPFS) {
                    if (GlobalConfig.SyncAPFS                    &&
                        Volume->FSType  == FS_TYPE_APFS          &&
                        Volume->VolRole == APFS_VOLUME_ROLE_PREBOOT
                    ) {
                        for (i = 0; i < SystemVolumesCount; i++) {
                            if (GuidsAreEqual (
                                &(SystemVolumes[i]->PartGuid),
                                &(Volume->PartGuid))
                            ) {
                                UseSystemAPFS = TRUE;
                                break;
                            }
                        } // for
                    }
                }

                DiagnosticsVolume = (UseSystemAPFS) ? SystemVolumes[i] : Volume;
                if (FileExists (DiagnosticsVolume->RootDir, MACOSX_DIAGNOSTICS)) {
                    SubEntry = CopyLoaderEntry (Entry);
                    if (SubEntry != NULL) {
                        MY_FREE_POOL(SubEntry->LoaderPath);
                        SubEntry->Volume          = DiagnosticsVolume;
                        SubEntry->me.Title        = StrDuplicate (L"Run Apple Hardware Test");
                        SubEntry->LoaderPath      = StrDuplicate (MACOSX_DIAGNOSTICS);
                        SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
                        AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                    }
                }
            } // if !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HWTEST)
            BREAD_CRUMB(L"%a:  A2 - OSType M:- END", __func__);
        }
        else if (Entry->OSType == 'L') {   // Entries for Linux kernels with EFI stub loaders
            LOG_SEP(L"X");
            BREAD_CRUMB(L"%a:  A1 - OSType L:- START", __func__);
            File = ReadLinuxOptionsFile (Entry->LoaderPath, Volume);

            BREAD_CRUMB(L"%a:  2", __func__);
            if (File != NULL) {
                BREAD_CRUMB(L"%a:  2a 1", __func__);
                KernelVersion = FindNumbers (Entry->LoaderPath);

                BREAD_CRUMB(L"%a:  2a 2", __func__);
                TokenCount = ReadTokenLine (File, &TokenList);

                BREAD_CRUMB(L"%a:  2a 3", __func__);
                if (TokenCount >= 2) {
                    BREAD_CRUMB(L"%a:  2a 3a 1", __func__);
                    ReplaceSubstring (
                        &(TokenList[1]), KERNEL_VERSION, KernelVersion
                    );
                }

                BREAD_CRUMB(L"%a:  2a 4", __func__);
                // First entry requires special processing, since it was initially set
                // up with a default title but correct options by InitializeSubScreen(),
                // earlier.
                if (TokenCount > 1                &&
                    SubScreen->Entries    != NULL &&
                    SubScreen->Entries[0] != NULL
                ) {
                    BREAD_CRUMB(L"%a:  2a 4a 1", __func__);
                    MY_FREE_POOL(SubScreen->Entries[0]->Title);
                    SubScreen->Entries[0]->Title = StrDuplicate (
                        (TokenList[0] != NULL)
                            ? TokenList[0] : L"Load Instance: Linux"
                    );
                }
                BREAD_CRUMB(L"%a:  2a 5", __func__);
                FreeTokenLine (&TokenList, &TokenCount);

                BREAD_CRUMB(L"%a:  2a 6", __func__);
                InitrdName = FindInitrd (Entry->LoaderPath, Volume);

                BREAD_CRUMB(L"%a:  2a 7", __func__);
                while ((TokenCount = ReadTokenLine (File, &TokenList)) > 1) {
                    LOG_SEP(L"X");
                    BREAD_CRUMB(L"%a:  2a 7a 1 - WHILE LOOP:- START", __func__);
                    ReplaceSubstring (
                        &(TokenList[1]), KERNEL_VERSION, KernelVersion
                    );

                    BREAD_CRUMB(L"%a:  2a 7a 2", __func__);
                    SubEntry = CopyLoaderEntry (Entry);

                    BREAD_CRUMB(L"%a:  2a 7a 3", __func__);
                    if (SubEntry != NULL) {
                        BREAD_CRUMB(L"%a:  2a 7a 3a 1", __func__);
                        SubEntry->me.Title = StrDuplicate (
                            (TokenList[0] != NULL)
                                ? TokenList[0] : L"Load Instance: Linux"
                        );

                        BREAD_CRUMB(L"%a:  2a 7a 3a 2", __func__);
                        MY_FREE_POOL(SubEntry->LoadOptions);

                        BREAD_CRUMB(L"%a:  2a 7a 3a 3", __func__);
                        SubEntry->LoadOptions = AddInitrdToOptions (
                            TokenList[1], InitrdName
                        );

                        BREAD_CRUMB(L"%a:  2a 7a 3a 4", __func__);
                        SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;

                        BREAD_CRUMB(L"%a:  2a 7a 3a 5", __func__);
                        AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                    }

                    BREAD_CRUMB(L"%a:  2a 7a 4", __func__);
                    FreeTokenLine (&TokenList, &TokenCount);

                    BREAD_CRUMB(L"%a:  2a 7a 5 - WHILE LOOP:- END", __func__);
                    LOG_SEP(L"X");
                } // while
                BREAD_CRUMB(L"%a:  2a 8", __func__);
                FreeTokenLine (&TokenList, &TokenCount);

                BREAD_CRUMB(L"%a:  2a 9", __func__);
                MY_FREE_POOL(KernelVersion);
                MY_FREE_POOL(InitrdName);
                MY_FREE_FILE(File);
            } // if File

            BREAD_CRUMB(L"%a:  3 - OSType L:- END", __func__);
        }
        else if (Entry->OSType == 'E') {   // Entries for ELILO
            LOG_SEP(L"X");
            BREAD_CRUMB(L"%a:  A1 - OSType E:- START", __func__);
            SubEntry = CopyLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Instance: ELILO in Interactive Mode");
                SubEntry->LoadOptions     = StrDuplicate (L"-p");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = CopyLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Instance: Linux for a 17\" iMac or a 15\" MacBook Pro (*)");
                SubEntry->LoadOptions     = StrDuplicate (L"-d 0 i17");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = CopyLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Instance: Linux for a 20\" iMac (*)");
                SubEntry->LoadOptions     = StrDuplicate (L"-d 0 i20");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = CopyLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Instance: Linux for a Mac Mini (*)");
                SubEntry->LoadOptions     = StrDuplicate (L"-d 0 mini");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            AddMenuInfoLine (SubScreen, L"NOTE: This is an example",             FALSE);
            AddMenuInfoLine (SubScreen, L"Entries marked with (*) may not work", FALSE);

            BREAD_CRUMB(L"%a:  A2 - OSType E:- END", __func__);
        }
        else if (Entry->OSType == 'X') {   // Entries for xom.efi
            LOG_SEP(L"X");
            BREAD_CRUMB(L"%a:  A1 - OSType X:- START", __func__);
            // Skip the built-in selection and boot from hard disk only by default
            Entry->LoadOptions = L"-s -h";

            SubEntry = CopyLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Instance: Windows on Hard Disk");
                SubEntry->LoadOptions     = StrDuplicate (L"-s -h");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = CopyLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Instance: Windows on Optical Disc");
                SubEntry->LoadOptions     = StrDuplicate (L"-s -c");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = CopyLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Instance: XOM in Text Mode");
                SubEntry->LoadOptions     = StrDuplicate (L"-v");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }
            BREAD_CRUMB(L"%a:  A2 - OSType X:- END", __func__);
        } // Entries for xom.efi

        BREAD_CRUMB(L"%a:  B", __func__);
        do {
            LOG_SEP(L"X");
            BREAD_CRUMB(L"%a:  B 1", __func__);
            if (GenerateReturn) {
                BREAD_CRUMB(L"%a:  B 1a 1", __func__);
                if (!GetMenuEntryReturn (&SubScreen)) {
                    BREAD_CRUMB(L"%a:  B 1a 1a 1 - Resource Exhaustion!!", __func__);
                    FreeMenuScreen (&SubScreen);
                    BREAD_CRUMB(L"%a:  B 1a 1a 2", __func__);

                    break;
                }
                BREAD_CRUMB(L"%a:  B 1a 2", __func__);
            }
            BREAD_CRUMB(L"%a:  B 2 - END", __func__);
            Entry->me.SubScreen = SubScreen;
        } while (0); // This 'loop' only runs once

        BREAD_CRUMB(L"%a:  A MAIN END - VOID", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
    }
} // VOID GenerateSubScreen()

// Sets a few defaults for a loader entry -- mainly the icon,
// but also the OS type code and shortcut letter.
// For Linux EFI stub loaders, also sets kernel options
// that will (with luck) work fairly automatically.
VOID SetLoaderDefaults (
    IN LOADER_ENTRY *Entry,
    IN CHAR16       *LoaderPath,
    IN REFIT_VOLUME *Volume
) {
    UINTN                   i;
    CHAR16                 *Temp;
    CHAR16                 *PathOnly;
    CHAR16                 *NameClues;
    CHAR16                 *OSIconName;
    CHAR16                 *TmpIconName;
    CHAR16                 *ThisIconName;
    CHAR16                 *TargetName; // *DO NOT* Free
    CHAR16                 *DisplayName;
    CHAR16                 *NoExtension;
    CHAR16                 *VentoyName;
    CHAR16                  ShortcutLetter;
    BOOLEAN                 GotFlag;
    BOOLEAN                 MacFlag;
    BOOLEAN                 GotUEFI;
    BOOLEAN                 GetImage;
    BOOLEAN                 VetVolIcon;
    BOOLEAN                 FoundVentoy;
    BOOLEAN                 MergeFsName;


    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Set Loader Defaults");
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);


    BREAD_CRUMB(L"%a:  2", __func__);
    if (LoaderPath == NULL) {
        NameClues = NULL;
        PathOnly  = NULL;
    }
    else {
        NameClues = Basename (LoaderPath);
        PathOnly  = FindPath (LoaderPath);
    }

    BREAD_CRUMB(L"%a:  3", __func__);
    ShortcutLetter =     0;
    TargetName     =  NULL;
    OSIconName     =  NULL;
    FoundVentoy    = FALSE;
    if (AllowGraphicsMode) {
        ThisIconName              =  NULL;
        VetVolIcon   = MacFlag    = FALSE;
        GotFlag      = GotUEFI    = FALSE;

        BREAD_CRUMB(L"%a:  3a 1", __func__);
        if (Volume->DiskKind == DISK_KIND_NET) {
            BREAD_CRUMB(L"%a:  3a 1a 1", __func__);
            MergeStrings (&NameClues, Entry->me.Title, L' ');
        }
        else {
            BREAD_CRUMB(L"%a:  3a 1b 0", __func__);
            if (Entry->me.Image == NULL) {
                BREAD_CRUMB(L"%a:  3a 1b 0a 1", __func__);
                LoadVolumeBadgeIcon (&Volume);
                BREAD_CRUMB(L"%a:  3a 1b 0a 2", __func__);
                if (Volume->VolBadgeImage != NULL) {
                    BREAD_CRUMB(L"%a:  3a 1b 0a 2a 1", __func__);
                    Entry->me.BadgeImage = egCopyImage (Volume->VolBadgeImage);
                    BREAD_CRUMB(L"%a:  3a 1b 0a 2a 2", __func__);
                }
                BREAD_CRUMB(L"%a:  3a 1b 0a 3", __func__);
            }

            BREAD_CRUMB(L"%a:  3a 1b 1", __func__);
            if (Entry->me.Image == NULL        &&
                GlobalConfig.HiddenIconsPrefer &&
                !GlobalConfig.HiddenIconsIgnore
            ) {
                BREAD_CRUMB(L"%a:  3a 1b 1a 1", __func__);
                if (Volume->VolIconImage == NULL) {
                    BREAD_CRUMB(L"%a:  3a 1b 1a 1a 1", __func__);
                    LoadVolumeIcon (Volume);
                    VetVolIcon = TRUE;
                    BREAD_CRUMB(L"%a:  3a 1b 1a 1a 2", __func__);
                }
                BREAD_CRUMB(L"%a:  3a 1b 1a 2", __func__);
                if (Volume->VolIconImage != NULL) {
                    // Use ".VolumeIcon" image icon for loader
                    // Takes precedence all over options
                    BREAD_CRUMB(L"%a:  3a 1b 1a 2a 1", __func__);
                    Entry->me.Image = egCopyImage (Volume->VolIconImage);
                    BREAD_CRUMB(L"%a:  3a 1b 1a 2a 2", __func__);
                }
                BREAD_CRUMB(L"%a:  3a 1b 1a 3", __func__);
            }

            BREAD_CRUMB(L"%a:  3a 1b 2", __func__);
            if (Entry->me.Image == NULL) {
                BREAD_CRUMB(L"%a:  3a 1b 2a 1", __func__);
                #if REFIT_DEBUG > 0
                if (!GlobalConfig.HiddenIconsIgnore &&
                    GlobalConfig.HiddenIconsPrefer
                ) {
                    ALT_LOG(1, LOG_LINE_NORMAL,
                        L"Could *NOT* Find '.VolumeIcon' Image"
                    );
                }
                #endif

                BREAD_CRUMB(L"%a:  3a 1b 2a 2", __func__);
                if (LoaderPath != NULL &&
                    FindSubStr (LoaderPath, L"System\\Library\\CoreServices")
                ) {
                    BREAD_CRUMB(L"%a:  3a 1b 2a 2a 1", __func__);
                    MacFlag = TRUE;
                }

                BREAD_CRUMB(L"%a:  3a 1b 2a 3", __func__);
                if (GlobalConfig.HelpIcon) {
                    BREAD_CRUMB(L"%a:  3a 1b 2a 3a 1", __func__);
                    #if REFIT_DEBUG > 0
                    ALT_LOG(1, LOG_THREE_STAR_MID,
                        L"Skip Bootloader Directory Icon Search ... Config Setting *IS NOT* Active:- 'decline_help_icon'"
                    );
                    #endif
                }
                else {
                    BREAD_CRUMB(L"%a:  3a 1b 2a 3b 0", __func__);
                    if (NameClues == NULL) {
                        BREAD_CRUMB(L"%a:  3a 1b 2a 3b 0a 1", __func__);
                        NoExtension = NULL;
                    }
                    else {
                        BREAD_CRUMB(L"%a:  3a 1b 2a 3b 0b 1", __func__);
                        NoExtension = StripEfiExtension (NameClues);
                    }

                    BREAD_CRUMB(L"%a:  3a 1b 2a 3b 1", __func__);
                    if (NoExtension != NULL) {
                        // Locate custom icon for loader
                        // Takes precedence over the "hints" in OSIconName variable
                        #if REFIT_DEBUG > 0
                        ALT_LOG(1, LOG_LINE_NORMAL, L"Search for Icon in Bootloader Directory");
                        #endif

                        BREAD_CRUMB(L"%a:  3a 1b 2a 3b 1a 1", __func__);
                        Entry->me.Image = egLoadIconAnyType (
                            Volume->RootDir,
                            PathOnly, NoExtension,
                            GlobalConfig.IconSizes[ICON_SIZE_BIG]
                        );

                        BREAD_CRUMB(L"%a:  3a 1b 2a 3b 1a 2", __func__);
                        if (Entry->me.Image == NULL &&
                            !GlobalConfig.HiddenIconsIgnore
                        ) {
                            BREAD_CRUMB(L"%a:  3a 1b 2a 3b 1a 2a 1", __func__);
                            if (Volume->VolIconImage == NULL) {
                                BREAD_CRUMB(L"%a:  3a 1b 2a 3b 1a 2a 1a 1", __func__);
                                if (!VetVolIcon) {
                                    BREAD_CRUMB(L"%a:  3a 1b 2a 3b 1a 2a 1a 1a 1", __func__);
                                    LoadVolumeIcon (Volume);
                                }
                                BREAD_CRUMB(L"%a:  3a 1b 2a 3b 1a 2a 1a 2", __func__);
                            }

                            BREAD_CRUMB(L"%a:  3a 1b 2a 3b 1a 2a 2", __func__);
                            if (Volume->VolIconImage != NULL) {
                                // Use ".VolumeIcon" image icon for loader
                                // no other icon type in bootloader directory
                                BREAD_CRUMB(L"%a:  3a 1b 2a 3b 1a 2a 2a 1", __func__);
                                Entry->me.Image = egCopyImage (Volume->VolIconImage);
                            }
                            BREAD_CRUMB(L"%a:  3a 1b 2a 3b 1a 2a 3", __func__);
                        }

                        MY_FREE_POOL(NoExtension);
                        BREAD_CRUMB(L"%a:  3a 1b 2a 3b 1a 3", __func__);
                    }  // if NoExtension != NULL
                    BREAD_CRUMB(L"%a:  3a 1b 2a 3b 2", __func__);
                } // if/else GlobalConfig.HelpIcon
                BREAD_CRUMB(L"%a:  3a 1b 2a 4", __func__);
            } // if Entry->me.Image == NULL

            // Begin creating icon "hints" from last part of loader dir path
            BREAD_CRUMB(L"%a:  3a 1b 3", __func__);
            if (Entry->me.Image != NULL) {
                BREAD_CRUMB(L"%a:  3a 1b 3a 1", __func__);
                GotFlag = TRUE;
            }
            else {
                #if REFIT_DEBUG > 0
                BREAD_CRUMB(L"%a:  3a 1b 3b 1", __func__);
                if (Entry->me.Image == NULL) {
                    ALT_LOG(1, LOG_LINE_NORMAL,
                        L"Create Icon Hint from Loader Path:- '%s'",
                        LoaderPath
                    );
                }
                #endif

                BREAD_CRUMB(L"%a:  3a 1b 3b 2", __func__);
                OSIconName = FindLastDirName (LoaderPath);

                BREAD_CRUMB(L"%a:  3a 1b 3b 3", __func__);
                if (OSIconName != NULL                  &&
                    GlobalConfig.HelpIcon               &&
                    !MyStriCmp (OSIconName, L"BOOT")    &&
                    !MyStriCmp (OSIconName, L"CoreServices")
                ) {
                    BREAD_CRUMB(L"%a:  3a 1b 3b 3a 1", __func__);
                    ShortcutLetter = OSIconName[0];
                    ThisIconName   = StrDuplicate (OSIconName);
                }
                else {
                    BREAD_CRUMB(L"%a:  3a 1b 3b 3b 1", __func__);
                    if (OSIconName != NULL) {
                        BREAD_CRUMB(L"%a:  3a 1b 3b 3b 1a 1", __func__);
                        MY_FREE_POOL(OSIconName);
                    }
                    ThisIconName = NULL;
                    BREAD_CRUMB(L"%a:  3a 1b 3b 3b 2", __func__);
                }

                BREAD_CRUMB(L"%a:  3a 1b 3b 4", __func__);
                // Add every "word" in the filesystem and partition names, delimited by
                // spaces, dashes (-), underscores (_), or colons (:), to the list of
                // hints to be used in searching for OS icons.
                if (Volume->FsName != NULL &&
                    Volume->FsName[0] != L'\0'
                ) {
                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 1", __func__);
                    if (GlobalConfig.SyncAPFS              &&
                        FindSubStr (Volume->FsName, L"PreBoot")
                    ) {
                        BREAD_CRUMB(L"%a:  3a 1b 3b 4a 1a 1", __func__);
                        MergeFsName = FALSE;
                    }
                    else {
                        BREAD_CRUMB(L"%a:  3a 1b 3b 4a 1b 1", __func__);
                        MergeFsName = TRUE;
                    }

                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2", __func__);
                    if (MergeFsName) {
                        BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1", __func__);
                        if (Entry->me.Image == NULL) {
                            BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 1", __func__);
                            i = 0;
                            while (
                                !FoundVentoy              &&
                                GlobalConfig.HandleVentoy &&
                                (VentoyName = FindCommaDelimited (VENTOY_NAMES, i++)) != NULL
                            ) {
                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 1a 1 - WHILE LOOP:- START ... Check for Ventoy Partition", __func__);
                                if (MyStrBegins (VentoyName, Volume->FsName)) {
                                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 1a 1a 1 - Found ... Set Ventoy Icon", __func__);
                                    FoundVentoy = TRUE;
                                    TargetName  = L"ventoy";
                                }
                                MY_FREE_POOL(VentoyName);
                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 1a 2 - WHILE LOOP:- END", __func__);
                            } // while

                            BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 2", __func__);
                            if (TargetName == NULL) {
                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 2a 1", __func__);
                                if (GlobalConfig.HelpIcon &&
                                    (
                                        (
                                            GuidsAreEqual (&(Volume->PartTypeGuid), &GuidESP)
                                        ) || (
                                            MyStriCmp (Volume->FsName, L"EFI") ||
                                            MyStriCmp (Volume->FsName, L"ESP") ||
                                            MyStriCmp (Volume->FsName, L"EFI System Partition")
                                        )
                                    )
                                ) {
                                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 2a 1a 1", __func__);
                                    GotUEFI = TRUE;
                                }

                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 2a 2", __func__);
                                if (Volume->FsName != NULL) {
                                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 2a 2a 1", __func__);
                                    if (!MyStriCmp (Volume->FsName, L"EFI") &&
                                        !MyStriCmp (Volume->FsName, L"ESP") &&
                                        !MyStriCmp (Volume->FsName, L"EFI System Partition")
                                    ) {
                                        BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 2a 2a 1a 1", __func__);
                                        TargetName = Volume->FsName;
                                    }
                                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 2a 2a 2", __func__);
                                }
                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 2a 3", __func__);
                            } // if TargetName == NULL

                            BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 3", __func__);
                            #if REFIT_DEBUG > 0
                            if (TargetName != NULL) {
                                ALT_LOG(1, LOG_LINE_NORMAL,
                                    L"Merge Hints Based on Name of Filesystem:- '%s'",
                                    TargetName
                                );
                            }
                            #endif

                            BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 4", __func__);
                            if (FoundVentoy) {
                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 4a 1", __func__);
                                MY_FREE_POOL(OSIconName);
                                OSIconName = StrDuplicate (TargetName);
                            }
                            else {
                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 4b 1", __func__);
                                if (GlobalConfig.HelpIcon) {
                                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 4b 1a 1", __func__);
                                    MergeUniqueItems (&OSIconName, TargetName, L',');
                                }

                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 4b 2", __func__);
                                MergeUniqueWords (&OSIconName, TargetName, L',');

                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 4b 3", __func__);
                            }
                            BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 5", __func__);
                        } // if Entry->me.Image == NULL
                        BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 2", __func__);
                    }
                    else {
                        BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1", __func__);
                        if (!FoundVentoy            &&
                            Volume->VolName != NULL &&
                            Volume->VolName[0] != L'\0'
                        ) {
                            BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 1", __func__);
                            DisplayName = NULL;
                            if (GlobalConfig.SyncAPFS                    &&
                                Volume->FSType  == FS_TYPE_APFS          &&
                                Volume->VolRole == APFS_VOLUME_ROLE_PREBOOT
                            ) {
                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 1a 1", __func__);
                                DisplayName = GetVolumeGroupName (Entry->LoaderPath, Volume);
                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 1a 2", __func__);
                            }

                            BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2", __func__);
                            if (Entry->me.Image == NULL) {
                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2a 1", __func__);
                                TargetName = (DisplayName != NULL)
                                    ? DisplayName : Volume->VolName;

                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2a 2", __func__);
                                i = 0;
                                while (
                                    !FoundVentoy              &&
                                    GlobalConfig.HandleVentoy &&
                                    (VentoyName = FindCommaDelimited (VENTOY_NAMES, i++)) != NULL
                                ) {
                                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2a 2a 1 - WHILE LOOP:- START ... Check for Ventoy Volume", __func__);
                                    if (MyStrBegins (VentoyName, TargetName)) {
                                        BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2a 2a 1a 1 - Found Ventoy Volume ... Set Ventoy Icon", __func__);
                                        FoundVentoy =      TRUE;
                                        TargetName  = L"ventoy";
                                    }
                                    MY_FREE_POOL(VentoyName);
                                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2a 2a 2 - WHILE LOOP:- END", __func__);
                                } // while

                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2a 3", __func__);
                                if (GlobalConfig.HelpIcon &&
                                    (
                                        (
                                            GuidsAreEqual (&(Volume->PartTypeGuid), &GuidESP)
                                        ) || (
                                            MyStriCmp (TargetName, L"EFI") ||
                                            MyStriCmp (TargetName, L"ESP") ||
                                            MyStriCmp (TargetName, L"EFI System Partition")
                                        )
                                    )
                                ) {
                                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2a 3a 1", __func__);
                                    GotUEFI = TRUE;
                                }

                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2a 4", __func__);
                                #if REFIT_DEBUG > 0
                                if (TargetName != NULL) {
                                    ALT_LOG(1, LOG_LINE_NORMAL,
                                        L"Merge Hints Based on Name of Volume:- '%s'",
                                        TargetName
                                    );
                                }
                                #endif

                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2a 4", __func__);
                                if (FoundVentoy) {
                                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2a 4a 1", __func__);
                                    MY_FREE_POOL(OSIconName);
                                    OSIconName = StrDuplicate (TargetName);
                                }
                                else {
                                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2a 4b 1", __func__);
                                    if (GlobalConfig.HelpIcon) {
                                        BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2a 1a 4b 1a 1", __func__);
                                        MergeUniqueItems (&OSIconName, TargetName, L',');
                                    }

                                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2a 4b 2", __func__);
                                    MergeUniqueWords (&OSIconName, TargetName, L',');

                                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2a 4b 3", __func__);
                                }
                                BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 2a 5", __func__);
                            } // if Entry->me.Image == NULL
                            BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 1a 3", __func__);
                            MY_FREE_POOL(DisplayName);
                        } // if Volume->VolName
                        BREAD_CRUMB(L"%a:  3a 1b 3b 4a 2b 2", __func__);
                    } // if/else MergeFsName
                    BREAD_CRUMB(L"%a:  3a 1b 3b 4a 3", __func__);
                } // if Volume->FsName

                BREAD_CRUMB(L"%a:  3a 1b 3b 5", __func__);
                if (!FoundVentoy             &&
                    Volume->PartName != NULL &&
                    Volume->PartName[0] != L'\0'
                ) {
                    BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1", __func__);
                    TargetName = NULL;
                    if (Entry->me.Image == NULL) {
                        BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 1", __func__);
                        i = 0;
                        while (
                            !FoundVentoy              &&
                            GlobalConfig.HandleVentoy &&
                            (VentoyName = FindCommaDelimited (VENTOY_NAMES, i++)) != NULL
                        ) {
                            BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 1a 1 - WHILE LOOP:- START ... Check for Ventoy Partition", __func__);
                            if (MyStrBegins (VentoyName, Volume->PartName)) {
                                BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 1a 1a 1 - Found ... Set Ventoy Icon", __func__);
                                FoundVentoy = TRUE;
                                TargetName  = L"ventoy";
                            }
                            MY_FREE_POOL(VentoyName);
                            BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 1a 2 - WHILE LOOP:- END", __func__);
                        } // while

                        BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 2", __func__);
                        if (!FoundVentoy &&
                            !MyStriCmp (Volume->FsName, Volume->PartName)
                        ) {
                            BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 2a 1", __func__);
                            if (GlobalConfig.HelpIcon &&
                                (
                                    (
                                        GuidsAreEqual (&(Volume->PartTypeGuid), &GuidESP)
                                    ) || (
                                        MyStriCmp (Volume->PartName, L"EFI") ||
                                        MyStriCmp (Volume->PartName, L"ESP") ||
                                        MyStriCmp (Volume->PartName, L"EFI System Partition")
                                    )
                                )
                            ) {
                                BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 2a 1a 1", __func__);
                                GotUEFI = TRUE;
                            }

                            BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 2a 2", __func__);
                            if (Volume->PartName != NULL &&
                                !MyStrStr (Volume->PartName, L"Untitled")
                            ) {
                                BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 2a 2a 1", __func__);
                                if (!MyStriCmp (Volume->PartName, L"EFI") &&
                                    !MyStriCmp (Volume->PartName, L"ESP") &&
                                    !MyStriCmp (Volume->PartName, L"EFI System Partition")
                                ) {
                                    BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 2a 2a 1a 1", __func__);
                                    TargetName = Volume->PartName;
                                }
                                BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 2a 2a 2", __func__);
                            }

                            BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 2a 3", __func__);
                            #if REFIT_DEBUG > 0
                            if (TargetName != NULL) {
                                ALT_LOG(1, LOG_LINE_NORMAL,
                                    L"Merge Hints Based on Name of Partition:- '%s'",
                                    TargetName
                                );
                            }
                            #endif

                            BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 2a 4", __func__);
                            if (GlobalConfig.HelpIcon) {
                                BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 2a 4a 1", __func__);
                                MergeUniqueItems (&OSIconName, TargetName, L',');
                                BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 2a 4a 2", __func__);
                            }
                            BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 2a 5", __func__);
                            MergeUniqueWords (&OSIconName, TargetName, L',');
                            BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 2a 6", __func__);
                        }
                        BREAD_CRUMB(L"%a:  3a 1b 3b 5a 1a 3", __func__);
                    }
                    BREAD_CRUMB(L"%a:  3a 1b 3b 5a 2", __func__);
                }
                BREAD_CRUMB(L"%a:  3a 1b 3b 6", __func__);
            } // if/else Entry->me.Image != NULL
            BREAD_CRUMB(L"%a:  3a 1b 4", __func__);
        } // if/else Volume->DiskKind == DISK_KIND_NET

        GetImage = (Entry->me.Image == NULL) ? TRUE : FALSE;
        BREAD_CRUMB(L"%a:  3a 2", __func__);
    } // if AllowGraphicsMode

    BREAD_CRUMB(L"%a:  4", __func__);
    if (!AllowGraphicsMode) {
        BREAD_CRUMB(L"%a:  4a 1", __func__);
        GetImage = FALSE;
    }

    // Detect specific loaders
    BREAD_CRUMB(L"%a:  5", __func__);
    TmpIconName = NULL;
    if (!FoundVentoy) {
        BREAD_CRUMB(L"%a:  5a A", __func__);
        if (FindSubStr (LoaderPath, MACOSX_LOADER_PATH)) {
            BREAD_CRUMB(L"%a:  5a 1", __func__);
            if (FileExists (Volume->RootDir, L"EFI\\RefindPlus\\config.conf") ||
                FileExists (Volume->RootDir, L"EFI\\RefindPlus\\refind.conf") ||
                FileExists (Volume->RootDir, L"EFI\\refind\\config.conf")     ||
                FileExists (Volume->RootDir, L"EFI\\refind\\refind.conf")
            ) {
                BREAD_CRUMB(L"%a:  5a 1a 1", __func__);
                if (GetImage) {
                    MergeUniqueStrings (&TmpIconName, L"refind", L',');
                }

                BREAD_CRUMB(L"%a:  5a 1a 2", __func__);
                Entry->OSType  = 'R';
                ShortcutLetter = 'R';
            }
            else {
                BREAD_CRUMB(L"%a:  5a 1b 1", __func__);
                if (GetImage) {
                    MergeUniqueStrings (&TmpIconName, L"mac", L',');
                }

                BREAD_CRUMB(L"%a:  5a 1b 2", __func__);
                Entry->OSType  = 'M';
                ShortcutLetter = 'M';
                Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
            }
            BREAD_CRUMB(L"%a:  5a 2", __func__);
        }
        else if (
            MyStriCmp (NameClues, L"bootmgfw.efi") ||
            MyStriCmp (NameClues, L"bootmgr.efi")  ||
            MyStriCmp (NameClues, L"cdboot.efi")   ||
            MyStriCmp (NameClues, L"bkpbootmgfw.efi")
        ) {
            BREAD_CRUMB(L"%a:  5b 1", __func__);
            if (GetImage) {
                MergeUniqueStrings (&TmpIconName, L"win,win8", L',');
            }

            BREAD_CRUMB(L"%a:  5b 2", __func__);
            Entry->OSType  = 'W';
            ShortcutLetter = 'W';
            Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
        }
        else if (IsListItemSubstringIn (NameClues, GlobalConfig.LinuxPrefixes)) {
            BREAD_CRUMB(L"%a:  5c 1", __func__);
            if (Volume->DiskKind != DISK_KIND_NET) {
                BREAD_CRUMB(L"%a:  5c 1a 1", __func__);
                GuessLinuxDistribution (&TmpIconName, Volume, LoaderPath);

                BREAD_CRUMB(L"%a:  5c 1a 2", __func__);
                Entry->LoadOptions = GetMainLinuxOptions (LoaderPath, Volume);
            }

            BREAD_CRUMB(L"%a:  5c 2", __func__);
            if (GetImage) {
                MergeUniqueStrings (&TmpIconName, L"linux", L',');
            }
            GotFlag       = TRUE;
            Entry->OSType =  'L';

            BREAD_CRUMB(L"%a:  5c 3", __func__);
            if (ShortcutLetter == 0) {
                BREAD_CRUMB(L"%a:  5c 3a 1", __func__);
                ShortcutLetter = 'L';
            }

            BREAD_CRUMB(L"%a:  5c 4", __func__);
            Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;
        }
        else if (FindSubStr (NameClues, L"grub")) {
            BREAD_CRUMB(L"%a:  5d 1", __func__);
            if (GetImage) {
                MergeUniqueItems (&TmpIconName, L"grub,linux", L',');
            }

            BREAD_CRUMB(L"%a:  5d 2", __func__);
            GotFlag        = TRUE;
            Entry->OSType  =  'G';
            ShortcutLetter =  'G';
            Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_GRUB;
        }
        else if (MyStriCmp (NameClues, L"opencore")) {
            BREAD_CRUMB(L"%a:  5e 1", __func__);
            if (GetImage) {
                MergeUniqueStrings (&TmpIconName, L"opencore", L',');
            }

            BREAD_CRUMB(L"%a:  5e 2", __func__);
            GotFlag = TRUE;
            Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OPENCORE;
        }
        else if (MyStriCmp (NameClues, L"clover")) {
            BREAD_CRUMB(L"%a:  5f 1", __func__);
            if (GetImage) {
                MergeUniqueStrings (&TmpIconName, L"clover", L',');
            }

            BREAD_CRUMB(L"%a:  5f 2", __func__);
            GotFlag = TRUE;
            Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_CLOVER;
        }
        else if (FindSubStr (LoaderPath, L"refindplus")) {
            BREAD_CRUMB(L"%a:  5g 1", __func__);
            if (GetImage) {
                MergeUniqueItems (
                    &TmpIconName, L"refindplus,refind,refit", L','
                );
            }

            BREAD_CRUMB(L"%a:  5g 2", __func__);
            GotFlag        = TRUE;
            Entry->OSType  =  'R';
            ShortcutLetter =  'R';
        }
        else if (FindSubStr (LoaderPath, L"refind")) {
            BREAD_CRUMB(L"%a:  5h 1 - A", __func__);
            if (GetImage) {
                MergeUniqueItems (&TmpIconName, L"refind,refit", L',');
            }

            BREAD_CRUMB(L"%a:  5h 2 - A", __func__);
            GotFlag        = TRUE;
            Entry->OSType  =  'R';
            ShortcutLetter =  'R';
        }
        else if (FindSubStr (LoaderPath, L"refit")) {
            BREAD_CRUMB(L"%a:  5i 1 - B", __func__);
            if (GetImage) {
                MergeUniqueStrings (&TmpIconName, L"refit", L',');
            }

            BREAD_CRUMB(L"%a:  5i 2 - B", __func__);
            GotFlag        = TRUE;
            Entry->OSType  =  'R';
            ShortcutLetter =  'R';
        }
        else if (MyStriCmp (NameClues, L"diags.efi")) {
            BREAD_CRUMB(L"%a:  5j 1", __func__);
            if (GetImage) {
                MergeUniqueStrings (&TmpIconName, L"hwtest", L',');
            }
        }
        else if (
            MyStriCmp (NameClues, L"e.efi")     ||
            MyStriCmp (NameClues, L"elilo.efi") ||
            FindSubStr (NameClues, L"elilo")
        ) {
            BREAD_CRUMB(L"%a:  5k 1", __func__);
            if (GetImage) {
                MergeUniqueItems (&TmpIconName, L"elilo,linux", L',');
            }
            GotFlag        = TRUE;
            Entry->OSType  =  'E';

            BREAD_CRUMB(L"%a:  5k 2", __func__);
            if (ShortcutLetter == 0) {
                BREAD_CRUMB(L"%a:  5f 2a 1", __func__);
                ShortcutLetter = 'L';
            }

            BREAD_CRUMB(L"%a:  5k 3", __func__);
            Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
        }
        else if (MyStriCmp (NameClues, L"xom.efi")) {
            BREAD_CRUMB(L"%a:  5l 1", __func__);
            if (GetImage) {
                MergeUniqueItems (&TmpIconName, L"xom,win8,win", L',');
            }

            BREAD_CRUMB(L"%a:  5l 2", __func__);
            GotFlag        = TRUE;
            Entry->OSType  =  'X';
            ShortcutLetter =  'W';
            Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
        }
        else if (FindSubStr (NameClues, L"ipxe")) {
            BREAD_CRUMB(L"%a:  5m 1", __func__);
            if (GetImage) {
                MergeUniqueItems (&TmpIconName, L"network", L',');
            }

            BREAD_CRUMB(L"%a:  5m 2", __func__);
            GotFlag        = TRUE;
            Entry->OSType  =  'N';
            ShortcutLetter =  'N';
        }
        BREAD_CRUMB(L"%a:  5a B", __func__);
    }

    BREAD_CRUMB(L"%a:  6", __func__);
    if (GetImage && TmpIconName != NULL) {
        BREAD_CRUMB(L"%a:  6a 1", __func__);
        if (!GlobalConfig.HelpIcon) {
            BREAD_CRUMB(L"%a:  6a 1a 1", __func__);
            MergeUniqueItems (&OSIconName, TmpIconName, L',');
            MY_FREE_POOL(TmpIconName);
        }
        else {
            BREAD_CRUMB(L"%a:  6a 1b 1", __func__);
            MergeUniqueItems (&TmpIconName, OSIconName, L',');
            MY_FREE_POOL(OSIconName);
            OSIconName = TmpIconName;
        }
        BREAD_CRUMB(L"%a:  6a 2", __func__);
    }

    BREAD_CRUMB(L"%a:  7", __func__);
    if (ShortcutLetter >= 'a' &&
        ShortcutLetter <= 'z'
    ) {
        BREAD_CRUMB(L"%a:  7a 1", __func__);
        ShortcutLetter = ShortcutLetter - 'a' + 'A'; // Convert to uppercase
    }

    BREAD_CRUMB(L"%a:  8", __func__);
    Entry->me.ShortcutLetter = ShortcutLetter;
    if (GetImage) {
        BREAD_CRUMB(L"%a:  8a 0", __func__);
        if (GotUEFI) {
            BREAD_CRUMB(L"%a:  8a 0a 1", __func__);
            MergeUniqueItems (&OSIconName, L"uefi", L',');
            BREAD_CRUMB(L"%a:  8a 0a 2", __func__);
        }

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Locate Icon Based on Hint String:- '%s'",
            OSIconName
        );
        #endif

        BREAD_CRUMB(L"%a:  8a 1", __func__);
        if (!GotFlag             &&
            !MacFlag             &&
            ThisIconName != NULL &&
            GlobalConfig.HelpIcon
        ) {
            BREAD_CRUMB(L"%a:  8a 1a 1", __func__);
            i = 0;
            while (
                !GotFlag &&
                (Temp = FindCommaDelimited (OSIconName, i++)) != NULL
            ) {
                LOG_SEP(L"X");
                BREAD_CRUMB(L"%a:  8a 1a 1a 1 - WHILE LOOP:- START", __func__);
                if (IsListItem (Temp, BASE_LINUX_DISTROS)) {
                    BREAD_CRUMB(L"%a:  8a 1a 1a 1a 1", __func__);
                    GotFlag = TRUE;
                    MergeUniqueItems (&OSIconName, L"linux", L',');
                    BREAD_CRUMB(L"%a:  8a 1a 1a 1a 1", __func__);
                }
                MY_FREE_POOL(Temp);

                BREAD_CRUMB(L"%a:  8a 1a 1a 2 - WHILE LOOP:- END", __func__);
                LOG_SEP(L"X");
            } // while
            BREAD_CRUMB(L"%a:  8a 1a 2", __func__);
        }

        BREAD_CRUMB(L"%a:  8a 2", __func__);
        Entry->me.Image = LoadOSIcon (OSIconName, L"unknown", FALSE);
        BREAD_CRUMB(L"%a:  8a 3", __func__);
    }

    BREAD_CRUMB(L"%a:  9", __func__);
    MY_FREE_POOL(PathOnly);
    MY_FREE_POOL(OSIconName);
    MY_FREE_POOL(NameClues);

    BREAD_CRUMB(L"%a:  10 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID SetLoaderDefaults()

CHAR16 * GetVolumeGroupName (
    IN CHAR16       *LoaderPath,
    IN REFIT_VOLUME *Volume
) {
    UINTN   TestLen, NameLen, FlagLen, i;
    CHAR16 *VolumeGroupName, *DataTag, *TempStr;

    if (!GlobalConfig.SyncAPFS) {
        // Early Return
        return NULL;
    }

    VolumeGroupName = NULL;
    if (SingleAPFS) {
        for (i = 0; i < SystemVolumesCount; i++) {
            if (GuidsAreEqual (
                    &(SystemVolumes[i]->PartGuid),
                    &(Volume->PartGuid)
                )
            ) {
                VolumeGroupName = StrDuplicate (
                    SystemVolumes[i]->VolName
                );
                break;
            }
        } // for
    }

    if (VolumeGroupName == NULL) {
        for (i = 0; i < SystemVolumesCount; i++) {
            if (FindSubStr (
                    LoaderPath,
                    GuidAsString (&(SystemVolumes[i]->VolUuid))
                )
            ) {
                VolumeGroupName = StrDuplicate (
                    SystemVolumes[i]->VolName
                );
                break;
            }
        } // for
    }

    if (VolumeGroupName == NULL) {
        for (i = 0; i < DataVolumesCount; i++) {
            if (FindSubStr (
                    LoaderPath,
                    GuidAsString (&(DataVolumes[i]->VolUuid))
                )
            ) {
                // DA-TAG: Use 'DATA' volume name as last resort if required
                DataTag = L" - Data";
                TempStr = StrDuplicate (DataVolumes[i]->VolName);
                if (FindSubStr (TempStr, DataTag)) {
                    FlagLen = StrLen (DataTag);
                    NameLen = StrLen (TempStr);
                    if (NameLen > FlagLen) {
                        TestLen = NameLen - FlagLen;
                        TruncateString (TempStr, TestLen);
                    }
                }

                VolumeGroupName = PoolPrint (L"%s", TempStr);
                MY_FREE_POOL(TempStr);
                break;
            }
        } // for
    }

    return VolumeGroupName;
} // CHAR16 * GetVolumeGroupName()

// Add an NVRAM-based EFI boot loader entry to the menu.
static
LOADER_ENTRY * AddEfiLoaderEntry (
    IN EFI_DEVICE_PATH_PROTOCOL *EfiLoaderPath,
    IN CHAR16                   *LoaderTitle,
    IN UINT16                    EfiBootNum,
    IN UINTN                     Row,
    IN EG_IMAGE                 *Icon   OPTIONAL
) {
    CHAR16        *TempStr;
    CHAR16        *FullTitle;
    CHAR16        *OSIconName;
    LOADER_ENTRY  *Entry;

    Entry = InitializeLoaderEntry (NULL);
    if (Entry == NULL) {
        return NULL;
    }

    FullTitle = (LoaderTitle != NULL)
        ? PoolPrint (L"Reboot to %s", LoaderTitle) : NULL;

    Entry->me.Row        = Row;
    Entry->me.Tag        = TAG_FIRMWARE_LOADER;
    Entry->DiscoveryType = DISCOVERY_TYPE_AUTO;
    Entry->me.Title      = StrDuplicate ((FullTitle   != NULL) ? FullTitle   : L"Instance: Unknown");
    Entry->Title         = StrDuplicate ((LoaderTitle != NULL) ? LoaderTitle : L"Instance: Unknown"); // without "Reboot to"
    Entry->EfiLoaderPath = DuplicateDevicePath (EfiLoaderPath);
    TempStr              = DevicePathToStr (EfiLoaderPath);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Append uEFI Loader Entry:- '%s'",
        Entry->Title
    );
    ALT_LOG(1, LOG_THREE_STAR_MID,
        L"uEFI Loader Path:- '%s'", TempStr
    );
    #endif

    MY_FREE_POOL(TempStr);

    Entry->EfiBootNum = EfiBootNum;

    OSIconName = NULL;
    MergeUniqueWords (&OSIconName, Entry->me.Title, L',');
    MergeUniqueStrings (&OSIconName, L"Unknown", L',');

    Entry->me.Image = (Icon != NULL)
        ? egCopyImage (Icon) : LoadOSIcon (OSIconName, NULL, FALSE);

    Entry->me.BadgeImage = (Row == 0)
        ? BuiltinIcon (BUILTIN_ICON_VOL_EFI) : NULL;

    MY_FREE_POOL(OSIconName);

    Entry->Volume      = NULL;
    Entry->LoaderPath  = NULL;
    Entry->LoadOptions = NULL;
    Entry->InitrdPath  = NULL;
    Entry->Enabled     = TRUE;

    AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *) Entry);

    return Entry;
} // LOADER_ENTRY * AddEfiLoaderEntry()


// Add a specified EFI boot loader to the list,
// using automatic settings for icons, options, etc.
static
LOADER_ENTRY * AddLoaderEntry (
    IN OUT CHAR16       *LoaderPath,
    IN     CHAR16       *LoaderTitle,
    IN     REFIT_VOLUME *Volume,
    IN     BOOLEAN       SubScreenReturn,
    IN     BOOLEAN       CheckLinux
) {
    UINTN                   i;
    CHAR16                 *DisplayName;
    CHAR16                 *TmpName;
    CHAR16                 *ShowName;
    CHAR16                 *LinuxName;
    CHAR16                 *SearchName;
    BOOLEAN                 Found;
    BOOLEAN                 GotSysD;
    BOOLEAN                 GotGrub;
    BOOLEAN                 GotElilo;
    BOOLEAN                 FoundPreBootName;
    LOADER_ENTRY           *Entry;


    if (!VolumeScanAllowed (Volume, TRUE, FALSE)) {
        return NULL;
    }

    Entry = InitializeLoaderEntry (NULL);
    if (Entry == NULL) {
        // Early Return
        return NULL;
    }

    CleanUpPathNameSlashes (LoaderPath);

    if (LoaderTitle != NULL) {
        Entry->Title = StrDuplicate (LoaderTitle);
    }
    else {
        Found = FALSE;
        if (CheckLinux) {
            if (FindSubStr (LoaderPath, L"Grub")) {
                GotElilo = FALSE;
                GotSysD  = FALSE;
                GotGrub  =  TRUE;
            }
            else if (FindSubStr (LoaderPath, L"SystemD")) {
                GotSysD  =  TRUE;
                GotGrub  = FALSE;
                GotElilo = FALSE;
            }
            else {
                GotGrub  = FALSE;
                GotSysD  = FALSE;
                GotElilo = (FindSubStr (LoaderPath, L"Elilo"))
                    ? TRUE : FALSE;
            }

            i = 0;
            while (
                !Found &&
                GlobalConfig.HelpScan &&
                (LinuxName = FindCommaDelimited (MAIN_LINUX_DISTROS, i++)) != NULL
            ) {
                SearchName = PoolPrint (L"\\%s", LinuxName);
                if (FindSubStr (LoaderPath, SearchName)) {
                    Found = TRUE;

                    // DA-TAG: *DO NOT* Free 'ShowName'
                    ShowName = GetShowName (LinuxName);

                    Entry->Title = (!GotGrub && !GotSysD && !GotElilo)
                        ? PoolPrint (
                            L"Instance: Linux - %s", ShowName
                        )
                        : (GotGrub)
                            ? PoolPrint (
                                L"Instance: Linux - %s via Grub", ShowName
                            )
                            : (GotSysD)
                                ? PoolPrint (
                                    L"Instance: Linux - %s via SystemdBoot", ShowName
                                )
                                : PoolPrint (
                                    L"Instance: Linux - %s via Elilo", ShowName
                                );
                }
                MY_FREE_POOL(LinuxName);
                MY_FREE_POOL(SearchName);
            } // while
        } // if CheckLinux

        if (!Found) {
            if (Entry->OSType != 'L' &&
                Entry->OSType != 'E' &&
                Entry->OSType != 'G'
            ) {
                Entry->Title = PoolPrint (L"%s", LoaderPath);
            }
            else {
                Entry->Title = PoolPrint (L"Instance: Linux - %s", LoaderPath);
            }
        }
    } // if/else LoaderTitle

    DisplayName = NULL;

    // DA-TAG: Limit to TianoCore
    #ifdef __MAKEWITH_TIANO
    if (Volume->FSType == FS_TYPE_APFS && GlobalConfig.SyncAPFS) {
        if (Volume->VolRole == APFS_VOLUME_ROLE_PREBOOT) {
            DisplayName = GetVolumeGroupName (LoaderPath, Volume);
            if (DisplayName == NULL) {
                return NULL;
            }

            FoundPreBootName = FindSubStr (DisplayName, L"PreBoot");
            if (FoundPreBootName) {
                MY_FREE_POOL(DisplayName);
            }
            else {
                FoundPreBootName = FindSubStr (DisplayName, L"PreBoot");
                if (FoundPreBootName) {
                    MY_FREE_POOL(DisplayName);
                }
                else {
                    if (!SingleAPFS &&
                        IsListItem (DisplayName, GlobalConfig.DontScanVolumes)
                    ) {
                        MY_FREE_POOL(DisplayName);

                        // Early Return on PreBoot of Blocked APFS Volume
                        return NULL;
                    }

                    MY_FREE_POOL(Volume->VolName);
                    Volume->VolName = PoolPrint (L"PreBoot - %s", DisplayName);
                }
            }
        }
    }
    #endif

    #if REFIT_DEBUG > 0
    if (DisplayName != NULL) {
        ALT_LOG(1, LOG_THREE_STAR_MID, L"Synced PreBoot:- '%s'", DisplayName);
    }
    ALT_LOG(1, LOG_LINE_NORMAL, L"Add Loader Entry:- '%s'", Entry->Title);
    ALT_LOG(1, LOG_LINE_NORMAL, L"uEFI Loader File:- '%s'", LoaderPath);
    #endif

    SetVolumeBadgeIcon (Volume);

    TmpName         = (DisplayName != NULL) ? DisplayName : Volume->VolName;
    Entry->me.Title = (DisplayName != NULL || Volume->VolName != NULL)
        ? PoolPrint (
            L"Load %s%s%s%s%s",
            Entry->Title,
            SetVolJoin (Entry->Title, TRUE                   ),
            SetVolKind (Entry->Title, TmpName, Volume->FSType),
            SetVolFlag (Entry->Title, TmpName                ),
            SetVolType (Entry->Title, TmpName, Volume->FSType)
        )
        : PoolPrint (
            L"Load %s",
            (LoaderTitle != NULL) ? LoaderTitle : LoaderPath
        );

    Entry->me.Row = 0;
    Entry->me.BadgeImage = egCopyImage (Volume->VolBadgeImage);
    Entry->DiscoveryType = DISCOVERY_TYPE_AUTO;

    Entry->LoaderPath = (LoaderPath != NULL && (LoaderPath[0] != L'\\'))
        ? StrDuplicate (L"\\") : NULL;
    MergeUniqueStrings (&(Entry->LoaderPath), LoaderPath, 0);

    Entry->Volume = Volume;
    SetLoaderDefaults (Entry, LoaderPath, Volume);
    GenerateSubScreen (Entry, Volume, SubScreenReturn);
    AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *) Entry);

    #if REFIT_DEBUG > 0
    TmpName = (DisplayName != NULL)
        ? DisplayName
        : (Volume->VolName != NULL)
            ? Volume->VolName
            : Entry->LoaderPath;
    LOG_MSG(
        "%s  - Found %s%s%s%s%s",
        OffsetNext,
        Entry->Title,
        SetVolJoin (Entry->Title, FALSE                  ),
        SetVolKind (Entry->Title, TmpName, Volume->FSType),
        SetVolFlag (Entry->Title, TmpName                ),
        SetVolType (Entry->Title, TmpName, Volume->FSType)
    );

    ALT_LOG(1, LOG_THREE_STAR_END,
        L"Successfully Created Menu Entry for %s",
        Entry->Title
    );
    #endif

    MY_FREE_POOL(DisplayName);

    return Entry;
} // LOADER_ENTRY * AddLoaderEntry()

// Returns -1 if (Time1 < Time2), +1 if (Time1 > Time2), or 0 if
// (Time1 == Time2). Precision is only to the nearest second; since
// this is used for sorting boot loader entries, differences smaller
// than this are likely to be meaningless (and unlikely!).
static
INTN TimeComp (
    IN EFI_TIME *Time1,
    IN EFI_TIME *Time2
) {
    INT64 Time1InSeconds, Time2InSeconds;

    // Following values are overestimates; I'm assuming 31 days in every month.
    // This is fine for the purpose of this function, which is limited
    Time1InSeconds = (INT64) (Time1->Second)
        + ((INT64) (Time1->Minute)      * 60)
        + ((INT64) (Time1->Hour)        * 3600)
        + ((INT64) (Time1->Day)         * 86400)
        + ((INT64) (Time1->Month)       * 2678400)
        + ((INT64) (Time1->Year - 1998) * 32140800);

    Time2InSeconds = (INT64) (Time2->Second)
        + ((INT64) (Time2->Minute)      * 60)
        + ((INT64) (Time2->Hour)        * 3600)
        + ((INT64) (Time2->Day)         * 86400)
        + ((INT64) (Time2->Month)       * 2678400)
        + ((INT64) (Time2->Year - 1998) * 32140800);

    if (Time1InSeconds < Time2InSeconds) {
        return -1;
    }

    if (Time1InSeconds > Time2InSeconds) {
        return 1;
    }

    return 0;
} // INTN TimeComp()

// Adds a loader list element, keeping it sorted by date. EXCEPTION: Fedora's rescue
// kernel, which begins with "vmlinuz-0-rescue," should not be at the top of the list,
// since that will make it the default if kernel folding is enabled, so float it to
// the end.
// Returns the new first element (the one with the most recent date).
static struct
LOADER_LIST * AddLoaderListEntry (
    struct LOADER_LIST *LoaderList,
    struct LOADER_LIST *NewEntry
) {
    struct LOADER_LIST *LatestEntry;
    struct LOADER_LIST *CurrentEntry;
    struct LOADER_LIST *PrevEntry;
    BOOLEAN             LinuxRescue;


    if (LoaderList == NULL) {
        LatestEntry = NewEntry;
    }
    else {
        PrevEntry   = NULL;
        LatestEntry = CurrentEntry = LoaderList;
        LinuxRescue = (StriSubCmp (L"vmlinuz-0-rescue", NewEntry->FileName))
            ? TRUE : FALSE;

        while (
            CurrentEntry != NULL &&
            !StriSubCmp (L"vmlinuz-0-rescue", CurrentEntry->FileName) &&
            (
                LinuxRescue ||
                TimeComp (
                    &(NewEntry->TimeStamp),
                    &(CurrentEntry->TimeStamp)
                ) < 0
            )
        ) {
            PrevEntry    = CurrentEntry;
            CurrentEntry = CurrentEntry->NextEntry;
        } // while

        NewEntry->NextEntry = CurrentEntry;

        if (PrevEntry == NULL) {
            LatestEntry = NewEntry;
        }
        else {
            PrevEntry->NextEntry = NewEntry;
        }
    }

    return LatestEntry;
} // static VOID AddLoaderListEntry()

// Delete the LOADER_LIST linked list
static
VOID CleanUpLoaderList (
    struct LOADER_LIST *LoaderList
) {
    struct LOADER_LIST *Temp;


    if (LoaderList == NULL) {
        return;
    }

    while (LoaderList != NULL) {
        Temp = LoaderList;
        LoaderList = LoaderList->NextEntry;
        MY_FREE_POOL(Temp->FileName);
        MY_FREE_POOL(Temp);
    } // while
} // static VOID CleanUpLoaderList()

CHAR16 * SetVolKind (
    IN CHAR16 *InstanceName,
    IN CHAR16 *VolumeName,
    IN UINT32  VolumeFSType
) {
    CHAR16 *RetVal;


    if (0);
    else if (VolumeFSType == FS_TYPE_FAT32         ) RetVal = L""         ;
    else if (VolumeFSType == FS_TYPE_FAT16         ) RetVal = L""         ;
    else if (VolumeFSType == FS_TYPE_FAT12         ) RetVal = L""         ;
    else if (VolumeFSType == FS_TYPE_EXFAT         ) RetVal = L""         ;
    else if (MyStriCmp (VolumeName,   L"EFI"      )) RetVal = L""         ;
    else if (MyStriCmp (VolumeName,   L"ESP"      )) RetVal = L""         ;
    else if (MyStrStr  (VolumeName,   L"Volume"   )) RetVal = L""         ;
    else if (MyStrStr  (VolumeName,   L"Partition")) RetVal = L""         ;
    else if (MyStrStr  (InstanceName, L"Instance:")) RetVal = L"Volume:- ";
    else                                             RetVal = L""         ;

    return RetVal;
} // CHAR16 * SetVolKind()

CHAR16 * SetVolJoin (
    IN CHAR16  *InstanceName,
    IN BOOLEAN  ForBoot
) {
    CHAR16 *RetVal;


    if (0);
    else if (MyStrStr  (InstanceName, L"Manual Stanza:" )) RetVal = L""      ;
    else if (MyStriCmp (InstanceName, L"Legacy Bootcode")) RetVal = L" for " ;
    else if (ForBoot                                     ) RetVal = L" from ";
    else                                                   RetVal = L" on "  ;

    return RetVal;
} // CHAR16 * SetVolJoin()

CHAR16 * SetVolFlag (
    IN CHAR16 *InstanceName,
    IN CHAR16 *VolumeName
) {
    CHAR16 *RetVal;


    if (MyStrStr (InstanceName, L"Manual Stanza:")) {
        RetVal = L"";
    }
    else {
        RetVal = VolumeName;
    }

    return RetVal;
} // CHAR16 * SetVolFlag()

CHAR16 * SetVolType (
    IN CHAR16 *InstanceName OPTIONAL,
    IN CHAR16 *VolumeName,
    IN UINT32  VolumeFSType
) {
    CHAR16 *RetVal;


    if (0);
    else if (MyStrStr    (InstanceName, L"Manual Stanza:" )) RetVal = L""                 ;
    else if (MyStrStr    (InstanceName, L"(Legacy"        )) RetVal = L""                 ;
    else if (MyStrStr    (InstanceName, L"Instance:"      )) RetVal = L""                 ;
    else if (MyStrStr    (VolumeName,   L"Partition"      )) RetVal = L""                 ;
    else if (MyStrStr    (VolumeName,   L"Volume"         )) RetVal = L""                 ;
    else if (MyStriCmp   (VolumeName,   L"ESP"            )) RetVal = L""                 ;
    else if (MyStriCmp   (VolumeName,   L"EFI"            )) RetVal = L" System Partition";
    else if (MyStriCmp   (VolumeName,   L"BOOTCAMP"       )) RetVal = L" Partition"       ;
    else if (MyStriCmp   (InstanceName, L"Legacy Bootcode")) RetVal = L" Partition"       ;
    else if (MyStrBegins (InstanceName, L"vmlinuz-"       )) RetVal = L" Partition"       ;
    else if (MyStrBegins (InstanceName, L"bzImage-"       )) RetVal = L" Partition"       ;
    else if (VolumeFSType == FS_TYPE_FAT32                 ) RetVal = L" Partition"       ;
    else if (VolumeFSType == FS_TYPE_FAT16                 ) RetVal = L" Partition"       ;
    else if (VolumeFSType == FS_TYPE_FAT12                 ) RetVal = L" Partition"       ;
    else if (VolumeFSType == FS_TYPE_EXFAT                 ) RetVal = L" Partition"       ;
    else                                                     RetVal = L""                 ;

    return RetVal;
} // CHAR16 * SetVolType()

// Returns FALSE if the specified file/volume matches the GlobalConfig.DontScanDirs
// or GlobalConfig.DontScanVolumes specification, or if Path points to a volume
// other than the one specified by Volume, or if the specified path is 'SelfDir',
// or if the volume has been otherwise flagged as being 'unreadable',
// Returns TRUE if none of these conditions is met -- that is, if the path is
// eligible for scanning.
BOOLEAN ShouldScan (
    REFIT_VOLUME *Volume,
    CHAR16       *Path
) {
    UINTN                   i;
    CHAR16                 *VolName;
    CHAR16                 *PathCopy;
    CHAR16                 *DontScanDir;
    CHAR16                 *TmpVolNameA;
    CHAR16                 *TmpVolNameB;
    CHAR16                 *VentoyName;
    BOOLEAN                 ScanIt;
    BOOLEAN                 FoundVentoy;


    if (GlobalConfig.NvramProtect &&
        (
            MyStrBegins (L"EFI\\APPLE",   Path) ||
            MyStrBegins (L"\\EFI\\APPLE", Path)
        )
    ) {
        return FALSE;
    }

    if (MyStriCmp (Path, SelfDirPath) &&
        Volume->DeviceHandle == SelfVolume->DeviceHandle
    ) {
        return FALSE;
    }

    if (MyStrStr (Path, L"\\memtest")) {
        return FALSE;
    }

    ScanIt = VolumeScanAllowed (Volume, FALSE, FALSE); // Do NOT Check Ventoy Here
    if (ScanIt && Volume->FSType == FS_TYPE_APFS) {
        if (GlobalConfig.SyncAPFS) {
            TmpVolNameA = PoolPrint (L"%s - DATA", Volume->VolName);
            if (IsListItem (TmpVolNameA, GlobalConfig.DontScanVolumes)) {
                ScanIt = FALSE;
            }
            MY_FREE_POOL(TmpVolNameA);

            if (ScanIt) {
                TmpVolNameB = SanitiseString (Volume->VolName);
                TmpVolNameA = PoolPrint (L"%s - DATA", TmpVolNameB);
                if (IsListItem (TmpVolNameA, GlobalConfig.DontScanVolumes)) {
                    ScanIt = FALSE;
                }
                MY_FREE_POOL(TmpVolNameA);
                MY_FREE_POOL(TmpVolNameB);
            }

            if (!ScanIt) {
                return FALSE;
            }
        }
    }

    if (GlobalConfig.HandleVentoy && !ScanIt) {
        FoundVentoy = FALSE;
        if (MyStriCmp (Path, L"EFI\\BOOT") &&
            FileExists (Volume->RootDir, FALLBACK_FULLNAME)
        ) {
            i = 0;
            while (
                !FoundVentoy &&
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
        }

        return FoundVentoy;
    }

    if (!ScanIt) {
        return FALSE;
    }

    // See if Path includes an explicit volume declaration that is NOT Volume.
    VolName  = NULL;
    PathCopy = StrDuplicate (Path);
    if (SplitVolumeAndFilename (&PathCopy, &VolName)) {
        if (VolName) {
            if (!MyStriCmp (VolName, Volume->FsName) &&
                !MyStriCmp (VolName, Volume->PartName)
            ) {
                ScanIt = FALSE;
            }
            MY_FREE_POOL(VolName);
        }
    }
    MY_FREE_POOL(PathCopy);

    if (!ScanIt) {
        return FALSE;
    }

    // See if Volume is in GlobalConfig.DontScanDirs.
    i = 0;
    while ((DontScanDir = FindCommaDelimited (GlobalConfig.DontScanDirs, i++)) != NULL) {
        SplitVolumeAndFilename (&DontScanDir, &VolName);
        CleanUpPathNameSlashes (DontScanDir);
        if (VolName == NULL) {
            if (MyStriCmp (DontScanDir, Path)) {
                ScanIt = FALSE;
            }
        }
        else if (
            MyStriCmp (DontScanDir, Path)          &&
            VolumeMatchesDescription (Volume, VolName)
        ) {
            ScanIt = FALSE;
        }

        MY_FREE_POOL(VolName);
        MY_FREE_POOL(DontScanDir);

        if (!ScanIt) {
            break;
        }
    } // while

    return ScanIt;
} // BOOLEAN ShouldScan()

// Returns TRUE if the file is byte-for-byte identical with the fallback file
// on the volume AND if the file is not itself the fallback file; returns
// FALSE if the file is not identical to the fallback file OR if the file
// IS the fallback file. Intended for use in excluding the fallback boot
// loader when it is a duplicate of another boot loader.
static
BOOLEAN DuplicatesFallback (
    IN REFIT_VOLUME *Volume,
    IN CHAR16       *FileName
) {
    EFI_STATUS       Status;
    EFI_FILE_HANDLE  FileHandle;
    EFI_FILE_HANDLE  FallbackHandle;
    EFI_FILE_INFO   *FileInfo;
    EFI_FILE_INFO   *FallbackInfo;
    CHAR8           *FileContents;
    CHAR8           *FallbackContents;
    UINTN            FileSize;
    UINTN            FallbackSize;
    BOOLEAN          AreIdentical;


    if (!FileExists (Volume->RootDir, FileName) ||
        !FileExists (Volume->RootDir, FALLBACK_FULLNAME)
    ) {
        return FALSE;
    }

    CleanUpPathNameSlashes (FileName);

    if (MyStriCmp (FileName, FALLBACK_FULLNAME)) {
        // Identical filenames ... so not a duplicate
        return FALSE;
    }

    Status = REFIT_CALL_5_WRAPPER(
        Volume->RootDir->Open, Volume->RootDir,
        &FileHandle, FileName,
        EFI_FILE_MODE_READ, 0
    );
    if (EFI_ERROR(Status)) {
        return FALSE;
    }

    FileInfo = LibFileInfo (FileHandle);
    FileSize = FileInfo->FileSize;
    MY_FREE_POOL(FileInfo);

    Status = REFIT_CALL_5_WRAPPER(
        Volume->RootDir->Open, Volume->RootDir,
        &FallbackHandle, FALLBACK_FULLNAME,
        EFI_FILE_MODE_READ, 0
    );
    if (EFI_ERROR(Status)) {
        REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);
        return FALSE;
    }

    FallbackInfo = LibFileInfo (FallbackHandle);
    FallbackSize = FallbackInfo->FileSize;
    MY_FREE_POOL(FallbackInfo);

    AreIdentical = FALSE;
    if (FallbackSize == FileSize) {
        // Do full check as could be identical
        FileContents     = AllocatePool (FileSize);
        FallbackContents = AllocatePool (FallbackSize);
        if (FileContents != NULL && FallbackContents != NULL) {
            Status = REFIT_CALL_3_WRAPPER(
                FileHandle->Read, FileHandle,
                &FileSize, FileContents
            );
            if (!EFI_ERROR(Status)) {
                Status = REFIT_CALL_3_WRAPPER(
                    FallbackHandle->Read, FallbackHandle,
                    &FallbackSize, FallbackContents
                );
            }
            if (!EFI_ERROR(Status)) {
                AreIdentical = (CompareMem (
                    FileContents,
                    FallbackContents,
                    FileSize
                ) == 0);
            }
        }

        MY_FREE_POOL(FileContents);
        MY_FREE_POOL(FallbackContents);
    }

    // DA-TAG: Investigate This
    //         Some systems (e.g., DUET, some Macs with large displays)
    //         may apparently crash if the following calls are reversed.
    REFIT_CALL_1_WRAPPER(FileHandle->Close, FallbackHandle);
    REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);

    return AreIdentical;
} // BOOLEAN DuplicatesFallback()

// Returns FALSE if two measures of file size are identical for a single file,
// TRUE if not or if the file cannot be opened and the other measure is non-0.
// Despite the function's name, this is not an actual direct test of symbolic
// link status, since EFI does not officially support symlinks. It does seem
// to be a reliable indicator, though. (OTOH, some disk errors might cause a
// file to fail to open, which would return a false positive -- but as this
// function is used to exclude symbolic links from the list of boot loaders,
// that would be fine, as such boot loaders would not work anyway.)
//
// NB: *FullName MUST be properly cleaned up (using 'CleanUpPathNameSlashes')
static
BOOLEAN IsSymbolicLink (
    REFIT_VOLUME  *Volume,
    CHAR16        *FullName,
    EFI_FILE_INFO *DirEntry
) {
    EFI_FILE_HANDLE  FileHandle;
    EFI_FILE_INFO   *FileInfo;
    EFI_STATUS       Status;
    UINTN            FileSize2;


    FileSize2 = 0;
    Status = REFIT_CALL_5_WRAPPER(
        Volume->RootDir->Open, Volume->RootDir,
        &FileHandle, FullName,
        EFI_FILE_MODE_READ, 0
    );
    if (!EFI_ERROR(Status)) {
        FileInfo = LibFileInfo (FileHandle);
        if (FileInfo != NULL) {
            FileSize2 = FileInfo->FileSize;
        }
        MY_FREE_POOL(FileInfo);
    }

    return (DirEntry->FileSize != FileSize2);
} // BOOLEAN IsSymbolicLink()

// Scan an individual directory for EFI boot loader files and, if found,
// add them to the list. Exception: Ignores FALLBACK_FULLNAME, which is picked
// up in ScanEfiFiles(). Sorts the entries within the loader directory so that
// the most recent one appears first in the list.
// Returns TRUE if a duplicate for FALLBACK_FILENAME was found, FALSE if not.
static
BOOLEAN ScanLoaderDir (
    IN REFIT_VOLUME *Volume,
    IN CHAR16       *Path,
    IN CHAR16       *Pattern
) {
    EFI_STATUS               Status;
    REFIT_DIR_ITER           DirIter;
    EFI_FILE_INFO           *DirEntry;
    CHAR16                  *Message;
    CHAR16                  *FullName;
    struct LOADER_LIST      *NewLoader;
    struct LOADER_LIST      *LoaderList;
    LOADER_ENTRY            *FirstKernel;
    LOADER_ENTRY            *LatestEntry;
    BOOLEAN                  IsLinux;
    BOOLEAN                  InSelfPath;
    BOOLEAN                  ShouldScanThis;
    BOOLEAN                  IsFallbackLoader;
    BOOLEAN                  FoundFallbackDuplicate;

    #if REFIT_DEBUG > 0
    BOOLEAN                  CheckMute = FALSE;


    MY_MUTELOGGER_SET;
    #endif
    ShouldScanThis = ShouldScan (Volume, Path);
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    if (!ShouldScanThis) {
        return FALSE;
    }

    #if REFIT_DEBUG > 0
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    ALT_LOG(
        1, LOG_LINE_NORMAL,
        L"Scan for '%s' in '%s' on Volume '%s'",
        Pattern, Path, Volume->VolName
    );
    #endif

    InSelfPath = MyStriCmp (Path, SelfDirPath);

    BREAD_CRUMB(L"%a:  2", __func__);
    FoundFallbackDuplicate = FALSE;
    if (!InSelfPath || SelfDirPath != NULL || Path != NULL ||
        (InSelfPath && Volume->DeviceHandle != SelfVolume->DeviceHandle)
    ) {
        LoaderList = NULL;

        //BREAD_CRUMB(L"%a:  2a 1", __func__);
        // Look through contents of the directory
        DirIterOpen (Volume->RootDir, Path, &DirIter);

        //BREAD_CRUMB(L"%a:  2a 2", __func__);
        while (DirIterNext (&DirIter, 2, Pattern, &DirEntry)) {
            LOG_SEP(L"X");
            BREAD_CRUMB(L"%a:  2a 2a 1 - WHILE LOOP:- START", __func__);
            do {
                //BREAD_CRUMB(L"%a:  2a 2a 1a 1", __func__);
                FullName = StrDuplicate (Path);

                //BREAD_CRUMB(L"%a:  2a 2a 1a 2", __func__);
                MergeStrings (&FullName, DirEntry->FileName, L'\\');

                //BREAD_CRUMB(L"%a:  2a 2a 1a 3", __func__);
                CleanUpPathNameSlashes (FullName);

                //BREAD_CRUMB(L"%a:  2a 2a 1a 4", __func__);
                if (!GlobalConfig.FollowSymlinks) {
                    //BREAD_CRUMB(L"%a:  2a 2a 1a 4a 1", __func__);
                    if (IsSymbolicLink (Volume, FullName, DirEntry)) {
                        BREAD_CRUMB(L"%a:  2a 2a 1a 5a 1a 1 - WHILE LOOP:- CONTINUE (Skip Symlink)", __func__);
                        // Skip This Entry
                        break;
                    }
                }

                //BREAD_CRUMB(L"%a:  2a 2a 1a 6", __func__);
                IsFallbackLoader = MyStriCmp (
                    DirEntry->FileName,
                    FALLBACK_BASENAME
                );

                #if REFIT_DEBUG > 0
                MY_MUTELOGGER_SET;
                #endif
                ShouldScanThis = IsValidLoader (Volume->RootDir, FullName);
                #if REFIT_DEBUG > 0
                MY_MUTELOGGER_OFF;
                #endif

                if (!ShouldScanThis               ||
                    DirEntry->FileName[0] ==  '.' ||
                    (
                        IsListItem (Path, MEMTEST_LOCATIONS)
                    ) || (
                        IsFallbackLoader &&
                        MyStriCmp (Path, L"EFI\\BOOT")
                    ) || (
                        // Handle MEMTEST_NAMES here and not in 'SyncDontScanFiles'
                        // to accomodate fallback loaders in the list.
                        !IsFallbackLoader &&
                        IsListItem (
                            DirEntry->FileName,
                            MEMTEST_NAMES
                        )
                    ) || (
                        IsListItem (
                            DirEntry->FileName,
                            GlobalConfig.DontScanFiles
                        )
                    ) || (
                        FilenameIn (
                            Volume, Path,
                            DirEntry->FileName,
                            GlobalConfig.DontScanFiles
                        )
                    ) || (
                        IsListMatch (
                            DirEntry->FileName,
                            SKIPNAME_PATTERNS
                        )
                    ) || (
                        // TRUE == "SameName" + ".efi.signed" file present
                        HasSignedCounterpart (Volume, FullName)
                    )
                ) {
                    BREAD_CRUMB(L"%a:  2a 2a 1a 6a 1 - WHILE LOOP:- CONTINUE (Skip Invalid Item:- '%s')", __func__,
                        DirEntry->FileName
                    );
                    // Skip This Entry
                    break;
                }

                //BREAD_CRUMB(L"%a:  2a 2a 1a 7", __func__);
                NewLoader = AllocateZeroPool (sizeof (struct LOADER_LIST));
                if (NewLoader != NULL) {
                    //BREAD_CRUMB(L"%a:  2a 2a 1a 7a 1", __func__);
                    NewLoader->FileName  = StrDuplicate (FullName);
                    NewLoader->TimeStamp = DirEntry->ModificationTime;
                    LoaderList           = AddLoaderListEntry (LoaderList, NewLoader);

                    //BREAD_CRUMB(L"%a:  2a 2a 1a 7a 2", __func__);
                    if (DuplicatesFallback (Volume, FullName)) {
                        //BREAD_CRUMB(L"%a:  2a 2a 1a 7a 2a 1", __func__);
                        FoundFallbackDuplicate = TRUE;
                    }
                }
            } while (0); // This 'loop' only runs once

            //BREAD_CRUMB(L"%a:  2a 2a 1a 8", __func__);
            MY_FREE_POOL(FullName);
            MY_FREE_POOL(DirEntry);

            BREAD_CRUMB(L"%a:  2a 2a 2 - WHILE LOOP:- END", __func__);
            LOG_SEP(L"X");
        } // while

        //BREAD_CRUMB(L"%a:  2a 3", __func__);
        if (LoaderList != NULL) {
            IsLinux     =      FALSE;
            NewLoader   = LoaderList;
            FirstKernel =       NULL;

            //LOG_SEP(L"X");
            //BREAD_CRUMB(L"%a:  2a 3a 1", __func__);
            while (NewLoader != NULL) {

                //BREAD_CRUMB(L"%a:  2a 3a 1a 1 - WHILE LOOP:- START", __func__);
                IsLinux = IsListItemSubstringIn (
                    NewLoader->FileName,
                    GlobalConfig.LinuxPrefixes
                );

                //BREAD_CRUMB(L"%a:  2a 3a 1a 2", __func__);
                if (IsLinux                    &&
                    FirstKernel != NULL        &&
                    GlobalConfig.FoldLinuxKernels
                ) {
                    //BREAD_CRUMB(L"%a:  2a 3a 1a 2a 1", __func__);
                    AddKernelToSubmenu (
                        FirstKernel, NewLoader->FileName, Volume
                    );
                }
                else {
                    //BREAD_CRUMB(L"%a:  2a 3a 1a 2b 1", __func__);
                    DisplayLoader = TRUE;
                    LatestEntry = AddLoaderEntry (
                        NewLoader->FileName,
                        NULL, Volume,
                        !(IsLinux && GlobalConfig.FoldLinuxKernels),
                        TRUE
                    );
                    //BREAD_CRUMB(L"%a:  2a 3a 1a 2b 2", __func__);
                    if (IsLinux && FirstKernel == NULL) {
                        //BREAD_CRUMB(L"%a:  2a 3a 1a 2b 2a 1", __func__);
                        FirstKernel = LatestEntry;
                    }
                }
                //BREAD_CRUMB(L"%a:  2a 3a 1a 3", __func__);
                NewLoader = NewLoader->NextEntry;

                //BREAD_CRUMB(L"%a:  2a 3a 1a 4 - WHILE LOOP:- END", __func__);
                //LOG_SEP(L"X");
            } // while

            //BREAD_CRUMB(L"%a:  2a 3a 2", __func__);
            if (IsLinux             &&
                FirstKernel != NULL &&
                GlobalConfig.FoldLinuxKernels
            ) {
                //BREAD_CRUMB(L"%a:  2a 3a 2a 1", __func__);
                GetMenuEntryReturn (&FirstKernel->me.SubScreen);
                //BREAD_CRUMB(L"%a:  2a 3a 2a 2", __func__);
            }

            //BREAD_CRUMB(L"%a:  2a 3a 3", __func__);
            CleanUpLoaderList (LoaderList);
        } // if LoaderList != NULL

        //BREAD_CRUMB(L"%a:  2a 4", __func__);
        Status = DirIterClose (&DirIter);
        // NOTE: EFI_INVALID_PARAMETER really is an error that should be reported;
        // but reports have been received from users that get this error occasionally
        // but nothing wrong has been found or the problem reproduced. It is therefore
        // being put down to buggy EFI implementations and that particular error igored.
        //BREAD_CRUMB(L"%a:  2a 5", __func__);
        if (EFI_ERROR(Status)            &&
            Status != EFI_NOT_FOUND      &&
            Status != EFI_INVALID_PARAMETER
        ) {
            //BREAD_CRUMB(L"%a:  2a 5a 1", __func__);
            if (Path != NULL) {
                //BREAD_CRUMB(L"%a:  2a 5a 1a 1", __func__);
                Message = PoolPrint (
                    L"While Scanning the '%s' Directory on '%s'",
                    Path, Volume->VolName
                );
            }
            else {
                //BREAD_CRUMB(L"%a:  2a 5a 1b 1", __func__);
                Message = PoolPrint (
                    L"While Scanning the Root Directory on '%s'",
                    Volume->VolName
                );
            }

            //BREAD_CRUMB(L"%a:  2a 5a 2", __func__);
            CheckError (Status, Message);

            //BREAD_CRUMB(L"%a:  2a 5a 3", __func__);
            MY_FREE_POOL(Message);
        }
        //BREAD_CRUMB(L"%a:  2a 6", __func__);
    } // if !InSelfPath

    BREAD_CRUMB(L"%a:  3 - END:- return BOOLEAN FoundFallbackDuplicate = '%s'", __func__,
        FoundFallbackDuplicate ? L"TRUE" : L"FALSE"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return FoundFallbackDuplicate;
} // static BOOLEAN ScanLoaderDir()

// Run the IPXE_DISCOVER_NAME program, which obtains the IP address
// of the boot server and the name of the boot file it delivers.
static
CHAR16 * RuniPXEDiscover (
    EFI_HANDLE Volume
) {
    EFI_STATUS                 Status;
    EFI_DEVICE_PATH_PROTOCOL  *FilePath;
    EFI_HANDLE                 iPXEHandle;
    CHAR16                    *boot_info;
    UINTN                      boot_info_size;

    FilePath = FileDevicePath (Volume, IPXE_DISCOVER_NAME);
    Status = REFIT_CALL_6_WRAPPER(
        gBS->LoadImage, FALSE,
        SelfImageHandle, FilePath,
        NULL, 0, &iPXEHandle
    );
    if (EFI_ERROR(Status)) {
        return NULL;
    }

    boot_info      = NULL;
    boot_info_size =    0;
    REFIT_CALL_3_WRAPPER(
        gBS->StartImage, iPXEHandle,
        &boot_info_size, &boot_info
    );

    return boot_info;
} // RuniPXEDiscover()

// Scan for network (PXE) boot servers. This function relies on the presence
// of the IPXE_NAME and IPXE_DISCOVER_NAME program files on the volume from
// which RefindPlus launched. Howver, the tools are not entirely reliable.
static
VOID ScanNetboot (VOID) {
    CHAR16        *Temp;
    CHAR16        *Location;
    REFIT_VOLUME  *NetVolume;


    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"Netboot (IPXE) Options");
    #endif

    if (FileExists (SelfVolume->RootDir, IPXE_NAME)             &&
        FileExists (SelfVolume->RootDir, IPXE_DISCOVER_NAME)    &&
        IsValidLoader (SelfVolume->RootDir, IPXE_DISCOVER_NAME) &&
        IsValidLoader (SelfVolume->RootDir, IPXE_NAME)
    ) {
        Location = RuniPXEDiscover (SelfVolume->DeviceHandle);
        if (Location != NULL &&
            FileExists (SelfVolume->RootDir, IPXE_NAME)
        ) {
            NetVolume = CopyVolume (SelfVolume);
            if (NetVolume != NULL) {
                NetVolume->DiskKind = DISK_KIND_NET;

                MY_FREE_IMAGE(NetVolume->VolBadgeImage);
                NetVolume->VolBadgeImage = BuiltinIcon (BUILTIN_ICON_VOL_NET);

                MY_FREE_POOL(NetVolume->PartName);
                MY_FREE_POOL(NetVolume->VolName);
                MY_FREE_POOL(NetVolume->FsName);

                DisplayLoader = TRUE;
                Temp = StrDuplicate(IPXE_NAME);
                AddLoaderEntry (Temp, Location, NetVolume, TRUE, FALSE);
                MY_FREE_POOL(Temp);

                FreeVolume (&NetVolume);
            }
        }
    }
} // VOID ScanNetboot()


// Adds *FullFileName as a macOS loader, if it exists.
// Returns TRUE if the fallback loader is NOT a duplicate of this one,
// FALSE if it IS a duplicate.
static
BOOLEAN ScanMacOsLoader (
    REFIT_VOLUME *Volume,
    CHAR16       *FullFileName
) {
    UINTN     i;
    CHAR16   *NameOS;
    CHAR16   *VolName;
    CHAR16   *PathName;
    CHAR16   *FileName;
    BOOLEAN   AddThisEntry;
    BOOLEAN   CheckFallback;

    CheckFallback                 = TRUE;
    VolName = PathName = FileName = NULL;

    SplitPathName (FullFileName, &VolName, &PathName, &FileName);
    if (FileExists (Volume->RootDir, FullFileName) &&
        !FilenameIn (
            Volume, PathName,
            L"boot.efi", GlobalConfig.DontScanFiles
        )
    ) {
        if (FileExists (Volume->RootDir, L"EFI\\refind\\config.conf")     ||
            FileExists (Volume->RootDir, L"EFI\\refind\\refind.conf")     ||
            FileExists (Volume->RootDir, L"EFI\\RefindPlus\\config.conf") ||
            FileExists (Volume->RootDir, L"EFI\\RefindPlus\\refind.conf")
        ) {
            DisplayLoader = TRUE;
            AddLoaderEntry (
                FullFileName, L"Instance: RefindPlus",
                Volume, TRUE, FALSE
            );
        }
        else {
            AddThisEntry = HasMacOS = TRUE;
            if (GlobalConfig.SyncAPFS) {
                for (i = 0; i < SystemVolumesCount; i++) {
                    if (GuidsAreEqual (
                            &(SystemVolumes[i]->VolUuid),
                            &(Volume->VolUuid)
                        )
                    ) {
                        AddThisEntry = FALSE;
                        break;
                    }
                }
            }

            if (AddThisEntry) {
                NameOS = (IsInstallerMac (Volume))
                    ? L"Instance: macOS Installer"
                    : L"Instance: macOS";

                DisplayLoader = TRUE;
                AddLoaderEntry (FullFileName, NameOS, Volume, TRUE, FALSE);
            }
        }

        if (DuplicatesFallback (Volume, FullFileName)) {
            CheckFallback = FALSE;
        }
    }

    MY_FREE_POOL(VolName);
    MY_FREE_POOL(PathName);
    MY_FREE_POOL(FileName);

    return CheckFallback;
} // static BOOLEAN ScanMacOsLoader()

static
VOID ScanEfiFiles (
    REFIT_VOLUME *Volume
) {
    EFI_STATUS              Status;
    UINTN                   i;
    UINTN                   Length;
    CHAR16                 *Temp;
    CHAR16                 *TmpMsg;
    CHAR16                 *VolName;
    CHAR16                 *FileName;
    CHAR16                 *SelfPath;
    CHAR16                 *Directory;
    CHAR16                 *Extension;
    CHAR16                 *VentoyName;
    BOOLEAN                 ScanFallbackLoader;
    BOOLEAN                 FoundBRBackup;
    BOOLEAN                 FoundVentoy;
    EFI_FILE_INFO          *EfiDirEntry;
    REFIT_DIR_ITER          EfiDirIter;

    static CHAR16          *MatchPatterns = NULL;


    //LOG_SEP(L"X");
    //LOG_INCREMENT();
    //BREAD_CRUMB(L"%a:  1 - START", __func__);

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

    // Skip Volumes in 'DontScanVolumes' List
    // Unless this is a 'Ventoy Volume'
    if (!VolumeScanAllowed (Volume, TRUE, FALSE)) {
        //BREAD_CRUMB(L"%a:  1a 1", __func__);
        if (!FoundVentoy) {
            //BREAD_CRUMB(L"%a:  1a 2a 1 - END:- VOID ... Exit on 'DontScan' Volume", __func__);
            //LOG_DECREMENT();
            //LOG_SEP(L"X");

            // Early Return on 'DontScan' Volume
            return;
        }
    }

    ScanFallbackLoader = TRUE;
    if (FoundVentoy) {
        //BREAD_CRUMB(L"%a:  1a - GOTO 'VentoyJump'", __func__);
        goto VentoyJump;
    }

    //BREAD_CRUMB(L"%a:  2", __func__);
    if (Volume->FSType == FS_TYPE_NTFS) {
        //BREAD_CRUMB(L"%a:  2a 1", __func__);
        if (AppleFirmware && MyStrStr (Volume->VolName, L"BOOTCAMP")) {
            //BREAD_CRUMB(L"%a:  2a 1a 1 - END:- VOID ... Exit on Windows BootCamp Volume", __func__);
            //LOG_DECREMENT();
            //LOG_SEP(L"X");

            // Early Return on Windows BootCamp Volume
            return;
        }
    }

    //BREAD_CRUMB(L"%a:  3", __func__);
    if (Volume->FSType == FS_TYPE_APFS) {
        //BREAD_CRUMB(L"%a:  3a 1", __func__);
        if (GlobalConfig.SyncAPFS) {
            //BREAD_CRUMB(L"%a:  3a 1a 1", __func__);
            if (SingleAPFS &&
                (
                    Volume->VolRole == APFS_VOLUME_ROLE_SYSTEM  ||
                    Volume->VolRole == APFS_VOLUME_ROLE_PREBOOT ||
                    Volume->VolRole == APFS_VOLUME_ROLE_UNDEFINED
                )
            ) {
                //BREAD_CRUMB(L"%a:  3a 1a 1a 1", __func__);
                for (i = 0; i < SkipApfsVolumesCount; i++) {
                    //LOG_SEP(L"X");
                    //BREAD_CRUMB(L"%a:  3a 1a 1a 1a 1 - FOR LOOP:- START", __func__);
                    if (GuidsAreEqual (
                            &(SkipApfsVolumes[i]->PartGuid),
                            &(Volume->PartGuid)
                        )
                    ) {
                        //BREAD_CRUMB(L"%a:  3a 1a 1a 1a 1a 1 - FOR LOOP:- END VOID ... Exit on APFS Volume Skip", __func__);
                        //LOG_DECREMENT();
                        //LOG_SEP(L"X");

                        // Early Return on APFS Volume Skip
                        return;
                    }
                    //BREAD_CRUMB(L"%a:  3a 1a 1a 1a 2 - FOR LOOP:- END", __func__);
                    //LOG_SEP(L"X");
                } // for
                //BREAD_CRUMB(L"%a:  3a 1a 1a 2", __func__);
            }

            //BREAD_CRUMB(L"%a:  3a 1a 2", __func__);
            for (i = 0; i < SystemVolumesCount; i++) {
                //LOG_SEP(L"X");
                //BREAD_CRUMB(L"%a:  3a 1a 2a 1 - FOR LOOP:- START", __func__);
                if (GuidsAreEqual (
                        &(SystemVolumes[i]->VolUuid),
                        &(Volume->VolUuid)
                    )
                ) {
                    //BREAD_CRUMB(L"%a:  3a 1a 2a 1a 1 - FOR LOOP:- END VOID ... Exit on ReMapped APFS System Volume", __func__);
                    //LOG_DECREMENT();
                    //LOG_SEP(L"X");

                    // Early Return on ReMapped APFS System Volume
                    return;
                }
                //BREAD_CRUMB(L"%a:  3a 1a 2a 2 - FOR LOOP:- END", __func__);
                //LOG_SEP(L"X");
            } // for
            //BREAD_CRUMB(L"%a:  3a 1a 3", __func__);
        }
        //BREAD_CRUMB(L"%a:  3a 2", __func__);
    } // if Volume->FSType == FS_TYPE_APFS

    //BREAD_CRUMB(L"%a:  4", __func__);
    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP,
        L"Handle uEFI Loaders on Volume:- '%s'",
        (Volume->VolName != NULL) ? Volume->VolName : L"** No Name **"
    );
    #endif

    //BREAD_CRUMB(L"%a:  5", __func__);
    if (MatchPatterns == NULL) {
        //BREAD_CRUMB(L"%a:  5a 1", __func__);
        MatchPatterns = StrDuplicate (LOADER_MATCH_PATTERNS);
        if (GlobalConfig.ScanAllLinux && GlobalConfig.LinuxMatchPatterns != NULL) {
            //BREAD_CRUMB(L"%a:  5a 1a 1", __func__);
            MergeUniqueStrings (
                &MatchPatterns, GlobalConfig.LinuxMatchPatterns, L','
            );
            //BREAD_CRUMB(L"%a:  5a 1a 2", __func__);
        }
        //BREAD_CRUMB(L"%a:  5a 2", __func__);
    }

    //BREAD_CRUMB(L"%a:  6", __func__);
    // Check for macOS boot loader
    if (Volume->FSType != FS_TYPE_NTFS &&
        ShouldScan (Volume, MACOSX_LOADER_DIR)
    ) {
        //BREAD_CRUMB(L"%a:  6a 1", __func__);
        FileName = StrDuplicate (MACOSX_LOADER_PATH);

        //BREAD_CRUMB(L"%a:  6a 2", __func__);
        ScanFallbackLoader &= ScanMacOsLoader (Volume, FileName);
        MY_FREE_POOL(FileName);

        //BREAD_CRUMB(L"%a:  6a 3", __func__);
        DirIterOpen (Volume->RootDir, L"\\", &EfiDirIter);

        //BREAD_CRUMB(L"%a:  6a 4", __func__);
        while (DirIterNext (&EfiDirIter, 1, NULL, &EfiDirEntry)) {
            //LOG_SEP(L"X");
            //BREAD_CRUMB(L"%a:  6a 4a 1 - WHILE LOOP:- START", __func__);
            if (IsGuid (EfiDirEntry->FileName)) {
                //BREAD_CRUMB(L"%a:  6a 4a 1a 1", __func__);
                FileName = PoolPrint (L"%s\\%s", EfiDirEntry->FileName, MACOSX_LOADER_PATH);

                //BREAD_CRUMB(L"%a:  6a 4a 1a 2", __func__);
                ScanFallbackLoader &= ScanMacOsLoader (Volume, FileName);

                //BREAD_CRUMB(L"%a:  6a 4a 1a 3", __func__);
                MY_FREE_POOL(FileName);
                FileName = PoolPrint (L"%s\\%s", EfiDirEntry->FileName, L"boot.efi");

                //BREAD_CRUMB(L"%a:  6a 4a 1a 4", __func__);
                if (Volume->FSType != FS_TYPE_APFS) {
                    //BREAD_CRUMB(L"%a:  6a 4a 1a 4a 1", __func__);
                    if (!StriSubCmp (FileName, GlobalConfig.MacOSRecoveryFiles)) {
                        //BREAD_CRUMB(L"%a:  6a 4a 1a 4a 1a 1", __func__);
                        MergeUniqueStrings (&GlobalConfig.MacOSRecoveryFiles, FileName, L',');
                    }
                    //BREAD_CRUMB(L"%a:  6a 4a 1a 4a 2", __func__);
                }
                MY_FREE_POOL(FileName);
                //BREAD_CRUMB(L"%a:  6a 4a 1a 5", __func__);
            }

            //BREAD_CRUMB(L"%a:  6a 4a 2", __func__);
            MY_FREE_POOL(EfiDirEntry);

            //BREAD_CRUMB(L"%a:  6a 4a 3 - WHILE LOOP:- END", __func__);
            //LOG_SEP(L"X");
        } // while

        //BREAD_CRUMB(L"%a:  6a 5", __func__);
        DirIterClose (&EfiDirIter);

        // Check for XOM
        //BREAD_CRUMB(L"%a:  6a 6", __func__);
        FileName = StrDuplicate (L"System\\Library\\CoreServices\\xom.efi");

        //BREAD_CRUMB(L"%a:  6a 7", __func__);
        if (FileExists (Volume->RootDir, FileName) &&
            !FilenameIn (Volume, MACOSX_LOADER_DIR, L"xom.efi", GlobalConfig.DontScanFiles)
        ) {
            //BREAD_CRUMB(L"%a:  6a 7a 1", __func__);
            DisplayLoader = TRUE;
            AddLoaderEntry (FileName, L"Instance: Windows XP (XoM)", Volume, TRUE, FALSE);

            //BREAD_CRUMB(L"%a:  6a 7a 2", __func__);
            if (DuplicatesFallback (Volume, FileName)) {
                //BREAD_CRUMB(L"%a:  6a 7a 2a 1", __func__);
                ScanFallbackLoader = FALSE;
            }
            //BREAD_CRUMB(L"%a:  6a 7a 3", __func__);
        }
        MY_FREE_POOL(FileName);

        //BREAD_CRUMB(L"%a:  6a 8", __func__);
    } // if ShouldScan MACOSX_LOADER_DIR

    //BREAD_CRUMB(L"%a:  7", __func__);
    // Check for Microsoft boot loader/menu
    if (ShouldScan (Volume, L"EFI\\Microsoft\\Boot")) {
        FoundBRBackup = FALSE;

        //BREAD_CRUMB(L"%a:  7a 1", __func__);
        FileName = StrDuplicate (L"EFI\\Microsoft\\Boot\\bkpbootmgfw.efi");

        //BREAD_CRUMB(L"%a:  7a 2", __func__);
        if (FileExists (Volume->RootDir, FileName) &&
            !FilenameIn (
                Volume,
                L"EFI\\Microsoft\\Boot",
                L"bkpbootmgfw.efi",
                GlobalConfig.DontScanFiles
            )
        ) {
            //BREAD_CRUMB(L"%a:  7a 2a 1", __func__);
            FoundBRBackup = DisplayLoader = TRUE;

            // Add Repair Backup
            AddLoaderEntry (
                FileName, L"Instance: UEFI Windows (BRBackup)",
                Volume, TRUE, FALSE
            );

            //BREAD_CRUMB(L"%a:  7a 2a 2", __func__);
            if (DuplicatesFallback (Volume, FileName)) {
                //BREAD_CRUMB(L"%a:  7a 2a 2a 1", __func__);
                ScanFallbackLoader = FALSE;
            }
            //BREAD_CRUMB(L"%a:  7a 2a 3", __func__);
        }
        MY_FREE_POOL(FileName);

        //BREAD_CRUMB(L"%a:  7a 3", __func__);
        FileName = StrDuplicate (L"EFI\\Microsoft\\Boot\\bootmgfw.efi");
        if (FileExists (Volume->RootDir, FileName) &&
            !FilenameIn (
                Volume,
                L"EFI\\Microsoft\\Boot",
                L"bootmgfw.efi",
                GlobalConfig.DontScanFiles
            )
        ) {
            DisplayLoader = TRUE;
            TmpMsg = (FoundBRBackup)
                ? L"Instance: Assumed UEFI Windows (Potentially GRUB)"
                : L"Instance: Windows (UEFI)";
            AddLoaderEntry (FileName, TmpMsg, Volume, TRUE, FALSE);

            if (DuplicatesFallback (Volume, FileName)) {
                ScanFallbackLoader = FALSE;
            }
        }
        MY_FREE_POOL(FileName);

        //BREAD_CRUMB(L"%a:  7a 4", __func__);
    } // if ShouldScan

    //BREAD_CRUMB(L"%a:  8", __func__);
    // Scan the root directory for EFI executables
    if (ScanLoaderDir (Volume, L"\\", MatchPatterns)) {
        //BREAD_CRUMB(L"%a:  8a 1", __func__);
        ScanFallbackLoader = FALSE;
    }

    // Scan subdirectories of the EFI directory (as per the standard)
    //BREAD_CRUMB(L"%a:  9", __func__);
    DirIterOpen (Volume->RootDir, L"EFI", &EfiDirIter);

    //BREAD_CRUMB(L"%a:  10", __func__);
    while (DirIterNext (&EfiDirIter, 1, NULL, &EfiDirEntry)) {
        //LOG_SEP(L"X");
        //BREAD_CRUMB(L"%a:  10a 1 - WHILE LOOP:- START", __func__);
        do {
            //BREAD_CRUMB(L"%a:  10a 1a 1", __func__);
            Extension = FindExtension (EfiDirEntry->FileName);

            //BREAD_CRUMB(L"%a:  10a 1a 2", __func__);
            if (EfiDirEntry->FileName[0] ==  '.'             ||
                MyStriCmp  (EfiDirEntry->FileName, L"tools") ||
                (
                    Extension != NULL &&
                    !MyStriCmp (Extension, L".efi")
                )
            ) {
                // Skip ... Not boot loader or scanned later
                break;
            }

            //BREAD_CRUMB(L"%a:  10a 1a 3", __func__);
            FileName = PoolPrint (L"EFI\\%s", EfiDirEntry->FileName);

            //BREAD_CRUMB(L"%a:  10a 1a 4", __func__);
            if (ScanLoaderDir (Volume, FileName, MatchPatterns)) {
                //BREAD_CRUMB(L"%a:  10a 1a 4a 1", __func__);
                ScanFallbackLoader = FALSE;
            }
            MY_FREE_POOL(FileName);
            //BREAD_CRUMB(L"%a:  10a 1a 5", __func__);
        } while (0); // This 'loop' only runs once

        //BREAD_CRUMB(L"%a:  10a 2", __func__);
        MY_FREE_POOL(Extension);
        MY_FREE_POOL(EfiDirEntry);

        //BREAD_CRUMB(L"%a:  10a 3 - WHILE LOOP:- END", __func__);
        //LOG_SEP(L"X");
    } // while

    //BREAD_CRUMB(L"%a:  11", __func__);
    Status = DirIterClose (&EfiDirIter);

    //BREAD_CRUMB(L"%a:  12", __func__);
    if (EFI_ERROR(Status)            &&
        Status != EFI_NOT_FOUND      &&
        Status != EFI_INVALID_PARAMETER
    ) {
        //BREAD_CRUMB(L"%a:  12a 1", __func__);
        Temp = PoolPrint (
            L"While Scanning the EFI System Partition on '%s'",
            Volume->VolName
        );
        //BREAD_CRUMB(L"%a:  12a 2", __func__);
        CheckError (Status, Temp);
        MY_FREE_POOL(Temp);
        //BREAD_CRUMB(L"%a:  12a 3", __func__);
    }

    //BREAD_CRUMB(L"%a:  13", __func__);
    // Do not scan fallback loader if on the same volume and a duplicate of RefindPlus.
    if (ScanFallbackLoader) {
        //BREAD_CRUMB(L"%a:  13a 1", __func__);
        SelfPath = DevicePathToStr (SelfLoadedImage->FilePath);

        //BREAD_CRUMB(L"%a:  13a 2", __func__);
        CleanUpPathNameSlashes (SelfPath);

        //BREAD_CRUMB(L"%a:  13a 3", __func__);
        if (DuplicatesFallback (Volume, SelfPath) &&
            Volume->DeviceHandle == SelfLoadedImage->DeviceHandle

        ) {
            //BREAD_CRUMB(L"%a:  13a 3a 1", __func__);
            ScanFallbackLoader = FALSE;
        }
        MY_FREE_POOL(SelfPath);
        //BREAD_CRUMB(L"%a:  13a 4", __func__);
    }

    //BREAD_CRUMB(L"%a:  14", __func__);
    // Scan user-specified (or additional default) directories.
    i = 0;
    VolName = NULL;
    while (
        ScanFallbackLoader &&
        (Directory = FindCommaDelimited (GlobalConfig.AlsoScan, i++)) != NULL
    ) {
        //LOG_SEP(L"X");
        //BREAD_CRUMB(L"%a:  14a 1 - WHILE LOOP:- START", __func__);
        if (ShouldScan (Volume, Directory)) {
            //BREAD_CRUMB(L"%a:  14a 1a 1", __func__);
            SplitVolumeAndFilename (&Directory, &VolName);

            //BREAD_CRUMB(L"%a:  14a 1a 2", __func__);
            CleanUpPathNameSlashes (Directory);

            //BREAD_CRUMB(L"%a:  14a 1a 3", __func__);
            Length = StrLen (Directory);

            //BREAD_CRUMB(L"%a:  14a 1a 4", __func__);
            if (Length > 0 &&
                ScanLoaderDir (Volume, Directory, MatchPatterns)
            ) {
                //BREAD_CRUMB(L"%a:  14a 1a 4a 1", __func__);
                ScanFallbackLoader = FALSE;
            }
            //BREAD_CRUMB(L"%a:  14a 1a 5", __func__);
            MY_FREE_POOL(VolName);
        }
        MY_FREE_POOL(Directory);

        //BREAD_CRUMB(L"%a:  14a 2 - WHILE LOOP:- END", __func__);
        //LOG_SEP(L"X");
    } // while

VentoyJump:
    //BREAD_CRUMB(L"%a:  16", __func__);
    // Create an entry for fallback loaders
    if (ScanFallbackLoader &&
        ShouldScan (Volume, L"EFI\\BOOT") &&
        FileExists (Volume->RootDir, FALLBACK_FULLNAME) &&
        !FilenameIn (Volume, L"EFI\\BOOT", FALLBACK_BASENAME, GlobalConfig.DontScanFiles)
    ) {
        //BREAD_CRUMB(L"%a:  16a 1", __func__);
        if (FoundVentoy) {
            //BREAD_CRUMB(L"%a:  16a 1a 1", __func__);
            TmpMsg = L"Instance: Ventoy";
        }
        else {
            //BREAD_CRUMB(L"%a:  16a 1b 1", __func__);
            TmpMsg = L"Fallback Loader";
        }

        //BREAD_CRUMB(L"%a:  16a 2", __func__);
        DisplayLoader = TRUE;
        Temp = StrDuplicate (FALLBACK_FULLNAME);
        AddLoaderEntry (Temp, TmpMsg, Volume, TRUE, FALSE);
        MY_FREE_POOL(Temp);
        //BREAD_CRUMB(L"%a:  16a 2", __func__);
    }

    //BREAD_CRUMB(L"%a:  17 - END:- VOID", __func__);
    //LOG_DECREMENT();
    //LOG_SEP(L"X");
} // static VOID ScanEfiFiles()

// Scan internal disks for valid EFI boot loaders.
static
VOID ScanInternal (VOID) {
    UINTN VolumeIndex;


    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"Internal Disk Volumes with Mode:- 'UEFI'");
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);

    DisplayLoader = FALSE;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_INTERNAL) {
            ScanEfiFiles (Volumes[VolumeIndex]);
        }
    } // for

    #if REFIT_DEBUG > 0
    if (!DisplayLoader) {
        ALT_LOG(1, LOG_STAR_HEAD_SEP, L"None Found");
    }
    #endif

    DisplayLoader = FALSE;

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // static VOID ScanInternal()

// Scan external disks for valid EFI boot loaders.
static
VOID ScanExternal (VOID) {
    UINTN VolumeIndex;


    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"External Disk Volumes with Mode:- 'UEFI'");
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);

    DisplayLoader = FALSE;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_EXTERNAL) {
            ScanEfiFiles (Volumes[VolumeIndex]);
        }
    } // for

    #if REFIT_DEBUG > 0
    if (!DisplayLoader) {
        ALT_LOG(1, LOG_STAR_HEAD_SEP, L"None Found");
    }
    #endif

    DisplayLoader = FALSE;

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // static VOID ScanExternal()

// Scan internal disks for valid EFI boot loaders.
static
VOID ScanOptical (VOID) {
    UINTN VolumeIndex;


    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"Optical Discs with Mode:- 'UEFI'");
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);

    DisplayLoader = FALSE;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_OPTICAL) {
            ScanEfiFiles (Volumes[VolumeIndex]);
        }
    } // for

    #if REFIT_DEBUG > 0
    if (!DisplayLoader) {
        ALT_LOG(1, LOG_STAR_HEAD_SEP, L"None Found");
    }
    #endif

    DisplayLoader = FALSE;

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // static VOID ScanOptical()

// Scan options stored in UEFI firmware's boot list. Adds discovered and allowed
// items to the specified Row.
// If MatchThis != NULL, only adds items with labels containing any element of
// the MatchThis comma-delimited string; otherwise, searches for anything that
// does not match GlobalConfig.DontScanFirmware or the contents of the
// HiddenFirmware UEFI variable.
// If Icon != NULL, uses the specified icon; otherwise tries to find one to
// match the label.
static
VOID ScanFirmwareDefined (
    IN UINTN     Row,
    IN CHAR16   *MatchThis  OPTIONAL,
    IN EG_IMAGE *Icon       OPTIONAL
) {
    UINTN            index;
    CHAR16          *SkipThese;
    CHAR16          *OneItem;
    BOOLEAN          ScanIt;
    BOOT_ENTRY_LIST *ThisEntry;
    BOOT_ENTRY_LIST *BootEntries;

    #if REFIT_DEBUG > 0
    EFI_STATUS       Status;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);

    if (Row == 0) {
        ALT_LOG(1, LOG_THREE_STAR_SEP, L"Firmware Defined Boot Options");
    }
    #endif

    SkipThese = ReadHiddenTags (L"HiddenFirmware");

    if (GlobalConfig.DontScanFirmware != NULL) {
        if (SkipThese == NULL) {
            SkipThese = StrDuplicate (
                GlobalConfig.DontScanFirmware
            );
        }
        else {
            MergeUniqueItems (
                &SkipThese, GlobalConfig.DontScanFirmware, L','
            );
        }
    }

    if (Row == 0) {
        if (SkipThese == NULL) {
            SkipThese = StrDuplicate (L"shell");
        }
        else {
            MergeUniqueStrings (&SkipThese, L"shell", L',');
        }
    }

    #if REFIT_DEBUG > 0
    Status = EFI_NOT_FOUND;

    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Firmware Defined Option Scan Match List:- '%s'",
        (MatchThis != NULL) ? MatchThis : L"NULL"
    );

    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Firmware Defined Option Scan Exclusions:- '%s'",
        (SkipThese != NULL) ? SkipThese : L"NULL"
    );
    #endif

    BootEntries = FindBootOrderEntries();
    ThisEntry   =            BootEntries;

    while (ThisEntry != NULL) {
        ScanIt = FALSE;

        if (MatchThis == NULL) {
            if (SkipThese == NULL ||
                !IsListItemSubstringIn (ThisEntry->BootEntry.Label, SkipThese)
            ) {
                ScanIt = TRUE;
            }
        }
        else {
            index = 0;
            while (
                !ScanIt &&
                (OneItem = FindCommaDelimited (MatchThis, index++)) != NULL
            ) {
                if (StriSubCmp (OneItem, ThisEntry->BootEntry.Label) &&
                    !IsListItemSubstringIn (ThisEntry->BootEntry.Label, SkipThese)
                ) {
                    ScanIt = TRUE;
                }

                MY_FREE_POOL(OneItem);
            } // while
        }

        if (ScanIt) {
            AddEfiLoaderEntry (
                ThisEntry->BootEntry.DevPath,
                ThisEntry->BootEntry.Label,
                ThisEntry->BootEntry.BootNum,
                Row, Icon
            );

            #if REFIT_DEBUG > 0
            Status = EFI_SUCCESS;
            #endif
        }

        ThisEntry = ThisEntry->NextBootEntry;
    } // while

    MY_FREE_POOL(SkipThese);
    DeleteBootOrderEntries (BootEntries);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_END,
        L"Evaluate and Add Firmware Defined Options:- '%r'", Status
    );
    #endif

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // static VOID ScanFirmwareDefined()

// Default volume badge icon based on disk kind
EG_IMAGE * GetDiskBadge (
    IN UINTN DiskType
) {
    EG_IMAGE *Badge;

    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_BADGES) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"Skip ... Config Setting is Active:- 'HideUI Badges'"
        );
        #endif

        return NULL;
    }

    switch (DiskType) {
        case BBS_HARDDISK: Badge = BuiltinIcon (BUILTIN_ICON_VOL_INTERNAL); break;
        case BBS_USB:      Badge = BuiltinIcon (BUILTIN_ICON_VOL_EXTERNAL); break;
        case BBS_CDROM:    Badge = BuiltinIcon (BUILTIN_ICON_VOL_OPTICAL) ; break;
        default:           Badge = NULL;
    } // switch

    return Badge;
} // EG_IMAGE * GetDiskBadge()

//
// pre-boot tool functions
//

static
LOADER_ENTRY * AddToolEntry (
    IN REFIT_VOLUME *Volume,
    IN CHAR16       *LoaderPath,
    IN CHAR16       *LoaderTitle,
    IN EG_IMAGE     *Image,
    IN CHAR16        ShortcutLetter,
    IN BOOLEAN       UseGraphicsMode
) {
    LOADER_ENTRY *Entry;

    Entry = AllocateZeroPool (sizeof (LOADER_ENTRY));
    if (Entry == NULL) {
        return NULL;
    }

    Entry->me.Title          = (LoaderTitle != NULL) ? LoaderTitle : StrDuplicate (L"Unknown Tool");
    Entry->me.Tag            = TAG_TOOL;
    Entry->me.Row            = 1;
    Entry->me.ShortcutLetter = ShortcutLetter;
    Entry->me.Image          = egCopyImage (Image);
    Entry->LoaderPath        = (LoaderPath != NULL) ? LoaderPath : NULL;
    Entry->Volume            = Volume;
    Entry->UseGraphicsMode   = UseGraphicsMode;

    AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *) Entry);

    return Entry;
} // static LOADER_ENTRY * AddToolEntry()


// Checks to see if a specified file seems to be a valid tool.
// Returns TRUE if it passes all tests, FALSE otherwise
static
BOOLEAN IsValidTool (
    REFIT_VOLUME *BaseVolume,
    CHAR16       *PathName
) {
    UINTN    i;
    CHAR16  *DontVolName;
    CHAR16  *TestVolName;
    CHAR16  *TestPathName;
    CHAR16  *TestFileName;
    CHAR16  *DontPathName;
    CHAR16  *DontFileName;
    CHAR16  *DontScanThis;
    CHAR16  *DontScanTools;
    BOOLEAN  retval;

    #if REFIT_DEBUG > 0
    BOOLEAN  CheckMute = FALSE;
    #endif


    if (!FileExists (BaseVolume->RootDir, PathName)) {
        // Early return ... File does not exist
        return FALSE;
    }

    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_SET;
    #endif
    retval = IsValidLoader (BaseVolume->RootDir, PathName);
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    if (!retval) {
        // Early return
        return FALSE;
    }

    if (gHiddenTools == NULL) {
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif

        DontScanTools = ReadHiddenTags (L"HiddenTools");
        gHiddenTools  = (DontScanTools != NULL)
            ? StrDuplicate (DontScanTools)
            : StrDuplicate (L"NotSet");

        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif
    }
    else if (!MyStriCmp (gHiddenTools, L"NotSet")) {
        DontScanTools = StrDuplicate (gHiddenTools);
    }
    else {
        DontScanTools = NULL;
    }

    if (GlobalConfig.DontScanTools) {
        MergeUniqueStrings (
            &DontScanTools, GlobalConfig.DontScanTools, L','
        );
    }

    if (DontScanTools == NULL) {
        // Early return ... Assume valid
        return TRUE;
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Check File is Valid:- '%s'",
        PathName
    );
    #endif

    retval       = TRUE;
    TestVolName  = TestPathName = TestFileName = NULL;
    DontPathName = DontFileName = DontVolName  = NULL;
    SplitPathName (PathName, &TestVolName, &TestPathName, &TestFileName);

    i = 0;
    while (
        retval &&
        (DontScanThis = FindCommaDelimited (DontScanTools, i++)) != NULL
    ) {
        SplitPathName (DontScanThis, &DontVolName, &DontPathName, &DontFileName);

        if (MyStriCmp (TestFileName, DontFileName) &&
            (
                !DontPathName ||
                MyStriCmp (TestPathName, DontPathName)
            ) && (
                !DontVolName  ||
                VolumeMatchesDescription (BaseVolume, DontVolName)
            )
        ) {
            retval = FALSE;
        }

        MY_FREE_POOL(DontVolName);
        MY_FREE_POOL(DontPathName);
        MY_FREE_POOL(DontFileName);
        MY_FREE_POOL(DontScanThis);
    } // while

    MY_FREE_POOL(TestVolName);
    MY_FREE_POOL(TestPathName);
    MY_FREE_POOL(TestFileName);
    MY_FREE_POOL(DontScanTools);

    return retval;
} // static BOOLEAN IsValidTool()

// Locate a single tool from the specified Locations using one of the
// specified Names and add it to the menu.
static
BOOLEAN FindTool (
    CHAR16 *Locations,
    CHAR16 *Names,
    CHAR16 *Description,
    UINTN   Icon
) {
    UINTN    i, j;
    UINTN    VolumeIndex;
    CHAR16  *DirName;
    CHAR16  *FileName;
    CHAR16  *PathName;
    CHAR16  *ToolData;
    BOOLEAN  FoundTool;

    #if REFIT_DEBUG > 0
    CHAR16 *ToolStr;
    #endif

    DirName   =  NULL;
    FoundTool = FALSE;

    i = 0;
    while ((DirName = FindCommaDelimited (Locations, i++)) != NULL) {
        j = 0;
        while ((FileName = FindCommaDelimited (Names, j++)) != NULL) {
            PathName = StrDuplicate (DirName);
            MergeStrings (&PathName, FileName, MyStriCmp (PathName, L"\\") ? 0 : L'\\');

            for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                if (Volumes[VolumeIndex]->RootDir == NULL ||
                    !IsValidTool (Volumes[VolumeIndex], PathName)
                ) {
                    continue;
                }

                // DA-TAG: Do not free 'ToolData'
                //         Used in 'AddToolEntry'
                ToolData = PoolPrint (
                    L"%s on %s%s via %s",
                    Description,
                    Volumes[VolumeIndex]->VolName,
                    SetVolType (
                        NULL,
                        Volumes[VolumeIndex]->VolName,
                        Volumes[VolumeIndex]->FSType
                    ),
                    PathName
                );

                #if REFIT_DEBUG > 0
                ALT_LOG(1, LOG_LINE_NORMAL,
                    L"Adding Tag for '%s' on '%s'",
                    FileName, Volumes[VolumeIndex]->VolName
                );
                #endif

                AddToolEntry (
                    Volumes[VolumeIndex],
                    StrDuplicate (PathName),
                    ToolData, BuiltinIcon (Icon),
                    'S', FALSE
                );

                #if REFIT_DEBUG > 0
                ToolStr = PoolPrint (
                    L"Added Tool:- '%-18s     :::     %s'",
                    Description, PathName
                );

                ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);

                if (FoundTool) {
                    LOG_MSG("%s%s", OffsetNext, Spacer);
                }
                LOG_MSG("%s", ToolStr);
                MY_FREE_POOL(ToolStr);
                #endif

                FoundTool = TRUE;
            } // for

            MY_FREE_POOL(PathName);
            MY_FREE_POOL(FileName);
        } // while Names

        MY_FREE_POOL(DirName);
    } // while Locations

    return FoundTool;
} // static BOOLEAN FindTool()

// Locates boot loaders.
// NOTE: This assumes that GlobalConfig.LegacyType is correctly set.
VOID ScanForBootloaders (VOID) {
    UINTN     i;
    UINTN     SetOptions;
    CHAR16   *HiddenTags;
    CHAR16   *HiddenLegacy;
    CHAR16   *DontScanItem;
    CHAR16   *OrigDontScanDirs;
    CHAR16   *OrigDontScanFiles;
    CHAR16   *OrigDontScanVolumes;
    BOOLEAN   DeleteItem;
    BOOLEAN   ScanForLegacy;
    BOOLEAN   AmendedDontScan;

    #if REFIT_DEBUG > 0
    UINTN     KeyNum;
    CHAR16   *MsgStr;
    CHAR16   *LogSection;
    BOOLEAN   LogNewLine;
    BOOLEAN   TmpLevel;
    #endif

    ScanningLoaders = TRUE;


    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    MsgStr = StrDuplicate (L"S E E K   I N S T A N C E   L O A D E R S");
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
    LOG_MSG("%s", MsgStr);
    LOG_MSG("\n");
    MY_FREE_POOL(MsgStr);
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);

    // Determine up-front if we will be scanning for legacy loaders
    ScanForLegacy = FALSE;
    for (i = 0; i < NUM_SCAN_OPTIONS; i++) {
        if (GlobalConfig.ScanFor[i] == 'c' || GlobalConfig.ScanFor[i] == 'C' ||
            GlobalConfig.ScanFor[i] == 'h' || GlobalConfig.ScanFor[i] == 'H' ||
            GlobalConfig.ScanFor[i] == 'b' || GlobalConfig.ScanFor[i] == 'B'
        ) {
            ScanForLegacy = TRUE;
            break;
        }
    } // for

    // Update NVRAM boot manager list if this is UEFI legacy type unit,
    // scanning for legacy loaders and deep legacy scan is active
    if (ScanForLegacy                            &&
        GlobalConfig.DeepLegacyScan              &&
        GlobalConfig.LegacyType == LEGACY_TYPE_UEFI
    ) {
        BdsDeleteAllInvalidLegacyBootOptions();
        BdsAddNonExistingLegacyBootOptions();
    }

    OrigDontScanFiles = OrigDontScanVolumes = NULL;
    if (GlobalConfig.HiddenTags) {
        // We temporarily modify GlobalConfig.DontScanFiles and GlobalConfig.DontScanVolumes
        // to include contents of EFI HiddenTags and HiddenLegacy variables so that we do not
        // have to re-load these UEFI variables in several functions called from this one. To
        // do this, we must be able to restore the original contents, so back them up first.
        // We *DO NOT* do this with the GlobalConfig.DontScanFirmware and
        // GlobalConfig.DontScanTools variables because they are used in only one function
        // each, so it is easier to create a temporary variable for the merged contents
        // there and not modify the global variable.
        OrigDontScanFiles   = StrDuplicate (GlobalConfig.DontScanFiles);
        OrigDontScanVolumes = StrDuplicate (GlobalConfig.DontScanVolumes);

        // Add hidden tags to two GlobalConfig.DontScan* variables.
        HiddenTags = ReadHiddenTags (L"HiddenTags");
        if (HiddenTags && StrLen (HiddenTags) > 0) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"Merge HiddenTags into 'Dont Scan Files':- '%s'",
                HiddenTags
            );
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            #endif

            MergeStrings (&GlobalConfig.DontScanFiles, HiddenTags, L',');
        }
        MY_FREE_POOL(HiddenTags);

        HiddenLegacy = ReadHiddenTags (L"HiddenLegacy");
        if (HiddenLegacy && StrLen (HiddenLegacy) > 0) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"Merge HiddenLegacy into 'Dont Scan Volumes':- '%s'",
                HiddenLegacy
            );
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            #endif

            MergeStrings (&GlobalConfig.DontScanVolumes, HiddenLegacy, L',');
        }

        MY_FREE_POOL(HiddenLegacy);
    } // if GlobalConfig.HiddenTags

    // DA-TAG: Override Hidden/Disabled PreBoot Volumes if SyncAPFS is Active
    OrigDontScanDirs =  NULL;
    AmendedDontScan  = FALSE;
    if (GlobalConfig.SyncAPFS) {
        DontScanItem = NULL;
        OrigDontScanDirs = StrDuplicate (GlobalConfig.DontScanDirs);

        if (GlobalConfig.DontScanFiles) {
            i = 0;
            while ((DontScanItem = FindCommaDelimited (GlobalConfig.DontScanFiles, i)) != NULL) {
                DeleteItem = (!FindSubStr (DontScanItem, L"PreBoot:"))
                    ? (MyStriCmp (DontScanItem, L"PreBoot")) ? TRUE : FALSE
                    : TRUE;

                if (!DeleteItem) {
                    i++;
                }
                else {
                    DeleteItemFromCsvList (DontScanItem, GlobalConfig.DontScanFiles);
                    AmendedDontScan = TRUE;
                }

                MY_FREE_POOL(DontScanItem);
            } // while
        } // if GlobalConfig.DontScanFiles

        if (GlobalConfig.DontScanDirs) {
            i = 0;
            while ((DontScanItem = FindCommaDelimited (GlobalConfig.DontScanDirs, i)) != NULL) {
                DeleteItem = (!FindSubStr (DontScanItem, L"PreBoot:"))
                    ? (MyStriCmp (DontScanItem, L"PreBoot")) ? TRUE : FALSE
                    : TRUE;

                if (!DeleteItem) {
                    i++;
                }
                else {
                    DeleteItemFromCsvList (DontScanItem, GlobalConfig.DontScanDirs);
                    AmendedDontScan = TRUE;
                }

                MY_FREE_POOL(DontScanItem);
            } // while
        } // if GlobalConfig.DontScanDirs

        if (GlobalConfig.DontScanVolumes) {
            i = 0;
            while ((DontScanItem = FindCommaDelimited (GlobalConfig.DontScanVolumes, i)) != NULL) {
                DeleteItem = (FindSubStr (DontScanItem, L"PreBoot:"))
                    ? TRUE
                    : (MyStriCmp (DontScanItem, L"PreBoot")) ? TRUE : FALSE;

                if (!DeleteItem) {
                    i++;
                }
                else {
                    DeleteItemFromCsvList (DontScanItem, GlobalConfig.DontScanVolumes);
                    AmendedDontScan = TRUE;
                }

                MY_FREE_POOL(DontScanItem);
            } // while
        } // if GlobalConfig.DontScanVolumes

        #if REFIT_DEBUG > 0
        if (AmendedDontScan) {
            ALT_LOG(1, LOG_STAR_SEPARATOR,
                L"Ignored PreBoot Volumes in 'Dont Scan' Lists ... Config Setting is *NOT* Active:- 'disable_apfs_sync'"
            );
        }
        #endif
    } // if GlobalConfig.SyncAPFS

    // Get count of options set to be scanned
    SetOptions = 0;
    for (i = 0; i < NUM_SCAN_OPTIONS; i++) {
        switch (GlobalConfig.ScanFor[i]) {
            case 'm': case 'M':
            case 'i': case 'I':
            case 'h': case 'H':
            case 'e': case 'E':
            case 'b': case 'B':
            case 'o': case 'O':
            case 'c': case 'C':
            case 'n': case 'N':
            case 'f': case 'F':
            SetOptions = SetOptions + 1;
        } // switch
    } // for

    #if REFIT_DEBUG > 0
    LogNewLine = FALSE;
    #endif

    // Scan for loaders and tools, add them to the menu
    for (i = 0; i <= SetOptions; i++) {
        switch (GlobalConfig.ScanFor[i]) {
            case 'm': case 'M':
                #if REFIT_DEBUG > 0
                if (LogNewLine) LOG_MSG("\n");
                LogNewLine = TRUE;

                LOG_MSG("Scan Manual:");
                ALT_LOG(1, LOG_THREE_STAR_SEP, L"Manually Defined Stanzas");
                #endif

                ScanUserConfigured (GlobalConfig.ConfigFilename);
                break;

            case 'i': case 'I':
                #if REFIT_DEBUG > 0
                if (LogNewLine) LOG_MSG("\n");
                LogNewLine = TRUE;

                LOG_MSG("Scan Internal:");
                #endif

                ScanInternal();
                break;

            case 'h': case 'H':
                #if REFIT_DEBUG > 0
                if (LogNewLine) LOG_MSG("\n");
                LogNewLine = TRUE;

                LOG_MSG("Scan Internal (Legacy):");
                #endif

                ScanLegacyInternal();
                break;

            case 'e': case 'E':
                #if REFIT_DEBUG > 0
                if (LogNewLine) LOG_MSG("\n");
                LogNewLine = TRUE;

                LOG_MSG("Scan External:");
                #endif

                ScanExternal();
                break;

            case 'b': case 'B':
            #if REFIT_DEBUG > 0
                if (LogNewLine) LOG_MSG("\n");
                LogNewLine = TRUE;

                LOG_MSG("Scan External (Legacy):");
                #endif

                ScanLegacyExternal();
                break;

            case 'o': case 'O':
                #if REFIT_DEBUG > 0
                if (LogNewLine) LOG_MSG("\n");
                LogNewLine = TRUE;

                LOG_MSG("Scan Optical:");
                #endif

                ScanOptical();
                break;

            case 'c': case 'C':
            #if REFIT_DEBUG > 0
                if (LogNewLine) LOG_MSG("\n");
                LogNewLine = TRUE;

                LOG_MSG("Scan Optical (Legacy):");
                #endif

                ScanLegacyDisc();
                break;

            case 'n': case 'N':
                #if REFIT_DEBUG > 0
                if (LogNewLine) LOG_MSG("\n");
                LogNewLine = TRUE;

                LOG_MSG("Scan Net Boot:");
                #endif

                ScanNetboot();
                break;

            case 'f': case 'F':
                #if REFIT_DEBUG > 0
                if (LogNewLine) LOG_MSG("\n");
                LogNewLine = TRUE;

                LOG_MSG("Scan Firmware:");
                #endif

                ScanFirmwareDefined (0, NULL, NULL);
                break;
        } // switch
    } // for

    #if REFIT_DEBUG > 0
    // Reset
    LogNewLine = FALSE;
    #endif

    if (GlobalConfig.HiddenTags) {
        // Restore the backed-up GlobalConfig.DontScan* variables
        MY_FREE_POOL(GlobalConfig.DontScanFiles);
        MY_FREE_POOL(GlobalConfig.DontScanVolumes);
        GlobalConfig.DontScanFiles   = OrigDontScanFiles;
        GlobalConfig.DontScanVolumes = OrigDontScanVolumes;
    }

    if (!AmendedDontScan) {
        MY_FREE_POOL(OrigDontScanDirs);
    }

    if (OrigDontScanDirs) {
        // Restore the stashed GlobalConfig.DontScanDirs variable
        MY_FREE_POOL(GlobalConfig.DontScanDirs);
        GlobalConfig.DontScanDirs = OrigDontScanDirs;
    }

    if (MainMenu->EntryCount == 0) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"Could *NOT* Locate Valid Instance Loaders");
        TmpLevel = (GlobalConfig.LogLevel == 0) ? TRUE : FALSE;
        if (TmpLevel) {
            GlobalConfig.LogLevel = 1;
        }
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
        if (TmpLevel) {
            GlobalConfig.LogLevel = 0;
        }
        MY_FREE_POOL(MsgStr);
        #endif
    }
    else {
        // Assign shortcut keys
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_STAR_HEAD_SEP,
            L"Located %d Instance Loader%s",
            MainMenu->EntryCount,
            (MainMenu->EntryCount == 1) ? L"" : L"s"
        );
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");

        LogSection = L"A S S I G N   S H O R T C U T   K E Y S";
        ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", LogSection);
        LOG_MSG("\n\n");
        LOG_MSG("%s", LogSection);

        if (GlobalConfig.DirectBoot) {
            MsgStr = StrDuplicate (L"Skip Shortcut Key Setup ... 'DirectBoot' is Active");
            LOG_MSG("%s", MsgStr); // MsgStr is freed later
        }
        else {
            MsgStr = StrDuplicate (L"Set Shortcuts");
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("\n");
            LOG_MSG("%s:", MsgStr);
            MY_FREE_POOL(MsgStr);
        }

        KeyNum = 0;
        #endif

        do {
            if (GlobalConfig.DirectBoot) {
                break;
            }

            for (i = 0; i < MainMenu->EntryCount && MainMenu->Entries[i]->Row == 0; i++) {
                if (i > 9) {
                    break;
                }

                if (i < 9) {
                    #if REFIT_DEBUG > 0
                    KeyNum = i + 1;
                    #endif

                    MainMenu->Entries[i]->ShortcutDigit = (CHAR16) ('1' + i);
                }
                else { // i == 9
                    MainMenu->Entries[i]->ShortcutDigit = (CHAR16) ('9' - i);

                    #if REFIT_DEBUG > 0
                    KeyNum = 0;
                    #endif
                }

                #if REFIT_DEBUG > 0
                MsgStr = PoolPrint (
                    L"Set Key '%d' to Run Item:- '%s'",
                    KeyNum, MainMenu->Entries[i]->Title
                );
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("%s  - %s", OffsetNext, MsgStr);
                MY_FREE_POOL(MsgStr);
                #endif
            }  // for

            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (
                L"Assigned Shortcut Key%s to %d of %d Instance Loader%s",
                (i == 1) ? L"" : L"s",
                i, MainMenu->EntryCount,
                (MainMenu->EntryCount == 1) ? L"" : L"s"
            );
            LOG_MSG("\n\n");
            LOG_MSG("INFO: %s", MsgStr);
            ALT_LOG(1, LOG_STAR_HEAD_SEP, L"%s", MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
            #endif
        } while (0); // This 'loop' only runs once
    } // if/else MainMenu->EntryCount

    // Wait for user acknowledgement if there were errors
    FinishTextScreen (FALSE);

    ScanningLoaders = FALSE;

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID ScanForBootloaders()

// Add the second-row tags containing built-in and external tools
VOID ScanForTools (VOID) {
    EFI_STATUS              Status;
    UINTN                   i, j, k;
    UINTN                   ToolTotal;
    UINTN                   VolumeIndex;
    VOID                   *ItemBuffer;
    CHAR16                 *TmpStr;
    CHAR16                 *ToolName;
    CHAR16                 *FileName;
    CHAR16                 *VolumeTag;
    CHAR16                 *RecoverVol;
    CHAR16                 *MokLocations;
    CHAR16                 *Description;
    UINT64                  osind;
    UINT32                  CsrValue;
    BOOLEAN                 FoundTool;
    BOOLEAN                 OtherFind;
    BOOLEAN                 FoundThis;

    REFIT_MENU_ENTRY *MenuEntryPreCleanNvram;
    REFIT_MENU_ENTRY *MenuEntryHiddenTags   ;
    REFIT_MENU_ENTRY *MenuEntryBootorder    ;
    REFIT_MENU_ENTRY *MenuEntryShutdown     ;
    REFIT_MENU_ENTRY *MenuEntryReset        ;
    REFIT_MENU_ENTRY *MenuEntryExit         ;
    REFIT_MENU_ENTRY *MenuEntryAbout        ;
    REFIT_MENU_ENTRY *MenuEntryInstall      ;
    REFIT_MENU_ENTRY *MenuEntryFirmware     ;
    REFIT_MENU_ENTRY *MenuEntryRotateCsr    ;

    #ifdef __MAKEWITH_TIANO
    BOOLEAN SkipSystemVolume;
    #endif

    #if REFIT_DEBUG > 0
    BOOLEAN   CheckMute = FALSE;
    BOOLEAN   FlagAPFS;
    CHAR16   *ToolStr;
    CHAR16   *LogSection = L"H A N D L E   T O O L   O P T I O N S";


    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", LogSection);
    LOG_MSG("%s", LogSection);

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);
    #endif

    if (GlobalConfig.DirectBoot) {
        #if REFIT_DEBUG > 0
        LogSection = L"INFO: Tool Options:- Skip ... 'DirectBoot' is Active";
        ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", LogSection);
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        LOG_MSG("\n");
        LOG_MSG("%s", LogSection);
        LOG_MSG("\n\n");
        #endif

        BREAD_CRUMB(L"%a:  Aa 1 - END:- VOID ... Exit on DirectBoot", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    ToolTotal = 0;
    for (i = 0; i < NUM_TOOLS; i++) {
        switch (GlobalConfig.ShowTools[i]) {
            case TAG_ABOUT:         break;
            case TAG_BOOTORDER:     break;
            case TAG_CLEAN_NVRAM:   break;
            case TAG_CSR_ROTATE:    break;
            case TAG_EXIT:          break;
            case TAG_FIRMWARE:      break;
            case TAG_FWUPDATE_TOOL: break;
            case TAG_INSTALL:       break;
            case TAG_GDISK:         break;
            case TAG_GPTSYNC:       break;
            case TAG_MEMTEST:       break;
            case TAG_HIDDEN:        break;
            case TAG_MOK_TOOL:      break;
            case TAG_NETBOOT:       break;
            case TAG_REBOOT:        break;
            case TAG_RECOVERY_MAC:  break;
            case TAG_RECOVERY_WIN:  break;
            case TAG_SHELL:         break;
            case TAG_SHUTDOWN:      break;
            default:             continue;
        } // switch
        ToolTotal++;
    } // for
    if (ToolTotal == 0) {
        #if REFIT_DEBUG > 0
        LogSection = L"INFO: Tool Options:- Skip ... Empty 'showtools' List";
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", LogSection);
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        LOG_MSG("\n");
        LOG_MSG("%s", LogSection);
        LOG_MSG("\n\n");
        #endif

        BREAD_CRUMB(L"%a:  Aa 2 - END:- VOID ... Exit on Empty List", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    #if REFIT_DEBUG > 0
    LogSection = L"Check and Set Items";
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", LogSection);
    LOG_MSG("\n");
    LOG_MSG("%s:", LogSection);
    #endif

    MokLocations = StrDuplicate (MOK_LOCATIONS);
    MergeUniqueStrings (&MokLocations, SelfDirPath, L',');

    ToolTotal = 0;
    VolumeTag = NULL;
    for (i = 0; i < NUM_TOOLS; i++) {
        switch (GlobalConfig.ShowTools[i]) {
            case TAG_ABOUT:         ToolName = LABEL_ABOUT       ;    break;
            case TAG_BOOTORDER:     ToolName = LABEL_BOOTORDER   ;    break;
            case TAG_CLEAN_NVRAM:   ToolName = LABEL_CLEAN_NVRAM ;    break;
            case TAG_CSR_ROTATE:    ToolName = LABEL_CSR_ROTATE  ;    break;
            case TAG_EXIT:          ToolName = LABEL_EXIT        ;    break;
            case TAG_FIRMWARE:      ToolName = LABEL_FIRMWARE    ;    break;
            case TAG_FWUPDATE_TOOL: ToolName = LABEL_FWUPDATE    ;    break;
            case TAG_INSTALL:       ToolName = LABEL_INSTALL     ;    break;
            case TAG_GDISK:         ToolName = LABEL_GDISK       ;    break;
            case TAG_GPTSYNC:       ToolName = LABEL_GPTSYNC     ;    break;
            case TAG_MEMTEST:       ToolName = LABEL_MEMTEST     ;    break;
            case TAG_HIDDEN:        ToolName = LABEL_HIDDEN      ;    break;
            case TAG_MOK_TOOL:      ToolName = LABEL_MOK         ;    break;
            case TAG_NETBOOT:       ToolName = LABEL_NETBOOT     ;    break;
            case TAG_REBOOT:        ToolName = LABEL_REBOOT      ;    break;
            case TAG_RECOVERY_MAC:  ToolName = LABEL_RECOVERY_MAC;    break;
            case TAG_RECOVERY_WIN:  ToolName = LABEL_RECOVERY_WIN;    break;
            case TAG_SHELL:         ToolName = LABEL_SHELL       ;    break;
            case TAG_SHUTDOWN:      ToolName = LABEL_SHUTDOWN    ;    break;
            default:                                               continue;
        } // switch
        ToolTotal++;

        #if REFIT_DEBUG > 0
        LOG_MSG("%s  - Tool List Item %02d ... ", OffsetNext, ToolTotal);
        #endif

        FoundTool = FALSE;
        switch (GlobalConfig.ShowTools[i]) {
            case TAG_CLEAN_NVRAM:
                MenuEntryPreCleanNvram = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
                if (MenuEntryPreCleanNvram) {
                    FoundTool = TRUE;

                    MenuEntryPreCleanNvram->Title          = StrDuplicate (ToolName);
                    MenuEntryPreCleanNvram->Tag            = TAG_CLEAN_NVRAM;
                    MenuEntryPreCleanNvram->Row            = 1;
                    MenuEntryPreCleanNvram->ShortcutDigit  = 0;
                    MenuEntryPreCleanNvram->ShortcutLetter = 0;
                    MenuEntryPreCleanNvram->Image          = BuiltinIcon (BUILTIN_ICON_TOOL_NVRAMCLEAN);

                    AddMenuEntry (MainMenu, MenuEntryPreCleanNvram);

                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (L"Added Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("%s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif
                }

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (L"Could *NOT* Load Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_SHUTDOWN:
                MenuEntryShutdown = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
                if (MenuEntryShutdown) {
                    FoundTool = TRUE;

                    MenuEntryShutdown->Title          = StrDuplicate (ToolName);
                    MenuEntryShutdown->Tag            = TAG_SHUTDOWN;
                    MenuEntryShutdown->Row            =  1;
                    MenuEntryShutdown->ShortcutDigit  =  0;
                    MenuEntryShutdown->ShortcutLetter = 'U';
                    MenuEntryShutdown->Image          = BuiltinIcon (BUILTIN_ICON_FUNC_SHUTDOWN);

                    AddMenuEntry (MainMenu, MenuEntryShutdown);

                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (L"Added Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("%s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif
                }

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (L"Could *NOT* Load Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_REBOOT:
                MenuEntryReset = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
                if (MenuEntryReset) {
                    FoundTool = TRUE;

                    MenuEntryReset->Title          = StrDuplicate (ToolName);
                    MenuEntryReset->Tag            = TAG_REBOOT;
                    MenuEntryReset->Row            =  1;
                    MenuEntryReset->ShortcutDigit  =  0;
                    MenuEntryReset->ShortcutLetter = 'R';
                    MenuEntryReset->Image          = BuiltinIcon (BUILTIN_ICON_FUNC_RESET);

                    AddMenuEntry (MainMenu, MenuEntryReset);

                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (L"Added Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("%s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif
                }

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (L"Could *NOT* Load Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_ABOUT:
                MenuEntryAbout = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
                if (MenuEntryAbout) {
                    FoundTool = TRUE;

                    MenuEntryAbout->Title          = StrDuplicate (ToolName);
                    MenuEntryAbout->Tag            = TAG_ABOUT;
                    MenuEntryAbout->Row            =  1;
                    MenuEntryAbout->ShortcutDigit  =  0;
                    MenuEntryAbout->ShortcutLetter = 'A';
                    MenuEntryAbout->Image          = BuiltinIcon (BUILTIN_ICON_FUNC_ABOUT);

                    AddMenuEntry (MainMenu, MenuEntryAbout);

                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (L"Added Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("%s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif
                }

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (L"Could *NOT* Load Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_EXIT:
                MenuEntryExit = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
                if (MenuEntryExit) {
                    FoundTool = TRUE;

                    MenuEntryExit->Title          = StrDuplicate (ToolName);
                    MenuEntryExit->Tag            = TAG_EXIT;
                    MenuEntryExit->Row            = 1;
                    MenuEntryExit->ShortcutDigit  = 0;
                    MenuEntryExit->ShortcutLetter = 0;
                    MenuEntryExit->Image          = BuiltinIcon (BUILTIN_ICON_FUNC_EXIT);

                    AddMenuEntry (MainMenu, MenuEntryExit);

                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (L"Added Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("%s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif
                }

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (
                        L"Could *NOT* Load Tool:- '%s'", ToolName
                    );
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_HIDDEN:
                if (!GlobalConfig.HiddenTags) {
                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (
                        L"Did *NOT* Enable Tool:- '%s'", ToolName
                    );
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG(" * NOTE *     %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif
                }
                else {
                    MenuEntryHiddenTags = AllocateZeroPool (
                        sizeof (REFIT_MENU_ENTRY)
                    );
                    if (MenuEntryHiddenTags == NULL) {
                        #if REFIT_DEBUG > 0
                        if (!FoundTool) {
                            ToolStr = PoolPrint (
                                L"Could *NOT* Load Tool:- '%s'",
                                ToolName
                            );
                            ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                            LOG_MSG("*_ WARN _*    %s", ToolStr);
                            MY_FREE_POOL(ToolStr);
                        }
                        #endif

                        break;
                    }

                    MenuEntryHiddenTags->Title          = StrDuplicate (ToolName);
                    MenuEntryHiddenTags->Tag            = TAG_HIDDEN;
                    MenuEntryHiddenTags->Row            = 1;
                    MenuEntryHiddenTags->ShortcutDigit  = 0;
                    MenuEntryHiddenTags->ShortcutLetter = 0;
                    MenuEntryHiddenTags->Image          = BuiltinIcon (BUILTIN_ICON_FUNC_HIDDEN);

                    AddMenuEntry (MainMenu, MenuEntryHiddenTags);

                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (L"Added Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("%s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif
                }

            break;
            case TAG_FIRMWARE:
                ItemBuffer = 0;

                if (EfivarGetRaw (
                    &GlobalGuid, L"OsIndicationsSupported",
                    &ItemBuffer, NULL
                ) != EFI_SUCCESS) {
                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (
                        L"Did *NOT* Enable Tool:- '%s' ... 'OsIndicationsSupported' Flag Was *NOT* Found",
                        ToolName
                    );
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG(" * NOTE *     %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif
                }
                else {
                    osind = *(UINT64 *) ItemBuffer;
                    if (osind & EFI_OS_INDICATIONS_BOOT_TO_FW_UI) {
                        MenuEntryFirmware = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
                        if (MenuEntryFirmware == NULL) {
                            #if REFIT_DEBUG > 0
                            ToolStr = PoolPrint (
                                L"Could *NOT* Load Tool:- '%s'", ToolName
                            );
                            ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                            LOG_MSG("*_ WARN _*    %s", ToolStr);
                            MY_FREE_POOL(ToolStr);
                            #endif
                        }
                        else {
                            MenuEntryFirmware->Title          = StrDuplicate (ToolName);
                            MenuEntryFirmware->Tag            = TAG_FIRMWARE;
                            MenuEntryFirmware->Row            = 1;
                            MenuEntryFirmware->ShortcutDigit  = 0;
                            MenuEntryFirmware->ShortcutLetter = 0;
                            MenuEntryFirmware->Image          = BuiltinIcon (BUILTIN_ICON_FUNC_FIRMWARE);

                            AddMenuEntry (MainMenu, MenuEntryFirmware);

                            #if REFIT_DEBUG > 0
                            ToolStr = PoolPrint (L"Added Tool:- '%s'", ToolName);
                            ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                            LOG_MSG("%s", ToolStr);
                            MY_FREE_POOL(ToolStr);
                            #endif
                        }
                    }
                    else {
                        #if REFIT_DEBUG > 0
                        ToolStr = PoolPrint (
                            L"Could *NOT* Find Tool:- '%s'", ToolName
                        );
                        ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                        LOG_MSG("*_ WARN _*    %s", ToolStr);
                        MY_FREE_POOL(ToolStr);
                        #endif
                    }
                    MY_FREE_POOL(ItemBuffer);
                }

            break;
            case TAG_SHELL:
                j = 0;
                OtherFind = FALSE;
                while ((FileName = FindCommaDelimited (SHELL_NAMES, j++)) != NULL) {
                    if (!IsValidTool (SelfVolume, FileName)) {
                        MY_FREE_POOL(FileName);

                        continue;
                    }

                    // DA-TAG: Do not free 'Description' or 'FileName'
                    //         Used in 'AddToolEntry'
                    Description = PoolPrint (
                        L"%s on %s%s via %s",
                        ToolName,
                        SelfVolume->VolName,
                        SetVolType (NULL, SelfVolume->VolName, SelfVolume->FSType),
                        FileName
                    );

                    #if REFIT_DEBUG > 0
                    ALT_LOG(1, LOG_LINE_NORMAL,
                        L"Add '%s' Tag:- '%s' on '%s'",
                        ToolName, FileName,
                        SelfVolume->VolName
                    );
                    #endif

                    FoundTool = TRUE;
                    AddToolEntry (
                        SelfVolume, FileName,
                        Description,
                        BuiltinIcon (BUILTIN_ICON_TOOL_SHELL),
                        'S', FALSE
                    );

                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (L"Added Tool:- '%-18s     :::     %s'", ToolName, FileName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    if (OtherFind) {
                        LOG_MSG("%s%s", OffsetNext, Spacer);
                    }
                    LOG_MSG("%s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif

                    OtherFind = TRUE;
                } // while

                if (!FoundTool) {
                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (
                        L"Could *NOT* Find Tool:- '%s'", ToolName
                    );
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif
                }

                #if REFIT_DEBUG > 0
                ALT_LOG(1, LOG_LINE_NORMAL,
                    L"Scan for Firmware Defined Shell Options"
                );
                #endif

                ScanFirmwareDefined (1, L"Shell", BuiltinIcon(BUILTIN_ICON_TOOL_SHELL));
            break;
            case TAG_GPTSYNC:
                j = 0;
                OtherFind = FALSE;
                while ((FileName = FindCommaDelimited (GPTSYNC_NAMES, j++)) != NULL) {
                    // DA-TAG: Do not free 'FileName'
                    //         If used in 'AddToolEntry'
                    if (!IsValidTool (SelfVolume, FileName)) {
                        MY_FREE_POOL(FileName);

                        continue;
                    }

                    // DA-TAG: Do not free 'Description' or 'FileName'
                    //         Used in 'AddToolEntry'
                    Description = PoolPrint (
                        L"%s on %s%s via %s",
                        ToolName,
                        SelfVolume->VolName,
                        SetVolType (NULL, SelfVolume->VolName, SelfVolume->FSType),
                        FileName
                    );

                    #if REFIT_DEBUG > 0
                    ALT_LOG(1, LOG_LINE_NORMAL,
                        L"Add '%s' Tag:- '%s' on '%s'",
                        ToolName, FileName,
                        SelfVolume->VolName
                    );
                    #endif

                    FoundTool = TRUE;
                    AddToolEntry (
                        SelfVolume, FileName,
                        Description,
                        BuiltinIcon (BUILTIN_ICON_TOOL_PART),
                        'P', FALSE
                    );

                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (L"Added Tool:- '%-18s     :::     %s'", ToolName, FileName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    if (OtherFind) {
                        LOG_MSG("%s%s", OffsetNext, Spacer);
                    }
                    LOG_MSG("%s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif

                    OtherFind = TRUE;
                } // while

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (
                        L"Could *NOT* Find Tool:- '%s'", ToolName
                    );
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_GDISK:
                j = 0;
                OtherFind = FALSE;
                while ((FileName = FindCommaDelimited (GDISK_NAMES, j++)) != NULL) {
                    // DA-TAG: Do not free 'FileName'
                    //         If used in 'AddToolEntry'
                    if (!IsValidTool (SelfVolume, FileName)) {
                        MY_FREE_POOL(FileName);

                        continue;
                    }

                    // DA-TAG: Do not free 'Description' or 'FileName'
                    //         Used in 'AddToolEntry'
                    Description = PoolPrint (
                        L"%s on %s%s via %s",
                        ToolName,
                        SelfVolume->VolName,
                        SetVolType (NULL, SelfVolume->VolName, SelfVolume->FSType),
                        FileName
                    );

                    #if REFIT_DEBUG > 0
                    ALT_LOG(1, LOG_LINE_NORMAL,
                        L"Add '%s' Tag:- '%s' on '%s'",
                        ToolName, FileName,
                        SelfVolume->VolName
                    );
                    #endif

                    FoundTool = TRUE;
                    AddToolEntry (
                        SelfVolume, FileName,
                        Description,
                        BuiltinIcon (BUILTIN_ICON_TOOL_PART),
                        'G', FALSE
                    );

                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (L"Added Tool:- '%-18s     :::     %s'", ToolName, FileName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    if (OtherFind) {
                        LOG_MSG("%s%s", OffsetNext, Spacer);
                    }
                    LOG_MSG("%s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif

                    OtherFind = TRUE;
                } // while

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (
                        L"Could *NOT* Find Tool:- '%s'", ToolName
                    );
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_NETBOOT:
                j = 0;
                OtherFind = FALSE;
                while ((FileName = FindCommaDelimited (NETBOOT_NAMES, j++)) != NULL) {
                    // DA-TAG: Do not free 'FileName'
                    //         If used in 'AddToolEntry'
                    if (!IsValidTool (SelfVolume, FileName)) {
                        MY_FREE_POOL(FileName);

                        continue;
                    }

                    // DA-TAG: Do not free 'Description' or 'FileName'
                    //         Used in 'AddToolEntry'
                    Description = PoolPrint (
                        L"%s on %s%s via %s",
                        ToolName,
                        SelfVolume->VolName,
                        SetVolType (NULL, SelfVolume->VolName, SelfVolume->FSType),
                        FileName
                    );

                    #if REFIT_DEBUG > 0
                    ALT_LOG(1, LOG_LINE_NORMAL,
                        L"Add '%s' Tag:- '%s' on '%s'",
                        ToolName, FileName,
                        SelfVolume->VolName
                    );
                    #endif

                    FoundTool = TRUE;
                    AddToolEntry (
                        SelfVolume, FileName,
                        Description,
                        BuiltinIcon (BUILTIN_ICON_TOOL_NETBOOT),
                        'N', FALSE
                    );

                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (L"Added Tool:- '%-18s     :::     %s'", ToolName, FileName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    if (OtherFind) {
                        LOG_MSG("%s%s", OffsetNext, Spacer);
                    }
                    LOG_MSG("%s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif

                    OtherFind = TRUE;
                } // while

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (
                        L"Could *NOT* Find Tool:- '%s'", ToolName
                    );
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_RECOVERY_MAC:
                OtherFind  = FALSE;
                RecoverVol =  NULL;
                for (VolumeIndex = 0; VolumeIndex < HfsRecoveryCount; VolumeIndex++) {
                    j = 0;
                    while ((FileName = FindCommaDelimited (GlobalConfig.MacOSRecoveryFiles, j++)) != NULL) {
                        // DA-TAG: Do not free 'FileName'
                        //         If used in 'AddToolEntry'
                        FoundThis = FALSE;
                        if ((HfsRecovery[VolumeIndex]->RootDir != NULL)     &&
                            (IsValidTool (HfsRecovery[VolumeIndex], FileName))
                        ) {
                            // Get a meaningful tag for the recovery tool
                            // DA-TAG: Limit to TianoCore
                            #ifdef __MAKEWITH_TIANO
                            RecoverVol = RefitGetAppleDiskLabel (
                                HfsRecovery[VolumeIndex]
                            );
                            #endif

                            VolumeTag = (RecoverVol != NULL)
                                ? PoolPrint (L"HFS+ Recovery : %s", RecoverVol)
                                : StrDuplicate (L"HFS+ Recovery : Recovery HD");

                            #if REFIT_DEBUG > 0
                            ALT_LOG(1, LOG_LINE_NORMAL,
                                L"Add '%s' Tag:- '%s' for '%s'",
                                ToolName, FileName,
                                VolumeTag
                            );
                            #endif

                            FoundTool = TRUE;
                            FoundThis = TRUE;
                            // DA-TAG: Do not free 'Description'
                            //         Used in 'AddToolEntry'
                            Description = PoolPrint (
                                L"%s for %s",
                                ToolName, VolumeTag
                            );
                            AddToolEntry (
                                HfsRecovery[VolumeIndex],
                                FileName, Description,
                                BuiltinIcon (BUILTIN_ICON_TOOL_APPLE_RESCUE),
                                'R', TRUE
                            );

                            #if REFIT_DEBUG > 0
                            ToolStr = PoolPrint (
                                L"Added Tool:- '%-18s     :::     %s for %s'",
                                ToolName, FileName, VolumeTag
                            );
                            ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                            if (OtherFind) {
                                LOG_MSG("%s%s", OffsetNext, Spacer);
                            }
                            LOG_MSG("%s", ToolStr);
                            MY_FREE_POOL(ToolStr);
                            #endif

                            OtherFind = TRUE;

                            MY_FREE_POOL(VolumeTag);
                        } // if (HfsRecovery[VolumeIndex]->RootDir

                        MY_FREE_POOL(RecoverVol);
                        if (!FoundThis) {
                            MY_FREE_POOL(FileName);
                        }
                    } // while
                } // for

                if (SingleAPFS) {
                    for (j = 0; j < RecoveryVolumesCount; j++) {
                        // Get a meaningful tag for the recovery tool
                        // DA-TAG: Limit to TianoCore
                        #ifdef __MAKEWITH_TIANO
                        for (k = 0; k < SystemVolumesCount; k++) {
                            SkipSystemVolume = FALSE;
                            for (VolumeIndex = 0; VolumeIndex < SkipApfsVolumesCount; VolumeIndex++) {
                                if (GuidsAreEqual (
                                    &(SkipApfsVolumes[VolumeIndex]->VolUuid),
                                    &(SystemVolumes[k]->VolUuid))
                                ) {
                                    SkipSystemVolume = TRUE;
                                    break;
                                }
                            } // for
                            if (SkipSystemVolume) {
                                continue;
                            }

                            if (GuidsAreEqual (
                                    &(RecoveryVolumes[j]->PartGuid),
                                    &(SystemVolumes[k]->PartGuid)
                                )
                            ) {
                                if (SystemVolumes[k]->VolRole == APFS_VOLUME_ROLE_SYSTEM ||
                                    SystemVolumes[k]->VolRole == APFS_VOLUME_ROLE_UNDEFINED
                                ) {
                                    // DA-TAG: Do not free 'FileName'
                                    //         Used in 'AddToolEntry'
                                    RecoverVol  = StrDuplicate (SystemVolumes[k]->VolName);
                                    TmpStr      = GuidAsString (&(SystemVolumes[k]->VolUuid));
                                    FileName    = PoolPrint (L"%s\\boot.efi", TmpStr);
                                    MY_FREE_POOL(TmpStr);

                                    break;
                                }
                            }
                        } // for k = 0
                        #endif

                        VolumeTag = (RecoverVol != NULL)
                            ? PoolPrint (L"APFS Instance : %s", RecoverVol)
                            : StrDuplicate (L"APFS Instance : Recovery");
                        #if REFIT_DEBUG > 0
                        ALT_LOG(1, LOG_LINE_NORMAL,
                            L"Adding Mac Recovery Tag:- '%s' for '%s'",
                            FileName, VolumeTag
                        );
                        #endif

                        FoundTool = TRUE;
                        // DA-TAG: Do not free 'Description'
                        //         Used in 'AddToolEntry'
                        Description = PoolPrint (
                            L"%s for %s",
                            ToolName, VolumeTag
                        );
                        AddToolEntry (
                            RecoveryVolumes[j],
                            FileName, Description,
                            BuiltinIcon (BUILTIN_ICON_TOOL_APPLE_RESCUE),
                            'R', FALSE
                        );

                        #if REFIT_DEBUG > 0
                        ToolStr = PoolPrint (
                            L"Added Tool:- '%-18s     :::     %s for %s'",
                            ToolName, FileName, VolumeTag
                        );
                        ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                        if (OtherFind) {
                            LOG_MSG("%s%s", OffsetNext, Spacer);
                        }
                        LOG_MSG("%s", ToolStr);
                        MY_FREE_POOL(ToolStr);
                        #endif

                        OtherFind = TRUE;

                        MY_FREE_POOL(VolumeTag);
                        MY_FREE_POOL(RecoverVol);
                    } // for j = 0
                } // if SingleAPFS

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    FlagAPFS = (!SingleAPFS && SystemVolumesCount > 0);
                    ToolStr = PoolPrint (
                        L"%s:- '%s'",
                        (FlagAPFS) ? L"Did *NOT* Enable Tool" : L"Could *NOT* Find Tool",
                        ToolName
                    );
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG(
                        "%s    %s",
                        (FlagAPFS) ? L" * NOTE * " : L"*_ WARN _*",
                        ToolStr
                    );
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_RECOVERY_WIN:
                j = 0;
                OtherFind = FALSE;
                // DA-TAG: Do not free 'FileName'
                //         If used in 'AddToolEntry'
                while ((FileName = FindCommaDelimited (GlobalConfig.WindowsRecoveryFiles, j++)) != NULL) {
                    FoundThis = FALSE;
                    SplitVolumeAndFilename (&FileName, &VolumeTag);
                    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                        if ((Volumes[VolumeIndex]->RootDir != NULL)        &&
                            (IsValidTool (Volumes[VolumeIndex], FileName)) &&
                            (
                                (VolumeTag == NULL) ||
                                MyStriCmp (
                                    VolumeTag,
                                    Volumes[VolumeIndex]->VolName
                                )
                            )
                        ) {
                            #if REFIT_DEBUG > 0
                            ALT_LOG(1, LOG_LINE_NORMAL,
                                L"Add '%s' Tag:- '%s' on '%s'",
                                ToolName, FileName,
                                Volumes[VolumeIndex]->VolName
                            );
                            #endif

                            FoundTool = TRUE;
                            FoundThis = TRUE;
                            // DA-TAG: Do not free 'Description'
                            //         Used in 'AddToolEntry'
                            Description = PoolPrint (
                                L"%s on %s%s via %s",
                                ToolName,
                                Volumes[VolumeIndex]->VolName,
                                SetVolType (NULL, Volumes[VolumeIndex]->VolName, Volumes[VolumeIndex]->FSType),
                                FileName
                            );
                            AddToolEntry (
                                Volumes[VolumeIndex],
                                FileName,
                                Description,
                                BuiltinIcon (BUILTIN_ICON_TOOL_WINDOWS_RESCUE),
                                'R', TRUE
                            );

                            #if REFIT_DEBUG > 0
                            ToolStr = PoolPrint (
                                L"Added Tool:- '%-18s     :::     %s'",
                                ToolName, FileName
                            );
                            ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                            if (OtherFind) {
                                LOG_MSG("%s%s", OffsetNext, Spacer);
                            }
                            LOG_MSG("%s", ToolStr);
                            MY_FREE_POOL(ToolStr);
                            #endif

                            OtherFind = TRUE;
                        } // if Volumes[VolumeIndex]->RootDir
                    } // for

                    MY_FREE_POOL(VolumeTag);
                    if (!FoundThis) {
                        MY_FREE_POOL(FileName);
                    }
                } // while

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (
                        L"Could *NOT* Find Tool:- '%s'", ToolName
                    );
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_MOK_TOOL:
                FoundTool = FindTool (
                    MokLocations,
                    MOK_NAMES,
                    ToolName,
                    BUILTIN_ICON_TOOL_MOK_TOOL
                );

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (
                        L"Could *NOT* Find Tool:- '%s'", ToolName
                    );
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_FWUPDATE_TOOL:
                FoundTool = FindTool (
                    MokLocations,
                    FWUPDATE_NAMES,
                    ToolName,
                    BUILTIN_ICON_TOOL_FWUPDATE
                );

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (
                        L"Could *NOT* Find Tool:- '%s'", ToolName
                    );
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_CSR_ROTATE:
                if (!AppleFirmware && !HasMacOS) {
                    VetCSR();

                    MY_FREE_POOL(gCsrStatus);
                    gCsrStatus = StrDuplicate (L"Incompatible Setup");
                    Status = EFI_UNSUPPORTED;
                }
                else if (GlobalConfig.CsrValues) {
                    #if REFIT_DEBUG > 0
                    MY_MUTELOGGER_SET;
                    #endif
                    // Only attempt to enable if CSR Values are set
                    // Sets 'gCsrStatus' and returns a Status Code based on outcome
                    Status = GetCsrStatus (&CsrValue);
                    if (!EFI_ERROR(Status)) {
                        if (GlobalConfig.DynamicCSR == -1) {
                            MY_FREE_POOL(gCsrStatus);
                            gCsrStatus = StrDuplicate (
                                L"Dynamic SIP/SSV Disable"
                            );
                        }
                        else if (GlobalConfig.DynamicCSR == 1) {
                            MY_FREE_POOL(gCsrStatus);
                            gCsrStatus = StrDuplicate (
                                L"Dynamic SIP/SSV Enable"
                            );
                        }
                    }
                    #if REFIT_DEBUG > 0
                    MY_MUTELOGGER_OFF;
                    #endif
                }
                else {
                    // Sets 'gCsrStatus' and always returns 'EFI_NOT_READY'
                    Status = FlagNoCSR();
                }

                if (EFI_ERROR(Status)) {
                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (
                        L"Did *NOT* Enable Tool:- '%s' ... %r (%s)",
                        ToolName, Status, gCsrStatus
                    );
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG(" * NOTE *     %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif

                    break;
                }

                MenuEntryRotateCsr = AllocateZeroPool (
                    sizeof (REFIT_MENU_ENTRY)
                );
                if (MenuEntryRotateCsr == NULL) {
                    BREAD_CRUMB(L"%a:  C - END:- VOID ... Resource Exhaution!", __func__);
                    LOG_DECREMENT();
                    LOG_SEP(L"X");
                    return;
                }

                MenuEntryRotateCsr->Title          = StrDuplicate (ToolName);
                MenuEntryRotateCsr->Tag            = TAG_CSR_ROTATE;
                MenuEntryRotateCsr->Row            = 1;
                MenuEntryRotateCsr->ShortcutDigit  = 0;
                MenuEntryRotateCsr->ShortcutLetter = 0;
                MenuEntryRotateCsr->Image          = BuiltinIcon (BUILTIN_ICON_FUNC_CSR_ROTATE);

                AddMenuEntry (MainMenu, MenuEntryRotateCsr);

                #if REFIT_DEBUG > 0
                ToolStr = PoolPrint (L"Added Tool:- '%s'", ToolName);
                ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                LOG_MSG("%s", ToolStr);
                MY_FREE_POOL(ToolStr);
                #endif

            break;
            case TAG_INSTALL:
                MenuEntryInstall = AllocateZeroPool (
                    sizeof (REFIT_MENU_ENTRY)
                );
                if (MenuEntryInstall == NULL) {
                    BREAD_CRUMB(L"%a:  C - END:- VOID ... Resource Exhaution!", __func__);
                    LOG_DECREMENT();
                    LOG_SEP(L"X");
                    return;
                }

                MenuEntryInstall->Title          = StrDuplicate (ToolName);
                MenuEntryInstall->Tag            = TAG_INSTALL;
                MenuEntryInstall->Row            = 1;
                MenuEntryInstall->ShortcutDigit  = 0;
                MenuEntryInstall->ShortcutLetter = 0;
                MenuEntryInstall->Image          = BuiltinIcon (BUILTIN_ICON_FUNC_INSTALL);

                AddMenuEntry (MainMenu, MenuEntryInstall);

                #if REFIT_DEBUG > 0
                ToolStr = PoolPrint (L"Added Tool:- '%s'", ToolName);
                ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                LOG_MSG("%s", ToolStr);
                MY_FREE_POOL(ToolStr);
                #endif

            break;
            case TAG_BOOTORDER:
                MenuEntryBootorder = AllocateZeroPool (
                    sizeof (REFIT_MENU_ENTRY)
                );
                if (MenuEntryBootorder == NULL) {
                    BREAD_CRUMB(L"%a:  C - END:- VOID ... Resource Exhaution!", __func__);
                    LOG_DECREMENT();
                    LOG_SEP(L"X");
                    return;
                }

                MenuEntryBootorder->Title          = StrDuplicate (ToolName);
                MenuEntryBootorder->Tag            = TAG_BOOTORDER;
                MenuEntryBootorder->Row            = 1;
                MenuEntryBootorder->ShortcutDigit  = 0;
                MenuEntryBootorder->ShortcutLetter = 0;
                MenuEntryBootorder->Image          = BuiltinIcon (BUILTIN_ICON_FUNC_BOOTORDER);

                AddMenuEntry (MainMenu, MenuEntryBootorder);

                #if REFIT_DEBUG > 0
                ToolStr = PoolPrint (L"Added Tool:- '%s'", ToolName);
                ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                LOG_MSG("%s", ToolStr);
                MY_FREE_POOL(ToolStr);
                #endif

            break;
            case TAG_MEMTEST:
                FoundTool = FindTool (
                    MEMTEST_LOCATIONS,
                    MEMTEST_NAMES,
                    ToolName,
                    BUILTIN_ICON_TOOL_MEMTEST
                );

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (
                        L"Could *NOT* Find Tool:- '%s'", ToolName
                    );
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
        } // switch
    } // for

    #if REFIT_DEBUG > 0
    ToolStr = PoolPrint (
        L"Processed %d Tool List Item%s",
        ToolTotal,
        (ToolTotal == 1) ? L"" : L"s"
    );
    ALT_LOG(1, LOG_STAR_HEAD_SEPX, L"%s", ToolStr);
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"%s", ToolStr);
    LOG_MSG("\n\n");
    LOG_MSG("INFO: %s", ToolStr);
    LOG_MSG("\n\n");
    #endif

    BREAD_CRUMB(L"%a:  B - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID ScanForTools
