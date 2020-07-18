/*
 * gptsync/os_efi.c
 * EFI glue for gptsync
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

#include "gptsync.h"
#include "../include/refit_call_wrapper.h"
#ifdef __MAKEWITH_TIANO
//#include "tiano_includes.h"
#include "AutoGen.h"
#endif

// variables

EFI_BLOCK_IO    *BlockIO = NULL;

//
// sector I/O functions
//

// Returns size of disk in blocks
UINT64 disk_size(VOID) {
   return (UINT64) (BlockIO->Media->LastBlock + 1);
} // UINT64 disk_size()

UINTN read_sector(UINT64 lba, UINT8 *buffer)
{
    EFI_STATUS          Status;

    Status = refit_call5_wrapper(BlockIO->ReadBlocks, BlockIO, BlockIO->Media->MediaId, lba, 512, buffer);
    if (EFI_ERROR(Status)) {
        // TODO: report error
        return 1;
    }
    return 0;
}

UINTN write_sector(UINT64 lba, UINT8 *buffer)
{
    EFI_STATUS          Status;

    Status = refit_call5_wrapper(BlockIO->WriteBlocks, BlockIO, BlockIO->Media->MediaId, lba, 512, buffer);
    if (EFI_ERROR(Status)) {
        // TODO: report error
        return 1;
    }
    return 0;
}

//
// Keyboard input
//

static BOOLEAN ReadAllKeyStrokes(VOID)
{
    EFI_STATUS          Status;
    BOOLEAN             GotKeyStrokes;
    EFI_INPUT_KEY       Key;

    GotKeyStrokes = FALSE;
    for (;;) {
        Status = refit_call2_wrapper(ST->ConIn->ReadKeyStroke, ST->ConIn, &Key);
        if (Status == EFI_SUCCESS) {
            GotKeyStrokes = TRUE;
            continue;
        }
        break;
    }
    return GotKeyStrokes;
}

static VOID PauseForKey(VOID)
{
    UINTN               Index;

    Print(L"\n* Hit any key to continue *");

    if (ReadAllKeyStrokes()) {  // remove buffered key strokes
        refit_call1_wrapper(BS->Stall, 5000000);     // 5 seconds delay
        ReadAllKeyStrokes();    // empty the buffer again
    }

    refit_call3_wrapper(BS->WaitForEvent, 1, &ST->ConIn->WaitForKey, &Index);
    ReadAllKeyStrokes();        // empty the buffer to protect the menu

    Print(L"\n");
}

UINTN input_boolean(CHARN *prompt, BOOLEAN *bool_out)
{
    EFI_STATUS          Status;
    UINTN               Index;
    EFI_INPUT_KEY       Key;

    Print(prompt);

    ReadAllKeyStrokes(); // Remove buffered key strokes
    do {
        refit_call3_wrapper(BS->WaitForEvent, 1, &ST->ConIn->WaitForKey, &Index);
        Status = refit_call2_wrapper(ST->ConIn->ReadKeyStroke, ST->ConIn, &Key);
        if (EFI_ERROR(Status) && Status != EFI_NOT_READY)
            return 1;
    } while (Status == EFI_NOT_READY);

    if (Key.UnicodeChar == 'y' || Key.UnicodeChar == 'Y') {
        Print(L"Yes\n");
        *bool_out = TRUE;
    } else {
        Print(L"No\n");
        *bool_out = FALSE;
    }

    ReadAllKeyStrokes();
    return 0;
}

#ifdef __MAKEWITH_TIANO

// EFI_GUID gEfiDxeServicesTableGuid = { 0x05AD34BA, 0x6F02, 0x4214, { 0x95, 0x2E, 0x4D, 0xA0, 0x39, 0x8E, 0x2B, 0xB9 }};

// Minimal initialization function
static VOID InitializeLib(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
   gST            = SystemTable;
   //    gImageHandle   = ImageHandle;
   gBS            = SystemTable->BootServices;
   //    gRS            = SystemTable->RuntimeServices;
   gRT = SystemTable->RuntimeServices; // Some BDS functions need gRT to be set

//   InitializeConsoleSim();
}

// EFI_GUID gEfiBlockIoProtocolGuid = { 0x964E5B21, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }};

#define LibLocateHandle gBS->LocateHandleBuffer
#define BlockIoProtocol gEfiBlockIoProtocolGuid

#endif

// Performs a case-insensitive string comparison. This function is necesary
// because some EFIs have buggy StriCmp() functions that actually perform
// case-sensitive comparisons.
// Returns TRUE if strings are identical, FALSE otherwise.
static BOOLEAN MyStriCmp(IN CHAR16 *FirstString, IN CHAR16 *SecondString) {
    if (FirstString && SecondString) {
        while ((*FirstString != L'\0') && ((*FirstString & ~0x20) == (*SecondString & ~0x20))) {
                FirstString++;
                SecondString++;
        }
        return (*FirstString == *SecondString);
    } else {
        return FALSE;
    }
} // BOOLEAN MyStriCmp()

// Check firmware vendor; get verification to continue if it's not Apple.
// Returns TRUE if Apple firmware or if user assents to use, FALSE otherwise.
static BOOLEAN VerifyGoOn(VOID) {
   BOOLEAN GoOn = TRUE;
   UINTN invalid;

   if (!MyStriCmp(L"Apple", ST->FirmwareVendor)) {
      Print(L"Your firmware is made by %s.\n", ST->FirmwareVendor);
      Print(L"Ordinarily, a hybrid MBR (which this program creates) should be used ONLY on\n");
      Print(L"Apple Macs that dual-boot with Windows or some other BIOS-mode OS. Are you\n");
      invalid = input_boolean(STR("SURE you want to continue? [y/N] "), &GoOn);
      if (invalid)
         GoOn = FALSE;
   }
   return GoOn;
} // BOOLEAN VerifyGoOn()

//
// main entry point
//

EFI_STATUS
EFIAPI
efi_main    (IN EFI_HANDLE           ImageHandle,
             IN EFI_SYSTEM_TABLE     *SystemTable)
{
    EFI_STATUS          Status;
    UINTN               SyncStatus;
    UINTN               Index;
    UINTN               HandleCount;
    EFI_HANDLE          *HandleBuffer;
    EFI_HANDLE          DeviceHandle;
    EFI_DEVICE_PATH     *DevicePath, *NextDevicePath;
    BOOLEAN             Usable;

    InitializeLib(ImageHandle, SystemTable);

    Status = refit_call5_wrapper(BS->LocateHandleBuffer, ByProtocol, &BlockIoProtocol, NULL, &HandleCount, &HandleBuffer);
    if (EFI_ERROR (Status)) {
        Status = EFI_NOT_FOUND;
        return Status;
    }

    if (!VerifyGoOn())
       return EFI_ABORTED;

    for (Index = 0; Index < HandleCount; Index++) {

        DeviceHandle = HandleBuffer[Index];

        // check device path
        DevicePath = DevicePathFromHandle(DeviceHandle);
        Usable = TRUE;
        while (DevicePath != NULL && !IsDevicePathEndType(DevicePath)) {
            NextDevicePath = NextDevicePathNode(DevicePath);

            if (DevicePathType(DevicePath) == MESSAGING_DEVICE_PATH &&
                (DevicePathSubType(DevicePath) == MSG_USB_DP ||
                 DevicePathSubType(DevicePath) == MSG_USB_CLASS_DP ||
                 DevicePathSubType(DevicePath) == MSG_1394_DP ||
                 DevicePathSubType(DevicePath) == MSG_FIBRECHANNEL_DP))
                Usable = FALSE;         // USB/FireWire/FC device
            if (DevicePathType(DevicePath) == MEDIA_DEVICE_PATH)
                Usable = FALSE;         // partition, El Torito entry, legacy BIOS device

            DevicePath = NextDevicePath;
        }
        if (!Usable)
            continue;

        Status = refit_call3_wrapper(BS->HandleProtocol, DeviceHandle, &BlockIoProtocol, (VOID **) &BlockIO);
        if (EFI_ERROR(Status)) {
            // TODO: report error
            BlockIO = NULL;
        } else {
            if (BlockIO->Media->BlockSize != 512)
                BlockIO = NULL;    // optical media
            else
                break;
        }

    }

    FreePool (HandleBuffer);

    if (BlockIO == NULL) {
        Print(L"Internal hard disk device not found!\n");
        return EFI_NOT_FOUND;
    }

    SyncStatus = gptsync();

    if (SyncStatus == 0)
        PauseForKey();


    if (SyncStatus)
        return EFI_NOT_FOUND;
    return EFI_SUCCESS;
}
