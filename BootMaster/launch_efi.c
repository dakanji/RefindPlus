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
 * Modifications copyright (c) 2012-2021 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2023 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "screenmgt.h"
#include "lib.h"
#include "mok.h"
#include "icns.h"
#include "menu.h"
#include "apple.h"
#include "mystrings.h"
#include "driver_support.h"
#include "../include/refit_call_wrapper.h"
#include "launch_efi.h"
#include "scan.h"

//
// constants

#ifdef __MAKEWITH_GNUEFI
#ifndef EFI_SECURITY_VIOLATION
#define EFI_SECURITY_VIOLATION    EFIERR (26)
#endif
#endif

#if defined (EFIX64)
#define EFI_STUB_ARCH           0x8664
#elif defined (EFI32)
#define EFI_STUB_ARCH           0x014c
#elif defined (EFIAARCH64)
#define EFI_STUB_ARCH           0xaa64
#else
#endif

#define APPLE_FAT_BINARY        0x0ef1fab9 /* ID for Apple "fat" binary */

// Amount of a file to read to search for the EFI identifying signatures.
// Signatures as far in as 3680 (0xE60) have been found, so read a bit more.
#define EFI_HEADER_SIZE 3800

CHAR16         *BootSelection = NULL;
CHAR16         *ValidText     = L"Invalid Loader";

extern BOOLEAN  IsBoot;
extern EFI_GUID AppleVendorOsGuid;

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

    if (Name) {
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

    if (Verbose && secure_mode()) {
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
            L" * Use a MOK utility to register %s ('Enroll its Hash') without signing it",
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

static
BOOLEAN ConfirmReboot (
    CHAR16 *PromptUser
) {
    INTN               DefaultEntry;
    UINTN              MenuExit;
    BOOLEAN            RetVal;
    BOOLEAN            Confirmation;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenOption;
    REFIT_MENU_SCREEN *ConfirmRebootMenu;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Creating 'Confirm %s' Screen", PromptUser);
    #endif

    ConfirmRebootMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (!ConfirmRebootMenu) {
        // Early Return ... Fail
        return FALSE;
    }

    ConfirmRebootMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_FIRMWARE);
    ConfirmRebootMenu->Title      = PoolPrint (L"Confirm %s", PromptUser   );
    ConfirmRebootMenu->Hint1      = StrDuplicate (SELECT_OPTION_HINT       );
    ConfirmRebootMenu->Hint2      = StrDuplicate (RETURN_MAIN_SCREEN_HINT  );

    AddMenuInfoLine (ConfirmRebootMenu, PoolPrint (L"%s?", PromptUser), TRUE);

    RetVal = GetYesNoMenuEntry (&ConfirmRebootMenu);
    if (!RetVal) {
        FreeMenuScreen (&ConfirmRebootMenu);

        // Early Return
        return FALSE;
    }

    DefaultEntry = 1;
    Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
    MenuExit = RunGenericMenu (ConfirmRebootMenu, Style, &DefaultEntry, &ChosenOption);

    #if REFIT_DEBUG > 0
    ALT_LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) From RunGenericMenu Call on '%s' in 'ConfirmReboot'",
        MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
    );
    #endif

    if (MenuExit != MENU_EXIT_ENTER || ChosenOption->Tag != TAG_YES) {
        Confirmation = FALSE;
    }
    else {
        Confirmation = TRUE;
    }

    FreeMenuScreen (&ConfirmRebootMenu);

    return Confirmation;
} // static BOOLEAN ConfirmReboot()

// See http://www.thomas-krenn.com/en/wiki/Activating_the_Intel_VT_Virtualization_Feature
// for information on Intel VMX features
static
VOID DoEnableAndLockVMX(VOID) {
#if defined (EFIX64) | defined (EFI32)
    UINT32 msr;
    UINT32 low_bits;
    UINT32 high_bits;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"Attempting to Enable and Lock VMX");
    #endif

    // is VMX active ?
    msr = 0x3a;
    low_bits = high_bits = 0;
    __asm__ volatile ("rdmsr" : "=a" (low_bits), "=d" (high_bits) : "c" (msr));

    // enable and lock vmx if not locked
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
EFI_STATUS ApfsRecoveryBoot (
    IN LOADER_ENTRY *Entry
) {
    if (!SingleAPFS) {
        // Early Return
        return EFI_INVALID_PARAMETER;
    }


#if 0
// DA-TAG: Force Load Error until finalised


    EFI_STATUS  Status;
    CHAR16     *InitNVRAM;
    CHAR16     *NameNVRAM;
    CHAR8      *DataNVRAM;
    UINTN       Size;

    // Set Relevant NVRAM Variable
    InitNVRAM = L"RecoveryModeDisk";
    NameNVRAM = L"internet-recovery-mode";
    DataNVRAM = NULL;
    UnicodeStrToAsciiStr (InitNVRAM, DataNVRAM);
    Status = EfivarSetRaw (
        &AppleVendorOsGuid, NameNVRAM,
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
        &AppleVendorOsGuid, NameNVRAM,
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
        Basename (Entry->EfiLoaderPath),
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

#endif

    return EFI_LOAD_ERROR;
} // static EFI_STATUS ApfsRecoveryBoot()

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
    BOOLEAN          AppleFatBinary;
    BOOLEAN          ApplePlainBinary;
    UINTN            SignaturePosition;
    UINTN            Size;
    CHAR8           *Header;
    EFI_FILE_HANDLE  FileHandle;

    #if REFIT_DEBUG > 0
    CHAR16          *AbortReason;
    #endif

    Header = AllocatePool (EFI_HEADER_SIZE);
    if (!Header) {
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
        IsValid = AppleFatBinary = ApplePlainBinary = FALSE;

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

        Size = EFI_HEADER_SIZE;
        Status = REFIT_CALL_3_WRAPPER(
            FileHandle->Read, FileHandle,
            &Size, Header
        );
        REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);
        if (EFI_ERROR(Status)) {
            #if REFIT_DEBUG > 0
            AbortReason = L":- 'File is *NOT* Readable'";
            #endif

            //LoaderType = LOADER_TYPE_INVALID;

            // Early Return
            break;
        }

        // Search for indications that this is a gzipped file.
        // NB: This is currently only used for logging in RefindPlus
        // and that all loaders at this point are considered invalid.
        // GZipped loaders are mainly ARM related and focus is on X86_64
        if (
            Header[0] == (CHAR8) 0x1F   &&
            Header[1] == (CHAR8) 0x8B   &&
            GlobalConfig.GzippedLoaders
        ) {
            #if REFIT_DEBUG > 0
            AbortReason = L":- 'GZipped Binary'";
            #endif

            //LoaderType = LOADER_TYPE_GZIP;

            // Early Return
            break;
        }

        // Search for standard PE32+ signature
        SignaturePosition = *((UINT32 *) &Header[0x3c]);
        IsValid = (
            Size == EFI_HEADER_SIZE                                     &&
            SignaturePosition < (EFI_HEADER_SIZE - 8)                   &&
            Header[0]                                    == 'M'         &&
            Header[1]                                    == 'Z'         &&
            Header[SignaturePosition]                    == 'P'         &&
            Header[SignaturePosition + 1]                == 'E'         &&
            Header[SignaturePosition + 2]                ==  0          &&
            Header[SignaturePosition + 3]                ==  0          &&
            *((UINT16 *) &Header[SignaturePosition + 4]) ==  EFI_STUB_ARCH
        );
        if (IsValid) {
            //LoaderType = LOADER_TYPE_EFI;

            // Early Return
            break;
        }

        // Search for Apple's 'Fat' Binary signature
        IsValid = AppleFatBinary = (
            AppleFirmware &&
            *((UINT32 *) &Header[0]) == APPLE_FAT_BINARY
        );
        if (IsValid) {
            //LoaderType = LOADER_TYPE_EFI;

            // Early Return
            break;
        }

        // Allow plain binaries on Apple Firmware
        IsValid = ApplePlainBinary = AppleFirmware;
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
        ? L"EFI File is *NOT* Valid"
        : (AppleFatBinary)
            ? L"Apple 'Fat' Binary is *ASSUMED* to be Valid on Apple Firmware"
            : (ApplePlainBinary)
                ? L"Plain Binary is *ASSUMED* to be Valid on Apple Firmware"
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
    EFI_HANDLE                           ChildImageHandle2;
    EFI_DEVICE_PATH_PROTOCOL            *DevicePath;
    EFI_LOADED_IMAGE_PROTOCOL           *ChildLoadedImage;

    #if REFIT_DEBUG > 0
    CHAR16  *ConstMsgStr;

    BOOLEAN  CheckMute = FALSE;
    #endif

    if (!Volume) {
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

        // DA-TAG: The last space is also added by the EFI shell and is
        //         significant when passing options to Apple's boot.efi
        if (OSType == 'M') MergeStrings (&FullLoadOptions, L" ", 0);
    }

    MsgStr = PoolPrint (
        L"Loading '%s' ... Load Options:- '%s'",
        ImageTitle, (FullLoadOptions) ? FullLoadOptions : L"NULL"
    );

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    #endif

    if (Verbose) {
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif
        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
        PrintUglyText (MsgStr, NEXTLINE);
        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif
    }

    MY_FREE_POOL(MsgStr);

    do {
        ReturnStatus = Status = EFI_LOAD_ERROR;  // in case the list is empty
        // Some EFIs crash if attempting to load drivers for an invalid architecture, so
        // protect for this condition; but sometimes Volume comes back NULL, so provide
        // an exception. (TODO: Handle this special condition better.)
        LoaderValid = IsValidLoader (Volume->RootDir, Filename);
        if (!LoaderValid) {
            #if REFIT_DEBUG > 0
            MsgStr = StrDuplicate (L"ERROR: Invalid Binary!!");
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
            LOG_MSG("* %s", MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
            #endif

            MsgStr = PoolPrint (
                L"When Loading %s ... %s",
                ImageTitle,
                ValidText
            );
            ValidText = L"Invalid Binary";
            CheckError (Status, MsgStr);
            MY_FREE_POOL(MsgStr);

            // Bail Out
            break;
        }

        // Store loader name if booting and set to do so
        if (BootSelection != NULL) {
            if (IsBoot) {
                StoreLoaderName (BootSelection);
            }
            BootSelection = NULL;
        }

        DevicePath = FileDevicePath (Volume->DeviceHandle, Filename);

        // Stall to avoid unwanted flash of text when starting loaders
        // Stall works best in smaller increments as per Specs
        if (!IsDriver && (!AllowGraphicsMode || Verbose)) {
            // DA-TAG: 100 Loops = 1 Sec
            RefitStall (150);
        }

        ChildImageHandle = NULL;
        // DA-TAG: Investigate This
        //         Commented-out lines below could be more efficient if file were read ahead of
        //         time and passed as a pre-loaded image to LoadImage(), but it does not work on my
        //         32-bit Mac Mini or my 64-bit Intel box when launching a Linux kernel.
        //         The kernel returns a "Failed to handle fs_proto" error message.
        //
        //         Track down the cause of this error and fix it, if possible.
        /*
        Status = REFIT_CALL_6_WRAPPER(
            gBS->LoadImage, FALSE,
            SelfImageHandle, DevicePath,
            ImageData, ImageSize, &ChildImageHandle
        );
        */
        Status = REFIT_CALL_6_WRAPPER(
            gBS->LoadImage, FALSE,
            SelfImageHandle, DevicePath,
            NULL, 0, &ChildImageHandle
        );
        MY_FREE_POOL(DevicePath);
        ReturnStatus = Status;

        if (secure_mode() && ShimLoaded()) {
            // Load ourself into memory. This is a trick to work around a bug in Shim 0.8,
            // which ties itself into the gBS->LoadImage() and gBS->StartImage() functions and
            // then unregisters itself from the UEFI system table when its replacement
            // StartImage() function is called *IF* the previous LoadImage() was for the same
            // program. The result is that RefindPlus can validate only the first program it
            // launches (often a filesystem driver). Loading a second program (RefindPlus itself,
            // here, to keep it smaller than a kernel) works around this problem. See the
            // replacements.c file in Shim, and especially its start_image() function, for
            // the source of the problem.
            //
            // NOTE: This does not check the return status or handle errors. It could
            // conceivably do weird things if, say, RefindPlus were on a USB drive that the
            // user pulls before launching a program.
            #if REFIT_DEBUG > 0
            MsgStr = StrDuplicate (L"Employing Shim 'LoadImage' Hack");
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
            LOG_MSG("INFO: %s", MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
            #endif

            // Ignore return status here
            ChildImageHandle2 = NULL;
            REFIT_CALL_6_WRAPPER(
                gBS->LoadImage, FALSE,
                SelfImageHandle, GlobalConfig.SelfDevicePath,
                NULL, 0, &ChildImageHandle2
            );
        }

        if ((Status == EFI_ACCESS_DENIED) || (Status == EFI_SECURITY_VIOLATION)) {
            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (
                L"'%r' Returned by Secure Boot While Loading %s!!",
                Status, ImageTitle
            );
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"ERROR: %s", MsgStr);
            LOG_MSG("* ERROR: %s", MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
            #endif

            WarnSecureBootError (ImageTitle, Verbose);

            // Bail Out
            break;
        }

        do {
            ChildLoadedImage = NULL;
            ReturnStatus = Status = REFIT_CALL_3_WRAPPER(
                gBS->HandleProtocol, ChildImageHandle,
                &LoadedImageProtocol, (VOID **) &ChildLoadedImage
            );
            if (EFI_ERROR(Status)) {
                CheckError (Status, L"while Getting LoadedImageProtocol Handle");

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
            if ((GlobalConfig.WriteSystemdVars) &&
                ((OSType == 'L') || (OSType == 'E') || (OSType == 'G'))
            ) {
                // Tell systemd what ESP RefindPlus used
                EspGUID = GuidAsString (&(SelfVolume->PartGuid));

                #if REFIT_DEBUG > 0
                MsgStr = PoolPrint (
                    L"Systemd LoaderDevicePartUUID:- '%s'",
                    EspGUID
                );
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                LOG_MSG("INFO: %s", MsgStr);
                LOG_MSG("\n\n");
                MY_FREE_POOL(MsgStr);
                #endif

                Status = EfivarSetRaw (
                    &SystemdGuid, L"LoaderDevicePartUUID",
                    EspGUID, StrLen (EspGUID) * 2 + 2, TRUE
                );
                #if REFIT_DEBUG > 0
                if (EFI_ERROR(Status)) {
                    MsgStr = PoolPrint (
                        L"'%r' When Trying to Set LoaderDevicePartUUID UEFI Variable!!",
                        Status
                    );
                    ALT_LOG(1, LOG_STAR_SEPARATOR, L"ERROR:- '%s'", MsgStr);
                    LOG_MSG("* ERROR: %s", MsgStr);
                    LOG_MSG("\n\n");
                    MY_FREE_POOL(MsgStr);
                }
                #endif

                MY_FREE_POOL(EspGUID);
            } // if write systemd UEFI variables

            // DA-TAG: SyncAPFS items are typically no longer required if not loading drivers
            //         "Typically" as users may place UEFI Shell etc in the first row (loaders)
            //         These may return to the RefindPlus screen but any issues will be trivial
            if (!IsDriver) FreeSyncVolumes();

            // Close open file handles
            UninitRefitLib();

            #if REFIT_DEBUG > 0
            ConstMsgStr = (!IsDriver) ? L"Running Child Image" : L"Loading UEFI Driver";
            if (!IsDriver) {
                ALT_LOG(1, LOG_LINE_NORMAL,
                    L"%s via Loader File:- '%s'",
                    ConstMsgStr, ImageTitle
                );
                OUT_TAG();
            }
            #endif

            Status = REFIT_CALL_3_WRAPPER(
                gBS->StartImage, ChildImageHandle,
                NULL, NULL
            );

            // Control returns here if the child image calls 'Exit()'
            ReturnStatus = Status;

            // DA-TAG: Used in 'ScanDriverDir()'
            NewImageHandle = ChildImageHandle;

            #if REFIT_DEBUG > 0
            MsgStrEx = PoolPrint (L"'%r' When %s", ReturnStatus, ConstMsgStr);
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

                if (!IsDriver                       &&
                    ReturnStatus == EFI_NOT_FOUND   &&
                    FindSubStr (ImageTitle, L"gptsync")
                ) {
                    #if REFIT_DEBUG > 0
                    MY_MUTELOGGER_SET;
                    #endif
                    PauseSeconds (4);
                    SwitchToText (FALSE);
                    PrintUglyText (L"                                ", NEXTLINE);
                    PrintUglyText (L"                                ", NEXTLINE);
                    PrintUglyText (L"  Applicable Disks *NOT* Found  ", NEXTLINE);
                    PrintUglyText (L"     Returning to Main Menu     ", NEXTLINE);
                    PrintUglyText (L"                                ", NEXTLINE);
                    PrintUglyText (L"                                ", NEXTLINE);
                    PauseSeconds (4);
                    #if REFIT_DEBUG > 0
                    MY_MUTELOGGER_OFF;
                    #endif
                }
                else {
                    MsgStrTmp = L"Returned From Child Image";
                    if (!IsDriver) {
                        MsgStrEx = StrDuplicate (MsgStrTmp);
                    }
                    else {
                        MsgStrEx = PoolPrint (L"%s:- '%s'", MsgStrTmp, ImageTitle);
                    }
                    CheckError (ReturnStatus, MsgStrEx);
                    MY_FREE_POOL(MsgStrEx);
                }
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
        if (!IsDriver) REFIT_CALL_1_WRAPPER(gBS->UnloadImage, ChildImageHandle);
    } while (0); // This 'loop' only runs once

    // DA-TAG: bailout:
    MY_FREE_POOL(FullLoadOptions);

    if (!IsDriver) FinishExternalScreen();

    return ReturnStatus;
} // EFI_STATUS StartEFIImage()

// From gummiboot: Reboot the computer into its built-in user interface
EFI_STATUS RebootIntoFirmware (VOID) {
    EFI_STATUS  Status;
    CHAR16     *TmpStr;
    CHAR16     *MsgStr;
    UINT64     *ItemBuffer;
    UINT64      osind;
    BOOLEAN     ConfirmAction;

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
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", TmpStr);
    #endif

    ConfirmAction = ConfirmReboot(TmpStr);
    if (!ConfirmAction) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Aborted by User");
        LOG_MSG("%s    ** Aborted by User", OffsetNext);
        LOG_MSG("\n\n");
        #endif

        return EFI_NOT_STARTED;
    }

    Status = EfivarSetRaw (
        &GlobalGuid, L"OsIndications",
        &osind, sizeof (UINT64), TRUE
    );
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Aborted ... OsIndications Not Found");
        LOG_MSG("%s    ** Aborted ... OsIndications Not Found", OffsetNext);
        LOG_MSG("\n\n");
        #endif

        return Status;
    }

    UninitRefitLib();

    #if REFIT_DEBUG > 0
    OUT_TAG();
    #endif

    REFIT_CALL_4_WRAPPER(
        gRT->ResetSystem, EfiResetCold,
        EFI_SUCCESS, 0, NULL
    );

    Status = EFI_LOAD_ERROR;
    MsgStr = PoolPrint (L"%s ... %r", TmpStr, Status);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("INFO: %s", MsgStr);
    LOG_MSG("\n\n");
    #endif

    ReinitRefitLib();

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
    BOOLEAN     ConfirmAction;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    TmpStr = L"Reboot into NVRAM Boot Option";

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", TmpStr);
    #endif

    ConfirmAction = ConfirmReboot(TmpStr);
    if (!ConfirmAction) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"Aborted by User");
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("%s    ** %s", OffsetNext, MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        return;
    }

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
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
        LOG_MSG("\n\n");
        #endif

        #if REFIT_DEBUG > 0
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

    StoreLoaderName(Entry->me.Title);

    #if REFIT_DEBUG > 0
    OUT_TAG();
    #endif

    REFIT_CALL_4_WRAPPER(
        gRT->ResetSystem, EfiResetCold,
        EFI_SUCCESS, 0, NULL
    );

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
} // VOID RebootIntoLoader()

// Directly launch an EFI boot loader (or similar program)
VOID StartLoader (
    LOADER_ENTRY *Entry,
    CHAR16       *SelectionName
) {
    CHAR16 *MsgStr;
    CHAR16 *LoaderPath;

    IsBoot        = TRUE;
    BootSelection = SelectionName;
    LoaderPath    = Basename (Entry->LoaderPath);
    MsgStr        = PoolPrint (L"Starting:- '%s'", SelectionName);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    #endif

    if (GlobalConfig.EnableAndLockVMX) {
        DoEnableAndLockVMX();
    }

    BeginExternalScreen (Entry->UseGraphicsMode, SelectionName);
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
} // VOID StartLoader()

// Launch an EFI tool (a shell, SB management utility, etc.)
VOID StartTool (
    IN LOADER_ENTRY *Entry
) {
    CHAR16  *MsgStr;
    CHAR16  *LoaderPath;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    IsBoot     = FALSE;
    LoaderPath = Basename (Entry->LoaderPath);
    MsgStr     = PoolPrint (L"Launching Child (Tool) Image:- '%s'", Entry->me.Title);

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
        EFI_STATUS Status = ApfsRecoveryBoot (Entry);
        if (EFI_ERROR(Status)) {
            MY_FREE_POOL(MsgStr);
            MsgStr = PoolPrint (
                L"'%r' While Running '%s'",
                Status, Entry->me.Title
            );

            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
            LOG_MSG("** WARN: %s", MsgStr);
            LOG_MSG("\n\n");
            #endif

            #if REFIT_DEBUG > 0
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
        TRUE, FALSE, NULL
    );

    MY_FREE_POOL(MsgStr);
    MY_FREE_POOL(LoaderPath);
} // VOID StartTool()
