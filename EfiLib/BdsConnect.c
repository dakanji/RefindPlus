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
#include "../refind/lib.h"

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
}

EFI_STATUS
ScanDeviceHandles(
    EFI_HANDLE ControllerHandle,
    UINTN      *HandleCount,
    EFI_HANDLE **HandleBuffer,
    UINT32     **HandleType
) {
    EFI_STATUS                          Status;
    UINTN                               k;
    EFI_GUID                            **ProtocolGuidArray;
    UINTN                               ArrayCount;
    UINTN                               ProtocolIndex;
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *OpenInfo;
    UINTN                               OpenInfoCount;
    UINTN                               OpenInfoIndex;
    UINTN                               ChildIndex;

    *HandleCount  = 0;
    *HandleBuffer = NULL;
    *HandleType   = NULL;
    //
    // Retrieve a list of handles with device paths
    // REF: https://bugzilla.tianocore.org/show_bug.cgi?id=2460
    //
    Status = gBS->LocateHandleBuffer(
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
                if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiComponentNameProtocolGuid) ) {
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
                            if ((OpenInfo[OpenInfoIndex].Attributes
                                & EFI_OPEN_PROTOCOL_BY_DRIVER) == EFI_OPEN_PROTOCOL_BY_DRIVER
                            ) {
                                for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++) {
                                    if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].AgentHandle) {
                                        (*HandleType)[ChildIndex] |= EFI_HANDLE_TYPE_DEVICE_DRIVER;
                                    }
                                }
                            }
                            if ((OpenInfo[OpenInfoIndex].Attributes
                                & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) == EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
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

                    MyFreePool(OpenInfo);
                }
            }

            MyFreePool(ProtocolGuidArray);
        }
    }

    return EFI_SUCCESS;

    Error:
    MyFreePool(*HandleType);
    MyFreePool(*HandleBuffer);

    *HandleCount  = 0;
    *HandleBuffer = NULL;
    *HandleType   = NULL;

    return Status;
}

EFI_STATUS BdsLibConnectMostlyAllEfi()
{
    EFI_STATUS           Status = EFI_SUCCESS;
    EFI_STATUS           XStatus;
    EFI_HANDLE           *AllHandleBuffer = NULL;
    EFI_HANDLE           *HandleBuffer = NULL;
    UINTN                AllHandleCount;
    UINTN                AllHandleCountTrigger;
    UINTN                i;
    UINTN                k;
    UINTN                LogVal;
    UINTN                HandleCount;
    UINT32               *HandleType = NULL;
    BOOLEAN              Parent;
    BOOLEAN              Device;
    PCI_TYPE00           Pci;
    EFI_PCI_IO_PROTOCOL* PciIo;

    #if REFIT_DEBUG > 0
    MsgLog("Connect Device Handles to Controllers...\n");
    #endif
    //
    // Only connect controllers with device paths.
    // REF: https://bugzilla.tianocore.org/show_bug.cgi?id=2460
    //
    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiDevicePathProtocolGuid,
        NULL,
        &AllHandleCount,
        &AllHandleBuffer
    );

     AllHandleCountTrigger = (UINTN) AllHandleCount - 1;

    if (!EFI_ERROR (Status)) {
        for (i = 0; i < AllHandleCount; i++) {
            if (i > 999) {
                LogVal = 999;
            } else {
                LogVal = i;
            }

            XStatus = ScanDeviceHandles(
                AllHandleBuffer[i],
                &HandleCount,
                &HandleBuffer,
                &HandleType
            );

            if (EFI_ERROR (XStatus)) {
                #if REFIT_DEBUG > 0
                MsgLog("Handle[%03d]  - FATAL: %r", LogVal, XStatus);
                #endif
            } else {
                // Assume Success
                XStatus = EFI_SUCCESS;
                // Assume Device
                Device = TRUE;

                if (HandleType[i] & EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE) {
                    Device = FALSE;
                }
                if (HandleType[i] & EFI_HANDLE_TYPE_IMAGE_HANDLE) {
                    Device = FALSE;
                }

                if (!Device) {
                    #if REFIT_DEBUG > 0
                    MsgLog("Handle[%03d] ...Skipped [Not a Device]", LogVal);
                    #endif
                } else {
                    Parent = FALSE;
                    for (k = 0; k < HandleCount; k++) {
                        if (HandleType[k] & EFI_HANDLE_TYPE_PARENT_HANDLE) {
                            Parent = TRUE;
                        }
                    } // for

                    if (Parent) {
                        #if REFIT_DEBUG > 0
                        MsgLog("Handle[%03d] ...Skipped [Parent Device]", LogVal);
                        #endif
                    } else {
                        if (HandleType[i] & EFI_HANDLE_TYPE_DEVICE_HANDLE) {
                            XStatus = gBS->HandleProtocol (
                                AllHandleBuffer[i],
                                &gEfiPciIoProtocolGuid,
                                (VOID*)&PciIo
                            );

                            if (!EFI_ERROR (XStatus)) {
                                XStatus = PciIo->Pci.Read (
                                    PciIo,
                                    EfiPciIoWidthUint32,
                                    0,
                                    sizeof (Pci) / sizeof (UINT32),
                                    &Pci
                                );

                                if (!EFI_ERROR (XStatus)) {
                                    if(IS_PCI_VGA(&Pci)==TRUE) {
                                        gBS->DisconnectController(AllHandleBuffer[i], NULL, NULL);
                                    }
                                }
                            } // if !EFI_ERROR (XStatus)
                            XStatus = daConnectController(AllHandleBuffer[i], NULL, NULL, TRUE);
                        } // if HandleType[i] & EFI_HANDLE_TYPE_DEVICE_HANDLE

                        #if REFIT_DEBUG > 0
                        if (!EFI_ERROR (XStatus)) {
                            MsgLog("Handle[%03d]  * %r", LogVal, XStatus);
                        } else {
                            if (XStatus == EFI_NOT_STARTED) {
                                MsgLog("Handle[%03d] ...Declined [Empty Device]", LogVal);
                            } else if (XStatus == EFI_NOT_FOUND) {
                                MsgLog("Handle[%03d] ...Bypassed [Not Linkable]", LogVal);
                            } else if (XStatus == EFI_INVALID_PARAMETER) {
                                MsgLog("Handle[%03d]  - ERROR: Invalid Param", LogVal);
                            } else {
                                MsgLog("Handle[%03d]  - WARN: %r", LogVal, XStatus);
                            }
                        } // if !EFI_ERROR (XStatus)
                        #endif
                    } // if Parent
                } // if !Device
            } // if EFI_ERROR (XStatus)

            if (EFI_ERROR (XStatus)) {
                // Change Overall Status on Error
                Status = XStatus;
            }

            #if REFIT_DEBUG > 0
            if (i == AllHandleCountTrigger) {
                MsgLog("\n\n", LogVal, XStatus);
            } else {
                MsgLog("\n", LogVal, XStatus);
            }
            #endif

        }  // for

        MyFreePool(HandleBuffer);
        MyFreePool(HandleType);
    } // if !EFI_ERROR (Status)

	MyFreePool(AllHandleBuffer);

	return Status;
}



/**
  Connects all drivers to all controllers.
  This function make sure all the current system driver will manage
  the correspoinding controllers if have. And at the same time, make
  sure all the system controllers have driver to manage it if have.

**/
VOID
EFIAPI
BdsLibConnectAllDriversToAllControllers (VOID)
{
    EFI_STATUS  Status;

    do {
        //
        // Connect All EFI 1.10 drivers following EFI 1.10 algorithm
        //
        //BdsLibConnectAllEfi ();
        BdsLibConnectMostlyAllEfi ();

        //
        // Check to see if it's possible to dispatch an more DXE drivers.
        // The BdsLibConnectAllEfi () may have made new DXE drivers show up.
        // If anything is Dispatched Status == EFI_SUCCESS and we will try
        // the connect again.
        //
        Status = gDS->Dispatch ();
    } while (!EFI_ERROR (Status));
}
