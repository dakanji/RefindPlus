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

#include "Platform.h"
#include "../MainRP/lib.h"
#include "../MainRP/mystrings.h"
#include "../MainRP/launch_efi.h"
#include "../include/refit_call_wrapper.h"
#include "../../ShellPkg/Include/Library/HandleParsingLib.h"

BOOLEAN FoundGOP = FALSE;
BOOLEAN ReLoaded = FALSE;
BOOLEAN ConsoleControlFlag;

extern EFI_STATUS AmendSysTable (VOID);
extern EFI_STATUS AcquireGOP (VOID);


STATIC
EFI_STATUS
EFIAPI
daConnectController (
    IN  EFI_HANDLE               ControllerHandle,
    IN  EFI_HANDLE               *DriverImageHandle   OPTIONAL,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL,
    IN  BOOLEAN                  Recursive
) {
    EFI_STATUS  Status;
    VOID        *DevicePath;

    if (ControllerHandle == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    //
    // Do not connect controllers without device paths.
    // REF: https://bugzilla.tianocore.org/show_bug.cgi?id=2460
    //
    Status = gBS->HandleProtocol (
        ControllerHandle,
        &gEfiDevicePathProtocolGuid,
        &DevicePath
    );

    if (EFI_ERROR (Status)) {
        return EFI_NOT_STARTED;
    }

    Status = gBS->ConnectController (
        ControllerHandle,
        DriverImageHandle,
        RemainingDevicePath,
        Recursive
    );

    return Status;
} // EFI_STATUS daConnectController()

EFI_STATUS
ScanDeviceHandles (
    EFI_HANDLE ControllerHandle,
    UINTN      *HandleCount,
    EFI_HANDLE **HandleBuffer,
    UINT32     **HandleType
) {
    EFI_STATUS                          Status;
    EFI_GUID                            **ProtocolGuidArray;
    UINTN                               k;
    UINTN                               ArrayCount;
    UINTN                               ProtocolIndex;
    UINTN                               OpenInfoCount;
    UINTN                               OpenInfoIndex;
    UINTN                               ChildIndex;
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *OpenInfo;

    *HandleCount  = 0;
    *HandleBuffer = NULL;
    *HandleType   = NULL;
    //
    // Retrieve a list of handles with device paths
    // REF: https://bugzilla.tianocore.org/show_bug.cgi?id=2460
    //
    Status = gBS->LocateHandleBuffer (
        ByProtocol,
        &gEfiDevicePathProtocolGuid,
        NULL,
        HandleCount,
        HandleBuffer
    );


    if (EFI_ERROR (Status)) {
        goto Error;
    }

    *HandleType = AllocatePool (*HandleCount * sizeof (UINT32));

    if (*HandleType == NULL) {
        goto Error;
    }

    for (k = 0; k < *HandleCount; k++) {
        (*HandleType)[k] = EFI_HANDLE_TYPE_UNKNOWN;
        //
        // Retrieve a list of all the protocols on each handle
        //
        Status = gBS->ProtocolsPerHandle (
            (*HandleBuffer)[k],
            &ProtocolGuidArray,
            &ArrayCount
        );

        if (!EFI_ERROR (Status)) {
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

                //
                // Retrieve the list of agents that have opened each protocol
                //
                Status = gBS->OpenProtocolInformation (
                    (*HandleBuffer)[k],
                    ProtocolGuidArray[ProtocolIndex],
                    &OpenInfo,
                    &OpenInfoCount
                );

                if (!EFI_ERROR (Status)) {
                    for (OpenInfoIndex = 0; OpenInfoIndex < OpenInfoCount; OpenInfoIndex++) {
                        if (OpenInfo[OpenInfoIndex].ControllerHandle == ControllerHandle) {
                            if ((OpenInfo[OpenInfoIndex].Attributes &
                                EFI_OPEN_PROTOCOL_BY_DRIVER) == EFI_OPEN_PROTOCOL_BY_DRIVER
                            ) {
                                for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++) {
                                    if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].AgentHandle) {
                                        (*HandleType)[ChildIndex] |= EFI_HANDLE_TYPE_DEVICE_DRIVER;
                                    }
                                }
                            }
                            if ((OpenInfo[OpenInfoIndex].Attributes &
                                EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) == EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                            ) {
                                (*HandleType)[k] |= EFI_HANDLE_TYPE_PARENT_HANDLE;
                                for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++) {
                                    if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].AgentHandle) {
                                        (*HandleType)[ChildIndex] |= EFI_HANDLE_TYPE_BUS_DRIVER;
                                    }
                                }
                            }
                        }
                    }

                    MyFreePool (OpenInfo);
                }
            }

            MyFreePool (ProtocolGuidArray);
        }
    }

    return EFI_SUCCESS;

    Error:
    MyFreePool (*HandleType);
    MyFreePool (*HandleBuffer);

    *HandleCount  = 0;
    *HandleBuffer = NULL;
    *HandleType   = NULL;

    return Status;
} // EFI_STATUS ScanDeviceHandles()


EFI_STATUS
BdsLibConnectMostlyAllEfi (
    VOID
) {
    EFI_STATUS           XStatus;
    EFI_STATUS           Status           = EFI_SUCCESS;
    EFI_HANDLE           *AllHandleBuffer = NULL;
    EFI_HANDLE           *HandleBuffer    = NULL;
    UINTN                AllHandleCount;
    UINTN                AllHandleCountTrigger;
    UINTN                i;
    UINTN                k;
    UINTN                HandleCount;
    UINT32               *HandleType    = NULL;
    BOOLEAN              MakeConnection = TRUE;
    BOOLEAN              Parent;
    BOOLEAN              Device;
    PCI_TYPE00           Pci;
    EFI_PCI_IO_PROTOCOL* PciIo;

    UINTN      GOPCount;
    CHAR16     *GopDevicePathStr = NULL;
    CHAR16     *DevicePathStr    = NULL;
    CHAR16     *DeviceData       = NULL;
    EFI_HANDLE *GOPArray         = NULL;

    UINTN  SegmentPCI;
    UINTN  BusPCI;
    UINTN  DevicePCI;
    UINTN  FunctionPCI;
    UINTN  HexIndex = 0;
    UINTN  m;

    ConsoleControlFlag = FALSE;


    #if REFIT_DEBUG > 0
    if (ReLoaded) {
        MsgLog ("Reconnect Device Handles to Controllers...\n");
    }
    else {
        MsgLog ("Link Device Handles to Controllers...\n");
    }
    #endif
    // DISABLE scan all handles
    //Status = gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &AllHandleCount, &AllHandleBuffer);
    //
    // Only connect controllers with device paths.
    // REF: https://bugzilla.tianocore.org/show_bug.cgi?id=2460
    //
    Status = gBS->LocateHandleBuffer (
        ByProtocol,
        &gEfiDevicePathProtocolGuid,
        NULL,
        &AllHandleCount,
        &AllHandleBuffer
    );

    if (EFI_ERROR (Status)) {
        #if REFIT_DEBUG > 0
        MsgLog ("** ERROR: Could Not Locate Device Handles\n\n");
        #endif
    }
    else {
        AllHandleCountTrigger = (UINTN) AllHandleCount - 1;

        for (i = 0; i < AllHandleCount; i++) {
            HexIndex = ConvertHandleToHandleIndex (AllHandleBuffer[i]);
            MakeConnection = TRUE;
            DeviceData = NULL;

            XStatus = ScanDeviceHandles (
                AllHandleBuffer[i],
                &HandleCount,
                &HandleBuffer,
                &HandleType
            );

            if (EFI_ERROR (XStatus)) {
                #if REFIT_DEBUG > 0
                MsgLog ("Handle 0x%03X - FATAL: %r", HexIndex, XStatus);
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
                    MsgLog ("Handle 0x%03X ...Discarded [Not Device]", HexIndex);
                    #endif
                }
                else {
                    // Assume Not Parent
                    Parent = FALSE;

                    for (k = 0; k < HandleCount; k++) {
                        if (HandleType[k] & EFI_HANDLE_TYPE_PARENT_HANDLE) {
                            MakeConnection = FALSE;
                            Parent = TRUE;
                            break;
                        }
                    } // for

                    // Assume Success
                    XStatus = EFI_SUCCESS;

                    if (HandleType[i] & EFI_HANDLE_TYPE_DEVICE_HANDLE) {
                        XStatus = refit_call3_wrapper(
                            gBS->HandleProtocol,
                            AllHandleBuffer[i],
                            &gEfiPciIoProtocolGuid,
                            (void **) &PciIo
                        );

                        if (EFI_ERROR (XStatus)) {
                            #if REFIT_DEBUG > 0
                            DeviceData = L" - Not PCIe Device";
                            #endif
                        }
                        else {
                            // Read PCI BUS
                            PciIo->GetLocation (PciIo, &SegmentPCI, &BusPCI, &DevicePCI, &FunctionPCI);
                            XStatus = PciIo->Pci.Read (
                                PciIo,
                                EfiPciIoWidthUint32,
                                0,
                                sizeof (Pci) / sizeof (UINT32),
                                &Pci
                            );

                            if (EFI_ERROR (XStatus)) {
                                MakeConnection = FALSE;

                                #if REFIT_DEBUG > 0
                                DeviceData = L" - Could Not Read PCIe Device Details";
                                #endif
                            }
                            else {
                                BOOLEAN VGADevice = IS_PCI_VGA(&Pci);
                                if (VGADevice) {
                                    // DA-TAG: Unable to reconnect after disconnecting here
                                    // Comment out and set MakeConnection to FALSE
                                    // gBS->DisconnectController (AllHandleBuffer[i], NULL, NULL);
                                    MakeConnection = FALSE;

                                    #if REFIT_DEBUG > 0
                                    DeviceData = L" - Display Device ";
                                    #endif
                                }
                                else {
                                    #if REFIT_DEBUG > 0
                                    DeviceData = PoolPrint (
                                        L" - PCI(%02llX|%02llX:%02llX.%llX)",
                                        SegmentPCI,
                                        BusPCI,
                                        DevicePCI,
                                        FunctionPCI
                                    );
                                    #endif
                                } // VGADevice
                            }
                        } // if !EFI_ERROR (XStatus)
                    } // if HandleType[i] & EFI_HANDLE_TYPE_DEVICE_HANDLE

                    if (!FoundGOP) {
                        XStatus = refit_call5_wrapper(
                            gBS->LocateHandleBuffer,
                            ByProtocol,
                            &gEfiGraphicsOutputProtocolGuid,
                            NULL,
                            &GOPCount,
                            &GOPArray
                        );

                        if (!EFI_ERROR (XStatus)) {
                            for (m = 0; m < GOPCount; m++) {
                                if (GOPArray[m] != gST->ConsoleOutHandle) {
                                    GopDevicePathStr = ConvertDevicePathToText (
                                        DevicePathFromHandle (GOPArray[m]),
                                        FALSE,
                                        FALSE
                                    );

                                    FoundGOP = TRUE;
                                    break;
                                }
                            }
                        }

                        MyFreePool (GOPArray);
                    }

                    if (FoundGOP) {
                        DevicePathStr = ConvertDevicePathToText (
                            DevicePathFromHandle (AllHandleBuffer[i]),
                            FALSE,
                            FALSE
                        );

                        #if REFIT_DEBUG > 0
                        if (StrStr (GopDevicePathStr, DevicePathStr)) {
                            DeviceData = PoolPrint (
                                L"%s : Leverages GOP",
                                DeviceData
                            );
                        }
                        #endif

                        MyFreePool (DevicePathStr);
                    }
                    // Temp from Clover END

                    if (MakeConnection) {
                        XStatus = daConnectController (AllHandleBuffer[i], NULL, NULL, TRUE);
                    }

                    if (DeviceData == NULL) {
                        DeviceData = L"";
                    }

                    if (Parent) {
                        #if REFIT_DEBUG > 0
                        MsgLog ("Handle 0x%03X ...Skipped [Parent Device]%s", HexIndex, DeviceData);
                        #endif
                    }
                    else if (!EFI_ERROR (XStatus)) {
                        ConsoleControlFlag = TRUE;

                        #if REFIT_DEBUG > 0
                        MsgLog ("Handle 0x%03X  * %r                %s", HexIndex, XStatus, DeviceData);
                        #endif
                    }
                    else {
                        if (XStatus == EFI_NOT_STARTED) {
                            #if REFIT_DEBUG > 0
                            MsgLog ("Handle 0x%03X ...Declined [Empty Device]%s", HexIndex, DeviceData);
                            #endif
                        }
                        else if (XStatus == EFI_NOT_FOUND) {
                            #if REFIT_DEBUG > 0
                            MsgLog ("Handle 0x%03X ...Bypassed [Not Linkable]%s", HexIndex, DeviceData);
                            #endif
                        }
                        else if (XStatus == EFI_INVALID_PARAMETER) {
                            #if REFIT_DEBUG > 0
                            MsgLog ("Handle 0x%03X - ERROR: Invalid Param%s", HexIndex, DeviceData);
                            #endif
                        }
                        else {
                            #if REFIT_DEBUG > 0
                            MsgLog ("Handle 0x%03X - WARN: %r%s", HexIndex, XStatus, DeviceData);
                            #endif
                        }
                    } // if Parent

                } // if !Device
            } // if EFI_ERROR (XStatus)

            if (EFI_ERROR (XStatus)) {
                // Change Overall Status on Error
                Status = XStatus;
            }

            #if REFIT_DEBUG > 0
            if (i == AllHandleCountTrigger) {
                MsgLog ("\n\n");
            }
            else {
                MsgLog ("\n");
            }
            #endif

            MyFreePool (DeviceData);
        }  // for

        MyFreePool (GopDevicePathStr);
        MyFreePool (HandleBuffer);
        MyFreePool (HandleType);
    } // if !EFI_ERROR (Status)

	MyFreePool (AllHandleBuffer);

	return Status;
} // EFI_STATUS BdsLibConnectMostlyAllEfi()

/**
  Connects all drivers to all controllers.
  This function make sure all the current system driver will manage
  the correspoinding controllers if have. And at the same time, make
  sure all the system controllers have driver to manage it if have.
**/
STATIC
EFI_STATUS
BdsLibConnectAllDriversToAllControllersEx (
    VOID
) {
    EFI_STATUS  Status;
    EFI_STATUS  XStatus;

    do {
        FoundGOP = FALSE;

        // Connect All drivers
        XStatus = BdsLibConnectMostlyAllEfi();

        // Check if possible to dispatch additional DXE drivers as
        // BdsLibConnectAllEfi() may have revealed new DXE drivers.
        // If Dispatched Status == EFI_SUCCESS, attempt to reconnect.
        Status = gDS->Dispatch();

        #if REFIT_DEBUG > 0
        if (EFI_ERROR (Status)) {
            if (!FoundGOP && ConsoleControlFlag) {
                MsgLog ("INFO: Could Not Find Path to GOP on Any Device Handle\n\n");
            }
        }
        else {
            MsgLog ("INFO: Additional DXE Drivers Revealed ...Relink Handles\n\n");
        }
        #endif

    } while (!EFI_ERROR (Status));

    if (FoundGOP) {
        return EFI_SUCCESS;
    }
    else {
        return EFI_NOT_FOUND;
    }
} // EFI_STATUS BdsLibConnectAllDriversToAllControllersEx()

// Many cases of of GPUs not working on EFI 1.x Units such as Classic MacPros are due
// to the GPU's GOP drivers failing to install on not detecting UEFI 2.x. This function
// amends SystemTable Revision information, provides the missing CreateEventEx capability
// then reloads the GPU's ROM from RAM (If Present) which will install GOP (If Available).
EFI_STATUS
ApplyGOPFix (
    VOID
) {
    EFI_STATUS Status;

    // Update Boot Services to permit reloading GPU ROM
    Status = AmendSysTable();
    #if REFIT_DEBUG > 0
    MsgLog ("INFO: Amend System Table ...%r\n\n", Status);
    #endif

    if (!EFI_ERROR (Status)) {
        Status = AcquireGOP();
        #if REFIT_DEBUG > 0
        MsgLog ("INFO: Acquire GOP on Volatile Memory ...%r\n\n", Status);
        #endif

        // connect all devices
        if (!EFI_ERROR (Status)) {
            Status = BdsLibConnectAllDriversToAllControllersEx();
        }
    }

    return Status;
} // BOOLEAN ApplyGOPFix()


/**
  Connects all drivers to all controllers.
  This function make sure all the current system driver will manage
  the correspoinding controllers if have. And at the same time, make
  sure all the system controllers have driver to manage it if have.
**/
VOID
EFIAPI
BdsLibConnectAllDriversToAllControllers (
    IN BOOLEAN ResetGOP
) {
    EFI_STATUS Status;

    Status = BdsLibConnectAllDriversToAllControllersEx();
    if (EFI_ERROR (Status) && ResetGOP && !ReLoaded && ConsoleControlFlag) {
        ReLoaded = TRUE;
        Status = ApplyGOPFix();

        #if REFIT_DEBUG > 0
        MsgLog ("INFO: Issue GOP from Volatile Memory ...%r\n\n", Status);
        #endif

        ReLoaded = FALSE;
    }
} // VOID BdsLibConnectAllDriversToAllControllers()
