/*
 * refind/main.c
 * Main code for the boot menu
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
#include "launch_legacy.h"
#include "lib.h"
#include "icns.h"
#include "install.h"
#include "menu.h"
#include "mok.h"
#include "apple.h"
#include "mystrings.h"
#include "security_policy.h"
#include "driver_support.h"
#include "launch_efi.h"
#include "scan.h"
#include "../include/refit_call_wrapper.h"
#include "../include/version.h"
#include "../libeg/libeg.h"

INT16 NowZone   = 0;
INT16 NowYear   = 0;
INT16 NowMonth  = 0;
INT16 NowDay    = 0;
INT16 NowHour   = 0;
INT16 NowMinute = 0;
INT16 NowSecond = 0;

//
// Some built-in menu definitions....

REFIT_MENU_ENTRY MenuEntryReturn = {
    L"Return to Main Menu",
    TAG_RETURN,
    1, 0, 0,
    NULL, NULL, NULL
};

REFIT_MENU_SCREEN MainMenu = {
    L"Main Menu",
    NULL,
    0, NULL, 0,
    NULL, 0, L"Automatic boot",
    L"Use arrow keys to move cursor; Enter to boot;",
    L"Insert, Tab, or F2 for more options; Esc or Backspace to refresh"
};

static REFIT_MENU_SCREEN AboutMenu = {
    L"About RefindPlus",
    NULL,
    0, NULL, 0,
    NULL, 0, NULL,
    L"Press 'Enter' to return to main menu",
    L""
};

REFIT_MENU_ENTRY MenuEntryBootKicker = {
    L"Load BootKicker",
    TAG_SHOW_BOOTKICKER,
    1, 0, 0,
    NULL, NULL, NULL
};

REFIT_MENU_ENTRY MenuEntryCleanNvram = {
    L"Load CleanNvram",
    TAG_NVRAMCLEAN,
    1, 0, 0,
    NULL, NULL, NULL
};

REFIT_CONFIG GlobalConfig = {
    /* TextOnly = */ FALSE,
    /* ScanAllLinux = */ TRUE,
    /* DeepLegacyScan = */ FALSE,
    /* EnableAndLockVMX = */ FALSE,
    /* FoldLinuxKernels = */ TRUE,
    /* EnableMouse = */ FALSE,
    /* EnableTouch = */ FALSE,
    /* HiddenTags = */ TRUE,
    /* UseNvram = */ TRUE,
    /* TextRenderer = */ TRUE,
    /* UgaPassThrough = */ TRUE,
    /* ProvideConsoleGOP = */ TRUE,
    /* UseDirectGop = */ FALSE,
    /* ContinueOnWarning = */ FALSE,
    /* ShutdownAfterTimeout = */ FALSE,
    /* Install = */ FALSE,
    /* RequestedScreenWidth = */ 0,
    /* RequestedScreenHeight = */ 0,
    /* BannerBottomEdge = */ 0,
    /* RequestedTextMode = */ DONT_CHANGE_TEXT_MODE,
    /* Timeout = */ 20,
    /* HideUIFlags = */ 0,
    /* MaxTags = */ 0,
    /* GraphicsFor = */ GRAPHICS_FOR_OSX,
    /* LegacyType = */ LEGACY_TYPE_MAC,
    /* ScanDelay = */ 0,
    /* ScreensaverTime = */ 0,
    /* MouseSpeed = */ 4,
    /* IconSizes = */ {
        DEFAULT_BIG_ICON_SIZE / 4,
        DEFAULT_SMALL_ICON_SIZE,
        DEFAULT_BIG_ICON_SIZE,
        DEFAULT_MOUSE_SIZE
    },
    /* BannerScale = */ BANNER_NOSCALE,
    /* *DiscoveredRoot = */ NULL,
    /* *SelfDevicePath = */ NULL,
    /* *BannerFileName = */ NULL,
    /* *ScreenBackground = */ NULL,
    /* *ConfigFilename = */ CONFIG_FILE_NAME,
    /* *SelectionSmallFileName = */ NULL,
    /* *SelectionBigFileName = */ NULL,
    /* *DefaultSelection = */ NULL,
    /* *AlsoScan = */ NULL,
    /* *DontScanVolumes = */ NULL,
    /* *DontScanDirs = */ NULL,
    /* *DontScanFiles = */ NULL,
    /* *DontScanTools = */ NULL,
    /* *WindowsRecoveryFiles = */ NULL,
    /* *MacOSRecoveryFiles = */ NULL,
    /* *DriverDirs = */ NULL,
    /* *IconsDir = */ NULL,
    /* *ExtraKernelVersionStrings = */ NULL,
    /* *SpoofOSXVersion = */ NULL,
    /* CsrValues = */ NULL,
    /* ShowTools = */ {
        TAG_SHELL,
        TAG_MEMTEST,
        TAG_GDISK,
        TAG_APPLE_RECOVERY,
        TAG_WINDOWS_RECOVERY,
        TAG_MOK_TOOL,
        TAG_ABOUT,
        TAG_HIDDEN,
        TAG_SHUTDOWN,
        TAG_REBOOT,
        TAG_FIRMWARE,
        TAG_FWUPDATE_TOOL,
        0, 0, 0, 0, 0, 0, 0, 0, 0
    }
};

EFI_GUID RefindGuid = REFIND_GUID_VALUE;

#define BOOTKICKER_FILES L"\\EFI\\BOOT\\x64_tools\\x64_BootKicker.efi,\\EFI\\BOOT\\x64_tools\\BootKicker_x64.efi,\\EFI\\BOOT\\x64_tools\\BootKicker.efi,\\EFI\\tools_x64\\x64_BootKicker.efi,\\EFI\\tools_x64\\BootKicker_x64.efi,\\EFI\\tools_x64\\BootKicker.efi,\\EFI\\tools\\x64_BootKicker.efi,\\EFI\\tools\\BootKicker_x64.efi,\\EFI\\tools\\BootKicker.efi,\\EFI\\x64_BootKicker.efi,\\EFI\\BootKicker_x64.efi,\\EFI\\BootKicker.efi,\\x64_BootKicker.efi,\\BootKicker_x64.efi,\\BootKicker.efi"

#define NVRAMCLEAN_FILES L"\\EFI\\BOOT\\x64_tools\\x64_CleanNvram.efi,\\EFI\\BOOT\\x64_tools\\CleanNvram_x64.efi,\\EFI\\BOOT\\x64_tools\\CleanNvram.efi,\\EFI\\tools_x64\\x64_CleanNvram.efi,\\EFI\\tools_x64\\CleanNvram_x64.efi,\\EFI\\tools_x64\\CleanNvram.efi,\\EFI\\tools\\x64_CleanNvram.efi,\\EFI\\tools\\CleanNvram_x64.efi,\\EFI\\tools\\CleanNvram.efi,\\EFI\\x64_CleanNvram.efi,\\EFI\\CleanNvram_x64.efi,\\EFI\\CleanNvram.efi,\\x64_CleanNvram.efi,\\CleanNvram_x64.efi,\\CleanNvram.efi"

static BOOLEAN ranCleanNvram = FALSE;

//
// misc functions
//

// Checks to see if a specified file seems to be a valid tool.
// Returns TRUE if it passes all tests, FALSE otherwise
static
BOOLEAN
IsValidTool(
    IN REFIT_VOLUME *BaseVolume,
    CHAR16          *PathName
) {
    CHAR16  *DontVolName  = NULL;
    CHAR16  *DontPathName = NULL;
    CHAR16  *DontFileName = NULL;
    CHAR16  *DontScanThis;
    CHAR16  *TestVolName  = NULL;
    CHAR16  *TestPathName = NULL;
    CHAR16  *TestFileName = NULL;
    BOOLEAN retval        = TRUE;
    UINTN   i = 0;

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
    } else {
        retval = FALSE;
    }

    MyFreePool(TestVolName);
    MyFreePool(TestPathName);
    MyFreePool(TestFileName);

    return retval;
} // BOOLEAN IsValidTool()

VOID
preBootKicker(
    VOID
) {
    UINTN               MenuExit;
    INTN                DefaultEntry = 1;
    MENU_STYLE_FUNC     Style = GraphicsMenuStyle;
    REFIT_MENU_ENTRY    *ChosenEntry;
    REFIT_MENU_SCREEN BootKickerMenu = {
        L"BootKicker",
        NULL,
        0, NULL, 0,
        NULL, 0, NULL,
        L"Press 'ESC', 'BackSpace' or 'SpaceBar' to Return to Main Menu",
        L""
    };

    if (BootKickerMenu.EntryCount == 0) {
        BootKickerMenu.TitleImage = BuiltinIcon(BUILTIN_ICON_TOOL_BOOTKICKER);
        BootKickerMenu.Title = L"BootKicker";
        AddMenuInfoLine(&BootKickerMenu, L"A tool to kick in the Apple Boot Screen");
        AddMenuInfoLine(&BootKickerMenu, L"Needs GOP Capable Fully Compatible GPUs on Apple Firmware");
        AddMenuInfoLine(&BootKickerMenu, L"Fully Compatible GPUs provide native Mac Boot Screen");
        AddMenuInfoLine(&BootKickerMenu, L"Hangs and needs physical reboot with other GPUs");
        AddMenuInfoLine(&BootKickerMenu, L"");
        AddMenuInfoLine(&BootKickerMenu, L"BootKicker is from OpenCore and Copyright Acidanthera");
        AddMenuInfoLine(&BootKickerMenu, L"You must have at least one of the files below:");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\BOOT\\x64_tools\\x64_BootKicker.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\BOOT\\x64_tools\\BootKicker_x64.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\BOOT\\x64_tools\\BootKicker.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\tools_x64\\x64_BootKicker.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\tools_x64\\BootKicker_x64.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\tools_x64\\BootKicker.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\tools\\x64_BootKicker.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\tools\\BootKicker_x64.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\tools\\BootKicker.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\x64_BootKicker.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\BootKicker_x64.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\BootKicker.efi");
        AddMenuInfoLine(&BootKickerMenu, L"");
        AddMenuInfoLine(&BootKickerMenu, L"The first file found in the order listed will be used");
        AddMenuInfoLine(&BootKickerMenu, L"You will be returned to the main menu if not found");
        AddMenuInfoLine(&BootKickerMenu, L"");
        AddMenuInfoLine(&BootKickerMenu, L"");
        AddMenuInfoLine(&BootKickerMenu, L"x64_BootKicker.efi is distributed with 'MyBootMgr':");
        AddMenuInfoLine(&BootKickerMenu, L"https://forums.macrumors.com/threads/thread.2231693");
        AddMenuInfoLine(&BootKickerMenu, L"'MyBootMgr' is a preconfigured RefindPlus/Opencore Chainloader");
        AddMenuInfoLine(&BootKickerMenu, L"");
        AddMenuInfoLine(&BootKickerMenu, L"You can also get BootKicker.efi from the OpenCore Project:");
        AddMenuInfoLine(&BootKickerMenu, L"https://github.com/acidanthera/OpenCorePkg/releases");
        AddMenuInfoLine(&BootKickerMenu, L"");
        AddMenuInfoLine(&BootKickerMenu, L"");
        AddMenuEntry(&BootKickerMenu, &MenuEntryBootKicker);
        AddMenuEntry(&BootKickerMenu, &MenuEntryReturn);
    }

    MenuExit = RunGenericMenu(&BootKickerMenu, Style, &DefaultEntry, &ChosenEntry);

    if (ChosenEntry) {
        #if REFIT_DEBUG > 0
        MsgLog("Received User Input:\n");
        #endif

        if (MyStriCmp(ChosenEntry->Title, L"Load BootKicker") && (MenuExit == MENU_EXIT_ENTER)) {
            UINTN        i = 0;
            UINTN        k = 0;
            CHAR16       *Names          = BOOTKICKER_FILES;
            CHAR16       *FilePath       = NULL;
            CHAR16       *Description    = ChosenEntry->Title;
            BOOLEAN      FoundTool       = FALSE;
            LOADER_ENTRY *ourLoaderEntry = NULL;

            #if REFIT_DEBUG > 0
            // Log Load BootKicker
            MsgLog("  - Seek BootKicker\n");
            #endif

            k = 0;
                while ((FilePath = FindCommaDelimited(Names, k++)) != NULL) {

                    #if REFIT_DEBUG > 0
                    MsgLog("    * Seek %s:\n", FilePath);
                    #endif

                    i = 0;
                    for (i = 0; i < VolumesCount; i++) {
                        if ((Volumes[i]->RootDir != NULL) && (IsValidTool(Volumes[i], FilePath))) {
                            ourLoaderEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));

                            ourLoaderEntry->me.Title = Description;
                            ourLoaderEntry->me.Tag = TAG_SHOW_BOOTKICKER;
                            ourLoaderEntry->me.Row = 1;
                            ourLoaderEntry->me.ShortcutLetter = 'S';
                            ourLoaderEntry->me.Image = BuiltinIcon(BUILTIN_ICON_TOOL_BOOTKICKER);
                            ourLoaderEntry->LoaderPath = StrDuplicate(FilePath);
                            ourLoaderEntry->Volume = Volumes[i];
                            ourLoaderEntry->UseGraphicsMode = TRUE;

                            FoundTool = TRUE;
                            break;
                        } // if
                    } // for

                    if (FoundTool == TRUE) {
                        break;
                    }
                } // while Names
                MyFreePool(FilePath);

            if (FoundTool == TRUE) {
                #if REFIT_DEBUG > 0
                MsgLog("    ** Success: Found %s\n", FilePath);
                MsgLog("  - Load BootKicker\n\n");
                #endif

                // Run BootKicker
                StartTool(ourLoaderEntry);
                #if REFIT_DEBUG > 0
                MsgLog("WARN: BootKicker Error ...Return to Main Menu\n\n");
                #endif
            } else {
                #if REFIT_DEBUG > 0
                MsgLog("  - WARN: Could not Find BootKicker ...Return to Main Menu\n\n");
                #endif
            }
        } else {
            #if REFIT_DEBUG > 0
            // Log Return to Main Screen
            MsgLog("  - %s\n\n", ChosenEntry->Title);
            #endif
        } // if
    } else {
        #if REFIT_DEBUG > 0
        MsgLog("WARN: Could not Get User Input  ...Reload Main Menu\n\n");
        #endif
    } // if
} /* VOID preBootKicker() */

VOID
preCleanNvram(
    VOID
) {
    UINTN               MenuExit;
    INTN                DefaultEntry = 1;
    MENU_STYLE_FUNC     Style = GraphicsMenuStyle;
    REFIT_MENU_ENTRY    *ChosenEntry;
    REFIT_MENU_SCREEN CleanNvramMenu = {
        L"Clean Mac NVRAM",
        NULL,
        0, NULL, 0,
        NULL, 0, NULL,
        L"Press 'ESC', 'BackSpace' or 'SpaceBar' to Return to Main Menu",
        L""
    };

    if (CleanNvramMenu.EntryCount == 0) {
        CleanNvramMenu.TitleImage = BuiltinIcon(BUILTIN_ICON_TOOL_NVRAMCLEAN);
        CleanNvramMenu.Title = L"Clean Mac NVRAM";
        AddMenuInfoLine(&CleanNvramMenu, L"A Tool to Clean/Reset Nvram on Macs");
        AddMenuInfoLine(&CleanNvramMenu, L"Requires Apple Firmware");
        AddMenuInfoLine(&CleanNvramMenu, L"");
        AddMenuInfoLine(&CleanNvramMenu, L"CleanNvram is from OpenCore and Copyright Acidanthera");
        AddMenuInfoLine(&CleanNvramMenu, L"You must have at least one of the files below:");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\BOOT\\x64_tools\\x64_CleanNvram.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\BOOT\\x64_tools\\CleanNvram_x64.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\BOOT\\x64_tools\\CleanNvram.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\tools_x64\\x64_CleanNvram.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\tools_x64\\CleanNvram_x64.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\tools_x64\\CleanNvram.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\tools\\x64_CleanNvram.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\tools\\CleanNvram_x64.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\tools\\CleanNvram.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\x64_CleanNvram.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\CleanNvram_x64.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\CleanNvram.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"");
        AddMenuInfoLine(&CleanNvramMenu, L"The first file found in the order listed will be used");
        AddMenuInfoLine(&CleanNvramMenu, L"You will be returned to the main menu if not found");
        AddMenuInfoLine(&CleanNvramMenu, L"");
        AddMenuInfoLine(&CleanNvramMenu, L"");
        AddMenuInfoLine(&CleanNvramMenu, L"x64_CleanNvram.efi is distributed with 'MyBootMgr':");
        AddMenuInfoLine(&CleanNvramMenu, L"https://forums.macrumors.com/threads/thread.2231693");
        AddMenuInfoLine(&CleanNvramMenu, L"'MyBootMgr' is a preconfigured RefindPlus/Opencore Chainloader");
        AddMenuInfoLine(&CleanNvramMenu, L"");
        AddMenuInfoLine(&CleanNvramMenu, L"You can also get CleanNvram.efi from the OpenCore Project:");
        AddMenuInfoLine(&CleanNvramMenu, L"https://github.com/acidanthera/OpenCorePkg/releases");
        AddMenuInfoLine(&CleanNvramMenu, L"");
        AddMenuInfoLine(&CleanNvramMenu, L"");
        AddMenuEntry(&CleanNvramMenu, &MenuEntryCleanNvram);
        AddMenuEntry(&CleanNvramMenu, &MenuEntryReturn);
    }

    MenuExit = RunGenericMenu(&CleanNvramMenu, Style, &DefaultEntry, &ChosenEntry);

    if (ChosenEntry) {
        #if REFIT_DEBUG > 0
        MsgLog("Received User Input:\n");
        #endif

        if (MyStriCmp(ChosenEntry->Title, L"Load CleanNvram") && (MenuExit == MENU_EXIT_ENTER)) {
            UINTN        i = 0;
            UINTN        k = 0;
            CHAR16       *Names       = NVRAMCLEAN_FILES;
            CHAR16       *FilePath    = NULL;
            CHAR16       *Description = ChosenEntry->Title;
            BOOLEAN      FoundTool    = FALSE;
            LOADER_ENTRY *ourLoaderEntry   = NULL;

            #if REFIT_DEBUG > 0
            // Log Load CleanNvram
            MsgLog("  - Seek CleanNvram\n");
            #endif

            k = 0;
                while ((FilePath = FindCommaDelimited(Names, k++)) != NULL) {

                    #if REFIT_DEBUG > 0
                    MsgLog("    * Seek %s:\n", FilePath);
                    #endif

                    i = 0;
                    for (i = 0; i < VolumesCount; i++) {
                        if ((Volumes[i]->RootDir != NULL) && (IsValidTool(Volumes[i], FilePath))) {
                            ourLoaderEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));

                            ourLoaderEntry->me.Title = Description;
                            ourLoaderEntry->me.Tag = TAG_NVRAMCLEAN;
                            ourLoaderEntry->me.Row = 1;
                            ourLoaderEntry->me.ShortcutLetter = 'S';
                            ourLoaderEntry->me.Image = BuiltinIcon(BUILTIN_ICON_TOOL_NVRAMCLEAN);
                            ourLoaderEntry->LoaderPath = StrDuplicate(FilePath);
                            ourLoaderEntry->Volume = Volumes[i];
                            ourLoaderEntry->UseGraphicsMode = FALSE;

                            FoundTool = TRUE;
                            break;
                        } // if
                    } // for

                    if (FoundTool == TRUE) {
                        break;
                    }
                } // while Names
                MyFreePool(FilePath);

            if (FoundTool == TRUE) {
                #if REFIT_DEBUG > 0
                MsgLog("    ** Success: Found %s\n", FilePath);
                MsgLog("  - Load CleanNvram\n\n");
                #endif

                ranCleanNvram = TRUE;

                // Run CleanNvram
                StartTool(ourLoaderEntry);

            } else {
                #if REFIT_DEBUG > 0
                MsgLog("  - WARN: Could not Find CleanNvram ...Return to Main Menu\n\n");
                #endif
            }
        } else {
            #if REFIT_DEBUG > 0
            // Log Return to Main Screen
            MsgLog("  - %s\n\n", ChosenEntry->Title);
            #endif
        } // if
    } else {
        #if REFIT_DEBUG > 0
        MsgLog("WARN: Could not Get User Input  ...Reload Main Menu\n\n");
        #endif
    } // if
} /* VOID preCleanNvram() */


VOID AboutRefindPlus(VOID)
{
    CHAR16     *FirmwareVendor;
    UINT32     CsrStatus;

    if (AboutMenu.EntryCount == 0) {
        AboutMenu.TitleImage = BuiltinIcon(BUILTIN_ICON_FUNC_ABOUT);
        AddMenuInfoLine(&AboutMenu, PoolPrint(L"RefindPlus v%s", REFIND_VERSION));
        AddMenuInfoLine(&AboutMenu, L"");

        AddMenuInfoLine(&AboutMenu, L"Copyright (c) 2006-2010 Christoph Pfisterer");
        AddMenuInfoLine(&AboutMenu, L"Copyright (c) 2012-2020 Roderick W. Smith");
        AddMenuInfoLine(&AboutMenu, L"Copyright (c) 2020 Dayo Akanji");
        AddMenuInfoLine(&AboutMenu, L"Portions Copyright (c) Intel Corporation and others");
        AddMenuInfoLine(&AboutMenu, L"Distributed under the terms of the GNU GPLv3 license");
        AddMenuInfoLine(&AboutMenu, L"");
        AddMenuInfoLine(&AboutMenu, L"Running on:");
        AddMenuInfoLine(
            &AboutMenu,
            PoolPrint(
                L" EFI Revision %d.%02d",
                gST->Hdr.Revision >> 16,
                gST->Hdr.Revision & ((1 << 16) - 1)
            )
        );

        #if defined(EFI32)
        AddMenuInfoLine(
            &AboutMenu,
            PoolPrint(
                L" Platform: x86 (32 bit); Secure Boot %s",
                secure_mode() ? L"active" : L"inactive"
            )
        );
        #elif defined(EFIX64)
        AddMenuInfoLine(
            &AboutMenu,
            PoolPrint(
                L" Platform: x86_64 (64 bit); Secure Boot %s",
                secure_mode() ? L"active" : L"inactive"
            )
        );
        #elif defined(EFIAARCH64)
        AddMenuInfoLine(
            &AboutMenu,
            PoolPrint(
                L" Platform: ARM (64 bit); Secure Boot %s",
                secure_mode() ? L"active" : L"inactive"
            )
        );
        #else
        AddMenuInfoLine(&AboutMenu, L" Platform: unknown");
        #endif

        if (GetCsrStatus(&CsrStatus) == EFI_SUCCESS) {
            RecordgCsrStatus(CsrStatus, FALSE);
            AddMenuInfoLine(&AboutMenu, gCsrStatus);
        }

        FirmwareVendor = StrDuplicate(gST->FirmwareVendor);

        // More than ~65 causes empty info page on 800x600 display
        LimitStringLength(FirmwareVendor, MAX_LINE_LENGTH);

        AddMenuInfoLine(
            &AboutMenu,
            PoolPrint(
                L" Firmware: %s %d.%02d",
                FirmwareVendor,
                gST->FirmwareRevision >> 16,
                gST->FirmwareRevision & ((1 << 16) - 1)
            )
        );

        AddMenuInfoLine(&AboutMenu, PoolPrint(L" Screen Output: %s", egScreenDescription()));
        AddMenuInfoLine(&AboutMenu, L"");

        #if defined(__MAKEWITH_GNUEFI)
        AddMenuInfoLine(&AboutMenu, L"Built with GNU-EFI");
        #else
        AddMenuInfoLine(&AboutMenu, L"Built with TianoCore EDK2");
        #endif

        AddMenuInfoLine(&AboutMenu, L"");
        AddMenuInfoLine(&AboutMenu, L"RefindPlus is a variant of rEFInd");
        AddMenuInfoLine(&AboutMenu, L"Visit the project repository for more information:");
        AddMenuInfoLine(&AboutMenu, L"https://github.com/dakanji/RefindPlus/blob/GOPFix/README.md");
        AddMenuInfoLine(&AboutMenu, L"");
        AddMenuInfoLine(&AboutMenu, L"For information on rEFInd, visit:");
        AddMenuInfoLine(&AboutMenu, L"http://www.rodsbooks.com/refind");
        AddMenuEntry(&AboutMenu, &MenuEntryReturn);
        MyFreePool(FirmwareVendor);
    }

    RunMenu(&AboutMenu, NULL);
} /* VOID AboutRefindPlus() */

// Record the loader's name/description in the "PreviousBoot" EFI variable
// if different from what is already stored there.
VOID StoreLoaderName(IN CHAR16 *Name) {
    EFI_STATUS   Status;
    CHAR16       *OldName = NULL;
    UINTN        Length;

    if (Name) {
        Status = EfivarGetRaw(&RefindGuid, L"PreviousBoot", (CHAR8**) &OldName, &Length);
        if ((Status != EFI_SUCCESS) || (StrCmp(OldName, Name) != 0)) {
            EfivarSetRaw(&RefindGuid, L"PreviousBoot", (CHAR8*) Name, StrLen(Name) * 2 + 2, TRUE);
        } // if
        MyFreePool(OldName);
    } // if
} // VOID StoreLoaderName()

// Rescan for boot loaders
VOID RescanAll(BOOLEAN DisplayMessage, BOOLEAN Reconnect) {

    #if REFIT_DEBUG > 0
    MsgLog("Rescan All...\n");
    #endif

    FreeList((VOID ***) &(MainMenu.Entries), &MainMenu.EntryCount);
    MainMenu.Entries = NULL;
    MainMenu.EntryCount = 0;

    // ConnectAllDriversToAllControllers() can cause system hangs with some
    // buggy filesystem drivers, so do it only if necessary....
    if (Reconnect) {
        ConnectAllDriversToAllControllers();
        ScanVolumes();
    }

    ReadConfig(GlobalConfig.ConfigFilename);
    SetVolumeIcons();
    ScanForBootloaders(DisplayMessage);
    ScanForTools();
} // VOID RescanAll()

#ifdef __MAKEWITH_TIANO

// Minimal initialization function
static VOID InitializeLib(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
    gImageHandle   = ImageHandle;
    gST            = SystemTable;
    gBS            = SystemTable->BootServices;
    gRT            = SystemTable->RuntimeServices;
    EfiGetSystemConfigurationTable (&gEfiDxeServicesTableGuid, (VOID **) &gDS);
}

#endif

// Set up our own Secure Boot extensions....
// Returns TRUE on success, FALSE otherwise
static BOOLEAN SecureBootSetup(VOID) {
    EFI_STATUS Status;
    BOOLEAN    Success = FALSE;

    if (secure_mode() && ShimLoaded()) {
        Status = security_policy_install();
        if (Status == EFI_SUCCESS) {
            Success = TRUE;
        } else {
            Print(L"Failed to install MOK Secure Boot extensions");
            PauseForKey();
        }
    }
    return Success;
} // VOID SecureBootSetup()

// Remove our own Secure Boot extensions....
// Returns TRUE on success, FALSE otherwise
static BOOLEAN SecureBootUninstall(VOID) {
    EFI_STATUS   Status;
    BOOLEAN      Success = TRUE;
    const CHAR16 *ShowScreenStr = NULL;

    if (secure_mode()) {
        Status = security_policy_uninstall();
        if (Status != EFI_SUCCESS) {
            Success = FALSE;
            BeginTextScreen(L"Secure Boot Policy Failure");

            BOOLEAN OurTempBool = GlobalConfig.ContinueOnWarning;
            GlobalConfig.ContinueOnWarning = TRUE;

            ShowScreenStr = L"Failed to uninstall MOK Secure Boot extensions ...Forcing Reboot";

            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
            PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

            #if REFIT_DEBUG > 0
            MsgLog("%s\n---------------\n\n", ShowScreenStr);
            #endif

            PauseForKey();
            GlobalConfig.ContinueOnWarning = OurTempBool;

            refit_call4_wrapper(gRT->ResetSystem, EfiResetCold, EFI_SUCCESS, 0, NULL);
        }
    }
    return Success;
} // VOID SecureBootUninstall

// Sets the global configuration filename; will be CONFIG_FILE_NAME unless the
// "-c" command-line option is set, in which case that takes precedence.
// If an error is encountered, leaves the value alone (it should be set to
// CONFIG_FILE_NAME when GlobalConfig is initialized).
static VOID SetConfigFilename(EFI_HANDLE ImageHandle) {
    EFI_LOADED_IMAGE *Info;
    EFI_STATUS       Status;
    CHAR16           *Options;
    CHAR16           *FileName;
    CHAR16           *SubString;
    const CHAR16     *ShowScreenStr = NULL;

    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        ImageHandle,
        &LoadedImageProtocol,
        (VOID **) &Info
    );
    if ((Status == EFI_SUCCESS) && (Info->LoadOptionsSize > 0)) {
        #if REFIT_DEBUG > 0
        MsgLog("Setting Config Filename:\n");
        #endif

        Options = (CHAR16 *) Info->LoadOptions;
        SubString = MyStrStr(Options, L" -c ");
        if (SubString) {
            FileName = StrDuplicate(&SubString[4]);
            if (FileName) {
                LimitStringLength(FileName, 256);
            }

            if (FileExists(SelfDir, FileName)) {
                GlobalConfig.ConfigFilename = FileName;

                #if REFIT_DEBUG > 0
                MsgLog("  - Config File = %s\n\n", FileName);
                #endif

            } else {
                #if REFIT_DEBUG > 0
                MsgLog("  - WARN: Config File '%s' Not Found in '%s'\n", FileName, SelfDir);
                MsgLog("    * Try Default 'refind.conf'\n\n");
                #endif

                ShowScreenStr = L"Specified configuration file not found";
                PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
                ShowScreenStr = L"Try default 'refind.conf'";
                PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

                #if REFIT_DEBUG > 0
                MsgLog("%s\n\n", ShowScreenStr);
                #endif

                HaltForKey();
            } // if/else

            MyFreePool(FileName);
        } // if
        else {
            #if REFIT_DEBUG > 0
            MsgLog("  - ERROR : Invalid Load Option\n\n");
            #endif
        }
    } // if
} // VOID SetConfigFilename()

// Adjust the GlobalConfig.DefaultSelection variable: Replace all "+" elements with the
//  PreviousBoot variable, if it's available. If it's not available, delete that element.
static VOID AdjustDefaultSelection() {
    UINTN i = 0, j;
    CHAR16 *Element = NULL, *NewCommaDelimited = NULL, *PreviousBoot = NULL;
    EFI_STATUS Status;

    #if REFIT_DEBUG > 0
    MsgLog("Adjust Default Selection...\n");
    #endif

    while ((Element = FindCommaDelimited(GlobalConfig.DefaultSelection, i++)) != NULL) {
        if (MyStriCmp(Element, L"+")) {
            Status = EfivarGetRaw(&RefindGuid, L"PreviousBoot", (CHAR8 **) &PreviousBoot, &j);
            if (Status == EFI_SUCCESS) {
                MyFreePool(Element);
                Element = PreviousBoot;
            } else {
                Element = NULL;
            }
        } // if
        if (Element && StrLen(Element)) {
            MergeStrings(&NewCommaDelimited, Element, L',');
        } // if
        MyFreePool(Element);
    } // while
    MyFreePool(GlobalConfig.DefaultSelection);
    GlobalConfig.DefaultSelection = NewCommaDelimited;
} // AdjustDefaultSelection()

extern VOID InitBooterLog(VOID);
//
// main entry point
//
EFI_STATUS
EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS         Status;
    BOOLEAN            DriversLoaded   = FALSE;
    BOOLEAN            MainLoopRunning = TRUE;
    BOOLEAN            MokProtocol;
    REFIT_MENU_ENTRY   *ChosenEntry;
    LOADER_ENTRY       *ourLoaderEntry;
    LEGACY_ENTRY       *ourLegacyEntry;
    UINTN              MenuExit;
    UINTN              i;
    CHAR16             *SelectionName = NULL;
    EG_PIXEL           BGColor        = COLOR_LIGHTBLUE;
    const CHAR16       *ShowScreenStr = NULL;

    // bootstrap
    InitializeLib(ImageHandle, SystemTable);
    Status = InitRefitLib(ImageHandle);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EFI_TIME Now;
    SystemTable->RuntimeServices->GetTime(&Now, NULL);
    NowYear   = Now.Year;
    NowMonth  = Now.Month;
    NowDay    = Now.Day;
    NowHour   = Now.Hour;
    NowMinute = Now.Minute;
    NowSecond = Now.Second;
    NowZone   = (Now.TimeZone / 60);

    CHAR16  NowDateStr[40]; // sizeof(L"0000-00-00 00:00:00") = 40
    SPrint(
        NowDateStr,
        40,
        L"%d-%02d-%02d %02d:%02d:%02d",
        NowYear,
        NowMonth,
        NowDay,
        NowHour,
        NowMinute,
        NowSecond
    );

    InitBooterLog();

    #if REFIT_DEBUG > 0
    MsgLog("Loading RefindPlus v%s on %s Firmware\n", REFIND_VERSION, gST->FirmwareVendor);
    if (NowZone < -12 || NowZone > 12 || (NowZone > -1 && NowZone < 1)) {
        MsgLog("Timestamp: %s (GMT)\n\n", NowDateStr);
    }
    else if (NowZone < 0) {
        MsgLog("Timestamp: %s (GMT%02d)\n\n", NowDateStr);
    }
    else {
        MsgLog("Timestamp: %s (GMT+%02d)\n\n", NowDateStr);
    }
    #endif

    // read configuration
    CopyMem(GlobalConfig.ScanFor, "ieom      ", NUM_SCAN_OPTIONS);
    FindLegacyBootType();
    if (GlobalConfig.LegacyType == LEGACY_TYPE_MAC) {
        CopyMem(GlobalConfig.ScanFor, "ihebocm   ", NUM_SCAN_OPTIONS);
    }
    SetConfigFilename(ImageHandle);
    MokProtocol = SecureBootSetup();

    // Scan volumes first to find SelfVolume, which is required by LoadDrivers() and ReadConfig();
    // however, if drivers are loaded, a second call to ScanVolumes() is needed
    // to register the new filesystem(s) accessed by the drivers.
    // Also, ScanVolumes() must be done before ReadConfig(), which needs
    // SelfVolume->VolName.
    #if REFIT_DEBUG > 0
    MsgLog("Scan for Self Volume...\n");
    #endif
    ScanVolumes();

    // Read Config first to get tokens that may be required by LoadDrivers();
    ReadConfig(GlobalConfig.ConfigFilename);

    DriversLoaded = LoadDrivers();
    if (DriversLoaded) {
        #if REFIT_DEBUG > 0
        MsgLog("Scan Volumes...\n");
        #endif
        ScanVolumes();
    }

    AdjustDefaultSelection();

    if (GlobalConfig.SpoofOSXVersion && GlobalConfig.SpoofOSXVersion[0] != L'\0') {
        #if REFIT_DEBUG > 0
        MsgLog("Spoof Mac OS Version...\n");
        #endif
        SetAppleOSInfo();
    }

    #if REFIT_DEBUG > 0
    MsgLog("Initialise Screen...\n");
    #endif
    InitScreen();

    WarnIfLegacyProblems();
    MainMenu.TimeoutSeconds = GlobalConfig.Timeout;
    // disable EFI watchdog timer
    refit_call4_wrapper(gBS->SetWatchdogTimer, 0x0000, 0x0000, 0x0000, NULL);

    // further bootstrap (now with config available)
    SetupScreen();
    SetVolumeIcons();
    ScanForBootloaders(FALSE);
    ScanForTools();
    // SetupScreen() clears the screen; but ScanForBootloaders() may display a
    // message that must be deleted, so do so
    BltClearScreen(TRUE);
    pdInitialize();

    if (GlobalConfig.ScanDelay > 0) {
       if (GlobalConfig.ScanDelay > 1) {
           egDisplayMessage(L"Pausing before disc scan. Please wait....", &BGColor, CENTER);
       }

       #if REFIT_DEBUG > 0
       MsgLog("Pause for Scan Delay:\n");
       #endif

       for (i = -1; i < GlobalConfig.ScanDelay; ++i) {
            refit_call1_wrapper(gBS->Stall, 1000000);
       }
       if (i == 1) {
           #if REFIT_DEBUG > 0
           MsgLog("  - Waited %d Second\n", i);
           #endif
       } else {
           #if REFIT_DEBUG > 0
           MsgLog("  - Waited %d Seconds\n", i);
           #endif
       }
       RescanAll(GlobalConfig.ScanDelay > 1, TRUE);
       BltClearScreen(TRUE);
    } // if

    if (GlobalConfig.DefaultSelection) {
        SelectionName = StrDuplicate(GlobalConfig.DefaultSelection);
    }
    if (GlobalConfig.ShutdownAfterTimeout) {
        MainMenu.TimeoutText = L"Shutdown";
    }

    #if REFIT_DEBUG > 0
    MsgLog(
        "INFO: Loaded RefindPlus v%s ...Awaiting User Input\n\n",
        REFIND_VERSION
    );
    #endif

    while (MainLoopRunning) {
        ourLoaderEntry = NULL;

        MenuExit = RunMainMenu(&MainMenu, &SelectionName, &ChosenEntry);

        // The Escape key triggers a re-scan operation....
        if (MenuExit == MENU_EXIT_ESCAPE) {
            MenuExit = 0;

            #if REFIT_DEBUG > 0
            MsgLog("Received User Input:\n");
            MsgLog("  - Escape Key Pressed ...Rescan All\n\n");
            #endif

            RescanAll(TRUE, TRUE);
            continue;
        }

        if ((MenuExit == MENU_EXIT_TIMEOUT) && GlobalConfig.ShutdownAfterTimeout) {
            ChosenEntry->Tag = TAG_SHUTDOWN;
        }

        switch (ChosenEntry->Tag) {

            case TAG_NVRAMCLEAN:    // Clean NVRAM

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                if (egIsGraphicsModeEnabled()) {
                    MsgLog("  - Clean NVRAM\n---------------\n\n");
                }
                else {
                    MsgLog("  - Clean NVRAM\n\n");
                }
                #endif

                StartTool((LOADER_ENTRY *) ChosenEntry);
                break;

            case TAG_PRE_NVRAMCLEAN:    // Clean NVRAM Info

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                MsgLog("  - Show Clean NVRAM Info\n\n");
                #endif

                preCleanNvram();

                // Reboot if CleanNvram was triggered
                if(ranCleanNvram == TRUE) {
                    #if REFIT_DEBUG > 0
                    MsgLog("INFO: Cleaned Nvram\n\n");
                    MsgLog("Restart Computer...\n");
                    MsgLog("Terminating Screen:\n");
                    #endif
                    TerminateScreen();

                    #if REFIT_DEBUG > 0
                    MsgLog("Reseting System\n---------------\n\n");
                    #endif
                    refit_call4_wrapper(gRT->ResetSystem, EfiResetCold, EFI_SUCCESS, 0, NULL);

                    ShowScreenStr = L"INFO: Computer Reboot Failed ...Attempt Fallback:.";
                    PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

                    #if REFIT_DEBUG > 0
                    MsgLog("%s\n\n", ShowScreenStr);
                    #endif

                    PauseForKey();

                    MainLoopRunning = FALSE;   // just in case we get this far
                }
                break;

            case TAG_SHOW_BOOTKICKER:    // Apple Boot Screen

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                if (egIsGraphicsModeEnabled()) {
                    MsgLog("  - Load Boot Screen\n---------------\n\n");
                }
                else {
                    MsgLog("  - Load Boot Screen\n\n");
                }
                #endif

                ourLoaderEntry = (LOADER_ENTRY *) ChosenEntry;
                ourLoaderEntry->UseGraphicsMode = TRUE;

                StartTool(ourLoaderEntry);
                break;

            case TAG_PRE_BOOTKICKER:    // Apple Boot Screen Info

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                MsgLog("  - Show BootKicker Info\n\n");
                #endif

                preBootKicker();
                break;

            case TAG_REBOOT:    // Reboot

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                if (egIsGraphicsModeEnabled()) {
                    MsgLog("  - Restart Computer\n---------------\n\n");
                }
                else {
                    MsgLog("  - Restart Computer\n\n");
                }
                #endif

                TerminateScreen();
                refit_call4_wrapper(gRT->ResetSystem, EfiResetCold, EFI_SUCCESS, 0, NULL);
                MainLoopRunning = FALSE;   // just in case we get this far
                break;

            case TAG_SHUTDOWN: // Shut Down

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                if (egIsGraphicsModeEnabled()) {
                    MsgLog("  - Shut Computer Down\n---------------\n\n");
                }
                else {
                    MsgLog("  - Shut Computer Down\n\n");
                }
                #endif

                TerminateScreen();
                refit_call4_wrapper(gRT->ResetSystem, EfiResetShutdown, EFI_SUCCESS, 0, NULL);
                MainLoopRunning = FALSE;   // just in case we get this far
                break;

            case TAG_ABOUT:    // About RefindPlus

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                MsgLog("  - Show 'About RefindPlus' Page\n\n");
                #endif

                AboutRefindPlus();
                break;

            case TAG_LOADER:   // Boot OS via .EFI loader
                ourLoaderEntry = (LOADER_ENTRY *) ChosenEntry;

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");

                if (MyStrStr(ourLoaderEntry->Title, L"OpenCore") != NULL) {
                    MsgLog(
                        "  - Load OpenCore Instance : '%s%s'",
                        ourLoaderEntry->Volume->VolName,
                        ourLoaderEntry->LoaderPath
                    );
                }
                else if (MyStrStr(ourLoaderEntry->Title, L"Mac OS") != NULL ||
                    MyStrStr(ourLoaderEntry->Title, L"macOS") != NULL
                ) {
                    if (ourLoaderEntry->Volume->VolName) {
                        MsgLog("  - Boot Mac OS from '%s'", ourLoaderEntry->Volume->VolName);
                    }
                    else {
                        MsgLog("  - Boot Mac OS : '%s'", ourLoaderEntry->LoaderPath);
                    }
                }
                else if (MyStrStr(ourLoaderEntry->Title, L"Windows") != NULL) {
                    if (ourLoaderEntry->Volume->VolName) {
                        MsgLog("  - Boot Windows from '%s'", ourLoaderEntry->Volume->VolName);
                    }
                    else {
                        MsgLog("  - Boot Windows : '%s'", ourLoaderEntry->LoaderPath);
                    }
                }
                else {
                    MsgLog(
                        "  - Boot OS via EFI Loader : '%s%s'",
                        ourLoaderEntry->Volume->VolName,
                        ourLoaderEntry->LoaderPath
                    );
                }

                if (egIsGraphicsModeEnabled()) {
                    MsgLog("\n---------------\n\n");
                }
                else {
                    MsgLog("\n\n");
                }
                #endif

                if (!GlobalConfig.TextOnly || MyStrStr(ourLoaderEntry->Title, L"OpenCore") != NULL) {
                    ourLoaderEntry->UseGraphicsMode = TRUE;
                }
                StartLoader(ourLoaderEntry, SelectionName);
                break;

            case TAG_LEGACY:   // Boot legacy OS
                ourLegacyEntry = (LEGACY_ENTRY *) ChosenEntry;

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                if (MyStrStr(ourLegacyEntry->Volume->OSName, L"Windows") != NULL) {
                    MsgLog(
                        "  - Boot %s from '%s'",
                        ourLegacyEntry->Volume->OSName,
                        ourLegacyEntry->Volume->VolName
                    );
                }
                else {
                    MsgLog(
                        "  - Boot Legacy OS : '%s'",
                        ourLegacyEntry->Volume->OSName
                    );
                }

                if (egIsGraphicsModeEnabled()) {
                    MsgLog("\n---------------\n\n");
                }
                else {
                    MsgLog("\n\n");
                }
                #endif

                StartLegacy(ourLegacyEntry, SelectionName);
                break;

            case TAG_LEGACY_UEFI: // Boot a legacy OS on a non-Mac
                ourLegacyEntry = (LEGACY_ENTRY *) ChosenEntry;

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                if (egIsGraphicsModeEnabled()) {
                    MsgLog("  - Boot Legacy UEFI : '%s'\n---------------\n\n", ourLegacyEntry->Volume->OSName);
                }
                else {
                    MsgLog("  - Boot Legacy UEFI : '%s'\n\n", ourLegacyEntry->Volume->OSName);
                }
                #endif

                StartLegacyUEFI(ourLegacyEntry, SelectionName);
                break;

            case TAG_TOOL:     // Start a EFI tool
                ourLoaderEntry = (LOADER_ENTRY *) ChosenEntry;

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                MsgLog("  - Start EFI Tool : '%s'\n\n", ourLoaderEntry->LoaderPath);
                #endif

                if (MyStrStr(ourLoaderEntry->Title, L"Boot Screen") != NULL) {
                    ourLoaderEntry->UseGraphicsMode = TRUE;
                }

                StartTool(ourLoaderEntry);
                break;

            case TAG_HIDDEN:  // Manage hidden tag entries

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                MsgLog("  - Manage Hidden Tag Entries\n\n");
                #endif

                ManageHiddenTags();
                break;

            case TAG_EXIT:    // Terminate RefindPlus

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                if (egIsGraphicsModeEnabled()) {
                    MsgLog("  - Terminate RefindPlus\n---------------\n\n");
                }
                else {
                    MsgLog("  - Terminate RefindPlus\n\n");
                }
                #endif

                if ((MokProtocol) && !SecureBootUninstall()) {
                   MainLoopRunning = FALSE;   // just in case we get this far
                }
                else {
                   BeginTextScreen(L" ");
                   return EFI_SUCCESS;
                }
                break;

            case TAG_FIRMWARE: // Reboot into firmware's user interface

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                if (egIsGraphicsModeEnabled()) {
                    MsgLog("  - Reboot into Firmware\n---------------\n\n");
                }
                else {
                    MsgLog("  - Reboot into Firmware\n\n");
                }
                #endif

                RebootIntoFirmware();
                break;

            case TAG_CSR_ROTATE:

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                MsgLog("  - Rotate CSR Values\n\n");
                #endif

                RotateCsrValue();
                break;

            case TAG_INSTALL:

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                if (egIsGraphicsModeEnabled()) {
                    MsgLog("  - Install RefindPlus\n---------------\n\n");
                }
                else {
                    MsgLog("  - Install RefindPlus\n\n");
                }
                #endif

                InstallRefind();
                break;

            case TAG_BOOTORDER:

                #if REFIT_DEBUG > 0
                MsgLog("Received User Input:\n");
                MsgLog("  - Manage Boot Order\n\n");
                #endif

                ManageBootorder();
                break;

        } // switch()
    } // while()
    MyFreePool(SelectionName);

    // If we end up here, things have gone wrong. Try to reboot, and if that
    // fails, go into an endless loop.
    #if REFIT_DEBUG > 0
    MsgLog("Fallback: Restart Computer...\n");
    MsgLog("Screen Termination:\n");
    #endif
    TerminateScreen();

    #if REFIT_DEBUG > 0
    MsgLog("System Reset:\n\n");
    #endif
    refit_call4_wrapper(gRT->ResetSystem, EfiResetCold, EFI_SUCCESS, 0, NULL);

    SwitchToText(FALSE);

    ShowScreenStr = L"INFO: Reboot Failed ...Entering Endless Idle Loop";

    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

    #if REFIT_DEBUG > 0
    MsgLog("%s\n---------------\n\n", ShowScreenStr);
    #endif

    PauseForKey();
    EndlessIdleLoop();

    return EFI_SUCCESS;
} /* efi_main() */
