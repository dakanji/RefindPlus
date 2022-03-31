/*
 * libeg/text.c
 * Text drawing functions
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
 * Modified for RefindPlus
 * Copyright (c) 2021-2022 Dayo Akanji (sf.net/u/dakanji/profile)
 * Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
 *
 * Modifications distributed under the preceding terms.
 */


#include "libegint.h"
#include "../BootMaster/global.h"
#include "../BootMaster/rp_funcs.h"
#include "../BootMaster/screenmgt.h"
#include "egemb_font.h"
#include "egemb_font_large.h"

#define FONT_NUM_CHARS 96

extern
BOOLEAN   DefaultBanner;

UINTN     FontCellWidth = 7;
EG_IMAGE *BaseFontImage = NULL;

//
// Text rendering
//

static
VOID egPrepareFont (VOID) {
    UINTN     ScreenW, ScreenH;
    EG_PIXEL  TextFontColor = { 0x00, 0x00, 0x00, 0 };

    egGetScreenSize(&ScreenW, &ScreenH);

    // Get longest and shortest edge dimensions
    UINTN ScreenLongest  = (ScreenW >= ScreenH) ? ScreenW : ScreenH;
    UINTN ScreenShortest = (ScreenW <= ScreenH) ? ScreenW : ScreenH;


    if (BaseFontImage == NULL) {
        if (GlobalConfig.ScaleUI == -1) {
            BaseFontImage = egPrepareEmbeddedImage (&egemb_font, TRUE, &TextFontColor);
        }
        else if (
            (GlobalConfig.ScaleUI == 1) ||
            (ScreenShortest >= HIDPI_SHORT && ScreenLongest >= HIDPI_LONG)
        ) {
            BaseFontImage = egPrepareEmbeddedImage (&egemb_font_large, TRUE, &TextFontColor);
        }
        else {
            BaseFontImage = egPrepareEmbeddedImage (&egemb_font, TRUE, &TextFontColor);
        }
    }
    if (BaseFontImage != NULL) FontCellWidth = BaseFontImage->Width / FONT_NUM_CHARS;
} // static VOID egPrepareFont();

UINTN egGetFontHeight (VOID) {
   egPrepareFont();
   return BaseFontImage->Height;
} // UINTN egGetFontHeight()

UINTN egGetFontCellWidth (VOID) {
   return FontCellWidth;
} // UINTN egGetFontCellWidth()

UINTN egComputeTextWidth (
    IN CHAR16 *Text
) {
    UINTN Width = 0;

    egPrepareFont();
    if (Text != NULL) Width = FontCellWidth * StrLen (Text);

    return Width;
} // UINTN egComputeTextWidth()

VOID egMeasureText (
    IN  CHAR16 *Text,
    OUT UINTN  *Width,
    OUT UINTN  *Height
) {
    egPrepareFont();

    if (Height != NULL) *Height = BaseFontImage->Height;
    if (Width  != NULL) *Width  = StrLen (Text) * FontCellWidth;
} // VOID egMeasureText()

VOID egRenderText (
    IN CHAR16       *Text,
    IN OUT EG_IMAGE *CompImage,
    IN UINTN         PosX,
    IN UINTN         PosY,
    IN UINT8         BGBrightness
) {
    EG_IMAGE        *FontImage;
    EG_PIXEL        *BufferPtr;
    EG_PIXEL        *FontPixelData;
    UINTN            BufferLineOffset;
    UINTN            FontLineOffset;
    UINTN            TextLength;
    UINTN            i, c;

    static EG_IMAGE *DarkFontImage  = NULL;
    static EG_IMAGE *LightFontImage = NULL;

    // Early Return if nothing was passed
    if (Text == NULL) return;

    egPrepareFont();

    // Clip the text
    TextLength = StrLen (Text);

    if (TextLength * FontCellWidth + PosX > CompImage->Width) {
        TextLength = (CompImage->Width - PosX) / FontCellWidth;
    }

    if (BGBrightness >= 128) {
        if (DarkFontImage == NULL) DarkFontImage = egCopyImage (BaseFontImage);
        if (DarkFontImage == NULL) return;

        FontImage = DarkFontImage;
    }
    else {
        if (LightFontImage == NULL) {
            LightFontImage = egCopyImage (BaseFontImage);
            if (LightFontImage == NULL) return;

            EG_PIXEL OurFont = {0xFF, 0xFF, 0xFF, 0};
            if (DefaultBanner || GlobalConfig.TextHelp) {
                OurFont = FontComplement();
            }

            for (i = 0; i < (LightFontImage->Width * LightFontImage->Height); i++) {
                if (LightFontImage->PixelData[i].r == 0 &&
                    LightFontImage->PixelData[i].g == 0 &&
                    LightFontImage->PixelData[i].b == 0
                ) {
                    LightFontImage->PixelData[i].r = OurFont.r;
                    LightFontImage->PixelData[i].g = OurFont.g;
                    LightFontImage->PixelData[i].b = OurFont.b;
                }
                else {
                    LightFontImage->PixelData[i].r = 255 - LightFontImage->PixelData[i].r;
                    LightFontImage->PixelData[i].g = 255 - LightFontImage->PixelData[i].g;
                    LightFontImage->PixelData[i].b = 255 - LightFontImage->PixelData[i].b;
                }
            } // for
        } // if LightFontImage == NULL

        FontImage = LightFontImage;
    } // if/else BGBrightness >= 128

    // Render it
    BufferPtr         = CompImage->PixelData;
    BufferLineOffset  = CompImage->Width;
    BufferPtr        += PosX + PosY * BufferLineOffset;
    FontPixelData     = FontImage->PixelData;
    FontLineOffset    = FontImage->Width;

    for (i = 0; i < TextLength; i++) {
        c = Text[i];
        if (c < 32 || c >= 127) {
            c = 95;
        }
        else {
            c -= 32;
        }

        egRawCompose (
            BufferPtr, FontPixelData + c * FontCellWidth,
            FontCellWidth, FontImage->Height,
            BufferLineOffset, FontLineOffset
        );
        BufferPtr += FontCellWidth;
    }
} // VOID egRenderText()

// Load a font bitmap from the specified file
VOID egLoadFont (
    IN CHAR16 *Filename
) {
    MY_FREE_IMAGE(BaseFontImage);
    BaseFontImage = egLoadImage (SelfDir, Filename, TRUE);
    if (BaseFontImage == NULL) {
        Print (L"Note: Font image file %s is invalid! Using default font!\n");
    }
    egPrepareFont();
} // VOID egLoadFont()
