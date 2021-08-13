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
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "screenmgt.h"
#include "lib.h"
#include "mok.h"
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

#define FAT_ARCH                0x0ef1fab9 /* ID for Apple "fat" binary */

CHAR16 *BootSelection = NULL;

static
VOID WarnSecureBootError(
    CHAR16  *Name,
    BOOLEAN  Verbose
) {
    CHAR16 *MsgStrA = NULL;
    CHAR16 *MsgStrB = NULL;
    CHAR16 *MsgStrC = NULL;
    CHAR16 *MsgStrD = NULL;
    CHAR16 *MsgStrE = NULL;

    if (Name == NULL) {
        Name = L"the Loader";
    }

    SwitchToText (FALSE);

    MsgStrA = PoolPrint (L"Secure Boot Validation Failure While Loading %s!!", Name);

    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText (MsgStrA, NEXTLINE);
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

    if (Verbose && secure_mode()) {
        MsgStrB = PoolPrint (
            L"This computer is configured with Secure Boot active but '%s' has failed validation.",
            Name
        );
        PrintUglyText (MsgStrB, NEXTLINE);
        PrintUglyText (L"You can:", NEXTLINE);
        PrintUglyText (L" * Launch another boot loader", NEXTLINE);
        PrintUglyText (L" * Disable Secure Boot in your firmware", NEXTLINE);
        MsgStrC = PoolPrint (
            L" * Sign %s with a machine owner key (MOK)",
            Name
        );
        PrintUglyText (MsgStrC, NEXTLINE);
        MsgStrD = PoolPrint (
            L" * Use a MOK utility to add a MOK with which '%s' has already been signed.",
            Name
        );
        PrintUglyText (MsgStrD, NEXTLINE);
        MsgStrE = PoolPrint (
            L" * Use a MOK utility to register '%s' ('Enroll its Hash') without signing it",
            Name
        );
        PrintUglyText (MsgStrE, NEXTLINE);
        PrintUglyText (L"See http://www.rodsbooks.com/refind/secureboot.html for more information", NEXTLINE);

    } // if
    PauseForKey();
    SwitchToGraphics();

    MyFreePool (&MsgStrA);
    MyFreePool (&MsgStrB);
    MyFreePool (&MsgStrC);
    MyFreePool (&MsgStrD);
    MyFreePool (&MsgStrE);
} // VOID WarnSecureBootError()

// Returns TRUE if this file is a valid EFI loader file, and is proper ARCH
BOOLEAN IsValidLoader (
    EFI_FILE *RootDir,
    CHAR16   *FileName
) {
    BOOLEAN IsValid;

    if (!FileExists (RootDir, FileName)) {
        // Early return if file does not exist
        return FALSE;
    }


#if defined (EFIX64) | defined (EFI32) | defined (EFIAARCH64)
    EFI_STATUS      Status;
    EFI_FILE_HANDLE FileHandle;
    CHAR8           Header[512];
    UINTN           Size = sizeof (Header);

    if ((RootDir == NULL) || (FileName == NULL)) {
        // Assume valid here, because Macs produce NULL RootDir (& maybe FileName)
        // when launching from a Firewire drive. This should be handled better, but
        // fix would have to be in StartEFIImage() and/or in FindVolumeAndFilename().
        #if REFIT_DEBUG > 0
        LOG(4, LOG_THREE_STAR_MID,
            L"EFI File is *ASSUMED* to be Valid:- '%s'",
            FileName
        );
        #endif

        return TRUE;
    } // if

    Status = REFIT_CALL_5_WRAPPER(
        RootDir->Open, RootDir,
        &FileHandle, FileName,
        EFI_FILE_MODE_READ, 0
    );

    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        LOG(4, LOG_THREE_STAR_MID,
            L"EFI File is *NOT* Valid:- '%s'",
            FileName
        );
        #endif

        return FALSE;
    }

    Status = REFIT_CALL_3_WRAPPER(FileHandle->Read, FileHandle, &Size, Header);
    REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);

    IsValid = !EFI_ERROR(Status) &&
              Size == sizeof (Header) &&
              ((Header[0] == 'M' && Header[1] == 'Z' &&
               (Size = *(UINT32 *) &Header[0x3c]) < 0x180 &&
               Header[Size] == 'P' && Header[Size+1] == 'E' &&
               Header[Size+2] == 0 && Header[Size+3] == 0 &&
               *(UINT16 *) &Header[Size+4] == EFI_STUB_ARCH) ||
              (*(UINT32 *) &Header == FAT_ARCH));

    #if REFIT_DEBUG > 0
    LOG(4, LOG_THREE_STAR_MID,
        L"EFI File is %s:- '%s'",
        IsValid ? L"Valid" : L"*NOT* Valid",
        FileName
    );
    #endif
#else
    IsValid = TRUE;
#endif

    return IsValid;
} // BOOLEAN IsValidLoader()

// Launch an EFI binary.
EFI_STATUS StartEFIImage (
    IN REFIT_VOLUME  *Volume,
    IN CHAR16        *Filename,
    IN CHAR16        *LoadOptions,
    IN CHAR16        *ImageTitle,
    IN CHAR8          OSType,
    IN BOOLEAN        Verbose,
    IN BOOLEAN        IsDriver
) {
    EFI_STATUS         Status;
    EFI_STATUS         ReturnStatus;
    EFI_HANDLE         ChildImageHandle  = NULL;
    EFI_HANDLE         ChildImageHandle2 = NULL;
    EFI_DEVICE_PATH   *DevicePath        = NULL;
    EFI_LOADED_IMAGE  *ChildLoadedImage  = NULL;
    CHAR16            *FullLoadOptions   = NULL;
    CHAR16            *EspGUID           = NULL;
    CHAR16            *MsgStr            = NULL;
    EFI_GUID           SystemdGuid       = SYSTEMD_GUID_VALUE;

    if (!Volume) {
        ReturnStatus = EFI_INVALID_PARAMETER;

        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (
            L"'%r' While Starting EFI Image!!",
            ReturnStatus
        );
        LOG(1, LOG_STAR_SEPARATOR, L"ERROR: %s", MsgStr);
        MsgLog ("* ERROR: %s\n\n", MsgStr);
        MyFreePool (&MsgStr);
        #endif

        return ReturnStatus;
    }

    // set load options
    if (LoadOptions != NULL) {
        FullLoadOptions = StrDuplicate (LoadOptions);
        if (OSType == 'M') {
            MergeStrings (&FullLoadOptions, L" ", 0);
            // NOTE: That last space is also added by the EFI shell and seems to be significant
            // when passing options to Apple's boot.efi...
        }
    }

    MsgStr = PoolPrint (
        L"Starting '%s' ... Load Options:- '%s'",
        ImageTitle, FullLoadOptions ? FullLoadOptions : L""
    );

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL, MsgStr);
    #endif

    if (Verbose) {
        Print(L"%s\n", MsgStr);
    }

    MyFreePool (&MsgStr);

    ReturnStatus = Status = EFI_LOAD_ERROR;  // in case the list is empty
    // Some EFIs crash if attempting to load driver for invalid architecture, so
    // protect for this condition; but sometimes Volume comes back NULL, so provide
    // an exception. (TODO: Handle this special condition better.)
    if (IsValidLoader (Volume->RootDir, Filename)) {
        // Store loader name if booting and set to do so
        if (IsBoot && BootSelection) {
            StoreLoaderName (BootSelection);
        }
        BootSelection = NULL;

        DevicePath = FileDevicePath (Volume->DeviceHandle, Filename);
        // NOTE: Commented-out line below could be more efficient if file were read ahead of
        // time and passed as a pre-loaded image to LoadImage(), but it doesn't work on my
        // 32-bit Mac Mini or my 64-bit Intel box when launching a Linux kernel; the
        // kernel returns a "Failed to handle fs_proto" error message.
        // TODO: Track down the cause of this error and fix it, if possible.
        // Status = REFIT_CALL_6_WRAPPER(gBS->LoadImage, FALSE, SelfImageHandle, DevicePath,
        //                              ImageData, ImageSize, &ChildImageHandle);
        // ReturnStatus = Status;
        Status = REFIT_CALL_6_WRAPPER(
            gBS->LoadImage, FALSE,
            SelfImageHandle, DevicePath,
            NULL, 0,
            &ChildImageHandle
        );
        MyFreePool (&DevicePath);
        ReturnStatus = Status;

        if (secure_mode() && ShimLoaded()) {
            // Load ourself into memory. This is a trick to work around a bug in Shim 0.8,
            // which ties itself into the gBS->LoadImage() and gBS->StartImage() functions and
            // then unregisters itself from the EFI system table when its replacement
            // StartImage() function is called *IF* the previous LoadImage() was for the same
            // program. The result is that RefindPlus can validate only the first program it
            // launches (often a filesystem driver). Loading a second program (RefindPlus itself,
            // here, to keep it smaller than a kernel) works around this problem. See the
            // replacements.c file in Shim, and especially its start_image() function, for
            // the source of the problem.
            // NOTE: This doesn't check the return status or handle errors. It could
            // conceivably do weird things if, say, RefindPlus were on a USB drive that the
            // user pulls before launching a program.
            #if REFIT_DEBUG > 0
            MsgStr = StrDuplicate (L"Employing Shim 'LoadImage' Hack");
            LOG(4, LOG_LINE_NORMAL, L"%s", MsgStr);
            MsgLog ("INFO: %s\n\n", MsgStr);
            MyFreePool (&MsgStr);
            #endif

            REFIT_CALL_6_WRAPPER(
                gBS->LoadImage,
                FALSE,
                SelfImageHandle,
                GlobalConfig.SelfDevicePath,
                NULL,
                0,
                &ChildImageHandle2
            );
        }
    }
    else {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"Invalid Loader!!");
        LOG(1, LOG_STAR_SEPARATOR, L"ERROR: %s", MsgStr);
        MsgLog ("* ERROR: %s\n\n", MsgStr);
        MyFreePool (&MsgStr);
        #endif
    }

    if ((Status == EFI_ACCESS_DENIED) || (Status == EFI_SECURITY_VIOLATION)) {
        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (
            L"'%r' Returned by Secure Boot While Loading %s!!",
            Status, ImageTitle
        );
        LOG(1, LOG_STAR_SEPARATOR, L"ERROR: %s", MsgStr);
        MsgLog ("* ERROR: %s\n\n", MsgStr);
        MyFreePool (&MsgStr);
        #endif

        WarnSecureBootError (ImageTitle, Verbose);
        goto bailout;
    }

    MsgStr = PoolPrint (L"While Loading %s", ImageTitle);
    if (CheckError (Status, MsgStr)) {
        MyFreePool (&MsgStr);
        goto bailout;
    }
    MyFreePool (&MsgStr);

    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol,
        ChildImageHandle,
        &LoadedImageProtocol,
        (VOID **) &ChildLoadedImage
    );
    ReturnStatus = Status;

    if (CheckError (Status, L"while Getting LoadedImageProtocol Handle")) {
        goto bailout_unload;
    }

    ChildLoadedImage->LoadOptions     = (VOID *) FullLoadOptions;
    ChildLoadedImage->LoadOptionsSize = FullLoadOptions
        ? ((UINT32) StrLen (FullLoadOptions) + 1) * sizeof (CHAR16)
        : 0;

    // turn control over to the image
    // TODO: (optionally) re-enable the EFI watchdog timer!
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
        LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        MsgLog ("INFO: %s\n\n", MsgStr);
        MyFreePool (&MsgStr);
        #endif

        Status = EfivarSetRaw (
            &SystemdGuid,
            L"LoaderDevicePartUUID",
            EspGUID,
            StrLen (EspGUID) * 2 + 2,
            TRUE
        );

        #if REFIT_DEBUG > 0
        if (EFI_ERROR(Status)) {
            MsgStr = PoolPrint (
                L"'%r' When Trying to Set LoaderDevicePartUUID UEFI Variable!!",
                Status
            );
            LOG(1, LOG_STAR_SEPARATOR, L"ERROR:- %s", MsgStr);
            MsgLog ("* ERROR: %s\n\n", MsgStr);
            MyFreePool (&MsgStr);
        }
        #endif

        MyFreePool (&EspGUID);
    } // if write systemd UEFI variables

    // close open file handles
    UninitRefitLib();

    #if REFIT_DEBUG > 0
    if (!IsDriver) {
        MsgStr = StrDuplicate (L"Loading Child Image");
    }
    else {
        // DA-TAG: This is used before 'MsgStr' is ultimately freed
        MsgStr = StrDuplicate (L"Loading EFI Driver");
    }

    if (!IsDriver) {
        // Do not log this for drivers
        LOG(3, LOG_LINE_NORMAL, L"%s:- '%s'", MsgStr , ImageTitle);
    }
    // DA-TAG: MsgStr is recycled and freed later
    #endif

    Status = REFIT_CALL_3_WRAPPER(
        gBS->StartImage, ChildImageHandle,
        NULL, NULL
    );
    ReturnStatus = Status;

    // control returns here when the child image calls Exit()
    #if REFIT_DEBUG > 0
    LOG(1, LOG_THREE_STAR_MID, L"'%r' While %s:- '%s'", ReturnStatus, MsgStr, ImageTitle);

    // DA-TAG: MsgStr from earlier is freed here
    MyFreePool (&MsgStr);
    #endif

    MsgStr = PoolPrint (L"Returned from %s", ImageTitle);

    #if REFIT_DEBUG > 0
    if (!IsDriver) {
        LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);
    }
    #endif

    CheckError (ReturnStatus, MsgStr);
    MyFreePool (&MsgStr);

    if (IsDriver) {
        // Below should have no effect on most systems, but works
        // around bug with some EFIs that prevents filesystem drivers
        // from binding to partitions.
        ConnectFilesystemDriver (ChildImageHandle);
    }

    // re-open file handles
    ReinitRefitLib();

bailout_unload:
    // unload the image, we do not care if it works or not
    if (!IsDriver) {
        REFIT_CALL_1_WRAPPER(gBS->UnloadImage, ChildImageHandle);
    }

bailout:
    MyFreePool (&FullLoadOptions);
    if (!IsDriver) {
        FinishExternalScreen();
    }

    return ReturnStatus;
} // EFI_STATUS StartEFIImage()

// From gummiboot: Reboot the computer into its built-in user interface
EFI_STATUS RebootIntoFirmware (VOID) {
    UINT64     *ItemBuffer;
    UINT64      osind;
    EFI_STATUS  err;

    osind = EFI_OS_INDICATIONS_BOOT_TO_FW_UI;

    err = EfivarGetRaw (
        &GlobalGuid,
        L"OsIndications",
        (VOID **) &ItemBuffer,
        NULL
    );

    if (err == EFI_SUCCESS) {
        osind |= *ItemBuffer;
    }
    MyFreePool (&ItemBuffer);

    err = EfivarSetRaw (
        &GlobalGuid,
        L"OsIndications",
        &osind,
        sizeof (UINT64),
        TRUE
    );

    #if REFIT_DEBUG > 0
    MsgLog ("INFO: Reboot into Firmware ... %r\n\n", err);
    #endif

    if (err != EFI_SUCCESS) {
        return err;
    }

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_SEPARATOR, L"Rebooting into the Computer's Firmware");
    #endif

    UninitRefitLib();

    REFIT_CALL_4_WRAPPER(
        gRT->ResetSystem,
        EfiResetCold,
        EFI_SUCCESS,
        0,
        NULL
    );

    ReinitRefitLib();

    CHAR16 *MsgStr = PoolPrint (L"Error Calling ResetSystem ... %r", err);

    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText (MsgStr, NEXTLINE);
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

    #if REFIT_DEBUG > 0
    MsgLog ("** WARN: %s\n\n", MsgStr);
    #endif

    PauseForKey();

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_NORMAL, MsgStr, err);
    #endif

    MyFreePool (&MsgStr);

    return err;
} // EFI_STATUS RebootIntoFirmware()

// Reboot into a loader defined in the EFI's NVRAM
VOID RebootIntoLoader (
    LOADER_ENTRY *Entry
) {
    EFI_STATUS  Status;
    CHAR16     *MsgStr = NULL;

    IsBoot = TRUE;

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"Reboot into NVRAM Boot Option:- '%s' (Boot%04x)",
        Entry->Title, Entry->EfiBootNum
    );
    LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
    MsgLog ("INFO: %s\n\n", MsgStr);
    MyFreePool (&MsgStr);
    #endif

    Status = EfivarSetRaw (
        &GlobalGuid,
        L"BootNext",
        &(Entry->EfiBootNum),
        sizeof (UINT16),
        TRUE
    );

    if (EFI_ERROR(Status)) {
        MsgStr = PoolPrint (
            L"'%r' while Rebooting into NVRAM Boot Option",
            Status
        );

        #if REFIT_DEBUG > 0
        LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
        #endif

        Print(L"%s\n", MsgStr);
        PauseForKey();

        MyFreePool (&MsgStr);
        return;
    }

    StoreLoaderName(Entry->me.Title);

    REFIT_CALL_4_WRAPPER(
        gRT->ResetSystem, EfiResetCold,
        EFI_SUCCESS, 0, NULL
    );

    MsgStr = PoolPrint (L"Call ResetSystem ... %r", Status);

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    #endif

    Print(MsgStr);
    PauseForKey();

    MyFreePool (&MsgStr);
} // RebootIntoLoader()

//
// EFI OS loader functions
//

// See http://www.thomas-krenn.com/en/wiki/Activating_the_Intel_VT_Virtualization_Feature
// for information on Intel VMX features
static
VOID DoEnableAndLockVMX(VOID) {
#if defined (EFIX64) | defined (EFI32)
    UINT32 msr       = 0x3a;
    UINT32 low_bits  = 0;
    UINT32 high_bits = 0;

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_NORMAL, L"Attempting to Enable and Lock VMX");
    #endif

    // is VMX active ?
    __asm__ volatile ("rdmsr" : "=a" (low_bits), "=d" (high_bits) : "c" (msr));

    // enable and lock vmx if not locked
    if ((low_bits & 1) == 0) {
        high_bits = 0;
        low_bits = 0x05;
        msr = 0x3a;
        __asm__ volatile ("wrmsr" : : "c" (msr), "a" (low_bits), "d" (high_bits));
    }
#endif
} // VOID DoEnableAndLockVMX()

// Directly launch an EFI boot loader (or similar program)
VOID StartLoader (
    LOADER_ENTRY *Entry,
    CHAR16       *SelectionName
) {
    CHAR16 *LoaderPath;

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_NORMAL, L"Launching Loader:- '%s'", SelectionName);
    #endif

    IsBoot = TRUE;

    if (GlobalConfig.EnableAndLockVMX) {
        DoEnableAndLockVMX();
    }

    BootSelection = SelectionName;
    LoaderPath    = Basename (Entry->LoaderPath);
    BeginExternalScreen (Entry->UseGraphicsMode, L"Booting into OS Loader");
    StartEFIImage (
        Entry->Volume,
        Entry->LoaderPath,
        Entry->LoadOptions,
        LoaderPath,
        Entry->OSType,
        !Entry->UseGraphicsMode,
        FALSE
    );

    MyFreePool (&LoaderPath);
} // VOID StartLoader()

// Launch an EFI tool (a shell, SB management utility, etc.)
VOID StartTool (
    IN LOADER_ENTRY *Entry
) {
    CHAR16 *LoaderPath;

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_NORMAL, L"Launching Tool:- '%s'", Entry->me.Title);
    #endif

    IsBoot = TRUE;

    LoaderPath = Basename (Entry->LoaderPath);
    BeginExternalScreen (Entry->UseGraphicsMode, Entry->me.Title);
    StartEFIImage (
        Entry->Volume,
        Entry->LoaderPath,
        Entry->LoadOptions,
        LoaderPath,
        Entry->OSType,
        TRUE,
        FALSE
    );

    MyFreePool (&LoaderPath);
} // VOID StartTool()
