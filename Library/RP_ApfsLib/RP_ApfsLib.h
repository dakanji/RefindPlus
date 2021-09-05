/** @file
Copyright (C) 2020, vit9696. All rights reserved.

Modified 2021, Dayo Akanji. (sf.net/u/dakanji/profile)

  All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef RP_APFS_LIB_H
#define RP_APFS_LIB_H

/**
  Connect APFS driver to partitions on media handle.

  @param[in] Handle   Media handle (disk).

  @retval EFI_SUCCESS if the device was connected.
**/
EFI_STATUS RP_ApfsConnectParentDevice (
  VOID
  );

/**
  Connect APFS driver to a device at handle.

  @param[in] Handle        Device handle (APFS container).

  @retval EFI_SUCCESS if the device was connected.
**/
EFI_STATUS RP_ApfsConnectHandle (
  IN EFI_HANDLE  Handle
  );

/**
  Connect APFS driver to all present devices.

  @param[in] Monitor   Setup monitoring for newly connected devices.

  @retval EFI_SUCCESS if at least one device was connected.
**/
EFI_STATUS RP_ApfsConnectDevices (
  VOID
  );

#endif // RP_APFS_LIB_H
