/*
 * BootMaster/screenmgt.h
 * Screen management header file
 *
 * Copyright (c) 2006-2009 Christoph Pfisterer
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
 * Modified for RefindPlus
 * Copyright (c) 2020-2022 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#ifndef __SCREEN_H_
#define __SCREEN_H_

#ifdef __MAKEWITH_GNUEFI
#include "efi.h"
#include "efilib.h"
#else
#include "../include/tiano_includes.h"
#endif

#include "libeg.h"

//
// Screen module
//

#define DONT_CHANGE_TEXT_MODE 1024 /* textmode # that is a code to not change the text mode */

#define ATTR_BASIC (EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK)
#define ATTR_ERROR (EFI_YELLOW | EFI_BACKGROUND_BLACK)
#define ATTR_BANNER (EFI_WHITE | EFI_BACKGROUND_BLUE)
#define ATTR_CHOICE_BASIC ATTR_BASIC
#define ATTR_CHOICE_CURRENT (EFI_WHITE | EFI_BACKGROUND_GREEN)
#define ATTR_SCROLLARROW (EFI_LIGHTGREEN | EFI_BACKGROUND_BLACK)

//#define LAYOUT_TEXT_WIDTH (512)
//#define LAYOUT_TEXT_WIDTH (425)
#define LAYOUT_BANNER_YGAP 32

//#define FONT_CELL_WIDTH (7)
//#define FONT_CELL_HEIGHT (12)

// Codes for text position, used by egDisplayMessage() and PrintUglyText()
#define CENTER 0
#define BOTTOM 1
#define TOP 2
#define NEXTLINE 3

extern UINTN     ConWidth;
extern UINTN     ConHeight;
extern UINTN     ScreenW;
extern UINTN     ScreenH;
extern CHAR16   *BlankLine;
extern BOOLEAN   AllowGraphicsMode;

EFI_STATUS SwitchToGraphics (VOID);

EG_PIXEL FontComplement (VOID);

BOOLEAN ReadAllKeyStrokes (VOID);
BOOLEAN CheckError (
    IN EFI_STATUS  Status,
    IN CHAR16     *where
);
BOOLEAN CheckFatalError (
    IN EFI_STATUS  Status,
    IN CHAR16     *where
);

UINTN GetLumIndex (
    UINTN PixelR,
    UINTN PixelG,
    UINTN PixelB
);

VOID InitScreen (VOID);
VOID SetupScreen (VOID);
VOID PauseForKey (VOID);
VOID FixIconScale (VOID);
VOID RefitDeadLoop (VOID);
VOID TerminateScreen (VOID);
VOID PrepareBlankLine (VOID);
VOID FinishExternalScreen (VOID);
VOID HaltSeconds (UINTN Seconds);
VOID PauseSeconds (UINTN Seconds);
VOID BeginTextScreen (IN CHAR16 *Title);
VOID DrawScreenHeader (IN CHAR16 *Title);
VOID BltClearScreen (IN BOOLEAN ShowBanner);
VOID SwitchToText (IN BOOLEAN CursorEnabled);
VOID FinishTextScreen (IN BOOLEAN WaitAlways);
VOID SwitchToGraphicsAndClear (IN BOOLEAN ShowBanner);
VOID BltImage (
    IN EG_IMAGE *Image,
    IN UINTN     XPos,
    IN UINTN     YPos
);
VOID BltImageAlpha (
    IN EG_IMAGE *Image,
    IN UINTN     XPos,
    IN UINTN     YPos,
    IN EG_PIXEL *BackgroundPixel
);
VOID BltImageCompositeBadge (
    IN EG_IMAGE *BaseImage,
    IN EG_IMAGE *TopImage,
    IN EG_IMAGE *BadgeImage,
    IN UINTN     XPos,
    IN UINTN     YPos
);
VOID BeginExternalScreen (
    IN BOOLEAN  UseGraphicsMode,
    IN CHAR16  *Title
);
VOID PrintUglyText (
    IN CHAR16 *Text,
    IN UINTN   PositionCode
);
#endif
