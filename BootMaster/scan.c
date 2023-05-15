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
 * Modifications copyright (c) 2012-2023 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2023 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "config.h"
#include "screenmgt.h"
#include "lib.h"
#include "icns.h"
#include "menu.h"
#include "mok.h"
#include "apple.h"
#include "mystrings.h"
#include "security_policy.h"
#include "driver_support.h"
#include "launch_efi.h"
#include "launch_legacy.h"
#include "linux.h"
#include "scan.h"
#include "install.h"
#include "../include/refit_call_wrapper.h"


//
// constants

#if defined (EFIX64)
    #define SHELL_NAMES L"\\EFI\\BOOT\\tools\\x64_Shell.efi,\\EFI\\BOOT\\tools\\Shell_x64.efi,\\EFI\\BOOT\\tools\\Shell.efi,\\EFI\\BOOT\\tools_x64\\x64_Shell.efi,\\EFI\\BOOT\\tools_x64\\Shell_x64.efi,\\EFI\\BOOT\\tools_x64\\Shell.efi,\\EFI\\tools_x64\\x64_Shell.efi,\\EFI\\tools_x64\\shell_x64.efi,\\EFI\\tools_x64\\shell.efi,\\EFI\\tools\\x64_Shell.efi,\\EFI\\tools\\shell_x64.efi,\\EFI\\tools\\shell.efi,\\EFI\\x64_Shell.efi,\\EFI\\shell_x64.efi,\\EFI\\shell.efi,\\x64_Shell.efi,\\shell_x64.efi,\\shell.efi"
    #define GPTSYNC_NAMES L"\\EFI\\BOOT\\tools\\x64_gptsync.efi,\\EFI\\BOOT\\tools\\gptsync_x64.efi,\\EFI\\BOOT\\tools\\gptsync.efi,\\EFI\\BOOT\\tools_x64\\x64_gptsync.efi,\\EFI\\BOOT\\tools_x64\\gptsync_x64.efi,\\EFI\\BOOT\\tools_x64\\gptsync.efi,\\EFI\\tools_x64\\gptsync.efi,\\EFI\\tools_x64\\gptsync_x64.efi,\\EFI\\tools\\gptsync.efi,\\EFI\\tools\\gptsync_x64.efi,\\EFI\\gptsync.efi,\\EFI\\gptsync_x64.efi,\\gptsync.efi,\\gptsync_x64.efi"
    #define GDISK_NAMES L"\\EFI\\BOOT\\tools\\x64_gdisk.efi,\\EFI\\BOOT\\tools\\gdisk_x64.efi,\\EFI\\BOOT\\tools\\gdisk.efi,\\EFI\\BOOT\\tools_x64\\x64_gdisk.efi,\\EFI\\BOOT\\tools_x64\\gdisk_x64.efi,\\EFI\\BOOT\\tools_x64\\gdisk.efi,\\EFI\\tools_x64\\gdisk.efi,\\EFI\\tools_x64\\gdisk_x64.efi,\\EFI\\tools\\gdisk.efi,\\EFI\\tools\\gdisk_x64.efi,\\EFI\\gdisk.efi,\\EFI\\gdisk_x64.efi,\\gdisk.efi,\\gdisk_x64.efi"
    #define NETBOOT_NAMES L"\\EFI\\BOOT\\tools\\x64_ipxe.efi,\\EFI\\BOOT\\tools\\ipxe_x64.efi,\\EFI\\BOOT\\tools\\ipxe.efi,\\EFI\\BOOT\\tools_x64\\x64_ipxe.efi,\\EFI\\BOOT\\tools_x64\\ipxe_x64.efi,\\EFI\\BOOT\\tools_x64\\ipxe.efi,\\EFI\\tools_x64\\ipxe.efi,\\EFI\\tools_x64\\ipxe_x64.efi,\\EFI\\tools\\ipxe.efi,\\EFI\\tools\\ipxe_x64.efi,\\EFI\\ipxe.efi,\\EFI\\ipxe_x64.efi,\\ipxe.efi,\\ipxe_x64.efi"
    #define MEMTEST_NAMES L"x64_memtest.efi,memtest.efi,memtest_x64.efi,x64_memtest86.efi,memtest86.efi,memtest86_x64.efi"
    #define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootx64.efi"
    #define FALLBACK_BASENAME       L"bootx64.efi"
#elif defined (EFI32)
    #define SHELL_NAMES L"\\EFI\\BOOT\\tools\\ia32_Shell.efi,\\EFI\\BOOT\\tools\\Shell_ia32.efi,\\EFI\\BOOT\\tools\\Shell.efi,\\EFI\\BOOT\\tools_ia32\\ia32_Shell.efi,\\EFI\\BOOT\\tools_ia32\\Shell_ia32.efi,\\EFI\\BOOT\\tools_ia32\\Shell.efi,\\EFI\\tools_ia32\\ia32_Shell.efi,\\EFI\\tools_ia32\\shell_ia32.efi,\\EFI\\tools_ia32\\shell.efi,\\EFI\\tools\\ia32_Shell.efi,\\EFI\\tools\\shell_ia32.efi,\\EFI\\tools\\shell.efi,\\EFI\\ia32_Shell.efi,\\EFI\\shell_ia32.efi,\\EFI\\shell.efi,\\ia32_Shell.efi,\\shell_ia32.efi,\\shell.efi"
    #define GPTSYNC_NAMES L"\\EFI\\BOOT\\tools\\ia32_gptsync.efi,\\EFI\\BOOT\\tools\\gptsync_ia32.efi,\\EFI\\BOOT\\tools\\gptsync.efi,\\EFI\\BOOT\\tools_ia32\\ia32_gptsync.efi,\\EFI\\BOOT\\tools_ia32\\gptsync_ia32.efi,\\EFI\\BOOT\\tools_ia32\\gptsync.efi,\\EFI\\tools_ia32\\gptsync.efi,\\EFI\\tools_ia32\\gptsync_ia32.efi,\\EFI\\tools\\gptsync.efi,\\EFI\\tools\\gptsync_ia32.efi,\\EFI\\gptsync.efi,\\EFI\\gptsync_ia32.efi,\\gptsync.efi,\\gptsync_ia32.efi"
    #define GDISK_NAMES L"\\EFI\\BOOT\\tools\\ia32_gdisk.efi,\\EFI\\BOOT\\tools\\gdisk_ia32.efi,\\EFI\\BOOT\\tools\\gdisk.efi,\\EFI\\BOOT\\tools_ia32\\ia32_gdisk.efi,\\EFI\\BOOT\\tools_ia32\\gdisk_ia32.efi,\\EFI\\BOOT\\tools_ia32\\gdisk.efi,\\EFI\\tools_ia32\\gdisk.efi,\\EFI\\tools_ia32\\gdisk_ia32.efi,\\EFI\\tools\\gdisk.efi,\\EFI\\tools\\gdisk_ia32.efi,\\EFI\\gdisk.efi,\\EFI\\gdisk_ia32.efi,\\gdisk.efi,\\gdisk_ia32.efi"
    #define NETBOOT_NAMES L"\\EFI\\BOOT\\tools\\ia32_ipxe.efi,\\EFI\\BOOT\\tools\\ipxe_ia32.efi,\\EFI\\BOOT\\tools\\ipxe.efi,\\EFI\\BOOT\\tools_ia32\\ia32_ipxe.efi,\\EFI\\BOOT\\tools_ia32\\ipxe_ia32.efi,\\EFI\\BOOT\\tools_ia32\\ipxe.efi,\\EFI\\tools_ia32\\ipxe.efi,\\EFI\\tools_ia32\\ipxe_ia32.efi,\\EFI\\tools\\ipxe.efi,\\EFI\\tools\\ipxe_ia32.efi,\\EFI\\ipxe.efi,\\EFI\\ipxe_ia32.efi,\\ipxe.efi,\\ipxe_ia32.efi"
    #define MEMTEST_NAMES L"ia32_memtest.efi,memtest.efi,memtest_ia32.efi,ia32_memtest86.efi,memtest86.efi,memtest86_ia32.efi"
    #define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootia32.efi"
    #define FALLBACK_BASENAME       L"bootia32.efi"
#elif defined (EFIAARCH64)
    #define SHELL_NAMES L"\\EFI\\BOOT\\tools\\aa64_Shell.efi,\\EFI\\BOOT\\tools\\Shell_aa64.efi,\\EFI\\BOOT\\tools\\Shell.efi,\\EFI\\BOOT\\tools_aa64\\aa64_Shell.efi,\\EFI\\BOOT\\tools_aa64\\Shell_aa64.efi,\\EFI\\BOOT\\tools_aa64\\Shell.efi,\\EFI\\tools_aa64\\aa64_Shell.efi,\\EFI\\tools_aa64\\shell_aa64.efi,\\EFI\\tools_aa64\\shell.efi,\\EFI\\tools\\aa64_Shell.efi,\\EFI\\tools\\shell_aa64.efi,\\EFI\\tools\\shell.efi,\\EFI\\aa64_Shell.efi,\\EFI\\shell_aa64.efi,\\EFI\\shell.efi,\\aa64_Shell.efi,\\shell_aa64.efi,\\shell.efi"
    #define GPTSYNC_NAMES L"\\EFI\\BOOT\\tools\\aa64_gptsync.efi,\\EFI\\BOOT\\tools\\gptsync_aa64.efi,\\EFI\\BOOT\\tools\\gptsync.efi,\\EFI\\BOOT\\tools_aa64\\aa64_gptsync.efi,\\EFI\\BOOT\\tools_aa64\\gptsync_aa64.efi,\\EFI\\BOOT\\tools_aa64\\gptsync.efi,\\EFI\\tools_aa64\\gptsync.efi,\\EFI\\tools_aa64\\gptsync_aa64.efi,\\EFI\\tools\\gptsync.efi,\\EFI\\tools\\gptsync_aa64.efi,\\EFI\\gptsync.efi,\\EFI\\gptsync_aa64.efi,\\gptsync.efi,\\gptsync_aa64.efi"
    #define GDISK_NAMES L"\\EFI\\BOOT\\tools\\aa64_gdisk.efi,\\EFI\\BOOT\\tools\\gdisk_aa64.efi,\\EFI\\BOOT\\tools\\gdisk.efi,\\EFI\\BOOT\\tools_aa64\\aa64_gdisk.efi,\\EFI\\BOOT\\tools_aa64\\gdisk_aa64.efi,\\EFI\\BOOT\\tools_aa64\\gdisk.efi,\\EFI\\tools_aa64\\gdisk.efi,\\EFI\\tools_aa64\\gdisk_aa64.efi,\\EFI\\tools\\gdisk.efi,\\EFI\\tools\\gdisk_aa64.efi,\\EFI\\gdisk.efi,\\EFI\\gdisk_aa64.efi,\\gdisk.efi,\\gdisk_aa64.efi"
    #define NETBOOT_NAMES L"\\EFI\\BOOT\\tools\\aa64_ipxe.efi,\\EFI\\BOOT\\tools\\ipxe_aa64.efi,\\EFI\\BOOT\\tools\\ipxe.efi,\\EFI\\BOOT\\tools_aa64\\aa64_ipxe.efi,\\EFI\\BOOT\\tools_aa64\\ipxe_aa64.efi,\\EFI\\BOOT\\tools_aa64\\ipxe.efi,\\EFI\\tools_aa64\\ipxe.efi,\\EFI\\tools_aa64\\ipxe_aa64.efi,\\EFI\\tools\\ipxe.efi,\\EFI\\tools\\ipxe_aa64.efi,\\EFI\\ipxe.efi,\\EFI\\ipxe_aa64.efi,\\ipxe.efi,\\ipxe_aa64.efi"
    #define MEMTEST_NAMES L"aa64_memtest.efi,memtest.efi,memtest_aa64.efi,aa64_memtest86.efi,memtest86.efi,memtest86_aa64.efi"
    #define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootaa64.efi"
    #define FALLBACK_BASENAME       L"bootaa64.efi"
#else
    #define SHELL_NAMES             L"\\EFI\\tools\\shell.efi,\\shell.efi"
    #define GPTSYNC_NAMES           L"\\EFI\\tools\\gptsync.efi"
    #define GDISK_NAMES             L"\\EFI\\tools\\gdisk.efi"
    #define NETBOOT_NAMES           L"\\EFI\\tools\\ipxe.efi"
    #define MEMTEST_NAMES           L"\\memtest.efi,\\memtest86.efi"
    #define DRIVER_DIRS             L"drivers"
    #define FALLBACK_FULLNAME       L"EFI\\BOOT\\boot.efi" // Not really correct
    #define FALLBACK_BASENAME       L"boot.efi"            // Not really correct
#endif

#define IPXE_DISCOVER_NAME      L"\\efi\\tools\\ipxe_discover.efi"
#define IPXE_NAME               L"\\efi\\tools\\ipxe.efi"

EFI_GUID GlobalGuid = EFI_GLOBAL_VARIABLE;

extern EFI_GUID GuidAPFS;
extern EFI_GUID AppleVendorOsGuid;

#if REFIT_DEBUG > 0
static CHAR16  *Spacer   = L"                ";

BOOLEAN  LogNewLine      = FALSE;
#endif

BOOLEAN  HasMacOS        = FALSE;
BOOLEAN  ScanningLoaders = FALSE;
BOOLEAN  FirstLoaderScan = FALSE;

// Structure used to hold boot loader filenames and time stamps in
// a linked list; used to sort entries within a directory.
struct
LOADER_LIST {
    CHAR16              *FileName;
    EFI_TIME             TimeStamp;
    struct LOADER_LIST  *NextEntry;
};

//
// misc functions
//

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
        &AppleVendorOsGuid, L"csr-active-config",
        (VOID **) &ReturnValue, &CsrLength
    );
    if (!EFI_ERROR(Status)) {
        // No Error ... Clear the Variable (Basically Enable SIP/SSV)
        EfivarSetRaw (
            &AppleVendorOsGuid, L"csr-active-config",
            NULL, 0, TRUE
        );
    }
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif
} // static VOID VetCSR()

// Creates a copy of a menu screen.
// Returns a pointer to the copy of the menu screen.
REFIT_MENU_SCREEN * CopyMenuScreen (
    REFIT_MENU_SCREEN *Entry
) {
    UINTN              i;
    REFIT_MENU_SCREEN *NewEntry;

    NewEntry = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));

    if ((Entry != NULL) && (NewEntry != NULL)) {
        NewEntry->Title = (Entry->Title) ? StrDuplicate (Entry->Title) : NULL;

        if (Entry->TitleImage != NULL) {
            NewEntry->TitleImage = egCopyImage (Entry->TitleImage);
        }

        NewEntry->InfoLineCount = Entry->InfoLineCount;
        NewEntry->InfoLines = (CHAR16 **) AllocateZeroPool (
            Entry->InfoLineCount * (sizeof (CHAR16 *))
        );
        for (i = 0; i < Entry->InfoLineCount && NewEntry->InfoLines; i++) {
            NewEntry->InfoLines[i] = (Entry->InfoLines[i])
                ? StrDuplicate (Entry->InfoLines[i]) : NULL;
        } // for

        NewEntry->EntryCount = Entry->EntryCount;
        NewEntry->Entries = (REFIT_MENU_ENTRY **) AllocateZeroPool (
            Entry->EntryCount * (sizeof (REFIT_MENU_ENTRY *))
        );
        for (i = 0; i < Entry->EntryCount && NewEntry->Entries; i++) {
            AddMenuEntryCopy (NewEntry, Entry->Entries[i]);
        } // for

        NewEntry->TimeoutSeconds =  Entry->TimeoutSeconds;
        NewEntry->TimeoutText    = (Entry->TimeoutText)
            ? StrDuplicate (Entry->TimeoutText) : NULL;

        NewEntry->Hint1 = (Entry->Hint1) ? StrDuplicate (Entry->Hint1) : NULL;
        NewEntry->Hint2 = (Entry->Hint2) ? StrDuplicate (Entry->Hint2) : NULL;
    }

    return (NewEntry);
} // REFIT_MENU_SCREEN * CopyMenuScreen()

// Creates a copy of a menu entry. Intended to enable moving a stack-based
// menu entry (such as the ones for the "reboot" and "exit" functions) to
// to the heap. This enables easier deletion of the whole set of menu
// entries when re-scanning.
// Returns a pointer to the copy of the menu entry.
REFIT_MENU_ENTRY * CopyMenuEntry (
    REFIT_MENU_ENTRY *Entry
) {
    REFIT_MENU_ENTRY *NewEntry;

    NewEntry = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
    if ((Entry != NULL) && (NewEntry != NULL)) {
        NewEntry->Tag            =  Entry->Tag;
        NewEntry->Row            =  Entry->Row;
        NewEntry->ShortcutDigit  =  Entry->ShortcutDigit;
        NewEntry->ShortcutLetter =  Entry->ShortcutLetter;
        NewEntry->Title          = (Entry->Title      != NULL) ? StrDuplicate (Entry->Title)       : NULL;
        NewEntry->Image          = (Entry->Image      != NULL) ? egCopyImage (Entry->Image)        : NULL;
        NewEntry->BadgeImage     = (Entry->BadgeImage != NULL) ? egCopyImage (Entry->BadgeImage)   : NULL;
        NewEntry->SubScreen      = (Entry->SubScreen  != NULL) ? CopyMenuScreen (Entry->SubScreen) : NULL;
    }
    return (NewEntry);
} // REFIT_MENU_ENTRY * CopyMenuEntry()

// Creates a new LOADER_ENTRY data structure and populates it with
// default values from the specified Entry, or NULL values if Entry
// is unspecified (NULL).
// Returns a pointer to the new data structure, or NULL if it
// couldn't be allocated
LOADER_ENTRY * InitializeLoaderEntry (
    IN LOADER_ENTRY *Entry
) {
    LOADER_ENTRY *NewEntry;

    NewEntry = AllocateZeroPool (sizeof (LOADER_ENTRY));
    if (NewEntry != NULL) {
        NewEntry->Enabled         = TRUE;
        NewEntry->UseGraphicsMode = FALSE;
        NewEntry->EfiLoaderPath   = NULL;
        NewEntry->me.Title        = NULL;
        NewEntry->me.Tag          = TAG_LOADER;
        NewEntry->OSType          = 0;
        NewEntry->EfiBootNum      = 0;
        if (Entry != NULL) {
            NewEntry->EfiBootNum      =  Entry->EfiBootNum;
            NewEntry->UseGraphicsMode =  Entry->UseGraphicsMode;
            NewEntry->Volume          = (Entry->Volume)        ? CopyVolume (Entry->Volume)                 : NULL;
            NewEntry->LoaderPath      = (Entry->LoaderPath)    ? StrDuplicate (Entry->LoaderPath)           : NULL;
            NewEntry->LoadOptions     = (Entry->LoadOptions)   ? StrDuplicate (Entry->LoadOptions)          : NULL;
            NewEntry->InitrdPath      = (Entry->InitrdPath)    ? StrDuplicate (Entry->InitrdPath)           : NULL;
            NewEntry->EfiLoaderPath   = (Entry->EfiLoaderPath) ? DuplicateDevicePath (Entry->EfiLoaderPath) : NULL;
        }
    }

    return (NewEntry);
} // LOADER_ENTRY *InitializeLoaderEntry()

// Prepare a REFIT_MENU_SCREEN data structure for a subscreen entry. This sets up
// the default entry that launches the boot loader using the same options as the
// main Entry does. Subsequent options can be added by the calling function.
// If a subscreen already exists in the Entry that is passed to this function,
// it is left unchanged and a pointer to it is returned.
// Returns a pointer to the new subscreen data structure, or NULL if there
// were problems allocating memory.
REFIT_MENU_SCREEN * InitializeSubScreen (
    IN LOADER_ENTRY *Entry
) {
    EFI_STATUS              Status;
    CHAR16                 *NameOS;
    CHAR16                 *TmpStr;
    CHAR16                 *TmpName;
    CHAR16                 *FileName;
    CHAR16                 *MainOptions;
    CHAR16                 *DisplayName;
    LOADER_ENTRY           *SubEntry;
    REFIT_MENU_SCREEN      *SubScreen;
    APPLE_APFS_VOLUME_ROLE  VolumeRole;

    FileName = Basename (Entry->LoaderPath);
    if (Entry->me.SubScreen) {
        // Existing subscreen ... Early Return ... Add new entry later.
        return CopyMenuScreen (Entry->me.SubScreen);
    }

    // No subscreen yet ... Initialize default entry
    SubScreen = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (SubScreen == NULL) {
        // Early Return
        return NULL;
    }

    DisplayName = NULL;
    if (Entry->Volume->FSType == FS_TYPE_APFS && GlobalConfig.SyncAPFS) {
        VolumeRole = 0;

        // DA-TAG: Limit to TianoCore
        #ifndef __MAKEWITH_TIANO
        Status = EFI_NOT_STARTED;
        #else
        Status = RP_GetApfsVolumeInfo (
            Entry->Volume->DeviceHandle,
            NULL, NULL,
            &VolumeRole
        );
        #endif

        if (!EFI_ERROR(Status)) {
            if (VolumeRole == APPLE_APFS_VOLUME_ROLE_PREBOOT) {
                DisplayName = GetVolumeGroupName (Entry->LoaderPath, Entry->Volume);
            }
        }
    }

    TmpStr = (Entry->Title != NULL) ? Entry->Title  : FileName;
    TmpName = (DisplayName) ? DisplayName : Entry->Volume->VolName;
    SubScreen->Title = PoolPrint (
        L"Boot Options for %s%s%s%s%s",
        TmpStr,
        SetVolJoin (TmpStr),
        SetVolKind (TmpStr, TmpName),
        SetVolFlag (TmpStr, TmpName),
        SetVolType (TmpStr, TmpName)
    );

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_MID, L"Build Subscreen:- '%s'", SubScreen->Title);
    #endif

    SubScreen->TitleImage = egCopyImage (Entry->me.Image);

    // Default entry
    SubEntry = InitializeLoaderEntry (Entry);
    if (SubEntry != NULL) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Setting Entries for '%s'", SubScreen->Title);
        #endif

        // DA-TAG: Do not free 'NameOS'
        NameOS = Entry->Title;
        // DA-TAG: Maintain space after NameOS
        if (0);
        else if (Entry->OSType == 'W' && FindSubStr (SubScreen->Title, L"Legacy")) NameOS = L"Instance: Windows (Legacy)";
        else if (Entry->OSType == 'W' && FindSubStr (SubScreen->Title, L"UEFI"))   NameOS = L"Instance: Windows (UEFI)"  ;
        else if (Entry->OSType == 'W')                                             NameOS = L"Instance: Windows"         ;
        else if (Entry->OSType == 'M')                                             NameOS = L"Instance: MacOS"           ;
        else if (Entry->OSType == 'L')                                             NameOS = L"Instance: Linux"           ;
        else if (Entry->OSType == 'G')                                             NameOS = L"Instance: Grub"            ;
        else if (Entry->OSType == 'X')                                             NameOS = L"Instance: XoM"             ;
        else if (Entry->OSType == 'E')                                             NameOS = L"Instance: Elilo"           ;
        else if (Entry->OSType == 'R')                                             NameOS = L"Instance: rEFit Variant"   ;
        else if (FindSubStr (SubScreen->Title, L"OpenCore"))                       NameOS = L"Instance: OpenCore"        ;
        else if (FindSubStr (SubScreen->Title, L"Clover"))                         NameOS = L"Instance: Clover"          ;

        SubEntry->me.Title    = PoolPrint (L"Load %s with Default Options", NameOS);
        MainOptions           = StrDuplicate (SubEntry->LoadOptions);
        SubEntry->LoadOptions = AddInitrdToOptions (MainOptions, SubEntry->InitrdPath);
        AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);

        MY_FREE_POOL(MainOptions);
    }

    SubScreen->Hint1 = StrDuplicate (SUBSCREEN_HINT1);
    SubScreen->Hint2 = (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR)
        ? StrDuplicate (SUBSCREEN_HINT2_NO_EDITOR)
        : StrDuplicate (SUBSCREEN_HINT2);

    MY_FREE_POOL(FileName);
    MY_FREE_POOL(DisplayName);

    return SubScreen;
} // REFIT_MENU_SCREEN *InitializeSubScreen()

VOID GenerateSubScreen (
    IN OUT LOADER_ENTRY *Entry,
    IN     REFIT_VOLUME *Volume,
    IN     BOOLEAN       GenerateReturn
) {
    EFI_STATUS               Status;
    UINTN                    i;
    UINTN                    TokenCount;
    CHAR16                  *InitrdName;
    CHAR16                  *KernelVersion;
    CHAR16                 **TokenList;
    BOOLEAN                  UseSystemVolume;
    REFIT_FILE              *File;
    LOADER_ENTRY            *SubEntry;
    REFIT_VOLUME            *DiagnosticsVolume;
    REFIT_MENU_SCREEN       *SubScreen;
    APPLE_APFS_VOLUME_ROLE   VolumeRole;

    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"GenerateSubScreen";
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  A - START MAIN", FuncTag);

    // Create the submenu
    if (StrLen (Entry->Title) == 0) {
        MY_FREE_POOL(Entry->Title);
    }

    SubScreen = InitializeSubScreen (Entry);

    // InitializeSubScreen cannot return NULL; but guard against this regardless
    if (SubScreen != NULL) {
        // Loader specific submenu entries
        if (Entry->OSType == 'M') {          // Entries for MacOS
            LOG_SEP(L"X");
            BREAD_CRUMB(L"%s:  A1 - OSType M:- START", FuncTag);
#if defined (EFIX64)
            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load MacOS Instance with a 64-bit Kernel");
                SubEntry->LoadOptions     = StrDuplicate (L"arch=x86_64");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load MacOS Instance with a 32-bit Kernel");
                SubEntry->LoadOptions     = StrDuplicate (L"arch=i386");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }
#endif

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load MacOS Instance in Verbose Mode");
                SubEntry->LoadOptions     = StrDuplicate (L"-v");
                SubEntry->UseGraphicsMode = FALSE;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

#if defined (EFIX64)
            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load MacOS Instance in Verbose Mode (64-bit)");
                SubEntry->LoadOptions     = StrDuplicate (L"-v arch=x86_64");
                SubEntry->UseGraphicsMode = FALSE;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }
            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load MacOS Instance in Verbose Mode (32-bit)");
                SubEntry->LoadOptions     = StrDuplicate (L"-v arch=i386");
                SubEntry->UseGraphicsMode = FALSE;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }
#endif

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_SAFEMODE)) {
                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    SubEntry->me.Title        = StrDuplicate (L"Load MacOS Instance in Safe Mode (Laconic)");
                    SubEntry->LoadOptions     = StrDuplicate (L"-x");
                    SubEntry->UseGraphicsMode = FALSE;
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                }
                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    SubEntry->me.Title        = StrDuplicate (L"Load MacOS Instance in Safe Mode (Verbose)");
                    SubEntry->LoadOptions     = StrDuplicate (L"-v -x");
                    SubEntry->UseGraphicsMode = FALSE;
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                }
            } // Safe mode allowed

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_SINGLEUSER)) {
                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    SubEntry->me.Title        = StrDuplicate (L"Load MacOS Instance in Single User Mode (Laconic)");
                    SubEntry->LoadOptions     = StrDuplicate (L"-s");
                    SubEntry->UseGraphicsMode = FALSE;
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                }
                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    SubEntry->me.Title        = StrDuplicate (L"Load MacOS Instance in Single User Mode (Verbose)");
                    SubEntry->LoadOptions     = StrDuplicate (L"-v -s");
                    SubEntry->UseGraphicsMode = FALSE;
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                }
            } // single-user mode allowed

            // Check for Apple hardware diagnostics
            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HWTEST)) {
                UseSystemVolume = FALSE;
                // DA-TAG: Investigate This
                //         Is SingleAPFS really needed?
                //         Play safe and add it for now
                if (SingleAPFS) {
                    if (Volume->FSType == FS_TYPE_APFS && GlobalConfig.SyncAPFS) {
                        VolumeRole = 0;

                        // DA-TAG: Limit to TianoCore
                        #ifndef __MAKEWITH_TIANO
                        Status = EFI_NOT_STARTED;
                        #else
                        Status = RP_GetApfsVolumeInfo (
                            Volume->DeviceHandle,
                            NULL, NULL,
                            &VolumeRole
                        );
                        #endif

                        if (!EFI_ERROR(Status)) {
                            if (VolumeRole == APPLE_APFS_VOLUME_ROLE_PREBOOT) {
                                for (i = 0; i < SystemVolumesCount; i++) {
                                    if (GuidsAreEqual (&(SystemVolumes[i]->PartGuid), &(Volume->PartGuid))) {
                                        UseSystemVolume = TRUE;
                                        break;
                                    }
                                } // for
                            }
                        }
                    }
                }

                DiagnosticsVolume = (UseSystemVolume) ? SystemVolumes[i] : Volume;
                if (FileExists (DiagnosticsVolume->RootDir, MACOSX_DIAGNOSTICS)) {
                    SubEntry = InitializeLoaderEntry (Entry);
                    if (SubEntry != NULL) {
                        MY_FREE_POOL(SubEntry->LoaderPath);
                        SubEntry->Volume          = CopyVolume (DiagnosticsVolume);
                        SubEntry->me.Title        = StrDuplicate (L"Run Apple Hardware Test");
                        SubEntry->LoaderPath      = StrDuplicate (MACOSX_DIAGNOSTICS);
                        SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
                        AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                    }
                }
            } // if !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HWTEST)
            BREAD_CRUMB(L"%s:  A2 - OSType M:- END", FuncTag);
        }
        else if (Entry->OSType == 'L') {   // Entries for Linux kernels with EFI stub loaders
            LOG_SEP(L"X");
            BREAD_CRUMB(L"%s:  A1 - OSType L:- START", FuncTag);
            File = ReadLinuxOptionsFile (Entry->LoaderPath, Volume);

            BREAD_CRUMB(L"%s:  2", FuncTag);
            if (File) {
                BREAD_CRUMB(L"%s:  2a 1", FuncTag);
                KernelVersion = FindNumbers (Entry->LoaderPath);

                BREAD_CRUMB(L"%s:  2a 2", FuncTag);
                TokenCount = ReadTokenLine (File, &TokenList);

                BREAD_CRUMB(L"%s:  2a 3", FuncTag);
                if (TokenCount >= 2) {
                    BREAD_CRUMB(L"%s:  2a 3a 1", FuncTag);
                    ReplaceSubstring (&(TokenList[1]), KERNEL_VERSION, KernelVersion);
                }

                BREAD_CRUMB(L"%s:  2a 4", FuncTag);
                // First entry requires special processing, since it was initially set
                // up with a default title but correct options by InitializeSubScreen(),
                // earlier.
                if ((TokenCount > 1)             &&
                    (SubScreen->Entries != NULL) &&
                    (SubScreen->Entries[0] != NULL)
                ) {
                    BREAD_CRUMB(L"%s:  2a 4a 1", FuncTag);
                    MY_FREE_POOL(SubScreen->Entries[0]->Title);
                    SubScreen->Entries[0]->Title = StrDuplicate (
                        (TokenList[0]) ? TokenList[0] : L"Load Instance: Linux"
                    );
                }
                BREAD_CRUMB(L"%s:  2a 5", FuncTag);
                FreeTokenLine (&TokenList, &TokenCount);

                BREAD_CRUMB(L"%s:  2a 6", FuncTag);
                InitrdName = FindInitrd (Entry->LoaderPath, Volume);

                BREAD_CRUMB(L"%s:  2a 7", FuncTag);
                while ((TokenCount = ReadTokenLine (File, &TokenList)) > 1) {
                    LOG_SEP(L"X");
                    BREAD_CRUMB(L"%s:  2a 7a 1 - WHILE LOOP:- START", FuncTag);
                    ReplaceSubstring (&(TokenList[1]), KERNEL_VERSION, KernelVersion);

                    BREAD_CRUMB(L"%s:  2a 7a 2", FuncTag);
                    SubEntry = InitializeLoaderEntry (Entry);

                    BREAD_CRUMB(L"%s:  2a 7a 3", FuncTag);
                    if (SubEntry != NULL) {
                        BREAD_CRUMB(L"%s:  2a 7a 3a 1", FuncTag);
                        SubEntry->me.Title = TokenList[0]
                            ? StrDuplicate (TokenList[0])
                            : StrDuplicate (L"Load Instance: Linux");

                        BREAD_CRUMB(L"%s:  2a 7a 3a 2", FuncTag);
                        MY_FREE_POOL(SubEntry->LoadOptions);

                        BREAD_CRUMB(L"%s:  2a 7a 3a 3", FuncTag);
                        SubEntry->LoadOptions = AddInitrdToOptions (TokenList[1], InitrdName);

                        BREAD_CRUMB(L"%s:  2a 7a 3a 4", FuncTag);
                        SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;

                        BREAD_CRUMB(L"%s:  2a 7a 3a 5", FuncTag);
                        AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                    }

                    BREAD_CRUMB(L"%s:  2a 7a 4", FuncTag);
                    FreeTokenLine (&TokenList, &TokenCount);

                    BREAD_CRUMB(L"%s:  2a 7a 5 - WHILE LOOP:- END", FuncTag);
                    LOG_SEP(L"X");
                } // while
                BREAD_CRUMB(L"%s:  2a 8", FuncTag);
                FreeTokenLine (&TokenList, &TokenCount);

                BREAD_CRUMB(L"%s:  2a 9", FuncTag);
                MY_FREE_POOL(KernelVersion);
                MY_FREE_POOL(InitrdName);
                MY_FREE_FILE(File);
            } // if File

            BREAD_CRUMB(L"%s:  3 - OSType L:- END", FuncTag);
        }
        else if (Entry->OSType == 'E') {   // Entries for ELILO
            LOG_SEP(L"X");
            BREAD_CRUMB(L"%s:  A1 - OSType E:- START", FuncTag);
            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Run ELILO in Interactive Mode");
                SubEntry->LoadOptions     = StrDuplicate (L"-p");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Linux Instance for a 17\" iMac or a 15\" MacBook Pro (*)");
                SubEntry->LoadOptions     = StrDuplicate (L"-d 0 i17");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Linux Instance for a 20\" iMac (*)");
                SubEntry->LoadOptions     = StrDuplicate (L"-d 0 i20");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Linux Instance for a Mac Mini (*)");
                SubEntry->LoadOptions     = StrDuplicate (L"-d 0 mini");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            AddMenuInfoLine (SubScreen, L"NOTE: This is an example",             FALSE);
            AddMenuInfoLine (SubScreen, L"Entries marked with (*) may not work", FALSE);

            BREAD_CRUMB(L"%s:  A2 - OSType E:- END", FuncTag);
        }
        else if (Entry->OSType == 'X') {   // Entries for xom.efi
            LOG_SEP(L"X");
            BREAD_CRUMB(L"%s:  A1 - OSType X:- START", FuncTag);
            // Skip the built-in selection and boot from hard disk only by default
            Entry->LoadOptions = L"-s -h";

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Windows Instance on Hard Disk");
                SubEntry->LoadOptions     = StrDuplicate (L"-s -h");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load Windows Instance on Optical Disc");
                SubEntry->LoadOptions     = StrDuplicate (L"-s -c");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = StrDuplicate (L"Load XOM Instance in Text Mode");
                SubEntry->LoadOptions     = StrDuplicate (L"-v");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }
            BREAD_CRUMB(L"%s:  A2 - OSType X:- END", FuncTag);
        } // Entries for xom.efi

        do {
            LOG_SEP(L"X");
            BREAD_CRUMB(L"%s:  Z 1 - START", FuncTag);
            if (GenerateReturn) {
                BREAD_CRUMB(L"%s:  Z 1a 1", FuncTag);
                if (!GetReturnMenuEntry (&SubScreen)) {
                    BREAD_CRUMB(L"%s:  Z 1a 1a 1 - Resource Exhaustion!!", FuncTag);
                    FreeMenuScreen (&SubScreen);
                    BREAD_CRUMB(L"%s:  Z 1a 1a 2", FuncTag);

                    break;
                }
            }
            Entry->me.SubScreen = SubScreen;
        } while (0); // This 'loop' only runs once

        BREAD_CRUMB(L"%s:  B END MAIN - VOID", FuncTag);
        LOG_DECREMENT();
        LOG_SEP(L"X");
    }
} // VOID GenerateSubScreen()

// Sets a few defaults for a loader entry -- mainly the icon, but also the OS type
// code and shortcut letter. For Linux EFI stub loaders, also sets kernel options
// that will (with luck) work fairly automatically.
VOID SetLoaderDefaults (
    LOADER_ENTRY *Entry,
    CHAR16       *LoaderPath,
    REFIT_VOLUME *Volume
) {
    EFI_STATUS              Status;
    CHAR16                 *Temp;
    CHAR16                 *PathOnly;
    CHAR16                 *NameClues;
    CHAR16                 *OSIconName;
    CHAR16                 *TmpIconName;
    CHAR16                 *TargetName;
    CHAR16                 *DisplayName;
    CHAR16                 *NoExtension;
    CHAR16                  ShortcutLetter;
    BOOLEAN                 MacFlag;
    BOOLEAN                 GetImage;
    BOOLEAN                 MergeFsName;
    APPLE_APFS_VOLUME_ROLE  VolumeRole;


    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"SetLoaderDefaults";

    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Getting Default Setting for Loader:- '%s'",
        (Entry->me.Title) ? Entry->me.Title : Entry->Title
    );
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  1 - START", FuncTag);
    NameClues = Basename (LoaderPath);

    BREAD_CRUMB(L"%s:  2", FuncTag);
    PathOnly = FindPath (LoaderPath);

    BREAD_CRUMB(L"%s:  3", FuncTag);
    if (!AllowGraphicsMode) {
        BREAD_CRUMB(L"%s:  3a 1", FuncTag);
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"In SetLoaderDefaults ... Skipped Loading Icon in %s Mode",
            (GlobalConfig.DirectBoot)
                ? L"DirectBoot"
                : L"Text Screen"
        );
        #endif
    }
    else {
        BREAD_CRUMB(L"%s:  3b 1", FuncTag);
        if (Volume->DiskKind == DISK_KIND_NET) {
            BREAD_CRUMB(L"%s:  3b 1a 1", FuncTag);
            MergeStrings (&NameClues, Entry->me.Title, L' ');
        }
        else {
            BREAD_CRUMB(L"%s:  3b 1b 1", FuncTag);
            if (!Entry->me.Image &&
                !GlobalConfig.HelpIcon &&
                !GlobalConfig.HiddenIconsIgnore &&
                GlobalConfig.HiddenIconsPrefer
            ) {
                BREAD_CRUMB(L"%s:  3b 1b 1a 1", FuncTag);
                #if REFIT_DEBUG > 0
                ALT_LOG(1, LOG_LINE_NORMAL, L"Checking for '.VolumeIcon' Image");
                #endif

                // use a ".VolumeIcon" image icon for the loader
                // Takes precedence all over options
                Entry->me.Image = egCopyImage (Volume->VolIconImage);
            }

            BREAD_CRUMB(L"%s:  3b 1b 2", FuncTag);
            if (!Entry->me.Image) {
                BREAD_CRUMB(L"%s:  3b 1b 2a 1", FuncTag);
                #if REFIT_DEBUG > 0
                if (!GlobalConfig.HelpIcon &&
                    !GlobalConfig.HiddenIconsIgnore &&
                    GlobalConfig.HiddenIconsPrefer
                ) {
                    ALT_LOG(1, LOG_LINE_NORMAL, L"Could Not Find '.VolumeIcon' Image");
                }
                #endif

                BREAD_CRUMB(L"%s:  3b 1b 2a 2", FuncTag);
                MacFlag = FALSE;
                if (LoaderPath &&
                    FindSubStr (LoaderPath, L"System\\Library\\CoreServices")
                ) {
                    BREAD_CRUMB(L"%s:  3b 1b 2a 2a 1", FuncTag);
                    MacFlag = TRUE;
                }

                BREAD_CRUMB(L"%s:  3b 1b 2a 3", FuncTag);
                if (!GlobalConfig.SyncAPFS || !MacFlag) {
                    BREAD_CRUMB(L"%s:  3b 1b 2a 3a 1", FuncTag);
                    NoExtension = StripEfiExtension (NameClues);
                    if (NoExtension != NULL) {
                        // locate a custom icon for the loader
                        // Anything found here takes precedence over the "hints" in the OSIconName variable
                        #if REFIT_DEBUG > 0
                        ALT_LOG(1, LOG_LINE_NORMAL, L"Search for Icon in Bootloader Directory");
                        #endif

                        BREAD_CRUMB(L"%s:  3b 1b 2a 3a 1a 1", FuncTag);
                        if (!Entry->me.Image) {
                            BREAD_CRUMB(L"%s:  3b 1b 2a 3a 1a 1a 1", FuncTag);
                            Entry->me.Image = egLoadIconAnyType (
                                Volume->RootDir,
                                PathOnly,
                                NoExtension,
                                GlobalConfig.IconSizes[ICON_SIZE_BIG]
                            );
                        }

                        BREAD_CRUMB(L"%s:  3b 1b 2a 3a 1a 2", FuncTag);
                        if (!Entry->me.Image &&
                            !GlobalConfig.HiddenIconsIgnore
                        ) {
                            BREAD_CRUMB(L"%s:  3b 1b 2a 3a 1a 2a 1", FuncTag);
                            Entry->me.Image = egCopyImage (Volume->VolIconImage);
                        }

                        BREAD_CRUMB(L"%s:  3b 1b 2a 3a 1a 3", FuncTag);
                        MY_FREE_POOL(NoExtension);
                    }
                    BREAD_CRUMB(L"%s:  3b 1b 2a 3a 2", FuncTag);
                }
                BREAD_CRUMB(L"%s:  3b 1b 2a 4", FuncTag);
            }

            BREAD_CRUMB(L"%s:  3b 1b 3", FuncTag);
            // Begin creating icon "hints" by using last part of directory path leading
            // to the loader
            #if REFIT_DEBUG > 0
            if (Entry->me.Image == NULL) {
                ALT_LOG(1, LOG_THREE_STAR_MID,
                    L"Creating Icon Hint from Loader Path:- '%s'",
                    LoaderPath
                );
            }
            #endif

            BREAD_CRUMB(L"%s:  3b 1b 4", FuncTag);
            Temp = FindLastDirName (LoaderPath);

            BREAD_CRUMB(L"%s:  3b 1b 5", FuncTag);
            OSIconName = NULL;
            MergeStrings (&OSIconName, Temp, L',');

            BREAD_CRUMB(L"%s:  3b 1b 6", FuncTag);
            MY_FREE_POOL(Temp);

            BREAD_CRUMB(L"%s:  3b 1b 7", FuncTag);
            ShortcutLetter = 0;
            if (OSIconName != NULL) {
                BREAD_CRUMB(L"%s:  3b 1b 7a 1", FuncTag);
                ShortcutLetter = OSIconName[0];
            }

            BREAD_CRUMB(L"%s:  3b 1b 8", FuncTag);
            // Add every "word" in the filesystem and partition names, delimited by
            // spaces, dashes (-), underscores (_), or colons (:), to the list of
            // hints to be used in searching for OS icons.
            if (Volume->FsName && (Volume->FsName[0] != L'\0')) {
                MergeFsName = TRUE;

                BREAD_CRUMB(L"%s:  3b 1b 8a 1", FuncTag);
                if (FindSubStr (Volume->FsName, L"PreBoot") && GlobalConfig.SyncAPFS) {
                    BREAD_CRUMB(L"%s:  3b 1b 8a 1a 1", FuncTag);
                    MergeFsName = FALSE;
                }

                BREAD_CRUMB(L"%s:  3b 1b 8a 2", FuncTag);
                if (MergeFsName) {
                    BREAD_CRUMB(L"%s:  3b 1b 8a 2a 1", FuncTag);
                    #if REFIT_DEBUG > 0
                    if (Entry->me.Image == NULL) {
                        ALT_LOG(1, LOG_LINE_NORMAL,
                            L"Merge Hints Based on Filesystem Name:- '%s'",
                            Volume->FsName
                        );
                    }
                    #endif

                    BREAD_CRUMB(L"%s:  3b 1b 8a 2a 2", FuncTag);
                    MergeUniqueWords (&OSIconName, Volume->FsName, L',');
                }
                else {
                    BREAD_CRUMB(L"%s:  3b 1b 8a 2b 1", FuncTag);
                    if (Volume->VolName && (Volume->VolName[0] != L'\0')) {
                        BREAD_CRUMB(L"%s:  3b 1b 8a 2b 1a 1", FuncTag);
                        DisplayName = NULL;

                        if (Volume->FSType == FS_TYPE_APFS && GlobalConfig.SyncAPFS) {
                            BREAD_CRUMB(L"%s:  3b 1b 8a 2b 1a 1a 1", FuncTag);
                            VolumeRole = 0;

                            // DA-TAG: Limit to TianoCore
                            #ifndef __MAKEWITH_TIANO
                            Status = EFI_NOT_STARTED;
                            #else
                            Status = RP_GetApfsVolumeInfo (
                                Volume->DeviceHandle,
                                NULL, NULL,
                                &VolumeRole
                            );
                            #endif

                            BREAD_CRUMB(L"%s:  3b 1b 8a 2b 1a 1a 2", FuncTag);
                            if (!EFI_ERROR(Status)) {
                                BREAD_CRUMB(L"%s:  3b 1b 8a 2b 1a 1a 2a 1", FuncTag);
                                if (VolumeRole == APPLE_APFS_VOLUME_ROLE_PREBOOT) {
                                    BREAD_CRUMB(L"%s:  3b 1b 8a 2b 1a 1a 2a 1a 1", FuncTag);
                                    DisplayName = GetVolumeGroupName (Entry->LoaderPath, Volume);
                                }
                                BREAD_CRUMB(L"%s:  3b 1b 8a 2b 1a 1a 2a 2", FuncTag);
                            }

                            BREAD_CRUMB(L"%s:  3b 1b 8a 2b 1a 1a 3", FuncTag);
                        }

                        BREAD_CRUMB(L"%s:  3b 1b 8a 2b 1a 2", FuncTag);
                        TargetName = (DisplayName)
                            ? StrDuplicate (DisplayName)
                            : StrDuplicate (Volume->VolName);

                        #if REFIT_DEBUG > 0
                        BREAD_CRUMB(L"%s:  3b 1b 8a 2b 1a 3", FuncTag);
                        if (Entry->me.Image == NULL) {
                            BREAD_CRUMB(L"%s:  3b 1b 8a 2b 1a 3a 1", FuncTag);
                            ALT_LOG(1, LOG_LINE_NORMAL,
                                L"Merge Hints Based on Volume Name:- '%s'",
                                TargetName
                            );
                        }
                        #endif

                        BREAD_CRUMB(L"%s:  3b 1b 8a 2b 1a 4", FuncTag);
                        MergeUniqueWords (&OSIconName, TargetName, L',');

                        BREAD_CRUMB(L"%s:  3b 1b 8a 2b 1a 5", FuncTag);
                        MY_FREE_POOL(DisplayName);
                        MY_FREE_POOL(TargetName);
                    } // if Volume->VolName

                    BREAD_CRUMB(L"%s:  3b 1b 8a 2b 2", FuncTag);
                } // if/else MergeFsName

                BREAD_CRUMB(L"%s:  3b 1b 8a 3", FuncTag);
            } // if Volume->FsName

            BREAD_CRUMB(L"%s:  3b 1b 9", FuncTag);
            if (Volume->PartName &&
                Volume->PartName[0] != L'\0'
            ) {
                #if REFIT_DEBUG > 0
                if (Entry->me.Image == NULL) {
                    ALT_LOG(1, LOG_LINE_NORMAL,
                        L"Merge Hints Based on Partition Name:- '%s'",
                        Volume->PartName
                    );
                }
                #endif

                BREAD_CRUMB(L"%s:  3b 1b 9a 1", FuncTag);
                MergeUniqueWords (&OSIconName, Volume->PartName, L',');
            }
            BREAD_CRUMB(L"%s:  3b 1b 10", FuncTag);
        } // if/else Volume->DiskKind == DISK_KIND_NET
        BREAD_CRUMB(L"%s:  3b 2", FuncTag);
    }

    BREAD_CRUMB(L"%s:  4", FuncTag);
    GetImage = (AllowGraphicsMode && !Entry->me.Image);

    // Detect specific loaders
    BREAD_CRUMB(L"%s:  5", FuncTag);
    if (GetImage) TmpIconName = NULL;
    if (IsInSubstring (NameClues, GlobalConfig.LinuxPrefixes)) {
        BREAD_CRUMB(L"%s:  5a 1", FuncTag);
        if (Volume->DiskKind != DISK_KIND_NET) {
            BREAD_CRUMB(L"%s:  5a 1a 1", FuncTag);
            GuessLinuxDistribution (&TmpIconName, Volume, LoaderPath);

            BREAD_CRUMB(L"%s:  5a 1a 2", FuncTag);
            Entry->LoadOptions = GetMainLinuxOptions (LoaderPath, Volume);
        }

        BREAD_CRUMB(L"%s:  5a 2", FuncTag);
        if (GetImage) MergeUniqueStrings (&TmpIconName, L"linux", L',');
        Entry->OSType = 'L';

        BREAD_CRUMB(L"%s:  5a 3", FuncTag);
        if (ShortcutLetter == 0) {
            BREAD_CRUMB(L"%s:  5a 3a 1", FuncTag);
            ShortcutLetter = 'L';
        }

        BREAD_CRUMB(L"%s:  5a 4", FuncTag);
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;
    }
    else if (FindSubStr (LoaderPath, L"refindplus")) {
        BREAD_CRUMB(L"%s:  5b 1", FuncTag);
        if (GetImage) MergeUniqueStrings (&TmpIconName, L"refindplus", L',');

        BREAD_CRUMB(L"%s:  5b 2", FuncTag);
        Entry->OSType = 'R';
        ShortcutLetter = 'R';
    }
    else if (FindSubStr (LoaderPath, L"refind")) {
        BREAD_CRUMB(L"%s:  5c 1 - A", FuncTag);
        if (GetImage) MergeUniqueStrings (&TmpIconName, L"refind", L',');

        BREAD_CRUMB(L"%s:  5c 2 - A", FuncTag);
        Entry->OSType = 'R';
        ShortcutLetter = 'R';
    }
    else if (FindSubStr (LoaderPath, L"refit")) {
        BREAD_CRUMB(L"%s:  5c 1 - B", FuncTag);
        if (GetImage) MergeUniqueStrings (&TmpIconName, L"refit", L',');

        BREAD_CRUMB(L"%s:  5c 2 - B", FuncTag);
        Entry->OSType = 'R';
        ShortcutLetter = 'R';
    }
    else if (FindSubStr (LoaderPath, MACOSX_LOADER_PATH)) {
        BREAD_CRUMB(L"%s:  5d 1", FuncTag);
        if (FileExists (Volume->RootDir, L"EFI\\refindplus\\config.conf") ||
            FileExists (Volume->RootDir, L"EFI\\refindplus\\refind.conf") ||
            FileExists (Volume->RootDir, L"EFI\\refind\\config.conf")     ||
            FileExists (Volume->RootDir, L"EFI\\refind\\refind.conf")
        ) {
            BREAD_CRUMB(L"%s:  5d 1a 1", FuncTag);
            if (GetImage) MergeUniqueStrings (&TmpIconName, L"refind", L',');

            BREAD_CRUMB(L"%s:  5d 1a 2", FuncTag);
            Entry->OSType = 'R';
            ShortcutLetter = 'R';
        }
        else {
            BREAD_CRUMB(L"%s:  5d 1b 1", FuncTag);
            if (GetImage) MergeUniqueStrings (&TmpIconName, L"mac", L',');

            BREAD_CRUMB(L"%s:  5d 1b 2", FuncTag);
            Entry->OSType = 'M';
            ShortcutLetter = 'M';
            Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
        }
        BREAD_CRUMB(L"%s:  5d 2", FuncTag);
    }
    else if (MyStriCmp (NameClues, L"diags.efi")) {
        BREAD_CRUMB(L"%s:  5e 1", FuncTag);
        if (GetImage) MergeUniqueStrings (&TmpIconName, L"hwtest", L',');
    }
    else if (MyStriCmp (NameClues, L"e.efi") ||
        MyStriCmp (NameClues, L"elilo.efi")  ||
        FindSubStr (NameClues, L"elilo")
    ) {
        BREAD_CRUMB(L"%s:  5f 1", FuncTag);
        if (GetImage) MergeUniqueStrings (&TmpIconName, L"elilo,linux", L',');
        Entry->OSType = 'E';

        BREAD_CRUMB(L"%s:  5f 2", FuncTag);
        if (ShortcutLetter == 0) {
            BREAD_CRUMB(L"%s:  5f 2a 1", FuncTag);
            ShortcutLetter = 'L';
        }

        BREAD_CRUMB(L"%s:  5f 3", FuncTag);
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
    }
    else if (FindSubStr (NameClues, L"grub")) {
        BREAD_CRUMB(L"%s:  5g 1", FuncTag);
        if (GetImage) MergeUniqueStrings (&TmpIconName, L"grub,linux", L',');

        BREAD_CRUMB(L"%s:  5g 2", FuncTag);
        Entry->OSType = 'G';
        ShortcutLetter = 'G';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_GRUB;
    }
    else if (MyStriCmp (NameClues, L"cdboot.efi") ||
        MyStriCmp (NameClues, L"bootmgr.efi")  ||
        MyStriCmp (NameClues, L"bootmgfw.efi") ||
        MyStriCmp (NameClues, L"bkpbootmgfw.efi")
    ) {
        BREAD_CRUMB(L"%s:  5h 1", FuncTag);
        if (GetImage) MergeUniqueStrings (&TmpIconName, L"win8", L',');

        BREAD_CRUMB(L"%s:  5h 2", FuncTag);
        Entry->OSType = 'W';
        ShortcutLetter = 'W';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
    }
    else if (MyStriCmp (NameClues, L"xom.efi")) {
        BREAD_CRUMB(L"%s:  5i 1", FuncTag);
        if (GetImage) MergeUniqueStrings (&TmpIconName, L"xom,win,win8", L',');

        BREAD_CRUMB(L"%s:  5i 2", FuncTag);
        Entry->OSType = 'X';
        ShortcutLetter = 'W';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
    }
    else if (MyStriCmp (NameClues, L"opencore")) {
        BREAD_CRUMB(L"%s:  5j 1", FuncTag);
        if (GetImage) MergeUniqueStrings (&TmpIconName, L"opencore", L',');

        BREAD_CRUMB(L"%s:  5j 2", FuncTag);
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OPENCORE;
    }
    else if (MyStriCmp (NameClues, L"clover")) {
        BREAD_CRUMB(L"%s:  5k 1", FuncTag);
        if (GetImage) MergeUniqueStrings (&TmpIconName, L"clover", L',');

        BREAD_CRUMB(L"%s:  5k 2", FuncTag);
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_CLOVER;
    }
    else if (FindSubStr (NameClues, L"ipxe")) {
        BREAD_CRUMB(L"%s:  5l 1", FuncTag);
        if (GetImage) MergeUniqueStrings (&TmpIconName, L"network", L',');

        BREAD_CRUMB(L"%s:  5l 2", FuncTag);
        Entry->OSType = 'N';
        ShortcutLetter = 'N';
    }

    BREAD_CRUMB(L"%s:  6", FuncTag);
    if (GlobalConfig.HelpIcon) {
        BREAD_CRUMB(L"%s:  6a 1", FuncTag);
        if (GetImage) {
            BREAD_CRUMB(L"%s:  6a 1a 1", FuncTag);
            MergeStrings (&TmpIconName, OSIconName, L',');
            MY_FREE_POOL(OSIconName);
            OSIconName = TmpIconName;
        }
    }
    else {
        BREAD_CRUMB(L"%s:  6b 1", FuncTag);
        if (GetImage) MergeStrings (&OSIconName, TmpIconName, L',');
    }

    BREAD_CRUMB(L"%s:  7", FuncTag);
    if ((ShortcutLetter >= 'a') && (ShortcutLetter <= 'z')) {
        BREAD_CRUMB(L"%s:  7a 1", FuncTag);
        ShortcutLetter = ShortcutLetter - 'a' + 'A'; // convert lowercase to uppercase
    }

    BREAD_CRUMB(L"%s:  8", FuncTag);
    Entry->me.ShortcutLetter = ShortcutLetter;
    if (GetImage) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Trying to Locate an Icon Based on Hints:- '%s'",
            OSIconName
        );
        #endif

        BREAD_CRUMB(L"%s:  8a 1", FuncTag);
        Entry->me.Image = LoadOSIcon (OSIconName, L"unknown", FALSE);
    }

    BREAD_CRUMB(L"%s:  9", FuncTag);
    MY_FREE_POOL(PathOnly);
    MY_FREE_POOL(OSIconName);
    MY_FREE_POOL(NameClues);

    BREAD_CRUMB(L"%s:  10 - END:- VOID", FuncTag);
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
            if (GuidsAreEqual (&(SystemVolumes[i]->PartGuid), &(Volume->PartGuid))) {
                VolumeGroupName = StrDuplicate (SystemVolumes[i]->VolName);
                break;
            }
        } // for
    }

    if (VolumeGroupName == NULL) {
        for (i = 0; i < SystemVolumesCount; i++) {
            if (
                FindSubStr (
                    LoaderPath,
                    GuidAsString (&(SystemVolumes[i]->VolUuid))
                )
            ) {
                VolumeGroupName = StrDuplicate (SystemVolumes[i]->VolName);
                break;
            }
        } // for
    }

    if (VolumeGroupName == NULL) {
        for (i = 0; i < DataVolumesCount; i++) {
            if (
                FindSubStr (
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
    IN EG_IMAGE                 *Icon
) {
    CHAR16        *TempStr;
    CHAR16        *FullTitle;
    CHAR16        *OSIconName;
    LOADER_ENTRY  *Entry;

    Entry = InitializeLoaderEntry (NULL);
    if (!Entry) {
        return NULL;
    }

    Entry->DiscoveryType = DISCOVERY_TYPE_AUTO;
    FullTitle = (!LoaderTitle)
        ? NULL
        : PoolPrint (L"Reboot to %s", LoaderTitle);

    Entry->me.Row        = Row;
    Entry->me.Tag        = TAG_FIRMWARE_LOADER;
    Entry->me.Title      = StrDuplicate ((FullTitle) ? FullTitle : L"Instance: Unknown");
    Entry->Title         = StrDuplicate ((LoaderTitle) ? LoaderTitle : L"Instance: Unknown"); // without "Reboot to"
    Entry->EfiLoaderPath = DuplicateDevicePath (EfiLoaderPath);
    TempStr              = DevicePathToStr (EfiLoaderPath);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"UEFI Loader Path:- '%s'", TempStr);
    #endif

    MY_FREE_POOL(TempStr);

    Entry->EfiBootNum = EfiBootNum;

    OSIconName = NULL;
    MergeUniqueWords (&OSIconName, Entry->me.Title, L',');
    MergeUniqueStrings (&OSIconName, L"Unknown", L',');

    Entry->me.Image = (Icon)
        ? egCopyImage (Icon)
        : LoadOSIcon (OSIconName, NULL, FALSE);

    Entry->me.BadgeImage = (Row == 0)
        ? BuiltinIcon (BUILTIN_ICON_VOL_EFI)
        : NULL;

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
    IN     BOOLEAN       SubScreenReturn
) {
    EFI_STATUS              Status;
    CHAR16                 *DisplayName;
    CHAR16                 *TmpName;
    BOOLEAN                 FoundPreBootName;
    LOADER_ENTRY           *Entry;
    APPLE_APFS_VOLUME_ROLE  VolumeRole;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    if (Volume == NULL) {
        // Early Return
        return NULL;
    }

    if (Volume->FSType == FS_TYPE_NTFS) {
        if (MyStriCmp (Volume->VolName, L"System Reserved") ||
            MyStriCmp (Volume->VolName, L"Basic Data Partition") ||
            MyStriCmp (Volume->VolName, L"Microsoft Reserved Partition")
        ) {
            // Early Return on Windows Support Volume
            return NULL;
        }
    }

    DisplayName = NULL;
    if (Volume->FSType == FS_TYPE_APFS) {
        VolumeRole = 0;

        // DA-TAG: Limit to TianoCore
        #ifndef __MAKEWITH_TIANO
        Status = EFI_NOT_STARTED;
        #else
        Status = RP_GetApfsVolumeInfo (
            Volume->DeviceHandle,
            NULL, NULL,
            &VolumeRole
        );
        #endif
        if (EFI_ERROR(Status) && Status != EFI_NOT_STARTED) {
            // Early Return on Invalid APFS Volume
            return NULL;
        }

        if (Status != EFI_NOT_STARTED) {
            if (VolumeRole != APPLE_APFS_VOLUME_ROLE_SYSTEM  &&
                VolumeRole != APPLE_APFS_VOLUME_ROLE_PREBOOT &&
                VolumeRole != APPLE_APFS_VOLUME_ROLE_UNDEFINED
            ) {
                // Early Return on APFS Support Volume
                return NULL;
            }

            if (GlobalConfig.SyncAPFS) {
                if (VolumeRole == APPLE_APFS_VOLUME_ROLE_PREBOOT) {
                    DisplayName = GetVolumeGroupName (LoaderPath, Volume);

                    if (!DisplayName) {
                        // Do not display this PreBoot Volume Menu Entry
                        return NULL;
                    }

                    FoundPreBootName = FindSubStr (DisplayName, L"PreBoot");
                    if (FoundPreBootName) {
                        MY_FREE_POOL(DisplayName);
                    }
                    else {
                        MY_FREE_POOL(Volume->VolName);
                        Volume->VolName = PoolPrint (L"PreBoot - %s", DisplayName);
                    }
                }
            }
        }
    } // if/else if Volume->FSType

    CleanUpPathNameSlashes (LoaderPath);
    Entry = InitializeLoaderEntry (NULL);
    if (Entry == NULL) {
        // Early Return
        return NULL;
    }

    Entry->DiscoveryType = DISCOVERY_TYPE_AUTO;

    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_SET;
    #endif
    if (FindSubStr(Volume->VolName, L"OS X Install") ||
        FindSubStr(Volume->VolName, L"Mac OS Install")
    ) {
        Entry->Title = StrDuplicate (L"MacOS Installer");
    }
    else {
        Entry->Title = StrDuplicate (
            (LoaderTitle) ? LoaderTitle : LoaderPath
        );
    }
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    #if REFIT_DEBUG > 0
    if (DisplayName) {
        ALT_LOG(1, LOG_THREE_STAR_MID, L"Synced PreBoot:- '%s'", DisplayName);
    }
    ALT_LOG(1, LOG_LINE_NORMAL, L"Add Loader Entry:- '%s'", Entry->Title);
    ALT_LOG(1, LOG_LINE_NORMAL, L"UEFI Loader File:- '%s'", LoaderPath);
    #endif

    TmpName = (DisplayName) ? DisplayName : Volume->VolName;
    Entry->me.Title = (DisplayName || Volume->VolName)
        ? PoolPrint (
            L"Load %s%s%s%s%s",
            Entry->Title,
            SetVolJoin (Entry->Title),
            SetVolKind (Entry->Title, TmpName),
            SetVolFlag (Entry->Title, TmpName),
            SetVolType (Entry->Title, TmpName)
        )
        : PoolPrint (
            L"Load %s",
            (LoaderTitle) ? LoaderTitle : LoaderPath
        );
    Entry->me.Row = 0;
    Entry->me.BadgeImage = egCopyImage (Volume->VolBadgeImage);

    if ((LoaderPath != NULL) && (LoaderPath[0] != L'\\')) {
        Entry->LoaderPath = StrDuplicate (L"\\");
    }
    else {
        Entry->LoaderPath = NULL;
    }

    MergeStrings (&(Entry->LoaderPath), LoaderPath, 0);
    Entry->Volume = CopyVolume (Volume);
    SetLoaderDefaults (Entry, LoaderPath, Volume);
    GenerateSubScreen (Entry, Volume, SubScreenReturn);
    AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *) Entry);

    #if REFIT_DEBUG > 0
    TmpName = (DisplayName)
        ? DisplayName
        : (Volume->VolName)
            ? Volume->VolName
            : Entry->LoaderPath;
    LOG_MSG(
        "%s  - Found %s%s%s%s%s",
        OffsetNext,
        Entry->Title,
        SetVolJoin (Entry->Title),
        SetVolKind (Entry->Title, TmpName),
        SetVolFlag (Entry->Title, TmpName),
        SetVolType (Entry->Title, TmpName)
    );

    ALT_LOG(1, LOG_THREE_STAR_END, L"Successfully Created Menu Entry for %s", Entry->Title);
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
        return (-1);
    }
    else if (Time1InSeconds > Time2InSeconds) {
        return (1);
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
    BOOLEAN LinuxRescue;

    if (LoaderList == NULL) {
        LatestEntry = NewEntry;
    }
    else {
        PrevEntry   = NULL;
        LatestEntry = CurrentEntry = LoaderList;
        LinuxRescue = (StriSubCmp (L"vmlinuz-0-rescue", NewEntry->FileName)) ? TRUE : FALSE;

        while ((CurrentEntry != NULL) &&
            !StriSubCmp (L"vmlinuz-0-rescue", CurrentEntry->FileName) &&
            (LinuxRescue || (TimeComp (&(NewEntry->TimeStamp), &(CurrentEntry->TimeStamp)) < 0))
        ) {
            PrevEntry   = CurrentEntry;
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

    return (LatestEntry);
} // static VOID AddLoaderListEntry()

// Delete the LOADER_LIST linked list
static
VOID CleanUpLoaderList (
    struct LOADER_LIST *LoaderList
) {
    struct LOADER_LIST *Temp;

    if (!LoaderList) {
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
    IN CHAR16 *VolumeName
) {
    CHAR16 *RetVal;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;

    MY_MUTELOGGER_SET;
    #endif
    if (0);
    else if (FindSubStr (InstanceName, L"Manual Stanza:" ))   RetVal = L""         ;
    else if (MyStriCmp  (VolumeName,   L"EFI"            ))   RetVal = L""         ;
    else if (MyStriCmp  (VolumeName,   L"ESP"            ))   RetVal = L""         ;
    else if (MyStriCmp  (InstanceName, L"(Legacy)"       ))   RetVal = L""         ;
    else if (MyStriCmp  (InstanceName, L"Legacy Bootcode"))   RetVal = L""         ;
    else if (MyStriCmp  (VolumeName,   L"BOOTCAMP"       ))   RetVal = L""         ;
    else if (FindSubStr (VolumeName,   L"Volume"         ))   RetVal = L""         ;
    else if (FindSubStr (VolumeName,   L"Partition"      ))   RetVal = L""         ;
    else if (FindSubStr (InstanceName, L"Instance:"      ))   RetVal = L"Volume - ";
    else                                                      RetVal = L""         ;
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    return RetVal;
} // CHAR16 * SetVolKind()

CHAR16 * SetVolJoin (
    IN CHAR16 *InstanceName
) {
    CHAR16 *RetVal;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;

    MY_MUTELOGGER_SET;
    #endif
    if (0);
    else if (FindSubStr (InstanceName, L"Manual Stanza:" ))   RetVal = L""     ;
    else if (MyStriCmp  (InstanceName, L"Legacy Bootcode"))   RetVal = L" for ";
    else                                                      RetVal = L" on " ;
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    return RetVal;
} // CHAR16 * SetVolJoin()

CHAR16 * SetVolFlag (
    IN CHAR16 *InstanceName,
    IN CHAR16 *VolumeName
) {
    CHAR16 *RetVal;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;

    MY_MUTELOGGER_SET;
    #endif
    if (0);
    else if (FindSubStr (InstanceName, L"Manual Stanza:"))   RetVal = L""       ;
    else                                                     RetVal = VolumeName;
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    return RetVal;
} // CHAR16 * SetVolFlag()

CHAR16 * SetVolType (
    IN CHAR16 *InstanceName OPTIONAL,
    IN CHAR16 *VolumeName
) {
    CHAR16 *RetVal;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;

    MY_MUTELOGGER_SET;
    #endif
    if (0);
    else if (FindSubStr (InstanceName, L"Manual Stanza:" ))   RetVal = L""                 ;
    else if (MyStriCmp  (VolumeName,   L"EFI"            ))   RetVal = L" System Partition";
    else if (MyStriCmp  (VolumeName,   L"ESP"            ))   RetVal = L""                 ;
    else if (MyStriCmp  (InstanceName, L"(Legacy)"       ))   RetVal = L" Partition"       ;
    else if (MyStriCmp  (InstanceName, L"Legacy Bootcode"))   RetVal = L" Partition"       ;
    else if (MyStriCmp  (VolumeName,   L"BOOTCAMP"       ))   RetVal = L" Partition"       ;
    else if (FindSubStr (VolumeName,   L"Volume"         ))   RetVal = L""                 ;
    else if (FindSubStr (VolumeName,   L"Partition"      ))   RetVal = L""                 ;
    else if (FindSubStr (InstanceName, L"Instance:"      ))   RetVal = L""                 ;
    else                                                      RetVal = L" Volume"          ;
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

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
    EFI_STATUS              Status;
    UINTN                   i;
    CHAR16                 *VolName;
    CHAR16                 *VolGuid;
    CHAR16                 *PathCopy;
    CHAR16                 *DontScanDir;
    CHAR16                 *TmpVolNameA;
    CHAR16                 *TmpVolNameB;
    BOOLEAN                 ScanIt;
    APPLE_APFS_VOLUME_ROLE  VolumeRole;


    // Skip 'UnReadable' volumes
    if (!Volume->IsReadable) {
        return FALSE;
    }

    if (MyStriCmp (Path, SelfDirPath)
        && (Volume->DeviceHandle == SelfVolume->DeviceHandle)
    ) {
        return FALSE;
    }

    // DA-TAG: Do not scan HFS+ Recovery HD
    if (Volume->FSType == FS_TYPE_HFSPLUS) {
        if (FindSubStr (Volume->VolName, L"Recovery HD")) {
            return FALSE;
        }
    }

    ScanIt = TRUE;
    if (Volume->FSType == FS_TYPE_APFS) {
        VolumeRole = 0;

        // DA-TAG: Limit to TianoCore
        #ifndef __MAKEWITH_TIANO
        Status = EFI_NOT_STARTED;
        #else
        Status = RP_GetApfsVolumeInfo (
            Volume->DeviceHandle,
            NULL, NULL,
            &VolumeRole
        );
        #endif

        if (!EFI_ERROR(Status)) {
            if (VolumeRole != APPLE_APFS_VOLUME_ROLE_SYSTEM  &&
                VolumeRole != APPLE_APFS_VOLUME_ROLE_PREBOOT &&
                VolumeRole != APPLE_APFS_VOLUME_ROLE_UNDEFINED
            ) {
                // Early Return on APFS Support Volume
                return FALSE;
            }

            if (GlobalConfig.SyncAPFS) {
                TmpVolNameB = PoolPrint (L"%s - DATA", Volume->VolName);
                if (IsIn (TmpVolNameB, GlobalConfig.DontScanVolumes)) {
                    ScanIt = FALSE;
                }
                MY_FREE_POOL(TmpVolNameB);
                if (!ScanIt) return FALSE;

                TmpVolNameA = SanitiseString (Volume->VolName);
                TmpVolNameB = PoolPrint (L"%s - DATA", TmpVolNameA);
                if (IsIn (TmpVolNameB, GlobalConfig.DontScanVolumes)) {
                    ScanIt = FALSE;
                }
                MY_FREE_POOL(TmpVolNameA);
                MY_FREE_POOL(TmpVolNameB);
                if (!ScanIt) return FALSE;
            }
        }
    } // if Volume->FSType

    VolGuid = GuidAsString (&(Volume->PartGuid));
    if (IsIn (VolGuid,          GlobalConfig.DontScanVolumes) ||
        IsIn (Volume->FsName,   GlobalConfig.DontScanVolumes) ||
        IsIn (Volume->VolName,  GlobalConfig.DontScanVolumes) ||
        IsIn (Volume->PartName, GlobalConfig.DontScanVolumes)
    ) {
        ScanIt = FALSE;
    }
    MY_FREE_POOL(VolGuid);
    if (!ScanIt) return FALSE;

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
        }
    }
    MY_FREE_POOL(PathCopy);
    MY_FREE_POOL(VolName);

    // See if Volume is in GlobalConfig.DontScanDirs.
    i = 0;
    while (ScanIt &&
        (DontScanDir = FindCommaDelimited (GlobalConfig.DontScanDirs, i++))
    ) {
        SplitVolumeAndFilename (&DontScanDir, &VolName);
        CleanUpPathNameSlashes (DontScanDir);
        if (VolName != NULL) {
            if (VolumeMatchesDescription (Volume, VolName)
                && MyStriCmp (DontScanDir, Path)
            ) {
                ScanIt = FALSE;
            }
        }
        else {
            if (MyStriCmp (DontScanDir, Path)) {
                ScanIt = FALSE;
            }
        }

        MY_FREE_POOL(DontScanDir);
        MY_FREE_POOL(VolName);
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
        FileContents = AllocatePool (FileSize);
        FallbackContents = AllocatePool (FallbackSize);
        if (FileContents && FallbackContents) {
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
    CHAR16                  *Extension;
    CHAR16                  *FullName;
    struct LOADER_LIST      *NewLoader;
    struct LOADER_LIST      *LoaderList;
    LOADER_ENTRY            *FirstKernel;
    LOADER_ENTRY            *LatestEntry;
    BOOLEAN                  IsLinux;
    BOOLEAN                  InSelfPath;
    BOOLEAN                  SkipDir;
    BOOLEAN                  RetVal;
    BOOLEAN                  FoundFallbackDuplicate;

    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"ScanLoaderDir";
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  1 - START", FuncTag);

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;

    ALT_LOG(1, LOG_LINE_NORMAL, L"Scanning for '%s' in '%s'", Pattern, Path);
    #endif

    InSelfPath = MyStriCmp (Path, SelfDirPath);

    BREAD_CRUMB(L"%s:  2", FuncTag);
    FoundFallbackDuplicate = FALSE;
    if ((!SelfDirPath || !Path ||
        (InSelfPath && (Volume->DeviceHandle != SelfVolume->DeviceHandle)) ||
        (!InSelfPath)) && (ShouldScan (Volume, Path))
    ) {
        LoaderList = NULL;

        //BREAD_CRUMB(L"%s:  2a 1", FuncTag);
        // Look through contents of the directory
        DirIterOpen (Volume->RootDir, Path, &DirIter);

        //BREAD_CRUMB(L"%s:  2a 2", FuncTag);
        while (DirIterNext (&DirIter, 2, Pattern, &DirEntry)) {
            //LOG_SEP(L"X");
            //BREAD_CRUMB(L"%s:  2a 2a 1 - WHILE LOOP:- START", FuncTag);
            Extension = FindExtension (DirEntry->FileName);

            //BREAD_CRUMB(L"%s:  2a 2a 2", FuncTag);
            FullName  = StrDuplicate (Path);

            //BREAD_CRUMB(L"%s:  2a 2a 3", FuncTag);
            MergeStrings (&FullName, DirEntry->FileName, L'\\');

            //BREAD_CRUMB(L"%s:  2a 2a 4", FuncTag);
            CleanUpPathNameSlashes (FullName);

            //BREAD_CRUMB(L"%s:  2a 2a 5", FuncTag);
            if (!GlobalConfig.FollowSymlinks) {
                //BREAD_CRUMB(L"%s:  2a 2a 5a 1", FuncTag);
                if (IsSymbolicLink (Volume, FullName, DirEntry)) {
                    //BREAD_CRUMB(L"%s:  2a 2a 5a 1a 1 - WHILE LOOP:- CONTINUE (Skipping This ... Symlink)", FuncTag);
                    //LOG_SEP(L"X");
                    // Skip This Entry
                    continue;
                }
            }

            //BREAD_CRUMB(L"%s:  2a 2a 6", FuncTag);
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_SET;
            #endif
            SkipDir = (
                FindSubStr (DirEntry->FileName, L"_BAK") ||
                MyStriCmp (DirEntry->FileName, L"APPLE")
            );
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_OFF;
            #endif

            // HasSignedCounterpart (Volume, FullName) = file with same name plus ".efi.signed" is present
            if (SkipDir                                                                   ||
                DirEntry->FileName[0] == '.'                                              ||
                MyStriCmp (Extension, L".log")                                            ||
                MyStriCmp (Extension, L".txt")                                            ||
                MyStriCmp (Extension, L".png")                                            ||
                MyStriCmp (Extension, L".bmp")                                            ||
                MyStriCmp (Extension, L".jpg")                                            ||
                MyStriCmp (Extension, L".jpeg")                                           ||
                MyStriCmp (Extension, L".icns")                                           ||
                (
                    MyStriCmp (DirEntry->FileName, FALLBACK_BASENAME) &&
                    MyStriCmp (Path, L"EFI\\BOOT")
                )                                                                         ||
                FilenameIn (Volume, Path, DirEntry->FileName, SHELL_NAMES)                ||
                HasSignedCounterpart (Volume, FullName)                                   ||
                FilenameIn (Volume, Path, DirEntry->FileName, GlobalConfig.DontScanFiles) ||
                !IsValidLoader (Volume->RootDir, FullName)
            ) {
                //BREAD_CRUMB(L"%s:  2a 2a 6a 1 - WHILE LOOP:- CONTINUE (Skipping This ... Invalid Item:- '%s')", FuncTag,
                //    DirEntry->FileName
                //);
                //LOG_SEP(L"X");
                // Skip This Entry
                MY_FREE_POOL(Extension);

                continue;
            }

            //BREAD_CRUMB(L"%s:  2a 2a 7", FuncTag);
            NewLoader = AllocateZeroPool (sizeof (struct LOADER_LIST));
            //BREAD_CRUMB(L"%s:  2a 2a 8", FuncTag);
            if (NewLoader != NULL) {
                //BREAD_CRUMB(L"%s:  2a 2a 8a 1", FuncTag);
                NewLoader->FileName  = StrDuplicate (FullName);
                NewLoader->TimeStamp = DirEntry->ModificationTime;
                LoaderList           = AddLoaderListEntry (LoaderList, NewLoader);

                if (DuplicatesFallback (Volume, FullName)) {
                    //BREAD_CRUMB(L"%s:  2a 2a 8a 1a 1", FuncTag);
                    FoundFallbackDuplicate = TRUE;
                }
            }

            //BREAD_CRUMB(L"%s:  2a 2a 8", FuncTag);
            MY_FREE_POOL(Extension);
            MY_FREE_POOL(FullName);
            MY_FREE_POOL(DirEntry);

            //BREAD_CRUMB(L"%s:  2a 2a 9 - WHILE LOOP:- END", FuncTag);
            //LOG_SEP(L"X");
        } // while

        //BREAD_CRUMB(L"%s:  2a 3", FuncTag);
        if (LoaderList != NULL) {
            IsLinux     = FALSE;
            NewLoader   = LoaderList;
            FirstKernel = NULL;

            //LOG_SEP(L"X");
            //BREAD_CRUMB(L"%s:  2a 3a 1", FuncTag);
            while (NewLoader != NULL) {

                //BREAD_CRUMB(L"%s:  2a 3a 1a 1 - WHILE LOOP:- START", FuncTag);
                IsLinux = IsInSubstring (NewLoader->FileName, GlobalConfig.LinuxPrefixes);

                //BREAD_CRUMB(L"%s:  2a 3a 1a 2", FuncTag);
                if ((FirstKernel != NULL) && IsLinux && GlobalConfig.FoldLinuxKernels) {
                    //BREAD_CRUMB(L"%s:  2a 3a 1a 2a 1", FuncTag);
                    AddKernelToSubmenu (FirstKernel, NewLoader->FileName, Volume);
                }
                else {
                    //BREAD_CRUMB(L"%s:  2a 3a 1a 2b 1", FuncTag);
                    LatestEntry = AddLoaderEntry (
                        NewLoader->FileName,
                        NULL, Volume,
                        !(IsLinux && GlobalConfig.FoldLinuxKernels)
                    );
                    //BREAD_CRUMB(L"%s:  2a 3a 1a 2b 2", FuncTag);
                    if (IsLinux && (FirstKernel == NULL)) {
                        //BREAD_CRUMB(L"%s:  2a 3a 1a 2b 2a 1", FuncTag);
                        FirstKernel = LatestEntry;
                    }
                }
                //BREAD_CRUMB(L"%s:  2a 3a 1a 3", FuncTag);
                NewLoader = NewLoader->NextEntry;

                //BREAD_CRUMB(L"%s:  2a 3a 1a 4 - WHILE LOOP:- END", FuncTag);
                //LOG_SEP(L"X");
            } // while

            //BREAD_CRUMB(L"%s:  2a 3a 2", FuncTag);
            if (FirstKernel != NULL && IsLinux && GlobalConfig.FoldLinuxKernels) {
                //BREAD_CRUMB(L"%s:  2a 3a 2a 1", FuncTag);
                #if REFIT_DEBUG > 0
                ALT_LOG(1, LOG_LINE_NORMAL, L"Adding 'Return' Entry to Folded Linux Kernels");
                #endif

                RetVal = GetReturnMenuEntry (&FirstKernel->me.SubScreen);
                //BREAD_CRUMB(L"%s:  2a 3a 2a 2", FuncTag);
                if (!RetVal) {
                    //BREAD_CRUMB(L"%s:  2a 3a 2a 2a 1", FuncTag);
                    FreeMenuScreen (&FirstKernel->me.SubScreen);
                }
            }

            //BREAD_CRUMB(L"%s:  2a 3a 3", FuncTag);
            CleanUpLoaderList (LoaderList);
        } // if LoaderList != NULL

        //BREAD_CRUMB(L"%s:  2a 4", FuncTag);
        Status = DirIterClose (&DirIter);
        // NOTE: EFI_INVALID_PARAMETER really is an error that should be reported;
        // but reports have been received from users that get this error occasionally
        // but nothing wrong has been found or the problem reproduced. It is therefore
        // being put down to buggy EFI implementations and that particular error igored.
        //BREAD_CRUMB(L"%s:  2a 5", FuncTag);
        if ((Status != EFI_NOT_FOUND) && (Status != EFI_INVALID_PARAMETER)) {
            //BREAD_CRUMB(L"%s:  2a 5a 1", FuncTag);
            if (Path) {
                //BREAD_CRUMB(L"%s:  2a 5a 1a 1", FuncTag);
                Message = PoolPrint (
                    L"While Scanning the '%s' Directory on '%s'",
                    Path, Volume->VolName
                );
            }
            else {
                //BREAD_CRUMB(L"%s:  2a 5a 1b 1", FuncTag);
                Message = PoolPrint (
                    L"While Scanning the Root Directory on '%s'",
                    Volume->VolName
                );
            }

            //BREAD_CRUMB(L"%s:  2a 5a 2", FuncTag);
            CheckError (Status, Message);

            //BREAD_CRUMB(L"%s:  2a 5a 3", FuncTag);
            MY_FREE_POOL(Message);
        }
        //BREAD_CRUMB(L"%s:  2a 6", FuncTag);
    } // if !SelfDirPath

    BREAD_CRUMB(L"%s:  3 - END:- return BOOLEAN FoundFallbackDuplicate = '%s'", FuncTag,
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

    boot_info = NULL;
    boot_info_size = 0;
    REFIT_CALL_3_WRAPPER(
        gBS->StartImage, iPXEHandle,
        &boot_info_size, &boot_info
    );

    return boot_info;
} // RuniPXEDiscover()

// Scan for network (PXE) boot servers. This function relies on the presence
// of the IPXE_DISCOVER_NAME and IPXE_NAME program files on the volume from
// which RefindPlus launched. As of December 6, 2014, these tools are not
// entirely reliable. See BUILDING.txt for information on building them.
static
VOID ScanNetboot (VOID) {
    CHAR16        *iPXEFileName;
    CHAR16        *Location;
    REFIT_VOLUME  *NetVolume;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Scanning for iPXE boot options");
    #endif

    if (FileExists (SelfVolume->RootDir, IPXE_NAME) &&
        FileExists (SelfVolume->RootDir, IPXE_DISCOVER_NAME) &&
        IsValidLoader (SelfVolume->RootDir, IPXE_DISCOVER_NAME) &&
        IsValidLoader (SelfVolume->RootDir, IPXE_NAME)
    ) {
        iPXEFileName = IPXE_NAME;
        Location = RuniPXEDiscover (SelfVolume->DeviceHandle);
        if (Location != NULL && FileExists (SelfVolume->RootDir, iPXEFileName)) {
            NetVolume = CopyVolume (SelfVolume);
            if (NetVolume) {
                NetVolume->DiskKind = DISK_KIND_NET;

                MY_FREE_IMAGE(NetVolume->VolBadgeImage);
                NetVolume->VolBadgeImage = BuiltinIcon (BUILTIN_ICON_VOL_NET);

                MY_FREE_POOL(NetVolume->PartName);
                MY_FREE_POOL(NetVolume->VolName);
                MY_FREE_POOL(NetVolume->FsName);

                AddLoaderEntry (iPXEFileName, Location, NetVolume, TRUE);

                FreeVolume (&NetVolume);
            }
        }
    }
} // VOID ScanNetboot()


// Adds *FullFileName as a MacOS loader, if it exists.
// Returns TRUE if the fallback loader is NOT a duplicate of this one,
// FALSE if it IS a duplicate.
static
BOOLEAN ScanMacOsLoader (
    REFIT_VOLUME *Volume,
    CHAR16       *FullFileName
) {
    UINTN     i;
    CHAR16   *VolName;
    CHAR16   *PathName;
    CHAR16   *FileName;
    BOOLEAN   AddThisEntry;
    BOOLEAN   ScanFallbackLoader;

    ScanFallbackLoader = TRUE;
    VolName = PathName = FileName = NULL;

    SplitPathName (FullFileName, &VolName, &PathName, &FileName);
    if (FileExists (Volume->RootDir, FullFileName) &&
        !FilenameIn (Volume, PathName, L"boot.efi", GlobalConfig.DontScanFiles)
    ) {
        if (FileExists (Volume->RootDir, L"EFI\\refind\\config.conf") ||
            FileExists (Volume->RootDir, L"EFI\\refind\\refind.conf")
        ) {
            AddLoaderEntry (FullFileName, L"Instance: RefindPlus", Volume, TRUE);
        }
        else {
            AddThisEntry = HasMacOS = TRUE;
            if (GlobalConfig.SyncAPFS) {
                for (i = 0; i < SystemVolumesCount; i++) {
                    if (GuidsAreEqual (&(SystemVolumes[i]->VolUuid), &(Volume->VolUuid))) {
                        AddThisEntry = FALSE;
                        break;
                    }
                }
            }

            if (AddThisEntry) {
                AddLoaderEntry (FullFileName, L"Instance: MacOS", Volume, TRUE);
            }
        }

        if (DuplicatesFallback (Volume, FullFileName)) {
            ScanFallbackLoader = FALSE;
        }
    }

    MY_FREE_POOL(VolName);
    MY_FREE_POOL(PathName);
    MY_FREE_POOL(FileName);

    return ScanFallbackLoader;
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
    CHAR16                 *MatchPatterns;
    BOOLEAN                 ScanFallbackLoader;
    BOOLEAN                 FoundBRBackup;
    BOOLEAN                 SkipDir;
    EFI_FILE_INFO          *EfiDirEntry;
    REFIT_DIR_ITER          EfiDirIter;
    APPLE_APFS_VOLUME_ROLE  VolumeRole;


    #if REFIT_DEBUG > 0
    UINTN    LogLineType;
    BOOLEAN  CheckMute = FALSE;
    #endif

    //#if REFIT_DEBUG > 1
    //CHAR16 *FuncTag = L"ScanEfiFiles";
    //#endif

    //LOG_SEP(L"X");
    //LOG_INCREMENT();
    //BREAD_CRUMB(L"%s:  1 - START", FuncTag);

    if (!Volume ||
        !Volume->RootDir ||
        !Volume->VolName ||
        !Volume->IsReadable
    ) {
        //BREAD_CRUMB(L"%s:  1a 1 - END:- VOID ... Exit on Invalid Volume", FuncTag);
        //LOG_DECREMENT();
        //LOG_SEP(L"X");

        // Early Return on Invalid Volume
        return;
    }

    //BREAD_CRUMB(L"%s:  2", FuncTag);
    if (Volume->FSType == FS_TYPE_NTFS) {
        //BREAD_CRUMB(L"%s:  2a 1", FuncTag);
        if (MyStriCmp (Volume->VolName, L"System Reserved") ||
            MyStriCmp (Volume->VolName, L"Basic Data Partition") ||
            MyStriCmp (Volume->VolName, L"Microsoft Reserved Partition")
        ) {
            //BREAD_CRUMB(L"%s:  2a 1a 1 - END:- VOID ... Exit on Windows Support Volume", FuncTag);
            //LOG_DECREMENT();
            //LOG_SEP(L"X");

            // Early Return on Windows Support Volume
            return;
        }
        //BREAD_CRUMB(L"%s:  2a 2", FuncTag);
        if (AppleFirmware && MyStrStr (Volume->VolName, L"BOOTCAMP")) {
            //BREAD_CRUMB(L"%s:  2a 2a 1 - END:- VOID ... Exit on Windows BootCamp Volume", FuncTag);
            //LOG_DECREMENT();
            //LOG_SEP(L"X");

            // Early Return on Windows BootCamp Volume
            return;
        }
    }

    //BREAD_CRUMB(L"%s:  3", FuncTag);
    if (Volume->FSType == FS_TYPE_APFS) {
        //BREAD_CRUMB(L"%s:  3a 1", FuncTag);
        VolumeRole = 0;

        // DA-TAG: Limit to TianoCore
        #ifndef __MAKEWITH_TIANO
        Status = EFI_NOT_STARTED;
        #else
        Status = RP_GetApfsVolumeInfo (
            Volume->DeviceHandle,
            NULL, NULL,
            &VolumeRole
        );
        #endif
        if (EFI_ERROR(Status) && Status != EFI_NOT_STARTED) {
            //BREAD_CRUMB(L"%s:  3a 1 X - END:- VOID ... Exit on Invalid APFS Volume", FuncTag);
            //LOG_DECREMENT();
            //LOG_SEP(L"X");

            // Early Return on Invalid APFS Volume
            return;
        }

        //BREAD_CRUMB(L"%s:  3a 2", FuncTag);
        if (Status != EFI_NOT_STARTED) {
            //BREAD_CRUMB(L"%s:  3a 2a 1", FuncTag);
            if (VolumeRole != APPLE_APFS_VOLUME_ROLE_SYSTEM  &&
                VolumeRole != APPLE_APFS_VOLUME_ROLE_PREBOOT &&
                VolumeRole != APPLE_APFS_VOLUME_ROLE_UNDEFINED
            ) {
                //BREAD_CRUMB(L"%s:  3a 2a 1a 1 - END:- VOID ... Exit on APFS Support Volume", FuncTag);
                //LOG_DECREMENT();
                //LOG_SEP(L"X");

                // Early Return on APFS Support Volume
                return;
            }
            //BREAD_CRUMB(L"%s:  3a 2a 2", FuncTag);
        }

        //BREAD_CRUMB(L"%s:  3a 3", FuncTag);
        if (GlobalConfig.SyncAPFS) {
            //BREAD_CRUMB(L"%s:  3a 3a 1", FuncTag);
            if (!EFI_ERROR(Status) && SingleAPFS &&
                (
                    VolumeRole == APPLE_APFS_VOLUME_ROLE_SYSTEM  ||
                    VolumeRole == APPLE_APFS_VOLUME_ROLE_PREBOOT ||
                    VolumeRole == APPLE_APFS_VOLUME_ROLE_UNDEFINED
                )
            ) {
                //BREAD_CRUMB(L"%s:  3a 3a 1a 1", FuncTag);
                for (i = 0; i < SkipApfsVolumesCount; i++) {
                    //LOG_SEP(L"X");
                    //BREAD_CRUMB(L"%s:  3a 3a 1a 1a 1 - FOR LOOP:- START", FuncTag);
                    if (GuidsAreEqual (&(SkipApfsVolumes[i]->PartGuid), &(Volume->PartGuid))) {
                        //BREAD_CRUMB(L"%s:  3a 3a 1a 1a 1a 1 - FOR LOOP:- END VOID ... Exit on Skipped APFS Volume", FuncTag);
                        //LOG_DECREMENT();
                        //LOG_SEP(L"X");

                        // Early Return on Skipped APFS Volume
                        return;
                    }
                    //BREAD_CRUMB(L"%s:  3a 3a 1a 1a 2 - FOR LOOP:- END", FuncTag);
                    //LOG_SEP(L"X");
                } // for
                //BREAD_CRUMB(L"%s:  3a 3a 1a 2", FuncTag);
            }

            //BREAD_CRUMB(L"%s:  3a 3a 2", FuncTag);
            for (i = 0; i < SystemVolumesCount; i++) {
                //LOG_SEP(L"X");
                //BREAD_CRUMB(L"%s:  3a 3a 2a 1 - FOR LOOP:- START", FuncTag);
                if (GuidsAreEqual (&(SystemVolumes[i]->VolUuid), &(Volume->VolUuid))) {
                    //BREAD_CRUMB(L"%s:  3a 3a 2a 1a 1 - FOR LOOP:- END VOID ... Exit on ReMapped APFS System Volume", FuncTag);
                    //LOG_DECREMENT();
                    //LOG_SEP(L"X");

                    // Early Return on ReMapped APFS System Volume
                    return;
                }
                //BREAD_CRUMB(L"%s:  3a 3a 2a 2 - FOR LOOP:- END", FuncTag);
                //LOG_SEP(L"X");
            } // for
            //BREAD_CRUMB(L"%s:  3a 3a 3", FuncTag);
        }
        //BREAD_CRUMB(L"%s:  3a 4", FuncTag);
    } // if Volume->FSType == FS_TYPE_APFS

    //BREAD_CRUMB(L"%s:  4", FuncTag);
    #if REFIT_DEBUG > 0
    /* Exception for LOG_THREE_STAR_SEP */
    LogLineType = (FirstLoaderScan) ? LOG_THREE_STAR_MID : LOG_THREE_STAR_SEP;
    ALT_LOG(1, LogLineType,
        L"Scanning Volume '%s' for UEFI Loaders",
        Volume->VolName ? Volume->VolName : L"** No Name **"
    );
    #endif

    FirstLoaderScan    = FALSE;
    ScanFallbackLoader =  TRUE;

    //BREAD_CRUMB(L"%s:  5", FuncTag);
    MatchPatterns = StrDuplicate (LOADER_MATCH_PATTERNS);
    if (GlobalConfig.ScanAllLinux) {
        //BREAD_CRUMB(L"%s:  5a 1", FuncTag);
        MergeStrings (&MatchPatterns, GlobalConfig.LinuxMatchPatterns, L',');
    }

    //BREAD_CRUMB(L"%s:  6", FuncTag);
    // Check for MacOS boot loader
    if (Volume->FSType != FS_TYPE_NTFS && ShouldScan (Volume, MACOSX_LOADER_DIR)) {
        //BREAD_CRUMB(L"%s:  6a 1", FuncTag);
        FileName = StrDuplicate (MACOSX_LOADER_PATH);

        //BREAD_CRUMB(L"%s:  6a 2", FuncTag);
        ScanFallbackLoader &= ScanMacOsLoader (Volume, FileName);
        MY_FREE_POOL(FileName);

        //BREAD_CRUMB(L"%s:  6a 3", FuncTag);
        DirIterOpen (Volume->RootDir, L"\\", &EfiDirIter);

        //BREAD_CRUMB(L"%s:  6a 4", FuncTag);
        while (DirIterNext (&EfiDirIter, 1, NULL, &EfiDirEntry)) {
            //LOG_SEP(L"X");
            //BREAD_CRUMB(L"%s:  6a 4a 1 - WHILE LOOP:- START", FuncTag);
            if (IsGuid (EfiDirEntry->FileName)) {
                //BREAD_CRUMB(L"%s:  6a 4a 1a 1", FuncTag);
                FileName = PoolPrint (L"%s\\%s", EfiDirEntry->FileName, MACOSX_LOADER_PATH);

                //BREAD_CRUMB(L"%s:  6a 4a 1a 2", FuncTag);
                ScanFallbackLoader &= ScanMacOsLoader (Volume, FileName);

                //BREAD_CRUMB(L"%s:  6a 4a 1a 3", FuncTag);
                MY_FREE_POOL(FileName);
                FileName = PoolPrint (L"%s\\%s", EfiDirEntry->FileName, L"boot.efi");

                //BREAD_CRUMB(L"%s:  6a 4a 1a 4", FuncTag);
                if (Volume->FSType != FS_TYPE_APFS) {
                    //BREAD_CRUMB(L"%s:  6a 4a 1a 4a 1", FuncTag);
                    if (!StriSubCmp (FileName, GlobalConfig.MacOSRecoveryFiles)) {
                        //BREAD_CRUMB(L"%s:  6a 4a 1a 4a 1a 1", FuncTag);
                        MergeStrings (&GlobalConfig.MacOSRecoveryFiles, FileName, L',');
                    }
                    //BREAD_CRUMB(L"%s:  6a 4a 1a 4a 2", FuncTag);
                }
                MY_FREE_POOL(FileName);
                //BREAD_CRUMB(L"%s:  6a 4a 1a 5", FuncTag);
            }

            //BREAD_CRUMB(L"%s:  6a 4a 2", FuncTag);
            MY_FREE_POOL(EfiDirEntry);

            //BREAD_CRUMB(L"%s:  6a 4a 3 - WHILE LOOP:- END", FuncTag);
            //LOG_SEP(L"X");
        } // while

        //BREAD_CRUMB(L"%s:  6a 5", FuncTag);
        DirIterClose (&EfiDirIter);

        // check for XOM
        //BREAD_CRUMB(L"%s:  6a 6", FuncTag);
        FileName = StrDuplicate (L"System\\Library\\CoreServices\\xom.efi");

        //BREAD_CRUMB(L"%s:  6a 7", FuncTag);
        if (FileExists (Volume->RootDir, FileName) &&
            !FilenameIn (Volume, MACOSX_LOADER_DIR, L"xom.efi", GlobalConfig.DontScanFiles)
        ) {
            //BREAD_CRUMB(L"%s:  6a 7a 1", FuncTag);
            AddLoaderEntry (FileName, L"Instance: Windows XP (XoM)", Volume, TRUE);

            //BREAD_CRUMB(L"%s:  6a 7a 2", FuncTag);
            if (DuplicatesFallback (Volume, FileName)) {
                //BREAD_CRUMB(L"%s:  6a 7a 2a 1", FuncTag);
                ScanFallbackLoader = FALSE;
            }
            //BREAD_CRUMB(L"%s:  6a 7a 3", FuncTag);
        }
        MY_FREE_POOL(FileName);

        //BREAD_CRUMB(L"%s:  6a 8", FuncTag);
    } // if ShouldScan MACOSX_LOADER_DIR

    //BREAD_CRUMB(L"%s:  7", FuncTag);
    // check for Microsoft boot loader/menu
    if (ShouldScan (Volume, L"EFI\\Microsoft\\Boot")) {
        FoundBRBackup = FALSE;

        //BREAD_CRUMB(L"%s:  7a 1", FuncTag);
        FileName = StrDuplicate (L"EFI\\Microsoft\\Boot\\bkpbootmgfw.efi");

        //BREAD_CRUMB(L"%s:  7a 2", FuncTag);
        if (FileExists (Volume->RootDir, FileName) &&
            !FilenameIn (
                Volume,
                L"EFI\\Microsoft\\Boot",
                L"bkpbootmgfw.efi",
                GlobalConfig.DontScanFiles
            )
        ) {
            //BREAD_CRUMB(L"%s:  7a 2a 1", FuncTag);
            // Boot Repair Backup
            AddLoaderEntry (FileName, L"Instance: UEFI Windows (BRBackup)", Volume, TRUE);

            //BREAD_CRUMB(L"%s:  7a 2a 2", FuncTag);
            FoundBRBackup = TRUE;
            if (DuplicatesFallback (Volume, FileName)) {
                //BREAD_CRUMB(L"%s:  7a 2a 2a 1", FuncTag);
                ScanFallbackLoader = FALSE;
            }
            //BREAD_CRUMB(L"%s:  7a 2a 3", FuncTag);
        }
        MY_FREE_POOL(FileName);

        //BREAD_CRUMB(L"%s:  7a 3", FuncTag);
        FileName = StrDuplicate (L"EFI\\Microsoft\\Boot\\bootmgfw.efi");
        if (FileExists (Volume->RootDir, FileName) &&
            !FilenameIn (
                Volume,
                L"EFI\\Microsoft\\Boot",
                L"bootmgfw.efi",
                GlobalConfig.DontScanFiles
            )
        ) {
            TmpMsg = (FoundBRBackup)
                ? L"Instance: Assumed UEFI Windows (Potentially GRUB)"
                : L"Instance: Windows (UEFI)";
            AddLoaderEntry (FileName, TmpMsg, Volume, TRUE);

            if (DuplicatesFallback (Volume, FileName)) {
                ScanFallbackLoader = FALSE;
            }
        }
        MY_FREE_POOL(FileName);

        //BREAD_CRUMB(L"%s:  7a 4", FuncTag);
    } // if ShouldScan

    //BREAD_CRUMB(L"%s:  8", FuncTag);
    // Scan the root directory for EFI executables
    if (ScanLoaderDir (Volume, L"\\", MatchPatterns)) {
        //BREAD_CRUMB(L"%s:  8a 1", FuncTag);
        ScanFallbackLoader = FALSE;
    }

    // Scan subdirectories of the EFI directory (as per the standard)
    //BREAD_CRUMB(L"%s:  9", FuncTag);
    DirIterOpen (Volume->RootDir, L"EFI", &EfiDirIter);
    //BREAD_CRUMB(L"%s:  9a 1", FuncTag);
    while (DirIterNext (&EfiDirIter, 1, NULL, &EfiDirEntry)) {
        Extension = FindExtension (EfiDirEntry->FileName);

        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif
        SkipDir = (
            FindSubStr (EfiDirEntry->FileName, L"_BAK") ||
            MyStriCmp (EfiDirEntry->FileName, L"APPLE")
        );
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif

        //LOG_SEP(L"X");
        //BREAD_CRUMB(L"%s:  9a 1a 1 - WHILE LOOP:- START", FuncTag);
        if (SkipDir                                  ||
            EfiDirEntry->FileName[0] == '.'          ||
            MyStriCmp (Extension, L".log")           ||
            MyStriCmp (Extension, L".txt")           ||
            MyStriCmp (Extension, L".png")           ||
            MyStriCmp (Extension, L".bmp")           ||
            MyStriCmp (Extension, L".jpg")           ||
            MyStriCmp (Extension, L".jpeg")          ||
            MyStriCmp (Extension, L".icns")          ||
            MyStriCmp (EfiDirEntry->FileName, L"tools")
        ) {
            //BREAD_CRUMB(L"%s:  9a 1a 1a 1 - WHILE LOOP:- CONTINUE (Skipping This ... Invalid Item:- '%s')", FuncTag,
            //    EfiDirEntry->FileName
            //);
            //LOG_SEP(L"X");

            // Skip this ... Does not contain boot loaders or is scanned later
            MY_FREE_POOL(Extension);
            MY_FREE_POOL(EfiDirEntry);

            continue;
        }

        //BREAD_CRUMB(L"%s:  9a 1a 2", FuncTag);
        FileName = PoolPrint (L"EFI\\%s", EfiDirEntry->FileName);

        //BREAD_CRUMB(L"%s:  9a 1a 3", FuncTag);
        if (ScanLoaderDir (Volume, FileName, MatchPatterns)) {
            //BREAD_CRUMB(L"%s:  9a 1a 3a 1", FuncTag);
            ScanFallbackLoader = FALSE;
        }

        MY_FREE_POOL(FileName);
        MY_FREE_POOL(Extension);
        MY_FREE_POOL(EfiDirEntry);

        //BREAD_CRUMB(L"%s:  9a 1a 4 - WHILE LOOP:- END", FuncTag);
        //LOG_SEP(L"X");
    } // while

    //BREAD_CRUMB(L"%s:  9a 2", FuncTag);
    Status = DirIterClose (&EfiDirIter);
    //BREAD_CRUMB(L"%s:  9a 3", FuncTag);
    if (Status != EFI_NOT_FOUND && Status != EFI_INVALID_PARAMETER) {
        //BREAD_CRUMB(L"%s:  9a 3a 1", FuncTag);
        Temp = PoolPrint (
            L"While Scanning the EFI System Partition on '%s'",
            Volume->VolName
        );
        //BREAD_CRUMB(L"%s:  9a 3a 2", FuncTag);
        CheckError (Status, Temp);
        MY_FREE_POOL(Temp);
        //BREAD_CRUMB(L"%s:  9a 3a 3", FuncTag);
    }

    //BREAD_CRUMB(L"%s:  9a 4", FuncTag);
    // Scan user-specified (or additional default) directories.
    i = 0;
    VolName = Directory = NULL;
    while ((Directory = FindCommaDelimited (GlobalConfig.AlsoScan, i++)) != NULL) {
        //LOG_SEP(L"X");
        //BREAD_CRUMB(L"%s:  9a 4a 1 - WHILE LOOP:- START", FuncTag);
        if (ShouldScan (Volume, Directory)) {
            //BREAD_CRUMB(L"%s:  9a 4a 1a 1", FuncTag);
            SplitVolumeAndFilename (&Directory, &VolName);

            //BREAD_CRUMB(L"%s:  9a 4a 1a 2", FuncTag);
            CleanUpPathNameSlashes (Directory);

            //BREAD_CRUMB(L"%s:  9a 4a 1a 3", FuncTag);
            Length = StrLen (Directory);

            //BREAD_CRUMB(L"%s:  9a 4a 1a 4", FuncTag);
            if ((Length > 0) && ScanLoaderDir (Volume, Directory, MatchPatterns)) {
                //BREAD_CRUMB(L"%s:  9a 4a 1a 4a 1", FuncTag);
                ScanFallbackLoader = FALSE;
            }
            //BREAD_CRUMB(L"%s:  9a 4a 1a 5", FuncTag);
            MY_FREE_POOL(VolName);
        }
        MY_FREE_POOL(Directory);

        //BREAD_CRUMB(L"%s:  9a 4a 2 - WHILE LOOP:- END", FuncTag);
        //LOG_SEP(L"X");
    } // while

    //BREAD_CRUMB(L"%s:  9a 5", FuncTag);
    // Do not scan the fallback loader if it is on the same volume and a duplicate of RefindPlus itself.
    SelfPath = DevicePathToStr (SelfLoadedImage->FilePath);

    //BREAD_CRUMB(L"%s:  9a 6", FuncTag);
    CleanUpPathNameSlashes (SelfPath);

    //BREAD_CRUMB(L"%s:  9a 7", FuncTag);
    if ((Volume->DeviceHandle == SelfLoadedImage->DeviceHandle) &&
        DuplicatesFallback (Volume, SelfPath)
    ) {
        //BREAD_CRUMB(L"%s:  9a 7a 1", FuncTag);
        ScanFallbackLoader = FALSE;
    }
    MY_FREE_POOL(SelfPath);
    //BREAD_CRUMB(L"%s:  9a 8", FuncTag);

    //BREAD_CRUMB(L"%s:  10", FuncTag);
    // Create an entry for fallback loaders
    if (ScanFallbackLoader &&
        FileExists (Volume->RootDir, FALLBACK_FULLNAME) &&
        ShouldScan (Volume, L"EFI\\BOOT") &&
        !FilenameIn (Volume, L"EFI\\BOOT", FALLBACK_BASENAME, GlobalConfig.DontScanFiles)
    ) {
        //BREAD_CRUMB(L"%s:  10a 1", FuncTag);
        Temp = StrDuplicate (FALLBACK_FULLNAME);
        AddLoaderEntry (Temp, L"Fallback Loader", Volume, TRUE);
        MY_FREE_POOL(Temp);
    }

    MY_FREE_POOL(MatchPatterns);

    //BREAD_CRUMB(L"%s:  11 - END:- VOID", FuncTag);
    //LOG_DECREMENT();
    //LOG_SEP(L"X");
} // static VOID ScanEfiFiles()

// Scan internal disks for valid EFI boot loaders.
static
VOID ScanInternal (VOID) {
    UINTN VolumeIndex;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Scan for Internal Disk Volumes with Mode:- 'UEFI'");
    #endif

    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"ScanInternal";
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  A - START", FuncTag);

    FirstLoaderScan = TRUE;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_INTERNAL) {
            ScanEfiFiles (Volumes[VolumeIndex]);
        }
    } // for

    FirstLoaderScan = FALSE;

    if (!HasMacOS) {
        // Disable DynamicCSR ... Mac OS not detected
        GlobalConfig.DynamicCSR = 0;
    }

    BREAD_CRUMB(L"%s:  Z - END:- VOID", FuncTag);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // static VOID ScanInternal()

// Scan external disks for valid EFI boot loaders.
static
VOID ScanExternal (VOID) {
    UINTN VolumeIndex;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Scan for External Disk Volumes with Mode:- 'UEFI'");
    #endif

    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"ScanExternal";
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  A - START", FuncTag);

    FirstLoaderScan = TRUE;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_EXTERNAL) {
            ScanEfiFiles (Volumes[VolumeIndex]);
        }
    } // for

    FirstLoaderScan = FALSE;

    BREAD_CRUMB(L"%s:  Z - END:- VOID", FuncTag);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // static VOID ScanExternal()

// Scan internal disks for valid EFI boot loaders.
static
VOID ScanOptical (VOID) {
    UINTN VolumeIndex;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Scan for Optical Discs with Mode:- 'UEFI'");
    #endif

    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"ScanOptical";
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  A - START", FuncTag);

    FirstLoaderScan = TRUE;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_OPTICAL) {
            ScanEfiFiles (Volumes[VolumeIndex]);
        }
    } // for
    FirstLoaderScan = FALSE;

    BREAD_CRUMB(L"%s:  Z - END:- VOID", FuncTag);
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
    IN CHAR16   *MatchThis,
    IN EG_IMAGE *Icon
) {
    UINTN            i;
    CHAR16          *OneElement;
    CHAR16          *DontScanFirmware;
    BOOLEAN          ScanIt;
    BOOLEAN          FoundItem;
    BOOT_ENTRY_LIST *BootEntries;
    BOOT_ENTRY_LIST *CurrentEntry;

    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"ScanFirmwareDefined";
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  A - START", FuncTag);

    #if REFIT_DEBUG > 0
    if (Row == 0) {
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Scanning for Firmware Defined Boot Options"
        );
    }
    #endif

    DontScanFirmware = ReadHiddenTags (L"HiddenFirmware");

    #if REFIT_DEBUG > 0
    if (GlobalConfig.DontScanFirmware != NULL) {
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"GlobalConfig.DontScanFirmware:- '%s'",
            GlobalConfig.DontScanFirmware
        );
    }
    if (DontScanFirmware != NULL) {
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Firmware Hidden Tags:- '%s'",
            DontScanFirmware
        );
    }
    #endif

    if (GlobalConfig.DontScanFirmware && *GlobalConfig.DontScanFirmware) {
        // Only merge GlobalConfig.DontScanFirmware if not empty
        MergeStrings (&DontScanFirmware, GlobalConfig.DontScanFirmware, L',');
    }

    if (Row == 0) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID, L"NB: Excluding UEFI Shell from Scan");
        #endif

        MergeStrings(&DontScanFirmware, L"shell", L',');
    }

    #if REFIT_DEBUG > 0
    if (DontScanFirmware != NULL) {
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Merged Firmware Scan Exclusion List:- '%s'",
            DontScanFirmware
        );
    }
    #endif

    BootEntries  = FindBootOrderEntries();
    CurrentEntry = BootEntries;
    FoundItem    = FALSE;
    ScanIt       = TRUE;

    while (CurrentEntry != NULL) {
        if (MatchThis) {
            OneElement  =  NULL;
            ScanIt      = FALSE;
            i = 0;
            while (!ScanIt && (OneElement = FindCommaDelimited (MatchThis, i++))) {
                if (StriSubCmp (OneElement, CurrentEntry->BootEntry.Label) &&
                    !IsInSubstring (CurrentEntry->BootEntry.Label, DontScanFirmware)
                ) {
                    ScanIt = TRUE;
                }
                MY_FREE_POOL(OneElement);
            } // while
        }
        else {
            if (IsInSubstring (CurrentEntry->BootEntry.Label, DontScanFirmware)) {
                ScanIt = FALSE;
            }
        }

        if (ScanIt) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"Adding UEFI Loader Entry for '%s'",
                CurrentEntry->BootEntry.Label
            );
            #endif

            FoundItem = TRUE;
            AddEfiLoaderEntry (
                CurrentEntry->BootEntry.DevPath,
                CurrentEntry->BootEntry.Label,
                CurrentEntry->BootEntry.BootNum,
                Row, Icon
            );
        }

        CurrentEntry = CurrentEntry->NextBootEntry;
    } // while

    MY_FREE_POOL(DontScanFirmware);
    DeleteBootOrderEntries (BootEntries);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Evaluate Firmware Defined Options ... Relevant Options:- '%s'",
        (FoundItem) ? L"Added" : L"None"
    );
    #endif

    BREAD_CRUMB(L"%s:  Z - END:- VOID", FuncTag);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // static VOID ScanFirmwareDefined()

// default volume badge icon based on disk kind
EG_IMAGE * GetDiskBadge (
    IN UINTN DiskType
) {
    EG_IMAGE *Badge;

    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_BADGES) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"Skipped ... Config Setting is Active:- 'HideUI Badges'"
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
    Entry->me.Title          = (LoaderTitle) ? StrDuplicate (LoaderTitle) : StrDuplicate (L"Unknown Tool");
    Entry->me.Tag            = TAG_TOOL;
    Entry->me.Row            = 1;
    Entry->me.ShortcutLetter = ShortcutLetter;
    Entry->me.Image          = egCopyImage (Image);
    Entry->LoaderPath        = (LoaderPath) ? StrDuplicate (LoaderPath) : NULL;
    Entry->Volume            = CopyVolume (Volume);
    Entry->UseGraphicsMode   = UseGraphicsMode;

    AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *) Entry);

    return Entry;
} // static LOADER_ENTRY * AddToolEntry()

// Locates boot loaders.
// NOTE: This assumes that GlobalConfig.LegacyType is correctly set.
VOID ScanForBootloaders (VOID) {
    UINTN     i;
    UINTN     SetOptions;
    CHAR16    ShortCutKey;
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
    UINTN    KeyNum;
    CHAR16  *MsgStr;
    #endif

    ScanningLoaders = TRUE;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    MsgStr = StrDuplicate (L"S E E K   U E F I   L O A D E R S");
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
    LOG_MSG("%s", MsgStr);
    LOG_MSG("\n");
    MY_FREE_POOL(MsgStr);
    #endif

    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"ScanForBootloaders";
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  A - START", FuncTag);

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

    // If UEFI & scanning for legacy loaders & deep legacy scan, update NVRAM boot manager list
    if ((GlobalConfig.LegacyType == LEGACY_TYPE_UEFI) &&
        ScanForLegacy && GlobalConfig.DeepLegacyScan
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
        // We do *NOT* do this with the GlobalConfig.DontScanFirmware and
        // GlobalConfig.DontScanTools variables because they are used in only one function
        // each, so it is easier to create a temporary variable for the merged contents
        // there and not modify the global variable.
        OrigDontScanFiles   = StrDuplicate (GlobalConfig.DontScanFiles);
        OrigDontScanVolumes = StrDuplicate (GlobalConfig.DontScanVolumes);

        // Add hidden tags to two GlobalConfig.DontScan* variables.
        HiddenTags = ReadHiddenTags (L"HiddenTags");
        if ((HiddenTags) && (StrLen (HiddenTags) > 0)) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"Merging HiddenTags into 'Dont Scan Files':- '%s'",
                HiddenTags
            );
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            #endif

            MergeStrings (&GlobalConfig.DontScanFiles, HiddenTags, L',');
        }
        MY_FREE_POOL(HiddenTags);

        HiddenLegacy = ReadHiddenTags (L"HiddenLegacy");
        if ((HiddenLegacy) && (StrLen (HiddenLegacy) > 0)) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"Merging HiddenLegacy into 'Dont Scan Volumes':- '%s'",
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
        OrigDontScanDirs = StrDuplicate (GlobalConfig.DontScanDirs);

        if (GlobalConfig.DontScanFiles) {
            DontScanItem = NULL;
            while ((DontScanItem = FindCommaDelimited (GlobalConfig.DontScanFiles, i)) != NULL) {
                DeleteItem = (FindSubStr (DontScanItem, L"PreBoot:"))
                    ? TRUE
                    : (MyStriCmp (DontScanItem, L"PreBoot"))
                        ? TRUE
                        : FALSE;

                if (!DeleteItem) {
                    i++;
                }
                else {
                    DeleteItemFromCsvList (DontScanItem, GlobalConfig.DontScanFiles);
                    AmendedDontScan = TRUE;
                }

                MY_FREE_POOL(DontScanItem);
            } // while
        } // if GlobalConfig

        if (GlobalConfig.DontScanDirs) {
            while ((DontScanItem = FindCommaDelimited (GlobalConfig.DontScanDirs, i)) != NULL) {
                DeleteItem = (FindSubStr (DontScanItem, L"PreBoot:"))
                    ? TRUE
                    : (MyStriCmp (DontScanItem, L"PreBoot"))
                        ? TRUE : FALSE;

                if (!DeleteItem) {
                    i++;
                }
                else {
                    DeleteItemFromCsvList (DontScanItem, GlobalConfig.DontScanDirs);
                    AmendedDontScan = TRUE;
                }

                MY_FREE_POOL(DontScanItem);
            } // while
        } // if GlobalConfig

        if (GlobalConfig.DontScanVolumes) {
            while ((DontScanItem = FindCommaDelimited (GlobalConfig.DontScanVolumes, i)) != NULL) {
                DeleteItem = (FindSubStr (DontScanItem, L"PreBoot:"))
                    ? TRUE
                    : (MyStriCmp (DontScanItem, L"PreBoot"))
                        ? TRUE
                        : FALSE;

                if (!DeleteItem) {
                    i++;
                }
                else {
                    DeleteItemFromCsvList (DontScanItem, GlobalConfig.DontScanVolumes);
                    AmendedDontScan = TRUE;
                }

                MY_FREE_POOL(DontScanItem);
            } // while
        } // if GlobalConfig

        #if REFIT_DEBUG > 0
        if (AmendedDontScan) {
            ALT_LOG(1, LOG_STAR_SEPARATOR,
                L"Ignored PreBoot Volumes in 'Dont Scan' List ... SyncAPFS is Active"
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

    // scan for loaders and tools, add them to the menu
    for (i = 0; i <= SetOptions; i++) {
        switch (GlobalConfig.ScanFor[i]) {
            case 'm': case 'M':
                #if REFIT_DEBUG > 0
                if (LogNewLine) LOG_MSG("\n");
                LogNewLine = TRUE;

                LOG_MSG("Scan Manual:");
                ALT_LOG(1, LOG_LINE_THIN_SEP, L"Process User Configured Stanzas");
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

    if (GlobalConfig.SyncAPFS && OrigDontScanDirs && AmendedDontScan) {
        // Restore the stashed GlobalConfig.DontScanDirs variable
        MY_FREE_POOL(GlobalConfig.DontScanDirs);
        GlobalConfig.DontScanDirs = OrigDontScanDirs;
    }
    else {
        MY_FREE_POOL(OrigDontScanDirs);
    }

    if (MainMenu->EntryCount < 1) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"Could Not Find Boot Loaders");
        ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
        LOG_MSG("* WARN: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif
    }
    else {
        // Assign shortcut keys
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        CHAR16 *LogSection = L"A S S I G N   S H O R T C U T   K E Y S";
        ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", LogSection);
        LOG_MSG("\n\n");
        LOG_MSG("%s", LogSection);

        MsgStr = StrDuplicate (L"Set Shortcuts:");
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("\n");
        LOG_MSG("%s", MsgStr);
        MY_FREE_POOL(MsgStr);

        KeyNum = 0;
        #endif

        for (i = 0; i < MainMenu->EntryCount && MainMenu->Entries[i]->Row == 0; i++) {
            if (i < 9) {
                #if REFIT_DEBUG > 0
                KeyNum = i + 1;
                #endif

                ShortCutKey = (CHAR16) ('1' + i);
            }
            else if (i == 9) {
                ShortCutKey = (CHAR16) ('9' - i);

                #if REFIT_DEBUG > 0
                KeyNum = 0;
                #endif
            }
            else {
                break;
            }
            MainMenu->Entries[i]->ShortcutDigit = ShortCutKey;

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
            L"Assigned Shortcut Key%s to %d of %d Loader%s",
            (i == 1) ? L"" : L"s",
            i, MainMenu->EntryCount,
            (MainMenu->EntryCount == 1) ? L"" : L"s"
        );
        ALT_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);
        LOG_MSG("\n\n");
        LOG_MSG("INFO: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif
    }

    // Wait for user acknowledgement if there were errors
    FinishTextScreen (FALSE);

    ScanningLoaders = FALSE;

    BREAD_CRUMB(L"%s:  Z - END:- VOID", FuncTag);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID ScanForBootloaders()

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
    BOOLEAN  retval = TRUE;

    if (!FileExists (BaseVolume->RootDir, PathName)) {
        // Early return ... File does not exist
        return FALSE;
    }

    if (gHiddenTools == NULL) {
        DontScanTools = ReadHiddenTags (L"HiddenTools");
        gHiddenTools  = (DontScanTools) ? StrDuplicate (DontScanTools) : StrDuplicate (L"NotSet");

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        #endif
    }
    else if (!MyStriCmp (gHiddenTools, L"NotSet")) {
        DontScanTools = StrDuplicate (gHiddenTools);
    }

    if (!MyStriCmp (gHiddenTools, L"NotSet")) {
        MergeStrings (&DontScanTools, GlobalConfig.DontScanTools, L',');
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Check File is Valid:- '%s'",
        PathName
    );
    #endif

    if (!FileExists (BaseVolume->RootDir, PathName) ||
        !IsValidLoader (BaseVolume->RootDir, PathName)
    ) {
        MY_FREE_POOL(DontScanTools);

        return FALSE;
    }

    TestVolName = TestPathName = TestFileName = NULL;
    SplitPathName (PathName, &TestVolName, &TestPathName, &TestFileName);
    MY_FREE_POOL(TestVolName);

    i = 0;
    DontScanThis = NULL;
    DontVolName = DontPathName = DontFileName = NULL;
    while (retval && (DontScanThis = FindCommaDelimited (DontScanTools, i++))) {
        SplitPathName (DontScanThis, &DontVolName, &DontPathName, &DontFileName);

        if (MyStriCmp (TestFileName, DontFileName) &&
            ((DontPathName == NULL) || (MyStriCmp (TestPathName, DontPathName))) &&
            ((DontVolName == NULL) || (VolumeMatchesDescription (BaseVolume, DontVolName)))
        ) {
            retval = FALSE;
        }

        MY_FREE_POOL(TestPathName);
        MY_FREE_POOL(TestFileName);

        MY_FREE_POOL(DontVolName);
        MY_FREE_POOL(DontPathName);
        MY_FREE_POOL(DontFileName);

        MY_FREE_POOL(DontScanThis);
    } // while

    MY_FREE_POOL(DontScanTools);

    return retval;
} // BOOLEAN IsValidTool()

// Locate a single tool from the specified Locations using one of the
// specified Names and add it to the menu.
static
BOOLEAN FindTool (
    CHAR16 *Locations,
    CHAR16 *Names,
    CHAR16 *Description,
    UINTN   Icon
) {
    UINTN    j;
    UINTN    k;
    UINTN    VolumeIndex;
    CHAR16  *DirName;
    CHAR16  *FileName;
    CHAR16  *PathName;
    BOOLEAN  FoundTool;

    #if REFIT_DEBUG > 0
    CHAR16 *ToolStr;
    #endif

    FoundTool = FALSE;
    PathName = FileName = DirName = NULL;

    j = 0;
    while ((DirName = FindCommaDelimited (Locations, j++)) != NULL) {
        k = 0;
        while ((FileName = FindCommaDelimited (Names, k++)) != NULL) {
            PathName  = StrDuplicate (DirName);
            MergeStrings (&PathName, FileName, MyStriCmp (PathName, L"\\") ? 0 : L'\\');
            for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                if (Volumes[VolumeIndex]->RootDir != NULL &&
                    IsValidTool (Volumes[VolumeIndex], PathName)
                ) {
                    #if REFIT_DEBUG > 0
                    ALT_LOG(1, LOG_LINE_NORMAL,
                        L"Adding Tag for '%s' on '%s'",
                        FileName, Volumes[VolumeIndex]->VolName
                    );
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

                    AddToolEntry (
                        Volumes[VolumeIndex],
                        PathName, Description,
                        BuiltinIcon (Icon),
                        'S', FALSE
                    );
                    FoundTool = TRUE;
                } // if Volumes[VolumeIndex]->RootDir
            } // for

            MY_FREE_POOL(PathName);
            MY_FREE_POOL(FileName);
        } // while Names

        MY_FREE_POOL(DirName);
    } // while Locations

    return FoundTool;
} // VOID FindTool()

// Add the second-row tags containing built-in and external tools
VOID ScanForTools (VOID) {
    EFI_STATUS              Status;
    UINTN                   i, j, k;
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
    APPLE_APFS_VOLUME_ROLE  VolumeRole;

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

    #if REFIT_DEBUG > 0
    BOOLEAN   FlagAPFS;
    UINTN     ToolTotal;
    CHAR16   *ToolStr;
    CHAR16   *LogSection = L"S E E K   U E F I   T O O L S";

    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", LogSection);
    LOG_MSG("%s", LogSection);
    #endif

    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"ScanForTools";
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  A - START", FuncTag);

    #if REFIT_DEBUG > 0
    if (GlobalConfig.DirectBoot) {
        LogSection = L"Skipped Loading UEFI Tools ... 'DirectBoot' is Active";
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", LogSection);
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        LOG_MSG("\n");
        LOG_MSG("%s", LogSection);
        LOG_MSG("\n\n");
    }
    else {
        LogSection = L"Get UEFI Tool Types:";
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", LogSection);
        LOG_MSG("\n");
        LOG_MSG("%s", LogSection);
    }
    #endif

    BREAD_CRUMB(L"%s:  B", FuncTag);
    if (GlobalConfig.DirectBoot) {
        BREAD_CRUMB(L"%s:  Ba 1 - END:- VOID ... Exit on DirectBoot", FuncTag);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    MokLocations = StrDuplicate (MOK_LOCATIONS);
    if (MokLocations != NULL) {
        MergeStrings (&MokLocations, SelfDirPath, L',');
    }

    #if REFIT_DEBUG > 0
    ToolTotal = 0;
    #endif

    VolumeTag = FileName = ToolName = NULL;
    for (i = 0; i < NUM_TOOLS; i++) {
        // Reset Vars
        MY_FREE_POOL(FileName);
        MY_FREE_POOL(VolumeTag);

        switch (GlobalConfig.ShowTools[i]) {
            case TAG_ABOUT:            ToolName = L"About RefindPlus"   ;      break;
            case TAG_BOOTORDER:        ToolName = L"Boot Order"         ;      break;
            case TAG_CSR_ROTATE:       ToolName = L"Rotate CSR"         ;      break;
            case TAG_EXIT:             ToolName = L"Exit RefindPlus"    ;      break;
            case TAG_FIRMWARE:         ToolName = L"Firmware Reboot"    ;      break;
            case TAG_FWUPDATE_TOOL:    ToolName = L"Firmware Update"    ;      break;
            case TAG_GDISK:            ToolName = L"GDisk Tool"         ;      break;
            case TAG_GPTSYNC:          ToolName = L"Manage HybridMBR"   ;      break;
            case TAG_HIDDEN:           ToolName = L"Restore Tags"       ;      break;
            case TAG_INFO_NVRAMCLEAN:  ToolName = L"Clean Nvram"        ;      break;
            case TAG_INSTALL:          ToolName = L"Install RefindPlus" ;      break;
            case TAG_MEMTEST:          ToolName = L"Memtest Tool"       ;      break;
            case TAG_MOK_TOOL:         ToolName = L"MOK Protocol"       ;      break;
            case TAG_NETBOOT:          ToolName = L"Net Boot"           ;      break;
            case TAG_REBOOT:           ToolName = L"System Restart"     ;      break;
            case TAG_RECOVERY_APPLE:   ToolName = L"Recovery (Mac)"     ;      break;
            case TAG_RECOVERY_WINDOWS: ToolName = L"Recovery (Win)"     ;      break;
            case TAG_SHELL:            ToolName = L"UEFI Shell"         ;      break;
            case TAG_SHUTDOWN:         ToolName = L"System Shutdown"    ;      break;
            default:                                                        continue;
        } // switch

        #if REFIT_DEBUG > 0
        ToolTotal = ToolTotal + 1;
        LOG_MSG("%s  - Type %02d ... ", OffsetNext, ToolTotal);
        #endif

        FoundTool = FALSE;
        switch (GlobalConfig.ShowTools[i]) {
            case TAG_INFO_NVRAMCLEAN:
                MenuEntryPreCleanNvram = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
                if (MenuEntryPreCleanNvram) {
                    FoundTool = TRUE;

                    MenuEntryPreCleanNvram->Title          = StrDuplicate (ToolName);
                    MenuEntryPreCleanNvram->Tag            = TAG_INFO_NVRAMCLEAN;
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
                    ToolStr = PoolPrint (L"Could Not Load Tool:- '%s'", ToolName);
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
                    ToolStr = PoolPrint (L"Could Not Load Tool:- '%s'", ToolName);
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
                    ToolStr = PoolPrint (L"Could Not Load Tool:- '%s'", ToolName);
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
                    ToolStr = PoolPrint (L"Could Not Load Tool:- '%s'", ToolName);
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
                    ToolStr = PoolPrint (L"Could Not Load Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_HIDDEN:
                if (!GlobalConfig.HiddenTags) {
                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (L"Did Not Enable Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG(" * NOTE *     %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif
                }
                else {
                    MenuEntryHiddenTags = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
                    if (!MenuEntryHiddenTags) {
                        #if REFIT_DEBUG > 0
                        if (!FoundTool) {
                            ToolStr = PoolPrint (L"Could Not Load Tool:- '%s'", ToolName);
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
                        L"Did Not Enable Tool:- '%s' ... 'OsIndicationsSupported' Flag Was Not Found",
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
                        if (!MenuEntryFirmware) {
                            #if REFIT_DEBUG > 0
                            ToolStr = PoolPrint (L"Could Not Load Tool:- '%s'", ToolName);
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
                        ToolStr = PoolPrint (L"Could Not Find Tool:- '%s'", ToolName);
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
                    if (IsValidTool (SelfVolume, FileName)) {
                        #if REFIT_DEBUG > 0
                        ALT_LOG(1, LOG_LINE_NORMAL,
                            L"Adding Shell Tag:- '%s' on '%s'",
                            FileName,
                            SelfVolume->VolName
                        );
                        #endif

                        FoundTool = TRUE;
                        AddToolEntry (
                            SelfVolume, FileName,
                            L"UEFI Shell",
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
                    }

                    MY_FREE_POOL(FileName);
                } // while

                if (!FoundTool) {
                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (L"Could Not Find Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif
                }

                #if REFIT_DEBUG > 0
                ALT_LOG(1, LOG_LINE_NORMAL,
                    L"Scanning for Firmware Defined Shell Options"
                );
                #endif

                ScanFirmwareDefined (1, L"Shell", BuiltinIcon(BUILTIN_ICON_TOOL_SHELL));

                #if REFIT_DEBUG > 0
                ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
                #endif

            break;
            case TAG_GPTSYNC:
                j = 0;
                OtherFind = FALSE;
                while ((FileName = FindCommaDelimited (GPTSYNC_NAMES, j++)) != NULL) {
                    if (IsValidTool (SelfVolume, FileName)) {
                        #if REFIT_DEBUG > 0
                        ALT_LOG(1, LOG_LINE_NORMAL,
                            L"Adding Hybrid MBR Tag:- '%s' on '%s'",
                            FileName, SelfVolume->VolName
                        );
                        #endif

                        FoundTool = TRUE;
                        AddToolEntry (
                            SelfVolume, FileName,
                            L"Manage HybridMBR",
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
                    }

                    MY_FREE_POOL(FileName);
                } // while

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (L"Could Not Find Tool:- '%s'", ToolName);
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
                    if (IsValidTool (SelfVolume, FileName)) {
                        #if REFIT_DEBUG > 0
                        ALT_LOG(1, LOG_LINE_NORMAL,
                            L"Adding GDisk Tag:- '%s' on '%s'",
                            FileName, SelfVolume->VolName
                        );
                        #endif

                        FoundTool = TRUE;
                        AddToolEntry (
                            SelfVolume, FileName,
                            L"Partition Disks",
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
                    }

                    MY_FREE_POOL(FileName);
                } // while

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (L"Could Not Find Tool:- '%s'", ToolName);
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
                    if (IsValidTool (SelfVolume, FileName)) {
                        #if REFIT_DEBUG > 0
                        ALT_LOG(1, LOG_LINE_NORMAL,
                            L"Adding Netboot Tag:- '%s' on '%s'",
                            FileName, SelfVolume->VolName
                        );
                        #endif

                        FoundTool = TRUE;
                        AddToolEntry (
                            SelfVolume, FileName,
                            L"Netboot",
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
                    }

                    MY_FREE_POOL(FileName);
                } // while

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (L"Could Not Find Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_RECOVERY_APPLE:
                OtherFind  = FALSE;
                RecoverVol =  NULL;

                for (VolumeIndex = 0; VolumeIndex < HfsRecoveryCount; VolumeIndex++) {
                    j = 0;
                    while (
                        (
                            FileName = FindCommaDelimited (GlobalConfig.MacOSRecoveryFiles, j++)
                        ) != NULL
                    ) {
                        if ((HfsRecovery[VolumeIndex]->RootDir != NULL) &&
                            (IsValidTool (HfsRecovery[VolumeIndex], FileName))
                        ) {
                            // Get a meaningful tag for the recovery tool
                            // DA-TAG: Limit to TianoCore
                            #ifdef __MAKEWITH_TIANO
                            RecoverVol = RP_GetAppleDiskLabel (HfsRecovery[VolumeIndex]);
                            #endif

                            VolumeTag = (RecoverVol)
                                ? PoolPrint (L"HFS+ Recovery : %s", RecoverVol)
                                : StrDuplicate (L"HFS+ Recovery : Recovery HD");

                            #if REFIT_DEBUG > 0
                            ALT_LOG(1, LOG_LINE_NORMAL,
                                L"Adding Mac Recovery Tag:- '%s' for '%s'",
                                FileName, VolumeTag
                            );
                            #endif

                            FoundTool = TRUE;
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
                            MY_FREE_POOL(Description);

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
                        }

                        MY_FREE_POOL(FileName);
                        MY_FREE_POOL(VolumeTag);
                        MY_FREE_POOL(RecoverVol);
                    } // while
                } // for


                if (SingleAPFS) {
                    for (j = 0; j < RecoveryVolumesCount; j++) {
                        // Get a meaningful tag for the recovery tool
                        // DA-TAG: Limit to TianoCore
                        #ifdef __MAKEWITH_TIANO
                        BOOLEAN SkipSystemVolume;
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

                            if (
                                GuidsAreEqual (
                                    &(RecoveryVolumes[j]->PartGuid),
                                    &(SystemVolumes[k]->PartGuid)
                                )
                            ) {
                                VolumeRole = 0;

                                Status = RP_GetApfsVolumeInfo (
                                    SystemVolumes[k]->DeviceHandle,
                                    NULL, NULL,
                                    &VolumeRole
                                );

                                if (!EFI_ERROR(Status)) {
                                    if (VolumeRole == APPLE_APFS_VOLUME_ROLE_SYSTEM ||
                                        VolumeRole == APPLE_APFS_VOLUME_ROLE_UNDEFINED
                                    ) {
                                        RecoverVol  = StrDuplicate (SystemVolumes[k]->VolName);
                                        TmpStr      = GuidAsString (&(SystemVolumes[k]->VolUuid));
                                        FileName    = PoolPrint (L"%s\\boot.efi", TmpStr);
                                        MY_FREE_POOL(TmpStr);

                                        break;
                                    }
                                }
                            }
                        } // for k = 0
                        #endif

                        VolumeTag = (RecoverVol)
                            ? PoolPrint (L"APFS Instance : %s", RecoverVol)
                            : StrDuplicate (L"APFS Instance : Recovery");
                        #if REFIT_DEBUG > 0
                        ALT_LOG(1, LOG_LINE_NORMAL,
                            L"Adding Mac Recovery Tag:- '%s' for '%s'",
                            FileName, VolumeTag
                        );
                        #endif

                        FoundTool = TRUE;
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
                        MY_FREE_POOL(Description);

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

                        MY_FREE_POOL(FileName);
                        MY_FREE_POOL(VolumeTag);
                        MY_FREE_POOL(RecoverVol);
                    } // for j = 0
                } // if SingleAPFS

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    FlagAPFS = (!SingleAPFS && SystemVolumesCount > 0);
                    ToolStr = PoolPrint (
                        L"%s:- '%s'",
                        (FlagAPFS) ? L"Did Not Enable Tool" : L"Could Not Find Tool",
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
            case TAG_RECOVERY_WINDOWS:
                j = 0;
                OtherFind = FALSE;
                while (
                    (FileName = FindCommaDelimited (GlobalConfig.WindowsRecoveryFiles, j++)) != NULL
                ) {
                    SplitVolumeAndFilename (&FileName, &VolumeTag);
                    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                        if ((Volumes[VolumeIndex]->RootDir != NULL) &&
                            (MyStrStr (FileName, L"\\BOOT\\BOOT")) &&
                            (IsValidTool (Volumes[VolumeIndex], FileName)) &&
                            ((VolumeTag == NULL) || MyStriCmp (VolumeTag, Volumes[VolumeIndex]->VolName))
                        ) {
                            #if REFIT_DEBUG > 0
                            ALT_LOG(1, LOG_LINE_NORMAL,
                                L"Adding Windows Recovery Tag:- '%s' on '%s'",
                                FileName, Volumes[VolumeIndex]->VolName
                            );
                            #endif

                            FoundTool = TRUE;
                            Description = PoolPrint (
                                L"%s on %s",
                                ToolName,
                                Volumes[VolumeIndex]->VolName
                            );
                            AddToolEntry (
                                Volumes[VolumeIndex],
                                FileName,
                                Description,
                                BuiltinIcon (BUILTIN_ICON_TOOL_WINDOWS_RESCUE),
                                'R', TRUE
                            );
                            MY_FREE_POOL(Description);

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
                        } // if Volumes[VolumeIndex]->RootDir
                    } // for

                    MY_FREE_POOL(FileName);
                    MY_FREE_POOL(VolumeTag);
                } // while

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (L"Could Not Find Tool:- '%s'", ToolName);
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
                    L"MOK Utility",
                    BUILTIN_ICON_TOOL_MOK_TOOL
                );

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (L"Could Not Find Tool:- '%s'", ToolName);
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
                    L"Firmware Update",
                    BUILTIN_ICON_TOOL_FWUPDATE
                );

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (L"Could Not Find Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

            break;
            case TAG_CSR_ROTATE:
                if (!HasMacOS) {
                    VetCSR();

                    MY_FREE_POOL(gCsrStatus);
                    gCsrStatus = StrDuplicate (L"Incompatible Setup");
                    Status = EFI_UNSUPPORTED;
                }
                else if (GlobalConfig.DynamicCSR == -1) {
                    MY_FREE_POOL(gCsrStatus);
                    gCsrStatus = StrDuplicate (L"Dynamic SIP/SSV Disable");
                    Status = EFI_NOT_STARTED;
                }
                else if (GlobalConfig.DynamicCSR == 1) {
                    MY_FREE_POOL(gCsrStatus);
                    gCsrStatus = StrDuplicate (L"Dynamic SIP/SSV Enable");
                    Status = EFI_NOT_STARTED;
                }
                else if (GlobalConfig.CsrValues) {
                    // Only attempt to enable if CSR Values are set
                    MuteLogger = TRUE;
                    // Sets 'gCsrStatus' and returns a Status Code based on outcome
                    Status = GetCsrStatus (&CsrValue);
                    MuteLogger = FALSE;
                }
                else {
                    // Sets 'gCsrStatus' and always returns 'EFI_NOT_READY'
                    Status = FlagNoCSR();
                }

                if (EFI_ERROR(Status)) {
                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (
                        L"Did Not Enable Tool:- '%s' ... %r (%s)",
                        ToolName, Status, gCsrStatus
                    );
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG(" * NOTE *     %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif

                    break;
                }

                MenuEntryRotateCsr = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
                if (!MenuEntryRotateCsr) {
                    BREAD_CRUMB(L"%s:  C - END:- VOID ... Resource Exhaution!", FuncTag);
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
                MenuEntryInstall = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
                if (!MenuEntryInstall) {
                    BREAD_CRUMB(L"%s:  C - END:- VOID ... Resource Exhaution!", FuncTag);
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
                MenuEntryBootorder = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
                if (!MenuEntryBootorder) {
                    BREAD_CRUMB(L"%s:  C - END:- VOID ... Resource Exhaution!", FuncTag);
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
                    L"Memory Test",
                    BUILTIN_ICON_TOOL_MEMTEST
                );

                #if REFIT_DEBUG > 0
                if (!FoundTool) {
                    ToolStr = PoolPrint (L"Could Not Find Tool:- '%s'", ToolName);
                    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", ToolStr);
                    LOG_MSG("*_ WARN _*    %s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                }
                #endif

                break;
        } // switch
    } // for

    #if REFIT_DEBUG > 0
    ToolStr = PoolPrint (L"Processed %d Tool Types", ToolTotal);
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"%s", ToolStr);
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"%s", ToolStr);
    LOG_MSG("\n\n");
    LOG_MSG("INFO: %s", ToolStr);
    LOG_MSG("\n\n");
    #endif

    BREAD_CRUMB(L"%s:  C - END:- VOID", FuncTag);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID ScanForTools
