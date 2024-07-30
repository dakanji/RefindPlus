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
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2024 Dayo Akanji (sf.net/u/dakanji/profile)
 * Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "screenmgt.h"
#include "lib.h"
#include "menu.h"
#include "config.h"
#include "libeg.h"
#include "libegint.h"
#include "line_edit.h"
#include "mystrings.h"
#include "icns.h"
#include "scan.h"
#include "apple.h"
#include "../include/version.h"
#include "../include/refit_call_wrapper.h"

#include "../include/egemb_back_selected_small.h"
#include "../include/egemb_back_selected_big.h"
#include "../include/egemb_arrow_left.h"
#include "../include/egemb_arrow_right.h"

// Other menu definitions

#define MENU_FUNCTION_INIT            (0)
#define MENU_FUNCTION_CLEANUP         (1)
#define MENU_FUNCTION_PAINT_ALL       (2)
#define MENU_FUNCTION_PAINT_SELECTION (3)
#define MENU_FUNCTION_PAINT_TIMEOUT   (4)
#define MENU_FUNCTION_PAINT_HINTS     (5)

static CHAR16 ArrowUp[2]   = {ARROW_UP, 0};
static CHAR16 ArrowDown[2] = {ARROW_DOWN, 0};
static UINTN  TileSizes[2] = {144, 64};

// Text and icon spacing constants.
#define TEXT_YMARGIN                  (2)
#define TITLEICON_SPACING            (16)

#define TILE_XSPACING                 (8)
#define TILE_YSPACING                (16)

// Alignment values for PaintIcon()
#define ALIGN_RIGHT 1
#define ALIGN_LEFT  0

EG_IMAGE *SelectionImages[2] = {NULL, NULL};

EFI_EVENT *WaitList          = NULL;
UINT64     MainMenuLoad      = 0;
UINTN      WaitListLength    = 0;

// Pointer variables
BOOLEAN PointerEnabled       = FALSE;
BOOLEAN PointerActive        = FALSE;
BOOLEAN DrawSelection        =  TRUE;

BOOLEAN SubScreenBoot        = FALSE;

REFIT_MENU_ENTRY MenuEntryNo = {
    L"No",
    TAG_RETURN,
    1, 0, 0,
    NULL, NULL, NULL
};
REFIT_MENU_ENTRY MenuEntryYes = {
    L"Yes",
    TAG_RETURN,
    1, 0, 0,
    NULL, NULL, NULL
};

extern UINT64              GetCurrentMS (VOID);
extern CHAR16             *VendorInfo;
extern EG_PIXEL            MenuBackgroundPixel;
extern BOOLEAN             FoundExternalDisk;
extern BOOLEAN             FlushFailedTag;
extern BOOLEAN             FlushFailReset;
extern BOOLEAN             ClearedBuffer;
extern BOOLEAN             BlockRescan;
extern BOOLEAN             OneMainLoop;
extern EFI_GUID            RefindPlusGuid;


#if REFIT_DEBUG > 0
VOID LogExit (
    IN  UINTN       MenuExit,
    IN  const char  FunctionName[],
    IN  CHAR16     *ChosenOptionTitle
) {
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) in '%a' Function from the \"%s\" Option in Menu Screen",
        MenuExit, MenuExitInfo (MenuExit), FunctionName, ChosenOptionTitle
    );
}
#endif

static
VOID InitSelection (VOID) {
    EG_IMAGE  *TempBigImage;
    EG_IMAGE  *TempSmallImage;
    UINTN      MaxAllowedImageSize;

    #if REFIT_DEBUG > 0
    CHAR16    *MsgStr;
    #endif


    if (!AllowGraphicsMode         ||
        SelectionImages[0] != NULL ||
        SelectionImages[1] != NULL
    ) {
        // Early Return ... Already run once or in text mode
        return;
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    ALT_LOG(1, LOG_LINE_NORMAL, L"Handle Default Selection Image:- 'Small'");
    #endif

    // Load small selection image
    MaxAllowedImageSize = 256;
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
                TempSmallImage->Width  > MaxAllowedImageSize ||
                TempSmallImage->Height > MaxAllowedImageSize
            )
        ) {
            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (
                L"Dropped Custom Small Selection Image ... %d x %d Exceeds %d x %d",
                TempSmallImage->Height, TempSmallImage->Width,
                MaxAllowedImageSize, MaxAllowedImageSize
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
    ALT_LOG(1, LOG_LINE_NORMAL, L"Handle Default Selection Image:- 'Big'");
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
        if (TempBigImage != NULL &&
            (
                TempBigImage->Width  > (MaxAllowedImageSize * 2) ||
                TempBigImage->Height > (MaxAllowedImageSize * 2)
            )
        ) {
            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (
                L"Dropped Custom Big Selection Image ... %d x %d Exceeds %d x %d",
                TempBigImage->Height, TempBigImage->Width,
                MaxAllowedImageSize, MaxAllowedImageSize
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
}

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
            // should not happen, but just in case.
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
            if (State->ScrollMode == SCROLL_MODE_ICONS) {
                if (State->CurrentSelection >= State->InitialRow1) {
                    if (State->MaxIndex > State->InitialRow1) {
                        // Avoid division by 0!
                        State->CurrentSelection = State->FirstVisible
                            + (State->LastVisible      - State->FirstVisible)
                            * (State->CurrentSelection - State->InitialRow1)
                            / (State->MaxIndex         - State->InitialRow1);
                    }
                    else {
                        State->CurrentSelection = State->FirstVisible;
                    }
                }
            }
            else {
                if (State->CurrentSelection > 0) {
                    State->CurrentSelection--;
                }
            }

        break;
        case SCROLL_LINE_DOWN:
            if (State->ScrollMode == SCROLL_MODE_ICONS) {
                if (State->CurrentSelection <= State->FinalRow0) {
                    if (State->LastVisible > State->FirstVisible) {
                        // Avoid division by 0!
                        State->CurrentSelection = State->InitialRow1 +
                            (State->MaxIndex         - State->InitialRow1) *
                            (State->CurrentSelection - State->FirstVisible) /
                            (State->LastVisible      - State->FirstVisible);
                    }
                    else {
                        State->CurrentSelection = State->InitialRow1;
                    }
                }
            }
            else {
                if (State->CurrentSelection < State->MaxIndex) {
                    State->CurrentSelection++;
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


//
// menu helper functions
//

// Returns a constant ... do not free
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
        L"Append Menu Entry to %s %s %s",
        Screen->Title,
        (MyStriCmp (Screen->Title, MAIN_MENU_NAME)) ? L"-" : L" - ",
        Entry->Title,
        SetVolType (NULL, Entry->Title, 0)
    );
    // DA-TAG: Doubled Delibrately in SetVolType
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
    while ((Shortcut = FindCommaDelimited (Defaults, j++)) != NULL) {
        if (StrLen (Shortcut) > 1) {
            for (i = 0; i < Screen->EntryCount; i++) {
                if (MyStriCmp (Shortcut, Screen->Entries[i]->Title)) {
                    FoundMatch = TRUE;
                    break;
                }
            } // for

            if (!FoundMatch) {
                for (i = 0; i < Screen->EntryCount; i++) {
                    if (StriSubCmp (Shortcut, Screen->Entries[i]->Title)) {
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
                for (i = 0; i < Screen->EntryCount; i++) {
                    if (Screen->Entries[i]->ShortcutDigit  == Shortcut[0] ||
                        Screen->Entries[i]->ShortcutLetter == Shortcut[0]
                    ) {
                        FoundMatch = TRUE;
                        break;
                    }
                } // for
            }
        } // if/else StrLen (Shortcut) > 1

        MY_FREE_POOL(Shortcut);

        if (FoundMatch) {
            return i;
        }
    } // while

    return -1;
} // INTN FindMenuShortcutEntry()

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
        else if (
            State->InitialRow1 > i    &&
            Screen->Entries[i]->Row == 1
        ) {
            State->InitialRow1 = i;
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
    UINTN  retval;
    UINTN  ColourIndex;
    UINT64 TimeWait;
    UINT64 BaseTimeWait;

    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;
    CHAR16 *LoopChange;

    BOOLEAN CheckMute = FALSE;

    MsgStr = L"Activity Wait Threshold Exceeded";
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    ALT_LOG(1, LOG_LINE_NORMAL,  L"%s", MsgStr);
    LOG_MSG("INFO: %s", MsgStr);

    MsgStr = L"Start Screensaver";
    ALT_LOG(1, LOG_THREE_STAR_MID,  L"%s", MsgStr);
    LOG_MSG("%s  - %s", OffsetNext, MsgStr);
    LOG_MSG("\n");
    #endif

    EG_PIXEL OUR_COLOUR;
    EG_PIXEL COLOUR_01 = {   0,  51,  51,  0 };
    EG_PIXEL COLOUR_02 = {   0, 102, 102,  0 };
    EG_PIXEL COLOUR_03 = {   0, 153, 153,  0 };
    EG_PIXEL COLOUR_04 = {   0, 204, 204,  0 };
    EG_PIXEL COLOUR_05 = {   0, 255, 255,  0 };
    EG_PIXEL COLOUR_06 = {  51,   0, 204,  0 };
    EG_PIXEL COLOUR_07 = {  51,  51, 153,  0 };
    EG_PIXEL COLOUR_08 = {  51, 102, 102,  0 };
    EG_PIXEL COLOUR_09 = {  51, 153,  51,  0 };
    EG_PIXEL COLOUR_10 = {  51, 204,   0,  0 };
    EG_PIXEL COLOUR_11 = {  51, 255,  51,  0 };
    EG_PIXEL COLOUR_12 = { 102,   0, 102,  0 };
    EG_PIXEL COLOUR_13 = { 102,  51, 153,  0 };
    EG_PIXEL COLOUR_14 = { 102, 102, 204,  0 };
    EG_PIXEL COLOUR_15 = { 102, 153, 255,  0 };
    EG_PIXEL COLOUR_16 = { 102, 204, 204,  0 };
    EG_PIXEL COLOUR_17 = { 102, 255, 153,  0 };
    EG_PIXEL COLOUR_18 = { 153,   0, 102,  0 };
    EG_PIXEL COLOUR_19 = { 153,  51,  51,  0 };
    EG_PIXEL COLOUR_20 = { 153, 102,   0,  0 };
    EG_PIXEL COLOUR_21 = { 153, 153,  51,  0 };
    EG_PIXEL COLOUR_22 = { 153, 204, 102,  0 };
    EG_PIXEL COLOUR_23 = { 153, 255, 153,  0 };
    EG_PIXEL COLOUR_24 = { 204,   0, 204,  0 };
    EG_PIXEL COLOUR_25 = { 204,  51, 255,  0 };
    EG_PIXEL COLOUR_26 = { 204, 102, 204,  0 };
    EG_PIXEL COLOUR_27 = { 204, 153, 153,  0 };
    EG_PIXEL COLOUR_28 = { 204, 204, 102,  0 };
    EG_PIXEL COLOUR_29 = { 204, 255,  51,  0 };
    EG_PIXEL COLOUR_30 = { 255,   0,   0,  0 };

    // Start with COLOUR_01 ... 0 will be incremented to 1
    ColourIndex = 0;

    // Start with BaseTimeWait
    BaseTimeWait = 3750;
    TimeWait = BaseTimeWait;
    for (;;) {
        ColourIndex = ColourIndex + 1;

        if (ColourIndex > 30) {
            ColourIndex = 1;

            TimeWait = TimeWait * 2;
            if (TimeWait > 120000) {
                // Reset TimeWait if greater than 2 minutes
                TimeWait = BaseTimeWait;

                #if REFIT_DEBUG > 0
                LoopChange = L"Reset";
                #endif
            }
            else {
                #if REFIT_DEBUG > 0
                LoopChange = L"Extend";
                #endif
            }

            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"%s Screensaver Timeout Loop:- %d",
                LoopChange, TimeWait
            );
            #endif
        }

        switch (ColourIndex) {
            case  1: OUR_COLOUR = COLOUR_01; break;
            case  2: OUR_COLOUR = COLOUR_02; break;
            case  3: OUR_COLOUR = COLOUR_03; break;
            case  4: OUR_COLOUR = COLOUR_04; break;
            case  5: OUR_COLOUR = COLOUR_05; break;
            case  6: OUR_COLOUR = COLOUR_06; break;
            case  7: OUR_COLOUR = COLOUR_07; break;
            case  8: OUR_COLOUR = COLOUR_08; break;
            case  9: OUR_COLOUR = COLOUR_09; break;
            case 10: OUR_COLOUR = COLOUR_10; break;
            case 11: OUR_COLOUR = COLOUR_11; break;
            case 12: OUR_COLOUR = COLOUR_12; break;
            case 13: OUR_COLOUR = COLOUR_13; break;
            case 14: OUR_COLOUR = COLOUR_14; break;
            case 15: OUR_COLOUR = COLOUR_15; break;
            case 16: OUR_COLOUR = COLOUR_16; break;
            case 17: OUR_COLOUR = COLOUR_17; break;
            case 18: OUR_COLOUR = COLOUR_18; break;
            case 19: OUR_COLOUR = COLOUR_19; break;
            case 20: OUR_COLOUR = COLOUR_20; break;
            case 21: OUR_COLOUR = COLOUR_21; break;
            case 22: OUR_COLOUR = COLOUR_22; break;
            case 23: OUR_COLOUR = COLOUR_23; break;
            case 24: OUR_COLOUR = COLOUR_24; break;
            case 25: OUR_COLOUR = COLOUR_25; break;
            case 26: OUR_COLOUR = COLOUR_26; break;
            case 27: OUR_COLOUR = COLOUR_27; break;
            case 28: OUR_COLOUR = COLOUR_28; break;
            case 29: OUR_COLOUR = COLOUR_29; break;
            default: OUR_COLOUR = COLOUR_30; break;
        }

        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif
        egClearScreen (&OUR_COLOUR);
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif

        retval = WaitForInput (TimeWait);
        if (retval == INPUT_KEY      ||
            retval == INPUT_TIMER_ERROR
        ) {
            break;
        }
    } // for

    #if REFIT_DEBUG > 0
    MsgStr = L"Detected Keypress ... Halt Screensaver";
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
    CHAR16 *retval;

    switch (ScanCode) {
        case SCAN_END:       retval = L"SCROLL_LAST";   break;
        case SCAN_HOME:      retval = L"SCROLL_FIRST";  break;
        case SCAN_PAGE_UP:   retval = L"PAGE_UP";       break;
        case SCAN_PAGE_DOWN: retval = L"PAGE_DOWN";     break;
        case SCAN_UP:        retval = L"ARROW_UP";      break;
        case SCAN_LEFT:      retval = L"ARROW_LEFT";    break;
        case SCAN_DOWN:      retval = L"ARROW_DOWN";    break;
        case SCAN_RIGHT:     retval = L"ARROW_RIGHT";   break;
        case SCAN_ESC:       retval = L"ESC-Rescan";    break;
        case SCAN_DELETE:    retval = L"DEL-Hide";      break;
        case SCAN_INSERT:    retval = L"INS-Details";   break;
        case SCAN_F2:        retval = L"F2-Details";    break;
        case SCAN_F10:       retval = L"F10-ScrnSht";   break; // 'ScrnSht' limits length
        case 0x0016:         retval = L"F12-Eject";     break;
        default:             retval = L"KEY_UNKNOWN";   break;
    } // switch

    return retval;
} // static CHAR16 * GetScanCodeText()
#endif

UINTN DrawMenuScreen (
    IN     REFIT_MENU_SCREEN   *Screen,
    IN     MENU_STYLE_FUNC      StyleFunc,
    IN OUT INTN                *DefaultEntryIndex,
    OUT    REFIT_MENU_ENTRY  **ChosenOption
) {
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
    EFI_INPUT_KEY               key;

    #if REFIT_DEBUG > 0
    CHAR16                     *MsgStr;
    CHAR16                     *KeyTxt;
    BOOLEAN                     TmpLevel;
    BOOLEAN                     CheckMute = FALSE;

    static BOOLEAN              OnceWait = FALSE;
    #endif


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
        Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
        if (!EFI_ERROR(Status)) {
            KeyAsString[0] = key.UnicodeChar;
            KeyAsString[1] = 0;

            ShortcutEntry = FindMenuShortcutEntry (Screen, KeyAsString);
            if (ShortcutEntry >= 0) {
                State.CurrentSelection = ShortcutEntry;
            }
            else {
                WaitForRelease =  TRUE;
                HaveTimeout    = FALSE;
            }
        }
    }

    if (!WaitForRelease) {
        // Quietly clear the keystroke buffer just in case
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif
        REFIT_CALL_2_WRAPPER(gST->ConIn->Reset, gST->ConIn, FALSE);
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif
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

        while (WaitForRelease) {
            // Esure no keys are being held down
            Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
            if (!EFI_ERROR(Status)) {
                // Reset to keep the keystroke buffer clear
                REFIT_CALL_2_WRAPPER(gST->ConIn->Reset, gST->ConIn, FALSE);
            }
            else {
                WaitForRelease = FALSE;
                REFIT_CALL_2_WRAPPER(gST->ConIn->Reset, gST->ConIn, TRUE);
            }
        } // while
    }

    if (GlobalConfig.DirectBoot) {
        // DA-TAG: DirectBoot is active.
        //         Either abort or proceed.
        if (MenuExit == MENU_EXIT_ZERO) {
            // DA-TAG: Proceed with DirectBoot.
            MenuExit = MENU_EXIT_ENTER;
        }
    }
    else {
        if (!AllowGraphicsMode &&
            (
                IsMainMenu ||
                MyStrBegins (L"Confirm System", Screen->Title)
            )
        ) {
            PrepareBlankLine();
            DrawScreenHeader (Screen->Title);
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
        TmpLevel = (GlobalConfig.LogLevel == 0) ? TRUE : FALSE;
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
            MsgStr = StrDuplicate (L"E X E C U T E   D I R E C T   B O O T");
        }
        else {
            MsgStr = StrDuplicate (L"P R O C E S S   U S E R   I N P U T");
        }
        ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
        LOG_MSG("%s", MsgStr);
        LOG_MSG("\n");
        MY_FREE_POOL(MsgStr);

        if (!GlobalConfig.DirectBoot) {
            TmpLevel = (GlobalConfig.LogLevel == 0) ? TRUE : FALSE;
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
        pdClear();
        if (State.PaintAll && GlobalConfig.ScreensaverTime != -1) {
            StyleFunc (Screen, &State, MENU_FUNCTION_PAINT_ALL, NULL);
            State.PaintAll = FALSE;
        }
        else if (State.PaintSelection) {
            StyleFunc (Screen, &State, MENU_FUNCTION_PAINT_SELECTION, NULL);
            State.PaintSelection = FALSE;
        }
        pdDraw();

        // DA-TAG: Investigate This
        //         Toggle the selection once to work around failure to
        //         display the default selection on load in text mode.
        //         This is a Workaround ... Proper solution needed.
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

        // Read keypress or pointer event (and wait for them if applicable)
        if (!PointerEnabled) {
            PointerStatus = EFI_NOT_READY;
        }
        else {
            PointerStatus = pdUpdateState();
        }

        Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
        if (!EFI_ERROR(Status)) {
            PointerActive      = FALSE;
            DrawSelection      =  TRUE;
            TimeSinceKeystroke =     0;
        }
        else if (!EFI_ERROR(PointerStatus)) {
            PointerActive      = TRUE;
            TimeSinceKeystroke =    0;

            if (StyleFunc != MainMenuStyle && pdGetState().Press) {
                // Prevent user from getting stuck on submenus
                // Only the 'About' screen is currently reachable without a keyboard
                MenuExit = MENU_EXIT_ENTER;
                break;
            }
        }
        else {
            if (HaveTimeout && TimeoutCountdown == 0) {
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

            if (!HaveTimeout && GlobalConfig.ScreensaverTime < 1) {
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
                    TimeoutCountdown = (TimeoutCountdown > ElapsCount)
                        ? TimeoutCountdown - ElapsCount : 0;
                }
                else if (
                    GlobalConfig.ScreensaverTime > 0 &&
                    TimeSinceKeystroke > (GlobalConfig.ScreensaverTime * 10)
                ) {
                    SaveScreen();
                    State.PaintAll     = TRUE;
                    TimeSinceKeystroke =    0;

                    if (!AllowGraphicsMode) {
                        PrepareBlankLine();
                        DrawScreenHeader (Screen->Title);
                    }
                }
            } // if/else !HaveTimeout

            continue;
        } // if/else !EFI_ERROR(Status)

        if (HaveTimeout) {
            // User pressed a key ... Cancel timeout
            StyleFunc (Screen, &State, MENU_FUNCTION_PAINT_TIMEOUT, L"");
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
                    ShortcutEntry  = FindMenuShortcutEntry (Screen, KeyAsString);

                    if (ShortcutEntry >= 0) {
                        State.CurrentSelection = ShortcutEntry;
                        MenuExit = MENU_EXIT_ENTER;
                    }

                    break;
            } // switch

            // Flag 'UserKeyPress' on Selection Change
            switch (key.ScanCode) {
                case SCAN_END:
                case SCAN_HOME:
                case SCAN_PAGE_UP:
                case SCAN_PAGE_DOWN:
                case SCAN_UP:
                case SCAN_LEFT:
                case SCAN_DOWN:
                case SCAN_RIGHT: UserKeyPress = TRUE;
            } // switch

            // Flag 'UserKeyScan' on Detecting Some Inputs
            switch (key.UnicodeChar) {
                case CHAR_BACKSPACE:
                case CHAR_TAB:
                case '+':
                case '-':
                default: UserKeyScan = TRUE;
            } // switch

            #if REFIT_DEBUG > 0
            KeyTxt = GetScanCodeText (key.ScanCode);
            if (MyStriCmp (KeyTxt, L"KEY_UNKNOWN")) {
                switch (key.UnicodeChar) {
                    case CHAR_LINEFEED:        KeyTxt = L"INFER_ENTER    Key: LineFeed"      ; break;
                    case CHAR_CARRIAGE_RETURN: KeyTxt = L"INFER_ENTER    Key: CarriageReturn"; break;
                    case CHAR_BACKSPACE:       KeyTxt = L"INFER_ESCAPE   Key: BackSpace"     ; break;
                    case ' ':                  KeyTxt = L"INFER_ESCAPE   Key: SpaceBar"      ; break;
                    case CHAR_TAB:             KeyTxt = L"INFER_DETAILS  Key: Tab"           ; break;
                    case '+':                  KeyTxt = L"INFER_DETAILS  Key: '+' (Plus)"    ; break;
                    case '-':                  KeyTxt = L"INFER_REMOVE   Key: '-' (Minus)"   ; break;
                } // switch
            }
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"Got Keystroke: UnicodeChar = 0x%02X ... ScanCode = 0x%02X - %s",
                key.UnicodeChar, key.ScanCode, KeyTxt
            );
            #endif

            if (BlockRescan) {
                if (MenuExit == MENU_EXIT_ESCAPE) {
                    MenuExit = MENU_EXIT_ZERO;
                }
                else if (MenuExit == MENU_EXIT_ZERO) {
                    // Unblock Rescan on Selection Change
                    switch (key.ScanCode) {
                        case SCAN_END:
                        case SCAN_HOME:
                        case SCAN_PAGE_UP:
                        case SCAN_PAGE_DOWN:
                        case SCAN_UP:
                        case SCAN_LEFT:
                        case SCAN_DOWN:
                        case SCAN_RIGHT: BlockRescan = FALSE;
                    } // switch

                    // Unblock Rescan on Detecting Some Inputs
                    switch (key.UnicodeChar) {
                        case CHAR_BACKSPACE:
                        case CHAR_TAB:
                        case '+':
                        case '-':
                        default: BlockRescan = FALSE;
                    } // switch
                }
            }

            if (MenuExit == MENU_EXIT_SCREENSHOT) {
                if (!GlobalConfig.DecoupleKeyF10 || key.ScanCode != SCAN_F10) {
                    egScreenShot();

                    // Unblock Rescan and Refresh Screen
                    BlockRescan = FALSE;
                    State.PaintAll = TRUE;
                    WaitForRelease = TRUE;
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
            POINTER_STATE PointerState = pdGetState();
            Item = FindMainMenuItem (
                Screen, &State,
                PointerState.X, PointerState.Y
            );

            switch (Item) {
                case POINTER_NO_ITEM:
                    if (DrawSelection) {
                        DrawSelection        = FALSE;
                        State.PaintSelection = TRUE;
                    }

                break;
                case POINTER_LEFT_ARROW:
                    if (PointerState.Press) {
                        UpdateScroll (&State, SCROLL_PAGE_UP);
                        UserKeyPress = TRUE;
                        BlockRescan = FALSE;
                    }

                    if (DrawSelection) {
                        DrawSelection        = FALSE;
                        State.PaintSelection = TRUE;
                    }

                    // React to Pointer Event
                    #if REFIT_DEBUG > 0
                    ALT_LOG(1, LOG_LINE_NORMAL,
                        L"Process Pointer Event ... Arrow Left"
                    );
                    #endif

                break;
                case POINTER_RIGHT_ARROW:
                    if (PointerState.Press) {
                        UpdateScroll (&State, SCROLL_PAGE_DOWN);
                        UserKeyPress = TRUE;
                        BlockRescan = FALSE;
                    }

                    if (DrawSelection) {
                        DrawSelection        = FALSE;
                        State.PaintSelection = TRUE;
                    }

                    // React to Pointer Event
                    #if REFIT_DEBUG > 0
                    ALT_LOG(1, LOG_LINE_NORMAL,
                        L"Process Pointer Event ... Arrow Right"
                    );
                    #endif

                break;
                default:
                    if (!DrawSelection || Item != State.CurrentSelection) {
                        DrawSelection          = TRUE;
                        State.PaintSelection   = TRUE;
                        State.CurrentSelection = Item;
                    }

                    if (PointerState.Press) {
                        MenuExit = MENU_EXIT_ENTER;

                        // React to Pointer Event
                        #if REFIT_DEBUG > 0
                        ALT_LOG(1, LOG_LINE_NORMAL,
                            L"Process Pointer Event ... Enter"
                        );
                        #endif
                    }
            } // switch
        } // if/else !PointerActive
    } // while

    pdClear();
    StyleFunc (Screen, &State, MENU_FUNCTION_CLEANUP, NULL);

    // Ignore MenuExit if FlushFailedTag is set and not previously reset
    if (FlushFailedTag && !FlushFailReset) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"FlushFailedTag is Set ... Ignore MenuExit");
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
            break;
        }

        if (!IsMainMenu              ||
            ClearedBuffer            ||
            AppleFirmware            ||
            FlushFailReset           ||
            GlobalConfig.DirectBoot  ||
            MenuExit != MENU_EXIT_ENTER
        ) {
            break;
        }

        // Ignore MenuExit if time between loading main menu and detecting
        // an 'Enter' keypress is too low. Primed Keystroke Buffers appear
        // to only affect UEFI PC but some provision to cover Macs made
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
            else if (GlobalConfig.LogLevel > 0) {
                MenuExitGate = MenuExitNumb * 4;
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

//
// Generic text-mode style
//

// Show information lines in text mode.
static
VOID ShowTextInfoLines (
    IN REFIT_MENU_SCREEN *Screen
) {
    INTN i;

    if (Screen->InfoLineCount == 0) {
        // Early Return
        return;
    }

    BeginTextScreen (Screen->Title);

    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

    for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
        REFIT_CALL_3_WRAPPER(
            gST->ConOut->SetCursorPosition, gST->ConOut,
            3, 4 + i
        );
        REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, Screen->InfoLines[i]);
    }
} // VOID ShowTextInfoLines()

// Do most of the work for text-based menus.
VOID TextMenuStyle (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    IN UINTN              Function,
    IN CHAR16            *ParamText
) {
    INTN    i;
    UINTN   MenuWidth;
    UINTN   ItemWidth;
    UINTN   MenuHeight;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    static UINTN    MenuPosY;
    static CHAR16 **DisplayStrings;

    State->ScrollMode = SCROLL_MODE_TEXT;

    switch (Function) {
        case MENU_FUNCTION_INIT:
            // Vertical layout
            MenuPosY = 4;
            if (Screen->InfoLineCount > 0) {
                MenuPosY += Screen->InfoLineCount + 1;
            }

            MenuHeight = ConHeight - MenuPosY - 3;
            if (Screen->TimeoutSeconds > 0) {
                MenuHeight -= 2;
            }
            InitScroll (State, Screen->EntryCount, MenuHeight);

            // Determine menu width ... Minimum = 20
            MenuWidth = 20;

            for (i = 0; i <= State->MaxIndex; i++) {
                ItemWidth = StrLen (Screen->Entries[i]->Title);
                if (MenuWidth < ItemWidth) {
                    MenuWidth = ItemWidth;
                }
            }

            MenuWidth += 2;
            if (MenuWidth > ConWidth - 3) {
                MenuWidth = ConWidth - 3;
            }

            // Prepare strings for display
            DisplayStrings = AllocatePool (sizeof (CHAR16 *) * Screen->EntryCount);
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
                DisplayStrings[i] = AllocateZeroPool (2 * sizeof (CHAR16));
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
            // Release temporary memory
            for (i = 0; i <= State->MaxIndex; i++) {
                MY_FREE_POOL(DisplayStrings[i]);
            }
            MY_FREE_POOL(DisplayStrings);

        break;
        case MENU_FUNCTION_PAINT_ALL:
            // Paint the whole screen (initially and after scrolling)
            ShowTextInfoLines (Screen);
            for (i = 0; i <= State->MaxIndex; i++) {
                if (i >= State->FirstVisible && i <= State->LastVisible) {
                    REFIT_CALL_3_WRAPPER(
                        gST->ConOut->SetCursorPosition, gST->ConOut,
                        2, MenuPosY + (i - State->FirstVisible)
                    );

                    if (i == State->CurrentSelection) {
                        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_CHOICE_CURRENT);
                    }
                    else if (DisplayStrings[i]) {
                        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_CHOICE_BASIC);
                        REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, DisplayStrings[i]);
                    }
                }
            }

            // Scrolling indicators
            REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_SCROLLARROW);
            REFIT_CALL_3_WRAPPER(
                gST->ConOut->SetCursorPosition, gST->ConOut,
                0, MenuPosY
            );

            if (State->FirstVisible > 0) {
                gST->ConOut->OutputString (gST->ConOut, ArrowUp);
            }
            else {
                gST->ConOut->OutputString (gST->ConOut, L" ");
            }

            gST->ConOut->SetCursorPosition (gST->ConOut, 0, MenuPosY + State->MaxVisible);

            if (State->LastVisible < State->MaxIndex) {
                REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, ArrowDown);
            }
            else {
                REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, L" ");
            }

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
               if (Screen->Hint1 != NULL) {
                   REFIT_CALL_3_WRAPPER(
                       gST->ConOut->SetCursorPosition, gST->ConOut,
                       0, ConHeight - 2
                   );
                   REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, Screen->Hint1);
               }

               if (Screen->Hint2 != NULL) {
                   REFIT_CALL_3_WRAPPER(
                       gST->ConOut->SetCursorPosition, gST->ConOut,
                       0, ConHeight - 1
                   );
                   REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, Screen->Hint2);
               }
            }

        break;
        case MENU_FUNCTION_PAINT_SELECTION:
            // Redraw selection cursor
            REFIT_CALL_3_WRAPPER(
                gST->ConOut->SetCursorPosition, gST->ConOut,
                2, MenuPosY + (State->PreviousSelection - State->FirstVisible)
            );
            REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_CHOICE_BASIC);
            if (DisplayStrings[State->PreviousSelection] != NULL) {
                REFIT_CALL_2_WRAPPER( gST->ConOut->OutputString, gST->ConOut, DisplayStrings[State->PreviousSelection]);
            }
            REFIT_CALL_3_WRAPPER(
                gST->ConOut->SetCursorPosition, gST->ConOut,
                2, MenuPosY + (State->CurrentSelection - State->FirstVisible)
            );
            REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_CHOICE_CURRENT);
            REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, DisplayStrings[State->CurrentSelection]);

        break;
        case MENU_FUNCTION_PAINT_TIMEOUT:
            if (ParamText[0] == 0) {
                // Clear message
                if (!BlankLine) {
                    PrepareBlankLine();
                }
                REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
                REFIT_CALL_3_WRAPPER(
                    gST->ConOut->SetCursorPosition, gST->ConOut,
                    0, ConHeight - 3
                );
                REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, BlankLine + 1);
            }
            else {
                // Paint or update message
                REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
                REFIT_CALL_3_WRAPPER(
                    gST->ConOut->SetCursorPosition, gST->ConOut,
                    3, ConHeight - 3
                );
                REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, ParamText);
            }
    } // switch
} // VOID TextMenuStyle()

//
// Graphical generic style
//

static
UINTN TextLineHeight (VOID) {
    return egGetFontHeight() + TEXT_YMARGIN * 2;
} // static UINTN TextLineHeight()

//
// Display a submenu
//

// Display text with a solid background (MenuBackgroundPixel or SelectionBackgroundPixel).
// Indents text by one character and placed TEXT_YMARGIN pixels down from the
// specified XPos and YPos locations.
static
VOID DrawText (
    IN CHAR16  *Text,
    IN BOOLEAN  Selected,
    IN UINTN    FieldWidth,
    IN UINTN    XPos,
    IN UINTN    YPos
) {
    EG_IMAGE *TextBuffer;
    EG_PIXEL  Bg;

    TextBuffer = egCreateFilledImage (
        FieldWidth,
        TextLineHeight(),
        FALSE,
        &MenuBackgroundPixel
    );

    if (TextBuffer == NULL) {
        // Early Return
        return;
    }

    Bg = MenuBackgroundPixel;
    if (Selected) {
        EG_PIXEL SelectionBackgroundPixel = { 0xFF, 0xFF, 0xFF, 0 };

        // Draw selection bar background
        egFillImageArea (
            TextBuffer,
            0, 0,
            FieldWidth,
            TextBuffer->Height,
            &SelectionBackgroundPixel
        );
        Bg = SelectionBackgroundPixel;
    }

    // Get Luminance Index
    UINTN LumIndex = GetLumIndex (
        (UINTN) Bg.r,
        (UINTN) Bg.g,
        (UINTN) Bg.b
    );

    // Render the text
    egRenderText (
        Text,
        TextBuffer,
        egGetFontCellWidth(),
        TEXT_YMARGIN,
        (UINT8) LumIndex
    );

    egDrawImageWithTransparency (
        TextBuffer, NULL,
        XPos, YPos,
        TextBuffer->Width,
        TextBuffer->Height
    );

    MY_FREE_IMAGE(TextBuffer);
} // VOID DrawText()

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

    if (Image == NULL || ((Image->Width * Image->Height) == 0)) {
        // Early Return
        return 0;
    }

    Sum = 0;
    for (i = 0; i < (Image->Width * Image->Height); i++) {
        Sum += (Image->PixelData[i].r + Image->PixelData[i].g + Image->PixelData[i].b);
    }
    Sum /= (Image->Width * Image->Height * 3);

    return (UINT8) Sum;
} // UINT8 AverageBrightness()

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
        Text,
        TextBuffer,
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
}

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

    *Width     = 20;
    *Height    = 5;
    TitleWidth = egComputeTextWidth (Screen->Title);

    for (i = 0; i < Screen->InfoLineCount; i++) {
        ItemWidth = StrLen (Screen->InfoLines[i]);

        if (*Width < ItemWidth) {
            *Width = ItemWidth;
        }

        (*Height)++;
    }

    for (i = 0; i <= State->MaxIndex; i++) {
        ItemWidth = StrLen (Screen->Entries[i]->Title);

        if (*Width < ItemWidth) {
            *Width = ItemWidth;
        }

        (*Height)++;
    }

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

    // Keep it within the bounds of the screen, or 2/3 of the screen's width
    // for screens over 800 pixels wide
    if (*Width > ScreenW) {
        *Width = ScreenW;
    }

    *XPos = (ScreenW - *Width) / 2;

    // Top of hint text
    FontCellHeight = egGetFontHeight();
    HintTop  = ScreenH - (FontCellHeight * 3);
    *Height *= TextLineHeight();

    if (Screen->TitleImage &&
        (*Height < (Screen->TitleImage->Height + TextLineHeight() * 4))
    ) {
        *Height = Screen->TitleImage->Height + TextLineHeight() * 4;
    }

    if (GlobalConfig.BannerBottomEdge >= HintTop) {
        // Probably a full-screen image; treat it as an empty banner
        BannerBottomEdge = 0;
    }
    else {
        BannerBottomEdge = GlobalConfig.BannerBottomEdge;
    }

    if (*Height > (HintTop - BannerBottomEdge - FontCellHeight * 2)) {
        BannerBottomEdge = 0;
    }

    if (*Height > (HintTop - BannerBottomEdge - FontCellHeight * 2)) {
        *Height = (HintTop - BannerBottomEdge - FontCellHeight * 2);
    }

    *YPos = ((ScreenH - *Height) / 2);
    if (*YPos < BannerBottomEdge) {
        *YPos = BannerBottomEdge +
            FontCellHeight +
            (HintTop - BannerBottomEdge - *Height) / 2;
    }
} // VOID ComputeSubScreenWindowSize()

// Displays sub-menus
VOID GraphicsMenuStyle (
    IN REFIT_MENU_SCREEN  *Screen,
    IN SCROLL_STATE       *State,
    IN UINTN               Function,
    IN CHAR16             *ParamText
) {
    INTN      i;
    UINTN     ItemWidth;
    EG_IMAGE *Window;
    EG_PIXEL *BackgroundPixel = &(GlobalConfig.ScreenBackground->PixelData[0]);

    static UINTN EntriesPosX, EntriesPosY;
    static UINTN LineWidth, MenuWidth, MenuHeight;
    static UINTN TitlePosX, TimeoutPosY, CharWidth;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    CharWidth = egGetFontCellWidth();
    State->ScrollMode = SCROLL_MODE_TEXT;

    switch (Function) {
        case MENU_FUNCTION_CLEANUP:
            // Nothing to do
        break;
        case MENU_FUNCTION_INIT:
            InitScroll (State, Screen->EntryCount, 0);
            ComputeSubScreenWindowSize (
                Screen, State,
                &EntriesPosX, &EntriesPosY,
                &MenuWidth, &MenuHeight,
                &LineWidth
            );

            TimeoutPosY = EntriesPosY + (Screen->EntryCount + 1) * TextLineHeight();

            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_SET;
            #endif
            // Initial painting
            SwitchToGraphicsAndClear (TRUE);
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_OFF;
            #endif

            Window = egCreateFilledImage (
                MenuWidth, MenuHeight,
                FALSE, BackgroundPixel
            );

            if (Window) {
                egDrawImage (Window, EntriesPosX, EntriesPosY);
                MY_FREE_IMAGE(Window);
            }

            ItemWidth = egComputeTextWidth (Screen->Title);

            if (MenuWidth > ItemWidth) {
                TitlePosX = EntriesPosX + (MenuWidth - ItemWidth) / 2 - CharWidth;
            }
            else {
               TitlePosX = EntriesPosX;
               if (CharWidth > 0) {
                  i = MenuWidth / CharWidth - 2;
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
                    EntriesPosY + TextLineHeight() * 2,
                    BackgroundPixel
                );
                EntriesPosX += (Screen->TitleImage->Width + TITLEICON_SPACING * 2);
            }

            EntriesPosY += (TextLineHeight() * 2);

            if (Screen->InfoLineCount > 0) {
                for (i = 0; i < (INTN) Screen->InfoLineCount; i++) {
                    DrawText (
                        Screen->InfoLines[i],
                        FALSE, LineWidth,
                        EntriesPosX, EntriesPosY
                    );

                    EntriesPosY += TextLineHeight();
                }

                // Also add a blank line
                EntriesPosY += TextLineHeight();
            }

            for (i = 0; i <= State->MaxIndex; i++) {
                DrawText (
                    Screen->Entries[i]->Title,
                    (i == State->CurrentSelection),
                    LineWidth,
                    EntriesPosX,
                    EntriesPosY + i * TextLineHeight()
                );
            }

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
                if ((Screen->Hint1 != NULL) && (StrLen (Screen->Hint1) > 0)) {
                    DrawTextWithTransparency (
                        Screen->Hint1,
                        (ScreenW - egComputeTextWidth (Screen->Hint1)) / 2,
                        ScreenH - (egGetFontHeight() * 3)
                    );
                }

                if ((Screen->Hint2 != NULL) && (StrLen (Screen->Hint2) > 0)) {
                    DrawTextWithTransparency (
                        Screen->Hint2,
                        (ScreenW - egComputeTextWidth (Screen->Hint2)) / 2,
                        ScreenH - (egGetFontHeight() * 2)
                    );
                }
            }

        break;
        case MENU_FUNCTION_PAINT_SELECTION:
            // Redraw selection cursor
            DrawText (
                Screen->Entries[State->PreviousSelection]->Title,
                FALSE, LineWidth,
                EntriesPosX,
                EntriesPosY + State->PreviousSelection * TextLineHeight()
            );

            DrawText (
                Screen->Entries[State->CurrentSelection]->Title,
                TRUE, LineWidth,
                EntriesPosX,
                EntriesPosY + State->CurrentSelection * TextLineHeight()
            );

        break;
        case MENU_FUNCTION_PAINT_TIMEOUT:
            DrawText (ParamText, FALSE, LineWidth, EntriesPosX, TimeoutPosY);

        break;
    } // switch
} // static VOID GraphicsMenuStyle()

//
// Graphical main menu style
//

static
VOID DrawMainMenuEntry (
    REFIT_MENU_ENTRY *Entry,
    BOOLEAN           selected,
    UINTN             XPos,
    UINTN             YPos
) {
    EG_IMAGE *Background;

    // Do not draw selection image when not hoverin if using pointer
    if (!selected || !DrawSelection) {
        // Image not selected ... copy background
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

        BltImageCompositeBadge (
            Background,
            Entry->Image,
            Entry->BadgeImage,
            XPos, YPos
        );

        MY_FREE_IMAGE(Background);
    }
} // VOID DrawMainMenuEntry()

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
        if (Screen->Entries[i]->Row == 0) {
            if (i <= State->LastVisible) {
                DrawMainMenuEntry (
                    Screen->Entries[i],
                    (i == State->CurrentSelection) ? TRUE : FALSE,
                    itemPosX[i - State->FirstVisible],
                    row0PosY
                );
            }
        }
        else {
            DrawMainMenuEntry (
                Screen->Entries[i],
                (i == State->CurrentSelection) ? TRUE : FALSE,
                itemPosX[i],
                row1PosY
            );
        }
    }

    if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL) &&
        (!PointerActive || DrawSelection)
    ) {
        DrawTextWithTransparency (L"", 0, textPosY);
        DrawTextWithTransparency (
            Screen->Entries[State->CurrentSelection]->Title,
            (ScreenW - egComputeTextWidth (Screen->Entries[State->CurrentSelection]->Title)) >> 1,
            textPosY
        );
    }
    else {
        DrawTextWithTransparency (L"", 0, textPosY);
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
    UINTN XSelectPrev, XSelectCur, YPosPrev, YPosCur;

    if (
        (State->CurrentSelection < State->InitialRow1) &&
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

    if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL) &&
        (!PointerActive || DrawSelection)
    ) {
        DrawTextWithTransparency (L"", 0, textPosY);
        DrawTextWithTransparency (
            Screen->Entries[State->CurrentSelection]->Title,
            (ScreenW - egComputeTextWidth (Screen->Entries[State->CurrentSelection]->Title)) >> 1,
            textPosY
        );
    }
    else {
        DrawTextWithTransparency (L"", 0, textPosY);
    }
} // static VOID MoveSelection (VOID)

// Fetch the icon specified by ExternalFilename if available,
// or by BuiltInIcon if not.
static
EG_IMAGE * GetIcon (
    IN EG_EMBEDDED_IMAGE *BuiltInIcon,
    IN CHAR16            *ExternalFilename
) {
    EG_IMAGE *Icon;

    Icon = egFindIcon (
        ExternalFilename,
        GlobalConfig.IconSizes[ICON_SIZE_SMALL]
    );
    if (Icon != NULL) {
        // Early Return
        return Icon;
    }

    return egPrepareEmbeddedImage (BuiltInIcon, TRUE, NULL);
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

UINTN ComputeRow0PosY (
    IN BOOLEAN ApplyOffset
) {
    UINTN Row0PosY;
    INTN  IconRowTweak;

    // Default IconRowTweak to zero
    // Keep rows in central position
    IconRowTweak = 0;

    // Amend IconRowTweak if 'ApplyOffset' is active
    if (ApplyOffset) {
        if (GlobalConfig.IconRowMove != 0) {
            // Set positive value
            // Moves rows to lower third of screen
            IconRowTweak = (ScreenH / 4);

            if (GlobalConfig.IconRowMove > 0) {
                // Set negative value
                // Moves rows to upper third of screen
                IconRowTweak *= -1;

                // Finetune position
                // Account for icon size ... only when in upper third
                IconRowTweak += (TileSizes[0] / 2);
            }
        }
    }

    // Set base row position
    // Adds 'IconRowTweak' value (which may be zero)
    Row0PosY = ((ScreenH / 2) - (TileSizes[0] / 2)) + IconRowTweak;

    // Amend row position if 'ApplyOffset' is active
    if (ApplyOffset) {
        // Amend row position
        // Adds 'icon_row_tune' value (which may be zero)
        Row0PosY += GlobalConfig.IconRowTune;
    }

    // Return row position
    return Row0PosY;
} // UINTN ComputeRow0PosY()

// Display (or erase) the arrow icons to the left
// and right of an icon's row ... as appropriate.
static
VOID PaintArrows (
    SCROLL_STATE *State,
    UINTN         PosX,
    UINTN         PosY,
    UINTN         row0Loaders
) {
    UINTN            RightX;
    static EG_IMAGE *LeftArrow       =  NULL;
    static EG_IMAGE *RightArrow      =  NULL;
    static EG_IMAGE *LeftBackground  =  NULL;
    static EG_IMAGE *RightBackground =  NULL;
    static BOOLEAN   LoadedArrows    = FALSE;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    RightX = (ScreenW + (TileSizes[0] + TILE_XSPACING) * State->MaxVisible) / 2 + TILE_XSPACING;

    if (!LoadedArrows && !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_ARROWS)) {
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
            ? PaintIcon (LeftArrow, PosX, PosY, ALIGN_RIGHT)
            : BltImage (LeftBackground, PosX - LeftArrow->Width, PosY - (LeftArrow->Height / 2));
    }

    if (RightArrow && RightBackground) {
        (State->LastVisible < (row0Loaders - 1))
            ? PaintIcon (RightArrow, RightX, PosY, ALIGN_LEFT)
            : BltImage (RightBackground, RightX, PosY - (RightArrow->Height / 2));
    }
} // VOID PaintArrows()

// Display main menu in graphics mode
VOID MainMenuStyle (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    IN UINTN              Function,
    IN CHAR16            *ParamText
) {
    INTN   i;
    UINTN  row0Count, row1Count, row1PosX, row1PosXRunning;

    static UINTN  row0PosX, row0PosXRunning, row1PosY, row0Loaders;
    static UINTN *itemPosX;
    static UINTN  row0PosY, textPosY;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    State->ScrollMode = SCROLL_MODE_ICONS;
    switch (Function) {
        case MENU_FUNCTION_INIT:
            InitScroll (State, Screen->EntryCount, GlobalConfig.MaxTags);

            // Layout
            row0Count   = 0;
            row1Count   = 0;
            row0Loaders = 0;

            for (i = 0; i <= State->MaxIndex; i++) {
                if (Screen->Entries[i]->Row == 1) {
                    row1Count++;
                }
                else {
                    row0Loaders++;
                    if (row0Count < State->MaxVisible) {
                        row0Count++;
                    }
                }
            } // for

            row0PosX = (ScreenW + TILE_XSPACING - (TileSizes[0] + TILE_XSPACING) * row0Count) >> 1;
            row0PosY = ComputeRow0PosY (TRUE);
            row1PosX = (ScreenW + TILE_XSPACING - (TileSizes[1] + TILE_XSPACING) * row1Count) >> 1;
            row1PosY = row0PosY + TileSizes[0] + TILE_YSPACING;

            if (row1Count == 0) {
                textPosY = row1PosY;
            }
            else {
                textPosY = row1PosY + TileSizes[1] + TILE_YSPACING;
            }

            itemPosX = AllocatePool (sizeof (UINTN) * Screen->EntryCount);
            if (itemPosX == NULL) {
                // Early Return
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

            // Initial painting
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
            PaintAll (Screen, State, itemPosX, row0PosY, row1PosY, textPosY);
            // For PaintArrows(), the starting Y position is moved to the midpoint
            // of the surrounding row; PaintIcon() adjusts this back up by half the
            // icon's height to properly center it.
            PaintArrows (State, row0PosX - TILE_XSPACING, row0PosY + (TileSizes[0] / 2), row0Loaders);

        break;
        case MENU_FUNCTION_PAINT_SELECTION:
            PaintSelection (Screen, State, itemPosX, row0PosY, row1PosY, textPosY);

        break;
        case MENU_FUNCTION_PAINT_TIMEOUT:
            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL)) {
               DrawTextWithTransparency (L"", 0, textPosY + TextLineHeight());
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
    UINTN  ItemIndex;
    UINTN  row0Count, row1Count, row1PosX, row1PosXRunning;

    static UINTN  row0PosX, row0PosXRunning, row1PosY, row0Loaders;
    static UINTN *itemPosX;
    static UINTN  row0PosY;

    row0Count = 0;
    row1Count = 0;
    row0Loaders = 0;
    for (i = 0; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 1) {
            row1Count++;
        }
        else {
            row0Loaders++;
            if (row0Count < State->MaxVisible) {
                row0Count++;
            }
        }
    } // for

    row0PosX = (ScreenW + TILE_XSPACING - (TileSizes[0] + TILE_XSPACING) * row0Count) >> 1;
    row0PosY = ComputeRow0PosY (TRUE);
    row1PosX = (ScreenW + TILE_XSPACING - (TileSizes[1] + TILE_XSPACING) * row1Count) >> 1;
    row1PosY = row0PosY + TileSizes[0] + TILE_YSPACING;

    if (PosY >= row0PosY && PosY <= row0PosY + TileSizes[0]) {
        itemRow = 0;
        if (PosX <= row0PosX) {
            // Early Return
            return POINTER_LEFT_ARROW;
        }

        if (PosX >= (ScreenW - row0PosX)) {
            // Early Return
            return POINTER_RIGHT_ARROW;
        }
    }
    else if (PosY >= row1PosY && PosY <= row1PosY + TileSizes[1]) {
        itemRow = 1;
    }
    else {
        // Early Return ... Y coordinate is outside of either row
        return POINTER_NO_ITEM;
    }

    ItemIndex = POINTER_NO_ITEM;
    itemPosX = AllocatePool (sizeof (UINTN) * Screen->EntryCount);
    if (itemPosX == NULL) {
        // Early Return
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
    }

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
        else if (Screen->Entries[i]->Row == 1 && itemRow == 1) {
            if (PosX >= itemPosX[i] && PosX <= itemPosX[i] + TileSizes[1]) {
                ItemIndex = i;
                break;
            }
        }
    } // fpr

    MY_FREE_POOL(itemPosX);

    return ItemIndex;
} // VOID FindMainMenuItem()

VOID GenerateWaitList(VOID) {
    UINTN PointerCount;

    if (WaitList != NULL) {
        // Early Return
        return;
    }

    PointerCount   = pdCount();
    WaitListLength = 2 + PointerCount;

    WaitList = AllocatePool (sizeof (EFI_EVENT) * WaitListLength);
    if (WaitList == NULL) {
        // Early Return
        return;
    }
    WaitList[0] = gST->ConIn->WaitForKey;

    for (UINTN Index = 0; Index < PointerCount; Index++) {
        WaitList[Index + 1] = pdWaitEvent (Index);
    } // for
} // VOID GenerateWaitList()

UINTN WaitForInput (
    IN UINTN Timeout
) {
    EFI_STATUS  Status;
    UINTN       Length;
    UINTN       Index;
    EFI_EVENT   TimerEvent;

    // Generate WaitList if not already generated.
    GenerateWaitList();

    Length = WaitListLength;
    TimerEvent = NULL;

    Status = REFIT_CALL_5_WRAPPER(
        gBS->CreateEvent, EVT_TIMER,
        0, NULL,
        NULL, &TimerEvent
    );
    if (Timeout == 0) {
        Length--;
    }
    else {
        if (EFI_ERROR(Status)) {
            // Pause for 100 ms
            // DA-TAG: 100 Loops == 1 Sec
            RefitStall (10);

            return INPUT_TIMER_ERROR;
        }

        REFIT_CALL_3_WRAPPER(
            gBS->SetTimer, TimerEvent,
            TimerRelative, Timeout * 10000
        );
        WaitList[Length - 1] = TimerEvent;
    }

    Index  = INPUT_TIMEOUT;
    Status = REFIT_CALL_3_WRAPPER(
        gBS->WaitForEvent, Length,
        WaitList, &Index
    );
    REFIT_CALL_1_WRAPPER(gBS->CloseEvent, TimerEvent);

    if (EFI_ERROR(Status)) {
        // Pause for 100 ms
        // DA-TAG: 100 Loops == 1 Sec
        RefitStall (10);

        return INPUT_TIMER_ERROR;
    }

    if (Index == 0) {
        return INPUT_KEY;
    }

    if (Index < Length - 1) {
        return INPUT_POINTER;
    }

    return INPUT_TIMEOUT;
} // UINTN WaitForInput()

// Enable the user to edit boot loader options.
// Returns TRUE if the user exited with edited options; FALSE if the user
// pressed Esc to terminate the edit.
static
BOOLEAN EditOptions (
    LOADER_ENTRY *MenuEntry
) {
    UINTN    x_max, y_max;
    CHAR16  *EditedOptions;
    BOOLEAN  retval;

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

        retval = TRUE;
    }
    else {
        retval = FALSE;
    }

    if (!GlobalConfig.TextOnly) {
        SwitchToGraphics();
    }

    return retval;
} // VOID EditOptions()

//
// user-callable dispatcher functions
//

VOID DisplaySimpleMessage (
    CHAR16 *Message,
    CHAR16 *Title OPTIONAL
) {
    INTN               DefaultEntry;
    INTN               MenuExit;
    BOOLEAN            RetVal;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_SCREEN *SimpleMessageMenu;
    REFIT_MENU_ENTRY  *ChosenOption;

    #if REFIT_DEBUG > 0
    CHAR16            *MsgStr;
    #endif

    if (Message == NULL) {
        // Early Return
        return;
    }

    SimpleMessageMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (SimpleMessageMenu == NULL) {
        // Early Return
        return;
    }

    if (Title == NULL) {
        Title = L"Information";
    }

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (L"SimpleMessage:- '%s ::: %s'", Title, Message);
    LOG_MSG("INFO: %s", MsgStr);
    LOG_MSG("\n\n");
    ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    SimpleMessageMenu->Title      = StrDuplicate (Title);
    SimpleMessageMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_ABOUT);
    SimpleMessageMenu->Hint1      = StrDuplicate (L"Press 'Enter' to Return to Main Menu");
    SimpleMessageMenu->Hint2      = StrDuplicate (L""                                    );

    AddMenuInfoLine (SimpleMessageMenu, Message, FALSE);

    RetVal = GetMenuEntryReturn (&SimpleMessageMenu);
    if (!RetVal) {
        FreeMenuScreen (&SimpleMessageMenu);

        // Early Return
        return;
    }

    DefaultEntry = 9999; // Use the Max Index
    Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
    MenuExit = DrawMenuScreen (
        SimpleMessageMenu, Style,
        &DefaultEntry, &ChosenOption
    );

    #if REFIT_DEBUG > 0
    LogExit (MenuExit, __func__, Title);
    #endif

    FreeMenuScreen (&SimpleMessageMenu);
} // VOID DisplaySimpleMessage()

// Check each filename in FilenameList to be sure it refers to a valid file. If
// not, delete it. This works only on filenames that are complete, with volume,
// path, and filename components; if the filename omits the volume, the search
// is not done and the item is left intact, no matter what.
// Returns TRUE if any files were deleted, FALSE otherwise.
static
BOOLEAN RemoveInvalidFilenames (
    CHAR16          *FilenameList,
    CHAR16          *VarName
) {
    EFI_STATUS       Status;
    UINTN            i;
    CHAR16          *VolName;
    CHAR16          *Filename;
    CHAR16          *OneElement;
    BOOLEAN          DeleteFlag;
    BOOLEAN          DeleteThis;
    REFIT_VOLUME    *Volume;
    EFI_FILE_HANDLE  FileHandle;

    VolName = NULL;
    DeleteFlag = FALSE;
    i = 0; // *DO NOT* increment in 'while' call
    while ((OneElement = FindCommaDelimited (FilenameList, i)) != NULL) {
        DeleteThis = FALSE;
        Filename = StrDuplicate (OneElement);

        if (SplitVolumeAndFilename (&Filename, &VolName)) {
            DeleteThis = TRUE;

            if (FindVolume (&Volume, VolName) && Volume->RootDir) {
                Status = REFIT_CALL_5_WRAPPER(
                    Volume->RootDir->Open, Volume->RootDir,
                    &FileHandle, Filename,
                    EFI_FILE_MODE_READ, 0
                );

                if (!EFI_ERROR(Status)) {
                    DeleteThis = FALSE;
                    REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);
                }
            }
        }

        // Increment index here if not deleting
        (DeleteThis) ? DeleteItemFromCsvList (OneElement, FilenameList) : i++;

        MY_FREE_POOL(OneElement);
        MY_FREE_POOL(Filename);
        MY_FREE_POOL(VolName);

        DeleteFlag |= DeleteThis;
    } // while

    return DeleteFlag;
} // BOOLEAN RemoveInvalidFilenames()

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
} // VOID SaveHiddenList()

// Present a menu for the user to delete (un-hide) hidden tags.
VOID ManageHiddenTags (VOID) {
    INTN                 DefaultEntry;
    UINTN                i;
    UINTN                MenuExit;
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

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Prepare Menu Screen");
    ALT_LOG(1, LOG_LINE_NORMAL, L"Screen Title:- '%s'", LABEL_HIDDEN);
    #endif

    AllTags  = NULL;
    SaveTags = SaveTools = SaveLegacy = SaveFirmware = FALSE;

    HiddenTags = ReadHiddenTags (L"HiddenTags");
    if (HiddenTags != NULL &&
        HiddenTags[0] != L'\0'
    ) {
        SaveTags = RemoveInvalidFilenames (HiddenTags, L"HiddenTags");
        AllTags = StrDuplicate (HiddenTags);
    }

    HiddenTools = ReadHiddenTags (L"HiddenTools");
    if (HiddenTools != NULL &&
        HiddenTools[0] != L'\0'
    ) {
        SaveTools = RemoveInvalidFilenames (HiddenTools, L"HiddenTools");
        if (AllTags == NULL) {
            AllTags = StrDuplicate (HiddenTools);
        }
        else {
            MergeUniqueStrings (&AllTags, HiddenTools, L',');
        }
    }

    HiddenLegacy = ReadHiddenTags (L"HiddenLegacy");
    if (HiddenLegacy != NULL &&
        HiddenLegacy[0] != L'\0'
    ) {
        if (AllTags == NULL) {
            AllTags = StrDuplicate (HiddenLegacy);
        }
        else {
            MergeUniqueStrings (&AllTags, HiddenLegacy, L',');
        }
    }

    HiddenFirmware = ReadHiddenTags (L"HiddenFirmware");
    if (HiddenFirmware != NULL &&
        HiddenFirmware[0] != L'\0'
    ) {
        if (AllTags == NULL) {
            AllTags = StrDuplicate (HiddenFirmware);
        }
        else {
            MergeUniqueStrings (&AllTags, HiddenFirmware, L',');
        }
    }

    if (AllTags == NULL || StrLen (AllTags) == 0) {
        DisplaySimpleMessage (L"No Hidden Entries Found", NULL);

        // Early Return
        return;
    }

    RestoreItemMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    RestoreItemMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_HIDDEN);
    RestoreItemMenu->Title      = StrDuplicate (LABEL_HIDDEN           );
    RestoreItemMenu->Hint1      = StrDuplicate (SELECT_OPTION_HINT     );
    RestoreItemMenu->Hint2      = StrDuplicate (RETURN_MAIN_SCREEN_HINT);
    AddMenuInfoLine (RestoreItemMenu, L"Select an Entry and Press 'Enter' to Restore", FALSE);

    OneElement    = NULL;
    MenuEntryItem = NULL;
    i = 0;
    while ((OneElement = FindCommaDelimited (AllTags, i++)) != NULL) {
        MenuEntryItem  = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
        MenuEntryItem->Title = StrDuplicate (OneElement);
        MenuEntryItem->Tag   = TAG_RETURN;
        AddMenuEntry (RestoreItemMenu, MenuEntryItem);

        MY_FREE_POOL(OneElement);
    } // while

    do {
        if (!GetMenuEntryReturn (&RestoreItemMenu)) {
            break;
        }

        DefaultEntry = 9999; // Use the Max Index
        Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
        MenuExit = DrawMenuScreen (
            RestoreItemMenu, Style,
            &DefaultEntry, &ChosenOption
        );

        #if REFIT_DEBUG > 0
        LogExit (MenuExit, __func__, ChosenOption->Title);
        #endif

        // Previously unset defaults
        if (MenuExit == MENU_EXIT_ENTER) {
            if (HiddenTags    ) SaveTags     |= DeleteItemFromCsvList (ChosenOption->Title, HiddenTags    );
            if (HiddenTools   ) SaveTools    |= DeleteItemFromCsvList (ChosenOption->Title, HiddenTools   );
            if (HiddenLegacy  ) SaveLegacy   |= DeleteItemFromCsvList (ChosenOption->Title, HiddenLegacy  );
            if (HiddenFirmware) SaveFirmware |= DeleteItemFromCsvList (ChosenOption->Title, HiddenFirmware);
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
    CHAR16     *Buffer;
    UINTN       Size;
    EFI_STATUS  Status;

    #if REFIT_DEBUG > 0
    CHAR16 *CheckErrMsg;
    #endif

    Buffer = NULL;
    Status = EfivarGetRaw (
        &RefindPlusGuid, VarName,
        (VOID **) &Buffer, &Size
    );
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        if (Status != EFI_NOT_FOUND) {
            #if REFIT_DEBUG > 0
            CheckErrMsg = PoolPrint (L"in ReadHiddenTags:- '%s'", VarName);
            CheckError (Status, CheckErrMsg);
            MY_FREE_POOL(CheckErrMsg);
            #endif
        }
        #endif

        return NULL;
    }

    if (Buffer == NULL || Size == 0 || StrLen (Buffer) == 0) {
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
} // CHAR16* ReadHiddenTags()

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
} // VOID AddToHiddenTags()

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

        DefaultEntry = 9999; // Use the Max Index
        Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
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
} // BOOLEAN HideEfiTag()

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
    Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
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
        (BaseCheck)
            ? LegacyLoader->me.Title
            : LegacyLoader->BdsOption->Description
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
    Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
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
} // BOOLEAN HideLegacyTag()

static
VOID HideTag (
    REFIT_MENU_ENTRY *ChosenOption
) {
    CHAR16            *NoChanges;
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

    HideTagMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_HIDDEN);
    HideTagMenu->Hint1      = StrDuplicate (SELECT_OPTION_HINT     );
    HideTagMenu->Hint2      = StrDuplicate (RETURN_MAIN_SCREEN_HINT);

    TagFlag      = 0;
    NoChanges    = L"No Changes Made on Hide Entry Call";
    Loader       = (LOADER_ENTRY *) ChosenOption;
    LegacyLoader = (LEGACY_ENTRY *) ChosenOption;

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
    //           - https://github.com/dakanji/RefindPlus/issues/163
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
                if (HideEfiTag (Loader, HideTagMenu, L"HiddenTags")) {
                    // Changes Triggered
                    TagFlag = 1;
                }
                else {
                    // No Changes Triggered
                    TagFlag = 2;
                }
            }

            break;
        case TAG_LEGACY:
        case TAG_LEGACY_UEFI:
            HideTagMenu->Title = L"Hide Legacy BIOS Entry";
            if (HideLegacyTag (LegacyLoader, HideTagMenu)) {
                // Changes Triggered
                TagFlag = 1;
            }
            else {
                // No Changes Triggered
                TagFlag = 2;
            }

            break;
        case TAG_FIRMWARE_LOADER:
            HideTagMenu->Title = L"Hide Firmware BootOption Entry";
            if (HideFirmwareTag(Loader, HideTagMenu)) {
                // Changes Triggered
                TagFlag = 1;
            }
            else {
                // No Changes Triggered
                TagFlag = 2;
            }

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
            if (HideEfiTag (Loader, HideTagMenu, L"HiddenTools")) {
                // Changes Triggered
                TagFlag = 1;
            }
            else {
                // No Changes Triggered
                TagFlag = 2;
            }
    } // switch

    if (TagFlag == 1) {
        #if REFIT_DEBUG > 0
        LOG_MSG("Received User Input:");
        LOG_MSG("%s  - %s", OffsetNext, HideTagMenu->Title);
        LOG_MSG("\n\n");
        #endif

        RescanAll (FALSE);
    }
    else if (TagFlag == 2) {
        #if REFIT_DEBUG > 0
        LOG_MSG("INFO: %s:- '%s'", NoChanges, HideTagMenu->Title);
        LOG_MSG("\n\n");
        ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", NoChanges);
        #endif
    }

    FreeMenuScreen (&HideTagMenu);
} // VOID HideTag()

// Present a menu for the user to confirm Bluetooth Sync
UINTN AbortSyncTrust (VOID) {
    INTN               DefaultEntry;
    UINTN              MenuExit;
    BOOLEAN            RetVal;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenOption;
    REFIT_MENU_SCREEN *AbortSyncTrustMenu;


    AbortSyncTrustMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
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

    RetVal = GetMenuEntryYesNo (&AbortSyncTrustMenu);
    if (!RetVal) {
        FreeMenuScreen (&AbortSyncTrustMenu);

        // Early Return
        return SYNC_TRUST_HALT;
    }

    DefaultEntry = 9999; // Use the Max Index
    Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
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


    ConfirmSyncNVramMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
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

    RetVal = GetMenuEntryYesNo (&ConfirmSyncNVramMenu);
    if (!RetVal) {
        FreeMenuScreen (&ConfirmSyncNVramMenu);

        // Early Return
        return FALSE;
    }

    DefaultEntry = 9999; // Use the Max Index
    Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
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

    ConfirmRotateMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (ConfirmRotateMenu == NULL) {
        // Resource Exhaustion ... Early Exit
        return TRUE;
    }

    /* coverity[check_return: SUPPRESS] */
    GetCsrStatus (&CurrentCsr);
    RecordgCsrStatus (CurrentCsr, FALSE);
    TmpStrA = PoolPrint (L"From : %s", gCsrStatus);
    EmptySIP = (CurrentCsr == SIP_ENABLED_EX) ? TRUE : FALSE;

    ListItem = GlobalConfig.CsrValues;
    if (EmptySIP) {
        // Store first config CsrValue when SIP is not set (for later use)
        TempCsr = GlobalConfig.CsrValues->Value;
    }
    else {
        while ((ListItem != NULL) && (ListItem->Value != CurrentCsr)) {
            ListItem = ListItem->Next;
        } // while
    }
    TargetCsr = (ListItem == NULL || ListItem->Next == NULL)
        ? GlobalConfig.CsrValues->Value
        : ListItem->Next->Value;

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

    RetVal = GetMenuEntryYesNo (&ConfirmRotateMenu);
    if (!RetVal) {
        FreeMenuScreen (&ConfirmRotateMenu);

        // Early Return
        return FALSE;
    }

    DefaultEntry = 9999; // Use the Max Index
    Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
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

// Present a menu for the user to confirm restart
BOOLEAN ConfirmRestart (VOID) {
    INTN               DefaultEntry;
    UINTN              MenuExit;
    BOOLEAN            RetVal;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenOption;
    REFIT_MENU_SCREEN *ConfirmRestartMenu;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Prepare Menu Screen");
    ALT_LOG(1, LOG_LINE_NORMAL, L"Screen Title:- 'Confirm System Restart'");
    #endif

    ConfirmRestartMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (ConfirmRestartMenu == NULL) {
        // Resource Exhaustion ... Execute Restart Immediately
        TerminateScreen();
        REFIT_CALL_4_WRAPPER(
            gRT->ResetSystem, EfiResetCold,
            EFI_SUCCESS, 0, NULL
        );

        // Useless return for Coverity
        return TRUE;
    }

    // Build the menu page
    ConfirmRestartMenu->Title      = StrDuplicate (L"Confirm System Restart");
    ConfirmRestartMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_RESET   );
    ConfirmRestartMenu->Hint1      = StrDuplicate (SELECT_OPTION_HINT       );
    ConfirmRestartMenu->Hint2      = StrDuplicate (RETURN_MAIN_SCREEN_HINT  );

    RetVal = GetMenuEntryYesNo (&ConfirmRestartMenu);
    if (!RetVal) {
        FreeMenuScreen (&ConfirmRestartMenu);

        // Early Return
        return FALSE;
    }

    DefaultEntry = 9999; // Use the Max Index
    Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
    MenuExit = DrawMenuScreen (
        ConfirmRestartMenu, Style,
        &DefaultEntry, &ChosenOption
    );

    #if REFIT_DEBUG > 0
    LogExit (MenuExit, __func__, ChosenOption->Title);
    #endif

    if (MenuExit == MENU_EXIT_ENTER &&
        MyStriCmp (ChosenOption->Title, L"Yes")
    ) {
        RetVal = TRUE;
    }
    else {
        RetVal = FALSE;
    }

    FreeMenuScreen (&ConfirmRestartMenu);

    return RetVal;
} // BOOLEAN ConfirmRestart()

// Present a menu for the user to confirm shutdown
BOOLEAN ConfirmShutdown (VOID) {
    INTN               DefaultEntry;
    UINTN              MenuExit;
    BOOLEAN            RetVal;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenOption;
    REFIT_MENU_SCREEN *ConfirmShutdownMenu;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Prepare Menu Screen");
    ALT_LOG(1, LOG_LINE_NORMAL, L"Screen Title:- 'Confirm System Shutdown'");
    #endif

    ConfirmShutdownMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (ConfirmShutdownMenu == NULL) {
        // Resource Exhaustion ... Execute Shutdown Immediately
        TerminateScreen();
        REFIT_CALL_4_WRAPPER(
            gRT->ResetSystem, EfiResetShutdown,
            EFI_SUCCESS, 0, NULL
        );

        // Useless return for Coverity
        return TRUE;
    }

    // Build the menu page
    ConfirmShutdownMenu->Title      = StrDuplicate (L"Confirm System Shutdown");
    ConfirmShutdownMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_SHUTDOWN );
    ConfirmShutdownMenu->Hint1      = StrDuplicate (SELECT_OPTION_HINT        );
    ConfirmShutdownMenu->Hint2      = StrDuplicate (RETURN_MAIN_SCREEN_HINT   );

    RetVal = GetMenuEntryYesNo (&ConfirmShutdownMenu);
    if (!RetVal) {
        FreeMenuScreen (&ConfirmShutdownMenu);

        // Early Return
        return FALSE;
    }

    DefaultEntry = 9999; // Use the Max Index
    Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
    MenuExit = DrawMenuScreen (
        ConfirmShutdownMenu, Style,
        &DefaultEntry, &ChosenOption
    );

    #if REFIT_DEBUG > 0
    LogExit (MenuExit, __func__, ChosenOption->Title);
    #endif

    if (MenuExit == MENU_EXIT_ENTER &&
        MyStriCmp (ChosenOption->Title, L"Yes")
    ) {
        RetVal = TRUE;
    }
    else {
        RetVal = FALSE;
    }

    FreeMenuScreen (&ConfirmShutdownMenu);

    return RetVal;
} // BOOLEAN ConfirmShutdown()

UINTN RunMainMenu (
    REFIT_MENU_SCREEN  *Screen,
    CHAR16            **DefaultSelection,
    REFIT_MENU_ENTRY  **ChosenOption
) {
    REFIT_MENU_ENTRY   *TempChosenOption;
    MENU_STYLE_FUNC     MainStyle;
    MENU_STYLE_FUNC     Style;
    BOOLEAN             KeyStrokeFound;
    UINTN               MenuExit;
    INTN                DefaultEntryIndex;
    INTN                DefaultSubmenuIndex;

    #if REFIT_DEBUG > 0
    CHAR16             *MsgStr;
    UINTN               EntryPosition;

    static BOOLEAN      ShowLoaded = TRUE;
    #endif


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    TileSizes[0] = (GlobalConfig.IconSizes[ICON_SIZE_BIG]   * 9) / 8;
    TileSizes[1] = (GlobalConfig.IconSizes[ICON_SIZE_SMALL] * 4) / 3;

    BREAD_CRUMB(L"%a:  2", __func__);
    DefaultEntryIndex = (DefaultSelection != NULL && *DefaultSelection != NULL)
        ? FindMenuShortcutEntry (Screen, *DefaultSelection) : -1;

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
        MsgStr = (DefaultSelection != NULL && *DefaultSelection != NULL)
            ? PoolPrint (L"Configured Default Loader:- '%s'", *DefaultSelection)
            : StrDuplicate (L"Configured Default Loader:- 'NULL'");
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("\n");
        LOG_MSG("%s", MsgStr);
        MY_FREE_POOL(MsgStr);

        BREAD_CRUMB(L"%a:  3a 3", __func__);
        if (!GlobalConfig.DirectBoot) {
            BREAD_CRUMB(L"%a:  3a 3a 1", __func__);
            EntryPosition = (DefaultEntryIndex < 0) ? 0 : DefaultEntryIndex;
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

    // Remove any buffered key strokes
    BREAD_CRUMB(L"%a:  4", __func__);
    KeyStrokeFound = ReadAllKeyStrokes();

    BREAD_CRUMB(L"%a:  5", __func__);
    if (!KeyStrokeFound || !AppleFirmware) {
        BREAD_CRUMB(L"%a:  5a 1", __func__);
        if (!AppleFirmware) {
            // Always reset the buffer on UEFI PC
            BREAD_CRUMB(L"%a:  5a 1a 1", __func__);
            REFIT_CALL_2_WRAPPER(gST->ConIn->Reset, gST->ConIn, FALSE);
        }
        BREAD_CRUMB(L"%a:  5a 2", __func__);
    }

    BREAD_CRUMB(L"%a:  6", __func__);
    if (!AllowGraphicsMode) {
        BREAD_CRUMB(L"%a:  6a 1", __func__);
        MainStyle = Style = TextMenuStyle;
    }
    else {
        BREAD_CRUMB(L"%a:  6b 1", __func__);
        Style          = GraphicsMenuStyle;
        MainStyle      = MainMenuStyle;
        PointerEnabled = PointerActive = pdAvailable();
        DrawSelection  = !PointerEnabled;
    }

    // Generate WaitList if not already generated.
    BREAD_CRUMB(L"%a:  7 - GenerateWaitList", __func__);
    GenerateWaitList();

    // Save time elaspsed from start til now
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
        if (MenuExit == MENU_EXIT_DETAILS) {
            BREAD_CRUMB(L"%a:  9a 3a 1", __func__);
            if (TempChosenOption->SubScreen == NULL) {
                BREAD_CRUMB(L"%a:  9a 3a 1a 1", __func__);
                // No sub-screen ... Ignore keypress
                MenuExit = MENU_EXIT_ZERO;
            }
            else {
                SubScreenBoot = TRUE;

                BREAD_CRUMB(L"%a:  9a 3a 1b 1", __func__);
                DefaultSubmenuIndex = -1;
                MenuExit = DrawMenuScreen (
                    TempChosenOption->SubScreen,
                    Style,
                    &DefaultSubmenuIndex,
                    &TempChosenOption
                );

                BREAD_CRUMB(L"%a:  9a 3a 1b 2", __func__);
                #if REFIT_DEBUG > 0
                ALT_LOG(1, LOG_LINE_NORMAL,
                    L"Returned '%d' (%s) in '%a' Function from DrawMenuScreen Call on SubScreen",
                    MenuExit, MenuExitInfo (MenuExit), __func__
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
        if (GlobalConfig.EnableTouch && MenuExit == MENU_EXIT_ZERO) {
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
    // No need to check "*DefaultSelection" below
    if (DefaultSelection != NULL) {
        BREAD_CRUMB(L"%a:  12a 1", __func__);
        MY_FREE_POOL(*DefaultSelection);
        *DefaultSelection = StrDuplicate (TempChosenOption->Title);
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
    UINTN i, j;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    if (Screen == NULL || *Screen == NULL) {
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
        j = 0;
        for (i = 0; i < (*Screen)->InfoLineCount; i++) {
            j++;
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
        j = 0;
        for (i = 0; i < (*Screen)->EntryCount; i++) {
            j++;
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
} // VOID FreeLegacyEntry()

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
} // VOID FreeLoaderEntry()

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
        case TAG_TOOL:            TagType = L"TAG_TOOL"           ; break;
        case TAG_LOADER:          TagType = L"TAG_LOADER"         ; break;
        case TAG_LEGACY:          TagType = L"TAG_LEGACY"         ; break;
        case TAG_LEGACY_UEFI:     TagType = L"TAG_LEGACY_UEFI"    ; break;
        case TAG_RESET_NVRAM:     TagType = L"TAG_RESET_NVRAM"    ; break;
        case TAG_FIRMWARE_LOADER: TagType = L"TAG_FIRMWARE_LOADER"; break;
        default:                  TagType = L"DEFAULT"            ; break;
    }
    #endif

    BREAD_CRUMB(L"%a:  3", __func__);
    if (EntryType == EntryTypeLoaderEntry) {
        BREAD_CRUMB(L"%a:  3a 1 - EntryType = EntryTypeLoaderEntry ... TagType = '%s'", __func__, TagType);
        FreeLoaderEntry ((LOADER_ENTRY **) Entry);
    }
    else if (EntryType == EntryTypeLegacyEntry) {
        BREAD_CRUMB(L"%a:  3b 1 - EntryType = EntryTypeLegacyEntry ... TagType = '%s'", __func__, TagType);
        FreeLegacyEntry ((LEGACY_ENTRY **) Entry);
    }
    else {
        BREAD_CRUMB(L"%a:  3c 1 - EntryType = EntryTypeRefitMenuEntry ... TagType = '%s'", __func__, TagType);
        MY_FREE_POOL((*Entry)->Title);
        MY_FREE_IMAGE((*Entry)->Image);
        MY_FREE_IMAGE((*Entry)->BadgeImage);

        BREAD_CRUMB(L"%a:  3c 2", __func__);
        FreeMenuScreen (&(*Entry)->SubScreen);
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

    NewBdsOption = AllocateCopyPool (sizeof (*BdsOption), BdsOption);
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

    MenuEntryReturn = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
    if (MenuEntryReturn == NULL) {
        // Early Return
        return FALSE;
    }

    MenuEntryReturn->Title = StrDuplicate (L"Return to Main Menu");
    MenuEntryReturn->Tag   = TAG_RETURN;
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

    MenuEntryYes = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
    if (MenuEntryYes == NULL) {
        // Early Return
        return FALSE;
    }

    MenuEntryYes->Title = StrDuplicate (L"Yes");
    MenuEntryYes->Tag   = TAG_YES;
    AddMenuEntry (*Screen, MenuEntryYes);

    MenuEntryNo = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
    if (MenuEntryNo == NULL) {
        FreeMenuEntry ((REFIT_MENU_ENTRY **) MenuEntryYes);

        // Early Return
        return FALSE;
    }

    MenuEntryNo->Title = StrDuplicate (L"No");
    MenuEntryNo->Tag   = TAG_NO;
    AddMenuEntry (*Screen, MenuEntryNo);

    return TRUE;
} // BOOLEAN GetMenuEntryYesNo()
