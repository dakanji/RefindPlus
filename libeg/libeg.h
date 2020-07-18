/*
 * libeg/libeg.h
 * EFI graphics library header for users
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

#ifndef __LIBEG_LIBEG_H__
#define __LIBEG_LIBEG_H__

#ifndef __MAKEWITH_GNUEFI
#include "../include/tiano_includes.h"
#endif

/* types */

typedef enum ColorTypes {
   white,
   black
} Colors;

/* This should be compatible with EFI_UGA_PIXEL */
typedef struct {
    UINT8 b, g, r, a;
} EG_PIXEL;

// Some colors for EG_IMAGE
#define COLOR_LIGHTBLUE {255, 175, 100, 0}
#define COLOR_RED {0, 0, 200, 0}

typedef struct {
    UINTN       Width;
    UINTN       Height;
    BOOLEAN     HasAlpha;
    EG_PIXEL    *PixelData;
} EG_IMAGE;

#define EG_EIPIXELMODE_GRAY         (0)
#define EG_EIPIXELMODE_GRAY_ALPHA   (1)
#define EG_EIPIXELMODE_COLOR        (2)
#define EG_EIPIXELMODE_COLOR_ALPHA  (3)
#define EG_EIPIXELMODE_ALPHA        (4)
#define EG_MAX_EIPIXELMODE          EG_EIPIXELMODE_ALPHA

#define EG_EICOMPMODE_NONE          (0)
#define EG_EICOMPMODE_RLE           (1)
#define EG_EICOMPMODE_EFICOMPRESS   (2)

#define ICON_EXTENSIONS L"png,icns,jpg,jpeg,bmp"

typedef struct {
    UINTN       Width;
    UINTN       Height;
    UINTN       PixelMode;
    UINTN       CompressMode;
    const UINT8 *Data;
    UINTN       DataLength;
} EG_EMBEDDED_IMAGE;

/* functions */

VOID egInitScreen(VOID);
BOOLEAN egGetResFromMode(UINTN *ModeWidth, UINTN *Height);
VOID egGetScreenSize(OUT UINTN *ScreenWidth, OUT UINTN *ScreenHeight);
CHAR16 * egScreenDescription(VOID);
BOOLEAN egHasGraphicsMode(VOID);
BOOLEAN egIsGraphicsModeEnabled(VOID);
VOID egSetGraphicsModeEnabled(IN BOOLEAN Enable);
// NOTE: Even when egHasGraphicsMode() returns FALSE, you should
//  call egSetGraphicsModeEnabled(FALSE) to ensure the system
//  is running in text mode. egHasGraphicsMode() only determines
//  if libeg can draw to the screen in graphics mode.

EG_IMAGE * egCreateImage(IN UINTN Width, IN UINTN Height, IN BOOLEAN HasAlpha);
EG_IMAGE * egCreateFilledImage(IN UINTN Width, IN UINTN Height, IN BOOLEAN HasAlpha, IN EG_PIXEL *Color);
EG_IMAGE * egCopyImage(IN EG_IMAGE *Image);
EG_IMAGE * egCropImage(IN EG_IMAGE *Image, IN UINTN StartX, IN UINTN StartY, IN UINTN Width, IN UINTN Height);
EG_IMAGE * egScaleImage(EG_IMAGE *Image, UINTN NewWidth, UINTN NewHeight);
VOID egFreeImage(IN EG_IMAGE *Image);

EG_IMAGE * egLoadImage(IN EFI_FILE* BaseDir, IN CHAR16 *FileName, IN BOOLEAN WantAlpha);
EG_IMAGE * egLoadIcon(IN EFI_FILE* BaseDir, IN CHAR16 *FileName, IN UINTN IconSize);
EG_IMAGE * egLoadIconAnyType(IN EFI_FILE *BaseDir, IN CHAR16 *SubdirName, IN CHAR16 *BaseName, IN UINTN IconSize);
EG_IMAGE * egFindIcon(IN CHAR16 *BaseName, IN UINTN IconSize);
EG_IMAGE * egPrepareEmbeddedImage(IN EG_EMBEDDED_IMAGE *EmbeddedImage, IN BOOLEAN WantAlpha);

EG_IMAGE * egEnsureImageSize(IN EG_IMAGE *Image, IN UINTN Width, IN UINTN Height, IN EG_PIXEL *Color);

EFI_STATUS egLoadFile(IN EFI_FILE* BaseDir, IN CHAR16 *FileName,
                      OUT UINT8 **FileData, OUT UINTN *FileDataLength);
EFI_STATUS egFindESP(OUT EFI_FILE_HANDLE *RootDir);
EFI_STATUS egSaveFile(IN EFI_FILE* BaseDir OPTIONAL, IN CHAR16 *FileName,
                      IN UINT8 *FileData, IN UINTN FileDataLength);

VOID egFillImage(IN OUT EG_IMAGE *CompImage, IN EG_PIXEL *Color);
VOID egFillImageArea(IN OUT EG_IMAGE *CompImage,
                     IN UINTN AreaPosX, IN UINTN AreaPosY,
                     IN UINTN AreaWidth, IN UINTN AreaHeight,
                     IN EG_PIXEL *Color);
VOID egComposeImage(IN OUT EG_IMAGE *CompImage, IN EG_IMAGE *TopImage, IN UINTN PosX, IN UINTN PosY);

UINTN egGetFontHeight(VOID);
UINTN egGetFontCellWidth(VOID);
UINTN egComputeTextWidth(IN CHAR16 *Text);
VOID egMeasureText(IN CHAR16 *Text, OUT UINTN *Width, OUT UINTN *Height);
VOID egRenderText(IN CHAR16 *Text, IN OUT EG_IMAGE *CompImage, IN UINTN PosX, IN UINTN PosY, IN UINT8 BGBrightness);
VOID egLoadFont(IN CHAR16 *Filename);

VOID egClearScreen(IN EG_PIXEL *Color);
VOID egDrawImage(IN EG_IMAGE *Image, IN UINTN ScreenPosX, IN UINTN ScreenPosY);
VOID egDrawImageWithTransparency(EG_IMAGE *Image, EG_IMAGE *BadgeImage, UINTN XPos, UINTN YPos, UINTN Width, UINTN Height);
VOID egDrawImageArea(IN EG_IMAGE *Image,
                     IN UINTN AreaPosX, IN UINTN AreaPosY,
                     IN UINTN AreaWidth, IN UINTN AreaHeight,
                     IN UINTN ScreenPosX, IN UINTN ScreenPosY);
VOID egDisplayMessage(IN CHAR16 *Text, EG_PIXEL *BGColor, UINTN PositionCode);
EG_IMAGE * egCopyScreen(VOID);
EG_IMAGE * egCopyScreenArea(UINTN XPos, UINTN YPos, UINTN Width, UINTN Height);
VOID egScreenShot(VOID);
BOOLEAN egSetTextMode(UINT32 RequestedMode);

EG_IMAGE * egDecodePNG(IN UINT8 *FileData, IN UINTN FileDataLength, IN UINTN IconSize, IN BOOLEAN WantAlpha);
EG_IMAGE * egDecodeJPEG(IN UINT8 *FileData, IN UINTN FileDataLength, IN UINTN IconSize, IN BOOLEAN WantAlpha);

#endif /* __LIBEG_LIBEG_H__ */

/* EOF */
