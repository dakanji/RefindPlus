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
 * Modified for rEFInd
 * Copyright (c) 2021 Roderick W Smith
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2024 Dayo Akanji (sf.net/u/dakanji/profile)
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

BUILTIN_ICON TableBuiltinIconOS[BASE_OS_ICON_COUNT] = {
    { NULL, L"os_mac",              ICON_SIZE_BIG   },
    { NULL, L"os_win",              ICON_SIZE_BIG   },
    { NULL, L"os_win8",             ICON_SIZE_BIG   },
    { NULL, L"os_linux",            ICON_SIZE_BIG   },
    { NULL, L"os_legacy",           ICON_SIZE_BIG   },
    { NULL, L"os_clover",           ICON_SIZE_BIG   },
    { NULL, L"os_opencore",         ICON_SIZE_BIG   },
    { NULL, L"os_unknown",          ICON_SIZE_BIG   },
    { NULL, L"os_dummy",            ICON_SIZE_BIG   },
    { NULL, L"os_uefi",             ICON_SIZE_BIG   }
};

BUILTIN_ICON BuiltinIconTable[BUILTIN_ICON_COUNT] = {
    { NULL, L"func_about",          ICON_SIZE_SMALL },
    { NULL, L"func_reset",          ICON_SIZE_SMALL },
    { NULL, L"func_shutdown",       ICON_SIZE_SMALL },
    { NULL, L"func_exit",           ICON_SIZE_SMALL },
    { NULL, L"func_firmware",       ICON_SIZE_SMALL },
    { NULL, L"func_csr_rotate",     ICON_SIZE_SMALL },
    { NULL, L"func_hidden",         ICON_SIZE_SMALL },
    { NULL, L"func_install",        ICON_SIZE_SMALL },
    { NULL, L"func_bootorder",      ICON_SIZE_SMALL },
    { NULL, L"tool_shell",          ICON_SIZE_SMALL },
    { NULL, L"tool_part",           ICON_SIZE_SMALL },
    { NULL, L"tool_rescue",         ICON_SIZE_SMALL },
    { NULL, L"tool_apple_rescue",   ICON_SIZE_SMALL },
    { NULL, L"tool_windows_rescue", ICON_SIZE_SMALL },
    { NULL, L"tool_mok_tool",       ICON_SIZE_SMALL },
    { NULL, L"tool_fwupdate",       ICON_SIZE_SMALL },
    { NULL, L"tool_memtest",        ICON_SIZE_SMALL },
    { NULL, L"tool_netboot",        ICON_SIZE_SMALL },
    { NULL, L"vol_internal",        ICON_SIZE_BADGE },
    { NULL, L"vol_external",        ICON_SIZE_BADGE },
    { NULL, L"vol_optical",         ICON_SIZE_BADGE },
    { NULL, L"vol_net",             ICON_SIZE_BADGE },
    { NULL, L"vol_efi",             ICON_SIZE_BADGE },
    { NULL, L"mouse",               ICON_SIZE_MOUSE },
    { NULL, L"tool_clean_nvram",    ICON_SIZE_SMALL }
};


static
EG_IMAGE * DummyImageEx (
    IN UINTN PixelSize
) {
    UINTN            LineOffset;
    UINTN            x, y;
    CHAR8           *Ptr;
    CHAR8           *YPtr;
    EG_PIXEL         BasePixel = { 0x00, 0x00, 0x00, 0 };

    static EG_IMAGE *Image = NULL;


    if (Image) {
        return Image;
    }

    Image = egCreateFilledImage (PixelSize, PixelSize, TRUE, &BasePixel);
    if (Image == NULL) {
        return NULL;
    }

    LineOffset = PixelSize * 4;
    YPtr = (CHAR8 *) Image->PixelData + (((PixelSize - 32) >> 1) * (LineOffset + 4));

    for (y = 0; y < 32; y++) {
        Ptr = YPtr;
        for (x = 0; x < 32; x++) {
            if (((x + y) % 12) < 6) {
                *Ptr++ =   0;
                *Ptr++ =   0;
                *Ptr++ =   0;
            }
            else {
                *Ptr++ =   0;
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
    if (!AllowGraphicsMode) {
        // Early Return
        return NULL;
    }

    if (Id >= BUILTIN_ICON_COUNT) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"Invalid Builtin Icon Request");
        #endif

        // Early Return
        return NULL;
    }

    if (BuiltinIconTable[Id].Image != NULL) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"Use Cached Icon:- '%s'",
            BuiltinIconTable[Id].FileName
        );
        #endif
    }
    else {
        BuiltinIconTable[Id].Image = egFindIcon (
            BuiltinIconTable[Id].FileName,
            GlobalConfig.IconSizes[BuiltinIconTable[Id].IconSize]
        );

        if (BuiltinIconTable[Id].Image == NULL) {
            if (Id == BUILTIN_ICON_TOOL_NVRAMCLEAN) {
                BuiltinIconTable[Id].Image = egPrepareEmbeddedImage (
                    &egemb_tool_clean_nvram, FALSE, NULL
                );
            }

            if (BuiltinIconTable[Id].Image == NULL) {
                BuiltinIconTable[Id].Image = DummyImageEx (
                    GlobalConfig.IconSizes[BuiltinIconTable[Id].IconSize]
                );
            }
        }
    }

    return egCopyImage (BuiltinIconTable[Id].Image);
} // EG_IMAGE * BuiltinIcon()

//
// Load an icon for an operating system
//

static
VOID UpdateBaseIcon (
    IN     CHAR16    *BaseName,
    IN OUT EG_IMAGE **Image
) {
    if (*Image != NULL) {
        return;
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Find Icon File '%s.xyz'",
        BaseName
    );
    #endif

    *Image = egFindIcon (
        BaseName, GlobalConfig.IconSizes[ICON_SIZE_BIG]
    );
} // static VOID UpdateBaseIcon()

static
EG_IMAGE * LoadIndexedIcon (
    IN  CHAR16 *BaseName OPTIONAL,
    IN  UINTN   Id
) {
    if (TableBuiltinIconOS[Id].Image == NULL) {
        return NULL;
    }

    if (BaseName != NULL &&
        !MyStriCmp (BaseName, TableBuiltinIconOS[Id].FileName)
    ) {
        return NULL;
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_MID,
        L"Use Cached Icon:- '%s'",
        TableBuiltinIconOS[Id].FileName
    );
    #endif

    return TableBuiltinIconOS[Id].Image;
} // static EG_IMAGE * LoadIndexedIcon()

// Load an OS icon from among the comma-delimited list provided in OSIconName.
// Searches for icons with extensions in the ICON_EXTENSIONS list (via
// egFindIcon()).
// Returns image data. On failure, returns an ugly "dummy" icon.
EG_IMAGE * LoadOSIcon (
    IN  CHAR16  *OSIconName OPTIONAL,
    IN  CHAR16  *FallbackIconName,
    IN  BOOLEAN  BootLogo
) {
    EG_IMAGE        *Image;
    CHAR16          *CutoutName;
    CHAR16          *BaseName;
    UINTN            Index2;
    UINTN            Index;
    INTN             OurId; // 'INTN' is important


    if (!AllowGraphicsMode) {
        return NULL;
    }

    // Try to find an icon from the OSIconName list
    Index =    0;
    OurId =   -1;
    Image = NULL;
    while (
        Image == NULL &&
        (CutoutName = FindCommaDelimited (OSIconName, Index++)) != NULL
    ) {
        BaseName = PoolPrint (
            L"%s_%s",
            (BootLogo) ? L"boot" : L"os",
            CutoutName
        );

        // Skip cache check if BootLogo is set
        // BootLogo is not cached
        if (GlobalConfig.HelpIcon && !BootLogo) {
            for (Index2 = 0; Index2 < BASE_OS_ICON_COUNT; Index2++) {
                Image = LoadIndexedIcon (BaseName, Index2);
                if (Image != NULL) {
                    OurId = (Index2 < BASE_OS_ICON_COUNT)
                        ? Index2 : (BASE_OS_ICON_COUNT - 1);

                    break;
                }
            } // for
        }

        UpdateBaseIcon (BaseName, &Image);

        MY_FREE_POOL(BaseName);
        MY_FREE_POOL(CutoutName);
    } // while

    // Try again using "FallbackIconName" if that fails
    if (Image == NULL) {
        BaseName = PoolPrint (
            L"%s_%s",
            (BootLogo) ? L"boot" : L"os",
            FallbackIconName
        );

        // Skip cache check if BootLogo is set
        // BootLogo is not cached
        if (GlobalConfig.HelpIcon && !BootLogo) {
            for (Index2 = 0; Index2 < BASE_OS_ICON_COUNT; Index2++) {
                Image = LoadIndexedIcon (BaseName, Index2);
                if (Image != NULL) {
                    OurId = (Index2 < BASE_OS_ICON_COUNT)
                        ? Index2 : (BASE_OS_ICON_COUNT - 1);

                    break;
                }
            } // for
        }

        UpdateBaseIcon (BaseName, &Image);
        MY_FREE_POOL(BaseName);
    }

    // Try again with "os_" if that fails and BootLogo was set
    if (BootLogo && Image == NULL) {
        BaseName = PoolPrint (L"os_%s", FallbackIconName);

        if (GlobalConfig.HelpIcon) {
            for (Index2 = 0; Index2 < BASE_OS_ICON_COUNT; Index2++) {
                Image = LoadIndexedIcon (BaseName, Index2);
                if (Image != NULL) {
                    OurId = (Index2 < BASE_OS_ICON_COUNT)
                        ? Index2 : (BASE_OS_ICON_COUNT - 1);

                    break;
                }
            } // for
        }

        UpdateBaseIcon (BaseName, &Image);
        MY_FREE_POOL(BaseName);
    }

    // Try again with the "unknown" icon specifically if that fails.
    // Only if BootLogo is *NOT* set ... No "unknown" BootLogo icon
    if (!BootLogo     &&
        Image == NULL &&
        !MyStriCmp (FallbackIconName, L"unknown")
    ) {
        BaseName = StrDuplicate (L"os_unknown");

        if (GlobalConfig.HelpIcon) {
            Image = LoadIndexedIcon (BaseName, BASE_OS_ICON_UNKNOWN);
        }

        UpdateBaseIcon (BaseName, &Image);
        MY_FREE_POOL(BaseName);

        if (Image != NULL) {
            OurId = BASE_OS_ICON_UNKNOWN;
        }
    }

    // Use the "dummy" image if still fails
    if (Image == NULL) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Set Dummy Image");
        #endif

        if (GlobalConfig.HelpIcon) {
            Image = LoadIndexedIcon (NULL, BASE_OS_ICON_DUMMY);
        }

        if (Image == NULL) {
            Image = DummyImageEx (GlobalConfig.IconSizes[ICON_SIZE_BIG]);
        }

        if (Image != NULL) {
            OurId = BASE_OS_ICON_DUMMY;
        }
    }

    // Return NULL if all failed
    if (Image == NULL) {
        return NULL;
    }

    // Cache the image if appropriate and not BootLogo
    if (GlobalConfig.HelpIcon && !BootLogo) {
        if (OurId >= 0 && TableBuiltinIconOS[OurId].Image == NULL) {
            TableBuiltinIconOS[OurId].Image = Image;
        }
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
