/*
 * libeg/image.c
 * Image handling functions
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
/*
 * Modified for RefindPlus
 * Copyright (c) 2021-2022 Dayo Akanji (sf.net/u/dakanji/profile)
 * Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
 *
 * Modifications distributed under the preceding terms.
 */

#include "libegint.h"
#include "../BootMaster/global.h"
#include "../BootMaster/lib.h"
#include "../BootMaster/screenmgt.h"
#include "../BootMaster/mystrings.h"
#include "../include/refit_call_wrapper.h"
#include "../include/egemb_refindplus_banner.h"
#include "../include/egemb_refindplus_banner_lorez.h"
#include "../include/egemb_refindplus_banner_hidpi.h"
#include "lodepng.h"
#include "libeg.h"

#define MAX_FILE_SIZE (1024*1024*1024)

// Multiplier for pseudo-floating-point operations in egScaleImage().
// A value of 4096 should keep us within limits on 32-bit systems, but I've
// seen some minor artifacts at this level, so give it a bit more precision
// on 64-bit systems.
#if defined(EFIX64) | defined(EFIAARCH64)
#   define FP_MULTIPLIER (UINTN) 65536
#else
#   define FP_MULTIPLIER (UINTN) 4096
#endif

#ifndef __MAKEWITH_GNUEFI
#   define LibLocateHandle gBS->LocateHandleBuffer
#   define LibOpenRoot EfiLibOpenRoot
#endif

#if REFIT_DEBUG > 0
extern BOOLEAN   DefaultBanner;
CHAR16          *OffsetNext = L"\n                   ";
#endif

//
// Basic image handling
//

EG_IMAGE * egCreateImage (
    IN UINTN    Width,
    IN UINTN    Height,
    IN BOOLEAN  HasAlpha
) {
    EG_IMAGE   *NewImage;

    NewImage = (EG_IMAGE *) AllocatePool (sizeof (EG_IMAGE));
    if (NewImage == NULL) {
        return NULL;
    }
    NewImage->PixelData = (EG_PIXEL *) AllocatePool (Width * Height * sizeof (EG_PIXEL));

    if (NewImage->PixelData == NULL) {
        MY_FREE_IMAGE(NewImage);
        return NULL;
    }

    NewImage->Width    = Width;
    NewImage->Height   = Height;
    NewImage->HasAlpha = HasAlpha;

    return NewImage;
}

EG_IMAGE * egCreateFilledImage (
    IN UINTN      Width,
    IN UINTN      Height,
    IN BOOLEAN    HasAlpha,
    IN EG_PIXEL  *Color
) {
    EG_IMAGE  *NewImage;

    NewImage = egCreateImage (Width, Height, HasAlpha);
    if (NewImage == NULL) {
        return NULL;
    }

    egFillImage (NewImage, Color);

    return NewImage;
}

EG_IMAGE * egCopyImage (
    IN EG_IMAGE *Image
) {
    EG_IMAGE  *NewImage = NULL;

    if (Image != NULL) {
        NewImage = egCreateImage (Image->Width, Image->Height, Image->HasAlpha);
    }
    if (NewImage == NULL) {
        return NULL;
    }

    CopyMem (NewImage->PixelData, Image->PixelData, Image->Width * Image->Height * sizeof (EG_PIXEL));

    return NewImage;
}

// Returns a smaller image composed of the specified crop area from the larger area.
// If the specified area is larger than is in the original, returns NULL.
EG_IMAGE * egCropImage (
    IN EG_IMAGE  *Image,
    IN UINTN      StartX,
    IN UINTN      StartY,
    IN UINTN      Width,
    IN UINTN      Height
) {
    EG_IMAGE *NewImage = NULL;
    UINTN x, y;

    if (((StartX + Width) > Image->Width) || ((StartY + Height) > Image->Height)) {
        return NULL;
    }

    NewImage = egCreateImage (Width, Height, Image->HasAlpha);
    if (NewImage == NULL) {
        return NULL;
    }

    for (y = 0; y < Height; y++) {
        for (x = 0; x < Width; x++) {
            NewImage->PixelData[y * NewImage->Width + x] = Image->PixelData[(y + StartY) * Image->Width + x + StartX];
        }
    }

    return NewImage;
} // EG_IMAGE * egCropImage()

// The following function implements a bilinear image scaling algorithm, based on
// code presented at http://tech-algorithm.com/articles/bilinear-image-scaling/.
// Resize an image; returns pointer to resized image if successful, NULL otherwise.
// Calling function is responsible for freeing allocated memory.
// NOTE: x_ratio, y_ratio, x_diff, and y_diff should really be float values;
// however, I've found that my 32-bit Mac Mini has a buggy EFI (or buggy CPU?), which
// causes this function to hang on float-to-UINT8 conversions on some (but not all!)
// float values. Therefore, this function uses integer arithmetic but multiplies
// all values by FP_MULTIPLIER to achieve something resembling the sort of precision
// needed for good results.
EG_IMAGE * egScaleImage (
    IN EG_IMAGE  *Image,
    IN UINTN      NewWidth,
    IN UINTN      NewHeight
) {
    EG_IMAGE  *NewImage;
    EG_PIXEL   a, b, c, d;
    UINTN      i, j;
    UINTN      x, y, Index;
    UINTN      Offset = 0;
    UINTN      x_diff, y_diff;
    UINTN      x_ratio, y_ratio;


    if (Image          == NULL ||
        Image->Height  ==    0 ||
        Image->Width   ==    0 ||
        NewHeight      ==    0 ||
        NewWidth       ==    0
    ) {
        return NULL;
    }

    if ((Image->Width == NewWidth) && (Image->Height == NewHeight)) {
        return (egCopyImage (Image));
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_MID, L"Scaling Image to %d x %d", NewWidth, NewHeight);
    #endif

    NewImage = egCreateImage (NewWidth, NewHeight, Image->HasAlpha);
    if (NewImage == NULL) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_END, L"In egScaleImage ... Could Not Create New Image!!");
        #endif

        return NULL;
    }

    x_ratio = ((Image->Width - 1) * FP_MULTIPLIER) / NewWidth;
    y_ratio = ((Image->Height - 1) * FP_MULTIPLIER) / NewHeight;

    for (i = 0; i < NewHeight; i++) {
        for (j = 0; j < NewWidth; j++) {
            x = (j * (Image->Width - 1)) / NewWidth;
            y = (i * (Image->Height - 1)) / NewHeight;
            x_diff = (x_ratio * j) - x * FP_MULTIPLIER;
            y_diff = (y_ratio * i) - y * FP_MULTIPLIER;
            Index = ((y * Image->Width) + x);
            a = Image->PixelData[Index];
            b = Image->PixelData[Index + 1];
            c = Image->PixelData[Index + Image->Width];
            d = Image->PixelData[Index + Image->Width + 1];

            // blue element
            NewImage->PixelData[Offset].b = ((a.b) * (FP_MULTIPLIER - x_diff) * (FP_MULTIPLIER - y_diff) +
                (b.b) * (x_diff) * (FP_MULTIPLIER - y_diff) +
                (c.b) * (y_diff) * (FP_MULTIPLIER - x_diff) +
                (d.b) * (x_diff * y_diff)) / (FP_MULTIPLIER * FP_MULTIPLIER);

            // green element
            NewImage->PixelData[Offset].g = ((a.g) * (FP_MULTIPLIER - x_diff) * (FP_MULTIPLIER - y_diff) +
                (b.g) * (x_diff) * (FP_MULTIPLIER - y_diff) +
                (c.g) * (y_diff) * (FP_MULTIPLIER - x_diff) +
                (d.g) * (x_diff * y_diff)) / (FP_MULTIPLIER * FP_MULTIPLIER);

            // red element
            NewImage->PixelData[Offset].r = ((a.r) * (FP_MULTIPLIER - x_diff) * (FP_MULTIPLIER - y_diff) +
                (b.r) * (x_diff) * (FP_MULTIPLIER - y_diff) +
                (c.r) * (y_diff) * (FP_MULTIPLIER - x_diff) +
                (d.r) * (x_diff * y_diff)) / (FP_MULTIPLIER * FP_MULTIPLIER);

            // alpha element
            NewImage->PixelData[Offset++].a = ((a.a) * (FP_MULTIPLIER - x_diff) * (FP_MULTIPLIER - y_diff) +
                (b.a) * (x_diff) * (FP_MULTIPLIER - y_diff) +
                (c.a) * (y_diff) * (FP_MULTIPLIER - x_diff) +
                (d.a) * (x_diff * y_diff)) / (FP_MULTIPLIER * FP_MULTIPLIER);
        } // for (j...)
    } // for (i...)

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_MID, L"Scaling Image Completed");
    #endif

    return NewImage;
} // EG_IMAGE * egScaleImage()

/*
VOID egFreeImage (
    IN EG_IMAGE *Image
) {
    if (Image == NULL) {
        return;
    }

    MY_FREE_POOL(Image->PixelData);
    MY_FREE_POOL(Image);
}
*/

//
// Basic file operations
//

EFI_STATUS egLoadFile (
    IN EFI_FILE  *BaseDir,
    IN CHAR16    *FileName,
    OUT UINT8   **FileData,
    OUT UINTN    *FileDataLength
) {
    EFI_STATUS          Status;
    UINT64              ReadSize;
    UINTN               BufferSize;
    UINT8              *Buffer;
    EFI_FILE_INFO      *FileInfo;
    EFI_FILE_HANDLE     FileHandle;

    if ((BaseDir == NULL) || (FileName == NULL)) {
        // Early Return
        return EFI_INVALID_PARAMETER;
    }

    Status = REFIT_CALL_5_WRAPPER(
        BaseDir->Open, BaseDir,
        &FileHandle, FileName,
        EFI_FILE_MODE_READ, 0
    );
    if (EFI_ERROR(Status)) {
        // Early Return
        return Status;
    }

    FileInfo = LibFileInfo (FileHandle);
    if (FileInfo == NULL) {
        REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);

        // Early Return
        return EFI_NOT_FOUND;
    }

    ReadSize = FileInfo->FileSize;

    if (ReadSize > MAX_FILE_SIZE) {
        ReadSize = MAX_FILE_SIZE;
    }

    MY_FREE_POOL(FileInfo);

    BufferSize = (UINTN) ReadSize;   // was limited to 1 GB above, so this is safe
    Buffer = (UINT8 *) AllocatePool (BufferSize);
    if (Buffer == NULL) {
        REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);

        // Early Return
        return EFI_OUT_OF_RESOURCES;
    }

    Status = REFIT_CALL_3_WRAPPER(
        FileHandle->Read, FileHandle,
        &BufferSize, Buffer
    );
    REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);
    if (EFI_ERROR(Status)) {
        MY_FREE_POOL(Buffer);

        // Early Return
        return Status;
    }

    if (FileData) {
        *FileData = Buffer;
    }
    else {
        MY_FREE_POOL(Buffer);
    }
    if (FileDataLength) {
        *FileDataLength = BufferSize;
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_MID, L"In egLoadFile ... Loaded File:- '%s'", FileName);
    #endif

    return EFI_SUCCESS;
} // EFI_STATUS egLoadFile()

EFI_STATUS egFindESP (
    OUT EFI_FILE_HANDLE *RootDir
) {
    EFI_STATUS   Status;
    EFI_HANDLE  *Handles;
    UINTN        HandleCount = 0;
    EFI_GUID     ESPGuid = ESP_GUID_VALUE;

    Status = LibLocateHandle (
        ByProtocol,
        &ESPGuid, NULL,
        &HandleCount, &Handles
    );
    if (!EFI_ERROR(Status) && HandleCount > 0) {
        *RootDir = LibOpenRoot (Handles[0]);

        if (*RootDir == NULL) {
            Status = EFI_NOT_FOUND;
        }

        MY_FREE_POOL(Handles);
    }

    return Status;
} // EFI_STATUS egFindESP()

EFI_STATUS egSaveFile (
    IN EFI_FILE  *BaseDir OPTIONAL,
    IN CHAR16    *FileName,
    IN UINT8     *FileData,
    IN UINTN      FileDataLength
) {
    EFI_STATUS       Status;
    UINTN            BufferSize;
    EFI_FILE_HANDLE  FileHandle;

    if (BaseDir == NULL) {
        Status = egFindESP (&BaseDir);
        if (EFI_ERROR(Status)) {
            // Early Return
            return Status;
        }
    }

    Status = REFIT_CALL_5_WRAPPER(
        BaseDir->Open, BaseDir,
        &FileHandle, FileName,
        ReadWriteCreate, 0
    );
    if (EFI_ERROR(Status)) {
        // Early Return
        return Status;
    }

    if (FileDataLength == 0) {
        Status = REFIT_CALL_1_WRAPPER(FileHandle->Delete, FileHandle);
    }
    else {
        BufferSize = FileDataLength;
        Status = REFIT_CALL_3_WRAPPER(
            FileHandle->Write, FileHandle,
            &BufferSize, FileData
        );
        REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);
    }

    return Status;
} // EFI_STATUS egSaveFile()

//
// Loading images from files and embedded data
//

// Decode the specified image data. The IconSize parameter is relevant only
// for ICNS, for which it selects which ICNS sub-image is decoded.
// Returns a pointer to the resulting EG_IMAGE or NULL if decoding failed.
static
EG_IMAGE * egDecodeAny (
    IN UINT8    *FileData,
    IN UINTN     FileDataLength,
    IN UINTN     IconSize,
    IN BOOLEAN   WantAlpha
) {
    EG_IMAGE       *NewImage = egDecodePNG  (FileData, FileDataLength, IconSize, WantAlpha);
    if (!NewImage)  NewImage = egDecodeJPEG (FileData, FileDataLength, IconSize, WantAlpha);
    if (!NewImage)  NewImage = egDecodeBMP  (FileData, FileDataLength, IconSize, WantAlpha);
    if (!NewImage)  NewImage = egDecodeICNS (FileData, FileDataLength, IconSize, WantAlpha);

    return NewImage;
} // static EG_IMAGE * egDecodeAny ()

EG_IMAGE * egLoadImage (
    IN EFI_FILE *BaseDir,
    IN CHAR16   *FileName,
    IN BOOLEAN   WantAlpha
) {
    EFI_STATUS   Status;
    UINTN        FileDataLength;
    UINT8       *FileData;
    EG_IMAGE    *NewImage;

    if (BaseDir == NULL || FileName == NULL) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"In egLoadImage ... Requirements Not Met!!");
        #endif

        // Early Return
        return NULL;
    }

    // Load file
    Status = egLoadFile (BaseDir, FileName, &FileData, &FileDataLength);
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"In egLoadImage ... '%r' Returned While Attempting to Load File!!",
            Status
        );
        #endif

        // Early Return
        return NULL;
    }

    // Decode it
    // '128' can be any arbitrary value
    NewImage = egDecodeAny (FileData, FileDataLength, 128, WantAlpha);
    MY_FREE_POOL(FileData);

    return NewImage;
} // EG_IMAGE * egLoadImage()

// Load an icon from (BaseDir)/Path, extracting the icon of size IconSize x IconSize.
// Returns a pointer to the image data, or NULL if the icon could not be loaded.
EG_IMAGE * egLoadIcon (
    IN EFI_FILE *BaseDir,
    IN CHAR16   *Path,
    IN UINTN     IconSize
) {
    EFI_STATUS      Status;
    UINTN           FileDataLength = 0;
    UINT8          *FileData;
    EG_IMAGE       *NewImage;
    EG_IMAGE       *Image;

    if ((BaseDir == NULL) || (Path == NULL)) {
        #if REFIT_DEBUG > 0
        // Set error status if unable to get to image
        Status = EFI_INVALID_PARAMETER;
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"In egLoadIcon ... '%r' When Trying to Load Icon!!",
            Status
        );
        #endif

        // Return null if error
        return NULL;
    }
    else if (!AllowGraphicsMode) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"In egLoadIcon ... Skipped Loading Icon in %s Mode",
            (GlobalConfig.DirectBoot)
                ? L"DirectBoot"
                : L"Text Screen"
        );
        #endif

        // Early Return
        return NULL;
    }

    // Try to load file if able to get to image
    Status = egLoadFile (BaseDir, Path, &FileData, &FileDataLength);
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"In egLoadIcon ... '%r' When Trying to Load Icon:- '%s'!!",
            Status, Path
        );
        #endif

        // Return null if error
        return NULL;
    }

    // Decode it
    Image = egDecodeAny (FileData, FileDataLength, IconSize, TRUE);
    MY_FREE_POOL(FileData);

    // Return null if unable to decode
    if (Image == NULL) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL,
            L"In egLoadIcon ... Could Not Decode File Data!!"
        );
        #endif

        // Early Return
        return NULL;
    }

    if ((Image->Width != IconSize) || (Image->Height != IconSize)) {
        // Do proportional scaling
        UINTN w = Image->Width;
        UINTN h = Image->Height;
        if (h < w) {
            h = IconSize * h / w;
            w = IconSize;
        }
        else if (w < h) {
            w = IconSize * w / h;
            h = IconSize;
        }
        else {
            w = IconSize;
            h = IconSize;
        }
        NewImage = egScaleImage (Image, w, h);

        // Use scaled image if available
        if (NewImage) {
            MY_FREE_IMAGE(Image);
            Image = NewImage;
        }
        else {
            CHAR16 *MsgStr = PoolPrint (
                L"Could Not Scale Icon in '%s' from %d x %d to %d x %d!!",
                Path, Image->Width, Image->Height, IconSize, IconSize
            );

            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL, L"In egLoadIcon ... %s", MsgStr);
            #endif

            Print(MsgStr);

            MY_FREE_POOL(MsgStr);
        }
    }

    return Image;
} // EG_IMAGE *egLoadIcon()

// Returns an icon of any type from the specified subdirectory using the specified
// base name. All directory references are relative to BaseDir. For instance, if
// SubdirName is "myicons" and BaseName is "os_linux", this function will return
// an image based on "myicons/os_linux.icns" or "myicons/os_linux.png", in that
// order of preference. Returns NULL if no such file is a valid icon file.
EG_IMAGE * egLoadIconAnyType (
    IN EFI_FILE  *BaseDir,
    IN CHAR16    *SubdirName,
    IN CHAR16    *BaseName,
    IN UINTN      IconSize
) {
    EG_IMAGE  *Image = NULL;
    CHAR16    *Extension;
    CHAR16    *FileName;
    UINTN      i = 0;

    if (!AllowGraphicsMode) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"In egLoadIconAnyType ... Skipped Loading Icon in %s Mode",
            (GlobalConfig.DirectBoot)
                ? L"DirectBoot"
                : L"Text Screen"
        );
        #endif

        // Early Return
        return NULL;
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_MID,
        L"Trying to Load Icon from '%s' with Base Name:- '%s'",
        (StrLen (SubdirName) != 0) ? SubdirName : L"\\",
        BaseName
    );
    #endif

    while ((Image == NULL) && ((Extension = FindCommaDelimited (ICON_EXTENSIONS, i++)) != NULL)) {
        FileName = PoolPrint (L"%s\\%s.%s", SubdirName, BaseName, Extension);
        Image    = egLoadIcon (BaseDir, FileName, IconSize);

        MY_FREE_POOL(Extension);
        MY_FREE_POOL(FileName);
    } // while

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"In egLoadIconAnyType ... %s",
        (Image != NULL) ? L"Loaded Icon" : L"Could Not Load Icon!!"
    );
    #endif

    return Image;
} // EG_IMAGE *egLoadIconAnyType()

// Returns an icon with any extension in ICON_EXTENSIONS from either the directory
// specified by GlobalConfig.IconsDir or DEFAULT_ICONS_DIR. The input BaseName
// should be the icon name without an extension. For instance, if BaseName is
// os_linux, GlobalConfig.IconsDir is myicons, DEFAULT_ICONS_DIR is icons, and
// ICON_EXTENSIONS is "icns,png", this function will return myicons/os_linux.icns,
// myicons/os_linux.png, icons/os_linux.icns, or icons/os_linux.png, in that
// order of preference. Returns NULL if no such icon can be found. All file
// references are relative to SelfDir.
EG_IMAGE * egFindIcon (
    IN CHAR16 *BaseName,
    IN UINTN   IconSize
) {
    EG_IMAGE *Image = NULL;

    if (GlobalConfig.IconsDir != NULL) {
        Image = egLoadIconAnyType (
            SelfDir, GlobalConfig.IconsDir,
            BaseName, IconSize
        );
    }

    if (Image == NULL) {
        Image = egLoadIconAnyType (
            SelfDir, DEFAULT_ICONS_DIR,
            BaseName, IconSize
        );
    }

    return Image;
} // EG_IMAGE * egFindIcon()

static
VOID egInvertPlane (
    IN UINT8 *DestPlanePtr,
    IN UINTN PixelCount
) {
    UINTN i;

    if (DestPlanePtr) {
        for (i = 0; i < PixelCount; i++) {
            *DestPlanePtr = 255 - *DestPlanePtr;
            DestPlanePtr += 4;
        }
    }
} // VOID egInvertPlane()

EG_IMAGE * egPrepareEmbeddedImage (
    IN EG_EMBEDDED_IMAGE *EmbeddedImage,
    IN BOOLEAN            WantAlpha,
    IN EG_PIXEL          *ForegroundColor
) {
    #if REFIT_DEBUG > 0
    static BOOLEAN LogTextColour = TRUE;
    #endif

    #if REFIT_DEBUG > 1
    CHAR16 *FuncTag = L"egPrepareEmbeddedImage";
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%s:  1 - START", FuncTag);

    // Sanity checks
    if (!EmbeddedImage) {
        BREAD_CRUMB(L"%s:  1a 1 - END:- return EG_IMAGE NewImage = 'NULL'", FuncTag);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return NULL;
    }

    //BREAD_CRUMB(L"%s:  2", FuncTag);
    if (EmbeddedImage->PixelMode > EG_MAX_EIPIXELMODE ||
        (
            EmbeddedImage->CompressMode != EG_EICOMPMODE_RLE &&
            EmbeddedImage->CompressMode != EG_EICOMPMODE_NONE
        )
    ) {
        BREAD_CRUMB(L"%s:  2a 1 - END:- return EG_IMAGE NewImage = 'NULL'", FuncTag);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return NULL;
    }

    // Allocate image structure and pixel buffer
    //BREAD_CRUMB(L"%s:  3", FuncTag);
    EG_IMAGE *NewImage = egCreateImage (
        EmbeddedImage->Width,
        EmbeddedImage->Height,
        WantAlpha
    );

    //BREAD_CRUMB(L"%s:  4", FuncTag);
    if (!NewImage) {
        BREAD_CRUMB(L"%s:  4a 1 - END:- return EG_IMAGE NewImage = 'NULL'", FuncTag);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return NULL;
    }

    //BREAD_CRUMB(L"%s:  5", FuncTag);
    #if REFIT_DEBUG > 1
    UINT8 *CompStart  = 0;
    #endif
    UINT8 *CompData   = (UINT8 *) EmbeddedImage->Data;   // drop const
    UINTN  CompLen    = EmbeddedImage->DataLength;
    UINTN  PixelCount = EmbeddedImage->Width * EmbeddedImage->Height;

    //BREAD_CRUMB(L"%s:  6", FuncTag);
    // DA-TAG: Investigate This
    //         Decompress whole data block here for EG_EICOMPMODE_EFICOMPRESS

    //BREAD_CRUMB(L"%s:  7", FuncTag);
    if (EmbeddedImage->PixelMode == EG_EIPIXELMODE_GRAY ||
        EmbeddedImage->PixelMode == EG_EIPIXELMODE_GRAY_ALPHA
    ) {
        //BREAD_CRUMB(L"%s:  7a 1", FuncTag);
        // Copy grayscale plane and expand
        if (EmbeddedImage->CompressMode == EG_EICOMPMODE_RLE) {
            BREAD_CRUMB(L"%s:  7a 1a 1 - Grey Plane Size:- '%d'", FuncTag,
                CompData - CompStart
            );
            egDecompressIcnsRLE (&CompData, &CompLen, PLPTR(NewImage, r), PixelCount);
        }
        else {
            //BREAD_CRUMB(L"%s:  7a 1b 1", FuncTag);
            egInsertPlane (CompData, PLPTR(NewImage, r), PixelCount);
            CompData += PixelCount;
        }

        //BREAD_CRUMB(L"%s:  7a 2", FuncTag);
        egCopyPlane (PLPTR(NewImage, r), PLPTR(NewImage, g), PixelCount);
        egCopyPlane (PLPTR(NewImage, r), PLPTR(NewImage, b), PixelCount);
    }
    else if (EmbeddedImage->PixelMode == EG_EIPIXELMODE_COLOR ||
        EmbeddedImage->PixelMode == EG_EIPIXELMODE_COLOR_ALPHA
    ) {
        //BREAD_CRUMB(L"%s:  7b 1", FuncTag);
        // Copy color planes
        if (EmbeddedImage->CompressMode == EG_EICOMPMODE_RLE) {
            //BREAD_CRUMB(L"%s:  7b 1a 1", FuncTag);
            #if REFIT_DEBUG > 1
            CompStart = CompData;
            #endif
            egDecompressIcnsRLE (&CompData, &CompLen, PLPTR(NewImage, r), PixelCount);

            BREAD_CRUMB(L"%s:  7b 1a 2 - Red Plane Size:- '%d'", FuncTag,
                CompData - CompStart
            );
            #if REFIT_DEBUG > 1
            CompStart = CompData;
            #endif
            egDecompressIcnsRLE (&CompData, &CompLen, PLPTR(NewImage, g), PixelCount);

            BREAD_CRUMB(L"%s:  7b 1a 3 - Green Plane Size:- '%d'", FuncTag,
                CompData - CompStart
            );
            egDecompressIcnsRLE (&CompData, &CompLen, PLPTR(NewImage, b), PixelCount);
        }
        else {
            //BREAD_CRUMB(L"%s:  7b 1b 1", FuncTag);
            egInsertPlane (CompData, PLPTR(NewImage, r), PixelCount);
            CompData += PixelCount;

            //BREAD_CRUMB(L"%s:  7b 1b 2", FuncTag);
            egInsertPlane (CompData, PLPTR(NewImage, g), PixelCount);
            CompData += PixelCount;

            //BREAD_CRUMB(L"%s:  7b 1b 3", FuncTag);
            egInsertPlane (CompData, PLPTR(NewImage, b), PixelCount);
            CompData += PixelCount;
        }
    }
    else {
        //BREAD_CRUMB(L"%s:  7c 1", FuncTag);

        // Set Colour Planes to 'ForegroundColor' or to Black
        UINT8   PixelValueR = ForegroundColor ? ForegroundColor->r : 0;
        UINT8   PixelValueG = ForegroundColor ? ForegroundColor->g : 0;
        UINT8   PixelValueB = ForegroundColor ? ForegroundColor->b : 0;

        //BREAD_CRUMB(L"%s:  7c 2", FuncTag);
        #if REFIT_DEBUG > 0
        // DA-TAG: Limit logging to embedded banner and only once
        if (DefaultBanner && LogTextColour) {
            LogTextColour = FALSE;
            //BREAD_CRUMB(L"%s:  7c 2a 1", FuncTag);
            LOG_MSG(
                "%s      Colour (Text) ... %3d %3d %3d",
                (GlobalConfig.LogLevel <= MAXLOGLEVEL)
                    ? OffsetNext
                    : L"",
                PixelValueR,
                PixelValueG,
                PixelValueB
            );
            BRK_MAX("\n");
        }
        #endif

        //BREAD_CRUMB(L"%s:  7c 3", FuncTag);
        egSetPlane (PLPTR(NewImage, r), PixelValueR, PixelCount);
        egSetPlane (PLPTR(NewImage, g), PixelValueG, PixelCount);
        egSetPlane (PLPTR(NewImage, b), PixelValueB, PixelCount);
    }

    // Handle Alpha
    //BREAD_CRUMB(L"%s:  8", FuncTag);
    if (
        WantAlpha && (
            EmbeddedImage->PixelMode == EG_EIPIXELMODE_GRAY_ALPHA ||
            EmbeddedImage->PixelMode == EG_EIPIXELMODE_COLOR_ALPHA ||
            EmbeddedImage->PixelMode == EG_EIPIXELMODE_ALPHA ||
            EmbeddedImage->PixelMode == EG_EIPIXELMODE_ALPHA_INVERT
        )
    ) {
        //BREAD_CRUMB(L"%s:  8a 1", FuncTag);
        // Alpha is Required and Available
        // Add Alpha Mask if Available and Required
        if (EmbeddedImage->CompressMode == EG_EICOMPMODE_RLE) {
            //BREAD_CRUMB(L"%s:  8a 1a 1", FuncTag);
            egDecompressIcnsRLE (&CompData, &CompLen, PLPTR(NewImage, a), PixelCount);
        }
        else {
            //BREAD_CRUMB(L"%s:  8a 1b 1", FuncTag);
            egInsertPlane (CompData, PLPTR(NewImage, a), PixelCount);
            CompData += PixelCount;
        }

        //BREAD_CRUMB(L"%s:  8a 2", FuncTag);
        if (EmbeddedImage->PixelMode == EG_EIPIXELMODE_ALPHA_INVERT) {
            //BREAD_CRUMB(L"%s:  8a 2a 1", FuncTag);
            egInvertPlane (PLPTR(NewImage, a), PixelCount);
        }
    }
    else {
        //BREAD_CRUMB(L"%s:  8a 2", FuncTag);
        // Alpha is Unavailable or Not Required
        // Default to 'Opaque' if Alpha was Required but Unavailable or to 'Zero' if it was Not Required
        // NB: 'Zero' clears unused bytes and is not the opposite of opaque in this case
        egSetPlane (PLPTR(NewImage, a), WantAlpha ? 255 : 0, PixelCount);
    }

    BREAD_CRUMB(L"%s:  9 - END:- return EG_IMAGE NewImage = '%s'", FuncTag,
        NewImage ? L"Embedded Image Data" : L"NULL"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return NewImage;
} // EG_IMAGE * egPrepareEmbeddedImage()

//
// Compositing
//

VOID egRestrictImageArea (
    IN EG_IMAGE   *Image,
    IN UINTN       AreaPosX,
    IN UINTN       AreaPosY,
    IN OUT UINTN  *AreaWidth,
    IN OUT UINTN  *AreaHeight
) {
    if (Image && AreaWidth && AreaHeight) {
        if (AreaPosX >= Image->Width || AreaPosY >= Image->Height) {
            // out of bounds, operation has no effect
            *AreaWidth = *AreaHeight = 0;
        }
        else {
            // calculate affected area
            if (*AreaWidth  > Image->Width  - AreaPosX) *AreaWidth  = Image->Width  - AreaPosX;
            if (*AreaHeight > Image->Height - AreaPosY) *AreaHeight = Image->Height - AreaPosY;
        }
    }
} // VOID egRestrictImageArea()

VOID egFillImage (
    IN OUT EG_IMAGE  *CompImage,
    IN EG_PIXEL      *Color
) {
    UINTN       i;
    EG_PIXEL    FillColor;
    EG_PIXEL   *PixelPtr;

    if (CompImage && Color) {
        FillColor = *Color;
        if (!CompImage->HasAlpha) {
            FillColor.a = 0;
        }

        PixelPtr = CompImage->PixelData;
        for (i = 0; i < CompImage->Width * CompImage->Height; i++, PixelPtr++) {
            *PixelPtr = FillColor;
        }
    }
} // VOID egFillImage()

VOID egFillImageArea (
    IN OUT EG_IMAGE *CompImage,
    IN UINTN         AreaPosX,
    IN UINTN         AreaPosY,
    IN UINTN         AreaWidth,
    IN UINTN         AreaHeight,
    IN EG_PIXEL     *Color
) {
    UINTN        x, y;
    EG_PIXEL     FillColor;
    EG_PIXEL    *PixelPtr;
    EG_PIXEL    *PixelBasePtr;

    if (CompImage && Color) {
        egRestrictImageArea (CompImage, AreaPosX, AreaPosY, &AreaWidth, &AreaHeight);

        if (AreaWidth > 0) {
            FillColor = *Color;
            if (!CompImage->HasAlpha) {
                FillColor.a = 0;
            }

            PixelBasePtr = CompImage->PixelData + AreaPosY * CompImage->Width + AreaPosX;
            for (y = 0; y < AreaHeight; y++) {
                PixelPtr = PixelBasePtr;
                for (x = 0; x < AreaWidth; x++, PixelPtr++) {
                    *PixelPtr = FillColor;
                }
                PixelBasePtr += CompImage->Width;
            }
        }
    }
} // VOID egFillImageArea ()

VOID egRawCopy (
    IN OUT EG_PIXEL *CompBasePtr,
    IN EG_PIXEL     *TopBasePtr,
    IN UINTN         Width,
    IN UINTN         Height,
    IN UINTN         CompLineOffset,
    IN UINTN         TopLineOffset
) {
    UINTN       x, y;
    EG_PIXEL   *TopPtr, *CompPtr;

    if (CompBasePtr && TopBasePtr) {
        for (y = 0; y < Height; y++) {
            TopPtr  = TopBasePtr;
            CompPtr = CompBasePtr;

            for (x = 0; x < Width; x++) {
                *CompPtr = *TopPtr;
                TopPtr++, CompPtr++;
            }

            TopBasePtr  += TopLineOffset;
            CompBasePtr += CompLineOffset;
        }
    }
} // VOID egRawCopy()

VOID egRawCompose (
    IN OUT EG_PIXEL *CompBasePtr,
    IN EG_PIXEL     *TopBasePtr,
    IN UINTN         Width,
    IN UINTN         Height,
    IN UINTN         CompLineOffset,
    IN UINTN         TopLineOffset
) {
    UINTN        x, y;
    EG_PIXEL    *TopPtr, *CompPtr;
    UINTN        Alpha;
    UINTN        RevAlpha;
    UINTN        Temp;

    if (CompBasePtr && TopBasePtr) {
        for (y = 0; y < Height; y++) {
            TopPtr  = TopBasePtr;
            CompPtr = CompBasePtr;

            for (x = 0; x < Width; x++) {
                Alpha    = TopPtr->a;
                RevAlpha = 255 - Alpha;

                Temp       = (UINTN) CompPtr->b * RevAlpha + (UINTN) TopPtr->b * Alpha + 0x80;
                CompPtr->b = (Temp + (Temp >> 8)) >> 8;
                Temp       = (UINTN) CompPtr->g * RevAlpha + (UINTN) TopPtr->g * Alpha + 0x80;
                CompPtr->g = (Temp + (Temp >> 8)) >> 8;
                Temp       = (UINTN) CompPtr->r * RevAlpha + (UINTN) TopPtr->r * Alpha + 0x80;
                CompPtr->r = (Temp + (Temp >> 8)) >> 8;

                TopPtr++, CompPtr++;
            }

            TopBasePtr  += TopLineOffset;
            CompBasePtr += CompLineOffset;
        }
    }
} // VOID egRawCompose()

VOID egComposeImage (
    IN OUT EG_IMAGE *CompImage,
    IN EG_IMAGE     *TopImage,
    IN UINTN         PosX,
    IN UINTN         PosY
) {
    UINTN CompWidth;
    UINTN CompHeight;

    if (CompImage && TopImage) {
        CompWidth  = TopImage->Width;
        CompHeight = TopImage->Height;

        egRestrictImageArea (CompImage, PosX, PosY, &CompWidth, &CompHeight);

        // compose
        if (CompWidth > 0) {
            if (TopImage->HasAlpha) {
                egRawCompose (
                    CompImage->PixelData + PosY * CompImage->Width + PosX,
                    TopImage->PixelData,
                    CompWidth, CompHeight,
                    CompImage->Width, TopImage->Width
                );
            }
            else {
                egRawCopy (
                    CompImage->PixelData + PosY * CompImage->Width + PosX,
                    TopImage->PixelData,
                    CompWidth, CompHeight,
                    CompImage->Width, TopImage->Width
                );
            }
        }
    }
} // VOID egComposeImage()

//
// misc internal functions
//

VOID egInsertPlane (
    IN UINT8 *SrcDataPtr,
    IN UINT8 *DestPlanePtr,
    IN UINTN  PixelCount
) {
    UINTN i;

    if (SrcDataPtr && DestPlanePtr) {
        for (i = 0; i < PixelCount; i++) {
            *DestPlanePtr  = *SrcDataPtr++;
             DestPlanePtr += 4;
        }
    }
} // VOID egInsertPlane()

VOID egSetPlane (
    IN UINT8 *DestPlanePtr,
    IN UINT8  Value,
    IN UINTN  PixelCount
) {
    UINTN i;

    if (DestPlanePtr) {
        for (i = 0; i < PixelCount; i++) {
            *DestPlanePtr  = Value;
             DestPlanePtr += 4;
        }
    }
} // VOID egSetPlane()

VOID egCopyPlane (
    IN UINT8 *SrcPlanePtr,
    IN UINT8 *DestPlanePtr,
    IN UINTN  PixelCount
) {
    UINTN i;

    if (SrcPlanePtr && DestPlanePtr) {
        for (i = 0; i < PixelCount; i++) {
            *DestPlanePtr  = *SrcPlanePtr;
             DestPlanePtr += 4, SrcPlanePtr += 4;
        }
    }
} // VOID egCopyPlane()

/* EOF */
