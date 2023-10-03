/** @file
  BDS Lib functions which relate with connect the device

Copyright (c) 2004 - 2008, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/
/**
 * Modified for RefindPlus
 * Copyright (c) 2020-2023 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
**/

#include "Platform.h"
#include "../BootMaster/lib.h"
#include "../BootMaster/screenmgt.h"
#include "../BootMaster/mystrings.h"
#include "../BootMaster/launch_efi.h"
#include "../include/refit_call_wrapper.h"

#if REFIT_DEBUG > 0
#include "../../ShellPkg/Include/Library/HandleParsingLib.h"
#endif

#define IS_PCI_GFX(_p) IS_CLASS2(_p, PCI_CLASS_DISPLAY, PCI_CLASS_DISPLAY_OTHER)

BOOLEAN FoundGOP        = FALSE;
BOOLEAN ReLoaded        = FALSE;
BOOLEAN ForceRescanDXE  = FALSE;
BOOLEAN AcquireErrorGOP = FALSE;
BOOLEAN ObtainHandleGOP = FALSE;
BOOLEAN DetectedDevices = FALSE;
BOOLEAN DevicePresence  = FALSE;

UINTN   AllHandleCount;


extern EFI_STATUS AmendSysTable (VOID);
extern EFI_STATUS AcquireGOP (VOID);
extern EFI_STATUS ReissueGOP (VOID);

extern BOOLEAN SetSysTab;
extern BOOLEAN SetPreferUGA;

// DA-TAG: Limit to TianoCore
#ifdef __MAKEWITH_TIANO
extern EFI_STATUS OcConnectDrivers (VOID);
#endif

static
VOID UpdatePreferUGA (VOID) {
    static BOOLEAN GotStatusUGA = FALSE;

    if (GotStatusUGA) return;
    if (GlobalConfig.PreferUGA && egInitUGADraw(FALSE)) {
        SetPreferUGA = TRUE;
    }
    GotStatusUGA = TRUE;
} // static VOID UpdatePreferUGA()

static
EFI_STATUS EFIAPI RefitConnectController (
    IN  EFI_HANDLE                ControllerHandle,
    IN  EFI_HANDLE               *DriverImageHandle   OPTIONAL,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL,
    IN  BOOLEAN                   Recursive
) {
    EFI_STATUS   Status;
    VOID        *DevicePath;

    if (ControllerHandle == NULL) {
        // Early Return
        return EFI_INVALID_PARAMETER;
    }

    // DA-TAG: Do not connect controllers without device paths.
    //         REF: https://bugzilla.tianocore.org/show_bug.cgi?id=2460
    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol, ControllerHandle,
        &gEfiDevicePathProtocolGuid, &DevicePath
    );
    if (EFI_ERROR(Status)) {
        // Early Return
        return EFI_NOT_STARTED;
    }

    Status = REFIT_CALL_4_WRAPPER(
        gBS->ConnectController, ControllerHandle,
        DriverImageHandle, RemainingDevicePath, Recursive
    );

    return Status;
} // EFI_STATUS RefitConnectController()

EFI_STATUS ScanDeviceHandles (
    EFI_HANDLE   ControllerHandle,
    UINTN       *HandleCount,
    EFI_HANDLE **HandleBuffer,
    UINT32     **HandleType
) {
    EFI_STATUS                            Status;
    EFI_GUID                            **ProtocolGuidArray;
    UINTN                                 k;
    UINTN                                 ArrayCount;
    UINTN                                 ProtocolIndex;
    UINTN                                 OpenInfoCount;
    UINTN                                 OpenInfoIndex;
    UINTN                                 ChildIndex;
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY  *OpenInfo;

    *HandleCount  = 0;
    *HandleBuffer = NULL;
    *HandleType   = NULL;

    // DA-TAG: Retrieve a list of handles with device paths
    //         REF: https://bugzilla.tianocore.org/show_bug.cgi?id=2460
    Status = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, ByProtocol,
        &gEfiDevicePathProtocolGuid, NULL,
        HandleCount, HandleBuffer
    );
    if (EFI_ERROR(Status)) {
        MY_FREE_POOL(*HandleType);
        MY_FREE_POOL(*HandleBuffer);
        *HandleCount  = 0;

        // Early Return
        return Status;
    }

    *HandleType = AllocatePool (*HandleCount * sizeof (UINT32));
    if (*HandleType == NULL) {
        MY_FREE_POOL(*HandleType);
        MY_FREE_POOL(*HandleBuffer);
        *HandleCount  = 0;

        // Early Return
        return Status;
    }

    for (k = 0; k < *HandleCount; k++) {
        (*HandleType)[k] = EFI_HANDLE_TYPE_UNKNOWN;

        // Retrieve a list of all the protocols on each handle
        Status = REFIT_CALL_3_WRAPPER(
            gBS->ProtocolsPerHandle, (*HandleBuffer)[k],
            &ProtocolGuidArray, &ArrayCount
        );
        if (EFI_ERROR(Status)) {
            continue;
        }

        for (ProtocolIndex = 0; ProtocolIndex < ArrayCount; ProtocolIndex++) {
            if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiLoadedImageProtocolGuid)) {
                (*HandleType)[k] |= EFI_HANDLE_TYPE_IMAGE_HANDLE;
            }

            if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDriverBindingProtocolGuid)) {
                (*HandleType)[k] |= EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE;
            }

            if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDriverConfigurationProtocolGuid)) {
                (*HandleType)[k] |= EFI_HANDLE_TYPE_DRIVER_CONFIGURATION_HANDLE;
            }

            if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDriverDiagnosticsProtocolGuid)) {
                (*HandleType)[k] |= EFI_HANDLE_TYPE_DRIVER_DIAGNOSTICS_HANDLE;
            }

            if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiComponentName2ProtocolGuid)) {
                (*HandleType)[k] |= EFI_HANDLE_TYPE_COMPONENT_NAME_HANDLE;
            }

            if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiComponentNameProtocolGuid)) {
                (*HandleType)[k] |= EFI_HANDLE_TYPE_COMPONENT_NAME_HANDLE;
            }

            if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDevicePathProtocolGuid)) {
                (*HandleType)[k] |= EFI_HANDLE_TYPE_DEVICE_HANDLE;
            }

            // Retrieve the list of agents that have opened each protocol
            Status = REFIT_CALL_4_WRAPPER(
                gBS->OpenProtocolInformation, (*HandleBuffer)[k],
                ProtocolGuidArray[ProtocolIndex], &OpenInfo, &OpenInfoCount
            );
            if (EFI_ERROR(Status)) {
                continue;
            }

            for (OpenInfoIndex = 0; OpenInfoIndex < OpenInfoCount; OpenInfoIndex++) {
                if (OpenInfo[OpenInfoIndex].ControllerHandle == ControllerHandle) {
                    if ((OpenInfo[OpenInfoIndex].Attributes &
                        EFI_OPEN_PROTOCOL_BY_DRIVER) == EFI_OPEN_PROTOCOL_BY_DRIVER
                    ) {
                        for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++) {
                            if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].AgentHandle) {
                                (*HandleType)[ChildIndex] |= EFI_HANDLE_TYPE_DEVICE_DRIVER;
                            }
                        } // for
                    }

                    if ((OpenInfo[OpenInfoIndex].Attributes &
                        EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) == EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                    ) {
                        (*HandleType)[k] |= EFI_HANDLE_TYPE_PARENT_HANDLE;
                        for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++) {
                            if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].AgentHandle) {
                                (*HandleType)[ChildIndex] |= EFI_HANDLE_TYPE_BUS_DRIVER;
                            }
                        } // for
                    }
                }
            } // for OpenInfoIndex = 0

            MY_FREE_POOL(OpenInfo);
        } // for ProtocolIndex = 0

        MY_FREE_POOL(ProtocolGuidArray);
    } // for k = 0

    return EFI_SUCCESS;
} // EFI_STATUS ScanDeviceHandles()


EFI_STATUS BdsLibConnectMostlyAllEfi (VOID) {
    EFI_STATUS            XStatus;
    EFI_STATUS            Status;
    UINTN                 i, k, m;
    UINTN                 BusPCI;
    UINTN                 GOPCount;
    UINTN                 DevicePCI;
    UINTN                 SegmentPCI;
    UINTN                 FunctionPCI;
    UINTN                 HandleCount;
    UINT32               *HandleType;
    BOOLEAN               Parent;
    BOOLEAN               Device;
    BOOLEAN               DevTag;
    BOOLEAN               VGADevice;
    BOOLEAN               GFXDevice;
    BOOLEAN               MakeConnection;
    EFI_HANDLE           *AllHandleBuffer;
    EFI_HANDLE           *HandleBuffer;
    EFI_HANDLE           *GOPArray;
    PCI_TYPE00            Pci;
    EFI_PCI_IO_PROTOCOL *PciIo;



    #if REFIT_DEBUG > 0
    CHAR16 *GopDevicePathStr;
    CHAR16 *StrDevicePath;
    CHAR16 *DeviceData;
    CHAR16 *FillStr;
    CHAR16 *MsgStr;
    CHAR16 *TmpStr;
    UINTN   HexIndex;
    UINTN   AllHandleCountTrigger;
    BOOLEAN CheckMute = FALSE;
    #endif

    DetectedDevices = FALSE;

    #if REFIT_DEBUG > 0
    if (ReLoaded) {
        MsgStr = StrDuplicate (L"R E C O N N E C T   D E V I C E   H A N D L E S");
        ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
    }
    else {
        MsgStr = StrDuplicate (L"C O N N E C T   D E V I C E   H A N D L E S");
        ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
    }
    LOG_MSG("%s", MsgStr);
    LOG_MSG("\n");
    MY_FREE_POOL(MsgStr);
    #endif

    // DA_TAG: Only connect controllers with device paths.
    //         See notes under ScanDeviceHandles
    //Status = REFIT_CALL_5_WRAPPER(
    //    gBS->LocateHandleBuffer, AllHandles,
    //    NULL, NULL,
    //    &AllHandleCount, &AllHandleBuffer
    //);
    AllHandleBuffer = NULL;
    Status = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, ByProtocol,
        &gEfiDevicePathProtocolGuid, NULL,
        &AllHandleCount, &AllHandleBuffer
    );
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"Did Not Find Any Contollers with Device Paths");
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
        LOG_MSG("INFO: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        // Early Return
        return Status;
    }

    #if REFIT_DEBUG > 0
    GopDevicePathStr      = NULL;
    AllHandleCountTrigger = AllHandleCount - 1;
    #endif

    HandleType = NULL;
    GOPArray = HandleBuffer = NULL;
    for (i = 0; i < AllHandleCount; i++) {
        MakeConnection = TRUE;

        #if REFIT_DEBUG > 0
        HexIndex   = ConvertHandleToHandleIndex (AllHandleBuffer[i]);
        DeviceData = NULL;
        #endif

        XStatus = ScanDeviceHandles (
            AllHandleBuffer[i],
            &HandleCount,
            &HandleBuffer,
            &HandleType
        );
        if (EFI_ERROR(XStatus)) {
            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (L"Handle 0x%03X      - ERROR: %r", HexIndex, XStatus);
            ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
            LOG_MSG("%s", MsgStr);
            MY_FREE_POOL(MsgStr);
            #endif
        }
        else if (HandleType == NULL) {
            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (L"Handle 0x%03X      - ERROR: Invalid Handle", HexIndex);
            ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
            LOG_MSG("%s", MsgStr);
            MY_FREE_POOL(MsgStr);
            #endif
        }
        else {
            // Assume Device
            Device = TRUE;

            for (k = 0; k < HandleCount; k++) {
                if (HandleType[k] & EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE) {
                    Device = FALSE;
                    break;
                }
                if (HandleType[k] & EFI_HANDLE_TYPE_IMAGE_HANDLE) {
                    Device = FALSE;
                    break;
                }
            } // for

            if (!Device) {
                #if REFIT_DEBUG > 0
                MsgStr = PoolPrint (L"Handle 0x%03X     Discounted [Other Item]", HexIndex);
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("%s", MsgStr);
                MY_FREE_POOL(MsgStr);
                #endif
            }
            else {
                // Flag Device Presence
                DevicePresence = TRUE;

                // Assume Not Parent
                Parent = FALSE;

                for (k = 0; k < HandleCount; k++) {
                    if (HandleType[k] & EFI_HANDLE_TYPE_PARENT_HANDLE) {
                        MakeConnection = FALSE;
                        Parent         = TRUE;
                        break;
                    }
                } // for

                // Assume Not Device
                DevTag = FALSE;

                for (k = 0; k < HandleCount; k++) {
                    if (HandleType[k] & EFI_HANDLE_TYPE_DEVICE_HANDLE) {
                        DevTag = TRUE;
                        break;
                    }
                } // for

                // Assume Success
                XStatus = EFI_SUCCESS;

                if (DevTag) {
                    XStatus = REFIT_CALL_3_WRAPPER(
                        gBS->HandleProtocol, AllHandleBuffer[i],
                        &gEfiPciIoProtocolGuid, (void **) &PciIo
                    );
                    if (EFI_ERROR(XStatus)) {
                        #if REFIT_DEBUG > 0
                        DeviceData = StrDuplicate (L"Not PCIe Device");
                        #endif
                    }
                    else {
                        // Read PCI BUS
                        REFIT_CALL_5_WRAPPER(
                            PciIo->GetLocation, PciIo,
                            &SegmentPCI, &BusPCI,
                            &DevicePCI, &FunctionPCI
                        );

                        XStatus = REFIT_CALL_5_WRAPPER(
                            PciIo->Pci.Read, PciIo,
                            EfiPciIoWidthUint32, 0,
                            sizeof (Pci) / sizeof (UINT32), &Pci
                        );
                        if (EFI_ERROR(XStatus)) {
                            MakeConnection = FALSE;

                            #if REFIT_DEBUG > 0
                            DeviceData = StrDuplicate (L"Unreadable Item");
                            #endif
                        }
                        else {
                            VGADevice = IS_PCI_VGA(&Pci);
                            GFXDevice = IS_PCI_GFX(&Pci);

                            if (VGADevice) {
                                // DA-TAG: Investigate This
                                //         Unable to reconnect later after disconnecting here
                                //         Comment out and set 'MakeConnection' to FALSE
                                //REFIT_CALL_3_WRAPPER(
                                //    gBS->DisconnectController, AllHandleBuffer[i],
                                //    NULL, NULL
                                //);
                                MakeConnection = FALSE;

                                #if REFIT_DEBUG > 0
                                DeviceData = StrDuplicate (L"Monitor Display");
                                #endif
                            }
                            else if (GFXDevice) {
                                // DA-TAG: Currently unable to detect GFX Device
                                //         Revisit Clover implementation later
                                //         Not currently missed but may allow new options
                                // UPDATE: Actually works on a Non-Mac Firmware Laptop
                                //         Is this because it is a laptop or Non-Mac Firmware?

                                #if REFIT_DEBUG > 0
                                DeviceData = StrDuplicate (L"GraphicsFX Card");
                                #endif
                            }
                            else {
                                // DA-TAG: Not doing anything with these and just logging
                                //         Might be options out of the items above later
                                #if REFIT_DEBUG > 0
                                DeviceData = PoolPrint (
                                    L"PCI(%02llX|%02llX:%02llX.%llX)",
                                    SegmentPCI, BusPCI,
                                    DevicePCI, FunctionPCI
                                );
                                #endif
                            } // if/else VGADevice/GFXDevicce
                        } // if/else EFI_ERROR(XStatus)
                    } // if/else !EFI_ERROR(XStatus)
                } // if DevTag

                if (!FoundGOP) {
                    XStatus = REFIT_CALL_5_WRAPPER(
                        gBS->LocateHandleBuffer, ByProtocol,
                        &gEfiGraphicsOutputProtocolGuid, NULL,
                        &GOPCount, &GOPArray
                    );
                    if (!EFI_ERROR(XStatus)) {
                        for (m = 0; m < GOPCount; m++) {
                            if (GOPArray[m] != gST->ConsoleOutHandle) {
                                #if REFIT_DEBUG > 0
                                GopDevicePathStr = ConvertDevicePathToText (
                                    DevicePathFromHandle (GOPArray[m]),
                                    FALSE, FALSE
                                );
                                #endif

                                FoundGOP = TRUE;
                                break;
                            }
                        }
                    }

                    MY_FREE_POOL(GOPArray);
                }

                #if REFIT_DEBUG > 0
                if (FoundGOP && GopDevicePathStr != NULL) {
                    StrDevicePath = ConvertDevicePathToText (
                        DevicePathFromHandle (AllHandleBuffer[i]),
                        FALSE, FALSE
                    );

                    if (StrStr (GopDevicePathStr, StrDevicePath)) {
                        DeviceData = PoolPrint (
                            L"%s : Leverages GOP",
                            DeviceData
                        );
                    }

                    MY_FREE_POOL(StrDevicePath);
                }
                #endif

                if (MakeConnection) {
                    XStatus = RefitConnectController (AllHandleBuffer[i], NULL, NULL, TRUE);
                }

                #if REFIT_DEBUG > 0
                if (DeviceData == NULL) {
                    FillStr = L"";
                    DeviceData = StrDuplicate (L"");
                }
                else {
                    MY_MUTELOGGER_SET;
                    if (FindSubStr (DeviceData, L"Monitor Display")) {
                        FillStr = L"  x  ";
                    }
                    else if (FindSubStr (DeviceData, L"GraphicsFX Card")) {
                        FillStr = L"  @  ";
                    }
                    else if (Parent) {
                        FillStr = L"     ";
                    }
                    else if (
                        XStatus != EFI_SUCCESS &&
                        XStatus != EFI_NOT_FOUND &&
                        XStatus != EFI_NOT_STARTED
                    ) {
                        FillStr = L"  .  ";
                    }
                    else if (EFI_ERROR(XStatus)) {
                        FillStr = L"     ";
                    }
                    else {
                        FillStr = L"  -  ";
                    }
                    MY_MUTELOGGER_OFF;
                }
                #endif

                if (Parent) {
                    #if REFIT_DEBUG > 0
                    MsgStr = PoolPrint (
                        L"Handle 0x%03X     Skipped [Parent Device]%s%s",
                        HexIndex, FillStr, DeviceData
                    );
                    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                    LOG_MSG("%s", MsgStr);
                    MY_FREE_POOL(MsgStr);
                    #endif
                }
                else if (XStatus == EFI_NOT_FOUND) {
                    #if REFIT_DEBUG > 0
                    MsgStr = PoolPrint (
                        L"Handle 0x%03X     Bypassed [Not Linkable]%s%s",
                        HexIndex, FillStr, DeviceData
                    );
                    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                    LOG_MSG("%s", MsgStr);
                    MY_FREE_POOL(MsgStr);
                    #endif
                }
                else if (!EFI_ERROR(XStatus)) {
                    DetectedDevices = TRUE;

                    #if REFIT_DEBUG > 0
                    MsgStr = PoolPrint (
                        L"Handle 0x%03X  *  %r                %s%s",
                        HexIndex, XStatus, FillStr, DeviceData
                    );
                    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                    LOG_MSG("%s", MsgStr);
                    MY_FREE_POOL(MsgStr);
                    #endif
                }
                else {
                    #if REFIT_DEBUG > 0

                    if (XStatus == EFI_NOT_STARTED) {
                        MsgStr = PoolPrint (
                            L"Handle 0x%03X     Declined [Empty Device]%s%s",
                            HexIndex, FillStr, DeviceData
                        );
                        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                        LOG_MSG("%s", MsgStr);
                        MY_FREE_POOL(MsgStr);
                    }
                    else if (XStatus == EFI_INVALID_PARAMETER) {
                        MsgStr = PoolPrint (
                            L"Handle 0x%03X  .  ERROR: Invalid Param   %s%s",
                            HexIndex, FillStr, DeviceData
                        );
                        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                        LOG_MSG("%s", MsgStr);
                        MY_FREE_POOL(MsgStr);
                    }
                    else {
                        TmpStr = PoolPrint (L"WARN: %r", XStatus);
                        MsgStr = PoolPrint (
                            L"Handle 0x%03X  .  %-23s%s%s",
                            HexIndex, TmpStr, FillStr, DeviceData
                        );
                        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                        LOG_MSG("%s", MsgStr);
                        MY_FREE_POOL(MsgStr);
                        MY_FREE_POOL(TmpStr);
                    }

                    #endif
                } // if Parent elseif
            } // if !Device
        } // if EFI_ERROR(XStatus)

        if (EFI_ERROR(XStatus)) {
            // Change Overall Status on Error
            Status = XStatus;
        }

        #if REFIT_DEBUG > 0
        if (i == AllHandleCountTrigger) {
            LOG_MSG("\n\n");
        }
        else {
            LOG_MSG("\n");
        }

        MY_FREE_POOL(DeviceData);
        #endif

        MY_FREE_POOL(HandleBuffer);
        MY_FREE_POOL(HandleType);
    }  // for i = 0

    #if REFIT_DEBUG > 0
    MY_FREE_POOL(GopDevicePathStr);
    #endif

	MY_FREE_POOL(AllHandleBuffer);

	return Status;
} // EFI_STATUS BdsLibConnectMostlyAllEfi()

/**
  Connects all drivers to all controllers.
  This function make sure all the current system driver will manage
  the correspoinding controllers if have. And at the same time, make
  sure all the system controllers have driver to manage it if have.
**/
static
EFI_STATUS BdsLibConnectAllDriversToAllControllersEx (VOID) {
    EFI_STATUS  Status;
    BOOLEAN     RescanDrivers;

    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr;
    #endif

    RescanDrivers = (GlobalConfig.RescanDXE || ForceRescanDXE);

    // DA-TAG: Limit to TianoCore
    #ifdef __MAKEWITH_TIANO
    if (RescanDrivers) {
        // Optional Silent First Pass Driver Connection
        OcConnectDrivers();
    }
    #endif

    if (!SetPreferUGA) {
        UpdatePreferUGA();
    }

    do {
        ObtainHandleGOP = FoundGOP = FALSE;

        // Connect All drivers
        BdsLibConnectMostlyAllEfi();

        // Update GOP FLag
        ObtainHandleGOP = FoundGOP;

        // Check if possible to dispatch additional DXE drivers as
        // BdsLibConnectAllEfi() may have revealed new DXE drivers.
        // If Dispatched status is EFI_SUCCESS, attempt to reconnect.
        // Forces 'EFI_NOT_FOUND' if 'RescanDrivers' or 'gDS' is unset.
        Status = (RescanDrivers && gDS) ? gDS->Dispatch() : EFI_NOT_FOUND;

        if (SetPreferUGA) {
            // DA-TAG: Rig 'FoundGOP' if forcing UGA-Only
            //         This skips ReloadGOP
            //         This makes this function return EFI_SUCCESS
            FoundGOP = TRUE;
        }

        #if REFIT_DEBUG > 0
        if (EFI_ERROR(Status)) {
            if (!FoundGOP && DetectedDevices) {
                LOG_MSG("INFO: Could *NOT* Identify Path to GOP on Device Handles");
            }
        }
        else {
            MsgStr = StrDuplicate (L"Additional DXE Drivers Revealed ... Relink Handles");
            ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
            LOG_MSG("INFO: %s", MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
        }
        #endif
    } while (!EFI_ERROR(Status));

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"Processed %d Handle%s ... Devices:- '%s'",
        AllHandleCount,
        (AllHandleCount == 1) ? L"" : L"s",
        (DevicePresence) ? L"Present" : L"Absent"
    );
    if (!FoundGOP && DetectedDevices) {
        LOG_MSG("%s      %s", OffsetNext, MsgStr);
    }
    else {
        LOG_MSG("INFO: %s", MsgStr);
    }
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    Status = (FoundGOP) ? EFI_SUCCESS : EFI_NOT_FOUND;

    return Status;
} // EFI_STATUS BdsLibConnectAllDriversToAllControllersEx()

// Many cases of of GPUs not working on EFI 1.x Units such as Classic MacPros are due
// to the GPU's GOP drivers failing to install on not detecting UEFI 2.x. This function
// amends SystemTable Revision information, provides the missing CreateEventEx capability
// then reloads the OptionROM from RAM (If Present) which will install GOP (If Available).
EFI_STATUS ApplyGOPFix (VOID) {
    EFI_STATUS Status;
    BOOLEAN    TempRescanDXE;

    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;
    #endif

    // Check whether OptionROMs are available in RAM
    Status = AcquireGOP();
    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"Reload OptionROM");
    MsgStr = PoolPrint (L"Status:- '%r' ... Acquire OptionROM From Volatile Memory", Status);
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("\n\n");
    LOG_MSG("INFO: %s", MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif
    if (EFI_ERROR(Status)) {
        AcquireErrorGOP = TRUE;

        // Early Return
        return Status;
    }

    // Update Boot Services Table to permit reloading OptionROMs
    Status = AmendSysTable();
    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (L"Status:- '%r' ... Amend Boot Services Table", Status);
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("%s      %s", OffsetNext, MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif
    if (Status == EFI_ALREADY_STARTED && SetSysTab == TRUE) {
        // Set to success if previously changed
        Status = EFI_SUCCESS;
    }
    if (EFI_ERROR(Status)) {
        AcquireErrorGOP = TRUE;

        // Early Return
        return Status;
    }

    // Reload OptionROMs
    Status = ReissueGOP();
    if (EFI_ERROR(Status)) {
        // Early Return
        return Status;
    }

    #if REFIT_DEBUG > 0
    BRK_MOD("\n\n");
    #endif

    // Connect all devices if no error
    TempRescanDXE = GlobalConfig.RescanDXE;
    GlobalConfig.RescanDXE = FALSE;
    Status = BdsLibConnectAllDriversToAllControllersEx();
    GlobalConfig.RescanDXE = TempRescanDXE;

    return Status;
} // BOOLEAN ApplyGOPFix()


/**
  Connects all drivers to all controllers.
  This function make sure all the current system driver will manage
  the correspoinding controllers if have. And at the same time, make
  sure all the system controllers have driver to manage it if have.
**/
VOID EFIAPI BdsLibConnectAllDriversToAllControllers (
    IN BOOLEAN ResetGOP
) {
    EFI_STATUS Status;
    BOOLEAN    KeyStrokeFound;

    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;
    #endif

    // Remove any buffered key strokes
    KeyStrokeFound = ReadAllKeyStrokes();
    if (!KeyStrokeFound && !AppleFirmware) {
        // No KeyStrokes found ... Reset the buffer on UEFI PC anyway
        REFIT_CALL_2_WRAPPER(gST->ConIn->Reset, gST->ConIn, FALSE);
    }

    Status = BdsLibConnectAllDriversToAllControllersEx();
    if (GlobalConfig.ReloadGOP) {
        if (EFI_ERROR(Status) && ResetGOP && !ReLoaded && DetectedDevices) {
            ReLoaded = TRUE;
            Status   = ApplyGOPFix();

            #if REFIT_DEBUG > 0
            if (!AcquireErrorGOP) {
                MsgStr = PoolPrint (L"Status:- '%r' ... Issue OptionROM From Volatile Memory", Status);
                ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
                LOG_MSG("%s      %s", OffsetNext, MsgStr);
                MY_FREE_POOL(MsgStr);
            }
            #endif

            ReLoaded = FALSE;
        }
    }
} // VOID BdsLibConnectAllDriversToAllControllers()
