/** @file
BDS internal function define the default device path string, it can be
replaced by platform device path.

Copyright (c) 2004 - 2009, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/
/**
 * Modified for RefindPlus
 * Copyright (c) 2021-2024 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
**/

#include "Platform.h"
#include "../BootMaster/rp_funcs.h"
#include "../include/refit_call_wrapper.h"

/**
Concatenates a formatted unicode string to allocated pool.
The caller must free the resulting buffer.

@param  Str      Tracks the allocated pool, size in use, and amount of pool allocated.
@param  Fmt      The format string
@param  ...      The data will be printed.

@return Allocated buffer with the formatted string printed in it.
The caller must free the allocated buffer.
The buffer allocation is not packed.
**/
CHAR16 * EFIAPI MyCatPrint (
    IN OUT POOL_PRINT *Str,
    IN     CHAR16     *Fmt,
    ...
) {
    UINT16  *AppendStr;
    VA_LIST  Args;
    UINTN    StringSize;

    AppendStr = AllocateZeroPool (0x1000);
    if (AppendStr == NULL) {
        return Str->Str;
    }

    VA_START(Args, Fmt);
    UnicodeVSPrint (AppendStr, 0x1000, Fmt, Args);
    VA_END(Args);

    if (NULL == Str->Str) {
        StringSize = StrSize (AppendStr);
        Str->Str   = AllocateZeroPool (StringSize);
    }
    else {
        StringSize  = StrSize (AppendStr);
        StringSize += (StrSize (Str->Str) - sizeof (UINT16));

        Str->Str = EfiReallocatePool (
            Str->Str,
            StrSize (Str->Str),
            StringSize
        );
    }

    if (Str->Str != NULL) {
        Str->Maxlen = MAX_CHAR * sizeof (UINT16);

        if (StringSize < Str->Maxlen) {
            StrCat (Str->Str, AppendStr);
            Str->Len = StringSize - sizeof (UINT16);
        }

        MY_FREE_POOL(AppendStr);
    }

    return Str->Str;
} // CHAR16 * EFIAPI MyCatPrint()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathPci (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    PCI_DEVICE_PATH *Pci = DevPath;

    MyCatPrint (Str, L"Pci(%x|%x)", (UINTN) Pci->Device, (UINTN) Pci->Function);
} // VOID DevPathPci()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathPccard (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    PCCARD_DEVICE_PATH  *Pccard;

    Pccard = DevPath;
    MyCatPrint (Str, L"Pcmcia(Function%x)", (UINTN) Pccard->FunctionNumber);
} // VOID DevPathPccard()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathMemMap (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    MEMMAP_DEVICE_PATH  *MemMap;

    MemMap = DevPath;
    MyCatPrint (
        Str,
        L"MemMap(%d:%lx-%lx)",
        (UINTN) MemMap->MemoryType,
        MemMap->StartingAddress,
        MemMap->EndingAddress
    );
} // VOID DevPathMemMap()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathController (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    CONTROLLER_DEVICE_PATH  *Controller;

    Controller = DevPath;
    MyCatPrint (Str, L"Ctrl(%d)", (UINTN) Controller->ControllerNumber);
} // VOID DevPathController()

/**
Convert Vendor device path to device name.

@param  Str      The buffer store device name
@param  DevPath  Pointer to vendor device path
**/
VOID DevPathVendor (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    VENDOR_DEVICE_PATH  *Vendor;
    CHAR16              *Type;
    UINTN                DataLength;
    UINTN                Index;
    UINT16               Info;
    //UINT32               FlowControlMap;

    Vendor = DevPath;

    switch (DevicePathType (&Vendor->Header)) {
        case HARDWARE_DEVICE_PATH:  Type = L"Hw";   break;
        case MESSAGING_DEVICE_PATH: Type = L"Msg";
/*
            if (CompareGuid (&Vendor->Guid, &gEfiPcAnsiGuid)) {
                MyCatPrint (Str, L"VenPcAnsi()");
                return;
            }
            else if (CompareGuid (&Vendor->Guid, &gEfiVT100Guid)) {
                MyCatPrint (Str, L"VenVt100()");
                return;
            }
            else if (CompareGuid (&Vendor->Guid, &gEfiVT100PlusGuid)) {
                MyCatPrint (Str, L"VenVt100Plus()");
                return;
            }
            else if (CompareGuid (&Vendor->Guid, &gEfiVTUTF8Guid)) {
                MyCatPrint (Str, L"VenUft8()");
                return;
            }
            else if (CompareGuid (&Vendor->Guid, &gEfiUartDevicePathGuid)) {
                FlowControlMap = (((UART_FLOW_CONTROL_DEVICE_PATH *) Vendor)->FlowControlMap);

                switch (FlowControlMap & 0x00000003) {
                    case 0: MyCatPrint (Str, L"UartFlowCtrl(%s)", L"None");       break;
                    case 1: MyCatPrint (Str, L"UartFlowCtrl(%s)", L"Hardware");   break;
                    case 2: MyCatPrint (Str, L"UartFlowCtrl(%s)", L"XonXoff");    break;
                    default:                                                      break;
                } // switch

                return;
            }
            else
*/
            if (CompareGuid (&Vendor->Guid, &gEfiSasDevicePathGuid)) {
                MyCatPrint (
                    Str,
                    L"SAS(%lx,%lx,%x,",
                    ((SAS_DEVICE_PATH *) Vendor)->SasAddress,
                    ((SAS_DEVICE_PATH *) Vendor)->Lun,
                    (UINTN) ((SAS_DEVICE_PATH *) Vendor)->RelativeTargetPort
                );

                Info = (((SAS_DEVICE_PATH *) Vendor)->DeviceTopology);

                if ((Info & 0x0f) == 0) {
                    MyCatPrint (Str, L"NoTopology,0,0,0,");
                }
                else if (((Info & 0x0f) == 1) || ((Info & 0x0f) == 2)) {
                    MyCatPrint (
                        Str,
                        L"%s,%s,%s,",
                        ((Info & (0x1 << 4)) != 0) ? L"SATA" : L"SAS",
                        ((Info & (0x1 << 5)) != 0) ? L"External" : L"Internal",
                        ((Info & (0x1 << 6)) != 0) ? L"Expanded" : L"Direct"
                    );

                    if ((Info & 0x0f) == 1) {
                        MyCatPrint (Str, L"0,");
                    }
                    else {
                        MyCatPrint (Str, L"%x,", (UINTN) ((Info >> 8) & 0xff));
                    }
                }
                else {
                    MyCatPrint (Str, L"0,0,0,0,");
                }

                MyCatPrint (Str, L"%x)", (UINTN) ((SAS_DEVICE_PATH *) Vendor)->Reserved);
                return;
            }
            else if (CompareGuid (&Vendor->Guid, &gEfiDebugPortProtocolGuid)) {
                MyCatPrint (Str, L"DebugPort()");
                return;
            }

            break;

        case MEDIA_DEVICE_PATH: Type = L"Media"; break;
        default:                Type = L"?";     break;
    } // switch

    MyCatPrint (Str, L"Ven%s(%g", Type, &Vendor->Guid);

    DataLength = DevicePathNodeLength (&Vendor->Header) - sizeof (VENDOR_DEVICE_PATH);

    if (DataLength > 0) {
        MyCatPrint (Str, L",");
        for (Index = 0; Index < DataLength; Index++) {
            MyCatPrint (
                Str,
                L"%02x",
                (UINTN) ((VENDOR_DEVICE_PATH_WITH_DATA *) Vendor)->VendorDefinedData[Index]
            );
        }
    }

    MyCatPrint (Str, L")");
} // VOID DevPathVendor()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathAcpi (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    ACPI_HID_DEVICE_PATH  *Acpi;

    Acpi = DevPath;
    if ((Acpi->HID & PNP_EISA_ID_MASK) == PNP_EISA_ID_CONST) {
        MyCatPrint (
            Str,
            L"Acpi(PNP%04x,%x)",
            (UINTN) EISA_ID_TO_NUM (Acpi->HID),
            (UINTN) Acpi->UID
        );
    }
    else {
        MyCatPrint (Str, L"Acpi(%08x,%x)", (UINTN) Acpi->HID, (UINTN) Acpi->UID);
    }
}

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathExtendedAcpi (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    ACPI_EXTENDED_HID_DEVICE_PATH  *ExtendedAcpi;

    // Index for HID, UID and CID strings, 0 for non-exist
    UINT16                          HIDSTRIdx;
    UINT16                          UIDSTRIdx;
    UINT16                          CIDSTRIdx;
    UINT16                          Index;
    UINT16                          Length;
    UINT16                          Anchor;
    CHAR8                          *AsChar8Array;

    HIDSTRIdx    = 0;
    UIDSTRIdx    = 0;
    CIDSTRIdx    = 0;
    ExtendedAcpi = DevPath;
    Length       = (UINT16) DevicePathNodeLength ((EFI_DEVICE_PATH_PROTOCOL *) ExtendedAcpi);

    AsChar8Array = (CHAR8 *) ExtendedAcpi;

    // find HIDSTR
    Anchor = 16;
    for (Index = Anchor; Index < Length && AsChar8Array[Index] != '\0'; Index++) {
        ;
    }

    if (Index > Anchor) {
        HIDSTRIdx = Anchor;
    }

    // find UIDSTR
    Anchor = (UINT16) (Index + 1);
    for (Index = Anchor; Index < Length && AsChar8Array[Index] != '\0'; Index++) {
        ;
    }

    if (Index > Anchor) {
        UIDSTRIdx = Anchor;
    }

    // find CIDSTR
    Anchor = (UINT16) (Index + 1);
    for (Index = Anchor; Index < Length && AsChar8Array[Index] != '\0'; Index++) {
        ;
    }

    if (Index > Anchor) {
        CIDSTRIdx = Anchor;
    }

    if (HIDSTRIdx == 0 && CIDSTRIdx == 0 && ExtendedAcpi->UID == 0) {
        MyCatPrint (Str, L"AcpiExp(");
        if ((ExtendedAcpi->HID & PNP_EISA_ID_MASK) == PNP_EISA_ID_CONST) {
            MyCatPrint (Str, L"PNP%04x,", (UINTN) EISA_ID_TO_NUM (ExtendedAcpi->HID));
        }
        else {
            MyCatPrint (Str, L"%08x,", (UINTN) ExtendedAcpi->HID);
        }

        if ((ExtendedAcpi->CID & PNP_EISA_ID_MASK) == PNP_EISA_ID_CONST) {
            MyCatPrint (Str, L"PNP%04x,", (UINTN)  EISA_ID_TO_NUM (ExtendedAcpi->CID));
        }
        else {
            MyCatPrint (Str, L"%08x,", (UINTN)  ExtendedAcpi->CID);
        }

        if (UIDSTRIdx != 0) {
            MyCatPrint (Str, L"%a)", AsChar8Array + UIDSTRIdx);
        }
        else {
            MyCatPrint (Str, L"\"\")");
        }
    }
    else {
        MyCatPrint (Str, L"AcpiEx(");
        if ((ExtendedAcpi->HID & PNP_EISA_ID_MASK) == PNP_EISA_ID_CONST) {
            MyCatPrint (Str, L"PNP%04x,", (UINTN)  EISA_ID_TO_NUM (ExtendedAcpi->HID));
        }
        else {
            MyCatPrint (Str, L"%08x,", (UINTN) ExtendedAcpi->HID);
        }

        if ((ExtendedAcpi->CID & PNP_EISA_ID_MASK) == PNP_EISA_ID_CONST) {
            MyCatPrint (Str, L"PNP%04x,", (UINTN) EISA_ID_TO_NUM (ExtendedAcpi->CID));
        }
        else {
            MyCatPrint (Str, L"%08x,", (UINTN) ExtendedAcpi->CID);
        }

        MyCatPrint (Str, L"%x,", (UINTN) ExtendedAcpi->UID);

        if (HIDSTRIdx != 0) {
            MyCatPrint (Str, L"%a,", AsChar8Array + HIDSTRIdx);
        }
        else {
            MyCatPrint (Str, L"\"\",");
        }

        if (CIDSTRIdx != 0) {
            MyCatPrint (Str, L"%a,", AsChar8Array + CIDSTRIdx);
        }
        else {
            MyCatPrint (Str, L"\"\",");
        }

        if (UIDSTRIdx != 0) {
            MyCatPrint (Str, L"%a)", AsChar8Array + UIDSTRIdx);
        }
        else {
            MyCatPrint (Str, L"\"\")");
        }
    } // if/else HIDSTRIdx
} // VOID DevPathExtendedAcpi()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathAdrAcpi (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    ACPI_ADR_DEVICE_PATH *AcpiAdr;
    UINT16                Index;
    UINT16                Length;
    UINT16                AdditionalAdrCount;

    AcpiAdr            = DevPath;
    Length             = (UINT16) DevicePathNodeLength ((EFI_DEVICE_PATH_PROTOCOL *) AcpiAdr);
    AdditionalAdrCount = (UINT16) ((Length - 8) / 4);

    MyCatPrint (Str, L"AcpiAdr(%x", (UINTN) AcpiAdr->ADR);

    for (Index = 0; Index < AdditionalAdrCount; Index++) {
        MyCatPrint (Str, L",%x", (UINTN) *(UINT32 *) ((UINT8 *) AcpiAdr + 8 + Index * 4));
    } // for

    MyCatPrint (Str, L")");
} // VOID DevPathAdrAcpi()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathAtapi (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    ATAPI_DEVICE_PATH *Atapi;

    Atapi = DevPath;
    MyCatPrint (
        Str,
        L"Ata(%s,%s)",
        (Atapi->PrimarySecondary != 0) ? L"Secondary" : L"Primary",
        (Atapi->SlaveMaster != 0)      ? L"Slave"     : L"Master"
    );
} // VOID DevPathAtapi()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathScsi (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    SCSI_DEVICE_PATH  *Scsi;

    Scsi = DevPath;
    MyCatPrint (Str, L"Scsi(Pun%x,Lun%x)", (UINTN) Scsi->Pun, (UINTN) Scsi->Lun);
} // VOID DevPathScsi()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathFibre (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    FIBRECHANNEL_DEVICE_PATH  *Fibre;

    Fibre = DevPath;
    MyCatPrint (Str, L"Fibre(Wwn%lx,Lun%x)", Fibre->WWN, Fibre->Lun);
} // VOID DevPathFibre()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPath1394 (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    F1394_DEVICE_PATH *F1394Path;

    F1394Path = DevPath;
    MyCatPrint (Str, L"1394(%lx)", &F1394Path->Guid);
} // VOID DevPath1394()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathUsb (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    USB_DEVICE_PATH *Usb;

    Usb = DevPath;
    MyCatPrint (Str, L"Usb(%x,%x)", (UINTN) Usb->ParentPortNumber, (UINTN) Usb->InterfaceNumber);
} // VOID DevPathUsb()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathUsbWWID (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    USB_WWID_DEVICE_PATH  *UsbWWId;

    UsbWWId = DevPath;
    MyCatPrint (
        Str,
        L"UsbWwid(%x,%x,%x,\"WWID\")",
        (UINTN) UsbWWId->VendorId,
        (UINTN) UsbWWId->ProductId,
        (UINTN) UsbWWId->InterfaceNumber
    );
} // VOID DevPathUsbWWID()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathLogicalUnit (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    DEVICE_LOGICAL_UNIT_DEVICE_PATH *LogicalUnit;

    LogicalUnit = DevPath;
    MyCatPrint (Str, L"Unit(%x)", (UINTN) LogicalUnit->Lun);
} // VOID DevPathLogicalUnit()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathUsbClass (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    USB_CLASS_DEVICE_PATH *UsbClass;

    UsbClass = DevPath;
    MyCatPrint (
        Str,
        L"Usb Class(%x,%x,%x,%x,%x)",
        (UINTN) UsbClass->VendorId,
        (UINTN) UsbClass->ProductId,
        (UINTN) UsbClass->DeviceClass,
        (UINTN) UsbClass->DeviceSubClass,
        (UINTN) UsbClass->DeviceProtocol
    );
} // VOID DevPathUsbClass()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathSata (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    SATA_DEVICE_PATH *Sata;

    Sata = DevPath;
    if ((Sata->PortMultiplierPortNumber & SATA_HBA_DIRECT_CONNECT_FLAG) != 0) {
        MyCatPrint (
            Str,
            L"Sata(%x,%x)",
            (UINTN) Sata->HBAPortNumber,
            (UINTN) Sata->Lun
        );
    }
    else {
        MyCatPrint (
            Str,
            L"Sata(%x,%x,%x)",
            (UINTN) Sata->HBAPortNumber,
            (UINTN) Sata->PortMultiplierPortNumber,
            (UINTN) Sata->Lun
        );
    }
} // VOID DevPathSata()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathI2O (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    I2O_DEVICE_PATH *I2OPath;

    I2OPath = DevPath;
    MyCatPrint (Str, L"I2O(%x)", (UINTN) I2OPath->Tid);
} // VOID DevPathI2O()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathMacAddr (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    MAC_ADDR_DEVICE_PATH *MACDevPath;
    UINTN                 HwAddressSize;
    UINTN                 Index;

    MACDevPath    = DevPath;
    HwAddressSize = sizeof (EFI_MAC_ADDRESS);
    if (MACDevPath->IfType == 0x01 || MACDevPath->IfType == 0x00) {
        HwAddressSize = 6;
    }

    MyCatPrint (Str, L"Mac(");

    for (Index = 0; Index < HwAddressSize; Index++) {
        MyCatPrint (Str, L"%02x", (UINTN) MACDevPath->MacAddress.Addr[Index]);
    }

    MyCatPrint (Str, L")");
} // VOID DevPathMacAddr()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathIPv4 (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    IPv4_DEVICE_PATH  *IPDevPath;

    IPDevPath = DevPath;
    MyCatPrint (
        Str,
        L"IPv4(%d.%d.%d.%d:%d)",
        (UINTN) IPDevPath->RemoteIpAddress.Addr[0],
        (UINTN) IPDevPath->RemoteIpAddress.Addr[1],
        (UINTN) IPDevPath->RemoteIpAddress.Addr[2],
        (UINTN) IPDevPath->RemoteIpAddress.Addr[3],
        (UINTN) IPDevPath->RemotePort
    );
} // VOID DevPathIPv4()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathIPv6 (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    IPv6_DEVICE_PATH  *IPv6DevPath;

    IPv6DevPath = DevPath;
    MyCatPrint (
        Str,
        L"IPv6(%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x)",
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[0],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[1],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[2],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[3],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[4],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[5],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[6],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[7],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[8],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[9],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[10],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[11],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[12],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[13],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[14],
        (UINTN) IPv6DevPath->RemoteIpAddress.Addr[15]
    );
} // VOID DevPathIPv6()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathInfiniBand (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    INFINIBAND_DEVICE_PATH  *InfiniBand;

    InfiniBand = DevPath;
    MyCatPrint (
        Str,
        L"Infiniband(%x,%g,%lx,%lx,%lx)",
        (UINTN) InfiniBand->ResourceFlags,
        InfiniBand->PortGid,
        InfiniBand->ServiceId,
        InfiniBand->TargetPortId,
        InfiniBand->DeviceId
    );
} // VOID DevPathInfiniBand()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathUart (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    UART_DEVICE_PATH  *Uart;
    CHAR8              Parity;

    Uart = DevPath;
    switch (Uart->Parity) {
        case 0:  Parity = 'D';  break;
        case 1:  Parity = 'N';  break;
        case 2:  Parity = 'E';  break;
        case 3:  Parity = 'O';  break;
        case 4:  Parity = 'M';  break;
        case 5:  Parity = 'S';  break;
        default: Parity = 'x';  break;
    } // switch

    if (Uart->BaudRate == 0) {
        MyCatPrint (Str, L"Uart(DEFAULT,%c,", Parity);
    }
    else {
        MyCatPrint (Str, L"Uart(%ld,%c,", Uart->BaudRate, Parity);
    }

    if (Uart->DataBits == 0) {
        MyCatPrint (Str, L"D,");
    }
    else {
        MyCatPrint (Str, L"%d,", (UINTN) Uart->DataBits);
    }

    switch (Uart->StopBits) {
        case 0:  MyCatPrint (Str, L"D)");   break;
        case 1:  MyCatPrint (Str, L"1)");   break;
        case 2:  MyCatPrint (Str, L"1.5)"); break;
        case 3:  MyCatPrint (Str, L"2)");   break;
        default: MyCatPrint (Str, L"x)");   break;
    } // switch
} // VOID DevPathUart()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathiSCSI (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    ISCSI_DEVICE_PATH_WITH_NAME *IScsi;
    UINT16                       Options;

    IScsi = DevPath;

    MyCatPrint (
        Str,
        L"iSCSI(%a,%x,%lx,",
        IScsi->TargetName,
        (UINTN) IScsi->TargetPortalGroupTag,
        IScsi->Lun
    );

    Options = IScsi->LoginOption;

    MyCatPrint (Str, L"%s,", (((Options >> 1) & 0x0001) != 0) ? L"CRC32C" : L"None");
    MyCatPrint (Str, L"%s,", (((Options >> 3) & 0x0001) != 0) ? L"CRC32C" : L"None");

    if (((Options >> 11) & 0x0001) != 0) {
        MyCatPrint (Str, L"%s,", L"None");
    }
    else if (((Options >> 12) & 0x0001) != 0) {
        MyCatPrint (Str, L"%s,", L"CHAP_UNI");
    }
    else {
        MyCatPrint (Str, L"%s,", L"CHAP_BI");
    }

    MyCatPrint (Str, L"%s)", (IScsi->NetworkProtocol == 0) ? L"TCP" : L"reserved");
} // VOID DevPathiSCSI()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathVlan (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    VLAN_DEVICE_PATH  *Vlan;

    Vlan = DevPath;
    MyCatPrint (Str, L"Vlan(%d)", (UINTN) Vlan->VlanId);
} // VOID DevPathVlan()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathHardDrive (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    HARDDRIVE_DEVICE_PATH *Hd;

    Hd = DevPath;
    switch (Hd->SignatureType) {
        case SIGNATURE_TYPE_MBR:
            MyCatPrint (
                Str,
                L"HD(Part%d,Sig%08x)",
                (UINTN) Hd->PartitionNumber,
                (UINTN) *((UINT32 *) (&(Hd->Signature[0])))
            );

            break;

        case SIGNATURE_TYPE_GUID:
            MyCatPrint (
                Str,
                L"HD(Part%d,Sig%g)",
                (UINTN) Hd->PartitionNumber,
                (EFI_GUID *) &(Hd->Signature[0])
            );

            break;

        default:
            MyCatPrint (
                Str,
                L"HD(Part%d,MBRType=%02x,SigType=%02x)",
                (UINTN) Hd->PartitionNumber,
                (UINTN) Hd->MBRType,
                (UINTN) Hd->SignatureType
            );
            break;
    } // switch
} // VOID DevPathHardDrive()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathCDROM (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    CDROM_DEVICE_PATH *Cd;

    Cd = DevPath;
    MyCatPrint (Str, L"CDROM(Entry%x)", (UINTN) Cd->BootEntry);
} // VOID DevPathCDROM()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathFilePath (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
) {
    FILEPATH_DEVICE_PATH  *Fp;

    Fp = DevPath;
    MyCatPrint (Str, L"%s", Fp->PathName);
} // VOID DevPathFilePath()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathMediaProtocol (
    IN OUT POOL_PRINT       *Str,
    IN VOID                 *DevPath
) {
    MEDIA_PROTOCOL_DEVICE_PATH  *MediaProt;

    MediaProt = DevPath;
    MyCatPrint (Str, L"Media(%g)", &MediaProt->Protocol);
} // VOID DevPathMediaProtocol()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathFvFilePath (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *FvFilePath;

    FvFilePath = DevPath;
    MyCatPrint (Str, L"%g", &FvFilePath->FvFileName);
} // VOID DevPathFvFilePath()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID MyDevPathRelativeOffsetRange (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    MEDIA_RELATIVE_OFFSET_RANGE_DEVICE_PATH *Offset;

    Offset = DevPath;
    MyCatPrint (
        Str,
        L"Offset(%lx,%lx)",
        Offset->StartingOffset,
        Offset->EndingOffset
    );
} // VOID MyDevPathRelativeOffsetRange()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathBssBss (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    BBS_BBS_DEVICE_PATH *Bbs;
    CHAR16              *Type;

    Bbs = DevPath;
    switch (Bbs->DeviceType) {
        case BBS_TYPE_FLOPPY:           Type = L"Floppy";     break;
        case BBS_TYPE_HARDDRIVE:        Type = L"Harddrive";  break;
        case BBS_TYPE_CDROM:            Type = L"CDROM";      break;
        case BBS_TYPE_PCMCIA:           Type = L"PCMCIA";     break;
        case BBS_TYPE_USB:              Type = L"Usb";        break;
        case BBS_TYPE_EMBEDDED_NETWORK: Type = L"Net";        break;
        case BBS_TYPE_BEV:              Type = L"BEV";        break;
        default:                        Type = L"?";          break;
    } // switch

    MyCatPrint (Str, L"Legacy-%s", Type);
} // VOID DevPathBssBss()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathEndInstance (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    MyCatPrint (Str, L",");
} // VOID DevPathEndInstance()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maixmum length reserved
for the string buffer.
@param DevPath         The device path.

**/
VOID DevPathNodeUnknown (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    MyCatPrint (Str, L"?");
} // VOID DevPathNodeUnknown()

/**
Convert Device Path to a Unicode string for printing.

@param Str             The buffer holding the output string.
This buffer contains the length of the
string and the maximum length reserved
for the string buffer.
@param DevPath         The device path.
**/
VOID DevPathFvPath (
    IN OUT POOL_PRINT *Str,
    IN     VOID       *DevPath
) {
    MEDIA_FW_VOL_DEVICE_PATH *FvPath;

    FvPath = DevPath;
    MyCatPrint (Str, L"Fv(%g)", &FvPath->FvName);
} // VOID DevPathFvPath()

DEVICE_PATH_STRING_TABLE  DevPathTable[] = {
    { HARDWARE_DEVICE_PATH,      HW_PCI_DP,                          DevPathPci                   },
    { HARDWARE_DEVICE_PATH,      HW_PCCARD_DP,                       DevPathPccard                },
    { HARDWARE_DEVICE_PATH,      HW_MEMMAP_DP,                       DevPathMemMap                },
    { HARDWARE_DEVICE_PATH,      HW_VENDOR_DP,                       DevPathVendor                },
    { HARDWARE_DEVICE_PATH,      HW_CONTROLLER_DP,                   DevPathController            },
    { ACPI_DEVICE_PATH,          ACPI_DP,                            DevPathAcpi                  },
    { ACPI_DEVICE_PATH,          ACPI_EXTENDED_DP,                   DevPathExtendedAcpi          },
    { ACPI_DEVICE_PATH,          ACPI_ADR_DP,                        DevPathAdrAcpi               },
    { MESSAGING_DEVICE_PATH,     MSG_ATAPI_DP,                       DevPathAtapi                 },
    { MESSAGING_DEVICE_PATH,     MSG_SCSI_DP,                        DevPathScsi                  },
    { MESSAGING_DEVICE_PATH,     MSG_FIBRECHANNEL_DP,                DevPathFibre                 },
    { MESSAGING_DEVICE_PATH,     MSG_1394_DP,                        DevPath1394                  },
    { MESSAGING_DEVICE_PATH,     MSG_USB_DP,                         DevPathUsb                   },
    { MESSAGING_DEVICE_PATH,     MSG_USB_WWID_DP,                    DevPathUsbWWID               },
    { MESSAGING_DEVICE_PATH,     MSG_DEVICE_LOGICAL_UNIT_DP,         DevPathLogicalUnit           },
    { MESSAGING_DEVICE_PATH,     MSG_USB_CLASS_DP,                   DevPathUsbClass              },
    { MESSAGING_DEVICE_PATH,     MSG_SATA_DP,                        DevPathSata                  },
    { MESSAGING_DEVICE_PATH,     MSG_I2O_DP,                         DevPathI2O                   },
    { MESSAGING_DEVICE_PATH,     MSG_MAC_ADDR_DP,                    DevPathMacAddr               },
    { MESSAGING_DEVICE_PATH,     MSG_IPv4_DP,                        DevPathIPv4                  },
    { MESSAGING_DEVICE_PATH,     MSG_IPv6_DP,                        DevPathIPv6                  },
    { MESSAGING_DEVICE_PATH,     MSG_INFINIBAND_DP,                  DevPathInfiniBand            },
    { MESSAGING_DEVICE_PATH,     MSG_UART_DP,                        DevPathUart                  },
    { MESSAGING_DEVICE_PATH,     MSG_VENDOR_DP,                      DevPathVendor                },
    { MESSAGING_DEVICE_PATH,     MSG_ISCSI_DP,                       DevPathiSCSI                 },
    { MESSAGING_DEVICE_PATH,     MSG_VLAN_DP,                        DevPathVlan                  },
    { MEDIA_DEVICE_PATH,         MEDIA_HARDDRIVE_DP,                 DevPathHardDrive             },
    { MEDIA_DEVICE_PATH,         MEDIA_CDROM_DP,                     DevPathCDROM                 },
    { MEDIA_DEVICE_PATH,         MEDIA_VENDOR_DP,                    DevPathVendor                },
    { MEDIA_DEVICE_PATH,         MEDIA_FILEPATH_DP,                  DevPathFilePath              },
    { MEDIA_DEVICE_PATH,         MEDIA_PROTOCOL_DP,                  DevPathMediaProtocol         },
    { MEDIA_DEVICE_PATH,         MEDIA_PIWG_FW_VOL_DP,               DevPathFvPath                },
    { MEDIA_DEVICE_PATH,         MEDIA_PIWG_FW_FILE_DP,              DevPathFvFilePath            },
    { MEDIA_DEVICE_PATH,         MEDIA_RELATIVE_OFFSET_RANGE_DP,     MyDevPathRelativeOffsetRange },
    { BBS_DEVICE_PATH,           BBS_BBS_DP,                         DevPathBssBss                },
    { END_DEVICE_PATH_TYPE,      END_INSTANCE_DEVICE_PATH_SUBTYPE,   DevPathEndInstance           },
    { 0,                         0,                                  NULL                         }
};


/**
This function converts an input device structure to a Unicode string.

@param DevPath                  A pointer to the device path structure.

@return A new allocated Unicode string that represents the device path.

**/
CHAR16 * EFIAPI DevicePathToStr (
    IN EFI_DEVICE_PATH_PROTOCOL *DevPath
) {
    POOL_PRINT                Str;
    EFI_DEVICE_PATH_PROTOCOL *DevPathNode;

    VOID (*DumpNode) (POOL_PRINT *, VOID *);

    UINTN Index;
    UINTN NewSize;

    EFI_STATUS                        Status;
    CHAR16                           *ToText;
    EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *DevPathToText;

    ZeroMem (&Str, sizeof (Str));

    if (DevPath == NULL) {
        goto Done;
    }

    Status = REFIT_CALL_3_WRAPPER(
        gBS->LocateProtocol, &gEfiDevicePathToTextProtocolGuid,
        NULL, (VOID **) &DevPathToText
    );
    if (!EFI_ERROR(Status)) {
        ToText = DevPathToText->ConvertDevicePathToText (
            DevPath, FALSE, TRUE
        );

        ASSERT (ToText != NULL);

        return ToText;
    }

    // Process each device path node
    DevPathNode = DevPath;

    while (!IsDevicePathEnd (DevPathNode)) {
        // Find the handler to dump this device path node
        DumpNode = NULL;
        for (Index = 0; DevPathTable[Index].Function != NULL; Index += 1) {

            if (DevicePathType (DevPathNode) == DevPathTable[Index].Type &&
                DevicePathSubType (DevPathNode) == DevPathTable[Index].SubType
            ) {
                DumpNode = DevPathTable[Index].Function;
                break;
            }
        } // for

        // If not found, use a generic function
        if (DumpNode == NULL) {
            DumpNode = DevPathNodeUnknown;
        }

        //  Put a path seperator in if needed
        if ((Str.Len != 0) && (DumpNode != DevPathEndInstance)) {
            MyCatPrint (&Str, L"/");
        }

        // Print this node of the device path
        DumpNode (&Str, DevPathNode);

        // Next device path node
        DevPathNode = NextDevicePathNode (DevPathNode);
    }

Done:
    NewSize = (Str.Len + 1) * sizeof (CHAR16);
    Str.Str = EfiReallocatePool (Str.Str, NewSize, NewSize);

    ASSERT (Str.Str != NULL);

    Str.Str[Str.Len] = 0;

    return Str.Str;
} // CHAR16 * EFIAPI DevicePathToStr()
