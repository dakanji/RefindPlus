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
CHAR16 *ShowScreenStr = NULL;

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
    } else if (CompareGuid (&gEfiUgaDrawProtocolGuid, Protocol)) {
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
    MsgLog ("Provide GOP on ConsoleOutHandle:\n");
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
    } else {
        if (OrigGop->Mode->MaxMode > 0) {
            #if REFIT_DEBUG > 0
            MsgLog ("  - Valid GOP Exists on ConsoleOutHandle ...Abort\n\n");
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
        MsgLog ("  - Seeking GOP Handles ...%r", Status);
        #endif

        if (EFI_ERROR (Status)) {
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
                    MsgLog ("  - Found Candidate Replacement GOP on GPU Handle[%d]\n", Index);
                    #endif

                    #if REFIT_DEBUG > 0
                    MsgLog ("    * Evaluating Candidate\n");
                    #endif

                    MaxMode = Gop->Mode->MaxMode;
                    Width   = 0;
                    Height  = 0;

                    for (Mode = 0; Mode < MaxMode; Mode++) {
                        Status = Gop->QueryMode(Gop, Mode, &SizeOfInfo, &Info);
                        if (Status == EFI_SUCCESS) {
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
                        MsgLog("    ** Invalid Candidate\n");
                        #endif
                    } else {
                        #if REFIT_DEBUG > 0
                        MsgLog("    ** Valid Candidate\n");
                        #endif

                        OurValidGOP = TRUE;

                        break;
                    } // if Width == 0 || Height == 0
                } // if !EFI_ERROR (Status)
            } // if HandleBuffer[Index]

            if (OurValidGOP = TRUE) {
                break;
            }
        } // for

        FreePool (HandleBuffer);

        if (OurValidGOP == FALSE || EFI_ERROR (Status)) {
            #if REFIT_DEBUG > 0
            MsgLog ("\n\n");
            MsgLog ("    * Could not Find Valid Replacement GOP\n");
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
    UINT32     MaxMode;
    UINT32     Mode;
    UINT32     ModeLog;
    UINT32     NumModes;
    UINT32     ModeCount;
    UINT32     LoopCount;
    UINTN      SizeOfInfo;
    CHAR16     *PixelFormatDesc;
    BOOLEAN    OurValidGOP = FALSE;

    if (GraphicsOutput == NULL) {
        SwitchToText(FALSE);

        SPrint(ShowScreenStr, 160, (CHAR16 *) "Unsupported EFI");

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
        } else {
            ModeCount = MaxMode;
        }
        LoopCount = 0;

        #if REFIT_DEBUG > 0
        MsgLog("Query GraphicsOutputProtocol Modes:\n");
        MsgLog(
            "Modes = %d, Framebuffer Base = %lx, Framebuffer Size = 0x%x\n",
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

            Status = GraphicsOutput->QueryMode(GraphicsOutput, Mode, &SizeOfInfo, &Info);

            #if REFIT_DEBUG > 0
            MsgLog("  - Query GOP Mode[%02d] ...%r", Mode, Status);
            #endif

            if (Status == EFI_SUCCESS) {
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
                } else {
                    MsgLog(
                        " @ %5dx%-5d (%5d Pixels Per Scanned Line, %s Pixel Format )\n\n",
                        Info->HorizontalResolution,
                        Info->VerticalResolution,
                        Info->PixelsPerScanLine,
                        PixelFormatDesc
                    );
                }
                #endif
            } else {
                // Limit logged value to 99
                if (Mode > 99) {
                    ModeLog = 99;
                } else {
                    ModeLog = Mode;
                }

                #if REFIT_DEBUG > 0
                MsgLog("  - Mode[%d]: %r", ModeLog, Status);
                if (LoopCount < ModeCount) {
                    if (Mode > 99) {
                        MsgLog( ". NB: Real Mode = %d\n", Mode);
                    } else {
                        MsgLog( "\n", Mode);
                    }
                } else {
                    if (Mode > 99) {
                        MsgLog( ". NB: Real Mode = %d\n\n", Mode);
                    } else {
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
    EFI_STATUS  Status;

    if (GraphicsOutput == NULL) {
        SwitchToText(FALSE);

        SPrint(ShowScreenStr, 160, (CHAR16 *) "Unsupported EFI");

        gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
        PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
        gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);

        #if REFIT_DEBUG > 0
        MsgLog("%s\n---------------\n\n", ShowScreenStr);
        #endif

        HaltForKey();
        return EFI_UNSUPPORTED;
    }

    Status = GraphicsOutput->SetMode(GraphicsOutput, ModeNumber);

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

    EFI_STATUS Status = EFI_UNSUPPORTED;
    UINT32     MaxMode;
    UINTN      SizeOfInfo;
    INT32      Mode;
    UINT32     i = 0;

    #if REFIT_DEBUG > 0
    MsgLog("Set GOP Mode:\n");
    #endif

    if (GraphicsOutput == NULL) {

        SwitchToText(FALSE);

        SPrint(ShowScreenStr, 160, (CHAR16 *) "Unsupported EFI");

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

        SPrint(ShowScreenStr, 160, (CHAR16 *) "  - Incompartible GPU");

        gST->ConOut->SetAttribute(gST->ConOut, ATTR_ERROR);
        PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);
        gST->ConOut->SetAttribute(gST->ConOut, ATTR_BASIC);

        #if REFIT_DEBUG > 0
        MsgLog("%s\n---------------\n\n", ShowScreenStr);
        #endif

        HaltForKey();
    } else {
        while (EFI_ERROR(Status) && i <= MaxMode) {
            Mode = Mode + Next;
            Mode = (Mode >= (INT32)MaxMode)?0:Mode;
            Mode = (Mode < 0)?((INT32)MaxMode - 1):Mode;
            Status = GraphicsOutput->QueryMode(GraphicsOutput, (UINT32)Mode, &SizeOfInfo, &Info);

            #if REFIT_DEBUG > 0
            MsgLog("  - Query GOP Mode[%02d] ...%r\n", Mode, Status);
            #endif

            if (Status == EFI_SUCCESS) {
                Status = GopSetModeAndReconnectTextOut((UINT32)Mode);

                if (Status == EFI_SUCCESS) {
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

    EFI_STATUS Status = EFI_UNSUPPORTED;
    UINT32     Width = 0;
    UINT32     Height = 0;
    UINT32     BestMode = 0;
    UINT32     MaxMode;
    UINT32     Mode;
    UINTN      SizeOfInfo;

  if (GraphicsOutput == NULL) {
      SwitchToText(FALSE);

      SPrint(ShowScreenStr, 160, (CHAR16 *) "Unsupported EFI");

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
    Status = GraphicsOutput->QueryMode(GraphicsOutput, Mode, &SizeOfInfo, &Info);
    if (Status == EFI_SUCCESS) {
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
  } else {
    Status = GopSetModeAndReconnectTextOut(BestMode);
    if (Status == EFI_SUCCESS) {
      egScreenWidth = Width;
      egScreenHeight = Height;

      #if REFIT_DEBUG > 0
      MsgLog("Screen Resolution Set\n\n", Status);
      #endif

    } else {
      // we can not set BestMode - search for first one that we can
      SwitchToText(FALSE);

      SPrint(ShowScreenStr, 160, (CHAR16 *) "Could not Set BestMode ...Seek Useable Mode");

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
    UINT32     UGAWidth;
    UINT32     UGAHeight;
    UINT32     UGADepth;
    UINT32     UGARefreshRate;

    // get screen size
    egHasGraphics = FALSE;
    if (GraphicsOutput != NULL) {
        egScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
        egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
        egHasGraphics = TRUE;
    } else if (UGADraw != NULL) {
        Status = UGADraw->GetMode(
            UGADraw,
            &UGAWidth,
            &UGAHeight,
            &UGADepth,
            &UGARefreshRate
        );
        if (EFI_ERROR(Status)) {
            UGADraw = NULL;   // graphics not available
        } else {
            egScreenWidth  = UGAWidth;
            egScreenHeight = UGAHeight;
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

    if (ScreenWidth != NULL)
        *ScreenWidth = egScreenWidth;
    if (ScreenHeight != NULL)
        *ScreenHeight = egScreenHeight;
}


VOID
egInitScreen(
    VOID
) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL  *OldGOP = NULL;
    EFI_STATUS                    Status = EFI_SUCCESS;
    EFI_STATUS                    UGAOnConsole = EFI_UNSUPPORTED;
    EFI_STATUS                    XStatus;
    EFI_STATUS                    XFlag;
    UINTN                         HandleCount;
    EFI_HANDLE                    *HandleBuffer;
    UINTN                         i;
    BOOLEAN                       thisValidGOP;


    #if REFIT_DEBUG > 0
            MsgLog("Check for Graphics:\n");
    #endif

    // Get ConsoleControl Protocol
    ConsoleControl = NULL;

    #if REFIT_DEBUG > 0
    MsgLog("  - Seek ConsoleControl\n");
    #endif

    // Check ConsoleOutHandle
    Status = gBS->HandleProtocol(
        gST->ConsoleOutHandle,
        &ConsoleControlProtocolGuid,
        (VOID **) &ConsoleControl
    );

    #if REFIT_DEBUG > 0
    MsgLog("    * Seek on ConsoleOutHandle ...%r\n", Status);
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
        if (!EFI_ERROR (Status)) {
            i = 0;
            for (i = 0; i < HandleCount; i++) {
                Status = gBS->HandleProtocol(
                    HandleBuffer[i],
                    &ConsoleControlProtocolGuid,
                    (VOID*) &ConsoleControl
                );

                #if REFIT_DEBUG > 0
                MsgLog("    * Seek on Handle[%02d] ...%r\n", i, Status);
                #endif

                if (!EFI_ERROR (Status)) {
                    break;
                }
            }
            FreePool(HandleBuffer);
        } else {
            #if REFIT_DEBUG > 0
            MsgLog("    * Seek on HandleBuffer ...%r\n", Status);
            #endif

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
        MsgLog("  - Check ConsoleControl ...NOT OK!\n\n");
    } else {
        MsgLog("  - Check ConsoleControl ...ok\n\n");
    }
    #endif


    // Get UGADraw Protocol
    UGADraw = NULL;

    #if REFIT_DEBUG > 0
    MsgLog("  - Seek UGADraw\n");
    #endif

    // Check ConsoleOutHandle
    Status = gBS->HandleProtocol(
        gST->ConsoleOutHandle,
        &UgaDrawProtocolGuid,
        (VOID **) &UGADraw
    );

    #if REFIT_DEBUG > 0
    MsgLog("    * Seek on ConsoleOutHandle ...%r\n", Status);
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
        if (!EFI_ERROR (Status)) {
            i = 0;
            for (i = 0; i < HandleCount; i++) {
                Status = gBS->HandleProtocol(
                    HandleBuffer[i],
                    &UgaDrawProtocolGuid,
                    (VOID*) &UGADraw
                );

                #if REFIT_DEBUG > 0
                MsgLog("    * Seek on HandleBuffer[%d] ...%r\n", i, Status);
                #endif

                if (!EFI_ERROR (Status)) {
                    break;
                }
            }
            FreePool(HandleBuffer);
        } else {
            #if REFIT_DEBUG > 0
            MsgLog("    * Seek on HandleBuffer ...%r\n", Status);
            #endif

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
    } else {
        UGAOnConsole = EFI_SUCCESS;
    }

    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
    	MsgLog("  - Check UGADraw ...NOT OK!\n\n");
        #endif
    } else {
        #if REFIT_DEBUG > 0
    	MsgLog("  - Check UGADraw ...ok\n\n");
        #endif
    }

    // Get GraphicsOutput Protocol
    GraphicsOutput = NULL;

    #if REFIT_DEBUG > 0
    MsgLog("  - Seek GraphicsOutput\n");
    #endif

    // Check ConsoleOutHandle
    Status = gBS->HandleProtocol(
        gST->ConsoleOutHandle,
        &GraphicsOutputProtocolGuid,
        (VOID **) &OldGOP
    );

    #if REFIT_DEBUG > 0
    MsgLog("    * Seek on ConsoleOutHandle ...%r\n", Status);
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
                MsgLog("    * Seek on Handle[%02d] ...%r\n", i, Status);
                #endif

                if (!EFI_ERROR (Status)) {
                    TmpGOP = OldGOP;
                    MaxMode = TmpGOP->Mode->MaxMode;
                    for (GOPMode = 0; GOPMode < MaxMode; GOPMode++) {
                        Status = TmpGOP->QueryMode(TmpGOP, GOPMode, &SizeOfInfo, &Info);
                        if (Status == EFI_SUCCESS) {
                            if (GOPWidth < Info->HorizontalResolution) {
                                if (GOPHeight < Info->VerticalResolution) {
                                    OldGOP = TmpGOP;
                                    GOPWidth = Info->HorizontalResolution;
                                    GOPHeight = Info->VerticalResolution;

                                    #if REFIT_DEBUG > 0
                                    MsgLog(
                                        "    ** Set to Handle[%02d][%02d] @ %dx%d\n",
                                        i,
                                        GOPMode,
                                        GOPWidth,
                                        GOPHeight
                                    );
                                    #endif
                                } else {
                                    #if REFIT_DEBUG > 0
                                    MsgLog(
                                        "       Ignore Handle[%02d][%02d] @ %dx%d\n",
                                        i,
                                        GOPMode,
                                        Info->HorizontalResolution,
                                        Info->VerticalResolution
                                    );
                                    #endif
                                }
                            } else {
                                #if REFIT_DEBUG > 0
                                MsgLog(
                                    "       Ignore Handle[%02d][%02d] @ %dx%d\n",
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
        } else {
            #if REFIT_DEBUG > 0
            MsgLog("    * Seek on HandleBuffer ...%r\n", Status);
            #endif

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
    	MsgLog("  - Check GraphicsOutput ...NOT FOUND!\n\n");
        #endif

        if (GlobalConfig.UseDirectGop) {
            Status = OcUseDirectGop (-1);

            if (!EFI_ERROR(Status)) {
                // Check ConsoleOutHandle
                Status = gBS->HandleProtocol(
                    gST->ConsoleOutHandle,
                    &GraphicsOutputProtocolGuid,
                    (VOID **) &OldGOP
                );
                if (!EFI_ERROR(Status)) {
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

    if (EFI_ERROR(Status) && XFlag == EFI_UNSUPPORTED) {
        XFlag = EFI_NOT_FOUND;

        // Not Found
        #if REFIT_DEBUG > 0
        MsgLog("  - Check GraphicsOutput ...ERROR!\n\n");
        #endif
    } else if (!EFI_ERROR(Status) && XFlag != EFI_ALREADY_STARTED) {
        if (OldGOP->Mode->MaxMode > 0) {
            XFlag = EFI_SUCCESS;

            // Set GOP to OldGOP
            GraphicsOutput = OldGOP;

            #if REFIT_DEBUG > 0
            MsgLog("  - Check GraphicsOutput ...ok\n\n");
            #endif
        } else {
            XFlag = EFI_UNSUPPORTED;

            #if REFIT_DEBUG > 0
            MsgLog("  - Check GraphicsOutput ...NOT OK!\n\n");
            #endif

            if (GlobalConfig.ProvideConsoleGOP) {
                Status = daCheckAltGop();

                if (!EFI_ERROR(Status)) {
                    OcProvideConsoleGop(TRUE);

                    Status = gBS->LocateHandleBuffer (
                        ByProtocol,
                        &GraphicsOutputProtocolGuid,
                        NULL,
                        &HandleCount,
                        &HandleBuffer
                    );
                    if (!EFI_ERROR (Status)) {
                        Status = EFI_UNSUPPORTED;
                        for (i = 0; i < HandleCount; ++i) {
                            if (HandleBuffer[i] == gST->ConsoleOutHandle) {
                                Status = gBS->HandleProtocol (
                                    HandleBuffer[i],
                                    &GraphicsOutputProtocolGuid,
                                    (VOID **) &GraphicsOutput
                                );

                                break;
                            }
                        }
                        FreePool(HandleBuffer);
                    }
                }
            }
        }
    }

    if (XFlag == EFI_NOT_FOUND) {
        #if REFIT_DEBUG > 0
        MsgLog ("INFO: Cannot Implement GraphicsOutputProtocol\n\n");
        #endif
    } else if (XFlag == EFI_UNSUPPORTED) {
        #if REFIT_DEBUG > 0
        MsgLog ("INFO: Provide GOP on ConsoleOutHandle ...%r\n\n", Status);
        #endif

        if (EFI_ERROR (Status)) {
            thisValidGOP = FALSE;
        } else {
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

            // Revert Console GOP Provision if Invalid
            if (XFlag == EFI_UNSUPPORTED && thisValidGOP == TRUE && OldGOP != NULL) {
                XStatus = gBS->UninstallProtocolInterface (
                    gST->ConsoleOutHandle,
                    &gEfiGraphicsOutputProtocolGuid,
                    GraphicsOutput
                );

                if (!EFI_ERROR (XStatus)) {
                    XStatus = gBS->InstallMultipleProtocolInterfaces (
                        &gST->ConsoleOutHandle,
                        &gEfiGraphicsOutputProtocolGuid,
                        OldGOP,
                        NULL
                    );
                }
            }

            GraphicsOutput = NULL;
        } else {
            egSetMaxResolution();
            egScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
            egScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
            egHasGraphics = TRUE;

            #if REFIT_DEBUG > 0
            // Only log this if GOPFix or Direct Renderer attempted
            if (XFlag == EFI_UNSUPPORTED || XFlag == EFI_ALREADY_STARTED) {
                MsgLog("INFO: Implemented GraphicsOutputProtocol\n\n");
            }
            #endif
        }
    }
    if (UGADraw != NULL && UGAOnConsole != EFI_SUCCESS) {
        if (GlobalConfig.UgaPassThrough) {
            #if REFIT_DEBUG > 0
            MsgLog ("Implement UniversalGraphicsAdapterProtocol Pass Through:\n");
            #endif

            // Run OcProvideUgaPassThrough from OpenCorePkg
            Status = OcProvideUgaPassThrough();

            if (EFI_ERROR (Status)) {
                Status = EFI_UNSUPPORTED;

                #if REFIT_DEBUG > 0
                MsgLog("  - %r: Could not Activate UGADraw on ConsoleOutHandle\n", Status);
                #endif
            } else {
                #if REFIT_DEBUG > 0
                MsgLog("  - %r: Activated UGADraw on ConsoleOutHandle\n", Status);
                #endif

                egHasGraphics = TRUE;
            }

            #if REFIT_DEBUG > 0
            MsgLog ("Implement UGAPassThrough ...%r\n\n", Status);
            #endif
        }
    }

    if (GraphicsOutput == NULL && UGADraw != NULL) {
        UINT32 Width, Height, Depth, RefreshRate;
        Status = UGADraw->GetMode(UGADraw, &Width, &Height, &Depth, &RefreshRate);
        if (EFI_ERROR(Status)) {
            // Graphics not available
            UGADraw = NULL;
            GlobalConfig.TextOnly = TRUE;

            #if REFIT_DEBUG > 0
            MsgLog("INFO: Graphics Not Available\n");
            MsgLog("      Switched to Text Mode\n\n");
            #endif

        } else {
            #if REFIT_DEBUG > 0
            MsgLog("INFO: GraphicsOutputProtocol Not Available\n");
            MsgLog("      Falling back to UGADraw\n\n");
            #endif

            egScreenWidth  = Width;
            egScreenHeight = Height;
            egHasGraphics = TRUE;
        }
    }

    if (GlobalConfig.TextRenderer || GlobalConfig.TextOnly) {
        // Implement Text Renderer
        OcUseBuiltinTextOutput();

        #if REFIT_DEBUG > 0
        MsgLog ("INFO: Initiated Builtin Text Renderer\n\n");
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
      Status = GraphicsOutput->QueryMode(
          GraphicsOutput,
          *ModeWidth,
          &Size,
          &Info
      );
      if ((Status == EFI_SUCCESS) && (Info != NULL)) {
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

    EFI_STATUS Status = EFI_SUCCESS;
    BOOLEAN    ModeSet = FALSE;
    UINTN      Size;
    UINT32     ModeNum = 0, CurrentModeNum;
    UINT32     UGAWidth;
    UINT32     UGAHeight;
    UINT32     UGADepth;
    UINT32     UGARefreshRate;

    #if REFIT_DEBUG > 0
    MsgLog("Set Screen Size Manually. H = %d and W = %d\n", ScreenHeight, ScreenWidth);
    #endif

    if ((ScreenWidth == NULL) || (ScreenHeight == NULL)) {

        #if REFIT_DEBUG > 0
        MsgLog("Invalid Resolution Input!\n");
        #endif

        return FALSE;
    }

    if (GraphicsOutput != NULL) { // GOP mode (UEFI)
        CurrentModeNum = GraphicsOutput->Mode->Mode;

        #if REFIT_DEBUG > 0
        MsgLog("GraphicsOutput Object Found ...Current Mode = %d\n", CurrentModeNum);
        #endif

        if (*ScreenHeight == 0) { // User specified a mode number (stored in *ScreenWidth); use it directly
            ModeNum = (UINT32) *ScreenWidth;
            if (ModeNum != CurrentModeNum) {

                #if REFIT_DEBUG > 0
                MsgLog("Mode Set from ScreenWidth\n");
                #endif

                ModeSet = TRUE;
            } else if (egGetResFromMode(ScreenWidth, ScreenHeight)
                && (GraphicsOutput->SetMode(
                    GraphicsOutput,
                    ModeNum
                ) == EFI_SUCCESS)
            ) {

                #if REFIT_DEBUG > 0
                MsgLog("Mode Set from egGetResFromMode\n");
                #endif

                ModeSet = TRUE;

                #if REFIT_DEBUG > 0
            } else {
                MsgLog("Could not Set GraphicsOutput Mode\n");
                #endif

            }
            // User specified width & height; must find mode...
        } else {
            // Do a loop through the modes to see if the specified one is available;
            // and if so, switch to it....
            do {
                Status = GraphicsOutput->QueryMode(
                    GraphicsOutput,
                    ModeNum,
                    &Size,
                    &Info
                );
                if ((Status == EFI_SUCCESS)
                    && (Size >= sizeof(*Info)
                    && (Info != NULL))
                    && (Info->HorizontalResolution == *ScreenWidth)
                    && (Info->VerticalResolution == *ScreenHeight)
                    && ((ModeNum == CurrentModeNum)
                    || (GraphicsOutput->SetMode(GraphicsOutput, ModeNum) == EFI_SUCCESS))
                ) {

                    #if REFIT_DEBUG > 0
                    MsgLog("Mode Set from GraphicsOutput Query\n");
                    #endif

                    ModeSet = TRUE;

                    #if REFIT_DEBUG > 0
                } else {
                    MsgLog("Could not Set GraphicsOutput Mode\n");
                    #endif

                }
            } while ((++ModeNum < GraphicsOutput->Mode->MaxMode) && !ModeSet);
        } // if/else

        if (ModeSet) {
            egScreenWidth = *ScreenWidth;
            egScreenHeight = *ScreenHeight;
        } else { // If unsuccessful, display an error message for the user....
            SwitchToText(FALSE);

            SPrint(ShowScreenStr, 160,
                (CHAR16 *) "Invalid Resolution Setting Provided ...Trying Default Modes:"
            );
            PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

            #if REFIT_DEBUG > 0
            MsgLog("%s\n", ShowScreenStr);
            #endif

            ModeNum = 0;
            do {
                Status = GraphicsOutput->QueryMode(
                    GraphicsOutput,
                    ModeNum,
                    &Size,
                    &Info
                );
                if ((Status == EFI_SUCCESS) && (Info != NULL)) {
                    SPrint(ShowScreenStr, 160,
                        (CHAR16 *) "  - Available Mode: Mode[%d] (%dx%d)",
                        ModeNum,
                        Info->HorizontalResolution,
                        Info->VerticalResolution
                    );
                    PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

                    #if REFIT_DEBUG > 0
                    MsgLog("%s\n", ShowScreenStr);
                    #endif

                    if (ModeNum == CurrentModeNum) {
                        egScreenWidth = Info->HorizontalResolution;
                        egScreenHeight = Info->VerticalResolution;
                    } // if

                } else {
                    SPrint(ShowScreenStr, 160,
                        (CHAR16 *) "  - Error : Could not Query GraphicsOutput Mode!",
                        *ScreenWidth,
                        *ScreenHeight
                    );
                    PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

                    #if REFIT_DEBUG > 0
                    MsgLog("%s\n", ShowScreenStr);
                    #endif

                } // if
            } while (++ModeNum < GraphicsOutput->Mode->MaxMode);

            PauseForKey();
            SwitchToGraphicsAndClear();
        } // if GOP mode (UEFI)
    } else if (UGADraw != NULL) { // UGA mode (EFI 1.x)
        // Try to use current color depth & refresh rate for new mode. Maybe not the best choice
        // in all cases, but I don't know how to probe for alternatives....
        Status = UGADraw->GetMode(
            UGADraw,
            &UGAWidth,
            &UGAHeight,
            &UGADepth,
            &UGARefreshRate
        );
        Status = UGADraw->SetMode(
            UGADraw,
            *ScreenWidth,
            *ScreenHeight,
            UGADepth,
            UGARefreshRate
        );
        if (Status == EFI_SUCCESS) {
            egScreenWidth = *ScreenWidth;
            egScreenHeight = *ScreenHeight;
            ModeSet = TRUE;
        } else {
            // TODO: Find a list of supported modes and display it.
            // NOTE: Below doesn't actually appear unless we explicitly switch to text mode.
            // This is just a placeholder until something better can be done....
            SPrint(ShowScreenStr, 160,
                (CHAR16 *) "Error setting %dx%d resolution ...Unsupported Mode",
                *ScreenWidth,
                *ScreenHeight
            );
            PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

            #if REFIT_DEBUG > 0
            MsgLog("%s\n", ShowScreenStr);
            #endif

        } // if/else
    } // if/else if (UGADraw != NULL)
    MyFreePool(ShowScreenStr);

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
    EFI_STATUS Status;
    BOOLEAN    ChangedIt = FALSE;
    UINTN      i = 0;
    UINTN      Width;
    UINTN      Height;

    if ((RequestedMode != DONT_CHANGE_TEXT_MODE) && (RequestedMode != gST->ConOut->Mode->Mode)) {
        Status = gST->ConOut->SetMode(gST->ConOut, RequestedMode);
        if (Status == EFI_SUCCESS) {
            ChangedIt = TRUE;
        } else {
            SwitchToText(FALSE);

            SPrint(ShowScreenStr, 160, (CHAR16 *) "Error Setting Resolution ...Unsupported Mode");
            PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

            #if REFIT_DEBUG > 0
            MsgLog("%s\n", ShowScreenStr);
            #endif

            SPrint(ShowScreenStr, 160, (CHAR16 *) "Seek Available Modes:");
            PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

            #if REFIT_DEBUG > 0
            MsgLog("%s\n", ShowScreenStr);
            #endif

            do {
                Status = gST->ConOut->QueryMode(gST->ConOut, i, &Width, &Height);

                if (Status == EFI_SUCCESS) {
                    SPrint(ShowScreenStr, 160, (CHAR16 *) "  - Mode[%d] (%dx%d)", i, Width, Height);
                    PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

                    #if REFIT_DEBUG > 0
                    MsgLog("%s\n", ShowScreenStr);
                    #endif
                }

            } while (++i < gST->ConOut->Mode->MaxMode);

            SPrint(ShowScreenStr, 160, (CHAR16 *) "Use Default Mode[%d]:", DONT_CHANGE_TEXT_MODE);
            PrintUglyText((CHAR16 *) ShowScreenStr, NEXTLINE);

            #if REFIT_DEBUG > 0
            MsgLog("%s\n", ShowScreenStr);
            #endif

            MyFreePool(ShowScreenStr);

            PauseForKey();
            SwitchToGraphicsAndClear();
        } // if/else successful change
    } // if need to change mode

    return ChangedIt;
} // BOOLEAN egSetTextMode()

CHAR16 * egScreenDescription(VOID) {
    CHAR16 *GraphicsInfo, *TextInfo = NULL;

    GraphicsInfo = AllocateZeroPool(256 * sizeof(CHAR16));
    if (GraphicsInfo == NULL) {
        SwitchToText(FALSE);

        SPrint(ShowScreenStr, 160, (CHAR16 *) "Memory Allocation Error!");

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
        } else if (UGADraw != NULL) {
            GraphicsInfo = AllocateZeroPool(256 * sizeof(CHAR16));
            SPrint(GraphicsInfo, 255, L"Universal Graphics Adapter (EFI 1.10), %dx%d", egScreenWidth, egScreenHeight);
        } else {
            MyFreePool(GraphicsInfo);
            MyFreePool(TextInfo);

            SwitchToText(FALSE);

            SPrint(ShowScreenStr, 160, (CHAR16 *) "Internal Error!");

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
    } else {
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
        ConsoleControl->GetMode(ConsoleControl, &CurrentMode, NULL, NULL);

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
        ConsoleControl->GetMode(ConsoleControl, &CurrentMode, NULL, NULL);

        NewMode = Enable ? EfiConsoleControlScreenGraphics
                         : EfiConsoleControlScreenText;
        if (CurrentMode != NewMode) {
            ConsoleControl->SetMode(ConsoleControl, NewMode);
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
    } else {
        FillColor.Red   = 0x0;
        FillColor.Green = 0x0;
        FillColor.Blue  = 0x0;
    }
    FillColor.Reserved = 0;

    if (GraphicsOutput != NULL) {
        // EFI_GRAPHICS_OUTPUT_BLT_PIXEL and EFI_UGA_PIXEL have the same
        // layout, and the header from TianoCore actually defines them
        // to be the same type.
        GraphicsOutput->Blt(
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
    } else if (UGADraw != NULL) {
        UGADraw->Blt(
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
    } else if (GlobalConfig.ScreenBackground == Image) {
       CompImage = GlobalConfig.ScreenBackground;
    } else {
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
        GraphicsOutput->Blt(
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
    } else if (UGADraw != NULL) {
        UGADraw->Blt(
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
    if ((CompImage != GlobalConfig.ScreenBackground) && (CompImage != Image))
       egFreeImage(CompImage);
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
        GraphicsOutput->Blt(
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
    } else if (UGADraw != NULL) {
        UGADraw->Blt(UGADraw,
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
       GraphicsOutput->Blt(
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
   } else if (UGADraw != NULL) {
       UGADraw->Blt(
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
    EFI_STATUS Status;
    EFI_FILE   *BaseDir;
    EG_IMAGE   *Image;
    UINT8      *FileData;
    UINTN      FileDataLength;
    UINTN      i = 0;
    UINTN      ssNum;
    CHAR16     Filename[80];

    Image = egCopyScreen();
    if (Image == NULL) {
        SwitchToText(FALSE);

        SPrint(ShowScreenStr, 160, (CHAR16 *) "Error: Unable to Take Screen Shot");

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

        SPrint(ShowScreenStr, 160, (CHAR16 *) "Error: Could not encode BMP");

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
