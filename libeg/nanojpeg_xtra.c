/*
 * libeg/nanojpeg_xtra.c
 * JPEG image handling functions -- rEFInd interface for NanoJPEG
 *
 * copyright (c) 2018 by by Roderick W. Smith, and distributed
 * under the terms of the GNU GPL v3, or (at your option) any
 * later version.
 *
 * See https://keyj.emphy.de/nanojpeg/ for the original NanoJPEG.
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
#include "../refind/screen.h"
// nanojpeg.c is weird; it doubles as both a header file and a .c file,
// depending on whether _NJ_INCLUDE_HEADER_ONLY is defined....
#define _NJ_INCLUDE_HEADER_ONLY
#include "nanojpeg.c"

typedef struct _jpeg_color {
   UINT8 red;
   UINT8 green;
   UINT8 blue;
} jpeg_color;

// Decode JPEG data into something libeg can use. This function is a wrapper around
// various NanoJPEG functions.
EG_IMAGE * egDecodeJPEG(IN UINT8 *FileData, IN UINTN FileDataLength, IN UINTN IconSize, IN BOOLEAN WantAlpha) {
    EG_IMAGE *NewImage = NULL;
    unsigned Width, Height;
    jpeg_color *JpegData;
    UINTN i;
    nj_result_t Result;

    if (njInit()) {
        Result = njDecode((VOID *) FileData, FileDataLength);
        if (Result != NJ_OK)
            return NULL;

        Width = njGetWidth();
        Height = njGetHeight();

        // allocate image structure and buffer
        NewImage = egCreateImage(Width, Height, WantAlpha);
        if ((NewImage == NULL) || (NewImage->Width != Width) || (NewImage->Height != Height))
            return NULL;

        JpegData = (jpeg_color *) njGetImage();

        // Annoyingly, EFI and NanoJPEG use different ordering of RGB values in
        // their pixel data representations, so we've got to adjust them....
        for (i = 0; i < (NewImage->Height * NewImage->Width); i++) {
            NewImage->PixelData[i].r = JpegData[i].red;
            NewImage->PixelData[i].g = JpegData[i].green;
            NewImage->PixelData[i].b = JpegData[i].blue;
            // Note: AFAIK, NanoJPEG doesn't support alpha/transparency, so if we're
            // asked to do this, set it to be fully opaque.
            if (WantAlpha)
                NewImage->PixelData[i].a = 255;
        }
        FreePool(JpegData);
        njDone();
    }

    return NewImage;
} // EG_IMAGE * egDecodeJPEG()
