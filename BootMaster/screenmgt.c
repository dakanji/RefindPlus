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
 * Modifications for rEFInd Copyright (c) 2012-2021 Roderick W. Smith
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
#include "libegint.h"
#include "config.h"
#include "lib.h"
#include "menu.h"
#include "mystrings.h"
#include "../include/refit_call_wrapper.h"
#include "../include/egemb_refindplus_banner.h"
#include "../include/egemb_refindplus_banner_lorez.h"
#include "../include/egemb_refindplus_banner_hidpi.h"

UINTN      ConWidth               = 80;
UINTN      ConHeight              = 25;

CHAR16    *BlankLine              = NULL;

UINTN      ScreenW                = 0;
UINTN      ScreenH                = 0;
UINTN      ScreenLongest          = 0;
UINTN      ScreenShortest         = 0;

BOOLEAN    GraphicsScreenDirty    = FALSE;
BOOLEAN    AllowGraphicsMode      = FALSE;
BOOLEAN    ClearedBuffer          = FALSE;
BOOLEAN    DefaultBanner          =  TRUE;
BOOLEAN    haveError              = FALSE;

EG_PIXEL   BlackPixel             = { 0x00, 0x00, 0x00, 0 };
EG_PIXEL   GrayPixel              = { 0xBF, 0xBF, 0xBF, 0 };
EG_PIXEL   WhitePixel             = { 0xFF, 0xFF, 0xFF, 0 };

EG_PIXEL   StdBackgroundPixel     = { 0xBF, 0xBF, 0xBF, 0 };
EG_PIXEL   MenuBackgroundPixel    = { 0xBF, 0xBF, 0xBF, 0 };
EG_PIXEL   DarkBackgroundPixel    = { 0x00, 0x00, 0x00, 0 };


extern BOOLEAN            IsBoot;
extern BOOLEAN            IconScaleSet;
extern BOOLEAN            ExtremeHiDPI;
extern BOOLEAN            egHasGraphics;
extern BOOLEAN            FlushFailedTag;
extern BOOLEAN            GotConsoleControl;


#if 0
// DA-TAG: Permit Image->PixelData Memory Leak on Qemu
//         Apparent Memory Conflict ... Needs Investigation.
//         See: sf.net/p/refind/discussion/general/thread/4dfcdfdd16
//         Temporary ... Eliminate when no longer required.
//
//         Probable 'El Gordo' manifestation.
//         See notes in 'HideTag' for more.
//
// UPDATE: Disabled for v0.13.2.AK ... Watch for issue reports
static
VOID egFreeImageQEMU (
    EG_IMAGE *Image
) {
    if (GotConsoleControl) {
        MY_FREE_IMAGE(Image);
    }
    else {
        MY_FREE_POOL(Image);
    }
} // static VOID egFreeImageQEMU()
#endif


VOID FixIconScale (VOID) {
    if (IconScaleSet || GlobalConfig.ScaleUI == 99) {
        // Early Return
        return;
    }

    IconScaleSet = TRUE;

    // Scale UI Elements as required
    if (GlobalConfig.ScaleUI == 1) {
        if (GlobalConfig.HelpSize        &&
            ScreenLongest  >= EXDPI_LONG &&
            ScreenShortest >= EXDPI_SHORT
        ) {
            ExtremeHiDPI = TRUE;
            GlobalConfig.IconSizes[ICON_SIZE_BIG]   *= 4;
            GlobalConfig.IconSizes[ICON_SIZE_SMALL] *= 4;
            GlobalConfig.IconSizes[ICON_SIZE_BADGE] *= 4;
            GlobalConfig.IconSizes[ICON_SIZE_MOUSE] *= 4;
        }
        else {
            GlobalConfig.IconSizes[ICON_SIZE_BIG]   *= 2;
            GlobalConfig.IconSizes[ICON_SIZE_SMALL] *= 2;
            GlobalConfig.IconSizes[ICON_SIZE_BADGE] *= 2;
            GlobalConfig.IconSizes[ICON_SIZE_MOUSE] *= 2;
        }
    }
    else if (GlobalConfig.ScaleUI == -1) {
        if (ScreenLongest  > BASE_REZ &&
            ScreenShortest > BASE_REZ
        ) {
            GlobalConfig.IconSizes[ICON_SIZE_BIG]   /= 2;
            GlobalConfig.IconSizes[ICON_SIZE_SMALL] /= 2;
            GlobalConfig.IconSizes[ICON_SIZE_BADGE] /= 2;
            GlobalConfig.IconSizes[ICON_SIZE_MOUSE] /= 2;
        }
        else {
            GlobalConfig.IconSizes[ICON_SIZE_BIG]   /= 4;
            GlobalConfig.IconSizes[ICON_SIZE_SMALL] /= 4;
            GlobalConfig.IconSizes[ICON_SIZE_BADGE] /= 4;
            GlobalConfig.IconSizes[ICON_SIZE_MOUSE] /= 4;
        }
    }
    else { // GlobalConfig.ScaleUI == 0 ... Technically any other value
        if (GlobalConfig.HelpSize        &&
            ScreenLongest  >= EXDPI_LONG &&
            ScreenShortest >= EXDPI_SHORT
        ) {
            ExtremeHiDPI = TRUE;
            GlobalConfig.IconSizes[ICON_SIZE_BIG]   *= 4;
            GlobalConfig.IconSizes[ICON_SIZE_SMALL] *= 4;
            GlobalConfig.IconSizes[ICON_SIZE_BADGE] *= 4;
            GlobalConfig.IconSizes[ICON_SIZE_MOUSE] *= 4;
        }
        else if (
            ScreenLongest  >= HIDPI_LONG &&
            ScreenShortest >= HIDPI_SHORT
        ) {
            GlobalConfig.IconSizes[ICON_SIZE_BIG]   *= 2;
            GlobalConfig.IconSizes[ICON_SIZE_SMALL] *= 2;
            GlobalConfig.IconSizes[ICON_SIZE_BADGE] *= 2;
            GlobalConfig.IconSizes[ICON_SIZE_MOUSE] *= 2;
        }
        else if (
            ScreenLongest  < LOREZ_LIMIT ||
            ScreenShortest < LOREZ_LIMIT
        ) {
            if (ScreenLongest  > BASE_REZ &&
                ScreenShortest > BASE_REZ
            ) {
                GlobalConfig.IconSizes[ICON_SIZE_BIG]   /= 2;
                GlobalConfig.IconSizes[ICON_SIZE_SMALL] /= 2;
                GlobalConfig.IconSizes[ICON_SIZE_BADGE] /= 2;
                GlobalConfig.IconSizes[ICON_SIZE_MOUSE] /= 2;
            }
            else {
                GlobalConfig.IconSizes[ICON_SIZE_BIG]   /= 4;
                GlobalConfig.IconSizes[ICON_SIZE_SMALL] /= 4;
                GlobalConfig.IconSizes[ICON_SIZE_BADGE] /= 4;
                GlobalConfig.IconSizes[ICON_SIZE_MOUSE] /= 4;
            }
        }
    } // if/else GlobalConfig.ScaleUI
} // VOID FixIconScale()

VOID PrepareBlankLine (VOID) {
    UINTN i;


    MY_FREE_POOL(BlankLine);

    // Make a buffer for a whole text line
    BlankLine = AllocatePool ((ConWidth + 1) * sizeof (CHAR16));
    if (BlankLine) {
        for (i = 0; i < ConWidth; i++) {
            BlankLine[i] = ' ';
        }
        BlankLine[i] = 0;
    }
} // VOID PrepareBlankLine()

VOID InitScreen (VOID) {
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);

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
        ALT_LOG(1, LOG_THREE_STAR_MID, L"Graphics Mode *NOT* Detected ... Set Text Mode");
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

        if (!GlobalConfig.DirectBoot &&
            GlobalConfig.ScreensaverTime != -1
        ) {
            DrawScreenHeader (MAIN_MENU_NAME);
        }
    }

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID InitScreen()

// Set the screen resolution and mode (text vs. graphics).
VOID SetupScreen (VOID) {
    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;
    CHAR16 *TmpStr;
    #endif

    UINTN   NewWidth;
    UINTN   NewHeight;
    BOOLEAN TextOption;

    static BOOLEAN BannerLoaded = FALSE;


    #if REFIT_DEBUG > 0
    if (!BannerLoaded) {
        LOG_MSG("D I S P L A Y   T I T L E   B A N N E R");
        LOG_MSG("\n");
    }
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);


    // Convert mode number to horizontal & vertical resolution values
    if (GlobalConfig.RequestedScreenWidth   > 0 &&
        GlobalConfig.RequestedScreenHeight == 0
    ) {
        #if REFIT_DEBUG > 0
        LOG_MSG("Get Resolution from Mode:");
        LOG_MSG("\n");
        #endif

        egGetResFromMode (
            &(GlobalConfig.RequestedScreenWidth),
            &(GlobalConfig.RequestedScreenHeight)
        );
    }

    // Set the believed-to-be current resolution to the LOWER of the current
    // believed-to-be resolution and the requested resolution (if exists).
    // This is done to enable setting a lower-than-default resolution.
    if (GlobalConfig.RequestedScreenWidth  > 0 &&
        GlobalConfig.RequestedScreenHeight > 0
    ) {
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
            L"Record *NEW* Current Text Mode Resolution as '%d x %d'",
            ScreenLongest, ScreenShortest
        );
        #endif

        if (ScreenW > GlobalConfig.RequestedScreenWidth ||
            ScreenH > GlobalConfig.RequestedScreenHeight
        ) {
            #if REFIT_DEBUG > 0
            MsgStr = L"Match Requested Resolution to Actual Resolution";
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("  - %s", MsgStr);
            LOG_MSG("\n");
            #endif

            // Requested text mode forces us to use a bigger graphics mode
            GlobalConfig.RequestedScreenWidth  = ScreenW;
            GlobalConfig.RequestedScreenHeight = ScreenH;
        }
    }

    if (GlobalConfig.RequestedScreenWidth > 0) {
        egSetScreenSize (
            &(GlobalConfig.RequestedScreenWidth),
            &(GlobalConfig.RequestedScreenHeight)
        );
        egGetScreenSize (&ScreenW, &ScreenH);
    } // if user requested a particular screen resolution

    if (!AllowGraphicsMode) {
        #if REFIT_DEBUG > 0
        if (GlobalConfig.TextOnly) {
            MsgStr = (GlobalConfig.DirectBoot)
                ? L"'DirectBoot' is Active"
                : L"Running in Text Only Mode";
            ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
            LOG_MSG("Skip Title Banner Display ... %s", MsgStr);
        }
        else {
            MsgStr = L"Invalid Screen Mode ... Switch to Text Mode";
            ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
            LOG_MSG("WARN: %s", MsgStr);
        }
        LOG_MSG("\n\n");
        #endif

        GlobalConfig.TextOnly = TRUE;
        AllowGraphicsMode = FALSE;
        SwitchToText (FALSE);
    }
    else {
        TextOption = (egIsGraphicsModeEnabled()) ? FALSE : TRUE;
        if (TextOption || !BannerLoaded) {
            #if REFIT_DEBUG > 0
            MsgStr = (TextOption)
                ? L"Text Screen Mode Active ... Prepare Graphics Mode Switch"
                : L"Graphics FX Mode Active ... Prepare Title Banner Display";
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("%s:", MsgStr);

            MsgStr = L"Display Mode Resolution";
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"%s:- '%d x %d'",
                MsgStr, ScreenLongest, ScreenShortest
            );
            LOG_MSG(
                "%s  - %s:- '%d x %d'",
                OffsetNext, MsgStr, ScreenLongest, ScreenShortest
            );

            // Scale UI Elements for HiDPI or LoRez graphics as required
            if (GlobalConfig.ScaleUI == 99) {
                MsgStr = L"UI Scaling Disabled ... Maintain UI Scale";
            }
            else if (GlobalConfig.ScaleUI == 1) {
                MsgStr = (IconScaleSet)
                    ? (ExtremeHiDPI)
                        ? L"ExDPI Flag ... Maintain UI Scale"
                        : L"HiDPI Flag ... Maintain UI Scale"
                    : (ExtremeHiDPI)
                        ? L"ExDPI Flag ... Scale UI Elements Up"
                        : L"HiDPI Flag ... Scale UI Elements Up";
            }
            else if (GlobalConfig.ScaleUI == -1) {
                if (IconScaleSet) {
                    MsgStr = (
                            ScreenLongest  > BASE_REZ &&
                            ScreenShortest > BASE_REZ
                        )
                        ? L"LoRez Flag ... Maintain UI Scale"
                        : L"Basic Flag ... Maintain UI Scale";
                }
                else {
                    MsgStr = (
                            ScreenLongest  > BASE_REZ &&
                            ScreenShortest > BASE_REZ
                        )
                        ? L"LoRez Flag ... Scale UI Elements Down"
                        : L"Basic Flag ... Scale UI Elements Down";
                }
            }
            else { // GlobalConfig.ScaleUI == 0 ... Technically any other value
                if (ScreenLongest  > HIDPI_LONG &&
                    ScreenShortest > HIDPI_SHORT
                ) {
                    MsgStr = (IconScaleSet)
                        ? (ExtremeHiDPI)
                            ? L"ExDPI Mode ... Maintain UI Scale"
                            : L"HiDPI Mode ... Maintain UI Scale"
                        : (ExtremeHiDPI)
                            ? L"ExDPI Mode ... Scale UI Elements Up"
                            : L"HiDPI Mode ... Scale UI Elements Up";
                }
                else if (
                    ScreenLongest  > LOREZ_LIMIT &&
                    ScreenShortest > LOREZ_LIMIT
                ) {
                    MsgStr = L"LoDPI Mode ... Maintain UI Scale";
                }
                else if (
                    ScreenLongest  > BASE_REZ &&
                    ScreenShortest > BASE_REZ
                ) {
                    MsgStr = (IconScaleSet)
                        ? L"LoRez Mode ... Maintain UI Scale"
                        : L"LoRez Mode ... Scale UI Elements Down";
                }
                else {
                    MsgStr = (IconScaleSet)
                        ? L"Basic Mode ... Maintain UI Scale"
                        : L"Basic Mode ... Scale UI Elements Down";
                }
            } // if/else GlobalConfig.ScaleUI
            #endif

            // Run the update
            FixIconScale();

            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("%s    * %s", OffsetNext, MsgStr);
            LOG_MSG("\n\n");
            #endif

            if (TextOption) {
                #if REFIT_DEBUG > 0
                MsgStr = L"Deploying Graphics Mode";
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("INFO: %s", MsgStr);
                LOG_MSG("\n\n");
                #endif

                // Clear screen and show banner
                // We now know we will stay in graphics mode
                SwitchToGraphics();
            }

            if (GlobalConfig.ScreensaverTime == -1) {
                #if REFIT_DEBUG > 0
                LOG_MSG("INFO: Changing to Screensaver Display");

                MsgStr = L"Configured to Start with Screensaver";
                ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
                LOG_MSG("%s      %s", OffsetNext, MsgStr);
                #endif

                // Start with screen blanked
                GraphicsScreenDirty = TRUE;
            }
            else {
                // Clear the screen
                BltClearScreen (TRUE);

                #if REFIT_DEBUG > 0
                TmpStr = L"Title Banner Displayed";
                MsgStr = (TextOption) ? L"Graphics Mode Deployed" : TmpStr;
                ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
                LOG_MSG("INFO: %s", MsgStr);

                if (TextOption) {
                    LOG_MSG("%s      %s", OffsetNext, TmpStr);
                }
                #endif
            }

            #if REFIT_DEBUG > 0
            if (NativeLogger && GlobalConfig.LogLevel > 0) {
                LOG_MSG("\n");
            }
            else {
                LOG_MSG("\n\n");
            }
            #endif

            BannerLoaded = TRUE;
        } // if TextOption || !BannerLoaded
    } // if/else !AllowGraphicsMode

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID SetupScreen()

VOID SwitchToText (
    IN BOOLEAN CursorEnabled
) {
    #if REFIT_DEBUG > 0
    BOOLEAN TextModeOnEntry;
    #endif

    EFI_STATUS Status;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);



    #ifdef __MAKEWITH_TIANO
    // DA-TAG: Limit to TianoCore
    if (!GlobalConfig.UseTextRenderer && !IsBoot && AppleFirmware) {
        // Override Text Renderer Setting on Apple Firmware
        //
        // DA-TAG: Investigate This
        //         Was needed on MacPro but maybe not all Macs?
        //         Confirm if really needed on MacPro or consider removing
        Status = OcUseBuiltinTextOutput (
            (egHasGraphics)
                ? EfiConsoleControlScreenGraphics
                : EfiConsoleControlScreenText
        );
        if (!EFI_ERROR(Status)) {
            GlobalConfig.UseTextRenderer = TRUE;

            #if REFIT_DEBUG > 0
            LOG_MSG("INFO: Config Setting Forced On:- 'renderer_text'");
            LOG_MSG("\n\n");
            #endif
        }
    }
    #endif



    egSetGraphicsModeEnabled (FALSE);
    REFIT_CALL_2_WRAPPER(gST->ConOut->EnableCursor, gST->ConOut, CursorEnabled);

    #if REFIT_DEBUG > 0
    TextModeOnEntry = (
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
                "Could *NOT* Get Text Console Size ... Using Default:- '%d x %d'",
                ConHeight, ConWidth
            );
        }
        #endif
    }
    else {
        #if REFIT_DEBUG > 0
        if (TextModeOnEntry) {
            LOG_MSG(
                "Text Console Size:- '%d x %d'",
                ConWidth, ConHeight
            );
        }
        #endif
    }

    #if REFIT_DEBUG > 0
    if (TextModeOnEntry) {
        LOG_MSG("\n\n");
    }
    #endif

    PrepareBlankLine();

    #if REFIT_DEBUG > 0
    if (TextModeOnEntry) {
        LOG_MSG("INFO: Switched to Text Mode");
        LOG_MSG("\n\n");
    }
    #endif

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID SwitchToText()

EFI_STATUS SwitchToGraphics (VOID) {
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    if (!AllowGraphicsMode) {
        BREAD_CRUMB(L"%a:  1a 1 - END:- Return EFI_NOT_STARTED (AllowGraphicsMode = FALSE)", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return EFI_NOT_STARTED;
    }

    BREAD_CRUMB(L"%a:  2", __func__);
    if (egIsGraphicsModeEnabled()) {
        BREAD_CRUMB(L"%a:  2a 1 - END:- Return EFI_ALREADY_STARTED (egIsGraphicsModeEnabled = TRUE)", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return EFI_ALREADY_STARTED;
    }

    BREAD_CRUMB(L"%a:  3", __func__);
    egSetGraphicsModeEnabled (TRUE);
    GraphicsScreenDirty = TRUE;

    BREAD_CRUMB(L"%a:  4 - END:- Return EFI_SUCCESS", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
    return EFI_SUCCESS;
} // EFI_STATUS SwitchToGraphics()

VOID BeginTextScreen (
    IN CHAR16 *Title
) {
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    DrawScreenHeader (Title);

    BREAD_CRUMB(L"%a:  2", __func__);
    SwitchToText (FALSE);

    // Reset error flag
    haveError = FALSE;

    BREAD_CRUMB(L"%a:  4 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID BeginTextScreen()

VOID FinishTextScreen (
    IN BOOLEAN WaitAlways
) {
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    if (haveError || WaitAlways) {
        BREAD_CRUMB(L"%a:  1a 1", __func__);
        SwitchToText (FALSE);
        BREAD_CRUMB(L"%a:  1a 2", __func__);
        PauseForKey();
    }

    // Reset error flag
    haveError = FALSE;

    BREAD_CRUMB(L"%a:  2 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID FinishTextScreen()

VOID BeginExternalScreen (
    IN BOOLEAN  UseGraphicsMode,
    IN CHAR16  *Title
) {
    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr;
    BOOLEAN  CheckMute = FALSE;
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    if (GlobalConfig.DirectBoot) {
        BREAD_CRUMB(L"%a:  1a 1 - END:- VOID - 'DirectBoot' is Active", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Reset error flag
        haveError = FALSE;

        // Early Return
        return;
    }

    BREAD_CRUMB(L"%a:  2", __func__);
    if (!AllowGraphicsMode) {
        BREAD_CRUMB(L"%a:  2a 1", __func__);
        UseGraphicsMode = FALSE;
    }

    BREAD_CRUMB(L"%a:  3", __func__);
    if (UseGraphicsMode) {
        BREAD_CRUMB(L"%a:  3a 1", __func__);
        #if REFIT_DEBUG > 0
        MsgStr = L"Begin Child Image Display with Screen Mode:- 'Graphics'";
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        if (!IsBoot) {
            BREAD_CRUMB(L"%a:  3a 1a 1", __func__);
            LOG_MSG("%s    * %s", OffsetNext, MsgStr);
            LOG_MSG("\n\n");
        }
        #endif

        BREAD_CRUMB(L"%a:  3a 2", __func__);
        SwitchToGraphicsAndClear (FALSE);
    }
    else {
        BREAD_CRUMB(L"%a:  3b 1", __func__);
        #if REFIT_DEBUG > 0
        MsgStr = L"Begin Child Image Display with Screen Mode:- 'Text'";
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        if (!IsBoot) {
            BREAD_CRUMB(L"%a:  3b 1a 1", __func__);
            LOG_MSG("%s    * %s", OffsetNext, MsgStr);
        }
        #endif

        BREAD_CRUMB(L"%a:  3b 2", __func__);
        SwitchToText (UseGraphicsMode);

        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif
        BREAD_CRUMB(L"%a:  3b 3", __func__);
        DrawScreenHeader (Title);
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif
    }

    // Reset error flag
    haveError = FALSE;
    BREAD_CRUMB(L"%a:  4 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID BeginExternalScreen()

VOID FinishExternalScreen (VOID) {
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    // Make sure we clean up later
    GraphicsScreenDirty = TRUE;

    if (haveError) {
        BREAD_CRUMB(L"%a:  1a 1", __func__);
        SwitchToText (FALSE);
        BREAD_CRUMB(L"%a:  1a 2", __func__);
        PauseForKey();
    }

    BREAD_CRUMB(L"%a:  2", __func__);
    // Reset the screen resolution, in case external program changed it.
    SetupScreen();

    // Reset error flag
    haveError = FALSE;
    BREAD_CRUMB(L"%a:  3 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID FinishExternalScreen()

VOID TerminateScreen (VOID) {
    // Clear text screen
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    REFIT_CALL_1_WRAPPER(gST->ConOut->ClearScreen,  gST->ConOut);

    // Enable cursor
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
        REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, i);
        if (BlankLine) {
            Print (BlankLine);
        }
    }

    // Print header text
    if (Title != NULL) {
        REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 3, 1);
        if (MyStriCmp (Title, MAIN_MENU_NAME)) {
            Print (L"%s  -  Select a Loader or Tool", Title);
        }
        else {
            Print (L"%s", Title);
        }
    }

    // Reposition cursor
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, 4);
} // VOID DrawScreenHeader()

BOOLEAN ReadAllKeyStrokes (VOID) {
    #if REFIT_DEBUG > 0
    CHAR16              *MsgStr;
    #endif

    EFI_STATUS           Status;
    BOOLEAN              GotKeyStrokes;
    BOOLEAN              EmptyBuffer;
    EFI_INPUT_KEY        key;

    static BOOLEAN       FirstCall = TRUE;


    GotKeyStrokes = FALSE;
    EmptyBuffer   = FALSE;

    if (FirstCall || !GlobalConfig.DirectBoot) {
        for (;;) {
            Status = REFIT_CALL_2_WRAPPER(
                gST->ConIn->ReadKeyStroke, gST->ConIn, &key
            );
            switch (Status) {
                case EFI_SUCCESS:
                    // Found keystrokes ... Tag this ... Repeat loop
                    ClearedBuffer = TRUE;
                    GotKeyStrokes = TRUE;

                break;
                case EFI_DEVICE_ERROR:
                    // Ensure the buffer is clear
                    REFIT_CALL_2_WRAPPER(gST->ConIn->Reset, gST->ConIn, FALSE);

                // No Break
                default:
                    EmptyBuffer = TRUE;
            } // switch

            if (EFI_ERROR(Status)) {
                // Exit loop on error ... Buffer is empty
                break;
            }
        } // for
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

    MsgStr = PoolPrint (L"Clear Keystroke Buffer ... %r", Status);
    LOG_MSG("INFO: %s", MsgStr);
    LOG_MSG("\n\n");
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    FirstCall = FALSE;

    return GotKeyStrokes;
} // BOOLEAN ReadAllKeyStrokes()

// Displays 'Text' without regard to appearance.
// Mainly for debugging and rare error messages.
// 'PositionCode' is only used in graphics mode.
//
// DA-TAG: Investigate This
//         Handle multi-line text.
VOID PrintUglyText (
    IN CHAR16 *Text,
    IN UINTN    PositionCode
) {
    if (Text != NULL) {
        if (AppleFirmware &&
            AllowGraphicsMode &&
            egIsGraphicsModeEnabled()
        ) {
            egDisplayMessage (
                Text, &BGColorFail,
                PositionCode, 0, NULL
            );
            GraphicsScreenDirty = TRUE;
        }
        else {
            // Non-Mac or in Text Mode
            // Print statement will work
            Print (Text);
            Print (L"\n");
        }
    }
} // VOID PrintUglyText()

VOID PauseForKey (VOID) {
    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr;
    BOOLEAN  CheckMute = FALSE;
    #endif

    UINTN   i, WaitOut;
    BOOLEAN Breakout  = FALSE;


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
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Resuming After Pause");
    #endif
} // VOID PauseForKey

// Pause for a specified number of seconds
// Can be terminated on keypress
VOID PauseSeconds (
    UINTN Seconds
) {
    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    UINTN   i, WaitOut;
    BOOLEAN Breakout  = FALSE;


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
    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    UINTN   i;


    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_MID, L"Halting for %d Seconds", Seconds);
    #endif

    for (i = 0; i < Seconds; ++i) {
        // Wait 1 second
        // DA-TAG: 100 Loops == 1 Sec
        RefitStall (100);
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

VOID RefitDeadLoop (VOID) {
    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    UINTN   index;

    #if REFIT_DEBUG > 0
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

BOOLEAN CheckFatalError (
    IN EFI_STATUS  Status,
    IN CHAR16     *where
) {
    CHAR16 *Temp;

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
    CHAR16 *Temp;

    if (!EFI_ERROR(Status)) {
        // Early Return
        return FALSE;
    }



#ifdef __MAKEWITH_GNUEFI
    CHAR16 ErrorName[64];
    StatusToString (ErrorName, Status);

    #if REFIT_DEBUG > 0
    LOG_MSG("** WARN: '%s' %s", ErrorName, where);
    LOG_MSG("\n\n");
    #endif

    Temp = PoolPrint (L"Error: '%s' %s", ErrorName, where);
#else
    Temp = PoolPrint (L"Error: '%r' %s", Status, where);

    #if REFIT_DEBUG > 0
    LOG_MSG("** WARN: '%r' %s", Status, where);
    LOG_MSG("\n\n");
    #endif
#endif



    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_STAR_SEPARATOR, Temp);
    #endif

    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText (Temp, NEXTLINE);
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

    PauseSeconds (6);

    // Defeat need to "Press a Key to Continue" in debug mode
    // Override this if volume is full
    haveError = (
        MyStrStr (where, L"While Reading Boot Sector") ||
        MyStrStr (where, L"in ReadHiddenTags")
    ) ? FALSE : TRUE;
    haveError = (!haveError && (Status == EFI_VOLUME_FULL)) ? TRUE : FALSE;

    MY_FREE_POOL(Temp);

    return haveError;
} // BOOLEAN CheckError()

VOID SwitchToGraphicsAndClear (
    IN BOOLEAN ShowBanner
) {
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    SwitchToGraphics();

    BREAD_CRUMB(L"%a:  2", __func__);
    if (GraphicsScreenDirty) {
        BREAD_CRUMB(L"%a:  2a 1", __func__);
        BltClearScreen (ShowBanner);
    }

    BREAD_CRUMB(L"%a:  3 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID SwitchToGraphicsAndClear()

UINTN GetLumIndex (
    UINTN DataR,
    UINTN DataG,
    UINTN DataB
) {
    UINTN FactorFP = 10;
    UINTN Divisor  = 3 * FactorFP;
    UINTN LumIndex = (
        (
            (DataR * FactorFP) +
            (DataG * FactorFP) +
            (DataB * FactorFP) +
            (Divisor / 2) // Added For Rounding
        ) / Divisor
    );

    return LumIndex;
} // UINTN GetLumIndex()

EG_PIXEL FontComplement (VOID) {
    EG_PIXEL OurPix = {0x0, 0x0, 0x0, 0};
    UINTN    MaxRGB;
    UINTN    MinRGB;
    UINTN    LumIndex;
    UINTN    PixelR = (UINTN) MenuBackgroundPixel.r;
    UINTN    PixelG = (UINTN) MenuBackgroundPixel.g;
    UINTN    PixelB = (UINTN) MenuBackgroundPixel.b;

    if (PixelR == 191 &&
        PixelG == 191 &&
        PixelB == 191
    ) {
        // Early Return
        // Default Background ... Use default dark font
        return OurPix;
    }

    // Get Screen Luminance Index
    LumIndex = GetLumIndex (PixelR, PixelG, PixelB);

    if (LumIndex >= 128) {
        // Early Return
        // Lightish Background ... Use default dark font
        return OurPix;
    }

    // Use complementary colour on darkish backgrounds
    // Complementary colour for Mid Grey is ... Mid Grey!
    // Check for a broadly defined 'Grey' to fix
    // Difficult to read text otherwise
    MaxRGB = (PixelR > PixelG) ? PixelR : PixelG;
    MaxRGB = (MaxRGB > PixelB) ? MaxRGB : PixelB;

    MinRGB = (PixelR < PixelG) ? PixelR : PixelG;
    MinRGB = (MinRGB < PixelB) ? MinRGB : PixelB;

    if ((MaxRGB - MinRGB) <= 64) {
        // We have 'Grey' ... Determine if it is 'Mid Grey'
        if (MaxRGB < 160 && MinRGB > 80) {
            // We have 'Mid Grey' ... Set base to 'Black'
            // Will be flipped to complementary 'White' later
            PixelR = PixelG = PixelB = 0;
        }
    }

    // Get complementary font colour
    OurPix.r = 255 - PixelR;
    OurPix.g = 255 - PixelG;
    OurPix.b = 255 - PixelB;

    return OurPix;
} // EG_PIXEL FontComplement()

VOID BltClearScreen (
    BOOLEAN ShowBanner
) {
    #if REFIT_DEBUG > 0
    CHAR16          *MsgStr;
    CHAR16          *StrSpacer;

    static BOOLEAN   LoggedBanner = FALSE;
    #endif

    EG_IMAGE        *CompImage;
    EG_IMAGE        *NewBanner;
    EG_PIXEL         BannerFont;
    UINTN            BannerType;
    INTN             BannerPosX;
    INTN             BannerPosY;
    BOOLEAN          BannerPass;

    static EG_IMAGE *Banner = NULL;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    #if REFIT_DEBUG > 0
    if (!IsBoot) {
        LOG_MSG("Refresh Screen:");
        BRK_MAX("\n");
    }
    #endif

    BREAD_CRUMB(L"%a:  2", __func__);
    BannerPass = (
        !IsBoot ||
        (
            ShowBanner &&
            !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_BANNER)
        )
    );
    if (!BannerPass) {
        BREAD_CRUMB(L"%a:  2a 1 - (Set Screen to Menu Background Colour)", __func__);
        #if REFIT_DEBUG > 0
        if (!IsBoot) {
            LOG_MSG("%s  - Clear Screen",
                (GlobalConfig.LogLevel <= LOGLEVELMAX) ? OffsetNext : L""
            );
        }
        #endif

        // Not showing banner
        // Clear to background colour
        egClearScreen (
            (GlobalConfig.DirectBoot)
                ? &BlackPixel : &MenuBackgroundPixel
        );
    }
    else {
        BREAD_CRUMB(L"%a:  2b 1", __func__);
        // Load banner on first call
        if (!Banner) {
            #if REFIT_DEBUG > 0
            LOG_MSG("%s  - Fetch Banner",
                (GlobalConfig.LogLevel <= LOGLEVELMAX) ? OffsetNext : L""
            );
            #endif

            if (GlobalConfig.BannerFileName) {
                Banner = egLoadImage (SelfDir, GlobalConfig.BannerFileName, FALSE);
            }

            if (Banner != NULL) {
                MenuBackgroundPixel = (GlobalConfig.DirectBoot)
                    ? BlackPixel : Banner->PixelData[0];

                // Using Custom Title Banner
                DefaultBanner = FALSE;

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
                if (GlobalConfig.DirectBoot) {
                    MenuBackgroundPixel = BlackPixel;
                }
                else if (!GlobalConfig.CustomScreenBG) {
                    MenuBackgroundPixel = GrayPixel;
                }
                else {
                    // Override Default Values
                    MenuBackgroundPixel.r = GlobalConfig.ScreenR;
                    MenuBackgroundPixel.g = GlobalConfig.ScreenG;
                    MenuBackgroundPixel.b = GlobalConfig.ScreenB;
                }

                #if REFIT_DEBUG > 0
                StrSpacer = L"    ";
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

                // Get complementary font colour if needed
                BannerFont = FontComplement();

                // Get default banner type
                BannerType = 0;
                if (0);
                else if (GlobalConfig.ScaleUI == 99) BannerType = 0;
                else if (GlobalConfig.ScaleUI == -1) BannerType = 2;
                else if (
                    (GlobalConfig.ScaleUI == 1) ||
                    (ScreenShortest >= HIDPI_SHORT && ScreenLongest >= HIDPI_LONG)
                ) {
                    BannerType = 1;
                }
                else if (ScreenShortest < LOREZ_LIMIT || ScreenLongest < LOREZ_LIMIT) {
                    BannerType = 2;
                }

                // Get default banner
                if (BannerType == 2) {
                    Banner = egPrepareEmbeddedImage (&egemb_refindplus_banner_lorez, TRUE, &BannerFont);
                }
                else if (BannerType == 1) {
                    Banner = egPrepareEmbeddedImage (&egemb_refindplus_banner_hidpi, TRUE, &BannerFont);
                }
                else {
                    Banner = egPrepareEmbeddedImage (&egemb_refindplus_banner,       TRUE, &BannerFont);
                }
            } // if/else Banner

            if (Banner != NULL) {
                // Compose on background
                CompImage = egCreateFilledImage (
                    Banner->Width,
                    Banner->Height,
                    FALSE,
                    &MenuBackgroundPixel
                );

                if (CompImage != NULL) {
                    egComposeImage (CompImage, Banner, 0, 0);
                    MY_FREE_IMAGE(Banner);
                    Banner = CompImage;
                }
            }
        } // if !Banner

        if (Banner != NULL) {
            #if REFIT_DEBUG > 0
            LOG_MSG("%s  - Scale Banner",
                (GlobalConfig.LogLevel <= LOGLEVELMAX)
                    ? OffsetNext : L""
            );
            BRK_MAX("\n");
            #endif

            if (GlobalConfig.BannerScale == BANNER_FILLSCREEN) {
                NewBanner = (Banner->Width != ScreenW || Banner->Height != ScreenH)
                    ? egScaleImage (Banner, ScreenW, ScreenH) : NULL;
            }
            else if (Banner->Width > ScreenW || Banner->Height > ScreenH) {
                NewBanner = egCropImage (
                    Banner, 0, 0,
                    (Banner->Width  > ScreenW) ? ScreenW : Banner->Width,
                    (Banner->Height > ScreenH) ? ScreenH : Banner->Height
                );
            }
            else {
                NewBanner = NULL;
            }

            if (NewBanner != NULL) {
                // DA-TAG: See notes in 'egFreeImageQEMU'
                MY_FREE_IMAGE(Banner);
                Banner = NewBanner;
            }
        }

        BREAD_CRUMB(L"%a:  2b 2", __func__);
        // Clear and draw banner
        #if REFIT_DEBUG > 0
        LOG_MSG("%s  - Clear Screen",
            (GlobalConfig.LogLevel <= LOGLEVELMAX)
                ? OffsetNext
                : L""
        );
        BRK_MAX("\n");
        #endif
        if (GlobalConfig.ScreensaverTime != -1) {
            BREAD_CRUMB(L"%a:  2b 2a 1 - (Set Screen to Menu Background Colour)", __func__);
            egClearScreen (&MenuBackgroundPixel);
        }
        else {
            BREAD_CRUMB(L"%a:  2b 2b 1 - (Set Screen to Black)", __func__);
            egClearScreen (&BlackPixel);
        }

        BREAD_CRUMB(L"%a:  2b 3", __func__);
        if (Banner != NULL) {
            #if REFIT_DEBUG > 0
            LOG_MSG("%s  - Offer Banner",
                (GlobalConfig.LogLevel <= LOGLEVELMAX)
                    ? OffsetNext
                    : L""
            );
            #endif

            BannerPosX = (Banner->Width < ScreenW) ? ((ScreenW - Banner->Width) / 2) : 0;
            BannerPosY = (INTN) (ComputeRow0PosY(FALSE) / 2) - (INTN) Banner->Height;
            if (BannerPosY < 0) {
                BannerPosY = 0;
            }

            GlobalConfig.BannerBottomEdge = BannerPosY + Banner->Height;

            if (GlobalConfig.ScreensaverTime != -1) {
                BltImage (Banner, (UINTN) BannerPosX, (UINTN) BannerPosY);
            }
        }

        #if REFIT_DEBUG > 0
        LOG_MSG("\n\n");
        #endif
    } // if/else !BannerPass

    GraphicsScreenDirty = FALSE;

    // DA-TAG: See notes in 'egFreeImageQEMU'
    MY_FREE_IMAGE(GlobalConfig.ScreenBackground);
    GlobalConfig.ScreenBackground = egCopyScreen();

    BREAD_CRUMB(L"%a:  3 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
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
    EG_IMAGE    *CompImage;


    // Compose on background
    CompImage = egCreateFilledImage (
        Image->Width,
        Image->Height,
        FALSE,
        BackgroundPixel
    );

    egComposeImage (CompImage, Image, 0, 0);

    // Blt to screen and clean up
    egDrawImage (CompImage, XPos, YPos);
    MY_FREE_IMAGE(CompImage);

    GraphicsScreenDirty = TRUE;
} // VOID BltImageAlpha()

// DA_TAG: Combines original 'BltImageComposite' and 'BltImageCompositeBadge'
VOID BltImageCompositeAny (
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


    // Initialize buffer with base image
    if (BaseImage != NULL) {
        CompImage   = egCopyImage (BaseImage);
        TotalWidth  = BaseImage->Width;
        TotalHeight = BaseImage->Height;
    }

    // Place the top image
    if (TopImage != NULL && CompImage != NULL) {
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

    // Place the badge image
    if (BadgeImage != NULL && CompImage != NULL &&
        (BadgeImage->Width  + 8) < CompWidth &&
        (BadgeImage->Height + 8) < CompHeight
    ) {
        OffsetX += CompWidth  - 8 - BadgeImage->Width;
        OffsetY += CompHeight - 8 - BadgeImage->Height;
        egComposeImage (CompImage, BadgeImage, OffsetX, OffsetY);
    }

    // Blt to screen and clean up
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
} // VOID BltImageCompositeAny()
