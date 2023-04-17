/*
 * BootMaster/driver_support.c
 * Functions related to drivers. Original copyright notices below.
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
 * Modified for RefindPlus
 * Copyright (c) 2020-2022 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "driver_support.h"
#include "lib.h"
#include "mystrings.h"
#include "screenmgt.h"
#include "launch_efi.h"
#include "../include/refit_call_wrapper.h"

#if defined (EFIX64)
#   define DRIVER_DIRS             L"drivers,drivers_x64"
#elif defined (EFI32)
#   define DRIVER_DIRS             L"drivers,drivers_ia32"
#elif defined (EFIAARCH64)
#   define DRIVER_DIRS             L"drivers,drivers_aa64"
#else
#   define DRIVER_DIRS             L"drivers"
#endif

#ifdef __MAKEWITH_GNUEFI
struct MY_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
struct MY_EFI_FILE_PROTOCOL;

typedef
EFI_STATUS (EFIAPI *MY_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME) (
    IN struct MY_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL    *This,
    OUT struct MY_EFI_FILE_PROTOCOL                **Root
);

typedef struct _MY_MY_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64                                         Revision;
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
    UINT64              Revision;
    EFI_BLOCK_IO_MEDIA *Media;
    EFI_BLOCK_RESET     Reset;
    EFI_BLOCK_READ      ReadBlocks;
    EFI_BLOCK_WRITE     WriteBlocks;
    EFI_BLOCK_FLUSH     FlushBlocks;
} MY_EFI_BLOCK_IO_PROTOCOL;
#else /* Make with Tianocore */
#define MY_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
#define MY_EFI_FILE_PROTOCOL               EFI_FILE_PROTOCOL
#define MY_EFI_BLOCK_IO_PROTOCOL           EFI_BLOCK_IO_PROTOCOL
#endif

#ifdef __MAKEWITH_TIANO
// DA-TAG: Limit to TianoCore
extern EFI_STATUS OcRegisterDriversToHighestPriority (
    IN EFI_HANDLE  *PriorityDrivers
);
#endif

/* LibScanHandleDatabase() is used by RefindPlus' driver-loading code (inherited
 * from rEFIt), but has not been implemented in GNU-EFI and seems to have been
 * dropped from current versions of the Tianocore library. This function was taken from
 * http://git.etherboot.org/?p=mirror/efi/shell/.git;a=commitdiff;h=b1b0c63423cac54dc964c2930e04aebb46a946ec,
 * The original files are copyright 2006-2011 Intel and BSD-licensed. Minor
 * modifications by Roderick Smith are GPLv3.
 */
EFI_STATUS LibScanHandleDatabase (
    EFI_HANDLE             DriverBindingHandle,
    OPTIONAL UINT32       *DriverBindingHandleIndex,
    OPTIONAL EFI_HANDLE    ControllerHandle,
    OPTIONAL UINT32       *ControllerHandleIndex,
    OPTIONAL UINTN        *HandleCount,
    EFI_HANDLE           **HandleBuffer,
    UINT32               **HandleType
) {
    EFI_STATUS                             Status;
    UINTN                                  HandleIndex;
    EFI_GUID                             **ProtocolGuidArray;
    UINTN                                  ArrayCount;
    UINTN                                  ProtocolIndex;
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY   *OpenInfo;
    UINTN                                  OpenInfoCount;
    UINTN                                  OpenInfoIndex;
    UINTN                                  ChildIndex;
    BOOLEAN                                DriverBindingHandleIndexValid;

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

    Status = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, AllHandles,
        NULL, NULL,
        HandleCount, HandleBuffer
    );
    if (EFI_ERROR(Status)) {
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

        if (ControllerHandle != NULL &&
            ControllerHandleIndex != NULL &&
            (*HandleBuffer)[HandleIndex] == ControllerHandle
        ) {
            *ControllerHandleIndex = (UINT32) HandleIndex;
        }
    }

    for (HandleIndex = 0; HandleIndex < *HandleCount; HandleIndex++) {
        //
        // Retrieve the list of all the protocols on each handle
        //
        Status = REFIT_CALL_3_WRAPPER(
            gBS->ProtocolsPerHandle, (*HandleBuffer)[HandleIndex],
            &ProtocolGuidArray, &ArrayCount
        );

        if (!EFI_ERROR(Status)) {
            for (ProtocolIndex = 0; ProtocolIndex < ArrayCount; ProtocolIndex++) {
                if (CompareGuid (
                    ProtocolGuidArray[ProtocolIndex], &gEfiLoadedImageProtocolGuid
                ) == 0) {
                    (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_IMAGE_HANDLE;
                }
                if (CompareGuid (
                    ProtocolGuidArray[ProtocolIndex], &gEfiDriverBindingProtocolGuid
                ) == 0) {
                    (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE;
                }
                if (CompareGuid (
                    ProtocolGuidArray[ProtocolIndex], &gEfiDriverConfigurationProtocolGuid
                ) == 0) {
                    (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_DRIVER_CONFIGURATION_HANDLE;
                }
                if (CompareGuid (
                    ProtocolGuidArray[ProtocolIndex], &gEfiDriverDiagnosticsProtocolGuid
                ) == 0) {
                    (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_DRIVER_DIAGNOSTICS_HANDLE;
                }
                if (CompareGuid (
                    ProtocolGuidArray[ProtocolIndex], &gEfiComponentNameProtocolGuid
                ) == 0) {
                    (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_COMPONENT_NAME_HANDLE;
                }
                if (CompareGuid (
                    ProtocolGuidArray[ProtocolIndex], &gEfiDevicePathProtocolGuid
                ) == 0) {
                    (*HandleType)[HandleIndex] |= EFI_HANDLE_TYPE_DEVICE_HANDLE;
                }

                //
                // Retrieve the list of agents that have opened each protocol
                //
                Status = REFIT_CALL_4_WRAPPER(
                    gBS->OpenProtocolInformation, (*HandleBuffer)[HandleIndex],
                    ProtocolGuidArray[ProtocolIndex], &OpenInfo, &OpenInfoCount
                );

                if (!EFI_ERROR(Status)) {
                    for (OpenInfoIndex = 0; OpenInfoIndex < OpenInfoCount; OpenInfoIndex++) {
                        if (DriverBindingHandle != NULL &&
                            OpenInfo[OpenInfoIndex].AgentHandle == DriverBindingHandle
                        ) {
                            if ((OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER)
                                == EFI_OPEN_PROTOCOL_BY_DRIVER
                            ) {
                                // Mark the device handle as being managed by the driver
                                // specified by DriverBindingHandle
                                (*HandleType)[HandleIndex] |=
                                (EFI_HANDLE_TYPE_DEVICE_HANDLE | EFI_HANDLE_TYPE_CONTROLLER_HANDLE);
                                // Mark the DriverBindingHandle as being a driver that is
                                // managing at least one controller
                                if (DriverBindingHandleIndexValid) {
                                    (*HandleType)[*DriverBindingHandleIndex] |= EFI_HANDLE_TYPE_DEVICE_DRIVER;
                                }
                            }

                            if ((OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER)
                                == EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                            ) {
                                // Mark the DriverBindingHandle as being a driver that is
                                // managing at least one child controller
                                if (DriverBindingHandleIndexValid) {
                                    (*HandleType)[*DriverBindingHandleIndex] |= EFI_HANDLE_TYPE_BUS_DRIVER;
                                }
                            }

                            if (ControllerHandle != NULL
                                && (*HandleBuffer)[HandleIndex] == ControllerHandle
                            ) {
                                if ((OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER)
                                    == EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                                ) {
                                    for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++) {
                                        if (
                                            (*HandleBuffer)[ChildIndex] ==
                                            OpenInfo[OpenInfoIndex].ControllerHandle
                                        ) {
                                            (*HandleType)[ChildIndex] |=
                                            (EFI_HANDLE_TYPE_DEVICE_HANDLE | EFI_HANDLE_TYPE_CHILD_HANDLE);
                                        }
                                    }
                                }
                            }
                        }

                        if (DriverBindingHandle == NULL &&
                            OpenInfo[OpenInfoIndex].ControllerHandle == ControllerHandle
                        ) {
                            if ((OpenInfo[OpenInfoIndex].Attributes &
                                EFI_OPEN_PROTOCOL_BY_DRIVER) == EFI_OPEN_PROTOCOL_BY_DRIVER
                            ) {
                                for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++) {
                                    if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].AgentHandle) {
                                        (*HandleType)[ChildIndex] |= EFI_HANDLE_TYPE_DEVICE_DRIVER;
                                    }
                                }
                            }

                            if ((OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER)
                                == EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
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

                    MY_FREE_POOL(OpenInfo);
                }
            }

            MY_FREE_POOL(ProtocolGuidArray);
        }
    }

    return EFI_SUCCESS;

Error:
    MY_FREE_POOL(*HandleType);
    MY_FREE_POOL(*HandleBuffer);

    *HandleCount  = 0;

    return Status;
} // EFI_STATUS LibScanHandleDatabase()

/* Modified from EDK2 function of a similar name; original copyright Intel &
 * BSD-licensed; modifications by Roderick Smith are GPLv3.
 */
EFI_STATUS ConnectAllDriversToAllControllers (
    IN BOOLEAN ResetGOP
) {
#ifndef __MAKEWITH_GNUEFI
    BdsLibConnectAllDriversToAllControllers (ResetGOP);
    return 0;
#else
    EFI_STATUS   Status;
    UINTN        AllHandleCount;
    EFI_HANDLE  *AllHandleBuffer;
    UINTN        Index;
    UINTN        HandleCount;
    EFI_HANDLE  *HandleBuffer;
    UINT32      *HandleType;
    UINTN        HandleIndex;
    BOOLEAN      Parent;
    BOOLEAN      Device;

    Status = LibLocateHandle (
        AllHandles,
        NULL, NULL,
        &AllHandleCount,
        &AllHandleBuffer
    );

    if (EFI_ERROR(Status)) {
        return Status;
    }

    for (Index = 0; Index < AllHandleCount; Index++) {
        // Scan the handle database
        Status = LibScanHandleDatabase (
            NULL, NULL,
            AllHandleBuffer[Index], NULL,
            &HandleCount,
            &HandleBuffer,
            &HandleType
        );

        if (EFI_ERROR(Status)) {
            break;
        }

        Device = TRUE;
        if (HandleType[Index] & EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE
            || HandleType[Index] & EFI_HANDLE_TYPE_IMAGE_HANDLE
        ) {
            Device = FALSE;
        }

        // Dummy ... just to use passed parameter which is only needed for TianoCore
        Parent = ResetGOP;
        if (Device) {
            Parent = FALSE;
            for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
                if (HandleType[HandleIndex] & EFI_HANDLE_TYPE_PARENT_HANDLE) {
                    Parent = TRUE;
                }
            } // for

            if (!Parent) {
                if (HandleType[Index] & EFI_HANDLE_TYPE_DEVICE_HANDLE) {
                   Status = REFIT_CALL_4_WRAPPER(
                       gBS->ConnectController, AllHandleBuffer[Index],
                       NULL, NULL, TRUE
                   );
               }
            }
        }

        MY_FREE_POOL(HandleBuffer);
        MY_FREE_POOL(HandleType);
    }
    MY_FREE_POOL(AllHandleBuffer);

    return Status;
#endif
} // EFI_STATUS ConnectAllDriversToAllControllers()

// DA-TAG: Exclude TianoCore - START
#ifndef __MAKEWITH_TIANO
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
VOID ConnectFilesystemDriver (
    EFI_HANDLE DriverHandle
) {
    EFI_STATUS                             Status;
    UINTN                                  HandleCount;
    UINTN                                  Index;
    UINTN                                  OpenInfoCount;
    UINTN                                  OpenInfoIndex;
    EFI_HANDLE                             DriverHandleList[2];
    EFI_HANDLE                            *Handles;
    MY_EFI_BLOCK_IO_PROTOCOL              *BlockIo;
    MY_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL    *Fs;
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY   *OpenInfo;

    // Get all DiskIo handles
    Handles = NULL;
    HandleCount = 0;
    Status = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, ByProtocol,
        &gEfiDiskIoProtocolGuid, NULL,
        &HandleCount, &Handles
    );

    if (EFI_ERROR(Status) || HandleCount == 0) {
        return;
    }

    // Check every DiskIo handle
    for (Index = 0; Index < HandleCount; Index++) {
        // If this is not partition - skip it.
        // This is then whole disk and DiskIo
        // should be opened here BY_DRIVER by Partition driver
        // to produce partition volumes.
        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, Handles[Index],
            &gEfiBlockIoProtocolGuid, (VOID **) &BlockIo
        );
        if (EFI_ERROR(Status)) {
            continue;
        }

        if (BlockIo->Media == NULL || !BlockIo->Media->LogicalPartition) {
            continue;
        }

        // If SimpleFileSystem is already produced - skip it, this is ok
        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, Handles[Index],
            &gEfiSimpleFileSystemProtocolGuid, (VOID **) &Fs
        );
        if (Status == EFI_SUCCESS) {
            continue;
        }

        // If no SimpleFileSystem on this handle but DiskIo is opened BY_DRIVER
        // then disconnect this connection and try to connect our driver to it
        Status = REFIT_CALL_4_WRAPPER(
            gBS->OpenProtocolInformation, Handles[Index],
            &gEfiDiskIoProtocolGuid, &OpenInfo, &OpenInfoCount
        );
        if (EFI_ERROR(Status)) {
            continue;
        }

        DriverHandleList[1] = NULL;
        for (OpenInfoIndex = 0; OpenInfoIndex < OpenInfoCount; OpenInfoIndex++) {
            if ((OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER)
                == EFI_OPEN_PROTOCOL_BY_DRIVER
            ) {
                Status = REFIT_CALL_3_WRAPPER(
                    gBS->DisconnectController, Handles[Index],
                    OpenInfo[OpenInfoIndex].AgentHandle, NULL
                );
                if (!EFI_ERROR(Status)) {
                    DriverHandleList[0] = DriverHandle;
                    REFIT_CALL_4_WRAPPER(
                        gBS->ConnectController, Handles[Index],
                        DriverHandleList, NULL, FALSE
                    );
                }
            }
        } // for

        MY_FREE_POOL(OpenInfo);
    }

    MY_FREE_POOL(Handles);
} // VOID ConnectFilesystemDriver()
#endif
// DA-TAG: Exclude TianoCore - END

// Scan a directory for drivers.
// Originally from rEFIt's main.c (BSD), but modified since then (GPLv3).
static
UINTN ScanDriverDir (
    IN  CHAR16      *Path,
    OUT EFI_HANDLE **DriversList
) {
    EFI_STATUS                     Status;
    EFI_STATUS                     XStatus;
    EFI_GUID                     **ProtocolGuidArray;
    BOOLEAN                        DriverBindingFlag;
    BOOLEAN                        FirstLoop;
    CHAR16                        *FileName;
    CHAR16                        *ErrMsg;
    UINTN                          NumFound;
    UINTN                          DriversArrSizeNew;
    UINTN                          DriversArrSize;
    UINTN                          DriversArrNum;
    UINTN                          ProtocolIndex;
    UINTN                          ArrayCount;
    EFI_HANDLE                    *DriversArr;
    EFI_HANDLE                     DriverHandle;
    EFI_FILE_INFO                 *DirEntry;
    REFIT_DIR_ITER                 DirIter;

    CleanUpPathNameSlashes (Path);

    #if REFIT_DEBUG > 0
    BRK_MOD("\n");
    LOG_MSG("Scan '%s' Folder:", Path);
    #endif

    // Look through contents of the directory
    DirIterOpen (SelfRootDir, Path, &DirIter);

    DriversArr = NULL;
    FirstLoop = TRUE;
    ArrayCount = ProtocolIndex = DriversArrNum = NumFound = 0;
    DriversArrSize = DriversArrSizeNew = 16;
    while (DirIterNext (&DirIter, 2, LOADER_MATCH_PATTERNS, &DirEntry)) {
        if (DirEntry->FileName[0] == '.') {
            // Skip this
            MY_FREE_POOL(DirEntry);
            continue;
        }

        DriverBindingFlag = FALSE;

        NumFound++;
        FileName = PoolPrint (L"%s\\%s", Path, DirEntry->FileName);

        Status = StartEFIImage (
            SelfVolume, FileName, L"",
            DirEntry->FileName, 0,
            FALSE, TRUE, &DriverHandle
        );

        MY_FREE_POOL(DirEntry);

        if (DriverHandle != NULL) {
            // Driver Loaded - Check for EFI_DRIVER_BINDING_PROTOCOL
            XStatus = REFIT_CALL_3_WRAPPER(
                gBS->ProtocolsPerHandle, DriverHandle,
                &ProtocolGuidArray, &ArrayCount
            );
            if (!EFI_ERROR(XStatus)) {
                for (ProtocolIndex = 0; ProtocolIndex < ArrayCount; ProtocolIndex++) {
                    if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDriverBindingProtocolGuid)) {
                        DriverBindingFlag = TRUE;
                        break;
                    }
                } // for

                if (DriverBindingFlag) {
                    if (FirstLoop) {
                        FirstLoop = FALSE;
                        // New Array
                        DriversArr = AllocatePool (
                            sizeof (EFI_HANDLE) * DriversArrSize
                        );
                    }
                    else {
                        // Extend Array
                        DriversArrSizeNew += 16;
                        DriversArr = ReallocatePool (
                            DriversArrSize,
                            DriversArrSizeNew,
                            DriversArr
                        );
                        DriversArrSize = DriversArrSizeNew;
                    }

                    DriversArr[DriversArrNum] = DriverHandle;
                    DriversArrNum++;
                    DriversArr[DriversArrNum] = NULL; // Terminate Array
                }

                MY_FREE_POOL(ProtocolGuidArray);
            } // if !EFI_ERROR Status
        }

        #if REFIT_DEBUG > 0
        LOG_MSG(
            "%s  - %r ... UEFI Driver:- '%s'",
            (GlobalConfig.LogLevel <= MAXLOGLEVEL)
                ? OffsetNext
                : L"",
            Status, FileName
        );
        BRK_MAX("\n");
        #endif

        MY_FREE_POOL(FileName);
    } // while

    Status = DirIterClose (&DirIter);
    if (Status != EFI_NOT_FOUND) {
        ErrMsg = PoolPrint (L"While Scanning the '%s' Directory", Path);
        CheckError (Status, ErrMsg);
        MY_FREE_POOL(ErrMsg);
    }

    *DriversList = DriversArr;

    return (NumFound);
} // static UINTN ScanDriverDir()


// Load all UEFI drivers from RefindPlus' "drivers" subdirectory and from the
// directories specified by the user in the "scan_driver_dirs" configuration
// file line.
// Originally from rEFIt's main.c (BSD), but modified since then (GPLv3).
// Returns TRUE if any drivers are loaded, FALSE otherwise.
BOOLEAN LoadDrivers (VOID) {
    CHAR16      *Directory;
    CHAR16      *SelfDirectory;
    UINTN        i;
    UINTN        k;
    UINTN        NumFound;
    UINTN        CurFound;
    EFI_HANDLE  *DriversListProg;
    EFI_HANDLE  *DriversListUser;
    EFI_HANDLE  *DriversListAll;

#ifdef __MAKEWITH_TIANO
// DA-TAG: Limit to TianoCore
    UINTN        HandleSize;
    UINTN        DriversIndex;
#endif

    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr;
    CHAR16  *MsgNotFound;

    ALT_LOG(1, LOG_LINE_SEPARATOR, L"Load UEFI Drivers");
    #endif

    // Load drivers from the subdirectories of RefindPlus' home directory
    // specified in the DRIVER_DIRS constant.
    #if REFIT_DEBUG > 0
    LOG_MSG("\n\n");
    LOG_MSG("L O A D   U E F I   D R I V E R S   :::::   P R O G R A M   D E F A U L T   F O L D E R");
#if REFIT_DEBUG > 1
    LOG_MSG("\n");
#endif
    #endif


    SelfDirectory = NULL;
    DriversListProg = DriversListUser = DriversListAll = NULL;
    NumFound = CurFound = i = 0;

    #if REFIT_DEBUG > 0
    MsgNotFound = L"Not Found or Empty";
    #endif

    while ((CurFound == 0) && (Directory = FindCommaDelimited (DRIVER_DIRS, i++)) != NULL) {
        CleanUpPathNameSlashes (Directory);
        if (SelfDirPath) {
            SelfDirectory = StrDuplicate (SelfDirPath);
        }
        CleanUpPathNameSlashes (SelfDirectory);
        MergeStrings (&SelfDirectory, Directory, L'\\');

        CurFound = ScanDriverDir (SelfDirectory, &DriversListProg);
        if (CurFound > 0) {
            // We only process one default folder
            // Increment 'NumFound' and exit loop if drivers were found
            NumFound = NumFound + CurFound;
        }
        else {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_LINE_NORMAL,
                L"'%s' ... Program Default Driver Folder:- '%s'",
                MsgNotFound, SelfDirectory
            );
            LOG_MSG(
                "%s  - %s",
                (GlobalConfig.LogLevel <= MAXLOGLEVEL)
                    ? OffsetNext
                    : L"",
                MsgNotFound
            );
            #endif
        }

        MY_FREE_POOL(Directory);
        MY_FREE_POOL(SelfDirectory);
    } // while

    // Scan additional user-specified driver directories.
    if (GlobalConfig.DriverDirs != NULL) {
        #if REFIT_DEBUG > 0
        LOG_MSG("\n\n");
        LOG_MSG("L O A D   U E F I   D R I V E R S   :::::   U S E R   D E F I N E D   F O L D E R S");
        BRK_MAX("\n");
        #endif

        i = 0;
        while ((Directory = FindCommaDelimited (GlobalConfig.DriverDirs, i++)) != NULL) {
            CleanUpPathNameSlashes (Directory);
            if (StrLen (Directory) > 0) {
                if (SelfDirPath) {
                    SelfDirectory = StrDuplicate (SelfDirPath);
                }
                CleanUpPathNameSlashes (SelfDirectory);

                if (MyStrBegins (SelfDirectory, Directory)) {
                    MY_FREE_POOL(SelfDirectory);
                    SelfDirectory = StrDuplicate (Directory);
                }
                else {
                    MergeStrings (&SelfDirectory, Directory, L'\\');
                }

                CurFound = ScanDriverDir (SelfDirectory, &DriversListUser);
                if (CurFound > 0) {
                    NumFound = NumFound + CurFound;
                }
                else {
                    #if REFIT_DEBUG > 0
                    ALT_LOG(1, LOG_LINE_NORMAL,
                        L"'%s' ... User Defined Driver Folder:- '%s'",
                        MsgNotFound, SelfDirectory
                    );
                    LOG_MSG(
                        "%s  - %s",
                        (GlobalConfig.LogLevel <= MAXLOGLEVEL)
                            ? OffsetNext
                            : L"",
                        MsgNotFound
                    );
                    #endif
                }
            }

            MY_FREE_POOL(Directory);
            MY_FREE_POOL(SelfDirectory);
        } // while
    }

    #if REFIT_DEBUG > 0
    LOG_MSG("\n\n");
    #endif

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"Processed %d UEFI Driver%s",
        NumFound, (NumFound == 1) ? L"" : L"s"
    );
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

#ifdef __MAKEWITH_TIANO
// DA-TAG: Limit to TianoCore
    DriversIndex = 0;
    if (NumFound > 0 && GlobalConfig.RansomDrives) {
        HandleSize = sizeof (EFI_HANDLE) * 16;
        /* Program Default Folders - START */
        if (DriversListProg) {
            i = 0;
            while (DriversListProg[i] != NULL) {
                ++i;
            } // while

            if (i > 0) {
                DriversListAll = AllocatePool (
                    HandleSize * i
                );
            }

            for (k = 0; k < i; k++) {
                if (!DriversListProg[k]) {
                    // Safety valve to exclude NULL
                    // NULL Termination Added Later
                    continue;
                }
                DriversListAll[DriversIndex] = DriversListProg[k],
                ++DriversIndex;
            } // for
        }
        /* Program Default Folders - END */

        /* User Defined Folders - START */
        if (DriversListUser) {
            i = 0;
            while (DriversListUser[i] != NULL) {
                ++i;
            } // while

            if (DriversIndex == 0) {
                // New Array
                DriversListAll = AllocatePool (
                    HandleSize * i
                );
            }
            else {
                // Extend Array
                DriversListAll = ReallocatePool (
                    HandleSize * DriversIndex,
                    HandleSize * (DriversIndex + i),
                    DriversListAll
                );
            }

            for (k = 0; k < i; k++) {
                if (!DriversListUser[k]) {
                    // Safety valve to exclude NULL
                    // NULL Termination Added Later
                    continue;
                }
                DriversListAll[DriversIndex] = DriversListUser[k],
                ++DriversIndex;
            } // for
        }
        /* User Defined Folders - END */

        if (DriversListAll) {
            DriversListAll[DriversIndex] = NULL; // Terminate Array

            // DA-TAG: Do not free 'DriversListXYZ'
            OcRegisterDriversToHighestPriority (DriversListAll);
        }
    } // if NumFound
#endif

    // Connect Devices
    // DA-TAG: Always run this
    ConnectAllDriversToAllControllers (TRUE);

    return (NumFound > 0);
} // BOOLEAN LoadDrivers()
