/*
 * Functions related to drivers. Original copyright notices below....
 */
/*
 * Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
 * This program and the accompanying materials are licensed and made available under
 * the terms and conditions of the BSD License that accompanies this distribution.
 * The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php.
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 *
 */
/*
 * refit/main.c
 * Main code for the boot menu
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
 * Modifications copyright (c) 2012-2017 Roderick W. Smith
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

#include "driver_support.h"
#include "lib.h"
#include "mystrings.h"
#include "screen.h"
#include "launch_efi.h"
#include "log.h"
#include "../include/refit_call_wrapper.h"

#if defined (EFIX64)
#define DRIVER_DIRS             L"drivers,drivers_x64"
#elif defined (EFI32)
#define DRIVER_DIRS             L"drivers,drivers_ia32"
#elif defined (EFIAARCH64)
#define DRIVER_DIRS             L"drivers,drivers_aa64"
#else
#define DRIVER_DIRS             L"drivers"
#endif

// Following "global" constants are from EDK2's AutoGen.c....
EFI_GUID gMyEfiLoadedImageProtocolGuid = { 0x5B1B31A1, 0x9562, 0x11D2, { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }};
EFI_GUID gMyEfiDriverBindingProtocolGuid = { 0x18A031AB, 0xB443, 0x4D1A, { 0xA5, 0xC0, 0x0C, 0x09, 0x26, 0x1E, 0x9F, 0x71 }};
EFI_GUID gMyEfiDriverConfigurationProtocolGuid = { 0x107A772B, 0xD5E1, 0x11D4, { 0x9A, 0x46, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D }};
EFI_GUID gMyEfiDriverDiagnosticsProtocolGuid = { 0x0784924F, 0xE296, 0x11D4, { 0x9A, 0x49, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D }};
EFI_GUID gMyEfiComponentNameProtocolGuid = { 0x107A772C, 0xD5E1, 0x11D4, { 0x9A, 0x46, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D }};
EFI_GUID gMyEfiDevicePathProtocolGuid = { 0x09576E91, 0x6D3F, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }};
EFI_GUID gMyEfiDiskIoProtocolGuid = { 0xCE345171, 0xBA0B, 0x11D2, { 0x8E, 0x4F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }};
EFI_GUID gMyEfiBlockIoProtocolGuid = { 0x964E5B21, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }};
EFI_GUID gMyEfiSimpleFileSystemProtocolGuid = { 0x964E5B22, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }};

#ifdef __MAKEWITH_GNUEFI
struct MY_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
struct MY_EFI_FILE_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *MY_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME)(
  IN struct MY_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL    *This,
  OUT struct MY_EFI_FILE_PROTOCOL                 **Root
  );

typedef struct _MY_MY_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
  UINT64                                      Revision;
  MY_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME OpenVolume;
} MY_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct _MY_EFI_FILE_PROTOCOL {
  UINT64                Revision;
  EFI_FILE_OPEN         Open;
  EFI_FILE_CLOSE        Close;
  EFI_FILE_DELETE       Delete;
  EFI_FILE_READ         Read;
  EFI_FILE_WRITE        Write;
  EFI_FILE_GET_POSITION GetPosition;
  EFI_FILE_SET_POSITION SetPosition;
  EFI_FILE_GET_INFO     GetInfo;
  EFI_FILE_SET_INFO     SetInfo;
  EFI_FILE_FLUSH        Flush;
} MY_EFI_FILE_PROTOCOL;

typedef struct _MY_EFI_BLOCK_IO_PROTOCOL {
  UINT64             Revision;
  EFI_BLOCK_IO_MEDIA *Media;
  EFI_BLOCK_RESET    Reset;
  EFI_BLOCK_READ     ReadBlocks;
  EFI_BLOCK_WRITE    WriteBlocks;
  EFI_BLOCK_FLUSH    FlushBlocks;
} MY_EFI_BLOCK_IO_PROTOCOL;
#else /* Make with Tianocore */
#define MY_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
#define MY_EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL
#define MY_EFI_BLOCK_IO_PROTOCOL EFI_BLOCK_IO_PROTOCOL
#endif

/* LibScanHandleDatabase() is used by rEFInd's driver-loading code (inherited
 * from rEFIt), but has not been implemented in GNU-EFI and seems to have been
 * dropped from current versions of the Tianocore library. This function was
 * taken from http://git.etherboot.org/?p=mirror/efi/shell/.git;a=commitdiff;h=b1b0c63423cac54dc964c2930e04aebb46a946ec,
 * The original files are copyright 2006-2011 Intel and BSD-licensed. Minor
 * modifications by Roderick Smith are GPLv3.
 */
EFI_STATUS
LibScanHandleDatabase (EFI_HANDLE  DriverBindingHandle, OPTIONAL
                       UINT32      *DriverBindingHandleIndex, OPTIONAL
                       EFI_HANDLE  ControllerHandle, OPTIONAL
                       UINT32      *ControllerHandleIndex, OPTIONAL
                       UINTN       *HandleCount,
                       EFI_HANDLE  **HandleBuffer,
                       UINT32      **HandleType) {
  EFI_STATUS                          Status;
  UINTN                               HandleIndex;
  EFI_GUID                            **ProtocolGuidArray;
  UINTN                               ArrayCount;
  UINTN                               ProtocolIndex;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *OpenInfo;
  UINTN                               OpenInfoCount;
  UINTN                               OpenInfoIndex;
  UINTN                               ChildIndex;
  BOOLEAN                             DriverBindingHandleIndexValid;

  DriverBindingHandleIndexValid = FALSE;
  if (DriverBindingHandleIndex != NULL) {
    *DriverBindingHandleIndex = 0xffffffff;
  }

  if (ControllerHandleIndex != NULL) {
    *ControllerHandleIndex = 0xffffffff;
  }

  *HandleCount  = 0;
  *HandleBuffer = NULL;
  *HandleType   = NULL;

  //
  // Retrieve the list of all handles from the handle database
  //

  Status = refit_call5_wrapper(BS->LocateHandleBuffer,
     AllHandles,
     NULL,
     NULL,
     HandleCount,
     HandleBuffer
  );
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  *HandleType = AllocatePool (*HandleCount * sizeof (UINT32));
  if (*HandleType == NULL) {
    goto Error;
  }

  for (HandleIndex = 0; HandleIndex < *HandleCount; HandleIndex++) {
    //
    // Assume that the handle type is unknown
    //
    (*HandleType)[HandleIndex] = EFI_HANDLE_TYPE_UNKNOWN;

    if (DriverBindingHandle != NULL &&
        DriverBindingHandleIndex != NULL &&
        (*HandleBuffer)[HandleIndex] == DriverBindingHandle
        ) {
      *DriverBindingHandleIndex     = (UINT32) HandleIndex;
      DriverBindingHandleIndexValid = TRUE;
    }

    if (ControllerHandle != NULL && ControllerHandleIndex != NULL && (*HandleBuffer)[HandleIndex] == ControllerHandle) {
      *ControllerHandleIndex      = (UINT32) HandleIndex;
    }

  }

  for (HandleIndex = 0; HandleIndex < *HandleCount; HandleIndex++) {
    //
    // Retrieve the list of all the protocols on each handle
    //

    Status = refit_call3_wrapper(BS->ProtocolsPerHandle,
                  (*HandleBuffer)[HandleIndex],
                  &ProtocolGuidArray,
                  &ArrayCount
                  );
    if (!EFI_ERROR (Status)) {

      for (ProtocolIndex = 0; ProtocolIndex < ArrayCount; ProtocolIndex++) {

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gMyEfiLoadedImageProtocolGuid) == 0) {
          (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_IMAGE_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gMyEfiDriverBindingProtocolGuid) == 0) {
          (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gMyEfiDriverConfigurationProtocolGuid) == 0) {
          (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_DRIVER_CONFIGURATION_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gMyEfiDriverDiagnosticsProtocolGuid) == 0) {
          (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_DRIVER_DIAGNOSTICS_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gMyEfiComponentNameProtocolGuid) == 0) {
          (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_COMPONENT_NAME_HANDLE;
        }

        if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gMyEfiDevicePathProtocolGuid) == 0) {
          (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_DEVICE_HANDLE;
        }
        //
        // Retrieve the list of agents that have opened each protocol
        //

        Status = refit_call4_wrapper(BS->OpenProtocolInformation,
                      (*HandleBuffer)[HandleIndex],
                      ProtocolGuidArray[ProtocolIndex],
                      &OpenInfo,
                      &OpenInfoCount
                      );
        if (!EFI_ERROR (Status)) {

          for (OpenInfoIndex = 0; OpenInfoIndex < OpenInfoCount; OpenInfoIndex++) {
            if (DriverBindingHandle != NULL && OpenInfo[OpenInfoIndex].AgentHandle == DriverBindingHandle) {
              if ((OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) == EFI_OPEN_PROTOCOL_BY_DRIVER) {
                //
                // Mark the device handle as being managed by the driver specified by DriverBindingHandle
                //
                (*HandleType)[HandleIndex] |= (EFI_HANDLE_TYPE_DEVICE_HANDLE | EFI_HANDLE_TYPE_CONTROLLER_HANDLE);
                //
                // Mark the DriverBindingHandle as being a driver that is managing at least one controller
                //
                if (DriverBindingHandleIndexValid) {
                  (*HandleType)[*DriverBindingHandleIndex] |= EFI_HANDLE_TYPE_DEVICE_DRIVER;
                }
              }

              if ((
                    OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                ) == EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  ) {
                //
                // Mark the DriverBindingHandle as being a driver that is managing at least one child controller
                //
                if (DriverBindingHandleIndexValid) {
                  (*HandleType)[*DriverBindingHandleIndex] |= EFI_HANDLE_TYPE_BUS_DRIVER;
                }
              }

              if (ControllerHandle != NULL && (*HandleBuffer)[HandleIndex] == ControllerHandle) {
                if ((
                      OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  ) == EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                    ) {
                  for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++) {
                    if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].ControllerHandle) {
                      (*HandleType)[ChildIndex] |= (EFI_HANDLE_TYPE_DEVICE_HANDLE | EFI_HANDLE_TYPE_CHILD_HANDLE);
                    }
                  }
                }
              }
            }

            if (DriverBindingHandle == NULL && OpenInfo[OpenInfoIndex].ControllerHandle == ControllerHandle) {
              if ((OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) == EFI_OPEN_PROTOCOL_BY_DRIVER) {
                for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++) {
                  if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].AgentHandle) {
                    (*HandleType)[ChildIndex] |= EFI_HANDLE_TYPE_DEVICE_DRIVER;
                  }
                }
              }

              if ((
                    OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                ) == EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  ) {
                (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_PARENT_HANDLE;
                for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++) {
                  if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].AgentHandle) {
                    (*HandleType)[ChildIndex] |= EFI_HANDLE_TYPE_BUS_DRIVER;
                  }
                }
              }
            }
          }

          MyFreePool (OpenInfo);
        }
      }

      MyFreePool (ProtocolGuidArray);
    }
  }

  return EFI_SUCCESS;

Error:
  MyFreePool (*HandleType);
  MyFreePool (*HandleBuffer);

  *HandleCount  = 0;
  *HandleBuffer = NULL;
  *HandleType   = NULL;

  return Status;
} /* EFI_STATUS LibScanHandleDatabase() */

#ifdef __MAKEWITH_GNUEFI
/* Modified from EDK2 function of a similar name; original copyright Intel &
 * BSD-licensed; modifications by Roderick Smith are GPLv3. */
EFI_STATUS ConnectAllDriversToAllControllers(VOID)
{
    EFI_STATUS           Status;
    UINTN                AllHandleCount;
    EFI_HANDLE           *AllHandleBuffer;
    UINTN                Index;
    UINTN                HandleCount;
    EFI_HANDLE           *HandleBuffer;
    UINT32               *HandleType;
    UINTN                HandleIndex;
    BOOLEAN              Parent;
    BOOLEAN              Device;

    Status = LibLocateHandle(AllHandles,
                             NULL,
                             NULL,
                             &AllHandleCount,
                             &AllHandleBuffer);
    if (EFI_ERROR(Status))
        return Status;

    for (Index = 0; Index < AllHandleCount; Index++) {
        //
        // Scan the handle database
        //
        Status = LibScanHandleDatabase(NULL,
                                       NULL,
                                       AllHandleBuffer[Index],
                                       NULL,
                                       &HandleCount,
                                       &HandleBuffer,
                                       &HandleType);
        if (EFI_ERROR (Status))
            goto Done;

        Device = TRUE;
        if (HandleType[Index] & EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE)
            Device = FALSE;
        if (HandleType[Index] & EFI_HANDLE_TYPE_IMAGE_HANDLE)
            Device = FALSE;

        if (Device) {
            Parent = FALSE;
            for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
                if (HandleType[HandleIndex] & EFI_HANDLE_TYPE_PARENT_HANDLE)
                    Parent = TRUE;
            } // for

            if (!Parent) {
                if (HandleType[Index] & EFI_HANDLE_TYPE_DEVICE_HANDLE) {
                   Status = refit_call4_wrapper(BS->ConnectController,
                                                AllHandleBuffer[Index],
                                                NULL,
                                                NULL,
                                                TRUE);
                }
            }
        }

        MyFreePool (HandleBuffer);
        MyFreePool (HandleType);
    }

Done:
    MyFreePool (AllHandleBuffer);
    return Status;
} /* EFI_STATUS ConnectAllDriversToAllControllers() */
#else
EFI_STATUS ConnectAllDriversToAllControllers(VOID) {
    BdsLibConnectAllDriversToAllControllers();
    return 0;
}
#endif

/*
 * ConnectFilesystemDriver() is modified from DisconnectInvalidDiskIoChildDrivers()
 * in Clover (https://sourceforge.net/projects/cloverefiboot/), which is derived
 * from rEFIt. The refit/main.c file from which this function was taken continues
 * to bear rEFIt's original copyright/licence notice (BSD); modifications by
 * Roderick Smith (2016) are GPLv3.
 */
/**
 * Some UEFI's (like HPQ EFI from HP notebooks) have DiskIo protocols
 * opened BY_DRIVER (by Partition driver in HP case) even when no file system
 * is produced from this DiskIo. This then blocks our FS drivers from connecting
 * and producing file systems.
 * To fix it: we will disconnect drivers that connected to DiskIo BY_DRIVER
 * if this is partition volume and if those drivers did not produce file system,
 * then try to connect every unconnected device to the driver whose handle is
 * passed to us. This should have no effect on systems unaffected by this EFI
 * bug/quirk.
 */
VOID ConnectFilesystemDriver(EFI_HANDLE DriverHandle) {
    EFI_STATUS                            Status;
    UINTN                                 HandleCount = 0;
    UINTN                                 Index;
    UINTN                                 OpenInfoIndex;
    EFI_HANDLE                            *Handles = NULL;
    MY_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL       *Fs;
    MY_EFI_BLOCK_IO_PROTOCOL                 *BlockIo;
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY   *OpenInfo;
    UINTN                                 OpenInfoCount;
    EFI_HANDLE                            DriverHandleList[2];

    //
    // Get all DiskIo handles
    //
    Status = refit_call5_wrapper(gBS->LocateHandleBuffer,
                                 ByProtocol,
                                 &gMyEfiDiskIoProtocolGuid,
                                 NULL,
                                 &HandleCount,
                                 &Handles);
    if (EFI_ERROR(Status) || HandleCount == 0)
        return;

    //
    // Check every DiskIo handle
    //
    for (Index = 0; Index < HandleCount; Index++) {
        //
        // If this is not partition - skip it.
        // This is then whole disk and DiskIo
        // should be opened here BY_DRIVER by Partition driver
        // to produce partition volumes.
        //
        Status = refit_call3_wrapper(gBS->HandleProtocol,
                                     Handles[Index],
                                     &gMyEfiBlockIoProtocolGuid,
                                     (VOID **) &BlockIo);
        if (EFI_ERROR (Status))
            continue;
        if (BlockIo->Media == NULL || !BlockIo->Media->LogicalPartition)
            continue;

        //
        // If SimpleFileSystem is already produced - skip it, this is ok
        //
        Status = refit_call3_wrapper(gBS->HandleProtocol,
                                     Handles[Index],
                                     &gMyEfiSimpleFileSystemProtocolGuid,
                                     (VOID **) &Fs);
        if (Status == EFI_SUCCESS)
            continue;

        //
        // If no SimpleFileSystem on this handle but DiskIo is opened BY_DRIVER
        // then disconnect this connection and try to connect our driver to it
        //
        Status = refit_call4_wrapper(gBS->OpenProtocolInformation,
                                     Handles[Index],
                                     &gMyEfiDiskIoProtocolGuid,
                                     &OpenInfo,
                                     &OpenInfoCount);
        if (EFI_ERROR (Status))
            continue;
        DriverHandleList[1] = NULL;
        for (OpenInfoIndex = 0; OpenInfoIndex < OpenInfoCount; OpenInfoIndex++) {
            if ((OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) == EFI_OPEN_PROTOCOL_BY_DRIVER) {
                Status = refit_call3_wrapper(gBS->DisconnectController,
                                             Handles[Index],
                                             OpenInfo[OpenInfoIndex].AgentHandle,
                                             NULL);
                if (!(EFI_ERROR(Status))) {
                    DriverHandleList[0] = DriverHandle;
                    refit_call4_wrapper(gBS->ConnectController,
                                        Handles[Index],
                                        DriverHandleList,
                                        NULL,
                                        FALSE);
                } // if
            } // if
        } // for
        FreePool (OpenInfo);
    }
    FreePool(Handles);
} // VOID ConnectFilesystemDriver()

// Scan a directory for drivers.
// Originally from rEFIt's main.c (BSD), but modified since then (GPLv3).
static UINTN ScanDriverDir(IN CHAR16 *Path)
{
    EFI_STATUS              Status;
    REFIT_DIR_ITER          DirIter;
    UINTN                   NumFound = 0;
    EFI_FILE_INFO           *DirEntry;
    CHAR16                  *FileName;

    CleanUpPathNameSlashes(Path);
    // look through contents of the directory
    DirIterOpen(SelfRootDir, Path, &DirIter);
    while (DirIterNext(&DirIter, 2, LOADER_MATCH_PATTERNS, &DirEntry)) {
        if (DirEntry->FileName[0] == '.')
            continue;   // skip this

        FileName = PoolPrint(L"%s\\%s", Path, DirEntry->FileName);
        NumFound++;
        Status = StartEFIImage(SelfVolume, FileName, L"", DirEntry->FileName, 0, FALSE, TRUE);
        MyFreePool(DirEntry);
        MyFreePool(FileName);
    } // while
    Status = DirIterClose(&DirIter);
    if ((Status != EFI_NOT_FOUND) && (Status != EFI_INVALID_PARAMETER)) {
        FileName = PoolPrint(L"while scanning the %s directory", Path);
        CheckError(Status, FileName);
        MyFreePool(FileName);
    }
    return (NumFound);
} // static UINTN ScanDriverDir()


// Load all EFI drivers from rEFInd's "drivers" subdirectory and from the
// directories specified by the user in the "scan_driver_dirs" configuration
// file line.
// Originally from rEFIt's main.c (BSD), but modified since then (GPLv3).
// Returns TRUE if any drivers are loaded, FALSE otherwise.
BOOLEAN LoadDrivers(VOID) {
    CHAR16        *Directory, *SelfDirectory;
    UINTN         i = 0, Length, NumFound = 0;

    LOG(1, LOG_LINE_SEPARATOR, L"Loading drivers");
    // load drivers from the subdirectories of rEFInd's home directory specified
    // in the DRIVER_DIRS constant.
    while ((Directory = FindCommaDelimited(DRIVER_DIRS, i++)) != NULL) {
        SelfDirectory = SelfDirPath ? StrDuplicate(SelfDirPath) : NULL;
        CleanUpPathNameSlashes(SelfDirectory);
        MergeStrings(&SelfDirectory, Directory, L'\\');
        NumFound += ScanDriverDir(SelfDirectory);
        MyFreePool(Directory);
        MyFreePool(SelfDirectory);
    }

    // Scan additional user-specified driver directories....
    i = 0;
    while ((Directory = FindCommaDelimited(GlobalConfig.DriverDirs, i++)) != NULL) {
        CleanUpPathNameSlashes(Directory);
        Length = StrLen(Directory);
        if (Length > 0) {
            NumFound += ScanDriverDir(Directory);
        } // if
        MyFreePool(Directory);
    } // while

    // connect all devices
    if (NumFound > 0)
        ConnectAllDriversToAllControllers();
    return (NumFound > 0);
} /* BOOLEAN LoadDrivers() */
