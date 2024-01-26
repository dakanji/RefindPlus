/*
 * BootMaster/icns.c
 * Loader for .icns icon files
 *
 * Copyright (c) 2006-2007 Christoph Pfisterer
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
 * Modified for eEFInd
 * Copyright (c) 2021 Roderick W Smith
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2022 Dayo Akanji (sf.net/u/dakanji/profile)
 * Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
 *
 * Modifications distributed under the preceding terms.
 */


#include "global.h"
#include "lib.h"
#include "icns.h"
#include "config.h"
#include "mystrings.h"
#include "../BootMaster/screenmgt.h"
#include "../include/egemb_tool_clean_nvram.h"

//
// well-known icons
//

typedef struct {
    EG_IMAGE    *Image;
    CHAR16      *FileName;
    UINTN        IconSize;
} BUILTIN_ICON;

BUILTIN_ICON BuiltinIconTable[BUILTIN_ICON_COUNT] = {
   { NULL, L"func_about", ICON_SIZE_SMALL },
   { NULL, L"func_reset", ICON_SIZE_SMALL },
   { NULL, L"func_shutdown", ICON_SIZE_SMALL },
   { NULL, L"func_exit", ICON_SIZE_SMALL },
   { NULL, L"func_firmware", ICON_SIZE_SMALL },
   { NULL, L"func_csr_rotate", ICON_SIZE_SMALL },
   { NULL, L"func_hidden", ICON_SIZE_SMALL },
   { NULL, L"func_install", ICON_SIZE_SMALL },
   { NULL, L"func_bootorder", ICON_SIZE_SMALL },
   { NULL, L"tool_shell", ICON_SIZE_SMALL },
   { NULL, L"tool_part", ICON_SIZE_SMALL },
   { NULL, L"tool_rescue", ICON_SIZE_SMALL },
   { NULL, L"tool_apple_rescue", ICON_SIZE_SMALL },
   { NULL, L"tool_windows_rescue", ICON_SIZE_SMALL },
   { NULL, L"tool_mok_tool", ICON_SIZE_SMALL },
   { NULL, L"tool_fwupdate", ICON_SIZE_SMALL },
   { NULL, L"tool_memtest", ICON_SIZE_SMALL },
   { NULL, L"tool_netboot", ICON_SIZE_SMALL },
   { NULL, L"vol_internal", ICON_SIZE_BADGE },
   { NULL, L"vol_external", ICON_SIZE_BADGE },
   { NULL, L"vol_optical", ICON_SIZE_BADGE },
   { NULL, L"vol_net", ICON_SIZE_BADGE },
   { NULL, L"vol_efi", ICON_SIZE_BADGE },
   { NULL, L"mouse", ICON_SIZE_MOUSE },
   { NULL, L"tool_clean_nvram", ICON_SIZE_SMALL }
};

static
EG_IMAGE * DummyImageEx (
    IN UINTN PixelSize
) {
    UINTN            x, y, LineOffset;
    CHAR8           *Ptr, *YPtr;
    EG_PIXEL         BasePixel = { 0x00, 0x00, 0x00, 0 };

    static EG_IMAGE        *Image = NULL;

    if (Image) {
        return Image;
    }

    Image = egCreateFilledImage (PixelSize, PixelSize, TRUE, &BasePixel);
    if (!Image) {
        return NULL;
    }

    LineOffset = PixelSize * 4;
    YPtr = (CHAR8 *) Image->PixelData + ((PixelSize - 32) >> 1) * (LineOffset + 4);

    for (y = 0; y < 32; y++) {
        Ptr = YPtr;
        for (x = 0; x < 32; x++) {
            if (((x + y) % 12) < 6) {
                *Ptr++ = 0;
                *Ptr++ = 0;
                *Ptr++ = 0;
            }
            else {
                *Ptr++ = 0;
                *Ptr++ = 255;
                *Ptr++ = 255;
            }
            *Ptr++ = 144;
        } // for x =0
        YPtr += LineOffset;
    } // for y = 0

    return Image;
} // EG_IMAGE * DummyImageEx()

EG_IMAGE * BuiltinIcon (
    IN UINTN Id
) {
    if (Id >= BUILTIN_ICON_COUNT) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"Invalid Builtin Icon Request");
        #endif

        // Early Return
        return NULL;
    }

    if (BuiltinIconTable[Id].Image) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID, L"Loaded Cached Builtin Icon:- '%s'", BuiltinIconTable[Id].FileName);
        #endif
    }
    else {
        BuiltinIconTable[Id].Image = egFindIcon (
            BuiltinIconTable[Id].FileName,
            GlobalConfig.IconSizes[BuiltinIconTable[Id].IconSize]
        );
        if (BuiltinIconTable[Id].Image == NULL) {
            if (Id == BUILTIN_ICON_TOOL_NVRAMCLEAN) {
                BuiltinIconTable[Id].Image = egPrepareEmbeddedImage (&egemb_tool_clean_nvram, FALSE, NULL);
            }

            if (BuiltinIconTable[Id].Image == NULL) {
                BuiltinIconTable[Id].Image = DummyImageEx (GlobalConfig.IconSizes[BuiltinIconTable[Id].IconSize]);
            }
        }

        #if REFIT_DEBUG > 0
        if (BuiltinIconTable[Id].Image) {
            ALT_LOG(1, LOG_THREE_STAR_MID, L"Saved in Icon Cache:- '%s'", BuiltinIconTable[Id].FileName);
        }
        #endif
    }

    return egCopyImage (BuiltinIconTable[Id].Image);
} // EG_IMAGE * BuiltinIcon()

//
// Load an icon for an operating system
//

// Load an OS icon from among the comma-delimited list provided in OSIconName.
// Searches for icons with extensions in the ICON_EXTENSIONS list (via
// egFindIcon()).
// Returns image data. On failure, returns an ugly "dummy" icon.
EG_IMAGE * LoadOSIcon (
    IN  CHAR16  *OSIconName OPTIONAL,
    IN  CHAR16  *FallbackIconName,
    IN  BOOLEAN  BootLogo
) {
    EG_IMAGE        *Image = NULL;
    CHAR16          *CutoutName, *BaseName;
    UINTN            Index = 0;

    if (!AllowGraphicsMode) {
        // skip loading if it is not used anyway
        return NULL;
    }

    // First, try to find an icon from the OSIconName list.
    while ((Image == NULL) &&
        ((CutoutName = FindCommaDelimited (OSIconName, Index++)) != NULL)
    ) {
        BaseName = PoolPrint (L"%s_%s", BootLogo ? L"boot" : L"os", CutoutName);
        Image    = egFindIcon (BaseName, GlobalConfig.IconSizes[ICON_SIZE_BIG]);
        MY_FREE_POOL(CutoutName);
        MY_FREE_POOL(BaseName);
    }

    // If that fails, try again using the FallbackIconName.
    if (Image == NULL) {
        BaseName = PoolPrint (L"%s_%s", BootLogo ? L"boot" : L"os", FallbackIconName);

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Trying to Find an Icon From '%s'", BaseName);
        #endif

        Image = egFindIcon (BaseName, GlobalConfig.IconSizes[ICON_SIZE_BIG]);
        MY_FREE_POOL(BaseName);
    }

    // If that fails and if BootLogo was set, try again using the "os_" start of the name.
    if (BootLogo && (Image == NULL)) {
        BaseName = PoolPrint (L"os_%s", FallbackIconName);

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Trying to Find an Icon From '%s'", BaseName);
        #endif

        Image = egFindIcon (BaseName, GlobalConfig.IconSizes[ICON_SIZE_BIG]);
        MY_FREE_POOL(BaseName);
    }

    // If that fails try again using the "unknown" icon.
    if (Image == NULL && !MyStriCmp (FallbackIconName, L"unknown")) {
        BaseName = PoolPrint (L"%s_unknown", BootLogo ? L"boot" : L"os");

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Trying to Find an Icon From '%s'", BaseName);
        #endif

        Image = egFindIcon (BaseName, GlobalConfig.IconSizes[ICON_SIZE_BIG]);
        MY_FREE_POOL(BaseName);

        // If that still fails try again using the "os_unknown" icon specifically.
        if (Image == NULL) {
            BaseName = StrDuplicate (L"os_unknown");

            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL, L"Trying to Find an Icon From '%s'", BaseName);
            #endif

            Image = egFindIcon (BaseName, GlobalConfig.IconSizes[ICON_SIZE_BIG]);
            MY_FREE_POOL(BaseName);
        }
    }

    // If all of these fail, return the dummy image.
    if (Image == NULL) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Setting dummy image");
        #endif

        Image = DummyImageEx (GlobalConfig.IconSizes[ICON_SIZE_BIG]);
    }

    return egCopyImage (Image);
} // EG_IMAGE * LoadOSIcon()

EG_IMAGE * DummyImage (
    IN UINTN PixelSize
) {
    static EG_IMAGE *Image = NULL;

    if (Image == NULL) {
        Image = DummyImageEx (PixelSize);
    }

    return egCopyImage (Image);
} // EG_IMAGE * DummyImage()
