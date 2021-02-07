/*
 * MainRP/launch_efi.c
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
 * Modifications copyright (c) 2012-2020 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 *
 */
/*
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
 */

#include "global.h"
#include "screen.h"
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

static VOID WarnSecureBootError(
    CHAR16 *Name,
    BOOLEAN Verbose
) {
    CHAR16 *ShowScreenStrA = NULL;
    CHAR16 *ShowScreenStrB = NULL;
    CHAR16 *ShowScreenStrC = NULL;
    CHAR16 *ShowScreenStrD = NULL;
    CHAR16 *ShowScreenStrE = NULL;

    if (Name == NULL) {
        Name = L"the Loader";
    }

    ShowScreenStrA = PoolPrint (L"Secure Boot Validation Failure While Loading %s!", Name);

    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText (ShowScreenStrA, NEXTLINE);
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

    if (Verbose && secure_mode()) {
        ShowScreenStrB = PoolPrint (
            L"This computer is configured with Secure Boot active but '%s' has failed validation.",
            Name
        );
        PrintUglyText (ShowScreenStrB, NEXTLINE);
        PrintUglyText (L"You can:", NEXTLINE);
        PrintUglyText (L" * Launch another boot loader", NEXTLINE);
        PrintUglyText (L" * Disable Secure Boot in your firmware", NEXTLINE);
        ShowScreenStrC = PoolPrint (
            L" * Sign %s with a machine owner key (MOK)",
            Name
        );
        PrintUglyText (ShowScreenStrC, NEXTLINE);
        ShowScreenStrD = PoolPrint (
            L" * Use a MOK utility to add a MOK with which '%s' has already been signed.",
            Name
        );
        PrintUglyText (ShowScreenStrD, NEXTLINE);
        ShowScreenStrE = PoolPrint (
            L" * Use a MOK utility to register '%s' ('Enroll its Hash') without signing it",
            Name
        );
        PrintUglyText (ShowScreenStrE, NEXTLINE);
        PrintUglyText (L"See http://www.rodsbooks.com/refind/secureboot.html for more information", NEXTLINE);

    } // if
    PauseForKey();
    MyFreePool (ShowScreenStrA);
    MyFreePool (ShowScreenStrB);
    MyFreePool (ShowScreenStrC);
    MyFreePool (ShowScreenStrD);
    MyFreePool (ShowScreenStrE);
} // VOID WarnSecureBootError()

// Returns TRUE if this file is a valid EFI loader file, and is proper ARCH
BOOLEAN IsValidLoader(EFI_FILE *RootDir, CHAR16 *FileName) {
    BOOLEAN         IsValid = TRUE;
#if defined (EFIX64) | defined (EFI32) | defined (EFIAARCH64)
    EFI_STATUS      Status;
    EFI_FILE_HANDLE FileHandle;
    CHAR8           Header[512];
    UINTN           Size           = sizeof (Header);

    if ((RootDir == NULL) || (FileName == NULL)) {
        // Assume valid here, because Macs produce NULL RootDir (& maybe FileName)
        // when launching from a Firewire drive. This should be handled better, but
        // fix would have to be in StartEFIImage() and/or in FindVolumeAndFilename().
        return TRUE;
    } // if

    Status = refit_call5_wrapper(
        RootDir->Open,
        RootDir,
        &FileHandle,
        FileName,
        EFI_FILE_MODE_READ,
        0
    );
    if (EFI_ERROR (Status)) {
        #if REFIT_DEBUG > 0
        MsgLog ("** WARN: Read Loader File ...\n\n", Status);
        #endif

        return FALSE;
    }

    Status = refit_call3_wrapper(FileHandle->Read, FileHandle, &Size, Header);
    refit_call1_wrapper(FileHandle->Close, FileHandle);

    IsValid = !EFI_ERROR (Status) &&
              Size == sizeof (Header) &&
              ((Header[0] == 'M' && Header[1] == 'Z' &&
               (Size = *(UINT32 *)&Header[0x3c]) < 0x180 &&
               Header[Size] == 'P' && Header[Size+1] == 'E' &&
               Header[Size+2] == 0 && Header[Size+3] == 0 &&
               *(UINT16 *)&Header[Size+4] == EFI_STUB_ARCH) ||
              (*(UINT32 *)&Header == FAT_ARCH));
#endif
    return IsValid;
} // BOOLEAN IsValidLoader()

// Launch an EFI binary.
EFI_STATUS
StartEFIImage (
    IN REFIT_VOLUME  *Volume,
    IN CHAR16        *Filename,
    IN CHAR16        *LoadOptions,
    IN CHAR16        *ImageTitle,
    IN CHAR8         OSType,
    IN BOOLEAN       Verbose,
    IN BOOLEAN       IsDriver
) {
    EFI_STATUS              Status, ReturnStatus;
    EFI_HANDLE              ChildImageHandle, ChildImageHandle2;
    EFI_DEVICE_PATH         *DevicePath;
    EFI_LOADED_IMAGE        *ChildLoadedImage = NULL;
    CHAR16                  *FullLoadOptions  = NULL;
    CHAR16                  *ShowScreenStr    = NULL;
    CHAR16                  ErrorInfo[256];

    // set load options
    if (LoadOptions != NULL) {
        FullLoadOptions = StrDuplicate(LoadOptions);
        if (OSType == 'M') {
            MergeStrings(&FullLoadOptions, L" ", 0);
            // NOTE: That last space is also added by the EFI shell and seems to be significant
            // when passing options to Apple's boot.efi...
        } // if
    } // if (LoadOptions != NULL)
    if (Verbose) {
        Print(
            L"Starting %s\nUsing load options '%s'\n",
            ImageTitle,
            FullLoadOptions ? FullLoadOptions : L""
        );
    }

    // load the image into memory
    ReturnStatus = Status = EFI_LOAD_ERROR;  // in case the list is empty
    // Some EFIs crash if attempting to load driver for invalid architecture, so
    // protect for this condition; but sometimes Volume comes back NULL, so provide
    // an exception. (TODO: Handle this special condition better.)
    if (IsValidLoader(Volume->RootDir, Filename)) {
        DevicePath = FileDevicePath(Volume->DeviceHandle, Filename);
        // NOTE: Commented-out line below could be more efficient if file were read ahead of
        // time and passed as a pre-loaded image to LoadImage(), but it doesn't work on my
        // 32-bit Mac Mini or my 64-bit Intel box when launching a Linux kernel; the
        // kernel returns a "Failed to handle fs_proto" error message.
        // TODO: Track down the cause of this error and fix it, if possible.
        // Status = refit_call6_wrapper(gBS->LoadImage, FALSE, SelfImageHandle, DevicePath,
        //                              ImageData, ImageSize, &ChildImageHandle);
        // ReturnStatus = Status;
        Status = refit_call6_wrapper(
            gBS->LoadImage,
            FALSE,
            SelfImageHandle,
            DevicePath,
            NULL,
            0,
            &ChildImageHandle
        );
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
            refit_call6_wrapper(
                gBS->LoadImage,
                FALSE,
                SelfImageHandle,
                GlobalConfig.SelfDevicePath,
                NULL,
                0,
                &ChildImageHandle2
            );
        }
    } else {
        SwitchToText (FALSE);

        ShowScreenStr = PoolPrint (L"Invalid Loader File:- '%s'", ImageTitle);

        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
        PrintUglyText (ShowScreenStr, NEXTLINE);
        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

        MyFreePool (ShowScreenStr);

        PauseSeconds(3);
        SwitchToGraphics();
    }

    if ((Status == EFI_ACCESS_DENIED) || (Status == EFI_SECURITY_VIOLATION)) {
        WarnSecureBootError(ImageTitle, Verbose);
        goto bailout;
    }

    SPrint(ErrorInfo, 255, L"while loading %s", ImageTitle);
    if (CheckError(Status, ErrorInfo)) {
        MyFreePool (ErrorInfo);
        goto bailout;
    }

    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        ChildImageHandle,
        &LoadedImageProtocol,
        (VOID **) &ChildLoadedImage
    );
    ReturnStatus = Status;
    if (CheckError(Status, L"while getting a LoadedImageProtocol handle")) {
        goto bailout_unload;
    }
    ChildLoadedImage->LoadOptions = (VOID *)FullLoadOptions;
    ChildLoadedImage->LoadOptionsSize = FullLoadOptions ? ((UINT32)StrLen(FullLoadOptions) + 1) * sizeof (CHAR16) : 0;
    // turn control over to the image
    // TODO: (optionally) re-enable the EFI watchdog timer!

    // close open file handles
    UninitRefitLib();
    Status = refit_call3_wrapper(gBS->StartImage, ChildImageHandle, NULL, NULL);
    ReturnStatus = Status;

    // control returns here when the child image calls Exit()
    SPrint(ErrorInfo, 255, L"returned from %s", ImageTitle);
    CheckError(Status, ErrorInfo);
    if (IsDriver) {
        // Below should have no effect on most systems, but works
        // around bug with some EFIs that prevents filesystem drivers
        // from binding to partitions.
        ConnectFilesystemDriver(ChildImageHandle);
    }

    // re-open file handles
    ReinitRefitLib();

bailout_unload:
    // unload the image, we don't care if it works or not...
    if (!IsDriver) {
        Status = refit_call1_wrapper(gBS->UnloadImage, ChildImageHandle);
    }

bailout:
    MyFreePool (FullLoadOptions);
    if (!IsDriver) {
        FinishExternalScreen();
    }

    return ReturnStatus;
} /* EFI_STATUS StartEFIImage() */

// From gummiboot: Reboot the computer into its built-in user interface
EFI_STATUS RebootIntoFirmware(VOID) {
    CHAR8 *b;
    UINTN size;
    UINT64 osind;
    EFI_STATUS err;

    osind = EFI_OS_INDICATIONS_BOOT_TO_FW_UI;

    err = EfivarGetRaw(&GlobalGuid, L"OsIndications", &b, &size);
    if (err == EFI_SUCCESS) {
        osind |= (UINT64)*b;
    }
    MyFreePool (b);

    err = EfivarSetRaw(&GlobalGuid, L"OsIndications", (CHAR8 *)&osind, sizeof (UINT64), TRUE);
    if (err != EFI_SUCCESS) {
        return err;
    }

    refit_call4_wrapper(
        gRT->ResetSystem,
        EfiResetCold,
        EFI_SUCCESS,
        0,
        NULL
    );
    Print(L"Error calling ResetSystem: %r", err);
    PauseForKey();

    return err;
} // EFI_STATUS RebootIntoFirmware()

//
// EFI OS loader functions
//

// See http://www.thomas-krenn.com/en/wiki/Activating_the_Intel_VT_Virtualization_Feature
// for information on Intel VMX features
static VOID DoEnableAndLockVMX(VOID) {
#if defined (EFIX64) | defined (EFI32)
    UINT32 msr = 0x3a;
    UINT32 low_bits = 0, high_bits = 0;

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

VOID StartLoader(LOADER_ENTRY *Entry, CHAR16 *SelectionName) {
    if (GlobalConfig.EnableAndLockVMX) {
        DoEnableAndLockVMX();
    }

    BeginExternalScreen(Entry->UseGraphicsMode, L"Booting OS");
    StoreLoaderName(SelectionName);
    StartEFIImage(
        Entry->Volume,
        Entry->LoaderPath,
        Entry->LoadOptions,
        Basename(Entry->LoaderPath),
        Entry->OSType,
        !Entry->UseGraphicsMode,
        FALSE
    );
} // VOID StartLoader()

VOID StartTool(IN LOADER_ENTRY *Entry) {
    BeginExternalScreen(Entry->UseGraphicsMode, Entry->me.Title + 6);  // assumes "Start <title>" as assigned below
    StoreLoaderName(Entry->me.Title);
    StartEFIImage(
        Entry->Volume,
        Entry->LoaderPath,
        Entry->LoadOptions,
        Basename(Entry->LoaderPath),
        Entry->OSType,
        TRUE,
        FALSE
    );
} /* VOID StartTool() */
