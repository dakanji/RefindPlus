/*
 * BootMaster/screenmgt.c
 * Screen handling functions
 *
 * Copyright (c) 2006 Christoph Pfisterer
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
 * Copyright (c) 2020-2022 Dayo Akanji (sf.net/u/dakanji/profile)
 * Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "screenmgt.h"
#include "config.h"
#include "libegint.h"
#include "lib.h"
#include "menu.h"
#include "mystrings.h"
#include "../include/refit_call_wrapper.h"
#include "../include/egemb_refindplus_banner.h"

// Console defines and variables

UINTN   ConWidth;
UINTN   ConHeight;
CHAR16 *BlankLine = NULL;

// UGA defines and variables

UINTN      ScreenW                = 0;
UINTN      ScreenH                = 0;
UINTN      ScreenLongest          = 0;
UINTN      ScreenShortest         = 0;

BOOLEAN    AllowGraphicsMode      = FALSE;
BOOLEAN    ClearedBuffer          = FALSE;

EG_PIXEL   BlackPixel             = { 0x00, 0x00, 0x00, 0 };
EG_PIXEL   GrayPixel              = { 0xBF, 0xBF, 0xBF, 0 };
EG_PIXEL   WhitePixel             = { 0xFF, 0xFF, 0xFF, 0 };

EG_PIXEL   StdBackgroundPixel     = { 0xBF, 0xBF, 0xBF, 0 };
EG_PIXEL   MenuBackgroundPixel    = { 0xBF, 0xBF, 0xBF, 0 };
EG_PIXEL   DarkBackgroundPixel    = { 0x00, 0x00, 0x00, 0 };

// General defines and variables
static BOOLEAN     GraphicsScreenDirty;
static BOOLEAN     haveError = FALSE;

extern BOOLEAN            IsBoot;
extern BOOLEAN            egHasGraphics;
extern BOOLEAN            FlushFailedTag;


VOID PrepareBlankLine (VOID) {
    UINTN i;

    MY_FREE_POOL(BlankLine);
    // Make a buffer for a whole text line
    BlankLine = AllocatePool ((ConWidth + 1) * sizeof (CHAR16));
    for (i = 0; i < ConWidth; i++) {
        BlankLine[i] = ' ';
    }
    BlankLine[i] = 0;
} // VOID PrepareBlankLine()

//
// Screen initialisation and switching
//

VOID InitScreen (VOID) {
    // Initialise libeg
    egInitScreen();

    if (egHasGraphicsMode()) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID, L"Graphics Mode Detected ... Getting Resolution");
        #endif

        egGetScreenSize (&ScreenW, &ScreenH);
        AllowGraphicsMode = TRUE;
    }
    else {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID, L"Graphics Mode *NOT* Detected ... Setting Text Mode");
        #endif

        AllowGraphicsMode = FALSE;
        egSetTextMode (GlobalConfig.RequestedTextMode);

        // Ensure we are in Text Mode
        egSetGraphicsModeEnabled (FALSE);
    }

    GraphicsScreenDirty = TRUE;

    // Disable cursor
    REFIT_CALL_2_WRAPPER(gST->ConOut->EnableCursor, gST->ConOut, FALSE);

    // Get size of text console
    if (REFIT_CALL_4_WRAPPER(
        gST->ConOut->QueryMode, gST->ConOut,
        gST->ConOut->Mode->Mode, &ConWidth, &ConHeight
    ) != EFI_SUCCESS) {
        // Use default values on error
        ConWidth  = 80;
        ConHeight = 25;
    }

    PrepareBlankLine();

    // Show the banner if in text mode and not in DirectBoot mode
    if (GlobalConfig.TextOnly || !AllowGraphicsMode) {
        // DA-TAG: Just to make sure this is set
        AllowGraphicsMode = FALSE;

        if (!GlobalConfig.DirectBoot && GlobalConfig.ScreensaverTime != -1) {
            DrawScreenHeader (L"Initialising...");
        }
    }
} // VOID InitScreen()

// Set the screen resolution and mode (text vs. graphics).
VOID SetupScreen (VOID) {
    UINTN   NewWidth;
    UINTN   NewHeight;
    BOOLEAN gotGraphics;
    static
    BOOLEAN BannerLoaded = FALSE;
    static
    BOOLEAN ScaledIcons  = FALSE;

    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr = NULL;
    CHAR16 *TmpStr = NULL;

    if (!BannerLoaded) {
        LOG_MSG("S H O W   T I T L E   B A N N E R");
        LOG_MSG("\n");
    }
    #endif

    // Convert mode number to horizontal & vertical resolution values
    if ((GlobalConfig.RequestedScreenWidth > 0) &&
        (GlobalConfig.RequestedScreenHeight == 0)
    ) {
        #if REFIT_DEBUG > 0
        LOG_MSG("Get Resolution From Mode:");
        LOG_MSG("\n");
        #endif

        egGetResFromMode (
            &(GlobalConfig.RequestedScreenWidth),
            &(GlobalConfig.RequestedScreenHeight)
        );
    }

    // Set the believed-to-be current resolution to the LOWER of the current
    // believed-to-be resolution and the requested resolution. This is done to
    // enable setting a lower-than-default resolution.
    if ((GlobalConfig.RequestedScreenWidth > 0) &&
        (GlobalConfig.RequestedScreenHeight > 0)
    ) {
        #if REFIT_DEBUG > 0
        LOG_MSG("Sync Resolution:");
        LOG_MSG("\n");
        #endif

        ScreenW = (ScreenW < GlobalConfig.RequestedScreenWidth)
            ? ScreenW
            : GlobalConfig.RequestedScreenWidth;

        ScreenH = (ScreenH < GlobalConfig.RequestedScreenHeight)
            ? ScreenH
            : GlobalConfig.RequestedScreenHeight;

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Recording Current Resolution as %d x %d",
            ScreenW, ScreenH
        );
        #endif
    }

    // Get longest and shortest edge dimensions
    ScreenLongest  = (ScreenW >= ScreenH) ? ScreenW : ScreenH;
    ScreenShortest = (ScreenW <= ScreenH) ? ScreenW : ScreenH;

    // Set text mode. If this requires increasing the size of the graphics mode, do so.
    if (egSetTextMode (GlobalConfig.RequestedTextMode)) {
        #if REFIT_DEBUG > 0
        LOG_MSG("Set Text Mode:");
        LOG_MSG("\n");
        #endif

        egGetScreenSize (&NewWidth, &NewHeight);
        if ((NewWidth > ScreenW) || (NewHeight > ScreenH)) {
            ScreenW = NewWidth;
            ScreenH = NewHeight;
        }

        // Get longest and shortest edge dimensions
        ScreenLongest  = (ScreenW >= ScreenH) ? ScreenW : ScreenH;
        ScreenShortest = (ScreenW <= ScreenH) ? ScreenW : ScreenH;

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"After Setting Text Mode ... Recording *NEW* Current Resolution as '%d x %d'",
            ScreenLongest, ScreenShortest
        );
        #endif

        if ((ScreenW > GlobalConfig.RequestedScreenWidth) ||
            (ScreenH > GlobalConfig.RequestedScreenHeight)
        ) {
            #if REFIT_DEBUG > 0
            MsgStr = StrDuplicate (L"Match Requested Resolution to Actual Resolution");
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("  - %s", MsgStr);
            LOG_MSG("\n");
            MY_FREE_POOL(MsgStr);
            #endif

            // Requested text mode forces us to use a bigger graphics mode
            GlobalConfig.RequestedScreenWidth  = ScreenW;
            GlobalConfig.RequestedScreenHeight = ScreenH;
        }

        if (GlobalConfig.RequestedScreenWidth > 0) {
            #if REFIT_DEBUG > 0
            LOG_MSG("Set to User Requested Screen Size:");
            LOG_MSG("\n");
            #endif

            egSetScreenSize (
                &(GlobalConfig.RequestedScreenWidth),
                &(GlobalConfig.RequestedScreenHeight)
            );

            egGetScreenSize (&ScreenW, &ScreenH);
        } // if user requested a particular screen resolution
    }

    if (AllowGraphicsMode) {
        gotGraphics = egIsGraphicsModeEnabled();
        if (!gotGraphics || !BannerLoaded) {
            #if REFIT_DEBUG > 0
            MsgStr = (!gotGraphics)
                ? StrDuplicate (L"Text Screen Mode Active ... Prepare Graphics Mode Switch")
                : StrDuplicate (L"Graphics Screen Mode Active ... Prepare Title Banner Display");
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("%s:", MsgStr);
            MY_FREE_POOL(MsgStr);

            MsgStr = PoolPrint (
                L"Graphics Mode Resolution:- '%d x %d'",
                ScreenLongest, ScreenShortest
            );
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("%s  - %s", OffsetNext, MsgStr);
            MY_FREE_POOL(MsgStr);
            #endif

            // Scale icons up for HiDPI graphics if required
            if (GlobalConfig.ScaleUI == -1) {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"UI Scaling Disabled ... Maintain Icon Scale");
                #endif
            }
            else if (ScreenShortest < HIDPI_SHORT || ScreenLongest < HIDPI_LONG) {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"LoDPI Mode ... Maintain Icon Scale");
                #endif
            }
            else {
                #if REFIT_DEBUG > 0
                TmpStr = (GlobalConfig.ScaleUI == 1)
                    ? L"HiDPI Flag" : L"HiDPI Mode";
                #endif

                if (ScaledIcons) {
                    #if REFIT_DEBUG > 0
                    MsgStr = PoolPrint (L"%s ... Maintain Upscaled Icons", TmpStr);
                    #endif
                }
                else {
                    #if REFIT_DEBUG > 0
                    MsgStr = PoolPrint (L"%s ... Scale Icons Up", TmpStr);
                    #endif

                    GlobalConfig.IconSizes[ICON_SIZE_BADGE] *= 2;
                    GlobalConfig.IconSizes[ICON_SIZE_SMALL] *= 2;
                    GlobalConfig.IconSizes[ICON_SIZE_BIG]   *= 2;
                    GlobalConfig.IconSizes[ICON_SIZE_MOUSE] *= 2;

                    ScaledIcons = TRUE;
                }
            } // if/else GlobalConfig.ScaleUI

            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("%s    * %s", OffsetNext, MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
            #endif

            if (!gotGraphics) {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"Running Graphics Mode Switch");
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("INFO: %s", MsgStr);
                LOG_MSG("\n\n");
                MY_FREE_POOL(MsgStr);
                #endif

                // Clear screen and show banner
                // We now know we will stay in graphics mode
                SwitchToGraphics();
            }

            if (GlobalConfig.ScreensaverTime == -1) {
                #if REFIT_DEBUG > 0
                LOG_MSG("INFO: Changing to Screensaver Display");

                MsgStr = StrDuplicate (L"Configured to Start with Screensaver");
                ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
                LOG_MSG("%s      %s", OffsetNext, MsgStr);
                MY_FREE_POOL(MsgStr);
                #endif

                // Start with screen blanked
                GraphicsScreenDirty = TRUE;
            }
            else {
                // Clear the screen
                BltClearScreen (TRUE);

                #if REFIT_DEBUG > 0
                TmpStr = L"Displayed Title Banner";
                MsgStr = (gotGraphics)
                    ? StrDuplicate (TmpStr)
                    : StrDuplicate (L"Switched to Graphics Mode");
                ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
                LOG_MSG("INFO: %s", MsgStr);
                MY_FREE_POOL(MsgStr);

                if (!gotGraphics) {
                    LOG_MSG("%s      %s", OffsetNext, TmpStr);
                }
                #endif
            }
            BannerLoaded = TRUE;

            #if REFIT_DEBUG > 0
            (NativeLogger && GlobalConfig.LogLevel > 0)
                ? LOG_MSG("\n")
                : LOG_MSG("\n\n");
            #endif
        }
    }
    else {
        #if REFIT_DEBUG > 0
        if (GlobalConfig.TextOnly) {
            MsgStr = (GlobalConfig.DirectBoot)
                ? StrDuplicate (L"'DirectBoot' is Active")
                : StrDuplicate (L"Screen is in Text Only Mode");
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("Skipped Title Banner Display ... %s", MsgStr);
            MY_FREE_POOL(MsgStr);
        }
        else {
            MsgStr = StrDuplicate (L"Invalid Screen Mode ... Switching to Text Mode");
            ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
            LOG_MSG("WARN: %s", MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
        }
        #endif

        GlobalConfig.TextOnly = TRUE;
        AllowGraphicsMode = FALSE;
        SwitchToText (FALSE);
    }
} // VOID SetupScreen()

VOID SwitchToText (
    IN BOOLEAN CursorEnabled
) {
    EFI_STATUS Status;

#ifdef __MAKEWITH_TIANO
// DA-TAG: Limit to TianoCore
    if (!GlobalConfig.UseTextRenderer && !IsBoot && AppleFirmware) {
        // Override Text Renderer Setting on Apple Firmware
        // DA-TAG: Investigate need ... was needed on MacPro but maybe not all Macs?
        //         Confirm if really needed on MacPro or else consider removing
        Status = OcUseBuiltinTextOutput (
            (egHasGraphics)
                ? EfiConsoleControlScreenGraphics
                : EfiConsoleControlScreenText
        );
        if (!EFI_ERROR(Status)) {
            GlobalConfig.UseTextRenderer = TRUE;

            #if REFIT_DEBUG > 0
            LOG_MSG("INFO: Config Setting Overriden:- 'renderer_text'");
            LOG_MSG("\n");
            #endif
        }
    }
#endif

    egSetGraphicsModeEnabled (FALSE);
    REFIT_CALL_2_WRAPPER(gST->ConOut->EnableCursor, gST->ConOut, CursorEnabled);

    #if REFIT_DEBUG > 0
    BOOLEAN TextModeOnEntry = (
        egIsGraphicsModeEnabled()
        && !AllowGraphicsMode
        && !IsBoot
    );

    if (TextModeOnEntry) {
        LOG_MSG("Determine Text Console Size:");
        LOG_MSG("\n");
    }
    #endif

    // Get size of text console
    Status = REFIT_CALL_4_WRAPPER(
        gST->ConOut->QueryMode, gST->ConOut,
        gST->ConOut->Mode->Mode, &ConWidth, &ConHeight
    );
    if (EFI_ERROR(Status)) {
        // Use default values on error
        ConWidth  = 80;
        ConHeight = 25;

        #if REFIT_DEBUG > 0
        if (TextModeOnEntry) {
            LOG_MSG(
                "  Could Not Get Text Console Size ... Using Default:- '%d x %d'",
                ConHeight, ConWidth
            );
            LOG_MSG("\n\n");
        }
        #endif
    }
    else {
        #if REFIT_DEBUG > 0
        if (TextModeOnEntry) {
            LOG_MSG(
                "  Text Console Size:- '%d x %d'",
                ConWidth, ConHeight
            );
            LOG_MSG("\n\n");
        }
        #endif
    }

    PrepareBlankLine();

    #if REFIT_DEBUG > 0
    if (TextModeOnEntry) {
        LOG_MSG("INFO: Switched to Text Mode");
    }
    LOG_MSG("\n\n");
    #endif
} // VOID SwitchToText()

EFI_STATUS SwitchToGraphics (VOID) {
    if (AllowGraphicsMode) {
        return EFI_NOT_STARTED;
    }

    if (egIsGraphicsModeEnabled()) {
        return EFI_ALREADY_STARTED;
    }

    egSetGraphicsModeEnabled (TRUE);
    GraphicsScreenDirty = TRUE;

    return EFI_SUCCESS;
} // EFI_STATUS SwitchToGraphics()

//
// Screen control for running tools
//
VOID BeginTextScreen (
    IN CHAR16 *Title
) {
    DrawScreenHeader (Title);
    SwitchToText (FALSE);

    // Reset error flag
    haveError = FALSE;
} // VOID BeginTextScreen()

VOID FinishTextScreen (
    IN BOOLEAN WaitAlways
) {
    if (haveError || WaitAlways) {
        SwitchToText (FALSE);
        PauseForKey();
    }

    // Reset error flag
    haveError = FALSE;
} // VOID FinishTextScreen()

VOID BeginExternalScreen (
    IN BOOLEAN  UseGraphicsMode,
    IN CHAR16  *Title
) {

    if (GlobalConfig.DirectBoot) {
        // Reset error flag
        haveError = FALSE;

        // Early Return
        return;
    }

    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr    =  NULL;
    BOOLEAN  CheckMute = FALSE;
    #endif

    if (!AllowGraphicsMode) {
        UseGraphicsMode = FALSE;
    }

    if (UseGraphicsMode) {
        #if REFIT_DEBUG > 0
        MsgStr = L"Beginning Child Display with Screen Mode:- 'Graphics'";
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        if (!IsBoot) {
            LOG_MSG("%s    * %s", OffsetNext, MsgStr);
            LOG_MSG("\n\n");
        }
        #endif

        SwitchToGraphicsAndClear (FALSE);
    }
    else {
        #if REFIT_DEBUG > 0
        MsgStr = L"Beginning Child Display with Screen Mode:- 'Text'";
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        if (!IsBoot) {
            LOG_MSG("%s    * %s", OffsetNext, MsgStr);
        }
        #endif

        SwitchToText (UseGraphicsMode);

        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif
        DrawScreenHeader (Title);
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif
    }

    // Reset error flag
    haveError = FALSE;
} // VOID BeginExternalScreen()

VOID FinishExternalScreen (VOID) {
    // Make sure we clean up later
    GraphicsScreenDirty = TRUE;

    if (haveError) {
        SwitchToText (FALSE);
        PauseForKey();
    }

    // Reset the screen resolution, in case external program changed it.
    SetupScreen();

    // Reset error flag
    haveError = FALSE;
} // VOID FinishExternalScreen()

VOID TerminateScreen (VOID) {
    // clear text screen
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    REFIT_CALL_1_WRAPPER(gST->ConOut->ClearScreen,  gST->ConOut);

    // enable cursor
    REFIT_CALL_2_WRAPPER(gST->ConOut->EnableCursor, gST->ConOut, TRUE);
} // VOID TerminateScreen()

VOID DrawScreenHeader (
    IN CHAR16 *Title
) {
    UINTN i;

    // Clear to black background ... First clear in graphics mode
    egClearScreen (&DarkBackgroundPixel);

    // Then clear in text mode
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    REFIT_CALL_1_WRAPPER(gST->ConOut->ClearScreen,  gST->ConOut);

    // Paint header background
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BANNER);
    for (i = 0; i < 3; i++) {
        REFIT_CALL_3_WRAPPER(
            gST->ConOut->SetCursorPosition, gST->ConOut,
            0, i
        );
        Print (BlankLine);
    }

    // Print header text
    REFIT_CALL_3_WRAPPER(
        gST->ConOut->SetCursorPosition, gST->ConOut,
        3, 1
    );
    Print (L"RefindPlus Boot Manager ... %s", Title);

    // Reposition cursor
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    REFIT_CALL_3_WRAPPER(
        gST->ConOut->SetCursorPosition, gST->ConOut,
        0, 4
    );
} // VOID DrawScreenHeader()

//
// Keyboard input
//

BOOLEAN ReadAllKeyStrokes (VOID) {
    EFI_STATUS           Status        = EFI_NOT_FOUND;
    BOOLEAN              GotKeyStrokes = FALSE;
    BOOLEAN              EmptyBuffer   = FALSE;
    static BOOLEAN       FirstCall     = TRUE;
    EFI_INPUT_KEY        key;

    if (FirstCall || !GlobalConfig.DirectBoot) {
        for (;;) {
            Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
            if (Status == EFI_SUCCESS) {
                ClearedBuffer = TRUE;
                GotKeyStrokes = TRUE;
                continue;
            }
            else if (Status == EFI_NOT_READY) {
                EmptyBuffer = TRUE;
            }
            break;
        }
    }

    #if REFIT_DEBUG > 0
    if (!FirstCall && GlobalConfig.DirectBoot) {
        Status = EFI_NOT_STARTED;
    }
    else if (GotKeyStrokes) {
        Status = EFI_SUCCESS;
    }
    else if (EmptyBuffer) {
        Status = EFI_ALREADY_STARTED;
    }
    else if (!GlobalConfig.DirectBoot) {
        FlushFailedTag = TRUE;
    }

    CHAR16 *MsgStr = PoolPrint (L"Clear Keystroke Buffer ... %r", Status);
    LOG_MSG("INFO: %s", MsgStr);
    LOG_MSG("\n\n");
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    FirstCall = FALSE;

    return GotKeyStrokes;
} // BOOLEAN ReadAllKeyStrokes()

// Displays *Text without regard to appearances. Used mainly for debugging
// and rare error messages.
// Position code is used only in graphics mode.
// TODO: Improve to handle multi-line text.
VOID PrintUglyText (
    IN CHAR16 *Text,
    IN UINTN    PositionCode
) {
    EG_PIXEL BGColor = COLOR_RED;

    if (Text) {
        if (AllowGraphicsMode &&
            AppleFirmware &&
            egIsGraphicsModeEnabled()
        ) {
            egDisplayMessage (Text, &BGColor, PositionCode);
            GraphicsScreenDirty = TRUE;
        }
        else {
            // Non-Mac or in Text Mode ... Print statement will work
            Print (Text);
            Print (L"\n");
        }
    }
} // VOID PrintUglyText()

VOID PauseForKey (VOID) {
    UINTN   i, WaitOut;
    BOOLEAN Breakout  = FALSE;

    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr    =  NULL;
    BOOLEAN  CheckMute = FALSE;
    #endif

    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_SET;
    #endif
    PrintUglyText (L"", NEXTLINE);
    PrintUglyText (L"", NEXTLINE);
    PrintUglyText (L"                                                          ", NEXTLINE);
    PrintUglyText (L"                                                          ", NEXTLINE);
    PrintUglyText (L"             * Paused for Error or Warning *              ", NEXTLINE);
    (GlobalConfig.ContinueOnWarning)
        ? PrintUglyText (L"        Press a Key or Wait 9 Seconds to Continue         ", NEXTLINE)
        : PrintUglyText (L"                 Press a Key to Continue                  ", NEXTLINE);
    PrintUglyText (L"                                                          ", NEXTLINE);
    PrintUglyText (L"                                                          ", NEXTLINE);
    // Clear the Keystroke Buffer
    ReadAllKeyStrokes();
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    if (GlobalConfig.ContinueOnWarning) {
        #if REFIT_DEBUG > 0
        MsgStr = L"Paused for Error/Warning ... Waiting 9 Seconds";
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("INFO: %s", MsgStr);
        #endif

        for (i = 0; i < 9; ++i) {
            WaitOut = WaitForInput (1000);
            if (WaitOut == INPUT_KEY) {
                #if REFIT_DEBUG > 0
                MsgStr = L"Pause Terminated by Keypress";
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("%s      * %s", OffsetNext, MsgStr);
                LOG_MSG("\n\n");
                #endif

                Breakout = TRUE;
            }
            else if (WaitOut == INPUT_TIMER_ERROR) {
                #if REFIT_DEBUG > 0
                MsgStr = L"Pause Terminated on Timer Error";
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
                LOG_MSG("%s      * %s", OffsetNext, MsgStr);
                LOG_MSG("\n\n");
                #endif

                Breakout = TRUE;
            }

            if (Breakout) {
                break;
            }
        } // for

        #if REFIT_DEBUG > 0
        if (!Breakout) {
            MsgStr = L"Pause Terminated on Timeout";
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
            LOG_MSG("%s      * %s", OffsetNext, MsgStr);
            LOG_MSG("\n\n");
        }
        #endif
    }
    else {
        #if REFIT_DEBUG > 0
        MsgStr = L"Paused for Error/Warning ... Keypress Required";
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("INFO: %s", MsgStr);
        #endif

        for (;;) {
            WaitOut = WaitForInput (1000);
            if (WaitOut == INPUT_KEY) {
                #if REFIT_DEBUG > 0
                MsgStr = L"Pause Terminated by Keypress";
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("%s      * %s", OffsetNext, MsgStr);
                LOG_MSG("\n\n");
                #endif

                Breakout = TRUE;
            }
            else if (WaitOut == INPUT_TIMER_ERROR) {
                #if REFIT_DEBUG > 0
                MsgStr = L"Pause Terminated on Timer Error";
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
                LOG_MSG("%s      * %s", OffsetNext, MsgStr);
                LOG_MSG("\n\n");
                #endif

                Breakout = TRUE;
            }

            if (Breakout) {
                break;
            }
        } // for
    }

    GraphicsScreenDirty = TRUE;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"Resuming After Pause");
    #endif
} // VOID PauseForKey

// Pause for a specified number of seconds
// Can be terminated on keypress
VOID PauseSeconds (
    UINTN Seconds
) {
    UINTN   i, WaitOut;
    BOOLEAN Breakout  = FALSE;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_SET;
    #endif
    // Clear the Keystroke Buffer
    ReadAllKeyStrokes();
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_MID, L"Pausing for %d Seconds", Seconds);
    #endif

    for (i = 0; i < Seconds; ++i) {
        WaitOut = WaitForInput (1000);
        if (WaitOut == INPUT_KEY) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL, L"Pause Terminated by Keypress");
            #endif

            Breakout = TRUE;
        }
        else if (WaitOut == INPUT_TIMER_ERROR) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL, L"Pause Terminated on Timer Error!!");
            #endif

            Breakout = TRUE;
        }

        if (Breakout) {
            break;
        }
    } // for

    #if REFIT_DEBUG > 0
    if (!Breakout) {
        ALT_LOG(1, LOG_LINE_NORMAL, L"Pause Terminated on Timeout");
    }
    #endif

    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_SET;
    #endif
    // Clear the Keystroke Buffer
    ReadAllKeyStrokes();
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif
} // VOID PauseSeconds()

// Halt for a specified number of seconds
// Not terminated on keypress
VOID HaltSeconds (
    UINTN Seconds
) {
    UINTN   i;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_MID, L"Halting for %d Seconds", Seconds);
    #endif

    for (i = 0; i < Seconds; ++i) {
        // Wait 1 second
        REFIT_CALL_1_WRAPPER(gBS->Stall, 250000);
        REFIT_CALL_1_WRAPPER(gBS->Stall, 250000);
        REFIT_CALL_1_WRAPPER(gBS->Stall, 250000);
        REFIT_CALL_1_WRAPPER(gBS->Stall, 250000);
    } // for

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Halt Terminated on Timeout");
    #endif

    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_SET;
    #endif
    // Clear the Keystroke Buffer
    ReadAllKeyStrokes();
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif
} // VOID HaltSeconds()

#if REFIT_DEBUG > 0
VOID DebugPause (VOID) {
    // show console and wait for key
    SwitchToText (FALSE);
    PauseForKey();

    // Reset error flag
    haveError = FALSE;
} // VOID DebugPause()
#endif

VOID RefitDeadLoop (VOID) {
    UINTN   index;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;

    MY_MUTELOGGER_SET;
    #endif
    for (;;) {
        ReadAllKeyStrokes();

        REFIT_CALL_3_WRAPPER(
            gBS->WaitForEvent, 1,
            &gST->ConIn->WaitForKey, &index
        );
    }
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif
} // VOID RefitDeadLoop()

//
// Error handling
//

BOOLEAN CheckFatalError (
    IN EFI_STATUS  Status,
    IN CHAR16     *where
) {
    CHAR16 *Temp = NULL;

    if (!EFI_ERROR(Status)) {
        return FALSE;
    }

#ifdef __MAKEWITH_GNUEFI
    CHAR16 ErrorName[64];
    StatusToString (ErrorName, Status);

    #if REFIT_DEBUG > 0
    LOG_MSG("** FATAL ERROR: '%s' %s", ErrorName, where);
    LOG_MSG("\n");
    #endif

    Temp = PoolPrint (L"Fatal Error: '%s' %s", ErrorName, where);
#else
    Temp = PoolPrint (L"Fatal Error: '%r' %s", Status, where);

    #if REFIT_DEBUG > 0
    LOG_MSG("** FATAL ERROR: '%r' %s", Status, where);
    LOG_MSG("\n");
    #endif
#endif
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText (Temp, NEXTLINE);
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    haveError = TRUE;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_STAR_SEPARATOR, Temp);
    #endif

    MY_FREE_POOL(Temp);

    return TRUE;
} // BOOLEAN CheckFatalError()

BOOLEAN CheckError (
    IN EFI_STATUS  Status,
    IN CHAR16     *where
) {
    CHAR16 *Temp = NULL;

    if (!EFI_ERROR(Status)) {
        // Early Return
        return FALSE;
    }

#ifdef __MAKEWITH_GNUEFI
    CHAR16 ErrorName[64];
    StatusToString (ErrorName, Status);

    #if REFIT_DEBUG > 0
    LOG_MSG("** WARN: '%s' %s", ErrorName, where);
    #endif

    Temp = PoolPrint (L"Error: '%s' %s", ErrorName, where);
#else
    Temp = PoolPrint (L"Error: '%r' %s", Status, where);

    #if REFIT_DEBUG > 0
    LOG_MSG("** WARN: '%r' %s", Status, where);
    #endif
#endif

    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText (Temp, NEXTLINE);
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

    // Defeat need to "Press a Key to Continue" in debug mode
    haveError = (
        MyStrStr (where, L"While Reading Boot Sector") ||
        MyStrStr (where, L"in ReadHiddenTags")
    ) ? FALSE : TRUE;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_STAR_SEPARATOR, Temp);
    #endif

    MY_FREE_POOL(Temp);

    return haveError;
} // BOOLEAN CheckError()

//
// Graphics functions
//

VOID SwitchToGraphicsAndClear (
    IN BOOLEAN ShowBanner
) {
    SwitchToGraphics();
    if (GraphicsScreenDirty) {
        BltClearScreen (ShowBanner);
    }
} // VOID SwitchToGraphicsAndClear()


#if 0
// DA-TAG: Permit Image->PixelData Memory Leak on Qemu
//         Apparent Memory Conflict ... Needs Investigation.
//         See: sf.net/p/refind/discussion/general/thread/4dfcdfdd16/
//         Temporary ... Eliminate when no longer required
// UPDATE: Disabled for v0.13.2.AK ... Watch for issue reports
static
VOID egFreeImageQEMU (
    EG_IMAGE *Image
) {
    if (DetectedDevices) {
        MY_FREE_IMAGE(Image);
    }
    else {
        MY_FREE_POOL(Image);
    }
} // static VOID egFreeImageQEMU()
#endif


VOID BltClearScreen (
    BOOLEAN ShowBanner
) {
    static EG_IMAGE  *Banner     = NULL;
    EG_IMAGE         *NewBanner  = NULL;
    INTN              BannerPosX = 0;
    INTN              BannerPosY = 0;

    #if REFIT_DEBUG > 0
    static BOOLEAN  LoggedBanner;
    CHAR16         *MsgStr = NULL;
    #endif

    #if REFIT_DEBUG > 0
    if (!IsBoot) {
        LOG_MSG("Refresh Screen:");
    }
    #endif

    BOOLEAN BannerPass = (
        !IsBoot ||
        (
            ShowBanner &&
            !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_BANNER)
        )
    );
    if (!BannerPass) {
        #if REFIT_DEBUG > 0
        if (!IsBoot) {
            LOG_MSG("%s  - Clear Screen", OffsetNext);
        }
        #endif

        // Not showing banner
        // Clear to menu background colour
        egClearScreen (&MenuBackgroundPixel);
    }
    else {
        // Load banner on first call
        if (Banner == NULL) {
            #if REFIT_DEBUG > 0
            LOG_MSG("%s  - Get Banner", OffsetNext);
            #endif

            if (GlobalConfig.BannerFileName) {
                Banner = egLoadImage (SelfDir, GlobalConfig.BannerFileName, FALSE);
            }

            if (GlobalConfig.CustomScreenBG) {
                // Override Default Values
                MenuBackgroundPixel.r = GlobalConfig.ScreenR;
                MenuBackgroundPixel.g = GlobalConfig.ScreenG;
                MenuBackgroundPixel.b = GlobalConfig.ScreenB;
            }
            else if (Banner) {
                MenuBackgroundPixel = Banner->PixelData[0];
            }
            else {
                MenuBackgroundPixel = GrayPixel;
            }

            if (Banner != NULL) {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"Custom Title Banner");
                LOG_MSG("%s    * %s", OffsetNext, MsgStr);
                if (!LoggedBanner) {
                    ALT_LOG(1, LOG_LINE_NORMAL, L"Using %s", MsgStr);
                    LoggedBanner = TRUE;
                }
                MY_FREE_POOL(MsgStr);
                #endif
            }
            else {
                #if REFIT_DEBUG > 0
                CHAR16 *StrSpacer = L"    ";
                MsgStr = StrDuplicate (L"Default Title Banner");
                LOG_MSG("%s%s* %s", OffsetNext, StrSpacer, MsgStr);
                if (!LoggedBanner) {
                    ALT_LOG(1, LOG_LINE_NORMAL, L"Using %s", MsgStr);
                }
                MY_FREE_POOL(MsgStr);

                LOG_MSG(
                    "%s%s  Colour (Mode) ...   R   G   B",
                    OffsetNext,
                    StrSpacer
                );

                MsgStr = PoolPrint (
                    L"Colour (Base) ... %3d %3d %3d",
                    MenuBackgroundPixel.r,
                    MenuBackgroundPixel.g,
                    MenuBackgroundPixel.b
                );
                LOG_MSG(
                    "%s%s  %s",
                    OffsetNext,
                    StrSpacer,
                    MsgStr
                );
                if (!LoggedBanner) {
                    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                    LoggedBanner = TRUE;
                }
                MY_FREE_POOL(MsgStr);
                #endif

                // Get Screen Luminance Index
                UINTN FactorFP = 10;
                UINTN Divisor  = 3 * FactorFP;
                UINTN PixelsR  = (UINTN) MenuBackgroundPixel.r;
                UINTN PixelsG  = (UINTN) MenuBackgroundPixel.g;
                UINTN PixelsB  = (UINTN) MenuBackgroundPixel.b;
                UINTN LumIndex = (
                    (
                        (PixelsR * FactorFP) +
                        (PixelsG * FactorFP) +
                        (PixelsB * FactorFP) +
                        (Divisor / 2) // Added For Rounding
                    ) / Divisor
                );

                EG_PIXEL BannerFontColor = BlackPixel;
                // DA-TAG: Use complementary colour for banner text on darkish backgrounds
                //         Do not apply to other text to avoid OTT effect
                if (LumIndex < 128) {
                    // Complementary colour for Mid Grey is ... Mid Grey!
                    // Check for a broadly defined 'Grey' to fix
                    // Difficult to read text otherwise
                    UINTN MaxRGB = (PixelsR > PixelsG) ? PixelsR : PixelsG;
                    MaxRGB       = (MaxRGB  > PixelsB) ? MaxRGB  : PixelsB;

                    UINTN MinRGB = (PixelsR < PixelsG) ? PixelsR : PixelsG;
                    MinRGB       = (MinRGB  < PixelsB) ? MinRGB  : PixelsB;

                    if ((MaxRGB - MinRGB) <= 64) {
                        // We have 'Grey' ... Determine if it is 'Mid Grey'
                        if (MaxRGB < 160 && MinRGB > 96) {
                            // We have 'Mid Grey' ... Set Input Pixels to Black
                            // Will be flipped to complementary White later
                            PixelsR = PixelsG = PixelsB = 0;
                        }
                    }

                    // Get complementary colour of Input Pixels
                    BannerFontColor.r = 255 - PixelsR;
                    BannerFontColor.g = 255 - PixelsG;
                    BannerFontColor.b = 255 - PixelsB;
                }

                Banner = egPrepareEmbeddedImage (
                    &egemb_refindplus_banner,
                    TRUE,
                    &BannerFontColor
                );
            } // if/else Banner != NULL

            if (Banner != NULL) {
                EG_IMAGE *CompImage;

                // compose on background
                CompImage = egCreateFilledImage (
                    Banner->Width,
                    Banner->Height,
                    FALSE,
                    &MenuBackgroundPixel
                );

                if (CompImage) {
                    egComposeImage (CompImage, Banner, 0, 0);
                    MY_FREE_IMAGE(Banner);
                    Banner = CompImage;
                    CompImage = NULL;
                }
            }
        } // if Banner == NULL

        if (Banner != NULL) {
            #if REFIT_DEBUG > 0
            LOG_MSG("%s  - Scale Banner", OffsetNext);
            #endif

            if (GlobalConfig.BannerScale == BANNER_FILLSCREEN) {
                if (Banner->Width != ScreenW || Banner->Height != ScreenH) {
                    NewBanner = egScaleImage (Banner, ScreenW, ScreenH);
                }
            }
            else if (Banner->Width > ScreenW || Banner->Height > ScreenH) {
                NewBanner = egCropImage (
                    Banner, 0, 0,
                    (Banner->Width  > ScreenW) ? ScreenW : Banner->Width,
                    (Banner->Height > ScreenH) ? ScreenH : Banner->Height
                );
            }

            if (NewBanner != NULL) {
                // DA-TAG: See notes in 'egFreeImageQEMU'
                MY_FREE_IMAGE(Banner);
                Banner = NewBanner;
            }
        }

        // Clear and draw banner
        #if REFIT_DEBUG > 0
        LOG_MSG("%s  - Clear Screen", OffsetNext);
        #endif

        if (GlobalConfig.ScreensaverTime != -1) {
            egClearScreen (&MenuBackgroundPixel);
        }
        else {
            egClearScreen (&BlackPixel);
        }

        if (Banner != NULL) {
            #if REFIT_DEBUG > 0
            LOG_MSG("%s  - Show Banner", OffsetNext);
            #endif

            BannerPosX = (Banner->Width < ScreenW) ? ((ScreenW - Banner->Width) / 2) : 0;
            BannerPosY = (INTN) (ComputeRow0PosY() / 2) - (INTN) Banner->Height;
            if (BannerPosY < 0) {
                BannerPosY = 0;
            }

            GlobalConfig.BannerBottomEdge = BannerPosY + Banner->Height;

            if (GlobalConfig.ScreensaverTime != -1) {
                BltImage (Banner, (UINTN) BannerPosX, (UINTN) BannerPosY);
            }
        }
    } // if/else !BannerPass

    #if REFIT_DEBUG > 0
    if (!IsBoot) {
        LOG_MSG("\n\n");
    }
    #endif

    GraphicsScreenDirty = FALSE;

    // DA-TAG: See notes in 'egFreeImageQEMU'
    MY_FREE_IMAGE(GlobalConfig.ScreenBackground);
    GlobalConfig.ScreenBackground = egCopyScreen();
} // VOID BltClearScreen()


VOID BltImage (
    IN EG_IMAGE *Image,
    IN UINTN     XPos,
    IN UINTN     YPos
) {
    egDrawImage (Image, XPos, YPos);
    GraphicsScreenDirty = TRUE;
} // VOID BltImage()

VOID BltImageAlpha (
    IN EG_IMAGE *Image,
    IN UINTN     XPos,
    IN UINTN     YPos,
    IN EG_PIXEL *BackgroundPixel
) {
    EG_IMAGE *CompImage;

    // compose on background
    CompImage = egCreateFilledImage (
        Image->Width,
        Image->Height,
        FALSE,
        BackgroundPixel
    );

    egComposeImage (CompImage, Image, 0, 0);

    // blit to screen and clean up
    egDrawImage (CompImage, XPos, YPos);
    MY_FREE_IMAGE(CompImage);

    GraphicsScreenDirty = TRUE;
} // VOID BltImageAlpha()

//VOID BltImageComposite (
//    IN EG_IMAGE *BaseImage,
//    IN EG_IMAGE *TopImage,
//    IN UINTN     XPos,
//    IN UINTN     YPos
//) {
//    UINTN TotalWidth, TotalHeight, CompWidth, CompHeight, OffsetX, OffsetY;
//    EG_IMAGE *CompImage;
//
//    // initialize buffer with base image
//    CompImage = egCopyImage (BaseImage);
//    TotalWidth  = BaseImage->Width;
//    TotalHeight = BaseImage->Height;
//
//    // Place the top image
//    CompWidth = TopImage->Width;
//    if (CompWidth > TotalWidth) {
//        CompWidth = TotalWidth;
//    }
//    OffsetX = (TotalWidth - CompWidth) >> 1;
//    CompHeight = TopImage->Height;
//    if (CompHeight > TotalHeight) {
//        CompHeight = TotalHeight;
//    }
//    OffsetY = (TotalHeight - CompHeight) >> 1;
//    egComposeImage (CompImage, TopImage, OffsetX, OffsetY);
//
//    // blit to screen and clean up
//    egDrawImage (CompImage, XPos, YPos);
//    MY_FREE_IMAGE(CompImage);
//    GraphicsScreenDirty = TRUE;
//} // VOID BltImageComposite()

VOID BltImageCompositeBadge (
    IN EG_IMAGE *BaseImage,
    IN EG_IMAGE *TopImage,
    IN EG_IMAGE *BadgeImage,
    IN UINTN     XPos,
    IN UINTN     YPos
) {
    UINTN     TotalWidth  = 0;
    UINTN     TotalHeight = 0;
    UINTN     CompWidth   = 0;
    UINTN     CompHeight  = 0;
    UINTN     OffsetX     = 0;
    UINTN     OffsetY     = 0;
    EG_IMAGE *CompImage   = NULL;

    // initialize buffer with base image
    if (BaseImage != NULL) {
        CompImage   = egCopyImage (BaseImage);
        TotalWidth  = BaseImage->Width;
        TotalHeight = BaseImage->Height;
    }

    // place the top image
    if ((TopImage != NULL) && (CompImage != NULL)) {
        CompWidth = TopImage->Width;

        if (CompWidth > TotalWidth) {
            CompWidth = TotalWidth;
        }

        OffsetX    = (TotalWidth - CompWidth) >> 1;
        CompHeight = TopImage->Height;

        if (CompHeight > TotalHeight) {
            CompHeight = TotalHeight;
        }

        OffsetY = (TotalHeight - CompHeight) >> 1;
        egComposeImage (CompImage, TopImage, OffsetX, OffsetY);
    }

    // place the badge image
    if (BadgeImage != NULL && CompImage != NULL &&
        (BadgeImage->Width  + 8) < CompWidth &&
        (BadgeImage->Height + 8) < CompHeight
    ) {
        OffsetX += CompWidth  - 8 - BadgeImage->Width;
        OffsetY += CompHeight - 8 - BadgeImage->Height;
        egComposeImage (CompImage, BadgeImage, OffsetX, OffsetY);
    }

    // blit to screen and clean up
    if (CompImage != NULL) {
        if (CompImage->HasAlpha) {
            egDrawImageWithTransparency (
                CompImage, NULL,
                XPos, YPos,
                CompImage->Width, CompImage->Height
            );
        }
        else {
            egDrawImage (CompImage, XPos, YPos);
        }

        MY_FREE_IMAGE(CompImage);
        GraphicsScreenDirty = TRUE;
    }
} // VOID BltImageCompositeBadge()
