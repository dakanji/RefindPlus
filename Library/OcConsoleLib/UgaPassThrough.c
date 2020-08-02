/** @file
Copyright (C) 2020, vit9696. All rights reserved.

All rights reserved.

This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.    The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "OcConsoleLibInternal.h"

#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

STATIC
EFI_STATUS
EFIAPI
OcUgaDrawGetMode (
    IN    EFI_UGA_DRAW_PROTOCOL *This,
    OUT UINT32                  *HorizontalResolution,
    OUT UINT32                  *VerticalResolution,
    OUT UINT32                  *ColorDepth,
    OUT UINT32                  *RefreshRate
    )
{
    OC_UGA_PROTOCOL                      *OcUgaDraw;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;

    #if REFIT_DEBUG > 0
    MsgLog ("OcUgaDrawGetMode %p\n", This);
    #endif

    if (This == NULL
        || HorizontalResolution == NULL
        || VerticalResolution == NULL
        || ColorDepth == NULL
        || RefreshRate == NULL
    ) {
        return EFI_INVALID_PARAMETER;
    }

    OcUgaDraw = BASE_CR (This, OC_UGA_PROTOCOL, Uga);
    Info      = OcUgaDraw->GraphicsOutput->Mode->Info;

    #if REFIT_DEBUG > 0
    MsgLog (
        "OcUgaDrawGetMode Info is %ux%u (%u)\n",
        Info->HorizontalResolution,
        Info->VerticalResolution,
        Info->PixelFormat
    );
    #endif

    if (Info->HorizontalResolution == 0 || Info->VerticalResolution == 0) {
        return EFI_NOT_STARTED;
    }

    *HorizontalResolution = Info->HorizontalResolution;
    *VerticalResolution   = Info->VerticalResolution;
    *ColorDepth           = DEFAULT_COLOUR_DEPTH;
    *RefreshRate          = DEFAULT_REFRESH_RATE;

    return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
OcUgaDrawSetMode (
    IN    EFI_UGA_DRAW_PROTOCOL *This,
    IN    UINT32                HorizontalResolution,
    IN    UINT32                VerticalResolution,
    IN    UINT32                ColorDepth,
    IN    UINT32                RefreshRate
    )
{
    EFI_STATUS      Status;
    OC_UGA_PROTOCOL *OcUgaDraw;

    OcUgaDraw = BASE_CR (This, OC_UGA_PROTOCOL, Uga);

    #if REFIT_DEBUG > 0
    MsgLog (
        "OcUgaDrawSetMode %p %ux%u@%u/%u\n",
        This,
        HorizontalResolution,
        VerticalResolution,
        ColorDepth,
        RefreshRate
    );
    #endif

    OcUgaDraw = BASE_CR (This, OC_UGA_PROTOCOL, Uga);

    Status = OcSetConsoleResolutionForProtocol (
        OcUgaDraw->GraphicsOutput,
        HorizontalResolution,
        VerticalResolution,
        ColorDepth
    );

    #if REFIT_DEBUG > 0
    MsgLog ("UGA SetConsoleResolutionOnHandle attempt - %r\n", Status);
    #endif

    if (EFI_ERROR (Status)) {
        Status = OcSetConsoleResolutionForProtocol (
            OcUgaDraw->GraphicsOutput,
            0,
            0,
            0
        );

        #if REFIT_DEBUG > 0
        MsgLog ("UGA SetConsoleResolutionOnHandle max attempt - %r\n", Status);
        #endif
    }

    return Status;
}

STATIC
EFI_STATUS
EFIAPI
OcUgaDrawBlt (
    IN    EFI_UGA_DRAW_PROTOCOL *This,
    IN    EFI_UGA_PIXEL         *BltBuffer OPTIONAL,
    IN    EFI_UGA_BLT_OPERATION BltOperation,
    IN    UINTN                 SourceX,
    IN    UINTN                 SourceY,
    IN    UINTN                 DestinationX,
    IN    UINTN                 DestinationY,
    IN    UINTN                 Width,
    IN    UINTN                 Height,
    IN    UINTN                 Delta            OPTIONAL
    )
{
    OC_UGA_PROTOCOL     *OcUgaDraw;

    OcUgaDraw = BASE_CR (This, OC_UGA_PROTOCOL, Uga);

    return OcUgaDraw->GraphicsOutput->Blt (
        OcUgaDraw->GraphicsOutput,
        (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) BltBuffer,
        (EFI_GRAPHICS_OUTPUT_BLT_OPERATION) BltOperation,
        SourceX,
        SourceY,
        DestinationX,
        DestinationY,
        Width,
        Height,
        Delta
    );
}

EFI_STATUS
OcProvideUgaPassThrough (
    VOID
    )
{
    EFI_STATUS                   Status;
    UINTN                        HandleCount;
    EFI_HANDLE                   *HandleBuffer;
    UINTN                        Index;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput;
    EFI_UGA_DRAW_PROTOCOL        *UgaDraw;
    OC_UGA_PROTOCOL              *OcUgaDraw;

    //
    // For now we do not need this but for launching 10.4, but as a side note:
    // MacPro5,1 has 2 GOP protocols:
    // - for GPU
    // - for ConsoleOutput
    // and 1 UGA protocol:
    // - for unknown handle
    //
    Status = gBS->LocateHandleBuffer (
        ByProtocol,
        &gEfiUgaDrawProtocolGuid,
        NULL,
        &HandleCount,
        &HandleBuffer
        );

    if (!EFI_ERROR (Status)) {
        #if REFIT_DEBUG > 0
        MsgLog ("Found %u handles with UGA draw\n", (UINT32) HandleCount);
        #endif

        FreePool (HandleBuffer);
    } else {
        #if REFIT_DEBUG > 0
        MsgLog ("Found NO handles with UGA draw\n");
        #endif
    }

    Status = gBS->LocateHandleBuffer (
        ByProtocol,
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        &HandleCount,
        &HandleBuffer
    );

    if (!EFI_ERROR (Status)) {
        #if REFIT_DEBUG > 0
        MsgLog ("Found %u handles with GOP for UGA check\n", (UINT32) HandleCount);
        #endif

        for (Index = 0; Index < HandleCount; ++Index) {
            #if REFIT_DEBUG > 0
            MsgLog ("Trying handle %u - %p\n", (UINT32) Index, HandleBuffer[Index]);
            #endif

            Status = gBS->HandleProtocol (
                HandleBuffer[Index],
                &gEfiGraphicsOutputProtocolGuid,
                (VOID **) &GraphicsOutput
            );

            if (EFI_ERROR (Status)) {
                #if REFIT_DEBUG > 0
                MsgLog ("No GOP protocol - %r\n", Status);
                #endif

                continue;
            }

            Status = gBS->HandleProtocol (
                HandleBuffer[Index],
                &gEfiUgaDrawProtocolGuid,
                (VOID **) &UgaDraw
            );

            if (EFI_ERROR (Status)) {
                #if REFIT_DEBUG > 0
                MsgLog ("No UGA protocol - %r\n", Status);
                #endif

                OcUgaDraw = AllocateZeroPool (sizeof (*OcUgaDraw));
                if (OcUgaDraw == NULL) {
                    #if REFIT_DEBUG > 0
                    MsgLog ("Failed to allocate UGA protocol\n");
                    #endif

                    continue;
                }

                OcUgaDraw->GraphicsOutput = GraphicsOutput;
                OcUgaDraw->Uga.GetMode = OcUgaDrawGetMode;
                OcUgaDraw->Uga.SetMode = OcUgaDrawSetMode;
                OcUgaDraw->Uga.Blt = OcUgaDrawBlt;

                Status = gBS->InstallMultipleProtocolInterfaces (
                    &HandleBuffer[Index],
                    &gEfiUgaDrawProtocolGuid,
                    &OcUgaDraw->Uga,
                    NULL
                );

                #if REFIT_DEBUG > 0
                MsgLog ("Installed UGA protocol - %r\n", Status);
                #endif
            } else {
                #if REFIT_DEBUG > 0
                MsgLog ("Has UGA protocol, skip\n");
                #endif

                continue;
            }
        }

        FreePool (HandleBuffer);

        #if REFIT_DEBUG > 0
    } else {
        MsgLog ("Failed to find handles with GOP\n");
        #endif
    }

    return Status;
}
