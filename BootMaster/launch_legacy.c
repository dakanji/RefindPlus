/*
 * BootMaster/launch_legacy.c
 * Functions related to Legacy BIOS Booting
 *
 * Copyright (c) 2006 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Modifications for rEFInd Copyright (c) 2012-2015 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 *
 */
/**
** Modified for RefindPlus
** Copyright (c) 2020-2026 Dayo Akanji (sf.net/u/dakanji/profile)
**
** Modifications distributed under the preceding terms.
**/

#include "global.h"
#include "icns.h"
#include "lib.h"
#include "menu.h"
#include "scan.h"
#include "mystrings.h"
#include "screenmgt.h"
#include "launch_legacy.h"
#include "../include/Handle.h"
#include "../include/refit_call_wrapper.h"
#include "../include/syslinux_mbr.h"
#include "../EfiLib/BdsHelper.h"
#include "../EfiLib/legacy.h"


extern BOOLEAN            IsBoot;
extern BOOLEAN            DisplayLoader;
extern REFIT_MENU_SCREEN *MainMenu;

#ifndef __MAKEWITH_GNUEFI
#define LibLocateHandle gBS->LocateHandleBuffer
#define DevicePathProtocol gEfiDevicePathProtocolGuid
#endif

// Previously 'MAX_DISCOVERED_PATHS'
#define MAX_UNIQUE_PATHS (16)

// Previously 'LegacyLoaderMediaPathData'
static
UINT8 AppleBootCampAppMedia[] = {
    0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
    0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
    0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};

// Previously 'LegacyLoaderMediaPath'
static
EFI_DEVICE_PATH_PROTOCOL *AppleBootCampFile = (EFI_DEVICE_PATH_PROTOCOL *) AppleBootCampAppMedia;

static
EFI_GUID AppleVariableVendorID = {
    0x7C436110, 0xAB2A, 0x4BBB,
    { 0xA8, 0x80, 0xFE, 0x41, 0x99, 0x5C, 0x9F, 0x82 }
};

BOOLEAN FirstLegacyScan  = TRUE;

static
EFI_STATUS ActivateMbrPartition (
    IN EFI_BLOCK_IO_PROTOCOL *BlockIO,
    IN UINTN                  PartitionIndex
) {
    EFI_STATUS           Status;
    UINTN                LogicalPartitionIndex, i;
    UINT8                SectorBuffer[512];
    UINT32               NextExtCurrent;
    UINT32               ExtCurrent;
    UINT32               ExtBase;
    BOOLEAN              HaveBootCode;
    MBR_PARTITION_INFO  *MbrTableEx;
    MBR_PARTITION_INFO  *MbrTable;


    // Read MBR
    Status = REFIT_CALL_5_WRAPPER(
        BlockIO->ReadBlocks, BlockIO,
        BlockIO->Media->MediaId, 0,
        512, SectorBuffer
    );
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (*((UINT16 *)(SectorBuffer + 510)) != 0xaa55) {
        // Safety Measure #1
        return EFI_NOT_FOUND;
    }

    // Add boot code if necessary
    HaveBootCode = FALSE;
    for (i = 0; i < MBR_BOOTCODE_SIZE; i++) {
        if (SectorBuffer[i] != 0) {
            HaveBootCode = TRUE;
            break;
        }
    } // for

    if (!HaveBootCode) {
        // No boot code found in the MBR ... Add syslinux MBR code
        REFIT_CALL_3_WRAPPER(
            gBS->SetMem, SectorBuffer,
            MBR_BOOTCODE_SIZE, 0
        );
        REFIT_CALL_3_WRAPPER(
            gBS->CopyMem, SectorBuffer,
            syslinux_mbr, SYSLINUX_MBR_SIZE
        );
    }

    // Set partition as active
    MbrTable = (MBR_PARTITION_INFO *) (SectorBuffer + 446);
    ExtBase = 0;
    for (i = 0; i < 4; i++) {
        if (MbrTable[i].Flags != 0x00 &&
            MbrTable[i].Flags != 0x80
        ) {
            // Safety Measure #2
            return EFI_NOT_FOUND;
        }

        if (i == PartitionIndex) {
            MbrTable[i].Flags = 0x80;
        }
        else if (
            PartitionIndex >= 4 &&
            IS_EXTENDED_PART_TYPE(MbrTable[i].Type)
        ) {
            MbrTable[i].Flags = 0x80;
            ExtBase = MbrTable[i].StartLBA;
        }
        else {
            MbrTable[i].Flags = 0x00;
        }
    }

    // Write MBR
    Status = REFIT_CALL_5_WRAPPER(
        BlockIO->WriteBlocks, BlockIO,
        BlockIO->Media->MediaId, 0,
        512, SectorBuffer
    );
    if (EFI_ERROR(Status) ||
        PartitionIndex < 4
    ) {
        // Early Return
        return Status;
    }

    // Must activate a logical partition ... Walk the EMBR chain
    //
    // NB: ExtBase was set above while looking at the MBR table
    for (
        ExtCurrent = ExtBase;
        ExtCurrent;
        ExtCurrent = NextExtCurrent
    ) {
        // Read current EMBR
        Status = REFIT_CALL_5_WRAPPER(
            BlockIO->ReadBlocks, BlockIO,
            BlockIO->Media->MediaId, ExtCurrent,
            512, SectorBuffer
        );
        if (EFI_ERROR(Status)) {
            return Status;
        }

        if (*((UINT16 *)(SectorBuffer + 510)) != 0xaa55) {
            // Safety Measure #3
            return EFI_NOT_FOUND;
        }

        // Scan EMBR ... Set appropriate partition active
        NextExtCurrent = 0;
        LogicalPartitionIndex = 4;
        MbrTableEx = (MBR_PARTITION_INFO *) (SectorBuffer + 446);
        for (i = 0; i < 4; i++) {
            if (MbrTableEx[i].Flags != 0x00 &&
                MbrTableEx[i].Flags != 0x80
            ) {
                // Safety Measure #4
                return EFI_NOT_FOUND;
            }

            if (MbrTableEx[i].Size     == 0 ||
                MbrTableEx[i].StartLBA == 0
            ) {
                break;
            }

            if (IS_EXTENDED_PART_TYPE(MbrTableEx[i].Type)) {
                // Link to next EMBR
                NextExtCurrent = ExtBase + MbrTableEx[i].StartLBA;
                MbrTableEx[i].Flags = (
                    PartitionIndex >= LogicalPartitionIndex
                ) ? 0x80 : 0x00;

                break;
            }

            // Logical Partition
            MbrTableEx[i].Flags = (
                PartitionIndex == LogicalPartitionIndex
            ) ? 0x80 : 0x00;

            LogicalPartitionIndex++;
        }

        // Write Current EMBR
        Status = REFIT_CALL_5_WRAPPER(
            BlockIO->WriteBlocks, BlockIO,
            BlockIO->Media->MediaId, ExtCurrent,
            512, SectorBuffer
        );
        if (EFI_ERROR(Status)) {
            return Status;
        }

        if (PartitionIndex < LogicalPartitionIndex) {
            // Stop the loop ... Ignore other EMBRs
            break;
        }
    } // for

    return EFI_SUCCESS;
} // static EFI_STATUS ActivateMbrPartition()

// Previously 'WriteBootDiskHint'
static
EFI_STATUS SetLegacyBootHintMac (
    IN EFI_DEVICE_PATH_PROTOCOL *WholeDiskDevicePath
){
   EFI_STATUS Status;

   Status = EfivarSetRaw (
       &AppleVariableVendorID, L"BootCampHD",
       WholeDiskDevicePath, GetDevicePathSize (WholeDiskDevicePath), TRUE
   );

   return Status;
} // EFI_STATUS SetLegacyBootHintMac

// Previously 'ExtractLegacyLoaderPaths'
static
EFI_STATUS FindBootCampFirmwareApp (
    EFI_DEVICE_PATH_PROTOCOL **PathList,
    UINTN                      MaxPaths
) {
    EFI_STATUS                 Status;
    EFI_STATUS                 StatusRet;
    UINTN                      PathIndex;
    UINTN                      PathCount;
    UINTN                      HandleCount;
    UINTN                      HandleIndex;
    BOOLEAN                    Seen;
    EFI_HANDLE                *Handles;
    EFI_HANDLE                 Handle;
    EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;


    MaxPaths--;  // Accomodate NULL Termination

    // Get all LoadedImage handles
    PathCount = HandleCount = 0;
    Status = LibLocateHandle (
        ByProtocol,
        &LoadedImageProtocol, NULL,
        &HandleCount, &Handles
    );
    if (EFI_ERROR(Status)) {
        CheckError (
            Status,
            L"While Listing LoadedImage Handles in 'FindBootCampFirmwareApp'"
        );
        PathList[PathCount] = NULL;

        return EFI_LOAD_ERROR;
    }

    StatusRet = EFI_NOT_FOUND;
    for (
        HandleIndex = 0;
        HandleIndex < HandleCount && PathCount < MaxPaths;
        HandleIndex++
    ) {
        Handle = Handles[HandleIndex];

        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, Handle,
            &LoadedImageProtocol, (VOID **) &LoadedImage
        );
        if (EFI_ERROR(Status)) {
            // Ignore Error ... Can only happen via firmware defect.
            continue;
        }

        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, LoadedImage->DeviceHandle,
            &DevicePathProtocol, (VOID **) &DevicePath
        );
        if (EFI_ERROR(Status)) {
            // Ignore Error ... Not significant.
            continue;
        }

        // Only grab memory range nodes
        if (DevicePathSubType (DevicePath) != HW_MEMMAP_DP      ||
            DevicePathType    (DevicePath) != HARDWARE_DEVICE_PATH
        ) {
            continue;
        }

        // Check if we have this device path in the list already
        // WARNING: Assumes first node in the device path is unique!
        Seen = FALSE;
        for (PathIndex = 0; PathIndex < PathCount; PathIndex++) {
            if (DevicePathNodeLength (
                    DevicePath
                ) != DevicePathNodeLength (
                    PathList[PathIndex]
                )
            ) {
                continue;
            }

            if (CompareMem (
                    DevicePath, PathList[PathIndex],
                    DevicePathNodeLength (DevicePath)
                ) == 0
            ) {
                Seen = TRUE;
                break;
            }
        } // for
        if (Seen) continue;

        PathList[PathCount++] = AppendDevicePath (
            DevicePath, AppleBootCampFile
        );

        StatusRet = EFI_SUCCESS;
    } // for HandleIndex
    MY_FREE_POOL(Handles);

    PathList[PathCount] = NULL;

    return StatusRet;
} // static EFI_STATUS FindBootCampFirmwareApp()

// Start device on Mac via AppleLegacyLoader AKA BootCamp
// Previously 'StartLegacyImageList'
static
EFI_STATUS LoadLegacyImageMac (
    IN     EFI_DEVICE_PATH_PROTOCOL **DevicePaths,
    IN     CHAR16                    *LoadOptions,
    IN OUT UINTN                     *ErrorInStep,
    EG_IMAGE                         *BootLogoImage  // Purely to be freed
) {
    EFI_STATUS                        Status;
    EFI_HANDLE                        ChildImageHandle;
    EFI_LOADED_IMAGE_PROTOCOL        *ChildLoadedImage;
    UINTN                             DevicePathIndex;


    // Default in case the DevicePath list is empty
    Status = EFI_LOAD_ERROR;

    // Load the image into memory
    for (
        DevicePathIndex = 0;
        DevicePaths[DevicePathIndex] != NULL;
        DevicePathIndex++
    ) {
        Status = REFIT_CALL_6_WRAPPER(
            gBS->LoadImage, FALSE,
            SelfImageHandle, DevicePaths[DevicePathIndex],
            NULL, 0, &ChildImageHandle
        );
        if (Status != EFI_NOT_FOUND) {
            break;
        }
    } // for

    if (EFI_ERROR(Status)) {
        CheckError (Status, L"While Loading Legacy Bootcode on Mac");
        *ErrorInStep = 1;

        goto bailout_direct;
    }

    ChildLoadedImage = NULL;
    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol, ChildImageHandle,
        &LoadedImageProtocol, (VOID **) &ChildLoadedImage
    );
    if (EFI_ERROR(Status)) {
        CheckError (
            Status,
            L"While Fetching 'AppleLegacyLoader' Handle"
        );
        *ErrorInStep = 2;

        goto bailout_unload;
    }

    ChildLoadedImage->LoadOptions     = (VOID *) LoadOptions;
    ChildLoadedImage->LoadOptionsSize = (
        LoadOptions != NULL
    ) ? ((UINT32) StrLen (LoadOptions) + 1) * sizeof (CHAR16) : 0;

    // Turn control over to child image
    // DA-TAG: (optionally) re-enable the EFI watchdog timer!

    // Close open file handles
    UninitRefitLib();

    #if REFIT_DEBUG > 0
    OUT_TAG();
    #endif

    if (GlobalConfig.BootLogoClear) {
        MY_FREE_IMAGE(BootLogoImage);
    }

    Status = REFIT_CALL_3_WRAPPER(
        gBS->StartImage, ChildImageHandle,
        NULL, NULL
    );
    if (EFI_ERROR(Status)) {
        CheckError (Status, L"Unexpected Return from AppleLegacyLoader");
        *ErrorInStep = 3;
    }

    // Control returned after error or 'Exit()' call by child image
    #if REFIT_DEBUG > 0
    RET_TAG();
    #endif

    // Re-open file handles
    ReinitRefitLib();

bailout_unload:
    // Unload child image
    REFIT_CALL_1_WRAPPER(
        gBS->UnloadImage,
        ChildImageHandle
    );

bailout_direct:
    // Free BootLogoImage on bailout
    MY_FREE_IMAGE(BootLogoImage);

    return Status;
} // static EFI_STATUS LoadLegacyImageMac()

static
EG_IMAGE * LegacyHelper (
    IN LEGACY_ENTRY *Entry,
    IN CHAR16       *SelectionName,
    IN BOOLEAN       TypeMac
) {
    CHAR16          *MsgStrA;
    EG_IMAGE        *BootLogoImage; // Must be freed by Caller


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    if (TypeMac) {
        MsgStrA = L"Load Legacy Bootcode:- 'Mac-Style'";
    }
    else {
        MsgStrA = L"Load Legacy Bootcode:- 'UEFI-Style'";
    }
    BeginExternalScreen (TRUE, MsgStrA);

    BREAD_CRUMB(L"%a:  2", __func__);
    if ((
            MyStrStr (Entry->Volume->OSName, L"Windows") &&
            !(GlobalConfig.DisableBootLogo & DISABLE_BOOTLOGO_WIN)
        ) || (
            MyStrStr (Entry->Volume->OSName, L"Linux"  ) &&
            !(GlobalConfig.DisableBootLogo & DISABLE_BOOTLOGO_LIN)
        )
    ) {
        BREAD_CRUMB(L"%a:  2a 1", __func__);
        BootLogoImage = LoadOSIcon (NULL, EXIT_SPLASH, TRUE);

        BREAD_CRUMB(L"%a:  2a 2", __func__);
        if (BootLogoImage == NULL) {
            BREAD_CRUMB(L"%a:  2a 2a 1", __func__);
            BootLogoImage = LoadOSIcon (
                Entry->Volume->OSIconName,
                L"legacy", TRUE
            );
            BREAD_CRUMB(L"%a:  2a 2a 2", __func__);
        }
        BREAD_CRUMB(L"%a:  2a 3", __func__);
    }
    else {
        BREAD_CRUMB(L"%a:  2b 1", __func__);
        BootLogoImage = NULL;
    }

    BREAD_CRUMB(L"%a:  3", __func__);
    if (BootLogoImage != NULL) {
        BREAD_CRUMB(L"%a:  3a 1", __func__);
        BltImageAlpha (
            BootLogoImage,
            (ScreenW - BootLogoImage->Width ) >> 1,
            (ScreenH - BootLogoImage->Height) >> 1,
            &(GlobalConfig.ScreenBackground->PixelData[0])
        );

        BREAD_CRUMB(L"%a:  3a 2", __func__);
        // Avoid mere flash
        //
        // Wait 0.75 seconds
        // DA-TAG: 100 Loops == 1 Sec
        RefitStall (75);
    }

    BREAD_CRUMB(L"%a:  4", __func__);
    if (IsBoot) {
        BREAD_CRUMB(L"%a:  4a 1", __func__);
        StoreLoaderName (SelectionName);
    }

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"%s for '%s'",
        MsgStrA, SelectionName
    );
    #endif

    BREAD_CRUMB(L"%a:  5 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return BootLogoImage; // Must be freed by Caller
} // static EG_IMAGE * LegacyHelper()

// Previously 'StartLegacy'
VOID LegacyBootMac (
    IN LEGACY_ENTRY *Entry,
    IN CHAR16       *SelectionName
) {
    EFI_STATUS                Status;
    UINTN                     ErrorInStep;
    CHAR16                   *MsgStrA;
    EG_IMAGE                 *BootLogoImage;
    EFI_DEVICE_PATH_PROTOCOL *BootCampAppList[MAX_UNIQUE_PATHS];


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    IsBoot = TRUE;

    BREAD_CRUMB(L"%a:  2", __func__);
    BootLogoImage = LegacyHelper (Entry, SelectionName, TRUE);

    BREAD_CRUMB(L"%a:  3", __func__);
    if (Entry->Volume->IsMbrPartition) {
        BREAD_CRUMB(L"%a:  3a 1", __func__);
        ActivateMbrPartition (
            Entry->Volume->WholeDiskBlockIO,
            Entry->Volume->MbrPartitionIndex
        );
        BREAD_CRUMB(L"%a:  3a 2", __func__);
    }

    BREAD_CRUMB(L"%a:  4", __func__);
    if (Entry->Volume->WholeDiskDevicePath != NULL &&
        Entry->Volume->DiskKind != DISK_KIND_OPTICAL
    ) {
        BREAD_CRUMB(L"%a:  4a 1", __func__);
        SetLegacyBootHintMac (Entry->Volume->WholeDiskDevicePath);
        BREAD_CRUMB(L"%a:  4a 2", __func__);
    }

    BREAD_CRUMB(L"%a:  5", __func__);
    Status = FindBootCampFirmwareApp (
        BootCampAppList, MAX_UNIQUE_PATHS
    );

    BREAD_CRUMB(L"%a:  6", __func__);
    ErrorInStep = 0;
    if (!EFI_ERROR(Status)) {
        Status = LoadLegacyImageMac (
            BootCampAppList, Entry->LoadOptions,
            &ErrorInStep, BootLogoImage
        );
    }

    BREAD_CRUMB(L"%a:  7", __func__);
    if (EFI_ERROR(Status)) {
        BREAD_CRUMB(L"%a:  7a 1", __func__);
        if (ErrorInStep < 2) {
            BREAD_CRUMB(L"%a:  7a 1a 1", __func__);
            SwitchToText (FALSE);

            BREAD_CRUMB(L"%a:  7a 1a 2", __func__);
            MsgStrA = (ErrorInStep == 0)
                ? L"Ensure Legacy BIOS Boot Support is Available"
                : L"Ensure Latest Firmware Updates Are Installed";
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->SetAttribute,
                gST->ConOut, ATTR_ERROR
            );

            PrintUglyText (MsgStrA, NEXTLINE);
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->SetAttribute,
                gST->ConOut, ATTR_BASIC
            );
            BREAD_CRUMB(L"%a:  7a 1a 3", __func__);

            #if REFIT_DEBUG > 0
            LOG_MSG("** WARN: %s", MsgStrA);
            LOG_MSG("\n\n");
            #endif

            BREAD_CRUMB(L"%a:  7a 1a 4", __func__);
            PauseForKey();
            BREAD_CRUMB(L"%a:  7a 1a 5", __func__);
            SwitchToGraphics();
        }
        else {
            BREAD_CRUMB(L"%a:  7a 1b 1", __func__);
            if (ErrorInStep == 3) {
                BREAD_CRUMB(L"%a:  7a 1b 1a 1", __func__);
                SwitchToText (FALSE);

                BREAD_CRUMB(L"%a:  7a 1b 1a 2", __func__);
                MsgStrA = L"Firmware Refused to Boot from Selected Volume";
                REFIT_CALL_2_WRAPPER(
                    gST->ConOut->SetAttribute,
                    gST->ConOut, ATTR_ERROR
                );
                PrintUglyText (MsgStrA, NEXTLINE);
                REFIT_CALL_2_WRAPPER(
                    gST->ConOut->SetAttribute,
                    gST->ConOut, ATTR_BASIC
                );

                #if REFIT_DEBUG > 0
                LOG_MSG("** WARN: %s", MsgStrA);
                #endif

                if (AppleFirmware) {
                    #if REFIT_DEBUG > 0
                    LOG_MSG("\n");
                    #endif
                    BREAD_CRUMB(L"%a:  7a 1b 1a 2a 1", __func__);
                    MsgStrA = L"Legacy Boot from External Drive *IS NOT* Well Supported by Apple Firmware";
                    PrintUglyText (MsgStrA, NEXTLINE);
                    #if REFIT_DEBUG > 0
                    LOG_MSG("         %s", MsgStrA);
                    #endif
                }

                #if REFIT_DEBUG > 0
                LOG_MSG("\n\n");
                #endif

                BREAD_CRUMB(L"%a:  7a 1b 1a 3", __func__);
                PauseForKey();

                BREAD_CRUMB(L"%a:  7a 1b 1a 4", __func__);
                SwitchToGraphics();
            } // if ErrorInStep == 3
            BREAD_CRUMB(L"%a:  7a 1b 2", __func__);
        } // if/else ErrorInStep < 2
        BREAD_CRUMB(L"%a:  7a 2", __func__);
    } // if EFI_ERROR(Status)

    BREAD_CRUMB(L"%a:  8", __func__);
    FinishExternalScreen();

    BREAD_CRUMB(L"%a:  9 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID LegacyBootMac()

// Start device on non-Mac via EFI_LEGACY_BIOS_PROTOCOL.
// Previously 'StartLegacyUEFI'
VOID LegacyBootUEFI (
    LEGACY_ENTRY *Entry,
    CHAR16       *SelectionName
) {
    CHAR16       *MsgStrA;
    EG_IMAGE     *BootLogoImage;


    IsBoot = TRUE;
    BootLogoImage = LegacyHelper (Entry, SelectionName, FALSE);

    UninitRefitLib();
    BdsLibConnectDevicePath (Entry->BdsOption->DevicePath);
    BdsLibDoLegacyBoot (Entry->BdsOption, BootLogoImage);

    // There was a failure if we get here.
    #if REFIT_DEBUG > 0
    RET_TAG();
    #endif
    ReinitRefitLib();

    MsgStrA = L"Failure on Loading 'UEFI-Style' Legacy Bootcode";

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStrA);
    #endif

    Print(L"%s", MsgStrA);
    PauseForKey();

    FinishExternalScreen();
} // static VOID LegacyBootUEFI()

static
VOID AddLegacyEntry (
    IN CHAR16         *LoaderTitle,
    IN REFIT_VOLUME   *Volume
) {
    LEGACY_ENTRY      *Entry;
    LEGACY_ENTRY      *SubEntry;
    REFIT_MENU_SCREEN *SubScreen;
    CHAR16            *LegacyTitle;
    CHAR16            *VolDesc;
    BOOLEAN            TempBool;


    if (LoaderTitle == NULL) {
        LoaderTitle = (
            Volume->OSName != NULL
        ) ? Volume->OSName : L"Legacy Bootcode";
    }

    VolDesc = (Volume->VolName != NULL)
        ? Volume->VolName
        : (Volume->DiskKind == DISK_KIND_OPTICAL)
            ? L"CD" : L"HD";

    LegacyTitle = PoolPrint (
        L"Load %s%s%s%s%s",
        LoaderTitle,
        SetVolJoin (LoaderTitle, TRUE                   ),
        SetVolKind (LoaderTitle, VolDesc, Volume->FSType),
        SetVolFlag (LoaderTitle, VolDesc                ),
        SetVolType (LoaderTitle, VolDesc, Volume->FSType)
    );

    TempBool = IsListItemSubstringIn (
        LegacyTitle, GlobalConfig.DontScanVolumes
    );
    if (TempBool) {
       MY_FREE_POOL(LegacyTitle);

       // Early Return
       return;
    }

    FirstLegacyScan = FALSE;

    // Prepare the menu entry
    Entry = AllocateZeroPool (sizeof (LEGACY_ENTRY));
    if (Entry == NULL) {
        MY_FREE_POOL(LegacyTitle);

        // Early Return
        return;
    }

    SetVolumeBadgeIcon (Volume);

    Entry->me.Row         = 0;
    Entry->Enabled        = TRUE;
    Entry->me.Tag         = TAG_LEGACY;
    Entry->me.Title       = LegacyTitle;
    Entry->me.SubScreen   = NULL; // Initial Setting
    Entry->me.ShortcutKey = 0;
    Entry->me.Image       = LoadOSIcon (Volume->OSIconName, L"legacy", FALSE);
    Entry->Volume         = CopyVolume (Volume);
    Entry->me.BadgeImage  = egCopyImage (Volume->VolBadgeImage);
    Entry->LoadOptions    = (  Volume->DiskKind != DISK_KIND_OPTICAL)
                            ? (Volume->DiskKind == DISK_KIND_EXTERNAL)
                                ? L"USB" : L"HD"
                            : L"CD";

    #if REFIT_DEBUG > 0
    LOG_MSG(
        "%s  - Found %s%s%s%s%s",
        OffsetNext,
        LoaderTitle,
        SetVolJoin (LoaderTitle, FALSE                  ),
        SetVolKind (LoaderTitle, VolDesc, Volume->FSType),
        SetVolFlag (LoaderTitle, VolDesc                ),
        SetVolType (LoaderTitle, VolDesc, Volume->FSType)
    );
    #endif

    // Create the submenu
    SubScreen = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (SubScreen == NULL) {
        FreeMenuEntry ((REFIT_MENU_ENTRY **) Entry);

        // Early Return
        return;
    }

    SubScreen->TitleImage = egCopyImage (Entry->me.Image);
    SubScreen->Title = PoolPrint (
        L"Boot Options for %s%s%s%s%s",
        LoaderTitle,
        SetVolJoin (LoaderTitle, TRUE                   ),
        SetVolKind (LoaderTitle, VolDesc, Volume->FSType),
        SetVolFlag (LoaderTitle, VolDesc                ),
        SetVolType (LoaderTitle, VolDesc, Volume->FSType)
    );

    SubScreen->Hint1 = StrDuplicate (SUBSCREEN_HINT1);
    SubScreen->Hint2 = (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR)
        ? StrDuplicate (SUBSCREEN_HINT2_NO_EDITOR)
        : StrDuplicate (SUBSCREEN_HINT2);

    // Default entry
    SubEntry = AllocateZeroPool (sizeof (LEGACY_ENTRY));
    if (SubEntry == NULL) {
        FreeMenuScreen (&SubScreen);
        FreeMenuEntry ((REFIT_MENU_ENTRY **) Entry);

        // Early Return
        return;
    }

    SubEntry->me.Title    = PoolPrint (L"Load %s", LoaderTitle);
    SubEntry->me.Tag      = TAG_LEGACY;
    SubEntry->Volume      = CopyVolume (Entry->Volume);
    SubEntry->LoadOptions = StrDuplicate (Entry->LoadOptions);

    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);

    TempBool = GetMenuEntryReturn (&SubScreen);
    if (TempBool) {
        Entry->me.SubScreen = SubScreen;
    }
    else {
        FreeMenuScreen (&SubScreen);
    }

    AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *) Entry);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_END,
        L"Successfully Created Menu Entry for %s",
        LoaderTitle
    );
    #endif
} // static VOID AddLegacyEntry()


/**
    Create RefindPlus boot option from Legacy BIOS protocol option.
*/
static
VOID AddLegacyEntryUEFI (
    BDS_COMMON_OPTION *BdsOption,
    IN UINT8           DiskType
) {
    LEGACY_ENTRY      *Entry;
    LEGACY_ENTRY      *SubEntry;
    REFIT_MENU_SCREEN *SubScreen;


    if (IsListItemSubstringIn (
            BdsOption->Description,
            GlobalConfig.DontScanVolumes
        )
    ) {
        // Early Return
        return;
    }

    // Remove stray spaces, since many EFIs produce descriptions with lots of
    //   extra spaces, especially at the end; this throws off centering of the
    //   description on the screen.
    LimitStringLength (BdsOption->Description, 100);

    // Prepare the menu entry
    Entry = AllocateZeroPool (sizeof (LEGACY_ENTRY));
    if (Entry == NULL) {
        // Early Return
        return;
    }

    Entry->me.Title = PoolPrint (
        L"Load Legacy Bootcode%s%s%s%s",
        SetVolJoin (L"Legacy Bootcode", TRUE                     ),
        SetVolKind (L"Legacy Bootcode", BdsOption->Description, 0),
        SetVolFlag (L"Legacy Bootcode", BdsOption->Description   ),
        SetVolType (L"Legacy Bootcode", BdsOption->Description, 0)
    );

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_MID,
        L"Append 'UEFI-Style' Legacy Entry for '%s'",
        Entry->me.Title
    );
    #endif

    Entry->me.Row         = 0;
    Entry->me.Tag         = TAG_LEGACY_UEFI;
    Entry->me.SubScreen   = NULL; // Initial Setting
    Entry->me.ShortcutKey = 0;
    if (GlobalConfig.HelpIcon) {
        Entry->me.Image = egFindIcon (
            L"os_legacy",
            GlobalConfig.IconSizes[ICON_SIZE_BIG]
        );
    }
    if (Entry->me.Image == NULL) {
        Entry->me.Image  = LoadOSIcon (
            L"legacy", L"legacy", FALSE
        );
    }
    Entry->LoadOptions   = (DiskType == BBS_CDROM)
                            ? L"CD"
                            : ((DiskType == BBS_USB)
                                ? L"USB" : L"HD");
    Entry->me.BadgeImage = GetDiskBadge (DiskType);
    Entry->BdsOption     = CopyBdsOption (BdsOption);
    Entry->Enabled       = TRUE;

    // Create the submenu
    SubScreen = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (SubScreen == NULL) {
        FreeMenuEntry ((REFIT_MENU_ENTRY **) Entry);

        // Early Return
        return;
    }

    SubScreen->TitleImage = egCopyImage (Entry->me.Image);
    SubScreen->Title = PoolPrint (
        L"Boot Options for Legacy Bootcode%s%s%s%s",
        SetVolJoin (L"Legacy Bootcode", TRUE                     ),
        SetVolKind (L"Legacy Bootcode", BdsOption->Description, 0),
        SetVolFlag (L"Legacy Bootcode", BdsOption->Description   ),
        SetVolType (L"Legacy Bootcode", BdsOption->Description, 0)
    );
    SubScreen->Hint1 = StrDuplicate (SUBSCREEN_HINT1);
    SubScreen->Hint2 = (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR)
        ? StrDuplicate (SUBSCREEN_HINT2_NO_EDITOR)
        : StrDuplicate (SUBSCREEN_HINT2);

    // Default entry
    SubEntry = AllocateZeroPool (sizeof (LEGACY_ENTRY));
    if (SubEntry == NULL) {
        FreeMenuScreen (&SubScreen);
        FreeMenuEntry ((REFIT_MENU_ENTRY **) Entry);

        // Early Return
        return;
    }

    SubEntry->me.Title  = PoolPrint (L"Load %s", BdsOption->Description);
    SubEntry->me.Tag    = TAG_LEGACY_UEFI;
    SubEntry->BdsOption = CopyBdsOption (BdsOption);

    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);

    if (!GetMenuEntryReturn (&SubScreen)) {
        FreeMenuScreen (&SubScreen);
    }
    Entry->me.SubScreen = SubScreen;

    AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *) Entry);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_END,
        L"Successfully Created Menu Entry for Legacy Bootcode for %s",
        BdsOption->Description
    );
    #endif
} // static VOID AddLegacyEntryUEFI()

/**
    Scan for legacy BIOS targets on machines with EFI_LEGACY_BIOS_PROTOCOL.
    Restricts output to disks of the specified 'DiskType'.
*/
static
VOID ScanLegacyUEFI (
    IN UINT8 DiskType
) {
    #if REFIT_DEBUG > 0
    UINTN                      LogLineType;
    #endif

    EFI_STATUS                 Status;
    UINT16                    *BootOrder;
    UINTN                      Index;
    UINTN                      BootOrderSize;
    CHAR16                     Buffer[20];
    CHAR16                     BootOption[10];
    BOOLEAN                    AssumeUSB;
    LIST_ENTRY                 TempList;
    BDS_COMMON_OPTION         *BdsOption;
    BBS_BBS_DEVICE_PATH       *BbsDevicePath;
    EFI_LEGACY_BIOS_PROTOCOL  *LegacyBIOS;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);

    #if REFIT_DEBUG > 0
    LogLineType = (
        FirstLegacyScan
    ) ? LOG_STAR_HEAD_SEP : LOG_LINE_THIN_SEP;

    ALT_LOG(1, LogLineType, L"'UEFI-Style' Legacy Boot Options");
    #endif

    FirstLegacyScan = FALSE;

    ZeroMem (Buffer, sizeof (Buffer));

    // If LegacyBIOS protocol is not implemented on this platform,
    //   we do not support this type of legacy boot on this machine.
    Status = REFIT_CALL_3_WRAPPER(
        gBS->LocateProtocol, &gEfiLegacyBootProtocolGuid,
        NULL, (VOID **) &LegacyBIOS
    );
    if (EFI_ERROR(Status)) {
        BREAD_CRUMB(L"%a:  Z - END:- VOID (LocateProtocol Error)", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    // UEFI calls USB drives "BBS_HARDDRIVE".
    // To distinguish from normal hard drives,
    //   "DiskType" was set wrongly earlier and is "translated" here.
    if (DiskType != BBS_USB) {
        AssumeUSB = FALSE;
    }
    else {
        AssumeUSB = TRUE;
        DiskType  = BBS_HARDDISK;
    }

    // Grab the boot order
    BootOrderSize = 0;
    BootOrder = BdsLibGetVariableAndSize (
        L"BootOrder",
        &gEfiGlobalVariableGuid,
        &BootOrderSize
    );
    if (BootOrder == NULL) {
        BootOrderSize = 0;
    }

    Index = 0;
    BbsDevicePath = NULL;
    while (Index < BootOrderSize / sizeof (UINT16)) {
        // Grab each boot option variable from the boot order
        //   and convert the variable into a BDS boot option
        SPrint (
            BootOption, sizeof (BootOption),
            L"Boot%04x", BootOrder[Index]
        );

        // Not building a list of boot options so init the head each time
        InitializeListHead (&TempList);
        BdsOption = BdsLibVariableToOption (&TempList, BootOption);
        if (BdsOption == NULL) {
            Index++;

            continue;
        }

        BbsDevicePath = (BBS_BBS_DEVICE_PATH *) BdsOption->DevicePath;
        // Only add the entry if it is of a requested type (e.g. USB, HD).
        // Two checks necessary because some systems return EFI boot loaders
        //   with a DeviceType value that would inappropriately
        //   identify them as legacy loaders.
        if (BbsDevicePath->DeviceType == DiskType &&
            BdsOption->DevicePath->Type == DEVICE_TYPE_BIOS
        ) {
            // USB flash drives appear as hard disks if certain media flags set.
            // Look for such and pass on, if present, with the technically
            //   incorrect but internally useful "BBS_TYPE_USB" flag.
            if (DiskType != BBS_HARDDISK) {
                DisplayLoader = TRUE;
                AddLegacyEntryUEFI (
                    BdsOption,
                    DiskType
                );
            }
            else if (
                AssumeUSB &&
                (
                    BbsDevicePath->StatusFlag &
                    (BBS_MEDIA_PRESENT | BBS_MEDIA_MAYBE_PRESENT)
                )
            ) {
                DisplayLoader = TRUE;
                AddLegacyEntryUEFI (
                    BdsOption,
                    BBS_USB
                );
            }
            else {
                if (!AssumeUSB &&
                    !(
                        BbsDevicePath->StatusFlag &
                        (BBS_MEDIA_PRESENT | BBS_MEDIA_MAYBE_PRESENT)
                    )
                ) {
                    DisplayLoader = TRUE;
                    AddLegacyEntryUEFI (
                        BdsOption,
                        DiskType
                    );
                }
            }
        } // if BbsDevicePath->DeviceType

        FreeBdsOption (&BdsOption);

        Index++;
    } // while

    MY_FREE_POOL(BootOrder);

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // static VOID ScanLegacyUEFI()

static
VOID ScanLegacyVolume (
    REFIT_VOLUME   *Volume,
    UINTN           BaseIndex
) {
    #if REFIT_DEBUG > 0
    CHAR16         *TheVolName;
    BOOLEAN         TypeWholeDisk;
    #endif

    UINTN           i;
    UINTN           VolumeIndex;
    CHAR16         *VentoyName;
    BOOLEAN         ShowVolume;
    BOOLEAN         HideIfOthersFound;

    static BOOLEAN  FoundVentoy = FALSE;


    if (Volume == NULL) {
        // Early Exit .. No Log
        return;
    }

    if (!Volume->HasBootCode) {
        // Early Exit .. No Log
        return;
    }

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    if (!VolumeScanAllowed (Volume, FALSE, TRUE)) {
        BREAD_CRUMB(L"%a:  1a 1 - END:- VOID - !VolumeScanAllowed", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Exit
        return;
    }

    BREAD_CRUMB(L"%a:  2 - HasBootCode and VolumeScanAllowed", __func__);
    if (Volume->FSType == FS_TYPE_WHOLEDISK ||
        (
            Volume->OSName           == NULL &&
            Volume->BlockIOOffset    == 0    &&
            Volume->WholeDiskBlockIO == Volume->BlockIO
        )
    ) {
        BREAD_CRUMB(L"%a:  2a 1 - MBR Entry Type = 'Whole Disk'", __func__);
        HideIfOthersFound = TRUE;

        #if REFIT_DEBUG > 0
        TypeWholeDisk = TRUE;
        #endif
    }
    else {
        BREAD_CRUMB(L"%a:  2b 1 - MBR Entry Type = 'Partition/Volume'", __func__);
        HideIfOthersFound = FALSE;

        #if REFIT_DEBUG > 0
        TypeWholeDisk = FALSE;
        #endif
    }

    #if REFIT_DEBUG > 0
    BREAD_CRUMB(L"%a:  3", __func__);
    if (Volume->VolName != NULL &&
        StrLen (Volume->VolName) > 0
    ) {
        BREAD_CRUMB(L"%a:  3a 1", __func__);
        TheVolName = Volume->VolName;
    }
    else {
        BREAD_CRUMB(L"%a:  3b 1", __func__);
        TheVolName = L"Unnamed";
    }

    BREAD_CRUMB(L"%a:  4", __func__);
    if (!TypeWholeDisk) {
        BREAD_CRUMB(L"%a:  4a 1", __func__);
        ALT_LOG(1, LOG_LINE_THIN_SEP,
            L"Handle Legacy Bootcode on Volume:- '%s'",
            TheVolName
        );
    }
    #endif

    BREAD_CRUMB(L"%a:  5", __func__);
    ShowVolume = TRUE;
    if (HideIfOthersFound) {
        #if REFIT_DEBUG > 1
        if (GlobalConfig.HandleVentoy) {
            BREAD_CRUMB(L"%a:  5a 1 - Check for Ventoy or Bootable Legacy Instances on *SAME* Disk", __func__);
        }
        else {
            BREAD_CRUMB(L"%a:  5b 1 - Check for Bootable Legacy Instances on *SAME* Disk", __func__);
        }
        #endif

        for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
            if (VolumeIndex == BaseIndex) {
                continue;
            }

            LOG_SEP(L"X");
            BREAD_CRUMB(L"%a:  5a 1a 1 - FOR LOOP:- START", __func__);
            /* coverity[copy_paste_error: SUPPRESS] */
            if (Volumes[VolumeIndex]->WholeDiskBlockIO == Volume->BlockIO       ||
                Volumes[VolumeIndex]->WholeDiskBlockIO == Volume->WholeDiskBlockIO
            ) {
                BREAD_CRUMB(L"%a:  5a 1a 1a 1", __func__);
                if (Volumes[VolumeIndex]->HasBootCode) {
                    BREAD_CRUMB(L"%a:  5a 1a 1a 1a 1 - Found Bootable Legacy Instance ... Set Whole Disk 'Skip' Flag", __func__);
                    ShowVolume = FALSE;
                }

                BREAD_CRUMB(L"%a:  5a 1a 1a 2", __func__);
                if (!FoundVentoy) {
                    BREAD_CRUMB(L"%a:  5a 1a 1a 2a 1", __func__);
                    i = 0;
                    while (ShowVolume && GlobalConfig.HandleVentoy) {
                        VentoyName = FindCommaDelimited (
                            VENTOY_NAMES, i++
                        );
                        if (VentoyName == NULL) break;

                        BREAD_CRUMB(L"%a:  5a 1a 1a 2a 1a 1 - WHILE LOOP:- START ... Check for Ventoy Partition", __func__);
                        if (MyStrBegins (VentoyName, Volumes[VolumeIndex]->VolName)) {
                            BREAD_CRUMB(L"%a:  5a 1a 1a 2a 1a 1a 1 - Found ... Set Whole Disk 'Skip' Flag", __func__);
                            ShowVolume = FALSE;
                        }
                        MY_FREE_POOL(VentoyName);
                        BREAD_CRUMB(L"%a:  5a 1a 1a 2a 1a 2 - WHILE LOOP:- END", __func__);
                    } // while
                    BREAD_CRUMB(L"%a:  5a 1a 1a 2a 2", __func__);
                } // if !FoundVentoy
                BREAD_CRUMB(L"%a:  5a 1a 1a 1a 3", __func__);
            } // if Volumes[VolumeIndex]->WholeDiskBlockIO
            BREAD_CRUMB(L"%a:  5a 1a 2 - FOR LOOP:- END", __func__);
            LOG_SEP(L"X");

            if (!ShowVolume) {
                break;
            }
        } // for
        BREAD_CRUMB(L"%a:  5a 2", __func__);
    }

    BREAD_CRUMB(L"%a:  6", __func__);
    if (!ShowVolume) {
        BREAD_CRUMB(L"%a:  6a 1 - END:- VOID ... Skip Whole Disk Volume", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return;
    }

    BREAD_CRUMB(L"%a:  7 - Process Legacy Boot Instance", __func__);
    DisplayLoader = TRUE;
    if ((Volume->VolName == NULL)   ||
        (StrLen (Volume->VolName) == 0)
    ) {
        BREAD_CRUMB(L"%a:  7a 1 - Get Legacy Boot Instance Name", __func__);
        Volume->VolName = GetVolumeName (Volume);
        BREAD_CRUMB(L"%a:  7a 2", __func__);
    }

    #if REFIT_DEBUG > 0
    BREAD_CRUMB(L"%a:  8", __func__);
    if (TypeWholeDisk) {
        BREAD_CRUMB(L"%a:  8a 1", __func__);
        // DA_TAG: In case 'Whole Disk' is being added
        ALT_LOG(1, LOG_LINE_THIN_SEP,
            L"Handle Legacy Bootcode on Volume:- '%s'",
            TheVolName
        );
    }
    #endif

    BREAD_CRUMB(L"%a:  9 - Add Legacy Boot Instance", __func__);
    AddLegacyEntry (NULL, Volume);

    BREAD_CRUMB(L"%a:  10 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // static VOID ScanLegacyVolume()

// Helper for Misc 'ScanLegacy' Funcions
static
VOID ScanLegacyEx (
    UINT8         DiskType
) {
    UINTN         VolumeIndex;
    REFIT_VOLUME *Volume;


    DisplayLoader = FALSE;
    FirstLegacyScan = TRUE;
    if (GlobalConfig.LegacyType == LEGACY_TYPE_UEFI ||
        GlobalConfig.LegacyType == LEGACY_TYPE_MAC2
    ) {
        ScanLegacyUEFI (DiskType);
    }
    else {
        if (GlobalConfig.LegacyType == LEGACY_TYPE_MAC1) {
            for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                Volume = Volumes[VolumeIndex];
                if ((
                        DiskType == BBS_HARDDISK &&
                        Volume->DiskKind == DISK_KIND_INTERNAL
                    ) || (
                        DiskType == BBS_USB &&
                        Volume->DiskKind == DISK_KIND_EXTERNAL
                    ) || (
                        DiskType == BBS_CDROM &&
                        Volume->DiskKind == DISK_KIND_OPTICAL
                    )
                ) {
                    ScanLegacyVolume (Volume, VolumeIndex);
                }
             } // for
        }
    }
    FirstLegacyScan = FALSE;

    #if REFIT_DEBUG > 0
    if (!DisplayLoader) {
        ALT_LOG(1, LOG_STAR_HEAD_SEP, L"None Found");
    }
    #endif
} // static VOID ScanLegacyEx()

// Scan attached optical discs for legacy boot code
//   and add anything found to the list.
VOID ScanLegacyDisc (VOID) {
    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_SEP,
        L"Scan for Optical Discs with Mode:- 'Legacy BIOS'"
    );
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);

    ScanLegacyEx (BBS_CDROM);

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID ScanLegacyDisc()

// Scan internal hard disks for legacy boot code
//   and add anything found to the list.
VOID ScanLegacyInternal (VOID) {
    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_SEP,
        L"Scan for Internal Disk Volumes with Mode:- 'Legacy BIOS'"
    );
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);

    // DA-TAG: Investigate This
    //         This may pick USB flash drives up.
    //         Try to find a way to differentiate.
    ScanLegacyEx (BBS_HARDDISK);

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID ScanLegacyInternal()

// Scan external disks for legacy boot code
//   and add anything found to the list.
VOID ScanLegacyExternal (VOID) {
    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_SEP,
        L"Scan for External Disk Volumes with Mode:- 'Legacy BIOS'"
    );
    #endif

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);

    // DA-TAG: Investigate This
    //         This does not actually do anything useful.
    //         Leaving in hope of fixing later.
    ScanLegacyEx (BBS_USB);

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID ScanLegacyExternal()

// Determine what/if legacy BIOS boot support is available
VOID FindLegacyBootType (VOID) {
    EFI_STATUS                 Status;
    EFI_LEGACY_BIOS_PROTOCOL  *LegacyBIOS;
    EFI_DEVICE_PATH_PROTOCOL  *BootCampAppList[MAX_UNIQUE_PATHS];


    if (AppleFirmware && !GlobalConfig.LegacySync) {
        // 'LegacySync' Inactive on Mac - Assume BIOS Boot Support
        GlobalConfig.LegacyType = LEGACY_TYPE_MAC8;

        return;
    }


    //
    // 'LegacySync' Active or Not a Mac
    //
    /* Locate UEFI Compatability Support Module (CSM) */
    Status = REFIT_CALL_3_WRAPPER(
        gBS->LocateProtocol, &gEfiLegacyBootProtocolGuid,
        NULL, (VOID **) &LegacyBIOS
    );
    if (!EFI_ERROR(Status)) {
        // UEFI Class 2 Mac/PC - Enable BIOS Boot Option
        GlobalConfig.LegacyType = (
            AppleFirmware
        ) ? LEGACY_TYPE_MAC2 : LEGACY_TYPE_UEFI;

        return;
    }


    //
    // UEFI CSM Not Available
    //
    if (!AppleFirmware) {
        // UEFI Class 3 PC - Use 'Disabled' Setting
        GlobalConfig.LegacyType = LEGACY_TYPE_NONE;

        return;
    }


    //
    // Running on Mac with 'LegacySync' Active
    //
    /* Locate 'AppleLegacyLoader' Firmware App */
    Status = FindBootCampFirmwareApp (
        BootCampAppList, MAX_UNIQUE_PATHS
    );
    if (!EFI_ERROR(Status)) {
        // 'Apple Style' Class 2 Mac - Enable BIOS Boot Option
        GlobalConfig.LegacyType = LEGACY_TYPE_MAC1;
    }
    else {
        if ((gST->Hdr.Revision >> 16U) == 1 ||
            (gBS->Hdr.Revision >> 16U) == 1 ||
            (gRT->Hdr.Revision >> 16U) == 1
        ) {
            // Tag as 'Broken' on EFI 1.x
            GlobalConfig.LegacyType = LEGACY_TYPE_MAC9;
        }
        else {
            // UEFI Class 3 Mac - Disable BIOS Boot Option
            GlobalConfig.LegacyType = LEGACY_TYPE_MAC3;
        }
    }
} // VOID FindLegacyBootType()

// Warn if legacy OS scans are enabled but the
//   firmware does not support legacy BIOS boot.
VOID WarnIfLegacyProblems (VOID) {
    #if REFIT_DEBUG > 0
    BOOLEAN   TmpLevel;
    BOOLEAN   CheckMute = FALSE;
    #endif

    UINTN     i;
    BOOLEAN   found;
    CHAR16   *MsgStr;
    CHAR16   *TxtMsg;
    CHAR16   *ExtMsg;
    CHAR16   *Spacer;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  A - START", __func__);

    if (GlobalConfig.LegacyType != LEGACY_TYPE_NONE &&
        GlobalConfig.LegacyType != LEGACY_TYPE_MAC3
    ) {
        BREAD_CRUMB(L"%a:  A1 - END:- VOID ... Early Return", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    i = 0;
    found = FALSE;
    do {
        if (GlobalConfig.ScanFor[i] == 'H' || GlobalConfig.ScanFor[i] == 'h' ||
            GlobalConfig.ScanFor[i] == 'C' || GlobalConfig.ScanFor[i] == 'c' ||
            GlobalConfig.ScanFor[i] == 'B' || GlobalConfig.ScanFor[i] == 'b'
        ) {
            found = TRUE;
        }
        i++;
    } while ((i < NUM_SCAN_OPTIONS) && (!found));

    if (!found) {
        BREAD_CRUMB(L"%a:  A2 - END:- VOID ... Early Return", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return;
    }

    #if REFIT_DEBUG > 0
    MsgStr = L"Legacy BIOS Boot Scan Enabled but *NOT* Available in Firmware";
    TmpLevel = (GlobalConfig.LogLevel == 0) ? TRUE : FALSE;
    if (TmpLevel) {
        GlobalConfig.LogLevel = 1;
    }
    ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
    if (TmpLevel) {
        GlobalConfig.LogLevel = 0;
    }
    #endif

    TxtMsg = L"WARN: Legacy BIOS Boot Issues                                  ";
    Spacer = L"                                                               ";
    MsgStr = L"Your 'scanfor' config line specifies scanning for one or more  \n"
             L"legacy (BIOS) boot options but this *IS NOT* possible as your  \n"
             L"computer lacks the required Compatibility Support Module (CSM) \n"
             L"or because Legacy BIOS Boot has been disabled in your firmware.";
    ExtMsg = L"Remove the legacy (BIOS) boot settings from the 'scanfor' list.\n"
             L"  - Items to remove from list: hdbios, biosexternal and cdbios.";

    #if REFIT_DEBUG > 0
    if (TmpLevel) {
        GlobalConfig.LogLevel = 1;
        LOG_SEP(L"X");
    }
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s\n%s", TxtMsg, MsgStr);
    if (AppleFirmware) {
        ALT_LOG(1, LOG_LINE_NORMAL, L"\n%s", ExtMsg);
    }
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    if (TmpLevel) {
        GlobalConfig.LogLevel = 0;
    }
    #endif

    if (!GlobalConfig.DirectBoot) {
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif
        SwitchToText (FALSE);

        REFIT_CALL_2_WRAPPER(
            gST->ConOut->SetAttribute,
            gST->ConOut, ATTR_ERROR
        );
        PrintUglyText (Spacer, NEXTLINE);
        PrintUglyText (TxtMsg, NEXTLINE);
        PrintUglyText (Spacer, NEXTLINE);

        REFIT_CALL_2_WRAPPER(
            gST->ConOut->SetAttribute,
            gST->ConOut, ATTR_BASIC
        );
        PrintUglyText (Spacer, NEXTLINE);
        PrintUglyText (MsgStr, NEXTLINE);
        PrintUglyText (Spacer, NEXTLINE);
        if (AppleFirmware) {
            PrintUglyText (ExtMsg, NEXTLINE);
            PrintUglyText (Spacer, NEXTLINE);
        }

        PauseForKey();
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif
    }

    BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID WarnIfLegacyProblems()
