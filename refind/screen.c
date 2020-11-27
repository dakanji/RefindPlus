/*
 * refind/screen.c
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
#include "screen.h"
#include "config.h"
#include "libegint.h"
#include "lib.h"
#include "menu.h"
#include "mystrings.h"
#include "../include/refit_call_wrapper.h"
#include "../include/egemb_refindplus_banner.h"

// Console defines and variables

UINTN ConWidth;
UINTN ConHeight;
CHAR16 *BlankLine = NULL;

// UGA defines and variables

UINTN   ScreenW;
UINTN   ScreenH;
BOOLEAN AllowGraphicsMode;

EG_PIXEL StdBackgroundPixel  = { 0xbf, 0xbf, 0xbf, 0 };
EG_PIXEL MenuBackgroundPixel = { 0xbf, 0xbf, 0xbf, 0 };
EG_PIXEL DarkBackgroundPixel = { 0x0, 0x0, 0x0, 0 };

// general defines and variables
static BOOLEAN GraphicsScreenDirty;
static BOOLEAN haveError = FALSE;


static VOID
PrepareBlankLine (
    VOID
) {
    UINTN i;

    MyFreePool(BlankLine);
    // make a buffer for a whole text line
    BlankLine = AllocatePool((ConWidth + 1) * sizeof(CHAR16));
    for (i = 0; i < ConWidth; i++) {
        BlankLine[i] = ' ';
    }
    BlankLine[i] = 0;
}

//
// Screen initialization and switching
//

VOID
InitScreen (
    VOID
) {
    // initialize libeg
    egInitScreen();

    if (egHasGraphicsMode()) {
        egGetScreenSize(&ScreenW, &ScreenH);
        AllowGraphicsMode = TRUE;
    }
    else {
        AllowGraphicsMode = FALSE;
        egSetTextMode(GlobalConfig.RequestedTextMode);
        egSetGraphicsModeEnabled(FALSE);   // just to be sure we are in text mode
    }
    GraphicsScreenDirty = TRUE;

    // disable cursor
    refit_call2_wrapper(gST->ConOut->EnableCursor, gST->ConOut, FALSE);

    // get size of text console
    if (refit_call4_wrapper(
        gST->ConOut->QueryMode,
        gST->ConOut,
        gST->ConOut->Mode->Mode,
        &ConWidth,
        &ConHeight) != EFI_SUCCESS
    ) {
        // use default values on error
        ConWidth = 80;
        ConHeight = 25;
    }

    PrepareBlankLine();

    // show the banner if in text mode
    if (GlobalConfig.TextOnly && (GlobalConfig.ScreensaverTime != -1)) {
        DrawScreenHeader(L"Initializing...");
    }
}

// Set the screen resolution and mode (text vs. graphics).
VOID
SetupScreen (
    VOID
) {
    UINTN NewWidth, NewHeight;

    #if REFIT_DEBUG > 0
    MsgLog("Setup Screen...\n");
    #endif

    // Convert mode number to horizontal & vertical resolution values
    if ((GlobalConfig.RequestedScreenWidth > 0) && (GlobalConfig.RequestedScreenHeight == 0)) {

        #if REFIT_DEBUG > 0
        MsgLog("Get Resolution From Mode:\n");
        #endif

        egGetResFromMode(&(GlobalConfig.RequestedScreenWidth), &(GlobalConfig.RequestedScreenHeight));
    }

    // Set the believed-to-be current resolution to the LOWER of the current
    // believed-to-be resolution and the requested resolution. This is done to
    // enable setting a lower-than-default resolution.
    if ((GlobalConfig.RequestedScreenWidth > 0) && (GlobalConfig.RequestedScreenHeight > 0)) {

        #if REFIT_DEBUG > 0
        MsgLog("Sync Resolution:\n");
        #endif

       ScreenW = (ScreenW < GlobalConfig.RequestedScreenWidth) ? ScreenW : GlobalConfig.RequestedScreenWidth;
       ScreenH = (ScreenH < GlobalConfig.RequestedScreenHeight) ? ScreenH : GlobalConfig.RequestedScreenHeight;
    }

    // Set text mode. If this requires increasing the size of the graphics mode, do so.
    if (egSetTextMode(GlobalConfig.RequestedTextMode)) {

        #if REFIT_DEBUG > 0
        MsgLog("Set Text Mode:\n");
        #endif

        egGetScreenSize(&NewWidth, &NewHeight);
        if ((NewWidth > ScreenW) || (NewHeight > ScreenH)) {
            ScreenW = NewWidth;
            ScreenH = NewHeight;
        }

        if ((ScreenW > GlobalConfig.RequestedScreenWidth) ||
            (ScreenH > GlobalConfig.RequestedScreenHeight)
        ) {

            #if REFIT_DEBUG > 0
            MsgLog("  - Increase Graphic Mode\n");
            #endif

            // Requested text mode forces us to use a bigger graphics mode
            GlobalConfig.RequestedScreenWidth = ScreenW;
            GlobalConfig.RequestedScreenHeight = ScreenH;
        } // if

        if (GlobalConfig.RequestedScreenWidth > 0) {

            #if REFIT_DEBUG > 0
            MsgLog("Set to User Requested Screen Size:\n");
            #endif

            egSetScreenSize(&(GlobalConfig.RequestedScreenWidth), &(GlobalConfig.RequestedScreenHeight));
            egGetScreenSize(&ScreenW, &ScreenH);
        } // if user requested a particular screen resolution
    }

    if (GlobalConfig.TextOnly) {
        // Set text mode if requested
        AllowGraphicsMode = FALSE;
        SwitchToText(FALSE);

        #if REFIT_DEBUG > 0
        MsgLog("INFO: Set Screen to Text Mode\n\n");
        #endif
    } else if (AllowGraphicsMode) {
        if (!egIsGraphicsModeEnabled()) {
            #if REFIT_DEBUG > 0
            MsgLog("Prepare for Graphics Mode Switch:\n");
            #endif

            // scale icons up for HiDPI Monitors if required
            if (GlobalConfig.ForceUIScale || ScreenH >= HIDPI_MIN) {
                #if REFIT_DEBUG > 0
                if (GlobalConfig.ForceUIScale) {
                    MsgLog("  - HiDPI Monitor Flagged\n");
                }
                else {
                    MsgLog("  - HiDPI Monitor Detected\n");
                }
                MsgLog("    * Scale Icons Up\n", ScreenH);
                #endif

                GlobalConfig.IconSizes[ICON_SIZE_BADGE] *= 2;
                GlobalConfig.IconSizes[ICON_SIZE_SMALL] *= 2;
                GlobalConfig.IconSizes[ICON_SIZE_BIG] *= 2;
                GlobalConfig.IconSizes[ICON_SIZE_MOUSE] *= 2;
            }
            else {
                #if REFIT_DEBUG > 0
                MsgLog("  - HiDPI Monitor Not Detected\n");
                MsgLog("    * Maintain Icon Scale\n", ScreenH);
                #endif
            } // if
            #if REFIT_DEBUG > 0
            MsgLog("    * %dpx Vertical Resolution\n\n", ScreenH);
            MsgLog("INFO: Running Graphics Mode Switch\n\n");
            #endif

            // clear screen and show banner
            // (now we know we will stay in graphics mode)
            SwitchToGraphics();

            if (GlobalConfig.ScreensaverTime != -1) {
                BltClearScreen(TRUE);
            }
            else {
                // start with screen blanked
                GraphicsScreenDirty = TRUE;
            }

            #if REFIT_DEBUG > 0
            MsgLog("INFO: Switched to Graphics Mode\n\n");
            #endif
        }
    }
    else {
        #if REFIT_DEBUG > 0
        MsgLog("WARN: Invalid Screen Mode\n");
        MsgLog("   Switching to Text Mode\n\n");
        #endif

        AllowGraphicsMode = FALSE;
        GlobalConfig.TextOnly = TRUE;
        SwitchToText(FALSE);
    }
} // VOID SetupScreen()

VOID
SwitchToText (
    IN BOOLEAN CursorEnabled
) {
    EFI_STATUS     Status;
    BOOLEAN        GraphicsModeOnEntry;
    STATIC BOOLEAN HaveOverriden = FALSE;

    GraphicsModeOnEntry = egIsGraphicsModeEnabled();

    if (!GlobalConfig.TextRenderer && !HaveOverriden) {
        // Override Text Renderer Setting
        Status = OcUseBuiltinTextOutput (EfiConsoleControlScreenText);
        HaveOverriden = TRUE;

        if (!EFI_ERROR (Status)) {
            #if REFIT_DEBUG > 0
            MsgLog ("INFO: 'text_renderer' Config Setting Overriden\n\n");
            #endif
        }
    }

    egSetGraphicsModeEnabled(FALSE);
    refit_call2_wrapper(gST->ConOut->EnableCursor, gST->ConOut, CursorEnabled);

    #if REFIT_DEBUG > 0
    if (GraphicsModeOnEntry && (!AllowGraphicsMode || GlobalConfig.TextOnly)) {
        MsgLog("Determine Text Console Size:\n");
    }
    #endif

    // get size of text console
    Status = refit_call4_wrapper(
        gST->ConOut->QueryMode,
        gST->ConOut,
        gST->ConOut->Mode->Mode,
        &ConWidth,
        &ConHeight
    );

    if (EFI_ERROR (Status)) {
        // use default values on error
        ConWidth = 80;
        ConHeight = 25;

        #if REFIT_DEBUG > 0
        if (GraphicsModeOnEntry && (!AllowGraphicsMode || GlobalConfig.TextOnly)) {
            MsgLog(
                "  Could not Get Text Console Size ...Using Default: %dx%d\n\n",
                ConHeight,
                ConWidth
            );
        }
        #endif
    }
    else {
        #if REFIT_DEBUG > 0
        if (GraphicsModeOnEntry && (!AllowGraphicsMode || GlobalConfig.TextOnly)) {
            MsgLog(
                "  Text Console Size = %dx%d\n\n",
                ConWidth,
                ConHeight
            );
        }
        #endif
    }
    PrepareBlankLine();

    if (GraphicsModeOnEntry) {
        MsgLog("INFO: Switched to Text Mode\n\n");
    }
}

VOID SwitchToGraphics (
    VOID
) {
    if (AllowGraphicsMode && !egIsGraphicsModeEnabled()) {
        egSetGraphicsModeEnabled(TRUE);
        GraphicsScreenDirty = TRUE;
    }
}

//
// Screen control for running tools
//
VOID
BeginTextScreen (
    IN CHAR16 *Title
) {
    DrawScreenHeader(Title);
    SwitchToText(FALSE);

    // reset error flag
    haveError = FALSE;
}

VOID
FinishTextScreen (
    IN BOOLEAN WaitAlways
) {
    if (haveError || WaitAlways) {
        SwitchToText(FALSE);
        PauseForKey();
    }

    // reset error flag
    haveError = FALSE;
}

VOID
BeginExternalScreen (
    IN BOOLEAN UseGraphicsMode,
    IN CHAR16 *Title
) {
    if (!AllowGraphicsMode) {
        UseGraphicsMode = FALSE;
    }

    if (UseGraphicsMode) {
        SwitchToGraphics();
        BltClearScreen(FALSE);
    }
    else {
        // clear to dark background
        egClearScreen(&DarkBackgroundPixel);
        DrawScreenHeader(Title);
        SwitchToText(TRUE);
    }

    // reset error flag
    haveError = FALSE;
}

VOID
FinishExternalScreen (
    VOID
) {
    // make sure we clean up later
    GraphicsScreenDirty = TRUE;

    if (haveError) {
        SwitchToText(FALSE);
        PauseForKey();
    }

    // Reset the screen resolution, in case external program changed it....
    SetupScreen();

    // reset error flag
    haveError = FALSE;
}

VOID
TerminateScreen (
    VOID
) {
    // clear text screen
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    refit_call1_wrapper(gST->ConOut->ClearScreen, gST->ConOut);

    // enable cursor
    refit_call2_wrapper(gST->ConOut->EnableCursor, gST->ConOut, TRUE);
}

VOID
DrawScreenHeader (
    IN CHAR16 *Title
) {
    UINTN y;

    // clear to black background
    egClearScreen(&DarkBackgroundPixel); // first clear in graphics mode
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    refit_call1_wrapper(gST->ConOut->ClearScreen, gST->ConOut); // then clear in text mode

    // paint header background
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BANNER);
    for (y = 0; y < 3; y++) {
        refit_call3_wrapper(gST->ConOut->SetCursorPosition, gST->ConOut, 0, y);
        Print(BlankLine);
    }

    // print header text
    refit_call3_wrapper(gST->ConOut->SetCursorPosition, gST->ConOut, 3, 1);
    Print(L"rEFInd - %s", Title);

    // reposition cursor
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    refit_call3_wrapper(gST->ConOut->SetCursorPosition, gST->ConOut, 0, 4);
}

//
// Keyboard input
//

BOOLEAN
ReadAllKeyStrokes (
    VOID
) {
    BOOLEAN       GotKeyStrokes;
    EFI_STATUS    Status;
    EFI_INPUT_KEY key;

    GotKeyStrokes = FALSE;
    for (;;) {
        Status = refit_call2_wrapper(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
        if (Status == EFI_SUCCESS) {
            GotKeyStrokes = TRUE;
            continue;
        }
        break;
    }
    return GotKeyStrokes;
}

// Displays *text without regard to appearances. Used mainly for debugging
// and rare error messages.
// Position code is used only in graphics mode.
// TODO: Improve to handle multi-line text.
VOID
PrintUglyText (
    IN CHAR16 *Text,
    IN UINTN PositionCode
) {
    EG_PIXEL BGColor = COLOR_RED;

    if (Text) {
        if (AllowGraphicsMode && MyStriCmp(L"Apple", gST->FirmwareVendor) && egIsGraphicsModeEnabled()) {
            egDisplayMessage(Text, &BGColor, PositionCode);
            GraphicsScreenDirty = TRUE;
        }
        else { // non-Mac or in text mode; a Print() statement will work
            Print(Text);
            Print(L"\n");
        } // if/else
    } // if
} // VOID PrintUglyText()

VOID
HaltForKey (
    VOID
) {
    UINTN index;

    Print(L"\n");

    PrintUglyText(L"", NEXTLINE);
    PrintUglyText(L"* Halted: Press Any Key to Continue *", NEXTLINE);

    if (ReadAllKeyStrokes()) {  // remove buffered key strokes
        refit_call1_wrapper(gBS->Stall, 5000000);     // 5 seconds delay
        ReadAllKeyStrokes();    // empty the buffer again
    }

    refit_call3_wrapper(gBS->WaitForEvent, 1, &gST->ConIn->WaitForKey, &index);

    GraphicsScreenDirty = TRUE;
    ReadAllKeyStrokes();        // empty the buffer to protect the menu
}

VOID
PauseForKey (
    VOID
) {
    UINTN index;

    Print(L"\n");

    if (GlobalConfig.ContinueOnWarning) {
        PrintUglyText(L"", NEXTLINE);
        PrintUglyText(L"* Paused for Error/Warning. Wait 3 Seconds *", NEXTLINE);
    }
    else {
        PrintUglyText(L"", NEXTLINE);
        PrintUglyText(L"* Paused: Press Any Key to Continue *", NEXTLINE);
    }

    if (GlobalConfig.ContinueOnWarning) {
        refit_call1_wrapper(gBS->Stall, 3000000);
    }
    else {
        if (ReadAllKeyStrokes()) {  // remove buffered key strokes
            refit_call1_wrapper(gBS->Stall, 5000000);     // 5 seconds delay
            ReadAllKeyStrokes();    // empty the buffer again
        }

        refit_call3_wrapper(gBS->WaitForEvent, 1, &gST->ConIn->WaitForKey, &index);
    }

    GraphicsScreenDirty = TRUE;
    ReadAllKeyStrokes();        // empty the buffer to protect the menu
}

// Pause a specified number of seconds
VOID
PauseSeconds (
    UINTN Seconds
) {
    refit_call1_wrapper(gBS->Stall, 1000000 * Seconds);
} // VOID PauseSeconds()

#if REFIT_DEBUG > 0
VOID
DebugPause (
    VOID
) {
    // show console and wait for key
    SwitchToText(FALSE);
    PauseForKey();

    // reset error flag
    haveError = FALSE;
}
#endif

VOID
EndlessIdleLoop (
    VOID
) {
    UINTN index;

    for (;;) {
        ReadAllKeyStrokes();
        refit_call3_wrapper(gBS->WaitForEvent, 1, &gST->ConIn->WaitForKey, &index);
    }
}

//
// Error handling
//

BOOLEAN
CheckFatalError (
    IN EFI_STATUS Status,
    IN CHAR16 *where
) {
    CHAR16 *Temp = NULL;

    if (!EFI_ERROR(Status)) {
        return FALSE;
    }

#ifdef __MAKEWITH_GNUEFI
    CHAR16 ErrorName[64];
    StatusToString(ErrorName, Status);

    #if REFIT_DEBUG > 0
    MsgLog("** FATAL ERROR: %s %s\n", ErrorName, where);
    #endif

    Temp = PoolPrint(L"Fatal Error: %s %s", ErrorName, where);
#else
    Temp = PoolPrint(L"Fatal Error: %s %s", Status, where);

    #if REFIT_DEBUG > 0
    MsgLog("** FATAL ERROR: %r %s\n", Status, where);
    #endif
#endif
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText(Temp, NEXTLINE);
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    haveError = TRUE;
    MyFreePool(Temp);

    return TRUE;
} // BOOLEAN CheckFatalError()

BOOLEAN
CheckError (
    IN EFI_STATUS Status,
    IN CHAR16 *where
) {
    CHAR16 *Temp = NULL;

    if (!EFI_ERROR(Status)) {
        return FALSE;
    }

#ifdef __MAKEWITH_GNUEFI
    CHAR16 ErrorName[64];
    StatusToString(ErrorName, Status);

    #if REFIT_DEBUG > 0
    MsgLog("** WARN: %s %s\n", ErrorName, where);
    #endif

    Temp = PoolPrint(L"Error: %s %s", ErrorName, where);
#else
    Temp = PoolPrint(L"Error: %r %s", Status, where);

    #if REFIT_DEBUG > 0
    MsgLog("** WARN: %r %s\n", Status, where);
    #endif
#endif

    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText(Temp, NEXTLINE);
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

    // Defeat need to "Press a key to continue" in debug mode
    if (MyStrStr(where, L"While Reading Boot Sector") != NULL ||
        MyStrStr(where, L"in ReadHiddenTags") != NULL
    ) {
        haveError = FALSE;
    }
    else {
        haveError = TRUE;
    }

    MyFreePool(Temp);

    return haveError;
} // BOOLEAN CheckError()

//
// Graphics functions
//

VOID
SwitchToGraphicsAndClear (
    VOID
) {
    SwitchToGraphics();
    if (GraphicsScreenDirty) {
        BltClearScreen(TRUE);
    }
}

VOID
BltClearScreen (
    BOOLEAN ShowBanner
) {
    static EG_IMAGE *Banner = NULL;
    EG_IMAGE *NewBanner = NULL;
    INTN BannerPosX, BannerPosY;
    EG_PIXEL Black = { 0x0, 0x0, 0x0, 0 };

    if (ShowBanner && !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_BANNER)) {
        #if REFIT_DEBUG > 0
        MsgLog("Refresh Screen:\n");
        #endif
        // load banner on first call
        if (Banner == NULL) {
            #if REFIT_DEBUG > 0
            MsgLog("  - Get Banner\n");
            #endif

            if (GlobalConfig.BannerFileName) {
                Banner = egLoadImage(SelfDir, GlobalConfig.BannerFileName, FALSE);
            }
            if (Banner == NULL) {
                #if REFIT_DEBUG > 0
                MsgLog("    * Using Embedded Banner\n");
                #endif
                Banner = egPrepareEmbeddedImage(&egemb_refindplus_banner, FALSE);
            }
            else {
                #if REFIT_DEBUG > 0
                MsgLog("    * Using Custom Banner\n");
                #endif
            }
        }

        if (Banner) {
            #if REFIT_DEBUG > 0
            MsgLog("  - Scale Banner\n");
            #endif

           if (GlobalConfig.BannerScale == BANNER_FILLSCREEN) {
              if ((Banner->Height != ScreenH) || (Banner->Width != ScreenW)) {
                 NewBanner = egScaleImage(Banner, ScreenW, ScreenH);
              } // if
           }
           else if ((Banner->Width > ScreenW) || (Banner->Height > ScreenH)) {
              NewBanner = egCropImage(
                  Banner, 0, 0,
                  (Banner->Width > ScreenW) ? ScreenW : Banner->Width,
                  (Banner->Height > ScreenH) ? ScreenH : Banner->Height
              );
           } // if/elseif
           if (NewBanner) {
              egFreeImage(Banner);
              Banner = NewBanner;
           }
           MenuBackgroundPixel = Banner->PixelData[0];
        } // if Banner exists

        // clear and draw banner
        #if REFIT_DEBUG > 0
        MsgLog("  - Clear Screen\n");
        #endif

        if (GlobalConfig.ScreensaverTime != -1) {
            egClearScreen(&MenuBackgroundPixel);
        }
        else {
            egClearScreen(&Black);
        }

        if (Banner != NULL) {
            #if REFIT_DEBUG > 0
            MsgLog("  - Show Banner\n\n");
            #endif

            BannerPosX = (Banner->Width < ScreenW) ? ((ScreenW - Banner->Width) / 2) : 0;
            BannerPosY = (INTN) (ComputeRow0PosY() / 2) - (INTN) Banner->Height;
            if (BannerPosY < 0) {
                BannerPosY = 0;
            }
            GlobalConfig.BannerBottomEdge = BannerPosY + Banner->Height;
            if (GlobalConfig.ScreensaverTime != -1) {
                BltImage(Banner, (UINTN) BannerPosX, (UINTN) BannerPosY);
            }
            egFreeImage(Banner);
        }

    }
    else { // not showing banner
        // clear to menu background color
        egClearScreen(&MenuBackgroundPixel);
    }

    GraphicsScreenDirty = FALSE;
    egFreeImage(GlobalConfig.ScreenBackground);
    GlobalConfig.ScreenBackground = egCopyScreen();
} // VOID BltClearScreen()


VOID
BltImage (
    IN EG_IMAGE *Image,
    IN UINTN XPos,
    IN UINTN YPos
) {
    egDrawImage(Image, XPos, YPos);
    GraphicsScreenDirty = TRUE;
}

VOID
BltImageAlpha (
    IN EG_IMAGE *Image,
    IN UINTN XPos,
    IN UINTN YPos,
    IN EG_PIXEL *BackgroundPixel
) {
    EG_IMAGE *CompImage;

    // compose on background
    CompImage = egCreateFilledImage(Image->Width, Image->Height, FALSE, BackgroundPixel);
    egComposeImage(CompImage, Image, 0, 0);

    // blit to screen and clean up
    egDrawImage(CompImage, XPos, YPos);
    egFreeImage(CompImage);
    GraphicsScreenDirty = TRUE;
}

//VOID
//BltImageComposite (
//    IN EG_IMAGE *BaseImage,
//    IN EG_IMAGE *TopImage,
//    IN UINTN XPos,
//    IN UINTN YPos
//) {
//    UINTN TotalWidth, TotalHeight, CompWidth, CompHeight, OffsetX, OffsetY;
//    EG_IMAGE *CompImage;
//
//    // initialize buffer with base image
//    CompImage = egCopyImage(BaseImage);
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
//    egComposeImage(CompImage, TopImage, OffsetX, OffsetY);
//
//    // blit to screen and clean up
//    egDrawImage(CompImage, XPos, YPos);
//    egFreeImage(CompImage);
//    GraphicsScreenDirty = TRUE;
//}

VOID
BltImageCompositeBadge (
    IN EG_IMAGE *BaseImage,
    IN EG_IMAGE *TopImage,
    IN EG_IMAGE *BadgeImage,
    IN UINTN    XPos,
    IN UINTN    YPos
) {
     UINTN    TotalWidth  = 0;
     UINTN    TotalHeight = 0;
     UINTN    CompWidth   = 0;
     UINTN    CompHeight  = 0;
     UINTN    OffsetX     = 0;
     UINTN    OffsetY     = 0;
     EG_IMAGE *CompImage  = NULL;

     // initialize buffer with base image
     if (BaseImage != NULL) {
         CompImage = egCopyImage(BaseImage);
         TotalWidth  = BaseImage->Width;
         TotalHeight = BaseImage->Height;
     }

     // place the top image
     if ((TopImage != NULL) && (CompImage != NULL)) {
         CompWidth = TopImage->Width;
         if (CompWidth > TotalWidth) {
             CompWidth = TotalWidth;
         }
         OffsetX = (TotalWidth - CompWidth) >> 1;
         CompHeight = TopImage->Height;
         if (CompHeight > TotalHeight) {
             CompHeight = TotalHeight;
         }
         OffsetY = (TotalHeight - CompHeight) >> 1;
         egComposeImage(CompImage, TopImage, OffsetX, OffsetY);
     }

     // place the badge image
     if (BadgeImage != NULL && CompImage != NULL &&
         (BadgeImage->Width + 8) < CompWidth &&
         (BadgeImage->Height + 8) < CompHeight
     ) {
         OffsetX += CompWidth  - 8 - BadgeImage->Width;
         OffsetY += CompHeight - 8 - BadgeImage->Height;
         egComposeImage(CompImage, BadgeImage, OffsetX, OffsetY);
     }

     // blit to screen and clean up
     if (CompImage != NULL) {
         if (CompImage->HasAlpha) {
             egDrawImageWithTransparency(
                 CompImage,
                 NULL,
                 XPos,
                 YPos,
                 CompImage->Width,
                 CompImage->Height
             );
         }
         else {
             egDrawImage(CompImage, XPos, YPos);
         }
         egFreeImage(CompImage);
         GraphicsScreenDirty = TRUE;
     }
}
