/*
 * EfiLib/BdsHelper.c
 * Functions to call legacy BIOS API.
 *
 */
/**

Copyright (c) 2004 - 2014, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifdef __MAKEWITH_TIANO
#include "../include/tiano_includes.h"
#else
#include "gnuefi-helper.h"
#include "GenericBdsLib.h"
#endif

#ifndef _BDS_HELPER_H_
#define _BDS_HELPER_H_


/**
  Boot the legacy system with the boot option

  @param  Option                 The legacy boot option which have BBS device path

  @retval EFI_UNSUPPORTED        There is no legacybios protocol, do not support
                                 legacy boot.
  @retval EFI_STATUS             Return the status of LegacyBios->LegacyBoot ().

**/
EFI_STATUS
BdsLibDoLegacyBoot (
  IN  BDS_COMMON_OPTION           *Option
  );

EFI_STATUS BdsConnectDevicePath  (  IN EFI_DEVICE_PATH_PROTOCOL *    DevicePath,
                                    OUT EFI_HANDLE *     Handle,
                                    OUT EFI_DEVICE_PATH_PROTOCOL **     RemainingDevicePath
);

#endif //_BDS_HELPER_H_
