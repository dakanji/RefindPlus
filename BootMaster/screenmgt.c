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
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
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

// general defines and variables
static
BOOLEAN    GraphicsScreenDirty;
static
BOOLEAN     haveError = FALSE;

extern
BOOLEAN     FlushFailedTag;
extern
BOOLEAN     IsBoot;

#if REFIT_DEBUG > 0
extern
CHAR16     *OffsetNext;
#endif

static
VOID PrepareBlankLine (VOID) {
    UINTN i;

    MY_FREE_POOL(BlankLine);
    // make a buffer for a whole text line
    BlankLine = AllocatePool ((ConWidth + 1) * sizeof (CHAR16));
    for (i = 0; i < ConWidth; i++) {
        BlankLine[i] = ' ';
    }
    BlankLine[i] = 0;
} // VOID PrepareBlankLine()

//
// Screen initialization and switching
//

VOID InitScreen (VOID) {
    // initialise libeg
    egInitScreen();

    if (egHasGraphicsMode()) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_THREE_STAR_MID, L"Graphics Mode Detected ... Getting Resolution");
        #endif

        egGetScreenSize (&ScreenW, &ScreenH);
        AllowGraphicsMode = TRUE;
    }
    else {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_THREE_STAR_MID, L"Graphics Mode *NOT* Detected ... Setting Text Mode");
        #endif

        AllowGraphicsMode = FALSE;
        egSetTextMode (GlobalConfig.RequestedTextMode);

         // Make double sure we are in text mode
         egSetGraphicsModeEnabled (FALSE);
    }

    GraphicsScreenDirty = TRUE;

    // disable cursor
    REFIT_CALL_2_WRAPPER(gST->ConOut->EnableCursor, gST->ConOut, FALSE);

    // get size of text console
    if (REFIT_CALL_4_WRAPPER(
        gST->ConOut->QueryMode, gST->ConOut,
        gST->ConOut->Mode->Mode,
        &ConWidth, &ConHeight
    ) != EFI_SUCCESS) {
        // use default values on error
        ConWidth  = 80;
        ConHeight = 25;
    }

    PrepareBlankLine();

    // show the banner if in text mode
    if (GlobalConfig.TextOnly) {
        // DA-TAG: Just to make sure this is set
        AllowGraphicsMode = FALSE;

        if (GlobalConfig.ScreensaverTime != -1) {
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

    if (!BannerLoaded) {
        MsgLog ("Set Screen Up...\n");
    }
    #endif

    // Convert mode number to horizontal & vertical resolution values
    if ((GlobalConfig.RequestedScreenWidth > 0) &&
        (GlobalConfig.RequestedScreenHeight == 0)
    ) {
        #if REFIT_DEBUG > 0
        MsgLog ("Get Resolution From Mode:\n");
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
        MsgLog ("Sync Resolution:\n");
        #endif

        ScreenW = (ScreenW < GlobalConfig.RequestedScreenWidth)
            ? ScreenW
            : GlobalConfig.RequestedScreenWidth;

        ScreenH = (ScreenH < GlobalConfig.RequestedScreenHeight)
            ? ScreenH
            : GlobalConfig.RequestedScreenHeight;

        #if REFIT_DEBUG > 0
        LOG(2, LOG_LINE_NORMAL,
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
        MsgLog ("Set Text Mode:\n");
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
        LOG(2, LOG_LINE_NORMAL,
            L"After Setting Text Mode ... Recording *NEW* Current Resolution as '%d x %d'",
            ScreenLongest, ScreenShortest
        );
        #endif

        if ((ScreenW > GlobalConfig.RequestedScreenWidth) ||
            (ScreenH > GlobalConfig.RequestedScreenHeight)
        ) {
            #if REFIT_DEBUG > 0
            MsgStr = StrDuplicate (L"Match Requested Resolution to Actual Resolution");
            LOG(2, LOG_LINE_NORMAL, L"%s", MsgStr);
            MsgLog ("  - %s\n", MsgStr);
            MY_FREE_POOL(MsgStr);
            #endif

            // Requested text mode forces us to use a bigger graphics mode
            GlobalConfig.RequestedScreenWidth  = ScreenW;
            GlobalConfig.RequestedScreenHeight = ScreenH;
        }

        if (GlobalConfig.RequestedScreenWidth > 0) {

            #if REFIT_DEBUG > 0
            MsgLog ("Set to User Requested Screen Size:\n");
            #endif

            egSetScreenSize (
                &(GlobalConfig.RequestedScreenWidth),
                &(GlobalConfig.RequestedScreenHeight)
            );

            egGetScreenSize (&ScreenW, &ScreenH);
        } // if user requested a particular screen resolution
    }

    if (GlobalConfig.TextOnly) {
        // Set text mode if requested
        AllowGraphicsMode = FALSE;
        SwitchToText (FALSE);

        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"Screen is in Text Mode");
        LOG(2, LOG_LINE_NORMAL, L"%s", MsgStr);
        MsgLog ("INFO: %s", MsgStr);
        MY_FREE_POOL(MsgStr);
        (GlobalConfig.LogLevel == 0)
            ? MsgLog ("\n\n")
            : MsgLog ("\n");
        #endif
    }
    else if (AllowGraphicsMode) {
        gotGraphics = egIsGraphicsModeEnabled();
        if (!gotGraphics || !BannerLoaded) {
            #if REFIT_DEBUG > 0
            MsgStr = (!gotGraphics)
                ? StrDuplicate (L"Prepare Graphics Mode Switch")
                : StrDuplicate (L"Prepare Placeholder Display");
            LOG(2, LOG_LINE_NORMAL, L"%s", MsgStr);
            MsgLog ("%s:", MsgStr);
            MsgLog ("\n");
            MY_FREE_POOL(MsgStr);

            MsgStr = PoolPrint (L"Graphics Mode Resolution:- '%d x %d'", ScreenLongest, ScreenShortest);
            LOG(2, LOG_LINE_NORMAL, L"%s", MsgStr);
            MsgLog ("  - %s", MsgStr);
            MY_FREE_POOL(MsgStr);
            #endif

            // scale icons up for HiDPI graphics if required
            if (GlobalConfig.ScaleUI == -1) {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"UI Scaling Disabled ... Maintain Icon Scale");
                LOG(2, LOG_LINE_NORMAL, L"%s", MsgStr);
                MsgLog ("%s    * %s", OffsetNext, MsgStr);
                MsgLog ("\n\n");
                MY_FREE_POOL(MsgStr);
                #endif
            }
            else if (
                (GlobalConfig.ScaleUI == 1)
                || (ScreenShortest >= HIDPI_SHORT && ScreenLongest >= HIDPI_LONG)
            ) {
                #if REFIT_DEBUG > 0
                CHAR16 *TmpStr = (GlobalConfig.ScaleUI == 1) ? L"HiDPI Flag" : L"HiDPI Mode";
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

                #if REFIT_DEBUG > 0
                LOG(2, LOG_LINE_NORMAL, L"%s", MsgStr);
                MsgLog ("%s    * %s", OffsetNext, MsgStr);
                MsgLog ("\n\n");
                MY_FREE_POOL(MsgStr);
                #endif
            }
            else {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"LoDPI Mode ... Maintain Icon Scale");
                LOG(2, LOG_LINE_NORMAL, L"%s", MsgStr);
                MsgLog ("%s    * %s", OffsetNext, MsgStr);
                MsgLog ("\n\n");
                MY_FREE_POOL(MsgStr);
                #endif
            } // if GlobalConfig.ScaleUI

            if (!gotGraphics) {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"Running Graphics Mode Switch");
                LOG(2, LOG_LINE_NORMAL, L"%s", MsgStr);
                MsgLog ("INFO: %s", MsgStr);
                MsgLog ("\n\n");
                MY_FREE_POOL(MsgStr);
                #endif

                // clear screen and show banner
                // (now we know we will stay in graphics mode)
                SwitchToGraphics();
            }
            else {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"Loading Placeholder Display");
                LOG(2, LOG_LINE_NORMAL, L"%s", MsgStr);
                MsgLog ("INFO: %s", MsgStr);
                MsgLog ("\n\n");
                MY_FREE_POOL(MsgStr);
                #endif
            }

            if (GlobalConfig.ScreensaverTime != -1) {
                BltClearScreen (TRUE);

                #if REFIT_DEBUG > 0
                MsgStr = (gotGraphics)
                    ? StrDuplicate (L"Displayed Placeholder")
                    : StrDuplicate (L"Switch to Graphics Mode ... Success");
                LOG(3, LOG_THREE_STAR_MID, L"%s", MsgStr);
                MsgLog ("INFO: %s", MsgStr);
                #endif
            }
            else {
                #if REFIT_DEBUG > 0
                MsgLog ("INFO: Changing to Screensaver Display");

                MsgStr = StrDuplicate (L"Configured to Start with Screensaver");
                LOG(3, LOG_THREE_STAR_MID, L"%s", MsgStr);
                MsgLog ("      %s", MsgStr);
                #endif

                // start with screen blanked
                GraphicsScreenDirty = TRUE;
            }
            BannerLoaded = TRUE;

            #if REFIT_DEBUG > 0
            MY_FREE_POOL(MsgStr);
            (NativeLogger && GlobalConfig.LogLevel > 0)
                ? MsgLog ("\n")
                : MsgLog ("\n\n");
            #endif
        }
    }
    else {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"Invalid Screen Mode ... Switching to Text Mode");
        LOG(3, LOG_THREE_STAR_MID, L"%s", MsgStr);
        MsgLog ("WARN: %s", MsgStr);
        MsgLog ("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        AllowGraphicsMode     = FALSE;
        GlobalConfig.TextOnly = TRUE;

        SwitchToText (FALSE);
    }
} // VOID SetupScreen()

VOID SwitchToText (
    IN BOOLEAN CursorEnabled
) {
    EFI_STATUS Status;

    if (!GlobalConfig.TextRenderer && !IsBoot) {
        // Override Text Renderer Setting
        Status = OcUseBuiltinTextOutput (EfiConsoleControlScreenText);

        if (!EFI_ERROR(Status)) {
            GlobalConfig.TextRenderer = TRUE;

            #if REFIT_DEBUG > 0
            MsgLog ("INFO: 'text_renderer' Config Setting Overriden\n\n");
            #endif
        }
    }

    egSetGraphicsModeEnabled (FALSE);
    REFIT_CALL_2_WRAPPER(gST->ConOut->EnableCursor, gST->ConOut, CursorEnabled);

    #if REFIT_DEBUG > 0
    BOOLEAN TextModeOnEntry = (
        egIsGraphicsModeEnabled()
        && !AllowGraphicsMode
        && !IsBoot
    );

    if (TextModeOnEntry) {
        MsgLog ("Determine Text Console Size:\n");
    }
    #endif

    // get size of text console
    Status = REFIT_CALL_4_WRAPPER(
        gST->ConOut->QueryMode,
        gST->ConOut,
        gST->ConOut->Mode->Mode,
        &ConWidth,
        &ConHeight
    );

    if (EFI_ERROR(Status)) {
        // use default values on error
        ConWidth  = 80;
        ConHeight = 25;

        #if REFIT_DEBUG > 0
        if (TextModeOnEntry) {
            MsgLog (
                "  Could Not Get Text Console Size ... Using Default:- '%d x %d'\n\n",
                ConHeight, ConWidth
            );
        }
        #endif
    }
    else {
        #if REFIT_DEBUG > 0
        if (TextModeOnEntry) {
            MsgLog (
                "  Text Console Size:- '%d x %d'\n\n",
                ConWidth, ConHeight
            );
        }
        #endif
    }

    PrepareBlankLine();

    #if REFIT_DEBUG > 0
    if (TextModeOnEntry) {
        MsgLog ("INFO: Switch to Text Mode ... Success\n\n");
    }
    #endif
} // VOID SwitchToText()

EFI_STATUS SwitchToGraphics (VOID) {
    if (AllowGraphicsMode) {
        if (!egIsGraphicsModeEnabled()) {
            egSetGraphicsModeEnabled (TRUE);
            GraphicsScreenDirty = TRUE;

            return EFI_SUCCESS;
        }

        return EFI_ALREADY_STARTED;
    }

    return EFI_NOT_FOUND;
} // EFI_STATUS SwitchToGraphics()

//
// Screen control for running tools
//
VOID BeginTextScreen (
    IN CHAR16 *Title
) {
    DrawScreenHeader (Title);
    SwitchToText (FALSE);

    // reset error flag
    haveError = FALSE;
} // VOID BeginTextScreen()

VOID FinishTextScreen (
    IN BOOLEAN WaitAlways
) {
    if (haveError || WaitAlways) {
        SwitchToText (FALSE);
        PauseForKey();
    }

    // reset error flag
    haveError = FALSE;
} // VOID FinishTextScreen()

VOID BeginExternalScreen (
    IN BOOLEAN  UseGraphicsMode,
    IN CHAR16  *Title
) {
    if (!AllowGraphicsMode) {
        UseGraphicsMode = FALSE;
    }

    if (UseGraphicsMode) {
        #if REFIT_DEBUG > 0
        LOG(2, LOG_LINE_NORMAL, L"Beginning Child Display with Screen Mode:- 'Graphics'");
        #endif

        SwitchToGraphicsAndClear (FALSE);
    }
    else {
        #if REFIT_DEBUG > 0
        LOG(2, LOG_LINE_NORMAL, L"Beginning Child Display with Screen Mode:- 'Text'");
        #endif

        SwitchToText (UseGraphicsMode);
        DrawScreenHeader (Title);
    }

    // reset error flag
    haveError = FALSE;
} // VOID BeginExternalScreen()

VOID FinishExternalScreen (VOID) {
    // make sure we clean up later
    GraphicsScreenDirty = TRUE;

    if (haveError) {
        SwitchToText (FALSE);
        PauseForKey();
    }

    // Reset the screen resolution, in case external program changed it.
    SetupScreen();

    // reset error flag
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

    // clear to black background ... first clear in graphics mode
    egClearScreen (&DarkBackgroundPixel);

    // then clear in text mode
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    REFIT_CALL_1_WRAPPER(gST->ConOut->ClearScreen,  gST->ConOut);

    // paint header background
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BANNER);
    for (i = 0; i < 3; i++) {
        REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, i);
        Print (BlankLine);
    }

    // print header text
    REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 3, 1);
    Print (L"RefindPlus - %s", Title);

    // reposition cursor
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute,      gST->ConOut, ATTR_BASIC);
    REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, 4);
} // VOID DrawScreenHeader()

//
// Keyboard input
//

BOOLEAN ReadAllKeyStrokes (VOID) {
    BOOLEAN       GotKeyStrokes = FALSE;
    BOOLEAN       EmptyBuffer   = FALSE ;
    EFI_STATUS    Status;
    EFI_INPUT_KEY key;

    for (;;) {
        Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
        if (Status == EFI_SUCCESS) {
            GotKeyStrokes = TRUE;
            continue;
        }
        else if (Status == EFI_NOT_READY) {
            EmptyBuffer = TRUE;
        }
        break;
    }

    #if REFIT_DEBUG > 0
    Status = (GotKeyStrokes)
        ? EFI_SUCCESS
        : (EmptyBuffer)
            ? EFI_ALREADY_STARTED
            : Status;

    CHAR16 *MsgStr = PoolPrint (L"Clear Keystroke Buffer ... %r", Status);
    MsgLog ("INFO: %s\n\n", MsgStr);
    LOG(2, LOG_LINE_NORMAL, L"%s", MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    // Flag device error and proceed if present
    // We will try to resolve under the main loop if required
    if (!GotKeyStrokes && !EmptyBuffer) {
        FlushFailedTag = TRUE;
    }
    else if (GotKeyStrokes) {
        ClearedBuffer = TRUE;
    }

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
            MyStrStr (L"Apple", gST->FirmwareVendor) &&
            egIsGraphicsModeEnabled()
        ) {
            egDisplayMessage (Text, &BGColor, PositionCode);
            GraphicsScreenDirty = TRUE;
        }
        else {
            // non-Mac or in text mode; a Print() statement will work
            Print (Text);
            Print (L"\n");
        }
    }
} // VOID PrintUglyText()

VOID PauseForKey (VOID) {
    UINTN   i, WaitOut;
    BOOLEAN Breakout = FALSE;

    MuteLogger = TRUE;
    PrintUglyText (L"", NEXTLINE);
    PrintUglyText (L"", NEXTLINE);
    PrintUglyText (L"                                                          ", NEXTLINE);
    PrintUglyText (L"                                                          ", NEXTLINE);
    PrintUglyText (L"             * Paused for Error or Warning *              ", NEXTLINE);

    if (GlobalConfig.ContinueOnWarning) {
        PrintUglyText (L"        Press a Key or Wait 9 Seconds to Continue         ", NEXTLINE);
    }
    else {
        PrintUglyText (L"                 Press a Key to Continue                  ", NEXTLINE);
    }
    PrintUglyText (L"                                                          ", NEXTLINE);
    PrintUglyText (L"                                                          ", NEXTLINE);
    MuteLogger = FALSE;

    // Clear the Keystroke Buffer
    ReadAllKeyStrokes();

    if (GlobalConfig.ContinueOnWarning) {
        #if REFIT_DEBUG > 0
        LOG(2, LOG_LINE_NORMAL, L"Paused for Error/Warning ... Waiting 9 Seconds");
        #endif

        for (i = 0; i < 9; ++i) {
            WaitOut = WaitForInput (1000);
            if (WaitOut == INPUT_KEY) {
                #if REFIT_DEBUG > 0
                LOG(2, LOG_LINE_NORMAL, L"Pause Terminated by Keypress");
                #endif

                Breakout = TRUE;
            }
            else if (WaitOut == INPUT_TIMER_ERROR) {
                #if REFIT_DEBUG > 0
                LOG(2, LOG_LINE_NORMAL, L"Pause Terminated by Timer Error!!");
                #endif

                Breakout = TRUE;
            }

            if (Breakout) {
                break;
            }
        } // for

        #if REFIT_DEBUG > 0
        if (!Breakout) {
            LOG(2, LOG_LINE_NORMAL, L"Pause Terminated on Timeout");
        }
        #endif
    }
    else {
        #if REFIT_DEBUG > 0
        LOG(2, LOG_LINE_NORMAL, L"Paused for Error/Warning ... Keypress Required");
        #endif

        for (;;) {
            WaitOut = WaitForInput (1000);
            if (WaitOut == INPUT_KEY) {
                #if REFIT_DEBUG > 0
                LOG(2, LOG_LINE_NORMAL, L"Pause Terminated by Keypress");
                #endif

                Breakout = TRUE;
            }
            else if (WaitOut == INPUT_TIMER_ERROR) {
                #if REFIT_DEBUG > 0
                LOG(2, LOG_LINE_NORMAL, L"Pause Terminated by Timer Error!!");
                #endif

                Breakout = TRUE;
            }

            if (Breakout) {
                break;
            }
        } // for
    }

    // Clear the Keystroke Buffer
    ReadAllKeyStrokes();

    GraphicsScreenDirty = TRUE;

    #if REFIT_DEBUG > 0
    LOG(1, LOG_THREE_STAR_SEP, L"Resuming After Pause");
    #endif
} // VOID PauseForKey

// Pause a specified number of seconds
VOID PauseSeconds (
    UINTN Seconds
) {
    UINTN   i, WaitOut;
    BOOLEAN Breakout = FALSE;

    // Clear the Keystroke Buffer
    ReadAllKeyStrokes();

    #if REFIT_DEBUG > 0
    LOG(3, LOG_THREE_STAR_MID, L"Pausing for %d Seconds", Seconds);
    #endif

    for (i = 0; i < Seconds; ++i) {
        WaitOut = WaitForInput (1000);
        if (WaitOut == INPUT_KEY) {
            #if REFIT_DEBUG > 0
            LOG(2, LOG_LINE_NORMAL, L"Pause Terminated by Keypress");
            #endif

            Breakout = TRUE;
        }
        else if (WaitOut == INPUT_TIMER_ERROR) {
            #if REFIT_DEBUG > 0
            LOG(2, LOG_LINE_NORMAL, L"Pause Terminated by Timer Error!!");
            #endif

            Breakout = TRUE;
        }

        if (Breakout) {
            break;
        }
    } // for

    #if REFIT_DEBUG > 0
    if (!Breakout) {
        LOG(2, LOG_LINE_NORMAL, L"Pause Terminated on Timeout");
    }
    #endif

    // Clear the Keystroke Buffer
    ReadAllKeyStrokes();
} // VOID PauseSeconds()

#if REFIT_DEBUG > 0
VOID DebugPause (VOID) {
    // show console and wait for key
    SwitchToText (FALSE);
    PauseForKey();

    // reset error flag
    haveError = FALSE;
} // VOID DebugPause()
#endif

VOID EndlessIdleLoop (VOID) {
    UINTN index;

    for (;;) {
        ReadAllKeyStrokes();
        REFIT_CALL_3_WRAPPER(gBS->WaitForEvent, 1, &gST->ConIn->WaitForKey, &index);
    }
} // VOID EndlessIdleLoop()

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
    MsgLog ("** FATAL ERROR: '%s' %s\n", ErrorName, where);
    #endif

    Temp = PoolPrint (L"Fatal Error: '%s' %s", ErrorName, where);
#else
    Temp = PoolPrint (L"Fatal Error: '%r' %s", Status, where);

    #if REFIT_DEBUG > 0
    MsgLog ("** FATAL ERROR: '%r' %s\n", Status, where);
    #endif
#endif
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText (Temp, NEXTLINE);
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    haveError = TRUE;

    #if REFIT_DEBUG > 0
    LOG(1, LOG_STAR_SEPARATOR, Temp);
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
        return FALSE;
    }

#ifdef __MAKEWITH_GNUEFI
    CHAR16 ErrorName[64];
    StatusToString (ErrorName, Status);

    #if REFIT_DEBUG > 0
    MsgLog ("** WARN: '%s' %s\n", ErrorName, where);
    #endif

    Temp = PoolPrint (L"Error: '%s' %s", ErrorName, where);
#else
    Temp = PoolPrint (L"Error: '%r' %s", Status, where);

    #if REFIT_DEBUG > 0
    MsgLog ("** WARN: '%r' %s\n", Status, where);
    #endif
#endif

    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText (Temp, NEXTLINE);
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

    // Defeat need to "Press a Key to Continue" in debug mode
    if (MyStrStr (where, L"While Reading Boot Sector") ||
        MyStrStr (where, L"in ReadHiddenTags")
    ) {
        haveError = FALSE;
    }
    else {
        haveError = TRUE;
    }

    #if REFIT_DEBUG > 0
    LOG(1, LOG_STAR_SEPARATOR, Temp);
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
    EFI_STATUS Status;

    #if REFIT_DEBUG > 0
    BOOLEAN    gotGraphics = egIsGraphicsModeEnabled();
    #endif

    Status = SwitchToGraphics();
    if (GraphicsScreenDirty) {
        BltClearScreen (ShowBanner);
    }

    #if REFIT_DEBUG > 0
    if (!gotGraphics) {
        MsgLog ("INFO: Restore Graphics Mode ... %r\n\n", Status);
    }
    #endif
} // VOID SwitchToGraphicsAndClear()


// DA-TAG: Permit Image->PixelData Memory Leak on Qemu
//         Apparent Memory Conflict ... Needs Investigation.
//         See: sf.net/p/refind/discussion/general/thread/4dfcdfdd16/
//         Temporary ... Eliminate when no longer required
// UPDATE: Disabled for v0.13.2.AK ... Watch for issue reports
#if 0
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
    static EG_IMAGE  *Banner      = NULL;
           EG_IMAGE  *NewBanner   = NULL;
           INTN       BannerPosX  = 0;
           INTN       BannerPosY  = 0;

    #if REFIT_DEBUG > 0
    static BOOLEAN LoggedBanner;
    CHAR16 *MsgStr = NULL;
    #endif

    #if REFIT_DEBUG > 0
    if (!IsBoot) {
        MsgLog ("Refresh Screen:");
    }
    #endif

    if (!IsBoot
        || (ShowBanner && !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_BANNER))
    ) {
        // Load banner on first call
        if (Banner == NULL) {
            #if REFIT_DEBUG > 0
            MsgLog ("\n");
            MsgLog ("  - Get Banner");
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

            if (Banner == NULL) {
                #if REFIT_DEBUG > 0
                CHAR16 *StrSpacer = L"    ";
                MsgStr = StrDuplicate (L"Default Title Banner");
                MsgLog ("%s%s* %s", OffsetNext, StrSpacer, MsgStr);
                if (!LoggedBanner) {
                    LOG(2, LOG_LINE_NORMAL, L"Using %s", MsgStr);
                }
                MY_FREE_POOL(MsgStr);

                MsgStr = PoolPrint (
                    L"Colour (Base):- '%02X%02X%02X'",
                    MenuBackgroundPixel.r,
                    MenuBackgroundPixel.g,
                    MenuBackgroundPixel.b
                );
                MsgLog (
                    "%s%s  %s",
                    OffsetNext,
                    StrSpacer,
                    MsgStr
                );
                if (!LoggedBanner) {
                    LOG(2, LOG_LINE_NORMAL, L"%s", MsgStr);
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
                // Use complementary colour for font on darkish backgrounds
                // DA-TAG: Do not apply to general text to avoid OTT effect
                if (LumIndex < 128) {
                    // Complementary colour for Mid Grey is Mid Grey
                    // Check for a broadly defined 'Grey' to fix
                    UINTN MaxRGB = (PixelsR > PixelsG) ? PixelsR : PixelsG;
                    MaxRGB       = (MaxRGB  > PixelsB) ? MaxRGB  : PixelsB;

                    UINTN MinRGB = (PixelsR < PixelsG) ? PixelsR : PixelsG;
                    MinRGB       = (MinRGB  < PixelsB) ? MinRGB  : PixelsB;

                    if ((MaxRGB - MinRGB) <= 64) {
                        // We have 'Grey' ... Determine if it is 'Mid Grey'
                        if (MaxRGB < 160 && MinRGB > 96) {
                            // We have 'Mid Grey' ... Set Input Pixels to Black
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
            }
            else {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"Custom Title Banner");
                MsgLog ("%s    * %s", OffsetNext, MsgStr);
                if (!LoggedBanner) {
                    LOG(2, LOG_LINE_NORMAL, L"Using %s", MsgStr);
                    LoggedBanner = TRUE;
                }
                MY_FREE_POOL(MsgStr);
                #endif
            }

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
            MsgLog ("\n");
            MsgLog ("  - Scale Banner");
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
        } // if Banner != NULL

        // clear and draw banner
        #if REFIT_DEBUG > 0
        MsgLog ("\n");
        MsgLog ("  - Clear Screen");
        #endif

        if (GlobalConfig.ScreensaverTime != -1) {
            egClearScreen (&MenuBackgroundPixel);
        }
        else {
            egClearScreen (&BlackPixel);
        }

        if (Banner != NULL) {
            #if REFIT_DEBUG > 0
            MsgLog ("\n");
            MsgLog ("  - Show Banner");
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
    }
    else {
        #if REFIT_DEBUG > 0
        if (!IsBoot) {
            MsgLog ("\n");
            MsgLog ("  - Clear Screen");
        }
        #endif

        // not showing banner
        // clear to menu background color
        egClearScreen (&MenuBackgroundPixel);
    }

    #if REFIT_DEBUG > 0
    if (!IsBoot) {
        MsgLog ("\n\n");
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
