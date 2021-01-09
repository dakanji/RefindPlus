/*
 * MainRP/apple.c
 * Functions specific to Apple computers
 *
 * Copyright (c) 2015 Roderick W. Smith
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
 *
 */

#include "global.h"
#include "config.h"
#include "lib.h"
#include "screen.h"
#include "apple.h"
#include "mystrings.h"
#include "../include/refit_call_wrapper.h"

CHAR16 *gCsrStatus = NULL;

// Get CSR (Apple's System Integrity Protection [SIP], or "rootless") status
// information. If the variable is not present and the firmware is Apple, fake
// it and claim it is enabled. This is consistent with how Mac OS El Capitan
// treats systems with the variable absent.
EFI_STATUS
GetCsrStatus (
    UINT32 *CsrStatus
) {
    UINTN      CsrLength;
    UINT32     *ReturnValue = NULL;
    EFI_GUID   CsrGuid      = APPLE_GUID;
    EFI_STATUS Status       = EFI_INVALID_PARAMETER;

    if (CsrStatus) {
        Status = EfivarGetRaw (
            &CsrGuid,
            L"csr-active-config",
            (CHAR8**) &ReturnValue,
            &CsrLength
        );

        if (Status == EFI_SUCCESS) {
            if (CsrLength == 4) {
                *CsrStatus = *ReturnValue;
            } else {
                Status = EFI_BAD_BUFFER_SIZE;
                gCsrStatus = L" Unknown System Integrity Protection version";
            }

            MyFreePool (ReturnValue);
        } else if (Status == EFI_NOT_FOUND &&
            StriSubCmp (L"Apple", gST->FirmwareVendor)
        ) {
            *CsrStatus = SIP_ENABLED;
            Status = EFI_SUCCESS;
        } // if (Status == EFI_SUCCESS)
    } // if (CsrStatus)

    return Status;
} // INTN GetCsrStatus()

// Store string describing CSR status value in gCsrStatus variable, which appears
// on the Info page. If DisplayMessage is TRUE, displays the new value of
// gCsrStatus on the screen for four seconds.
VOID RecordgCsrStatus (
    UINT32 CsrStatus,
    BOOLEAN DisplayMessage
) {
    EG_PIXEL BGColor = COLOR_LIGHTBLUE;

    switch (CsrStatus) {
        case SIP_ENABLED:
            gCsrStatus = PoolPrint (
                L" System Integrity Protection Enabled (0x%03x)",
                CsrStatus
            );
            break;

        case SIP_DISABLED:
        case SIP_DISABLED_EX:
        case SIP_DISABLED_ANY:
        case SIP_DISABLED_KEXT:
        case SIP_DISABLED_DEBUG:
        case SIP_DISABLED_DEVICE:
        case CSR_MAX_LEGAL_VALUE:
            gCsrStatus = PoolPrint (
                L" System Integrity Protection Disabled (0x%03x)",
                CsrStatus
            );
            break;

        default:
            gCsrStatus = PoolPrint (
                L" System Integrity Protection Status: 0x%03x",
                CsrStatus
            );
    } // switch
    if (DisplayMessage) {
        egDisplayMessage (gCsrStatus, &BGColor, CENTER);
        PauseSeconds (4);
    } // if

    MyFreePool (gCsrStatus);
} // VOID RecordgCsrStatus()

// Find the current CSR status and reset it to the next one in the
// GlobalConfig.CsrValues list, or to the first value if the current
// value is not on the list.
VOID RotateCsrValue (VOID) {
    UINT32       CurrentValue, TargetCsr;
    UINT32_LIST  *ListItem;
    EFI_GUID     CsrGuid = APPLE_GUID;
    EFI_STATUS   Status;

    Status = GetCsrStatus (&CurrentValue);
    if ((Status == EFI_SUCCESS) && GlobalConfig.CsrValues) {
        ListItem = GlobalConfig.CsrValues;

        while ((ListItem != NULL) && (ListItem->Value != CurrentValue)) {
            ListItem = ListItem->Next;
        }

        if (ListItem == NULL || ListItem->Next == NULL) {
            TargetCsr = GlobalConfig.CsrValues->Value;
        } else {
            TargetCsr = ListItem->Next->Value;
        }

        Status = EfivarSetRaw (
            &CsrGuid,
            L"csr-active-config",
            (CHAR8 *) &TargetCsr,
            4,
            TRUE
        );

        if (Status == EFI_SUCCESS) {
            RecordgCsrStatus (TargetCsr, TRUE);
        } else {
            EG_PIXEL BGColor = COLOR_LIGHTBLUE;
            egDisplayMessage (
                L" Error setting System Integrity Protection status",
                &BGColor,
                CENTER
            );
            PauseSeconds (4);
        }
    } // if
} // VOID RotateCsrValue()


/*
 * The definitions below and the SetAppleOSInfo() function are based on a GRUB patch by Andreas Heider:
 * https://lists.gnu.org/archive/html/grub-devel/2013-12/msg00442.html
 */

#define EFI_APPLE_SET_OS_PROTOCOL_GUID  \
{ 0xc5c5da95, 0x7d5c, 0x45e6, { 0xb2, 0xf1, 0x3f, 0xd5, 0x2b, 0xb1, 0x00, 0x77 } }

typedef struct EfiAppleSetOsInterface {
    UINT64 Version;
    EFI_STATUS EFIAPI (*SetOsVersion) (IN CHAR8 *Version);
    EFI_STATUS EFIAPI (*SetOsVendor) (IN CHAR8 *Vendor);
} EfiAppleSetOsInterface;

// Function to tell the firmware that Mac OS X is being launched. This is
// required to work around problems on some Macs that don't fully
// initialize some hardware (especially video displays) when third-party
// OSes are launched in EFI mode.
EFI_STATUS
SetAppleOSInfo (
    VOID
) {
    EFI_STATUS              Status;
    EFI_GUID                apple_set_os_guid  = EFI_APPLE_SET_OS_PROTOCOL_GUID;
    CHAR16                  *AppleOSVersion    = NULL;
    CHAR8                   *AppleOSVersion8   = NULL;
    EfiAppleSetOsInterface  *SetOs             = NULL;

    Status = refit_call3_wrapper(
        gBS->LocateProtocol,
        &apple_set_os_guid,
        NULL,
        (VOID**) &SetOs
    );

    // If not a Mac, ignore the call....
    if ((Status != EFI_SUCCESS) || (!SetOs)) {
        Status = EFI_INVALID_PARAMETER;
    }
    else {
        if (SetOs->Version != 0 && GlobalConfig.SpoofOSXVersion) {
            AppleOSVersion = StrDuplicate (L"Mac OS");
            MergeStrings (&AppleOSVersion, GlobalConfig.SpoofOSXVersion, ' ');

            if (AppleOSVersion) {
                AppleOSVersion8 = AllocateZeroPool ((StrLen (AppleOSVersion) + 1) * sizeof (CHAR8));
                UnicodeStrToAsciiStr (AppleOSVersion, AppleOSVersion8);

                if (AppleOSVersion8) {
                    Status = refit_call1_wrapper(SetOs->SetOsVersion, AppleOSVersion8);
                    if (!EFI_ERROR (Status)) {
                        Status = EFI_SUCCESS;
                    }
                    MyFreePool (AppleOSVersion8);
                }
                else {
                    Status = EFI_OUT_OF_RESOURCES;
                }

                if (Status == EFI_SUCCESS && SetOs->Version >= 2) {
                    Status = refit_call1_wrapper(SetOs->SetOsVendor, (CHAR8 *) "Apple Inc.");
                }
                MyFreePool (AppleOSVersion);
            } // if (AppleOSVersion)
        } // if
    }

    return Status;
} // EFI_STATUS SetAppleOSInfo()

// Reads and returns values of Apple NVRAM entries.
VOID *GetAppleNvramEntry (
    IN      CHAR16        *VariableName,
    OUT     UINT32        *Attributes    OPTIONAL,
    OUT     UINTN         *DataSize      OPTIONAL
) {
    EFI_STATUS  Status;
    VOID        *Data        = NULL;
    UINTN       IntDataSize  = 0;
    EFI_GUID    AppleGUID    = APPLE_GUID;

    do {
        Status = gRT->GetVariable (
            VariableName,
            &AppleGUID,
            Attributes,
            &IntDataSize,
            Data
        );

        if (Status == EFI_BUFFER_TOO_SMALL) {
            IntDataSize = IntDataSize + 1;
            Data = (VOID *) AllocateZeroPool (IntDataSize);
            if (Data == NULL) {
                break;
            }
        }
    } while (Status == EFI_BUFFER_TOO_SMALL);

    if (DataSize != NULL) {
        *DataSize = IntDataSize;
    }

    return Data;
}

// Checks NVRAM entries.
EFI_STATUS CheckAppleNvramEntry (
    IN  CHAR16      *NameNVRAM,
    IN  CONST VOID  *DataNVRAM
) {
    VOID        *OldData;
    UINTN       NewDataSize    = sizeof (DataNVRAM);
    UINTN       OldDataSize    = 0;
    UINT32      OldAttributes  = 0;
    EFI_STATUS  Status         = EFI_SUCCESS;

    OldData = (VOID *) GetAppleNvramEntry (
        NameNVRAM,
        &OldAttributes,
        &OldDataSize
    );

    if (OldData != NULL) {
        if ((OldDataSize == NewDataSize) &&
            (CompareMem (DataNVRAM, OldData, OldDataSize) == 0)
        ) {
            Status = EFI_ALREADY_STARTED;
        }
    }
    MyFreePool (OldData);

    return Status;
}
