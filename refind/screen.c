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
#include "log.h"
#include "../include/refit_call_wrapper.h"

#include "../include/egemb_refind_banner.h"

// Console defines and variables

UINTN ConWidth;
UINTN ConHeight;
CHAR16 *BlankLine = NULL;

// UGA defines and variables

UINTN UGAWidth;
UINTN UGAHeight;
BOOLEAN AllowGraphicsMode;
BOOLEAN HaveResized = FALSE;

EG_PIXEL StdBackgroundPixel  = { 0xbf, 0xbf, 0xbf, 0 };
EG_PIXEL MenuBackgroundPixel = { 0xbf, 0xbf, 0xbf, 0 };
EG_PIXEL DarkBackgroundPixel = { 0x0, 0x0, 0x0, 0 };

static BOOLEAN GraphicsScreenDirty;

// general defines and variables

static VOID PrepareBlankLine(VOID) {
    UINTN i;

    MyFreePool(BlankLine);
    // make a buffer for a whole text line
    BlankLine = AllocatePool((ConWidth + 1) * sizeof(CHAR16));
    for (i = 0; i < ConWidth; i++)
        BlankLine[i] = ' ';
    BlankLine[i] = 0;
}

//
// Screen initialization and switching
//

VOID InitScreen(VOID)
{
    LOG(1, LOG_LINE_NORMAL, L"Entering InitScreen()");
    // initialize libeg
    egInitScreen();

    if (egHasGraphicsMode()) {
        LOG(2, LOG_LINE_NORMAL, L"Have graphics mode; setting screen size");
        egGetScreenSize(&UGAWidth, &UGAHeight);
        AllowGraphicsMode = TRUE;
    } else {
        LOG(2, LOG_LINE_NORMAL, L"No graphics mode detected; setting text mode");
        AllowGraphicsMode = FALSE;
        egSetTextMode(GlobalConfig.RequestedTextMode);
        egSetGraphicsModeEnabled(FALSE);   // just to be sure we are in text mode
    }
    GraphicsScreenDirty = TRUE;

    // disable cursor
    refit_call2_wrapper(ST->ConOut->EnableCursor, ST->ConOut, FALSE);

    // get size of text console
    if (refit_call4_wrapper(ST->ConOut->QueryMode, ST->ConOut,
                            ST->ConOut->Mode->Mode, &ConWidth,
                            &ConHeight) != EFI_SUCCESS) {
        // use default values on error
        ConWidth = 80;
        ConHeight = 25;
    }

    PrepareBlankLine();

    // show the banner if in text mode
    if (GlobalConfig.TextOnly && (GlobalConfig.ScreensaverTime != -1))
       DrawScreenHeader(L"Initializing...");
}

// Set the screen resolution and mode (text vs. graphics).
VOID SetupScreen(VOID)
{
    UINTN NewWidth, NewHeight;

    LOG(1, LOG_LINE_NORMAL, L"Setting screen resolution and mode");
    // Convert mode number to horizontal & vertical resolution values
    if ((GlobalConfig.RequestedScreenWidth > 0) && (GlobalConfig.RequestedScreenHeight == 0))
       egGetResFromMode(&(GlobalConfig.RequestedScreenWidth), &(GlobalConfig.RequestedScreenHeight));

    // Set the believed-to-be current resolution to the LOWER of the current
    // believed-to-be resolution and the requested resolution. This is done to
    // enable setting a lower-than-default resolution.
    if ((GlobalConfig.RequestedScreenWidth > 0) && (GlobalConfig.RequestedScreenHeight > 0)) {
       UGAWidth = (UGAWidth < GlobalConfig.RequestedScreenWidth) ? UGAWidth : GlobalConfig.RequestedScreenWidth;
       UGAHeight = (UGAHeight < GlobalConfig.RequestedScreenHeight) ? UGAHeight : GlobalConfig.RequestedScreenHeight;
       LOG(2, LOG_LINE_NORMAL, L"Recording current resolution as %dx%d", UGAWidth, UGAHeight);
    }

    // Set text mode. If this requires increasing the size of the graphics mode, do so.
    if (egSetTextMode(GlobalConfig.RequestedTextMode)) {
       egGetScreenSize(&NewWidth, &NewHeight);
       if ((NewWidth > UGAWidth) || (NewHeight > UGAHeight)) {
          UGAWidth = NewWidth;
          UGAHeight = NewHeight;
          LOG(2, LOG_LINE_NORMAL, L"After setting text mode, recording new current resolution as %dx%d",
              UGAWidth, UGAHeight);
       }
       if ((UGAWidth > GlobalConfig.RequestedScreenWidth) ||
           (UGAHeight > GlobalConfig.RequestedScreenHeight)) {
           LOG(2, LOG_LINE_NORMAL, L"Adjusting requested screen size based on actual screen size");
           // Requested text mode forces us to use a bigger graphics mode
           GlobalConfig.RequestedScreenWidth = UGAWidth;
           GlobalConfig.RequestedScreenHeight = UGAHeight;
       } // if
    }

    if (GlobalConfig.RequestedScreenWidth > 0) {
       LOG(2, LOG_LINE_NORMAL, L"Setting screen size to %dx%d", GlobalConfig.RequestedScreenWidth,
           GlobalConfig.RequestedScreenHeight);
       egSetScreenSize(&(GlobalConfig.RequestedScreenWidth), &(GlobalConfig.RequestedScreenHeight));
       egGetScreenSize(&UGAWidth, &UGAHeight);
    } // if user requested a particular screen resolution

    if (GlobalConfig.TextOnly) {
        LOG(2, LOG_LINE_NORMAL, L"Setting text-only mode");
        // switch to text mode if requested
        AllowGraphicsMode = FALSE;
        SwitchToText(FALSE);
    } else if (AllowGraphicsMode) {
        // clear screen and show banner
        // (now we know we'll stay in graphics mode)
        if ((UGAWidth >= HIDPI_MIN) && !HaveResized) {
            LOG(2, LOG_LINE_NORMAL, L"Doubling icon sizes for HiDPI display");
            GlobalConfig.IconSizes[ICON_SIZE_BADGE] *= 2;
            GlobalConfig.IconSizes[ICON_SIZE_SMALL] *= 2;
            GlobalConfig.IconSizes[ICON_SIZE_BIG] *= 2;
            GlobalConfig.IconSizes[ICON_SIZE_MOUSE] *= 2;
            HaveResized = TRUE;
        } // if
        SwitchToGraphics();
        if (GlobalConfig.ScreensaverTime != -1) {
           BltClearScreen(TRUE);
        } else { // start with screen blanked
           GraphicsScreenDirty = TRUE;
        }
    }
} // VOID SetupScreen()

VOID SwitchToText(IN BOOLEAN CursorEnabled)
{
    egSetGraphicsModeEnabled(FALSE);
    refit_call2_wrapper(ST->ConOut->EnableCursor, ST->ConOut, CursorEnabled);
    // get size of text console
    if (refit_call4_wrapper(ST->ConOut->QueryMode, ST->ConOut, ST->ConOut->Mode->Mode, &ConWidth, &ConHeight) != EFI_SUCCESS) {
        // use default values on error
        ConWidth = 80;
        ConHeight = 25;
    }
    PrepareBlankLine();
}

VOID SwitchToGraphics(VOID)
{
    if (AllowGraphicsMode && !egIsGraphicsModeEnabled()) {
        egSetGraphicsModeEnabled(TRUE);
        GraphicsScreenDirty = TRUE;
    }
}

//
// Screen control for running tools
//

VOID BeginTextScreen(IN CHAR16 *Title)
{
    DrawScreenHeader(Title);
    SwitchToText(FALSE);
}

VOID BeginExternalScreen(IN BOOLEAN UseGraphicsMode, IN CHAR16 *Title)
{
    if (!AllowGraphicsMode)
        UseGraphicsMode = FALSE;

    if (UseGraphicsMode) {
        SwitchToGraphics();
        BltClearScreen(FALSE);
    } else {
        egClearScreen(&DarkBackgroundPixel);
        DrawScreenHeader(Title);
        SwitchToText(TRUE);
    }
}

VOID FinishExternalScreen(VOID)
{
    // make sure we clean up later
    GraphicsScreenDirty = TRUE;

    // Reset the screen resolution, in case external program changed it....
    SetupScreen();
}

VOID TerminateScreen(VOID)
{
    // clear text screen
    refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_BASIC);
    refit_call1_wrapper(ST->ConOut->ClearScreen, ST->ConOut);

    // enable cursor
    refit_call2_wrapper(ST->ConOut->EnableCursor, ST->ConOut, TRUE);
}

VOID DrawScreenHeader(IN CHAR16 *Title)
{
    UINTN y;

    // clear to black background
    egClearScreen(&DarkBackgroundPixel); // first clear in graphics mode
    refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_BASIC);
    refit_call1_wrapper(ST->ConOut->ClearScreen, ST->ConOut); // then clear in text mode

    // paint header background
    refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_BANNER);
    for (y = 0; y < 3; y++) {
        refit_call3_wrapper(ST->ConOut->SetCursorPosition, ST->ConOut, 0, y);
        Print(BlankLine);
    }

    // print header text
    refit_call3_wrapper(ST->ConOut->SetCursorPosition, ST->ConOut, 3, 1);
    Print(L"rEFInd - %s", Title);

    // reposition cursor
    refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_BASIC);
    refit_call3_wrapper(ST->ConOut->SetCursorPosition, ST->ConOut, 0, 4);
}

//
// Keyboard input
//

BOOLEAN ReadAllKeyStrokes(VOID)
{
    BOOLEAN       GotKeyStrokes;
    EFI_STATUS    Status;
    EFI_INPUT_KEY key;

    GotKeyStrokes = FALSE;
    for (;;) {
        Status = refit_call2_wrapper(ST->ConIn->ReadKeyStroke, ST->ConIn, &key);
        if (Status == EFI_SUCCESS) {
            GotKeyStrokes = TRUE;
            continue;
        }
        break;
    }
    return GotKeyStrokes;
}

// Displays *Text without regard to appearances. Used mainly for debugging
// and rare error messages.
// Position code is used only in graphics mode.
// TODO: Improve to handle multi-line text.
VOID PrintUglyText(IN CHAR16 *Text, IN UINTN PositionCode) {
    EG_PIXEL BGColor = COLOR_RED;

    if (Text) {
        if (AllowGraphicsMode && MyStriCmp(L"Apple", ST->FirmwareVendor) && egIsGraphicsModeEnabled()) {
            egDisplayMessage(Text, &BGColor, PositionCode);
            GraphicsScreenDirty = TRUE;
        } else { // non-Mac or in text mode; a Print() statement will work
            Print(Text);
            Print(L"\n");
        } // if/else
    } // if
} // VOID PrintUglyText()

VOID PauseForKey(VOID)
{
    UINTN index;

    Print(L"\n");
    PrintUglyText(L"* Hit any key to continue *", BOTTOM);

    if (ReadAllKeyStrokes()) {  // remove buffered key strokes
        refit_call1_wrapper(BS->Stall, 5000000);     // 5 seconds delay
        ReadAllKeyStrokes();    // empty the buffer again
    }

    refit_call3_wrapper(BS->WaitForEvent, 1, &ST->ConIn->WaitForKey, &index);
    ReadAllKeyStrokes();        // empty the buffer to protect the menu
}

// Pause a specified number of seconds
VOID PauseSeconds(UINTN Seconds) {
     refit_call1_wrapper(BS->Stall, 1000000 * Seconds);
} // VOID PauseSeconds()

VOID EndlessIdleLoop(VOID)
{
    UINTN index;

    for (;;) {
        ReadAllKeyStrokes();
        refit_call3_wrapper(BS->WaitForEvent, 1, &ST->ConIn->WaitForKey, &index);
    }
}

//
// Error handling
//

BOOLEAN CheckFatalError(IN EFI_STATUS Status, IN CHAR16 *where)
{
    CHAR16 *Temp = NULL;

    if (!EFI_ERROR(Status))
        return FALSE;

#ifdef __MAKEWITH_GNUEFI
    CHAR16 ErrorName[64];
    StatusToString(ErrorName, Status);
    Temp = PoolPrint(L"Fatal Error: %s %s", ErrorName, where);
#else
    Temp = PoolPrint(L"Fatal Error: %s %s", Status, where);
#endif
    refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_ERROR);
    PrintUglyText(Temp, NEXTLINE);
    refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_BASIC);
    LOG(1, LOG_LINE_NORMAL, Temp);
    MyFreePool(Temp);

    return TRUE;
} // BOOLEAN CheckFatalError()

BOOLEAN CheckError(IN EFI_STATUS Status, IN CHAR16 *where)
{
    CHAR16 *Temp = NULL;

    if (!EFI_ERROR(Status))
        return FALSE;

#ifdef __MAKEWITH_GNUEFI
    CHAR16 ErrorName[64];
    StatusToString(ErrorName, Status);
    Temp = PoolPrint(L"Error: %s %s", ErrorName, where);
#else
    Temp = PoolPrint(L"Error: %r %s", Status, where);
#endif
    refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_ERROR);
    PrintUglyText(Temp, NEXTLINE);
    refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_BASIC);
    LOG(1, LOG_LINE_NORMAL, Temp);
    MyFreePool(Temp);

    return TRUE;
} // BOOLEAN CheckError()

//
// Graphics functions
//

VOID SwitchToGraphicsAndClear(VOID)
{
    SwitchToGraphics();
    if (GraphicsScreenDirty)
        BltClearScreen(TRUE);
}

VOID BltClearScreen(BOOLEAN ShowBanner)
{
    static EG_IMAGE *Banner = NULL;
    EG_IMAGE *NewBanner = NULL;
    INTN BannerPosX, BannerPosY;
    EG_PIXEL Black = { 0x0, 0x0, 0x0, 0 };

    if (ShowBanner && !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_BANNER)) {
        // load banner on first call
        if (Banner == NULL) {
            if (GlobalConfig.BannerFileName)
                Banner = egLoadImage(SelfDir, GlobalConfig.BannerFileName, FALSE);
            if (Banner == NULL)
                Banner = egPrepareEmbeddedImage(&egemb_refind_banner, FALSE);
        }

        if (Banner) {
           if (GlobalConfig.BannerScale == BANNER_FILLSCREEN) {
              if ((Banner->Height != UGAHeight) || (Banner->Width != UGAWidth)) {
                 NewBanner = egScaleImage(Banner, UGAWidth, UGAHeight);
              } // if
           } else if ((Banner->Width > UGAWidth) || (Banner->Height > UGAHeight)) {
              NewBanner = egCropImage(Banner, 0, 0, (Banner->Width > UGAWidth) ? UGAWidth : Banner->Width,
                                      (Banner->Height > UGAHeight) ? UGAHeight : Banner->Height);
           } // if/elseif
           if (NewBanner) {
              egFreeImage(Banner);
              Banner = NewBanner;
           }
           MenuBackgroundPixel = Banner->PixelData[0];
        } // if Banner exists

        // clear and draw banner
        if (GlobalConfig.ScreensaverTime != -1)
           egClearScreen(&MenuBackgroundPixel);
        else
           egClearScreen(&Black);

        if (Banner != NULL) {
            BannerPosX = (Banner->Width < UGAWidth) ? ((UGAWidth - Banner->Width) / 2) : 0;
            BannerPosY = (INTN) (ComputeRow0PosY() / 2) - (INTN) Banner->Height;
            if (BannerPosY < 0)
               BannerPosY = 0;
            GlobalConfig.BannerBottomEdge = BannerPosY + Banner->Height;
            if (GlobalConfig.ScreensaverTime != -1)
               BltImage(Banner, (UINTN) BannerPosX, (UINTN) BannerPosY);
        }

    } else { // not showing banner
        // clear to menu background color
        egClearScreen(&MenuBackgroundPixel);
    }

    GraphicsScreenDirty = FALSE;
    egFreeImage(GlobalConfig.ScreenBackground);
    GlobalConfig.ScreenBackground = egCopyScreen();
} // VOID BltClearScreen()


VOID BltImage(IN EG_IMAGE *Image, IN UINTN XPos, IN UINTN YPos)
{
    egDrawImage(Image, XPos, YPos);
    GraphicsScreenDirty = TRUE;
}

VOID BltImageAlpha(IN EG_IMAGE *Image, IN UINTN XPos, IN UINTN YPos, IN EG_PIXEL *BackgroundPixel)
{
    EG_IMAGE *CompImage;

    // compose on background
    CompImage = egCreateFilledImage(Image->Width, Image->Height, FALSE, BackgroundPixel);
    egComposeImage(CompImage, Image, 0, 0);

    // blit to screen and clean up
    egDrawImage(CompImage, XPos, YPos);
    egFreeImage(CompImage);
    GraphicsScreenDirty = TRUE;
}

// VOID BltImageComposite(IN EG_IMAGE *BaseImage, IN EG_IMAGE *TopImage, IN UINTN XPos, IN UINTN YPos)
// {
//     UINTN TotalWidth, TotalHeight, CompWidth, CompHeight, OffsetX, OffsetY;
//     EG_IMAGE *CompImage;
//     
//     // initialize buffer with base image
//     CompImage = egCopyImage(BaseImage);
//     TotalWidth  = BaseImage->Width;
//     TotalHeight = BaseImage->Height;
//     
//     // place the top image
//     CompWidth = TopImage->Width;
//     if (CompWidth > TotalWidth)
//         CompWidth = TotalWidth;
//     OffsetX = (TotalWidth - CompWidth) >> 1;
//     CompHeight = TopImage->Height;
//     if (CompHeight > TotalHeight)
//         CompHeight = TotalHeight;
//     OffsetY = (TotalHeight - CompHeight) >> 1;
//     egComposeImage(CompImage, TopImage, OffsetX, OffsetY);
//
//     // blit to screen and clean up
//     egDrawImage(CompImage, XPos, YPos);
//     egFreeImage(CompImage);
//     GraphicsScreenDirty = TRUE;
// }

VOID BltImageCompositeBadge(IN EG_IMAGE *BaseImage,
                            IN EG_IMAGE *TopImage,
                            IN EG_IMAGE *BadgeImage,
                            IN UINTN XPos,
                            IN UINTN YPos)
{
     UINTN TotalWidth = 0, TotalHeight = 0, CompWidth = 0, CompHeight = 0, OffsetX = 0, OffsetY = 0;
     EG_IMAGE *CompImage = NULL;

     // initialize buffer with base image
     if (BaseImage != NULL) {
         CompImage = egCopyImage(BaseImage);
         TotalWidth  = BaseImage->Width;
         TotalHeight = BaseImage->Height;
     }

     // place the top image
     if ((TopImage != NULL) && (CompImage != NULL)) {
         CompWidth = TopImage->Width;
         if (CompWidth > TotalWidth)
               CompWidth = TotalWidth;
         OffsetX = (TotalWidth - CompWidth) >> 1;
         CompHeight = TopImage->Height;
         if (CompHeight > TotalHeight)
               CompHeight = TotalHeight;
         OffsetY = (TotalHeight - CompHeight) >> 1;
         egComposeImage(CompImage, TopImage, OffsetX, OffsetY);
     }

     // place the badge image
     if (BadgeImage != NULL && CompImage != NULL && (BadgeImage->Width + 8) < CompWidth && (BadgeImage->Height + 8) < CompHeight) {
         OffsetX += CompWidth  - 8 - BadgeImage->Width;
         OffsetY += CompHeight - 8 - BadgeImage->Height;
         egComposeImage(CompImage, BadgeImage, OffsetX, OffsetY);
     }

     // blit to screen and clean up
     if (CompImage != NULL) {
         if (CompImage->HasAlpha)
             egDrawImageWithTransparency(CompImage, NULL, XPos, YPos, CompImage->Width, CompImage->Height);
         else
             egDrawImage(CompImage, XPos, YPos);
         egFreeImage(CompImage);
         GraphicsScreenDirty = TRUE;
     }
}
