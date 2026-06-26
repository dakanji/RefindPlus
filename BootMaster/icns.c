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
/**
** Modified for RefindPlus
** Copyright (c) 2020-2026 Dayo Akanji (sf.net/u/dakanji/profile)
** Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
**
** Modifications distributed under the preceding terms.
**/


#include "global.h"
#include "lib.h"
#include "icns.h"
#include "config.h"
#include "mystrings.h"
#include "../BootMaster/screenmgt.h"
#include "../include/egemb_tool_clean_nvram.h"


BOOLEAN          UsingAltImg = FALSE;
extern BOOLEAN   ExitLogoFlag;

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
    { NULL, L"os_windows",          ICON_SIZE_BIG   },
    { NULL, L"os_win8",             ICON_SIZE_BIG   },
    { NULL, L"os_win",              ICON_SIZE_BIG   },
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
VOID VetAltImgTool (
    IN UINTN Id,
    IN UINTN TileSize
) {
    EG_IMAGE *TmpImage;


    if (BuiltinIconTable[Id].Image != NULL) return;
    switch (Id) {
        case BUILTIN_ICON_TOOL_NVRAMCLEAN: {
            TmpImage = egPrepareEmbeddedImage (
                &egemb_tool_clean_nvram, TRUE, NULL
            );
        }

        break;
        default: { TmpImage = NULL; }
    }
    if (TmpImage == NULL) return;

    #if REFIT_DEBUG > 0
    ALT_LOG(
        1, LOG_THREE_STAR_MID,
        L"Fetch Embedded:- %dx%d px",
        TmpImage->Width, TmpImage->Height
    );
    #endif

    BuiltinIconTable[Id].Image = egScaleImage (
        TmpImage, TileSize, TileSize
    );

    #if REFIT_DEBUG > 0
    ALT_LOG(
        1, LOG_THREE_STAR_MID,
        L"Cache Embedded:- %dx%d px",
        TileSize, TileSize
    );
    #endif

    MY_FREE_IMAGE(TmpImage);
} // static VOID VetAltImgTool()

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

    Image = egCreateFilledImage (
        PixelSize, PixelSize,
        TRUE, &BasePixel
    );
    if (Image == NULL) {
        return NULL;
    }

    LineOffset = PixelSize * 4;
    YPtr = (CHAR8 *) Image->PixelData + (
        ((PixelSize - 32) >> 1) * (LineOffset + 4)
    );

    for (y = 0; y < 32; y++) {
        Ptr = YPtr;
        for (x = 0; x < 32; x++) {
            if (((x + y) % 12) < 6) {
                *Ptr++ = 0;
                *Ptr++ = 0;
                *Ptr++ = 0;
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
    UINTN             TileSize;


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
        TileSize = GlobalConfig.IconSizes[
            BuiltinIconTable[Id].IconSize
        ];
        BuiltinIconTable[Id].Image = egFindIcon (
            BuiltinIconTable[Id].FileName, TileSize
        );

        VetAltImgTool (Id, TileSize);

        if (BuiltinIconTable[Id].Image == NULL) {
            BuiltinIconTable[Id].Image = DummyImageEx (
                TileSize
            );
        }
    }

    return egCopyImage (
        BuiltinIconTable[Id].Image
    );
} // EG_IMAGE * BuiltinIcon()

//
// Load an icon for an operating system
//

static
INTN UpdateBaseIcon (
    IN     CHAR16    *BaseName,
    IN OUT EG_IMAGE **Image,
    IN     INTN       InID
) {
    INTN              RetId;
    UINTN             Index;
    UINTN             TileSize;


    if (*Image != NULL) {
        return InID;
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Find %s Img: '%s.xyz'",
        (ExitLogoFlag) ? L"ExitLogo" : L"Icon",
        BaseName
    );
    #endif

    TileSize = GlobalConfig.IconSizes[ICON_SIZE_BIG];

    *Image = egFindIcon (
        BaseName, TileSize
    );

    RetId = -1;
    if (*Image != NULL) {
        for (Index = 0; Index < BASE_OS_ICON_COUNT; Index++) {
            if (TableBuiltinIconOS[Index].Image == NULL &&
                MyStriCmp (BaseName, TableBuiltinIconOS[Index].FileName)
            ) {
                RetId = Index;

                break;
            }
        } // for
    }

    return RetId;
} // static INTN UpdateBaseIcon()

static
EG_IMAGE * LoadIndexedIcon (
    IN  CHAR16 *BaseName OPTIONAL,
    IN  UINTN   Id
) {
    if (BaseName != NULL &&
        !MyStriCmp (BaseName, TableBuiltinIconOS[Id].FileName)
    ) {
        return NULL;
    }

    if (TableBuiltinIconOS[Id].Image == NULL) {
        return NULL;
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_MID,
        L"Use Cached Icon:- '%s'",
        TableBuiltinIconOS[Id].FileName
    );
    #endif

    return egCopyImage (TableBuiltinIconOS[Id].Image);
} // static EG_IMAGE * LoadIndexedIcon()

static
INTN CheckTheCache (
    IN     CHAR16    *BaseName,
    IN OUT EG_IMAGE **Image
) {
    INTN              RetId;
    UINTN             Index;


    RetId = -1;
    if (GlobalConfig.HelpIcon) {
        for (Index = 0; Index < BASE_OS_ICON_COUNT; Index++) {
            *Image = LoadIndexedIcon (BaseName, Index);
            if (*Image != NULL) {
                RetId = Index;

                break;
            }
        } // for
    }

    return RetId;
} // static INTN CheckTheCache()

// Load an OS icon from among the comma-delimited list provided in OSIconName.
// Searches for icons with extensions in 'ICON_EXTENSIONS' via egFindIcon().
// Returns image data ... An ugly "dummy" icon on failure.
EG_IMAGE * LoadOSIcon (
    IN  CHAR16  *OSIconName OPTIONAL,
    IN  CHAR16  *FallbackIconName,
    IN  BOOLEAN  BootLogo
) {
    EG_IMAGE        *Image;
    BOOLEAN          WinAltIcon;
    BOOLEAN          LoadCustom;
    CHAR16          *CutoutName;
    CHAR16          *BaseName;
    UINTN            Index;
    INTN             OurId; // DA-TAG: 'INTN' is important


    UsingAltImg = FALSE;

    if (!AllowGraphicsMode) {
        return NULL;
    }

    if (BootLogo &&
        GlobalConfig.DisableBootLogo == DISABLE_BOOTLOGO_ALL
    ) {
        return NULL;
    }

    LoadCustom = MyStriCmp (FallbackIconName, EXIT_SPLASH);
    WinAltIcon = MyStriCmp (FallbackIconName, L"windows");

    do {
        /* Try to find icon from 'OSIconName' list. */
        Index =    0;
        OurId =   -1;
        Image = NULL;
        do {
            if (OSIconName == NULL) break;
            CutoutName = FindCommaDelimited (
                OSIconName, Index++
            );
            if (CutoutName == NULL) break;

            BaseName = PoolPrint (
                L"%s_%s",
                (BootLogo) ? L"boot" : L"os",
                CutoutName
            );

            // Skip cache check if BootLogo is set.
            // BootLogo is not cached.
            if (!BootLogo) {
                OurId = CheckTheCache (
                    BaseName, &Image
                );
            }

            OurId = UpdateBaseIcon (
                BaseName, &Image, OurId
            );

            MY_FREE_POOL(BaseName);
            MY_FREE_POOL(CutoutName);
        } while (Image == NULL);
        if (Image != NULL) break;

        /* Try with 'os_' if BootLogo was set but not 'LoadCustom'. */
        if (!LoadCustom && BootLogo) {
            Index = 0;
            do {
                if (OSIconName == NULL) break;
                CutoutName = FindCommaDelimited (
                    OSIconName, Index++
                );
                if (CutoutName == NULL) break;

                BaseName = PoolPrint (
                    L"os_%s",
                    CutoutName
                );

                // No cache check as BootLogo is set.
                // BootLogo is not cached.
                OurId = UpdateBaseIcon (
                    BaseName, &Image, OurId
                );

                MY_FREE_POOL(BaseName);
                MY_FREE_POOL(CutoutName);
            } while (Image == NULL);
            if (Image != NULL) break;
        }

        /* Try with 'FallbackIconName'. */
        BaseName = PoolPrint (
            L"%s_%s",
            (BootLogo) ? L"boot" : L"os",
            FallbackIconName
        );
        // BootLogo is not cached.
        // Skip cache check if BootLogo is set.
        if (!BootLogo) {
            OurId = CheckTheCache (
                BaseName, &Image
            );
        }

        OurId = UpdateBaseIcon (
            BaseName, &Image, OurId
        );
        MY_FREE_POOL(BaseName);
        if (Image != NULL) break;

        /* Try with 'win' as 'FallbackIconName' if 'WinAltIcon' is set. */
        if (WinAltIcon) {
            BaseName = PoolPrint (
                L"%s_%s",
                (BootLogo) ? L"boot" : L"os",
                L"win"
            );

            // BootLogo is not cached.
            // Skip cache check if BootLogo is set.
            if (!BootLogo) {
                OurId = CheckTheCache (
                    BaseName, &Image
                );
            }

            OurId = UpdateBaseIcon (
                BaseName, &Image, OurId
            );
            MY_FREE_POOL(BaseName);
            if (Image != NULL) break;
        }

        if (BootLogo) {
            /* Try with 'os_' if BootLogo was set but not 'LoadCustom'. */
            if (!LoadCustom) {
                BaseName = PoolPrint (
                    L"os_%s",
                    FallbackIconName
                );

                OurId = UpdateBaseIcon (
                    BaseName, &Image, OurId
                );
                MY_FREE_POOL(BaseName);

                if (Image == NULL && WinAltIcon) {
                    // Try 'os_win' if 'WinAltIcon' is set.
                    OurId = UpdateBaseIcon (
                        L"os_win", &Image, OurId
                    );
                }
            }

            // BootLogo is set ... End of road
            break;
        }

        /* Try the "unknown" icon specifically */
        if (!MyStriCmp (FallbackIconName, L"unknown")) {
            BaseName = StrDuplicate (L"os_unknown");

            Image = LoadIndexedIcon (
                BaseName, BASE_OS_ICON_UNKNOWN
            );

            OurId = UpdateBaseIcon (
                BaseName, &Image, OurId
            );
            MY_FREE_POOL(BaseName);

            if (Image != NULL) {
                OurId = BASE_OS_ICON_UNKNOWN;

                break;
            }
        }

        /* Use "dummy" image. */
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Set Dummy Image");
        #endif

        Image = LoadIndexedIcon (
            NULL, BASE_OS_ICON_DUMMY
        );

        if (Image == NULL) {
            Image = egCopyImage (
                DummyImageEx (
                    GlobalConfig.IconSizes[ICON_SIZE_BIG]
                )
            );
        }

        if (Image != NULL) {
            OurId = BASE_OS_ICON_DUMMY;
        }
    } while (0); // This 'loop' only runs once

    if (OurId == BASE_OS_ICON_DUMMY ||
        OurId == BASE_OS_ICON_UNKNOWN
    ) {
        UsingAltImg = TRUE;
    }

    // Cache the image if appropriate.
    if (!BootLogo             &&
        OurId >= 0            &&
        Image != NULL         &&
        GlobalConfig.HelpIcon &&
        TableBuiltinIconOS[OurId].Image == NULL
    ) {
        TableBuiltinIconOS[OurId].Image = egCopyImage (Image);
    }

    return Image;
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
