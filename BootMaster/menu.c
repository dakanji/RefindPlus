/*
 * BootMaster/menu.c
 * Menu functions
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
 * Modifications for rEFInd Copyright (c) 2012-2020 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 */
/**
** Modified for RefindPlus
** Copyright (c) 2020-2026 Dayo Akanji (sf.net/u/dakanji/profile)
** Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
**
** Modifications distributed under the preceding terms.
**/

#include "global.h"
#include "menu.h"
#include "icns.h"
#include "scan.h"
#include "lib.h"
#include "apple.h"
#include "libeg.h"
#include "config.h"
#include "libegint.h"
#include "line_edit.h"
#include "mystrings.h"
#include "screenmgt.h"
#include "../include/version.h"
#include "../include/refit_call_wrapper.h"

#include "../include/egemb_back_selected_small.h"
#include "../include/egemb_back_selected_big.h"
#include "../include/egemb_arrow_right.h"
#include "../include/egemb_arrow_left.h"

// Other menu definitions

#define MENU_FUNCTION_INIT            (0)
#define MENU_FUNCTION_CLEANUP         (1)
#define MENU_FUNCTION_PAINT_ALL       (2)
#define MENU_FUNCTION_PAINT_SELECTION (3)
#define MENU_FUNCTION_PAINT_TIMEOUT   (4)
#define MENU_FUNCTION_PAINT_HINTS     (5)

static CHAR16 ArrowUp[2]   = {ARROW_UP,   0};
static CHAR16 ArrowDown[2] = {ARROW_DOWN, 0};
static UINTN  TileSizes[2] = {144,       64};

// Text and icon spacing constants.
#define TEXT_YMARGIN                  (2)
#define TITLEICON_SPACING            (16)

#define TILE_XSPACING                 (8)
#define TILE_YSPACING                (16)

// Alignment values for PaintIcon()
#define ALIGN_LEFT                     0
#define ALIGN_RIGHT                    1

EG_IMAGE *SelectionImages[2] = {NULL, NULL};

EFI_EVENT *WaitList          =  NULL;
UINT64     MainMenuLoad      =     0;
UINTN      WaitListLength    =     0;
UINTN      IconRowPosX       =     0;
UINTN      IconRowPosY       =     0;
UINTN      ToolRowPosX       =     0;
UINTN      ToolRowPosY       =     0;
UINTN      IconRowCount      =     0;
UINTN      ToolRowCount      =     0;
UINTN      IconRowItems      =     0;

// Pointer variables
BOOLEAN PointerEnabled       = FALSE;
BOOLEAN PointerActive        = FALSE;
BOOLEAN DrawSelection        =  TRUE;

BOOLEAN SubScreenBoot        = FALSE;

REFIT_MENU_ENTRY MenuEntryNo = {
    L"No", TAG_RETURN,
    1, 0, NULL, NULL, NULL
};
REFIT_MENU_ENTRY MenuEntryYes = {
    L"Yes", TAG_RETURN,
    1, 0, NULL, NULL, NULL
};

extern CHAR16             *BlankLine;
extern CHAR16             *VendorInfo;
extern UINT64              GetCurrentMS (VOID);
extern BOOLEAN             FoundExternalDisk;
extern BOOLEAN             FlushFailedTag;
extern BOOLEAN             FlushFailReset;
extern BOOLEAN             ClearedBuffer;
extern BOOLEAN             BlockRescan;
extern BOOLEAN             OneMainLoop;
extern EG_PIXEL            PixelMenuBG;
extern EFI_GUID            RefindPlusGuid;


#if REFIT_DEBUG > 0
VOID LogExit (
    IN  UINTN       MenuExit,
    IN  const char  FunctionName[],
    IN  CHAR16     *ChosenOptionTitle
) {
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from Menu Screen Option in '%a' Call ... %s",
        MenuExit, MenuExitInfo (MenuExit), FunctionName, ChosenOptionTitle
    );
}
#endif

static
VOID InitSelection (VOID) {
    #if REFIT_DEBUG > 0
    CHAR16    *MsgStr;
    #endif

    EG_IMAGE  *TempBigImage;
    EG_IMAGE  *TempSmallImage;
    UINTN      MaxAllowedSize;


    if (!AllowGraphicsMode         ||
        SelectionImages[0] != NULL ||
        SelectionImages[1] != NULL
    ) {
        // Early Return ... Already run once or in text mode
        return;
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    ALT_LOG(1, LOG_LINE_NORMAL, L"Handle Selection Background File:- 'Small'");
    #endif

    // Load small selection image
    MaxAllowedSize = 256;
    if (GlobalConfig.SelectionSmallFileName == NULL) {
        TempSmallImage = NULL;
    }
    else {
        TempSmallImage = egLoadImage (
            SelfDir,
            GlobalConfig.SelectionSmallFileName,
            TRUE
        );

        // DA-TAG: Impose maximum size for security
        if (TempSmallImage != NULL &&
            (
                TempSmallImage->Width  > MaxAllowedSize ||
                TempSmallImage->Height > MaxAllowedSize
            )
        ) {
            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (
                L"Discarded Custom Selection Image:- 'Small ... %d x %d Exceeds %d x %d'",
                TempSmallImage->Height, TempSmallImage->Width,
                MaxAllowedSize, MaxAllowedSize
            );
            LOG_MSG("INFO: %s", MsgStr);
            LOG_MSG("\n\n");
            ALT_LOG(1, LOG_STAR_SEPARATOR, MsgStr);
            MY_FREE_POOL(MsgStr);
            #endif

            MY_FREE_IMAGE(TempSmallImage);
        }
    }

    if (TempSmallImage == NULL) {
        TempSmallImage = egPrepareEmbeddedImage (
            &egemb_back_selected_small, TRUE, NULL
        );
    }

    if (TempSmallImage->Width  == TileSizes[1] &&
        TempSmallImage->Height == TileSizes[1]
    ) {
        SelectionImages[1] = egCopyImage (TempSmallImage);
    }
    else {
        SelectionImages[1] = egScaleImage (
            TempSmallImage, TileSizes[1], TileSizes[1]
        );
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(
        1, LOG_THREE_STAR_MID,
        L"Item Selection Background Size: %dx%d px",
        SelectionImages[1]->Width, SelectionImages[1]->Height
    );

    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    ALT_LOG(1, LOG_LINE_NORMAL, L"Handle Selection Background File:- 'Large'");
    #endif

    // Load big selection image
    if (GlobalConfig.SelectionBigFileName == NULL) {
        TempBigImage = NULL;
    }
    else {
        TempBigImage = egLoadImage (
            SelfDir,
            GlobalConfig.SelectionBigFileName,
            TRUE
        );

        // DA-TAG: Impose maximum size for security
        //         Using double the small image max
        MaxAllowedSize *= 2;
        if (TempBigImage != NULL &&
            (
                TempBigImage->Width  > MaxAllowedSize ||
                TempBigImage->Height > MaxAllowedSize
            )
        ) {
            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (
                L"Discarded Custom Selection Image:- 'Large ... %d x %d Exceeds %d x %d'",
                TempBigImage->Height, TempBigImage->Width,
                MaxAllowedSize, MaxAllowedSize
            );
            LOG_MSG("INFO: %s", MsgStr);
            LOG_MSG("\n\n");
            ALT_LOG(1, LOG_STAR_SEPARATOR, MsgStr);
            MY_FREE_POOL(MsgStr);
            #endif

            MY_FREE_IMAGE(TempBigImage);
        }
    }

    if (TempBigImage == NULL) {
       TempBigImage = egPrepareEmbeddedImage (
           &egemb_back_selected_big, TRUE, NULL
       );
    }

    if (TempBigImage->Width  == TileSizes[0] &&
        TempBigImage->Height == TileSizes[0]
    ) {
        SelectionImages[0] = egCopyImage (TempBigImage);
    }
    else {
        SelectionImages[0] = egScaleImage (
            TempBigImage, TileSizes[0], TileSizes[0]
        );
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(
        1, LOG_THREE_STAR_MID,
        L"Item Selection Background Size: %dx%d px",
        SelectionImages[0]->Width, SelectionImages[0]->Height
    );
    #endif

    MY_FREE_IMAGE(TempSmallImage);
    MY_FREE_IMAGE(TempBigImage);
} // static VOID InitSelection()

static
VOID InitScroll (
    OUT SCROLL_STATE *State,
    IN UINTN          ItemCount,
    IN UINTN          VisibleSpace
) {
    State->PreviousSelection =                    0;
    State->CurrentSelection  =                    0;
    State->FirstVisible      =                    0;
    State->MaxIndex          = (INTN) ItemCount - 1;

    if (AllowGraphicsMode) {
        State->MaxVisible = ScreenW / (TileSizes[0] + TILE_XSPACING) - 1;
    }
    else {
        State->MaxVisible = ConHeight - 4;
    }

    if (VisibleSpace > 0 && VisibleSpace < State->MaxVisible) {
        State->MaxVisible = (INTN) VisibleSpace;
    }

    State->PaintAll       = TRUE;
    State->PaintSelection = FALSE;
    State->LastVisible    = State->FirstVisible + State->MaxVisible - 1;
} // static VOID InitScroll()

// Adjust variables relating to the scrolling of tags, for when a selected icon
// is not visible given the current scrolling condition.
static
VOID AdjustScrollState (
    IN SCROLL_STATE *State
) {
    // Scroll forward
    if (State->CurrentSelection > State->LastVisible) {
        State->LastVisible  = State->CurrentSelection;
        State->FirstVisible = 1 + State->CurrentSelection - State->MaxVisible;

        if (State->FirstVisible < 0) {
            // Should not happen, but just in case.
            State->FirstVisible = 0;
        }

        State->PaintAll = TRUE;
    }

    // Scroll backward
    if (State->CurrentSelection < State->FirstVisible) {
        State->FirstVisible = State->CurrentSelection;
        State->LastVisible  = State->CurrentSelection + State->MaxVisible - 1;
        State->PaintAll     = TRUE;
    }
} // static VOID AdjustScrollState

static
VOID UpdateScroll (
    IN OUT SCROLL_STATE  *State,
    IN UINTN              Movement
) {
    State->PreviousSelection = State->CurrentSelection;

    switch (Movement) {
        case SCROLL_NONE:
            // Do Nothing
        break;
        case SCROLL_LINE_LEFT:
            if (State->CurrentSelection > 0) {
                State->CurrentSelection--;
            }

        break;
        case SCROLL_LINE_RIGHT:
            if (State->CurrentSelection < State->MaxIndex) {
                State->CurrentSelection++;
            }

        break;
        case SCROLL_LINE_UP:
            if (State->ScrollMode != SCROLL_MODE_ICONS) {
                if (State->CurrentSelection > 0) {
                    State->CurrentSelection--;
                }
            }
            else {
                if (State->CurrentSelection >= State->InitialRow1) {
                    if (State->MaxIndex <= State->InitialRow1) {
                        State->CurrentSelection = State->FirstVisible;
                    }
                    else {
                        // Avoid division by 0!
                        State->CurrentSelection = State->FirstVisible
                            + (State->LastVisible      - State->FirstVisible)
                            * (State->CurrentSelection - State->InitialRow1)
                            / (State->MaxIndex         - State->InitialRow1);
                    }
                }
            }

        break;
        case SCROLL_LINE_DOWN:
            if (State->ScrollMode != SCROLL_MODE_ICONS) {
                if (State->CurrentSelection < State->MaxIndex) {
                    State->CurrentSelection++;
                }
            }
            else {
                if (State->CurrentSelection <= State->FinalRow0) {
                    if (State->LastVisible <= State->FirstVisible) {
                        State->CurrentSelection = State->InitialRow1;
                    }
                    else {
                        // Avoid division by 0!
                        State->CurrentSelection = State->InitialRow1 +
                            (State->MaxIndex         - State->InitialRow1) *
                            (State->CurrentSelection - State->FirstVisible) /
                            (State->LastVisible      - State->FirstVisible);
                    }
                }
            }

        break;
        case SCROLL_PAGE_UP:
            if (State->CurrentSelection <= State->FinalRow0) {
                State->CurrentSelection -= State->MaxVisible;
            }
            else if (State->CurrentSelection == State->InitialRow1) {
                State->CurrentSelection = State->FinalRow0;
            }
            else {
                State->CurrentSelection = State->InitialRow1;
            }

            if (State->CurrentSelection < 0) {
                State->CurrentSelection = 0;
            }

        break;
        case SCROLL_FIRST:
            if (State->CurrentSelection > 0) {
                State->CurrentSelection = 0;
                State->PaintAll = TRUE;
            }

        break;
        case SCROLL_PAGE_DOWN:
            if (State->CurrentSelection  < State->FinalRow0) {
                State->CurrentSelection += State->MaxVisible;
                if (State->CurrentSelection > State->FinalRow0) {
                    State->CurrentSelection = State->FinalRow0;
                }
            }
            else if (State->CurrentSelection == State->FinalRow0) {
                State->CurrentSelection++;
            }
            else {
                State->CurrentSelection = State->MaxIndex;
            }

            if (State->CurrentSelection > State->MaxIndex) {
                State->CurrentSelection = State->MaxIndex;
            }

        break;
        case SCROLL_LAST:
            if (State->CurrentSelection < State->MaxIndex) {
                State->CurrentSelection = State->MaxIndex;
                State->PaintAll = TRUE;
            }

            break;
    } // switch

    if (State->ScrollMode == SCROLL_MODE_TEXT) {
        AdjustScrollState (State);
    }

    if (!State->PaintAll &&
        State->CurrentSelection != State->PreviousSelection
    ) {
        State->PaintSelection = TRUE;
    }
    State->LastVisible = State->FirstVisible + State->MaxVisible - 1;
} // static VOID UpdateScroll()

// Identify the end of row 0 and the beginning of row 1; store the results in the
// appropriate fields in State. Also reduce MaxVisible if that value is greater
// than the total number of row-0 tags and if we are in an icon-based screen
static
VOID IdentifyRows (
    IN SCROLL_STATE      *State,
    IN REFIT_MENU_SCREEN *Screen
) {
    UINTN i;


    State->FinalRow0   = 0;
    State->InitialRow1 = State->MaxIndex;
    for (i = 0; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 0) {
            State->FinalRow0 = i;
        }
        else {
            if (State->InitialRow1 > i    &&
                Screen->Entries[i]->Row == 1
            ) {
                State->InitialRow1 = i;
            }
        }
    } // for

    if (State->ScrollMode == SCROLL_MODE_ICONS &&
        State->MaxVisible > (State->FinalRow0 + 1)
    ) {
        State->MaxVisible = State->FinalRow0 + 1;
    }
} // static VOID IdentifyRows()

// Blank the screen, wait for a keypress or pointer event,
// and restore banner/background. Screen may still require
// redrawing of text and icons on return.
// DA-TAG: Investigate This
//         Support more sophisticated screen savers
//         E.g., power-saving mode and dynamic images
static
VOID SaveScreen (VOID) {
    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr;
    CHAR16  *LoopChange;
    BOOLEAN  CheckMute = FALSE;
    #endif

    UINTN    Retval;
    UINTN    OurIndex;
    UINT64   TimeWait;
    UINT64   BaseWait;
    EG_PIXEL OUR_COLOR;
    EG_PIXEL COLOR_001 = {   0,  51,  51,  0 };
    EG_PIXEL COLOR_002 = {   0, 102, 102,  0 };
    EG_PIXEL COLOR_003 = {   0, 153, 153,  0 };
    EG_PIXEL COLOR_004 = {   0, 204, 204,  0 };
    EG_PIXEL COLOR_005 = {   0, 255, 255,  0 };
    EG_PIXEL COLOR_006 = {  51,   0, 204,  0 };
    EG_PIXEL COLOR_007 = {  51,  51, 153,  0 };
    EG_PIXEL COLOR_008 = {  51, 102, 102,  0 };
    EG_PIXEL COLOR_009 = {  51, 153,  51,  0 };
    EG_PIXEL COLOR_010 = {  51, 204,   0,  0 };
    EG_PIXEL COLOR_011 = {  51, 255,  51,  0 };
    EG_PIXEL COLOR_012 = { 102,   0, 102,  0 };
    EG_PIXEL COLOR_013 = { 102,  51, 153,  0 };
    EG_PIXEL COLOR_014 = { 102, 102, 204,  0 };
    EG_PIXEL COLOR_015 = { 102, 153, 255,  0 };
    EG_PIXEL COLOR_016 = { 102, 204, 204,  0 };
    EG_PIXEL COLOR_017 = { 102, 255, 153,  0 };
    EG_PIXEL COLOR_018 = { 153,   0, 102,  0 };
    EG_PIXEL COLOR_019 = { 153,  51,  51,  0 };
    EG_PIXEL COLOR_020 = { 153, 102,   0,  0 };
    EG_PIXEL COLOR_021 = { 153, 153,  51,  0 };
    EG_PIXEL COLOR_022 = { 153, 204, 102,  0 };
    EG_PIXEL COLOR_023 = { 153, 255, 153,  0 };
    EG_PIXEL COLOR_024 = { 204,   0, 204,  0 };
    EG_PIXEL COLOR_025 = { 204,  51, 255,  0 };
    EG_PIXEL COLOR_026 = { 204, 102, 204,  0 };
    EG_PIXEL COLOR_027 = { 204, 153, 153,  0 };
    EG_PIXEL COLOR_028 = { 204, 204, 102,  0 };
    EG_PIXEL COLOR_029 = { 204, 255,  51,  0 };
    EG_PIXEL COLOR_030 = { 255,   0,   0,  0 };


    #if REFIT_DEBUG > 0
    MsgStr = L"Input Activity Wait Threshold Exceeded";
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    ALT_LOG(1, LOG_LINE_NORMAL,  L"%s", MsgStr);
    LOG_MSG("%s", MsgStr);

    MsgStr = L"Start Screensaver";
    ALT_LOG(1, LOG_THREE_STAR_MID,  L"%s", MsgStr);
    LOG_MSG("%s  - %s", OffsetNext, MsgStr);
    LOG_MSG("\n");
    #endif

    // Start with COLOR_001 ... 0 will be incremented to 1
    OurIndex = 0;

    // Start with BaseWait
    BaseWait = 3750;
    TimeWait = BaseWait;
    while (1) {
        ++OurIndex;

        if (OurIndex > 30) {
            OurIndex = 1;

            TimeWait = TimeWait * 2;
            if (TimeWait <= 120000) {
                #if REFIT_DEBUG > 0
                LoopChange = L"Extend";
                #endif
            }
            else {
                // Reset TimeWait if greater than 2 minutes
                TimeWait = BaseWait;

                #if REFIT_DEBUG > 0
                LoopChange = L"Reset";
                #endif
            }

            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"%s Screensaver Timeout Loop:- %d",
                LoopChange, TimeWait
            );
            #endif
        }

        switch (OurIndex) {
            case   1: OUR_COLOR = COLOR_001; break;
            case   2: OUR_COLOR = COLOR_002; break;
            case   3: OUR_COLOR = COLOR_003; break;
            case   4: OUR_COLOR = COLOR_004; break;
            case   5: OUR_COLOR = COLOR_005; break;
            case   6: OUR_COLOR = COLOR_006; break;
            case   7: OUR_COLOR = COLOR_007; break;
            case   8: OUR_COLOR = COLOR_008; break;
            case   9: OUR_COLOR = COLOR_009; break;
            case  10: OUR_COLOR = COLOR_010; break;
            case  11: OUR_COLOR = COLOR_011; break;
            case  12: OUR_COLOR = COLOR_012; break;
            case  13: OUR_COLOR = COLOR_013; break;
            case  14: OUR_COLOR = COLOR_014; break;
            case  15: OUR_COLOR = COLOR_015; break;
            case  16: OUR_COLOR = COLOR_016; break;
            case  17: OUR_COLOR = COLOR_017; break;
            case  18: OUR_COLOR = COLOR_018; break;
            case  19: OUR_COLOR = COLOR_019; break;
            case  20: OUR_COLOR = COLOR_020; break;
            case  21: OUR_COLOR = COLOR_021; break;
            case  22: OUR_COLOR = COLOR_022; break;
            case  23: OUR_COLOR = COLOR_023; break;
            case  24: OUR_COLOR = COLOR_024; break;
            case  25: OUR_COLOR = COLOR_025; break;
            case  26: OUR_COLOR = COLOR_026; break;
            case  27: OUR_COLOR = COLOR_027; break;
            case  28: OUR_COLOR = COLOR_028; break;
            case  29: OUR_COLOR = COLOR_029; break;
            default : OUR_COLOR = COLOR_030; break;
        }

        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif
        egClearScreen (&OUR_COLOR);
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif

        Retval = WaitForInput (TimeWait);
        if (Retval == INPUT_KEY      ||
            Retval == INPUT_TIMER_ERROR
        ) {
            break;
        }
    } // while {Infinite}

    #if REFIT_DEBUG > 0
    MsgStr = L"Activity Detected ... Halt Screensaver";
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    LOG_MSG("%s", MsgStr);
    LOG_MSG("\n\n");
    #endif

    if (AllowGraphicsMode) {
        SwitchToGraphicsAndClear (TRUE);
    }

    ReadAllKeyStrokes();
} // VOID SaveScreen()

//
// Generic menu function
//
#if REFIT_DEBUG > 0
static
CHAR16 * GetScanCodeText (
    IN UINTN ScanCode
) {
    CHAR16 *Retval;


    switch (ScanCode) {
        case SCAN_END:       Retval = L"SCROLL_LAST" ; break;
        case SCAN_HOME:      Retval = L"SCROLL_FIRST"; break;
        case SCAN_PAGE_UP:   Retval = L"PAGE_UP"     ; break;
        case SCAN_PAGE_DOWN: Retval = L"PAGE_DOWN"   ; break;
        case SCAN_UP:        Retval = L"ARROW_UP"    ; break;
        case SCAN_LEFT:      Retval = L"ARROW_LEFT"  ; break;
        case SCAN_DOWN:      Retval = L"ARROW_DOWN"  ; break;
        case SCAN_RIGHT:     Retval = L"ARROW_RIGHT" ; break;
        case SCAN_ESC:       Retval = L"RESCAN_ESC"  ; break;
        case SCAN_DELETE:    Retval = L"HIDE_DEL"    ; break;
        case SCAN_INSERT:    Retval = L"DETAILS_INS" ; break;
        case SCAN_F2:        Retval = L"DETAILS_F2"  ; break;
        case SCAN_F10:       Retval = L"SCRNSHOT_F10"; break;
        case 0x0016:         Retval = L"EJECT_F12"   ; break;
        default:             Retval = L"UNMAPPED_KEY"; break;
    } // switch

    return Retval;
} // static CHAR16 * GetScanCodeText()
#endif

// Show information lines in text mode.
static
VOID ShowTextInfoLines (
    IN REFIT_MENU_SCREEN *Screen
) {
    UINTN i;


    if (Screen->InfoLineCount == 0) {
        // Early Return
        return;
    }

    BeginTextScreen (Screen->Title);

    REFIT_CALL_2_WRAPPER(
        gST->ConOut->SetAttribute,
        gST->ConOut, ATTR_BASIC
    );

    for (i = 0; i < Screen->InfoLineCount; i++) {
        REFIT_CALL_3_WRAPPER(
            gST->ConOut->SetCursorPosition, gST->ConOut,
            3, 4 + i
        );
        REFIT_CALL_2_WRAPPER(
            gST->ConOut->OutputString,
            gST->ConOut, Screen->InfoLines[i]
        );
    }
} // static VOID ShowTextInfoLines()

static
UINTN TextLineHeight (VOID) {
    static UINTN TheLineHeight = 0;

    if (TheLineHeight != 0) {
        return TheLineHeight;
    }

    TheLineHeight = egGetFontHeight() + TEXT_YMARGIN * 2;
    return TheLineHeight;
} // static UINTN TextLineHeight()

// Display text with a solid background
static
VOID DrawText (
    IN CHAR16  *Text,
    IN BOOLEAN  Selected,
    IN UINTN    FieldWidth,
    IN UINTN    XPos,
    IN UINTN    YPos
) {
    UINTN     LumIndex;
    EG_IMAGE *TextBuffer;
    EG_PIXEL  TextBackground;
    EG_PIXEL  SelectionPixel = { 0xFF, 0xFF, 0xFF, 0 };


    TextBuffer = egCreateFilledImage (
        FieldWidth,
        TextLineHeight(),
        FALSE,
        &PixelMenuBG
    );
    if (TextBuffer == NULL) {
        // Early Return
        return;
    }

    if (!Selected) {
        TextBackground = PixelMenuBG;
    }
    else {
        // Draw selection bar background
        egFillImageArea (
            TextBuffer,
            0, 0,
            FieldWidth,
            TextBuffer->Height,
            &SelectionPixel
        );
        TextBackground = SelectionPixel;
    }

    // Get Luminance Index
    LumIndex = GetLumIndex (
        (UINTN) TextBackground.r,
        (UINTN) TextBackground.g,
        (UINTN) TextBackground.b
    );

    // Render the text
    egRenderText (
        Text, TextBuffer,
        egGetFontCellWidth(),
        TEXT_YMARGIN, (UINT8) LumIndex
    );

    egDrawImageWithTransparency (
        TextBuffer, NULL,
        XPos, YPos,
        TextBuffer->Width,
        TextBuffer->Height
    );

    MY_FREE_IMAGE(TextBuffer);
} // static VOID DrawText()

// Finds the average brightness of the input Image.
// NOTE: Passing an Image that covers the whole screen can strain the
// capacity of a UINTN on a 32-bit system with a very large display.
// Using UINT64 instead is unworkable, as the code will not compile
// on a 32-bit system. As the intended use for this function is to
// handle a single text string's background, this should not be a
// problem, but may need addressing if applied more broadly.
static
UINT8 AverageBrightness (
    EG_IMAGE *Image
) {
    UINTN i;
    UINTN Sum;
    UINTN Size;


    if (Image == NULL) {
        // Early Return
        return 0;
    }

    Size = Image->Width * Image->Height;
    if (Size == 0) {
        // Early Return
        return 0;
    }

    Sum = 0;

    for (i = 0; i < Size; i++) {
        Sum += Image->PixelData[i].r;
        Sum += Image->PixelData[i].g;
        Sum += Image->PixelData[i].b;
    }
    Sum /= (Size * 3);

    return (UINT8) Sum;
} // static UINT8 AverageBrightness()

// Display text against the screen's background image.
// Special case: clear the line if Text is NULL or 0-length.
// Does NOT indent the text or reposition it relative
// to the specified XPos and YPos values.
static
VOID DrawTextWithTransparency (
    IN CHAR16 *Text,
    IN UINTN   XPos,
    IN UINTN   YPos
) {
    UINTN     TextWidth;
    EG_IMAGE *TextBuffer;


    if (Text == NULL) {
        Text = L"";
    }

    egMeasureText (Text, &TextWidth, NULL);

    if (TextWidth == 0) {
       TextWidth = ScreenW;
       XPos      = 0;
    }

    TextBuffer = egCropImage (
        GlobalConfig.ScreenBackground,
        XPos, YPos,
        TextWidth,
        TextLineHeight()
    );

    if (TextBuffer == NULL) {
        return;
    }

    // Render the text
    egRenderText (
        Text, TextBuffer,
        0, 0,
        AverageBrightness (TextBuffer)
    );

    egDrawImageWithTransparency (
        TextBuffer, NULL,
        XPos, YPos,
        TextBuffer->Width,
        TextBuffer->Height
    );

    MY_FREE_IMAGE(TextBuffer);
} // static VOID DrawTextWithTransparency()

// Compute the size and position of the window
// that will hold a subscreen's information.
static
VOID ComputeSubScreenWindowSize (
    REFIT_MENU_SCREEN *Screen,
    SCROLL_STATE      *State,
    UINTN             *XPos,
    UINTN             *YPos,
    UINTN             *Width,
    UINTN             *Height,
    UINTN             *LineWidth
) {
    UINTN i;
    UINTN HintTop;
    UINTN ItemWidth;
    UINTN TitleWidth;
    UINTN FontCellWidth;
    UINTN FontCellHeight;
    UINTN BannerBottomEdge;
    UINTN TheHeightThreshold;


    *Width     = 20;
    *Height    = 5;
    TitleWidth = egComputeTextWidth (Screen->Title);

    for (i = 0; i < Screen->InfoLineCount; i++) {
        ItemWidth = StrLen (Screen->InfoLines[i]);

        if (*Width < ItemWidth) {
            *Width = ItemWidth;
        }

        (*Height)++;
    } // for

    for (i = 0; i <= State->MaxIndex; i++) {
        ItemWidth = StrLen (Screen->Entries[i]->Title);

        if (*Width < ItemWidth) {
            *Width = ItemWidth;
        }

        (*Height)++;
    } // for

    FontCellWidth  = egGetFontCellWidth();
    *Width = (*Width + 2) * FontCellWidth;
    *LineWidth = *Width;

    if (Screen->TitleImage) {
        *Width += (Screen->TitleImage->Width + (TITLEICON_SPACING * 2) + FontCellWidth);
    }
    else {
        *Width += FontCellWidth;
    }

    if (*Width < TitleWidth) {
        *Width = TitleWidth + 2 * FontCellWidth;
    }

    // Keep within 2/3 of screen width or
    // screen bounds if under 800 pixels

    if (*Width > ScreenW) {
        *Width = ScreenW;
    }

    *XPos = (ScreenW - *Width) / 2;

    // Top of Hint Text
    FontCellHeight = egGetFontHeight();
    HintTop  = ScreenH - (FontCellHeight * 3);

    *Height *= TextLineHeight();
    if (Screen->TitleImage) {
        TheHeightThreshold = Screen->TitleImage->Height + (TextLineHeight() * 4);

        if (*Height < TheHeightThreshold) {
            *Height = TheHeightThreshold;
        }
    }

    if (GlobalConfig.BannerBottomEdge >= HintTop) {
        // Treat as Empty Banner ... Probably Fullscreen Image
        BannerBottomEdge = 0;
    }
    else {
        BannerBottomEdge = GlobalConfig.BannerBottomEdge;
    }

    TheHeightThreshold = HintTop - BannerBottomEdge - (FontCellHeight * 2);
    if (*Height > TheHeightThreshold) {
        *Height = TheHeightThreshold;
        BannerBottomEdge = 0;
    }

    *YPos = ((ScreenH - *Height) / 2);
    if (*YPos < BannerBottomEdge) {
        *YPos = BannerBottomEdge +
            FontCellHeight +
            (HintTop - BannerBottomEdge - *Height) / 2;
    }
} // static VOID ComputeSubScreenWindowSize()

static
VOID DrawMainMenuEntry (
    REFIT_MENU_ENTRY *Entry,
    BOOLEAN           Selected,
    UINTN             XPos,
    UINTN             YPos
) {
    EG_IMAGE *Background;


    // Do not draw selection image when not hovering (if using pointer)
    if (!Selected || !DrawSelection) {
        // Image not Selected ... Copy Background
        egDrawImageWithTransparency (
            Entry->Image,
            Entry->BadgeImage,
            XPos, YPos,
            SelectionImages[Entry->Row]->Width,
            SelectionImages[Entry->Row]->Height
        );

        // Early Return
        return;
    }

    Background = egCropImage (
        GlobalConfig.ScreenBackground,
        XPos, YPos,
        SelectionImages[Entry->Row]->Width,
        SelectionImages[Entry->Row]->Height
    );

    if (Background) {
        egComposeImage (
            Background,
            SelectionImages[Entry->Row],
            0, 0
        );

        BltImageCompositeAny (
            Background,
            Entry->Image,
            Entry->BadgeImage,
            XPos, YPos
        );

        MY_FREE_IMAGE(Background);
    }
} // static VOID DrawMainMenuEntry()

static
VOID PaintAll (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    UINTN                *itemPosX,
    UINTN                 row0PosY,
    UINTN                 row1PosY,
    UINTN                 textPosY
) {
    INTN i;


    if (Screen->Entries[State->CurrentSelection]->Row == 0) {
        AdjustScrollState (State);
    }

    for (i = State->FirstVisible; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row != 0) {
            DrawMainMenuEntry (
                Screen->Entries[i],
                (i == State->CurrentSelection) ? TRUE : FALSE,
                itemPosX[i],
                row1PosY
            );
        }
        else {
            if (i <= State->LastVisible) {
                DrawMainMenuEntry (
                    Screen->Entries[i],
                    (i == State->CurrentSelection) ? TRUE : FALSE,
                    itemPosX[i - State->FirstVisible],
                    row0PosY
                );
            }
        }
    }

    DrawTextWithTransparency (L"", 0, textPosY);
    if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL) &&
        (!PointerActive || DrawSelection)
    ) {
        DrawTextWithTransparency (
            Screen->Entries[State->CurrentSelection]->Title,
            (ScreenW - egComputeTextWidth (Screen->Entries[State->CurrentSelection]->Title)) >> 1,
            textPosY
        );
    }

    if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
        DrawTextWithTransparency (
            Screen->Hint1,
            (ScreenW - egComputeTextWidth (Screen->Hint1)) / 2,
            ScreenH - (egGetFontHeight() * 3)
        );

        DrawTextWithTransparency (
            Screen->Hint2,
            (ScreenW - egComputeTextWidth (Screen->Hint2)) / 2,
            ScreenH - (egGetFontHeight() * 2)
        );
    }
} // static VOID PaintAll()

// Move the selection to State->CurrentSelection
// Adjust icon row if necessary
static
VOID PaintSelection (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    UINTN                *itemPosX,
    UINTN                 row0PosY,
    UINTN                 row1PosY,
    UINTN                 textPosY
) {
    UINTN XSelectPrev;
    UINTN XSelectCur;
    UINTN YPosPrev;
    UINTN YPosCur;


    if ((State->CurrentSelection < State->InitialRow1) &&
        (
            (State->CurrentSelection > State->LastVisible) ||
            (State->CurrentSelection < State->FirstVisible)
        )
    ) {
        // Current selection is not visible ... Redraw the menu
        MainMenuStyle (Screen, State, MENU_FUNCTION_PAINT_ALL, NULL);

        // Early Return
        return;
    }

    if (Screen->Entries[State->PreviousSelection]->Row == 0) {
        XSelectPrev = State->PreviousSelection - State->FirstVisible;
        YPosPrev = row0PosY;
    }
    else {
        XSelectPrev = State->PreviousSelection;
        YPosPrev = row1PosY;
    }

    if (Screen->Entries[State->CurrentSelection]->Row == 0) {
        XSelectCur = State->CurrentSelection - State->FirstVisible;
        YPosCur = row0PosY;
    }
    else {
        XSelectCur = State->CurrentSelection;
        YPosCur = row1PosY;
    }

    DrawMainMenuEntry (
        Screen->Entries[State->PreviousSelection],
        FALSE,
        itemPosX[XSelectPrev],
        YPosPrev
    );

    DrawMainMenuEntry (
        Screen->Entries[State->CurrentSelection],
        TRUE,
        itemPosX[XSelectCur],
        YPosCur
    );

    DrawTextWithTransparency (L"", 0, textPosY);
    if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL) &&
        (!PointerActive || DrawSelection)
    ) {
        DrawTextWithTransparency (
            Screen->Entries[State->CurrentSelection]->Title,
            (ScreenW - egComputeTextWidth (Screen->Entries[State->CurrentSelection]->Title)) >> 1,
            textPosY
        );
    }
} // static VOID MoveSelection (VOID)

// Fetch the icon specified by ExternalFilename if available,
// or by BuiltInIcon if not.
static
EG_IMAGE * GetIcon (
    IN EG_EMBEDDED_IMAGE *BuiltInIcon,
    IN CHAR16            *ExternalFilename
) {
    EG_IMAGE             *Icon;


    Icon = egFindIcon (
        ExternalFilename,
        GlobalConfig.IconSizes[ICON_SIZE_SMALL]
    );

    if (Icon == NULL) {
        Icon = egPrepareEmbeddedImage (
            BuiltInIcon, TRUE, NULL
        );
    }

    return Icon;
} // static EG_IMAGE * GetIcon()

// Display an icon at the specified location. The Y position is
// specified as the center value, and so is adjusted by half
// the icon's height. The X position is set along the icon's left
// edge if Alignment == ALIGN_LEFT, and along the right edge if
// Alignment == ALIGN_RIGHT
static
VOID PaintIcon (
    IN EG_IMAGE *Icon,
    UINTN        PosX,
    UINTN        PosY,
    UINTN        Alignment
) {
    if (Icon == NULL) {
        // Early Return
        return;
    }

    if (Alignment == ALIGN_RIGHT) {
        PosX -= Icon->Width;
    }

    egDrawImageWithTransparency (
        Icon,
        NULL,
        PosX,
        PosY - (Icon->Height / 2),
        Icon->Width,
        Icon->Height
    );
} // static VOID PaintIcon()

// Display (or erase) the arrow icons to the left
// and right of an icon's row ... as appropriate.
static
VOID PaintArrows (
    SCROLL_STATE *State,
    UINTN         PosX,
    UINTN         PosY,
    UINTN         row0Loaders
) {
    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    UINTN            RightX;
    UINTN            VetValue;
    UINTN            AllTileWidth;
    static EG_IMAGE *LeftArrow       =  NULL;
    static EG_IMAGE *RightArrow      =  NULL;
    static EG_IMAGE *LeftBackground  =  NULL;
    static EG_IMAGE *RightBackground =  NULL;
    static BOOLEAN   LoadedArrows    = FALSE;


    AllTileWidth = (TILE_XSPACING + TileSizes[0]) * State->MaxVisible;
    RightX = TILE_XSPACING + ((ScreenW + AllTileWidth) / 2);

    if (!LoadedArrows &&
        !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_ARROWS)
    ) {
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif
        LeftArrow  = GetIcon (&egemb_arrow_left,  L"arrow_left" );
        RightArrow = GetIcon (&egemb_arrow_right, L"arrow_right");
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif

        if (LeftArrow) {
            LeftBackground = egCropImage (
                GlobalConfig.ScreenBackground,
                PosX - LeftArrow->Width,
                PosY - (LeftArrow->Height / 2),
                LeftArrow->Width,
                LeftArrow->Height
            );
        }
        if (RightArrow) {
            RightBackground = egCropImage (
                GlobalConfig.ScreenBackground,
                RightX,
                PosY - (RightArrow->Height / 2),
                RightArrow->Width,
                RightArrow->Height
            );
        }
        LoadedArrows = TRUE;
    }

    // For PaintIcon() calls, the starting Y position is moved to the midpoint
    // of the surrounding row; PaintIcon() adjusts this back up by half the
    // icon's height to properly center it.
    if (LeftArrow && LeftBackground) {
        (State->FirstVisible > 0)
            ? PaintIcon (
                LeftArrow,
                PosX, PosY,
                ALIGN_RIGHT
            )
            : BltImage (
                LeftBackground,
                PosX - LeftArrow->Width,
                PosY - (LeftArrow->Height / 2)
            );
    }

    if (RightArrow && RightBackground) {
        VetValue = (
            row0Loaders > 1
        ) ? row0Loaders - 1 : 0;
        (State->LastVisible < VetValue)
            ? PaintIcon (
                RightArrow,
                RightX, PosY,
                ALIGN_LEFT
            )
            : BltImage (
                RightBackground,
                RightX,
                PosY - (RightArrow->Height / 2)
            );
    }
} // static VOID PaintArrows()

// Enable the user to edit boot loader options.
// Returns TRUE if the user exited with edited options; FALSE if the user
// pressed Esc to terminate the edit.
static
BOOLEAN EditOptions (
    LOADER_ENTRY *MenuEntry
) {
    UINTN    x_max, y_max;
    CHAR16  *EditedOptions;
    BOOLEAN  Retval;


    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) {
        // Early Return
        return FALSE;
    }

    REFIT_CALL_4_WRAPPER(
        gST->ConOut->QueryMode, gST->ConOut,
        gST->ConOut->Mode->Mode, &x_max, &y_max
    );

    if (!GlobalConfig.TextOnly) {
        SwitchToText (TRUE);
    }

    if (line_edit (MenuEntry->LoadOptions, &EditedOptions, x_max)) {
        MY_FREE_POOL(MenuEntry->LoadOptions);
        MenuEntry->LoadOptions = EditedOptions;

        Retval = TRUE;
    }
    else {
        Retval = FALSE;
    }

    if (!GlobalConfig.TextOnly) {
        SwitchToGraphics();
    }

    return Retval;
} // static VOID EditOptions()

// Save a list of items to be hidden to NVRAM or disk,
// as determined by GlobalConfig.UseNvram.
static
VOID SaveHiddenList (
    IN CHAR16 *HiddenList,
    IN CHAR16 *VarName
) {
    EFI_STATUS Status;
    UINTN      ListLen;


    if (VarName == NULL) {
        CheckError (EFI_INVALID_PARAMETER, L"in SaveHiddenList!!");

        // Early Return ... Prevent NULL dererencing
        return;
    }

    if (HiddenList == NULL) {
        ListLen = 0;
    }
    else {
        ListLen = StrLen (HiddenList) * 2 + 2;
    }

    Status = EfivarSetRaw (
        &RefindPlusGuid, VarName,
        HiddenList, ListLen, TRUE
    );
    if (EFI_ERROR(Status)) {
        CheckError (Status, L"in SaveHiddenList!!");
    }
} // static VOID SaveHiddenList()

// Add PathName to the hidden tags variable specified by *VarName.
static
VOID AddToHiddenTags (
    CHAR16 *VarName,
    CHAR16 *Pathname
) {
    EFI_STATUS  Status;
    CHAR16     *HiddenTags;


    if (Pathname == NULL || StrLen (Pathname) == 0) {
        // Early Return
        return;
    }

    HiddenTags = ReadHiddenTags (VarName);
    if (FindSubStr (HiddenTags, Pathname)) {
        CheckError (EFI_ALREADY_STARTED, L"in 'AddToHiddenTags'");

        // Early Return
        return;
    }

    MergeUniqueStrings (&HiddenTags, Pathname, L',');
    Status = EfivarSetRaw (
        &RefindPlusGuid, VarName,
        HiddenTags, StrLen (HiddenTags) * 2 + 2, TRUE
    );
    if (EFI_ERROR(Status)) {
        CheckError (Status, L"in 'AddToHiddenTags'!!");
    }

    MY_FREE_POOL(HiddenTags);
} // static VOID AddToHiddenTags()

// Adds a filename, specified by the *Loader variable, to the *VarName UEFI variable,
// using the mostly-prepared *HideEfiMenu structure to prompt the user to confirm
// hiding that item.
// Returns TRUE if item was hidden, FALSE otherwise.
static
BOOLEAN HideEfiTag (
    LOADER_ENTRY      *Loader,
    REFIT_MENU_SCREEN *HideEfiMenu,
    CHAR16            *VarName
) {
    INTN               DefaultEntry;
    UINTN              MenuExit;
    CHAR16            *GuidStr;
    CHAR16            *FullPath;
    CHAR16            *TempPath;
    BOOLEAN            TagHidden;
    BOOLEAN            MyLoadPath;
    BOOLEAN            GotVolName;
    REFIT_VOLUME      *TestVolume;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenOption;


    if (Loader             == NULL ||
        VarName            == NULL ||
        HideEfiMenu        == NULL ||
        Loader->Volume     == NULL ||
        Loader->LoaderPath == NULL
    ) {
        // Early Return
        return FALSE;
    }

    GotVolName = (
        Loader->Volume->VolName           &&
        StrLen (Loader->Volume->VolName) > 0
    );

    MyLoadPath = (
        Loader->LoaderPath           &&
        StrLen (Loader->LoaderPath) > 0
    );

    if (GotVolName) {
        if (!MyLoadPath) {
            TempPath = StrDuplicate (Loader->Volume->VolName);
        }
        else {
            TempPath = PoolPrint (
                L"%s:%s", Loader->Volume->VolName, Loader->LoaderPath
            );
        }
    }
    else if (MyLoadPath) {
        TempPath = StrDuplicate (Loader->LoaderPath);
    }
    else {
        TempPath = StrDuplicate (L"Item Selected on Main Screen");
    }

    AddMenuInfoLine (HideEfiMenu, L"Hide EFI Entry Below?",    FALSE);
    AddMenuInfoLine (HideEfiMenu, PoolPrint (L"%s", TempPath),  TRUE);
    MY_FREE_POOL(TempPath);

    do {
        TagHidden = GetMenuEntryYesNo (&HideEfiMenu);
        if (!TagHidden) {
            FreeMenuScreen (&HideEfiMenu);

            break;
        }

        HideEfiMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_HIDDEN);

        DefaultEntry = 9999; // Use the Max Index
        Style = (
            AllowGraphicsMode
        ) ? GraphicsMenuStyle : TextMenuStyle;
        MenuExit = DrawMenuScreen (
            HideEfiMenu, Style,
            &DefaultEntry, &ChosenOption
        );

        #if REFIT_DEBUG > 0
        LogExit (MenuExit, __func__, ChosenOption->Title);
        #endif

        if (MenuExit != MENU_EXIT_ENTER        ||
            MyStriCmp (ChosenOption->Title, L"No")
        ) {
            TagHidden = FALSE;
        }
        else {
            TagHidden  = TRUE;

            TestVolume = NULL;
            GuidStr    = GuidAsString (&Loader->Volume->PartGuid);
            FindVolume (&TestVolume, GuidStr);

            if (TestVolume != NULL && TestVolume->RootDir != NULL) {
                if (!GotVolName) {
                    FullPath = PoolPrint (L"%s:", GuidStr);
                }
                else {
                    TempPath = StrDuplicate (
                        Loader->Volume->VolName
                    );
                    LimitStringLength (TempPath, 20);

                    FullPath = PoolPrint (
                        L"%-20s%s%s:",
                        TempPath, DEFAULT_STRING_DELIM, GuidStr
                    );

                    MY_FREE_POOL(TempPath);
                }

                if (MyLoadPath) {
                    MergeStrings (
                        &FullPath,
                        Loader->LoaderPath,
                        (Loader->LoaderPath[0] == L'\\' ? L'\0' : L'\\')
                    );
                }
            }
            else if (GotVolName) {
                FullPath = PoolPrint (L"%s:", Loader->Volume->VolName);
            }
            else {
                FullPath = NULL;
            }

            MY_FREE_POOL(GuidStr);

            if (FullPath != NULL) {
                AddToHiddenTags (VarName, FullPath);
            }
            MY_FREE_POOL(FullPath);
        }
    } while (0); // This 'loop' only runs once

    return TagHidden;
} // static BOOLEAN HideEfiTag()

static
BOOLEAN HideFirmwareTag (
    LOADER_ENTRY      *Loader,
    REFIT_MENU_SCREEN *HideFirmwareMenu
) {
    INTN               DefaultEntry;
    UINTN              MenuExit;
    BOOLEAN            TagHidden;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenOption;


    AddMenuInfoLine (HideFirmwareMenu, L"Hide Firmware Entry Below?",    FALSE);
    AddMenuInfoLine (HideFirmwareMenu, PoolPrint (L"%s", Loader->Title),  TRUE);

    TagHidden = GetMenuEntryYesNo (&HideFirmwareMenu);
    if (!TagHidden) {
        FreeMenuScreen (&HideFirmwareMenu);

        // Early Return
        return FALSE;
    }

    DefaultEntry = 9999; // Use the Max Index
    Style = (
        AllowGraphicsMode
    ) ? GraphicsMenuStyle : TextMenuStyle;
    MenuExit = DrawMenuScreen (
        HideFirmwareMenu, Style,
        &DefaultEntry, &ChosenOption
    );

    #if REFIT_DEBUG > 0
    LogExit (MenuExit, __func__, ChosenOption->Title);
    #endif

    if (MenuExit != MENU_EXIT_ENTER        ||
        MyStriCmp (ChosenOption->Title, L"No")
    ) {
        TagHidden = FALSE;
    }
    else {
        TagHidden = TRUE;
        AddToHiddenTags(L"HiddenFirmware", Loader->Title);
    }

    return TagHidden;
} // BOOLEAN HideFirmwareTag()

static
BOOLEAN HideLegacyTag (
    LEGACY_ENTRY      *LegacyLoader,
    REFIT_MENU_SCREEN *HideLegacyMenu
) {
    INTN               DefaultEntry;
    UINTN              MenuExit;
    CHAR16            *Name;
    BOOLEAN            TagHidden;
    BOOLEAN            BaseCheck;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenOption;


    BaseCheck = (
        LegacyLoader->me.Title  != NULL &&
        GlobalConfig.LegacyType != LEGACY_TYPE_MAC1
    );

    if (!BaseCheck &&
        (
            LegacyLoader->BdsOption              == NULL ||
            LegacyLoader->BdsOption->Description == NULL ||
            (
                GlobalConfig.LegacyType != LEGACY_TYPE_UEFI &&
                GlobalConfig.LegacyType != LEGACY_TYPE_MAC2
            )
        )
    ) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_STAR_SEPARATOR,
            L"Could *NOT* Load Menu in 'HideLegacyTag' Function ... 1"
        );
        #endif

        FreeMenuScreen (&HideLegacyMenu);

        // Early Return
        return FALSE;
    }

    Name = PoolPrint (
        L"%-20s%s%s"
        L"Legacy BIOS Item",
        DEFAULT_STRING_DELIM,
        (
            BaseCheck
        ) ? LegacyLoader->me.Title : LegacyLoader->BdsOption->Description
    );

    AddMenuInfoLine (HideLegacyMenu, L"Hide Legacy Entry Below?", FALSE);
    AddMenuInfoLine (HideLegacyMenu, PoolPrint (L"%s", Name),      TRUE);

    if (!GetMenuEntryYesNo (&HideLegacyMenu)) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_STAR_SEPARATOR,
            L"Could *NOT* Load Menu in 'HideLegacyTag' Function ... 2"
        );
        #endif

        FreeMenuScreen (&HideLegacyMenu);

        // Early Return
        return FALSE;
    }

    DefaultEntry = 9999; // Use the Max Index
    Style = (
        AllowGraphicsMode
    ) ? GraphicsMenuStyle : TextMenuStyle;
    MenuExit = DrawMenuScreen (
        HideLegacyMenu, Style,
        &DefaultEntry, &ChosenOption
    );

    #if REFIT_DEBUG > 0
    LogExit (MenuExit, __func__, ChosenOption->Title);
    #endif

    if (MenuExit != MENU_EXIT_ENTER        ||
        MyStriCmp (ChosenOption->Title, L"No")
    ) {
        TagHidden = FALSE;
    }
    else {
        TagHidden = TRUE;
        AddToHiddenTags (L"HiddenLegacy", Name);
    }
    MY_FREE_POOL(Name);

    return TagHidden;
} // static BOOLEAN HideLegacyTag()

static
VOID HideTag (
    REFIT_MENU_ENTRY *ChosenOption
) {
    #if REFIT_DEBUG > 0
    CHAR16            *NoChanges;
    #endif

    UINTN              TagFlag;
    LOADER_ENTRY      *Loader;
    LEGACY_ENTRY      *LegacyLoader;
    REFIT_MENU_SCREEN *HideTagMenu;


    if (ChosenOption == NULL) {
        // Early Return
        return;
    }

    HideTagMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (HideTagMenu == NULL) {
        // Early Return
        return;
    }

    // DA-TAG: Set 'Hidden' icon in 'HideEfiTag' function later
    HideTagMenu->Hint1 = StrDuplicate (SELECT_OPTION_HINT     );
    HideTagMenu->Hint2 = StrDuplicate (RETURN_MAIN_SCREEN_HINT);

    TagFlag      = 0;
    LegacyLoader = (LEGACY_ENTRY *) ChosenOption;
    Loader       = (LOADER_ENTRY *) ChosenOption;

    // DA-TAG: Investigate This ... Probably related to 'El Gordo'.
    // Original: (BUG) RescanAll calls should be conditional on successful calls
    //         to HideEfiTag or HideLegacyTag. For the former however, this
    //         causes crashes on a second hide a tag call if the user chose "No"
    //         to the first call. This seems to be related to memory management
    //         of Volumes; the crash occurs in FindVolumeAndFilename() and lib.c
    //         when calling DevicePathToStr. Calling RescanAll() on all returns
    //         from HideEfiTag seems to be an effective workaround, but there is
    //         likely a memory management bug somewhere that is the root cause.
    // Update: One or more unknown memory conflicts, 'El Gordo' (The Big One),
    //         is likely what has been noted upstream above.
    //         Other apparent manifestations of El Gordo:
    //           - https://sf.net/p/refind/discussion/general/thread/ed3185fb40
    //           - https://sf.net/p/refind/discussion/general/thread/4dfcdfdd16
    //           - https://github.com/joevt/RefindPlus/commit/8c303d504d58bb235e9d2218df8bdb939de9ed77
    //           - https://github.com/RefindPlusRepo/RefindPlus/issues/163
    //         El Gordo is most likely a buffer overflow of some sort.
    //         El Gordo might be in one or more filesystem drivers.
    //         El Gordo might be spread across a number of files.
    switch (ChosenOption->Tag) {
        case TAG_LOADER:
            if (GlobalConfig.SyncAPFS &&
                Loader->Volume->FSType == FS_TYPE_APFS
            ) {
                DisplaySimpleMessage (
                    L"Amend Config File Instead ... Update \"dont_scan_volumes\" Token",
                    L"Hide Entry *IS NOT* Available on Synced APFS Loaders"
                );
            }
            else if (Loader->DiscoveryType != DISCOVERY_TYPE_AUTO) {
                DisplaySimpleMessage (
                    L"Amend Config File Instead ... Disable Stanza",
                    L"Hide Entry *IS NOT* Available on Manual Stanzas"
                );
            }
            else {
                HideTagMenu->Title = L"Hide UEFI Entry";
                TagFlag = (
                    HideEfiTag (Loader, HideTagMenu, L"HiddenTags")
                ) ? 1 : 2; // Changes Triggered : No Changes Triggered
            }

            break;
        case TAG_LEGACY:
        case TAG_LEGACY_UEFI:
            HideTagMenu->Title = L"Hide Legacy BIOS Entry";
            TagFlag = (
                HideLegacyTag (LegacyLoader, HideTagMenu)
            ) ? 1 : 2; // Changes Triggered : No Changes Triggered

            break;
        case TAG_FIRMWARE_LOADER:
            HideTagMenu->Title = L"Hide Firmware BootOption Entry";
            TagFlag = (
                HideFirmwareTag(Loader, HideTagMenu)
            ) ? 1 : 2; // Changes Triggered : No Changes Triggered

            break;
        case TAG_MOK:
        case TAG_SHELL:
        case TAG_GDISK:
        case TAG_GPTSYNC:
        case TAG_MEMTEST:
        case TAG_NETBOOT:
        case TAG_FWUPDATE:
        case TAG_RECOVERY_MAC:
        case TAG_RECOVERY_WIN:
            DisplaySimpleMessage (
                L"Amend Config File Instead ... Update \"showtools\" Token",
                L"Hide Entry *IS NOT* Available on This External Tool"
            );

            break;
        case TAG_EXIT:
        case TAG_ABOUT:
        case TAG_REBOOT:
        case TAG_HIDDEN:
        case TAG_INSTALL:
        case TAG_SHUTDOWN:
        case TAG_FIRMWARE:
        case TAG_BOOTORDER:
        case TAG_CSR_ROTATE:
        case TAG_CLEAN_NVRAM:
            DisplaySimpleMessage (
                L"Amend Config File Instead ... Update \"showtools\" Token",
                L"Hide Entry *IS NOT* Available on Any Internal Tool"
            );

            break;
        case TAG_TOOL:
            HideTagMenu->Title = L"Hide Tool Entry";
            TagFlag = (
                HideEfiTag (Loader, HideTagMenu, L"HiddenTags")
            ) ? 1 : 2; // Changes Triggered : No Changes Triggered
    } // switch

    if (TagFlag == 1) {
        #if REFIT_DEBUG > 0
        LOG_MSG("Received User Input:");
        LOG_MSG("%s  - %s", OffsetNext, HideTagMenu->Title);
        LOG_MSG("\n\n");
        #endif

        RescanAll (FALSE);
    }
    #if REFIT_DEBUG > 0
    else {
        if (TagFlag == 2) {
            NoChanges = L"No Changes Made on Hide Entry Call";
            LOG_MSG("INFO: %s:- '%s'", NoChanges, HideTagMenu->Title);
            LOG_MSG("\n\n");
            ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", NoChanges);
        }
    }
    #endif

    FreeMenuScreen (&HideTagMenu);
} // static VOID HideTag()

static
VOID FreeLegacyEntry (
    IN LEGACY_ENTRY **Entry
) {
    if (*Entry == NULL) {
        // Early Return
        return;
    }

    MY_FREE_POOL((*Entry)->me.Title);
    MY_FREE_IMAGE((*Entry)->me.Image);
    MY_FREE_IMAGE((*Entry)->me.BadgeImage);
    FreeMenuScreen (&(*Entry)->me.SubScreen);

    FreeBdsOption (&(*Entry)->BdsOption);
    MY_FREE_POOL((*Entry)->LoadOptions);
    MY_FREE_POOL(*Entry);
} // static VOID FreeLegacyEntry()

static
VOID FreeLoaderEntry (
    IN LOADER_ENTRY **Entry
) {
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    if (*Entry == NULL) {
        BREAD_CRUMB(L"%a:  1a 1 - END:- VOID", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    BREAD_CRUMB(L"%a:  2", __func__);
    FreeMenuScreen (&(*Entry)->me.SubScreen);

    BREAD_CRUMB(L"%a:  3", __func__);
    MY_FREE_POOL((*Entry)->me.Title);
    MY_FREE_IMAGE((*Entry)->me.Image);
    MY_FREE_IMAGE((*Entry)->me.BadgeImage);

    BREAD_CRUMB(L"%a:  4", __func__);
    MY_FREE_POOL((*Entry)->Title);
    MY_FREE_POOL((*Entry)->LoaderPath);
    MY_FREE_POOL((*Entry)->InitrdPath);
    MY_FREE_POOL((*Entry)->LoadOptions);
    MY_FREE_POOL((*Entry)->EfiLoaderPath);

    BREAD_CRUMB(L"%a:  5", __func__);
    MY_FREE_POOL(*Entry);

    BREAD_CRUMB(L"%a:  6 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // static VOID FreeLoaderEntry()

static
VOID GetStateInfo (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State
) {
    UINTN                 i;

    // Layout
    IconRowCount = 0;
    ToolRowCount = 0;
    IconRowItems = 0;

    for (i = 0; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 1) {
            ToolRowCount++;
        }
        else {
            IconRowItems++;
            if (IconRowCount < State->MaxVisible) {
                IconRowCount++;
            }
        }
    } // for

    IconRowPosX = (ScreenW + TILE_XSPACING - (TileSizes[0] + TILE_XSPACING) * IconRowCount) >> 1;
    IconRowPosY = ComputeRow0PosY (TRUE);
    ToolRowPosX = (ScreenW + TILE_XSPACING - (TileSizes[1] + TILE_XSPACING) * ToolRowCount) >> 1;
    ToolRowPosY = IconRowPosY + TileSizes[0] + TILE_YSPACING;

} // static VOID GetStateInfo()

static
VOID TextScreenHeader (
    IN CHAR16 *Title
) {
    BlankScreenLine();
    DrawScreenHeader (
        Title
    );
} // static VOID TextScreenHeader()

// Returns a constant ... Do *NOT* Free
CHAR16 * MenuExitInfo (
    IN UINTN MenuExit
) {
    CHAR16 *MenuExitData;


    switch (MenuExit) {
        case  1:  MenuExitData = L"ENTER"  ; break;
        case  2:  MenuExitData = L"ESCAPE" ; break;
        case  3:  MenuExitData = L"DETAILS"; break;
        case  4:  MenuExitData = L"TIMEOUT"; break;
        case  5:  MenuExitData = L"EJECT"  ; break;
        case  6:  MenuExitData = L"REMOVE" ; break;
        default:  MenuExitData = L"RETURN" ;  // Actually '99'
    } // switch

    return MenuExitData;
} // CHAR16 * MenuExitInfo()

VOID AddMenuInfoLine (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16            *InfoLine,
    IN BOOLEAN            CanFree
) {
    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Screen Menu Info Line:- '%s'", InfoLine);
    #endif

    // DA-TAG: The 'NewElement' item may be freed later
    //         So pass a duplicate if 'CanFree' is false
    AddListElement (
        (VOID ***) &(Screen->InfoLines),
        &(Screen->InfoLineCount),
        (CanFree) ? InfoLine : StrDuplicate (InfoLine)
    );
} // VOID AddMenuInfoLine()

VOID AddSubMenuEntry (
    IN REFIT_MENU_SCREEN *SubScreen,
    IN REFIT_MENU_ENTRY  *SubEntry
) {
    if (SubScreen == NULL || SubEntry == NULL) {
        // Early Return
        return;
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Set SubMenu Entry in %s - %s%s",
        SubScreen->Title,
        SubEntry->Title,
        SetVolType (NULL, SubEntry->Title, 0)
    );
    #endif

    AddListElement (
        (VOID ***) &(SubScreen->Entries),
        &(SubScreen->EntryCount),
        SubEntry
    );
} // VOID AddSubMenuEntry()

VOID AddMenuEntry (
    IN REFIT_MENU_SCREEN *Screen,
    IN REFIT_MENU_ENTRY  *Entry
) {
    if (Screen == NULL || Entry == NULL) {
        // Early Return
        return;
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Append Menu Entry to %s %s %s%s",
        Screen->Title,
        (
            MyStriCmp (Screen->Title, MAIN_MENU_NAME)
        ) ? L"-" : L" - ",
        Entry->Title,
        SetVolType (NULL, Entry->Title, 0)
    );
    // DA-TAG: Doubled Deliberately in SetVolType
    //         Find a better way
    #endif

    AddListElement (
        (VOID ***) &(Screen->Entries),
        &(Screen->EntryCount),
        Entry
    );
} // VOID AddMenuEntry()

VOID AddMenuEntryCopy (
    IN REFIT_MENU_SCREEN *Screen,
    IN REFIT_MENU_ENTRY  *Entry
) {
    if (Screen == NULL || Entry == NULL) {
        // Early Return
        return;
    }

    AddMenuEntry (Screen, CopyMenuEntry (Entry));
} // VOID AddMenuEntryCopy()

INTN FindMenuShortcutEntry (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16            *Defaults
) {
    UINTN    i, j;
    CHAR16  *Shortcut;
    BOOLEAN  FoundMatch;


    i = j = 0;
    FoundMatch = FALSE;
    while (1) {
        Shortcut = FindCommaDelimited (
            Defaults, j++
        );
        if (Shortcut == NULL) break;

        if (StrLen (Shortcut) > 1) {
            for (i = 0; i < Screen->EntryCount; i++) {
                if (MyStriCmp (Shortcut, Screen->Entries[i]->Title)) {
                    FoundMatch = TRUE;
                    break;
                }
            } // for

            if (!FoundMatch) {
                for (i = 0; i < Screen->EntryCount; i++) {
                    if (IsStriStr (Screen->Entries[i]->Title, Shortcut)) {
                        FoundMatch = TRUE;
                        break;
                    }
                } // for
            }
        }
        else {
            if (Shortcut[0] >= 'a' && Shortcut[0] <= 'z') {
                Shortcut[0] -= ('a' - 'A');
            }

            if (Shortcut[0]) {
                if (Shortcut[0] != 0 && Shortcut[0] != 'O') {
                    for (i = 0; i < Screen->EntryCount; i++) {
                        if (Screen->Entries[i]->ShortcutKey == Shortcut[0]) {
                            FoundMatch = TRUE;
                            break;
                        }
                    } // for
                }
            }
        } // if/else StrLen (Shortcut) > 1

        MY_FREE_POOL(Shortcut);

        if (FoundMatch) {
            return i;
        }
    } // while {Infinite}

    return -1;
} // INTN FindMenuShortcutEntry()

UINTN DrawMenuScreen (
    IN     REFIT_MENU_SCREEN   *Screen,
    IN     MENU_STYLE_FUNC      StyleFunc,
    IN OUT INTN                *DefaultEntryIndex,
    OUT    REFIT_MENU_ENTRY  **ChosenOption
) {
    #if REFIT_DEBUG > 0
    CHAR16                     *MsgStr;
    CHAR16                     *KeyTxt;
    BOOLEAN                     TmpLevel;

    static BOOLEAN              OnceWait  = FALSE;
    static INTN                 PreviousItem = -1;
    #endif

    EFI_STATUS                  Status;
    EFI_STATUS                  PointerStatus;
    BOOLEAN                     Rotated;
    BOOLEAN                     IsMainMenu;
    BOOLEAN                     HaveTimeout;
    BOOLEAN                     UserKeyScan;
    BOOLEAN                     UserKeyPress;
    BOOLEAN                     WaitForRelease;
    INTN                        TimeoutCountdown;
    INTN                        TimeSinceKeystroke;
    INTN                        PreviousTime;
    INTN                        CurrentTime;
    INTN                        ShortcutEntry;
    UINTN                       ElapsCount;
    UINTN                       Input;
    UINTN                       Item;
    UINTN                       MenuExit;
    UINT64                      MenuExitNumb;
    UINT64                      MenuExitGate;
    UINT64                      MenuExitTime;
    UINT64                      MenuExitDiff;
    CHAR16                     *TimeoutMessage;
    CHAR16                      KeyAsString[2];
    SCROLL_STATE                State;
    POINTER_STATE               PointerState;
    EFI_INPUT_KEY               key;


    IsMainMenu = MyStriCmp (Screen->Title, MAIN_MENU_NAME);

    #if REFIT_DEBUG > 0
    if (OnceWait || !IsMainMenu) {
        if (!IsMainMenu) {
            ALT_LOG(1, LOG_LINE_THIN_SEP, L"Draw Menu Screen");
        }
        else {
            ALT_LOG(1, LOG_THREE_STAR_SEP, L"Show Main Screen");
        }
        ALT_LOG(1, LOG_LINE_NORMAL, L"Screen Title:- '%s'", Screen->Title);
    }

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    #endif

    if (Screen->TimeoutSeconds < 1) {
        HaveTimeout = FALSE;
        TimeoutCountdown = 0;
    }
    else {
        HaveTimeout = TRUE;
        TimeoutCountdown = Screen->TimeoutSeconds * 10;
    }

    StyleFunc (Screen, &State, MENU_FUNCTION_INIT, NULL);
    IdentifyRows (&State, Screen);

    // Override the starting selection with the default index, if any
    if (*DefaultEntryIndex == 9999) {
        *DefaultEntryIndex = State.MaxIndex;
    }

    if (*DefaultEntryIndex >= 0           &&
        *DefaultEntryIndex <= State.MaxIndex
    ) {
        State.CurrentSelection = *DefaultEntryIndex;
        if (GlobalConfig.ScreensaverTime != -1) {
            UpdateScroll (&State, SCROLL_NONE);
        }
    }

    WaitForRelease = FALSE;
    MenuExit = MENU_EXIT_ZERO; // Temporary in case we need to abort DirectBoot

    if (Screen->TimeoutSeconds == -1) {
        Status = REFIT_CALL_2_WRAPPER(
            gST->ConIn->ReadKeyStroke,
            gST->ConIn, &key
        );
        if (!EFI_ERROR(Status)) {
            KeyAsString[0] = key.UnicodeChar;
            KeyAsString[1] = 0;

            ShortcutEntry = FindMenuShortcutEntry (Screen, KeyAsString);
            if (ShortcutEntry >= 0) {
                State.CurrentSelection = ShortcutEntry;
            }
            else {
                if (GlobalConfig.DirectBoot) {
                    // DA-TAG: If we enter here, a shortcut key was pressed but not found.
                    //         Load the screen menu ... Without tools for Main Menu.
                    //         Tools are not loaded with DirectBoot for speed.
                    //         Enable Rescan to allow tools to be loaded.
                    //         Also disable Timeout just in case.
                    BlockRescan = FALSE;
                    Screen->TimeoutSeconds = 0;

                    // Flag Abort DirectBoot
                    MenuExit = MENU_EXIT_SHOWSCREEN;
                }

                WaitForRelease =  TRUE;
                HaveTimeout    = FALSE;

                while (WaitForRelease) {
                    // Ensure no keys are being held down
                    Status = REFIT_CALL_2_WRAPPER(
                        gST->ConIn->ReadKeyStroke,
                        gST->ConIn, &key
                    );
                    if (!EFI_ERROR(Status)) {
                        // Reset to keep the keystroke buffer clear
                        REFIT_CALL_2_WRAPPER(
                            gST->ConIn->Reset,
                            gST->ConIn, FALSE
                        );

                        continue;
                    }

                    WaitForRelease = FALSE;
                    REFIT_CALL_2_WRAPPER(
                        gST->ConIn->Reset,
                        gST->ConIn, TRUE
                    );
                } // while
            } // if/else ShortcutEntry >= 0
        } // if !EFI_ERROR(Status))
    } // if Screen->TimeoutSeconds == -1

    if (GlobalConfig.DirectBoot) {
        // DA-TAG: DirectBoot is active.
        //         Either abort or proceed.
        if (MenuExit == MENU_EXIT_ZERO) {
            // DA-TAG: Proceed with DirectBoot.
            MenuExit = MENU_EXIT_ENTER;
        }
    }
    else {
        if (!AllowGraphicsMode && IsMainMenu) {
            TextScreenHeader (
                Screen->Title
            );
        }

        if (GlobalConfig.ScreensaverTime != -1) {
            State.PaintAll = TRUE;
        }
    }

    #if REFIT_DEBUG > 0
    if (!OnceWait && IsMainMenu) {
        OnceWait = TRUE;

        MsgStr = PoolPrint (
            L"Loaded RefindPlus %s on %s Firmware",
            REFINDPLUS_VERSION, VendorInfo
        );
        TmpLevel = (
            GlobalConfig.LogLevel == 0
        ) ? TRUE : FALSE;
        if (TmpLevel) {
            GlobalConfig.LogLevel = 1;
        }
        else {
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        }
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        if (TmpLevel) {
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            GlobalConfig.LogLevel = 0;
        }

        MY_FREE_POOL(MsgStr);
        if (GlobalConfig.DirectBoot) {
            MsgStr = StrDuplicate (
                L"E X E C U T E   D I R E C T   B O O T"
            );
        }
        else {
            MsgStr = StrDuplicate (
                L"P R O C E S S   U S E R   I N P U T"
            );
        }
        ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
        LOG_MSG("%s", MsgStr);
        LOG_MSG("\n");
        MY_FREE_POOL(MsgStr);

        if (!GlobalConfig.DirectBoot) {
            TmpLevel = (
                GlobalConfig.LogLevel == 0
            ) ? TRUE : FALSE;
            if (TmpLevel) {
                GlobalConfig.LogLevel = 1;
            }
            ALT_LOG(1, LOG_LINE_NORMAL, L"** Awaiting User Input **");
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            if (TmpLevel) {
                GlobalConfig.LogLevel = 0;
            }
        }
    }
    #endif

    PreviousTime                         =    -1;
    TimeSinceKeystroke                   =     0;
    UserKeyPress = UserKeyScan = Rotated = FALSE;
    while (MenuExit == MENU_EXIT_ZERO) {
        // Update the screen
        pdClear (TRUE);
        if (State.PaintAll               &&
            GlobalConfig.ScreensaverTime != -1
        ) {
            StyleFunc (
                Screen, &State,
                MENU_FUNCTION_PAINT_ALL,
                NULL
            );
            State.PaintAll = FALSE;
        }
        else {
            if (State.PaintSelection) {
                StyleFunc (
                    Screen, &State,
                    MENU_FUNCTION_PAINT_SELECTION,
                    NULL
                );
                State.PaintSelection = FALSE;
            }
        }
        pdDraw();

        // DA-TAG: Investigate This
        //         Toggle the selection once to work around failure to
        //         display the default selection on load in text mode.
        //         This is a workaround ... Proper solution needed.
        if (!Rotated) {
            Rotated = TRUE;
            if (State.ScrollMode == SCROLL_MODE_TEXT) {
                if (State.CurrentSelection < State.MaxIndex) {
                    UpdateScroll (&State, SCROLL_LINE_DOWN);
                    REFIT_CALL_1_WRAPPER(gBS->Stall, 1250);
                    UpdateScroll (&State, SCROLL_LINE_UP);
                }
                else if (State.CurrentSelection > 0) {
                    UpdateScroll (&State, SCROLL_LINE_UP);
                    REFIT_CALL_1_WRAPPER(gBS->Stall, 1250);
                    UpdateScroll (&State, SCROLL_LINE_DOWN);
                }
                else {
                    UpdateScroll (&State, SCROLL_NONE);
                }
            }
        }

        if (HaveTimeout) {
            CurrentTime = (TimeoutCountdown + 5) / 10;
            if (CurrentTime != PreviousTime) {
               TimeoutMessage = PoolPrint (
                   L"%s in %d Seconds",
                   Screen->TimeoutText,
                   CurrentTime
               );

               if (GlobalConfig.ScreensaverTime != -1) {
                   StyleFunc (
                       Screen, &State,
                       MENU_FUNCTION_PAINT_TIMEOUT,
                       TimeoutMessage
                   );
               }

               MY_FREE_POOL(TimeoutMessage);

               PreviousTime = CurrentTime;
            }
        }

        // Read keypress or pointer event
        //   (wait for them if applicable)
        if (!PointerEnabled) {
            PointerStatus = EFI_NOT_READY;
        }
        else {
            PointerStatus = pdUpdateState();
        }

        Status = REFIT_CALL_2_WRAPPER(
            gST->ConIn->ReadKeyStroke,
            gST->ConIn, &key
        );
        if (!EFI_ERROR(Status)) {
            PointerActive      = FALSE;
            DrawSelection      =  TRUE;
            TimeSinceKeystroke =     0;
        }
        else if (!EFI_ERROR(PointerStatus)) {
            PointerActive      = TRUE;
            TimeSinceKeystroke =    0;

            if (pdGetState().Press &&
                StyleFunc != MainMenuStyle
            ) {
                // Exit submenus on pointer click.
                // Returns to main menu by default.
                MenuExit = MENU_EXIT_ENTER;
                UserKeyPress = TRUE;

                break;
            }
        }
        else {
            if (HaveTimeout      &&
                TimeoutCountdown == 0
            ) {
                // Timeout expired
                #if REFIT_DEBUG > 0
                ALT_LOG(1, LOG_LINE_NORMAL,
                    L"Menu Timeout Expired:- '%d Seconds'",
                    Screen->TimeoutSeconds
                );
                #endif

                MenuExit = MENU_EXIT_TIMEOUT;
                break;
            }

            if (!HaveTimeout &&
                GlobalConfig.ScreensaverTime < 1
            ) {
                /* coverity[check_return: SUPPRESS] */
                WaitForInput (0);
            }
            else {
                ElapsCount =                   1;
                Input      = WaitForInput (1000); // 1s Timeout

                if (Input == INPUT_KEY ||
                    Input == INPUT_POINTER
                ) {
                    TimeSinceKeystroke = 0;
                    continue;
                }

                if (Input == INPUT_TIMEOUT) {
                    // Always counted as is to end of the timeout
                    ElapsCount = 10;
                }

                TimeSinceKeystroke += ElapsCount;
                if (HaveTimeout) {
                    TimeoutCountdown = (
                        TimeoutCountdown > ElapsCount
                    ) ? TimeoutCountdown - ElapsCount : 0;
                }
                else {
                    if (GlobalConfig.ScreensaverTime > 0 &&
                        TimeSinceKeystroke > (
                            GlobalConfig.ScreensaverTime * 10
                        )
                    ) {
                        SaveScreen();
                        State.PaintAll     = TRUE;
                        TimeSinceKeystroke =    0;

                        if (!AllowGraphicsMode) {
                            TextScreenHeader (
                                Screen->Title
                            );
                        }
                    }
                }
            } // if/else !HaveTimeout && GlobalConfig.ScreensaverTime < 1

            continue;
        } // if/else !EFI_ERROR(Status)

        if (HaveTimeout) {
            // User pressed a key ... Cancel timeout
            StyleFunc (
                Screen, &State,
                MENU_FUNCTION_PAINT_TIMEOUT, L""
            );
            HaveTimeout = FALSE;

            if (GlobalConfig.ScreensaverTime == -1) {
                // Cancel start-with-blank-screen coding
                GlobalConfig.ScreensaverTime = 0;

                if (!GlobalConfig.TextOnly) {
                    BltClearScreen (TRUE);
                }
            }
        }

        if (!PointerActive) {
            // React to key press
            switch (key.ScanCode) {
                case SCAN_RIGHT:     UpdateScroll (&State, SCROLL_LINE_RIGHT);  break;
                case SCAN_LEFT:      UpdateScroll (&State, SCROLL_LINE_LEFT );  break;
                case SCAN_DOWN:      UpdateScroll (&State, SCROLL_LINE_DOWN );  break;
                case SCAN_UP:        UpdateScroll (&State, SCROLL_LINE_UP   );  break;
                case SCAN_END:       UpdateScroll (&State, SCROLL_LAST      );  break;
                case SCAN_HOME:      UpdateScroll (&State, SCROLL_FIRST     );  break;
                case SCAN_PAGE_UP:   UpdateScroll (&State, SCROLL_PAGE_UP   );  break;
                case SCAN_PAGE_DOWN: UpdateScroll (&State, SCROLL_PAGE_DOWN );  break;
                case SCAN_INSERT:
                case SCAN_F2:                  MenuExit = MENU_EXIT_DETAILS   ; break;
                case SCAN_F10:                 MenuExit = MENU_EXIT_SCREENSHOT; break;
                case SCAN_ESC:                 MenuExit = MENU_EXIT_ESCAPE    ; break;
                case SCAN_DELETE:              MenuExit = MENU_EXIT_HIDE      ; break;
                case 0x0016: if (EjectMedia()) MenuExit = MENU_EXIT_ESCAPE    ; break; // F12
            } // switch

            switch (key.UnicodeChar) {
                case CHAR_LINEFEED:
                case CHAR_CARRIAGE_RETURN: MenuExit = MENU_EXIT_ENTER     ;     break;
                case ' ':
                case CHAR_BACKSPACE:       MenuExit = MENU_EXIT_ESCAPE    ;     break;
                case '+':
                case CHAR_TAB:             MenuExit = MENU_EXIT_DETAILS   ;     break;
                case '-':                  MenuExit = MENU_EXIT_HIDE      ;     break;
                case '\\':                 MenuExit = MENU_EXIT_SCREENSHOT;     break;
                default:
                    KeyAsString[1] = 0;
                    KeyAsString[0] = key.UnicodeChar;
                    ShortcutEntry  = FindMenuShortcutEntry (
                        Screen,
                        KeyAsString
                    );

                    if (ShortcutEntry >= 0) {
                        State.CurrentSelection = ShortcutEntry;
                        MenuExit = MENU_EXIT_ENTER;
                    }

                    break;
            } // switch

            // Flag 'UserKeyPress' on Selection Change
            switch (key.ScanCode) {
                case SCAN_PAGE_DOWN:
                case SCAN_PAGE_UP:
                case SCAN_HOME:
                case SCAN_END:
                case SCAN_UP:
                case SCAN_LEFT:
                case SCAN_DOWN:
                case SCAN_RIGHT: UserKeyPress = TRUE;
            } // switch

            // Flag 'UserKeyScan' on Detecting Some Inputs
            switch (key.UnicodeChar) {
                case CHAR_LINEFEED:
                case CHAR_CARRIAGE_RETURN:
                case CHAR_BACKSPACE:
                case CHAR_TAB:
                case '\\':
                case ' ':
                case '+':
                case '-': UserKeyScan = TRUE;
            } // switch

            #if REFIT_DEBUG > 0
            KeyTxt = GetScanCodeText (key.ScanCode);
            if (MyStriCmp (KeyTxt, L"UNMAPPED_KEY")) {
                switch (key.UnicodeChar) {
                    case CHAR_LINEFEED:        KeyTxt = L"INFER_ENTER       Key: LineFeed"        ; break;
                    case CHAR_CARRIAGE_RETURN: KeyTxt = L"INFER_ENTER       Key: CarriageReturn"  ; break;
                    case CHAR_BACKSPACE:       KeyTxt = L"INFER_ESCAPE      Key: BackSpace"       ; break;
                    case ' ':                  KeyTxt = L"INFER_ESCAPE      Key: SpaceBar"        ; break;
                    case CHAR_TAB:             KeyTxt = L"INFER_DETAILS     Key: Tab"             ; break;
                    case '+':                  KeyTxt = L"INFER_DETAILS     Key: '+' (Plus)"      ; break;
                    case '-':                  KeyTxt = L"INFER_REMOVE      Key: '-' (Minus)"     ; break;
                    case '\\':                 KeyTxt = L"SCREENSHOT        Key: '\\' (BackSlash)"; break;
                } // switch
            }
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"Keystroke : UnicodeChar = 0x%02X ... ScanCode = 0x%02X - %s",
                key.UnicodeChar, key.ScanCode, KeyTxt
            );
            #endif

            if (BlockRescan) {
                if (MenuExit == MENU_EXIT_ESCAPE) {
                    MenuExit =  MENU_EXIT_ZERO;
                }
                else {
                    if (MenuExit == MENU_EXIT_ZERO) {
                        // Unblock Rescan on Key Press/Scan
                        BlockRescan = (
                            UserKeyPress || UserKeyScan
                        ) ? FALSE : TRUE;
                    }
                }
            }

            if (MenuExit == MENU_EXIT_SCREENSHOT) {
                if (key.ScanCode != SCAN_F10  ||
                    !GlobalConfig.DecoupleKeyF10
                ) {
                    egScreenShot();

                    // Unblock Rescan then Refresh Screen
                    BlockRescan    = FALSE;
                    State.PaintAll =  TRUE;
                    WaitForRelease =  TRUE;
                }

                MenuExit = MENU_EXIT_ZERO;
                continue;
            }
        }
        else {
            if (StyleFunc != MainMenuStyle) {
                // Nothing to find on submenus
                continue;
            }

            State.PreviousSelection = State.CurrentSelection;
            PointerState = pdGetState();
            Item = FindMainMenuItem (
                Screen, &State,
                PointerState.X, PointerState.Y
            );

            switch (Item) {
                case POINTER_NO_ITEM:
                    #if REFIT_DEBUG > 0
                    // Reset 'PreviousItem' Flag
                    if (PreviousItem != -1) {
                        PreviousItem  = -1;
                    }
                    #endif

                    if (DrawSelection) {
                        DrawSelection        = FALSE;
                        State.PaintSelection =  TRUE;
                    }
                    else {
                        if (!PointerState.Press) {
                            pdUpdateState();
                        }
                    }

                break;
                case POINTER_LEFT_ARROW:
                    if (PointerState.Press) {
                        UpdateScroll (
                            &State,
                            SCROLL_PAGE_UP
                        );
                        UserKeyPress = TRUE;
                        BlockRescan = FALSE;
                    }

                    if (DrawSelection) {
                        DrawSelection        = FALSE;
                        State.PaintSelection =  TRUE;
                    }

                    // Log Pointer Event
                    #if REFIT_DEBUG > 0
                    if (PreviousItem != Item) {
                        PreviousItem  = Item;

                        ALT_LOG(1, LOG_LINE_NORMAL,
                            L"Process Pointer Event ... Arrow Left"
                        );
                    }
                    #endif

                break;
                case POINTER_RIGHT_ARROW:
                    if (PointerState.Press) {
                        UpdateScroll (
                            &State,
                            SCROLL_PAGE_DOWN
                        );
                        UserKeyPress = TRUE;
                        BlockRescan = FALSE;
                    }

                    if (DrawSelection) {
                        DrawSelection        = FALSE;
                        State.PaintSelection =  TRUE;
                    }

                    // Log Pointer Event
                    #if REFIT_DEBUG > 0
                    if (PreviousItem != Item) {
                        PreviousItem  = Item;

                        ALT_LOG(1, LOG_LINE_NORMAL,
                            L"Process Pointer Event ... Arrow Right"
                        );
                    }
                    #endif

                break;
                default:
                    #if REFIT_DEBUG > 0
                    // Reset 'PreviousItem' Flag
                    if (PreviousItem != -1) {
                        PreviousItem  = -1;
                    }
                    #endif

                    if (!DrawSelection || Item != State.CurrentSelection) {
                        DrawSelection          = TRUE;
                        State.PaintSelection   = TRUE;
                        State.CurrentSelection = Item;
                    }

                    if (!PointerState.Press) {
                        pdUpdateState();
                    }
                    else {
                        MenuExit = MENU_EXIT_ENTER;
                        BlockRescan        = FALSE;
                        UserKeyPress        = TRUE;

                        // Log Pointer Event
                        #if REFIT_DEBUG > 0
                        ALT_LOG(1, LOG_LINE_NORMAL,
                            L"Process Pointer Event ... Click"
                        );
                        #endif
                    }
            } // switch
        } // if/else !PointerActive
    } // while

    pdClear (TRUE);
    StyleFunc (
        Screen, &State,
        MENU_FUNCTION_CLEANUP, NULL
    );

    // Ignore MenuExit if FlushFailedTag is set and not previously reset
    if (FlushFailedTag && !FlushFailReset) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (
            L"FlushFailedTag is Set ... Ignore MenuExit"
        );
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
        LOG_MSG("INFO: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        FlushFailedTag = FALSE;
        FlushFailReset = TRUE;
        MenuExit = MENU_EXIT_ZERO;
    }

    do {
        if (UserKeyPress || UserKeyScan) {
            OneMainLoop = TRUE;
        }

        // Ignore MenuExit if time between loading main menu and detecting
        // an 'Enter' keypress is too low. Primed Keystroke Buffers appear
        // to only affect UEFI PC and provision is not made for Apple Macs
        if (AppleFirmware) break;

        // Others to be ignored
        if (!IsMainMenu              ||
            OneMainLoop              ||
            ClearedBuffer            ||
            FlushFailReset           ||
            GlobalConfig.DirectBoot  ||
            MenuExit != MENU_EXIT_ENTER
        ) {
            break;
        }

        MenuExitNumb = 768; // 512 + 256
        MenuExitGate = MenuExitNumb;
        MenuExitTime = GetCurrentMS();
        MenuExitDiff = MenuExitTime - MainMenuLoad;

        if (GlobalConfig.MitigatePrimedBuffer) {
            MenuExitGate = MenuExitNumb * 3;

            #if REFIT_DEBUG > 0
            if (GlobalConfig.LogLevel > 1) {
                MenuExitGate = MenuExitNumb * 5;
            }
            else {
                if (GlobalConfig.LogLevel > 0) {
                    MenuExitGate = MenuExitNumb * 4;
                }
            }
            #endif

            if (FoundExternalDisk) {
                MenuExitGate = MenuExitGate * 4;
            }
        }

        if (MenuExitDiff < MenuExitGate) {
            #if REFIT_DEBUG > 0
            LOG_MSG("INFO: Invalid Post-Load MenuExit Interval ... Ignore MenuExit");
            MsgStr = L"Mitigated Potential Persistent Primed Keystroke Buffer";
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
            LOG_MSG("%s      %s", OffsetNext, MsgStr);
            LOG_MSG("\n\n");
            #endif

            FlushFailedTag = FALSE;
            FlushFailReset = TRUE;
            MenuExit = MENU_EXIT_ZERO;
        }
    } while (0); // This 'loop' only runs once

    if (ChosenOption) {
        *ChosenOption = Screen->Entries[State.CurrentSelection];
    }

    *DefaultEntryIndex = State.CurrentSelection;

    BREAD_CRUMB(L"%a:  2 - END:- return UINTN MenuExit = '%d'", __func__, MenuExit);
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return MenuExit;
} // UINTN DrawMenuScreen()

// Do most of the work for text-based menus.
VOID TextMenuStyle (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    IN UINTN              Function,
    IN CHAR16            *ParamText
) {
    #if REFIT_DEBUG > 0
    BOOLEAN         CheckMute = FALSE;
    #endif

    UINTN           i;
    UINTN           MenuWidth;
    UINTN           ItemWidth;
    UINTN           MenuHeight;

    static UINTN    MenuPosY;
    static CHAR16 **DisplayStrings;


    State->ScrollMode = SCROLL_MODE_TEXT;

    switch (Function) {
        case MENU_FUNCTION_INIT:
            // Vertical Layout
            MenuPosY = 4;
            if (Screen->InfoLineCount > 0) {
                MenuPosY += Screen->InfoLineCount + 1;
            }

            MenuHeight = ConHeight - MenuPosY - 3;
            if (Screen->TimeoutSeconds > 0) {
                MenuHeight -= 2;
            }
            InitScroll (
                State,
                Screen->EntryCount,
                MenuHeight
            );

            // Determine Menu Width ... Minimum = 20
            MenuWidth = 20;
            for (i = 0; i <= State->MaxIndex; i++) {
                ItemWidth = StrLen (Screen->Entries[i]->Title);
                if (MenuWidth < ItemWidth) {
                    MenuWidth = ItemWidth;
                }
            }

            i = (
                ConWidth > 2
            ) ? ConWidth - 3 : 0;

            MenuWidth += 2;
            if (MenuWidth > i) {
                MenuWidth = i;
            }

            // Prepare Strings for Display
            DisplayStrings = AllocatePool (
                Screen->EntryCount * sizeof (CHAR16 *)
            );
            for (i = 0; i <= State->MaxIndex; i++) {
                // Note: Theoretically, 'SPrint' is a cleaner way to do this; but the
                // description of the StrSize parameter to SPrint implies it is measured
                // in characters, but in practice both TianoCore and GNU-EFI seem to
                // use bytes instead, resulting in truncated displays. The size of
                // the StrSize parameter could just be doubled, but that seems unsafe
                // in case a future library change starts treating this as characters,
                // so it is being done the 'hard' way in this instance.
                //
                // DA-TAG: Investigate This
                //         Review the above and possibly change other uses of 'SPrint'
                DisplayStrings[i] = AllocateZeroPool (
                    sizeof (CHAR16) * 2
                );
                DisplayStrings[i][0] = L' ';

                #if REFIT_DEBUG > 0
                MY_MUTELOGGER_SET;
                #endif
                MergeStrings (&DisplayStrings[i], Screen->Entries[i]->Title, 0);
                #if REFIT_DEBUG > 0
                MY_MUTELOGGER_OFF;
                #endif

                // DA-TAG: Investigate This
                //         1. Improve shortening long strings ... Ellipses in the middle
                //         2. Account for double-width characters
                if (StrLen (DisplayStrings[i]) > MenuWidth) {
                    DisplayStrings[i][MenuWidth - 1] = 0;
                }
            } // for

        break;
        case MENU_FUNCTION_CLEANUP:
            // Release Temp Memory
            for (i = 0; i <= State->MaxIndex; i++) {
                MY_FREE_POOL(DisplayStrings[i]);
            }
            MY_FREE_POOL(DisplayStrings);

        break;
        case MENU_FUNCTION_PAINT_ALL:
            // Paint Whole Screen ... Initially and After Scrolling
            ShowTextInfoLines (Screen);
            for (i = 0; i <= State->MaxIndex; i++) {
                if (i >= State->FirstVisible && i <= State->LastVisible) {
                    REFIT_CALL_3_WRAPPER(
                        gST->ConOut->SetCursorPosition, gST->ConOut,
                        2, MenuPosY + (i - State->FirstVisible)
                    );

                    if (i == State->CurrentSelection) {
                        REFIT_CALL_2_WRAPPER(
                            gST->ConOut->SetAttribute,
                            gST->ConOut, ATTR_CHOICE_CURRENT
                        );
                    }
                    else {
                        if (DisplayStrings[i]) {
                            REFIT_CALL_2_WRAPPER(
                                gST->ConOut->SetAttribute,
                                gST->ConOut, ATTR_CHOICE_BASIC
                            );
                            REFIT_CALL_2_WRAPPER(
                                gST->ConOut->OutputString,
                                gST->ConOut, DisplayStrings[i]
                            );
                        }
                    }
                }
            } // for

            // Scrolling Indicators
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->SetAttribute,
                gST->ConOut, ATTR_SCROLLARROW
            );
            REFIT_CALL_3_WRAPPER(
                gST->ConOut->SetCursorPosition, gST->ConOut,
                0, MenuPosY
            );

            if (State->FirstVisible > 0) {
                gST->ConOut->OutputString (
                    gST->ConOut, ArrowUp
                );
            }
            else {
                gST->ConOut->OutputString (
                    gST->ConOut, L" "
                );
            }

            gST->ConOut->SetCursorPosition (
                gST->ConOut, 0,
                MenuPosY + State->MaxVisible
            );

            if (State->LastVisible < State->MaxIndex) {
                REFIT_CALL_2_WRAPPER(
                    gST->ConOut->OutputString,
                    gST->ConOut, ArrowDown
                );
            }
            else {
                REFIT_CALL_2_WRAPPER(
                    gST->ConOut->OutputString,
                    gST->ConOut, L" "
                );
            }

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
               if (Screen->Hint1 != NULL) {
                   REFIT_CALL_3_WRAPPER(
                       gST->ConOut->SetCursorPosition, gST->ConOut,
                       0, ConHeight - 2
                   );
                   REFIT_CALL_2_WRAPPER(
                       gST->ConOut->OutputString,
                       gST->ConOut, Screen->Hint1
                   );
               }

               if (Screen->Hint2 != NULL) {
                   REFIT_CALL_3_WRAPPER(
                       gST->ConOut->SetCursorPosition, gST->ConOut,
                       0, ConHeight - 1
                   );
                   REFIT_CALL_2_WRAPPER(
                       gST->ConOut->OutputString,
                       gST->ConOut, Screen->Hint2
                   );
               }
            }

        break;
        case MENU_FUNCTION_PAINT_SELECTION:
            // Redraw Selection Cursor
            REFIT_CALL_3_WRAPPER(
                gST->ConOut->SetCursorPosition, gST->ConOut,
                2, MenuPosY + (State->PreviousSelection - State->FirstVisible)
            );
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->SetAttribute,
                gST->ConOut, ATTR_CHOICE_BASIC
            );

            if (DisplayStrings[State->PreviousSelection] != NULL) {
                REFIT_CALL_2_WRAPPER(
                    gST->ConOut->OutputString,
                    gST->ConOut, DisplayStrings[State->PreviousSelection]
                );
            }

            REFIT_CALL_3_WRAPPER(
                gST->ConOut->SetCursorPosition, gST->ConOut,
                2, MenuPosY + (State->CurrentSelection - State->FirstVisible)
            );
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->SetAttribute,
                gST->ConOut, ATTR_CHOICE_CURRENT
            );
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->OutputString,
                gST->ConOut, DisplayStrings[State->CurrentSelection]
            );

        break;
        case MENU_FUNCTION_PAINT_TIMEOUT:
            if (ParamText[0] == 0) {
                // Clear Message
                if (BlankLine == NULL) {
                    BlankScreenLine();
                }

                REFIT_CALL_2_WRAPPER(
                    gST->ConOut->SetAttribute,
                    gST->ConOut, ATTR_BASIC
                );
                REFIT_CALL_3_WRAPPER(
                    gST->ConOut->SetCursorPosition, gST->ConOut,
                    0, ConHeight - 3
                );
                REFIT_CALL_2_WRAPPER(
                    gST->ConOut->OutputString,
                    gST->ConOut, BlankLine + 1
                );
            }
            else {
                // Paint or Update Message
                REFIT_CALL_2_WRAPPER(
                    gST->ConOut->SetAttribute,
                    gST->ConOut, ATTR_ERROR
                );
                REFIT_CALL_3_WRAPPER(
                    gST->ConOut->SetCursorPosition, gST->ConOut,
                    3, ConHeight - 3
                );
                REFIT_CALL_2_WRAPPER(
                    gST->ConOut->OutputString,
                    gST->ConOut, ParamText
                );
            }
    } // switch
} // VOID TextMenuStyle()

// Displays sub-menus
VOID GraphicsMenuStyle (
    IN REFIT_MENU_SCREEN  *Screen,
    IN SCROLL_STATE       *State,
    IN UINTN               Function,
    IN CHAR16             *ParamText
) {
    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    INTN      i;
    UINTN     TmpDim;
    BOOLEAN   VetStr;
    EG_IMAGE *Window;
    EG_PIXEL *BackgroundPixel = &(
        GlobalConfig.ScreenBackground->PixelData[0]
    );

    static UINTN EntriesPosX;
    static UINTN EntriesPosY;
    static UINTN LineWidth;
    static UINTN MenuWidth;
    static UINTN MenuHeight;
    static UINTN TimeoutPosY;
    static UINTN TitlePosX;
    static UINTN CharWidth;


    CharWidth = egGetFontCellWidth();
    State->ScrollMode = SCROLL_MODE_TEXT;

    switch (Function) {
        case MENU_FUNCTION_CLEANUP:
            // Nothing To Do Here
        break;
        case MENU_FUNCTION_INIT:
            InitScroll (
                State,
                Screen->EntryCount, 0
            );
            ComputeSubScreenWindowSize (
                Screen, State,
                &EntriesPosX, &EntriesPosY,
                &MenuWidth, &MenuHeight,
                &LineWidth
            );

            // Timeout Position is After Entry Block Plus Spacer Line
            TmpDim = TextLineHeight() * (Screen->EntryCount + 1);
            TimeoutPosY = EntriesPosY + TmpDim;

            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_SET;
            #endif
            // Initial Painting
            SwitchToGraphicsAndClear (TRUE);
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_OFF;
            #endif

            Window = egCreateFilledImage (
                MenuWidth, MenuHeight,
                FALSE, BackgroundPixel
            );

            if (Window) {
                egDrawImage (
                    Window,
                    EntriesPosX,
                    EntriesPosY
                );
                MY_FREE_IMAGE(Window);
            }

            TmpDim = egComputeTextWidth (
                Screen->Title
            );
            if (MenuWidth > TmpDim) {
                TitlePosX = EntriesPosX + (
                    (MenuWidth - TmpDim) / 2
                ) - CharWidth;
            }
            else {
               TitlePosX = EntriesPosX;
               if (CharWidth > 0) {
                  i = (MenuWidth / CharWidth) - 2;
                  if (i > 0) {
                      Screen->Title[i] = 0;
                  }
               }
            }

        break;
        case MENU_FUNCTION_PAINT_ALL:
            ComputeSubScreenWindowSize (
                Screen, State,
                &EntriesPosX, &EntriesPosY,
                &MenuWidth, &MenuHeight,
                &LineWidth
            );

            DrawText (
                Screen->Title,
                FALSE,
                (StrLen (Screen->Title) + 2) * CharWidth,
                TitlePosX,
                EntriesPosY += TextLineHeight()
            );

            if (Screen->TitleImage) {
                BltImageAlpha (
                    Screen->TitleImage,
                    EntriesPosX + TITLEICON_SPACING,
                    EntriesPosY + (TextLineHeight() * 2),
                    BackgroundPixel
                );
                EntriesPosX += (
                    Screen->TitleImage->Width + TITLEICON_SPACING * 2
                );
            }

            EntriesPosY += (TextLineHeight() * 2);

            if (Screen->InfoLineCount > 0) {
                for (i = 0; i < Screen->InfoLineCount; i++) {
                    DrawText (
                        Screen->InfoLines[i],
                        FALSE, LineWidth,
                        EntriesPosX, EntriesPosY
                    );

                    EntriesPosY += TextLineHeight();
                }

                // Also Add a Blank Line
                EntriesPosY += TextLineHeight();
            }

            for (i = 0; i <= State->MaxIndex; i++) {
                VetStr = MyStriCmp (
                    Screen->Entries[i]->Title, GEN_TAG
                );

                DrawText (
                    (!VetStr) ? Screen->Entries[i]->Title : L"",
                    (!VetStr) ? (i == State->CurrentSelection) : FALSE,
                    LineWidth,
                    EntriesPosX,
                    EntriesPosY + i * TextLineHeight()
                );
            }

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
                if (Screen->Hint1 != NULL && StrLen (Screen->Hint1) > 0) {
                    DrawTextWithTransparency (
                        Screen->Hint1,
                        (ScreenW - egComputeTextWidth (Screen->Hint1)) / 2,
                        ScreenH - (egGetFontHeight() * 3)
                    );
                }

                if (Screen->Hint2 != NULL && StrLen (Screen->Hint2) > 0) {
                    DrawTextWithTransparency (
                        Screen->Hint2,
                        (ScreenW - egComputeTextWidth (Screen->Hint2)) / 2,
                        ScreenH - (egGetFontHeight() * 2)
                    );
                }
            }

        break;
        case MENU_FUNCTION_PAINT_SELECTION:
            // Redraw Selection Cursor
            DrawText (
                Screen->Entries[State->PreviousSelection]->Title,
                FALSE, LineWidth,
                EntriesPosX,
                EntriesPosY + State->PreviousSelection * TextLineHeight()
            );

            VetStr = MyStriCmp (
                Screen->Entries[State->CurrentSelection]->Title, GEN_TAG
            );
            if (VetStr) {
                if (State->CurrentSelection > State->PreviousSelection) {
                    if (State->CurrentSelection < State->MaxIndex) {
                        State->CurrentSelection += 1;
                    }
                }
                else {
                    if (State->CurrentSelection < State->PreviousSelection) {
                        if (State->CurrentSelection > 0) {
                            State->CurrentSelection -= 1;
                        }
                    }
                }
            }

            // DA-TAG: IMPORTANT ... Deliberate/Required
            VetStr = MyStriCmp (
                Screen->Entries[State->CurrentSelection]->Title, GEN_TAG
            );
            DrawText (
                (!VetStr) ? Screen->Entries[State->CurrentSelection]->Title : L"",
                (!VetStr) ? TRUE : FALSE,
                LineWidth,
                EntriesPosX,
                EntriesPosY + (TextLineHeight() * State->CurrentSelection)
            );

        break;
        case MENU_FUNCTION_PAINT_TIMEOUT:
            DrawText (
                ParamText,
                FALSE, LineWidth,
                EntriesPosX, TimeoutPosY
            );

        break;
    } // switch
} // VOID GraphicsMenuStyle()

UINTN ComputeRow0PosY (
    IN BOOLEAN ApplyOffset
) {
    UINTN Row0PosY;
    INTN  IconRowTweak;


    // Default 'IconRowTweak' to Zero
    // Keeps rows in central position
    IconRowTweak = 0;

    // Amend 'IconRowTweak' if 'ApplyOffset' is Active
    if (ApplyOffset) {
        if (GlobalConfig.IconRowMove != 0) {
            // Set positive value
            // Moves rows to lower third of screen
            IconRowTweak = (ScreenH / 4);

            if (GlobalConfig.IconRowMove > 0) {
                // Set Negative Value
                // Moves rows to upper third of screen
                IconRowTweak *= -1;

                // Finetune Position
                // Account for icon size ... only when in upper third
                IconRowTweak += (TileSizes[0] / 2);
            }
        }
    }

    // Set Base Row Position
    // Adds 'IconRowTweak' value (which may be zero)
    Row0PosY = ((ScreenH / 2) - (TileSizes[0] / 2)) + IconRowTweak;

    // Amend Row Position if 'ApplyOffset' is Active
    if (ApplyOffset) {
        // Amend Row Position
        // Adds 'icon_row_tune' value (which may be zero)
        Row0PosY += GlobalConfig.IconRowTune;
    }

    // Return Row Position
    return Row0PosY;
} // UINTN ComputeRow0PosY()

// Display Main Menu in Graphics Mode
VOID MainMenuStyle (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    IN UINTN              Function,
    IN CHAR16            *ParamText
) {
    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    INTN   i;
    UINTN  row1PosX;
    UINTN  row0PosXRunning;
    UINTN  row1PosXRunning;

    static UINTN  row0Loaders = 0;
    static UINTN *itemPosX    = 0;
    static UINTN  row0PosX    = 0;
    static UINTN  row0PosY    = 0;
    static UINTN  row1PosY    = 0;
    static UINTN  textPosY    = 0;


    State->ScrollMode = SCROLL_MODE_ICONS;
    switch (Function) {
        case MENU_FUNCTION_INIT:
            InitScroll (
                State,
                Screen->EntryCount,
                GlobalConfig.MaxTags
            );

            GetStateInfo (Screen, State);
            row0Loaders  =  IconRowItems;
            row0PosX     =   IconRowPosX;
            row0PosY     =   IconRowPosY;
            row1PosX     =   ToolRowPosX;
            row1PosY     =   ToolRowPosY;

            textPosY = (
                ToolRowCount == 0
            ) ? row1PosY : row1PosY + TileSizes[1] + TILE_YSPACING;

            itemPosX = AllocatePool (
                Screen->EntryCount * sizeof (UINTN)
            );
            if (itemPosX == NULL) {
                // Early Return on Resource Exhaustion
                return;
            }

            row0PosXRunning = row0PosX;
            row1PosXRunning = row1PosX;

            for (i = 0; i <= State->MaxIndex; i++) {
                if (Screen->Entries[i]->Row == 0) {
                    itemPosX[i] = row0PosXRunning;
                    row0PosXRunning += TileSizes[0] + TILE_XSPACING;
                }
                else {
                    itemPosX[i] = row1PosXRunning;
                    row1PosXRunning += TileSizes[1] + TILE_XSPACING;
                }
            } // for

            // Initial Painting
            InitSelection();

            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_SET;
            #endif
            SwitchToGraphicsAndClear (TRUE);
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_OFF;
            #endif

        break;
        case MENU_FUNCTION_CLEANUP:
            MY_FREE_POOL(itemPosX);

        break;
        case MENU_FUNCTION_PAINT_ALL:
            PaintAll (
                Screen, State, itemPosX,
                row0PosY, row1PosY, textPosY
            );

            // Moves starting Y position to
            //   midpoint of surrounding row.
            // Adjusted by half of icon height
            //  in PaintIcon() to center it.
            PaintArrows (
                State, row0PosX - TILE_XSPACING,
                row0PosY + (TileSizes[0] / 2), row0Loaders
            );

        break;
        case MENU_FUNCTION_PAINT_SELECTION:
            PaintSelection (
                Screen, State, itemPosX,
                row0PosY, row1PosY, textPosY
            );

        break;
        case MENU_FUNCTION_PAINT_TIMEOUT:
            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL)) {
               DrawTextWithTransparency (
                   L"", 0, textPosY + TextLineHeight()
               );

               DrawTextWithTransparency (
                   ParamText,
                   (ScreenW - egComputeTextWidth (ParamText)) >> 1,
                   textPosY + TextLineHeight()
               );
            }

        break;
    } // switch
} // VOID MainMenuStyle()

// Determines the index of the main menu item at the given coordinates.
UINTN FindMainMenuItem (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    IN UINTN              PosX,
    IN UINTN              PosY
) {
    UINTN  i;
    UINTN  itemRow;
    UINTN  row0PosX;
    UINTN  row0PosY;
    UINTN  row1PosX;
    UINTN  row1PosY;
    UINTN *itemPosX;
    UINTN  ItemIndex;
    UINTN  row0PosXRunning;
    UINTN  row1PosXRunning;


    GetStateInfo (Screen, State);
    row0PosX     =   IconRowPosX;
    row0PosY     =   IconRowPosY;
    row1PosX     =   ToolRowPosX;
    row1PosY     =   ToolRowPosY;

    if (PosY >= row0PosY && PosY <= row0PosY + TileSizes[0]) {
        if (PosX <= row0PosX) {
            // Early Return
            return POINTER_LEFT_ARROW;
        }

        if (PosX >= (ScreenW - row0PosX)) {
            // Early Return
            return POINTER_RIGHT_ARROW;
        }

        itemRow = 0;
    }
    else if (PosY >= row1PosY && PosY <= row1PosY + TileSizes[1]) {
        itemRow = 1;
    }
    else {
        // Early Return ... Y coordinate outside either row
        return POINTER_NO_ITEM;
    }

    ItemIndex = POINTER_NO_ITEM;

    itemPosX = AllocatePool (
        Screen->EntryCount * sizeof (UINTN)
    );
    if (itemPosX == NULL) {
        // Resource Exhaustion ... Early Exit
        return POINTER_NO_ITEM;
    }

    row0PosXRunning = row0PosX;
    row1PosXRunning = row1PosX;

    for (i = 0; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 0) {
            itemPosX[i] = row0PosXRunning;
            row0PosXRunning += TileSizes[0] + TILE_XSPACING;
        }
        else {
            itemPosX[i] = row1PosXRunning;
            row1PosXRunning += TileSizes[1] + TILE_XSPACING;
        }
    } // for

    for (i = State->FirstVisible; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 0 && itemRow == 0) {
            if (i <= State->LastVisible) {
                if (PosX >= itemPosX[i - State->FirstVisible] &&
                    PosX <= itemPosX[i - State->FirstVisible] + TileSizes[0]
                ) {
                    ItemIndex = i;
                    break;
                }
            }
        }
        else {
            if (itemRow == 1                       &&
                PosX >= itemPosX[i]                &&
                PosX <= itemPosX[i] + TileSizes[1] &&
                Screen->Entries[i]->Row == 1
            ) {
                ItemIndex = i;
                break;
            }
        }
    } // for

    MY_FREE_POOL(itemPosX);

    return ItemIndex;
} // VOID FindMainMenuItem()

VOID GenerateWaitList (VOID) {
    UINTN      Index;
    UINTN      PointerCount;


    if (WaitList != NULL) {
        // Already generated
        return;
    }

    PointerCount   = pdCount();
    WaitListLength = PointerCount + 1;

    WaitList = AllocatePool (
        WaitListLength * sizeof (EFI_EVENT)
    );
    if (WaitList == NULL) {
        return;
    }

    WaitList[0] = gST->ConIn->WaitForKey;
    for (Index = 0; Index < PointerCount; Index++) {
        WaitList[Index + 1] = pdWaitEvent (Index);
    } // for
} // VOID GenerateWaitList()

UINTN WaitForInput (
    IN UINTN Timeout
) {
    EFI_STATUS  Status;
    UINTN       Index;
    UINTN       NumEvents;
    EFI_EVENT   TimerEvent;


    // Generate WaitList
    GenerateWaitList();

    if (WaitListLength == 0) {
        // No input events available
        return INPUT_TIMER_ERROR;
    }

    TimerEvent = NULL;

    if (Timeout == 0) {
        // No timeout requested
        // Waits indefinitely for input events.
        NumEvents = WaitListLength;
    }
    else {
        // Create timer event
        Status = REFIT_CALL_5_WRAPPER(
            gBS->CreateEvent, EVT_TIMER,
            0, NULL,
            NULL, &TimerEvent
        );
        if (EFI_ERROR(Status)) {
            // Pause for 0.1 Sec
            // DA-TAG: 100 Loops == 1 Sec
            RefitStall (10);

            return INPUT_TIMER_ERROR;
        }

        // Set timer for timeout
        REFIT_CALL_3_WRAPPER(
            gBS->SetTimer, TimerEvent,
            TimerRelative, Timeout * 10000
        );

        // Append timer event to the WaitList
        // Allowed for in 'GenerateWaitList()'
        WaitList[WaitListLength] = TimerEvent;
        NumEvents = WaitListLength + 1;
    } // if/else Timeout == 0

    // Event index default value
    Index = INPUT_TIMEOUT;

    // Wait for event signal
    Status = REFIT_CALL_3_WRAPPER(
        gBS->WaitForEvent, NumEvents,
        WaitList, &Index
    );

    // Close timer event if present
    if (TimerEvent != NULL) {
        REFIT_CALL_1_WRAPPER(
            gBS->CloseEvent, TimerEvent
        );
    }

    if (EFI_ERROR(Status)) {
        // Pause for 0.2 Sec
        // DA-TAG: 100 Loops == 1 Sec
        RefitStall (20);

        return INPUT_TIMER_ERROR;
    }

    if (Index == 0) {
        // Keyboard Event: Index == 0
        return INPUT_KEY;
    }

    if (Timeout == 0) {
        // No timer event set
        // Pointer Event: Index > 0
        if (Index > 0 &&
            Index < WaitListLength
        ) {
            return INPUT_POINTER;
        }
    }
    else {
        // NB: Timer event previously appended at "WaitListLength" index
        // Pointer Event: 1 <= Index <= WaitListLength-1
        if (Index > 0 &&
            Index < WaitListLength
        ) {
            return INPUT_POINTER;
        }
    }

    // Timer event timed out if we get here
    return INPUT_TIMEOUT;
} // UINTN WaitForInput()

VOID DisplaySimpleMessage (
    CHAR16 *Message,
    CHAR16 *Title OPTIONAL
) {
    #if REFIT_DEBUG > 0
    CHAR16            *MsgStr;
    INTN               MenuExit;
    BOOLEAN            CheckMute = FALSE;
    #endif

    INTN               DefaultEntry;
    BOOLEAN            RetVal;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_SCREEN *SimpleMessageMenu;
    REFIT_MENU_ENTRY  *ChosenOption;


    if (Message == NULL) {
        // Early Return
        return;
    }

    SimpleMessageMenu = AllocateZeroPool (
        sizeof (REFIT_MENU_SCREEN)
    );
    if (SimpleMessageMenu == NULL) {
        // Early Return
        return;
    }

    if (Title == NULL) {
        Title = L"Information";
    }

    SimpleMessageMenu->Title      = StrDuplicate (Title);
    SimpleMessageMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_ABOUT);
    SimpleMessageMenu->Hint1      = StrDuplicate (L"Press 'Enter' to Return to Main Menu");
    SimpleMessageMenu->Hint2      = StrDuplicate (L""                                    );

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"DisplaySimpleMessage:- '%s ::: %s'",
        Title, Message
    );
    LOG_MSG("INFO: %s", MsgStr);
    LOG_MSG("\n\n");
    ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
    MY_FREE_POOL(MsgStr);

    MY_MUTELOGGER_SET;
    #endif
    AddMenuInfoLine (
        SimpleMessageMenu,
        Message, FALSE
    );
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    RetVal = GetMenuEntryReturn (
        &SimpleMessageMenu
    );
    if (!RetVal) {
        FreeMenuScreen (
            &SimpleMessageMenu
        );

        // Early Return
        return;
    }

    DefaultEntry = 9999; // Use the Max Index
    Style = (
        AllowGraphicsMode
    ) ? GraphicsMenuStyle : TextMenuStyle;

    #if REFIT_DEBUG > 0
    // DA-TAG: Deliberate for Codacy
    MenuExit =
    #endif
    DrawMenuScreen (
        SimpleMessageMenu, Style,
        &DefaultEntry, &ChosenOption
    );

    #if REFIT_DEBUG > 0
    LogExit (MenuExit, __func__, Title);
    #endif

    FreeMenuScreen (&SimpleMessageMenu);
} // VOID DisplaySimpleMessage()

// Present a menu for the user to delete (un-hide) hidden tags.
VOID ManageHiddenTags (VOID) {
    INTN                 DefaultEntry;
    UINTN                MenuExit, i;
    CHAR16              *AllTags;
    CHAR16              *OneElement;
    CHAR16              *HiddenTags;
    CHAR16              *HiddenTools;
    CHAR16              *HiddenLegacy;
    CHAR16              *HiddenFirmware;
    BOOLEAN              SaveTags;
    BOOLEAN              SaveTools;
    BOOLEAN              SaveLegacy;
    BOOLEAN              SaveFirmware;
    MENU_STYLE_FUNC      Style;
    REFIT_MENU_ENTRY    *MenuEntryItem;
    REFIT_MENU_ENTRY    *ChosenOption;
    REFIT_MENU_SCREEN   *RestoreItemMenu;


#define UPDATE_HIDE_TAG(x)                   \
    do {                                     \
        if (x != NULL && x[0] != L'\0') {    \
            if (AllTags == NULL) {           \
                AllTags = StrDuplicate (x);  \
            }                                \
            else {                           \
                MergeUniqueStrings (         \
                    &AllTags, x, L','        \
                );                           \
            }                                \
        }                                    \
    } while (0)

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Prepare Menu Screen");
    ALT_LOG(1, LOG_LINE_NORMAL, L"Screen Title:- '%s'", LABEL_HIDDEN);
    #endif

    AllTags = NULL;

    HiddenTags = ReadHiddenTags (
        L"HiddenTags"
    );
    UPDATE_HIDE_TAG(HiddenTags);

    HiddenTools = ReadHiddenTags (
        L"HiddenTools"
    );
    UPDATE_HIDE_TAG(HiddenTools);

    HiddenLegacy = ReadHiddenTags (
        L"HiddenLegacy"
    );
    UPDATE_HIDE_TAG(HiddenLegacy);

    HiddenFirmware = ReadHiddenTags (
        L"HiddenFirmware"
    );
    UPDATE_HIDE_TAG(HiddenFirmware);

    if (AllTags == NULL  ||
        StrLen (AllTags) == 0
    ) {
        DisplaySimpleMessage (
            L"No Hidden Entries Found", NULL
        );

        // Early Return
        return;
    }

    RestoreItemMenu = AllocateZeroPool (
        sizeof (REFIT_MENU_SCREEN)
    );
    RestoreItemMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_HIDDEN);
    RestoreItemMenu->Title      = StrDuplicate (LABEL_HIDDEN           );
    RestoreItemMenu->Hint1      = StrDuplicate (SELECT_OPTION_HINT     );
    RestoreItemMenu->Hint2      = StrDuplicate (RETURN_MAIN_SCREEN_HINT);
    AddMenuInfoLine (RestoreItemMenu, L"Select an Entry and Press 'Enter' to Restore", FALSE);

    OneElement    = NULL;
    MenuEntryItem = NULL;
    i = 0;
    while (1) {
        OneElement = FindCommaDelimited (
            AllTags, i++
        );
        if (OneElement == NULL) break;

        MenuEntryItem  = AllocateZeroPool (
            sizeof (REFIT_MENU_ENTRY)
        );
        MenuEntryItem->Title = StrDuplicate (OneElement);
        MenuEntryItem->Tag   = TAG_RETURN;
        AddMenuEntry (
            RestoreItemMenu,
            MenuEntryItem
        );

        MY_FREE_POOL(OneElement);
    } // while {Infinite}

    do {
        if (!GetMenuEntryReturn (&RestoreItemMenu)) {
            break;
        }

        DefaultEntry = 9999; // Use the Max Index
        Style = (
            AllowGraphicsMode
        ) ? GraphicsMenuStyle : TextMenuStyle;
        MenuExit = DrawMenuScreen (
            RestoreItemMenu, Style,
            &DefaultEntry, &ChosenOption
        );

        #if REFIT_DEBUG > 0
        LogExit (MenuExit, __func__, ChosenOption->Title);
        #endif

        SaveTags = SaveTools = SaveLegacy = SaveFirmware = FALSE;

        if (MenuExit == MENU_EXIT_ENTER) {
            if (HiddenTags    ) SaveTags     |= DeleteItemFromCsvList (ChosenOption->Title, &HiddenTags    );
            if (HiddenTools   ) SaveTools    |= DeleteItemFromCsvList (ChosenOption->Title, &HiddenTools   );
            if (HiddenLegacy  ) SaveLegacy   |= DeleteItemFromCsvList (ChosenOption->Title, &HiddenLegacy  );
            if (HiddenFirmware) SaveFirmware |= DeleteItemFromCsvList (ChosenOption->Title, &HiddenFirmware);
        }

        if (SaveTags    ) SaveHiddenList (HiddenTags,     L"HiddenTags"    );
        if (SaveTools   ) SaveHiddenList (HiddenTools,    L"HiddenTools"   );
        if (SaveLegacy  ) SaveHiddenList (HiddenLegacy,   L"HiddenLegacy"  );
        if (SaveFirmware) SaveHiddenList (HiddenFirmware, L"HiddenFirmware");

        if (SaveTags || SaveTools || SaveLegacy || SaveFirmware) {
            if (SaveTools) {
                MY_FREE_POOL(gHiddenTools);
            }

            RescanAll (FALSE);
        }
    } while (0); // This 'loop' only runs once

    FreeMenuScreen (&RestoreItemMenu);

    MY_FREE_POOL(AllTags);
    MY_FREE_POOL(HiddenTags);
    MY_FREE_POOL(HiddenTools);
    MY_FREE_POOL(HiddenLegacy);
    MY_FREE_POOL(HiddenFirmware);
} // VOID ManageHiddenTags()

CHAR16 * ReadHiddenTags (
    CHAR16 *VarName
) {
    #if REFIT_DEBUG > 0
    CHAR16 *CheckErrMsg;
    #endif

    CHAR16     *Buffer;
    UINTN       Size;
    EFI_STATUS  Status;


    Buffer = NULL;
    Status = EfivarGetRaw (
        &RefindPlusGuid, VarName,
        (VOID **) &Buffer, &Size
    );
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        if (Status != EFI_NOT_FOUND) {
            #if REFIT_DEBUG > 0
            CheckErrMsg = PoolPrint (
                L"in ReadHiddenTags:- '%s'",
                VarName
            );
            CheckError (Status, CheckErrMsg);
            MY_FREE_POOL(CheckErrMsg);
            #endif
        }
        #endif

        return NULL;
    }

    if (Size == 0 ||
        Buffer == NULL ||
        StrLen (Buffer) == 0
    ) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"Invalid Buffer in ReadHiddenTags ... Clearing Buffer"
        );
        #endif

        EfivarSetRaw (
            &RefindPlusGuid, VarName,
            NULL, 0, TRUE
        );
        MY_FREE_POOL(Buffer);
    }

    return Buffer;
} // CHAR16 * ReadHiddenTags()

// Present a menu for the user to confirm Bluetooth Sync
UINTN AbortSyncTrust (VOID) {
    INTN               DefaultEntry;
    UINTN              MenuExit;
    BOOLEAN            RetVal;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenOption;
    REFIT_MENU_SCREEN *AbortSyncTrustMenu;


    AbortSyncTrustMenu = AllocateZeroPool (
        sizeof (REFIT_MENU_SCREEN)
    );
    if (AbortSyncTrustMenu == NULL) {
        // Resource Exhaustion ... Early Exit
        return SYNC_TRUST_HALT;
    }

    // Build the menu page
    AbortSyncTrustMenu->Title      = StrDuplicate (TRUSTED_BOOT_CONFIRM   );
    AbortSyncTrustMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_ABOUT );
    AbortSyncTrustMenu->Hint1      = StrDuplicate (SELECT_OPTION_HINT     );
    AbortSyncTrustMenu->Hint2      = StrDuplicate (RETURN_MAIN_SCREEN_HINT);

    AddMenuInfoLine (AbortSyncTrustMenu, L"The boot \"Chain of Trust\" may be servered on booting with 3rd-party tools.", FALSE);
    AddMenuInfoLine (AbortSyncTrustMenu, L"This typically affects units with T2/TPM chips after an SMC/Similar reset.",   FALSE);
    AddMenuInfoLine (AbortSyncTrustMenu, L"",                                                                             FALSE);
    AddMenuInfoLine (AbortSyncTrustMenu, L"When manifested, the machine will fail to boot and/or become unresponsive.",   FALSE);
    AddMenuInfoLine (AbortSyncTrustMenu, L"",                                                                             FALSE);
    AddMenuInfoLine (AbortSyncTrustMenu, L"RefindPlus will start a native reboot into your target to avoid the issue.",   FALSE);
    AddMenuInfoLine (AbortSyncTrustMenu, L"Would you prefer RefindPlus to load your selected target directly instead?",   FALSE);
    AddMenuInfoLine (AbortSyncTrustMenu, L"",                                                                             FALSE);

    RetVal = GetMenuEntryYesNo (
        &AbortSyncTrustMenu
    );
    if (!RetVal) {
        FreeMenuScreen (
            &AbortSyncTrustMenu
        );

        // Early Return
        return SYNC_TRUST_HALT;
    }

    DefaultEntry = 9999; // Use the Max Index
    Style = (
        AllowGraphicsMode
    ) ? GraphicsMenuStyle : TextMenuStyle;
    MenuExit = DrawMenuScreen (
        AbortSyncTrustMenu, Style,
        &DefaultEntry, &ChosenOption
    );

    #if REFIT_DEBUG > 0
    LogExit (MenuExit, __func__, ChosenOption->Title);
    #endif

    if (MenuExit == MENU_EXIT_ESCAPE) {
        RetVal = SYNC_TRUST_EXIT;
    }
    else if (
        MenuExit == MENU_EXIT_ENTER &&
        MyStriCmp (ChosenOption->Title, L"Yes")
    ) {
        RetVal = SYNC_TRUST_SKIP;
    }
    else {
        RetVal = SYNC_TRUST_BOOT;
    }

    FreeMenuScreen (&AbortSyncTrustMenu);

    return RetVal;
} // UINTN AbortSyncTrust()

// Present a menu for the user to confirm Bluetooth Sync
BOOLEAN ConfirmSyncNVram (VOID) {
    INTN               DefaultEntry;
    UINTN              MenuExit;
    BOOLEAN            RetVal;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenOption;
    REFIT_MENU_SCREEN *ConfirmSyncNVramMenu;


    ConfirmSyncNVramMenu = AllocateZeroPool (
        sizeof (REFIT_MENU_SCREEN)
    );
    if (ConfirmSyncNVramMenu == NULL) {
        // Resource Exhaustion ... Early Exit
        return FALSE;
    }

    // Build the menu page
    ConfirmSyncNVramMenu->Title      = StrDuplicate (L"Confirm nvRAM Sync");
    ConfirmSyncNVramMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_ABOUT   );
    ConfirmSyncNVramMenu->Hint1      = StrDuplicate (SELECT_OPTION_HINT       );
    ConfirmSyncNVramMenu->Hint2      = StrDuplicate (RETURN_MAIN_SCREEN_HINT  );

    AddMenuInfoLine (ConfirmSyncNVramMenu, L"Sync Misc nvRAM Entries?",  FALSE);
    AddMenuInfoLine (ConfirmSyncNVramMenu, L"",                          FALSE);

    RetVal = GetMenuEntryYesNo (
        &ConfirmSyncNVramMenu
    );
    if (!RetVal) {
        FreeMenuScreen (
            &ConfirmSyncNVramMenu
        );

        // Early Return
        return FALSE;
    }

    DefaultEntry = 9999; // Use the Max Index
    Style = (
        AllowGraphicsMode
    ) ? GraphicsMenuStyle : TextMenuStyle;
    MenuExit = DrawMenuScreen (
        ConfirmSyncNVramMenu, Style,
        &DefaultEntry, &ChosenOption
    );

    #if REFIT_DEBUG > 0
    LogExit (MenuExit, __func__, ChosenOption->Title);
    #endif

    if (MenuExit == MENU_EXIT_ENTER &&
        MyStriCmp (ChosenOption->Title, L"Yes")
    ) {
        RetVal =  TRUE;
    }
    else {
        RetVal = FALSE;
    }

    FreeMenuScreen (&ConfirmSyncNVramMenu);

    return RetVal;
} // BOOLEAN ConfirmSyncNVram()

// Present a menu for the user to confirm CSR rotatation
BOOLEAN ConfirmRotate (VOID) {
    UINT32             CurrentCsr;
    UINT32             TargetCsr;
    UINT32             TempCsr;
    CHAR16            *TmpStrA;
    CHAR16            *TmpStrB;
    INTN               DefaultEntry;
    UINTN              MenuExit;
    BOOLEAN            EmptySIP;
    BOOLEAN            RetVal;
    UINT32_LIST       *ListItem;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenOption;
    REFIT_MENU_SCREEN *ConfirmRotateMenu;


    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Prepare Menu Screen");
    ALT_LOG(1, LOG_LINE_NORMAL, L"Screen Title:- 'Confirm CSR Rotation'");
    #endif

    if (GlobalConfig.CsrValues == NULL) {
        // Early Exit
        return FALSE;
    }

    ConfirmRotateMenu = AllocateZeroPool (
        sizeof (REFIT_MENU_SCREEN)
    );
    if (ConfirmRotateMenu == NULL) {
        // Resource Exhaustion ... Early Exit
        return TRUE;
    }

    /* coverity[check_return: SUPPRESS] */
    GetCsrStatus (&CurrentCsr);
    RecordgCsrStatus (CurrentCsr, FALSE);
    TmpStrA = PoolPrint (L"From : %s", gCsrStatus);
    EmptySIP = (
        CurrentCsr == SIP_ENABLED_EX
    ) ? TRUE : FALSE;

    ListItem = GlobalConfig.CsrValues;
    if (EmptySIP) {
        // Store first config CsrValue when SIP is not set (for later use)
        TempCsr = GlobalConfig.CsrValues->Value;
    }
    else {
        while (
            ListItem        != NULL &&
            ListItem->Value != CurrentCsr
        ) {
            ListItem = ListItem->Next;
        } // while
    }

    TargetCsr = (
        ListItem       == NULL ||
        ListItem->Next == NULL
    ) ? GlobalConfig.CsrValues->Value : ListItem->Next->Value;

    // Set recorded Human Readable String to Target CSR
    RecordgCsrStatus (TargetCsr, FALSE);
    // Save recorded Human Readable String for display later
    TmpStrB = PoolPrint (L"To   : %s", gCsrStatus);
    // Revert recorded Human Readable String to Current CSR
    RecordgCsrStatus (CurrentCsr, FALSE);

    // Build the menu page
    ConfirmRotateMenu->Title      = StrDuplicate (L"Confirm CSR Rotation"    );
    ConfirmRotateMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_CSR_ROTATE);
    ConfirmRotateMenu->Hint1      = StrDuplicate (SELECT_OPTION_HINT         );
    ConfirmRotateMenu->Hint2      = StrDuplicate (RETURN_MAIN_SCREEN_HINT    );

    AddMenuInfoLine (ConfirmRotateMenu, TmpStrA,                        FALSE);
    AddMenuInfoLine (ConfirmRotateMenu, TmpStrB,                        FALSE);
    AddMenuInfoLine (ConfirmRotateMenu, L"",                            FALSE);

    MY_FREE_POOL(TmpStrA);
    MY_FREE_POOL(TmpStrB);

    RetVal = GetMenuEntryYesNo (
        &ConfirmRotateMenu
    );
    if (!RetVal) {
        FreeMenuScreen (&ConfirmRotateMenu);

        // Early Return
        return FALSE;
    }

    DefaultEntry = 9999; // Use the Max Index
    Style = (
        AllowGraphicsMode
    ) ? GraphicsMenuStyle : TextMenuStyle;
    MenuExit = DrawMenuScreen (
        ConfirmRotateMenu, Style,
        &DefaultEntry, &ChosenOption
    );

    #if REFIT_DEBUG > 0
    LogExit (MenuExit, __func__, ChosenOption->Title);
    #endif

    if (MenuExit != MENU_EXIT_ENTER ||
        !MyStriCmp (ChosenOption->Title, L"Yes")
    ) {
        RetVal = FALSE;
    }
    else {
        RetVal = TRUE;

        if (EmptySIP) {
            // Save first config CsrValue when SIP is not set
            // Allows rotation to the second value, usually "Disabled" setting
            EfivarSetRaw (
                &AppleBootGuid, L"csr-active-config",
                &TempCsr, sizeof (UINT32), TRUE
            );
        }
    }

    FreeMenuScreen (&ConfirmRotateMenu);

    return RetVal;
} // BOOLEAN ConfirmRotate()

UINTN RunMainMenu (
    REFIT_MENU_SCREEN  *Screen,
    CHAR16            **DefaultSelection,
    REFIT_MENU_ENTRY  **ChosenOption
) {
    #if REFIT_DEBUG > 0
    CHAR16             *MsgStr;
    UINTN               EntryPosition;

    static BOOLEAN      ShowLoaded = TRUE;
    #endif

    REFIT_MENU_ENTRY   *TempChosenOption;
    REFIT_MENU_ENTRY   *KeptChosenOption;
    MENU_STYLE_FUNC     MainStyle;
    MENU_STYLE_FUNC     Style;
    BOOLEAN             KeyStrokeFound;
    UINTN               MenuExit;
    INTN                DefaultEntryIndex;
    INTN                DefaultSubmenuIndex;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    TileSizes[0] = (GlobalConfig.IconSizes[ICON_SIZE_BIG]   * 9) / 8;
    TileSizes[1] = (GlobalConfig.IconSizes[ICON_SIZE_SMALL] * 4) / 3;

    BREAD_CRUMB(L"%a:  2", __func__);
    if (DefaultSelection == NULL || *DefaultSelection == NULL) {
        BREAD_CRUMB(L"%a:  2a 1", __func__);
        DefaultEntryIndex = -1;
    }
    else {
        BREAD_CRUMB(L"%a:  2b 1", __func__);
        DefaultEntryIndex = FindMenuShortcutEntry (
            Screen, *DefaultSelection
        );
    }

    #if REFIT_DEBUG > 0
    BREAD_CRUMB(L"%a:  3", __func__);
    if (ShowLoaded) {
        ShowLoaded = FALSE;

        BREAD_CRUMB(L"%a:  3a 1", __func__);
        MsgStr = StrDuplicate (
            L"C O M P L E T E   B O O T S T R A P   S E Q U E N C E"
        );
        ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
        LOG_MSG("%s", MsgStr);
        MY_FREE_POOL(MsgStr);

        BREAD_CRUMB(L"%a:  3a 2", __func__);
        if ( DefaultSelection == NULL ||
            *DefaultSelection == NULL
        ) {
            BREAD_CRUMB(L"%a:  3a 2a 1", __func__);
            MsgStr = StrDuplicate (
                L"Configured Default Loader:- 'NULL'"
            );
        }
        else {
            BREAD_CRUMB(L"%a:  3a 2b 1", __func__);
            MsgStr = PoolPrint (
                L"Configured Default Loader:- '%s'",
                *DefaultSelection
            );
        }

        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("\n");
        LOG_MSG("%s", MsgStr);
        MY_FREE_POOL(MsgStr);

        BREAD_CRUMB(L"%a:  3a 3", __func__);
        if (!GlobalConfig.DirectBoot) {
            BREAD_CRUMB(L"%a:  3a 3a 1", __func__);
            EntryPosition = (
                DefaultEntryIndex >= 0
            ) ? DefaultEntryIndex : 0;
            MsgStr = PoolPrint (
                L"Highlighted Screen Option:- '%s'",
                Screen->Entries[EntryPosition]->Title
            );
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            LOG_MSG("\n");
            LOG_MSG("%s", MsgStr);
            MY_FREE_POOL(MsgStr);
        }
        BREAD_CRUMB(L"%a:  3a 4", __func__);
        LOG_MSG("\n\n");
    }
    #endif

    // Remove buffered key strokes
    BREAD_CRUMB(L"%a:  4", __func__);
    KeyStrokeFound = ReadAllKeyStrokes();

    BREAD_CRUMB(L"%a:  5", __func__);
    // NB: Buffer is always reset on UEFI PC
    if (!KeyStrokeFound || !AppleFirmware) {
        BREAD_CRUMB(L"%a:  5a 1 - Clear Keystroke Buffer", __func__);
        REFIT_CALL_2_WRAPPER(
            gST->ConIn->Reset,
            gST->ConIn, FALSE
        );
        BREAD_CRUMB(L"%a:  5a 2", __func__);
    }

    BREAD_CRUMB(L"%a:  6", __func__);
    if (!AllowGraphicsMode) {
        BREAD_CRUMB(L"%a:  6a 1", __func__);
        Style     = TextMenuStyle;
        MainStyle = TextMenuStyle;
    }
    else {
        BREAD_CRUMB(L"%a:  6b 1", __func__);
        Style          = GraphicsMenuStyle;
        MainStyle      = MainMenuStyle;
        PointerEnabled = PointerActive = pdAvailable();
        DrawSelection  = !PointerEnabled;
    }

    // Generate WaitList if not already generated
    BREAD_CRUMB(L"%a:  7 - GenerateWaitList", __func__);
    GenerateWaitList();

    // Save time elaspsed since start
    BREAD_CRUMB(L"%a:  8 - GetCurrentMS", __func__);
    MainMenuLoad = GetCurrentMS();

    BREAD_CRUMB(L"%a:  9", __func__);
    do {
        LOG_SEP(L"X");
        BREAD_CRUMB(L"%a:  9a 1 - DO LOOP:- START", __func__);
        TempChosenOption = NULL;
        MenuExit = DrawMenuScreen (
            Screen, MainStyle,
            &DefaultEntryIndex, &TempChosenOption
        );

        #if REFIT_DEBUG > 0
        LogExit (MenuExit, __func__, TempChosenOption->Title);
        #endif

        BREAD_CRUMB(L"%a:  9a 2", __func__);
        Screen->TimeoutSeconds = 0;

        BREAD_CRUMB(L"%a:  9a 3", __func__);
        SubScreenBoot = FALSE;
        if (MenuExit == MENU_EXIT_DETAILS) {
            BREAD_CRUMB(L"%a:  9a 3a 1", __func__);
            if (TempChosenOption->SubScreen == NULL) {
                BREAD_CRUMB(L"%a:  9a 3a 1a 1 - No Subscreen ... Ignore Keypress", __func__);
                MenuExit = MENU_EXIT_ZERO;
            }
            else {
                BREAD_CRUMB(L"%a:  9a 3a 1b 1 - Show SubScreen", __func__);
                DefaultSubmenuIndex = 9999;
                KeptChosenOption = TempChosenOption;
                while (1) {
                    BREAD_CRUMB(L"%a:  9a 3a 1b 1a 1", __func__);
                    MenuExit = DrawMenuScreen (
                        TempChosenOption->SubScreen, Style,
                        &DefaultSubmenuIndex, &TempChosenOption
                    );

                    BREAD_CRUMB(L"%a:  9a 3a 1b 1a 2", __func__);
                    if (TempChosenOption->Tag == TAG_SPACER) {
                        BREAD_CRUMB(L"%a:  9a 3a 1b 1a 2a 1", __func__);
                        TempChosenOption = KeptChosenOption;
                        continue;
                    }

                    BREAD_CRUMB(L"%a:  9a 3a 1b 1a 3", __func__);
                    break;
                } // while {Infinite}

                BREAD_CRUMB(L"%a:  9a 3a 1b 2", __func__);
                #if REFIT_DEBUG > 0
                ALT_LOG(1, LOG_LINE_NORMAL,
                    L"Returned '%d' (%s) from Sub Screen Option in '%a' Call ... %s",
                    MenuExit, MenuExitInfo (MenuExit), __func__, TempChosenOption->Title
                );
                #endif

                BREAD_CRUMB(L"%a:  9a 3a 1b 3", __func__);
                if (MenuExit == MENU_EXIT_ESCAPE ||
                    TempChosenOption->Tag == TAG_RETURN
                ) {
                    BREAD_CRUMB(L"%a:  9a 3a 1b 3a 1", __func__);
                    MenuExit = MENU_EXIT_ZERO;
                }

                BREAD_CRUMB(L"%a:  9a 3a 1b 4", __func__);
                if (MenuExit == MENU_EXIT_DETAILS) {
                    BREAD_CRUMB(L"%a:  9a 3a 1b 4a 1", __func__);
                    if (!EditOptions ((LOADER_ENTRY *) TempChosenOption)) {
                        BREAD_CRUMB(L"%a:  9a 3a 1b 4a 1a 1", __func__);
                        MenuExit = MENU_EXIT_ZERO;
                    }
                    BREAD_CRUMB(L"%a:  9a 3a 1b 4a 2", __func__);
                }
                BREAD_CRUMB(L"%a:  9a 3a 1b 5", __func__);
            }

            BREAD_CRUMB(L"%a:  9a 3a 2", __func__);
            if (MenuExit == MENU_EXIT_ENTER) {
                BREAD_CRUMB(L"%a:  9a 3a 2a 1", __func__);
                SubScreenBoot = TRUE;
            }

            BREAD_CRUMB(L"%a:  9a 3a 3", __func__);
        } // if MenuExit == MENU_EXIT_DETAILS

        BREAD_CRUMB(L"%a:  9a 4", __func__);
        if (MenuExit == MENU_EXIT_HIDE) {
            BREAD_CRUMB(L"%a:  9a 4a 1", __func__);
            if (GlobalConfig.HiddenTags) {
                BREAD_CRUMB(L"%a:  9a 4a 1a 1", __func__);
                HideTag (TempChosenOption);
            }
            else {
                BREAD_CRUMB(L"%a:  9a 4a 1b 1", __func__);
                egDisplayMessage (
                    L"Enable 'hidden_tags' in 'showtools' config to hide tag",
                    &BGColorBase, CENTER, 3, L"PauseSeconds"
                );
            }

            BREAD_CRUMB(L"%a:  9a 4a 2", __func__);
            MenuExit = MENU_EXIT_ZERO;
        }

        BREAD_CRUMB(L"%a:  9a 5", __func__);
        if (MenuExit == MENU_EXIT_ZERO && GlobalConfig.EnableTouch) {
            // Break out of loop and reload page
            // Reload happens in 'main.c -> MainLoopRunning'

            BREAD_CRUMB(L"%a:  9a 5a 1", __func__);
            break;
        }

        BREAD_CRUMB(L"%a:  9a 6 - DO LOOP:- END", __func__);
        LOG_SEP(L"X");
    } while (MenuExit == MENU_EXIT_ZERO);

    // Ignore MenuExit if FlushFailedTag is set and not previously reset
    BREAD_CRUMB(L"%a:  10", __func__);
    if (FlushFailedTag && !FlushFailReset) {
        BREAD_CRUMB(L"%a:  10a 1", __func__);
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"FlushFailedTag is Set ... Ignore MenuExit");
        ALT_LOG(1, LOG_THREE_STAR_END, L"%s", MsgStr);
        LOG_MSG("INFO: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        FlushFailedTag = FALSE;
        FlushFailReset = TRUE;
        MenuExit = MENU_EXIT_ZERO;
        BREAD_CRUMB(L"%a:  10a 2", __func__);
    }

    BREAD_CRUMB(L"%a:  11", __func__);
    if (ChosenOption) {
        BREAD_CRUMB(L"%a:  11a 1", __func__);
        *ChosenOption = TempChosenOption;
    }

    BREAD_CRUMB(L"%a:  12", __func__);
    // No need to check '*DefaultSelection' below
    if (DefaultSelection != NULL) {
        BREAD_CRUMB(L"%a:  12a 1", __func__);
        MY_FREE_POOL(*DefaultSelection);
        *DefaultSelection = StrDuplicate (
            TempChosenOption->Title
        );
    }

    BREAD_CRUMB(L"%a:  13 - END:- return UINTN MenuExit = '%d'", __func__,
        MenuExit
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return MenuExit;
} // UINTN RunMainMenu()

VOID FreeMenuScreen (
    IN REFIT_MENU_SCREEN **Screen
) {
    #if REFIT_DEBUG > 1
    UINTN j;
    #endif

    UINTN i;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    if ( Screen == NULL ||
        *Screen == NULL
    ) {
        BREAD_CRUMB(L"%a:  1a 1 - END:- VOID", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    BREAD_CRUMB(L"%a:  2", __func__);
    MY_FREE_POOL((*Screen)->Title);

    BREAD_CRUMB(L"%a:  3", __func__);
    MY_FREE_IMAGE((*Screen)->TitleImage);

    BREAD_CRUMB(L"%a:  4", __func__);
    if ((*Screen)->InfoLines) {
        BREAD_CRUMB(L"%a:  4a 1 - Free InfoLines", __func__);
        LOG_SEP(L"X");
        #if REFIT_DEBUG > 1
        j = 0;
        #endif
        for (i = 0; i < (*Screen)->InfoLineCount; i++) {
            #if REFIT_DEBUG > 1
            j++;
            #endif
            BREAD_CRUMB(L"%a:  4a 1a 1 - FOR LOOP:- START ... InfoLine %d of %d", __func__,
                j, (*Screen)->InfoLineCount
            );
            MY_FREE_POOL((*Screen)->InfoLines[i]);
            BREAD_CRUMB(L"%a:  4a 1a 2 - FOR LOOP:- END", __func__);
        }
        LOG_SEP(L"X");
        BREAD_CRUMB(L"%a:  4a 2", __func__);
        (*Screen)->InfoLineCount = 0;

        BREAD_CRUMB(L"%a:  4a 3", __func__);
        MY_FREE_POOL((*Screen)->InfoLines);
    }

    BREAD_CRUMB(L"%a:  5", __func__);
    if ((*Screen)->Entries) {
        BREAD_CRUMB(L"%a:  5a 1 - Free Entries", __func__);
        #if REFIT_DEBUG > 1
        j = 0;
        #endif
        for (i = 0; i < (*Screen)->EntryCount; i++) {
            #if REFIT_DEBUG > 1
            j++;
            #endif
            LOG_SEP(L"X");
            BREAD_CRUMB(L"%a:  5a 1a 1 - FOR LOOP:- START ... Entry %d of %d", __func__,
                j, (*Screen)->EntryCount
            );
            FreeMenuEntry (&(*Screen)->Entries[i]);
            BREAD_CRUMB(L"%a:  5a 1a 2 - FOR LOOP:- END", __func__);
            LOG_SEP(L"X");
        }
        BREAD_CRUMB(L"%a:  5a 2", __func__);
        (*Screen)->EntryCount = 0;

        BREAD_CRUMB(L"%a:  5a 3", __func__);
        MY_FREE_POOL((*Screen)->Entries);
    }

    BREAD_CRUMB(L"%a:  6", __func__);
    MY_FREE_POOL((*Screen)->TimeoutText);
    MY_FREE_POOL((*Screen)->Hint1);
    MY_FREE_POOL((*Screen)->Hint2);
    MY_FREE_POOL(*Screen);

    BREAD_CRUMB(L"%a:  7 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID FreeMenuScreen()

VOID FreeMenuEntry (
    REFIT_MENU_ENTRY **Entry
) {
    #if REFIT_DEBUG > 1
    CHAR16            *TagType;
    #endif

    typedef enum {
        EntryTypeRefitMenuEntry,
        EntryTypeLoaderEntry,
        EntryTypeLegacyEntry,
    } ENTRY_TYPE;
    ENTRY_TYPE       EntryType;


    if (*Entry == NULL) {
        // Early Return
        return;
    }

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    BREAD_CRUMB(L"%a:  2", __func__);
    switch ((*Entry)->Tag) {
        case TAG_TOOL:            EntryType = EntryTypeLoaderEntry   ;  break;
        case TAG_LOADER:          EntryType = EntryTypeLoaderEntry   ;  break;
        case TAG_LEGACY:          EntryType = EntryTypeLegacyEntry   ;  break;
        case TAG_LEGACY_UEFI:     EntryType = EntryTypeLegacyEntry   ;  break;
        case TAG_RESET_NVRAM:     EntryType = EntryTypeLoaderEntry   ;  break;
        case TAG_FIRMWARE_LOADER: EntryType = EntryTypeLoaderEntry   ;  break;
        default:                  EntryType = EntryTypeRefitMenuEntry;  break;
    }

    #if REFIT_DEBUG > 1
    switch ((*Entry)->Tag) {
        case TAG_TOOL:              TagType = L"TAG_TOOL"            ;  break;
        case TAG_LOADER:            TagType = L"TAG_LOADER"          ;  break;
        case TAG_LEGACY:            TagType = L"TAG_LEGACY"          ;  break;
        case TAG_LEGACY_UEFI:       TagType = L"TAG_LEGACY_UEFI"     ;  break;
        case TAG_RESET_NVRAM:       TagType = L"TAG_RESET_NVRAM"     ;  break;
        case TAG_FIRMWARE_LOADER:   TagType = L"TAG_FIRMWARE_LOADER" ;  break;
        default:                    TagType = L"DEFAULT"             ;  break;
    }
    #endif

    BREAD_CRUMB(L"%a:  3", __func__);
    if (EntryType == EntryTypeLoaderEntry) {
        BREAD_CRUMB(L"%a:  3a 1 - EntryType = EntryTypeLoaderEntry ... TagType = '%s'", __func__, TagType);
        FreeLoaderEntry (
            (LOADER_ENTRY **) Entry
        );
    }
    else if (EntryType == EntryTypeLegacyEntry) {
        BREAD_CRUMB(L"%a:  3b 1 - EntryType = EntryTypeLegacyEntry ... TagType = '%s'", __func__, TagType);
        FreeLegacyEntry (
            (LEGACY_ENTRY **) Entry
        );
    }
    else {
        BREAD_CRUMB(L"%a:  3c 1 - EntryType = EntryTypeRefitMenuEntry ... TagType = '%s'", __func__, TagType);
        MY_FREE_POOL((*Entry)->Title);
        MY_FREE_IMAGE((*Entry)->Image);
        MY_FREE_IMAGE((*Entry)->BadgeImage);

        BREAD_CRUMB(L"%a:  3c 2", __func__);
        FreeMenuScreen (
            &(*Entry)->SubScreen
        );
    }

    BREAD_CRUMB(L"%a:  4", __func__);
    MY_FREE_POOL(*Entry);

    BREAD_CRUMB(L"%a:  5 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID FreeMenuEntry()

BDS_COMMON_OPTION * CopyBdsOption (
    BDS_COMMON_OPTION *BdsOption
) {
    BDS_COMMON_OPTION *NewBdsOption;


    if (BdsOption == NULL) {
        // Early Return
        return NULL;
    }

    NewBdsOption = AllocateCopyPool (
        sizeof (*BdsOption), BdsOption
    );
    if (NewBdsOption == NULL) {
        // Early Return
        return NULL;
    }

    if (BdsOption->DevicePath) {
        NewBdsOption->DevicePath = AllocateCopyPool (
            GetDevicePathSize (BdsOption->DevicePath),
            BdsOption->DevicePath
        );
    }

    if (BdsOption->OptionName) {
        NewBdsOption->OptionName = AllocateCopyPool (
            StrSize (BdsOption->OptionName),
            BdsOption->OptionName
        );
    }

    if (BdsOption->Description) {
        NewBdsOption->Description = AllocateCopyPool (
            StrSize (BdsOption->Description),
            BdsOption->Description
        );
    }

    if (BdsOption->LoadOptions) {
        NewBdsOption->LoadOptions = AllocateCopyPool (
            BdsOption->LoadOptionsSize,
            BdsOption->LoadOptions
        );
    }

    if (BdsOption->StatusString) {
        NewBdsOption->StatusString = AllocateCopyPool (
            StrSize (BdsOption->StatusString),
            BdsOption->StatusString
        );
    }

    return NewBdsOption;
} // BDS_COMMON_OPTION * CopyBdsOption()

VOID FreeBdsOption (
    BDS_COMMON_OPTION **BdsOption
) {
    if (BdsOption == NULL || *BdsOption == NULL) {
        // Early Return
        return;
    }

    MY_FREE_POOL((*BdsOption)->DevicePath);
    MY_FREE_POOL((*BdsOption)->OptionName);
    MY_FREE_POOL((*BdsOption)->Description);
    MY_FREE_POOL((*BdsOption)->LoadOptions);
    MY_FREE_POOL((*BdsOption)->StatusString);
    MY_FREE_POOL(*BdsOption);
} // VOID FreeBdsOption()

BOOLEAN GetMenuEntryReturn (
    IN OUT REFIT_MENU_SCREEN **Screen
) {
    REFIT_MENU_ENTRY *MenuEntryReturn;


    if (Screen == NULL || *Screen == NULL) {
        // Early Return
        return FALSE;
    }

    MenuEntryReturn = AllocateZeroPool (
        sizeof (REFIT_MENU_ENTRY)
    );
    if (MenuEntryReturn == NULL) {
        // Early Return
        return FALSE;
    }

    MenuEntryReturn->Title = StrDuplicate (
        L"Return to Main Menu"
    );
    MenuEntryReturn->Tag = TAG_RETURN;
    AddMenuEntry (*Screen, MenuEntryReturn);

    return TRUE;
} // BOOLEAN GetMenuEntryReturn()

BOOLEAN GetMenuEntryYesNo (
    IN OUT REFIT_MENU_SCREEN **Screen
) {
    REFIT_MENU_ENTRY *MenuEntryYes;
    REFIT_MENU_ENTRY *MenuEntryNo;


    if (Screen == NULL || *Screen == NULL) {
        // Early Return
        return FALSE;
    }

    MenuEntryYes = AllocateZeroPool (
        sizeof (REFIT_MENU_ENTRY)
    );
    if (MenuEntryYes == NULL) {
        // Early Return
        return FALSE;
    }

    MenuEntryYes->Title = StrDuplicate (L"Yes");
    MenuEntryYes->Tag   = TAG_YES;
    AddMenuEntry (*Screen, MenuEntryYes);

    MenuEntryNo = AllocateZeroPool (
        sizeof (REFIT_MENU_ENTRY)
    );
    if (MenuEntryNo == NULL) {
        FreeMenuEntry (
            (REFIT_MENU_ENTRY **) MenuEntryYes
        );

        // Early Return
        return FALSE;
    }

    MenuEntryNo->Title = StrDuplicate (L"No");
    MenuEntryNo->Tag   = TAG_NO;
    AddMenuEntry (*Screen, MenuEntryNo);

    return TRUE;
} // BOOLEAN GetMenuEntryYesNo()
