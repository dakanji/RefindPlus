/*
 * BootMaster/launch_efi.c
 * Code related to launching EFI programs
 *
 * Copyright (c) 2006-2010 Christoph Pfisterer
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
 * Modifications for rEFInd Copyright (c) 2012-2024 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2024 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "lib.h"
#include "mok.h"
#include "scan.h"
#include "icns.h"
#include "menu.h"
#include "apple.h"
#include "mystrings.h"
#include "screenmgt.h"
#include "launch_efi.h"
#include "driver_support.h"
#include "../include/refit_call_wrapper.h"

//
// constants

#ifdef __MAKEWITH_GNUEFI
#ifndef EFI_SECURITY_VIOLATION
#define EFI_SECURITY_VIOLATION    EFIERR (26)
#endif
#endif

#if defined (EFI32)
#define EFI_STUB_ARCH           0x0000014c00004550
#elif defined (EFIAARCH64)
#define EFI_STUB_ARCH           0x0000aa6400004550
#else
#define EFI_STUB_ARCH           0x0000866400004550 /* Default: Assume EFIX64 */
#endif

#define APPLE_FAT_BINARY        0x0ef1fab9 /* ID for Apple "fat" binary */

// Amount of a file to read to search for the EFI identifying signatures.
// Signatures as far in as 3680 (0xE60) have been found, so read a bit more.
#define EFI_HEADER_SIZE 4096

CHAR16         *BootSelection = NULL;
CHAR16         *ValidText     = L"Invalid Loader";

extern BOOLEAN  IsBoot;
extern BOOLEAN  ShimFound;
extern BOOLEAN  SecureFlag;

static
VOID WarnSecureBootError(
    CHAR16  *Name,
    BOOLEAN  Verbose
) {
    CHAR16 *LoaderName;
    CHAR16 *MsgStrA;
    CHAR16 *MsgStrB;
    CHAR16 *MsgStrC;
    CHAR16 *MsgStrD;
    CHAR16 *MsgStrE;


    if (Name != NULL) {
        LoaderName = PoolPrint (L"'%s'", Name);
    }
    else {
        LoaderName = StrDuplicate (L"the Loader");
    }

    SwitchToText (FALSE);

    MsgStrA = PoolPrint (L"Secure Boot Validation Failure While Starting %s", LoaderName);
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText (MsgStrA,                                        NEXTLINE);
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

    if (SecureFlag && Verbose) {
        MsgStrB = PoolPrint (
            L"This computer is configured with Secure Boot active but %s has failed validation",
            LoaderName
        );
        MsgStrC = PoolPrint (
            L" * Sign %s with a Machine Owner Key (MOK)",
            LoaderName
        );
        MsgStrD = PoolPrint (
            L" * Use a MOK utility to add a MOK with which %s has already been signed",
            LoaderName
        );
        MsgStrE = PoolPrint (
            L" * Use a MOK utility to register %s ('Enroll Hash') without signing it",
            LoaderName
        );

        PrintUglyText (MsgStrB,                                        NEXTLINE);
        PrintUglyText (L"You can:",                                    NEXTLINE);
        PrintUglyText (L" * Launch another boot loader",               NEXTLINE);
        PrintUglyText (L" * Disable Secure Boot in your firmware",     NEXTLINE);
        PrintUglyText (MsgStrC,                                        NEXTLINE);
        PrintUglyText (MsgStrD,                                        NEXTLINE);
        PrintUglyText (MsgStrE,                                        NEXTLINE);
        PrintUglyText (
            L"See http://www.rodsbooks.com/refind/secureboot.html for more information",
            NEXTLINE
        );

        MY_FREE_POOL(MsgStrE);
        MY_FREE_POOL(MsgStrD);
        MY_FREE_POOL(MsgStrC);
        MY_FREE_POOL(MsgStrB);
    } // if
    PauseForKey();
    SwitchToGraphics();

    MY_FREE_POOL(MsgStrA);
    MY_FREE_POOL(LoaderName);
} // static VOID WarnSecureBootError()

// See http://www.thomas-krenn.com/en/wiki/Activating_the_Intel_VT_Virtualization_Feature
// for information on Intel VMX features
static
VOID DoEnableAndLockVMX(VOID) {
#if defined (EFIX64) | defined (EFI32)
    UINT32 msr;
    UINT32 low_bits;
    UINT32 high_bits;


    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Attempt to Enable and Lock VMX");
    #endif

    // Is VMX active ?
    msr = 0x3a;
    low_bits = high_bits = 0;
    __asm__ volatile ("rdmsr" : "=a" (low_bits), "=d" (high_bits) : "c" (msr));

    // Enable and lock vmx if not locked
    if ((low_bits & 1) == 0) {
        high_bits = 0;
        low_bits = 0x05;
        msr = 0x3a;
        __asm__ volatile ("wrmsr" : : "c" (msr), "a" (low_bits), "d" (high_bits));
    }
#endif
} // static VOID DoEnableAndLockVMX()

// Load APFS Recovery Instance
static
EFI_STATUS RecoveryBootAPFS (
    IN LOADER_ENTRY *Entry
) {
    if (!SingleAPFS) {
        // Early Return
        return EFI_INVALID_PARAMETER;
    }


#if 0
// DA-TAG: Force Error Until Finalised - START


    EFI_STATUS  Status;
    CHAR16     *InitNVRAM;
    CHAR16     *NameNVRAM;
    CHAR8      *DataNVRAM;
    UINTN       Size;

    // Set Relevant NVRAM Variable
    DataNVRAM = NULL;
    InitNVRAM = L"RecoveryModeDisk";
    NameNVRAM = L"internet-recovery-mode";

    UnicodeStrToAsciiStr (InitNVRAM, DataNVRAM);

    Status = EfivarSetRaw (
        &AppleBootGuid, NameNVRAM,
        DataNVRAM, AsciiStrSize (DataNVRAM), TRUE
    );
    MY_FREE_POOL(NameNVRAM);
    MY_FREE_POOL(DataNVRAM);
    if (EFI_ERROR(Status)) {
        // Early Return
        return Status;
    }

    // Set Recovery Initiator
    NameNVRAM = L"RecoveryBootInitiator";
    Status = EfivarSetRaw (
        &AppleBootGuid, NameNVRAM,
        (VOID **) &Entry->Volume->DevicePath, StrSize (DevicePathToStr (Entry->Volume->DevicePath)), TRUE
    );
    MY_FREE_POOL(NameNVRAM);
    MY_FREE_POOL(DataNVRAM);
    if (EFI_ERROR(Status)) {
        // Early Return
        return Status;
    }

    // Construct Boot Entry
    Entry->EfiBootNum = StrToHex (L"80", 0, 16);
    MY_FREE_POOL(Entry->EfiLoaderPath);
    Entry->EfiLoaderPath = FileDevicePath (Entry->Volume->DeviceHandle, Entry->LoaderPath);

    Status = ConstructBootEntry (
        Entry->Volume->DeviceHandle,
        Basename (DevicePathToStr (Entry->EfiLoaderPath)),
        L"Mac Recovery",
        (CHAR8**) &Entry->EfiLoaderPath,
        &Size
    );
    if (EFI_ERROR(Status)) {
        // Early Return
        return Status;
    }

    // Set as BootNext entry
    Status = EfivarSetRaw (
        &GlobalGuid, L"BootNext",
        &(Entry->EfiBootNum), sizeof (UINT16), TRUE
    );
    if (EFI_ERROR(Status)) {
        // Early Return
        return Status;
    }

    // Reboot into new BootNext entry
    REFIT_CALL_4_WRAPPER(
        gRT->ResetSystem, EfiResetCold,
        EFI_SUCCESS, 0, NULL
    );


// DA-TAG: Force Error Until Finalised - END
#endif


    return EFI_LOAD_ERROR;
} // static EFI_STATUS RecoveryBootAPFS()

#if REFIT_DEBUG > 0
static
VOID LogAssumedValid (
    CHAR16   *FileName,
    BOOLEAN   FIreWire
) {
    ValidText = L"EFI File is *CONSIDERED* to be Valid";
    ALT_LOG(1, LOG_THREE_STAR_MID,
        L"%s%s:- '%s'",
        ValidText,
        FIreWire ? L" on Apple Firmware (FireWire Workaround)" : L"",
        FileName ? FileName : L"NULL File"
    );
    if (IsBoot) {
        LOG_MSG("\n");
        LOG_MSG("%s ... Loading", ValidText);
    }
} // static VOID LogAssumedValid()
#endif

// Returns TRUE if this file is a valid EFI loader file, and is proper ARCH
BOOLEAN IsValidLoader (
    EFI_FILE_PROTOCOL *RootDir,
    CHAR16            *FileName
) {
    //UINTN            LoaderType;

    if (AppleFirmware &&
        (RootDir == NULL || FileName == NULL)
    ) {
        // DA-TAG: Investigate This
        //         Assume "Valid" here, because Macs produce a NULL RootDir, and maybe FileName,
        //         when launching from Firewire drives. This should be handled better, but the
        //         fix would have to be in StartEFIImage() and/or in FindVolumeAndFilename().
        #if REFIT_DEBUG > 0
        LogAssumedValid (FileName, TRUE);
        #endif

        //LoaderType = LOADER_TYPE_EFI;

        return TRUE;
    }

#if !defined (EFIX64) && !defined (EFI32) && !defined (EFIAARCH64)
    #if REFIT_DEBUG > 0
    LogAssumedValid (FileName, FALSE);
    #endif

    //LoaderType = LOADER_TYPE_EFI;

    return TRUE;
#else
    EFI_STATUS       Status;
    BOOLEAN          IsValid;
    BOOLEAN          AppleBinaryFat;
    BOOLEAN          AppleBinaryPlain;
    UINTN            SignaturePos;
    UINTN            LoadedSize;
    CHAR8           *Header;
    UINT64           PESig;
    EFI_FILE_HANDLE  FileHandle;

    #if REFIT_DEBUG > 0
    CHAR16          *AbortReason;
    #endif

    Header = AllocatePool (EFI_HEADER_SIZE);
    if (Header == NULL) {
        // DA-TAG: Set ValidText in REL for 'FALSE' outcome
        //         Allows accurate screen message
        ValidText = L"EFI File is *ASSUMED* to be Invalid";

        #if REFIT_DEBUG > 0
        AbortReason = L":- 'Unable to Allocate Memory'";
        ALT_LOG(1, LOG_THREE_STAR_MID, L"%s ... Aborting%s", ValidText, AbortReason);
        LOG_MSG("\n\n");
        LOG_MSG("INFO: %s ... Aborting%s", ValidText, AbortReason);
        LOG_MSG("\n\n");
        #endif

        //LoaderType = LOADER_TYPE_INVALID;

        return FALSE;
    }

    do {
        IsValid = AppleBinaryFat = AppleBinaryPlain = FALSE;

        #if REFIT_DEBUG > 0
        AbortReason = L"";
        #endif

        if (!FileExists (RootDir, FileName)) {
            #if REFIT_DEBUG > 0
            AbortReason = L":- 'File *NOT* Found'";
            #endif

            //LoaderType = LOADER_TYPE_INVALID;

            // Early Return
            break;
        }

        Status = REFIT_CALL_5_WRAPPER(
            RootDir->Open, RootDir,
            &FileHandle, FileName,
            EFI_FILE_MODE_READ, 0
        );
        if (EFI_ERROR(Status)) {
            #if REFIT_DEBUG > 0
            AbortReason = L":- 'File Handle *NOT* Accessible'";
            #endif

            //LoaderType = LOADER_TYPE_INVALID;

            // Early Return
            break;
        }

        LoadedSize = EFI_HEADER_SIZE;

        Status = REFIT_CALL_3_WRAPPER(
            FileHandle->Read, FileHandle,
            &LoadedSize, Header
        );
        REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);
        if (EFI_ERROR(Status)) {
            #if REFIT_DEBUG > 0
            AbortReason = L":- 'File *IS NOT* Readable'";
            #endif

            //LoaderType = LOADER_TYPE_INVALID;

            // Early Return
            break;
        }

        // Search for indications that this is a gzipped file.
        // NB: This is currently only used for logging in RefindPlus
        // and that such loaders are considered invalid at this point.
        // GZipped loaders are mainly for ARM but our focus is on x86.
        if (GlobalConfig.GzippedLoaders &&
            Header[0] == (CHAR8) 0x1F   &&
            Header[1] == (CHAR8) 0x8B
        ) {
            #if REFIT_DEBUG > 0
            AbortReason = L":- 'GZipped Binary'";
            #endif

            //LoaderType = LOADER_TYPE_GZIP;

            // Early Return
            break;
        }

        // Search for standard PE32+ signature
        PESig        = EFI_STUB_ARCH;
        SignaturePos = *((UINT32 *) &Header[0x3c]);
        IsValid = (
            Header[0]   == 'M'                   &&
            Header[1]   == 'Z'                   &&
            LoadedSize  ==  EFI_HEADER_SIZE      &&
            SignaturePos < (EFI_HEADER_SIZE - 8) &&
            CompareMem(
                &Header[SignaturePos],
                &PESig, 6
            ) == 0
        );
        if (IsValid) {
            //LoaderType = LOADER_TYPE_EFI;

            // Early Return
            break;
        }

        // Search for Apple's 'Fat' Binary signature
        IsValid = AppleBinaryFat = (
            *((UINT32 *) Header) == APPLE_FAT_BINARY
        );
        if (IsValid) {
            //LoaderType = LOADER_TYPE_EFI;

            // Early Return
            break;
        }

        // Allow plain binaries on Apple firmware
        IsValid = AppleBinaryPlain = AppleFirmware;
        if (IsValid) {
            //LoaderType = LOADER_TYPE_EFI;

            // Early Return
            break;
        }

        // Invalid if we get here
        #if REFIT_DEBUG > 0
        AbortReason = L":- 'Unknown Binary Type'";
        #endif
    } while (0); // This 'loop' only runs once


    // DA-TAG: Set ValidText in REL for 'FALSE' outcome
    //         Allows accurate screen message
    // NOTES:  Assume 'Fat' binaries are valid on Apple firmware
    //         Assume the same for plain binaries on Apple firmware
    //         Test variables are only ever true on Apple firmware
    ValidText = (!IsValid)
        ? L"EFI File *IS NOT* Valid"
        : (AppleBinaryFat)
            ? L"EFI File (Apple 'Fat' Binary) is *ASSUMED* to be Valid"
            : (AppleBinaryPlain)
                ? L"EFI File ('Plain' Binary) is *ASSUMED* to be Valid on Apple Firmware"
                : L"EFI File is Valid";

    #if REFIT_DEBUG > 0
    if (!IsValid) {
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"%s:- '%s' ... Aborting%s",
            ValidText,
            FileName ? FileName : L"NULL File",
            AbortReason
        );
    }
    else {
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"%s:- '%s'",
            ValidText,
            FileName ? FileName : L"NULL File"
        );
    }
    #endif

    if (IsBoot) {
        // Reset IsBoot if required
        IsBoot = IsValid;

        #if REFIT_DEBUG > 0
        if (IsValid) {
            LOG_MSG("\n");
            LOG_MSG("%s ... Loading", ValidText);
        }
        else {
            LOG_MSG("\n\n");
            LOG_MSG("INFO: %s ... Aborting%s", ValidText, AbortReason);
            LOG_MSG("\n\n");
        }
        #endif
    }

    MY_FREE_POOL(Header);

    return IsValid;
#endif
} // BOOLEAN IsValidLoader()

// Launch an EFI binary
EFI_STATUS StartEFIImage (
    IN   REFIT_VOLUME  *Volume,
    IN   CHAR16        *Filename,
    IN   CHAR16        *LoadOptions,
    IN   CHAR16        *ImageTitle,
    IN   CHAR8          OSType,
    IN   BOOLEAN        Verbose,
    IN   BOOLEAN        IsDriver,
    OUT  EFI_HANDLE    *NewImageHandle OPTIONAL
) {
    EFI_STATUS                           Status;
    EFI_STATUS                           ReturnStatus;
    EFI_GUID                             SystemdGuid = SYSTEMD_GUID_VALUE;
    CHAR16                              *MsgStrTmp;
    CHAR16                              *MsgStrEx;
    CHAR16                              *MsgStr;
    CHAR16                              *EspGUID;
    CHAR16                              *FullLoadOptions;
    BOOLEAN                              LoaderValid;
    EFI_HANDLE                           ChildImageHandle;
    EFI_HANDLE                           TempImageHandle;
    EFI_DEVICE_PATH_PROTOCOL            *DevicePath;
    EFI_LOADED_IMAGE_PROTOCOL           *ChildLoadedImage;

    #if REFIT_DEBUG > 0
    CHAR16  *ConstMsgStr;

    BOOLEAN  CheckMute = FALSE;
    #endif

    if (Volume == NULL) {
        ReturnStatus = EFI_INVALID_PARAMETER;

        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (
            L"'%r' While Starting EFI Image!!",
            ReturnStatus
        );
        ALT_LOG(1, LOG_STAR_SEPARATOR, L"ERROR: %s", MsgStr);
        LOG_MSG("* ERROR: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        return ReturnStatus;
    }

    // Set load options
    if (LoadOptions == NULL) {
        FullLoadOptions = NULL;
    }
    else {
        FullLoadOptions = StrDuplicate (LoadOptions);

        // DA-TAG: The last space is also added by the uEFI shell and is
        //         significant when passing options to Apple's boot.efi
        if (OSType == 'M') {
            MergeStrings (&FullLoadOptions, L" ", 0);
        }
    }

    MsgStr = PoolPrint (
        L"Start '%s' with Load Options:- '%s'",
        ImageTitle, (FullLoadOptions != NULL) ? FullLoadOptions : L"NULL"
    );

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    #endif

    if (Verbose) {
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif
        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
        PrintUglyText (MsgStr, NEXTLINE);
        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif
    }

    MY_FREE_POOL(MsgStr);

    do {
        ReturnStatus = EFI_LOAD_ERROR;  // In case list is empty

        // DA-TAG: Investigate This
        //         Some EFIs crash if attempting to load drivers for an invalid
        //         architecture, so protect for this condition; but 'Volume'
        //         sometimes comes back NULL; so an exception is provided.
        //
        //         Handle this special condition better.
        LoaderValid = IsValidLoader (Volume->RootDir, Filename);
        if (!LoaderValid) {
            #if REFIT_DEBUG > 0
            MsgStr = StrDuplicate (L"Found Invalid Binary");
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
            LOG_MSG("\n\n");
            LOG_MSG("INFO: %s", MsgStr);
            MY_FREE_POOL(MsgStr);
            #endif

            MsgStr = PoolPrint (
                L"When Loading %s ... %s",
                ImageTitle, ValidText
            );
            ValidText = L"Invalid Binary";
            CheckError (ReturnStatus, MsgStr);
            MY_FREE_POOL(MsgStr);

            // Bail Out
            break;
        }

        DevicePath = FileDevicePath (Volume->DeviceHandle, Filename);

        #if REFIT_DEBUG < 1
        // Stall to avoid unwanted flash of text when starting loaders
        // Stall works best in smaller increments as per specs
        // Stall appears to be only needed on REL builds
        if (!IsDriver && (!AllowGraphicsMode || Verbose)) {
            // DA-TAG: 100 Loops == 1 Sec
            RefitStall (50);
        }
        #endif

        ChildImageHandle = NULL;
        // DA-TAG: Investigate This
        //         Commented-out lines below could be more efficient if the file
        //         were read ahead of time and passed as a pre-loaded image to
        //         LoadImage(), but it does not work on a 32-bit Mac Mini
        //         or a 64-bit Intel box when launching a Linux kernel.
        //         The kernel returns "Failed to handle fs_proto".
        //
        //         Track down the cause of this error, and fix it, if possible.
        /*
        ReturnStatus = REFIT_CALL_6_WRAPPER(
            gBS->LoadImage, FALSE,
            SelfImageHandle, DevicePath,
            ImageData, ImageSize, &ChildImageHandle
        );
        */
        ReturnStatus = REFIT_CALL_6_WRAPPER(
            gBS->LoadImage, FALSE,
            SelfImageHandle, DevicePath,
            NULL, 0, &ChildImageHandle
        );
        MY_FREE_POOL(DevicePath);

        if (SecureFlag && ShimFound) {
            // Load ourself into memory. This is a trick to work around a bug in
            // Shim 0.8, which hooks into gBS->LoadImage() and gBS->StartImage()
            // and then unregisters itself from the UEFI system table when its
            // replacement StartImage() function is called *IF* the previous
            // LoadImage() was for the same program. The result is that
            // RefindPlus can only validate the first program it launches (often
            // a filesystem driver). Loading a second program (RefindPlus itself
            // here to keep it smaller than a kernel) works around this problem.
            // See the replacements.c file in Shim, especially its start_image()
            // function, for the source of the problem.
            //
            // NOTE: This does not check the return status or handle errors.
            // It could conceivably do weird things if, say, RefindPlus were on
            // a USB drive that the user pulls before launching a program.
            #if REFIT_DEBUG > 0
            MsgStr = StrDuplicate (L"Shim 0.8 'Load/Start Image' Hack Applied");
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("\n\n");
            LOG_MSG("INFO: %s", MsgStr);
            MY_FREE_POOL(MsgStr);
            #endif

            // Ignore return status here
            TempImageHandle = NULL;
            REFIT_CALL_6_WRAPPER(
                gBS->LoadImage, FALSE,
                SelfImageHandle, GlobalConfig.SelfDevicePath,
                NULL, 0, &TempImageHandle
            );
        }

        if (ReturnStatus == EFI_ACCESS_DENIED   ||
            ReturnStatus == EFI_SECURITY_VIOLATION
        ) {
            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (
                L"'%r' Returned by Secure Boot While Loading %s",
                ReturnStatus, ImageTitle
            );
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"ERROR: %s", MsgStr);
            LOG_MSG("\n\n");
            LOG_MSG("INFO: %s", MsgStr);
            MY_FREE_POOL(MsgStr);
            #endif

            WarnSecureBootError (ImageTitle, Verbose);

            // Bail Out
            break;
        }

        do {
            ChildLoadedImage = NULL;
            ReturnStatus = REFIT_CALL_3_WRAPPER(
                gBS->HandleProtocol, ChildImageHandle,
                &LoadedImageProtocol, (VOID **) &ChildLoadedImage
            );
            if (EFI_ERROR(ReturnStatus)) {
                CheckError (
                    ReturnStatus,
                    L"While Getting 'Child' LoadedImageProtocol Handle"
                );

                // Unload and Bail Out
                break;
            }

            ChildLoadedImage->LoadOptions     = (VOID *) FullLoadOptions;
            ChildLoadedImage->LoadOptionsSize = (FullLoadOptions)
                ? ((UINT32) StrLen (FullLoadOptions) + 1) * sizeof (CHAR16)
                : 0;

            // DA-TAG: Investigate This
            //         Re-enable the EFI watchdog timer (optionally)
            //
            // Turn control over to the image
            if (IsBoot &&
                OSType == 'L' &&
                GlobalConfig.WriteSystemdVars
            ) {
                // Inform SystemD of RefindPlus ESP
                EspGUID = GuidAsString (&(SelfVolume->PartGuid));

                #if REFIT_DEBUG > 0
                MsgStr = PoolPrint (
                    L"LoaderDevicePartUUID Value for SystemD:- '%s'",
                    EspGUID
                );
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                MY_FREE_POOL(MsgStr);
                #endif

                Status = EfivarSetRaw (
                    &SystemdGuid, L"LoaderDevicePartUUID",
                    EspGUID, StrLen (EspGUID) * 2 + 2, FALSE
                );
                #if REFIT_DEBUG > 0
                if (EFI_ERROR(Status)) {
                    MsgStr = PoolPrint (
                        L"'%r' When Setting 'LoaderDevicePartUUID' UEFI Variable",
                        Status
                    );
                    ALT_LOG(1, LOG_STAR_SEPARATOR, L"WARN: '%s'", MsgStr);
                    LOG_MSG("\n\n");
                    LOG_MSG("WARN: %s", MsgStr);
                    MY_FREE_POOL(MsgStr);
                }
                #endif

                MY_FREE_POOL(EspGUID);
            } // if GlobalConfig.WriteSystemdVars

            // Store loader name if booting and set to do so
            if (BootSelection != NULL) {
                if (IsBoot) {
                    StoreLoaderName (BootSelection);
                }
                BootSelection = NULL;
            }

            // DA-TAG: SyncAPFS items are typically no longer required if
            //         not loading drivers.  "Typically" as users may put
            //         uEFI Shell etc in the first row (loaders).
            //         These may return to the RefindPlus screen
            //         but any issues will be trivial.
            if (!IsDriver) {
                FreeSyncVolumes();
            }

            // Close open file handles
            UninitRefitLib();

            #if REFIT_DEBUG > 0
            if (IsDriver) {
                ConstMsgStr = L"UEFI Driver";
            }
            else {
                ConstMsgStr = L"Child Image";

                ALT_LOG(1, LOG_LINE_NORMAL,
                    L"Load %s via Loader:- '%s'",
                    ConstMsgStr, ImageTitle
                );
                OUT_TAG();
            }
            #endif

            ReturnStatus = REFIT_CALL_3_WRAPPER(
                gBS->StartImage, ChildImageHandle,
                NULL, NULL
            );

            /* Control returns here if the child image calls 'Exit()' */

            // DA-TAG: Used in 'ScanDriverDir()'
            NewImageHandle = ChildImageHandle;

            #if REFIT_DEBUG > 0
            MsgStrEx = PoolPrint (
                L"'%r' While Loading %s",
                ReturnStatus, ConstMsgStr
            );
            ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStrEx);
            if (!IsDriver) {
                LOG_MSG("%s", MsgStrEx);
                RET_TAG();
            }
            MY_FREE_POOL(MsgStrEx);
            #endif

            if (EFI_ERROR(ReturnStatus)) {
                // Reset IsBoot if required
                IsBoot = FALSE;

                MsgStrTmp = L"Returned from Child Image";
                if (IsDriver) {
                    MsgStrEx = PoolPrint (L"%s:- '%s'", MsgStrTmp, ImageTitle);
                }
                else {
                    MsgStrEx = StrDuplicate (MsgStrTmp);

                    #if REFIT_DEBUG > 0
                    MY_MUTELOGGER_SET;
                    #endif
                    if (ReturnStatus == EFI_NOT_FOUND &&
                        FindSubStr (ImageTitle, L"gptsync")
                    ) {
                        SwitchToText (FALSE);
                        PauseSeconds (4);
                        PrintUglyText (L"                                            ", NEXTLINE);
                        PrintUglyText (L"                                            ", NEXTLINE);
                        PrintUglyText (L"  Applicable Disks *NOT* Found for GPTSync  ", NEXTLINE);
                        PrintUglyText (L"           Returning to Main Menu           ", NEXTLINE);
                        PrintUglyText (L"                                            ", NEXTLINE);
                        PrintUglyText (L"                                            ", NEXTLINE);
                        PauseSeconds (4);
                    }
                    #if REFIT_DEBUG > 0
                    MY_MUTELOGGER_OFF;
                    #endif
                }

                CheckError (ReturnStatus, MsgStrEx);
                MY_FREE_POOL(MsgStrEx);
            }

            // DA-TAG: Exclude TianoCore - START
            #ifndef __MAKEWITH_TIANO
            if (IsDriver && GlobalConfig.RansomDrives) {
                // The function below should have no effect on most systems, but
                // works around a bug with some firmware implementations that
                // prevent filesystem drivers from binding to partitions.
                ConnectFilesystemDriver (ChildImageHandle);
            }
            #endif
            // DA-TAG: Exclude TianoCore - END

            // Re-open file handles
            ReinitRefitLib();
        } while (0); // This 'loop' only runs once

        // DA-TAG: bailout_unload:
        // Unload the image ... we do not care if it works or not
        if (!IsDriver) {
            REFIT_CALL_1_WRAPPER(gBS->UnloadImage, ChildImageHandle);
        }
    } while (0); // This 'loop' only runs once

    // DA-TAG: bailout:
    MY_FREE_POOL(FullLoadOptions);

    if (!IsDriver) {
        FinishExternalScreen();
    }

    return ReturnStatus;
} // EFI_STATUS StartEFIImage()

// From gummiboot: Reboot the computer into its built-in user interface
EFI_STATUS RebootIntoFirmware (VOID) {
    EFI_STATUS  Status;
    CHAR16     *TmpStr;
    CHAR16     *MsgStr;
    UINT64     *ItemBuffer;
    UINT64      osind;


    osind = EFI_OS_INDICATIONS_BOOT_TO_FW_UI;

    Status = EfivarGetRaw (
        &GlobalGuid, L"OsIndications",
        (VOID **) &ItemBuffer, NULL
    );
    if (!EFI_ERROR(Status)) {
        osind |= *ItemBuffer;
    }
    MY_FREE_POOL(ItemBuffer);

    TmpStr = L"Reboot into Firmware";
    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"%s", TmpStr);
    #endif

    Status = EfivarSetRaw (
        &GlobalGuid, L"OsIndications",
        &osind, sizeof (UINT64), TRUE
    );
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        TmpStr = L"Aborted ... OsIndications *NOT* Found";
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", TmpStr);
        LOG_MSG("%s    ** %s", OffsetNext, TmpStr);
        LOG_MSG("\n\n");
        #endif

        return Status;
    }

    #if REFIT_DEBUG > 0
    OUT_TAG();
    #endif

    UninitRefitLib();
    REFIT_CALL_4_WRAPPER(
        gRT->ResetSystem, EfiResetCold,
        EFI_SUCCESS, 0, NULL
    );
    ReinitRefitLib();

    Status = EFI_LOAD_ERROR;
    MsgStr = PoolPrint (L"%s ... %r", TmpStr, Status);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("INFO: %s", MsgStr);
    LOG_MSG("\n\n");
    #endif

    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText (MsgStr, NEXTLINE);
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

    PauseForKey();

    MY_FREE_POOL(MsgStr);

    return Status;
} // EFI_STATUS RebootIntoFirmware()

// Reboot into a loader defined in the EFI's NVRAM
VOID RebootIntoLoader (
    LOADER_ENTRY *Entry
) {
    EFI_STATUS  Status;
    CHAR16     *TmpStr;
    CHAR16     *MsgStr;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif


    TmpStr = L"Reboot into nvRAM Boot Option";

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"%s", TmpStr);
    #endif

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"%s:- '%s' (Boot%04x)",
        TmpStr, Entry->Title, Entry->EfiBootNum
    );
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("%s    * %s", OffsetNext, MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    Status = EfivarSetRaw (
        &GlobalGuid, L"BootNext",
        &(Entry->EfiBootNum), sizeof (UINT16), TRUE
    );
    if (EFI_ERROR(Status)) {
        MsgStr = PoolPrint (
            L"'%r' While Running '%s'",
            Status, TmpStr
        );

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("\n\n");

        MY_MUTELOGGER_SET;
        #endif
        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
        PrintUglyText (MsgStr, NEXTLINE);
        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif

        PauseForKey();
        MY_FREE_POOL(MsgStr);
        return;
    }

    StoreLoaderName (Entry->me.Title);

    #if REFIT_DEBUG > 0
    OUT_TAG();
    #endif

    REFIT_CALL_4_WRAPPER(
        gRT->ResetSystem, EfiResetCold,
        EFI_SUCCESS, 0, NULL
    );

    EfivarSetRaw (
        &GlobalGuid, L"BootNext",
        NULL, 0, TRUE
    );

    Status = EFI_LOAD_ERROR;
    MsgStr = PoolPrint (L"%s ... %r", TmpStr, Status);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("INFO: %s", MsgStr);
    RET_TAG();

    MY_MUTELOGGER_SET;
    #endif
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText (MsgStr, NEXTLINE);
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    PauseForKey();

    MY_FREE_POOL(MsgStr);
} // VOID RebootIntoLoader()

// Directly launch an EFI boot loader (or similar program)
VOID StartLoader (
    IN LOADER_ENTRY *Entry,
    IN CHAR16       *SelectionName,
    IN BOOLEAN       TrustSynced
) {
    CHAR16 *LoaderPath;


    IsBoot        = TRUE;
    BootSelection = SelectionName;

    #if REFIT_DEBUG > 0
    if (TrustSynced) {
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    }
    #endif

    if (GlobalConfig.EnableAndLockVMX) {
        DoEnableAndLockVMX();
    }

    BeginExternalScreen (Entry->UseGraphicsMode, SelectionName);

    LoaderPath = Basename (Entry->LoaderPath);

    StartEFIImage (
        Entry->Volume,
        Entry->LoaderPath,
        Entry->LoadOptions,
        LoaderPath,
        Entry->OSType,
        !Entry->UseGraphicsMode,
        FALSE, NULL
    );

    MY_FREE_POOL(LoaderPath);
} // VOID StartLoader()

// Launch an EFI tool (a shell, SB management utility, etc.)
VOID StartTool (
    IN LOADER_ENTRY *Entry
) {
    EFI_STATUS  Status;
    CHAR16     *MsgStr;
    CHAR16     *LoaderPath;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif


    IsBoot     = FALSE;
    LoaderPath = Basename (Entry->LoaderPath);
    MsgStr     = PoolPrint (
        L"Start Child (Tool) Image Loader:- '%s'",
        Entry->LoaderPath
    );

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    #endif

    BeginExternalScreen (Entry->UseGraphicsMode, MsgStr);

    #if REFIT_DEBUG > 0
    LOG_MSG("%s    * %s", OffsetNext, MsgStr);
    LOG_MSG("\n");
    #endif

    if (FindSubStr (Entry->me.Title, L"APFS Instance")) {
        /* APFS Recovery Instance */
        Status = RecoveryBootAPFS (Entry);
        if (EFI_ERROR(Status)) {
            MY_FREE_POOL(MsgStr);
            MsgStr = PoolPrint (
                L"'%r' While Running '%s'",
                Status, Entry->me.Title
            );

            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("** WARN: %s", MsgStr);
            LOG_MSG("\n\n");

            MY_MUTELOGGER_SET;
            #endif
            REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
            PrintUglyText (MsgStr, NEXTLINE);
            REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_OFF;
            #endif

            PauseForKey();

            MY_FREE_POOL(MsgStr);
        }

        // Early Return
        return;
    }

    StartEFIImage (
        Entry->Volume,
        Entry->LoaderPath,
        Entry->LoadOptions,
        LoaderPath,
        Entry->OSType,
        !Entry->UseGraphicsMode,
        FALSE, NULL
    );

    MY_FREE_POOL(MsgStr);
    MY_FREE_POOL(LoaderPath);
} // VOID StartTool()
