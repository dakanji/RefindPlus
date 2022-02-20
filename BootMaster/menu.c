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
 * Modifications copyright (c) 2012-2020 Roderick W. Smith
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

static CHAR16 ArrowUp[2]   = { ARROW_UP, 0 };
static CHAR16 ArrowDown[2] = { ARROW_DOWN, 0 };
static UINTN  TileSizes[2] = { 144, 64 };

// Text and icon spacing constants.
#define TEXT_YMARGIN       (2)
#define TITLEICON_SPACING (16)

#define TILE_XSPACING      (8)
#define TILE_YSPACING     (16)

// Alignment values for PaintIcon()
#define ALIGN_RIGHT 1
#define ALIGN_LEFT  0

EG_IMAGE *SelectionImages[2]       = { NULL, NULL };

EFI_EVENT *WaitList       = NULL;
UINT64     MainMenuLoad   = 0;
UINTN      WaitListLength = 0;

// Pointer variables
BOOLEAN PointerEnabled    = FALSE;
BOOLEAN PointerActive     = FALSE;
BOOLEAN DrawSelection     = TRUE;

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
extern BOOLEAN             FlushFailedTag;
extern BOOLEAN             FlushFailReset;
extern BOOLEAN             ClearedBuffer;
extern BOOLEAN             BlockRescan;
extern BOOLEAN             SingleAPFS;
extern EFI_GUID            RefindPlusGuid;


//
// Graphics helper functions
//

static
VOID InitSelection (VOID) {
    EG_IMAGE  *TempSmallImage    = NULL;
    EG_IMAGE  *TempBigImage      = NULL;
    BOOLEAN    LoadedSmallImage  = FALSE;

    if (!AllowGraphicsMode || SelectionImages[0] != NULL || SelectionImages[1] != NULL) {
        // Early Return ... Already Run Once
        return;
    }

    // Load small selection image
    if (GlobalConfig.SelectionSmallFileName != NULL) {
        TempSmallImage = egLoadImage (SelfDir, GlobalConfig.SelectionSmallFileName, TRUE);

        // DA-TAG: Impose maximum size for security
        if (TempSmallImage->Width > 96 || TempSmallImage->Height > 96) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"Discarding Input Small Selection Image ... Too Large");
            #endif

            MY_FREE_IMAGE(TempSmallImage);
        }
    }

    if (TempSmallImage == NULL) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Using Embedded Selection Image:- 'egemb_back_selected_small'");
        #endif

        TempSmallImage = egPrepareEmbeddedImage (&egemb_back_selected_small, TRUE, NULL);
    }
    else {
        LoadedSmallImage = TRUE;
    }

    if ((TempSmallImage->Width != TileSizes[1]) || (TempSmallImage->Height != TileSizes[1])) {
        SelectionImages[1] = egScaleImage (TempSmallImage, TileSizes[1], TileSizes[1]);
    }
    else {
        SelectionImages[1] = egCopyImage (TempSmallImage);
    }

    // Load big selection image
    if (GlobalConfig.SelectionBigFileName != NULL) {
        TempBigImage = egLoadImage (SelfDir, GlobalConfig.SelectionBigFileName, TRUE);

        // DA-TAG: Impose maximum size for security
        if (TempBigImage->Width > 216 || TempBigImage->Height > 216) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"Discarding Input Big Selection Image ... Too Large");
            #endif

            MY_FREE_IMAGE(TempBigImage);
        }
    }

    if (TempBigImage == NULL) {
        if (LoadedSmallImage) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL, L"Scaling Selection Image from LoadedSmallImage");
            #endif

           // Derive the big selection image from the small one
           TempBigImage = egCopyImage (TempSmallImage);
        }
        else {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL, L"Using Embedded Selection Image:- 'egemb_back_selected_big'");
            #endif

           TempBigImage = egPrepareEmbeddedImage (&egemb_back_selected_big, TRUE, NULL);
        }
    }

    if ((TempBigImage->Width != TileSizes[0]) || (TempBigImage->Height != TileSizes[0])) {
        SelectionImages[0] = egScaleImage (TempBigImage, TileSizes[0], TileSizes[0]);
    }
    else {
        SelectionImages[0] = egCopyImage (TempBigImage);
    }

    MY_FREE_IMAGE(TempSmallImage);
    MY_FREE_IMAGE(TempBigImage);
} // VOID InitSelection()

//
// Scrolling functions
//

static
VOID InitScroll (
    OUT SCROLL_STATE *State,
    IN UINTN          ItemCount,
    IN UINTN          VisibleSpace
) {
    State->PreviousSelection = State->CurrentSelection = 0;
    State->MaxIndex = (INTN) ItemCount - 1;
    State->FirstVisible = 0;

    if (AllowGraphicsMode) {
        State->MaxVisible = ScreenW / (TileSizes[0] + TILE_XSPACING) - 1;
    }
    else {
        State->MaxVisible = ConHeight - 4;
    }

    if ((VisibleSpace > 0) && (VisibleSpace < State->MaxVisible)) {
        State->MaxVisible = (INTN) VisibleSpace;
    }

    State->PaintAll        = TRUE;
    State->PaintSelection  = FALSE;
    State->LastVisible     = State->FirstVisible + State->MaxVisible - 1;
}

// Adjust variables relating to the scrolling of tags, for when a selected icon
// is not visible given the current scrolling condition.
static
VOID AdjustScrollState (
    IN SCROLL_STATE *State
) {
    // Scroll forward
    if (State->CurrentSelection > State->LastVisible) {
        State->LastVisible   = State->CurrentSelection;
        State->FirstVisible  = 1 + State->CurrentSelection - State->MaxVisible;

        if (State->FirstVisible < 0) {
            // should not happen, but just in case.
            State->FirstVisible = 0;
        }

        State->PaintAll = TRUE;
    }

    // Scroll backward
    if (State->CurrentSelection < State->FirstVisible) {
        State->FirstVisible  = State->CurrentSelection;
        State->LastVisible   = State->CurrentSelection + State->MaxVisible - 1;
        State->PaintAll      = TRUE;
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
                State->CurrentSelection --;
            }

        break;
        case SCROLL_LINE_RIGHT:
            if (State->CurrentSelection < State->MaxIndex) {
                State->CurrentSelection ++;
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
                State->PaintAll = TRUE;
                State->CurrentSelection = 0;
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
                State->PaintAll = TRUE;
                State->CurrentSelection = State->MaxIndex;
            }

            break;
    } // switch

    if (State->ScrollMode == SCROLL_MODE_TEXT) {
        AdjustScrollState (State);
    }

    if (!State->PaintAll && State->CurrentSelection != State->PreviousSelection) {
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
    CHAR16 *MenuExitData = NULL;

    switch (MenuExit) {
        case 1:  MenuExitData = L"ENTER";   break;
        case 2:  MenuExitData = L"ESCAPE";  break;
        case 3:  MenuExitData = L"DETAILS"; break;
        case 4:  MenuExitData = L"TIMEOUT"; break;
        case 5:  MenuExitData = L"EJECT";   break;
        case 6:  MenuExitData = L"REMOVE";  break;
        default: MenuExitData = L"RETURN";  // Actually '99'
    } // switch

    return MenuExitData;
} // CHAR16 * MenuExitInfo()

VOID AddMenuInfoLineAlt (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16            *InfoLine
) {
    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Adding Menu Info Line:- '%s'", InfoLine);
    #endif

    AddListElement (
        (VOID ***) &(Screen->InfoLines),
        &(Screen->InfoLineCount),
        InfoLine
    );
} // VOID AddMenuInfoLineAlt()

VOID AddMenuInfoLine (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16            *InfoLine
) {
    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Adding Menu Info Line:- '%s'", InfoLine);
    #endif

    AddListElement (
        (VOID ***) &(Screen->InfoLines),
        &(Screen->InfoLineCount),
        StrDuplicate (InfoLine)
    );
} // VOID AddMenuInfoLine()

VOID AddMenuEntry (
    IN REFIT_MENU_SCREEN *Screen,
    IN REFIT_MENU_ENTRY  *Entry
) {
    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Adding Menu Entry to %s - %s",
        Screen->Title,
        Entry->Title
    );
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
    AddMenuEntry (Screen, CopyMenuEntry (Entry));
} // VOID AddMenuEntryCopy()

INTN FindMenuShortcutEntry (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16            *Defaults
) {
    UINTN   i, j = 0, ShortcutLength;
    CHAR16 *Shortcut;

    while ((Shortcut = FindCommaDelimited (Defaults, j)) != NULL) {
        ShortcutLength = StrLen (Shortcut);
        if (ShortcutLength == 1) {
            if (Shortcut[0] >= 'a' && Shortcut[0] <= 'z') {
                Shortcut[0] -= ('a' - 'A');
            }

            if (Shortcut[0]) {
                for (i = 0; i < Screen->EntryCount; i++) {
                    if (Screen->Entries[i]->ShortcutDigit  == Shortcut[0] ||
                        Screen->Entries[i]->ShortcutLetter == Shortcut[0]
                    ) {
                        MY_FREE_POOL(Shortcut);

                        return i;
                    }
                } // for
            }
        }
        else if (ShortcutLength > 1) {
            for (i = 0; i < Screen->EntryCount; i++) {
                if (StriSubCmp (Shortcut, Screen->Entries[i]->Title)) {
                    MY_FREE_POOL(Shortcut);

                    return i;
                }
            } // for
        }

        MY_FREE_POOL(Shortcut);
        j++;
    } // while

    return -1;
} // static INTN FindMenuShortcutEntry()

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
        else if ((Screen->Entries[i]->Row == 1) && (State->InitialRow1 > i)) {
            State->InitialRow1 = i;
        }
    } // for

    if ((State->ScrollMode == SCROLL_MODE_ICONS) &&
        (State->MaxVisible > (State->FinalRow0 + 1))
    ) {
        State->MaxVisible = State->FinalRow0 + 1;
    }
} // static VOID IdentifyRows()

// Blank the screen, wait for a keypress or pointer event, and restore banner/background.
// Screen may still require redrawing of text and icons on return.
// TODO: Support more sophisticated screen savers, such as power-saving
// mode and dynamic images.
static
VOID SaveScreen (VOID) {
    UINTN  retval;
    UINTN  ColourIndex;
    UINT64 TimeWait;
    UINT64 BaseTimeWait = 3750;

    #if REFIT_DEBUG > 0
    CHAR16 *LoopChange = NULL;
    CHAR16 *MsgStr = L"Activity Wait Threshold Exceeded ... Start Screensaver";
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    ALT_LOG(1, LOG_LINE_NORMAL,  L"%s", MsgStr);
    LOG_MSG("INFO: %s", MsgStr);

    MsgStr = L"Running Screensaver";
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

    // Start with COLOUR_01
    ColourIndex = 0;

    // Start with BaseTimeWait
    TimeWait = BaseTimeWait;
    for (;;) {
        ColourIndex = ColourIndex + 1;

        if (ColourIndex < 1 || ColourIndex > 30) {
            ColourIndex = 1;
            TimeWait    = TimeWait * 2;

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
            ALT_LOG(1, LOG_LINE_NORMAL, L"%d Timeout Loop - %d", LoopChange, TimeWait);
            ALT_LOG(1, LOG_THREE_STAR_MID,  L"Running Screensaver");
            #endif
        }

        switch (ColourIndex) {
            case 1:  OUR_COLOUR = COLOUR_01; break;
            case 2:  OUR_COLOUR = COLOUR_02; break;
            case 3:  OUR_COLOUR = COLOUR_03; break;
            case 4:  OUR_COLOUR = COLOUR_04; break;
            case 5:  OUR_COLOUR = COLOUR_05; break;
            case 6:  OUR_COLOUR = COLOUR_06; break;
            case 7:  OUR_COLOUR = COLOUR_07; break;
            case 8:  OUR_COLOUR = COLOUR_08; break;
            case 9:  OUR_COLOUR = COLOUR_09; break;
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

        egClearScreen (&OUR_COLOUR);
        retval = WaitForInput (TimeWait);
        if (retval == INPUT_KEY || retval == INPUT_TIMER_ERROR) {
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
    CHAR16 *retval = NULL;

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
        case SCAN_F10:       retval = L"F10-ScrnSht";   break; // Using 'ScrnSht' to limit length
        case 0x0016:         retval = L"F12-Eject";     break;
        default:             retval = L"KEY_UNKNOWN";   break;
    } // switch

    return retval;
} // static CHAR16 * GetScanCodeText()
#endif

UINTN RunGenericMenu (
    IN     REFIT_MENU_SCREEN   *Screen,
    IN     MENU_STYLE_FUNC      StyleFunc,
    IN OUT INTN                *DefaultEntryIndex,
    OUT    REFIT_MENU_ENTRY  **ChosenEntry
) {
    EFI_STATUS     Status;
    EFI_STATUS     PointerStatus      =  EFI_NOT_READY;
    BOOLEAN        HaveTimeout        =  FALSE;
    BOOLEAN        WaitForRelease     =  FALSE;
    UINTN          TimeoutCountdown   =  0;
    INTN           TimeSinceKeystroke =  0;
    INTN           PreviousTime       = -1;
    INTN           CurrentTime;
    INTN           ShortcutEntry;
    UINTN          ElapsCount;
    UINTN          MenuExit = 0;
    UINTN          Input;
    UINTN          Item;
    CHAR16        *TimeoutMessage;
    CHAR16         KeyAsString[2];
    SCROLL_STATE   State;
    EFI_INPUT_KEY  key;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"Entering RunGenericMenu");
    ALT_LOG(1, LOG_LINE_NORMAL, L"Running Menu Screen:- '%s'", Screen->Title);
    #endif

    if (Screen->TimeoutSeconds > 0) {
        HaveTimeout      = TRUE;
        TimeoutCountdown = Screen->TimeoutSeconds * 10;
    }

    StyleFunc (Screen, &State, MENU_FUNCTION_INIT, NULL);
    IdentifyRows (&State, Screen);

    // Override the starting selection with the default index, if any
    if (*DefaultEntryIndex >= 0 && *DefaultEntryIndex <= State.MaxIndex) {
        State.CurrentSelection = *DefaultEntryIndex;
        if (GlobalConfig.ScreensaverTime != -1) {
            UpdateScroll (&State, SCROLL_NONE);
        }
    }

    if (Screen->TimeoutSeconds == -1) {
        Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
        if (Status == EFI_NOT_READY) {
            MenuExit = MENU_EXIT_TIMEOUT;
        }
        else {
            KeyAsString[0] = key.UnicodeChar;
            KeyAsString[1] = 0;
            ShortcutEntry  = FindMenuShortcutEntry (Screen, KeyAsString);

            if (ShortcutEntry >= 0) {
                State.CurrentSelection = ShortcutEntry;
                MenuExit = MENU_EXIT_ENTER;
            }
            else {
                WaitForRelease = TRUE;
                HaveTimeout    = FALSE;
            }
        }
    }

    if (GlobalConfig.DirectBoot && WaitForRelease) {
        // DA-TAG: If we enter here, a shortcut key was pressed but not found
        //         Load the screen menu ... Without tools for Main Menu
        //         Tools are not loaded with DirectBoot for speed
        //         Enable Rescan to allow tools to be loaded
        //         Also disable Timeout just in case
        BlockRescan = FALSE;
        Screen->TimeoutSeconds = 0;
        DrawScreenHeader (Screen->Title);
    }

    if (GlobalConfig.ScreensaverTime != -1) {
        State.PaintAll = TRUE;
    }

    BOOLEAN Toggled = FALSE;
    while (MenuExit == 0) {
        // Update the screen
        pdClear();
        if (State.PaintAll && (GlobalConfig.ScreensaverTime != -1)) {
            StyleFunc (Screen, &State, MENU_FUNCTION_PAINT_ALL, NULL);
            State.PaintAll = FALSE;
        }
        else if (State.PaintSelection) {
            StyleFunc (Screen, &State, MENU_FUNCTION_PAINT_SELECTION, NULL);
            State.PaintSelection = FALSE;
        }
        pdDraw();

        if (WaitForRelease) {
            Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
            if (Status == EFI_SUCCESS) {
                // Reset to keep the keystroke buffer clear
                REFIT_CALL_2_WRAPPER(gST->ConIn->Reset, gST->ConIn, FALSE);
                REFIT_CALL_1_WRAPPER(gBS->Stall, 100000);
            }
            else {
                WaitForRelease = FALSE;
                REFIT_CALL_2_WRAPPER(gST->ConIn->Reset, gST->ConIn, TRUE);
            }

            continue;
        }

        // DA-TAG: Toggle the selection once to work around failure to
        //         display the default selection on load in text mode.
        //         This is a Workaround ... 'Proper' solution needed.
        if (!Toggled) {
            Toggled = TRUE;
            if (State.ScrollMode == SCROLL_MODE_TEXT) {
                if (State.CurrentSelection < State.MaxIndex) {
                    UpdateScroll (&State, SCROLL_LINE_DOWN);
                    REFIT_CALL_1_WRAPPER(gBS->Stall, 5000);
                    UpdateScroll (&State, SCROLL_LINE_UP);
                }
                else if (State.CurrentSelection > 0) {
                    UpdateScroll (&State, SCROLL_LINE_UP);
                    REFIT_CALL_1_WRAPPER(gBS->Stall, 5000);
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
                   L"%s in %d seconds",
                   Screen->TimeoutText,
                   CurrentTime
               );

               if (GlobalConfig.ScreensaverTime != -1) {
                   StyleFunc (Screen, &State, MENU_FUNCTION_PAINT_TIMEOUT, TimeoutMessage);
               }

               MY_FREE_POOL(TimeoutMessage);

               PreviousTime = CurrentTime;
            }
        }

        // Read keypress or pointer event (and wait for them if applicable)
        if (PointerEnabled) {
            PointerStatus = pdUpdateState();
        }
        Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);

        if (Status == EFI_SUCCESS) {
            PointerActive      = FALSE;
            DrawSelection      = TRUE;
            TimeSinceKeystroke = 0;
        }
        else if (PointerStatus == EFI_SUCCESS) {
            if (StyleFunc != MainMenuStyle && pdGetState().Press) {
                // Prevent user from getting stuck on submenus
                // (Only the 'About' screen is currently reachable without a keyboard)
                MenuExit = MENU_EXIT_ENTER;
                break;
            }

            PointerActive      = TRUE;
            TimeSinceKeystroke = 0;
        }
        else {
            if (HaveTimeout && TimeoutCountdown == 0) {
                // Timeout expired
                #if REFIT_DEBUG > 0
                ALT_LOG(1, LOG_LINE_NORMAL, L"Menu Timeout Expired:- '%d Seconds'", Screen->TimeoutSeconds);
                #endif

                MenuExit = MENU_EXIT_TIMEOUT;
                break;
            }
            else if (HaveTimeout || GlobalConfig.ScreensaverTime > 0) {
                ElapsCount = 1;
                Input      = WaitForInput (1000); // 1s Timeout

                if (Input == INPUT_KEY || Input == INPUT_POINTER) {
                    TimeSinceKeystroke = 0;
                    continue;
                }
                else if (Input == INPUT_TIMEOUT) {
                    // Always counted as is to end of the timeout
                    ElapsCount = 10;
                }

                TimeSinceKeystroke += ElapsCount;
                if (HaveTimeout) {
                    TimeoutCountdown = (TimeoutCountdown > ElapsCount)
                        ? TimeoutCountdown - ElapsCount : 0;
                }
                else if (GlobalConfig.ScreensaverTime > 0 &&
                    TimeSinceKeystroke > (GlobalConfig.ScreensaverTime * 10)
                ) {
                    SaveScreen();
                    State.PaintAll     = TRUE;
                    TimeSinceKeystroke = 0;
                }
            }
            else {
                WaitForInput (0);
            } // if/else HaveTimeout

            continue;
        } // if/else Status == EFI_SUCCESS

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
                case SCAN_END:       UpdateScroll (&State, SCROLL_LAST);       break;
                case SCAN_HOME:      UpdateScroll (&State, SCROLL_FIRST);      break;
                case SCAN_PAGE_UP:   UpdateScroll (&State, SCROLL_PAGE_UP);    break;
                case SCAN_PAGE_DOWN: UpdateScroll (&State, SCROLL_PAGE_DOWN);  break;
                case SCAN_UP:        UpdateScroll (&State, SCROLL_LINE_UP);    break;
                case SCAN_LEFT:      UpdateScroll (&State, SCROLL_LINE_LEFT);  break;
                case SCAN_DOWN:      UpdateScroll (&State, SCROLL_LINE_DOWN);  break;
                case SCAN_RIGHT:     UpdateScroll (&State, SCROLL_LINE_RIGHT); break;
                case SCAN_INSERT:
                case SCAN_F2:        MenuExit = MENU_EXIT_DETAILS;             break;
                case SCAN_F10:       MenuExit = MENU_EXIT_SCREENSHOT;          break;
                case SCAN_ESC:       MenuExit = MENU_EXIT_ESCAPE;              break;
                case SCAN_DELETE:    MenuExit = MENU_EXIT_HIDE;                break;
                case 0x0016: // F12
                    if (EjectMedia()) {
                        MenuExit = MENU_EXIT_ESCAPE;
                    }

                    break;
            } // switch

            switch (key.UnicodeChar) {
                case ' ':
                case CHAR_LINEFEED:
                case CHAR_CARRIAGE_RETURN: MenuExit = MENU_EXIT_ENTER;   break;
                case CHAR_BACKSPACE:       MenuExit = MENU_EXIT_ESCAPE;  break;
                case '+':
                case CHAR_TAB:             MenuExit = MENU_EXIT_DETAILS; break;
                case '-':                  MenuExit = MENU_EXIT_HIDE;    break;
                default:
                    KeyAsString[0] = key.UnicodeChar;
                    KeyAsString[1] = 0;
                    ShortcutEntry  = FindMenuShortcutEntry (Screen, KeyAsString);

                    if (ShortcutEntry >= 0) {
                        State.CurrentSelection = ShortcutEntry;
                        MenuExit = MENU_EXIT_ENTER;
                    }

                    break;
            } // switch

            #if REFIT_DEBUG > 0
            CHAR16 *KeyTxt = GetScanCodeText (key.ScanCode);
            if (MyStriCmp (KeyTxt, L"KEY_UNKNOWN")) {
                switch (key.UnicodeChar) {
                    case ' ':                  KeyTxt = L"INFER_ENTER    Key:SpaceBar";        break;
                    case CHAR_LINEFEED:        KeyTxt = L"INFER_ENTER    Key:LineFeed";        break;
                    case CHAR_CARRIAGE_RETURN: KeyTxt = L"INFER_ENTER    Key:CarriageReturn";  break;
                    case CHAR_BACKSPACE:       KeyTxt = L"INFER_ESCAPE   Key:BackSpace";       break;
                    case CHAR_TAB:             KeyTxt = L"INFER_DETAILS  Key:Tab";             break;
                    case '+':                  KeyTxt = L"INFER_DETAILS  Key:'+'...'Plus'";    break;
                    case '-':                  KeyTxt = L"INFER_REMOVE   Key:'-'...'Minus'";   break;
                } // switch
            }
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"Processing Keystroke: UnicodeChar = 0x%02X ... ScanCode = 0x%02X - %s",
                key.UnicodeChar, key.ScanCode, KeyTxt
            );
            #endif

            if (BlockRescan) {
                if (MenuExit == MENU_EXIT_ESCAPE) {
                    MenuExit = 0;
                }
                else if (MenuExit == 0) {
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
                }
            }

            if (MenuExit == MENU_EXIT_SCREENSHOT) {
                MenuExit = 0;
                egScreenShot();
                State.PaintAll = TRUE;
                WaitForRelease = TRUE;

                // Unblock Rescan and Refresh Screen
                BlockRescan = FALSE;
                continue;
            }
        }
        else {
            // React to pointer event
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL, L"Processing Pointer Event");
            #endif

            if (StyleFunc != MainMenuStyle) {
                // Nothing to find on submenus
                continue;
            }

            State.PreviousSelection = State.CurrentSelection;
            POINTER_STATE PointerState = pdGetState();
            Item = FindMainMenuItem (Screen, &State, PointerState.X, PointerState.Y);

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
                    }

                    if (DrawSelection) {
                        DrawSelection        = FALSE;
                        State.PaintSelection = TRUE;
                    }

                break;
                case POINTER_RIGHT_ARROW:
                    if (PointerState.Press) {
                        UpdateScroll (&State, SCROLL_PAGE_DOWN);
                    }

                    if (DrawSelection) {
                        DrawSelection        = FALSE;
                        State.PaintSelection = TRUE;
                    }

                break;
                default:
                    if (!DrawSelection || Item != State.CurrentSelection) {
                        DrawSelection          = TRUE;
                        State.PaintSelection   = TRUE;
                        State.CurrentSelection = Item;
                    }

                    if (PointerState.Press) {
                        MenuExit = MENU_EXIT_ENTER;
                    }
            } // switch
        } // if/else !PointerActive
    } // while

    pdClear();
    StyleFunc (Screen, &State, MENU_FUNCTION_CLEANUP, NULL);

    // Ignore MenuExit if FlushFailedTag is set and not previously reset
    if (FlushFailedTag && !FlushFailReset) {
        #if REFIT_DEBUG > 0
        CHAR16 *MsgStr = StrDuplicate (L"FlushFailedTag is Set ... Ignore MenuExit");
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
        LOG_MSG("INFO: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        FlushFailedTag = FALSE;
        FlushFailReset = TRUE;
        MenuExit = 0;
    }

    // Ignore MenuExit if time between loading main menu and detecting an 'Enter' keypress is too low
    // Primed Keystroke Buffer appears to only affect UEFI PC
    if (!GlobalConfig.DirectBoot &&
        MenuExit == MENU_EXIT_ENTER &&
        !ClearedBuffer && !FlushFailReset &&
        MyStriCmp (Screen->Title, L"Main Menu")
    ) {
        UINT64 MenuExitTime = GetCurrentMS();
        UINT64 MenuExitDiff = MenuExitTime - MainMenuLoad;
        UINT64 MenuExitNumb = 500;
        UINT64 MenuExitGate = MenuExitNumb;

        if (!AppleFirmware) {
            MenuExitGate = MenuExitGate + MenuExitNumb;

            #if REFIT_DEBUG > 0
            if (GlobalConfig.LogLevel > 0) {
                MenuExitGate = MenuExitGate + MenuExitNumb;
            }
            if (GlobalConfig.LogLevel > 1) {
                MenuExitGate = MenuExitGate + MenuExitNumb;
            }
            #endif
        }

        if (MenuExitDiff < MenuExitGate) {
            #if REFIT_DEBUG > 0
            LOG_MSG("INFO: Invalid Post-Load MenuExit Interval ... Ignoring MenuExit");
            CHAR16 *MsgStr = StrDuplicate (L"Mitigated Potential Persistent Primed Keystroke Buffer");
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
            LOG_MSG("%s      %s", OffsetNext, MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
            #endif

            FlushFailedTag = FALSE;
            FlushFailReset = TRUE;
            MenuExit = 0;
        }
    } // if !GlobalConfig.DirectBoot

    if (ChosenEntry) {
        *ChosenEntry = Screen->Entries[State.CurrentSelection];
    }

    *DefaultEntryIndex = State.CurrentSelection;

    return MenuExit;
} // UINTN RunGenericMenu()

//
// Generic text-mode style
//

// Show information lines in text mode.
static
VOID ShowTextInfoLines (
    IN REFIT_MENU_SCREEN *Screen
) {
    INTN i;

    if (Screen->InfoLineCount < 1) {
        // Early Return
        return;
    }

    BeginTextScreen (Screen->Title);

    REFIT_CALL_2_WRAPPER(
        gST->ConOut->SetAttribute,
        gST->ConOut,
        ATTR_BASIC
    );

    for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
        REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 3, 4 + i);
        REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, Screen->InfoLines[i]);
    }
} // VOID ShowTextInfoLines()

// Do most of the work for text-based menus.
VOID TextMenuStyle (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    IN UINTN              Function,
    IN CHAR16            *ParamText
) {
    INTN  i;
    UINTN MenuWidth;
    UINTN ItemWidth;
    UINTN MenuHeight;

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
                // DA-TAG: TODO - Review the above and possibly change other uses of 'SPrint'
                DisplayStrings[i] = AllocateZeroPool (2 * sizeof (CHAR16));
                DisplayStrings[i][0] = L' ';
                MuteLogger = TRUE;
                MergeStrings (&DisplayStrings[i], Screen->Entries[i]->Title, 0);
                MuteLogger = FALSE;
                if (StrLen (DisplayStrings[i]) > MenuWidth) {
                    DisplayStrings[i][MenuWidth - 1] = 0;
                }
                // DA-TAG: TODO - Improve shortening long strings ... Ellipses in the middle
                // DA-TAG: TODO - Account for double-width characters
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
                        gST->ConOut->SetCursorPosition,
                        gST->ConOut,
                        2,
                        MenuPosY + (i - State->FirstVisible)
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
            REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute,      gST->ConOut, ATTR_SCROLLARROW);
            REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, MenuPosY);

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
                   REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, ConHeight - 2);
                   REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, Screen->Hint1);
               }

               if (Screen->Hint2 != NULL) {
                   REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, ConHeight - 1);
                   REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, Screen->Hint2);
               }
            }

        break;
        case MENU_FUNCTION_PAINT_SELECTION:
            // Redraw selection cursor
            REFIT_CALL_3_WRAPPER(
                gST->ConOut->SetCursorPosition,
                gST->ConOut, 2,
                MenuPosY + (State->PreviousSelection - State->FirstVisible)
            );
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->SetAttribute,
                gST->ConOut,
                ATTR_CHOICE_BASIC
            );
            if (DisplayStrings[State->PreviousSelection] != NULL) {
                REFIT_CALL_2_WRAPPER(
                    gST->ConOut->OutputString,
                    gST->ConOut,
                    DisplayStrings[State->PreviousSelection]
                );
            }
            REFIT_CALL_3_WRAPPER(
                gST->ConOut->SetCursorPosition,
                gST->ConOut, 2,
                MenuPosY + (State->CurrentSelection - State->FirstVisible)
            );
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->SetAttribute,
                gST->ConOut,
                ATTR_CHOICE_CURRENT
            );
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->OutputString,
                gST->ConOut,
                DisplayStrings[State->CurrentSelection]
            );

        break;
        case MENU_FUNCTION_PAINT_TIMEOUT:
            if (ParamText[0] == 0) {
                // Clear message
                if (!BlankLine) PrepareBlankLine();
                REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute,      gST->ConOut, ATTR_BASIC);
                REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, ConHeight - 3);
                REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, BlankLine + 1);
            }
            else {
                // Paint or update message
                REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute,      gST->ConOut, ATTR_ERROR);
                REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 3, ConHeight - 3);
                REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, ParamText);
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

        // draw selection bar background
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
    UINTN FactorFP = 10;
    UINTN Divisor  = 3 * FactorFP;
    UINTN PixelsR  = (UINTN) Bg.r;
    UINTN PixelsG  = (UINTN) Bg.g;
    UINTN PixelsB  = (UINTN) Bg.b;
    UINTN LumIndex = (
        (
            (PixelsR * FactorFP) +
            (PixelsG * FactorFP) +
            (PixelsB * FactorFP) +
            (Divisor / 2) // Added For Rounding
        ) / Divisor
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
    UINTN Sum = 0;

    if (Image == NULL || ((Image->Width * Image->Height) == 0)) {
        // Early Return
        return 0;
    }

    for (i = 0; i < (Image->Width * Image->Height); i++) {
        Sum += (Image->PixelData[i].r + Image->PixelData[i].g + Image->PixelData[i].b);
    }
    Sum /= (Image->Width * Image->Height * 3);

    return (UINT8) Sum;
} // UINT8 AverageBrightness()

// Display text against the screen's background image. Special case: If Text is NULL
// or 0-length, clear the line. Does NOT indent the text or reposition it relative
// to the specified XPos and YPos values.
static
VOID DrawTextWithTransparency (
    IN CHAR16 *Text,
    IN UINTN   XPos,
    IN UINTN   YPos
) {
    UINTN TextWidth;
    EG_IMAGE *TextBuffer = NULL;

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

// Compute the size and position of the window that will hold a subscreen's information.
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
    UINTN i, ItemWidth, HintTop, BannerBottomEdge, TitleWidth;
    UINTN FontCellWidth  = egGetFontCellWidth();
    UINTN FontCellHeight = egGetFontHeight();

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

    *Width = (*Width + 2) * FontCellWidth;
    *LineWidth = *Width;

    if (Screen->TitleImage) {
        *Width += (Screen->TitleImage->Width + TITLEICON_SPACING * 2 + FontCellWidth);
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

    // top of hint text
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
        // DA-TAG: TODO - Port downstream scrolling feature.
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

    static UINTN LineWidth, MenuWidth, MenuHeight;
    static UINTN EntriesPosX, EntriesPosY;
    static UINTN TitlePosX, TimeoutPosY, CharWidth;

    CharWidth = egGetFontCellWidth();
    State->ScrollMode = SCROLL_MODE_TEXT;

    switch (Function) {
        case MENU_FUNCTION_INIT:
            InitScroll (State, Screen->EntryCount, 0);
            ComputeSubScreenWindowSize (
                Screen, State,
                &EntriesPosX, &EntriesPosY,
                &MenuWidth, &MenuHeight,
                &LineWidth
            );

            TimeoutPosY = EntriesPosY + (Screen->EntryCount + 1) * TextLineHeight();

            // Initial painting
            SwitchToGraphicsAndClear (TRUE);

            Window = egCreateFilledImage (MenuWidth, MenuHeight, FALSE, BackgroundPixel);

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

        case MENU_FUNCTION_CLEANUP:
            // Nothing to do
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
                for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
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
// graphical main menu style
//

static
VOID DrawMainMenuEntry (
    REFIT_MENU_ENTRY *Entry,
    BOOLEAN           selected,
    UINTN             XPos,
    UINTN             YPos
) {
    EG_IMAGE *Background;

    // If using pointer, do not draw selection image when not hovering
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
        (!PointerActive || (PointerActive && DrawSelection))
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

// Move the selection to State->CurrentSelection, adjusting icon row if necessary...
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
        (!PointerActive || (PointerActive && DrawSelection))
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
    EG_IMAGE *Icon = egFindIcon (ExternalFilename, GlobalConfig.IconSizes[ICON_SIZE_SMALL]);
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

UINTN ComputeRow0PosY (VOID) {
    return ((ScreenH / 2) - TileSizes[0] / 2);
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
    static EG_IMAGE *LeftArrow       = NULL;
    static EG_IMAGE *RightArrow      = NULL;
    static EG_IMAGE *LeftBackground  = NULL;
    static EG_IMAGE *RightBackground = NULL;
    static BOOLEAN   LoadedArrows    = FALSE;

    UINTN RightX = (ScreenW + (TileSizes[0] + TILE_XSPACING) * State->MaxVisible) / 2 + TILE_XSPACING;

    if (!LoadedArrows && !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_ARROWS)) {
        MuteLogger = TRUE;
        LeftArrow  = GetIcon (&egemb_arrow_left,  L"arrow_left" );
        RightArrow = GetIcon (&egemb_arrow_right, L"arrow_right");
        MuteLogger = FALSE;

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

    State->ScrollMode = SCROLL_MODE_ICONS;
    switch (Function) {
        case MENU_FUNCTION_INIT:
            InitScroll (State, Screen->EntryCount, GlobalConfig.MaxTags);

            // Layout
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
            row0PosY = ComputeRow0PosY();
            row1PosX = (ScreenW + TILE_XSPACING - (TileSizes[1] + TILE_XSPACING) * row1Count) >> 1;
            row1PosY = row0PosY + TileSizes[0] + TILE_YSPACING;

            textPosY = (row1Count > 0)
                ? row1PosY + TileSizes[1] + TILE_YSPACING
                : row1PosY;

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
            SwitchToGraphicsAndClear (TRUE);
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
    row0PosY = ComputeRow0PosY();
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

    UINTN ItemIndex = POINTER_NO_ITEM;

    itemPosX = AllocatePool (sizeof (UINTN) * Screen->EntryCount);
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
    if (WaitList != NULL) {
        // Early Return
        return;
    }

    UINTN PointerCount = pdCount();

    WaitListLength = 2 + PointerCount;
    WaitList       = AllocatePool (sizeof (EFI_EVENT) * WaitListLength);
    WaitList[0]    = gST->ConIn->WaitForKey;

    for (UINTN Index = 0; Index < PointerCount; Index++) {
        WaitList[Index + 1] = pdWaitEvent (Index);
    } // for
} // VOID GenerateWaitList()

UINTN WaitForInput (
    UINTN Timeout
) {
    EFI_STATUS  Status;
    UINTN       Length;
    UINTN       Index      = INPUT_TIMEOUT;
    EFI_EVENT   TimerEvent = NULL;

    // Generate WaitList if not already generated.
    GenerateWaitList();

    Length = WaitListLength;

    Status = REFIT_CALL_5_WRAPPER(gBS->CreateEvent, EVT_TIMER, 0, NULL, NULL, &TimerEvent);
    if (Timeout == 0) {
        Length--;
    }
    else {
        if (EFI_ERROR(Status)) {
            REFIT_CALL_1_WRAPPER(gBS->Stall, 100000); // Pause for 100 ms
            return INPUT_TIMER_ERROR;
        }
        else {
            REFIT_CALL_3_WRAPPER(gBS->SetTimer, TimerEvent, TimerRelative, Timeout * 10000);
            WaitList[Length - 1] = TimerEvent;
        }
    }

    Status = REFIT_CALL_3_WRAPPER(gBS->WaitForEvent, Length, WaitList, &Index);
    REFIT_CALL_1_WRAPPER(gBS->CloseEvent, TimerEvent);

    if (EFI_ERROR(Status)) {
        REFIT_CALL_1_WRAPPER(gBS->Stall, 100000); // Pause for 100 ms
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
    BOOLEAN  retval = FALSE;

    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) {
        // Early Return
        return FALSE;
    }

    REFIT_CALL_4_WRAPPER(
        gST->ConOut->QueryMode, gST->ConOut,
        gST->ConOut->Mode->Mode,
        &x_max, &y_max
    );

    if (!GlobalConfig.TextOnly) {
        SwitchToText (TRUE);
    }

    if (line_edit (MenuEntry->LoadOptions, &EditedOptions, x_max)) {
        MY_FREE_POOL(MenuEntry->LoadOptions);
        MenuEntry->LoadOptions = EditedOptions;

        retval = TRUE;
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
    CHAR16 *Title,
    CHAR16 *Message
) {
    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_MID, L"Entering DisplaySimpleMessage");
    #endif

    if (!Message) {
        // Early Return
        return;
    }

    REFIT_MENU_SCREEN *SimpleMessageMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (SimpleMessageMenu == NULL) {
        // Early Return
        return;
    }

    SimpleMessageMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_ABOUT);
    SimpleMessageMenu->Title      = StrDuplicate (Title);

    AddMenuInfoLine (SimpleMessageMenu, Message);

    BOOLEAN RetVal = GetReturnMenuEntry (&SimpleMessageMenu);
    if (!RetVal) {
        FreeMenuScreen (&SimpleMessageMenu);

        // Early Return
        return;
    }

    INTN          DefaultEntry = 0;
    INTN              MenuExit = 0;
    REFIT_MENU_ENTRY *ChosenOption;
    MENU_STYLE_FUNC Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
    MenuExit = RunGenericMenu (SimpleMessageMenu, Style, &DefaultEntry, &ChosenOption);

    #if REFIT_DEBUG > 0
    // DA-TAG: Run check on MenuExit for Coverity
    //         L"UNKNOWN!!" is never reached
    //         Constant ... Do Not Free
    CHAR16 *TypeMenuExit = (MenuExit < 0) ? L"UNKNOWN!!" : MenuExitInfo (MenuExit);

    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu Call in 'DisplaySimpleMessage'",
        MenuExit, TypeMenuExit
    );
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
    CHAR16 *FilenameList,
    CHAR16 *VarName
) {
    EFI_STATUS       Status;
    UINTN            i = 0;
    CHAR16          *Filename, *OneElement, *VolName = NULL;
    BOOLEAN          DeleteIt, DeletedSomething = FALSE;
    REFIT_VOLUME    *Volume;
    EFI_FILE_HANDLE  FileHandle;

    while ((OneElement = FindCommaDelimited (FilenameList, i)) != NULL) {
        DeleteIt = FALSE;
        Filename = StrDuplicate (OneElement);

        if (SplitVolumeAndFilename (&Filename, &VolName)) {
            DeleteIt = TRUE;

            if (FindVolume (&Volume, VolName) && Volume->RootDir) {
                Status = REFIT_CALL_5_WRAPPER(
                    Volume->RootDir->Open, Volume->RootDir,
                    &FileHandle, Filename,
                    EFI_FILE_MODE_READ, 0
                );

                if (Status == EFI_SUCCESS) {
                    DeleteIt = FALSE;
                    REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);
                }
            }
        }

        (DeleteIt) ? DeleteItemFromCsvList (OneElement, FilenameList) : i++;

        MY_FREE_POOL(OneElement);
        MY_FREE_POOL(Filename);
        MY_FREE_POOL(VolName);

        DeletedSomething |= DeleteIt;
    } // while

    return DeletedSomething;
} // BOOLEAN RemoveInvalidFilenames()

// Save a list of items to be hidden to NVRAM or disk,
// as determined by GlobalConfig.UseNvram.
static
VOID SaveHiddenList (
    IN CHAR16 *HiddenList,
    IN CHAR16 *VarName
) {
    EFI_STATUS Status = EFI_INVALID_PARAMETER;
    UINTN      ListLen;

    if (HiddenList == NULL || VarName == NULL) {
        CheckError (Status, L"in SaveHiddenList!!");

        // Early Return ... Prevent NULL dererencing
        return;
    }

    ListLen = StrLen (HiddenList);

    Status = EfivarSetRaw (
        &RefindPlusGuid,
        VarName,
        HiddenList,
        ListLen * 2 + 2 * (ListLen > 0),
        TRUE
    );

    CheckError (Status, L"in SaveHiddenList!!");
} // VOID SaveHiddenList()

// Present a menu for the user to delete (un-hide) hidden tags.
VOID ManageHiddenTags (VOID) {
    EFI_STATUS           Status  = EFI_SUCCESS;
    CHAR16              *AllTags = NULL, *OneElement = NULL;
    CHAR16              *HiddenLegacy, *HiddenFirmware, *HiddenTags, *HiddenTools;
    UINTN                i             = 0;
    BOOLEAN              SaveTags      = FALSE;
    BOOLEAN              SaveTools     = FALSE;
    BOOLEAN              SaveLegacy    = FALSE;
    BOOLEAN              SaveFirmware  = FALSE;
    REFIT_MENU_ENTRY    *MenuEntryItem = NULL;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Creating 'Restore Hidden Tags' Screen");
    #endif

    HiddenTags = ReadHiddenTags (L"HiddenTags");
    if (HiddenTags) {
        SaveTags = RemoveInvalidFilenames (HiddenTags, L"HiddenTags");
        if (HiddenTags && (HiddenTags[0] != L'\0')) {
            AllTags = StrDuplicate (HiddenTags);
        }
    }

    HiddenTools = ReadHiddenTags (L"HiddenTools");
    if (HiddenTools) {
        SaveTools = RemoveInvalidFilenames (HiddenTools, L"HiddenTools");
        if (HiddenTools && (HiddenTools[0] != L'\0')) {
            MergeStrings (&AllTags, HiddenTools, L',');
        }
    }

    HiddenLegacy = ReadHiddenTags (L"HiddenLegacy");
    if (HiddenLegacy && (HiddenLegacy[0] != L'\0')) {
        MergeStrings (&AllTags, HiddenLegacy, L',');
    }

    HiddenFirmware = ReadHiddenTags (L"HiddenFirmware");
    if (HiddenFirmware && (HiddenFirmware[0] != L'\0')) {
        MergeStrings (&AllTags, HiddenFirmware, L',');
    }

    if (!AllTags || StrLen (AllTags) < 1) {
        DisplaySimpleMessage (L"Information", L"No Hidden Tags Found");

        // Early Return
        return;
    }

    REFIT_MENU_SCREEN *RestoreItemMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    RestoreItemMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_HIDDEN);
    RestoreItemMenu->Title      = StrDuplicate (L"Restore Hidden Tags");
    RestoreItemMenu->Hint1      = StrDuplicate (L"Select an Option and Press 'Enter' to Apply the Option");
    RestoreItemMenu->Hint2      = StrDuplicate (L"Press 'Esc' to Return to Main Menu (Without Changes)");

    AddMenuInfoLine (RestoreItemMenu, L"Select a Tag and Press 'Enter' to Restore");
    while ((OneElement = FindCommaDelimited (AllTags, i++)) != NULL) {
        MenuEntryItem = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY)); // do not free
        MenuEntryItem->Title = StrDuplicate (OneElement);
        MenuEntryItem->Tag   = TAG_RETURN;
        AddMenuEntryCopy (RestoreItemMenu, MenuEntryItem);

        MY_FREE_POOL(OneElement);
    } // while

    INTN           DefaultEntry = 0;
    REFIT_MENU_ENTRY  *ChosenOption;
    MENU_STYLE_FUNC Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
    UINTN MenuExit = RunGenericMenu (RestoreItemMenu, Style, &DefaultEntry, &ChosenOption);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'ManageHiddenTags'",
        MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
    );
    #endif

    if (MenuExit == MENU_EXIT_ENTER) {
        SaveTags     |= DeleteItemFromCsvList (ChosenOption->Title, HiddenTags);
        SaveTools    |= DeleteItemFromCsvList (ChosenOption->Title, HiddenTools);
        SaveFirmware |= DeleteItemFromCsvList (ChosenOption->Title, HiddenFirmware);
        SaveLegacy   |= DeleteItemFromCsvList (ChosenOption->Title, HiddenLegacy);

        if (DeleteItemFromCsvList (ChosenOption->Title, HiddenLegacy)) {
            i = HiddenLegacy ? StrLen (HiddenLegacy) : 0;
            Status = EfivarSetRaw (
                &RefindPlusGuid,
                L"HiddenLegacy",
                HiddenLegacy,
                i * 2 + 2 * (i > 0),
                TRUE
            );
            SaveLegacy = TRUE;
            CheckError (Status, L"in ManageHiddenTags!!");
        }
    }

    if (SaveTags) {
        SaveHiddenList (HiddenTags, L"HiddenTags");
    }

    if (SaveLegacy) {
        SaveHiddenList (HiddenLegacy, L"HiddenLegacy");
    }

    if (SaveTools) {
        SaveHiddenList (HiddenTools, L"HiddenTools");
        MY_FREE_POOL(gHiddenTools);
    }

    if (SaveFirmware) {
        SaveHiddenList (HiddenFirmware, L"HiddenFirmware");
    }

    if (SaveTags || SaveTools || SaveLegacy || SaveFirmware) {
        RescanAll (FALSE);
    }

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
    CHAR16     *Buffer = NULL;
    UINTN       Size;
    EFI_STATUS  Status;

    Status = EfivarGetRaw (&RefindPlusGuid, VarName, (VOID **) &Buffer, &Size);

    #if REFIT_DEBUG > 0
    if ((Status != EFI_SUCCESS) && (Status != EFI_NOT_FOUND)) {
        CHAR16 *CheckErrMsg = PoolPrint (L"in ReadHiddenTags:- '%s'", VarName);
        CheckError (Status, CheckErrMsg);
        MY_FREE_POOL(CheckErrMsg);
    }
    #endif

    if ((Status == EFI_SUCCESS) && (Size == 0)) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Zero Size in ReadHiddenTags ... Clearing Buffer"
        );
        #endif

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
    EFI_STATUS  Status = EFI_SUCCESS;
    BOOLEAN     AddTag = TRUE;
    CHAR16     *HiddenTags;

    if (Pathname == NULL || StrLen (Pathname) < 1) {
        // Early Return
        return;
    }

    HiddenTags = ReadHiddenTags (VarName);
    if (HiddenTags == NULL) {
        // Prevent NULL dererencing
        HiddenTags = StrDuplicate (Pathname);
    }
    else {
        if (MyStrStr (HiddenTags, Pathname)) {
            // Skip ... Already Present in List
            AddTag = FALSE;
        }
        else {
            MergeStrings (&HiddenTags, Pathname, L',');
        }
    }

    if (AddTag) {
        Status = EfivarSetRaw (
            &RefindPlusGuid,
            VarName,
            HiddenTags,
            StrLen (HiddenTags) * 2 + 2,
            TRUE
        );
    }

    CheckError (Status, L"in 'AddToHiddenTags'!!");
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
    REFIT_VOLUME      *TestVolume   = NULL;
    BOOLEAN            TagHidden    = FALSE;
    CHAR16            *FullPath     = NULL;
    CHAR16            *GuidStr      = NULL;

    if (!Loader          ||
        !VarName         ||
        !HideEfiMenu     ||
        !Loader->Volume  ||
        !Loader->LoaderPath
    ) {
        // Early Return
        return FALSE;
    }

    if (Loader->Volume->VolName && (StrLen (Loader->Volume->VolName) > 0)) {
        FullPath = StrDuplicate (Loader->Volume->VolName);
    }

    MergeStrings (&FullPath, Loader->LoaderPath, L':');

    AddMenuInfoLine (HideEfiMenu, L"Hide EFI Tag Below?");

    AddMenuInfoLineAlt (
        HideEfiMenu,
        PoolPrint (L"%s?", FullPath)
    );

    AddMenuEntryCopy (HideEfiMenu, &MenuEntryYes);
    AddMenuEntryCopy (HideEfiMenu, &MenuEntryNo);

    INTN           DefaultEntry = 1;
    REFIT_MENU_ENTRY  *ChosenOption;
    MENU_STYLE_FUNC Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
    UINTN MenuExit = RunGenericMenu (HideEfiMenu, Style, &DefaultEntry, &ChosenOption);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'HideEfiTag'",
        MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
    );
    #endif

    if (MyStriCmp (ChosenOption->Title, L"Yes") && (MenuExit == MENU_EXIT_ENTER)) {
        GuidStr = GuidAsString (&Loader->Volume->PartGuid);
        if (FindVolume (&TestVolume, GuidStr) && TestVolume->RootDir) {
            MY_FREE_POOL(FullPath);
            MergeStrings (&FullPath, GuidStr, L'\0');
            MergeStrings (&FullPath, L":", L'\0');
            MergeStrings (
                &FullPath,
                Loader->LoaderPath,
                (Loader->LoaderPath[0] == L'\\' ? L'\0' : L'\\')
            );
        }

        AddToHiddenTags (VarName, FullPath);
        TagHidden = TRUE;
        MY_FREE_POOL(GuidStr);
    }

    MY_FREE_POOL(FullPath);

    return TagHidden;
} // BOOLEAN HideEfiTag()

static
BOOLEAN HideFirmwareTag(
    LOADER_ENTRY      *Loader,
    REFIT_MENU_SCREEN *HideFirmwareMenu
) {
    BOOLEAN TagHidden = FALSE;

    AddMenuInfoLine (HideFirmwareMenu, L"Hide Firmware Tag Below?");
    AddMenuInfoLineAlt (HideFirmwareMenu, PoolPrint (L"%s?", Loader->Title));

    AddMenuEntryCopy (HideFirmwareMenu, &MenuEntryYes);
    AddMenuEntryCopy (HideFirmwareMenu, &MenuEntryNo);

    INTN           DefaultEntry = 1;
    REFIT_MENU_ENTRY  *ChosenOption;
    MENU_STYLE_FUNC Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
    UINTN MenuExit = RunGenericMenu (HideFirmwareMenu, Style, &DefaultEntry, &ChosenOption);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'HideFirmwareTag'",
        MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
    );
    #endif

    if ((MyStriCmp(ChosenOption->Title, L"Yes")) &&
        (MenuExit == MENU_EXIT_ENTER)
    ) {
        AddToHiddenTags(L"HiddenFirmware", Loader->Title);
        TagHidden = TRUE;
    }

    return TagHidden;
} // BOOLEAN HideFirmwareTag()


static
BOOLEAN HideLegacyTag (
    LEGACY_ENTRY      *LegacyLoader,
    REFIT_MENU_SCREEN *HideLegacyMenu
) {
    CHAR16  *Name      = NULL;
    BOOLEAN  TagHidden = FALSE;

    if ((GlobalConfig.LegacyType == LEGACY_TYPE_MAC) && LegacyLoader->me.Title) {
        Name = StrDuplicate (LegacyLoader->me.Title);
    }
    else if ((GlobalConfig.LegacyType == LEGACY_TYPE_UEFI) &&
        LegacyLoader->BdsOption && LegacyLoader->BdsOption->Description
    ) {
        Name = StrDuplicate (LegacyLoader->BdsOption->Description);
    }
    else {
        Name = StrDuplicate (L"Legacy (BIOS) OS");
    }

    AddMenuInfoLine (HideLegacyMenu, L"Hide Legacy Tag Below?");

    AddMenuInfoLineAlt (
        HideLegacyMenu,
        PoolPrint (L"%s?", Name)
    );

    AddMenuEntryCopy (HideLegacyMenu, &MenuEntryYes);
    AddMenuEntryCopy (HideLegacyMenu, &MenuEntryNo);

    INTN           DefaultEntry = 1;
    REFIT_MENU_ENTRY  *ChosenOption;
    MENU_STYLE_FUNC Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
    UINTN MenuExit = RunGenericMenu (HideLegacyMenu, Style, &DefaultEntry, &ChosenOption);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'HideLegacyTag'",
        MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
    );
    #endif

    if (MyStriCmp (ChosenOption->Title, L"Yes") && (MenuExit == MENU_EXIT_ENTER)) {
        AddToHiddenTags (L"HiddenLegacy", Name);
        TagHidden = TRUE;
    }
    MY_FREE_POOL(Name);

    return TagHidden;
} // BOOLEAN HideLegacyTag()

static
VOID HideTag (
    REFIT_MENU_ENTRY *ChosenEntry
) {
    LOADER_ENTRY      *Loader        = (LOADER_ENTRY *) ChosenEntry;
    LEGACY_ENTRY      *LegacyLoader  = (LEGACY_ENTRY *) ChosenEntry;

    if (ChosenEntry == NULL) {
        // Early Return
        return;
    }

    REFIT_MENU_SCREEN *HideTagMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (HideTagMenu == NULL) {
        // Early Return
        return;
    }

    HideTagMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_HIDDEN);
    HideTagMenu->Hint1      = StrDuplicate (L"Select an Option and Press 'Enter' or");
    HideTagMenu->Hint2      = StrDuplicate (L"Press 'Esc' to Return to Main Menu (Without Changes)");

    // BUG: The RescanAll() calls should be conditional on successful calls to
    // HideEfiTag() or HideLegacyTag(); but for the former, this causes
    // crashes on a second call hide a tag if the user chose "No" to the first
    // call. This seems to be related to memory management of Volumes; the
    // crash occurs in FindVolumeAndFilename() and lib.c when calling
    // DevicePathToStr(). Calling RescanAll() on all returns from HideEfiTag()
    // seems to be an effective workaround, but there is likely a memory
    // management bug somewhere that is the root cause.
    switch (ChosenEntry->Tag) {
        case TAG_LOADER:
            if (GlobalConfig.SyncAPFS && Loader->Volume->FSType == FS_TYPE_APFS) {
                CHAR16 *Clarify = (SingleAPFS)
                    ? L"Amend config file instead ... Update 'dont_scan_volumes'"
                    : L"Multi-Instance APFS Container Present";
                DisplaySimpleMessage (
                    L"Not Allowed on Synced APFS Loader",
                    Clarify
                );
            }
            else if (Loader->DiscoveryType != DISCOVERY_TYPE_AUTO) {
                DisplaySimpleMessage (
                    L"Not Allowed on User Configured Loader",
                    L"Amend config file instead ... Disable Stanza"
                );
            }
            else {
                HideTagMenu->Title = L"Hide EFI OS Tag";
                HideEfiTag (Loader, HideTagMenu, L"HiddenTags");

                #if REFIT_DEBUG > 0
                LOG_MSG("User Input Received:");
                LOG_MSG("\n");
                LOG_MSG("  - %s", HideTagMenu->Title);
                LOG_MSG("\n\n");
                #endif

                FreeMenuScreen (&HideTagMenu);
                RescanAll (FALSE);
            }

        break;
        case TAG_LEGACY:
        case TAG_LEGACY_UEFI:
            HideTagMenu->Title = L"Hide Legacy (BIOS) OS Tag";
            if (HideLegacyTag (LegacyLoader, HideTagMenu)) {
                #if REFIT_DEBUG > 0
                LOG_MSG("User Input Received:");
                LOG_MSG("\n");
                LOG_MSG("  - %s", HideTagMenu->Title);
                LOG_MSG("\n\n");
                #endif

                FreeMenuScreen (&HideTagMenu);
                RescanAll (FALSE);
            }

        break;
        case TAG_FIRMWARE_LOADER:
            HideTagMenu->Title = L"Hide Firmware Boot Option Tag";
            if (HideFirmwareTag(Loader, HideTagMenu)) {
                FreeMenuScreen (&HideTagMenu);
                RescanAll (FALSE);
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
        case TAG_INFO_BOOTKICKER:
        case TAG_INFO_NVRAMCLEAN:
            DisplaySimpleMessage (
                L"Not Allowed on Internal Tool",
                L"Amend config file instead ... Update 'showtools'"
            );

        break;
        case TAG_TOOL:
            HideTagMenu->Title = L"Hide Tool Tag";
            HideEfiTag (Loader, HideTagMenu, L"HiddenTools");
            MY_FREE_POOL(gHiddenTools);

            #if REFIT_DEBUG > 0
            LOG_MSG("User Input Received:");
            LOG_MSG("\n");
            LOG_MSG("  - %s", HideTagMenu->Title);
            LOG_MSG("\n\n");
            #endif

            FreeMenuScreen (&HideTagMenu);
            RescanAll (FALSE);
    } // switch

    FreeMenuScreen (&HideTagMenu);
} // VOID HideTag()

UINTN RunMenu (
    IN  REFIT_MENU_SCREEN  *Screen,
    OUT REFIT_MENU_ENTRY  **ChosenEntry
) {
    INTN DefaultEntry = -1;
    MENU_STYLE_FUNC Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
    UINTN MenuExit = RunGenericMenu (Screen, Style, &DefaultEntry, ChosenEntry);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'RunMenu'",
        MenuExit, MenuExitInfo (MenuExit), Screen->Title
    );
    #endif

    return MenuExit;
} // UINTN RunMenu()

UINTN RunMainMenu (
    REFIT_MENU_SCREEN  *Screen,
    CHAR16            **DefaultSelection,
    REFIT_MENU_ENTRY  **ChosenEntry
) {
    REFIT_MENU_ENTRY   *TempChosenEntry     =  NULL;
    MENU_STYLE_FUNC     Style               =  TextMenuStyle;
    MENU_STYLE_FUNC     MainStyle           =  TextMenuStyle;
    UINTN               MenuExit            =  0;
    INTN                DefaultEntryIndex   = -1;
    INTN                DefaultSubmenuIndex = -1;

    #if REFIT_DEBUG > 0
    static
    BOOLEAN  ShowLoaded   = TRUE;
    BOOLEAN  SetSelection = FALSE;
    CHAR16  *MsgStr       = NULL;
    #endif

    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"RunMainMenu";
    #endif

    LOG_SEP(L"X");
    BREAD_CRUMB(L"In %s ... 1 - START", FuncTag);

    TileSizes[0] = (GlobalConfig.IconSizes[ICON_SIZE_BIG]   * 9) / 8;
    TileSizes[1] = (GlobalConfig.IconSizes[ICON_SIZE_SMALL] * 4) / 3;

    BREAD_CRUMB(L"In %s ... 2", FuncTag);

    #if REFIT_DEBUG > 0
    if (ShowLoaded) {
        MsgStr = PoolPrint (
            L"Loaded RefindPlus v%s on %s Firmware",
            REFINDPLUS_VERSION, VendorInfo
        );
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
        LOG_MSG("INFO: %s", MsgStr);
        MY_FREE_POOL(MsgStr);
    }
    #endif

    BREAD_CRUMB(L"In %s ... 3", FuncTag);
    if (DefaultSelection && *DefaultSelection) {
        BREAD_CRUMB(L"In %s ... 3a 1", FuncTag);
        // Find a menu entry that includes *DefaultSelection as a substring
        DefaultEntryIndex = FindMenuShortcutEntry (Screen, *DefaultSelection);

        BREAD_CRUMB(L"In %s ... 3a 2", FuncTag);
        #if REFIT_DEBUG > 0
        if (ShowLoaded) {
            BREAD_CRUMB(L"In %s ... 3a 2a 1", FuncTag);
            SetSelection = (GlobalConfig.DirectBoot) ? FALSE : TRUE;

            MsgStr = PoolPrint (L"Configured Default Loader:- '%s'", *DefaultSelection);
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("%s      %s", OffsetNext, MsgStr);
            MY_FREE_POOL(MsgStr);
        }
        #endif
        BREAD_CRUMB(L"In %s ... 3a 3", FuncTag);
    }

    BREAD_CRUMB(L"In %s ... 4", FuncTag);
    #if REFIT_DEBUG > 0
    if (ShowLoaded) {
        BREAD_CRUMB(L"In %s ... 4a 1", FuncTag);
        ShowLoaded = FALSE;

        BREAD_CRUMB(L"In %s ... 4a 2", FuncTag);
        if (SetSelection) {
            BREAD_CRUMB(L"In %s ... 4a 2a 1", FuncTag);
            UINTN EntryPosition = (DefaultEntryIndex < 0) ? 0 : DefaultEntryIndex;
            MsgStr = PoolPrint (
                L"Highlighted Screen Option:- '%s'",
                Screen->Entries[EntryPosition]->Title
            );
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            LOG_MSG("%s      %s", OffsetNext, MsgStr);
            MY_FREE_POOL(MsgStr);
        }
        BREAD_CRUMB(L"In %s ... 4a 3", FuncTag);
        LOG_MSG("\n\n");
    }
    #endif
    BREAD_CRUMB(L"In %s ... 5", FuncTag);

    // Remove any buffered key strokes
    ReadAllKeyStrokes();
    BREAD_CRUMB(L"In %s ... 6", FuncTag);

    if (AllowGraphicsMode) {
        BREAD_CRUMB(L"In %s ... 6a 1", FuncTag);
        Style          = GraphicsMenuStyle;
        MainStyle      = MainMenuStyle;
        PointerEnabled = PointerActive = pdAvailable();
        DrawSelection  = !PointerEnabled;
    }

    BREAD_CRUMB(L"In %s ... 7 - GenerateWaitList", FuncTag);
    // Generate WaitList if not already generated.
    GenerateWaitList();

    BREAD_CRUMB(L"In %s ... 8 - GetCurrentMS", FuncTag);
    // Save time elaspsed from start til now
    MainMenuLoad = GetCurrentMS();

    BREAD_CRUMB(L"In %s ... 9", FuncTag);
    do {
        LOG_SEP(L"X");
        BREAD_CRUMB(L"In %s ... 9a 1 START DO LOOP", FuncTag);
        MenuExit = RunGenericMenu (Screen, MainStyle, &DefaultEntryIndex, &TempChosenEntry);

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'RunMainMenu'",
            MenuExit, MenuExitInfo (MenuExit), TempChosenEntry->Title
        );
        #endif

        BREAD_CRUMB(L"In %s ... 9a 2", FuncTag);
        Screen->TimeoutSeconds = 0;

        BREAD_CRUMB(L"In %s ... 9a 3", FuncTag);
        if (MenuExit == MENU_EXIT_DETAILS) {
            BREAD_CRUMB(L"In %s ... 9a 3a 1", FuncTag);
            if (!TempChosenEntry->SubScreen) {
                BREAD_CRUMB(L"In %s ... 9a 3a 1a 1", FuncTag);
                // No sub-screen ... Ignore keypress
                MenuExit = 0;
            }
            else {
                BREAD_CRUMB(L"In %s ... 9a 3a 1b 1", FuncTag);
                MenuExit = RunGenericMenu (
                    TempChosenEntry->SubScreen,
                    Style,
                    &DefaultSubmenuIndex,
                    &TempChosenEntry
                );

                BREAD_CRUMB(L"In %s ... 9a 3a 1b 2", FuncTag);
                #if REFIT_DEBUG > 0
                ALT_LOG(1, LOG_LINE_NORMAL,
                    L"Returned '%d' (%s) from RunGenericMenu Call on SubScreen in 'RunMainMenu'",
                    MenuExit, MenuExitInfo (MenuExit)
                );
                #endif

                BREAD_CRUMB(L"In %s ... 9a 3a 1b 3", FuncTag);
                if (MenuExit == MENU_EXIT_ESCAPE || TempChosenEntry->Tag == TAG_RETURN) {
                    BREAD_CRUMB(L"In %s ... 9a 3a 1b 3a 1", FuncTag);
                    MenuExit = 0;
                }

                BREAD_CRUMB(L"In %s ... 9a 3a 1b 4", FuncTag);
                if (MenuExit == MENU_EXIT_DETAILS) {
                    BREAD_CRUMB(L"In %s ... 9a 3a 1b 4a 1", FuncTag);
                    if (!EditOptions ((LOADER_ENTRY *) TempChosenEntry)) {
                        BREAD_CRUMB(L"In %s ... 9a 3a 1b 4a 1a 1", FuncTag);
                        MenuExit = 0;
                    }
                    BREAD_CRUMB(L"In %s ... 9a 3a 1b 4a 2", FuncTag);
                }
                BREAD_CRUMB(L"In %s ... 9a 3a 1b 5", FuncTag);
            }
            BREAD_CRUMB(L"In %s ... 9a 3a 2", FuncTag);
        } // if MenuExit == MENU_EXIT_DETAILS

        BREAD_CRUMB(L"In %s ... 9a 4", FuncTag);
        if (MenuExit == MENU_EXIT_HIDE) {
            BREAD_CRUMB(L"In %s ... 9a 4a 1", FuncTag);
            if (GlobalConfig.HiddenTags) {
                BREAD_CRUMB(L"In %s ... 9a 4a 1a 1", FuncTag);
                HideTag (TempChosenEntry);
            }

            BREAD_CRUMB(L"In %s ... 9a 4a 2", FuncTag);
            MenuExit = 0;
        }

        BREAD_CRUMB(L"In %s ... 9a 5 END DO LOOP", FuncTag);
        LOG_SEP(L"X");
    } while (MenuExit == 0);
    BREAD_CRUMB(L"In %s ... 10", FuncTag);

    // Ignore MenuExit if FlushFailedTag is set and not previously reset
    if (FlushFailedTag && !FlushFailReset) {
        #if REFIT_DEBUG > 0
        CHAR16 *MsgStr = StrDuplicate (L"FlushFailedTag is Set ... Ignore MenuExit");
        ALT_LOG(1, LOG_THREE_STAR_END, L"%s", MsgStr);
        LOG_MSG("INFO: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        FlushFailedTag = FALSE;
        FlushFailReset = TRUE;
        MenuExit = 0;
    }

    BREAD_CRUMB(L"In %s ... 11", FuncTag);
    if (ChosenEntry) {
        BREAD_CRUMB(L"In %s ... 11a 1", FuncTag);
        *ChosenEntry = TempChosenEntry;
    }

    BREAD_CRUMB(L"In %s ... 12", FuncTag);
    if (DefaultSelection) {
        BREAD_CRUMB(L"In %s ... 12a 1", FuncTag);
        MY_FREE_POOL(*DefaultSelection);
        *DefaultSelection = StrDuplicate (TempChosenEntry->Title);
    }

    BREAD_CRUMB(L"In %s ... 13 - END:- return UINTN MenuExit = '%d'", FuncTag,
        MenuExit
    );
    LOG_SEP(L"X");
    return MenuExit;
} // UINTN RunMainMenu()

VOID FreeMenuScreen (
    IN REFIT_MENU_SCREEN **Screen
) {
    UINTN i;

    if (Screen == NULL || *Screen == NULL) {
        // Early Return
        return;
    }

    MY_FREE_POOL((*Screen)->Title);
    MY_FREE_IMAGE((*Screen)->TitleImage);

    if ((*Screen)->InfoLines) {
        for (i = 0; i < (*Screen)->InfoLineCount; i++) {
            MY_FREE_POOL((*Screen)->InfoLines[i]);
        }
        (*Screen)->InfoLineCount = 0;
        MY_FREE_POOL((*Screen)->InfoLines);
    }

    if ((*Screen)->Entries) {
        for (i = 0; i < (*Screen)->EntryCount; i++) {
            FreeMenuEntry (&(*Screen)->Entries[i]);
        } // for
        (*Screen)->EntryCount = 0;
        MY_FREE_POOL((*Screen)->Entries);
    }

    MY_FREE_POOL((*Screen)->TimeoutText);
    MY_FREE_POOL((*Screen)->Hint1);
    MY_FREE_POOL((*Screen)->Hint2);
    MY_FREE_POOL(*Screen);
} // VOID FreeMenuScreen()

static
VOID FreeLegacyEntry (
    IN LEGACY_ENTRY **Entry
) {
    if (Entry == NULL || *Entry == NULL) {
        // Early Return
        return;
    }

    MY_FREE_POOL((*Entry)->me.Title);
    MY_FREE_IMAGE((*Entry)->me.Image);
    MY_FREE_IMAGE((*Entry)->me.BadgeImage);
    FreeMenuScreen (&(*Entry)->me.SubScreen);

    FreeVolume (&(*Entry)->Volume);
    FreeBdsOption (&(*Entry)->BdsOption);
    MY_FREE_POOL((*Entry)->LoadOptions);
    MY_FREE_POOL(*Entry);
} // VOID FreeLegacyEntry()

static
VOID FreeLoaderEntry (
    IN LOADER_ENTRY **Entry
) {
    if (Entry == NULL || *Entry == NULL) {
        // Early Return
        return;
    }

    MY_FREE_POOL((*Entry)->me.Title);
    MY_FREE_IMAGE((*Entry)->me.Image);
    MY_FREE_IMAGE((*Entry)->me.BadgeImage);
    FreeMenuScreen (&(*Entry)->me.SubScreen);

    MY_FREE_POOL((*Entry)->Title);
    MY_FREE_POOL((*Entry)->LoaderPath);
    FreeVolume (&(*Entry)->Volume);
    MY_FREE_POOL((*Entry)->LoadOptions);
    MY_FREE_POOL((*Entry)->InitrdPath);
    MY_FREE_POOL((*Entry)->EfiLoaderPath);
    MY_FREE_POOL(*Entry);
} // VOID FreeLoaderEntry()

VOID FreeMenuEntry (
    REFIT_MENU_ENTRY **Entry
) {
    if (Entry == NULL || *Entry == NULL) {
        // Early Return
        return;
    }

    typedef enum {
        EntryTypeRefitMenuEntry,
        EntryTypeLoaderEntry,
        EntryTypeLegacyEntry,
    } ENTRY_TYPE;

    ENTRY_TYPE EntryType;

    switch ((*Entry)->Tag) {
        case TAG_TOOL:               EntryType = EntryTypeLoaderEntry;     break;
        case TAG_LOADER:             EntryType = EntryTypeLoaderEntry;     break;
        case TAG_LEGACY:             EntryType = EntryTypeLegacyEntry;     break;
        case TAG_LEGACY_UEFI:        EntryType = EntryTypeLegacyEntry;     break;
        case TAG_FIRMWARE_LOADER:    EntryType = EntryTypeLoaderEntry;     break;
        case TAG_LOAD_BOOTKICKER:    EntryType = EntryTypeLoaderEntry;     break;
        case TAG_LOAD_NVRAMCLEAN:    EntryType = EntryTypeLoaderEntry;     break;
        default:                     EntryType = EntryTypeRefitMenuEntry;  break;
    }

    if (EntryType == EntryTypeLoaderEntry) {
        FreeLoaderEntry ((LOADER_ENTRY **) Entry);
    }
    else if (EntryType == EntryTypeLegacyEntry) {
        FreeLegacyEntry ((LEGACY_ENTRY **) Entry);
    }
    else {
        MY_FREE_POOL((*Entry)->Title);
        MY_FREE_IMAGE((*Entry)->Image);
        MY_FREE_IMAGE((*Entry)->BadgeImage);
        FreeMenuScreen (&(*Entry)->SubScreen);
    }

    MY_FREE_POOL(*Entry);
} // VOID FreeMenuEntry()

BDS_COMMON_OPTION * CopyBdsOption (
    BDS_COMMON_OPTION *BdsOption
) {
    BDS_COMMON_OPTION *NewBdsOption = NULL;

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

BOOLEAN GetReturnMenuEntry (
    IN OUT REFIT_MENU_SCREEN **Screen
) {
    if (Screen == NULL || *Screen == NULL) {
        // Early Return
        return FALSE;
    }

    REFIT_MENU_ENTRY *MenuEntryReturn = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
    if (MenuEntryReturn == NULL) {
        // Early Return
        return FALSE;
    }
    MenuEntryReturn->Title = StrDuplicate (L"Return to Main Menu");
    MenuEntryReturn->Tag   = TAG_RETURN;
    AddMenuEntry (*Screen, MenuEntryReturn);

    return TRUE;
} // BOOLEAN GetReturnMenuEntry()

BOOLEAN GetYesNoMenuEntry (
    IN OUT REFIT_MENU_SCREEN **Screen
) {
    if (Screen == NULL || *Screen == NULL) {
        // Early Return
        return FALSE;
    }

    REFIT_MENU_ENTRY *MenuEntryYes = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
    if (MenuEntryYes == NULL) {
        // Early Return
        return FALSE;
    }
    MenuEntryYes->Title = StrDuplicate (L"Yes");
    MenuEntryYes->Tag   = TAG_YES;
    AddMenuEntry (*Screen, MenuEntryYes);

    REFIT_MENU_ENTRY *MenuEntryNo = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
    if (MenuEntryNo == NULL) {
        FreeMenuEntry ((REFIT_MENU_ENTRY **) MenuEntryYes);

        // Early Return
        return FALSE;
    }
    MenuEntryNo->Title = StrDuplicate (L"No");
    MenuEntryNo->Tag   = TAG_NO;
    AddMenuEntry (*Screen, MenuEntryNo);

    return TRUE;
} // BOOLEAN GetYesNoMenuEntry()
