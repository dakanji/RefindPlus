/*
 * refind/gpt.h
 * Functions related to GPT data structures
 *
 * Copyright (c) 2014-2015 Roderick W. Smith
 * All rights reserved.
 *
 * This program is distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), a copy of which must be distributed
 * with this source code or binaries made from it.
 *
 */

#include "global.h"

#ifndef __GPT_H_
#define __GPT_H_

#ifdef __MAKEWITH_GNUEFI
#include "efi.h"
#include "efilib.h"
#else
#include "../include/tiano_includes.h"
#endif

#pragma pack(1)
typedef struct {
   UINT8   flags;
   UINT8   start_chs[3];
   UINT8   type;
   UINT8   end_chs[3];
   UINT32  start_lba;
   UINT32  size;
} MBR_PART_INFO;

// A 512-byte data structure into which the MBR can be loaded in one
// go. Also used when loading logical partitions.

typedef struct {
   UINT8            code[440];
   UINT32           diskSignature;
   UINT16           nulls;
   MBR_PART_INFO    partitions[4];
   UINT16           MBRSignature;
} MBR_RECORD;

typedef struct {
   UINT64  signature;
   UINT32  spec_revision;
   UINT32  header_size;
   UINT32  header_crc32;
   UINT32  reserved;
   UINT64  header_lba;
   UINT64  alternate_header_lba;
   UINT64  first_usable_lba;
   UINT64  last_usable_lba;
   UINT8   disk_guid[16];
   UINT64  entry_lba;
   UINT32  entry_count;
   UINT32  entry_size;
   UINT32  entry_crc32;
   UINT8   reserved2[420];
} GPT_HEADER;

typedef struct {
   UINT8   type_guid[16];
   UINT8   partition_guid[16];
   UINT64  start_lba;
   UINT64  end_lba;
   UINT64  attributes;
   CHAR16  name[36];
} GPT_ENTRY;

typedef struct _gpt_data {
   MBR_RECORD         *ProtectiveMBR;
   GPT_HEADER         *Header;
   GPT_ENTRY          *Entries;
   struct _gpt_data   *NextEntry;
} GPT_DATA;

#pragma pack(0)

VOID ClearGptData(GPT_DATA *Data);
EFI_STATUS ReadGptData(REFIT_VOLUME *Volume, GPT_DATA **Data);
// CHAR16 * PartNameFromGuid(EFI_GUID *Guid);
GPT_ENTRY * FindPartWithGuid(EFI_GUID *Guid);
VOID ForgetPartitionTables(VOID);
VOID AddPartitionTable(REFIT_VOLUME *Volume);

#endif