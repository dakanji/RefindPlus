/*
 * File to implement LibScanHandleDatabase(), which is used by rEFInd's
 * driver-loading code (inherited from rEFIt), but which has not been
 * implemented in GNU-EFI and seems to have been dropped from current
 * versions of the Tianocore library. This function was taken from a git
 * site with EFI code. The original file bore the following copyright
 * notice:
 *
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

#ifndef _DRIVER_SUPPORT
#define _DRIVER_SUPPORT

#ifdef __MAKEWITH_GNUEFI
#include <efi.h>
#else
#include "../include/tiano_includes.h"
#endif
#include "global.h"

#define EFI_HANDLE_TYPE_UNKNOWN                     0x000
#define EFI_HANDLE_TYPE_IMAGE_HANDLE                0x001
#define EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE       0x002
#define EFI_HANDLE_TYPE_DEVICE_DRIVER               0x004
#define EFI_HANDLE_TYPE_BUS_DRIVER                  0x008
#define EFI_HANDLE_TYPE_DRIVER_CONFIGURATION_HANDLE 0x010
#define EFI_HANDLE_TYPE_DRIVER_DIAGNOSTICS_HANDLE   0x020
#define EFI_HANDLE_TYPE_COMPONENT_NAME_HANDLE       0x040
#define EFI_HANDLE_TYPE_DEVICE_HANDLE               0x080
#define EFI_HANDLE_TYPE_PARENT_HANDLE               0x100
#define EFI_HANDLE_TYPE_CONTROLLER_HANDLE           0x200
#define EFI_HANDLE_TYPE_CHILD_HANDLE                0x400

// Below is from http://git.etherboot.org/?p=mirror/efi/shell/.git;a=commitdiff;h=b1b0c63423cac54dc964c2930e04aebb46a946ec;
// Seems to have been replaced by ParseHandleDatabaseByRelationshipWithType(), but the latter isn't working for me....
EFI_STATUS
LibScanHandleDatabase (
  EFI_HANDLE  DriverBindingHandle, OPTIONAL
  UINT32      *DriverBindingHandleIndex, OPTIONAL
  EFI_HANDLE  ControllerHandle, OPTIONAL
  UINT32      *ControllerHandleIndex, OPTIONAL
  UINTN       *HandleCount,
  EFI_HANDLE  **HandleBuffer,
  UINT32      **HandleType
  );
EFI_STATUS ConnectAllDriversToAllControllers(VOID);
VOID ConnectFilesystemDriver(EFI_HANDLE DriverHandle);
BOOLEAN LoadDrivers(VOID);

#endif
