/*
 * EfiLib/gnuefi-helper.h
 * Header file for GNU-EFI support in legacy boot code
 *
 * Borrowed from the TianoCore EDK II, with modifications by Rod Smith
 *
 * Copyright (c) 2004 - 2011, Intel Corporation. All rights reserved.<BR>
 * This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the BSD License
 * which accompanies this distribution.  The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php
 * 
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 *
 */
/*
 * THIS FILE SHOULD NOT BE INCLUDED WHEN COMPILING UNDER TIANOCORE'S TOOLKIT!
 */

#ifndef __EFILIB_GNUEFI_H
#define __EFILIB_GNUEFI_H

#include "efi.h"
#include "efilib.h"

#define EFI_DEVICE_PATH_PROTOCOL EFI_DEVICE_PATH
#define gRT RT
#define gBS BS
#ifndef CONST
#define CONST
#endif
#define ASSERT_EFI_ERROR(status)  ASSERT(!EFI_ERROR(status))

CHAR8 *
UnicodeStrToAsciiStr (
   IN       CHAR16              *Source,
   OUT      CHAR8               *Destination
);

UINTN
AsciiStrLen (
   IN      CONST CHAR8               *String
);

UINTN
EFIAPI
GetDevicePathSize (
   IN CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
);

EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
GetNextDevicePathInstance (
   IN OUT EFI_DEVICE_PATH_PROTOCOL    **DevicePath,
   OUT UINTN                          *Size
);

#endif
