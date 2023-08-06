/*
 * refind/launch_efi.c
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
 * Modifications copyright (c) 2012-2023 Roderick W. Smith
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
#include "log.h"
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

// Amount of a file to read to search for the EFI identifying signatures....
// I've seen signatures as far in as 3680 (0xE60), so read a bit more than
// that....
#define EFI_HEADER_SIZE 3800

static VOID WarnSecureBootError(CHAR16 *Name, BOOLEAN Verbose) {
    if (Name == NULL)
        Name = L"the loader";

    refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_ERROR);
    Print(L"Secure Boot validation failure loading %s!\n", Name);
    refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_BASIC);
    if (Verbose && secure_mode()) {
        Print(L"\nThis computer is configured with Secure Boot active, but\n%s has failed validation.\n", Name);
        Print(L"\nYou can:\n * Launch another boot loader\n");
        Print(L" * Disable Secure Boot in your firmware\n");
        Print(L" * Sign %s with a machine owner key (MOK)\n", Name);
        Print(L" * Use a MOK utility (often present on the second row) to add a MOK with which\n");
        Print(L"   %s has already been signed.\n", Name);
        Print(L" * Use a MOK utility to register %s (\"enroll its hash\") without\n", Name);
        Print(L"   signing it.\n");
        Print(L"\nSee http://www.rodsbooks.com/refind/secureboot.html for more information\n");
        PauseForKey();
    } // if
} // VOID WarnSecureBootError()

// Returns file type:
//  LOADER_TYPE_INVALID if the file type is unknown
//  LOADER_TYPE_EFI if the file is an EFI executable for the current platform
//  LOADER_TYPE_GZIP if the file is a gzip binary *AND* if GlobalConfig.GzippedLoaders is set
// Note that gzip files are not further interrogated; they could uncompress
// into non-executable files and this function would still call them valid
// gzip loaders.
UINTN IsValidLoader(EFI_FILE_PROTOCOL *RootDir, CHAR16 *FileName) {
    UINTN           LoaderType = LOADER_TYPE_EFI;
#if defined (EFIX64) | defined (EFI32) | defined (EFIAARCH64)
    BOOLEAN         IsValid = TRUE;
    EFI_STATUS      Status;
    EFI_FILE_HANDLE FileHandle;
    CHAR8           *Header;
    CHAR16          *TypeDesc = L"a valid";
    UINTN           LoadedSize = EFI_HEADER_SIZE;
    UINTN           SignaturePos;

    Header = AllocatePool(EFI_HEADER_SIZE);
    if (!Header) {
        LOG(1, LOG_LINE_NORMAL, L"Unable to allocate memory in IsValidLoader()!");
        return LOADER_TYPE_INVALID;
    }
    if ((RootDir == NULL) || (FileName == NULL)) {
        // Assume valid here, because Macs produce NULL RootDir (& maybe FileName)
        // when launching from a Firewire drive. This should be handled better, but
        // fix would have to be in StartEFIImage() and/or in FindVolumeAndFilename().
        return LOADER_TYPE_EFI;
    } // if

    Status = refit_call5_wrapper(RootDir->Open, RootDir, &FileHandle, FileName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status))
        return LOADER_TYPE_INVALID;

    Status = refit_call3_wrapper(FileHandle->Read, FileHandle, &LoadedSize, Header);
    refit_call1_wrapper(FileHandle->Close, FileHandle);

    if (EFI_ERROR(Status)) {
        MyFreePool(Header);
        LOG(1, LOG_LINE_NORMAL, L"Error reading %s to determine if it's a valid loader!", FileName);
        return LOADER_TYPE_INVALID;
    }
    SignaturePos = *(UINT32 *)&Header[0x3c];
    LOG(4, LOG_LINE_NORMAL, L"In IsValidLoader(), LoadedSize is %lu, SignaturePos is %lu", LoadedSize, SignaturePos);
    // Search for the normal signatures for an EFI binary....
    IsValid = (LoadedSize == EFI_HEADER_SIZE) &&
              ((Header[0] == 'M' && Header[1] == 'Z' &&
               (SignaturePos < (EFI_HEADER_SIZE - 8)) &&
               Header[SignaturePos] == 'P' && Header[SignaturePos+1] == 'E' &&
               Header[SignaturePos+2] == 0 && Header[SignaturePos+3] == 0 &&
               *(UINT16 *)&Header[SignaturePos+4] == EFI_STUB_ARCH) ||
              (*(UINT32 *)&Header == FAT_ARCH));
    if (!IsValid) {
        // Search for indications that this is a gzipped file. Note, however,
        // that we don't dig deeper into gzipped files, so some invalid
        // gzipped files could be called valid.
        if ((Header[0] == (CHAR8) 0x1F && Header[1] == (CHAR8) 0x8B) && GlobalConfig.GzippedLoaders) {
            LoaderType = LOADER_TYPE_GZIP;
            TypeDesc = L"a gzipped";
        } else {
            LoaderType = LOADER_TYPE_INVALID;
            TypeDesc = L"an invalid";
        }
    }
    LOG(1, LOG_LINE_NORMAL, L"'%s' is %s loader file", FileName, TypeDesc);
    MyFreePool(Header);
#endif
    return LoaderType;
} // UINTN IsValidLoader()

// Launch an EFI binary.
EFI_STATUS StartEFIImage(IN REFIT_VOLUME *Volume,
                         IN CHAR16 *Filename,
                         IN CHAR16 *LoadOptions,
                         IN CHAR16 *ImageTitle,
                         IN CHAR8 OSType,
                         IN BOOLEAN Verbose,
                         IN BOOLEAN IsDriver)
{
    EFI_STATUS              Status, ReturnStatus;
    EFI_HANDLE              ChildImageHandle, ChildImageHandle2;
    EFI_DEVICE_PATH         *DevicePath;
    EFI_LOADED_IMAGE        *ChildLoadedImage = NULL;
    CHAR16                  *ErrorInfo;
    CHAR16                  *FullLoadOptions = NULL;
    CHAR16                  *EspGUID;
    UINT8                   *gzData, *ImageData = NULL;
    UINTN                   gzSize, ImageSize;
    UINTN                   LoaderType;
    int                     gzStatus;
    long                    ReadSize = 0;
    EFI_GUID                SystemdGuid = SYSTEMD_GUID_VALUE;

    // set load options
    if (LoadOptions != NULL) {
        FullLoadOptions = StrDuplicate(LoadOptions);
        if (OSType == 'M') {
            MergeStrings(&FullLoadOptions, L" ", 0);
            // NOTE: That last space is also added by the EFI shell and seems to be significant
            // when passing options to Apple's boot.efi...
        } // if
    } // if (LoadOptions != NULL)
    LOG(1, LOG_LINE_NORMAL, L"Starting %s", ImageTitle);
    LOG(1, LOG_LINE_NORMAL, L"Using load options '%s'", FullLoadOptions ? FullLoadOptions : L"");
    if (IsDriver)
        LOG(1, LOG_LINE_NORMAL, L"Note: %s is a driver", ImageTitle);
    if (Verbose)
        Print(L"Starting %s\nUsing load options '%s'\n", ImageTitle, FullLoadOptions ? FullLoadOptions : L"");

    // load the image into memory
    ReturnStatus = Status = EFI_NOT_FOUND;  // in case the list is empty
    // Some EFIs crash if attempting to load driver for invalid architecture, so
    // protect for this condition; but sometimes Volume comes back NULL, so provide
    // an exception. (TODO: Handle this special condition better.)
    LoaderType = IsValidLoader(Volume->RootDir, Filename);
    if ((LoaderType == LOADER_TYPE_EFI) || (LoaderType == LOADER_TYPE_GZIP)) {
        DevicePath = FileDevicePath(Volume->DeviceHandle, Filename);
        // NOTE: Below commented-out line could be more efficient if file were read ahead of
        // time and passed as a pre-loaded image to LoadImage(), but it doesn't work on my
        // 32-bit Mac Mini or my 64-bit Intel box when launching a Linux kernel; the
        // kernel returns a "Failed to handle fs_proto" error message.
        // TODO: Track down the cause of this error and fix it, if possible.
        if (LoaderType == LOADER_TYPE_EFI) {
            // ReturnStatus = Status = refit_call6_wrapper(BS->LoadImage, FALSE, SelfImageHandle, DevicePath,
            //                                            ImageData, ImageSize, &ChildImageHandle);
            ReturnStatus = Status = refit_call6_wrapper(BS->LoadImage, FALSE, SelfImageHandle, DevicePath,
                                                        NULL, 0, &ChildImageHandle);
        } else {
            Status = egLoadFile(Volume->RootDir, Filename, &gzData, &gzSize);
            if (!EFI_ERROR(Status)) {
                // We need to allocate a buffer sufficient to hold the uncompressed loader.
                // If the buffer is inadequate, the gunzip() call should fail.
                // On ARM64 Linux, an uncompressed kernel is about 3-3.5x the size of a
                // gzip-compressed kernel. To be sure we're well within those limits, go
                // to a factor of 10x....
                ImageSize = gzSize * 10;
                ImageData = AllocateZeroPool(ImageSize);
                gzStatus = gunzip(gzData, gzSize, NULL, NULL, ImageData, ImageSize, &ReadSize);
                LOG(1, LOG_LINE_NORMAL, L"gunzip returned '%d'; ReadSize of '%ld'", gzStatus, ReadSize);
                if (gzStatus == 0) {
                    ReturnStatus = Status = refit_call6_wrapper(BS->LoadImage, FALSE, SelfImageHandle, DevicePath,
                                                                ImageData, 0x8000000, &ChildImageHandle);
                } else {
                    LOG(1, LOG_LINE_NORMAL, L"gunzip failure!");
                    ReturnStatus = Status = EFI_LOAD_ERROR;
                }
            }
        }
        if (secure_mode() && ShimLoaded() && !EFI_ERROR(Status)) {
            // Load ourself into memory. This is a trick to work around a bug in Shim 0.8,
            // which ties itself into the BS->LoadImage() and BS->StartImage() functions and
            // then unregisters itself from the EFI system table when its replacement
            // StartImage() function is called *IF* the previous LoadImage() was for the same
            // program. The result is that rEFInd can validate only the first program it
            // launches (often a filesystem driver). Loading a second program (rEFInd itself,
            // here, to keep it smaller than a kernel) works around this problem. See the
            // replacements.c file in Shim, and especially its start_image() function, for
            // the source of the problem.
            // NOTE: This doesn't check the return status or handle errors. It could
            // conceivably do weird things if, say, rEFInd were on a USB drive that the
            // user pulls before launching a program.
            LOG(3, LOG_LINE_NORMAL, L"Employing Shim LoadImage() hack");
            refit_call6_wrapper(BS->LoadImage, FALSE, SelfImageHandle, GlobalConfig.SelfDevicePath,
                                NULL, 0, &ChildImageHandle2);
        }
    } else {
        Print(L"Invalid loader file!\n");
        LOG(1, LOG_LINE_NORMAL, L"Invalid loader file!");
        ReturnStatus = EFI_LOAD_ERROR;
    }
    if ((Status == EFI_ACCESS_DENIED) || (Status == EFI_SECURITY_VIOLATION)) {
        LOG(1, LOG_LINE_NORMAL, L"Secure boot error while loading '%s'; Status = %d", ImageTitle, Status);
        WarnSecureBootError(ImageTitle, Verbose);
        goto bailout;
    }
    ErrorInfo = PoolPrint(L"while loading %s", ImageTitle);
    if (CheckError(Status, ErrorInfo)) {
        MyFreePool(ErrorInfo);
        goto bailout;
    }

    ReturnStatus = Status = refit_call3_wrapper(BS->HandleProtocol, ChildImageHandle, &LoadedImageProtocol,
                                                (VOID **) &ChildLoadedImage);
    if (CheckError(Status, L"while getting a LoadedImageProtocol handle")) {
        goto bailout_unload;
    }
    ChildLoadedImage->LoadOptions = (VOID *)FullLoadOptions;
    ChildLoadedImage->LoadOptionsSize = FullLoadOptions ? ((UINT32)StrLen(FullLoadOptions) + 1) * sizeof(CHAR16) : 0;
    // turn control over to the image
    // TODO: (optionally) re-enable the EFI watchdog timer!

    if ((GlobalConfig.WriteSystemdVars) && ((OSType == 'L') || (OSType == 'E') || (OSType == 'G'))) {
        // Tell systemd what ESP rEFInd used
        EspGUID = GuidAsString(&(SelfVolume->PartGuid));
        LOG(1, LOG_LINE_NORMAL, L"Setting systemd's LoaderDevicePartUUID variable to %s", EspGUID);
        Status = EfivarSetRaw(&SystemdGuid, L"LoaderDevicePartUUID", (CHAR8 *) EspGUID,
                              StrLen(EspGUID) * 2 + 2, TRUE);
        if (EFI_ERROR(Status)) {
            LOG(1, LOG_LINE_NORMAL,
                L"Error %d when trying to set LoaderDevicePartUUID EFI variable", Status);
        }
        MyFreePool(EspGUID);
    } // if write systemd EFI variables

    // close open file handles
    LOG(1, LOG_LINE_NORMAL, L"Launching '%s'", ImageTitle);
    UninitRefitLib();

    // Actually launch the program....
    ReturnStatus = Status = refit_call3_wrapper(BS->StartImage, ChildImageHandle, NULL, NULL);

    // control returns here when the child image calls Exit()
    ErrorInfo = PoolPrint(L"returned from %s", ImageTitle);
    CheckError(Status, ErrorInfo);
    MyFreePool(ErrorInfo);
    if (IsDriver) {
        // Below should have no effect on most systems, but works
        // around bug with some EFIs that prevents filesystem drivers
        // from binding to partitions.
        ConnectFilesystemDriver(ChildImageHandle);
    }

    // re-open file handles
    ReinitRefitLib();
    LOG(1, LOG_LINE_NORMAL, L"Program has returned %d", Status);

bailout_unload:
    // unload the image, we don't care if it works or not...
    if (!IsDriver)
        Status = refit_call1_wrapper(BS->UnloadImage, ChildImageHandle);

bailout:
    MyFreePool(ImageData);
    MyFreePool(FullLoadOptions);
    if (!IsDriver)
        FinishExternalScreen();

    LOG(1, LOG_LINE_THIN_SEP, L"Next loader");
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
    if (err == EFI_SUCCESS)
        osind |= (UINT64)*b;
    MyFreePool(b);

    err = EfivarSetRaw(&GlobalGuid, L"OsIndications", (CHAR8 *)&osind, sizeof(UINT64), TRUE);
    if (err != EFI_SUCCESS)
        return err;

    LOG(1, LOG_LINE_SEPARATOR, L"Rebooting into the computer's firmware");
    UninitRefitLib();
    refit_call4_wrapper(RT->ResetSystem, EfiResetCold, EFI_SUCCESS, 0, NULL);
    ReinitRefitLib();
    LOG(1, LOG_LINE_NORMAL, L"Error calling ResetSystem: %r", err);
    Print(L"Error calling ResetSystem: %r\n", err);
    PauseForKey();
    return err;
} // EFI_STATUS RebootIntoFirmware()

// Reboot into a loader defined in the EFI's NVRAM
VOID RebootIntoLoader(LOADER_ENTRY *Entry) {
    EFI_STATUS Status;

    LOG(1, LOG_LINE_SEPARATOR, L"Rebooting into EFI loader '%s' (Boot%04x)",
        Entry->Title, Entry->EfiBootNum);
    Status = EfivarSetRaw(&GlobalGuid, L"BootNext", (CHAR8*) &(Entry->EfiBootNum), sizeof(UINT16), TRUE);
    if (EFI_ERROR(Status)) {
        LOG(1, LOG_LINE_NORMAL, L"Error: %d", Status);
        Print(L"Error: %d\n", Status);
        return;
    }
    StoreLoaderName(Entry->me.Title);
    LOG(1, LOG_LINE_NORMAL, L"Attempting to reboot....", Entry->Title, Entry->EfiBootNum);
    UninitRefitLib();
    refit_call4_wrapper(RT->ResetSystem, EfiResetCold, EFI_SUCCESS, 0, NULL);
    ReinitRefitLib();
    LOG(1, LOG_LINE_NORMAL, L"Error calling ResetSystem: %r", Status);
    Print(L"Error calling ResetSystem: %r\n", Status);
    PauseForKey();
} // RebootIntoLoader()

//
// EFI OS loader functions
//

// See http://www.thomas-krenn.com/en/wiki/Activating_the_Intel_VT_Virtualization_Feature
// for information on Intel VMX features
static VOID DoEnableAndLockVMX(VOID) {
#if defined (EFIX64) | defined (EFI32)
    UINT32 msr = 0x3a;
    UINT32 low_bits = 0, high_bits = 0;

    LOG(1, LOG_LINE_NORMAL, L"Attempting to enable and lock VMX");
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
VOID StartLoader(LOADER_ENTRY *Entry, CHAR16 *SelectionName) {
    CHAR16 *LoaderPath;

    LOG(1, LOG_LINE_SEPARATOR, L"Launching '%s'", SelectionName);
    if (GlobalConfig.EnableAndLockVMX) {
        DoEnableAndLockVMX();
    }

    LoaderPath = Basename(Entry->LoaderPath);
    BeginExternalScreen(Entry->UseGraphicsMode, L"Booting OS");
    StoreLoaderName(SelectionName);
    StartEFIImage(Entry->Volume, Entry->LoaderPath, Entry->LoadOptions,
                  LoaderPath, Entry->OSType, !Entry->UseGraphicsMode, FALSE);
    MyFreePool(LoaderPath);
} // VOID StartLoader()

// Launch an EFI tool (a shell, SB management utility, etc.)
VOID StartTool(IN LOADER_ENTRY *Entry) {
    CHAR16 *LoaderPath;

    LOG(1, LOG_LINE_SEPARATOR, L"Starting '%s'", Entry->me.Title);
    BeginExternalScreen(Entry->UseGraphicsMode, Entry->me.Title + 6);  // assumes "Start <title>" as assigned below
    StoreLoaderName(Entry->me.Title);
    LoaderPath = Basename(Entry->LoaderPath);
    StartEFIImage(Entry->Volume, Entry->LoaderPath, Entry->LoadOptions,
                  LoaderPath, Entry->OSType, TRUE, FALSE);
    MyFreePool(LoaderPath);
} /* VOID StartTool() */
