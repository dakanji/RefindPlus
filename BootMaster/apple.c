/*
 * BootMaster/apple.c
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
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "config.h"
#include "lib.h"
#include "screenmgt.h"
#include "apple.h"
#include "mystrings.h"
#include "../include/refit_call_wrapper.h"

CHAR16    *gCsrStatus     = NULL;
BOOLEAN    MuteLogger     = FALSE;
BOOLEAN    MsgNormalised  = FALSE;

// Get CSR (Apple's Configurable Security Restrictions; aka System Integrity
// Protection [SIP], or "rootless") status information. If the variable is not
// present and the firmware is Apple, fake it and claim it is enabled, since
// that's how OS X 10.11 treats a system with the variable absent.
EFI_STATUS GetCsrStatus (
    IN OUT UINT32 *CsrStatus
) {
    EFI_STATUS  Status;
    UINTN       CsrLength;
    UINT32     *ReturnValue  = NULL;
    EFI_GUID    CsrGuid      = APPLE_GUID;

    Status = EfivarGetRaw (
        &CsrGuid,
        L"csr-active-config",
        (CHAR8 **) &ReturnValue,
        &CsrLength
    );

    if (Status == EFI_SUCCESS) {
        if (CsrLength == 4) {
            *CsrStatus = *ReturnValue;
        }
        else {
            Status     = EFI_BAD_BUFFER_SIZE;
            gCsrStatus = StrDuplicate (L"Unknown SIP/SSV Status");
        }

        MyFreePool (&ReturnValue);
    }
    else if (Status == EFI_NOT_FOUND) {
        *CsrStatus = SIP_ENABLED_EX;
        gCsrStatus = StrDuplicate (L"SIP/SSV Enabled (Cleared/Empty)");

        // Treat as Success
        Status = EFI_SUCCESS;
    }
    else {
        gCsrStatus = StrDuplicate (L"Error While Getting SIP/SSV Status");
    }

    return Status;
} // EFI_STATUS GetCsrStatus()

// Store string describing CSR status value in gCsrStatus variable, which appears
// on the Info page. If DisplayMessage is TRUE, displays the new value of
// gCsrStatus on the screen for four seconds.
VOID RecordgCsrStatus (
    UINT32  CsrStatus,
    BOOLEAN DisplayMessage
) {
    UINTN     WaitSeconds = 3;
    CHAR16   *MsgStr      = NULL;
    EG_PIXEL  BGColor     = COLOR_LIGHTBLUE;

    switch (CsrStatus) {
        // SIP "Cleared" Setting
        case SIP_ENABLED_EX:
            gCsrStatus = PoolPrint (L"SIP Enabled (Cleared/Empty)");
            break;

        // SIP "Enabled" Setting
        case SIP_ENABLED:
            gCsrStatus = PoolPrint (
                L"SIP Enabled (0x%04x)",
                CsrStatus
            );
            break;

        // SIP "Disabled" Settings
        case SIP_DISABLED:
        case SIP_DISABLED_B:
        case SIP_DISABLED_EX:
        case SIP_DISABLED_DBG:
        case SIP_DISABLED_KEXT:
        case SIP_DISABLED_EXTRA:
            gCsrStatus = PoolPrint (
                L"SIP Disabled (0x%04x)",
                CsrStatus
            );
            break;

        // SSV "Disabled" Settings
        case SSV_DISABLED:
        case SSV_DISABLED_B:
        case SSV_DISABLED_EX:
            gCsrStatus = PoolPrint (
                L"SIP/SSV Disabled (0x%04x)",
                CsrStatus
            );
            break;

        // Recognised Custom SIP "Disabled" Settings
        case SSV_DISABLED_ANY:
        case SSV_DISABLED_KEXT:
        case SSV_DISABLED_ANY_EX:
            gCsrStatus = PoolPrint (
                L"SIP/SSV Disabled (0x%04x - Custom Setting)",
                CsrStatus
            );
            break;

        // Wide Open and Max Legal CSR "Disabled" Settings
        case SSV_DISABLED_WIDE_OPEN:
        case CSR_MAX_LEGAL_VALUE:
            gCsrStatus = PoolPrint (
                L"SIP/SSV Removed (0x%04x - Caution!)",
                CsrStatus
            );
            break;

        // Unknown Custom Setting
        default:
            gCsrStatus = PoolPrint (
                L"SIP/SSV Disabled: 0x%04x - Caution: Unknown Custom Setting",
                CsrStatus
            );
    } // switch

    if (DisplayMessage) {
        if (MsgNormalised) {
            WaitSeconds = 4;
            MsgStr = PoolPrint (L"Normalised CSR:- '%s'", gCsrStatus);
        }
        else {
            MsgStr = PoolPrint (L"%s", gCsrStatus);
        }

        egDisplayMessage (MsgStr, &BGColor, CENTER);
        PauseSeconds (WaitSeconds);

        #if REFIT_DEBUG > 0
        MsgLog ("    * %s\n\n", MsgStr);
        #endif

        MyFreePool (&MsgStr);
    }
} // VOID RecordgCsrStatus()

// Find the current CSR status and reset it to the next one in the
// GlobalConfig.CsrValues list, or to the first value if the current
// value is not on the list.
VOID RotateCsrValue (VOID) {
    EFI_STATUS    Status;
    UINT32        CurrentValue, TargetCsr;
    UINT32        StorageFlags = APPLE_FLAGS;
    EFI_GUID      CsrGuid      = APPLE_GUID;
    UINT32_LIST  *ListItem;

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_SEPARATOR, L"Rotating CSR Value");
    #endif

    Status = GetCsrStatus (&CurrentValue);
    if ((Status == EFI_SUCCESS) && GlobalConfig.CsrValues) {
        ListItem = GlobalConfig.CsrValues;

        while ((ListItem != NULL) && (ListItem->Value != CurrentValue)) {
            ListItem = ListItem->Next;
        }

        if (ListItem == NULL || ListItem->Next == NULL) {
            TargetCsr = GlobalConfig.CsrValues->Value;
        }
        else {
            TargetCsr = ListItem->Next->Value;
        }

        #if REFIT_DEBUG > 0
        if (TargetCsr == 0) {
            // Set target CSR value to NULL
            LOG(1, LOG_LINE_NORMAL,
                L"Clearing CSR to 'NULL' from '0x%04x'",
                CurrentValue
            );
        }
        else if (CurrentValue == 0) {
            LOG(1, LOG_LINE_NORMAL,
                L"Setting CSR to '0x%04x' from 'NULL'",
                TargetCsr
            );
        }
        else {
            LOG(1, LOG_LINE_NORMAL,
                L"Setting CSR to '0x%04x' from '0x%04x'",
                CurrentValue, TargetCsr
            );
        }
        #endif

        if (TargetCsr != 0) {
            Status = EfivarSetRaw (
                &CsrGuid, L"csr-active-config",
                (CHAR8 *) &TargetCsr, 4, TRUE
            );
        }
        else {
            Status = REFIT_CALL_5_WRAPPER(
                gRT->SetVariable, L"csr-active-config",
                &CsrGuid, StorageFlags, 0, NULL
            );
        }

        if (Status == EFI_SUCCESS) {
            RecordgCsrStatus (TargetCsr, TRUE);

            #if REFIT_DEBUG > 0
            LOG(2, LOG_LINE_NORMAL,
                L"Successfully Set SIP/SSV:- '0x%04x'",
                TargetCsr
            );
            #endif
        }
        else {
            gCsrStatus = StrDuplicate (L"Error While Setting SIP/SSV");

            #if REFIT_DEBUG > 0
            LOG(1, LOG_LINE_NORMAL, gCsrStatus);
            #endif

            EG_PIXEL BGColor = COLOR_LIGHTBLUE;
            egDisplayMessage (
                gCsrStatus,
                &BGColor,
                CENTER
            );
            PauseSeconds (4);
        }
    }
    else {
        gCsrStatus = StrDuplicate (L"Could Not Retrieve SIP/SSV Status");

        #if REFIT_DEBUG > 0
        LOG(1, LOG_LINE_NORMAL, gCsrStatus);
        #endif

        EG_PIXEL BGColor = COLOR_LIGHTBLUE;
        egDisplayMessage (
            gCsrStatus,
            &BGColor,
            CENTER
        );
        PauseSeconds (4);
    } // if/else
} // VOID RotateCsrValue()


EFI_STATUS NormaliseCSR (VOID) {
    EFI_STATUS  Status;
    UINT32      OurCSR;

    // Mute logging if active
    MuteLogger = TRUE;

    // Get csr-active-config value
    Status = GetCsrStatus (&OurCSR);

    if (Status == EFI_NOT_FOUND) {
        // csr-active-config not found ... Proceed as 'OK'
        Status = EFI_ALREADY_STARTED;
    }
    else if (Status == EFI_SUCCESS) {
        if ((OurCSR & CSR_ALLOW_APPLE_INTERNAL) == 0) {
            // 'CSR_ALLOW_APPLE_INTERNAL' bit not present ... Proceed as 'OK'
            Status = EFI_ALREADY_STARTED;
        }
        else {
            // 'CSR_ALLOW_APPLE_INTERNAL' bit present ... Clear and Reset
            OurCSR &= ~CSR_ALLOW_APPLE_INTERNAL;

            MsgNormalised = TRUE;
            RecordgCsrStatus (OurCSR, TRUE);
            MsgNormalised = FALSE;
        }
    }

    // Restore logging if previously active
    MuteLogger = FALSE;

    return Status;
} // EFI_STATUS NormaliseCSR()


/*
 * The definitions below and the SetAppleOSInfo() function are based on a GRUB patch by Andreas Heider:
 * https://lists.gnu.org/archive/html/grub-devel/2013-12/msg00442.html
 */

#define EFI_APPLE_SET_OS_PROTOCOL_GUID  { 0xc5c5da95, 0x7d5c, 0x45e6, \
    { 0xb2, 0xf1, 0x3f, 0xd5, 0x2b, 0xb1, 0x00, 0x77 } }

typedef struct EfiAppleSetOsInterface {
    UINT64 Version;
    EFI_STATUS EFIAPI (*SetOsVersion) (IN CHAR8 *Version);
    EFI_STATUS EFIAPI (*SetOsVendor) (IN CHAR8 *Vendor);
} EfiAppleSetOsInterface;

// Function to tell the firmware that Mac OS X is being launched. This is
// required to work around problems on some Macs that don't fully
// initialize some hardware (especially video displays) when third-party
// OSes are launched in EFI mode.
EFI_STATUS SetAppleOSInfo (
    VOID
) {
    EFI_STATUS               Status;
    EFI_GUID                 apple_set_os_guid  = EFI_APPLE_SET_OS_PROTOCOL_GUID;
    CHAR16                  *AppleOSVersion     = NULL;
    CHAR8                   *AppleOSVersion8    = NULL;
    EfiAppleSetOsInterface  *SetOs              = NULL;

    Status = REFIT_CALL_3_WRAPPER(
        gBS->LocateProtocol,
        &apple_set_os_guid,
        NULL,
        (VOID**) &SetOs
    );

    // If not a Mac, ignore the call.
    if ((Status != EFI_SUCCESS) || (!SetOs)) {
        #if REFIT_DEBUG > 0
        LOG(2, LOG_LINE_NORMAL,
            L"Not a Mac ... Not setting Apple OS information"
        );
        #endif

        Status = EFI_SUCCESS;
    }
    else if (SetOs->Version != 0 && GlobalConfig.SpoofOSXVersion) {
        #if REFIT_DEBUG > 0
        LOG(1, LOG_LINE_NORMAL,
            L"Setting Apple OS information"
        );
        #endif

        AppleOSVersion = L"Mac OS";
        MergeStrings (&AppleOSVersion, GlobalConfig.SpoofOSXVersion, ' ');

        if (AppleOSVersion) {
            #if REFIT_DEBUG > 0
            LOG(2, LOG_LINE_NORMAL,
                L"Setting Apple OS information to '%s'",
                AppleOSVersion
            );
            #endif

            AppleOSVersion8 = AllocateZeroPool (
                (StrLen (AppleOSVersion) + 1) * sizeof (CHAR8)
            );
            if (!AppleOSVersion8) {
                #if REFIT_DEBUG > 0
                LOG(1, LOG_THREE_STAR_SEP,
                    L"Memory Error!!"
                );
                #endif

                Status = EFI_OUT_OF_RESOURCES;
            }
            else {
                UnicodeStrToAsciiStr (AppleOSVersion, AppleOSVersion8);
                Status = REFIT_CALL_1_WRAPPER(
                    SetOs->SetOsVersion, AppleOSVersion8
                );
                if (!EFI_ERROR (Status)) {
                    Status = EFI_SUCCESS;
                }
                MyFreePool (&AppleOSVersion8);
            }

            if (Status == EFI_SUCCESS && SetOs->Version >= 2) {
                Status = REFIT_CALL_1_WRAPPER(
                    SetOs->SetOsVendor, (CHAR8 *) "Apple Inc."
                );
            }
            MyFreePool (&AppleOSVersion);
        } // if (AppleOSVersion)
    } // if/else

    return Status;
} // EFI_STATUS SetAppleOSInfo()
