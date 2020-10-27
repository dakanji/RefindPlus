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

// July 2020: Extensively modiied by dakanji (sourceforge.net/u/dakanji/profile)
#include "libegint.h"
#include "../refind/screen.h"
#include "../refind/lib.h"
#include "../refind/mystrings.h"
#include "../include/refit_call_wrapper.h"
#include "libeg.h"
#include "../include/Handle.h"

#include <efiUgaDraw.h>
#include <efiConsoleControl.h>

#ifndef __MAKEWITH_GNUEFI
#define LibLocateProtocol EfiLibLocateProtocol
#endif

// Console defines and variables
static EFI_GUID ConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;
static EFI_GUID UgaDrawProtocolGuid = EFI_UGA_DRAW_PROTOCOL_GUID;
static EFI_GUID GraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

static EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;
static EFI_UGA_DRAW_PROTOCOL        *UGADraw = NULL;
static EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput = NULL;

static EFI_HANDLE_PROTOCOL daOrigProtocol;
static BOOLEAN egHasGraphics  = FALSE;
static UINTN   egScreenWidth  = 0;
static UINTN   egScreenHeight = 0;

STATIC
EFI_STATUS
EFIAPI
daConsoleHandleProtocol (
    IN  EFI_HANDLE        Handle,
    IN  EFI_GUID          *Protocol,
    OUT VOID              **Interface
) {
    EFI_STATUS  Status;

    Status = daOrigProtocol (Handle, Protocol, Interface);

    if (Status != EFI_UNSUPPORTED) {
        return Status;
    }

    if (CompareGuid (&gEfiGraphicsOutputProtocolGuid, Protocol)) {
        if (GraphicsOutput != NULL) {
            *Interface = GraphicsOutput;
            return EFI_SUCCESS;
        }
    }
    else if (CompareGuid (&gEfiUgaDrawProtocolGuid, Protocol)) {
        //
        // EfiBoot from 10.4 can only use UgaDraw protocol.
        //
        Status = gBS->LocateProtocol (
            &gEfiUgaDrawProtocolGuid,
            NULL,
            Interface
        );
        if (!EFI_ERROR (Status)) {
            return EFI_SUCCESS;
        }
    }

    return EFI_UNSUPPORTED;
}


STATIC
EFI_STATUS
EFIAPI
daCheckAltGop (
    VOID
) {
    EFI_STATUS                    Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL  *OrigGop;
    EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop;
    UINTN                         HandleCount;
    EFI_HANDLE                    *HandleBuffer;
    UINTN                         Index;

    daOrigProtocol      = gBS->HandleProtocol;
    gBS->HandleProtocol = daConsoleHandleProtocol;
    gBS->CalculateCrc32 (gBS, gBS->Hdr.HeaderSize, 0);

    #if REFIT_DEBUG > 0
    MsgLog ("Validate Replacement GOP for ConsoleOut Handle:\n");
    #endif

    OrigGop = NULL;
    Status = gBS->HandleProtocol (
        gST->ConsoleOutHandle,
        &gEfiGraphicsOutputProtocolGuid,
        (VOID **) &OrigGop
    );

    if (EFI_ERROR (Status)) {
        Status = gBS->LocateProtocol (
            &gEfiGraphicsOutputProtocolGuid,
            NULL,
            (VOID **) &Gop
        );
    }
    else {
        if (OrigGop->Mode->MaxMode > 0) {
            #if REFIT_DEBUG > 0
            MsgLog ("  - Valid GOP Already Exists on ConsoleOut Handle\n\n");
            #endif

            GraphicsOutput = OrigGop;

            // Restore Protocol and Return
            gBS->HandleProtocol = daOrigProtocol;
            return EFI_ALREADY_STARTED;
        }

        Status = gBS->LocateHandleBuffer (
            ByProtocol,
            &gEfiGraphicsOutputProtocolGuid,
            NULL,
            &HandleCount,
            &HandleBuffer
        );

        #if REFIT_DEBUG > 0
        MsgLog ("  - Seeking Firmware GOP Handles ...%r", Status);
        #endif

        if (EFI_ERROR (Status)) {
            #if REFIT_DEBUG > 0
            MsgLog ("\n");
            #endif

            // Restore Protocol and Return
            gBS->HandleProtocol = daOrigProtocol;
            return EFI_NOT_FOUND;
        }

        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;

        UINT32  Width = 0;
        UINT32  Height = 0;
        UINT32  MaxMode;
        UINT32  Mode;
        UINTN   SizeOfInfo;
        BOOLEAN OurValidGOP;

        Status = EFI_NOT_FOUND;
        for (Index = 0; Index < HandleCount; ++Index) {
            OurValidGOP = FALSE;
            if (HandleBuffer[Index] != gST->ConsoleOutHandle) {
                Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiGraphicsOutputProtocolGuid,
                    (VOID **) &Gop
                );

                if (!EFI_ERROR (Status)) {
                    #if REFIT_DEBUG > 0
                    MsgLog ("\n");
                    MsgLog ("  - Found Candidate Replacement GOP on Firmware Handle[%02d]\n", Index);
                    #endif

                    #if REFIT_DEBUG > 0
                    MsgLog ("    * Evaluating Candidate\n");
                    #endif

                    MaxMode  = Gop->Mode->MaxMode;
                    Width    = 0;
                    Height   = 0;

                    for (Mode = 0; Mode < MaxMode; Mode++) {
                        Status = Gop->QueryMode(Gop, Mode, &SizeOfInfo, &Info);
                        if (!EFI_ERROR (Status)) {
                            if (Width > Info->HorizontalResolution) {
                                continue;
                            }
                            if (Height > Info->VerticalResolution) {
                                continue;
                            }
                            Width = Info->HorizontalResolution;
                            Height = Info->VerticalResolution;
                        }
                    } // for

                    if (Width == 0 || Height == 0) {
                        #if REFIT_DEBUG > 0
                        MsgLog("    ** Invalid Candidate : Width = %d, Height = %d", Width, Height);
                        #endif
                    }
                    else {
                        #if REFIT_DEBUG > 0
                        MsgLog("    ** Valid Candidate : Width = %d, Height = %d", Width, Height);
                        #endif

                        OurValidGOP = TRUE;

                        break;
                    } // if Width == 0 || Height == 0
                } // if !EFI_ERROR (Status)
            } // if HandleBuffer[Index]
        } // for

        FreePool (HandleBuffer);

        #if REFIT_DEBUG > 0
        MsgLog ("\n\n");
        #endif

        if (OurValidGOP == FALSE || EFI_ERROR (Status)) {
            #if REFIT_DEBUG > 0
            MsgLog ("INFO: Could not Find Valid Replacement GOP\n\n");
            #endif

            // Restore Protocol and Return
            gBS->HandleProtocol = daOrigProtocol;
            return EFI_UNSUPPORTED;
        }
    } // if !EFI_ERROR (Status)

    // Restore Protocol and Return
    gBS->HandleProtocol = daOrigProtocol;
    return EFI_SUCCESS;
}

EFI_STATUS
egDumpGOPVideoModes(
    VOID
) {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;

    EFI_STATUS Status;
    UINT32       MaxMode;
    UINT32       Mode;
    UINT32       ModeLog;
    UINT32       NumModes;
    UINT32       ModeCount;
    UINT32       LoopCount;
    UINTN        SizeOfInfo;
    CHAR16       *PixelFormatDesc;
    BOOLEAN      OurValidGOP = FALSE;
    const CHAR16 *ShowScreenStr = NULL;

    if (GraphicsOutput == NULL) {
        SwitchToText(FALSE);

        ShowScreenStr = L"Unsupported EFI";

        gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
        PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
        gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);

        #if REFIT_DEBUG > 0
        MsgLog("%s\n---------------\n\n", ShowScreenStr);
        #endif

        HaltForKey();

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
        MsgLog(
            "Query GOP Modes (Modes=%d, FrameBufferBase=%lx, FrameBufferSize=0x%x):\n",
            ModeCount,
            GraphicsOutput->Mode->FrameBufferBase,
            GraphicsOutput->Mode->FrameBufferSize
        );
        #endif

        for (Mode = 0; Mode < NumModes; Mode++) {
            LoopCount++;
            if (LoopCount == NumModes) {
                break;
            }

            Status = refit_call4_wrapper(GraphicsOutput->QueryMode, GraphicsOutput, Mode, &SizeOfInfo, &Info);

            #if REFIT_DEBUG > 0
            MsgLog("  - Mode[%02d] ...%r", Mode, Status);
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
                    MsgLog(
                        " @ %5dx%-5d (%5d Pixels Per Scanned Line, %s Pixel Format )\n",
                        Info->HorizontalResolution,
                        Info->VerticalResolution,
                        Info->PixelsPerScanLine,
                        PixelFormatDesc
                    );
                }
                else {
                    MsgLog(
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
                MsgLog("  - Mode[%d]: %r", ModeLog, Status);
                if (LoopCount < ModeCount) {
                    if (Mode > 99) {
                        MsgLog( ". NB: Real Mode = %d\n", Mode);
                    }
                    else {
                        MsgLog( "\n", Mode);
                    }
                }
                else {
                    if (Mode > 99) {
                        MsgLog( ". NB: Real Mode = %d\n\n", Mode);
                    }
                    else {
                        MsgLog( "\n\n", Mode);
                    }
                }
                #endif
            } // if Status == EFI_SUCCESS
        } // for (Mode = 0; Mode < NumModes; Mode++)
    } // if MaxMode > 0

    if (OurValidGOP == FALSE) {
        #if REFIT_DEBUG > 0
        MsgLog ("INFO: Could not Find Valid GOP\n\n");
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
GopSetModeAndReconnectTextOut(
    IN UINT32 ModeNumber
) {
    EFI_STATUS   Status;
    const CHAR16 *ShowScreenStr = NULL;

    if (GraphicsOutput == NULL) {
        SwitchToText(FALSE);

        ShowScreenStr = L"Unsupported EFI";

        gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
        PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
        gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);

        #if REFIT_DEBUG > 0
        MsgLog("%s\n---------------\n\n", ShowScreenStr);
        #endif

        HaltForKey();

        return EFI_UNSUPPORTED;
    }

    Status = refit_call2_wrapper(GraphicsOutput->SetMode, GraphicsOutput, ModeNumber);

    #if REFIT_DEBUG > 0
    MsgLog("  - Switch to GOP Mode[%d] ...%r\n", ModeNumber, Status);
    #endif

    return Status;
}

EFI_STATUS
egSetGOPMode(
    INT32 Next
) {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;

    EFI_STATUS   Status = EFI_UNSUPPORTED;
    UINT32       MaxMode;
    UINTN        SizeOfInfo;
    INT32        Mode;
    UINT32       i = 0;
    const CHAR16 *ShowScreenStr = NULL;

    #if REFIT_DEBUG > 0
    MsgLog("Set GOP Mode:\n");
    #endif

    if (GraphicsOutput == NULL) {

        SwitchToText(FALSE);

        ShowScreenStr = L"Unsupported EFI";

        gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
        PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
        gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);

        #if REFIT_DEBUG > 0
        MsgLog("%s\n---------------\n\n", ShowScreenStr);
        #endif

        HaltForKey();
        SwitchToGraphics();

        return EFI_UNSUPPORTED;
    }

    MaxMode = GraphicsOutput->Mode->MaxMode;
    Mode = GraphicsOutput->Mode->Mode;


    if (MaxMode < 1) {
        Status = EFI_UNSUPPORTED;

        SwitchToText(FALSE);

        ShowScreenStr = L"  - Incompatible GPU";

        gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
        PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
        gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);

        #if REFIT_DEBUG > 0
        MsgLog("%s\n---------------\n\n", ShowScreenStr);
        #endif

        HaltForKey();
    }
    else {
        while (EFI_ERROR(Status) && i <= MaxMode) {
            Mode = Mode + Next;
            Mode = (Mode >= (INT32)MaxMode)?0:Mode;
            Mode = (Mode < 0)?((INT32)MaxMode - 1):Mode;
            Status = refit_call4_wrapper(GraphicsOutput->QueryMode, GraphicsOutput, (UINT32)Mode, &SizeOfInfo, &Info);

            #if REFIT_DEBUG > 0
            MsgLog("  - Mode[%02d] ...%r\n", Mode, Status);
            #endif

            if (!EFI_ERROR (Status)) {
                Status = GopSetModeAndReconnectTextOut((UINT32)Mode);

                if (!EFI_ERROR (Status)) {
                    egScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
                    egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
                }
            }

            i++;
        }
    }

    return Status;
}

EFI_STATUS
egSetMaxResolution(
    VOID
) {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;

    EFI_STATUS   Status = EFI_UNSUPPORTED;
    UINT32       Width = 0;
    UINT32       Height = 0;
    UINT32       BestMode = 0;
    UINT32       MaxMode;
    UINT32       Mode;
    UINTN        SizeOfInfo;
    const CHAR16 *ShowScreenStr = NULL;

  if (GraphicsOutput == NULL) {
      SwitchToText(FALSE);

      ShowScreenStr = L"Unsupported EFI";

      gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
      PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
      gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);

      #if REFIT_DEBUG > 0
      MsgLog("%s\n---------------\n\n", ShowScreenStr);
      #endif

      HaltForKey();

      return EFI_UNSUPPORTED;
  }

  #if REFIT_DEBUG > 0
  MsgLog("Set Screen Resolution:\n");
  #endif

  MaxMode = GraphicsOutput->Mode->MaxMode;
  for (Mode = 0; Mode < MaxMode; Mode++) {
    Status = refit_call4_wrapper(GraphicsOutput->QueryMode, GraphicsOutput, Mode, &SizeOfInfo, &Info);
    if (!EFI_ERROR (Status)) {
      if (Width > Info->HorizontalResolution) {
        continue;
      }
      if (Height > Info->VerticalResolution) {
        continue;
      }
      Width = Info->HorizontalResolution;
      Height = Info->VerticalResolution;
      BestMode = Mode;
    }
  }

  #if REFIT_DEBUG > 0
  MsgLog("  - BestMode: GOP Mode[%d] @ %dx%d\n", BestMode, Width, Height);
  #endif

  // check if requested mode is equal to current mode
  if (BestMode == GraphicsOutput->Mode->Mode) {

      #if REFIT_DEBUG > 0
      MsgLog("Screen Resolution Already Set\n\n");
      #endif
      egScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
      egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
      Status = EFI_SUCCESS;
  }
  else {
    Status = GopSetModeAndReconnectTextOut(BestMode);
    if (!EFI_ERROR (Status)) {
      egScreenWidth = Width;
      egScreenHeight = Height;

      #if REFIT_DEBUG > 0
      MsgLog("Screen Resolution Set\n\n", Status);
      #endif

    }
    else {
      // we can not set BestMode - search for first one that we can
      SwitchToText(FALSE);

      ShowScreenStr = L"Could not Set BestMode ...Seek Useable Mode";

      PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

      #if REFIT_DEBUG > 0
      MsgLog("%s\n", ShowScreenStr);
      #endif

      PauseForKey();

      Status = egSetGOPMode(1);

      #if REFIT_DEBUG > 0
      MsgLog("  - Mode Seek ...%r\n\n", Status);
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
egDetermineScreenSize(
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
        egScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
        egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
        egHasGraphics = TRUE;
    }
    else if (UGADraw != NULL) {
        Status = refit_call5_wrapper (
            UGADraw->GetMode,
            UGADraw,
            &ScreenW,
            &ScreenH,
            &UGADepth,
            &UGARefreshRate
        );
        if (EFI_ERROR(Status)) {
            UGADraw = NULL;   // graphics not available
        }
        else {
            egScreenWidth  = ScreenW;
            egScreenHeight = ScreenH;
            egHasGraphics = TRUE;
        }
    }
} // static VOID egDetermineScreenSize()

VOID
egGetScreenSize(
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
egInitScreen(
    VOID
) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL  *OldGOP = NULL;
    EFI_STATUS                    Status = EFI_SUCCESS;
    EFI_STATUS                    XFlag;
    UINTN                         HandleCount;
    EFI_HANDLE                    *HandleBuffer;
    UINTN                         i;
    BOOLEAN                       thisValidGOP = FALSE;


    #if REFIT_DEBUG > 0
            MsgLog("Check for Graphics:\n");
    #endif

    // Get ConsoleControl Protocol
    ConsoleControl = NULL;

    #if REFIT_DEBUG > 0
    MsgLog("  - Seek Console Control\n");
    #endif

    // Check ConsoleOut Handle
    Status = gBS->HandleProtocol(
        gST->ConsoleOutHandle,
        &ConsoleControlProtocolGuid,
        (VOID **) &ConsoleControl
    );

    #if REFIT_DEBUG > 0
    MsgLog("    * Seek on ConsoleOut Handle ...%r\n", Status);
    #endif

    if (EFI_ERROR(Status) || ConsoleControl == NULL) {
        // Try Locating by Handle
        Status = gBS->LocateHandleBuffer(
            ByProtocol,
            &ConsoleControlProtocolGuid,
            NULL,
            &HandleCount,
            &HandleBuffer
        );

        #if REFIT_DEBUG > 0
        MsgLog("    * Seek on Handle Buffer ...%r\n", Status);
        #endif

        if (!EFI_ERROR (Status)) {
            i = 0;
            for (i = 0; i < HandleCount; i++) {
                Status = gBS->HandleProtocol(
                    HandleBuffer[i],
                    &ConsoleControlProtocolGuid,
                    (VOID*) &ConsoleControl
                );

                #if REFIT_DEBUG > 0
                MsgLog("    ** Evaluate on Handle[%02d] ...%r\n", i, Status);
                #endif

                if (!EFI_ERROR (Status)) {
                    break;
                }
            }
            FreePool(HandleBuffer);
        }
        else {
            // try locating directly
            Status = gBS->LocateProtocol(
                &ConsoleControlProtocolGuid,
                NULL,
                (VOID*) &ConsoleControl
            );

            #if REFIT_DEBUG > 0
            MsgLog("    * Seek Directly ...%r\n", Status);
            #endif
        }
    }

    #if REFIT_DEBUG > 0
    if (EFI_ERROR(Status)) {
        MsgLog("  - Assess Console Control ...NOT OK!\n\n");
    }
    else {
        MsgLog("  - Assess Console Control ...ok\n\n");
    }
    #endif


    // Get UGADraw Protocol
    UGADraw = NULL;

    #if REFIT_DEBUG > 0
    MsgLog("  - Seek Universal Graphics Adapter\n");
    #endif

    // Check ConsoleOut Handle
    Status = gBS->HandleProtocol(
        gST->ConsoleOutHandle,
        &UgaDrawProtocolGuid,
        (VOID **) &UGADraw
    );

    #if REFIT_DEBUG > 0
    MsgLog("    * Seek on ConsoleOut Handle ...%r\n", Status);
    #endif

    if (EFI_ERROR(Status) || ConsoleControl == NULL) {
        // Try Locating by Handle
        Status = gBS->LocateHandleBuffer(
            ByProtocol,
            &UgaDrawProtocolGuid,
            NULL,
            &HandleCount,
            &HandleBuffer
        );

        #if REFIT_DEBUG > 0
        MsgLog("    * Seek on Handle Buffer ...%r\n", Status);
        #endif

        if (!EFI_ERROR (Status)) {
            i = 0;
            for (i = 0; i < HandleCount; i++) {
                Status = gBS->HandleProtocol(
                    HandleBuffer[i],
                    &UgaDrawProtocolGuid,
                    (VOID*) &UGADraw
                );

                #if REFIT_DEBUG > 0
                MsgLog("    ** Evaluate on Handle[%d] ...%r\n", i, Status);
                #endif

                if (!EFI_ERROR (Status)) {
                    break;
                }
            }
            FreePool(HandleBuffer);
        }
        else {
            // try locating directly
            Status = gBS->LocateProtocol(
                &UgaDrawProtocolGuid,
                NULL,
                (VOID*) &UGADraw
            );

            #if REFIT_DEBUG > 0
            MsgLog("    * Seek Directly ...%r\n", Status);
            #endif
        }
    }

    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
    	MsgLog("  - Assess Universal Graphics Adapter ...NOT OK!\n\n");
        #endif
    }
    else {
        #if REFIT_DEBUG > 0
    	MsgLog("  - Assess Universal Graphics Adapter ...ok\n\n");
        #endif
    }

    // Get GraphicsOutput Protocol
    GraphicsOutput = NULL;

    #if REFIT_DEBUG > 0
    MsgLog("  - Seek Graphics Output Protocol\n");
    #endif

    // Check ConsoleOut Handle
    Status = gBS->HandleProtocol(
        gST->ConsoleOutHandle,
        &GraphicsOutputProtocolGuid,
        (VOID **) &OldGOP
    );

    #if REFIT_DEBUG > 0
    MsgLog("    * Seek on ConsoleOut Handle ...%r\n", Status);
    #endif

    if (EFI_ERROR(Status) || ConsoleControl == NULL) {
        // Try Locating by Handle
        Status = gBS->LocateHandleBuffer(
            ByProtocol,
            &GraphicsOutputProtocolGuid,
            NULL,
            &HandleCount,
            &HandleBuffer
        );

        #if REFIT_DEBUG > 0
        MsgLog("    * Seek on Handle Buffer ...%r\n", Status);
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
                Status = gBS->HandleProtocol(
                    HandleBuffer[i],
                    &GraphicsOutputProtocolGuid,
                    (VOID*) &OldGOP
                );

                #if REFIT_DEBUG > 0
                MsgLog("    ** Evaluate on Handle[%02d] ...%r\n", i, Status);
                #endif

                if (!EFI_ERROR (Status)) {
                    TmpGOP = OldGOP;
                    MaxMode = TmpGOP->Mode->MaxMode;
                    for (GOPMode = 0; GOPMode < MaxMode; GOPMode++) {
                        Status = TmpGOP->QueryMode(TmpGOP, GOPMode, &SizeOfInfo, &Info);
                        if (!EFI_ERROR (Status)) {
                            if (GOPWidth < Info->HorizontalResolution) {
                                if (GOPHeight < Info->VerticalResolution) {
                                    OldGOP = TmpGOP;
                                    GOPWidth = Info->HorizontalResolution;
                                    GOPHeight = Info->VerticalResolution;

                                    #if REFIT_DEBUG > 0
                                    MsgLog(
                                        "    *** Select Handle[%02d][%02d] @ %dx%d\n",
                                        i,
                                        GOPMode,
                                        GOPWidth,
                                        GOPHeight
                                    );
                                    #endif
                                }
                                else {
                                    #if REFIT_DEBUG > 0
                                    MsgLog(
                                        "        Ignore Handle[%02d][%02d] @ %dx%d\n",
                                        i,
                                        GOPMode,
                                        Info->HorizontalResolution,
                                        Info->VerticalResolution
                                    );
                                    #endif
                                }
                            }
                            else {
                                #if REFIT_DEBUG > 0
                                MsgLog(
                                    "        Ignore Handle[%02d][%02d] @ %dx%d\n",
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
            FreePool(HandleBuffer);
        }
        else {
            // try locating directly
            Status = gBS->LocateProtocol(
                &GraphicsOutputProtocolGuid,
                NULL,
                (VOID*) &OldGOP
            );

            #if REFIT_DEBUG > 0
            MsgLog("    * Seek Directly ...%r\n", Status);
            #endif

            if (EFI_ERROR (Status)) {
                // Force to NOT FOUND on Error as subsequent code relies on this
                Status = EFI_NOT_FOUND;
            }
        }
    }

    XFlag = EFI_UNSUPPORTED;

    if (Status == EFI_NOT_FOUND) {
        XFlag = EFI_NOT_FOUND;

        // Not Found
        #if REFIT_DEBUG > 0
    	MsgLog("  - Assess Graphics Output Protocol ...NOT FOUND!\n\n");
        #endif
    }

    if (EFI_ERROR(Status) && XFlag == EFI_UNSUPPORTED) {
        XFlag = EFI_NOT_FOUND;

        // Not Found
        #if REFIT_DEBUG > 0
        MsgLog("  - Assess Graphics Output Protocol ...ERROR!\n\n");
        #endif
    }
    else if (!EFI_ERROR(Status) && XFlag != EFI_ALREADY_STARTED) {
        if (OldGOP->Mode->MaxMode > 0) {
            XFlag = EFI_SUCCESS;
            thisValidGOP = TRUE;

            // Set GOP to OldGOP
            GraphicsOutput = OldGOP;

            #if REFIT_DEBUG > 0
            MsgLog("  - Assess Graphics Output Protocol ...ok\n\n");
            #endif
        }
        else {
            XFlag = EFI_UNSUPPORTED;

            #if REFIT_DEBUG > 0
            MsgLog("  - Assess Graphics Output Protocol ...NOT OK!\n\n");
            #endif

            if (GlobalConfig.ProvideConsoleGOP) {
                Status = daCheckAltGop();

                if (!EFI_ERROR(Status)) {
                    Status = OcProvideConsoleGop(TRUE);

                    if (!EFI_ERROR (Status)) {
                        Status = gBS->HandleProtocol(
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
            XFlag = EFI_LOAD_ERROR;
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
                Status = OcUseDirectGop(-1);
            }

            if (!EFI_ERROR(Status)) {
                // Check ConsoleOut Handle
                Status = gBS->HandleProtocol(
                    gST->ConsoleOutHandle,
                    &GraphicsOutputProtocolGuid,
                    (VOID **) &OldGOP
                );
                if (EFI_ERROR(Status)) {
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
        MsgLog ("INFO: Cannot Implement Graphics Output Protocol\n\n");
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
            MsgLog("INFO: Invalid GOP Instance\n\n");
            #endif

            GraphicsOutput = NULL;
        }
        else {
            egSetMaxResolution();
            egScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
            egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
            egHasGraphics = TRUE;

            #if REFIT_DEBUG > 0
            // Only log this if GOPFix or Direct Renderer attempted
            if (XFlag == EFI_UNSUPPORTED || XFlag == EFI_ALREADY_STARTED) {
                MsgLog("INFO: Implemented Graphics Output Protocol\n\n");
            }
            #endif
        }
    }
    if (UGADraw != NULL) {
        if (GlobalConfig.UgaPassThrough && thisValidGOP == TRUE) {
            // Run OcProvideUgaPassThrough from OpenCorePkg
            Status = OcProvideUgaPassThrough();

            #if REFIT_DEBUG > 0
            MsgLog ("INFO: Implement UGA Pass Through ...%r\n\n", Status);
            #endif
        }

        if (GraphicsOutput == NULL) {
            UINT32 Width, Height, Depth, RefreshRate;
            Status = UGADraw->GetMode(UGADraw, &Width, &Height, &Depth, &RefreshRate);
            if (EFI_ERROR(Status)) {
                // Graphics not available
                UGADraw = NULL;
                GlobalConfig.TextOnly = TRUE;

                #if REFIT_DEBUG > 0
                MsgLog("INFO: Pre-Boot Graphics not Available\n");
                MsgLog("      Attempting Text Mode\n\n");
                #endif
            }
            else {
                #if REFIT_DEBUG > 0
                MsgLog("INFO: GOP not Available\n");
                MsgLog("      Fallback on UGADraw\n\n");
                #endif

                //egScreenWidth  = Width;
                //egScreenHeight = Height;
                // To trigeer UGADraw, set Width to 0 and Height to a value
                UINTN   TmpWidth  = 0;
                UINTN   TmpHeight = Height;

                Status = egSetScreenSize((UINTN *) TmpWidth, (UINTN *) TmpHeight);
                if (!EFI_ERROR (Status)) {
                    egHasGraphics = TRUE;
                }
            }
        }
    }

    if (GlobalConfig.TextRenderer || GlobalConfig.TextOnly) {
        // Implement Text Renderer
        Status = OcUseBuiltinTextOutput();

        #if REFIT_DEBUG > 0
        MsgLog ("INFO: Implement Text Renderer ...%r\n\n", Status);
        #endif
    }
}

// Convert a graphics mode (in *ModeWidth) to a width and height (returned in
// *ModeWidth and *Height, respectively).
// Returns TRUE if successful, FALSE if not (invalid mode, typically)
BOOLEAN
egGetResFromMode(
    UINTN *ModeWidth,
    UINTN *Height
) {
   UINTN                                 Size;
   EFI_STATUS                            Status;
   EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info = NULL;

   if ((ModeWidth != NULL) && (Height != NULL)) {
      Status = refit_call4_wrapper(
          GraphicsOutput->QueryMode,
          GraphicsOutput,
          *ModeWidth,
          &Size,
          &Info
      );
      if (!EFI_ERROR (Status) && (Info != NULL)) {
         *ModeWidth = Info->HorizontalResolution;
         *Height = Info->VerticalResolution;
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
// resolution, even 0x0 or ridiculously large values.
// Upon success, returns actual screen resolution in *ScreenWidth and *ScreenHeight.
// These values are unchanged upon failure.
// Returns TRUE if successful, FALSE if not.
BOOLEAN
egSetScreenSize(
    IN OUT UINTN *ScreenWidth,
    IN OUT UINTN *ScreenHeight
) {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;

    EFI_STATUS   Status = EFI_SUCCESS;
    BOOLEAN      ModeSet = FALSE;
    UINTN        Size;
    UINT32       ModeNum = 0;
    UINT32       CurrentModeNum;
    UINT32       ScreenW;
    UINT32       ScreenH;
    UINT32       UGADepth;
    UINT32       UGARefreshRate;
    CHAR16       TmpShowScreenStr[128];
    const CHAR16 *ShowScreenStr = NULL;

    if (*ScreenWidth == 0 && ScreenHeight > 0) {
        #if REFIT_DEBUG > 0
        MsgLog("Activate UGADraw:\n");
        #endif
    }
    else {
        if ((ScreenWidth == NULL) || (ScreenHeight == NULL)) {
            SwitchToText(FALSE);

            ShowScreenStr = L"INFO: Invalid Input Resolution";

            gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
            PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
            gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);


            #if REFIT_DEBUG > 0
            MsgLog("%s\n\n", ShowScreenStr);
            #endif

            PauseForKey();

            return FALSE;
        }

        #if REFIT_DEBUG > 0
        MsgLog("Set Screen Size to %dx%d:\n", ScreenWidth, ScreenHeight);
        #endif
    }


    if (GraphicsOutput != NULL && *ScreenWidth != 0) { // GOP mode (UEFI)
        CurrentModeNum = GraphicsOutput->Mode->Mode;

        #if REFIT_DEBUG > 0
        MsgLog("  - GraphicsOutput Object Found ...Current Mode = %d\n", CurrentModeNum);
        #endif

        if (*ScreenHeight == 0) { // User specified a mode number (stored in *ScreenWidth); use it directly
            ModeNum = (UINT32) *ScreenWidth;
            if (ModeNum != CurrentModeNum) {
                #if REFIT_DEBUG > 0
                MsgLog("  - Mode Set from ScreenWidth\n\n");
                #endif

                ModeSet = TRUE;
            }
            else if (egGetResFromMode(ScreenWidth, ScreenHeight) &&
                (refit_call2_wrapper(
                    GraphicsOutput->SetMode,
                    GraphicsOutput,
                    ModeNum
                ) == EFI_SUCCESS)
            ) {
                #if REFIT_DEBUG > 0
                MsgLog("  - Mode Set from egGetResFromMode\n\n");
                #endif

                ModeSet = TRUE;
            }
            else {
                #if REFIT_DEBUG > 0
                MsgLog("  - Could not Set GraphicsOutput Mode\n\n");
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
                if (!EFI_ERROR (Status)
                    && (Size >= sizeof(*Info)
                    && (Info != NULL))
                    && (Info->HorizontalResolution == *ScreenWidth)
                    && (Info->VerticalResolution == *ScreenHeight)
                    && ((ModeNum == CurrentModeNum)
                    || (refit_call2_wrapper(GraphicsOutput->SetMode, GraphicsOutput, ModeNum) == EFI_SUCCESS))
                ) {
                    #if REFIT_DEBUG > 0
                    MsgLog("  - Mode Set from GraphicsOutput Query\n\n");
                    #endif

                    ModeSet = TRUE;
                }
                else {
                    #if REFIT_DEBUG > 0
                    MsgLog("  - Could not Set GraphicsOutput Mode\n\n");
                    #endif
                }
            } while ((++ModeNum < GraphicsOutput->Mode->MaxMode) && !ModeSet);
        } // if/else

        if (ModeSet) {
            egScreenWidth = *ScreenWidth;
            egScreenHeight = *ScreenHeight;
        }
        else { // If unsuccessful, display an error message for the user....
            SwitchToText(FALSE);

            ShowScreenStr = L"Invalid Resolution Setting Provided ...Trying Default Modes:";
            PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

            #if REFIT_DEBUG > 0
            MsgLog("%s", ShowScreenStr);
            #endif

            ModeNum = 0;
            do {

                #if REFIT_DEBUG > 0
                MsgLog("\n");
                #endif

                Status = refit_call4_wrapper(
                    GraphicsOutput->QueryMode,
                    GraphicsOutput,
                    ModeNum,
                    &Size,
                    &Info
                );
                if (!EFI_ERROR (Status) && (Info != NULL)) {
                    SPrint(TmpShowScreenStr, sizeof(TmpShowScreenStr),
                        L"Available Mode: Mode[%02d][%dx%d]",
                        ModeNum,
                        Info->HorizontalResolution,
                        Info->VerticalResolution
                    );
                    ShowScreenStr = TmpShowScreenStr;
                    PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

                    #if REFIT_DEBUG > 0
                    MsgLog("  - %s", ShowScreenStr);
                    #endif

                    if (ModeNum == CurrentModeNum) {
                        egScreenWidth = Info->HorizontalResolution;
                        egScreenHeight = Info->VerticalResolution;
                    } // if

                }
                else {
                    ShowScreenStr = L"Error : Could not Query GraphicsOutput Mode";
                    PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

                    #if REFIT_DEBUG > 0
                    MsgLog("  - %s", ShowScreenStr);
                    #endif

                } // if
            } while (++ModeNum < GraphicsOutput->Mode->MaxMode);

            #if REFIT_DEBUG > 0
            MsgLog("\n\n");
            #endif

            PauseForKey();
            SwitchToGraphicsAndClear();
        } // if GOP mode (UEFI)
    }
    else {
        if (UGADraw == NULL) {
            #if REFIT_DEBUG > 0
            MsgLog("  - UGADraw not available\n\n");
            #endif
        }
        else {
            // Try to use current color depth & refresh rate for new mode. Maybe not the best choice
            // in all cases, but I don't know how to probe for alternatives....
            Status = UGADraw->GetMode(
                UGADraw,
                &ScreenW,
                &ScreenH,
                &UGADepth,
                &UGARefreshRate
            );

            #if REFIT_DEBUG > 0
            MsgLog("  - Get UGADraw ...%r\n", Status);
            #endif

            if (!EFI_ERROR (Status)) {
                Status = UGADraw->SetMode(
                    UGADraw,
                    ScreenW,
                    ScreenH,
                    UGADepth,
                    UGARefreshRate
                );

                *ScreenWidth = (UINTN) ScreenW;
                *ScreenHeight = (UINTN) ScreenH;
                egScreenWidth = *ScreenWidth;
                egScreenHeight = *ScreenHeight;

                #if REFIT_DEBUG > 0
                MsgLog("  - Set UGADraw ...%r", Status);
                #endif

                if (EFI_ERROR (Status)) {
                    #if REFIT_DEBUG > 0
                    MsgLog("\n\n");
                    #endif
                }
                else {
                    ModeSet = TRUE;

                    #if REFIT_DEBUG > 0
                    MsgLog("\n");
                    MsgLog("    * Screen Width: %d\n", egScreenWidth);
                    MsgLog("    * Screen Height: %d\n", egScreenHeight);
                    MsgLog("    * Colour Depth: %d\n", UGADepth);
                    MsgLog("    * Refresh Rate: %d\n\n", UGARefreshRate);
                    #endif
                }
            }
            else {
                // TODO: Find a list of supported modes and display it.
                // NOTE: Below doesn't actually appear unless we explicitly switch to text mode.
                // This is just a placeholder until something better can be done....
                SPrint(TmpShowScreenStr, sizeof(TmpShowScreenStr),
                    L"Error Setting %dx%d Resolution ...Unsupported Mode",
                    *ScreenWidth,
                    *ScreenHeight
                );
                ShowScreenStr = TmpShowScreenStr;
            } // if/else
        } // if (UGADraw == NULL)
    } // if (GraphicsOutput != NULL && *ScreenWidth != 0)

    return (ModeSet);
} // BOOLEAN egSetScreenSize()

// Set a text mode.
// Returns TRUE if the mode actually changed, FALSE otherwise.
// Note that a FALSE return value can mean either an error or no change
// necessary.
BOOLEAN
egSetTextMode(
    UINT32 RequestedMode
) {
    EFI_STATUS   Status;
    BOOLEAN      ChangedIt = FALSE;
    UINTN        i = 0;
    UINTN        Width;
    UINTN        Height;
    CHAR16       TmpShowScreenStr[128];
    const CHAR16 *ShowScreenStr = NULL;

    if ((RequestedMode != DONT_CHANGE_TEXT_MODE) && (RequestedMode != gST->ConOut->Mode->Mode)) {
        Status = gST->ConOut->SetMode(gST->ConOut, RequestedMode);
        if (!EFI_ERROR (Status)) {
            ChangedIt = TRUE;
        }
        else {
            SwitchToText(FALSE);

            ShowScreenStr = L"Error Setting Resolution ...Unsupported Mode";
            PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

            #if REFIT_DEBUG > 0
            MsgLog("%s\n", ShowScreenStr);
            #endif

            ShowScreenStr = L"Seek Available Modes:";
            PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

            #if REFIT_DEBUG > 0
            MsgLog("%s\n", ShowScreenStr);
            #endif

            do {
                Status = gST->ConOut->QueryMode(gST->ConOut, i, &Width, &Height);

                if (!EFI_ERROR (Status)) {
                    SPrint(TmpShowScreenStr, sizeof(TmpShowScreenStr),
                        L"  - Mode[%d] (%dx%d)",
                        i,
                        Width,
                        Height
                    );
                    ShowScreenStr = TmpShowScreenStr;
                    PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

                    #if REFIT_DEBUG > 0
                    MsgLog("%s\n", ShowScreenStr);
                    #endif
                }

            } while (++i < gST->ConOut->Mode->MaxMode);

            SPrint(TmpShowScreenStr, sizeof(TmpShowScreenStr),
                L"Use Default Mode[%d]:",
                DONT_CHANGE_TEXT_MODE
            );
            ShowScreenStr = TmpShowScreenStr;
            PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

            #if REFIT_DEBUG > 0
            MsgLog("%s\n", ShowScreenStr);
            #endif

            PauseForKey();
            SwitchToGraphicsAndClear();
        } // if/else successful change
    } // if need to change mode

    return ChangedIt;
} // BOOLEAN egSetTextMode()

CHAR16 *
egScreenDescription(
    VOID
) {
    CHAR16       *GraphicsInfo;
    CHAR16       *TextInfo = NULL;
    const CHAR16 *ShowScreenStr = NULL;

    GraphicsInfo = AllocateZeroPool(256 * sizeof(CHAR16));
    if (GraphicsInfo == NULL) {
        SwitchToText(FALSE);

        ShowScreenStr = L"Memory Allocation Error!";

        gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
        PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
        gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);

        #if REFIT_DEBUG > 0
        MsgLog("%s\n\n", ShowScreenStr);
        #endif

        PauseForKey();
        SwitchToGraphics();

        return L"Memory Allocation Error";
    }

    if (egHasGraphics) {
        if (GraphicsOutput != NULL) {
            SPrint(GraphicsInfo, 255, L"Graphics Output Protocol (UEFI), %dx%d", egScreenWidth, egScreenHeight);
        }
        else if (UGADraw != NULL) {
            SPrint(GraphicsInfo, 255, L"Universal Graphics Adapter (EFI 1.10), %dx%d", egScreenWidth, egScreenHeight);
        }
        else {
            MyFreePool(GraphicsInfo);
            MyFreePool(TextInfo);

            SwitchToText(FALSE);

            ShowScreenStr = L"Internal Error!";

            gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
            PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
            gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);

            #if REFIT_DEBUG > 0
            MsgLog("%s\n\n", ShowScreenStr);
            #endif

            PauseForKey();
            SwitchToGraphics();

            return L"Internal Error";
        }

        if (!AllowGraphicsMode) { // graphics-capable HW, but in text mode
            TextInfo = AllocateZeroPool(256 * sizeof(CHAR16));
            SPrint(TextInfo, 255, L"(Text Mode: %dx%d [Graphics Capable])", ConWidth, ConHeight);
            MergeStrings(&GraphicsInfo, TextInfo, L' ');
        }
    }
    else {
        SPrint(GraphicsInfo, 255, L"Text-Foo Console: %dx%d", ConWidth, ConHeight);
    }
    MyFreePool(TextInfo);

    return GraphicsInfo;
}

BOOLEAN
egHasGraphicsMode(
    VOID
) {
    return egHasGraphics;
}

BOOLEAN
egIsGraphicsModeEnabled(
    VOID
) {
    EFI_CONSOLE_CONTROL_SCREEN_MODE CurrentMode;

    if (ConsoleControl != NULL) {
        refit_call4_wrapper(ConsoleControl->GetMode, ConsoleControl, &CurrentMode, NULL, NULL);

        return (CurrentMode == EfiConsoleControlScreenGraphics) ? TRUE : FALSE;
    }

    return FALSE;
}

VOID
egSetGraphicsModeEnabled(
    IN BOOLEAN Enable
) {
    EFI_CONSOLE_CONTROL_SCREEN_MODE CurrentMode;
    EFI_CONSOLE_CONTROL_SCREEN_MODE NewMode;

    if (ConsoleControl != NULL) {
        refit_call4_wrapper(ConsoleControl->GetMode, ConsoleControl, &CurrentMode, NULL, NULL);

        NewMode = Enable ? EfiConsoleControlScreenGraphics
                         : EfiConsoleControlScreenText;
        if (CurrentMode != NewMode) {
            refit_call2_wrapper(ConsoleControl->SetMode, ConsoleControl, NewMode);
        }
    }
}

//
// Drawing to the screen
//

VOID
egClearScreen(
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
            GraphicsOutput->Blt,
            GraphicsOutput,
            (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)&FillColor,
             EfiBltVideoFill,
             0,
             0,
             0,
             0,
             egScreenWidth,
             egScreenHeight,
             0
         );
    }
    else if (UGADraw != NULL) {
        refit_call10_wrapper(
            UGADraw->Blt,
            UGADraw,
            &FillColor,
            EfiUgaVideoFill,
            0,
            0,
            0,
            0,
            egScreenWidth,
            egScreenHeight,
            0
        );
    }
}

VOID
egDrawImage(
    IN EG_IMAGE *Image,
    IN UINTN    ScreenPosX,
    IN UINTN    ScreenPosY
) {
    EG_IMAGE *CompImage = NULL;

    // NOTE: Weird seemingly redundant tests because some placement code can "wrap around" and
    // send "negative" values, which of course become very large unsigned ints that can then
    // wrap around AGAIN if values are added to them.....
    if (!egHasGraphics || ((ScreenPosX + Image->Width) > egScreenWidth) || ((ScreenPosY + Image->Height) > egScreenHeight) ||
        (ScreenPosX > egScreenWidth) || (ScreenPosY > egScreenHeight))
        return;

    if ((GlobalConfig.ScreenBackground == NULL)
        || ((Image->Width == egScreenWidth) && (Image->Height == egScreenHeight))
    ) {
       CompImage = Image;
    }
    else if (GlobalConfig.ScreenBackground == Image) {
       CompImage = GlobalConfig.ScreenBackground;
    }
    else {
       CompImage = egCropImage(
           GlobalConfig.ScreenBackground,
           ScreenPosX,
           ScreenPosY,
           Image->Width,
           Image->Height
       );
       if (CompImage == NULL) {
           #if REFIT_DEBUG > 0
          MsgLog("Error! Cannot crop image in egDrawImage()!\n");
          #endif

          return;
       }
       egComposeImage(CompImage, Image, 0, 0);
    }

    if (GraphicsOutput != NULL) {
        refit_call10_wrapper(
            GraphicsOutput->Blt,
            GraphicsOutput,
            (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)CompImage->PixelData,
            EfiBltBufferToVideo,
            0,
            0,
            ScreenPosX,
            ScreenPosY,
            CompImage->Width,
            CompImage->Height,
            0
        );
    }
    else if (UGADraw != NULL) {
        refit_call10_wrapper(
            UGADraw->Blt,
            UGADraw,
            (EFI_UGA_PIXEL *)CompImage->PixelData,
            EfiUgaBltBufferToVideo,
            0,
            0,
            ScreenPosX,
            ScreenPosY,
            CompImage->Width,
            CompImage->Height,
            0
        );
    }
    if ((CompImage != GlobalConfig.ScreenBackground) && (CompImage != Image)) {
        egFreeImage(CompImage);
    }
} /* VOID egDrawImage() */

// Display an unselected icon on the screen, so that the background image shows
// through the transparency areas. The BadgeImage may be NULL, in which case
// it's not composited in.
VOID
egDrawImageWithTransparency(
    EG_IMAGE *Image,
    EG_IMAGE *BadgeImage,
    UINTN    XPos,
    UINTN    YPos,
    UINTN    Width,
    UINTN    Height
) {
   EG_IMAGE *Background;

   Background = egCropImage(GlobalConfig.ScreenBackground, XPos, YPos, Width, Height);
   if (Background != NULL) {
      BltImageCompositeBadge(Background, Image, BadgeImage, XPos, YPos);
      egFreeImage(Background);
   }
} // VOID DrawImageWithTransparency()

VOID
egDrawImageArea(
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

    egRestrictImageArea(Image, AreaPosX, AreaPosY, &AreaWidth, &AreaHeight);
    if (AreaWidth == 0)
        return;

    if (GraphicsOutput != NULL) {
        refit_call10_wrapper(
            GraphicsOutput->Blt,
            GraphicsOutput,
            (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)Image->PixelData,
            EfiBltBufferToVideo,
            AreaPosX,
            AreaPosY,
            ScreenPosX,
            ScreenPosY,
            AreaWidth,
            AreaHeight,
            Image->Width * 4
        );
    }
    else if (UGADraw != NULL) {
        refit_call10_wrapper(
            UGADraw->Blt,
            UGADraw,
            (EFI_UGA_PIXEL *)Image->PixelData,
            EfiUgaBltBufferToVideo,
            AreaPosX,
            AreaPosY,
            ScreenPosX,
            ScreenPosY,
            AreaWidth,
            AreaHeight,
            Image->Width * 4
        );
    }
}

// Display a message in the center of the screen, surrounded by a box of the
// specified color. For the moment, uses graphics calls only. (It still works
// in text mode on GOP/UEFI systems, but not on UGA/EFI 1.x systems.)
VOID
egDisplayMessage(
    IN CHAR16 *Text,
    EG_PIXEL  *BGColor,
    UINTN     PositionCode
) {
   UINTN BoxWidth, BoxHeight;
   static UINTN Position = 1;
   EG_IMAGE *Box;

   if ((Text != NULL) && (BGColor != NULL)) {
      egMeasureText(Text, &BoxWidth, &BoxHeight);
      BoxWidth += 14;
      BoxHeight *= 2;
      if (BoxWidth > egScreenWidth)
         BoxWidth = egScreenWidth;
      Box = egCreateFilledImage(BoxWidth, BoxHeight, FALSE, BGColor);
      egRenderText(Text, Box, 7, BoxHeight / 4, (BGColor->r + BGColor->g + BGColor->b) / 3);
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
      egDrawImage(Box, (egScreenWidth - BoxWidth) / 2, Position);
      if ((PositionCode == CENTER) || (Position >= egScreenHeight - (BoxHeight * 5)))
          Position = 1;
   } // if non-NULL inputs
} // VOID egDisplayMessage()

// Copy the current contents of the display into an EG_IMAGE....
// Returns pointer if successful, NULL if not.
EG_IMAGE * egCopyScreen(VOID) {
   return egCopyScreenArea(0, 0, egScreenWidth, egScreenHeight);
} // EG_IMAGE * egCopyScreen()

// Copy the current contents of the specified display area into an EG_IMAGE....
// Returns pointer if successful, NULL if not.
EG_IMAGE * egCopyScreenArea(UINTN XPos, UINTN YPos, UINTN Width, UINTN Height) {
   EG_IMAGE *Image = NULL;

   if (!egHasGraphics)
      return NULL;

   // allocate a buffer for the screen area
   Image = egCreateImage(Width, Height, FALSE);
   if (Image == NULL) {
      return NULL;
   }

   // get full screen image
   if (GraphicsOutput != NULL) {
       refit_call10_wrapper(
           GraphicsOutput->Blt,
           GraphicsOutput,
           (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)Image->PixelData,
           EfiBltVideoToBltBuffer,
           XPos,
           YPos,
           0,
           0,
           Image->Width,
           Image->Height,
           0
       );
   }
   else if (UGADraw != NULL) {
       refit_call10_wrapper(
           UGADraw->Blt,
           UGADraw,
           (EFI_UGA_PIXEL *)Image->PixelData,
           EfiUgaVideoToBltBuffer,
           XPos,
           YPos,
           0,
           0,
           Image->Width,
           Image->Height,
           0
       );
   }

   return Image;
} // EG_IMAGE * egCopyScreenArea()

//
// Make a screenshot
//

VOID
egScreenShot(
    VOID
) {
    EFI_STATUS   Status;
    EFI_FILE     *BaseDir;
    EG_IMAGE     *Image;
    UINT8        *FileData;
    UINTN        FileDataLength;
    UINTN        i = 0;
    UINTN        ssNum;
    CHAR16       Filename[80];
    const CHAR16 *ShowScreenStr = NULL;

    Image = egCopyScreen();
    if (Image == NULL) {
        SwitchToText(FALSE);

        ShowScreenStr = L"Error: Unable to Take Screen Shot";

        gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
        PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
        gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);

        #if REFIT_DEBUG > 0
        MsgLog("%s\n\n", ShowScreenStr);
        #endif

        PauseForKey();
        SwitchToGraphics();

       goto bailout_wait;
    }

    // encode as BMP
    egEncodeBMP(Image, &FileData, &FileDataLength);
    egFreeImage(Image);
    if (FileData == NULL) {
        SwitchToText(FALSE);

        ShowScreenStr = L"Error: Could not encode BMP";

        gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
        PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
        gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);

        #if REFIT_DEBUG > 0
        MsgLog("%s\n\n", ShowScreenStr);
        #endif

        HaltForKey();
        SwitchToGraphics();

        goto bailout_wait;
    }

    Status = egFindESP(&BaseDir);
    if (EFI_ERROR(Status))
        return;

    // Search for existing screen shot files; increment number to an unused value...
    ssNum = 001;
    do {
       SPrint(Filename, 80, L"screenshot_%03d.bmp", ssNum++);
    } while (FileExists(BaseDir, Filename));

    // save to file on the ESP
    Status = egSaveFile(BaseDir, Filename, FileData, FileDataLength);
    FreePool(FileData);
    if (CheckError(Status, L"in egSaveFile()")) {
        goto bailout_wait;
    }

    return;

    // DEBUG: switch to text mode
bailout_wait:
    egSetGraphicsModeEnabled(FALSE);
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &i);
}

/* EOF */
