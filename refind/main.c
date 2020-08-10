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
    L"About",
    NULL,
    0, NULL, 0,
    NULL, 0, NULL,
    L"Press Enter to return to main menu",
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
    /* TextRenderer = */ FALSE,
    /* UgaPassThrough = */ TRUE,
    /* ProvideConsoleGOP = */ TRUE,
    /* UseDirectGop = */ TRUE,
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

#define BOOTKICKER_FILES L"\\EFI\\BOOT\\tools_x64\\BootKicker_x64.efi,\\EFI\\BOOT\\tools_x64\\BootKicker.efi,\\EFI\\tools_x64\\BootKicker_x64.efi,\\EFI\\tools_x64\\BootKicker.efi,\\EFI\\tools\\BootKicker_x64.efi,\\EFI\\tools\\BootKicker.efi,\\EFI\\BootKicker_x64.efi,\\EFI\\BootKicker.efi,\\BootKicker_x64.efi,\\BootKicker.efi"

#define NVRAMCLEAN_FILES L"\\EFI\\BOOT\\tools_x64\\CleanNvram_x64.efi,\\EFI\\BOOT\\tools_x64\\CleanNvram.efi,\\EFI\\tools_x64\\CleanNvram_x64.efi,\\EFI\\tools_x64\\CleanNvram.efi,\\EFI\\tools\\CleanNvram_x64.efi,\\EFI\\tools\\CleanNvram.efi,\\EFI\\CleanNvram_x64.efi,\\EFI\\CleanNvram.efi,\\CleanNvram_x64.efi,\\CleanNvram.efi"

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
        AddMenuInfoLine(&BootKickerMenu, L"Needs GOP Compatible Official GPUs on Apple Firmware");
        AddMenuInfoLine(&BootKickerMenu, L"Official GPUs are those With Native Apple Boot Screen");
        AddMenuInfoLine(&BootKickerMenu, L"Hangs and requires physical reboot with Custom GPUs");
        AddMenuInfoLine(&BootKickerMenu, L"");
        AddMenuInfoLine(&BootKickerMenu, L"BootKicker is from OpenCore and Copyright Acidanthera");
        AddMenuInfoLine(&BootKickerMenu, L"You must have at least one of the files below:");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\BOOT\\tools_x64\\BootKicker_x64.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\BOOT\\tools_x64\\BootKicker.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\tools_x64\\BootKicker_x64.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\tools_x64\\BootKicker.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\tools\\BootKicker_x64.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\tools\\BootKicker.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\BootKicker_x64.efi");
        AddMenuInfoLine(&BootKickerMenu, L"\\EFI\\BootKicker.efi");
        AddMenuInfoLine(&BootKickerMenu, L"");
        AddMenuInfoLine(&BootKickerMenu, L"The first file found in the order listed will be used");
        AddMenuInfoLine(&BootKickerMenu, L"You will be returned to the main menu if none is Found");
        AddMenuInfoLine(&BootKickerMenu, L"");
        AddMenuInfoLine(&BootKickerMenu, L"");
        AddMenuInfoLine(&BootKickerMenu, L"BootKicker_x64.efi is distributed with 'MyBootMgr':");
        AddMenuInfoLine(&BootKickerMenu, L"https://forums.macrumors.com/threads/thread.2231693");
        AddMenuInfoLine(&BootKickerMenu, L"'MyBootMgr' is a preconfigured rEFInd/Opencore Chainloader");
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
        MsgLog("Get User Input:\n");
        #endif

        if (MyStriCmp(ChosenEntry->Title, L"Load BootKicker") && (MenuExit == MENU_EXIT_ENTER)) {
            UINTN        i = 0;
            UINTN        k = 0;
            CHAR16       *Names       = BOOTKICKER_FILES;
            CHAR16       *FilePath    = NULL;
            CHAR16       *Description = ChosenEntry->Title;
            BOOLEAN      FoundTool    = FALSE;
            LOADER_ENTRY *TempEntry   = NULL;

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
                            TempEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));

                            TempEntry->me.Title = Description;
                            TempEntry->me.Tag = TAG_SHOW_BOOTKICKER;
                            TempEntry->me.Row = 1;
                            TempEntry->me.ShortcutLetter = 'S';
                            TempEntry->me.Image = BuiltinIcon(BUILTIN_ICON_TOOL_BOOTKICKER);
                            TempEntry->LoaderPath = StrDuplicate(FilePath);
                            TempEntry->Volume = Volumes[i];
                            TempEntry->UseGraphicsMode = TRUE;

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
                if (egHasGraphicsMode()) {
                    MsgLog("  - Load BootKicker\n------------\n\n");
                } else {
                    MsgLog("  - Load BootKicker\n\n");
                }
                #endif

                // Run BootKicker
                StartTool(TempEntry);
            } else {
                #if REFIT_DEBUG > 0
                MsgLog(
                    "  - Could not Find BootKicker ...Return to Main Menu\n\n",
                    ChosenEntry->Title
                );
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
        MsgLog("Could not Get User Input  ...Return to Main Menu\n\n");
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
        L"Clean NVRAM",
        NULL,
        0, NULL, 0,
        NULL, 0, NULL,
        L"Press 'ESC', 'BackSpace' or 'SpaceBar' to Return to Main Menu",
        L""
    };

    if (CleanNvramMenu.EntryCount == 0) {
        CleanNvramMenu.TitleImage = BuiltinIcon(BUILTIN_ICON_TOOL_NVRAMCLEAN);
        CleanNvramMenu.Title = L"Clean NVRAM";
        AddMenuInfoLine(&CleanNvramMenu, L"A tool to clean/reset nvram");
        AddMenuInfoLine(&CleanNvramMenu, L"Requires Apple Firmware");
        AddMenuInfoLine(&CleanNvramMenu, L"");
        AddMenuInfoLine(&CleanNvramMenu, L"CleanNvram is from OpenCore and Copyright Acidanthera");
        AddMenuInfoLine(&CleanNvramMenu, L"You must have at least one of the files below:");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\BOOT\\tools_x64\\CleanNvram_x64.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\BOOT\\tools_x64\\CleanNvram.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\tools_x64\\CleanNvram_x64.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\tools_x64\\CleanNvram.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\tools\\CleanNvram_x64.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\tools\\CleanNvram.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\CleanNvram_x64.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"\\EFI\\CleanNvram.efi");
        AddMenuInfoLine(&CleanNvramMenu, L"");
        AddMenuInfoLine(&CleanNvramMenu, L"The first file found in the order listed will be used");
        AddMenuInfoLine(&CleanNvramMenu, L"You will be returned to the main menu if none is Found");
        AddMenuInfoLine(&CleanNvramMenu, L"");
        AddMenuInfoLine(&CleanNvramMenu, L"");
        AddMenuInfoLine(&CleanNvramMenu, L"CleanNvram_x64.efi is distributed with 'MyBootMgr':");
        AddMenuInfoLine(&CleanNvramMenu, L"https://forums.macrumors.com/threads/thread.2231693");
        AddMenuInfoLine(&CleanNvramMenu, L"'MyBootMgr' is a preconfigured rEFInd/Opencore Chainloader");
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
        MsgLog("Get User Input:\n");
        #endif

        if (MyStriCmp(ChosenEntry->Title, L"Load CleanNvram") && (MenuExit == MENU_EXIT_ENTER)) {
            UINTN        i = 0;
            UINTN        k = 0;
            CHAR16       *Names       = NVRAMCLEAN_FILES;
            CHAR16       *FilePath    = NULL;
            CHAR16       *Description = ChosenEntry->Title;
            BOOLEAN      FoundTool    = FALSE;
            LOADER_ENTRY *TempEntry   = NULL;

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
                            TempEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));

                            TempEntry->me.Title = Description;
                            TempEntry->me.Tag = TAG_NVRAMCLEAN;
                            TempEntry->me.Row = 1;
                            TempEntry->me.ShortcutLetter = 'S';
                            TempEntry->me.Image = BuiltinIcon(BUILTIN_ICON_TOOL_NVRAMCLEAN);
                            TempEntry->LoaderPath = StrDuplicate(FilePath);
                            TempEntry->Volume = Volumes[i];
                            TempEntry->UseGraphicsMode = TRUE;

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
                StartTool(TempEntry);

            } else {
                #if REFIT_DEBUG > 0
                MsgLog(
                    "  - Could not Find CleanNvram ...Return to Main Menu\n\n",
                    ChosenEntry->Title
                );
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
        MsgLog("Could not Get User Input  ...Return to Main Menu\n\n");
        #endif
    } // if
} /* VOID preCleanNvram() */


VOID AboutrEFInd(VOID)
{
    CHAR16     *FirmwareVendor;
    UINT32     CsrStatus;

    if (AboutMenu.EntryCount == 0) {
        AboutMenu.TitleImage = BuiltinIcon(BUILTIN_ICON_FUNC_ABOUT);
        AddMenuInfoLine(&AboutMenu, PoolPrint(L"rEFInd Version %s", REFIND_VERSION));
        AddMenuInfoLine(&AboutMenu, L"");
        AddMenuInfoLine(&AboutMenu, L"Copyright (c) 2006-2010 Christoph Pfisterer");
        AddMenuInfoLine(&AboutMenu, L"Copyright (c) 2012-2020 Roderick W. Smith");
        AddMenuInfoLine(&AboutMenu, L"Portions Copyright (c) Intel Corporation and others");
        AddMenuInfoLine(&AboutMenu, L"Distributed under the terms of the GNU GPLv3 license");
        AddMenuInfoLine(&AboutMenu, L"");
        AddMenuInfoLine(&AboutMenu, L"Running on:");
        AddMenuInfoLine(
            &AboutMenu,
            PoolPrint(
                L" EFI Revision %d.%02d",
                ST->Hdr.Revision >> 16,
                ST->Hdr.Revision & ((1 << 16) - 1)
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

        FirmwareVendor = StrDuplicate(ST->FirmwareVendor);

        // More than ~65 causes empty info page on 800x600 display
        LimitStringLength(FirmwareVendor, MAX_LINE_LENGTH);

        AddMenuInfoLine(
            &AboutMenu,
            PoolPrint(
                L" Firmware: %s %d.%02d",
                FirmwareVendor,
                ST->FirmwareRevision >> 16,
                ST->FirmwareRevision & ((1 << 16) - 1)
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
        AddMenuInfoLine(&AboutMenu, L"For more information, see the rEFInd Web site:");
        AddMenuInfoLine(&AboutMenu, L"http://www.rodsbooks.com/refind/");
        AddMenuEntry(&AboutMenu, &MenuEntryReturn);
        MyFreePool(FirmwareVendor);
    }

    RunMenu(&AboutMenu, NULL);
} /* VOID AboutrEFInd() */

// Record the value of the loader's name/description in rEFInd's "PreviousBoot" EFI variable,
// if it's different from what's already stored there.
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

    #if REFIT_DEBUG > 0
    MsgLog("Init System Lib...\n");
    #endif

    gST            = SystemTable;
    //    gImageHandle   = ImageHandle;
    gBS            = SystemTable->BootServices;
    //    gRS            = SystemTable->RuntimeServices;
    gRT = SystemTable->RuntimeServices; // Some BDS functions need gRT to be set
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
    EFI_STATUS Status;
    BOOLEAN    Success = TRUE;

    if (secure_mode()) {
        Status = security_policy_uninstall();
        if (Status != EFI_SUCCESS) {
            Success = FALSE;
            BeginTextScreen(L"Secure Boot Policy Failure");
            Print(L"Failed to uninstall MOK Secure Boot extensions; forcing a reboot.");
            PauseForKey();
            refit_call4_wrapper(RT->ResetSystem, EfiResetCold, EFI_SUCCESS, 0, NULL);
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
    CHAR16 *Options, *FileName, *SubString;
    EFI_STATUS Status;

        #if REFIT_DEBUG > 0
        MsgLog("Set Config Filename...\n");
        #endif

    Status = refit_call3_wrapper(BS->HandleProtocol, ImageHandle, &LoadedImageProtocol, (VOID **) &Info);
    if ((Status == EFI_SUCCESS) && (Info->LoadOptionsSize > 0)) {
        Options = (CHAR16 *) Info->LoadOptions;
        SubString = MyStrStr(Options, L" -c ");
        if (SubString) {
            FileName = StrDuplicate(&SubString[4]);
            if (FileName) {
                LimitStringLength(FileName, 256);
            }

            if (FileExists(SelfDir, FileName)) {
                GlobalConfig.ConfigFilename = FileName;
            } else {
                Print(L"Specified configuration file (%s) doesn't exist; using\n'refind.conf' default\n", FileName);
                MyFreePool(FileName);
            } // if/else
        } // if
    } // if
} // VOID SetConfigFilename()

// Adjust the GlobalConfig.DefaultSelection variable: Replace all "+" elements with the
// rEFInd PreviousBoot variable, if it's available. If it's not available, delete that
// element.
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
    BOOLEAN            MainLoopRunning = TRUE;
    BOOLEAN            MokProtocol;
    REFIT_MENU_ENTRY   *ChosenEntry;
    LOADER_ENTRY       *TempEntry;
    UINTN              MenuExit, i;
    CHAR16             *SelectionName = NULL;
    EG_PIXEL           BGColor = COLOR_LIGHTBLUE;

    // bootstrap
    InitializeLib(ImageHandle, SystemTable);
    Status = InitRefitLib(ImageHandle);
    if (EFI_ERROR(Status))
        return Status;

    InitBooterLog();
    EFI_TIME          Now;
    SystemTable->RuntimeServices->GetTime(&Now, NULL);

#if REFIT_DEBUG > 0
    MsgLog("Start rEFInd on %s Firmware...\n", gST->FirmwareVendor);
    if (Now.TimeZone < 0 || Now.TimeZone > 24) {
        MsgLog("Date Time: %d-%d-%d  %d:%d:%d (GMT)\n\n",
        Now.Year, Now.Month, Now.Day, Now.Hour, Now.Minute, Now.Second);
    } else {
        MsgLog("Date Time: %d-%d-%d  %d:%d:%d (GMT+%d)\n\n",
        Now.Year, Now.Month, Now.Day, Now.Hour, Now.Minute, Now.Second, Now.TimeZone);
    }
#endif

    // read configuration
    CopyMem(GlobalConfig.ScanFor, "ieom      ", NUM_SCAN_OPTIONS);
    FindLegacyBootType();
    if (GlobalConfig.LegacyType == LEGACY_TYPE_MAC)
       CopyMem(GlobalConfig.ScanFor, "ihebocm   ", NUM_SCAN_OPTIONS);
    SetConfigFilename(ImageHandle);
    MokProtocol = SecureBootSetup();

    // Scan volumes first to find SelfVolume, which is required by LoadDrivers();
    // however, if drivers are loaded, a second call to ScanVolumes() is needed
    // to register the new filesystem(s) accessed by the drivers.
    // Also, ScanVolumes() must be done before ReadConfig(), which needs
    // SelfVolume->VolName.
    #if REFIT_DEBUG > 0
    MsgLog("Scan for SelfVolume...\n");
    #endif
    ScanVolumes();

    if (LoadDrivers()) {
        #if REFIT_DEBUG > 0
        MsgLog("Scan Volumes...\n");
        #endif
        ScanVolumes();
    }

    ReadConfig(GlobalConfig.ConfigFilename);
    AdjustDefaultSelection();

    if (GlobalConfig.SpoofOSXVersion && GlobalConfig.SpoofOSXVersion[0] != L'\0') {
        #if REFIT_DEBUG > 0
        MsgLog("Spoof Mac OS Version...\n");
        #endif
        SetAppleOSInfo();
    }

    #if REFIT_DEBUG > 0
    MsgLog("Init Screen...\n");
    #endif
    InitScreen();

    WarnIfLegacyProblems();
    MainMenu.TimeoutSeconds = GlobalConfig.Timeout;
    // disable EFI watchdog timer
    refit_call4_wrapper(BS->SetWatchdogTimer, 0x0000, 0x0000, 0x0000, NULL);

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

       for (i = 0; i < GlobalConfig.ScanDelay; i++) {
            if ((i + 1) == 1) {
                #if REFIT_DEBUG > 0
                MsgLog("  - Waited %d Second\n", i + 1);
                #endif
            } else {
                #if REFIT_DEBUG > 0
                MsgLog("  - Waited %d Seconds\n", i + 1);
                #endif
            }
            refit_call1_wrapper(BS->Stall, 1000000);
       }
       RescanAll(GlobalConfig.ScanDelay > 1, TRUE);
       BltClearScreen(TRUE);
    } // if

    if (GlobalConfig.DefaultSelection)
       SelectionName = StrDuplicate(GlobalConfig.DefaultSelection);
    if (GlobalConfig.ShutdownAfterTimeout)
        MainMenu.TimeoutText = L"Shutdown";

    #if REFIT_DEBUG > 0
    MsgLog("Running rEFInd on %s Firmware...\n", gST->FirmwareVendor);
    #endif

    while (MainLoopRunning) {
        TempEntry = NULL;

        MenuExit = RunMainMenu(&MainMenu, &SelectionName, &ChosenEntry);

        // The Escape key triggers a re-scan operation....
        if (MenuExit == MENU_EXIT_ESCAPE) {
            MenuExit = 0;

            #if REFIT_DEBUG > 0
            MsgLog("Get User Input:\n");
            MsgLog("  - Rescan All\n\n");
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
                MsgLog("Get User Input:\n");
                if (egHasGraphicsMode()) {
                    MsgLog("  - Clean NVRAM\n------------\n\n");
                } else {
                    MsgLog("  - Clean NVRAM\n\n");
                }
                #endif

                StartTool((LOADER_ENTRY *)ChosenEntry);
                break;

            case TAG_PRE_NVRAMCLEAN:    // Clean NVRAM Info

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                MsgLog("  - Show Clean NVRAM Info\n\n");
                #endif

                preCleanNvram();

                // Reboot if CleanNvram was triggered
                if(ranCleanNvram == FALSE) {
                    #if REFIT_DEBUG > 0
                    MsgLog("INFO: Returned to Main Menu\n\n");
                    #endif
                } else {
                    #if REFIT_DEBUG > 0
                    MsgLog("INFO: Cleaned Nvram\n\n");
                    MsgLog("Reboot Computer...\n\n");
                    MsgLog("INFO: Terminating Screen:\n\n");
                    #endif
                    TerminateScreen();

                    #if REFIT_DEBUG > 0
                    MsgLog("INFO: Reseting System:\n\n");
                    #endif
                    refit_call4_wrapper(RT->ResetSystem, EfiResetCold, EFI_SUCCESS, 0, NULL);

                    #if REFIT_DEBUG > 0
                    MsgLog("INFO: Computer Reboot Failed ...Attempt Fallback:\n\n");
                    #endif
                    MainLoopRunning = FALSE;   // just in case we get this far
                }
                break;

            case TAG_SHOW_BOOTKICKER:    // Apple Boot Screen

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                if (egHasGraphicsMode()) {
                    MsgLog("  - Load Boot Screen\n------------\n\n");
                } else {
                    MsgLog("  - Load Boot Screen\n\n");
                }
                #endif

                TempEntry = (LOADER_ENTRY *)ChosenEntry;
                TempEntry->UseGraphicsMode = TRUE;

                StartTool(TempEntry);
                break;

            case TAG_PRE_BOOTKICKER:    // Apple Boot Screen Info

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                MsgLog("  - Show BootKicker Info\n\n");
                #endif

                preBootKicker();
                break;

            case TAG_REBOOT:    // Reboot

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                if (egHasGraphicsMode()) {
                    MsgLog("  - Reboot Computer\n------------\n\n");
                } else {
                    MsgLog("  - Reboot Computer\n\n");
                }
                #endif

                TerminateScreen();
                refit_call4_wrapper(RT->ResetSystem, EfiResetCold, EFI_SUCCESS, 0, NULL);
                MainLoopRunning = FALSE;   // just in case we get this far
                break;

            case TAG_SHUTDOWN: // Shut Down

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                if (egHasGraphicsMode()) {
                    MsgLog("  - Shut Computer Down\n------------\n\n");
                } else {
                    MsgLog("  - Shut Computer Down\n\n");
                }
                #endif

                TerminateScreen();
                refit_call4_wrapper(RT->ResetSystem, EfiResetShutdown, EFI_SUCCESS, 0, NULL);
                MainLoopRunning = FALSE;   // just in case we get this far
                break;

            case TAG_ABOUT:    // About rEFInd

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                MsgLog("  - Show 'About rEFInd' Box\n\n");
                #endif

                AboutrEFInd();
                break;

            case TAG_LOADER:   // Boot OS via .EFI loader

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                if (egHasGraphicsMode()) {
                    MsgLog("  - Boot OS via *.efi Loader\n------------\n\n");
                } else {
                    MsgLog("  - Boot OS via *.efi Loader\n\n");
                }
                #endif

                TempEntry = (LOADER_ENTRY *)ChosenEntry;
                if (!GlobalConfig.TextOnly) {
                    TempEntry->UseGraphicsMode = TRUE;
                }
                StartLoader(TempEntry, SelectionName);
                break;

            case TAG_LEGACY:   // Boot legacy OS

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                if (egHasGraphicsMode()) {
                    MsgLog("  - Boot Legacy OS\n------------\n\n");
                } else {
                    MsgLog("  - Boot Legacy OS\n\n");
                }
                #endif

                StartLegacy((LEGACY_ENTRY *)ChosenEntry, SelectionName);
                break;

            case TAG_LEGACY_UEFI: // Boot a legacy OS on a non-Mac

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                if (egHasGraphicsMode()) {
                    MsgLog("  - Boot Legacy UEFI\n------------\n\n");
                } else {
                    MsgLog("  - Boot Legacy UEFI\n\n");
                }
                #endif

                StartLegacyUEFI((LEGACY_ENTRY *)ChosenEntry, SelectionName);
                break;

            case TAG_TOOL:     // Start a EFI tool

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                MsgLog("  - Start EFI Tool\n\n");
                #endif

                TempEntry = (LOADER_ENTRY *)ChosenEntry;
                if (MyStrStr(TempEntry->Title, L"Boot Screen") != NULL) {
                    TempEntry->UseGraphicsMode = TRUE;
                }

                StartTool(TempEntry);
                break;

            case TAG_HIDDEN:  // Manage hidden tag entries

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                MsgLog("  - Manage Hidden Tag Entries\n\n");
                #endif

                ManageHiddenTags();
                break;

            case TAG_EXIT:    // Terminate rEFInd

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                if (egHasGraphicsMode()) {
                    MsgLog("  - Terminate rEFInd\n------------\n\n");
                } else {
                    MsgLog("  - Terminate rEFInd\n\n");
                }
                #endif

                if ((MokProtocol) && !SecureBootUninstall()) {
                   MainLoopRunning = FALSE;   // just in case we get this far
                } else {
                   BeginTextScreen(L" ");
                   return EFI_SUCCESS;
                }
                break;

            case TAG_FIRMWARE: // Reboot into firmware's user interface

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                if (egHasGraphicsMode()) {
                    MsgLog("  - Reboot into Firmware\n------------\n\n");
                } else {
                    MsgLog("  - Reboot into Firmware\n\n");
                }
                #endif

                RebootIntoFirmware();
                break;

            case TAG_CSR_ROTATE:

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                MsgLog("  - Rotate CSR Values\n\n");
                #endif

                RotateCsrValue();
                break;

            case TAG_INSTALL:

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
                if (egHasGraphicsMode()) {
                    MsgLog("  - Install rEFInd\n------------\n\n");
                } else {
                    MsgLog("  - Install rEFInd\n\n");
                }
                #endif

                InstallRefind();
                break;

            case TAG_BOOTORDER:

                #if REFIT_DEBUG > 0
                MsgLog("Get User Input:\n");
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
    MsgLog("Reboot Computer...\n");
    #endif
    TerminateScreen();

    #if REFIT_DEBUG > 0
    MsgLog("Fallback Screen Termination:\n");
    MsgLog("Fallback System Reset:\n\n");
    #endif
    refit_call4_wrapper(RT->ResetSystem, EfiResetCold, EFI_SUCCESS, 0, NULL);

    #if REFIT_DEBUG > 0
    MsgLog("INFO: Reboot Failed ...Entering Endless Idle Loop\n\n");
    #endif
    EndlessIdleLoop();

    return EFI_SUCCESS;
} /* efi_main() */
