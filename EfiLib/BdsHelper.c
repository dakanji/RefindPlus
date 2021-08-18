/*
 * EfiLib/BdsHelper.c
 * Functions to call legacy BIOS API.
 *
 */
/**
 * Copyright (c) 2004 - 2014, Intel Corporation. All rights reserved.<BR>
 * This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the BSD License
 * which accompanies this distribution.  The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/
/**
 * Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
**/

#include "BdsHelper.h"
#include "legacy.h"
#include "mystrings.h"
#include "../BootMaster/screenmgt.h"
#include "../BootMaster/lib.h"
#include "../include/refit_call_wrapper.h"

EFI_GUID gEfiLegacyBootProtocolGuid = { 0xdb9a1e3d, 0x45cb, 0x4abb, \
    { 0x85, 0x3b, 0xe5, 0x38, 0x7f, 0xdb, 0x2e, 0x2d }};

/**
    Internal helper function.

    Update the BBS Table so that devices of DeviceType have their boot priority
    updated to a high/bootable value.

    See "DeviceType values" in
    http://www.intel.com/content/dam/doc/reference-guide/efi-compatibility-support-module-specification-v097.pdf

    NOTE: This function should probably be refactored! Currently, all devices of
    type are enabled. This should be updated so that only a specific device is
    enabled. The wrong device could boot if there are multiple targets of the same
    type.

    @param DeviceType   The device type that we wish to enable
**/
VOID UpdateBbsTable (
    BDS_COMMON_OPTION *Option
) {
    UINT16                     Idx;
    EFI_LEGACY_BIOS_PROTOCOL  *LegacyBios;
    EFI_STATUS                 Status;
    UINT16                     HddCount      = 0;
    HDD_INFO                  *HddInfo       = NULL;
    UINT16                     BbsCount      = 0;
    BBS_TABLE                 *LocalBbsTable = NULL;
    BBS_BBS_DEVICE_PATH       *OptionBBS;
    CHAR16                     Desc[100];

    Status = REFIT_CALL_3_WRAPPER(
        gBS->LocateProtocol,
        &gEfiLegacyBootProtocolGuid,
        NULL,
        (VOID **) &LegacyBios
    );

    if (EFI_ERROR(Status) || Option == NULL) {
        return;
    }

    OptionBBS = (BBS_BBS_DEVICE_PATH *) Option->DevicePath;
    REFIT_CALL_5_WRAPPER(
        LegacyBios->GetBbsInfo,
        LegacyBios,
        &HddCount,
        &HddInfo,
        &BbsCount,
        &LocalBbsTable
    );

    for (Idx = 0; Idx < BbsCount; Idx++) {
        if(LocalBbsTable[Idx].DeviceType == 0) {
            continue;
        }

        BdsBuildLegacyDevNameString (&LocalBbsTable[Idx], Idx, sizeof (Desc), Desc);

        // Set devices of a particular type to BootPriority of 0 or 1. 0 is the highest priority.
        if (LocalBbsTable[Idx].DeviceType == OptionBBS->DeviceType) {
            if (MyStriCmp(Desc, Option->Description)) {
                // This entry exactly matches what we are looking for; make it highest priority
                LocalBbsTable[Idx].BootPriority = 0;
            }
            else {
                // This entry does not exactly match, but is the right disk type; make it a bit lower
                // in priority. Done mainly as a fallback in case of string-matching weirdness.
                LocalBbsTable[Idx].BootPriority = 1;
            }
        }
        else if (LocalBbsTable[Idx].BootPriority <= 1) {
            // Something has got a high enough boot priority to interfere with booting
            // our chosen entry, so bump it down a bit.
            LocalBbsTable[Idx].BootPriority = 2;
        }
    } // for
} // UpdateBbsTable();

/**
    Boot the legacy system with the boot option

    @param  Option           Legacy boot option with BBS device paths

    @retval EFI_UNSUPPORTED  No legacybios protocol. Does not support legacy boot.
    @retval EFI_STATUS       Return the status of LegacyBios->LegacyBoot().

**/
EFI_STATUS BdsLibDoLegacyBoot (
    IN  BDS_COMMON_OPTION *Option
) {
    EFI_STATUS                Status;
    EFI_LEGACY_BIOS_PROTOCOL  *LegacyBios;

    Status = REFIT_CALL_3_WRAPPER(
        gBS->LocateProtocol,
        &gEfiLegacyBootProtocolGuid,
        NULL,
        (VOID **) &LegacyBios
    );

    if (EFI_ERROR(Status)) {
        return EFI_UNSUPPORTED;
    }

    UpdateBbsTable(Option);

    Status = REFIT_CALL_4_WRAPPER(
        LegacyBios->LegacyBoot,
        LegacyBios,
        (BBS_BBS_DEVICE_PATH *) Option->DevicePath,
        Option->LoadOptionsSize,
        Option->LoadOptions
    );

    return Status;
} // EFI_STATUS BdsLibDoLegacyBoot()
