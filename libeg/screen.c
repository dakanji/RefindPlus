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
 * Modifications copyright (c) 2012-2021 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 *
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (dakanji@users.sourceforge.net)
 *
 * Modifications distributed under the preceding terms.
 */

// July 2020: Extensively modiied by dakanji (dakanji@users.sourceforge.net)
#include "libegint.h"
#include "../MainLoader/screen.h"
#include "../MainLoader/global.h"
#include "../MainLoader/lib.h"
#include "../MainLoader/mystrings.h"
#include "../include/refit_call_wrapper.h"
#include "libeg.h"
#include "lodepng.h"
#include "../include/Handle.h"

#include <efiUgaDraw.h>
#include <efiConsoleControl.h>

#ifndef __MAKEWITH_GNUEFI
#define LibLocateProtocol EfiLibLocateProtocol
#define LibOpenRoot EfiLibOpenRoot
#endif

// Console defines and variables
static EFI_GUID ConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;
static EFI_GUID UgaDrawProtocolGuid        = EFI_UGA_DRAW_PROTOCOL_GUID;
static EFI_GUID GraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;
EFI_UGA_DRAW_PROTOCOL        *UGADraw        = NULL;
EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput = NULL;

static BOOLEAN egHasGraphics  = FALSE;
static UINTN   egScreenWidth  = 800;
static UINTN   egScreenHeight = 600;

STATIC
EFI_STATUS
EncodeAsPNG (
  IN  VOID    *RawData,
  IN  UINT32  Width,
  IN  UINT32  Height,
  OUT VOID    **Buffer,
  OUT UINTN   *BufferSize
  )
{
  unsigned ErrorCode;

  // Should return 0 on success
  ErrorCode = lodepng_encode32 (
      (unsigned char **) Buffer,
      BufferSize,
      RawData,
      Width,
      Height
  );

  if (ErrorCode != 0) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
daCheckAltGop (
    VOID
) {
    EFI_STATUS                    Status;
    UINTN                         i;
    UINTN                         HandleCount;
    EFI_HANDLE                    *HandleBuffer;
    EFI_GRAPHICS_OUTPUT_PROTOCOL  *OrigGop;
    EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop;

    OrigGop = NULL;
    Status  = refit_call3_wrapper(
        gBS->HandleProtocol,
        gST->ConsoleOutHandle,
        &GraphicsOutputProtocolGuid,
        (VOID **) &OrigGop
    );

    if (EFI_ERROR (Status)) {
        Status = refit_call3_wrapper(
            gBS->LocateProtocol,
            &GraphicsOutputProtocolGuid,
            NULL,
            (VOID **) &Gop
        );
    }
    else {
        if (OrigGop->Mode->MaxMode > 0) {
            #if REFIT_DEBUG > 0
            MsgLog ("INFO: Usable GOP Exists on ConsoleOut Handle\n\n");
            #endif

            GraphicsOutput = OrigGop;

            return EFI_ALREADY_STARTED;
        }

        Status = refit_call5_wrapper(
            gBS->LocateHandleBuffer,
            ByProtocol,
            &GraphicsOutputProtocolGuid,
            NULL,
            &HandleCount,
            &HandleBuffer
        );

        #if REFIT_DEBUG > 0
        MsgLog ("Seek Replacement GOP Candidates:");
        #endif

        if (EFI_ERROR (Status)) {
            #if REFIT_DEBUG > 0
            MsgLog ("\n");
            #endif

            return EFI_NOT_FOUND;
        }

        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;

        UINT32  Width  = 0;
        UINT32  Height = 0;
        UINT32  MaxMode;
        UINT32  Mode;
        UINTN   SizeOfInfo;
        BOOLEAN OurValidGOP;

        Status = EFI_NOT_FOUND;
        for (i = 0; i < HandleCount; i++) {
            OurValidGOP = FALSE;
            if (HandleBuffer[i] != gST->ConsoleOutHandle) {
                Status = refit_call3_wrapper(
                    gBS->HandleProtocol,
                    HandleBuffer[i],
                    &GraphicsOutputProtocolGuid,
                    (VOID **) &Gop
                );

                if (!EFI_ERROR (Status)) {
                    #if REFIT_DEBUG > 0
                    MsgLog ("\n");
                    MsgLog ("  - Found Replacement Candidate on GPU Handle[%02d]\n", i);
                    #endif

                    #if REFIT_DEBUG > 0
                    MsgLog ("    * Evaluate Candidate\n");
                    #endif

                    MaxMode  = Gop->Mode->MaxMode;
                    Width    = 0;
                    Height   = 0;

                    for (Mode = 0; Mode < MaxMode; Mode++) {
                        Status = Gop->QueryMode (Gop, Mode, &SizeOfInfo, &Info);
                        if (!EFI_ERROR (Status)) {
                            if (Width > Info->HorizontalResolution) {
                                continue;
                            }
                            if (Height > Info->VerticalResolution) {
                                continue;
                            }
                            Width  = Info->HorizontalResolution;
                            Height = Info->VerticalResolution;
                        }
                    } // for

                    if (Width == 0 || Height == 0) {
                        #if REFIT_DEBUG > 0
                        MsgLog (
                            "    ** Invalid Candidate : Width = %d, Height = %d",
                            Width, Height
                        );
                        #endif
                    }
                    else {
                        #if REFIT_DEBUG > 0
                        MsgLog (
                            "    ** Valid Candidate : Width = %d, Height = %d",
                            Width, Height
                        );
                        #endif

                        OurValidGOP = TRUE;

                        break;
                    } // if Width == 0 || Height == 0
                } // if !EFI_ERROR (Status)
            } // if HandleBuffer[i]
        } // for

        FreePool (HandleBuffer);

        #if REFIT_DEBUG > 0
        MsgLog ("\n\n");
        #endif

        if (!OurValidGOP || EFI_ERROR (Status)) {
            #if REFIT_DEBUG > 0
            MsgLog ("INFO: Could Not Find Usable Replacement GOP\n\n");
            #endif

            return EFI_UNSUPPORTED;
        }
    } // if !EFI_ERROR (Status)

    return EFI_SUCCESS;
}

EFI_STATUS
egDumpGOPVideoModes (
    VOID
) {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;

    EFI_STATUS Status;
    UINT32     MaxMode;
    UINT32     Mode;
    UINT32     ModeLog;
    UINT32     NumModes;
    UINT32     ModeCount;
    UINT32     LoopCount;
    UINTN      SizeOfInfo;
    CHAR16     *PixelFormatDesc;
    BOOLEAN    OurValidGOP    = FALSE;
    CHAR16     *ShowScreenStr = NULL;

    if (GraphicsOutput == NULL) {
        #if REFIT_DEBUG > 0
        MsgLog ("\n\n");
        MsgLog ("** WARN: Could not Find GOP Instance\n\n");
        #endif

        return EFI_UNSUPPORTED;
    }

    // get dump
    MaxMode = GraphicsOutput->Mode->MaxMode;
    if (MaxMode > 0) {
        Mode = GraphicsOutput->Mode->Mode;
        NumModes = (INT32)MaxMode + 1;
        if (MaxMode == 0) {
            ModeCount = NumModes;
        }
        else {
            ModeCount = MaxMode;
        }
        LoopCount = 0;

        #if REFIT_DEBUG > 0
        MsgLog (
            "Query GOP Modes (Modes=%d, FrameBuffer=0x%lx-0x%lx):\n",
            ModeCount,
            GraphicsOutput->Mode->FrameBufferBase,
            GraphicsOutput->Mode->FrameBufferBase + GraphicsOutput->Mode->FrameBufferSize
        );
        #endif

        for (Mode = 0; Mode < NumModes; Mode++) {
            LoopCount++;
            if (LoopCount == NumModes) {
                break;
            }

            Status = refit_call4_wrapper(
                GraphicsOutput->QueryMode,
                GraphicsOutput,
                Mode,
                &SizeOfInfo,
                &Info
            );

            #if REFIT_DEBUG > 0
            MsgLog ("  - Mode[%02d] ...%r", Mode, Status);
            #endif

            if (!EFI_ERROR (Status)) {
                OurValidGOP = TRUE;

                switch (Info->PixelFormat) {
                    case PixelRedGreenBlueReserved8BitPerColor:
                        PixelFormatDesc = L"8bit RGB";
                        break;

                    case PixelBlueGreenRedReserved8BitPerColor:
                        PixelFormatDesc = L"8bit BGR";
                        break;

                    case PixelBitMask:
                        PixelFormatDesc = L"BIT MASK";
                        break;

                    case PixelBltOnly:
                        PixelFormatDesc = L"!FBuffer";
                        break;

                    default:
                        PixelFormatDesc = L"Invalid!";
                        break;
                }

                #if REFIT_DEBUG > 0
                if (LoopCount < ModeCount) {
                    MsgLog (
                        " @ %5dx%-5d (%5d Pixels Per Scanned Line, %s Pixel Format )\n",
                        Info->HorizontalResolution,
                        Info->VerticalResolution,
                        Info->PixelsPerScanLine,
                        PixelFormatDesc
                    );
                }
                else {
                    MsgLog (
                        " @ %5dx%-5d (%5d Pixels Per Scanned Line, %s Pixel Format )\n\n",
                        Info->HorizontalResolution,
                        Info->VerticalResolution,
                        Info->PixelsPerScanLine,
                        PixelFormatDesc
                    );
                }
                #endif
            }
            else {
                // Limit logged value to 99
                if (Mode > 99) {
                    ModeLog = 99;
                }
                else {
                    ModeLog = Mode;
                }

                #if REFIT_DEBUG > 0
                MsgLog ("  - Mode[%d]: %r", ModeLog, Status);
                if (LoopCount < ModeCount) {
                    if (Mode > 99) {
                        MsgLog ( ". NB: Real Mode = %d\n", Mode);
                    }
                    else {
                        MsgLog ( "\n", Mode);
                    }
                }
                else {
                    if (Mode > 99) {
                        MsgLog ( ". NB: Real Mode = %d\n\n", Mode);
                    }
                    else {
                        MsgLog ( "\n\n", Mode);
                    }
                }
                #endif
            } // if Status == EFI_SUCCESS
        } // for (Mode = 0; Mode < NumModes; Mode++)
    } // if MaxMode > 0

    if (!OurValidGOP) {
        #if REFIT_DEBUG > 0
        MsgLog ("INFO: Could Not Find Usable GOP\n\n");
        #endif

        return EFI_UNSUPPORTED;
    }

    return EFI_SUCCESS;
}

//
// Sets mode via GOP protocol, and reconnects simple text out drivers
//
static
EFI_STATUS
GopSetModeAndReconnectTextOut (
    IN UINT32 ModeNumber
) {
    EFI_STATUS   Status;

    if (GraphicsOutput == NULL) {
        return EFI_UNSUPPORTED;
    }

    Status = refit_call2_wrapper(
        GraphicsOutput->SetMode,
        GraphicsOutput,
        ModeNumber
    );

    #if REFIT_DEBUG > 0
    MsgLog ("  - Switch to GOP Mode[%d] ...%r\n\n", ModeNumber, Status);
    #endif

    return Status;
}

EFI_STATUS
egSetGOPMode (
    INT32 Next
) {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;

    EFI_STATUS   Status = EFI_UNSUPPORTED;
    UINT32       MaxMode;
    UINTN        SizeOfInfo;
    INT32        Mode;
    UINT32       i = 0;

    #if REFIT_DEBUG > 0
    MsgLog ("Set GOP Mode:");
    #endif

    if (GraphicsOutput == NULL) {
        #if REFIT_DEBUG > 0
        MsgLog ("\n\n");
        MsgLog ("** WARN: Could not Set GOP Mode\n\n");
        #endif

        return EFI_UNSUPPORTED;
    }

    MaxMode = GraphicsOutput->Mode->MaxMode;
    Mode    = GraphicsOutput->Mode->Mode;


    if (MaxMode < 1) {
        Status = EFI_UNSUPPORTED;

        #if REFIT_DEBUG > 0
        MsgLog ("\n\n");
        MsgLog ("** WARN: Incompatible GPU\n\n");
        #endif
    }
    else {
        while (EFI_ERROR (Status) && i <= MaxMode) {
            Mode = Mode + Next;
            Mode = (Mode >= (INT32)MaxMode)?0:Mode;
            Mode = (Mode < 0)?((INT32)MaxMode - 1):Mode;

            Status = refit_call4_wrapper(
                GraphicsOutput->QueryMode,
                GraphicsOutput,
                (UINT32)Mode,
                &SizeOfInfo,
                &Info
            );

            #if REFIT_DEBUG > 0
            MsgLog ("\n");
            MsgLog ("  - Mode[%02d] ...%r\n", Mode, Status);
            #endif

            if (!EFI_ERROR (Status)) {
                Status = GopSetModeAndReconnectTextOut ((UINT32)Mode);

                if (!EFI_ERROR (Status)) {
                    egScreenWidth  = GraphicsOutput->Mode->Info->HorizontalResolution;
                    egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
                }
            }

            i++;
        }
    }

    return Status;
}

// On GOP systems, set the maximum available resolution.
// On UGA systems, just record the current resolution.
EFI_STATUS
egSetMaxResolution (
    VOID
) {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;

    EFI_STATUS   Status   = EFI_UNSUPPORTED;
    UINT32       Width    = 0;
    UINT32       Height   = 0;
    UINT32       BestMode = 0;
    UINT32       MaxMode;
    UINT32       Mode;
    UINTN        SizeOfInfo;

    if (GraphicsOutput == NULL) {
        // Cannot do this in text mode or with UGA.
        // So get and set basic data and ignore.
        UINT32 Depth, RefreshRate;

        Status = refit_call5_wrapper(
            UGADraw->GetMode, UGADraw,
            &Width, &Height,
            &Depth, &RefreshRate
        );

        GlobalConfig.RequestedScreenWidth  = Width;
        GlobalConfig.RequestedScreenHeight = Height;

        return EFI_UNSUPPORTED;
    }

    #if REFIT_DEBUG > 0
    MsgLog ("Set Screen Resolution:\n");
    #endif

    MaxMode = GraphicsOutput->Mode->MaxMode;
    for (Mode = 0; Mode < MaxMode; Mode++) {
        Status = refit_call4_wrapper(
            GraphicsOutput->QueryMode,
            GraphicsOutput,
            Mode,
            &SizeOfInfo,
            &Info
        );

        if (!EFI_ERROR (Status)) {
            if (Width > Info->HorizontalResolution) {
                continue;
            }
            if (Height > Info->VerticalResolution) {
                continue;
            }

            BestMode = Mode;
            Width    = Info->HorizontalResolution;
            Height   = Info->VerticalResolution;
        }
    }

    #if REFIT_DEBUG > 0
    MsgLog ("  - BestMode: GOP Mode[%d] @ %dx%d", BestMode, Width, Height);
    #endif

    // check if requested mode is equal to current mode
    if (BestMode == GraphicsOutput->Mode->Mode) {
        Status = EFI_SUCCESS;

        #if REFIT_DEBUG > 0
        MsgLog ("\n\n");
        MsgLog ("INFO: Screen Resolution Already Set\n\n");
        #endif

        egScreenWidth  = GraphicsOutput->Mode->Info->HorizontalResolution;
        egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
    }
    else {
        #if REFIT_DEBUG > 0
        MsgLog ("\n");
        #endif

        Status = GopSetModeAndReconnectTextOut (BestMode);
        if (!EFI_ERROR (Status)) {
            egScreenWidth  = Width;
            egScreenHeight = Height;
        }
        else {
            // we cannot set BestMode - search for first one that we can use
            Status = egSetGOPMode (1);

            #if REFIT_DEBUG > 0
            MsgLog ("** WARN: Could Not Set BestMode ...Using First Useable Mode\n\n");
            #endif
        }
    }

    return Status;
}


//
// Screen handling
//

// Make the necessary system calls to identify the current graphics mode.
// Stores the results in the file-global variables egScreenWidth,
// egScreenHeight, and egHasGraphics. The first two of these will be
// unchanged if neither GraphicsOutput nor UGADraw is a valid pointer.
static
VOID
egDetermineScreenSize (
    VOID
) {
    EFI_STATUS Status = EFI_SUCCESS;
    UINT32     ScreenW;
    UINT32     ScreenH;
    UINT32     UGADepth;
    UINT32     UGARefreshRate;

    // get screen size
    egHasGraphics = FALSE;
    if (GraphicsOutput != NULL) {
        egScreenWidth   = GraphicsOutput->Mode->Info->HorizontalResolution;
        egScreenHeight  = GraphicsOutput->Mode->Info->VerticalResolution;
        egHasGraphics   = TRUE;
    }
    else if (UGADraw != NULL) {
        Status = refit_call5_wrapper(
            UGADraw->GetMode, UGADraw,
            &ScreenW, &ScreenH,
            &UGADepth, &UGARefreshRate
        );
        if (EFI_ERROR (Status)) {
            UGADraw = NULL;   // graphics not available
        }
        else {
            egScreenWidth  = ScreenW;
            egScreenHeight = ScreenH;
            egHasGraphics  = TRUE;
        }
    }
} // static VOID egDetermineScreenSize()

VOID
egGetScreenSize (
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
}


VOID
egInitScreen (
    VOID
) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL  *OldGOP = NULL;
    EFI_STATUS                    Status  = EFI_SUCCESS;
    EFI_STATUS                    XFlag;
    UINTN                         HandleCount;
    EFI_HANDLE                    *HandleBuffer;
    UINTN                         i;
    BOOLEAN                       thisValidGOP = FALSE;


    #if REFIT_DEBUG > 0
            MsgLog ("Check for Graphics:\n");
    #endif

    // Get ConsoleControl Protocol
    ConsoleControl = NULL;

    #if REFIT_DEBUG > 0
    MsgLog ("  - Seek Console Control\n");
    #endif

    // Check ConsoleOut Handle
    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        gST->ConsoleOutHandle,
        &ConsoleControlProtocolGuid,
        (VOID **) &ConsoleControl
    );

    #if REFIT_DEBUG > 0
    MsgLog ("    * Seek on ConsoleOut Handle ...%r\n", Status);
    #endif

    if (EFI_ERROR (Status) || ConsoleControl == NULL) {
        // Try Locating by Handle
        Status = refit_call5_wrapper(
            gBS->LocateHandleBuffer,
            ByProtocol,
            &ConsoleControlProtocolGuid,
            NULL,
            &HandleCount,
            &HandleBuffer
        );

        #if REFIT_DEBUG > 0
        MsgLog ("    * Seek on Handle Buffer ...%r\n", Status);
        #endif

        if (!EFI_ERROR (Status)) {
            i = 0;
            for (i = 0; i < HandleCount; i++) {
                Status = refit_call3_wrapper(
                    gBS->HandleProtocol,
                    HandleBuffer[i],
                    &ConsoleControlProtocolGuid,
                    (VOID*) &ConsoleControl
                );

                #if REFIT_DEBUG > 0
                MsgLog ("    ** Evaluate on Handle[%02d] ...%r\n", i, Status);
                #endif

                if (!EFI_ERROR (Status)) {
                    break;
                }
            }
            FreePool (HandleBuffer);
        }
        else {
            // try locating directly
            Status = refit_call3_wrapper(
                gBS->LocateProtocol,
                &ConsoleControlProtocolGuid,
                NULL,
                (VOID*) &ConsoleControl
            );

            #if REFIT_DEBUG > 0
            MsgLog ("    * Seek Directly ...%r\n", Status);
            #endif
        }
    }

    if (EFI_ERROR (Status)) {
        DetectedDevices  = FALSE;

        #if REFIT_DEBUG > 0
        MsgLog ("  - Assess Console Control ...NOT OK!\n\n");
        #endif
    }
    else {
        #if REFIT_DEBUG > 0
        MsgLog ("  - Assess Console Control ...ok\n\n");
        #endif
    }


    // Get UGADraw Protocol
    UGADraw = NULL;

    #if REFIT_DEBUG > 0
    MsgLog ("  - Seek Universal Graphics Adapter\n");
    #endif

    // Check ConsoleOut Handle
    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        gST->ConsoleOutHandle,
        &UgaDrawProtocolGuid,
        (VOID **) &UGADraw
    );

    #if REFIT_DEBUG > 0
    MsgLog ("    * Seek on ConsoleOut Handle ...%r\n", Status);
    #endif

    if (EFI_ERROR (Status)) {
        // Try Locating by Handle
        Status = refit_call5_wrapper(
            gBS->LocateHandleBuffer,
            ByProtocol,
            &UgaDrawProtocolGuid,
            NULL,
            &HandleCount,
            &HandleBuffer
        );

        #if REFIT_DEBUG > 0
        MsgLog ("    * Seek on Handle Buffer ...%r\n", Status);
        #endif

        if (!EFI_ERROR (Status)) {
            EFI_UGA_DRAW_PROTOCOL *TmpUGA   = NULL;
            UINT32                UGAWidth  = 0;
            UINT32                UGAHeight = 0;
            UINT32                Width;
            UINT32                Height;
            UINT32                Depth;
            UINT32                RefreshRate;

            i = 0;
            for (i = 0; i < HandleCount; i++) {
                Status = refit_call3_wrapper(
                    gBS->HandleProtocol,
                    HandleBuffer[i],
                    &UgaDrawProtocolGuid,
                    (VOID*) &TmpUGA
                );

                #if REFIT_DEBUG > 0
                MsgLog ("    ** Examine Handle[%02d] ...%r\n", i, Status);
                #endif

                if (!EFI_ERROR (Status)) {
                    Status = refit_call5_wrapper(
                        TmpUGA->GetMode,
                        TmpUGA,
                        &Width,
                        &Height,
                        &Depth,
                        &RefreshRate
                    );
                    if (!EFI_ERROR (Status)) {
                        if (UGAWidth < Width ||
                            UGAHeight < Height
                        ) {
                            UGADraw   = TmpUGA;
                            UGAWidth  = Width;
                            UGAHeight = Height;

                            #if REFIT_DEBUG > 0
                            MsgLog (
                                "    *** Select Handle[%02d] @ %5dx%-5d\n",
                                i,
                                UGAWidth,
                                UGAHeight
                            );
                            #endif
                        }
                        else {
                            #if REFIT_DEBUG > 0
                            MsgLog (
                                "    *** Ignore Handle[%02d] @ %5dx%-5d\n",
                                i,
                                Width,
                                Height
                            );
                            #endif
                        }
                    }
                }
            }
            FreePool (HandleBuffer);
        }
        else {
            // try locating directly
            Status = refit_call3_wrapper(
                gBS->LocateProtocol,
                &UgaDrawProtocolGuid,
                NULL,
                (VOID*) &UGADraw
            );

            #if REFIT_DEBUG > 0
            MsgLog ("    * Seek Directly ...%r\n", Status);
            #endif
        }
    }

    if (EFI_ERROR (Status)) {
        #if REFIT_DEBUG > 0
        MsgLog ("  - Assess Universal Graphics Adapter ...NOT OK!\n\n");
        #endif
    }
    else {
        #if REFIT_DEBUG > 0
        MsgLog ("  - Assess Universal Graphics Adapter ...ok\n\n");
        #endif
    }

    // Get GraphicsOutput Protocol
    GraphicsOutput = NULL;

    #if REFIT_DEBUG > 0
    MsgLog ("  - Seek Graphics Output Protocol\n");
    #endif

    // Check ConsoleOut Handle
    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        gST->ConsoleOutHandle,
        &GraphicsOutputProtocolGuid,
        (VOID **) &OldGOP
    );

    #if REFIT_DEBUG > 0
    MsgLog ("    * Seek on ConsoleOut Handle ...%r\n", Status);
    #endif

    if (EFI_ERROR (Status)) {
        // Try Locating by Handle
        Status = refit_call5_wrapper(
            gBS->LocateHandleBuffer,
            ByProtocol,
            &GraphicsOutputProtocolGuid,
            NULL,
            &HandleCount,
            &HandleBuffer
        );

        #if REFIT_DEBUG > 0
        MsgLog ("    * Seek on Handle Buffer ...%r\n", Status);
        #endif

        if (!EFI_ERROR (Status)) {
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;
            EFI_GRAPHICS_OUTPUT_PROTOCOL *TmpGOP = NULL;
            UINT32 GOPWidth  = 0;
            UINT32 GOPHeight = 0;
            UINT32 MaxMode   = 0;
            UINT32 GOPMode;
            UINTN  SizeOfInfo;

            i = 0;
            for (i = 0; i < HandleCount; i++) {
                Status = refit_call3_wrapper(
                    gBS->HandleProtocol,
                    HandleBuffer[i],
                    &GraphicsOutputProtocolGuid,
                    (VOID*) &OldGOP
                );

                #if REFIT_DEBUG > 0
                MsgLog ("    ** Evaluate on Handle[%02d] ...%r\n", i, Status);
                #endif

                if (!EFI_ERROR (Status)) {
                    TmpGOP = OldGOP;
                    MaxMode = TmpGOP->Mode->MaxMode;
                    for (GOPMode = 0; GOPMode < MaxMode; GOPMode++) {
                        Status = TmpGOP->QueryMode (TmpGOP, GOPMode, &SizeOfInfo, &Info);
                        if (!EFI_ERROR (Status)) {
                            if (GOPWidth < Info->HorizontalResolution ||
                                GOPHeight < Info->VerticalResolution
                            ) {
                                OldGOP    = TmpGOP;
                                GOPWidth  = Info->HorizontalResolution;
                                GOPHeight = Info->VerticalResolution;

                                #if REFIT_DEBUG > 0
                                MsgLog (
                                    "    *** Select Handle[%02d][%02d] @ %5dx%-5d\n",
                                    i,
                                    GOPMode,
                                    GOPWidth,
                                    GOPHeight
                                );
                                #endif
                            }
                            else {
                                #if REFIT_DEBUG > 0
                                MsgLog (
                                    "        Ignore Handle[%02d][%02d] @ %5dx%-5d\n",
                                    i,
                                    GOPMode,
                                    Info->HorizontalResolution,
                                    Info->VerticalResolution
                                );
                                #endif
                            }
                        }
                    }
                }
            }
            FreePool (HandleBuffer);
        }
        else {
            // try locating directly
            Status = refit_call3_wrapper(
                gBS->LocateProtocol,
                &GraphicsOutputProtocolGuid,
                NULL,
                (VOID*) &OldGOP
            );

            #if REFIT_DEBUG > 0
            MsgLog ("    * Seek Directly ...%r\n", Status);
            #endif

            if (EFI_ERROR (Status)) {
                // Force to NOT FOUND on error as subsequent code relies on this
                Status = EFI_NOT_FOUND;
            }
        }
    }

    XFlag = EFI_UNSUPPORTED;

    if (Status == EFI_NOT_FOUND) {
        XFlag = EFI_NOT_FOUND;

        // Not Found
        #if REFIT_DEBUG > 0
        MsgLog ("  - Assess Graphics Output Protocol ...NOT FOUND!\n\n");
        #endif
    }

    if (EFI_ERROR (Status) && XFlag == EFI_UNSUPPORTED) {
        XFlag = EFI_NOT_FOUND;

        // Not Found
        #if REFIT_DEBUG > 0
        MsgLog ("  - Assess Graphics Output Protocol ...ERROR!\n\n");
        #endif
    }
    else if (!EFI_ERROR (Status) && XFlag != EFI_ALREADY_STARTED) {
        if (OldGOP->Mode->MaxMode > 0) {
            XFlag        = EFI_SUCCESS;
            thisValidGOP = TRUE;

            // Set GOP to OldGOP
            GraphicsOutput = OldGOP;

            #if REFIT_DEBUG > 0
            MsgLog ("  - Assess Graphics Output Protocol ...ok\n\n");
            #endif
        }
        else {
            XFlag = EFI_UNSUPPORTED;

            #if REFIT_DEBUG > 0
            MsgLog ("  - Assess Graphics Output Protocol ...NOT OK!\n\n");
            #endif

            if (GlobalConfig.ProvideConsoleGOP) {
                Status = daCheckAltGop();

                if (!EFI_ERROR (Status)) {
                    Status = OcProvideConsoleGop (TRUE);

                    if (!EFI_ERROR (Status)) {
                        Status = gBS->HandleProtocol (
                            gST->ConsoleOutHandle,
                            &GraphicsOutputProtocolGuid,
                            (VOID **) &GraphicsOutput
                        );
                    }
                }
            }
        }
    }

    if (XFlag != EFI_NOT_FOUND && XFlag != EFI_UNSUPPORTED && GlobalConfig.UseDirectGop) {
        if (XFlag != EFI_SUCCESS) {
            XFlag  = EFI_LOAD_ERROR;
        }

        if (GraphicsOutput == NULL) {
            #if REFIT_DEBUG > 0
            MsgLog ("INFO: Cannot Implement Direct GOP Renderer\n\n");
            #endif
        }
        else {
            if (GraphicsOutput->Mode->Info->PixelFormat == PixelBltOnly) {
                Status = EFI_UNSUPPORTED;
            }
            else {
                Status = OcUseDirectGop (-1);
            }

            if (!EFI_ERROR (Status)) {
                // Check ConsoleOut Handle
                Status = refit_call3_wrapper(
                    gBS->HandleProtocol,
                    gST->ConsoleOutHandle,
                    &GraphicsOutputProtocolGuid,
                    (VOID **) &OldGOP
                );
                if (EFI_ERROR (Status)) {
                    OldGOP = NULL;
                }
                else {
                    if (OldGOP->Mode->MaxMode > 0) {
                        GraphicsOutput = OldGOP;
                        XFlag = EFI_ALREADY_STARTED;
                    }
                }
            }

            #if REFIT_DEBUG > 0
            MsgLog ("INFO: Implement Direct GOP Renderer ...%r\n\n", Status);
            #endif
        }
    }

    if (XFlag == EFI_NOT_FOUND || XFlag == EFI_LOAD_ERROR) {
        #if REFIT_DEBUG > 0
        MsgLog ("INFO: Implement Graphics Output Protocol ...%r\n\n", XFlag);
        #endif
    }
    else if (XFlag == EFI_UNSUPPORTED) {
        #if REFIT_DEBUG > 0
        MsgLog ("INFO: Provide GOP on ConsoleOut Handle ...%r\n\n", Status);
        #endif

        if (!EFI_ERROR (Status)) {
            thisValidGOP = TRUE;
        }
    }

    // Get Screen Size
    egHasGraphics = FALSE;
    if (GraphicsOutput != NULL) {
        Status = egDumpGOPVideoModes();

        if (EFI_ERROR (Status)) {
            #if REFIT_DEBUG > 0
            MsgLog ("** WARN: Invalid GOP Instance\n\n");
            #endif

            GraphicsOutput = NULL;
        }
        else {
            egHasGraphics  = TRUE;

            Status = egSetMaxResolution();

            if (!EFI_ERROR (Status)) {
                egScreenWidth  = GraphicsOutput->Mode->Info->HorizontalResolution;
                egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
            }
            else {
                egScreenWidth  = GlobalConfig.RequestedScreenWidth;
                egScreenHeight = GlobalConfig.RequestedScreenHeight;
            }

            #if REFIT_DEBUG > 0
            // Only log this if GOPFix or Direct Renderer attempted
            if (XFlag == EFI_UNSUPPORTED || XFlag == EFI_ALREADY_STARTED) {
                MsgLog ("INFO: Implement Graphics Output Protocol ...%r\n\n", Status);
            }
            #endif
        }
    }
    if (UGADraw != NULL) {
        if (GlobalConfig.UgaPassThrough && thisValidGOP) {
            // Run OcProvideUgaPassThrough from OpenCorePkg
            Status = OcProvideUgaPassThrough();

            #if REFIT_DEBUG > 0
            MsgLog ("INFO: Implement UGA Pass Through ...%r\n\n", Status);
            #endif
        }

        if (GraphicsOutput == NULL) {
            UINT32 Width, Height, Depth, RefreshRate;
            Status = refit_call5_wrapper(
                UGADraw->GetMode, UGADraw,
                &Width, &Height,
                &Depth, &RefreshRate
            );

            if (!EFI_ERROR (Status)) {
                #if REFIT_DEBUG > 0
                MsgLog ("INFO: GOP not Available\n");
                MsgLog ("      Fall Back on UGA\n\n");
                #endif

                egHasGraphics  = TRUE;
                egScreenWidth  = GlobalConfig.RequestedScreenWidth  = Width;
                egScreenHeight = GlobalConfig.RequestedScreenHeight = Height;
            }
            else {
                // Graphics not available
                UGADraw               = NULL;
                GlobalConfig.TextOnly = TRUE;

                #if REFIT_DEBUG > 0
                MsgLog ("INFO: Graphics not Available\n");
                MsgLog ("      Fall Back on Text Mode\n\n");
                #endif
            }
        }
    }

    if (GraphicsOutput != NULL &&
        (GlobalConfig.TextRenderer || GlobalConfig.TextOnly)
    ) {
        // Implement Text Renderer
        EFI_CONSOLE_CONTROL_SCREEN_MODE  ScreenMode;

        if (egHasGraphics) {
            ScreenMode = EfiConsoleControlScreenGraphics;
        }
        else {
            ScreenMode = EfiConsoleControlScreenText;
        }

        Status = OcUseBuiltinTextOutput (ScreenMode);

        #if REFIT_DEBUG > 0
        MsgLog ("INFO: Implement Text Renderer ...%r\n\n", Status);
        #endif
    }
}

// Convert a graphics mode (in *ModeWidth) to a width and height (returned in
// *ModeWidth and *Height, respectively).
// Returns TRUE if successful, FALSE if not (invalid mode, typically)
BOOLEAN
egGetResFromMode (
    UINTN *ModeWidth,
    UINTN *Height
) {
   UINTN                                 Size;
   EFI_STATUS                            Status;
   EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info = NULL;

   if ((ModeWidth != NULL) && (Height != NULL) && GraphicsOutput) {
      Status = refit_call4_wrapper(
          GraphicsOutput->QueryMode,
          GraphicsOutput,
          *ModeWidth,
          &Size,
          &Info
      );
      if (!EFI_ERROR (Status) && (Info != NULL)) {
         *ModeWidth = Info->HorizontalResolution;
         *Height    = Info->VerticalResolution;

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
BOOLEAN
egSetScreenSize (
    IN OUT UINTN *ScreenWidth,
    IN OUT UINTN *ScreenHeight
) {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;

    EFI_STATUS   Status  = EFI_SUCCESS;
    BOOLEAN      ModeSet = FALSE;
    UINTN        Size;
    UINT32       ModeNum = 0;
    UINT32       CurrentModeNum;
    UINT32       ScreenW;
    UINT32       ScreenH;
    UINT32       UGADepth;
    UINT32       UGARefreshRate;
    CHAR16       *ShowScreenStr     = NULL;

    #if REFIT_DEBUG > 0
    MsgLog ("Set Screen Size Manually. H = %d and W = %d\n", ScreenHeight, ScreenWidth);
    #endif

    if ((ScreenWidth == NULL) || (ScreenHeight == NULL)) {
        SwitchToText (FALSE);

        ShowScreenStr = L"Invalid Input Resolution in Config File!";

        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
        PrintUglyText (ShowScreenStr, NEXTLINE);
        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);


        #if REFIT_DEBUG > 0
        MsgLog ("%s\n", ShowScreenStr);
        #endif

        PauseForKey();
        MyFreePool (ShowScreenStr);

        return FALSE;
    }

    if (GraphicsOutput != NULL) {
        // GOP mode (UEFI)
        CurrentModeNum = GraphicsOutput->Mode->Mode;

        #if REFIT_DEBUG > 0
        MsgLog ("  - GraphicsOutput Object Found ...Current Mode = %d\n", CurrentModeNum);
        #endif

        if (*ScreenHeight == 0) {
            // User specified a mode number (stored in *ScreenWidth); use it directly
            ModeNum = (UINT32) *ScreenWidth;
            if (ModeNum != CurrentModeNum) {
                #if REFIT_DEBUG > 0
                MsgLog ("  - Mode Set from ScreenWidth\n\n");
                #endif

                ModeSet = TRUE;
            }
            else if (egGetResFromMode (ScreenWidth, ScreenHeight) &&
                (refit_call2_wrapper(
                    GraphicsOutput->SetMode,
                    GraphicsOutput,
                    ModeNum
                ) == EFI_SUCCESS)
            ) {
                #if REFIT_DEBUG > 0
                MsgLog ("  - Mode Set from egGetResFromMode\n\n");
                #endif

                ModeSet = TRUE;
            }
            else {
                #if REFIT_DEBUG > 0
                MsgLog ("  - Could Not Set GraphicsOutput Mode\n\n");
                #endif
            }
            // User specified width & height; must find mode...
        }
        else {
            // Do a loop through the modes to see if the specified one is available;
            // and if so, switch to it....
            do {
                Status = refit_call4_wrapper(
                    GraphicsOutput->QueryMode,
                    GraphicsOutput,
                    ModeNum,
                    &Size,
                    &Info
                );
                if ((!EFI_ERROR (Status)) &&
                    (Size >= sizeof (*Info) &&
                    (Info != NULL)) &&
                    (Info->HorizontalResolution == *ScreenWidth) &&
                    (Info->VerticalResolution   == *ScreenHeight) &&
                    ((ModeNum == CurrentModeNum) ||
                    (refit_call2_wrapper(
                        GraphicsOutput->SetMode,
                        GraphicsOutput,
                        ModeNum
                    ) == EFI_SUCCESS))
                ) {
                    #if REFIT_DEBUG > 0
                    MsgLog ("  - Mode Set from GraphicsOutput Query\n\n");
                    #endif

                    ModeSet = TRUE;
                }
                else {
                    #if REFIT_DEBUG > 0
                    MsgLog ("  - Could Not Set GraphicsOutput Mode\n\n");
                    #endif
                }
            } while ((++ModeNum < GraphicsOutput->Mode->MaxMode) && !ModeSet);
        } // if/else

        if (ModeSet) {
            egScreenWidth  = *ScreenWidth;
            egScreenHeight = *ScreenHeight;
        }
        else {
            // If unsuccessful, display an error message for the user....
            SwitchToText (FALSE);

            ShowScreenStr = L"Invalid Resolution Setting Provided ...Trying Default Modes:";
            PrintUglyText (ShowScreenStr, NEXTLINE);

            #if REFIT_DEBUG > 0
            MsgLog ("%s", ShowScreenStr);
            #endif

            MyFreePool (ShowScreenStr);

            ModeNum = 0;
            do {

                #if REFIT_DEBUG > 0
                MsgLog ("\n");
                #endif

                Status = refit_call4_wrapper(
                    GraphicsOutput->QueryMode,
                    GraphicsOutput,
                    ModeNum,
                    &Size,
                    &Info
                );
                if (!EFI_ERROR (Status) && (Info != NULL)) {

                    ShowScreenStr = PoolPrint (
                        L"Available Mode: Mode[%02d][%dx%d]",
                        ModeNum,
                        Info->HorizontalResolution,
                        Info->VerticalResolution
                    );
                    PrintUglyText (ShowScreenStr, NEXTLINE);

                    #if REFIT_DEBUG > 0
                    MsgLog ("  - %s", ShowScreenStr);
                    #endif

                    if (ModeNum == CurrentModeNum) {
                        egScreenWidth  = Info->HorizontalResolution;
                        egScreenHeight = Info->VerticalResolution;
                    } // if

                }
                else {
                    ShowScreenStr = L"Error : Could Not Query GraphicsOutput Mode";
                    PrintUglyText (ShowScreenStr, NEXTLINE);

                    #if REFIT_DEBUG > 0
                    MsgLog ("  - %s", ShowScreenStr);
                    #endif

                } // if

                MyFreePool (ShowScreenStr);
            } while (++ModeNum < GraphicsOutput->Mode->MaxMode);

            #if REFIT_DEBUG > 0
            MsgLog ("\n\n");
            #endif

            PauseForKey();
            SwitchToGraphicsAndClear (TRUE);
        } // if GOP mode (UEFI)
    }
    else if (UGADraw != NULL) { // UGA mode (EFI 1.x)
        // Try to use current color depth & refresh rate for new mode. Maybe not the best choice
        // in all cases, but I don't know how to probe for alternatives....
        Status = refit_call5_wrapper(
            UGADraw->GetMode, UGADraw,
            &ScreenW, &ScreenH,
            &UGADepth, &UGARefreshRate
        );
        Status = refit_call5_wrapper(
            UGADraw->SetMode, UGADraw,
            ScreenW, ScreenH,
            UGADepth, UGARefreshRate
        );

        *ScreenWidth  = (UINTN) ScreenW;
        *ScreenHeight = (UINTN) ScreenH;

        if (!EFI_ERROR (Status)) {
            egScreenWidth  = *ScreenWidth;
            egScreenHeight = *ScreenHeight;
            ModeSet = TRUE;
        }
        else {
            // TODO: Find a list of supported modes and display it.
            // NOTE: Below doesn't actually appear unless we explicitly switch to text mode.
            // This is just a placeholder until something better can be done....
            ShowScreenStr = PoolPrint (
                L"Error setting %dx%d resolution ...Unsupported Mode",
                *ScreenWidth,
                *ScreenHeight
            );
            PrintUglyText (ShowScreenStr, NEXTLINE);

            #if REFIT_DEBUG > 0
            MsgLog ("%s\n", ShowScreenStr);
            #endif

            MyFreePool (ShowScreenStr);
        } // if/else
    } // if/else if (UGADraw != NULL)

    return (ModeSet);
} // BOOLEAN egSetScreenSize()

// Set a text mode.
// Returns TRUE if the mode actually changed, FALSE otherwise.
// Note that a FALSE return value can mean either an error or no change
// necessary.
BOOLEAN
egSetTextMode (
    UINT32 RequestedMode
) {
    EFI_STATUS   Status;
    BOOLEAN      ChangedIt = FALSE;
    UINTN        i = 0;
    UINTN        Width;
    UINTN        Height;
    CHAR16       *ShowScreenStr     = NULL;

    if ((RequestedMode != DONT_CHANGE_TEXT_MODE) &&
        (RequestedMode != gST->ConOut->Mode->Mode)
    ) {
        Status = refit_call2_wrapper(
            gST->ConOut->SetMode,
            gST->ConOut,
            RequestedMode
        );
        if (!EFI_ERROR (Status)) {
            ChangedIt = TRUE;
        }
        else {
            SwitchToText (FALSE);

            ShowScreenStr = L"Error Setting Resolution ...Unsupported Mode";
            PrintUglyText (ShowScreenStr, NEXTLINE);

            #if REFIT_DEBUG > 0
            MsgLog ("%s\n", ShowScreenStr);
            #endif

            ShowScreenStr = L"Seek Available Modes:";
            PrintUglyText (ShowScreenStr, NEXTLINE);

            #if REFIT_DEBUG > 0
            MsgLog ("%s\n", ShowScreenStr);
            #endif

            do {
                Status = refit_call4_wrapper(
                    gST->ConOut->QueryMode,
                    gST->ConOut,
                    i,
                    &Width,
                    &Height
                );

                if (!EFI_ERROR (Status)) {
                    ShowScreenStr = PoolPrint (L"  - Mode[%d] (%dx%d)", i, Width, Height);
                    PrintUglyText (ShowScreenStr, NEXTLINE);

                    #if REFIT_DEBUG > 0
                    MsgLog ("%s\n", ShowScreenStr);
                    #endif

                    MyFreePool (ShowScreenStr);
                }

            } while (++i < gST->ConOut->Mode->MaxMode);

            ShowScreenStr = PoolPrint (L"Use Default Mode[%d]:", DONT_CHANGE_TEXT_MODE);
            PrintUglyText (ShowScreenStr, NEXTLINE);

            #if REFIT_DEBUG > 0
            MsgLog ("%s\n", ShowScreenStr);
            #endif

            PauseForKey();
            SwitchToGraphicsAndClear (TRUE);
            MyFreePool (ShowScreenStr);
        } // if/else successful change
    } // if need to change mode

    return ChangedIt;
} // BOOLEAN egSetTextMode()

CHAR16 *
egScreenDescription (
    VOID
) {
    CHAR16       *GraphicsInfo;
    CHAR16       *TextInfo      = NULL;
    CHAR16       *ShowScreenStr = NULL;

    GraphicsInfo = AllocateZeroPool (256 * sizeof (CHAR16));
    if (GraphicsInfo == NULL) {
        SwitchToText (FALSE);

        ShowScreenStr = L"Memory Allocation Error";

        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
        PrintUglyText (ShowScreenStr, NEXTLINE);
        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

        #if REFIT_DEBUG > 0
        MsgLog ("%s\n\n", ShowScreenStr);
        #endif

        PauseForKey();
        SwitchToGraphics();

        return ShowScreenStr;
    }

    if (egHasGraphics) {
        if (GraphicsOutput != NULL) {
            SPrint (GraphicsInfo, 255,
                L"Graphics Output Protocol (UEFI 2.x): %dx%d",
                egScreenWidth, egScreenHeight
            );
        }
        else if (UGADraw != NULL) {
            SPrint (GraphicsInfo, 255,
                L"Universal Graphics Adapter (EFI 1.x): %dx%d",
                egScreenWidth, egScreenHeight
            );
        }
        else {
            MyFreePool (GraphicsInfo);
            MyFreePool (TextInfo);

            SwitchToText (FALSE);

            ShowScreenStr = L"Internal Error While Getting Graphics Details";

            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
            PrintUglyText (ShowScreenStr, NEXTLINE);
            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

            #if REFIT_DEBUG > 0
            MsgLog ("** Warn: %s\n\n", ShowScreenStr);
            #endif

            PauseForKey();
            SwitchToGraphics();

            return ShowScreenStr;
        }

        if (!AllowGraphicsMode) {
            // Graphics Capable Hardware in Text Mode
            TextInfo = AllocateZeroPool (256 * sizeof (CHAR16));
            SPrint (
                TextInfo, 255,
                L"(Text Mode: %dx%d [Graphics Capable])",
                ConWidth, ConHeight
            );
            MergeStrings (&GraphicsInfo, TextInfo, L' ');
        }
    }
    else {
        SPrint (
            GraphicsInfo, 255,
            L"Text-Only Console: %dx%d",
            ConWidth, ConHeight
        );
    }

    MyFreePool (TextInfo);

    return GraphicsInfo;
}

BOOLEAN
egHasGraphicsMode (
    VOID
) {
    return egHasGraphics;
}

BOOLEAN
egIsGraphicsModeEnabled (
    VOID
) {
    EFI_CONSOLE_CONTROL_SCREEN_MODE CurrentMode;

    if (ConsoleControl != NULL) {
        refit_call4_wrapper(
            ConsoleControl->GetMode,
            ConsoleControl,
            &CurrentMode,
            NULL, NULL
        );

        return (CurrentMode == EfiConsoleControlScreenGraphics) ? TRUE : FALSE;
    }

    return FALSE;
}

VOID
egSetGraphicsModeEnabled (
    IN BOOLEAN Enable
) {
    EFI_CONSOLE_CONTROL_SCREEN_MODE CurrentMode;
    EFI_CONSOLE_CONTROL_SCREEN_MODE NewMode;

    if (ConsoleControl != NULL) {
        refit_call4_wrapper(
            ConsoleControl->GetMode,
            ConsoleControl,
            &CurrentMode,
            NULL, NULL
        );

        if (Enable) {
            NewMode = EfiConsoleControlScreenGraphics;
        }
        else {
            NewMode = EfiConsoleControlScreenText;
        }

        if (CurrentMode != NewMode) {
            refit_call2_wrapper(
                ConsoleControl->SetMode,
                ConsoleControl,
                NewMode
            );
        }
    }
}

//
// Drawing to the screen
//

VOID
egClearScreen (
    IN EG_PIXEL *Color
) {
    EFI_UGA_PIXEL FillColor;

    if (!egHasGraphics) {
        return;
    }

    if (Color != NULL) {
        FillColor.Red   = Color->r;
        FillColor.Green = Color->g;
        FillColor.Blue  = Color->b;
    }
    else {
        FillColor.Red   = 0x0;
        FillColor.Green = 0x0;
        FillColor.Blue  = 0x0;
    }
    FillColor.Reserved = 0;

    if (GraphicsOutput != NULL) {
        // EFI_GRAPHICS_OUTPUT_BLT_PIXEL and EFI_UGA_PIXEL have the same
        // layout, and the header from TianoCore actually defines them
        // to be the same type.
        refit_call10_wrapper(
            GraphicsOutput->Blt, GraphicsOutput,
            (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) &FillColor,
             EfiBltVideoFill,
             0, 0, 0, 0,
             egScreenWidth, egScreenHeight,
             0
         );
    }
    else if (UGADraw != NULL) {
        refit_call10_wrapper(
            UGADraw->Blt, UGADraw,
            &FillColor,
            EfiUgaVideoFill,
            0, 0, 0, 0,
            egScreenWidth, egScreenHeight,
            0
        );
    }
}

VOID
egDrawImage (
    IN EG_IMAGE *Image,
    IN UINTN    ScreenPosX,
    IN UINTN    ScreenPosY
) {
    EG_IMAGE *CompImage = NULL;

    // NOTE: Weird seemingly redundant tests because some placement code can "wrap around" and
    // send "negative" values, which of course become very large unsigned ints that can then
    // wrap around AGAIN if values are added to them.....
    if ((!egHasGraphics) ||
        ((ScreenPosX + Image->Width)  > egScreenWidth) ||
        ((ScreenPosY + Image->Height) > egScreenHeight) ||
        (ScreenPosX > egScreenWidth) ||
        (ScreenPosY > egScreenHeight)
    ) {
        return;
    }

    if ((GlobalConfig.ScreenBackground == NULL) ||
        ((Image->Width == egScreenWidth) && (Image->Height == egScreenHeight))
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
          MsgLog ("Error! Cannot crop image in egDrawImage()!\n");
          #endif

          return;
       }

       egComposeImage (CompImage, Image, 0, 0);
    }

    if (GraphicsOutput != NULL) {
        refit_call10_wrapper(
            GraphicsOutput->Blt, GraphicsOutput,
            (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) CompImage->PixelData,
            EfiBltBufferToVideo,
            0, 0,
            ScreenPosX, ScreenPosY,
            CompImage->Width, CompImage->Height,
            0
        );
    }
    else if (UGADraw != NULL) {
        refit_call10_wrapper(
            UGADraw->Blt, UGADraw,
            (EFI_UGA_PIXEL *) CompImage->PixelData,
            EfiUgaBltBufferToVideo,
            0, 0,
            ScreenPosX, ScreenPosY,
            CompImage->Width, CompImage->Height,
            0
        );
    }
    if ((CompImage != GlobalConfig.ScreenBackground) && (CompImage != Image)) {
        egFreeImage (CompImage);
    }
} /* VOID egDrawImage() */

// Display an unselected icon on the screen, so that the background image shows
// through the transparency areas. The BadgeImage may be NULL, in which case
// it's not composited in.
VOID
egDrawImageWithTransparency (
    EG_IMAGE *Image,
    EG_IMAGE *BadgeImage,
    UINTN    XPos,
    UINTN    YPos,
    UINTN    Width,
    UINTN    Height
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
      egFreeImage (Background);
   }
} // VOID DrawImageWithTransparency()

VOID
egDrawImageArea (
    IN EG_IMAGE *Image,
    IN UINTN    AreaPosX,
    IN UINTN    AreaPosY,
    IN UINTN    AreaWidth,
    IN UINTN    AreaHeight,
    IN UINTN    ScreenPosX,
    IN UINTN    ScreenPosY
) {
    if (!egHasGraphics)
        return;

    egRestrictImageArea (
        Image,
        AreaPosX, AreaPosY,
        &AreaWidth, &AreaHeight
    );

    if (AreaWidth == 0) {
        return;
    }

    if (GraphicsOutput != NULL) {
        refit_call10_wrapper(
            GraphicsOutput->Blt, GraphicsOutput,
            (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) Image->PixelData,
            EfiBltBufferToVideo,
            AreaPosX, AreaPosY,
            ScreenPosX, ScreenPosY,
            AreaWidth, AreaHeight,
            Image->Width * 4
        );
    }
    else if (UGADraw != NULL) {
        refit_call10_wrapper(
            UGADraw->Blt, UGADraw,
            (EFI_UGA_PIXEL *) Image->PixelData,
            EfiUgaBltBufferToVideo,
            AreaPosX, AreaPosY,
            ScreenPosX, ScreenPosY,
            AreaWidth, AreaHeight,
            Image->Width * 4
        );
    }
}

// Display a message in the center of the screen, surrounded by a box of the
// specified color. For the moment, uses graphics calls only. (It still works
// in text mode on GOP/UEFI systems, but not on UGA/EFI 1.x systems.)
VOID
egDisplayMessage (
    IN CHAR16 *Text,
    EG_PIXEL  *BGColor,
    UINTN     PositionCode
) {
   UINTN BoxWidth, BoxHeight;
   static UINTN Position = 1;
   EG_IMAGE *Box;

   if ((Text != NULL) && (BGColor != NULL)) {
      egMeasureText (Text, &BoxWidth, &BoxHeight);
      BoxWidth += 14;
      BoxHeight *= 2;

      if (BoxWidth > egScreenWidth) {
          BoxWidth = egScreenWidth;
      }

      Box = egCreateFilledImage (BoxWidth, BoxHeight, FALSE, BGColor);
      egRenderText (
          Text, Box, 7,
          BoxHeight / 4,
          (BGColor->r + BGColor->g + BGColor->b) / 3
      );

      switch (PositionCode) {
          case CENTER:
              Position = (egScreenHeight - BoxHeight) / 2;
              break;
          case BOTTOM:
              Position = egScreenHeight - (BoxHeight * 2);
              break;
          case TOP:
              Position = 1;
              break;
          default: // NEXTLINE
              Position += BoxHeight + (BoxHeight / 10);
              break;
      } // switch()

      egDrawImage (Box, (egScreenWidth - BoxWidth) / 2, Position);
      if ((PositionCode == CENTER) || (Position >= egScreenHeight - (BoxHeight * 5))) {
          Position = 1;
      }
   } // if non-NULL inputs
} // VOID egDisplayMessage()

// Copy the current contents of the display into an EG_IMAGE....
// Returns pointer if successful, NULL if not.
EG_IMAGE * egCopyScreen (VOID) {
   return egCopyScreenArea (0, 0, egScreenWidth, egScreenHeight);
} // EG_IMAGE * egCopyScreen()

// Copy the current contents of the specified display area into an EG_IMAGE....
// Returns pointer if successful, NULL if not.
EG_IMAGE * egCopyScreenArea (
    UINTN XPos,  UINTN YPos,
    UINTN Width, UINTN Height
) {
    EG_IMAGE *Image = NULL;

   if (!egHasGraphics) {
       return NULL;
   }

   // allocate a buffer for the screen area
   Image = egCreateImage (Width, Height, FALSE);
   if (Image == NULL) {
      return NULL;
   }

   // get full screen image
   if (GraphicsOutput != NULL) {
       refit_call10_wrapper(
           GraphicsOutput->Blt, GraphicsOutput,
           (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)Image->PixelData,
           EfiBltVideoToBltBuffer,
           XPos, YPos,
           0, 0,
           Image->Width, Image->Height,
           0
       );
   }
   else if (UGADraw != NULL) {
       refit_call10_wrapper(
           UGADraw->Blt, UGADraw,
           (EFI_UGA_PIXEL *)Image->PixelData,
           EfiUgaVideoToBltBuffer,
           XPos, YPos,
           0, 0,
           Image->Width, Image->Height,
           0
       );
   }

   return Image;
} // EG_IMAGE * egCopyScreenArea()

//
// Make a screenshot
//

VOID
egScreenShot (
    VOID
) {
    EFI_STATUS   Status;
    EFI_FILE     *BaseDir;
    EG_IMAGE     *Image;
    UINT8        *FileData;
    UINT8        Temp;
    UINTN        i = 0;
    UINTN        FileDataSize;         ///< Size in bytes
    UINTN        FilePixelSize;        ///< Size in pixels
    CHAR16       *FileName       = NULL;
    CHAR16       *ShowScreenStr  = NULL;

    #if REFIT_DEBUG > 0
    MsgLog ("User Input Received:\n");
    MsgLog ("  - Take Screenshot\n");
    #endif

    Image = egCopyScreen();
    if (Image == NULL) {
        SwitchToText (FALSE);

        ShowScreenStr = L"    * Error: Unable to Take Screen Shot";

        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
        PrintUglyText (ShowScreenStr, NEXTLINE);
        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

        #if REFIT_DEBUG > 0
        MsgLog ("%s\n\n", ShowScreenStr);
        #endif

        PauseForKey();
        SwitchToGraphics();
        MyFreePool (ShowScreenStr);

       goto bailout_wait;
    }

    // fix pixels
    FilePixelSize = Image->Width * Image->Height;
    for (i = 0; i < FilePixelSize; ++i) {
        Temp                   = Image->PixelData[i].b;
        Image->PixelData[i].b  = Image->PixelData[i].r;
        Image->PixelData[i].r  = Temp;
        Image->PixelData[i].a  = 0xFF;
    }

    // encode as PNG
    Status = EncodeAsPNG (
        (VOID *)  Image->PixelData,
        (UINT32)  Image->Width,
        (UINT32)  Image->Height,
        (VOID **) &FileData,
        &FileDataSize
    );

    egFreeImage (Image);
    if (EFI_ERROR (Status)) {
        SwitchToText (FALSE);

        ShowScreenStr = L"    * Error: Could Not Encode PNG";

        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
        PrintUglyText (ShowScreenStr, NEXTLINE);
        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

        #if REFIT_DEBUG > 0
        MsgLog ("%s\n\n", ShowScreenStr);
        #endif

        HaltForKey();
        SwitchToGraphics();
        MyFreePool (ShowScreenStr);

        return;
    }

    // Save to first available ESP if not running from ESP
    if (!MyStriCmp (SelfVolume->VolName, L"EFI") &&
        !MyStriCmp (SelfVolume->VolName, L"ESP")
    ) {
        Status = egFindESP (&BaseDir);
        if (EFI_ERROR (Status)) {
            SwitchToText (FALSE);

            ShowScreenStr = L"    * Error: Could Not Save Screenshot";

            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
            PrintUglyText (ShowScreenStr, NEXTLINE);
            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

            #if REFIT_DEBUG > 0
            MsgLog ("%s\n\n", ShowScreenStr);
            #endif

            HaltForKey();
            SwitchToGraphics();
            MyFreePool (ShowScreenStr);

            return;
        }
    }
    else {
        SelfRootDir = LibOpenRoot (SelfLoadedImage->DeviceHandle);
        if (SelfRootDir != NULL) {
            BaseDir = SelfRootDir;
        }
        else {
            // Try to save to first available ESP
            Status = egFindESP (&BaseDir);
            if (EFI_ERROR (Status)) {
                SwitchToText (FALSE);

                ShowScreenStr = L"    * Error: Could Not Find ESP for Screenshot";

                refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
                PrintUglyText (ShowScreenStr, NEXTLINE);
                refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

                #if REFIT_DEBUG > 0
                MsgLog ("%s\n\n", ShowScreenStr);
                #endif

                HaltForKey();
                SwitchToGraphics();
                MyFreePool (ShowScreenStr);

                return;
            }
        }
    }

    // Search for existing screen shot files; increment number to an unused value...
    i = 0;
    do {
        MyFreePool (FileName);
        FileName = PoolPrint (L"ScreenShot_%03d.png", i++);
    } while (FileExists (BaseDir, FileName));

    // save to file on the ESP
    Status = egSaveFile (BaseDir, FileName, (UINT8 *) FileData, FileDataSize);
    FreePool (FileData);
    if (CheckError (Status, L"in egSaveFile()")) {
        goto bailout_wait;
    }

    #if REFIT_DEBUG > 0
    MsgLog ("    * Screenshot Taken and Saved:- '%s'\n\n", FileName);
    #endif

    return;

    // DEBUG: switch to text mode
bailout_wait:
    i = 0;
    egSetGraphicsModeEnabled (FALSE);
    refit_call3_wrapper(
        gBS->WaitForEvent, 1,
        &gST->ConIn->WaitForKey, &i
    );
}

/* EOF */
