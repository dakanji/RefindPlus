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


EFI_STATUS ScanDeviceHandles(EFI_HANDLE ControllerHandle,
                             UINTN *HandleCount,
                             EFI_HANDLE **HandleBuffer,
                             UINT32 **HandleType)
{
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
  // Retrieve the list of all handles from the handle database
  //
  Status = gBS->LocateHandleBuffer (AllHandles, NULL, NULL, HandleCount, HandleBuffer);
  if (EFI_ERROR (Status)) goto Error;

  *HandleType = AllocatePool (*HandleCount * sizeof (UINT32));
  if (*HandleType == NULL) goto Error;

  for (k = 0; k < *HandleCount; k++) {
    (*HandleType)[k] = EFI_HANDLE_TYPE_UNKNOWN;
    //
    // Retrieve the list of all the protocols on each handle
    //
    Status = gBS->ProtocolsPerHandle (
        (*HandleBuffer)[k],
        &ProtocolGuidArray,
        &ArrayCount
    );
    
    if (!EFI_ERROR (Status)) {
      for (ProtocolIndex = 0; ProtocolIndex < ArrayCount; ProtocolIndex++)
      {

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiLoadedImageProtocolGuid))
        {
          (*HandleType)[k] |= EFI_HANDLE_TYPE_IMAGE_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDriverBindingProtocolGuid))
        {
          (*HandleType)[k] |= EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDriverConfigurationProtocolGuid))
        {
          (*HandleType)[k] |= EFI_HANDLE_TYPE_DRIVER_CONFIGURATION_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDriverDiagnosticsProtocolGuid))
        {
          (*HandleType)[k] |= EFI_HANDLE_TYPE_DRIVER_DIAGNOSTICS_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiComponentName2ProtocolGuid))
        {
          (*HandleType)[k] |= EFI_HANDLE_TYPE_COMPONENT_NAME_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiComponentNameProtocolGuid) )
        {
          (*HandleType)[k] |= EFI_HANDLE_TYPE_COMPONENT_NAME_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDevicePathProtocolGuid))
        {
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

            if (OpenInfo[OpenInfoIndex].ControllerHandle == ControllerHandle)
            {
              if ((OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) == EFI_OPEN_PROTOCOL_BY_DRIVER)
              {
                for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++)
                {
                  if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].AgentHandle)
                  {
                    (*HandleType)[ChildIndex] |= EFI_HANDLE_TYPE_DEVICE_DRIVER;
                  }
                }
              }

              if ((OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) == EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER)
              {
                (*HandleType)[k] |= EFI_HANDLE_TYPE_PARENT_HANDLE;
                for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++)
                {
                  if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].AgentHandle)
                  {
                    (*HandleType)[ChildIndex] |= EFI_HANDLE_TYPE_BUS_DRIVER;
                  }
                }
              }
            }
          }

          FreePool (OpenInfo);
        }
      }

      FreePool (ProtocolGuidArray);
    }
  }

  return EFI_SUCCESS;

Error:
  if (*HandleType != NULL) {
    FreePool (*HandleType);
  }

  if (*HandleBuffer != NULL) {
    FreePool (*HandleBuffer);
  }

  *HandleCount  = 0;
  *HandleBuffer = NULL;
  *HandleType   = NULL;

  return Status;
}

EFI_STATUS BdsLibConnectMostlyAllEfi()
{
    EFI_STATUS				Status = EFI_SUCCESS;
    EFI_STATUS				XStatus;
	UINTN                   AllHandleCount;
    UINTN                   AllHandleCountTrigger;
	EFI_HANDLE				*AllHandleBuffer;
	UINTN                   i;
    UINTN                   k;
	UINTN                   HandleCount;
	EFI_HANDLE				*HandleBuffer;
	UINT32                  *HandleType;
	BOOLEAN                 Parent;
	BOOLEAN                 Device;
	EFI_PCI_IO_PROTOCOL*	PciIo;
	PCI_TYPE00				Pci;

    #if REFIT_DEBUG > 0
    MsgLog("Connect DeviceHandles to Controllers...\n");
    #endif

    Status = gBS->LocateHandleBuffer(
        AllHandles,
        NULL,
        NULL,
        &AllHandleCount,
        &AllHandleBuffer
    );

     AllHandleCountTrigger = (UINTN) AllHandleCount - 1;

    if (!EFI_ERROR (Status)) {
        for (i = 0; i < AllHandleCount; i++) {
            // Assume Success
            XStatus = EFI_SUCCESS;

            XStatus = ScanDeviceHandles(
                AllHandleBuffer[i],
                &HandleCount,
                &HandleBuffer,
                &HandleType
            );

            if (!EFI_ERROR (XStatus)) {
                Device = TRUE;

                if (HandleType[i] & EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE) {
                    Device = FALSE;
                }
                if (HandleType[i] & EFI_HANDLE_TYPE_IMAGE_HANDLE) {
                    Device = FALSE;
                }

                if (Device) {
                    Parent = FALSE;
                    for (k = 0; k < HandleCount; k++) {
                        if (HandleType[k] & EFI_HANDLE_TYPE_PARENT_HANDLE) {
                            Parent = TRUE;
                        }
                    } // for

                    if (!Parent) {
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
                            }
                            XStatus = gBS->ConnectController(AllHandleBuffer[i], NULL, NULL, TRUE);
                        }

                        #if REFIT_DEBUG > 0
                        if (EFI_ERROR (XStatus)) {
                            if (i == AllHandleCountTrigger) {
                                MsgLog("Connect DeviceHandle[%d] ...WARN: %r\n\n", i, XStatus);
                            } else {
                                MsgLog("Connect DeviceHandle[%d] ...WARN: %r\n", i, XStatus);
                            }
                        } else {
                            if (i == AllHandleCountTrigger) {
                                MsgLog("Connect DeviceHandle[%d] ...%r\n\n", i, XStatus);
                            } else {
                                MsgLog("Connect DeviceHandle[%d] ...%r\n", i, XStatus);
                            }
                        }
                    } else {
                        if (i == AllHandleCountTrigger) {
                            MsgLog("Connect DeviceHandle[%d] ...Skipped [Parent Device]\n\n", i);
                        } else {
                            MsgLog("Connect DeviceHandle[%d] ...Skipped [Parent Device]\n", i);
                        }
                        #endif
                    }

                    #if REFIT_DEBUG > 0
                } else {
                    if (i == AllHandleCountTrigger) {
                        MsgLog("Connect DeviceHandle[%d] ...Skipped [Not Device]\n\n", i);
                    } else {
                        MsgLog("Connect DeviceHandle[%d] ...Skipped [Not Device]\n", i);
                    }
                    #endif
                }

                #if REFIT_DEBUG > 0
            } else {
                if (EFI_ERROR (XStatus)) {
                    if (i == AllHandleCountTrigger) {
                        MsgLog("Connect DeviceHandle[%d] ...WARN: %r\n\n", i, XStatus);
                    } else {
                        MsgLog("Connect DeviceHandle[%d] ...WARN: %r\n", i, XStatus);
                    }
                } else {
                    if (i == AllHandleCountTrigger) {
                        MsgLog("Connect DeviceHandle[%d] ...%r\n\n", i, XStatus);
                    } else {
                        MsgLog("Connect DeviceHandle[%d] ...%r\n", i, XStatus);
                    }
                }
                #endif
            }

            if (EFI_ERROR (XStatus)) {
                // Change Overall Status on Error
                Status = XStatus;
            }
        }  // for
        FreePool (HandleBuffer);
        FreePool (HandleType);
    }
	FreePool (AllHandleBuffer);

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
