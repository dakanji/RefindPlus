/*
 * EfiLib/legacy.h
 * CSM/legacy boot support functions
 * 
 * Taken from Tianocore source code (mostly IntelFrameworkModulePkg/Universal/BdsDxe/BootMaint/BBSsupport.c)
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

#include "LegacyBios.h"

#ifndef __LEGACY_H_
#define __LEGACY_H_

#define BBS_MEDIA_PRESENT        0x0800
#define BBS_MEDIA_MAYBE_PRESENT  0x0400

typedef UINT8 BBS_TYPE;

#define VAR_FLAG  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE

#pragma pack(1)
///
/// For each legacy boot option in BBS table, a corresponding Boot#### variables is created.
/// The structure saves the mapping relationship between #### and the index in the BBS table.
///
typedef struct {
   UINT16    BootOptionNumber;
   UINT16    BbsIndex;
   UINT16    BbsType;
} BOOT_OPTION_BBS_MAPPING;
#pragma pack()

#pragma pack(1)
typedef struct {
   BBS_TYPE  BbsType;
   ///
   /// Length = sizeof (UINT16) + sizeof (Data)
   ///
   UINT16    Length;
   UINT16    Data[1];
} LEGACY_DEV_ORDER_ENTRY;
#pragma pack()

EFI_STATUS
BdsAddNonExistingLegacyBootOptions (
   VOID
);

/**
  Delete all the invalid legacy boot options.

  @retval EFI_SUCCESS             All invalide legacy boot options are deleted.
  @retval EFI_OUT_OF_RESOURCES    Fail to allocate necessary memory.
  @retval EFI_NOT_FOUND           Fail to retrive variable of boot order.
**/
EFI_STATUS
BdsDeleteAllInvalidLegacyBootOptions (
  VOID
  );

BOOLEAN
BdsIsLegacyBootOption (
   IN UINT8                 *BootOptionVar,
   OUT BBS_TABLE            **BbsEntry,
   OUT UINT16               *BbsIndex
);

VOID
BdsBuildLegacyDevNameString (
   IN  BBS_TABLE                 *CurBBSEntry,
   IN  UINTN                     Index,
   IN  UINTN                     BufSize,
   OUT CHAR16                    *BootString
);

#endif