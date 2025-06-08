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
 * Copyright (c) 2020-2025 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "pointer.h"
#include "screenmgt.h"
#include "rp_funcs.h"
#include "icns.h"
#include "../include/refit_call_wrapper.h"

UINTN                           NumSPointerDevices =                                  0;
EFI_GUID                        SPointerGuid       =   EFI_SIMPLE_POINTER_PROTOCOL_GUID;
EFI_HANDLE                     *HandleS            =                               NULL;
EFI_SIMPLE_POINTER_PROTOCOL   **ProtocolS          =                               NULL;

UINTN                           NumAPointerDevices =                                  0;
EFI_GUID                        APointerGuid       = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;
EFI_HANDLE                     *HandleA            =                               NULL;
EFI_ABSOLUTE_POINTER_PROTOCOL **ProtocolA          =                               NULL;

UINTN                           LastXPos           =                                  0;
UINTN                           LastYPos           =                                  0;
EG_IMAGE                       *MouseImage         =                               NULL;
EG_IMAGE                       *Background         =                               NULL;

BOOLEAN                         MouseTouchActive   =                               TRUE;
BOOLEAN                         PointerAvailable   =                              FALSE;
POINTER_STATE                   State;

extern BOOLEAN                  RunningOC;
BOOLEAN gSuppressPointerDraw = TRUE;
////////////////////////////////////////////////////////////////////////////////
// Initialise Pointer Devices
////////////////////////////////////////////////////////////////////////////////
VOID pdInitialize (VOID) {
    #if REFIT_DEBUG > 0
    EFI_STATUS  EnableStatusTouch;
    EFI_STATUS  EnableStatusMouse;
    CHAR16     *MsgStr;
    #endif

    EFI_STATUS Status;
    EFI_STATUS HandleStatus;
    UINTN      NumPointerHandles;
    UINTN      Index;


    #if REFIT_DEBUG > 0
    MsgStr = StrDuplicate (L"M A N A G E   P O I N T E R   D E V I C E S");
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
    LOG_MSG("%s", MsgStr);
    LOG_MSG("\n");
    MY_FREE_POOL(MsgStr);
    #endif

    if (GlobalConfig.DirectBoot) {
        #if REFIT_DEBUG > 0
        LOG_MSG("Skip Pointer Device Setup ... 'DirectBoot' is Active");
        LOG_MSG("\n\n");
        #endif

        // Early Return
        return;
    }

    pdCleanup(); // Just in case

    if (!GlobalConfig.EnableMouse && !GlobalConfig.EnableTouch) {
        MouseTouchActive = FALSE;

        #if REFIT_DEBUG > 0
        // DA-TAG: Use LOG_THREE_STAR_END for this instance
        MsgStr = StrDuplicate (L"Running in 'Keyboard Only' Mode");
        ALT_LOG(1, LOG_STAR_HEAD_SEP, L"%s", MsgStr);
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        LOG_MSG("\n\n");
        LOG_MSG("INFO: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        // Early Return
        return;
    }

    #if REFIT_DEBUG > 0
    MsgStr = StrDuplicate (L"Activate Pointer Devices:");
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("\n");
    LOG_MSG("%s", MsgStr);
    MY_FREE_POOL(MsgStr);

    EnableStatusTouch = EFI_NOT_STARTED;
    #endif

    if (GlobalConfig.EnableTouch && !RunningOC) {
        // Get handles that support the absolute pointer protocol
        // These are usually touchscreens but some mice also do
        NumPointerHandles = 0;
        HandleStatus = REFIT_CALL_5_WRAPPER(
            gBS->LocateHandleBuffer, ByProtocol,
            &APointerGuid, NULL,
            &NumPointerHandles, &HandleA
        );
        if (EFI_ERROR(HandleStatus)) {
            #if REFIT_DEBUG > 0
            EnableStatusTouch = EFI_LOAD_ERROR;
            #endif

            GlobalConfig.EnableTouch = FALSE;
        }
        else {
            ProtocolA = AllocatePool (
                NumPointerHandles * sizeof (EFI_ABSOLUTE_POINTER_PROTOCOL*)
            );
            if (ProtocolA == NULL) {
                #if REFIT_DEBUG > 0
                EnableStatusTouch = EFI_OUT_OF_RESOURCES;
                #endif
            }
            else {
                for (Index = 0; Index < NumPointerHandles; Index++) {
                    // Open the protocol on the handle
                    Status = REFIT_CALL_6_WRAPPER(
                        gBS->OpenProtocol, HandleA[Index],
                        &APointerGuid, (VOID **) &ProtocolA[NumAPointerDevices],
                        SelfImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
                    );
                    if (!EFI_ERROR(Status)) {
                        NumAPointerDevices++;

                        #if REFIT_DEBUG > 0
                        EnableStatusTouch = EFI_SUCCESS;
                        #endif
                    }
                } // for
            }
        }
    } // if GlobalConfig.EnableTouch

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"Enable Type 'A' (Touch Style) Pointer ... %r",
        EnableStatusTouch
    );
    ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
    LOG_MSG("%s  - %s", OffsetNext, MsgStr);
    MY_FREE_POOL(MsgStr);

    EnableStatusMouse = EFI_NOT_STARTED;
    #endif

    if (GlobalConfig.EnableMouse && !RunningOC) {
        // Get handles that support the simple pointer protocol (mice)
        NumPointerHandles = 0;
        HandleStatus = REFIT_CALL_5_WRAPPER(
            gBS->LocateHandleBuffer, ByProtocol,
            &SPointerGuid, NULL,
            &NumPointerHandles, &HandleS
        );

        if (EFI_ERROR(HandleStatus)) {
            #if REFIT_DEBUG > 0
            EnableStatusMouse = EFI_LOAD_ERROR;
            #endif

            GlobalConfig.EnableMouse = FALSE;
        }
        else {
            ProtocolS = AllocatePool (
                NumPointerHandles * sizeof (EFI_SIMPLE_POINTER_PROTOCOL*)
            );
            if (ProtocolS == NULL) {
                #if REFIT_DEBUG > 0
                EnableStatusMouse = EFI_OUT_OF_RESOURCES;
                #endif
            }
            else {
                for (Index = 0; Index < NumPointerHandles; Index++) {
                    // Open the protocol on the handle
                    Status = REFIT_CALL_6_WRAPPER(
                        gBS->OpenProtocol, HandleS[Index],
                        &SPointerGuid, (VOID **) &ProtocolS[NumSPointerDevices],
                        SelfImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
                    );
                    if (!EFI_ERROR(Status)) {
                        NumSPointerDevices += 1;

                        #if REFIT_DEBUG > 0
                        EnableStatusMouse = EFI_SUCCESS;
                        #endif
                    }
                    else {
                        #if REFIT_DEBUG > 0
                        if (Status != EFI_NOT_FOUND &&
                            EFI_ERROR(EnableStatusMouse)
                        ) {
                            EnableStatusMouse = EFI_LOAD_ERROR;
                        }
                        #endif
                    }
                } // for
            } // if ProtocolS
        } // if/else EFI_ERROR(HandleStatus)
    } // if GlobalConfig.EnableMouse

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"Enable Type 'S' (Mouse Style) Pointer ... %r",
        EnableStatusMouse
    );
    ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
    LOG_MSG("%s  - %s", OffsetNext, MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    // Load mouse icon
    if (NumAPointerDevices > 0 ||
        NumSPointerDevices > 0
    ) {
        if (GlobalConfig.EnableMouse) {
            MouseImage = BuiltinIcon (BUILTIN_ICON_MOUSE);
        }

        PointerAvailable =  TRUE;
    }
    else {
        PointerAvailable = FALSE;
        MouseTouchActive = FALSE;
    }

    #if REFIT_DEBUG > 0
    if (RunningOC) {
        Status = EFI_NOT_STARTED;
    }
    else if (
        !EFI_ERROR(EnableStatusTouch) ||
        !EFI_ERROR(EnableStatusMouse)
    ) {
        Status = EFI_SUCCESS;
    }
    else {
        Status = EFI_DEVICE_ERROR;
    }

    MsgStr = PoolPrint (L"Enable Pointer Devices ... %r", Status);
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    LOG_MSG("\n\n");
    LOG_MSG("INFO: %s", MsgStr);
    LOG_MSG("\n\n");
    MY_FREE_POOL(MsgStr);
    #endif
} // VOID pdInitialize()

////////////////////////////////////////////////////////////////////////////////
// Frees allocated memory and closes pointer protocols
////////////////////////////////////////////////////////////////////////////////
VOID pdCleanup (VOID) {
    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;
    #endif

    UINTN   Index;

    #if REFIT_DEBUG > 0
    MsgStr = L"Pointer Scenarios";
    ALT_LOG(1, LOG_LINE_NORMAL, L"Dismantle %s", MsgStr);
    LOG_MSG("Deconfigure %s:", MsgStr);
    #endif

    PointerAvailable = FALSE;
    pdClear();

    if (RunningOC) {
        return;
    }

    if (HandleA != NULL) {
        for (Index = 0; Index < NumAPointerDevices; Index++) {
            REFIT_CALL_4_WRAPPER(
                gBS->CloseProtocol, HandleA[Index],
                &APointerGuid, SelfImageHandle, NULL
            );
        }
    }

    if (HandleS != NULL) {
        for (Index = 0; Index < NumSPointerDevices; Index++) {
            REFIT_CALL_4_WRAPPER(
                gBS->CloseProtocol, HandleS[Index],
                &SPointerGuid, SelfImageHandle, NULL
            );
        }
    }

    NumAPointerDevices = 0;
    NumSPointerDevices = 0;

    LastXPos = ScreenW / 2;
    LastYPos = ScreenH / 2;

    State.X  = ScreenW / 2;
    State.Y  = ScreenH / 2;
    State.Press    = FALSE;
    State.Holding  = FALSE;

    #if REFIT_DEBUG > 0
    MsgStr = L"Disable Pointer Protocols ... Success";
    ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
    LOG_MSG("%s  - %s", OffsetNext, MsgStr);
    #endif

    MY_FREE_POOL(HandleA);
    MY_FREE_POOL(HandleS);
    MY_FREE_POOL(ProtocolA);
    MY_FREE_POOL(ProtocolS);
    MY_FREE_IMAGE(MouseImage);
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
    EFI_EVENT TheEvent;


    if (!PointerAvailable ||
        Index >= (NumAPointerDevices + NumSPointerDevices)
    ) {
        TheEvent = NULL;
    }
    else if (Index >= NumAPointerDevices) {
        TheEvent = ProtocolS[Index - NumAPointerDevices]->WaitForInput;
    }
    else {
        TheEvent = ProtocolA[Index]->WaitForInput;
    }

    return TheEvent;
} // EFI_EVENT pdWaitEvent()

////////////////////////////////////////////////////////////////////////////////
// Gets the current state of pointer devices and
// assigns this to the first available device
////////////////////////////////////////////////////////////////////////////////
static
INT32 Int64ToInt32 (
    INT64 TempINT64
) {
    if (TempINT64 > INT32_MAX) return INT32_MAX;
    if (TempINT64 < INT32_MIN) return INT32_MIN;

    return (INT32) TempINT64;
} // static INT32 Int64ToInt32()

static
UINTN Uint64ToUintn (
    UINT64 TempUINT64
) {
    /* coverity[unsigned_compare: SUPPRESS] */
    /* coverity[dead_error_condition: SUPPRESS] */
    /* coverity[dead_error_line: SUPPRESS] */
    if (TempUINT64 > UINTN_MAX) return UINTN_MAX;

    /* coverity[unsigned_compare: SUPPRESS] */
    /* coverity[dead_error_condition: SUPPRESS] */
    /* coverity[dead_error_line: SUPPRESS] */
    if (TempUINT64 < UINTN_MIN) return UINTN_MIN;

    return (UINTN) TempUINT64;
} // static UINTN Uint64ToUintn()

static
UINTN Int32ToUintn (
    INT32 TempINT32
) {
    /* coverity[result_independent_of_operands: SUPPRESS] */
    /* coverity[dead_error_condition: SUPPRESS] */
    /* coverity[dead_error_line: SUPPRESS] */
    if (TempINT32 > UINTN_MAX) return UINTN_MAX;

    /* coverity[unsigned_conversion: SUPPRESS] */
    /* coverity[unsigned_compare: SUPPRESS] */
    /* coverity[dead_error_condition: SUPPRESS] */
    /* coverity[dead_error_line: SUPPRESS] */
    if (TempINT32 < UINTN_MIN) return UINTN_MIN;

    return (UINTN) TempINT32;
} // static UINTN Int32ToUintn()

static
UINTN Int64ToUintn (
    INT64 TempINT64
) {
    /* coverity[result_independent_of_operands: SUPPRESS] */
    /* coverity[dead_error_condition: SUPPRESS] */
    /* coverity[dead_error_line: SUPPRESS] */
    if (TempINT64 > UINTN_MAX) return UINTN_MAX;

    /* coverity[unsigned_conversion: SUPPRESS] */
    /* coverity[unsigned_compare: SUPPRESS] */
    if (TempINT64 < UINTN_MIN) return UINTN_MIN;

    return (UINTN) TempINT64;
} // static UINTN Int64ToUintn()

EFI_STATUS pdUpdateState (VOID) {
    EFI_STATUS                 Status;
    UINTN                      Index;
    INT32                      TargetX;
    INT32                      TargetY;
    INT64                      TempINT64;
    UINT64                     TempUINT64;
    BOOLEAN                    LastHolding;
    EFI_SIMPLE_POINTER_STATE   SPointerState;
    EFI_ABSOLUTE_POINTER_STATE APointerState;


    #if defined (EFI32) && defined (__MAKEWITH_GNUEFI)
    return EFI_NOT_READY;
    #endif

    if (!PointerAvailable) {
        return EFI_NOT_READY;
    }

    LastHolding = State.Holding;

    do {
        for (Index = 0; Index < NumAPointerDevices; Index++) {
            Status = REFIT_CALL_2_WRAPPER(
                ProtocolA[Index]->GetState,
                ProtocolA[Index], &APointerState
            );
            if (EFI_ERROR(Status)) {
                continue; // 'for' loop
            }

            TempUINT64 = DivU64x64Remainder (
                (UINT64) APointerState.CurrentX *
                (UINT64) ScreenW,
                (UINT64) ProtocolA[Index]->Mode->AbsoluteMaxX,
                NULL
            );
            State.X = Uint64ToUintn (TempUINT64);

            TempUINT64 = DivU64x64Remainder (
                (UINT64) APointerState.CurrentY *
                (UINT64) ScreenH,
                (UINT64) ProtocolA[Index]->Mode->AbsoluteMaxY,
                NULL
            );
            State.Y = Uint64ToUintn (TempUINT64);

            State.Holding = (
                APointerState.ActiveButtons & EFI_ABSP_TouchActive
            );

            break; // 'for' loop
        } // for

        if (!EFI_ERROR(Status)) {
            break; // 'do' loop
        }

        for (Index = 0; Index < NumSPointerDevices; Index++) {
            Status = REFIT_CALL_2_WRAPPER(
                ProtocolS[Index]->GetState,
                ProtocolS[Index], &SPointerState
            );
            if (EFI_ERROR(Status)) {
                continue; // 'for' loop
            }

            TempINT64 = (INT64) State.X + DivS64x64Remainder (
                (INT64) SPointerState.RelativeMovementX *
                (INT64) GlobalConfig.MouseSpeed,
                (INT64) ProtocolS[Index]->Mode->ResolutionX,
                NULL
            );
            TargetX = Int64ToInt32 (TempINT64);

            TempINT64 = (INT64) State.Y + DivS64x64Remainder (
                (INT64) SPointerState.RelativeMovementY *
                (INT64) GlobalConfig.MouseSpeed,
                (INT64) ProtocolS[Index]->Mode->ResolutionY,
                NULL
            );
            TargetY = Int64ToInt32 (TempINT64);

            TempINT64 = ScreenW - 1;
            if (0);
            else if (TargetX < 0)        State.X = 0;
            else if (TargetX >= ScreenW) State.X = Int64ToUintn (TempINT64);
            else                         State.X = Int32ToUintn (TargetX);

            TempINT64 = ScreenH - 1;
            if (0);
            else if (TargetY < 0)        State.Y = 0;
            else if (TargetY >= ScreenH) State.Y = Int64ToUintn (TempINT64);
            else                         State.Y = Int32ToUintn (TargetY);

            State.Holding = SPointerState.LeftButton;

            break; // 'for' loop
        } // for
    } while (0); // This 'loop' only runs once

    State.Press = (LastHolding && !State.Holding);
    if (State.X != LastXPos || State.Y != LastYPos) { // Mouse has moved
        if (gSuppressPointerDraw) { // If pointer was suppressed (hidden)
            gSuppressPointerDraw = FALSE; }// Show the pointer
        }
    if (EFI_ERROR(Status)) {
        Status = EFI_NOT_READY;
    }

    return Status;
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
    UINTN Width;
    UINTN Height;

    if (!MouseTouchActive) {
        return;
    }
    if (gSuppressPointerDraw) {
        return;
    }
    MY_FREE_IMAGE(Background);
    if (MouseImage != NULL) {
        Width = (
            (State.X + MouseImage->Width) > ScreenW
        ) ? ScreenW - State.X : MouseImage->Width;
        Height = (
            (State.Y + MouseImage->Height) > ScreenH
        ) ? ScreenH - State.Y : MouseImage->Height;

        Background = egCopyScreenArea (
            State.X, State.Y,
            Width, Height
        );
        if (Background != NULL) {
            BltImageCompositeAny (
                Background, MouseImage,
                NULL, State.X, State.Y
            );
        }
    }
    LastXPos = State.X;
    LastYPos = State.Y;
} // VOID pdDraw()

////////////////////////////////////////////////////////////////////////////////
// Restores the background at the position the mouse was last drawn
////////////////////////////////////////////////////////////////////////////////
VOID pdClear (VOID) {
    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;

    static BOOLEAN NotLogged = TRUE;
    #endif


    if (!MouseTouchActive) {
        return;
    }

    if (Background != NULL) {
        egDrawImage (Background, LastXPos, LastYPos);
        MY_FREE_IMAGE(Background);
    }

    #if REFIT_DEBUG > 0
    if (NotLogged) {
        MsgStr = L"Cleanse Pointer Artefacts ... Success";
        ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
        LOG_MSG("%s  - %s", OffsetNext, MsgStr);
        NotLogged = FALSE;
    }
    #endif
} // VOID pdClear()
