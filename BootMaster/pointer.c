/*
 * BootMaster/pointer.c
 * Pointer device functions
 *
 * Copyright (c) 2018 CJ Vaughter
 * All rights reserved.
 *
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
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2022 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "pointer.h"
#include "global.h"
#include "screenmgt.h"
#include "icns.h"
#include "rp_funcs.h"
#include "../include/refit_call_wrapper.h"

UINTN                           NumSPointerDevices = 0;
EFI_GUID                        SPointerGuid       = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
EFI_HANDLE                     *HandleS            = NULL;
EFI_SIMPLE_POINTER_PROTOCOL   **ProtocolS          = NULL;
UINTN                           NumAPointerDevices = 0;
EFI_GUID                        APointerGuid       = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;
EFI_HANDLE                     *HandleA            = NULL;
EFI_ABSOLUTE_POINTER_PROTOCOL **ProtocolA          = NULL;

UINTN                           LastXPos           = 0;
UINTN                           LastYPos           = 0;
EG_IMAGE                       *MouseImage         = NULL;
EG_IMAGE                       *Background         = NULL;

BOOLEAN                         PointerAvailable   = FALSE;
POINTER_STATE                   State;


////////////////////////////////////////////////////////////////////////////////
// Initialise Pointer Devices
////////////////////////////////////////////////////////////////////////////////
VOID pdInitialize (VOID) {
    #if REFIT_DEBUG > 0
    LOG_MSG("I N I T I A L I S E   P O I N T E R   D E V I C E S");
    LOG_MSG("\n");
    #endif

    pdCleanup(); // Just in case

    if (!GlobalConfig.EnableMouse && !GlobalConfig.EnableTouch) {
        #if REFIT_DEBUG > 0
        LOG_MSG("%s  - Detected Keyboard-Only Mode", OffsetNext);
        LOG_MSG("\n\n");
        #endif

        // Early Return
        return;
    }

    // Get all handles that support absolute pointer protocol
    // Usually touchscreens but sometimes mice
    UINTN NumPointerHandles = 0;
    EFI_STATUS handlestatus = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, ByProtocol,
        &APointerGuid, NULL,
        &NumPointerHandles, &HandleA
    );

    if (EFI_ERROR(handlestatus)) {
        #if REFIT_DEBUG > 0
        if (GlobalConfig.EnableTouch) {
            LOG_MSG("%s  - Could Not Enable Touch", OffsetNext);
        }
        #endif

        GlobalConfig.EnableTouch = FALSE;
    }
    else {
        ProtocolA = AllocatePool (sizeof (EFI_ABSOLUTE_POINTER_PROTOCOL*) * NumPointerHandles);
        UINTN Index;
        for (Index = 0; Index < NumPointerHandles; Index++) {
            // Open the protocol on the handle
            EFI_STATUS status = REFIT_CALL_6_WRAPPER(
                gBS->OpenProtocol, HandleA[Index],
                &APointerGuid, (VOID **) &ProtocolA[NumAPointerDevices],
                SelfImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
            );
            if (status == EFI_SUCCESS) {
                #if REFIT_DEBUG > 0
                if (GlobalConfig.EnableTouch) {
                    LOG_MSG("%s  - Enabled Touch", OffsetNext);
                }
                #endif

                NumAPointerDevices++;
            }
        }
    }

    // Get all handles that support simple pointer protocol (mice)
    NumPointerHandles = 0;
    handlestatus = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, ByProtocol,
        &SPointerGuid, NULL,
        &NumPointerHandles, &HandleS
    );

    if (EFI_ERROR(handlestatus)) {
        #if REFIT_DEBUG > 0
        if (GlobalConfig.EnableMouse) {
            LOG_MSG("%s  - Could Not Enable Mouse", OffsetNext);
        }
        #endif

        GlobalConfig.EnableMouse = FALSE;
    }
    else {
        ProtocolS = AllocatePool (sizeof (EFI_SIMPLE_POINTER_PROTOCOL*) * NumPointerHandles);
        UINTN Index;
        BOOLEAN GotMouse = FALSE;
        for (Index = 0; Index < NumPointerHandles; Index++) {
            // Open the protocol on the handle
            EFI_STATUS status = REFIT_CALL_6_WRAPPER(
                gBS->OpenProtocol, HandleS[Index],
                &SPointerGuid, (VOID **) &ProtocolS[NumSPointerDevices],
                SelfImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
            );
            if (status == EFI_SUCCESS) {
                if (GlobalConfig.EnableMouse) {
                    GotMouse = TRUE;
                }

                NumSPointerDevices++;
            }
        } // for

        #if REFIT_DEBUG > 0
        if (GotMouse) {
            LOG_MSG("%s  - Enabled Mouse", OffsetNext);
        }
        #endif
    }

    PointerAvailable = (NumAPointerDevices + NumSPointerDevices > 0);

    // Load mouse icon
    if (PointerAvailable && GlobalConfig.EnableMouse) {
        MouseImage = BuiltinIcon (BUILTIN_ICON_MOUSE);
    }

    #if REFIT_DEBUG > 0
    LOG_MSG("\n");
    LOG_MSG("Pointer Devices Initialised");
    LOG_MSG("\n\n");
    #endif
} // VOID pdInitialize()

////////////////////////////////////////////////////////////////////////////////
// Frees allocated memory and closes pointer protocols
////////////////////////////////////////////////////////////////////////////////
VOID pdCleanup (VOID) {
    UINTN Index;

    #if REFIT_DEBUG > 0
    LOG_MSG("Close Existing Pointer Protocols:");
    #endif

    PointerAvailable = FALSE;
    pdClear();

    if (HandleA) {
        for (Index = 0; Index < NumAPointerDevices; Index++) {
            REFIT_CALL_4_WRAPPER(
                gBS->CloseProtocol, HandleA[Index],
                &APointerGuid, SelfImageHandle, NULL
            );
        }
        MY_FREE_POOL(HandleA);
    }

    MY_FREE_POOL(ProtocolA);
    if (HandleS) {
        for (Index = 0; Index < NumSPointerDevices; Index++) {
            REFIT_CALL_4_WRAPPER(
                gBS->CloseProtocol, HandleS[Index],
                &SPointerGuid, SelfImageHandle, NULL
            );
        }
        MY_FREE_POOL(HandleS);
    }

    MY_FREE_POOL(ProtocolS);
    MY_FREE_IMAGE(MouseImage);

    NumAPointerDevices = 0;
    NumSPointerDevices = 0;

    LastXPos = ScreenW / 2;
    LastYPos = ScreenH / 2;

    State.X = ScreenW / 2;
    State.Y = ScreenH / 2;
    State.Press = FALSE;
    State.Holding = FALSE;
} // VOID pdCleanup()

////////////////////////////////////////////////////////////////////////////////
// Returns whether or not any pointer devices are available
////////////////////////////////////////////////////////////////////////////////
BOOLEAN pdAvailable (VOID) {
    return PointerAvailable;
} // BOOLEAN pdAvailable()

////////////////////////////////////////////////////////////////////////////////
// Returns the number of pointer devices available
////////////////////////////////////////////////////////////////////////////////
UINTN pdCount (VOID) {
    return NumAPointerDevices + NumSPointerDevices;
} // UINTN pdCount()

////////////////////////////////////////////////////////////////////////////////
// Returns a pointer device's WaitForInput event
////////////////////////////////////////////////////////////////////////////////
EFI_EVENT pdWaitEvent (
    UINTN Index
) {
    if (!PointerAvailable || Index >= NumAPointerDevices + NumSPointerDevices) {
        return NULL;
    }

    if (Index >= NumAPointerDevices) {
        return ProtocolS[Index - NumAPointerDevices]->WaitForInput;
    }
    return ProtocolA[Index]->WaitForInput;
} // EFI_EVENT pdWaitEvent()

////////////////////////////////////////////////////////////////////////////////
// Gets the current state of all pointer devices and assigns State to
// the first available device's state
////////////////////////////////////////////////////////////////////////////////
EFI_STATUS pdUpdateState (VOID) {
#if defined (EFI32) && defined (__MAKEWITH_GNUEFI)
    return EFI_NOT_READY;
#else
    if (!PointerAvailable) {
        return EFI_NOT_READY;
    }

    EFI_STATUS Status = EFI_NOT_READY;
    EFI_ABSOLUTE_POINTER_STATE APointerState;
    EFI_SIMPLE_POINTER_STATE SPointerState;
    BOOLEAN LastHolding = State.Holding;

    UINTN Index;
    EFI_STATUS PointerStatus;
    for (Index = 0; Index < NumAPointerDevices; Index++) {
        PointerStatus = REFIT_CALL_2_WRAPPER(ProtocolA[Index]->GetState, ProtocolA[Index], &APointerState);

        // if new state found and we have not already found a new state
        if (!EFI_ERROR(PointerStatus) && EFI_ERROR(Status)) {
            Status = EFI_SUCCESS;

#ifdef EFI32
            State.X = (UINTN) DivU64x64Remainder (
                APointerState.CurrentX * ScreenW,
                ProtocolA[Index]->Mode->AbsoluteMaxX,
                NULL
            );
            State.Y = (UINTN) DivU64x64Remainder (
                APointerState.CurrentY * ScreenH,
                ProtocolA[Index]->Mode->AbsoluteMaxY,
                NULL
            );
#else
            State.X = (APointerState.CurrentX * ScreenW) / ProtocolA[Index]->Mode->AbsoluteMaxX;
            State.Y = (APointerState.CurrentY * ScreenH) / ProtocolA[Index]->Mode->AbsoluteMaxY;
#endif
            State.Holding = (APointerState.ActiveButtons & EFI_ABSP_TouchActive);
        }
    }
    for (Index = 0; Index < NumSPointerDevices; Index++) {
        PointerStatus = REFIT_CALL_2_WRAPPER(ProtocolS[Index]->GetState, ProtocolS[Index], &SPointerState);

        // if new state found and we have not already found a new state
        if (!EFI_ERROR(PointerStatus) && EFI_ERROR(Status)) {
            Status = EFI_SUCCESS;

            INT32 TargetX = 0;
            INT32 TargetY = 0;

#ifdef EFI32
	    TargetX = State.X + (INTN) DivS64x64Remainder (
            SPointerState.RelativeMovementX * GlobalConfig.MouseSpeed,
            ProtocolS[Index]->Mode->ResolutionX,
            NULL
        );
            TargetY = State.Y + (INTN) DivS64x64Remainder (
                SPointerState.RelativeMovementY * GlobalConfig.MouseSpeed,
                ProtocolS[Index]->Mode->ResolutionY,
                NULL
            );
#else
            TargetX = State.X + SPointerState.RelativeMovementX *
                GlobalConfig.MouseSpeed / ProtocolS[Index]->Mode->ResolutionX;
            TargetY = State.Y + SPointerState.RelativeMovementY *
                GlobalConfig.MouseSpeed / ProtocolS[Index]->Mode->ResolutionY;
#endif

            if (TargetX < 0) {
                State.X = 0;
            }
            else if (TargetX >= ScreenW) {
                State.X = ScreenW - 1;
            }
            else {
                State.X = TargetX;
            }

            if (TargetY < 0) {
                State.Y = 0;
            }
            else if (TargetY >= ScreenH) {
                State.Y = ScreenH - 1;
            }
            else {
                State.Y = TargetY;
            }

            State.Holding = SPointerState.LeftButton;
        }
    }

    State.Press = (LastHolding && !State.Holding);

    return Status;
#endif
} // EFI_STATUS pdUpdateState()

////////////////////////////////////////////////////////////////////////////////
// Returns the current pointer state
////////////////////////////////////////////////////////////////////////////////
POINTER_STATE pdGetState (VOID) {
    return State;
} // POINTER_STATE pdGetState()

////////////////////////////////////////////////////////////////////////////////
// Draw the mouse at the current coordinates
////////////////////////////////////////////////////////////////////////////////
VOID pdDraw (VOID) {
    MY_FREE_IMAGE(Background);
    if (MouseImage) {
        UINTN Width = MouseImage->Width;
        UINTN Height = MouseImage->Height;

        if (State.X + Width > ScreenW) {
            Width = ScreenW - State.X;
        }
        if (State.Y + Height > ScreenH) {
            Height = ScreenH - State.Y;
        }

        Background = egCopyScreenArea (State.X, State.Y, Width, Height);
        if (Background) {
            BltImageCompositeBadge (Background, MouseImage, NULL, State.X, State.Y);
        }
    }
    LastXPos = State.X;
    LastYPos = State.Y;
} // VOID pdDraw()

////////////////////////////////////////////////////////////////////////////////
// Restores the background at the position the mouse was last drawn
////////////////////////////////////////////////////////////////////////////////
VOID pdClear (VOID) {
    if (Background) {
        egDrawImage (Background, LastXPos, LastYPos);
        MY_FREE_IMAGE(Background);
    }
} // VOID pdClear()
