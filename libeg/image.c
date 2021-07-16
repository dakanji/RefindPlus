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

#include "libegint.h"
#include "../refind/global.h"
#include "../refind/lib.h"
#include "../refind/screen.h"
#include "../refind/mystrings.h"
#include "../include/refit_call_wrapper.h"
#include "lodepng.h"
#include "libeg.h"
#include "log.h"

#define MAX_FILE_SIZE (1024*1024*1024)

// Multiplier for pseudo-floating-point operations in egScaleImage().
// A value of 4096 should keep us within limits on 32-bit systems, but I've
// seen some minor artifacts at this level, so give it a bit more precision
// on 64-bit systems....
#if defined(EFIX64) | defined(EFIAARCH64)
#define FP_MULTIPLIER (UINTN) 65536
#else
#define FP_MULTIPLIER (UINTN) 4096
#endif

#ifndef __MAKEWITH_GNUEFI
#define LibLocateHandle gBS->LocateHandleBuffer
#define LibOpenRoot EfiLibOpenRoot
#endif

//
// Basic image handling
//

EG_IMAGE * egCreateImage(IN UINTN Width, IN UINTN Height, IN BOOLEAN HasAlpha)
{
    EG_IMAGE        *NewImage;

    NewImage = (EG_IMAGE *) AllocatePool(sizeof(EG_IMAGE));
    if (NewImage == NULL)
        return NULL;
    NewImage->PixelData = (EG_PIXEL *) AllocatePool(Width * Height * sizeof(EG_PIXEL));
    if (NewImage->PixelData == NULL) {
        egFreeImage(NewImage);
        return NULL;
    }

    NewImage->Width = Width;
    NewImage->Height = Height;
    NewImage->HasAlpha = HasAlpha;
    return NewImage;
}

EG_IMAGE * egCreateFilledImage(IN UINTN Width, IN UINTN Height, IN BOOLEAN HasAlpha, IN EG_PIXEL *Color)
{
    EG_IMAGE        *NewImage;

    NewImage = egCreateImage(Width, Height, HasAlpha);
    if (NewImage == NULL)
        return NULL;

    egFillImage(NewImage, Color);
    return NewImage;
}

EG_IMAGE * egCopyImage(IN EG_IMAGE *Image)
{
    EG_IMAGE        *NewImage = NULL;

    if (Image != NULL)
       NewImage = egCreateImage(Image->Width, Image->Height, Image->HasAlpha);
    if (NewImage == NULL)
        return NULL;

    CopyMem(NewImage->PixelData, Image->PixelData, Image->Width * Image->Height * sizeof(EG_PIXEL));
    return NewImage;
}

// Returns a smaller image composed of the specified crop area from the larger area.
// If the specified area is larger than is in the original, returns NULL.
EG_IMAGE * egCropImage(IN EG_IMAGE *Image, IN UINTN StartX, IN UINTN StartY, IN UINTN Width, IN UINTN Height) {
   EG_IMAGE *NewImage = NULL;
   UINTN x, y;

   if (((StartX + Width) > Image->Width) || ((StartY + Height) > Image->Height))
      return NULL;

   NewImage = egCreateImage(Width, Height, Image->HasAlpha);
   if (NewImage == NULL)
      return NULL;

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
EG_IMAGE * egScaleImage(IN EG_IMAGE *Image, IN UINTN NewWidth, IN UINTN NewHeight) {
   EG_IMAGE *NewImage = NULL;
   EG_PIXEL a, b, c, d;
   UINTN x, y, Index ;
   UINTN i, j;
   UINTN Offset = 0;
   UINTN x_ratio, y_ratio, x_diff, y_diff;

   LOG(3, LOG_LINE_NORMAL, L"Scaling image to %d x %d", NewWidth, NewHeight);
   if ((Image == NULL) || (Image->Height == 0) || (Image->Width == 0) || (NewWidth == 0) || (NewHeight == 0)) {
      LOG(1, LOG_LINE_NORMAL, L"In egScaleImage(), Image is NULL or a size is 0");
      return NULL;
   }

   if ((Image->Width == NewWidth) && (Image->Height == NewHeight))
      return (egCopyImage(Image));

   NewImage = egCreateImage(NewWidth, NewHeight, Image->HasAlpha);
   if (NewImage == NULL) {
      LOG(1, LOG_LINE_NORMAL, L"In egScaleImage(), unable to create new image");
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
   LOG(3, LOG_LINE_NORMAL, L"Scaling of image complete");
   return NewImage;
} // EG_IMAGE * egScaleImage()

VOID egFreeImage(IN EG_IMAGE *Image)
{
    if (Image != NULL) {
        if (Image->PixelData != NULL)
            FreePool(Image->PixelData);
        FreePool(Image);
    }
}

//
// Basic file operations
//

EFI_STATUS egLoadFile(IN EFI_FILE *BaseDir, IN CHAR16 *FileName, OUT UINT8 **FileData, OUT UINTN *FileDataLength)
{
    EFI_STATUS          Status;
    EFI_FILE_HANDLE     FileHandle;
    EFI_FILE_INFO       *FileInfo;
    UINT64              ReadSize;
    UINTN               BufferSize;
    UINT8               *Buffer;

    if ((BaseDir == NULL) || (FileName == NULL))
       return EFI_NOT_FOUND;

    LOG(3, LOG_LINE_NORMAL, L"Loading file '%s'", FileName);
    Status = refit_call5_wrapper(BaseDir->Open, BaseDir, &FileHandle, FileName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    FileInfo = LibFileInfo(FileHandle);
    if (FileInfo == NULL) {
        refit_call1_wrapper(FileHandle->Close, FileHandle);
        return EFI_NOT_FOUND;
    }
    ReadSize = FileInfo->FileSize;
    if (ReadSize > MAX_FILE_SIZE)
        ReadSize = MAX_FILE_SIZE;
    FreePool(FileInfo);

    BufferSize = (UINTN)ReadSize;   // was limited to 1 GB above, so this is safe
    Buffer = (UINT8 *) AllocatePool(BufferSize);
    if (Buffer == NULL) {
        refit_call1_wrapper(FileHandle->Close, FileHandle);
        return EFI_OUT_OF_RESOURCES;
    }

    Status = refit_call3_wrapper(FileHandle->Read, FileHandle, &BufferSize, Buffer);
    refit_call1_wrapper(FileHandle->Close, FileHandle);
    if (EFI_ERROR(Status)) {
        FreePool(Buffer);
        return Status;
    }

    *FileData = Buffer;
    *FileDataLength = BufferSize;
    return EFI_SUCCESS;
}

EFI_STATUS egFindESP(OUT EFI_FILE_HANDLE *RootDir)
{
    EFI_STATUS          Status;
    UINTN               HandleCount = 0;
    EFI_HANDLE          *Handles;
    EFI_GUID            ESPGuid = ESP_GUID_VALUE;

    Status = LibLocateHandle(ByProtocol, &ESPGuid, NULL, &HandleCount, &Handles);
    if (!EFI_ERROR(Status) && HandleCount > 0) {
        *RootDir = LibOpenRoot(Handles[0]);
        if (*RootDir == NULL)
            Status = EFI_NOT_FOUND;
        FreePool(Handles);
    }
    return Status;
}

EFI_STATUS egSaveFile(IN EFI_FILE* BaseDir OPTIONAL, IN CHAR16 *FileName,
                      IN UINT8 *FileData, IN UINTN FileDataLength)
{
    EFI_STATUS          Status;
    EFI_FILE_HANDLE     FileHandle;
    UINTN               BufferSize;

    if (BaseDir == NULL) {
        Status = egFindESP(&BaseDir);
        if (EFI_ERROR(Status))
            return Status;
    }

    Status = refit_call5_wrapper(BaseDir->Open, BaseDir, &FileHandle, FileName,
                                 EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (EFI_ERROR(Status))
        return Status;

    if (FileDataLength > 0) {
        BufferSize = FileDataLength;
        Status = refit_call3_wrapper(FileHandle->Write, FileHandle, &BufferSize, FileData);
        refit_call1_wrapper(FileHandle->Close, FileHandle);
    } else {
        Status = refit_call1_wrapper(FileHandle->Delete, FileHandle);
    } // if/else (FileDataLength > 0)
    return Status;
}

//
// Loading images from files and embedded data
//

// Decode the specified image data. The IconSize parameter is relevant only
// for ICNS, for which it selects which ICNS sub-image is decoded.
// Returns a pointer to the resulting EG_IMAGE or NULL if decoding failed.
static EG_IMAGE * egDecodeAny(IN UINT8 *FileData, IN UINTN FileDataLength, IN UINTN IconSize, IN BOOLEAN WantAlpha)
{
   EG_IMAGE        *NewImage = NULL;

   NewImage = egDecodePNG(FileData, FileDataLength, IconSize, WantAlpha);
   if (NewImage == NULL)
      NewImage = egDecodeJPEG(FileData, FileDataLength, IconSize, WantAlpha);
   if (NewImage == NULL)
      NewImage = egDecodeBMP(FileData, FileDataLength, IconSize, WantAlpha);
   if (NewImage == NULL)
      NewImage = egDecodeICNS(FileData, FileDataLength, IconSize, WantAlpha);

   return NewImage;
}

EG_IMAGE * egLoadImage(IN EFI_FILE* BaseDir, IN CHAR16 *FileName, IN BOOLEAN WantAlpha)
{
    EFI_STATUS      Status;
    UINT8           *FileData;
    UINTN           FileDataLength;
    EG_IMAGE        *NewImage;

    if (BaseDir == NULL || FileName == NULL)
        return NULL;

    // load file
    Status = egLoadFile(BaseDir, FileName, &FileData, &FileDataLength);
    if (EFI_ERROR(Status))
        return NULL;

    // decode it
    NewImage = egDecodeAny(FileData, FileDataLength, 128 /* arbitrary value */, WantAlpha);
    FreePool(FileData);

    return NewImage;
}

// Load an icon from (BaseDir)/Path, extracting the icon of size IconSize x IconSize.
// Returns a pointer to the image data, or NULL if the icon could not be loaded.
EG_IMAGE * egLoadIcon(IN EFI_FILE* BaseDir, IN CHAR16 *Path, IN UINTN IconSize)
{
    EFI_STATUS      Status;
    UINT8           *FileData;
    UINTN           FileDataLength;
    EG_IMAGE        *Image, *NewImage;

    if (BaseDir == NULL || Path == NULL)
        return NULL;

    // load file
    Status = egLoadFile(BaseDir, Path, &FileData, &FileDataLength);
    if (EFI_ERROR(Status))
        return NULL;

    // decode it
    Image = egDecodeAny(FileData, FileDataLength, IconSize, TRUE);
    FreePool(FileData);
    if ((Image->Width != IconSize) || (Image->Height != IconSize)) {
        NewImage = egScaleImage(Image, IconSize, IconSize);
        if (NewImage) {
            egFreeImage(Image);
            LOG(4, LOG_LINE_NORMAL, L"In egLoadIcon(), have called egFreeImage()");
            Image = NewImage;
        } else {
            LOG(1, LOG_LINE_NORMAL, L"Warning: Unable to scale icon from %d x %d to %d x %d from '%s'",
                Image->Width, Image->Height, IconSize, IconSize, Path);
            Print(L"Warning: Unable to scale icon from %d x %d to %d x %d from '%s'\n",
                  Image->Width, Image->Height, IconSize, IconSize, Path);
        }
    }

    return Image;
} // EG_IMAGE *egLoadIcon()

// Returns an icon of any type from the specified subdirectory using the specified
// base name. All directory references are relative to BaseDir. For instance, if
// SubdirName is "myicons" and BaseName is "os_linux", this function will return
// an image based on "myicons/os_linux.icns" or "myicons/os_linux.png", in that
// order of preference. Returns NULL if no such file is a valid icon file.
EG_IMAGE * egLoadIconAnyType(IN EFI_FILE *BaseDir, IN CHAR16 *SubdirName, IN CHAR16 *BaseName, IN UINTN IconSize) {
    EG_IMAGE *Image = NULL;
    CHAR16 *Extension;
    CHAR16 *FileName;
    UINTN i = 0;

    while (((Extension = FindCommaDelimited(ICON_EXTENSIONS, i++)) != NULL) && (Image == NULL)) {
        FileName = PoolPrint(L"%s\\%s.%s", SubdirName, BaseName, Extension);
        Image = egLoadIcon(BaseDir, FileName, IconSize);
        LOG(4, LOG_LINE_NORMAL, L"Have loaded Image in egLoadIconAnyType()");
        MyFreePool(Extension);
        MyFreePool(FileName);
    } // while()

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
EG_IMAGE * egFindIcon(IN CHAR16 *BaseName, IN UINTN IconSize) {
   EG_IMAGE *Image = NULL;

   if (GlobalConfig.IconsDir != NULL) {
      Image = egLoadIconAnyType(SelfDir, GlobalConfig.IconsDir, BaseName, IconSize);
   }

   if (Image == NULL) {
      Image = egLoadIconAnyType(SelfDir, DEFAULT_ICONS_DIR, BaseName, IconSize);
   }

   return Image;
} // EG_IMAGE * egFindIcon()

EG_IMAGE * egPrepareEmbeddedImage(IN EG_EMBEDDED_IMAGE *EmbeddedImage, IN BOOLEAN WantAlpha)
{
    EG_IMAGE            *NewImage;
    UINT8               *CompData;
    UINTN               CompLen;
    UINTN               PixelCount;

    if (EmbeddedImage == NULL)
        return NULL;

    // sanity check
    if (EmbeddedImage->PixelMode > EG_MAX_EIPIXELMODE ||
        (EmbeddedImage->CompressMode != EG_EICOMPMODE_NONE && EmbeddedImage->CompressMode != EG_EICOMPMODE_RLE))
        return NULL;

    // allocate image structure and pixel buffer
    NewImage = egCreateImage(EmbeddedImage->Width, EmbeddedImage->Height, WantAlpha);
    if (NewImage == NULL)
        return NULL;

    CompData = (UINT8 *)EmbeddedImage->Data;   // drop const
    CompLen  = EmbeddedImage->DataLength;
    PixelCount = EmbeddedImage->Width * EmbeddedImage->Height;

    // FUTURE: for EG_EICOMPMODE_EFICOMPRESS, decompress whole data block here

    if (EmbeddedImage->PixelMode == EG_EIPIXELMODE_GRAY ||
        EmbeddedImage->PixelMode == EG_EIPIXELMODE_GRAY_ALPHA) {

        // copy grayscale plane and expand
        if (EmbeddedImage->CompressMode == EG_EICOMPMODE_RLE) {
            egDecompressIcnsRLE(&CompData, &CompLen, PLPTR(NewImage, r), PixelCount);
        } else {
            egInsertPlane(CompData, PLPTR(NewImage, r), PixelCount);
            CompData += PixelCount;
        }
        egCopyPlane(PLPTR(NewImage, r), PLPTR(NewImage, g), PixelCount);
        egCopyPlane(PLPTR(NewImage, r), PLPTR(NewImage, b), PixelCount);

    } else if (EmbeddedImage->PixelMode == EG_EIPIXELMODE_COLOR ||
               EmbeddedImage->PixelMode == EG_EIPIXELMODE_COLOR_ALPHA) {

        // copy color planes
        if (EmbeddedImage->CompressMode == EG_EICOMPMODE_RLE) {
            egDecompressIcnsRLE(&CompData, &CompLen, PLPTR(NewImage, r), PixelCount);
            egDecompressIcnsRLE(&CompData, &CompLen, PLPTR(NewImage, g), PixelCount);
            egDecompressIcnsRLE(&CompData, &CompLen, PLPTR(NewImage, b), PixelCount);
        } else {
            egInsertPlane(CompData, PLPTR(NewImage, r), PixelCount);
            CompData += PixelCount;
            egInsertPlane(CompData, PLPTR(NewImage, g), PixelCount);
            CompData += PixelCount;
            egInsertPlane(CompData, PLPTR(NewImage, b), PixelCount);
            CompData += PixelCount;
        }

    } else {

        // set color planes to black
        egSetPlane(PLPTR(NewImage, r), 0, PixelCount);
        egSetPlane(PLPTR(NewImage, g), 0, PixelCount);
        egSetPlane(PLPTR(NewImage, b), 0, PixelCount);

    }

    if (WantAlpha && (EmbeddedImage->PixelMode == EG_EIPIXELMODE_GRAY_ALPHA ||
                      EmbeddedImage->PixelMode == EG_EIPIXELMODE_COLOR_ALPHA ||
                      EmbeddedImage->PixelMode == EG_EIPIXELMODE_ALPHA)) {

        // copy alpha plane
        if (EmbeddedImage->CompressMode == EG_EICOMPMODE_RLE) {
            egDecompressIcnsRLE(&CompData, &CompLen, PLPTR(NewImage, a), PixelCount);
        } else {
            egInsertPlane(CompData, PLPTR(NewImage, a), PixelCount);
            CompData += PixelCount;
        }

    } else {
        egSetPlane(PLPTR(NewImage, a), WantAlpha ? 255 : 0, PixelCount);
    }

    return NewImage;
}

//
// Compositing
//

VOID egRestrictImageArea(IN EG_IMAGE *Image,
                         IN UINTN AreaPosX, IN UINTN AreaPosY,
                         IN OUT UINTN *AreaWidth, IN OUT UINTN *AreaHeight)
{
    if (Image && AreaWidth && AreaHeight) {
        if (AreaPosX >= Image->Width || AreaPosY >= Image->Height) {
            // out of bounds, operation has no effect
            *AreaWidth  = 0;
            *AreaHeight = 0;
        } else {
            // calculate affected area
            if (*AreaWidth > Image->Width - AreaPosX)
                *AreaWidth = Image->Width - AreaPosX;
            if (*AreaHeight > Image->Height - AreaPosY)
                *AreaHeight = Image->Height - AreaPosY;
        }
    }
}

VOID egFillImage(IN OUT EG_IMAGE *CompImage, IN EG_PIXEL *Color)
{
    UINTN       i;
    EG_PIXEL    FillColor;
    EG_PIXEL    *PixelPtr;

    if (CompImage && Color) {
        FillColor = *Color;
        if (!CompImage->HasAlpha)
            FillColor.a = 0;

        PixelPtr = CompImage->PixelData;
        for (i = 0; i < CompImage->Width * CompImage->Height; i++, PixelPtr++)
            *PixelPtr = FillColor;
    }
}

VOID egFillImageArea(IN OUT EG_IMAGE *CompImage,
                     IN UINTN AreaPosX, IN UINTN AreaPosY,
                     IN UINTN AreaWidth, IN UINTN AreaHeight,
                     IN EG_PIXEL *Color)
{
    UINTN       x, y;
    EG_PIXEL    FillColor;
    EG_PIXEL    *PixelPtr;
    EG_PIXEL    *PixelBasePtr;

    if (CompImage && Color) {
        egRestrictImageArea(CompImage, AreaPosX, AreaPosY, &AreaWidth, &AreaHeight);

        if (AreaWidth > 0) {
            FillColor = *Color;
            if (!CompImage->HasAlpha)
                FillColor.a = 0;

            PixelBasePtr = CompImage->PixelData + AreaPosY * CompImage->Width + AreaPosX;
            for (y = 0; y < AreaHeight; y++) {
                PixelPtr = PixelBasePtr;
                for (x = 0; x < AreaWidth; x++, PixelPtr++)
                    *PixelPtr = FillColor;
                PixelBasePtr += CompImage->Width;
            }
        }
    }
}

VOID egRawCopy(IN OUT EG_PIXEL *CompBasePtr, IN EG_PIXEL *TopBasePtr,
               IN UINTN Width, IN UINTN Height,
               IN UINTN CompLineOffset, IN UINTN TopLineOffset)
{
    UINTN       x, y;
    EG_PIXEL    *TopPtr, *CompPtr;

    if (CompBasePtr && TopBasePtr) {
        for (y = 0; y < Height; y++) {
            TopPtr = TopBasePtr;
            CompPtr = CompBasePtr;
            for (x = 0; x < Width; x++) {
                *CompPtr = *TopPtr;
                TopPtr++, CompPtr++;
            }
            TopBasePtr += TopLineOffset;
            CompBasePtr += CompLineOffset;
        }
    }
}

VOID egRawCompose(IN OUT EG_PIXEL *CompBasePtr, IN EG_PIXEL *TopBasePtr,
                  IN UINTN Width, IN UINTN Height,
                  IN UINTN CompLineOffset, IN UINTN TopLineOffset)
{
    UINTN       x, y;
    EG_PIXEL    *TopPtr, *CompPtr;
    UINTN       Alpha;
    UINTN       RevAlpha;
    UINTN       Temp;

    if (CompBasePtr && TopBasePtr) {
        for (y = 0; y < Height; y++) {
            TopPtr = TopBasePtr;
            CompPtr = CompBasePtr;
            for (x = 0; x < Width; x++) {
                Alpha = TopPtr->a;
                RevAlpha = 255 - Alpha;
                Temp = (UINTN)CompPtr->b * RevAlpha + (UINTN)TopPtr->b * Alpha + 0x80;
                CompPtr->b = (Temp + (Temp >> 8)) >> 8;
                Temp = (UINTN)CompPtr->g * RevAlpha + (UINTN)TopPtr->g * Alpha + 0x80;
                CompPtr->g = (Temp + (Temp >> 8)) >> 8;
                Temp = (UINTN)CompPtr->r * RevAlpha + (UINTN)TopPtr->r * Alpha + 0x80;
                CompPtr->r = (Temp + (Temp >> 8)) >> 8;
                TopPtr++, CompPtr++;
            }
            TopBasePtr += TopLineOffset;
            CompBasePtr += CompLineOffset;
        }
    }
}

VOID egComposeImage(IN OUT EG_IMAGE *CompImage, IN EG_IMAGE *TopImage, IN UINTN PosX, IN UINTN PosY)
{
    UINTN       CompWidth, CompHeight;

    if (CompImage && TopImage) {
        CompWidth  = TopImage->Width;
        CompHeight = TopImage->Height;
        egRestrictImageArea(CompImage, PosX, PosY, &CompWidth, &CompHeight);

        // compose
        if (CompWidth > 0) {
            if (TopImage->HasAlpha) {
                egRawCompose(CompImage->PixelData + PosY * CompImage->Width + PosX, TopImage->PixelData,
                            CompWidth, CompHeight, CompImage->Width, TopImage->Width);
            } else {
                egRawCopy(CompImage->PixelData + PosY * CompImage->Width + PosX, TopImage->PixelData,
                        CompWidth, CompHeight, CompImage->Width, TopImage->Width);
            }
        }
    }
} /* VOID egComposeImage() */

//
// misc internal functions
//

VOID egInsertPlane(IN UINT8 *SrcDataPtr, IN UINT8 *DestPlanePtr, IN UINTN PixelCount)
{
    UINTN i;

    if (SrcDataPtr && DestPlanePtr) {
        for (i = 0; i < PixelCount; i++) {
            *DestPlanePtr = *SrcDataPtr++;
            DestPlanePtr += 4;
        }
    }
}

VOID egSetPlane(IN UINT8 *DestPlanePtr, IN UINT8 Value, IN UINTN PixelCount)
{
    UINTN i;

    if (DestPlanePtr) {
        for (i = 0; i < PixelCount; i++) {
            *DestPlanePtr = Value;
            DestPlanePtr += 4;
        }
    }
}

VOID egCopyPlane(IN UINT8 *SrcPlanePtr, IN UINT8 *DestPlanePtr, IN UINTN PixelCount)
{
    UINTN i;

    if (SrcPlanePtr && DestPlanePtr) {
        for (i = 0; i < PixelCount; i++) {
            *DestPlanePtr = *SrcPlanePtr;
            DestPlanePtr += 4, SrcPlanePtr += 4;
        }
    }
}

/* EOF */
