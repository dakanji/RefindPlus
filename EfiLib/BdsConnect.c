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
#include "../include/refit_call_wrapper.h"
#include "../Library/GpuLib/OneLinerMacros.h"
#include "../Library/GpuLib/ati.h"
#include "../Library/GpuLib/nvidia.h"
#include "../Library/GpuLib/intel.h"

#define PCI_CLASS_DISPLAY_GFX 0x80
#define IS_PCI_GFX(_p)        IS_CLASS3 (_p, PCI_CLASS_DISPLAY, PCI_CLASS_DISPLAY_GFX, 0)

BOOLEAN FoundGOP = FALSE;
BOOLEAN ReLoaded = FALSE;

typedef enum {
  Unknown,
  AMD,    /* 0x1002 */
  Intel,  /* 0x8086 */
  Nvidia, /* 0x10de */
  RDC,    /* 0x17f3 */
  VIA,    /* 0x1106 */
  SiS,    /* 0x1039 */
  ULI     /* 0x10b9 */
} HRDW_MANUFACTURER;

CONST
CHAR16 *get_nvidia_model (
    UINT32 device_id,
    UINT32 subsys_id
) {
  UINTN i, j;
  CHAR16 *generic_name;

    // First check exceptions table
    if (subsys_id) {
      for (i = 0; i < (sizeof (nvidia_card_exceptions) / sizeof (nvidia_card_exceptions[0])); i++) {
        if ((nvidia_card_exceptions[i].device == device_id) &&
            (nvidia_card_exceptions[i].subdev == subsys_id)) {
          return (CHAR16 *) nvidia_card_exceptions[i].name_model;
        }
      }
    }

  // Then, try generic names
  for (i = 1; i < (sizeof (nvidia_card_generic) / sizeof (nvidia_card_generic[0])); i++) {
    if (nvidia_card_generic[i].device == device_id) {
      if (subsys_id) {
        for (j = 0; j < (sizeof (nvidia_card_vendors) / sizeof (nvidia_card_vendors[0])); j++) {
          if (nvidia_card_vendors[j].device == (subsys_id & 0xffff0000)) {
              generic_name = PoolPrint (
                  L"%s %s",
                  nvidia_card_vendors[j].name_model,
                  nvidia_card_generic[i].name_model
              );

            return generic_name;
          }
        }
      }
      return (CHAR16 *) nvidia_card_generic[i].name_model;
    }
  }
  return (CHAR16 *) nvidia_card_generic[0].name_model;
}

CONST CHAR16 *get_gma_model (
    IN UINT16 DeviceID
) {
  UINTN i;

  for (i = 0; i < (sizeof (KnownGPUS) / sizeof (KnownGPUS[0])); i++)
  {
    if (KnownGPUS[i].device == DeviceID)
      return (CHAR16 *) KnownGPUS[i].name;
  }
  return (CHAR16 *) KnownGPUS[0].name;
}

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
ScanDeviceHandles (
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
}

EFI_STATUS BdsLibConnectMostlyAllEfi()
{
    EFI_STATUS           Status = EFI_SUCCESS;
    EFI_STATUS           XStatus;
    EFI_HANDLE           *AllHandleBuffer = NULL;
    EFI_HANDLE           *HandleBuffer    = NULL;
    UINTN                AllHandleCount;
    UINTN                AllHandleCountTrigger;
    UINTN                i;
    UINTN                k;
    UINTN                x;
    UINTN                LogVal;
    UINTN                HandleCount;
    UINT32               *HandleType = NULL;
    BOOLEAN              Parent;
    BOOLEAN              MakeConnection = TRUE;
    BOOLEAN              Device;
    PCI_TYPE00           Pci;
    EFI_PCI_IO_PROTOCOL* PciIo;

    HRDW_MANUFACTURER    GfxVendor;
    UINT8                *GfxMmio;
    UINT16               UFamily;
    UINT16               GfxFamily;
    UINT32               Bar0;
    CHAR16               *DeviceData = NULL;
    CHAR16               *GfxModel   = NULL;
    CHAR16               *CardFamily = NULL;
    radeon_card_info_t   *info;


    UINTN      GOPCount;
    EFI_HANDLE *GOPArray         = NULL;
    CHAR16     *GopDevicePathStr = NULL;
    CHAR16     *DevicePathStr    = NULL;


    UINTN               SegmentPCI;
    UINTN               BusPCI;
    UINTN               DevicePCI;
    UINTN               FunctionPCI;



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
        MsgLog ("** ERROR: Could not Locate Device Handles\n\n");
        #endif
    }
    else {
        AllHandleCountTrigger = (UINTN) AllHandleCount - 1;

        for (i = 0; i < AllHandleCount; i++) {
            MakeConnection = TRUE;
            DeviceData = NULL;
            if (i > 999) {
                LogVal = 999;
            }
            else {
                LogVal = i;
            }

            XStatus = ScanDeviceHandles (
                AllHandleBuffer[i],
                &HandleCount,
                &HandleBuffer,
                &HandleType
            );

            if (EFI_ERROR (XStatus)) {
                #if REFIT_DEBUG > 0
                MsgLog ("Handle[%03d] - FATAL: %r", LogVal, XStatus);
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
                    MsgLog ("Handle[%03d] ...Discarded [Not Device]", LogVal);
                    #endif
                }
                else {
                    // Assume Not Parent
                    Parent = FALSE;

                    for (k = 0; k < HandleCount; k++) {
                        if (HandleType[k] & EFI_HANDLE_TYPE_PARENT_HANDLE) {
                            Parent = TRUE;
                            break;
                        }
                    } // for

                    //if (Parent) {
                    //    #if REFIT_DEBUG > 0
                    //    MsgLog ("Handle[%03d] ...Skipped [Parent Device]", LogVal);
                    //    #endif
                    //}
                    //else {
                        // Assume Success
                        XStatus = EFI_SUCCESS;

                        if (HandleType[i] & EFI_HANDLE_TYPE_DEVICE_HANDLE) {
                            XStatus = gBS->HandleProtocol (
                                AllHandleBuffer[i],
                                &gEfiPciIoProtocolGuid,
                                (VOID*)&PciIo
                            );

                            if (EFI_ERROR (XStatus)) {
                                DeviceData = L" - Not PCIe Device";
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
                                    DeviceData = L" - Could not Read PCIe Device Details";
                                }
                                //else if (IS_PCI_VGA(&Pci)==TRUE) {
                                //    MakeConnection = FALSE;
                                //    //gBS->DisconnectController (AllHandleBuffer[i], NULL, NULL);
                                //    DeviceData = L" - Display Device  ";
                                //}
                                else {
                                    if (IS_PCI_VGA(&Pci)==TRUE) {
                                        MakeConnection = FALSE;
                                        //gBS->DisconnectController (AllHandleBuffer[i], NULL, NULL);
                                        DeviceData = L" - Display Device ";
                                    }
                                    else {
                                        DeviceData = PoolPrint (
                                            L" - PCI(%02llX|%02llX:%02llX.%llX)",
                                            SegmentPCI,
                                            BusPCI,
                                            DevicePCI,
                                            FunctionPCI
                                        );
                                    }
                                    //BOOLEAN GPUDevice = ((Pci.Hdr.ClassCode[2] == PCI_CLASS_DISPLAY) &&
                                    //    ((Pci.Hdr.ClassCode[1] == PCI_CLASS_DISPLAY_VGA) ||
                                    //    (Pci.Hdr.ClassCode[1] == PCI_CLASS_DISPLAY_OTHER) ) );

                                    BOOLEAN GPUDevice = IS_PCI_GFX(&Pci);
                                    if (GPUDevice) {
                                        switch (Pci.Hdr.VendorId) {
                                            case 0x1002:
                                                info      = NULL;
                                                GfxVendor = AMD;

                                                x = 0;
                                                do {
                                                    info = &radeon_cards[x];
                                                    if (info->device_id == Pci.Hdr.DeviceId) {
                                                        break;
                                                    }
                                                } while (radeon_cards[x++].device_id != 0);

                                                GfxModel = PoolPrint (L"%s", info->model_name);
                                                DeviceData = PoolPrint (
                                                    L" - GPU Device      : %s by AMD",
                                                    GfxModel
                                                );

                                                break;

                                            case 0x8086:
                                                GfxVendor = Intel;

                                                GfxModel = PoolPrint (
                                                    L"%s",
                                                    get_gma_model (Pci.Hdr.DeviceId)
                                                );
                                                DeviceData = PoolPrint (
                                                    L" - GPU Device      : %s by Intel",
                                                    GfxModel
                                                );

                                                break;

                                            case 0x10de:
                                                GfxVendor = Nvidia;
                                                Bar0        = Pci.Device.Bar[0];
                                                GfxMmio   = (UINT8*) (UINTN) (Bar0 & ~0x0f);

                                                // get card type
                                                GfxFamily = (REG32 (GfxMmio, 0) >> 20) & 0x1ff;
                                                UFamily = GfxFamily & 0x1F0;
                                                if ((UFamily == NV_ARCH_KEPLER1) ||
                                                    (UFamily == NV_ARCH_KEPLER2) ||
                                                    (UFamily == NV_ARCH_KEPLER3)
                                                ) {
                                                    CardFamily = L"Kepler";
                                                }
                                                else if ((UFamily == NV_ARCH_FERMI1) ||
                                                    (UFamily == NV_ARCH_FERMI2)
                                                ) {
                                                    CardFamily = L"Fermi";
                                                }
                                                else if ((UFamily == NV_ARCH_MAXWELL1) ||
                                                    (UFamily == NV_ARCH_MAXWELL2)
                                                ) {
                                                    CardFamily = L"Maxwell";
                                                }
                                                else if (UFamily == NV_ARCH_PASCAL) {
                                                    CardFamily = L"Pascal";
                                                }
                                                else if (UFamily == NV_ARCH_VOLTA) {
                                                    CardFamily = L"Volta";
                                                }
                                                else if (UFamily == NV_ARCH_TURING) {
                                                    CardFamily = L"Turing";
                                                }
                                                else if ((UFamily >= NV_ARCH_TESLA) &&
                                                    (UFamily < 0xB0)
                                                ) { //not sure if 0xB0 is Tesla or Fermi
                                                    CardFamily = L"Tesla";
                                                }
                                                else {
                                                    CardFamily = L"Unknown";
                                                }

                                                GfxModel = PoolPrint (
                                                    L"%s",
                                                    get_nvidia_model (
                                                        ((Pci.Hdr.VendorId << 16) | Pci.Hdr.DeviceId),
                                                        ((Pci.Device.SubsystemVendorID << 16) | Pci.Device.SubsystemID)
                                                    )
                                                );
                                                DeviceData = PoolPrint (
                                                    L" - GPU Device      : %s (%s Card Family) by NVidia",
                                                    GfxModel,
                                                    CardFamily
                                                );

                                                break;

                                            default:
                                            GfxVendor = Unknown;
                                            GfxModel = PoolPrint (
                                                L"pci%hx[%hx]",
                                                Pci.Hdr.VendorId,
                                                Pci.Hdr.DeviceId
                                            );
                                            DeviceData = PoolPrint (
                                                L" - GPU Device      : %s by Unknown Vendor",
                                                GfxModel
                                            );

                                            break;
                                        }
                                    } // IS_PCI_GFX

                                }
                            } // if !EFI_ERROR (XStatus)
                        } // if HandleType[i] & EFI_HANDLE_TYPE_DEVICE_HANDLE

                        // Temp from Clover START
                        if (!FoundGOP) {
                            XStatus = refit_call5_wrapper (
                                gBS->LocateHandleBuffer,
                                ByProtocol,
                                &gEfiGraphicsOutputProtocolGuid,
                                NULL,
                                &GOPCount,
                                &GOPArray
                            );

                            if (!EFI_ERROR (XStatus)) {
                                UINTN m;
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
                        }
                        // Temp from Clover END

                        if (MakeConnection) {
                            XStatus = daConnectController (AllHandleBuffer[i], NULL, NULL, TRUE);
                        }

                        #if REFIT_DEBUG > 0
                        if (DeviceData == NULL) {
                            DeviceData == L"";
                        }

                        if (Parent) {
                            MsgLog ("Handle[%03d] ...Skipped [Parent Device]%s", LogVal, DeviceData);
                        }
                        else if (!EFI_ERROR (XStatus)) {
                            MsgLog ("Handle[%03d]  * %r                %s", LogVal, XStatus, DeviceData);
                        }
                        else {
                            if (XStatus == EFI_NOT_STARTED) {
                                MsgLog ("Handle[%03d] ...Declined [Empty Device]%s", LogVal, DeviceData);
                            }
                            else if (XStatus == EFI_NOT_FOUND) {
                                MsgLog ("Handle[%03d] ...Bypassed [Not Linkable]%s", LogVal, DeviceData);
                            }
                            else if (XStatus == EFI_INVALID_PARAMETER) {
                                MsgLog ("Handle[%03d] - ERROR: Invalid Param%s", LogVal, DeviceData);
                            }
                            else {
                                MsgLog ("Handle[%03d] - WARN: %r%s", LogVal, XStatus, DeviceData);
                            }
                        } // if !EFI_ERROR (XStatus)
                        #endif
                    //} // if Parent
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

        #if REFIT_DEBUG > 0
        if (FoundGOP) {
            MsgLog ("INFO: Found Path to GOP on One or More Device Handles\n\n");
        }
        else {
            MsgLog ("INFO: Could Not Find Path to GOP on Any Device Handle\n\n");
        }
        #endif

        MyFreePool (HandleBuffer);
        MyFreePool (HandleType);
    } // if !EFI_ERROR (Status)

	MyFreePool (AllHandleBuffer);

	return Status;
}

/**
  Connects all drivers to all controllers.
  This function make sure all the current system driver will manage
  the correspoinding controllers if have. And at the same time, make
  sure all the system controllers have driver to manage it if have.
**/
STATIC
EFI_STATUS
BdsLibConnectAllDriversToAllControllersEx (VOID)
{
    EFI_STATUS  Status;
    EFI_STATUS  XStatus;

    do {
        //
        // Connect All EFI 1.10 drivers following EFI 1.10 algorithm
        //
        //BdsLibConnectAllEfi();
        XStatus = BdsLibConnectMostlyAllEfi();

        //
        // Check to see if it's possible to dispatch an more DXE drivers.
        // The BdsLibConnectAllEfi() may have made new DXE drivers show up.
        // If anything is Dispatched Status == EFI_SUCCESS and we will try
        // the connect again.
        //
        Status = gDS->Dispatch();
    } while (!EFI_ERROR (Status));

    if (FoundGOP) {
        return EFI_SUCCESS;
    }
    else {
        return EFI_NOT_FOUND;
    }
}

// Scan a directory for drivers.
// Originally from rEFIt's main.c (BSD), but modified since then (GPLv3).
static UINTN
ScanDriverDirGOP(
    IN CHAR16 *Path
) {
    EFI_STATUS              Status;
    REFIT_DIR_ITER          DirIter;
    UINTN                   NumFound = 0;
    EFI_FILE_INFO           *DirEntry;
    CHAR16                  *FileName;

    CleanUpPathNameSlashes(Path);

    #if REFIT_DEBUG > 0
    MsgLog("\n");
    MsgLog("Scan '%s' Folder:\n", Path);
    #endif

    // look through contents of the directory
    DirIterOpen(SelfRootDir, Path, &DirIter);

    #if REFIT_DEBUG > 0
    BOOLEAN RunOnce = FALSE;
    #endif

    while (DirIterNext(&DirIter, 2, LOADER_MATCH_PATTERNS, &DirEntry)) {
        if (DirEntry->FileName[0] == '.') {
            continue;   // skip this
        }

        // needed here in case next block returns error message
        #if REFIT_DEBUG > 0
        if (RunOnce) {
            MsgLog("\n");
        }
        #endif

        FileName = PoolPrint(L"%s\\%s", Path, DirEntry->FileName);
        NumFound++;
        Status = StartEFIImage(SelfVolume, FileName, L"", DirEntry->FileName, 0, FALSE, TRUE);

        #if REFIT_DEBUG > 0
        MsgLog("  - Load '%s' ...%r", FileName, Status);
        #endif

        MyFreePool(DirEntry);
        #if REFIT_DEBUG > 0
        RunOnce = TRUE;
        #endif
    } // while

    Status = DirIterClose(&DirIter);
    if ((Status != EFI_NOT_FOUND) && (Status != EFI_INVALID_PARAMETER)) {
        SPrint(FileName, 255, L"while scanning the %s directory", Path);
        CheckError(Status, FileName);
    }

    return (NumFound);
} // static UINTN ScanDriverDirGOP()


// Load all EFI drivers from rEFInd's "drivers" subdirectory and from the
// directories specified by the user in the "scan_driver_dirs" configuration
// file line.
// Originally from rEFIt's main.c (BSD), but modified since then (GPLv3).
// Returns TRUE if any drivers are loaded, FALSE otherwise.
BOOLEAN
LoadDriversGOP(
    VOID
) {
    CHAR16     *Directory;
    CHAR16     *SelfDirectory;
    UINTN      Length;
    UINTN      i = 0;
    UINTN      NumFound = 0;
    UINTN      CurFound = 0;
    EFI_STATUS Status = EFI_NOT_READY;

    // load drivers from the subdirectories of rEFInd's home directory specified
    #if REFIT_DEBUG > 0
    MsgLog("Load Drivers for GOPFix...");
    #endif

    Directory = L"x64_GOPFix";
    SelfDirectory = SelfDirPath ? StrDuplicate(SelfDirPath) : NULL;
    CleanUpPathNameSlashes(SelfDirectory);
    MergeStrings(&SelfDirectory, Directory, L'\\');
    CurFound = ScanDriverDirGOP(SelfDirectory);
    MyFreePool(Directory);
    MyFreePool(SelfDirectory);
    if (CurFound > 0) {
        NumFound = NumFound + CurFound;
    }
    else {
        #if REFIT_DEBUG > 0
        MsgLog("  - Not Found or Empty", SelfDirectory);
        #endif
    }

    #if REFIT_DEBUG > 0
    MsgLog("\n\n");
    #endif

    // connect all devices
    if (NumFound > 0) {
        Status = BdsLibConnectAllDriversToAllControllersEx();
    }

    return Status;
} /* BOOLEAN LoadDriversGOP() */


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
    EFI_STATUS Status;

    Status = BdsLibConnectAllDriversToAllControllersEx();
    if (EFI_ERROR (Status) && !ReLoaded) {
        #if REFIT_DEBUG > 0
        MsgLog ("INFO: Attempt GOP Fix\n\n");
        #endif

        ReLoaded = TRUE;
        Status = LoadDriversGOP();

        #if REFIT_DEBUG > 0
        MsgLog ("INFO: Install GOP from Graphics Device ...%r\n\n", Status);
        #endif
    }
}
