/*
 * libeg/screen.c
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
 *
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2024 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "libegint.h"
#include "../BootMaster/screenmgt.h"
#include "../BootMaster/global.h"
#include "../BootMaster/apple.h"
#include "../BootMaster/lib.h"
#include "../BootMaster/mystrings.h"
#include "../include/refit_call_wrapper.h"
#include "libeg.h"
#include "lodepng.h"
#include "../include/Handle.h"

#include <efiUgaDraw.h>
#include <efiConsoleControl.h>

#ifndef __MAKEWITH_GNUEFI
#define LibLocateProtocol EfiLibLocateProtocol
#define LibOpenRoot EfiLibOpenRoot
#else
#include <efilib.h>
#endif

extern UINTN    AppleFramebuffers;
extern BOOLEAN  ForceTextOnly;
extern BOOLEAN  ObtainHandleGOP;
extern EG_PIXEL MenuBackgroundPixel;


// Console defines and variables
EFI_GUID UgaDrawProtocolGuid        =        EFI_UGA_DRAW_PROTOCOL_GUID;
EFI_GUID GOPDrawProtocolGuid        = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
EFI_GUID ConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

EFI_UGA_DRAW_PROTOCOL        *UGADraw        = NULL;
EFI_GRAPHICS_OUTPUT_PROTOCOL *GOPDraw        = NULL;
EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;

BOOLEAN GotConsoleControl =  TRUE;
BOOLEAN egHasGraphics     = FALSE;
BOOLEAN SetPreferUGA      = FALSE;
BOOLEAN GotGoodGOP        = FALSE;

UINTN   MinPixelsLine  =   0;
UINTN   SelectedGOP    =   0;
UINTN   egScreenWidth  = 800;
UINTN   egScreenHeight = 600;

// DA-TAG: Code within 'MAKEWITH_TIANO' block below is directly from OpenCore
//         Licensed under the BSD 3 license
//         Added directly for control
#ifdef __MAKEWITH_TIANO
// DA-TAG: Limit to TianoCore - START
typedef struct {
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *GraphicsOutput;
  EFI_UGA_DRAW_PROTOCOL          Uga;
} RP_UGA_PROTOCOL;

/**
  The ForceVideoMode function below was adapted from UefiSeven
  https://github.com/manatails/uefiseven/blob/b8f0baba63e60e74d4ed3e86b15b76319d316b83/UefiSevenPkg/Platform/UefiSeven/Display.c#L299-L368

  UefiSeven is an EFI loader that emulates int10h interrupts needed for booting Windows 7 under UEFI Class 3 systems.

  The code is licensed and made available under the terms and conditions of the BSD License
  The full text of the license may be found at http://opensource.org/licenses/bsd-license.php
**/
static
EFI_STATUS ForceVideoMode (
    IN UINTN      TargetWidth,
    IN UINTN      TargetHeight
) {
    UINT32        HorizontalResolution;
    UINT32        VerticalResolution;
    UINT32        PixelsPerScanLine;
    UINT32        FrameBufferSize;
    UINT32        ScanlineScale;
    UINTN         TargetPixelsLine;

    if (GOPDraw == NULL) {
        return EFI_UNSUPPORTED;
    }

    if (TargetWidth  == 0 ||
        TargetHeight == 0
    ) {
        return EFI_INVALID_PARAMETER;
    }

    HorizontalResolution = (UINT32) TargetWidth;
    VerticalResolution   = (UINT32) TargetHeight;

    ScanlineScale = 1;
    while ((MinPixelsLine * ScanlineScale) < TargetWidth) {
        ScanlineScale++;
    }

    TargetPixelsLine  =  MinPixelsLine;
    PixelsPerScanLine = (MinPixelsLine * ScanlineScale);
    while (
        (TargetPixelsLine > 3) &&
        (TargetPixelsLine % 2) == 0 &&
        (PixelsPerScanLine - (TargetPixelsLine / 2)) > TargetWidth
    ) {
        TargetPixelsLine  -= (TargetPixelsLine / 2);
        PixelsPerScanLine -= TargetPixelsLine;
    }
    FrameBufferSize = PixelsPerScanLine * 4 * VerticalResolution;

    GOPDraw->Mode->Info->HorizontalResolution = HorizontalResolution;
    GOPDraw->Mode->Info->VerticalResolution   = VerticalResolution;
    GOPDraw->Mode->Info->PixelsPerScanLine    = PixelsPerScanLine;
    GOPDraw->Mode->FrameBufferSize            = FrameBufferSize;

    return EFI_SUCCESS;
} // EFI_STATUS ForceVideoMode()

static
EFI_STATUS RefitSetConsoleResolutionForProtocol (
    IN  EFI_GRAPHICS_OUTPUT_PROTOCOL      *GraphicsOutput,
    IN  UINT32                             Width  OPTIONAL,
    IN  UINT32                             Height OPTIONAL,
    IN  UINT32                             Bpp    OPTIONAL
) {
    EFI_STATUS                             Status;

    UINT32                                 MaxMode;
    UINT32                                 ModeIndex;
    INT64                                  ModeNumber;
    UINTN                                  SizeOfInfo;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;
    BOOLEAN                                SetMax;

    SetMax = (Width == 0 && Height == 0);

    // Find required resolution
    ModeNumber = -1;
    MaxMode    = GraphicsOutput->Mode->MaxMode;
    for (ModeIndex = 0; ModeIndex < MaxMode; ++ModeIndex) {
        Status = GraphicsOutput->QueryMode (
            GraphicsOutput, ModeIndex,
            &SizeOfInfo, &Info
        );
        if (EFI_ERROR (Status)) {
            continue;
        }

        if (!SetMax) {
            // Custom resolution requested
            if (Info->HorizontalResolution == Width &&
                Info->VerticalResolution   == Height
                && (
                    Bpp == 0 || Bpp == 24 || Bpp == 32
                ) && (
                    Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor ||
                    Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor ||
                    (
                        Info->PixelFormat == PixelBitMask &&
                        (
                            Info->PixelInformation.RedMask == 0xFF000000U ||
                            Info->PixelInformation.RedMask == 0xFF0000U   ||
                            Info->PixelInformation.RedMask == 0xFF00U     ||
                            Info->PixelInformation.RedMask == 0xFFU
                        )
                    ) || Info->PixelFormat == PixelBltOnly
                )
            ) {
                ModeNumber = ModeIndex;
                MY_FREE_POOL(Info);
                break;
            }
        }
        else if (Info->HorizontalResolution > Width ||
            (
                Info->HorizontalResolution == Width  &&
                Info->VerticalResolution    > Height
            )
        ) {
            Width      = Info->HorizontalResolution;
            Height     = Info->VerticalResolution;
            ModeNumber = ModeIndex;
        } // if !SetMax elseif Info->HorizontalResolution > Width

        MY_FREE_POOL(Info);
    }

    if (ModeNumber < 0) {
        return EFI_NOT_FOUND;
    }

    if (ModeNumber == GraphicsOutput->Mode->Mode) {
        return EFI_ALREADY_STARTED;
    }

    // Current graphics mode not set, or not set to mode found above.
    // Set new graphics mode.
    Status = GraphicsOutput->SetMode (GraphicsOutput, (UINT32) ModeNumber);
    if (!EFI_ERROR(Status)) {
        egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
        egScreenWidth  = GraphicsOutput->Mode->Info->HorizontalResolution;
    }

    return Status;
} // static EFI_STATUS RefitSetConsoleResolutionForProtocol()

static
EFI_STATUS EFIAPI RefitUGADrawGetMode (
    IN  EFI_UGA_DRAW_PROTOCOL *This,
    OUT UINT32                *HorizontalResolution,
    OUT UINT32                *VerticalResolution,
    OUT UINT32                *ColorDepth,
    OUT UINT32                *RefreshRate
) {
    RP_UGA_PROTOCOL                       *Refit_UGA;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;

    if (This                 == NULL ||
        ColorDepth           == NULL ||
        RefreshRate          == NULL ||
        VerticalResolution   == NULL ||
        HorizontalResolution == NULL
    ) {
        return EFI_INVALID_PARAMETER;
    }

    Refit_UGA = BASE_CR (This, RP_UGA_PROTOCOL, Uga);
    Info      = Refit_UGA->GraphicsOutput->Mode->Info;

    if (Info->HorizontalResolution == 0 ||
        Info->VerticalResolution   == 0
    ) {
        return EFI_NOT_STARTED;
    }

    *HorizontalResolution = Info->HorizontalResolution;
    *VerticalResolution   = Info->VerticalResolution;
    *ColorDepth           = DEFAULT_COLOUR_DEPTH;
    *RefreshRate          = DEFAULT_REFRESH_RATE;

    return EFI_SUCCESS;
} // static EFI_STATUS EFIAPI RefitUGADrawGetMode()

static
EFI_STATUS EFIAPI RefitUGADrawSetMode (
    IN  EFI_UGA_DRAW_PROTOCOL *This,
    IN  UINT32                 HorizontalResolution,
    IN  UINT32                 VerticalResolution,
    IN  UINT32                 ColorDepth,
    IN  UINT32                 RefreshRate
) {
    EFI_STATUS        Status;
    RP_UGA_PROTOCOL   *Refit_UGA;

    Refit_UGA = BASE_CR (This, RP_UGA_PROTOCOL, Uga);

    Status = RefitSetConsoleResolutionForProtocol (
        Refit_UGA->GraphicsOutput,
        HorizontalResolution,
        VerticalResolution,
        ColorDepth
    );
    if (EFI_ERROR (Status)) {
        Status = RefitSetConsoleResolutionForProtocol (
            Refit_UGA->GraphicsOutput,
            0, 0, 0
        );
    }

    return Status;
} // static EFI_STATUS EFIAPI RefitUGADrawSetMode()

static
EFI_STATUS EFIAPI RefitUGADrawBlt (
    IN  EFI_UGA_DRAW_PROTOCOL                   *This,
    IN  EFI_UGA_PIXEL                           *BltBuffer OPTIONAL,
    IN  EFI_UGA_BLT_OPERATION                    BltOperation,
    IN  UINTN                                    SourceX,
    IN  UINTN                                    SourceY,
    IN  UINTN                                    DestinationX,
    IN  UINTN                                    DestinationY,
    IN  UINTN                                    Width,
    IN  UINTN                                    Height,
    IN  UINTN                                    Delta      OPTIONAL
) {
    RP_UGA_PROTOCOL   *Refit_UGA;

    Refit_UGA = BASE_CR (This, RP_UGA_PROTOCOL, Uga);

    return Refit_UGA->GraphicsOutput->Blt (
        Refit_UGA->GraphicsOutput,
        (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) BltBuffer,
        (EFI_GRAPHICS_OUTPUT_BLT_OPERATION) BltOperation,
        SourceX, SourceY,
        DestinationX, DestinationY,
        Width, Height,
        Delta
    );
} // static EFI_STATUS EFIAPI RefitUGADrawBlt()

static
EFI_STATUS RefitPassUgaThrough (VOID) {
    EFI_STATUS                      Status;
    EFI_STATUS                      ReturnStatus;
    UINTN                           HandleCount;
    UINTN                           Index;
    EFI_HANDLE                     *HandleBuffer;
    RP_UGA_PROTOCOL                *Refit_UGA;
    EFI_UGA_DRAW_PROTOCOL          *UgaDraw;
    EFI_GRAPHICS_OUTPUT_PROTOCOL   *GraphicsOutput;

    // DA-TAG: UGAPassThru is currently only for macOS 10.4
    //         As a side note from OpenCore notes...
    //         MacPro5,1 has 2 GOP protocols:
    //           - for the GPU
    //           - for ConsoleOutput
    //         and 1 UGA protocol:
    //           - for an as yet unknown handle
    Status = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, ByProtocol,
        &gEfiGraphicsOutputProtocolGuid, NULL,
        &HandleCount, &HandleBuffer
    );
    if (EFI_ERROR (Status)) {
        // Early Return ... No GOP handles to process
        return EFI_NOT_READY;
    }

    ReturnStatus = EFI_LOAD_ERROR;
    for (Index = 0; Index < HandleCount; ++Index) {
        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, HandleBuffer[Index],
            &gEfiGraphicsOutputProtocolGuid, (VOID **) &GraphicsOutput
        );
        if (EFI_ERROR (Status)) {
            if (ReturnStatus == EFI_LOAD_ERROR) {
                ReturnStatus = EFI_NOT_FOUND;
            }

            continue;
        }

        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, HandleBuffer[Index],
            &gEfiUgaDrawProtocolGuid, (VOID **) &UgaDraw
        );
        if (!EFI_ERROR (Status)) {
            if (ReturnStatus == EFI_LOAD_ERROR ||
                ReturnStatus == EFI_NOT_FOUND
            ) {
                ReturnStatus = EFI_UNSUPPORTED;
            }

            continue;
        }

        Refit_UGA = AllocateZeroPool (sizeof (RP_UGA_PROTOCOL));
        if (Refit_UGA == NULL) {
            ReturnStatus = EFI_OUT_OF_RESOURCES;
            break;
        }

        Refit_UGA->GraphicsOutput = GraphicsOutput;
        Refit_UGA->Uga.GetMode    = RefitUGADrawGetMode;
        Refit_UGA->Uga.SetMode    = RefitUGADrawSetMode;
        Refit_UGA->Uga.Blt        = RefitUGADrawBlt;

        Status = REFIT_CALL_4_WRAPPER(
            gBS->InstallMultipleProtocolInterfaces, &HandleBuffer[Index],
            &gEfiUgaDrawProtocolGuid, &Refit_UGA->Uga,
            NULL
        );
        if (EFI_ERROR (ReturnStatus)) {
            ReturnStatus = Status;
        }
    } // for

    MY_FREE_POOL(HandleBuffer);

    return ReturnStatus;
} // static EFI_STATUS RefitPassUgaThrough()
// DA-TAG: Limit to TianoCore - END
#endif

// DA-TAG: Investigate This
//         Code within 'MAKEWITH_TIANO' block below is directly from OpenCore
//         Licensed under the BSD 3 license
//         Added directly as missing in OpenCorePkg version in RefindPlusUDK
//         Potentially hook directly if/when OpenCorePkg is updated
#ifdef __MAKEWITH_TIANO
// DA-TAG: Limit to TianoCore - START
typedef struct {
  EFI_UGA_DRAW_PROTOCOL           *Uga;
  EFI_GRAPHICS_OUTPUT_PROTOCOL     GraphicsOutput;
} RP_GOP_PROTOCOL;

static
VOID DisableGOP (VOID) {
    GOPDraw->Mode->Info = NULL;
    GOPDraw->Mode       = NULL;
    GOPDraw             = NULL;
} // static VOID DisableGOP()

static
EFI_STATUS EFIAPI RefitGopDrawQueryMode (
    IN  EFI_GRAPHICS_OUTPUT_PROTOCOL           *This,
    IN  UINT32                                  ModeNumber,
    OUT UINTN                                  *SizeOfInfo,
    OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
) {
    if (ModeNumber != 0) {
        return EFI_INVALID_PARAMETER;
    }

    if ((SizeOfInfo == NULL) || (Info == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    *SizeOfInfo = This->Mode->SizeOfInfo;
    *Info       = AllocateCopyPool (This->Mode->SizeOfInfo, This->Mode->Info);
    if (*Info == NULL) {
        return EFI_DEVICE_ERROR;
    }

    return EFI_SUCCESS;
} // static EFI_STATUS EFIAPI RefitGopDrawQueryMode()

static
EFI_STATUS EFIAPI OcGopDrawSetMode (
    IN  EFI_GRAPHICS_OUTPUT_PROTOCOL  *This,
    IN  UINT32                         ModeNumber
) {
    if (ModeNumber != 0) {
        return EFI_UNSUPPORTED;
    }

    // Assuming 0 is the only mode that is accepted, which is already set.
    return EFI_SUCCESS;
} // static EFI_STATUS EFIAPI OcGopDrawSetMode()

static
EFI_STATUS EFIAPI RefitGopDrawBlt (
    IN  EFI_GRAPHICS_OUTPUT_PROTOCOL       *This,
    IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL      *BltBuffer    OPTIONAL,
    IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION   BltOperation,
    IN  UINTN                               SourceX,
    IN  UINTN                               SourceY,
    IN  UINTN                               DestinationX,
    IN  UINTN                               DestinationY,
    IN  UINTN                               Width,
    IN  UINTN                               Height,
    IN  UINTN                               Delta         OPTIONAL
) {
    RP_GOP_PROTOCOL  *OcGopDraw;

    OcGopDraw = BASE_CR (This, RP_GOP_PROTOCOL, GraphicsOutput);

    return REFIT_CALL_10_WRAPPER(
        OcGopDraw->Uga->Blt, OcGopDraw->Uga,
        (EFI_UGA_PIXEL *)BltBuffer, (EFI_UGA_BLT_OPERATION)BltOperation,
        SourceX, SourceY,
        DestinationX, DestinationY,
        Width, Height, Delta
    );
} // static EFI_STATUS EFIAPI RefitGopDrawBlt()

static
EFI_STATUS RefitProvideGopPassThrough (
    IN BOOLEAN  ForAll
) {
    EFI_STATUS                        Status;
    UINTN                             HandleCount;
    EFI_HANDLE                       *HandleBuffer;
    UINTN                             Index;
    EFI_GRAPHICS_OUTPUT_PROTOCOL     *GraphicsOutput;
    EFI_UGA_DRAW_PROTOCOL            *UgaDraw;
    RP_GOP_PROTOCOL                  *OcGopDraw;
    APPLE_FRAMEBUFFER_INFO_PROTOCOL  *FramebufferInfo;
    EFI_PHYSICAL_ADDRESS              FramebufferBase;
    UINT32                            FramebufferSize;
    UINT32                            ScreenRowBytes;
    UINT32                            ScreenWidth;
    UINT32                            ScreenHeight;
    UINT32                            ScreenDepth;
    UINT32                            HorizontalResolution;
    UINT32                            VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT         PixelFormat;
    UINT32                            ColorDepth;
    UINT32                            RefreshRate;
    BOOLEAN                           HasAppleFramebuffer;

    // We should not proxy UGA when there is no AppleFramebuffer,
    // but on systems where there is nothing, it is the only option.
    // REF: https://github.com/acidanthera/bugtracker/issues/1498
    Status = REFIT_CALL_3_WRAPPER(
        gBS->LocateProtocol, &gAppleFramebufferInfoProtocolGuid,
        NULL, (VOID *)&FramebufferInfo
    );
    HasAppleFramebuffer = !EFI_ERROR(Status);

    Status = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, ByProtocol,
        &gEfiUgaDrawProtocolGuid, NULL,
        &HandleCount, &HandleBuffer
    );
    if (EFI_ERROR (Status)) {
        return Status;
    }

    for (Index = 0; Index < HandleCount; ++Index) {
        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, HandleBuffer[Index],
            &gEfiUgaDrawProtocolGuid, (VOID **)&UgaDraw
        );
        if (EFI_ERROR (Status)) {
            continue;
        }

        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, HandleBuffer[Index],
            &gEfiGraphicsOutputProtocolGuid, (VOID **)&GraphicsOutput
        );
        if (!EFI_ERROR (Status)) {
            continue;
        }

        FramebufferBase = 0;
        FramebufferSize = 0;
        ScreenRowBytes  = 0;
        PixelFormat     = PixelBltOnly;

        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, HandleBuffer[Index],
            &gAppleFramebufferInfoProtocolGuid, (VOID **)&FramebufferInfo
        );
        if (EFI_ERROR (Status)) {
            if (HasAppleFramebuffer || !ForAll) {
                continue;
            }
        }
        else {
            Status = REFIT_CALL_7_WRAPPER(
                FramebufferInfo->GetInfo, FramebufferInfo,
                &FramebufferBase, &FramebufferSize,
                &ScreenRowBytes, &ScreenWidth,
                &ScreenHeight, &ScreenDepth
            );
            if (!EFI_ERROR (Status)) {
                PixelFormat = PixelRedGreenBlueReserved8BitPerColor;  ///< or PixelBlueGreenRedReserved8BitPerColor?
            }
            else {
                if (HasAppleFramebuffer) {
                    continue;
                }
            }
        }

        Status = REFIT_CALL_5_WRAPPER(
            UgaDraw->GetMode, UgaDraw,
            &HorizontalResolution, &VerticalResolution,
            &ColorDepth, &RefreshRate
        );
        if (EFI_ERROR (Status)) {
            continue;
        }

        OcGopDraw = AllocateZeroPool (sizeof (*OcGopDraw));
        if (OcGopDraw == NULL) {
            continue;
        }

        OcGopDraw->Uga                      = UgaDraw;
        OcGopDraw->GraphicsOutput.QueryMode = RefitGopDrawQueryMode;
        OcGopDraw->GraphicsOutput.SetMode   = OcGopDrawSetMode;
        OcGopDraw->GraphicsOutput.Blt       = RefitGopDrawBlt;
        OcGopDraw->GraphicsOutput.Mode      = AllocateZeroPool (sizeof (*OcGopDraw->GraphicsOutput.Mode));
        if (OcGopDraw->GraphicsOutput.Mode == NULL) {
            MY_FREE_POOL(OcGopDraw);
            continue;
        }

        // Only Mode 0 is supported, so there is only one mode supported in total.
        OcGopDraw->GraphicsOutput.Mode->MaxMode = 1;

        // Again, only Mode 0 is supported.
        OcGopDraw->GraphicsOutput.Mode->Mode = 0;
        OcGopDraw->GraphicsOutput.Mode->Info = AllocateZeroPool (sizeof (*OcGopDraw->GraphicsOutput.Mode->Info));
        if (OcGopDraw->GraphicsOutput.Mode->Info == NULL) {
            MY_FREE_POOL(OcGopDraw->GraphicsOutput.Mode);
            MY_FREE_POOL(OcGopDraw);
            continue;
        }

        OcGopDraw->GraphicsOutput.Mode->Info->Version              = 0;
        OcGopDraw->GraphicsOutput.Mode->Info->HorizontalResolution = HorizontalResolution;
        OcGopDraw->GraphicsOutput.Mode->Info->VerticalResolution   = VerticalResolution;
        OcGopDraw->GraphicsOutput.Mode->Info->PixelFormat          = PixelFormat;

        // No pixel mask is needed (i.e. all zero) in PixelInformation,
        // plus AllocateZeroPool already assigns zero for it.
        // Skip.
        // ------------------------------------------------------------------------------
        // ScreenRowBytes is PixelsPerScanLine * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL),
        // so here to divide it back.
        OcGopDraw->GraphicsOutput.Mode->Info->PixelsPerScanLine = ScreenRowBytes / sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
        OcGopDraw->GraphicsOutput.Mode->SizeOfInfo              = sizeof (*OcGopDraw->GraphicsOutput.Mode->Info);
        OcGopDraw->GraphicsOutput.Mode->FrameBufferBase         = FramebufferBase;
        OcGopDraw->GraphicsOutput.Mode->FrameBufferSize         = FramebufferSize;

        Status = REFIT_CALL_4_WRAPPER(
            gBS->InstallMultipleProtocolInterfaces, &HandleBuffer[Index],
            &gEfiGraphicsOutputProtocolGuid, &OcGopDraw->GraphicsOutput,
            NULL
        );
        if (EFI_ERROR (Status)) {
            MY_FREE_POOL(OcGopDraw->GraphicsOutput.Mode->Info);
            MY_FREE_POOL(OcGopDraw->GraphicsOutput.Mode);
            MY_FREE_POOL(OcGopDraw);
        }
    }

    MY_FREE_POOL(HandleBuffer);

    return Status;
} // static EFI_STATUS RefitProvideGopPassThrough()
// DA-TAG: Limit to TianoCore - END
#endif


static
EFI_STATUS EncodeAsPNG (
    IN  VOID     *RawData,
    IN  UINT32    Width,
    IN  UINT32    Height,
    OUT VOID    **Buffer,
    OUT UINTN    *BufferSize
) {
    UINTN ErrorCode;

    // Should return 0 on success
    ErrorCode = lodepng_encode32 (
        (unsigned char **) Buffer,
        BufferSize,
        RawData,
        Width,
        Height
    );

    return (ErrorCode == 0) ? EFI_SUCCESS : EFI_INVALID_PARAMETER;
} // static EFI_STATUS EncodeAsPNG()

static
EFI_STATUS RefitCheckGOP (
    BOOLEAN FixGOP
) {
    EFI_STATUS                            Status;
    UINTN                                 index;
    UINTN                                 HandleCount;
    UINTN                                 SizeOfInfo;
    UINT32                                Mode;
    UINT32                                Width;
    UINT32                                Height;
    BOOLEAN                               DoneLoop;
    BOOLEAN                               OurValidGOP;
    EFI_HANDLE                           *HandleBuffer;
    EFI_GRAPHICS_OUTPUT_PROTOCOL         *OrigGop;
    EFI_GRAPHICS_OUTPUT_PROTOCOL         *Gop;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;

    #if REFIT_DEBUG > 0
    CHAR16 *TmpStr;
    #endif

    // Search for GOP on ConsoleOut handle
    OrigGop = NULL;
    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol, gST->ConsoleOutHandle,
        &GOPDrawProtocolGuid, (VOID **) &OrigGop
    );
    if (!OrigGop || EFI_ERROR(Status)) {
        // Early Return on Failure ... Proceed to Try to Provide
        // Need to return 'Success' to trigger provision
        return EFI_SUCCESS;
    }

    // Search for avaliable modes on ConsoleOut GOP
    if (OrigGop->Mode->MaxMode > 0) {
        GOPDraw = OrigGop;

        // Early Return ... Skip Provision
        return EFI_ALREADY_STARTED;
    }

    #if REFIT_DEBUG > 0
    TmpStr = (FixGOP) ? L"Replacement" : L"Appropriate";
    LOG_MSG("\n\n");
    LOG_MSG("Seek %s ConsoleOut GOP Candidates:", TmpStr);
    #endif

    // Search for GOP on handle buffer
    Status = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, ByProtocol,
        &GOPDrawProtocolGuid, NULL,
        &HandleCount, &HandleBuffer
    );
    if (EFI_ERROR(Status) || HandleCount == 1) {
        #if REFIT_DEBUG > 0
        LOG_MSG("%s  - Could *NOT* Find %s ConsoleOut GOP Candidates", OffsetNext, TmpStr);
        LOG_MSG("\n\n");
        #endif

        MY_FREE_POOL(HandleBuffer);

        // Early Return ... Skip Provision
        return EFI_NOT_FOUND;
    }

    Info = NULL;
    OurValidGOP = FALSE;

    // Assess GOP instances on handle buffer
    for (index = 0; index < HandleCount; index++) {
        if (HandleBuffer[index] == gST->ConsoleOutHandle) {
            #if REFIT_DEBUG > 0
            LOG_MSG("%s  - Ignore Handle[%02d] ... ConOut Handle", OffsetNext, index);
            #endif

            // Restart Loop
            continue;
        }

        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, HandleBuffer[index],
            &GOPDrawProtocolGuid, (VOID **) &Gop
        );
        if (EFI_ERROR(Status)) {
            #if REFIT_DEBUG > 0
            LOG_MSG("%s  - Ignore Handle[%02d] ... %r", OffsetNext, index, Status);
            #endif

            // Skip handle on error
            continue;
        }

        #if REFIT_DEBUG > 0
        LOG_MSG(
            "%s  - Found %s Candidate on Handle[%02d]",
            OffsetNext,
            (FixGOP) ? L"Replacement" : L"Console GOP",
            index
        );
        LOG_MSG("%s    * Evaluate Candidate", OffsetNext);
        #endif

        Width = Height = 0;
        DoneLoop = FALSE;

        for (Mode = 0; Mode < Gop->Mode->MaxMode; Mode++) {
            if (DoneLoop) {
                MY_FREE_POOL(Info);
            }

            Status = Gop->QueryMode (Gop, Mode, &SizeOfInfo, &Info);
            if (EFI_ERROR(Status)) {
                DoneLoop = FALSE;

                // Skip handle
                continue;
            }
            DoneLoop = TRUE;

            if (Width  > Info->HorizontalResolution ||
                Height > Info->VerticalResolution
            ) {
                // Skip handle
                continue;
            }

            if (Width  == Info->HorizontalResolution &&
                Height == Info->VerticalResolution
            ) {
                // Skip handle
                continue;
            }

            Width  = Info->HorizontalResolution;
            Height = Info->VerticalResolution;
        } // for Mode = 0

        MY_FREE_POOL(Info);

        #if REFIT_DEBUG > 0
        LOG_MSG(
            "%s    ** %s Candidate : %5d x %-5d",
            OffsetNext,
            (Width == 0 || Height == 0)
                ? L"Invalid"
                : L"Valid",
            Width, Height
        );
        #endif

        if (Width != 0 && Height != 0) {
            OurValidGOP = TRUE;

            // Found valid candiadte ... Break out of loop
            break;
        }
    } // for index = 0

    MY_FREE_POOL(HandleBuffer);

    #if REFIT_DEBUG > 0
    LOG_MSG("\n\n");
    #endif

    if (!OurValidGOP || EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        LOG_MSG(
            "INFO: Could *NOT* Find Usable %s Candidate",
            (FixGOP) ? L"Replacement" : L"Console GOP"
        );
        LOG_MSG("\n\n");
        #endif

        // Early Return ... Skip Provision
        return EFI_UNSUPPORTED;
    }

    // Return 'Success' to trigger provision
    return EFI_SUCCESS;
} // static EFI_STATUS RefitCheckGOP()

static
BOOLEAN SupplyConsoleGop (
    BOOLEAN    FixGOP
) {
    EFI_STATUS Status;
    BOOLEAN    ValueValidGOP;

// DA-TAG: Limit to TianoCore
#ifndef __MAKEWITH_TIANO
    ValueValidGOP = FALSE;
#else
    ValueValidGOP = GotGoodGOP = FALSE;
    if (GlobalConfig.SetConsoleGOP) {
        Status = RefitCheckGOP (FixGOP);
        if (Status == EFI_ALREADY_STARTED) {
            GotGoodGOP = TRUE;

            return TRUE;
        }

        #if REFIT_DEBUG > 0
        LOG_MSG("%s ConOut GOP:", (FixGOP) ? L"Replace" : L"Provide");
        LOG_MSG("%s  - Status:- '%r' ... RefitCheckGOP", OffsetNext, Status);
        #endif

        if (!EFI_ERROR(Status)) {
            // Run OpenCore Function
            Status = OcProvideConsoleGop (TRUE);

            #if REFIT_DEBUG > 0
            LOG_MSG("%s  - Status:- '%r' ... SetConsoleOutGOP", OffsetNext, Status);
            #endif

            if (!EFI_ERROR(Status)) {
                Status = REFIT_CALL_3_WRAPPER(
                    gBS->HandleProtocol, gST->ConsoleOutHandle,
                    &GOPDrawProtocolGuid, (VOID **) &GOPDraw
                );

                #if REFIT_DEBUG > 0
                LOG_MSG("%s  - Status:- '%r' ... HandleProtocolGOP", OffsetNext, Status);
                #endif

                if (!EFI_ERROR(Status)) {
                    ValueValidGOP = TRUE;
                }
            }
        }
    }
#endif

    #if REFIT_DEBUG > 0
    LOG_MSG("\n\n");
    #endif

    return ValueValidGOP;
} // static BOOLEAN SupplyConsoleGop()

EFI_STATUS egDumpGOPVideoModes (VOID) {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;

    EFI_STATUS Status;
    UINT32     Mode;
    UINT32     MaxMode;
    UINT32     LoopCount;
    UINTN      SizeOfInfo;
    BOOLEAN    OurValidGOP;
    BOOLEAN    IncrementLoop;

    #if REFIT_DEBUG > 0
    UINT32     ModeLog;
    CHAR16    *MsgStr;
    CHAR16    *PixelFormatDesc;
    #endif

    if (GOPDraw == NULL) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"Could *NOT* Find GOP Instance");
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s!!", MsgStr);
        LOG_MSG("** WARN: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        return EFI_UNSUPPORTED;
    }

    // Get dump
    MaxMode = GOPDraw->Mode->MaxMode;
    if (MaxMode == 0) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"Could *NOT* Find Valid GOP Modes");
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s!!", MsgStr);
        LOG_MSG("INFO: %s:", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        return EFI_UNSUPPORTED;
    }

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (L"Analyse GOP Modes on GFX Handle[%02d]", SelectedGOP);
    ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
    LOG_MSG("%s:", MsgStr);
    MY_FREE_POOL(MsgStr);

    MsgStr = PoolPrint (
        L"%02d GOP Mode%s ... 0x%lx <<< -- >>> 0x%lx Framebuffer",
        MaxMode,
        (MaxMode != 1) ? L"s" : L"",
        GOPDraw->Mode->FrameBufferBase,
        GOPDraw->Mode->FrameBufferBase + GOPDraw->Mode->FrameBufferSize
    );
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("%s  - Summary:- '%s'", OffsetNext, MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    MinPixelsLine = LoopCount   = 0;
    IncrementLoop = OurValidGOP = FALSE;
    for (Mode = 0; Mode <= MaxMode; Mode++) {
        if (IncrementLoop) {
            LoopCount++;
        }
        else {
            IncrementLoop = TRUE;
        }

        if (LoopCount == MaxMode) {
            break;
        }

        #if REFIT_DEBUG > 0
        // Limit logged value to 99
        ModeLog = (Mode > 99) ? 99 : Mode;

        LOG_MSG("%s    * Mode[%02d]", OffsetNext, ModeLog);
        #endif

        Status = REFIT_CALL_4_WRAPPER(
            GOPDraw->QueryMode, GOPDraw,
            Mode, &SizeOfInfo, &Info
        );
        if (EFI_ERROR(Status)) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_THREE_STAR_MID, L"Mode[%02d]: %r", ModeLog, Status);
            LOG_MSG(" ... Query Status: %r", Status);
            #endif
        }
        else {
            OurValidGOP = TRUE;
            if (MinPixelsLine == 0 ||
                MinPixelsLine > Info->PixelsPerScanLine
            ) {
                MinPixelsLine = Info->PixelsPerScanLine;
            }

            #if REFIT_DEBUG > 0
            switch (Info->PixelFormat) {
                case PixelRedGreenBlueReserved8BitPerColor: PixelFormatDesc = L"8bit RGB";  break;
                case PixelBlueGreenRedReserved8BitPerColor: PixelFormatDesc = L"8bit BGR";  break;
                case PixelBitMask:                          PixelFormatDesc = L"BIT Mask";  break;
                case PixelBltOnly:                          PixelFormatDesc = L"BLT Only";  break;
                default:                                    PixelFormatDesc = L"Unknown!";  break;
            } // switch

            // DA-TAG: Alignment of 'Expected Length + 4 Spaces' added before 'PixelFormatDesc'
            //         Alignment is 'Expected Length + 3 Spaces' for pixel size and 'Pixels Per Scanned Line'
            LOG_MSG(
                " @%6d x %-6d( Pixels Per Scanned Line : %-6d| Pixel Format : %s )",
                Info->HorizontalResolution,
                Info->VerticalResolution,
                Info->PixelsPerScanLine,
                PixelFormatDesc
            );
            #endif
        }

        #if REFIT_DEBUG > 0
        if (Mode > 99) {
            LOG_MSG( " NB: Real Mode is Mode %04d", Mode);
        }

        if (LoopCount >= (MaxMode - 1)) {
            LOG_MSG("\n\n");
        }
        #endif

        MY_FREE_POOL(Info);
    } // for

    // DA-TAG: Investigate This
    //         Seems likely to be closest to target later
    //
    // Try to drop to close to 64
    while (
        (MinPixelsLine !=   0) &&
        (MinPixelsLine  > 127) &&
        (MinPixelsLine  %   2) == 0
    ) {
        MinPixelsLine /= 2;
    }

    if (OurValidGOP) {
        Status = EFI_SUCCESS;
    }
    else {
        Status = EFI_UNSUPPORTED;

        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"Could *NOT* Find Usable GOP");
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s!!", MsgStr);
        LOG_MSG("INFO: %s:", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif
    }

    return Status;
} // EFI_STATUS egDumpGOPVideoModes()

//
// Sets mode via GOP protocol, and reconnects simple text out drivers
//
static
EFI_STATUS GopSetModeAndReconnectTextOut (
    IN UINT32 ModeNumber
) {
    EFI_STATUS Status;

    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;
    #endif


    if (GOPDraw == NULL) {
        return EFI_UNSUPPORTED;
    }

    Status = REFIT_CALL_2_WRAPPER(GOPDraw->SetMode, GOPDraw, ModeNumber);
    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (L"Switch Mode to GOP Mode[%02d] ... %r", ModeNumber, Status);
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("%s", MsgStr);
    LOG_MSG("\n\n");
    MY_FREE_POOL(MsgStr);
    #endif

    return Status;
} // static EFI_STATUS GopSetModeAndReconnectTextOut()

static
EFI_STATUS egSetGopMode (
    INT32 Next
) {
    EFI_STATUS                            Status;
    UINT32                                i, MaxMode;
    UINTN                                 SizeOfInfo;
    INT32                                 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;

    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;
    #endif


    if (GOPDraw == NULL) {
        #if REFIT_DEBUG > 0
        MsgStr = L"Could *NOT* Set GOP Mode";
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
        LOG_MSG("\n\n");
        LOG_MSG("** WARN: %s **", MsgStr);
        LOG_MSG("\n\n");
        #endif

        return EFI_UNSUPPORTED;
    }

    #if REFIT_DEBUG > 0
    LOG_MSG("Set GOP Mode:");
    #endif

    MaxMode = GOPDraw->Mode->MaxMode;
    Mode    = GOPDraw->Mode->Mode;
    Status  = EFI_UNSUPPORTED;

    if (MaxMode == 0) {
        #if REFIT_DEBUG > 0
        MsgStr = L"Incompatible GPU";
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
        LOG_MSG("\n\n");
        LOG_MSG("** WARN: %s", MsgStr);
        LOG_MSG("\n\n");
        #endif
    }
    else {
        i = 0;
        while (EFI_ERROR(Status) && i <= MaxMode) {
            Mode = Mode + Next;
            Mode = (Mode >= (INT32) MaxMode) ? 0 : Mode;
            Mode = (Mode < 0) ? ((INT32) MaxMode - 1) : Mode;

            Status = REFIT_CALL_4_WRAPPER(
                GOPDraw->QueryMode, GOPDraw,
                (UINT32) Mode, &SizeOfInfo, &Info
            );

            #if REFIT_DEBUG > 0
            LOG_MSG("%s    * Mode[%02d]", OffsetNext, Status);
            #endif

            if (!EFI_ERROR(Status)) {
                #if REFIT_DEBUG > 0
                LOG_MSG("\n");
                #endif

                Status = GopSetModeAndReconnectTextOut ((UINT32) Mode);
                if (!EFI_ERROR(Status)) {
                    egScreenHeight = GOPDraw->Mode->Info->VerticalResolution;
                    egScreenWidth  = GOPDraw->Mode->Info->HorizontalResolution;
                }
            }

            i++;
        } // while

        #if REFIT_DEBUG > 0
        if (EFI_ERROR(Status)) {
            LOG_MSG("\n\n");
        }
        #endif
    }

    return Status;
} // static EFI_STATUS egSetGopMode()

// On systems with GOP, set maximum available resolution.
// On systems with UGA, just record current resolution.
static
EFI_STATUS egSetMaxResolution (VOID) {
    EFI_STATUS                            Status;
    EFI_STATUS                            XStatus;
    UINT32                                Mode;
    UINT32                                Depth;
    UINT32                                Width;
    UINT32                                Height;
    UINT32                                SumOld;
    UINT32                                SumNew;
    UINT32                                MaxMode;
    UINT32                                BestMode;
    UINT32                                RefreshRate;
    UINTN                                 SizeOfInfo;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;

    #if REFIT_DEBUG > 0
    CHAR16                               *MsgStr;
    #endif

    if (GOPDraw == NULL) {
        if (UGADraw != NULL) {
            // Cannot do this with UGA.
            // So get and set basic data, then exit.
            REFIT_CALL_5_WRAPPER(
                UGADraw->GetMode, UGADraw,
                &Width, &Height,
                &Depth, &RefreshRate
            );

            GlobalConfig.RequestedScreenWidth  =  Width;
            GlobalConfig.RequestedScreenHeight = Height;
        }

        return EFI_UNSUPPORTED;
    }

    XStatus = EFI_NOT_READY;
    Info    =          NULL;
    Width   = Height   =  0;
    SumOld  = BestMode =  0;
    MaxMode = GOPDraw->Mode->MaxMode;

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"Set %s Available Mode on GOP-Mode-Active GFX Handle(s)",
        (MaxMode > 1) ? L"Best" : L"Only"
    );
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("%s:", MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    for (Mode = 0; Mode < MaxMode; Mode++) {
        Status = REFIT_CALL_4_WRAPPER(
            GOPDraw->QueryMode, GOPDraw,
            Mode, &SizeOfInfo, &Info
        );
        SumNew = Info->VerticalResolution + Info->HorizontalResolution;
        if (!EFI_ERROR(Status)) {
            if (SumNew > SumOld) {
                BestMode = Mode;
                SumOld   = SumNew;
                Height   = Info->VerticalResolution;
                Width    = Info->HorizontalResolution;
            }
        }
        if (EFI_ERROR(XStatus)) {
            XStatus = Status;
        }

        MY_FREE_POOL(Info);
    } // for

    #if REFIT_DEBUG > 0
    if (EFI_ERROR(XStatus)) {
        MsgStr = PoolPrint (
            L"%s (Max Rez) Mode ... %r",
            (MaxMode > 1) ? L"Best" : L"Only",
            XStatus
        );
    }
    else {
        MsgStr = PoolPrint (
            L"%s Mode:- 'GOP Mode[%02d] on GFX Handle[%02d] @ %d x %d'",
            (MaxMode > 1) ? L"Best (Max Rez)" : L"Only Available",
            BestMode, SelectedGOP,
            Width, Height
        );
    }

    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("%s  - %s", OffsetNext, MsgStr);
    LOG_MSG("\n");
    MY_FREE_POOL(MsgStr);
    #endif

    if (!EFI_ERROR(XStatus) && BestMode == GOPDraw->Mode->Mode) {
        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (
            L"Resolution Already Set to %s",
            (MaxMode > 1) ? L"Best Mode" : L"Only Mode Present"
        );
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("%s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        // Explicitly use 'GOPDraw->Mode->Info' below
        // as 'Info' is not available (freed earlier)
        egScreenHeight = GOPDraw->Mode->Info->VerticalResolution;
        egScreenWidth  = GOPDraw->Mode->Info->HorizontalResolution;

        // Proceed
        Status = EFI_SUCCESS;
    }
    else {
        if (EFI_ERROR(XStatus)) {
            Status = XStatus;
        }
        else {
            Status = GopSetModeAndReconnectTextOut (BestMode);
        }

        if (!EFI_ERROR(Status)) {
            egScreenWidth  = Width;
            egScreenHeight = Height;
        }
        else {
            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (
                L"Could *NOT* Set Intended GOP Mode ... %s",
                (MaxMode > 1) ? L"Try First Useable Mode" : L"Exit Mode Search"
            );
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
            LOG_MSG("** WARN: %s", MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
            #endif

            // Cannot set BestMode ... Search for first usable one or exit
            Status = (MaxMode > 1) ? egSetGopMode (1) : EFI_UNSUPPORTED;
        }
    }

    return Status;
} // static EFI_STATUS egSetMaxResolution()


//
// Screen handling
//

// Make the necessary system calls to identify the current graphics mode.
// Stores the results in the file-global variables egScreenWidth,
// egScreenHeight, and egHasGraphics. The first two of these will be
// unchanged if neither GOPDraw nor UGADraw is a valid pointer.
static
VOID egDetermineScreenSize (VOID) {
    EFI_STATUS Status;
    UINT32     ScreenW;
    UINT32     ScreenH;
    UINT32     UgaDepth;
    UINT32     UgaRefreshRate;

    // Get screen size
    if (GOPDraw != NULL) {
        egHasGraphics  = TRUE;
        egScreenHeight = GOPDraw->Mode->Info->VerticalResolution;
        egScreenWidth  = GOPDraw->Mode->Info->HorizontalResolution;
    }
    else if (UGADraw != NULL) {
        Status = REFIT_CALL_5_WRAPPER(
            UGADraw->GetMode, UGADraw,
            &ScreenW, &ScreenH,
            &UgaDepth, &UgaRefreshRate
        );
        if (EFI_ERROR(Status)) {
            // Graphics *IS NOT* Available
            UGADraw       =  NULL;
            egHasGraphics = FALSE;
        }
        else {
            egScreenWidth  = ScreenW;
            egScreenHeight = ScreenH;
            egHasGraphics  =    TRUE;
        }
    }
} // static VOID egDetermineScreenSize()

UINTN egCountAppleFramebuffers (VOID) {
    EFI_STATUS                       Status;
    UINTN                            HandleCount;
    EFI_GUID                         AppleFramebufferInfoProtocolGuid = APPLE_FRAMEBUFFER_INFO_PROTOCOL_GUID;
    EFI_HANDLE                      *HandleBuffer;
    APPLE_FRAMEBUFFER_INFO_PROTOCOL *FramebufferInfo;


    if (!AppleFirmware) {
        return 0;
    }

    Status = LibLocateProtocol (&AppleFramebufferInfoProtocolGuid, (VOID *) &FramebufferInfo);
    if (EFI_ERROR(Status)) {
        return 0;
    }

    HandleBuffer = NULL;
    Status = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, ByProtocol,
        &AppleFramebufferInfoProtocolGuid, NULL,
        &HandleCount, &HandleBuffer
    );
    if (EFI_ERROR(Status)) {
        HandleCount = 0;
    }

    MY_FREE_POOL(HandleBuffer);

    return HandleCount;
} // UINTN egCountAppleFramebuffers()

VOID egGetScreenSize (
    OUT UINTN *ScreenWidth,
    OUT UINTN *ScreenHeight
) {
    egDetermineScreenSize();

    if (ScreenWidth != NULL) {
        *ScreenWidth = egScreenWidth;
    }
    if (ScreenHeight != NULL) {
        *ScreenHeight = egScreenHeight;
    }
} // VOID egGetScreenSize()

static
VOID egInitConsoleControl (VOID) {
    EFI_STATUS                     Status;
    UINTN                          i;
    UINTN                          HandleCount;
    EFI_HANDLE                    *HandleBuffer;

    #if REFIT_DEBUG > 0
    CHAR16     *MsgStr;

    LOG_MSG("%s  - Locate Console Control Protocol", OffsetNext);
    #endif

    // Check ConsoleOut Handle
    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol, gST->ConsoleOutHandle,
        &ConsoleControlProtocolGuid, (VOID **) &ConsoleControl
    );
    if (Status == EFI_UNSUPPORTED) {
        Status = EFI_NOT_FOUND;
    }
    #if REFIT_DEBUG > 0
    LOG_MSG("%s    * Seek on ConOut Handle ... %r", OffsetNext, Status);
    #endif

    if (EFI_ERROR(Status)) {
        // Try Locating by Handle
        HandleBuffer = NULL;
        Status = REFIT_CALL_5_WRAPPER(
            gBS->LocateHandleBuffer, ByProtocol,
            &ConsoleControlProtocolGuid, NULL,
            &HandleCount, &HandleBuffer
        );
        if (Status == EFI_UNSUPPORTED) {
            Status = EFI_NOT_FOUND;
        }
        #if REFIT_DEBUG > 0
        LOG_MSG("%s    * Seek on GFX Handle(s) ... %r", OffsetNext, Status);
        #endif

        if (!EFI_ERROR(Status)) {
            for (i = 0; i < HandleCount; i++) {
                Status = REFIT_CALL_3_WRAPPER(
                    gBS->HandleProtocol, HandleBuffer[i],
                    &ConsoleControlProtocolGuid, (VOID *) &ConsoleControl
                );
                if (HandleBuffer[i] == gST->ConsoleOutHandle) {
                    #if REFIT_DEBUG > 0
                    LOG_MSG("%s    ** Bypassed ConOut Handle[%02d]",
                        OffsetNext, i
                    );
                    #endif

                    // Restart Loop
                    continue;
                }
                #if REFIT_DEBUG > 0
                LOG_MSG("%s    ** Evaluate on GFX Handle[%02d] ... %r",
                    OffsetNext, i, Status
                );
                #endif
                if (!EFI_ERROR(Status)) {
                    break;
                }
            } // for
        }
        MY_FREE_POOL(HandleBuffer);
    }

    #if REFIT_DEBUG > 0
    MsgStr = (EFI_ERROR(Status))
        ? L"Assess Console Control Protocol ... NOT OK!!"
        : L"Assess Console Control Protocol ... ok";
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("%s  - %s", OffsetNext, MsgStr);
    #endif

    if (EFI_ERROR(Status)) {
        GotConsoleControl = FALSE;
    }
} // static VOID egInitConsoleControl()

static
VOID LogUGADrawExit (
    EFI_STATUS Status
) {
    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;

    MsgStr = (EFI_ERROR(Status))
        ? L"Assess Universal Graphics Adapter ... NOT OK!!"
        : L"Assess Universal Graphics Adapter ... ok";
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("%s  - %s", OffsetNext, MsgStr);
    #endif
} // static VOID LogUGADrawExit()

BOOLEAN egInitUGADraw (
    BOOLEAN LogOutput
) {
    EFI_STATUS                     Status;
    UINTN                          i;
    UINTN                          HandleCount;
    UINT32                         Depth;
    UINT32                         Width;
    UINT32                         Height;
    UINT32                         RefreshRate;
    BOOLEAN                        UGAonGPU;
    EFI_HANDLE                    *HandleBuffer;
    EFI_UGA_DRAW_PROTOCOL         *TmpUGA;

    #if REFIT_DEBUG > 0
    BOOLEAN    CheckMute = FALSE;

    if (!LogOutput) {
        MY_MUTELOGGER_SET;
    }
    #endif

    // Check ConsoleOut Handle
    // DA-TAG: Investigate This
    //         ConsoleOut UGA appears inadequate
    //         Using TmpUGA to keep UGADraw NULL
    TmpUGA = NULL;
    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol, gST->ConsoleOutHandle,
        &UgaDrawProtocolGuid, (VOID *) &TmpUGA
    );
    if (Status == EFI_UNSUPPORTED) {
        Status = EFI_NOT_FOUND;
    }
    #if REFIT_DEBUG > 0
    LOG_MSG("%s", OffsetNext);
    LOG_MSG("%s  - Locate Universal Graphics Adapter", OffsetNext);
    LOG_MSG("%s    * Seek on ConOut Handle ... %r", OffsetNext, Status);
    #endif

    if (!EFI_ERROR(Status)) {
        UGADraw = TmpUGA;
        LogUGADrawExit (Status);

        return TRUE;
    }

    // Try Locating on GFX Handle
    HandleBuffer = NULL;
    Status = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, ByProtocol,
        &UgaDrawProtocolGuid, NULL,
        &HandleCount, &HandleBuffer
    );
    if (HandleCount == 1 || Status == EFI_UNSUPPORTED) {
        Status = EFI_NOT_FOUND;
    }
    #if REFIT_DEBUG > 0
    LOG_MSG("%s    * Seek on GFX Handle(s) ... %r", OffsetNext, Status);
    #endif

    UGAonGPU = FALSE;
    if (!EFI_ERROR(Status)) {
        for (i = 0; i < HandleCount; i++) {
            if (HandleBuffer[i] == gST->ConsoleOutHandle) {
                #if REFIT_DEBUG > 0
                LOG_MSG("%s    ** Skipped Handle[%02d] ... ConsoleOut Handle",
                    OffsetNext, i
                );
                #endif

                // Restart Loop
                continue;
            }

            #if REFIT_DEBUG > 0
            LOG_MSG("%s    ** Examine Handle[%02d] ... %r",
                OffsetNext, i, Status
            );
            #endif

            Status = REFIT_CALL_3_WRAPPER(
                gBS->HandleProtocol, HandleBuffer[i],
                &UgaDrawProtocolGuid, (VOID *) &TmpUGA
            );
            if (EFI_ERROR(Status)) {
                // Restart Loop
                continue;
            }

            Status = REFIT_CALL_5_WRAPPER(
                TmpUGA->GetMode, TmpUGA,
                &Width, &Height,
                &Depth, &RefreshRate
            );
            if (!EFI_ERROR(Status)) {
                if (Width  > 0 &&
                    Height > 0
                ) {
                    UGAonGPU  =   TRUE;
                    UGADraw   = TmpUGA;

                    #if REFIT_DEBUG > 0
                    LOG_MSG("%s    *** Select Handle[%02d] @ %5d x %-5d",
                        OffsetNext, i, Width, Height
                    );
                    #endif

                    // Break Loop
                    break;
                }

                #if REFIT_DEBUG > 0
                LOG_MSG("%s    *** Ignore Handle[%02d] @ %5d x %-5d",
                    OffsetNext, i, Width, Height
                );
                #endif
            }
        } // for
        MY_FREE_POOL(HandleBuffer);
    } // if !EFI_ERROR(Status)

    #if REFIT_DEBUG > 0
    LogUGADrawExit (Status);

    if (!LogOutput) {
        MY_MUTELOGGER_OFF;
    }
    #endif

    return UGAonGPU;
} // static BOOLEAN egInitUGADraw()

VOID egInitScreen (VOID) {
    EFI_STATUS                             Status;
    EFI_STATUS                             XFlag;
    UINTN                                  i;
    UINTN                                  HandleCount;
    UINTN                                  SizeOfInfo;
    UINT32                                 SumOld;
    UINT32                                 SumNew;
    UINT32                                 Depth;
    UINT32                                 Width;
    UINT32                                 Height;
    UINT32                                 MaxMode;
    UINT32                                 GopMode;
    UINT32                                 GopWidth;
    UINT32                                 GopHeight;
    UINT32                                 TmpScreenW;
    UINT32                                 TmpScreenH;
    UINT32                                 TmpUgaDepth;
    UINT32                                 RefreshRate;
    UINT32                                 TmpUgaRefreshRate;
    BOOLEAN                                NewAppleFramebuffers;
    BOOLEAN                                FoundHandleUGA;
    BOOLEAN                                FlagUGA;
    BOOLEAN                                thisValidGOP;
    EFI_HANDLE                            *HandleBuffer;
    EFI_GRAPHICS_OUTPUT_PROTOCOL          *OldGop;
    EFI_GRAPHICS_OUTPUT_PROTOCOL          *TmpGop;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;

    #if REFIT_DEBUG > 0
    CHAR16     *MsgStr;
    BOOLEAN     PrevFlag;
    BOOLEAN     SelectedOnce;

    LOG_MSG("Determine Graphics Control Method:");
    #endif

    // Get ConsoleControl Protocol
    egInitConsoleControl();

    // Get UGADraw Protocol
    FoundHandleUGA = FlagUGA = egInitUGADraw (TRUE);

    // Align PreferUGA
    if (GlobalConfig.PreferUGA && FoundHandleUGA && !SetPreferUGA) {
        Status = REFIT_CALL_5_WRAPPER(
            UGADraw->GetMode, UGADraw,
            &TmpScreenW, &TmpScreenH,
            &TmpUgaDepth, &TmpUgaRefreshRate
        );
        if (EFI_ERROR(Status)) {
            UGADraw = NULL;
        }
        else {
            SetPreferUGA = TRUE;
        }
    }

    // Set 'ObtainHandleGOP', if false, to 'SetConsoleGOP' value
    if (!ObtainHandleGOP) {
        ObtainHandleGOP = GlobalConfig.SetConsoleGOP;
    }

    // Get GOPDraw Protocol
    GotGoodGOP   = FALSE;
    thisValidGOP = FALSE;
    HandleBuffer =  NULL;
    if (FoundHandleUGA && (SetPreferUGA || !ObtainHandleGOP)) {
        #if REFIT_DEBUG > 0
        LOG_MSG("\n\n");
        LOG_MSG(
            "INFO: Skip GOP Search ... %s",
            (SetPreferUGA)
                ? L"Enforced 'UGA-Only' Mode"
                : L"Apparent 'UGA-Only' Graphics"
        );
        LOG_MSG("\n\n");
        #endif
    }
    else {
        do {
            // Check ConsoleOut Handle
            Status = REFIT_CALL_3_WRAPPER(
                gBS->HandleProtocol, gST->ConsoleOutHandle,
                &GOPDrawProtocolGuid, (VOID **) &OldGop
            );
            if (Status == EFI_UNSUPPORTED) {
                Status  = EFI_NOT_FOUND;
            }

            #if REFIT_DEBUG > 0
            LOG_MSG("%s", OffsetNext);
            LOG_MSG("%s  - Locate Graphics Output Protocol", OffsetNext);
            LOG_MSG("%s    * Seek on ConOut Handle ... %r", OffsetNext, Status);
            #endif

            if (!EFI_ERROR(Status)) {
                break;
            }

            // Try Locating by Handle
            Status = REFIT_CALL_5_WRAPPER(
                gBS->LocateHandleBuffer, ByProtocol,
                &GOPDrawProtocolGuid, NULL,
                &HandleCount, &HandleBuffer
            );
            // Force all errors to NOT FOUND on error as subsequent code relies on this
            if (EFI_ERROR(Status)) {
                Status = EFI_NOT_FOUND;
            }
            #if REFIT_DEBUG > 0
            LOG_MSG("%s    * Seek on GFX Handle(s) ... %r", OffsetNext, Status);
            #endif
            if (!EFI_ERROR(Status)) {
                break;
            }

            TmpGop = NULL;
            GopWidth  = 0;
            GopHeight = 0;
            MaxMode   = 0;
            SumOld    = 0;
            SumNew    = 0;
            for (i = 0; i < HandleCount; i++) {
                if (HandleBuffer[i] == gST->ConsoleOutHandle) {
                    #if REFIT_DEBUG > 0
                    LOG_MSG("%s       Avoid ConOut Handle[%02d]", OffsetNext, i);
                    #endif

                    // Restart Loop
                    continue;
                }

                #if REFIT_DEBUG > 0
                LOG_MSG("%s    ** Evaluate GFX Handle[%02d] ... %r",
                    OffsetNext, i, Status
                );
                #endif

                Status = REFIT_CALL_3_WRAPPER(
                    gBS->HandleProtocol, HandleBuffer[i],
                    &GOPDrawProtocolGuid, (VOID*) &TmpGop
                );
                if (!EFI_ERROR(Status)) {
                    #if REFIT_DEBUG > 0
                    SelectedOnce = FALSE;
                    #endif
                    MaxMode = TmpGop->Mode->MaxMode;
                    for (GopMode = 0; GopMode < MaxMode; GopMode++) {
                        Status = TmpGop->QueryMode (
                            TmpGop, GopMode, &SizeOfInfo, &Info
                        );
                        if (EFI_ERROR(Status)) {
                            #if REFIT_DEBUG > 0
                            LOG_MSG(
                                "%s         Ignore GFX Handle[%02d][%02d] ... QueryMode Failure",
                                OffsetNext, i, GopMode
                            );
                            #endif

                            // Restart Loop
                            continue;
                        }

                        SumNew = Info->VerticalResolution + Info->HorizontalResolution;
                        if (SumOld >= SumNew) {
                            #if REFIT_DEBUG > 0
                            LOG_MSG(
                                "%s         Ignore GFX Handle[%02d][%02d] @ %5d x %-5d%s",
                                OffsetNext, i, GopMode,
                                Info->HorizontalResolution,
                                Info->VerticalResolution
                            );
                            #endif
                        }
                        else {
                            OldGop      = TmpGop;
                            SumOld      = SumNew;
                            GopWidth    = Info->HorizontalResolution;
                            GopHeight   = Info->VerticalResolution;
                            SelectedGOP = i;

                            #if REFIT_DEBUG > 0
                            LOG_MSG(
                                "%s    **** Select GFX Handle[%02d][%02d] @ %5d x %-5d%s",
                                OffsetNext, i, GopMode,
                                GopWidth,
                                GopHeight,
                                (SelectedOnce) ? L"  :  Discard Previous" : L""
                            );
                            SelectedOnce = TRUE;
                            #endif
                        }

                        #if REFIT_DEBUG > 0
                        LOG_MSG(
                            "%s         Ignore GFX Handle[%02d][%02d] @ %5d x %-5d",
                            OffsetNext, i, GopMode,
                            Info->HorizontalResolution,
                            Info->VerticalResolution
                        );
                        #endif

                        MY_FREE_POOL(Info);
                    } // for GopMode = 0
                } // if !EFI_ERROR(Status)
            } // for

            MY_FREE_POOL(HandleBuffer);
        } while (0); // This 'loop' only runs once

        XFlag = EFI_UNSUPPORTED;
        if (EFI_ERROR(Status)) {
            // Tag as "Not Found"
            XFlag = EFI_NOT_FOUND;

            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (L"Assess Graphics Output Protocol ... %r!!", Status);
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("%s  - %s", OffsetNext, MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
            #endif
        }
        else {
            if (OldGop != NULL &&
                OldGop->Mode->MaxMode > 0
            ) {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"Assess Graphics Output Protocol ... ok");
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("%s  - %s", OffsetNext, MsgStr);
                MY_FREE_POOL(MsgStr);
                #endif

                if (!GlobalConfig.SetConsoleGOP) {
                    GOPDraw = OldGop;
                    thisValidGOP = TRUE;

                    #if REFIT_DEBUG > 0
                    LOG_MSG("\n\n");
                    #endif
                }
                else {
                    thisValidGOP = SupplyConsoleGop (FALSE);
                    XFlag = (thisValidGOP) ? EFI_SUCCESS : EFI_LOAD_ERROR;
                    #if REFIT_DEBUG > 0
                    if (!GotGoodGOP) {
                        MsgStr = PoolPrint (
                            L"Provide GOP on ConsoleOut Handle ... %r",
                            XFlag
                        );
                        ALT_LOG(1, LOG_THREE_STAR_END, L"%s", MsgStr);
                        LOG_MSG("INFO: %s", MsgStr);
                    }
                    #endif
                    if (EFI_ERROR(XFlag)) {
                        #if REFIT_DEBUG > 0
                        MY_FREE_POOL(MsgStr);
                        MsgStr = StrDuplicate (L"Leveraging GFX Handle GOP");
                        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                        if (GotGoodGOP) {
                            LOG_MSG("INFO: ");
                        }
                        else {
                            LOG_MSG("%s      ", OffsetNext);
                        }
                        LOG_MSG("%s", MsgStr);
                        #endif

                        // Set to Needed Value ... Previous, EFI_LOAD_ERROR, was only for logging
                        XFlag = EFI_SUCCESS;

                        // Use valid 'OldGop' and force 'thisValidGOP' to true
                        GOPDraw = OldGop;
                        thisValidGOP = TRUE;
                    }
                    #if REFIT_DEBUG > 0
                    LOG_MSG("\n\n");
                    MY_FREE_POOL(MsgStr);
                    #endif
                }
            }
            else {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (
                    L"Assess Graphics Output Protocol ... NOT OK!!"
                );
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("%s  - %s", OffsetNext, MsgStr);
                MY_FREE_POOL(MsgStr);
                #endif

                if (!GlobalConfig.SetConsoleGOP) {
                    #if REFIT_DEBUG > 0
                    LOG_MSG("\n\n");
                    #endif

                    // Disable GOP
                    thisValidGOP = FALSE;
                    DisableGOP();
                }
                else {
                    thisValidGOP = SupplyConsoleGop (TRUE);
                    XFlag = (thisValidGOP) ? EFI_SUCCESS : EFI_LOAD_ERROR;
                    #if REFIT_DEBUG > 0
                    if (!GotGoodGOP) {
                        MsgStr = PoolPrint (
                            L"Replace GOP on ConsoleOut Handle ... %r",
                            XFlag
                        );
                        ALT_LOG(1, LOG_THREE_STAR_END, L"%s", MsgStr);
                        LOG_MSG("INFO: %s", MsgStr);
                    }
                    #endif
                    if (EFI_ERROR(XFlag)) {
                        #if REFIT_DEBUG > 0
                        MY_FREE_POOL(MsgStr);
                        // DA-TAG: Actually already disabled
                        MsgStr = StrDuplicate (L"Disabling Available GOP");
                        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                        LOG_MSG("%s      %s", OffsetNext, MsgStr);
                        #endif

                        // Set to Needed Value ... Previous, EFI_LOAD_ERROR, was only for logging
                        XFlag = EFI_UNSUPPORTED;

                        // Disable GOP
                        // NB: 'thisValidGOP' already false
                        DisableGOP();
                    }
                    #if REFIT_DEBUG > 0
                    LOG_MSG("\n\n");
                    MY_FREE_POOL(MsgStr);
                    #endif
                }
            } // if/else OldGop
        } // if/else Status == EFI_NOT_FOUND

        if (XFlag == EFI_SUCCESS && GlobalConfig.UseDirectGop) {
            if (GOPDraw == NULL) {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"Cannot Implement Direct GOP Renderer");
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("INFO: %s", MsgStr);
                LOG_MSG("\n\n");
                MY_FREE_POOL(MsgStr);
                #endif
            }
            else {
                if (GOPDraw->Mode->Info->PixelFormat == PixelBltOnly) {
                    Status = EFI_UNSUPPORTED;
                }
                else {
                    // DA-TAG: Limit to TianoCore
                    #ifdef __MAKEWITH_TIANO
                    Status = OcUseDirectGop (-1);
                    #else
                    Status = EFI_NOT_STARTED;
                    #endif
                }
                if (!EFI_ERROR(Status)) {
                    // Check ConsoleOut Handle
                    Status = REFIT_CALL_3_WRAPPER(
                        gBS->HandleProtocol, gST->ConsoleOutHandle,
                        &GOPDrawProtocolGuid, (VOID **) &OldGop
                    );
                    if (!EFI_ERROR(Status)) {
                        if (OldGop != NULL        &&
                            OldGop->Mode->MaxMode != 0
                        ) {
                            GOPDraw = OldGop;
#if REFIT_DEBUG > 0
                            Status = EFI_SUCCESS;
                        }
                        else {
                            Status = EFI_NOT_READY;
#endif
                        }
                    }
                }

                #if REFIT_DEBUG > 0
                MsgStr = PoolPrint (
                    L"Implement Direct GOP Renderer ... %r",
                    Status
                );
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("INFO: %s", MsgStr);
                LOG_MSG("\n\n");
                MY_FREE_POOL(MsgStr);
                #endif
            } // if/else GOPDraw == NULL
        } // if XFlag == (EFI_SUCCESS && GlobalConfig.UseDirectGop)
    } // if/else FoundHandleUGA && (SetPreferUGA || !ObtainHandleGOP)

    if (thisValidGOP && GOPDraw) {
        do {
            if (FoundHandleUGA && SetPreferUGA) {
                // Disable GOP
                thisValidGOP = FALSE;
                DisableGOP();

                // Break Loop
                break;
            }

            Status = egDumpGOPVideoModes();
            if (EFI_ERROR(Status)) {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"Invalid GOP Instance");
                ALT_LOG(1, LOG_LINE_NORMAL, L"WARNING: %s!!", MsgStr);
                LOG_MSG("** WARN: %s", MsgStr);
                LOG_MSG("\n\n");
                MY_FREE_POOL(MsgStr);
                #endif

                // Disable GOP
                thisValidGOP = FALSE;
                DisableGOP();

                // Break Loop
                break;
            }

            // GOP Graphics Available
            egHasGraphics = TRUE;

            Status = egSetMaxResolution();
            if (!EFI_ERROR(Status)) {
                egScreenHeight = GOPDraw->Mode->Info->VerticalResolution;
                egScreenWidth  = GOPDraw->Mode->Info->HorizontalResolution;
            }
            else {
                egScreenWidth  = GlobalConfig.RequestedScreenWidth;
                egScreenHeight = GlobalConfig.RequestedScreenHeight;
            }
        } while (0); // This 'loop' only runs once
    } // if GOPDraw

    NewAppleFramebuffers = FALSE;

// DA-TAG: Limit to TianoCore
#ifdef __MAKEWITH_TIANO
    #if REFIT_DEBUG > 0
    PrevFlag = FALSE;
    #endif

    // Install AppleFramebuffers and Update AppleFramebuffer Count
    RefitAppleFbInfoInstallProtocol();
    if (AppleFramebuffers == 0) {
        AppleFramebuffers = egCountAppleFramebuffers();
        if (AppleFramebuffers > 0) {
            NewAppleFramebuffers = TRUE;
        }
    }
#endif
// Limit to TianoCore ... END

    if (thisValidGOP && GOPDraw) {
        Status = EFI_NOT_STARTED;
        if (thisValidGOP) {
            // DA-TAG: Limit to TianoCore
            #ifdef __MAKEWITH_TIANO
            if (GlobalConfig.PassUgaThrough) {
                Status = RefitPassUgaThrough();
            }
            #endif
            if (EFI_ERROR(Status)) {
                // Prefer GOP ... Discard UGA
                UGADraw =  NULL;
                FlagUGA = FALSE;
            }
        }

        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (L"Implement Pass UGA Thru ... %r", Status);
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("INFO: %s", MsgStr);
        MY_FREE_POOL(MsgStr);
        PrevFlag = TRUE;
        #endif
    } // if GOPDraw

    Height = Width = 0; // DA-TAG: Redundant for Infer
    if (UGADraw != NULL) {
        Status = REFIT_CALL_5_WRAPPER(
            UGADraw->GetMode, UGADraw,
            &Width, &Height,
            &Depth, &RefreshRate
        );
        if (EFI_ERROR(Status)) {
            UGADraw = NULL;
        }
    }

    if (!thisValidGOP && UGADraw == NULL) {
        // Graphics *IS NOT* Available
        // Fall Back on Text Mode
        egHasGraphics         = FlagUGA       = FALSE;
        GlobalConfig.TextOnly = ForceTextOnly =  TRUE;

        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (
            L"Graphics *NOT* Available ... Falling Back on Text Mode"
        );
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        if (PrevFlag) {
            LOG_MSG("%s      ", OffsetNext);
        }
        else {
            LOG_MSG("INFO: ");
            PrevFlag = TRUE;
        }
        LOG_MSG("%s", MsgStr);
        MY_FREE_POOL(MsgStr);
        #endif
    }
    else if (UGADraw != NULL) {
        // UGA Graphics Available
        egHasGraphics = FlagUGA = TRUE;

        if (SetPreferUGA) {
            #if REFIT_DEBUG > 0
            MsgStr = StrDuplicate (L"Forcing Universal Graphics Adapter");
            #endif

            // Infer: Do not flag 'thisValidGOP' as no longer used
            DisableGOP();
        }
        else if (thisValidGOP) {
            // Prefer GOP ... Ignore UGA
            FlagUGA = FALSE;
        }
        else {
            #if REFIT_DEBUG > 0
            MsgStr = StrDuplicate (
                L"GOP *NOT* Available ... Falling Back on UGA"
            );
            #endif
        }

        if (FlagUGA) {
            egScreenWidth  = GlobalConfig.RequestedScreenWidth  =  Width;
            egScreenHeight = GlobalConfig.RequestedScreenHeight = Height;

            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            if (PrevFlag) {
                LOG_MSG("%s      ", OffsetNext);
            }
            else {
                LOG_MSG("INFO: ");
                PrevFlag = TRUE;
            }
            LOG_MSG("%s", MsgStr);
            MY_FREE_POOL(MsgStr);
            #endif
        }
    } // if !thisValidGOP && UGADraw == NULL

    // DA-TAG: Limit to TianoCore
    #ifdef __MAKEWITH_TIANO
    if (UGADraw != NULL && GOPDraw == NULL) {
        if (!GlobalConfig.PassGopThrough) {
            Status = EFI_NOT_STARTED;
        }
        else {
            Status = RefitProvideGopPassThrough (FALSE);
        }

        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (L"Implement Pass GOP Thru ... %r", Status);
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        if (PrevFlag) {
            LOG_MSG("%s      ", OffsetNext);
        }
        else {
            LOG_MSG("\n\n");
            LOG_MSG("INFO: ");
            PrevFlag = TRUE;
        }
        LOG_MSG("%s", MsgStr);
        MY_FREE_POOL(MsgStr);
        #endif
    }
    #endif
    // Limit to TianoCore ... END

    if (GOPDraw != NULL) {
        // We have graphics from GOP ... Discard UGA
        if (UGADraw != NULL){
            UGADraw = NULL;
        }
        egHasGraphics =  TRUE;
        FlagUGA       = FALSE;

        // Prime Status for Text Renderer
        Status = EFI_NOT_STARTED;

        #ifdef __MAKEWITH_TIANO
        // DA-TAG: Limit to TianoCore
        if ((GlobalConfig.UseTextRenderer) ||
            (AppleFirmware && GlobalConfig.TextOnly)
        ) {
            // Implement Text Renderer
            Status = OcUseBuiltinTextOutput (
                (egHasGraphics)
                    ? EfiConsoleControlScreenGraphics
                    : EfiConsoleControlScreenText
            );
        }
        #endif
        // Limit to TianoCore ... END

        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (L"Implement Text Renderer ... %r", Status);
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        if (PrevFlag) {
            LOG_MSG("%s      ", OffsetNext);
        }
        else {
            LOG_MSG("\n\n");
            LOG_MSG("INFO: ");
            PrevFlag = TRUE;
        }
        LOG_MSG("%s", MsgStr);
        MY_FREE_POOL(MsgStr);
        #endif
    } // if GOPDraw

    if (!egHasGraphics) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"NO");
        #endif
    }
    else if (GOPDraw == NULL && NewAppleFramebuffers) {
        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (
            L"YES (Potentially Without Display%s)",
            (GlobalConfig.TextOnly)
                ? L""
                : L" ... Try 'textonly' config setting if so"
        );
        #endif
    }
    else if (!FlagUGA || !AppleFirmware || AppleFramebuffers > 0) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"YES");
        #endif
    }
    else {
        // Force Text Mode ... AppleFramebuffers Missing on Mac with UGA
        // Disable GOP
        DisableGOP();
        UGADraw                               =  NULL;
        egHasGraphics                         = FALSE;
        GlobalConfig.TextOnly = ForceTextOnly =  TRUE;

        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"YES (Without Display ... Forcing Text Mode)");
        #endif
    }

    #if REFIT_DEBUG > 0
    if (PrevFlag) {
        LOG_MSG("%s      ", OffsetNext);
    }
    else {
        LOG_MSG("\n\n");
        LOG_MSG("INFO: ");
    }
    LOG_MSG("Graphical View Possible ... %s", MsgStr);
    LOG_MSG("\n\n");
    MY_FREE_POOL(MsgStr);
    #endif
} // VOID egInitScreen()

// Convert a graphics mode (in *ModeWidth) to a width and height (returned in
// *ModeWidth and *Height, respectively).
// Returns TRUE if successful, FALSE if not (invalid mode, typically)
BOOLEAN egGetResFromMode (
    UINTN *ModeWidth,
    UINTN *Height
) {
    UINTN                                  Size;
    EFI_STATUS                             Status;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;

    if ((ModeWidth != NULL) && (Height != NULL) && GOPDraw) {
        Info = NULL;
        Status = REFIT_CALL_4_WRAPPER(
            GOPDraw->QueryMode, GOPDraw,
            *ModeWidth, &Size, &Info
        );
        if (!EFI_ERROR(Status) && (Info != NULL)) {
            *ModeWidth = Info->HorizontalResolution;
            *Height    = Info->VerticalResolution;

            MY_FREE_POOL(Info);

            return TRUE;
        }
    }

    return FALSE;
} // BOOLEAN egGetResFromMode()

// Sets the screen resolution to the specified value, if possible. If *ScreenHeight
// is 0 and GOP mode is detected, assume that *ScreenWidth contains a GOP mode
// number rather than a horizontal resolution. If the specified resolution is not
// valid, displays a warning with the valid modes on GOP (UEFI) systems, or silently
// fails on UGA (EFI 1.x) systems. Note that this function attempts to set ANY screen
// resolution, even 1x1 or ridiculously large values.
// Upon success, returns actual screen resolution in *ScreenWidth and *ScreenHeight.
// These values are unchanged upon failure.
// Returns TRUE if successful, FALSE if not.
BOOLEAN egSetScreenSize (
    IN OUT UINTN *ScreenWidth,
    IN OUT UINTN *ScreenHeight
) {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;

    EFI_STATUS   Status;
    BOOLEAN      ModeSet;
    UINTN        Size;
    UINT32       ModeNum;
    UINT32       ScreenW;
    UINT32       ScreenH;
    UINT32       UgaDepth;
    UINT32       UgaRefreshRate;
    UINT32       CurrentModeNum;

    #if REFIT_DEBUG > 0
    CHAR16      *MsgStr;
    CHAR16      *PixelFormatDesc;


    MsgStr = StrDuplicate (L"Set User Defined Resolution");
    LOG_MSG("%s:", MsgStr);
    MY_FREE_POOL(MsgStr);

    MsgStr = PoolPrint (L"Target:- '%d x %d'", *ScreenWidth, *ScreenHeight);
    LOG_MSG("%s  - %s", OffsetNext, MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    ModeSet = FALSE;
    if (UGADraw != NULL) {
        if (*ScreenHeight == 0) {
            #if REFIT_DEBUG > 0
            MsgStr = StrDuplicate (
                L"Defined 'ScreenHeight' Setting is *NOT* Valid for UGA"
            );
            LOG_MSG("\n\n");
            LOG_MSG("INFO: %s", MsgStr);
            MY_FREE_POOL(MsgStr);
            #endif
        }
        else {
            // Try to use current color depth and refresh rate for new mode.
            // May not always be the best, but unable to probe for alternatives.
            REFIT_CALL_5_WRAPPER(
                UGADraw->GetMode, UGADraw,
                &ScreenW, &ScreenH,
                &UgaDepth, &UgaRefreshRate
            );

            Status = REFIT_CALL_5_WRAPPER(
                UGADraw->SetMode, UGADraw,
                ScreenW, ScreenH,
                UgaDepth, UgaRefreshRate
            );
            *ScreenWidth  = (UINTN) ScreenW;
            *ScreenHeight = (UINTN) ScreenH;
            if (!EFI_ERROR(Status)) {
                ModeSet        = TRUE;
                egScreenWidth  = *ScreenWidth;
                egScreenHeight = *ScreenHeight;
            }
            else {
                // DA-TAG: Investigate This
                //         Find a list of supported modes and display it.
                //
                //         Below does not appear unless text mode is explicitly set.
                //         This is just a placeholder pending something better.
                #if REFIT_DEBUG > 0
                MsgStr = PoolPrint (
                    L"Error: When Setting %d x %d Resolution ... Unsupported Mode",
                    *ScreenWidth, *ScreenHeight
                );
                LOG_MSG("\n");
                LOG_MSG("%s", MsgStr);
                MY_FREE_POOL(MsgStr);
                #endif
            }
        }
    }
    else if (GOPDraw != NULL) {
        // GOP mode (UEFI)
        CurrentModeNum = GOPDraw->Mode->Mode;

        if (*ScreenHeight == 0) {
            // Use user specified mode number (stored in *ScreenWidth) directly
            ModeNum = (UINT32) *ScreenWidth;
            if (ModeNum != CurrentModeNum) {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (
                    L"Set GOP Mode from Configured Screen Width"
                );
                #endif

                ModeSet = TRUE;
            }
            else if (
                egGetResFromMode (ScreenWidth, ScreenHeight) &&
                REFIT_CALL_2_WRAPPER(
                    GOPDraw->SetMode, GOPDraw, ModeNum
                ) == EFI_SUCCESS
            ) {
                ModeSet = TRUE;

                #if REFIT_DEBUG > 0
                MsgStr = PoolPrint (
                    L"Mode Pointed at GOP Mode[%02d]",
                    ModeNum
                );
                #endif

            }
            else {
                #if REFIT_DEBUG > 0
                MsgStr = PoolPrint (
                    L"Could *NOT* Set GOP Mode[%02d]",
                    ModeNum
                );
                #endif
            }

            #if REFIT_DEBUG > 0
            LOG_MSG("%s    * %s", OffsetNext, MsgStr);
            MY_FREE_POOL(MsgStr);
            #endif
        }
        else {
            // Loop through modes to see if the specified
            // one is available and switch to this if so.
            ModeNum = 0;
            while (!ModeSet && ModeNum < GOPDraw->Mode->MaxMode) {
                Status = REFIT_CALL_4_WRAPPER(
                    GOPDraw->QueryMode, GOPDraw,
                    ModeNum, &Size, &Info
                );
                if (!EFI_ERROR(Status) &&
                    (Info != NULL && Size >= sizeof (*Info)) &&
                    (Info->HorizontalResolution == *ScreenWidth) &&
                    (Info->VerticalResolution   == *ScreenHeight) &&
                    (
                        ModeNum == CurrentModeNum ||
                        REFIT_CALL_2_WRAPPER(
                            GOPDraw->SetMode, GOPDraw, ModeNum
                        ) == EFI_SUCCESS
                    )
                ) {
                    ModeSet = TRUE;

                    #if REFIT_DEBUG > 0
                    MsgStr = PoolPrint (
                        L"Mode Pointed at GOP Mode[%02d]",
                        ModeNum
                    );
                    #endif
                }
                else {
                    #if REFIT_DEBUG > 0
                    MsgStr = PoolPrint (
                        L"%s GOP Mode[%02d]",
                        (ModeNum == CurrentModeNum)
                            ? L"Mode Already at"
                            : L"Could *NOT* Set",
                        ModeNum
                    );
                    #endif
                }

                #if REFIT_DEBUG > 0
                LOG_MSG("%s    * %s", OffsetNext, MsgStr);
                MY_FREE_POOL(MsgStr);
                #endif

                MY_FREE_POOL(Info);

                ModeNum++;
            } // while
        } // if/else *ScreenHeight == 0

        if (ModeSet) {
            egScreenWidth  = *ScreenWidth;
            egScreenHeight = *ScreenHeight;
        }
        else {
            #if REFIT_DEBUG > 0
            LOG_MSG("\n\n");
            LOG_MSG("INFO: GOP Mode Matching User Defined Resolution *NOT* Found");
            #endif

            if (GlobalConfig.HelpSize) {
                Status = ForceVideoMode (*ScreenWidth, *ScreenHeight);
                if (!EFI_ERROR(Status)) {
                    ModeSet = TRUE;
                }

                #if REFIT_DEBUG > 0
                LOG_MSG("%s      Force Mode[%02d] to Match %d x %d Resolution:- '%r'",
                    OffsetNext, CurrentModeNum,
                    *ScreenWidth, *ScreenHeight,
                    Status
                );

                MsgStr = PoolPrint (L"Analyse Amended Mode[%02d]", CurrentModeNum);
                LOG_MSG("\n\n");
                LOG_MSG("%s:", MsgStr);
                MY_FREE_POOL(MsgStr);

                MsgStr = PoolPrint (
                    L"01 GOP Mode ... 0x%lx <<< --- >>> 0x%lx Framebuffer",
                    GOPDraw->Mode->FrameBufferBase,
                    GOPDraw->Mode->FrameBufferBase + GOPDraw->Mode->FrameBufferSize
                );
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("%s  - Summary:- '%s'", OffsetNext, MsgStr);
                MY_FREE_POOL(MsgStr);

                switch (GOPDraw->Mode->Info->PixelFormat) {
                    case PixelRedGreenBlueReserved8BitPerColor: PixelFormatDesc = L"8bit RGB";  break;
                    case PixelBlueGreenRedReserved8BitPerColor: PixelFormatDesc = L"8bit BGR";  break;
                    case PixelBitMask:                          PixelFormatDesc = L"BIT Mask";  break;
                    case PixelBltOnly:                          PixelFormatDesc = L"BLT Only";  break;
                    default:                                    PixelFormatDesc = L"Unknown!";  break;
                } // switch

                LOG_MSG("%s    * Mode[%02d]", OffsetNext, CurrentModeNum);
                LOG_MSG(
                    " @%6d x %-6d( Pixels Per Scanned Line : %-6d| Pixel Format : %s )",
                    GOPDraw->Mode->Info->HorizontalResolution,
                    GOPDraw->Mode->Info->VerticalResolution,
                    GOPDraw->Mode->Info->PixelsPerScanLine,
                    PixelFormatDesc
                );
                #endif
            }

            if (ModeSet) {
                egScreenWidth  = *ScreenWidth;
                egScreenHeight = *ScreenHeight;
            }
            else {
                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (
                    L"Set Default Mode"
                );
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("%s:", MsgStr);
                MY_FREE_POOL(MsgStr);
                #endif

                ModeNum = 0;
                while (ModeNum < GOPDraw->Mode->MaxMode) {
                    Status = REFIT_CALL_4_WRAPPER(
                        GOPDraw->QueryMode, GOPDraw,
                        ModeNum, &Size, &Info
                    );
                    if (EFI_ERROR(Status) || Info == NULL) {
                        #if REFIT_DEBUG > 0
                        MsgStr = StrDuplicate (L"Error : Could *NOT* Query GOP Mode");
                        #endif
                    }
                    else {
                        #if REFIT_DEBUG > 0
                        MsgStr = PoolPrint (
                            L"Available Mode: Mode[%02d][%d x %d]",
                            ModeNum,
                            Info->HorizontalResolution,
                            Info->VerticalResolution
                        );
                        #endif

                        if (ModeNum == CurrentModeNum) {
                            egScreenWidth  = Info->HorizontalResolution;
                            egScreenHeight = Info->VerticalResolution;
                        }
                    }

                    #if REFIT_DEBUG > 0
                    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                    LOG_MSG("\n");
                    LOG_MSG("  - %s", MsgStr);
                    MY_FREE_POOL(MsgStr);
                    #endif

                    ModeNum++;
                } // while
            } // if/else ModeSet ... Inner
        } // if/else ModeSet ... Outer
    } // if/else if UGADraw

    #if REFIT_DEBUG > 0
    LOG_MSG("\n\n");
    #endif

    return ModeSet;
} // BOOLEAN egSetScreenSize()

// Set a text mode.
// Returns TRUE if the mode actually changed, FALSE otherwise.
// Note that FALSE can either mean an error or no change made.
BOOLEAN egSetTextMode (
    UINT32 RequestedMode
) {
    EFI_STATUS   Status;

    #if REFIT_DEBUG > 0
    UINTN        i;
    UINTN        Width;
    UINTN        Height;
    CHAR16      *MsgStr;
    BOOLEAN      GotOne;
    #endif


    if (RequestedMode == DONT_CHANGE_TEXT_MODE ||
        RequestedMode == gST->ConOut->Mode->Mode
    ) {
        // Early Return
        return FALSE;
    }

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (L"Set Text Mode to %d", RequestedMode);
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("%s", MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    Status = REFIT_CALL_2_WRAPPER(gST->ConOut->SetMode, gST->ConOut, RequestedMode);
    if (!EFI_ERROR(Status)) {
        return TRUE;
    }

    #if REFIT_DEBUG > 0
    MsgStr = StrDuplicate (L"Unsupported Text Mode!!'");
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("\n\n");
    LOG_MSG("INFO: %s", MsgStr);
    MY_FREE_POOL(MsgStr);

    MsgStr = StrDuplicate (L"Seek Available Modes");
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("\n\n");
    LOG_MSG("%s:", MsgStr);
    MY_FREE_POOL(MsgStr);

    i = 0;
    GotOne = FALSE;
    do {
        Status = REFIT_CALL_4_WRAPPER(
            gST->ConOut->QueryMode, gST->ConOut,
            i, &Width, &Height
        );
        if (!EFI_ERROR(Status)) {
            GotOne = TRUE;
            MsgStr = PoolPrint (
                L"Mode %4d (%5d x %-5d)",
                i, Width, Height
            );
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("%s  - %s", OffsetNext, MsgStr);
            MY_FREE_POOL(MsgStr);
        }
    } while (++i < gST->ConOut->Mode->MaxMode);

    if (!GotOne) {
        MsgStr = StrDuplicate (L"No Valid Mode Found!!'");
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("\n\n");
        LOG_MSG("INFO: %s", MsgStr);
        MY_FREE_POOL(MsgStr);
    }

    MsgStr = PoolPrint (L"Use Default Mode:- '%d'", DONT_CHANGE_TEXT_MODE);
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("\n\n");
    LOG_MSG("%s", MsgStr);
    LOG_MSG("\n\n");
    MY_FREE_POOL(MsgStr);
    #endif

    return FALSE;
} // BOOLEAN egSetTextMode()

CHAR16 * egScreenDescription (VOID) {
    CHAR16  *GraphicsInfo  = NULL;
    CHAR16  *TextInfo      = NULL;

    if (!egHasGraphics) {
        GraphicsInfo = PoolPrint (
            L"Text-Only Console: %d x %d",
            ConWidth, ConHeight
        );
    }
    else {
        if (GOPDraw != NULL) {
            GraphicsInfo = PoolPrint (
                L"Graphics Output Protocol @ %d x %d",
                egScreenWidth, egScreenHeight
            );
        }
        else if (UGADraw != NULL) {
            GraphicsInfo = PoolPrint (
                L"Universal Graphics Adapter @ %d x %d",
                egScreenWidth, egScreenHeight
            );
        }
        else {
            GraphicsInfo = StrDuplicate (L"Could *NOT* Get Graphics Details");
        }

        if (!AllowGraphicsMode) {
            // Graphics Capable Hardware in Text Mode
            TextInfo = PoolPrint (
                L"(Text Mode: %d x %d [Graphics Capable])",
                ConWidth, ConHeight
            );
            MergeStrings (&GraphicsInfo, TextInfo, L' ');
        }
    }

    MY_FREE_POOL(TextInfo);

    return GraphicsInfo;
} // CHAR16 * egScreenDescription()

BOOLEAN egHasGraphicsMode (VOID) {
    return egHasGraphics;
} // BOOLEAN egHasGraphicsMode()

BOOLEAN egIsGraphicsModeEnabled (VOID) {
    EFI_CONSOLE_CONTROL_SCREEN_MODE CurrentMode;

    if (ConsoleControl != NULL) {
        REFIT_CALL_4_WRAPPER(
            ConsoleControl->GetMode, ConsoleControl,
            &CurrentMode, NULL, NULL
        );

        return (CurrentMode == EfiConsoleControlScreenGraphics) ? TRUE : FALSE;
    }

    return FALSE;
} // BOOLEAN egIsGraphicsModeEnabled()

VOID egSetGraphicsModeEnabled (
    IN BOOLEAN Enable
) {
    EFI_CONSOLE_CONTROL_SCREEN_MODE CurrentMode;
    EFI_CONSOLE_CONTROL_SCREEN_MODE NewMode;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    if (ConsoleControl == NULL) {
        BREAD_CRUMB(L"%a:  1a 1 - END:- VOID (Aborted ... ConsoleControl == NULL)", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    BREAD_CRUMB(L"%a:  2", __func__);
    REFIT_CALL_4_WRAPPER(
        ConsoleControl->GetMode, ConsoleControl,
        &CurrentMode, NULL, NULL
    );

    BREAD_CRUMB(L"%a:  3", __func__);
    if (Enable) {
        BREAD_CRUMB(L"%a:  3a 1 - (Tag for Graphics Mode)", __func__);
        NewMode = EfiConsoleControlScreenGraphics;
    }
    else {
        BREAD_CRUMB(L"%a:  3b 1 - (Tag for Text Mode)", __func__);
        NewMode = EfiConsoleControlScreenText;
    }

    BREAD_CRUMB(L"%a:  4", __func__);
    if (CurrentMode != NewMode) {
        BREAD_CRUMB(L"%a:  4a 1 - (Set to Tagged Mode)", __func__);
        REFIT_CALL_2_WRAPPER(ConsoleControl->SetMode, ConsoleControl, NewMode);
    }
    else {
        BREAD_CRUMB(L"%a:  4b 1 - (Tagged Mode is Already Active)", __func__);
    }

    BREAD_CRUMB(L"%a:  5 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID egSetGraphicsModeEnabl()

//
// Drawing to the screen
//

VOID egClearScreen (
    IN EG_PIXEL *Color
) {
    EFI_UGA_PIXEL FillColor;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    if (!egHasGraphics) {
        BREAD_CRUMB(L"%a:  1a 1 - END:- VOID (Clearing in Text Mode)", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Try to clear in text mode
        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
        REFIT_CALL_1_WRAPPER(gST->ConOut->ClearScreen,  gST->ConOut);

        // Early Return
        return;
    }

    BREAD_CRUMB(L"%a:  2 - (Set Fill Colour)", __func__);
    if (Color != NULL) {
        BREAD_CRUMB(L"%a:  2a 1 - (Use Input Colour)", __func__);
        FillColor.Red   = Color->r;
        FillColor.Green = Color->g;
        FillColor.Blue  = Color->b;
    }
    else {
        BREAD_CRUMB(L"%a:  2b 1 - (Use Default Black)", __func__);
        FillColor.Red   = 0x0;
        FillColor.Green = 0x0;
        FillColor.Blue  = 0x0;
    }
    FillColor.Reserved = 0;

    BREAD_CRUMB(L"%a:  3", __func__);
    if (GOPDraw != NULL) {
        BREAD_CRUMB(L"%a:  3a 1 - (Apply Fill via GOP)", __func__);
        // EFI_GRAPHICS_OUTPUT_BLT_PIXEL and EFI_UGA_PIXEL have the same
        // layout and the TianoCore header file actually defines them
        // as being the same type.
        REFIT_CALL_10_WRAPPER(
            GOPDraw->Blt, GOPDraw,
            (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) &FillColor, EfiBltVideoFill,
             0, 0,
             0, 0,
             egScreenWidth, egScreenHeight, 0
         );
    }
    else if (UGADraw != NULL) {
        BREAD_CRUMB(L"%a:  3b 1 - (Apply Fill via UGA)", __func__);
        REFIT_CALL_10_WRAPPER(
            UGADraw->Blt, UGADraw,
            &FillColor, EfiUgaVideoFill,
            0, 0,
            0, 0,
            egScreenWidth, egScreenHeight, 0
        );
    }

    BREAD_CRUMB(L"%a:  4 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID egClearScreen()

VOID egDrawImage (
    IN EG_IMAGE *Image,
    IN UINTN     ScreenPosX,
    IN UINTN     ScreenPosY
) {
    BOOLEAN   SetImage;
    EG_IMAGE *CompImage;

    // DA-TAg: Investigate This
    //         Weird seemingly redundant tests because some placement code can "wrap around" and
    //         send "negative" values, which of course become very large unsigned ints that can then
    //         wrap around AGAIN if values are added to them.
    if (!egHasGraphics                                ||
        ScreenPosX > egScreenWidth                    ||
        ScreenPosY > egScreenHeight                   ||
        (ScreenPosX + Image->Width)  > egScreenWidth  ||
        (ScreenPosY + Image->Height) > egScreenHeight
    ) {
        return;
    }

    SetImage = FALSE;
    if (GlobalConfig.ScreenBackground == NULL ||
        (
            Image->Width  == egScreenWidth &&
            Image->Height == egScreenHeight
        )
    ) {
        CompImage = Image;
    }
    else if (GlobalConfig.ScreenBackground == Image) {
        CompImage = GlobalConfig.ScreenBackground;
    }
    else {
        CompImage = egCropImage (
            GlobalConfig.ScreenBackground,
            ScreenPosX, ScreenPosY,
            Image->Width, Image->Height
        );

        if (CompImage == NULL) {
            #if REFIT_DEBUG > 0
            LOG_MSG("Error! Cannot Crop Image in egDrawImage()!\n");
            #endif

            return;
        }

        egComposeImage (CompImage, Image, 0, 0);
        SetImage = TRUE;
    }

    if (GOPDraw != NULL) {
        REFIT_CALL_10_WRAPPER(
            GOPDraw->Blt, GOPDraw,
            (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) CompImage->PixelData, EfiBltBufferToVideo,
            0, 0,
            ScreenPosX, ScreenPosY,
            CompImage->Width, CompImage->Height, 0
        );
    }
    else if (UGADraw != NULL) {
        REFIT_CALL_10_WRAPPER(
            UGADraw->Blt, UGADraw,
            (EFI_UGA_PIXEL *) CompImage->PixelData, EfiUgaBltBufferToVideo,
            0, 0,
            ScreenPosX, ScreenPosY,
            CompImage->Width, CompImage->Height, 0
        );
    }

    if (SetImage) {
        MY_FREE_IMAGE(CompImage);
    }
} // VOID egDrawImage()

// Display an unselected icon on the screen, so that the background image shows
// through the transparency areas. The BadgeImage may be NULL, in which case
// it is not composited in.
VOID egDrawImageWithTransparency (
    EG_IMAGE *Image,
    EG_IMAGE *BadgeImage,
    UINTN     XPos,
    UINTN     YPos,
    UINTN     Width,
    UINTN     Height
) {
    EG_IMAGE *Background;

    Background = egCropImage (
        GlobalConfig.ScreenBackground,
        XPos, YPos,
        Width, Height
    );

    if (Background != NULL) {
        BltImageCompositeBadge (
            Background,
            Image, BadgeImage,
            XPos, YPos
        );
        MY_FREE_IMAGE(Background);
    }
} // VOID DrawImageWithTransparency()

VOID egDrawImageArea (
    IN EG_IMAGE *Image,
    IN UINTN     AreaPosX,
    IN UINTN     AreaPosY,
    IN UINTN     AreaWidth,
    IN UINTN     AreaHeight,
    IN UINTN     ScreenPosX,
    IN UINTN     ScreenPosY
) {
    if (!egHasGraphics) {
        return;
    }

    egRestrictImageArea (
        Image,
        AreaPosX, AreaPosY,
        &AreaWidth, &AreaHeight
    );

    if (AreaWidth  == 0 ||
        AreaHeight == 0
    ) {
        return;
    }

    if (GOPDraw != NULL) {
        REFIT_CALL_10_WRAPPER(
            GOPDraw->Blt, GOPDraw,
            (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) Image->PixelData, EfiBltBufferToVideo,
            AreaPosX, AreaPosY,
            ScreenPosX, ScreenPosY,
            AreaWidth, AreaHeight, Image->Width * 4
        );
    }
    else if (UGADraw != NULL) {
        REFIT_CALL_10_WRAPPER(
            UGADraw->Blt, UGADraw,
            (EFI_UGA_PIXEL *) Image->PixelData, EfiUgaBltBufferToVideo,
            AreaPosX, AreaPosY,
            ScreenPosX, ScreenPosY,
            AreaWidth, AreaHeight, Image->Width * 4
        );
    }
} // VOID egDrawImageArea()

static
VOID egDisplayMessageEx (
    CHAR16    *Text,
    EG_PIXEL  *MessageBG,
    UINTN      PositionCode,
    BOOLEAN    ResetPosition
) {
    UINTN LumIndex;
    UINTN BoxWidth;
    UINTN BoxHeight;
    static UINTN Position = 1;
    EG_IMAGE *Box;

    if (Text == NULL || MessageBG == NULL) {
        // Early Return
        return;
    }

    egMeasureText (Text, &BoxWidth, &BoxHeight);
    BoxWidth  += 14;
    BoxHeight *=  2;
    if (BoxWidth > egScreenWidth) {
        BoxWidth = egScreenWidth;
    }
    Box = egCreateFilledImage (BoxWidth, BoxHeight, FALSE, MessageBG);

    if (!ResetPosition) {
        // Get Luminance Index
        LumIndex = GetLumIndex (
            (UINTN) MessageBG->r,
            (UINTN) MessageBG->g,
            (UINTN) MessageBG->b
        );

        egRenderText (
            Text, Box, 7,
            BoxHeight / 4,
            (UINT8) LumIndex
        );
    }

    switch (PositionCode) {
        case TOP:     Position  = 1;                                  break;
        case CENTER:  Position  = ((egScreenHeight - BoxHeight) / 2); break;
        case BOTTOM:  Position  = (egScreenHeight - (BoxHeight * 2)); break;
        default:      Position += (BoxHeight + (BoxHeight / 10));     break; // NEXTLINE
    } // switch

    if (ResetPosition) {
        if (PositionCode != TOP    &&
            PositionCode != CENTER &&
            PositionCode != BOTTOM
        ) {
            Position -= (BoxHeight + (BoxHeight / 10));
        }
    }

    egDrawImage (Box, (egScreenWidth - BoxWidth) / 2, Position);

    if ((PositionCode == CENTER) || (Position >= egScreenHeight - (BoxHeight * 5))) {
        Position = 1;
    }
} // VOID egDisplayMessageEx()

// Display a message in the center of the screen, surrounded by a box of the
// specified color. For the moment, uses graphics calls only. (It still works
// in text mode on GOP/UEFI systems, but not on UGA/EFI 1.x systems.)
VOID egDisplayMessage (
    CHAR16    *Text,
    EG_PIXEL  *MessageBG,
    UINTN      PositionCode,
    UINTN      PauseLength,
    CHAR16    *PauseType     OPTIONAL
) {
    if (Text == NULL || MessageBG == NULL) {
        // Early Return
        return;
    }

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    MY_MUTELOGGER_SET;
    #endif
    // Display the message
    egDisplayMessageEx (Text, MessageBG, PositionCode, FALSE);

    if (PauseType && PauseLength > 0) {
        if (MyStriCmp (PauseType, L"HaltSeconds")) {
            HaltSeconds (PauseLength);
        }
        else {
            PauseSeconds (PauseLength);
        }

        // Erase the message
        egDisplayMessageEx (Text, &MenuBackgroundPixel, PositionCode, TRUE);
    }
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif
} // VOID egDisplayMessage()

// Copy the current contents of the display into an EG_IMAGE.
// Returns pointer if successful, NULL if not.
EG_IMAGE * egCopyScreen (VOID) {
   return egCopyScreenArea (0, 0, egScreenWidth, egScreenHeight);
} // EG_IMAGE * egCopyScreen()

// Copy the current contents of the specified display area into an EG_IMAGE.
// Returns pointer if successful, NULL if not.
EG_IMAGE * egCopyScreenArea (
    UINTN XPos,  UINTN YPos,
    UINTN Width, UINTN Height
) {
    EG_IMAGE *Image;

   if (!egHasGraphics) {
       return NULL;
   }

   // Allocate a buffer for the screen area
   Image = egCreateImage (Width, Height, FALSE);
   if (Image == NULL) {
      return NULL;
   }

   // Get full screen image
   if (GOPDraw != NULL) {
       REFIT_CALL_10_WRAPPER(
           GOPDraw->Blt, GOPDraw,
           (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) Image->PixelData, EfiBltVideoToBltBuffer,
           XPos, YPos,
           0, 0,
           Image->Width, Image->Height, 0
       );
   }
   else if (UGADraw != NULL) {
       REFIT_CALL_10_WRAPPER(
           UGADraw->Blt, UGADraw,
           (EFI_UGA_PIXEL *) Image->PixelData, EfiUgaVideoToBltBuffer,
           XPos, YPos,
           0, 0,
           Image->Width, Image->Height, 0
       );
   }
   else {
       MY_FREE_IMAGE(Image);
   }

   return Image;
} // EG_IMAGE * egCopyScreenArea()


// Make a screenshot
VOID egScreenShot (VOID) {
    EFI_STATUS            Status;
    EG_IMAGE             *Image;
    UINT8                *FileData;
    UINT8                 Temp;
    UINTN                 i;
    UINTN                 FileDataSize;         ///< Size in bytes
    UINTN                 FilePixelSize;        ///< Size in pixels
    CHAR16               *FileName;
    CHAR16               *MsgStr;
    EFI_FILE_PROTOCOL    *BaseDir;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;

    MY_MUTELOGGER_SET;
    #endif
    // Clear the Keystroke Buffer (Silently)
    ReadAllKeyStrokes();
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;

    LOG_MSG("Received User Input:");
    LOG_MSG("%s  - Take Screenshot", OffsetNext);
    #endif

    Image = egCopyScreen();
    if (!Image) {
        MsgStr = L"Unable to Take Screenshot ... Image is NULL";

        egDisplayMessage (
            MsgStr, &BGColorFail,
            CENTER, 4, L"HaltSeconds"
        );

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("%s    ** WARN: %s", OffsetNext, MsgStr);
        LOG_MSG("\n\n");
        #endif

        return;
    }

    // Fix pixels
    FilePixelSize = Image->Width * Image->Height;
    for (i = 0; i < FilePixelSize; ++i) {
        Temp                   = Image->PixelData[i].b;
        Image->PixelData[i].b  = Image->PixelData[i].r;
        Image->PixelData[i].r  = Temp;
        Image->PixelData[i].a  = 0xFF;
    }

    // Encode as PNG
    Status = EncodeAsPNG (
        (VOID *)   Image->PixelData,
        (UINT32)   Image->Width,
        (UINT32)   Image->Height,
        (VOID **) &FileData,
        &FileDataSize
    );
    MY_FREE_IMAGE(Image);

    if (!FileData) {
        MsgStr = L"No FileData!";

        egDisplayMessage (
            MsgStr, &BGColorFail,
            CENTER, 4, L"HaltSeconds"
        );

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("%s    ** WARN: %s", OffsetNext, MsgStr);
        LOG_MSG("\n\n");
        #endif

        return;
    }

    if (EFI_ERROR(Status)) {
        MsgStr = L"Could *NOT* Encode PNG";
        egDisplayMessage (
            MsgStr, &BGColorFail,
            CENTER, 4, L"HaltSeconds"
        );

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("%s    ** WARN: %s", OffsetNext, MsgStr);
        LOG_MSG("\n\n");
        #endif

        MY_FREE_POOL(FileData);

        return;
    }

    Status = EFI_NOT_STARTED;
    if (SelfVolume->FSType == FS_TYPE_FAT32 ||
        SelfVolume->FSType == FS_TYPE_FAT16 ||
        SelfVolume->FSType == FS_TYPE_FAT12
    ) {
        SelfRootDir = LibOpenRoot (SelfLoadedImage->DeviceHandle);
        if (SelfRootDir) {
            BaseDir = SelfRootDir;
            Status  = EFI_SUCCESS;
        }
    }

    if (EFI_ERROR(Status)) {
        // Try to set to use first available ESP
        Status = egFindESP (&BaseDir);
    }

    if (EFI_ERROR(Status)) {
        MsgStr = L"Could *NOT* Find an ESP to Save Screenshot";
        egDisplayMessage (
            MsgStr, &BGColorFail,
            CENTER, 4, L"HaltSeconds"
        );

        #if REFIT_DEBUG > 0
        LOG_MSG("%s    ** WARN: %s", OffsetNext, MsgStr);
        LOG_MSG("\n\n");
        #endif

        MY_FREE_POOL(FileData);

        return;
    }

    // Search for existing screen shot files ... Increment index to an unused value
    i = 0;
    FileName = NULL;
    do {
        MY_FREE_POOL(FileName);
        if (i > 999) {
            Status = EFI_INVALID_PARAMETER;
            break;
        }

        FileName = PoolPrint (L"ScreenShot_%03d.png", i++);
    } while (FileExists (BaseDir, FileName));

    if (EFI_ERROR(Status)) {
        MsgStr = L"Aborting Screenshot ... Excessive Number of Saved Files Found";
        egDisplayMessage (
            MsgStr, &BGColorFail,
            CENTER, 4, L"HaltSeconds"
        );

        #if REFIT_DEBUG > 0
        LOG_MSG("%s    ** %s", OffsetNext, MsgStr);
        #endif
    }
    else {
        // Save to file on the ESP
        Status = egSaveFile (BaseDir, FileName, (UINT8 *) FileData, FileDataSize);
        if (EFI_ERROR(Status)) {
            CheckError (Status, L"on 'egSaveFile' call in 'egScreenShot'");
            MsgStr = L"Error on 'egSaveFile' call in 'egScreenShot'";
            egDisplayMessage (
                MsgStr, &BGColorFail,
                CENTER, 4, L"HaltSeconds"
            );

            #if REFIT_DEBUG > 0
            LOG_MSG("%s    ** %s", OffsetNext, MsgStr);
            #endif
        }
        else {
            MsgStr = L"Saved Screenshot";
            egDisplayMessage (
                MsgStr, &BGColorBase,
                CENTER, 2, L"HaltSeconds"
            );

            #if REFIT_DEBUG > 0
            LOG_MSG("%s    * %s:- '%s'", OffsetNext, MsgStr, FileName);
            #endif
        }
    }

    #if REFIT_DEBUG > 0
    LOG_MSG("\n\n");
    #endif

    MY_FREE_POOL(FileName);
    MY_FREE_POOL(FileData);
} // VOID egScreenShot()

/* EOF */
