/*
 * refind/scan.c
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
#include "config.h"
#include "screen.h"
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
#include "../include/refit_call_wrapper.h"


//
// constants

#define MACOSX_LOADER_DIR      L"System\\Library\\CoreServices"
#define MACOSX_LOADER_PATH     ( MACOSX_LOADER_DIR L"\\boot.efi" )
#if defined (EFIX64)
#define SHELL_NAMES L"\\EFI\\BOOT\\tools_x64\\shell.efi,\\EFI\\BOOT\\tools_x64\\shell_x64.efi,\\EFI\\tools_x64\\shell.efi,\\EFI\\tools_x64\\shell_x64.efi,\\EFI\\tools\\shell.efi,\\EFI\\tools\\shell_x64.efi,\\EFI\\shell.efi,\\EFI\\shell_x64.efi,\\shell.efi,\\shell_x64.efi"
#define GPTSYNC_NAMES L"\\EFI\\BOOT\\tools_x64\\gptsync.efi,\\EFI\\BOOT\\tools_x64\\gptsync_x64.efi,\\EFI\\tools_x64\\gptsync.efi,\\EFI\\tools_x64\\gptsync_x64.efi,\\EFI\\tools\\gptsync.efi,\\EFI\\tools\\gptsync_x64.efi,\\EFI\\gptsync.efi,\\EFI\\gptsync_x64.efi,\\gptsync.efi,\\gptsync_x64.efi"
#define GDISK_NAMES L"\\EFI\\BOOT\\tools_x64\\gdisk.efi,\\EFI\\BOOT\\tools_x64\\gdisk_x64.efi,\\EFI\\tools_x64\\gdisk.efi,\\EFI\\tools_x64\\gdisk_x64.efi,\\EFI\\tools\\gdisk.efi,\\EFI\\tools\\gdisk_x64.efi,\\EFI\\gdisk.efi,\\EFI\\gdisk_x64.efi,\\gdisk.efi,\\gdisk_x64.efi"
#define NETBOOT_NAMES L"\\EFI\\BOOT\\tools_x64\\ipxe.efi,\\EFI\\BOOT\\tools_x64\\ipxe_x64.efi,\\EFI\\tools_x64\\ipxe.efi,\\EFI\\tools_x64\\ipxe_x64.efi,\\EFI\\tools\\ipxe.efi,\\EFI\\tools\\ipxe_x64.efi,\\EFI\\ipxe.efi,\\EFI\\ipxe_x64.efi,\\ipxe.efi,\\ipxe_x64.efi"
#define MEMTEST_NAMES L"\\EFI\\BOOT\\tools_x64\\memtest.efi,\\EFI\\BOOT\\tools_x64\\memtest_x64.efi,\\EFI\\tools_x64\\memtest.efi,\\EFI\\tools_x64\\memtest_x64.efi,\\EFI\\tools\\memtest.efi,\\EFI\\tools\\memtest_x64.efi,\\EFI\\memtest.efi,\\EFI\\memtest_x64.efi,\\memtest.efi,\\memtest_x64.efi,\\EFI\\BOOT\\tools_x64\\memtest86.efi,\\EFI\\BOOT\\tools_x64\\memtest86_x64.efi,\\EFI\\tools_x64\\memtest86.efi,\\EFI\\tools_x64\\memtest86_x64.efi,\\EFI\\tools\\memtest86.efi,\\EFI\\tools\\memtest86_x64.efi,\\EFI\\memtest86.efi,\\EFI\\memtest86_x64.efi,\\memtest86.efi,\\memtest86_x64.efi"
#define BOOTKICKER_NAMES L"\\EFI\\BOOT\\tools_x64\\BootKicker_x64.efi,\\EFI\\BOOT\\tools_x64\\BootKicker.efi,\\EFI\\tools_x64\\BootKicker_x64.efi,\\EFI\\tools_x64\\BootKicker.efi,\\EFI\\tools\\BootKicker_x64.efi,\\EFI\\tools\\BootKicker.efi,\\EFI\\BootKicker_x64.efi,\\EFI\\BootKicker.efi,\\BootKicker_x64.efi,\\BootKicker.efi"
#define NVRAMCLEAN_NAMES L"\\EFI\\BOOT\\tools_x64\\CleanNvram_x64.efi,\\EFI\\BOOT\\tools_x64\\CleanNvram.efi,\\EFI\\tools_x64\\CleanNvram_x64.efi,\\EFI\\tools_x64\\CleanNvram.efi,\\EFI\\tools\\CleanNvram_x64.efi,\\EFI\\tools\\CleanNvram.efi,\\EFI\\CleanNvram_x64.efi,\\EFI\\CleanNvram.efi,\\CleanNvram_x64.efi,\\CleanNvram.efi"
#define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootx64.efi"
#define FALLBACK_BASENAME       L"bootx64.efi"
#elif defined (EFI32)
#define SHELL_NAMES L"\\EFI\\BOOT\\tools_ia32\\shell.efi,\\EFI\\BOOT\\tools_ia32\\shell_ia32.efi,\\EFI\\tools_ia32\\shell.efi,\\EFI\\tools_ia32\\shell_ia32.efi,\\EFI\\tools\\shell.efi,\\EFI\\tools\\shell_ia32.efi,\\EFI\\shell.efi,\\EFI\\shell_ia32.efi,\\shell.efi,\\shell_ia32.efi"
#define GPTSYNC_NAMES L"\\EFI\\BOOT\\tools_ia32\\gptsync.efi,\\EFI\\BOOT\\tools_ia32\\gptsync_ia32.efi,\\EFI\\tools_ia32\\gptsync.efi,\\EFI\\tools_ia32\\gptsync_ia32.efi,\\EFI\\tools\\gptsync.efi,\\EFI\\tools\\gptsync_ia32.efi,\\EFI\\gptsync.efi,\\EFI\\gptsync_ia32.efi,\\gptsync.efi,\\gptsync_ia32.efi"
#define GDISK_NAMES L"\\EFI\\BOOT\\tools_ia32\\gdisk.efi,\\EFI\\BOOT\\tools_ia32\\gdisk_ia32.efi,\\EFI\\tools_ia32\\gdisk.efi,\\EFI\\tools_ia32\\gdisk_ia32.efi,\\EFI\\tools\\gdisk.efi,\\EFI\\tools\\gdisk_ia32.efi,\\EFI\\gdisk.efi,\\EFI\\gdisk_ia32.efi,\\gdisk.efi,\\gdisk_ia32.efi"
#define NETBOOT_NAMES L"\\EFI\\BOOT\\tools_ia32\\ipxe.efi,\\EFI\\BOOT\\tools_ia32\\ipxe_ia32.efi,\\EFI\\tools_ia32\\ipxe.efi,\\EFI\\tools_ia32\\ipxe_ia32.efi,\\EFI\\tools\\ipxe.efi,\\EFI\\tools\\ipxe_ia32.efi,\\EFI\\ipxe.efi,\\EFI\\ipxe_ia32.efi,\\ipxe.efi,\\ipxe_ia32.efi"
#define MEMTEST_NAMES L"\\EFI\\BOOT\\tools_ia32\\memtest.efi,\\EFI\\BOOT\\tools_ia32\\memtest_ia32.efi,\\EFI\\tools_ia32\\memtest.efi,\\EFI\\tools_ia32\\memtest_ia32.efi,\\EFI\\tools\\memtest.efi,\\EFI\\tools\\memtest_ia32.efi,\\EFI\\memtest.efi,\\EFI\\memtest_ia32.efi,\\memtest.efi,\\memtest_ia32.efi,\\EFI\\BOOT\\tools_ia32\\memtest86.efi,\\EFI\\BOOT\\tools_ia32\\memtest86_ia32.efi,\\EFI\\tools_ia32\\memtest86.efi,\\EFI\\tools_ia32\\memtest86_ia32.efi,\\EFI\\tools\\memtest86.efi,\\EFI\\tools\\memtest86_ia32.efi,\\EFI\\memtest86.efi,\\EFI\\memtest86_ia32.efi,\\memtest86.efi,\\memtest86_ia32.efi"
#define BOOTKICKER_NAMES L"\\EFI\\BOOT\\tools_ia32\\BootKicker_ia32.efi,\\EFI\\BOOT\\tools_ia32\\BootKicker.efi,\\EFI\\tools_ia32\\BootKicker_ia32.efi,\\EFI\\tools_ia32\\BootKicker.efi,\\EFI\\tools\\BootKicker_ia32.efi,\\EFI\\tools\\BootKicker.efi,\\EFI\\BootKicker_ia32.efi,\\EFI\\BootKicker.efi,\\BootKicker_ia32.efi,\\BootKicker.efi"
#define NVRAMCLEAN_NAMES L"\\EFI\\BOOT\\tools_ia32\\CleanNvram_ia32.efi,\\EFI\\BOOT\\tools_ia32\\CleanNvram.efi,\\EFI\\tools_ia32\\CleanNvram_ia32.efi,\\EFI\\tools_ia32\\CleanNvram.efi,\\EFI\\tools\\CleanNvram_ia32.efi,\\EFI\\tools\\CleanNvram.efi,\\EFI\\CleanNvram_ia32.efi,\\EFI\\CleanNvram.efi,\\CleanNvram_ia32.efi,\\CleanNvram.efi"
#define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootia32.efi"
#define FALLBACK_BASENAME       L"bootia32.efi"
#elif defined (EFIAARCH64)
#define SHELL_NAMES L"\\EFI\\BOOT\\tools_aa64\\shell.efi,\\EFI\\BOOT\\tools_aa64\\shell_aa64.efi,\\EFI\\tools_aa64\\shell.efi,\\EFI\\tools_aa64\\shell_aa64.efi,\\EFI\\tools\\shell.efi,\\EFI\\tools\\shell_aa64.efi,\\EFI\\shell.efi,\\EFI\\shell_aa64.efi,\\shell.efi,\\shell_aa64.efi"
#define GPTSYNC_NAMES L"\\EFI\\BOOT\\tools_aa64\\gptsync.efi,\\EFI\\BOOT\\tools_aa64\\gptsync_aa64.efi,\\EFI\\tools_aa64\\gptsync.efi,\\EFI\\tools_aa64\\gptsync_aa64.efi,\\EFI\\tools\\gptsync.efi,\\EFI\\tools\\gptsync_aa64.efi,\\EFI\\gptsync.efi,\\EFI\\gptsync_aa64.efi,\\gptsync.efi,\\gptsync_aa64.efi"
#define GDISK_NAMES L"\\EFI\\BOOT\\tools_aa64\\gdisk.efi,\\EFI\\BOOT\\tools_aa64\\gdisk_aa64.efi,\\EFI\\tools_aa64\\gdisk.efi,\\EFI\\tools_aa64\\gdisk_aa64.efi,\\EFI\\tools\\gdisk.efi,\\EFI\\tools\\gdisk_aa64.efi,\\EFI\\gdisk.efi,\\EFI\\gdisk_aa64.efi,\\gdisk.efi,\\gdisk_aa64.efi"
#define NETBOOT_NAMES L"\\EFI\\BOOT\\tools_aa64\\ipxe.efi,\\EFI\\BOOT\\tools_aa64\\ipxe_aa64.efi,\\EFI\\tools_aa64\\ipxe.efi,\\EFI\\tools_aa64\\ipxe_aa64.efi,\\EFI\\tools\\ipxe.efi,\\EFI\\tools\\ipxe_aa64.efi,\\EFI\\ipxe.efi,\\EFI\\ipxe_aa64.efi,\\ipxe.efi,\\ipxe_aa64.efi"
#define MEMTEST_NAMES L"\\EFI\\BOOT\\tools_aa64\\memtest.efi,\\EFI\\BOOT\\tools_aa64\\memtest_aa64.efi,\\EFI\\tools_aa64\\memtest.efi,\\EFI\\tools_aa64\\memtest_aa64.efi,\\EFI\\tools\\memtest.efi,\\EFI\\tools\\memtest_aa64.efi,\\EFI\\memtest.efi,\\EFI\\memtest_aa64.efi,\\memtest.efi,\\memtest_aa64.efi,\\EFI\\BOOT\\tools_aa64\\memtest86.efi,\\EFI\\BOOT\\tools_aa64\\memtest86_aa64.efi,\\EFI\\tools_aa64\\memtest86.efi,\\EFI\\tools_aa64\\memtest86_aa64.efi,\\EFI\\tools\\memtest86.efi,\\EFI\\tools\\memtest86_aa64.efi,\\EFI\\memtest86.efi,\\EFI\\memtest86_aa64.efi,\\memtest86.efi,\\memtest86_aa64.efi"
#define BOOTKICKER_NAMES L"\\EFI\\BOOT\\tools_aa64\\BootKicker_aa64.efi,\\EFI\\BOOT\\tools_aa64\\BootKicker.efi,\\EFI\\tools_aa64\\BootKicker_aa64.efi,\\EFI\\tools_aa64\\BootKicker.efi,\\EFI\\tools\\BootKicker_aa64.efi,\\EFI\\tools\\BootKicker.efi,\\EFI\\BootKicker_aa64.efi,\\EFI\\BootKicker.efi,\\BootKicker_aa64.efi,\\BootKicker.efi"
#define NVRAMCLEAN_NAMES L"\\EFI\\BOOT\\tools_aa64\\CleanNvram_aa64.efi,\\EFI\\BOOT\\tools_aa64\\CleanNvram.efi,\\EFI\\tools_aa64\\CleanNvram_aa64.efi,\\EFI\\tools_aa64\\CleanNvram.efi,\\EFI\\tools\\CleanNvram_aa64.efi,\\EFI\\tools\\CleanNvram.efi,\\EFI\\CleanNvram_aa64.efi,\\EFI\\CleanNvram.efi,\\CleanNvram_aa64.efi,\\CleanNvram.efi"
#define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootaa64.efi"
#define FALLBACK_BASENAME       L"bootaa64.efi"
#else
#define SHELL_NAMES             L"\\EFI\\tools\\shell.efi,\\shell.efi"
#define GPTSYNC_NAMES           L"\\EFI\\tools\\gptsync.efi"
#define GDISK_NAMES             L"\\EFI\\tools\\gdisk.efi"
#define NETBOOT_NAMES           L"\\EFI\\tools\\ipxe.efi"
#define MEMTEST_NAMES           L"\\memtest.efi,\\memtest86.efi"
#define BOOTKICKER_NAMES        L"\\BootKicker.efi"
#define BOOTKICKER_NAMES        L"\\CleanNvram.efi"
#define DRIVER_DIRS             L"drivers"
#define FALLBACK_FULLNAME       L"EFI\\BOOT\\boot.efi" /* Not really correct */
#define FALLBACK_BASENAME       L"boot.efi"            /* Not really correct */

#endif

#define IPXE_DISCOVER_NAME      L"\\efi\\tools\\ipxe_discover.efi"
#define IPXE_NAME               L"\\efi\\tools\\ipxe.efi"

// Patterns that identify Linux kernels. Added to the loader match pattern when the
// scan_all_linux_kernels option is set in the configuration file. Causes kernels WITHOUT
// a ".efi" extension to be found when scanning for boot loaders.
#define LINUX_MATCH_PATTERNS    L"vmlinuz*,bzImage*,kernel*"

EFI_GUID GlobalGuid = EFI_GLOBAL_VARIABLE;

static REFIT_MENU_ENTRY MenuEntryAbout = {
    L"About rEFInd",
    TAG_ABOUT,
    1, 0, 'A',
    NULL, NULL, NULL
};
static REFIT_MENU_ENTRY MenuEntryReset = {
    L"Reboot Computer",
    TAG_REBOOT,
    1, 0, 'R',
    NULL, NULL, NULL
};
static REFIT_MENU_ENTRY MenuEntryShutdown = {
    L"Shut Down Computer",
    TAG_SHUTDOWN,
    1, 0, 'U',
    NULL, NULL, NULL
};
static REFIT_MENU_ENTRY MenuEntryRotateCsr = {
    L"Change SIP Policy",
    TAG_CSR_ROTATE,
    1, 0, 0,
    NULL, NULL, NULL
};
static REFIT_MENU_ENTRY MenuEntryFirmware = {
    L"Boot into Firmaware",
    TAG_FIRMWARE,
    1, 0, 0,
    NULL, NULL, NULL
};
static REFIT_MENU_ENTRY MenuEntryHiddenTags = {
    L"Manage Hidden Tags Menu",
    TAG_HIDDEN,
    1, 0, 0, NULL,
    NULL, NULL
};
static REFIT_MENU_ENTRY MenuEntryInstall = {
    L"Install rEFInd to Disk",
    TAG_INSTALL,
    1, 0, 0,
    NULL, NULL, NULL
};
static REFIT_MENU_ENTRY MenuEntryBootorder = {
    L"Manage EFI Boot Order",
    TAG_BOOTORDER,
    1, 0, 0,
    NULL, NULL, NULL
};
static REFIT_MENU_ENTRY MenuEntryExit = {
    L"Exit rEFInd",
    TAG_EXIT,
    1, 0, 0,
    NULL, NULL, NULL
};
static REFIT_MENU_ENTRY MenuEntryPreBootKicker = {
    L"Show Boot Screen",
    TAG_PRE_BOOTKICKER,
    1, 0, 0,
    NULL, NULL, NULL
};
static REFIT_MENU_ENTRY MenuEntryPreCleanNvram = {
    L"Clean NVRAM",
    TAG_PRE_NVRAMCLEAN,
    1, 0, 0,
    NULL, NULL, NULL
};

// Structure used to hold boot loader filenames and time stamps in
// a linked list; used to sort entries within a directory.
struct LOADER_LIST {
    CHAR16              *FileName;
    EFI_TIME            TimeStamp;
    struct LOADER_LIST  *NextEntry;
};

//
// misc functions
//

// Creates a copy of a menu screen.
// Returns a pointer to the copy of the menu screen.
static REFIT_MENU_SCREEN* CopyMenuScreen(REFIT_MENU_SCREEN *Entry) {
    REFIT_MENU_SCREEN *NewEntry;
    UINTN i;

    NewEntry = AllocateZeroPool(sizeof(REFIT_MENU_SCREEN));
    if ((Entry != NULL) && (NewEntry != NULL)) {
        CopyMem(NewEntry, Entry, sizeof(REFIT_MENU_SCREEN));
        NewEntry->Title = (Entry->Title) ? StrDuplicate(Entry->Title) : NULL;
        NewEntry->TimeoutText = (Entry->TimeoutText) ? StrDuplicate(Entry->TimeoutText) : NULL;
        if (Entry->TitleImage != NULL) {
            NewEntry->TitleImage = AllocatePool(sizeof(EG_IMAGE));
            if (NewEntry->TitleImage != NULL)
                CopyMem(NewEntry->TitleImage, Entry->TitleImage, sizeof(EG_IMAGE));
        } // if
        NewEntry->InfoLines = (CHAR16**) AllocateZeroPool(Entry->InfoLineCount * (sizeof(CHAR16*)));
        for (i = 0; i < Entry->InfoLineCount && NewEntry->InfoLines; i++) {
            NewEntry->InfoLines[i] = (Entry->InfoLines[i]) ? StrDuplicate(Entry->InfoLines[i]) : NULL;
        } // for
        NewEntry->Entries = (REFIT_MENU_ENTRY**) AllocateZeroPool(Entry->EntryCount * (sizeof (REFIT_MENU_ENTRY*)));
        for (i = 0; i < Entry->EntryCount && NewEntry->Entries; i++) {
            AddMenuEntry(NewEntry, Entry->Entries[i]);
        } // for
        NewEntry->Hint1 = (Entry->Hint1) ? StrDuplicate(Entry->Hint1) : NULL;
        NewEntry->Hint2 = (Entry->Hint2) ? StrDuplicate(Entry->Hint2) : NULL;
    } // if
    return (NewEntry);
} // REFIT_MENU_SCREEN* CopyMenuScreen()

// Creates a copy of a menu entry. Intended to enable moving a stack-based
// menu entry (such as the ones for the "reboot" and "exit" functions) to
// to the heap. This enables easier deletion of the whole set of menu
// entries when re-scanning.
// Returns a pointer to the copy of the menu entry.
static REFIT_MENU_ENTRY* CopyMenuEntry(REFIT_MENU_ENTRY *Entry) {
    REFIT_MENU_ENTRY *NewEntry;

    NewEntry = AllocateZeroPool(sizeof(REFIT_MENU_ENTRY));
    if ((Entry != NULL) && (NewEntry != NULL)) {
        CopyMem(NewEntry, Entry, sizeof(REFIT_MENU_ENTRY));
        NewEntry->Title = (Entry->Title) ? StrDuplicate(Entry->Title) : NULL;
        if (Entry->BadgeImage != NULL) {
            NewEntry->BadgeImage = AllocatePool(sizeof(EG_IMAGE));
            if (NewEntry->BadgeImage != NULL)
                CopyMem(NewEntry->BadgeImage, Entry->BadgeImage, sizeof(EG_IMAGE));
        }
        if (Entry->Image != NULL) {
            NewEntry->Image = AllocatePool(sizeof(EG_IMAGE));
            if (NewEntry->Image != NULL)
                CopyMem(NewEntry->Image, Entry->Image, sizeof(EG_IMAGE));
        }
        if (Entry->SubScreen != NULL) {
            NewEntry->SubScreen = CopyMenuScreen(Entry->SubScreen);
        }
    } // if
    return (NewEntry);
} // REFIT_MENU_ENTRY* CopyMenuEntry()

// Creates a new LOADER_ENTRY data structure and populates it with
// default values from the specified Entry, or NULL values if Entry
// is unspecified (NULL).
// Returns a pointer to the new data structure, or NULL if it
// couldn't be allocated
LOADER_ENTRY *InitializeLoaderEntry(IN LOADER_ENTRY *Entry) {
    LOADER_ENTRY *NewEntry = NULL;

    NewEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
    if (NewEntry != NULL) {
        NewEntry->me.Title        = NULL;
        NewEntry->me.Tag          = TAG_LOADER;
        NewEntry->Enabled         = TRUE;
        NewEntry->UseGraphicsMode = FALSE;
        NewEntry->OSType          = 0;
        if (Entry != NULL) {
            NewEntry->LoaderPath      = (Entry->LoaderPath) ? StrDuplicate(Entry->LoaderPath) : NULL;
            NewEntry->Volume          = Entry->Volume;
            NewEntry->UseGraphicsMode = Entry->UseGraphicsMode;
            NewEntry->LoadOptions     = (Entry->LoadOptions) ? StrDuplicate(Entry->LoadOptions) : NULL;
            NewEntry->InitrdPath      = (Entry->InitrdPath) ? StrDuplicate(Entry->InitrdPath) : NULL;
        }
    } // if
    return (NewEntry);
} // LOADER_ENTRY *InitializeLoaderEntry()

// Prepare a REFIT_MENU_SCREEN data structure for a subscreen entry. This sets up
// the default entry that launches the boot loader using the same options as the
// main Entry does. Subsequent options can be added by the calling function.
// If a subscreen already exists in the Entry that's passed to this function,
// it's left unchanged and a pointer to it is returned.
// Returns a pointer to the new subscreen data structure, or NULL if there
// were problems allocating memory.
REFIT_MENU_SCREEN *InitializeSubScreen(IN LOADER_ENTRY *Entry) {
    CHAR16              *FileName, *MainOptions = NULL;
    REFIT_MENU_SCREEN   *SubScreen = NULL;
    LOADER_ENTRY        *SubEntry;

    FileName = Basename(Entry->LoaderPath);
    if (Entry->me.SubScreen == NULL) { // No subscreen yet; initialize default entry....
        SubScreen = AllocateZeroPool(sizeof(REFIT_MENU_SCREEN));
        if (SubScreen != NULL) {
            SubScreen->Title = AllocateZeroPool(sizeof(CHAR16) * 256);
            SPrint(SubScreen->Title, 255, L"Boot Options for %s on %s",
                   (Entry->Title != NULL) ? Entry->Title : FileName, Entry->Volume->VolName);
            SubScreen->TitleImage = Entry->me.Image;
            // default entry
            SubEntry = InitializeLoaderEntry(Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title = StrDuplicate(L"Boot using default options");
                MainOptions = SubEntry->LoadOptions;
                SubEntry->LoadOptions = AddInitrdToOptions(MainOptions, SubEntry->InitrdPath);
                MyFreePool(MainOptions);
                AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            } // if (SubEntry != NULL)
            SubScreen->Hint1 = StrDuplicate(SUBSCREEN_HINT1);
            if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) {
                SubScreen->Hint2 = StrDuplicate(SUBSCREEN_HINT2_NO_EDITOR);
            } else {
                SubScreen->Hint2 = StrDuplicate(SUBSCREEN_HINT2);
            } // if/else
        } // if (SubScreen != NULL)
    } else { // existing subscreen; less initialization, and just add new entry later....
        SubScreen = Entry->me.SubScreen;
    } // if/else
    MyFreePool(FileName);
    return SubScreen;
} // REFIT_MENU_SCREEN *InitializeSubScreen()

VOID GenerateSubScreen(LOADER_ENTRY *Entry, IN REFIT_VOLUME *Volume, IN BOOLEAN GenerateReturn) {
    REFIT_MENU_SCREEN  *SubScreen;
    LOADER_ENTRY       *SubEntry;
    CHAR16             *InitrdName, *KernelVersion = NULL;
    CHAR16             DiagsFileName[256];
    REFIT_FILE         *File;
    UINTN              TokenCount;
    CHAR16             **TokenList;

    // create the submenu
    if (StrLen(Entry->Title) == 0) {
        MyFreePool(Entry->Title);
        Entry->Title = NULL;
    }
    SubScreen = InitializeSubScreen(Entry);

    // loader-specific submenu entries
    if (Entry->OSType == 'M') {          // entries for macOS
#if defined(EFIX64)
        SubEntry = InitializeLoaderEntry(Entry);
        if (SubEntry != NULL) {
            SubEntry->me.Title        = L"Boot macOS with a 64-bit kernel";
            SubEntry->LoadOptions     = L"arch=x86_64";
            SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
            AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        } // if

        SubEntry = InitializeLoaderEntry(Entry);
        if (SubEntry != NULL) {
            SubEntry->me.Title        = L"Boot macOS with a 32-bit kernel";
            SubEntry->LoadOptions     = L"arch=i386";
            SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
            AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        } // if
#endif

        if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_SINGLEUSER)) {
            SubEntry = InitializeLoaderEntry(Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = L"Boot macOS in verbose mode";
                SubEntry->UseGraphicsMode = FALSE;
                SubEntry->LoadOptions     = L"-v";
                AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            } // if

#if defined(EFIX64)
            SubEntry = InitializeLoaderEntry(Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = L"Boot macOS in verbose mode (64-bit)";
                SubEntry->UseGraphicsMode = FALSE;
                SubEntry->LoadOptions     = L"-v arch=x86_64";
                AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            }

            SubEntry = InitializeLoaderEntry(Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = L"Boot macOS in verbose mode (32-bit)";
                SubEntry->UseGraphicsMode = FALSE;
                SubEntry->LoadOptions     = L"-v arch=i386";
                AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            }
#endif

            SubEntry = InitializeLoaderEntry(Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = L"Boot macOS in single user mode";
                SubEntry->UseGraphicsMode = FALSE;
                SubEntry->LoadOptions     = L"-v -s";
                AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            } // if
        } // single-user mode allowed

        if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_SAFEMODE)) {
            SubEntry = InitializeLoaderEntry(Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = L"Boot macOS in safe mode";
                SubEntry->UseGraphicsMode = FALSE;
                SubEntry->LoadOptions     = L"-v -x";
                AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            } // if
        } // safe mode allowed

        // check for Apple hardware diagnostics
        StrCpy(DiagsFileName, L"System\\Library\\CoreServices\\.diagnostics\\diags.efi");
        if (FileExists(Volume->RootDir, DiagsFileName) && !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HWTEST)) {
            SubEntry = InitializeLoaderEntry(Entry);
            if (SubEntry != NULL) {
                SubEntry->me.Title        = L"Run Apple Hardware Test";
                MyFreePool(SubEntry->LoaderPath);
                SubEntry->LoaderPath      = StrDuplicate(DiagsFileName);
                SubEntry->Volume          = Volume;
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
                AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            } // if
        } // if diagnostics entry found

    } else if (Entry->OSType == 'L') {   // entries for Linux kernels with EFI stub loaders
        File = ReadLinuxOptionsFile(Entry->LoaderPath, Volume);
        if (File != NULL) {
            InitrdName =  FindInitrd(Entry->LoaderPath, Volume);
            TokenCount = ReadTokenLine(File, &TokenList);
            KernelVersion = FindNumbers(Entry->LoaderPath);
            ReplaceSubstring(&(TokenList[1]), KERNEL_VERSION, KernelVersion);
            // first entry requires special processing, since it was initially set
            // up with a default title but correct options by InitializeSubScreen(),
            // earlier....
            if ((TokenCount > 1) && (SubScreen->Entries != NULL) && (SubScreen->Entries[0] != NULL)) {
                MyFreePool(SubScreen->Entries[0]->Title);
                SubScreen->Entries[0]->Title = TokenList[0] ? StrDuplicate(TokenList[0]) : StrDuplicate(L"Boot Linux");
            } // if
            FreeTokenLine(&TokenList, &TokenCount);
            while ((TokenCount = ReadTokenLine(File, &TokenList)) > 1) {
                ReplaceSubstring(&(TokenList[1]), KERNEL_VERSION, KernelVersion);
                SubEntry = InitializeLoaderEntry(Entry);
                SubEntry->me.Title = TokenList[0] ? StrDuplicate(TokenList[0]) : StrDuplicate(L"Boot Linux");
                MyFreePool(SubEntry->LoadOptions);
                SubEntry->LoadOptions = AddInitrdToOptions(TokenList[1], InitrdName);
                FreeTokenLine(&TokenList, &TokenCount);
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;
                AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            } // while
            MyFreePool(KernelVersion);
            MyFreePool(InitrdName);
            MyFreePool(File);
        } // if

    } else if (Entry->OSType == 'E') {   // entries for ELILO
        SubEntry = InitializeLoaderEntry(Entry);
        if (SubEntry != NULL) {
            SubEntry->me.Title        = L"Run ELILO in interactive mode";
            SubEntry->LoadOptions     = L"-p";
            SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
            AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        }

        SubEntry = InitializeLoaderEntry(Entry);
        if (SubEntry != NULL) {
            SubEntry->me.Title        = L"Boot Linux for a 17\" iMac or a 15\" MacBook Pro (*)";
            SubEntry->LoadOptions     = L"-d 0 i17";
            SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
            AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        }

        SubEntry = InitializeLoaderEntry(Entry);
        if (SubEntry != NULL) {
            SubEntry->me.Title        = L"Boot Linux for a 20\" iMac (*)";
            SubEntry->LoadOptions     = L"-d 0 i20";
            SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
            AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        }

        SubEntry = InitializeLoaderEntry(Entry);
        if (SubEntry != NULL) {
            SubEntry->me.Title        = L"Boot Linux for a Mac Mini (*)";
            SubEntry->LoadOptions     = L"-d 0 mini";
            SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
            AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        }

        AddMenuInfoLine(SubScreen, L"NOTE: This is an example. Entries");
        AddMenuInfoLine(SubScreen, L"marked with (*) may not work.");

    } else if (Entry->OSType == 'X') {   // entries for xom.efi
        // by default, skip the built-in selection and boot from hard disk only
        Entry->LoadOptions = L"-s -h";

        SubEntry = InitializeLoaderEntry(Entry);
        if (SubEntry != NULL) {
            SubEntry->me.Title        = L"Boot Windows from Hard Disk";
            SubEntry->LoadOptions     = L"-s -h";
            SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
            AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        }

        SubEntry = InitializeLoaderEntry(Entry);
        if (SubEntry != NULL) {
            SubEntry->me.Title        = L"Boot Windows from CD-ROM";
            SubEntry->LoadOptions     = L"-s -c";
            SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
            AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        }

        SubEntry = InitializeLoaderEntry(Entry);
        if (SubEntry != NULL) {
            SubEntry->me.Title        = L"Run XOM in text mode";
            SubEntry->LoadOptions     = L"-v";
            SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
            AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        }
    } // entries for xom.efi
    if (GenerateReturn)
        AddMenuEntry(SubScreen, &MenuEntryReturn);
    Entry->me.SubScreen = SubScreen;
} // VOID GenerateSubScreen()

// Sets a few defaults for a loader entry -- mainly the icon, but also the OS type
// code and shortcut letter. For Linux EFI stub loaders, also sets kernel options
// that will (with luck) work fairly automatically.
VOID SetLoaderDefaults(LOADER_ENTRY *Entry, CHAR16 *LoaderPath, REFIT_VOLUME *Volume) {
    CHAR16      *NameClues, *PathOnly, *NoExtension, *OSIconName = NULL, *Temp;
    CHAR16      ShortcutLetter = 0;

    NameClues = Basename(LoaderPath);
    PathOnly = FindPath(LoaderPath);
    NoExtension = StripEfiExtension(NameClues);

    if (Volume->DiskKind == DISK_KIND_NET) {
        MergeStrings(&NameClues, Entry->me.Title, L' ');
    } else {
        // locate a custom icon for the loader
        // Anything found here takes precedence over the "hints" in the OSIconName variable
        if (!Entry->me.Image) {
            Entry->me.Image = egLoadIconAnyType(Volume->RootDir, PathOnly, NoExtension, GlobalConfig.IconSizes[ICON_SIZE_BIG]);
        }
        if (!Entry->me.Image) {
            Entry->me.Image = egCopyImage(Volume->VolIconImage);
        }

        // Begin creating icon "hints" by using last part of directory path leading
        // to the loader
        Temp = FindLastDirName(LoaderPath);
        MergeStrings(&OSIconName, Temp, L',');
        MyFreePool(Temp);
        Temp = NULL;
        if (OSIconName != NULL) {
            ShortcutLetter = OSIconName[0];
        }

        // Add every "word" in the volume label, delimited by spaces, dashes (-), or
        // underscores (_), to the list of hints to be used in searching for OS
        // icons.
        MergeWords(&OSIconName, Volume->VolName, L',');
    } // if/else network boot

    // detect specific loaders
    if (StriSubCmp(L"bzImage", NameClues) || StriSubCmp(L"vmlinuz", NameClues) || StriSubCmp(L"kernel", NameClues)) {
        if (Volume->DiskKind != DISK_KIND_NET) {
            GuessLinuxDistribution(&OSIconName, Volume, LoaderPath);
            Entry->LoadOptions = GetMainLinuxOptions(LoaderPath, Volume);
        }
        MergeStrings(&OSIconName, L"linux", L',');
        Entry->OSType = 'L';
        if (ShortcutLetter == 0)
            ShortcutLetter = 'L';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;
    } else if (StriSubCmp(L"refit", LoaderPath)) {
        MergeStrings(&OSIconName, L"refit", L',');
        Entry->OSType = 'R';
        ShortcutLetter = 'R';
    } else if (StriSubCmp(L"refind", LoaderPath)) {
        MergeStrings(&OSIconName, L"refind", L',');
        Entry->OSType = 'R';
        ShortcutLetter = 'R';
    } else if (StriSubCmp(MACOSX_LOADER_PATH, LoaderPath)) {
        MergeStrings(&OSIconName, L"mac", L',');
        Entry->OSType = 'M';
        ShortcutLetter = 'M';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
    } else if (MyStriCmp(NameClues, L"diags.efi")) {
        MergeStrings(&OSIconName, L"hwtest", L',');
    } else if (MyStriCmp(NameClues, L"e.efi") || MyStriCmp(NameClues, L"elilo.efi") || StriSubCmp(L"elilo", NameClues)) {
        MergeStrings(&OSIconName, L"elilo,linux", L',');
        Entry->OSType = 'E';
        if (ShortcutLetter == 0)
            ShortcutLetter = 'L';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
    } else if (StriSubCmp(L"grub", NameClues)) {
        MergeStrings(&OSIconName, L"grub,linux", L',');
        Entry->OSType = 'G';
        ShortcutLetter = 'G';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_GRUB;
    } else if (MyStriCmp(NameClues, L"cdboot.efi") ||
               MyStriCmp(NameClues, L"bootmgr.efi") ||
               MyStriCmp(NameClues, L"bootmgfw.efi") ||
               MyStriCmp(NameClues, L"bkpbootmgfw.efi")) {
        MergeStrings(&OSIconName, L"win8", L',');
        Entry->OSType = 'W';
        ShortcutLetter = 'W';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
    } else if (MyStriCmp(NameClues, L"xom.efi")) {
        MergeStrings(&OSIconName, L"xom,win,win8", L',');
        Entry->OSType = 'X';
        ShortcutLetter = 'W';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
    }
    else if (StriSubCmp(L"ipxe", NameClues)) {
        Entry->OSType = 'N';
        ShortcutLetter = 'N';
        MergeStrings(&OSIconName, L"network", L',');
    }

    if ((ShortcutLetter >= 'a') && (ShortcutLetter <= 'z'))
        ShortcutLetter = ShortcutLetter - 'a' + 'A'; // convert lowercase to uppercase
    Entry->me.ShortcutLetter = ShortcutLetter;
    if (Entry->me.Image == NULL)
        Entry->me.Image = LoadOSIcon(OSIconName, L"unknown", FALSE);
    MyFreePool(PathOnly);
    MyFreePool(OSIconName);
    MyFreePool(NoExtension);
} // VOID SetLoaderDefaults()

// Add a specified EFI boot loader to the list, using automatic settings
// for icons, options, etc.
static LOADER_ENTRY * AddLoaderEntry(IN CHAR16 *LoaderPath, IN CHAR16 *LoaderTitle, IN REFIT_VOLUME *Volume, IN BOOLEAN SubScreenReturn) {
    LOADER_ENTRY  *Entry;

    CleanUpPathNameSlashes(LoaderPath);
    Entry = InitializeLoaderEntry(NULL);
    Entry->DiscoveryType = DISCOVERY_TYPE_AUTO;
    if (Entry != NULL) {
        Entry->Title = StrDuplicate((LoaderTitle != NULL) ? LoaderTitle : LoaderPath);
        Entry->me.Title = AllocateZeroPool(sizeof(CHAR16) * 256);
        // Extra space at end of Entry->me.Title enables searching on Volume->VolName even if another volume
        // name is identical except for something added to the end (e.g., VolB1 vs. VolB12).
        // Note: Volume->VolName will be NULL for network boot programs.
        if ((Volume->VolName) && (!MyStriCmp(Volume->VolName, L"Recovery HD")))
            SPrint(Entry->me.Title, 255, L"Boot %s from %s ", (LoaderTitle != NULL) ? LoaderTitle : LoaderPath, Volume->VolName);
        else
            SPrint(Entry->me.Title, 255, L"Boot %s ", (LoaderTitle != NULL) ? LoaderTitle : LoaderPath);
        Entry->me.Row = 0;
        Entry->me.BadgeImage = Volume->VolBadgeImage;
        if ((LoaderPath != NULL) && (LoaderPath[0] != L'\\')) {
            Entry->LoaderPath = StrDuplicate(L"\\");
        } else {
            Entry->LoaderPath = NULL;
        }
        MergeStrings(&(Entry->LoaderPath), LoaderPath, 0);
        Entry->Volume = Volume;
        SetLoaderDefaults(Entry, LoaderPath, Volume);
        GenerateSubScreen(Entry, Volume, SubScreenReturn);
        AddMenuEntry(&MainMenu, (REFIT_MENU_ENTRY *)Entry);
    }

    return(Entry);
} // LOADER_ENTRY * AddLoaderEntry()

// Returns -1 if (Time1 < Time2), +1 if (Time1 > Time2), or 0 if
// (Time1 == Time2). Precision is only to the nearest second; since
// this is used for sorting boot loader entries, differences smaller
// than this are likely to be meaningless (and unlikely!).
static INTN TimeComp(IN EFI_TIME *Time1, IN EFI_TIME *Time2) {
    INT64 Time1InSeconds, Time2InSeconds;

    // Following values are overestimates; I'm assuming 31 days in every month.
    // This is fine for the purpose of this function, which is limited
    Time1InSeconds = Time1->Second + (Time1->Minute * 60) + (Time1->Hour * 3600) + (Time1->Day * 86400) +
                        (Time1->Month * 2678400) + ((Time1->Year - 1998) * 32140800);
    Time2InSeconds = Time2->Second + (Time2->Minute * 60) + (Time2->Hour * 3600) + (Time2->Day * 86400) +
                        (Time2->Month * 2678400) + ((Time2->Year - 1998) * 32140800);
    if (Time1InSeconds < Time2InSeconds)
        return (-1);
    else if (Time1InSeconds > Time2InSeconds)
        return (1);

    return 0;
} // INTN TimeComp()

// Adds a loader list element, keeping it sorted by date. EXCEPTION: Fedora's rescue
// kernel, which begins with "vmlinuz-0-rescue," should not be at the top of the list,
// since that will make it the default if kernel folding is enabled, so float it to
// the end.
// Returns the new first element (the one with the most recent date).
static struct LOADER_LIST * AddLoaderListEntry(struct LOADER_LIST *LoaderList, struct LOADER_LIST *NewEntry) {
    struct LOADER_LIST *LatestEntry, *CurrentEntry, *PrevEntry = NULL;
    BOOLEAN LinuxRescue = FALSE;

    LatestEntry = CurrentEntry = LoaderList;
    if (LoaderList == NULL) {
        LatestEntry = NewEntry;
    } else {
        if (StriSubCmp(L"vmlinuz-0-rescue", NewEntry->FileName))
            LinuxRescue = TRUE;
        while ((CurrentEntry != NULL) && !StriSubCmp(L"vmlinuz-0-rescue", CurrentEntry->FileName) &&
               (LinuxRescue || (TimeComp(&(NewEntry->TimeStamp), &(CurrentEntry->TimeStamp)) < 0))) {
            PrevEntry = CurrentEntry;
            CurrentEntry = CurrentEntry->NextEntry;
        } // while
        NewEntry->NextEntry = CurrentEntry;
        if (PrevEntry == NULL) {
            LatestEntry = NewEntry;
        } else {
            PrevEntry->NextEntry = NewEntry;
        } // if/else
    } // if/else
    return (LatestEntry);
} // static VOID AddLoaderListEntry()

// Delete the LOADER_LIST linked list
static VOID CleanUpLoaderList(struct LOADER_LIST *LoaderList) {
    struct LOADER_LIST *Temp;

    while (LoaderList != NULL) {
        Temp = LoaderList;
        LoaderList = LoaderList->NextEntry;
        MyFreePool(Temp->FileName);
        MyFreePool(Temp);
    } // while
} // static VOID CleanUpLoaderList()

// Returns FALSE if the specified file/volume matches the GlobalConfig.DontScanDirs
// or GlobalConfig.DontScanVolumes specification, or if Path points to a volume
// other than the one specified by Volume, or if the specified path is SelfDir.
// Returns TRUE if none of these conditions is met -- that is, if the path is
// eligible for scanning.
static BOOLEAN ShouldScan(REFIT_VOLUME *Volume, CHAR16 *Path) {
    CHAR16   *VolName = NULL, *DontScanDir, *PathCopy = NULL, *VolGuid = NULL;
    UINTN    i = 0;
    BOOLEAN  ScanIt = TRUE;

    VolGuid = GuidAsString(&(Volume->PartGuid));
    if ((IsIn(Volume->VolName, GlobalConfig.DontScanVolumes)) ||
        (IsIn(Volume->PartName, GlobalConfig.DontScanVolumes)) ||
        (IsIn(VolGuid, GlobalConfig.DontScanVolumes))) {
        MyFreePool(VolGuid);
        return FALSE;
    } else {
        MyFreePool(VolGuid);
    } // if/else

    if (MyStriCmp(Path, SelfDirPath) && (Volume->DeviceHandle == SelfVolume->DeviceHandle))
        return FALSE;

    // See if Path includes an explicit volume declaration that's NOT Volume....
    PathCopy = StrDuplicate(Path);
    if (SplitVolumeAndFilename(&PathCopy, &VolName)) {
        if (VolName && !MyStriCmp(VolName, Volume->VolName)) {
            ScanIt = FALSE;
        } // if
    } // if Path includes volume specification
    MyFreePool(PathCopy);
    MyFreePool(VolName);
    VolName = NULL;

    // See if Volume is in GlobalConfig.DontScanDirs....
    while (ScanIt && (DontScanDir = FindCommaDelimited(GlobalConfig.DontScanDirs, i++))) {
        SplitVolumeAndFilename(&DontScanDir, &VolName);
        CleanUpPathNameSlashes(DontScanDir);
        if (VolName != NULL) {
            if (VolumeMatchesDescription(Volume, VolName) && MyStriCmp(DontScanDir, Path))
                ScanIt = FALSE;
        } else {
            if (MyStriCmp(DontScanDir, Path))
                ScanIt = FALSE;
        }
        MyFreePool(DontScanDir);
        MyFreePool(VolName);
        DontScanDir = NULL;
        VolName = NULL;
    } // while()

    return ScanIt;
} // BOOLEAN ShouldScan()

// Returns TRUE if the file is byte-for-byte identical with the fallback file
// on the volume AND if the file is not itself the fallback file; returns
// FALSE if the file is not identical to the fallback file OR if the file
// IS the fallback file. Intended for use in excluding the fallback boot
// loader when it's a duplicate of another boot loader.
static BOOLEAN DuplicatesFallback(IN REFIT_VOLUME *Volume, IN CHAR16 *FileName) {
    CHAR8           *FileContents, *FallbackContents;
    EFI_FILE_HANDLE FileHandle, FallbackHandle;
    EFI_FILE_INFO   *FileInfo, *FallbackInfo;
    UINTN           FileSize = 0, FallbackSize = 0;
    EFI_STATUS      Status;
    BOOLEAN         AreIdentical = FALSE;

    if (!FileExists(Volume->RootDir, FileName) || !FileExists(Volume->RootDir, FALLBACK_FULLNAME))
        return FALSE;

    CleanUpPathNameSlashes(FileName);

    if (MyStriCmp(FileName, FALLBACK_FULLNAME))
        return FALSE; // identical filenames, so not a duplicate....

    Status = refit_call5_wrapper(Volume->RootDir->Open, Volume->RootDir, &FileHandle, FileName, EFI_FILE_MODE_READ, 0);
    if (Status == EFI_SUCCESS) {
        FileInfo = LibFileInfo(FileHandle);
        FileSize = FileInfo->FileSize;
    } else {
        return FALSE;
    }
    MyFreePool(FileInfo);

    Status = refit_call5_wrapper(Volume->RootDir->Open, Volume->RootDir, &FallbackHandle, FALLBACK_FULLNAME, EFI_FILE_MODE_READ, 0);
    if (Status == EFI_SUCCESS) {
        FallbackInfo = LibFileInfo(FallbackHandle);
        FallbackSize = FallbackInfo->FileSize;
    } else {
        refit_call1_wrapper(FileHandle->Close, FileHandle);
        return FALSE;
    }
    MyFreePool(FallbackInfo);

    if (FallbackSize == FileSize) { // could be identical; do full check....
        FileContents = AllocatePool(FileSize);
        FallbackContents = AllocatePool(FallbackSize);
        if (FileContents && FallbackContents) {
            Status = refit_call3_wrapper(FileHandle->Read, FileHandle, &FileSize, FileContents);
            if (Status == EFI_SUCCESS) {
                Status = refit_call3_wrapper(FallbackHandle->Read, FallbackHandle, &FallbackSize, FallbackContents);
            }
            if (Status == EFI_SUCCESS) {
                AreIdentical = (CompareMem(FileContents, FallbackContents, FileSize) == 0);
            } // if
        } // if
        MyFreePool(FileContents);
        MyFreePool(FallbackContents);
    } // if/else

    // BUG ALERT: Some systems (e.g., DUET, some Macs with large displays) crash if the
    // following two calls are reversed. Go figure....
    refit_call1_wrapper(FileHandle->Close, FallbackHandle);
    refit_call1_wrapper(FileHandle->Close, FileHandle);
    return AreIdentical;
} // BOOLEAN DuplicatesFallback()

// Returns FALSE if two measures of file size are identical for a single file,
// TRUE if not or if the file can't be opened and the other measure is non-0.
// Despite the function's name, this isn't really a direct test of symbolic
// link status, since EFI doesn't officially support symlinks. It does seem
// to be a reliable indicator, though. (OTOH, some disk errors might cause a
// file to fail to open, which would return a false positive -- but as I use
// this function to exclude symbolic links from the list of boot loaders,
// that would be fine, since such boot loaders wouldn't work.)
// CAUTION: *FullName MUST be properly cleaned up (via CleanUpPathNameSlashes())
static BOOLEAN IsSymbolicLink(REFIT_VOLUME *Volume, CHAR16 *FullName, EFI_FILE_INFO *DirEntry) {
    EFI_FILE_HANDLE FileHandle;
    EFI_FILE_INFO   *FileInfo = NULL;
    EFI_STATUS      Status;
    UINTN           FileSize2 = 0;

    Status = refit_call5_wrapper(Volume->RootDir->Open, Volume->RootDir, &FileHandle, FullName, EFI_FILE_MODE_READ, 0);
    if (Status == EFI_SUCCESS) {
        FileInfo = LibFileInfo(FileHandle);
        if (FileInfo != NULL)
            FileSize2 = FileInfo->FileSize;
    }

    MyFreePool(FileInfo);

    return (DirEntry->FileSize != FileSize2);
} // BOOLEAN IsSymbolicLink()

// Scan an individual directory for EFI boot loader files and, if found,
// add them to the list. Exception: Ignores FALLBACK_FULLNAME, which is picked
// up in ScanEfiFiles(). Sorts the entries within the loader directory so that
// the most recent one appears first in the list.
// Returns TRUE if a duplicate for FALLBACK_FILENAME was found, FALSE if not.
static BOOLEAN ScanLoaderDir(IN REFIT_VOLUME *Volume, IN CHAR16 *Path, IN CHAR16 *Pattern)
{
    EFI_STATUS              Status;
    REFIT_DIR_ITER          DirIter;
    EFI_FILE_INFO           *DirEntry;
    CHAR16                  Message[256], *Extension, *FullName;
    struct LOADER_LIST      *LoaderList = NULL, *NewLoader;
    LOADER_ENTRY            *FirstKernel = NULL, *LatestEntry = NULL;
    BOOLEAN                 FoundFallbackDuplicate = FALSE, IsLinux = FALSE, InSelfPath;

    InSelfPath = MyStriCmp(Path, SelfDirPath);
    if ((!SelfDirPath || !Path || (InSelfPath && (Volume->DeviceHandle != SelfVolume->DeviceHandle)) ||
           (!InSelfPath)) && (ShouldScan(Volume, Path))) {
       // look through contents of the directory
       DirIterOpen(Volume->RootDir, Path, &DirIter);
       while (DirIterNext(&DirIter, 2, Pattern, &DirEntry)) {
          Extension = FindExtension(DirEntry->FileName);
          FullName = StrDuplicate(Path);
          MergeStrings(&FullName, DirEntry->FileName, L'\\');
          CleanUpPathNameSlashes(FullName);
          if (DirEntry->FileName[0] == '.' ||
              MyStriCmp(Extension, L".icns") ||
              MyStriCmp(Extension, L".png") ||
              (MyStriCmp(DirEntry->FileName, FALLBACK_BASENAME) && (MyStriCmp(Path, L"EFI\\BOOT"))) ||
              FilenameIn(Volume, Path, DirEntry->FileName, SHELL_NAMES) ||
              IsSymbolicLink(Volume, FullName, DirEntry) || /* is symbolic link */
              HasSignedCounterpart(Volume, FullName) || /* a file with same name plus ".efi.signed" is present */
              FilenameIn(Volume, Path, DirEntry->FileName, GlobalConfig.DontScanFiles) ||
              !IsValidLoader(Volume->RootDir, FullName)) {
                continue;   // skip this
          }

          NewLoader = AllocateZeroPool(sizeof(struct LOADER_LIST));
          if (NewLoader != NULL) {
             NewLoader->FileName = StrDuplicate(FullName);
             NewLoader->TimeStamp = DirEntry->ModificationTime;
             LoaderList = AddLoaderListEntry(LoaderList, NewLoader);
             if (DuplicatesFallback(Volume, FullName))
                FoundFallbackDuplicate = TRUE;
          } // if
          MyFreePool(Extension);
          MyFreePool(FullName);
       } // while

       NewLoader = LoaderList;
       while (NewLoader != NULL) {
           IsLinux = (StriSubCmp(L"bzImage", NewLoader->FileName) ||
                      StriSubCmp(L"vmlinuz", NewLoader->FileName) ||
                      StriSubCmp(L"kernel", NewLoader->FileName));
           if ((FirstKernel != NULL) && IsLinux && GlobalConfig.FoldLinuxKernels) {
               AddKernelToSubmenu(FirstKernel, NewLoader->FileName, Volume);
           } else {
               LatestEntry = AddLoaderEntry(NewLoader->FileName, NULL, Volume, !(IsLinux && GlobalConfig.FoldLinuxKernels));
               if (IsLinux && (FirstKernel == NULL))
                   FirstKernel = LatestEntry;
           }
           NewLoader = NewLoader->NextEntry;
       } // while
       if ((NewLoader != NULL) && (FirstKernel != NULL) && IsLinux && GlobalConfig.FoldLinuxKernels)
           AddMenuEntry(FirstKernel->me.SubScreen, &MenuEntryReturn);

       CleanUpLoaderList(LoaderList);
       Status = DirIterClose(&DirIter);
       // NOTE: EFI_INVALID_PARAMETER really is an error that should be reported;
       // but I've gotten reports from users who are getting this error occasionally
       // and I can't find anything wrong or reproduce the problem, so I'm putting
       // it down to buggy EFI implementations and ignoring that particular error....
       if ((Status != EFI_NOT_FOUND) && (Status != EFI_INVALID_PARAMETER)) {
          if (Path)
             SPrint(Message, 255, L"while scanning the %s directory on %s", Path, Volume->VolName);
          else
             SPrint(Message, 255, L"while scanning the root directory on %s", Path, Volume->VolName);
          CheckError(Status, Message);
       } // if (Status != EFI_NOT_FOUND)
    } // if not scanning a blacklisted directory

    return FoundFallbackDuplicate;
} /* static VOID ScanLoaderDir() */

// Run the IPXE_DISCOVER_NAME program, which obtains the IP address of the boot
// server and the name of the boot file it delivers.
static CHAR16* RuniPXEDiscover(EFI_HANDLE Volume)
{
    EFI_STATUS       Status;
    EFI_DEVICE_PATH  *FilePath;
    EFI_HANDLE       iPXEHandle;
    CHAR16           *boot_info = NULL;
    UINTN            boot_info_size = 0;

    FilePath = FileDevicePath (Volume, IPXE_DISCOVER_NAME);
    Status = refit_call6_wrapper(BS->LoadImage, FALSE, SelfImageHandle, FilePath, NULL, 0, &iPXEHandle);
    if (Status != 0)
        return NULL;

    Status = refit_call3_wrapper(BS->StartImage, iPXEHandle, &boot_info_size, &boot_info);

    return boot_info;
} // RuniPXEDiscover()

// Scan for network (PXE) boot servers. This function relies on the presence
// of the IPXE_DISCOVER_NAME and IPXE_NAME program files on the volume from
// which rEFInd launched. As of December 6, 2014, these tools aren't entirely
// reliable. See BUILDING.txt for information on building them.
static VOID ScanNetboot() {
    CHAR16        *iPXEFileName = IPXE_NAME;
    CHAR16        *Location;
    REFIT_VOLUME  *NetVolume;

    if (FileExists(SelfVolume->RootDir, IPXE_DISCOVER_NAME) &&
        FileExists(SelfVolume->RootDir, IPXE_NAME) &&
        IsValidLoader(SelfVolume->RootDir, IPXE_DISCOVER_NAME) &&
        IsValidLoader(SelfVolume->RootDir, IPXE_NAME)) {
            Location = RuniPXEDiscover(SelfVolume->DeviceHandle);
            if (Location != NULL && FileExists(SelfVolume->RootDir, iPXEFileName)) {
                NetVolume = AllocatePool(sizeof(REFIT_VOLUME));
                CopyMem(NetVolume, SelfVolume, sizeof(REFIT_VOLUME));
                NetVolume->DiskKind = DISK_KIND_NET;
                NetVolume->VolBadgeImage = BuiltinIcon(BUILTIN_ICON_VOL_NET);
                NetVolume->PartName = NetVolume->VolName = NULL;
                AddLoaderEntry(iPXEFileName, Location, NetVolume, TRUE);
                MyFreePool(NetVolume);
            } // if support files exist and are valid
    }
} // VOID ScanNetboot()

// Adds *FullFileName as a macOS loader, if it exists.
// Returns TRUE if the fallback loader is NOT a duplicate of this one,
// FALSE if it IS a duplicate.
static BOOLEAN ScanMacOsLoader(REFIT_VOLUME *Volume, CHAR16* FullFileName) {
    BOOLEAN  ScanFallbackLoader = TRUE;
    CHAR16   *VolName = NULL, *PathName = NULL, *FileName = NULL;

    SplitPathName(FullFileName, &VolName, &PathName, &FileName);
    if (FileExists(Volume->RootDir, FullFileName) && !FilenameIn(Volume, PathName, L"boot.efi", GlobalConfig.DontScanFiles)) {
        AddLoaderEntry(FullFileName, L"macOS", Volume, TRUE);
        if (DuplicatesFallback(Volume, FullFileName))
            ScanFallbackLoader = FALSE;
    } // if
    MyFreePool(VolName);
    MyFreePool(PathName);
    MyFreePool(FileName);
    return ScanFallbackLoader;
} // VOID ScanMacOsLoader()

static VOID ScanEfiFiles(REFIT_VOLUME *Volume) {
    EFI_STATUS       Status;
    REFIT_DIR_ITER   EfiDirIter;
    EFI_FILE_INFO    *EfiDirEntry;
    CHAR16           FileName[256], *Directory = NULL, *MatchPatterns, *VolName = NULL, *SelfPath, *Temp;
    UINTN            i, Length;
    BOOLEAN          ScanFallbackLoader = TRUE;
    BOOLEAN          FoundBRBackup = FALSE;

    if (Volume && (Volume->RootDir != NULL) && (Volume->VolName != NULL) && (Volume->IsReadable)) {
        MatchPatterns = StrDuplicate(LOADER_MATCH_PATTERNS);
        if (GlobalConfig.ScanAllLinux)
            MergeStrings(&MatchPatterns, LINUX_MATCH_PATTERNS, L',');

        // check for macOS boot loader
        if (ShouldScan(Volume, MACOSX_LOADER_DIR)) {
            StrCpy(FileName, MACOSX_LOADER_PATH);
            ScanFallbackLoader &= ScanMacOsLoader(Volume, FileName);
            DirIterOpen(Volume->RootDir, L"\\", &EfiDirIter);
            while (DirIterNext(&EfiDirIter, 1, NULL, &EfiDirEntry)) {
                if (IsGuid(EfiDirEntry->FileName)) {
                    SPrint(FileName, 255, L"%s\\%s", EfiDirEntry->FileName, MACOSX_LOADER_PATH);
                    ScanFallbackLoader &= ScanMacOsLoader(Volume, FileName);
                    SPrint(FileName, 255, L"%s\\%s", EfiDirEntry->FileName, L"boot.efi");
                    if (!StriSubCmp(FileName, GlobalConfig.MacOSRecoveryFiles))
                        MergeStrings(&GlobalConfig.MacOSRecoveryFiles, FileName, L',');
                } // if
            } // while
            Status = DirIterClose(&EfiDirIter);

            // check for XOM
            StrCpy(FileName, L"System\\Library\\CoreServices\\xom.efi");
            if (FileExists(Volume->RootDir, FileName) && !FilenameIn(Volume, MACOSX_LOADER_DIR, L"xom.efi", GlobalConfig.DontScanFiles)) {
                AddLoaderEntry(FileName, L"Windows XP (XoM)", Volume, TRUE);
                if (DuplicatesFallback(Volume, FileName))
                    ScanFallbackLoader = FALSE;
            }
        } // if should scan Mac directory

        // check for Microsoft boot loader/menu
        if (ShouldScan(Volume, L"EFI\\Microsoft\\Boot")) {
            StrCpy(FileName, L"EFI\\Microsoft\\Boot\\bkpbootmgfw.efi");
            if (FileExists(Volume->RootDir, FileName) &&  !FilenameIn(Volume, L"EFI\\Microsoft\\Boot", L"bkpbootmgfw.efi",
                GlobalConfig.DontScanFiles)) {
                    AddLoaderEntry(FileName, L"Microsoft EFI boot (Boot Repair backup)", Volume, TRUE);
                    FoundBRBackup = TRUE;
                    if (DuplicatesFallback(Volume, FileName))
                        ScanFallbackLoader = FALSE;
            }
            StrCpy(FileName, L"EFI\\Microsoft\\Boot\\bootmgfw.efi");
            if (FileExists(Volume->RootDir, FileName) &&
                !FilenameIn(Volume, L"EFI\\Microsoft\\Boot", L"bootmgfw.efi", GlobalConfig.DontScanFiles)) {
                    if (FoundBRBackup)
                        AddLoaderEntry(FileName, L"Supposed Microsoft EFI boot (probably GRUB)", Volume, TRUE);
                    else
                        AddLoaderEntry(FileName, L"Microsoft EFI boot", Volume, TRUE);
                    if (DuplicatesFallback(Volume, FileName))
                        ScanFallbackLoader = FALSE;
            }
        } // if

        // scan the root directory for EFI executables
        if (ScanLoaderDir(Volume, L"\\", MatchPatterns))
            ScanFallbackLoader = FALSE;

        // scan subdirectories of the EFI directory (as per the standard)
        DirIterOpen(Volume->RootDir, L"EFI", &EfiDirIter);
        while (DirIterNext(&EfiDirIter, 1, NULL, &EfiDirEntry)) {
            if (MyStriCmp(EfiDirEntry->FileName, L"tools") || EfiDirEntry->FileName[0] == '.')
                continue;   // skip this, doesn't contain boot loaders or is scanned later
            SPrint(FileName, 255, L"EFI\\%s", EfiDirEntry->FileName);
            if (ScanLoaderDir(Volume, FileName, MatchPatterns))
                ScanFallbackLoader = FALSE;
        } // while()
        Status = DirIterClose(&EfiDirIter);
        if ((Status != EFI_NOT_FOUND) && (Status != EFI_INVALID_PARAMETER)) {
            Temp = PoolPrint(L"while scanning the EFI directory on %s", Volume->VolName);
            CheckError(Status, Temp);
            MyFreePool(Temp);
        } // if

        // Scan user-specified (or additional default) directories....
        i = 0;
        while ((Directory = FindCommaDelimited(GlobalConfig.AlsoScan, i++)) != NULL) {
            if (ShouldScan(Volume, Directory)) {
                SplitVolumeAndFilename(&Directory, &VolName);
                CleanUpPathNameSlashes(Directory);
                Length = StrLen(Directory);
                if ((Length > 0) && ScanLoaderDir(Volume, Directory, MatchPatterns))
                    ScanFallbackLoader = FALSE;
                MyFreePool(VolName);
            } // if should scan dir
            MyFreePool(Directory);
        } // while

        // Don't scan the fallback loader if it's on the same volume and a duplicate of rEFInd itself....
        SelfPath = DevicePathToStr(SelfLoadedImage->FilePath);
        CleanUpPathNameSlashes(SelfPath);
        if ((Volume->DeviceHandle == SelfLoadedImage->DeviceHandle) && DuplicatesFallback(Volume, SelfPath))
            ScanFallbackLoader = FALSE;
        MyFreePool(SelfPath);

        // If not a duplicate & if it exists & if it's not us, create an entry
        // for the fallback boot loader
        if (ScanFallbackLoader && FileExists(Volume->RootDir, FALLBACK_FULLNAME) && ShouldScan(Volume, L"EFI\\BOOT") &&
            !FilenameIn(Volume, L"EFI\\BOOT", FALLBACK_BASENAME, GlobalConfig.DontScanFiles)) {
                AddLoaderEntry(FALLBACK_FULLNAME, L"Fallback boot loader", Volume, TRUE);
        }
        MyFreePool(MatchPatterns);
    } // if
} // static VOID ScanEfiFiles()

// Scan internal disks for valid EFI boot loaders....
static VOID ScanInternal(VOID) {
    UINTN                   VolumeIndex;

    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_INTERNAL) {
            ScanEfiFiles(Volumes[VolumeIndex]);
        }
    } // for
} // static VOID ScanInternal()

// Scan external disks for valid EFI boot loaders....
static VOID ScanExternal(VOID) {
    UINTN                   VolumeIndex;

    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_EXTERNAL) {
            ScanEfiFiles(Volumes[VolumeIndex]);
        }
    } // for
} // static VOID ScanExternal()

// Scan internal disks for valid EFI boot loaders....
static VOID ScanOptical(VOID) {
    UINTN                   VolumeIndex;

    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_OPTICAL) {
            ScanEfiFiles(Volumes[VolumeIndex]);
        }
    } // for
} // static VOID ScanOptical()

// default volume badge icon based on disk kind
EG_IMAGE * GetDiskBadge(IN UINTN DiskType) {
    EG_IMAGE * Badge = NULL;

    switch (DiskType) {
        case BBS_HARDDISK:
            Badge = BuiltinIcon(BUILTIN_ICON_VOL_INTERNAL);
            break;
        case BBS_USB:
            Badge = BuiltinIcon(BUILTIN_ICON_VOL_EXTERNAL);
            break;
        case BBS_CDROM:
            Badge = BuiltinIcon(BUILTIN_ICON_VOL_OPTICAL);
            break;
    } // switch()
    return Badge;
} // EG_IMAGE * GetDiskBadge()

//
// pre-boot tool functions
//

static LOADER_ENTRY * AddToolEntry(
    REFIT_VOLUME *Volume,
    IN CHAR16 *LoaderPath,
    IN CHAR16 *LoaderTitle,
    IN EG_IMAGE *Image,
    IN CHAR16 ShortcutLetter,
    IN BOOLEAN UseGraphicsMode
) {
    LOADER_ENTRY *Entry;
    CHAR16       *TitleStr = NULL;

    Entry = AllocateZeroPool(sizeof(LOADER_ENTRY));

    TitleStr = PoolPrint(L"Load %s", LoaderTitle);
    Entry->me.Title = TitleStr;
    Entry->me.Tag = TAG_TOOL;
    Entry->me.Row = 1;
    Entry->me.ShortcutLetter = ShortcutLetter;
    Entry->me.Image = Image;
    Entry->LoaderPath = (LoaderPath) ? StrDuplicate(LoaderPath) : NULL;
    Entry->Volume = Volume;
    Entry->UseGraphicsMode = UseGraphicsMode;

    AddMenuEntry(&MainMenu, (REFIT_MENU_ENTRY *)Entry);
    return Entry;
} /* static LOADER_ENTRY * AddToolEntry() */

// Locates boot loaders. NOTE: This assumes that GlobalConfig.LegacyType is set correctly.
VOID ScanForBootloaders(BOOLEAN ShowMessage) {
    UINTN    i;
    CHAR8    s;
    BOOLEAN  ScanForLegacy = FALSE;
    EG_PIXEL BGColor = COLOR_LIGHTBLUE;
    CHAR16   *HiddenTags;

    #if REFIT_DEBUG > 0
    MsgLog("Scan for Bootloaders...\n");
    #endif

    if (ShowMessage){
        egDisplayMessage(L"Scanning for boot loaders; please wait....", &BGColor, CENTER);
    }

    // Determine up-front if we'll be scanning for legacy loaders....
    for (i = 0; i < NUM_SCAN_OPTIONS; i++) {
        s = GlobalConfig.ScanFor[i];
        if ((s == 'c') || (s == 'C') || (s == 'h') || (s == 'H') || (s == 'b') || (s == 'B'))
            ScanForLegacy = TRUE;
    } // for

    // If UEFI & scanning for legacy loaders & deep legacy scan, update NVRAM boot manager list
    if ((GlobalConfig.LegacyType == LEGACY_TYPE_UEFI) && ScanForLegacy && GlobalConfig.DeepLegacyScan) {
        BdsDeleteAllInvalidLegacyBootOptions();
        BdsAddNonExistingLegacyBootOptions();
    } // if

    HiddenTags = ReadHiddenTags(L"HiddenTags");
    if ((HiddenTags) && (StrLen(HiddenTags) > 0)) {
        MergeStrings(&GlobalConfig.DontScanFiles, HiddenTags, L',');
    }
    HiddenTags = ReadHiddenTags(L"HiddenLegacy");
    if ((HiddenTags) && (StrLen(HiddenTags) > 0)) {
        MergeStrings(&GlobalConfig.DontScanVolumes, HiddenTags, L',');
    }
    MyFreePool(HiddenTags);

    // scan for loaders and tools, add them to the menu
    for (i = 0; i < NUM_SCAN_OPTIONS; i++) {
        switch(GlobalConfig.ScanFor[i]) {
            case 'c': case 'C':
                #if REFIT_DEBUG > 0
                MsgLog("Scan Legacy Disc:\n");
                #endif

                ScanLegacyDisc();
                break;
            case 'h': case 'H':
                #if REFIT_DEBUG > 0
                MsgLog("Scan Legacy Internal:\n");
                #endif

                ScanLegacyInternal();
                break;
            case 'b': case 'B':
                #if REFIT_DEBUG > 0
                MsgLog("Scan Legacy External:\n");
                #endif

                ScanLegacyExternal();
                break;
            case 'm': case 'M':
                #if REFIT_DEBUG > 0
                MsgLog("Scan Manual Stanzas:\n");
                #endif

                ScanUserConfigured(GlobalConfig.ConfigFilename);
                break;
            case 'e': case 'E':
                #if REFIT_DEBUG > 0
                MsgLog("Scan External:\n");
                #endif

                ScanExternal();
                break;
            case 'i': case 'I':
                #if REFIT_DEBUG > 0
                MsgLog("Scan Internal:\n");
                #endif

                ScanInternal();
                break;
            case 'o': case 'O':
                #if REFIT_DEBUG > 0
                MsgLog("Scan Optical:\n");
                #endif

                ScanOptical();
                break;
            case 'n': case 'N':
                #if REFIT_DEBUG > 0
                MsgLog("Scan Net boot:\n");
                #endif

                ScanNetboot();
                break;
        } // switch()
    } // for

    // assign shortcut keys
    #if REFIT_DEBUG > 0
    MsgLog("Assign Shortcut Keys:\n");
    #endif

    for (i = 0; i < MainMenu.EntryCount && MainMenu.Entries[i]->Row == 0 && i < 9; i++) {
        #if REFIT_DEBUG > 0
        if ((i + 1) < MainMenu.EntryCount && MainMenu.Entries[i]->Row == 0 && (i + 1) < 9) {
            MsgLog("  - Shortcut Key[%d] Assigned\n", i + 1);
        } else {
            MsgLog("  - Shortcut Key[%d] Assigned\n\n", i + 1);
        }
        #endif

        MainMenu.Entries[i]->ShortcutDigit = (CHAR16)('1' + i);
    }

    // wait for user ACK when there were errors
    FinishTextScreen(FALSE);
} // VOID ScanForBootloaders()

// Checks to see if a specified file seems to be a valid tool.
// Returns TRUE if it passes all tests, FALSE otherwise
static BOOLEAN IsValidTool(IN REFIT_VOLUME *BaseVolume, CHAR16 *PathName) {
    CHAR16 *DontVolName = NULL, *DontPathName = NULL, *DontFileName = NULL, *DontScanThis;
    CHAR16 *TestVolName = NULL, *TestPathName = NULL, *TestFileName = NULL;
    BOOLEAN retval = TRUE;
    UINTN i = 0;

    if (FileExists(BaseVolume->RootDir, PathName) && IsValidLoader(BaseVolume->RootDir, PathName)) {
        SplitPathName(PathName, &TestVolName, &TestPathName, &TestFileName);
        while (retval && (DontScanThis = FindCommaDelimited(GlobalConfig.DontScanTools, i++))) {
            SplitPathName(DontScanThis, &DontVolName, &DontPathName, &DontFileName);
            if (MyStriCmp(TestFileName, DontFileName) &&
                ((DontPathName == NULL) || (MyStriCmp(TestPathName, DontPathName))) &&
                ((DontVolName == NULL) || (VolumeMatchesDescription(BaseVolume, DontVolName)))
            ) {
                retval = FALSE;
            } // if
            MyFreePool(DontScanThis);
        } // while
    } else
        retval = FALSE;
    MyFreePool(TestVolName);
    MyFreePool(TestPathName);
    MyFreePool(TestFileName);
    return retval;
} // BOOLEAN IsValidTool()

// Locate a single tool from the specified Locations using one of the
// specified Names and add it to the menu.
static BOOLEAN FindTool(
    CHAR16 *Locations,
    CHAR16 *Names,
    CHAR16 *Description,
    UINTN  Icon
) {
    UINTN   j = 0;
    UINTN   k;
    UINTN   VolumeIndex;
    CHAR16  *DirName;
    CHAR16  *FileName;
    CHAR16  *PathName;
    BOOLEAN FoundTool = FALSE;

//DA-TAG: Commented Out
//    CHAR16 FullDescription[256];

    while ((DirName = FindCommaDelimited(Locations, j++)) != NULL) {
        k = 0;
        while ((FileName = FindCommaDelimited(Names, k++)) != NULL) {
            PathName = StrDuplicate(DirName);
            MergeStrings(&PathName, FileName, MyStriCmp(PathName, L"\\") ? 0 : L'\\');
            for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                if ((Volumes[VolumeIndex]->RootDir != NULL)
                    && (IsValidTool(Volumes[VolumeIndex], PathName))
                ) {
                    FoundTool = TRUE;
//DA-TAG: Commented Out
//                    SPrint(
//                        FullDescription,
//                        255,
//                        L"%s at %s on %s",
//                        Description,
//                        PathName,
//                        Volumes[VolumeIndex]->VolName
//                    );

//DA-TAG: Changed "FullDescription" to "Description" below
                    AddToolEntry(
                        Volumes[VolumeIndex],
                        PathName,
                        Description,
                        BuiltinIcon(Icon),
                        'S',
                        FALSE
                    );

                    #if REFIT_DEBUG > 0
                    MsgLog(
                        "  - Added %s Tool : %s%s\n",
                        Description,
                        StrDuplicate(DirName),
                        StrDuplicate(FileName)
                    );
                    #endif

                } // if
            } // for
            MyFreePool(PathName);
            MyFreePool(FileName);
        } // while Names
        MyFreePool(DirName);
    } // while Locations

    return FoundTool;
} // VOID FindTool()

// Add the second-row tags containing built-in and external tools (EFI shell,
// reboot, etc.)
VOID ScanForTools(VOID) {
    CHAR16           *FileName = NULL;
    CHAR16           *VolName = NULL;
    CHAR16           *MokLocations;
    CHAR16           Description[256];
    CHAR16           *HiddenTools;
    REFIT_MENU_ENTRY *TempMenuEntry;
    UINTN            i;
    UINTN            j;
    UINTN            VolumeIndex;
    UINT64           osind;
    CHAR8            *b = 0;
    UINT32           CsrValue;
    BOOLEAN          FoundTool;
    CHAR16           *ToolName = NULL;

    #if REFIT_DEBUG > 0
    MsgLog("Scan for Builtin/External Tools...\n");
    MsgLog("Tool Type Count = %d\n", NUM_TOOLS);
    #endif

    MokLocations = StrDuplicate(MOK_LOCATIONS);
    if (MokLocations != NULL) {
        MergeStrings(&MokLocations, SelfDirPath, L',');
    }

    HiddenTools = ReadHiddenTags(L"HiddenTools");
    if ((HiddenTools) && (StrLen(HiddenTools) > 0)) {
        MergeStrings(&GlobalConfig.DontScanTools, HiddenTools, L',');
    }
    MyFreePool(HiddenTools);

    for (i = 0; i < NUM_TOOLS; i++) {
        // Reset Vars
        FileName = NULL;
        VolName = NULL;
        FoundTool = FALSE;
        ToolName = NULL;

        #if REFIT_DEBUG > 0
        switch(GlobalConfig.ShowTools[i]) {
            // NOTE: Be sure that FileName is NULL at the end of each case.
            case TAG_SHUTDOWN:
                ToolName = L"Shutdown Computer";
                break;

            case TAG_REBOOT:
                ToolName = L"Reboot Computer";
                break;

            case TAG_ABOUT:
                ToolName = L"About rEFInd";
                break;

            case TAG_EXIT:
                ToolName = L"Exit rEFInd";
                break;

            case TAG_HIDDEN:
                ToolName = L"Hidden Tags";
                break;

            case TAG_FIRMWARE:
                ToolName = L"Firmware";
                break;

            case TAG_SHELL:
                ToolName = L"EFI Shell";
                break;

            case TAG_GPTSYNC:
                ToolName = L"GPT Sync";
                break;

            case TAG_GDISK:
                ToolName = L"GDisk";
                break;

            case TAG_NETBOOT:
                ToolName = L"Net Boot";
                break;

            case TAG_APPLE_RECOVERY:
                ToolName = L"Apple Recovery";
                break;

            case TAG_WINDOWS_RECOVERY:
                ToolName = L"Windows Recovery";
                break;

            case TAG_MOK_TOOL:
                ToolName = L"MOK";
                break;

            case TAG_FWUPDATE_TOOL:
                ToolName = L"Firmware Update";
                break;

            case TAG_CSR_ROTATE:
                ToolName = L"Change SIP Policy";
                break;

            case TAG_INSTALL:
                ToolName = L"Install rEFInd";
                break;

            case TAG_BOOTORDER:
                ToolName = L"Manage Boot Order";
                break;

            case TAG_PRE_BOOTKICKER:
                ToolName = L"BootKicker";
                break;

            case TAG_PRE_NVRAMCLEAN:
                ToolName = L"CleanNvram";
                break;

            case TAG_MEMTEST:
                ToolName = L"Memtest";
                break;

            default:
                ToolName = L"Skipped";
        }

        if(!GlobalConfig.ShowTools[i]) {
            ToolName = L"Skipped";
        }
        MsgLog("Scan for Tool Type %d - %s\n", i + 1, ToolName);
        #endif

        if (MyStrStr(ToolName, L"Skipped") != NULL) {
            continue;
        }

        switch(GlobalConfig.ShowTools[i]) {
            case TAG_PRE_NVRAMCLEAN:
                TempMenuEntry = CopyMenuEntry(&MenuEntryPreCleanNvram);
                TempMenuEntry->Image = BuiltinIcon(BUILTIN_ICON_TOOL_NVRAMCLEAN);
                AddMenuEntry(&MainMenu, TempMenuEntry);

                #if REFIT_DEBUG > 0
                MsgLog("  - Added %s Tool\n", ToolName);
                #endif

                break;

            case TAG_PRE_BOOTKICKER:
                TempMenuEntry = CopyMenuEntry(&MenuEntryPreBootKicker);
                TempMenuEntry->Image = BuiltinIcon(BUILTIN_ICON_TOOL_BOOTKICKER);
                AddMenuEntry(&MainMenu, TempMenuEntry);

                #if REFIT_DEBUG > 0
                MsgLog("  - Added %s Tool\n", ToolName);
                #endif

                break;

            case TAG_SHUTDOWN:
                TempMenuEntry = CopyMenuEntry(&MenuEntryShutdown);
                TempMenuEntry->Image = BuiltinIcon(BUILTIN_ICON_FUNC_SHUTDOWN);
                AddMenuEntry(&MainMenu, TempMenuEntry);

                #if REFIT_DEBUG > 0
                MsgLog("  - Added %s Tool\n", ToolName);
                #endif

                break;

            case TAG_REBOOT:
                TempMenuEntry = CopyMenuEntry(&MenuEntryReset);
                TempMenuEntry->Image = BuiltinIcon(BUILTIN_ICON_FUNC_RESET);
                AddMenuEntry(&MainMenu, TempMenuEntry);

                #if REFIT_DEBUG > 0
                MsgLog("  - Added %s Tool\n", ToolName);
                #endif

                break;

            case TAG_ABOUT:
                TempMenuEntry = CopyMenuEntry(&MenuEntryAbout);
                TempMenuEntry->Image = BuiltinIcon(BUILTIN_ICON_FUNC_ABOUT);
                AddMenuEntry(&MainMenu, TempMenuEntry);

                #if REFIT_DEBUG > 0
                MsgLog("  - Added %s Tool\n", ToolName);
                #endif

                break;

            case TAG_EXIT:
                TempMenuEntry = CopyMenuEntry(&MenuEntryExit);
                TempMenuEntry->Image = BuiltinIcon(BUILTIN_ICON_FUNC_EXIT);
                AddMenuEntry(&MainMenu, TempMenuEntry);

                #if REFIT_DEBUG > 0
                MsgLog("  - Added %s Tool\n", ToolName);
                #endif

                break;

            case TAG_HIDDEN:
                if (GlobalConfig.HiddenTags) {
                    FoundTool = TRUE;
                    TempMenuEntry = CopyMenuEntry(&MenuEntryHiddenTags);
                    TempMenuEntry->Image = BuiltinIcon(BUILTIN_ICON_FUNC_HIDDEN);
                    AddMenuEntry(&MainMenu, TempMenuEntry);

                    #if REFIT_DEBUG > 0
                    MsgLog("  - Added %s Tool\n", ToolName);
                    #endif
                }

                if (FoundTool == FALSE) {
                    #if REFIT_DEBUG > 0
                    MsgLog("  - WARN: Could not Find %s Tool\n", ToolName);
                    #endif
                }

                break;

            case TAG_FIRMWARE:
                if (EfivarGetRaw(
                    &GlobalGuid,
                    L"OsIndicationsSupported",
                    &b,
                    &j
                ) == EFI_SUCCESS) {
                    osind = (UINT64)*b;
                    if (osind & EFI_OS_INDICATIONS_BOOT_TO_FW_UI) {
                        FoundTool = TRUE;
                        TempMenuEntry = CopyMenuEntry(&MenuEntryFirmware);
                        TempMenuEntry->Image = BuiltinIcon(BUILTIN_ICON_FUNC_FIRMWARE);
                        AddMenuEntry(&MainMenu, TempMenuEntry);

                        #if REFIT_DEBUG > 0
                        MsgLog("  - Added %s Tool\n", ToolName);
                        #endif
                    } // if
                    MyFreePool(b);
                } // if

                if (FoundTool == FALSE) {
                    #if REFIT_DEBUG > 0
                    MsgLog("  - WARN: Could not Find %s Tool\n", ToolName);
                    #endif
                }

                break;

            case TAG_SHELL:
                j = 0;
                while ((FileName = FindCommaDelimited(SHELL_NAMES, j++)) != NULL) {
                    if (IsValidTool(SelfVolume, FileName)) {
                        FoundTool = TRUE;
                        AddToolEntry(
                            SelfVolume,
                            FileName,
                            L"EFI Shell",
                            BuiltinIcon(BUILTIN_ICON_TOOL_SHELL),
                            'S',
                            FALSE
                        );

                        #if REFIT_DEBUG > 0
                        MsgLog("  - Added %s : %s\n", ToolName, FileName);
                        #endif
                    } // if
                MyFreePool(FileName);
                } // while

                if (FoundTool == FALSE) {
                    #if REFIT_DEBUG > 0
                    MsgLog("  - WARN: Could not Find %s\n", ToolName);
                    #endif
                }

                break;

            case TAG_GPTSYNC:
                j = 0;
                while ((FileName = FindCommaDelimited(GPTSYNC_NAMES, j++)) != NULL) {
                    if (IsValidTool(SelfVolume, FileName)) {
                        FoundTool = TRUE;
                        AddToolEntry(
                            SelfVolume,
                            FileName,
                            L"Hybrid MBR tool",
                            BuiltinIcon(BUILTIN_ICON_TOOL_PART),
                            'P',
                            FALSE
                        );

                        #if REFIT_DEBUG > 0
                        MsgLog("  - Added %s Tool - %s\n", ToolName, FileName);
                        #endif
                    } // if

                    MyFreePool(FileName);
                } // while

                if (FoundTool == FALSE) {
                    #if REFIT_DEBUG > 0
                    MsgLog("  - WARN: Could not Find %s Tool\n", ToolName);
                    #endif
                }

                break;

            case TAG_GDISK:
                j = 0;
                while ((FileName = FindCommaDelimited(GDISK_NAMES, j++)) != NULL) {
                    if (IsValidTool(SelfVolume, FileName)) {
                        FoundTool = TRUE;
                        AddToolEntry(
                            SelfVolume,
                            FileName,
                            L"disk partitioning tool",
                            BuiltinIcon(BUILTIN_ICON_TOOL_PART),
                            'G',
                            FALSE
                        );


                     #if REFIT_DEBUG > 0
                     MsgLog("  - Added %s Tool - %s\n", ToolName, FileName);
                     #endif
                  } // if
                    MyFreePool(FileName);
                } // while

                if (FoundTool == FALSE) {
                    #if REFIT_DEBUG > 0
                    MsgLog("  - WARN: Could not Find %s Tool\n", ToolName);
                    #endif
                }

                break;

            case TAG_NETBOOT:
                j = 0;
                while ((FileName = FindCommaDelimited(NETBOOT_NAMES, j++)) != NULL) {
                    if (IsValidTool(SelfVolume, FileName)) {
                        FoundTool = TRUE;
                        AddToolEntry(
                            SelfVolume,
                            FileName,
                            L"Netboot",
                            BuiltinIcon(BUILTIN_ICON_TOOL_NETBOOT),
                            'N',
                            FALSE
                        );

                     #if REFIT_DEBUG > 0
                     MsgLog("  - Added %s Tool - %s\n", ToolName, FileName);
                     #endif
                    } // if
                    MyFreePool(FileName);
                } // while

                if (FoundTool == FALSE) {
                    #if REFIT_DEBUG > 0
                    MsgLog("  - WARN: Could not Find %s Tool\n", ToolName);
                    #endif
                }

                break;

            case TAG_APPLE_RECOVERY:
                for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                    j = 0;
                    while (
                        (FileName = FindCommaDelimited(GlobalConfig.MacOSRecoveryFiles, j++)) != NULL
                    ) {
                        if ((Volumes[VolumeIndex]->RootDir != NULL)) {
                            if ((IsValidTool(Volumes[VolumeIndex], FileName))) {
                                FoundTool = TRUE;
                                SPrint(
                                    Description,
                                    255,
                                    L"Apple Recovery on %s",
                                    Volumes[VolumeIndex]->VolName
                                );
                                AddToolEntry(
                                    Volumes[VolumeIndex],
                                    FileName,
                                    Description,
                                    BuiltinIcon(BUILTIN_ICON_TOOL_APPLE_RESCUE),
                                    'R',
                                    TRUE
                                );

                                #if REFIT_DEBUG > 0
                                MsgLog("  - Added %s Tool - %s\n", ToolName, FileName);
                                #endif
                            } // if
                        } // if
                    } // while
                } // for

                if (FoundTool == FALSE) {
                    #if REFIT_DEBUG > 0
                    MsgLog("  - WARN: Could not Find %s Tool\n", ToolName);
                    #endif
                }

                break;

            case TAG_WINDOWS_RECOVERY:
                j = 0;
                while (
                    (FileName = FindCommaDelimited(GlobalConfig.WindowsRecoveryFiles, j++)) != NULL
                ) {
                    SplitVolumeAndFilename(&FileName, &VolName);
                    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                        if ((Volumes[VolumeIndex]->RootDir != NULL) &&
                            (IsValidTool(Volumes[VolumeIndex], FileName)) &&
                            ((VolName == NULL) || MyStriCmp(VolName, Volumes[VolumeIndex]->VolName))
                        ) {
                            FoundTool = TRUE;
                            SPrint(
                                Description,
                                255,
                                L"Microsoft Recovery on %s",
                                Volumes[VolumeIndex]->VolName
                            );
                            AddToolEntry(
                                Volumes[VolumeIndex],
                                FileName,
                                Description,
                                BuiltinIcon(BUILTIN_ICON_TOOL_WINDOWS_RESCUE),
                                'R',
                                TRUE
                            );

                             #if REFIT_DEBUG > 0
                             MsgLog("  - Added %s Tool - %s\n", ToolName, FileName);
                             #endif
                        } // if
                    } // for
                } // while

                if (FoundTool == FALSE) {
                    #if REFIT_DEBUG > 0
                    MsgLog("  - WARN: Could not Find %s Tool\n", ToolName);
                    #endif
                }

                MyFreePool(FileName);
                MyFreePool(VolName);
                VolName = NULL;

                break;

            case TAG_MOK_TOOL:
                FoundTool = FindTool(
                    MokLocations,
                    MOK_NAMES,
                    L"MOK Utility",
                    BUILTIN_ICON_TOOL_MOK_TOOL
                );

                if (FoundTool == FALSE) {
                    #if REFIT_DEBUG > 0
                    MsgLog("  - WARN: Could not Find %s Tool\n", ToolName);
                    #endif
                }
                break;

            case TAG_FWUPDATE_TOOL:
                FoundTool = FindTool(
                    MokLocations,
                    FWUPDATE_NAMES,
                    L"Firmware Update Utility",
                    BUILTIN_ICON_TOOL_FWUPDATE
                );

                if (FoundTool == FALSE) {
                    #if REFIT_DEBUG > 0
                    MsgLog("  - WARN: Could not Find %s Tool\n", ToolName);
                    #endif
                }
                break;

            case TAG_CSR_ROTATE:
                if ((GetCsrStatus(&CsrValue) == EFI_SUCCESS) && (GlobalConfig.CsrValues)) {
                    FoundTool = TRUE;
                    TempMenuEntry = CopyMenuEntry(&MenuEntryRotateCsr);
                    TempMenuEntry->Image = BuiltinIcon(BUILTIN_ICON_FUNC_CSR_ROTATE);
                    AddMenuEntry(&MainMenu, TempMenuEntry);

                    #if REFIT_DEBUG > 0
                    MsgLog("  - Added %s Tool\n", ToolName);
                    #endif
                } // if

                if (FoundTool == FALSE) {
                    #if REFIT_DEBUG > 0
                    MsgLog("  - WARN: Could not Find %s Tool\n", ToolName);
                    #endif
                }

                break;

            case TAG_INSTALL:
                TempMenuEntry = CopyMenuEntry(&MenuEntryInstall);
                TempMenuEntry->Image = BuiltinIcon(BUILTIN_ICON_FUNC_INSTALL);
                AddMenuEntry(&MainMenu, TempMenuEntry);

                #if REFIT_DEBUG > 0
                MsgLog("  - Added %s\n", ToolName);
                #endif

                break;

            case TAG_BOOTORDER:
                TempMenuEntry = CopyMenuEntry(&MenuEntryBootorder);
                TempMenuEntry->Image = BuiltinIcon(BUILTIN_ICON_FUNC_BOOTORDER);
                AddMenuEntry(&MainMenu, TempMenuEntry);

                #if REFIT_DEBUG > 0
                MsgLog("  - Added %s\n", ToolName);
                #endif

                break;

            case TAG_MEMTEST:
                FoundTool = FindTool(
                    MEMTEST_LOCATIONS,
                    MEMTEST_NAMES,
                    L"Memory Test Utility",
                    BUILTIN_ICON_TOOL_MEMTEST
                );

                if (FoundTool == FALSE) {
                    #if REFIT_DEBUG > 0
                    MsgLog("  - WARN: Could not Find %s Tool\n", ToolName);
                    #endif
                }
                break;
        } // switch()
    } // for

    #if REFIT_DEBUG > 0
    MsgLog("Scanned Tool Types\n\n");
    #endif

} // VOID ScanForTools
