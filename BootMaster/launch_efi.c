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
 * Copyright (c) 2020-2022 Dayo Akanji (sf.net/u/dakanji/profile)
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

#define FAT_ARCH                0x0ef1fab9 /* ID for Apple "fat" binary */

CHAR16         *BootSelection = NULL;
CHAR16         *ValidText     = L"NOT SET";

extern BOOLEAN  IsBoot;

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

    MY_FREE_POOL(MsgStrA);
    MY_FREE_POOL(MsgStrB);
    MY_FREE_POOL(MsgStrC);
    MY_FREE_POOL(MsgStrD);
    MY_FREE_POOL(MsgStrE);
} // VOID WarnSecureBootError()

// Returns TRUE if this file is a valid EFI loader file, and is proper ARCH
BOOLEAN IsValidLoader (
    EFI_FILE *RootDir,
    CHAR16   *FileName
) {
#if !defined (EFIX64) && !defined (EFI32) && !defined (EFIAARCH64)
    // DA-TAG: Investigate This
    //
    // Return TRUE
    #if REFIT_DEBUG > 0
    ValidText = L"EFI File is *TAKEN* as Valid";
    ALT_LOG(1, LOG_THREE_STAR_MID,
        L"%s:- '%s'",
        ValidText,
        FileName ? FileName : L"NULL File"
    );
    if (IsBoot) {
        LOG_MSG("\n");
        LOG_MSG("%s ... Loading", ValidText);
        LOG_MSG("\n <<----- * ----->>\n\n");
    }
    #endif

    return TRUE;
#else
    EFI_FILE_HANDLE FileHandle;
    EFI_STATUS      Status;
    BOOLEAN         IsValid;
    CHAR8           Header[512];
    UINTN           Size = sizeof (Header);

    if ((RootDir == NULL) || (FileName == NULL)) {
        // DA-TAG: Investigate This
        //         Assume "Valid" here, because Macs produce a NULL RootDir, and maybe FileName,
        //         when launching from Firewire drives. This should be handled better, but the
        //         fix would have to be in StartEFIImage() and/or in FindVolumeAndFilename().
        #if REFIT_DEBUG > 0
        ValidText = L"EFI File is *ASSUMED* to be Valid";
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"%s:- '%s'",
            ValidText,
            FileName ? FileName : L"NULL File"
        );
        if (IsBoot) {
            LOG_MSG("\n");
            LOG_MSG("%s ... Loading", ValidText);
            LOG_MSG("\n <<----- * ----->>\n\n");
        }
        #endif

        return TRUE;
    }
    else if (!FileExists (RootDir, FileName)) {
        #if REFIT_DEBUG > 0
        ValidText = L"EFI File *NOT* Found";
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"%s:- '%s'",
            ValidText,
            FileName ? FileName : L"NULL File"
        );
        if (IsBoot) {
            LOG_MSG("\n\n");
            LOG_MSG("INFO: %s ... Aborting", ValidText);
            LOG_MSG("\n\n");
        }
        #endif

        // Early return if file does not exist
        return FALSE;
    }

    Status = REFIT_CALL_5_WRAPPER(
        RootDir->Open, RootDir,
        &FileHandle, FileName,
        EFI_FILE_MODE_READ, 0
    );

    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        ValidText = L"EFI File is *NOT* Readable";
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"%s:- '%s'",
            ValidText,
            FileName ? FileName : L"NULL File"
        );
        if (IsBoot) {
            LOG_MSG("\n\n");
            LOG_MSG("INFO: %s ... Aborting", ValidText);
            LOG_MSG("\n\n");
        }
        #endif

        return FALSE;
    }

    Status = REFIT_CALL_3_WRAPPER(
        FileHandle->Read, FileHandle,
        &Size, Header
    );
    REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);

    IsValid = (
        !EFI_ERROR(Status) &&
        Size == sizeof (Header) &&
        (
            (
                (Size = *(UINT32 *) &Header[0x3c]) < 0x180  &&
                Header[0]                    == 'M'         &&
                Header[1]                    == 'Z'         &&
                Header[Size]                 == 'P'         &&
                Header[Size+1]               == 'E'         &&
                Header[Size+2]               ==  0          &&
                Header[Size+3]               ==  0          &&
                *(UINT16 *) &Header[Size+4]  ==  EFI_STUB_ARCH
            ) ||
            (*(UINT32 *) &Header == FAT_ARCH)
        )
    );

    #if REFIT_DEBUG > 0
    ValidText = (IsValid)
        ? L"EFI File is Valid"
        : L"EFI File is *NOT* Valid";
    ALT_LOG(1, LOG_THREE_STAR_MID,
        L"%s:- '%s'",
        ValidText,
        FileName ? FileName : L"NULL File"
    );
    #endif

    if (IsBoot) {
        // Reset IsBoot if required
        IsBoot = IsValid;

        #if REFIT_DEBUG > 0
        if (!IsValid) {
            LOG_MSG("\n\n");
            LOG_MSG("INFO: %s ... Aborting", ValidText);
            LOG_MSG("\n\n");
        }
        else {
            LOG_MSG("\n");
            LOG_MSG("%s ... Loading", ValidText);
            LOG_MSG("\n <<----- * ----->>\n\n");
        }
        #endif
    }

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

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
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
    if (LoadOptions != NULL) {
        FullLoadOptions = StrDuplicate (LoadOptions);
        if (OSType == 'M') {
            MergeStrings (&FullLoadOptions, L" ", 0);
            // DA-TAG: The last space is also added by the EFI shell and is
            //         significant when passing options to Apple's boot.efi
        }
    }

    MsgStr = PoolPrint (
        L"Starting '%s' ... Load Options:- '%s'",
        ImageTitle, FullLoadOptions ? FullLoadOptions : L"NULL"
    );

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, MsgStr);
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

    BOOLEAN LoaderValid = IsValidLoader (Volume->RootDir, Filename);

    ReturnStatus = Status = EFI_LOAD_ERROR;  // in case the list is empty
    // Some EFIs crash if attempting to load drivers for an invalid architecture, so
    // protect for this condition; but sometimes Volume comes back NULL, so provide
    // an exception. (TODO: Handle this special condition better.)
    if (!LoaderValid) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"ERROR: Invalid Loader!!");
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
        ValidText = L"NOT SET";
        CheckError (Status, MsgStr);
        MY_FREE_POOL(MsgStr);

        goto bailout;
    }
    else {
        // Store loader name if booting and set to do so
        if (IsBoot && BootSelection) {
            StoreLoaderName (BootSelection);
        }
        BootSelection = NULL;

        DevicePath = FileDevicePath (Volume->DeviceHandle, Filename);

        // Stall to avoid unwanted flash of text when starting loaders
        // Stall works best in smaller increments as per Specs
        if (!IsDriver && (!AllowGraphicsMode || Verbose)) {
            REFIT_CALL_1_WRAPPER(gBS->Stall, 250000);
            REFIT_CALL_1_WRAPPER(gBS->Stall, 250000);
            REFIT_CALL_1_WRAPPER(gBS->Stall, 250000);
            REFIT_CALL_1_WRAPPER(gBS->Stall, 250000);
            REFIT_CALL_1_WRAPPER(gBS->Stall, 250000);
            REFIT_CALL_1_WRAPPER(gBS->Stall, 250000);
            REFIT_CALL_1_WRAPPER(gBS->Stall, 250000);
            REFIT_CALL_1_WRAPPER(gBS->Stall, 250000);
        }

        // NOTE: Commented-out lines below could be more efficient if file were read ahead of
        // time and passed as a pre-loaded image to LoadImage(), but it does not work on my
        // 32-bit Mac Mini or my 64-bit Intel box when launching a Linux kernel.
        // The kernel returns a "Failed to handle fs_proto" error message.
        // TODO: Track down the cause of this error and fix it, if possible.
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
            REFIT_CALL_6_WRAPPER(
                gBS->LoadImage, FALSE,
                SelfImageHandle, GlobalConfig.SelfDevicePath,
                NULL, 0, &ChildImageHandle2
            );
        }
    } // if/else !IsValidLoader

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
        goto bailout;
    }

    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol, ChildImageHandle,
        &LoadedImageProtocol, (VOID **) &ChildLoadedImage
    );
    ReturnStatus = Status;

    if (CheckError (Status, L"while Getting LoadedImageProtocol Handle")) {
        goto bailout_unload;
    }

    ChildLoadedImage->LoadOptions     = (VOID *) FullLoadOptions;
    ChildLoadedImage->LoadOptionsSize = FullLoadOptions
        ? ((UINT32) StrLen (FullLoadOptions) + 1) * sizeof (CHAR16) : 0;

    // Turn control over to the image
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
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"ERROR:- %s", MsgStr);
            LOG_MSG("* ERROR: %s", MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
        }
        #endif

        MY_FREE_POOL(EspGUID);
    } // if write systemd UEFI variables

    // Close open file handles
    UninitRefitLib();

    #if REFIT_DEBUG > 0
    CHAR16 *ConstMsgStr = L"Loading UEFI Driver";
    #endif

    if (!IsDriver) {
        #if REFIT_DEBUG > 0
        ConstMsgStr = L"Running Child Image";
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s via Loader File:- '%s'", ConstMsgStr , ImageTitle);
        #endif

        // DA-TAG: SyncAPFS infrastrcture is typically no longer required
        //         "Typically" as users may place UEFI Shell etc in the first row (loaders)
        //         These may return to the RefindPlus screen but any issues will be trivial
        FreeSyncVolumes();
    }

    Status = REFIT_CALL_3_WRAPPER(
        gBS->StartImage, ChildImageHandle,
        NULL, NULL
    );
    ReturnStatus = Status;

    NewImageHandle = ChildImageHandle;

    // Control returns here when the child image calls Exit()
    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_MID, L"'%r' When %s", ReturnStatus, ConstMsgStr);
    #endif

    MsgStr = StrDuplicate (L"Returned from Child Image");

    #if REFIT_DEBUG > 0
    if (!IsDriver) {
        ALT_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);

        if (Verbose) {
            LOG_MSG("Received Input:");
            LOG_MSG("%s  - Exit Child Image Loader:- '%s'", OffsetNext, ImageTitle);
            LOG_MSG("\n\n");
        }
    }
    #endif

    if (EFI_ERROR(ReturnStatus)) {
        CHAR16 *MsgStrEx = NULL;
        if (IsDriver) {
            #if REFIT_DEBUG > 0
            LOG_MSG("\n");
            #endif

            MsgStrEx = PoolPrint (L"%s:-", MsgStr, ImageTitle);
        }
        else {
            MsgStrEx = StrDuplicate (MsgStr);
        }

        CheckError (ReturnStatus, MsgStrEx);
        MY_FREE_POOL(MsgStrEx);
    }
    MY_FREE_POOL(MsgStr);

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

bailout_unload:
    // unload the image, we do not care if it works or not
    if (!IsDriver) {
        REFIT_CALL_1_WRAPPER(gBS->UnloadImage, ChildImageHandle);
    }

bailout:
    MY_FREE_POOL(FullLoadOptions);
    if (!IsDriver) {
        FinishExternalScreen();
    }

    return ReturnStatus;
} // EFI_STATUS StartEFIImage()

static
BOOLEAN ConfirmReboot (
    CHAR16 *PromptUser
) {
    BOOLEAN Confirmation = TRUE;

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Creating 'Confirm %s' Screen", PromptUser);
    #endif

    REFIT_MENU_SCREEN *ConfirmRebootMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (!ConfirmRebootMenu) {
        // Early Return ... Fail
        return FALSE;
    }

    ConfirmRebootMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_FIRMWARE);
    ConfirmRebootMenu->Title      = PoolPrint (L"Confirm %s", PromptUser);
    ConfirmRebootMenu->Hint1      = StrDuplicate (L"Select an Option and Press 'Enter' or");
    ConfirmRebootMenu->Hint2      = StrDuplicate (L"Press 'Esc' to Return to Main Menu (Without Changes)");

    AddMenuInfoLine (ConfirmRebootMenu, PoolPrint (L"%s?", PromptUser));

    BOOLEAN RetVal = GetYesNoMenuEntry (&ConfirmRebootMenu);
    if (!RetVal) {
        FreeMenuScreen (&ConfirmRebootMenu);

        // Early Return
        return FALSE;
    }

    INTN           DefaultEntry = 1;
    REFIT_MENU_ENTRY  *ChosenOption;
    MENU_STYLE_FUNC Style = (AllowGraphicsMode) ? GraphicsMenuStyle : TextMenuStyle;
    UINTN MenuExit = RunGenericMenu (ConfirmRebootMenu, Style, &DefaultEntry, &ChosenOption);

    #if REFIT_DEBUG > 0
    ALT_LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'ConfirmReboot'",
        MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
    );
    #endif

    if (MenuExit != MENU_EXIT_ENTER || ChosenOption->Tag != TAG_YES) {
        Confirmation = FALSE;
    }

    FreeMenuScreen (&ConfirmRebootMenu);

    return Confirmation;
} // BOOLEAN ConfirmReboot()

// From gummiboot: Reboot the computer into its built-in user interface
EFI_STATUS RebootIntoFirmware (VOID) {
    CHAR16     *TmpStr = L"Reboot into Firmware";
    UINT64     *ItemBuffer;
    UINT64      osind;
    EFI_STATUS  Status;

    osind = EFI_OS_INDICATIONS_BOOT_TO_FW_UI;

    Status = EfivarGetRaw (
        &GlobalGuid, L"OsIndications",
        (VOID **) &ItemBuffer, NULL
    );
    if (Status == EFI_SUCCESS) {
        osind |= *ItemBuffer;
    }
    MY_FREE_POOL(ItemBuffer);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", TmpStr);
    #endif

    BOOLEAN ConfirmAction = ConfirmReboot(TmpStr);
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
    if (Status != EFI_SUCCESS) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Aborted ... OsIndications not Found");
        LOG_MSG("%s    ** Aborted ... OsIndications not Found", OffsetNext);
        LOG_MSG("\n\n");
        #endif

        return Status;
    }

    UninitRefitLib();

    #if REFIT_DEBUG > 0
    LOG_MSG("\n <<----- * ----->>\n\n");
    #endif

    REFIT_CALL_4_WRAPPER(
        gRT->ResetSystem, EfiResetCold,
        EFI_SUCCESS, 0, NULL
    );

    Status = EFI_LOAD_ERROR;
    CHAR16 *MsgStr = PoolPrint (L"%s ... %r", TmpStr, Status);

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
    CHAR16     *TmpStr    = L"Reboot into NVRAM Boot Option";
    CHAR16     *MsgStr    = NULL;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", TmpStr);
    #endif

    BOOLEAN ConfirmAction = ConfirmReboot(TmpStr);
    if (!ConfirmAction) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Aborted by User", MsgStr);
        LOG_MSG("%s    ** Aborted by User", OffsetNext, MsgStr);
        LOG_MSG("\n\n");
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
    LOG_MSG("\n <<----- * ----->>\n\n");
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
    ALT_LOG(1, LOG_LINE_NORMAL, L"Attempting to Enable and Lock VMX");
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
    CHAR16     *NameNVRAM = NULL;
    CHAR8      *DataNVRAM = NULL;
    EFI_GUID    AppleGUID = APPLE_GUID;

    // Set Relevant NVRAM Variable
    CHAR16 *InitNVRAM = L"RecoveryModeDisk";
    NameNVRAM = L"internet-recovery-mode";
    UnicodeStrToAsciiStr (InitNVRAM, DataNVRAM);
    Status = EfivarSetRaw (
        &AppleGUID, NameNVRAM,
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
        &AppleGUID, NameNVRAM,
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

    UINTN Size;
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
} // EFI_STATUS ApfsRecoveryBoot()

// Directly launch an EFI boot loader (or similar program)
VOID StartLoader (
    LOADER_ENTRY *Entry,
    CHAR16       *SelectionName
) {
    CHAR16 *MsgStr     = NULL;
    CHAR16 *LoaderPath = NULL;

    IsBoot        = TRUE;
    BootSelection = SelectionName;
    LoaderPath    = Basename (Entry->LoaderPath);
    MsgStr        = PoolPrint (L"Launching Loader:- '%s'", SelectionName);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    #endif

    if (GlobalConfig.EnableAndLockVMX) {
        DoEnableAndLockVMX();
    }

    BeginExternalScreen (Entry->UseGraphicsMode, MsgStr);
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
    CHAR16  *MsgStr     =  NULL;
    CHAR16  *LoaderPath =  NULL;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    IsBoot        = FALSE;
    LoaderPath    = Basename (Entry->LoaderPath);
    MsgStr        = PoolPrint (L"Launching Tool:- '%s'", Entry->me.Title);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    #endif

    BeginExternalScreen (Entry->UseGraphicsMode, MsgStr);

    #if REFIT_DEBUG > 0
    if (AllowGraphicsMode && !Entry->UseGraphicsMode) {
        LOG_MSG("INFO: Switched Graphics to Text Mode");
        LOG_MSG("%s      %s", OffsetNext, MsgStr);
        LOG_MSG("\n\n");
    }
    #endif

    if (FoundSubStr (Entry->me.Title, L"APFS Instance")) {
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
