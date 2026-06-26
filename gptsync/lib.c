/*
 * gptsync/lib.c
 * Platform-independent code common to gptsync and showpart
 *
 * Copyright (c) 2006-2007 Christoph Pfisterer
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
 * Modified for RefindPlus
 * Copyright (c) 2025 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "gptsync.h"

// variables

UINT8           empty_guid[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

PARTITION_INFO  mbr_parts[4];
UINTN           mbr_part_count = 0;
PARTITION_INFO  gpt_parts[128];
UINTN           gpt_part_count = 0;

PARTITION_INFO  new_mbr_parts[4];
UINTN           new_mbr_part_count = 0;

UINT8           sector[512];

MBR_PARTTYPE    mbr_types[] = {
    { 0x01, STR("FAT12 (CHS)") },
    { 0x04, STR("FAT16 <32M (CHS)") },
    { 0x05, STR("Extended (CHS)") },
    { 0x06, STR("FAT16 (CHS)") },
    { 0x07, STR("NTFS/HPFS") },
    { 0x0b, STR("FAT32 (CHS)") },
    { 0x0c, STR("FAT32 (LBA)") },
    { 0x0e, STR("FAT16 (LBA)") },
    { 0x0f, STR("Extended (LBA)") },
    { 0x11, STR("Hidden FAT12 (CHS)") },
    { 0x14, STR("Hidden FAT16 <32M (CHS)") },
    { 0x16, STR("Hidden FAT16 (CHS)") },
    { 0x17, STR("Hidden NTFS/HPFS") },
    { 0x1b, STR("Hidden FAT32 (CHS)") },
    { 0x1c, STR("Hidden FAT32 (LBA)") },
    { 0x1e, STR("Hidden FAT16 (LBA)") },
    { 0x82, STR("Linux swap / Solaris") },
    { 0x83, STR("Linux") },
    { 0x85, STR("Linux Extended") },
    { 0x86, STR("NT FAT volume set") },
    { 0x87, STR("NTFS volume set") },
    { 0x8e, STR("Linux LVM") },
    { 0xa5, STR("FreeBSD") },
    { 0xa6, STR("OpenBSD") },
    { 0xa7, STR("NeXTSTEP") },
    { 0xa8, STR("MacOS UFS") },
    { 0xa9, STR("NetBSD") },
    { 0xab, STR("MacOS Boot") },
    { 0xac, STR("Apple RAID") },
    { 0xaf, STR("MacOS HFS+") },
    { 0xbe, STR("Solaris Boot") },
    { 0xbf, STR("Solaris") },
    { 0xeb, STR("BeOS") },
    { 0xee, STR("EFI Protective") },
    { 0xef, STR("EFI System (FAT)") },
    { 0xfd, STR("Linux RAID") },
    { 0, NULL },
};

GPT_PARTTYPE    gpt_types[] = {
    // Sony uses this one
    { { 0x32, 0x97, 0x01, 0xF4, 0x6E, 0x06, 0x12, 0x4E, 0x82, 0x73, 0x34, 0x6C, 0x56, 0x41, 0x49, 0x4F }, 0x00, STR("Sony System (FAT)"), GPT_KIND_FATAL },
    // Defined by UEFI specification
    { { 0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B }, 0xef, STR("EFI System (FAT)"), GPT_KIND_SYSTEM },
    { { 0x41, 0xEE, 0x4D, 0x02, 0xE7, 0x33, 0xD3, 0x11, 0x9D, 0x69, 0x00, 0x08, 0xC7, 0x81, 0xF3, 0x9F }, 0x00, STR("MBR partition scheme"), GPT_KIND_FATAL },
    // Generally well-known
    { { 0x16, 0xE3, 0xC9, 0xE3, 0x5C, 0x0B, 0xB8, 0x4D, 0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE }, 0x00, STR("MS Reserved"), GPT_KIND_SYSTEM },
    { { 0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 }, 0x00, STR("Basic Data"), GPT_KIND_BASIC_DATA },
    // From Wikipedia
    { { 0xAA, 0xC8, 0x08, 0x58, 0x8F, 0x7E, 0xE0, 0x42, 0x85, 0xD2, 0xE1, 0xE9, 0x04, 0x34, 0xCF, 0xB3 }, 0x00, STR("MS LDM Metadata"), GPT_KIND_FATAL },
    { { 0xA0, 0x60, 0x9B, 0xAF, 0x31, 0x14, 0x62, 0x4F, 0xBC, 0x68, 0x33, 0x11, 0x71, 0x4A, 0x69, 0xAD }, 0x00, STR("MS LDM Data"), GPT_KIND_FATAL },
    { { 0x1E, 0x4C, 0x89, 0x75, 0xEB, 0x3A, 0xD3, 0x11, 0xB7, 0xC1, 0x7B, 0x03, 0xA0, 0x00, 0x00, 0x00 }, 0x00, STR("HP/UX Data"), GPT_KIND_DATA },
    { { 0x28, 0xE7, 0xA1, 0xE2, 0xE3, 0x32, 0xD6, 0x11, 0xA6, 0x82, 0x7B, 0x03, 0xA0, 0x00, 0x00, 0x00 }, 0x00, STR("HP/UX Service"), GPT_KIND_SYSTEM },
    // From Linux repository, fs/partitions/efi.h
    { { 0x0F, 0x88, 0x9D, 0xA1, 0xFC, 0x05, 0x3B, 0x4D, 0xA0, 0x06, 0x74, 0x3F, 0x0F, 0x84, 0x91, 0x1E }, 0xfd, STR("Linux RAID"), GPT_KIND_DATA },
    { { 0x6D, 0xFD, 0x57, 0x06, 0xAB, 0xA4, 0xC4, 0x43, 0x84, 0xE5, 0x09, 0x33, 0xC8, 0x4B, 0x4F, 0x4F }, 0x82, STR("Linux Swap"), GPT_KIND_SYSTEM },
    { { 0x79, 0xD3, 0xD6, 0xE6, 0x07, 0xF5, 0xC2, 0x44, 0xA2, 0x3C, 0x23, 0x8F, 0x2A, 0x3D, 0xF9, 0x28 }, 0x8e, STR("Linux LVM"), GPT_KIND_DATA },
    { { 0xAF, 0x3D, 0xC6, 0x0F, 0x83, 0x84, 0x72, 0x47, 0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4 }, 0x83, STR("Linux Filesystem"), GPT_KIND_DATA },
    // From Wikipedia
    { { 0x39, 0x33, 0xA6, 0x8D, 0x07, 0x00, 0xC0, 0x60, 0xC4, 0x36, 0x08, 0x3A, 0xC8, 0x23, 0x09, 0x08 }, 0x00, STR("Linux Reserved"), GPT_KIND_SYSTEM },
    // From grub2 repository, grub/include/grub/gpt_partition.h
    { { 0x48, 0x61, 0x68, 0x21, 0x49, 0x64, 0x6F, 0x6E, 0x74, 0x4E, 0x65, 0x65, 0x64, 0x45, 0x46, 0x49 }, 0x00, STR("GRUB2 BIOS Boot"), GPT_KIND_SYSTEM },
    // From FreeBSD repository, sys/sys/gpt.h
    { { 0xB4, 0x7C, 0x6E, 0x51, 0xCF, 0x6E, 0xD6, 0x11, 0x8F, 0xF8, 0x00, 0x02, 0x2D, 0x09, 0x71, 0x2B }, 0xa5, STR("FreeBSD Data"), GPT_KIND_DATA },
    { { 0xB5, 0x7C, 0x6E, 0x51, 0xCF, 0x6E, 0xD6, 0x11, 0x8F, 0xF8, 0x00, 0x02, 0x2D, 0x09, 0x71, 0x2B }, 0x00, STR("FreeBSD Swap"), GPT_KIND_SYSTEM },
    { { 0xB6, 0x7C, 0x6E, 0x51, 0xCF, 0x6E, 0xD6, 0x11, 0x8F, 0xF8, 0x00, 0x02, 0x2D, 0x09, 0x71, 0x2B }, 0xa5, STR("FreeBSD UFS"), GPT_KIND_DATA },
    { { 0xB8, 0x7C, 0x6E, 0x51, 0xCF, 0x6E, 0xD6, 0x11, 0x8F, 0xF8, 0x00, 0x02, 0x2D, 0x09, 0x71, 0x2B }, 0x00, STR("FreeBSD Vinum"), GPT_KIND_DATA },
    { { 0xBA, 0x7C, 0x6E, 0x51, 0xCF, 0x6E, 0xD6, 0x11, 0x8F, 0xF8, 0x00, 0x02, 0x2D, 0x09, 0x71, 0x2B }, 0xa5, STR("FreeBSD ZFS"), GPT_KIND_DATA },
    { { 0x9D, 0x6B, 0xBD, 0x83, 0x41, 0x7F, 0xDC, 0x11, 0xBE, 0x0B, 0x00, 0x15, 0x60, 0xB8, 0x4F, 0x0F }, 0xa5, STR("FreeBSD Boot"), GPT_KIND_DATA },
    // From NetBSD repository, sys/sys/disklabel_gpt.h
    { { 0x32, 0x8D, 0xF4, 0x49, 0x0E, 0xB1, 0xDC, 0x11, 0xB9, 0x9B, 0x00, 0x19, 0xD1, 0x87, 0x96, 0x48 }, 0x00, STR("NetBSD Swap"), GPT_KIND_SYSTEM },
    { { 0x5A, 0x8D, 0xF4, 0x49, 0x0E, 0xB1, 0xDC, 0x11, 0xB9, 0x9B, 0x00, 0x19, 0xD1, 0x87, 0x96, 0x48 }, 0xa9, STR("NetBSD FFS"), GPT_KIND_DATA },
    { { 0x82, 0x8D, 0xF4, 0x49, 0x0E, 0xB1, 0xDC, 0x11, 0xB9, 0x9B, 0x00, 0x19, 0xD1, 0x87, 0x96, 0x48 }, 0xa9, STR("NetBSD LFS"), GPT_KIND_DATA },
    { { 0xAA, 0x8D, 0xF4, 0x49, 0x0E, 0xB1, 0xDC, 0x11, 0xB9, 0x9B, 0x00, 0x19, 0xD1, 0x87, 0x96, 0x48 }, 0xa9, STR("NetBSD RAID"), GPT_KIND_DATA },
    { { 0xC4, 0x19, 0xB5, 0x2D, 0x0E, 0xB1, 0xDC, 0x11, 0xB9, 0x9B, 0x00, 0x19, 0xD1, 0x87, 0x96, 0x48 }, 0xa9, STR("NetBSD CCD"), GPT_KIND_DATA },
    { { 0xEC, 0x19, 0xB5, 0x2D, 0x0E, 0xB1, 0xDC, 0x11, 0xB9, 0x9B, 0x00, 0x19, 0xD1, 0x87, 0x96, 0x48 }, 0xa9, STR("NetBSD CGD"), GPT_KIND_DATA },
    // From http://developer.apple.com/mac/library/technotes/tn2006/tn2166.html
    //{ { 0x00, 0x53, 0x46, 0x48, 0x00, 0x00, 0xAA, 0x11, 0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC }, 0x00, STR("MacOS HFS+"), GPT_KIND_SYSTEM },
    { { 0x00, 0x53, 0x46, 0x48, 0x00, 0x00, 0xAA, 0x11, 0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC }, 0xaf, STR("MacOS HFS+"), GPT_KIND_DATA },
    { { 0x72, 0x6F, 0x74, 0x53, 0x67, 0x61, 0xAA, 0x11, 0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC }, 0xaf, STR("MacOS Core Storage"), GPT_KIND_DATA },
    { { 0x00, 0x53, 0x46, 0x55, 0x00, 0x00, 0xAA, 0x11, 0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC }, 0xa8, STR("MacOS UFS"), GPT_KIND_DATA },
    { { 0x74, 0x6F, 0x6F, 0x42, 0x00, 0x00, 0xAA, 0x11, 0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC }, 0xab, STR("MacOS Boot"), GPT_KIND_DATA },
    { { 0x44, 0x49, 0x41, 0x52, 0x00, 0x00, 0xAA, 0x11, 0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC }, 0xac, STR("Apple RAID"), GPT_KIND_DATA },
    { { 0x44, 0x49, 0x41, 0x52, 0x4F, 0x5F, 0xAA, 0x11, 0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC }, 0xac, STR("Apple RAID (Offline)"), GPT_KIND_DATA },
    { { 0x65, 0x62, 0x61, 0x4C, 0x00, 0x6C, 0xAA, 0x11, 0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC }, 0x00, STR("Apple Label"), GPT_KIND_SYSTEM },
    // From Wikipedia
    { { 0x6F, 0x63, 0x65, 0x52, 0x65, 0x76, 0xAA, 0x11, 0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC }, 0x00, STR("Apple TV Recovery"), GPT_KIND_DATA },
    // From OpenSolaris repository, usr/src/uts/common/sys/efi_partition.h
    { { 0x7f, 0x23, 0x96, 0x6A, 0xD2, 0x1D, 0xB2, 0x11, 0x99, 0xa6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 }, 0x00, STR("Solaris Reserved"), GPT_KIND_SYSTEM },
    { { 0x45, 0xCB, 0x82, 0x6A, 0xD2, 0x1D, 0xB2, 0x11, 0x99, 0xa6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 }, 0xbf, STR("Solaris Boot"), GPT_KIND_DATA },
    { { 0x4D, 0xCF, 0x85, 0x6A, 0xD2, 0x1D, 0xB2, 0x11, 0x99, 0xa6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 }, 0xbf, STR("Solaris Root"), GPT_KIND_DATA },
    { { 0x6F, 0xC4, 0x87, 0x6A, 0xD2, 0x1D, 0xB2, 0x11, 0x99, 0xa6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 }, 0x00, STR("Solaris Swap"), GPT_KIND_SYSTEM },
    { { 0xC3, 0x8C, 0x89, 0x6A, 0xD2, 0x1D, 0xB2, 0x11, 0x99, 0xa6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 }, 0xbf, STR("Solaris Usr / Apple ZFS"), GPT_KIND_DATA },
    { { 0x2B, 0x64, 0x8B, 0x6A, 0xD2, 0x1D, 0xB2, 0x11, 0x99, 0xa6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 }, 0x00, STR("Solaris Backup"), GPT_KIND_SYSTEM },
    { { 0xC7, 0x2A, 0x8D, 0x6A, 0xD2, 0x1D, 0xB2, 0x11, 0x99, 0xa6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 }, 0x00, STR("Solaris Reserved (Stand)"), GPT_KIND_SYSTEM },
    { { 0xE9, 0xF2, 0x8E, 0x6A, 0xD2, 0x1D, 0xB2, 0x11, 0x99, 0xa6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 }, 0xbf, STR("Solaris Var"), GPT_KIND_DATA },
    { { 0x39, 0xBA, 0x90, 0x6A, 0xD2, 0x1D, 0xB2, 0x11, 0x99, 0xa6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 }, 0xbf, STR("Solaris Home"), GPT_KIND_DATA },
    { { 0xA5, 0x83, 0x92, 0x6A, 0xD2, 0x1D, 0xB2, 0x11, 0x99, 0xa6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 }, 0x00, STR("Solaris Alternate Sector"), GPT_KIND_SYSTEM },
    { { 0x3B, 0x5A, 0x94, 0x6A, 0xD2, 0x1D, 0xB2, 0x11, 0x99, 0xa6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 }, 0x00, STR("Solaris Reserved (Cache)"), GPT_KIND_SYSTEM },
    { { 0xD1, 0x30, 0x96, 0x6A, 0xD2, 0x1D, 0xB2, 0x11, 0x99, 0xa6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 }, 0x00, STR("Solaris Reserved"), GPT_KIND_SYSTEM },
    { { 0x67, 0x07, 0x98, 0x6A, 0xD2, 0x1D, 0xB2, 0x11, 0x99, 0xa6, 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 }, 0x00, STR("Solaris Reserved"), GPT_KIND_SYSTEM },
    // List sentinel
    { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }, 0, NULL, 0 },
};
GPT_PARTTYPE    gpt_dummy_type =
    { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }, 0, STR("Unknown"), GPT_KIND_FATAL };

//
// MBR functions
//

CHARN * mbr_parttype_name(UINT8 type)
{
    int i;

    for (i = 0; mbr_types[i].name; i++)
        if (mbr_types[i].type == type)
            return mbr_types[i].name;
    return STR("Unknown");
}

UINTN read_mbr(VOID)
{
    UINTN               status;
    UINTN               i;
    BOOLEAN             used;
    MBR_PART_INFO       *table;

    Print(L"\nCurrent MBR partition table:\n");

    // read MBR data
    status = read_sector(0, sector);
    if (status != 0)
        return status;

    // check for validity
    if (*((UINT16 *)(sector + 510)) != 0xaa55) {
        Print(L" No MBR partition table present!\n");
        return 1;
    }
    table = (MBR_PART_INFO *)(sector + 446);
    for (i = 0; i < 4; i++) {
        if (table[i].flags != 0x00 && table[i].flags != 0x80) {
            Print(L" MBR partition table is invalid!\n");
            return 1;
        }
    }

    // check if used
    used = FALSE;
    for (i = 0; i < 4; i++) {
        if (table[i].start_lba > 0 && table[i].size > 0) {
            used = TRUE;
            break;
        }
    }
    if (!used) {
        Print(L" No partitions defined\n");
        return 0;
    }

    // dump current state & fill internal structures
    Print(L" # A    Start LBA      End LBA  Type\n");
    for (i = 0; i < 4; i++) {
        if (table[i].start_lba == 0 || table[i].size == 0)
            continue;

        mbr_parts[mbr_part_count].index     = i;
        mbr_parts[mbr_part_count].start_lba = (UINT64)table[i].start_lba;
        mbr_parts[mbr_part_count].end_lba   = (UINT64)table[i].start_lba + (UINT64)table[i].size - 1;
        mbr_parts[mbr_part_count].mbr_type  = table[i].type;
        mbr_parts[mbr_part_count].active    = (table[i].flags == 0x80) ? TRUE : FALSE;

        Print(L" %d %s %12lld %12lld  %02x  %s\n",
              mbr_parts[mbr_part_count].index + 1,
              mbr_parts[mbr_part_count].active ? STR("*") : STR(" "),
              mbr_parts[mbr_part_count].start_lba,
              mbr_parts[mbr_part_count].end_lba,
              mbr_parts[mbr_part_count].mbr_type,
              mbr_parttype_name(mbr_parts[mbr_part_count].mbr_type));

        mbr_part_count++;
    }

    return 0;
}

//
// GPT functions
//

GPT_PARTTYPE * gpt_parttype(UINT8 *type_guid)
{
    int i;

    for (i = 0; gpt_types[i].name; i++)
        if (guids_are_equal(gpt_types[i].guid, type_guid))
            return &(gpt_types[i]);
    return &gpt_dummy_type;
}

UINTN read_gpt (VOID) {
    GPT_HEADER *header;
    GPT_ENTRY  *entry;
    UINT64      entry_lba;
    UINTN       i;
    UINTN       status;
    UINTN       offset;
    UINTN       entry_size;
    UINTN       entry_count;


    Print(L"\nCurrent GUID partition table:\n");

    // read GPT header
    status = read_sector(1, sector);
    if (status != 0)
        return status;

    // check signature
    header = (GPT_HEADER *)sector;
    if (header->signature != 0x5452415020494645ULL) {
        Print(L" No GPT partition table present!\n");
        return 0;
    }
    if (header->spec_revision != 0x00010000UL) {
        Print(L" Warning: Unknown GPT spec revision 0x%08x\n", header->spec_revision);
    }
    if ((512 % header->entry_size) > 0 || header->entry_size > 512) {
        Print(L" Error: Invalid GPT entry size (misaligned or more than 512 bytes)\n");
        return 0;
    }

    // read entries
    entry_lba   = header->entry_lba;
    entry_size  = header->entry_size;
    entry_count = header->entry_count;

    for (i = 0; i < entry_count; i++) {
        if (((i * entry_size) % 512) == 0) {
            status = read_sector(entry_lba, sector);
            if (status != 0) return status;

            entry_lba++;
        }

        offset = (i * entry_size) % 512;
        CopyMem (&entry, sector + offset, sizeof (entry));

        if (guids_are_equal(entry->type_guid, empty_guid)) {
            continue;
        }

        if (gpt_part_count == 0) {
            Print(L" #      Start LBA      End LBA  Type\n");
        }

        gpt_parts[gpt_part_count].index     = i;
        gpt_parts[gpt_part_count].start_lba = entry->start_lba;
        gpt_parts[gpt_part_count].end_lba   = entry->end_lba;
        gpt_parts[gpt_part_count].mbr_type  = 0;
        copy_guid(gpt_parts[gpt_part_count].gpt_type, entry->type_guid);
        gpt_parts[gpt_part_count].gpt_parttype = gpt_parttype(gpt_parts[gpt_part_count].gpt_type);
        gpt_parts[gpt_part_count].active    = FALSE;

        Print(L" %d   %12lld %12lld  %s\n",
              gpt_parts[gpt_part_count].index + 1,
              gpt_parts[gpt_part_count].start_lba,
              gpt_parts[gpt_part_count].end_lba,
              gpt_parts[gpt_part_count].gpt_parttype->name);

        gpt_part_count++;
    }
    if (gpt_part_count == 0) {
        Print(L" No partitions defined\n");
        return 0;
    }

    return 0;
}

//
// detect file system type
//

UINTN detect_mbrtype_fs(UINT64 partlba, UINTN *parttype, CHARN **fsname)
{
    UINTN   status;
    UINTN   signature, score;
    UINTN   sectsize, clustersize, reserved, fatcount, dirsize, sectcount, fatsize, clustercount;

    *fsname = STR("Unknown");
    *parttype = 0;

    // READ sector 0 / offset 0K
    status = read_sector(partlba, sector);
    if (status != 0)
        return status;

    // detect XFS
    signature = *((UINT32 *)(sector));
    if (signature == 0x42534658) {
        *parttype = 0x83;
        *fsname = STR("XFS");
        return 0;
    }

    // detect FAT and NTFS
    sectsize = *((UINT16 *)(sector + 11));
    clustersize = sector[13];
    if (sectsize >= 512 && (sectsize & (sectsize - 1)) == 0 &&
        clustersize > 0 && (clustersize & (clustersize - 1)) == 0) {
        // preconditions for both FAT and NTFS are now met

        if (CompareMem(sector + 3, "NTFS    ", 8) == 0) {
            *parttype = 0x07;
            *fsname = STR("NTFS");
            return 0;
        }

        score = 0;
        // boot jump
        if ((sector[0] == 0xEB && sector[2] == 0x90) ||
            sector[0] == 0xE9)
            score++;
        // boot signature
        if (sector[510] == 0x55 && sector[511] == 0xAA)
            score++;
        // reserved sectors
        reserved = *((UINT16 *)(sector + 14));
        if (reserved == 1 || reserved == 32)
            score++;
        // number of FATs
        fatcount = sector[16];
        if (fatcount == 2)
            score++;
        // number of root dir entries
        dirsize = *((UINT16 *)(sector + 17));
        // sector count (16-bit and 32-bit versions)
        sectcount = *((UINT16 *)(sector + 19));
        if (sectcount == 0)
            sectcount = *((UINT32 *)(sector + 32));
        // media byte
        if (sector[21] == 0xF0 || sector[21] >= 0xF8)
            score++;
        // FAT size in sectors
        fatsize = *((UINT16 *)(sector + 22));
        if (fatsize == 0)
            fatsize = *((UINT32 *)(sector + 36));

        // determine FAT type
        dirsize = ((dirsize * 32) + (sectsize - 1)) / sectsize;
        clustercount = sectcount - (reserved + (fatcount * fatsize) + dirsize);
        clustercount /= clustersize;

        if (score >= 3) {
            if (clustercount < 4085) {
                *parttype = 0x01;
                *fsname = STR("FAT12");
            }
            else if (clustercount < 65525) {
                *parttype = 0x0e;
                *fsname = STR("FAT16");
            }
            else {
                *parttype = 0x0c;
                *fsname = STR("FAT32");
            }
            // TODO: check if 0e and 0c are okay to use, maybe we should use 06 and 0b instead...
            return 0;
        }
    }

    // READ sector 2 / offset 1K
    status = read_sector(partlba + 2, sector);
    if (status != 0) {
        return status;
    }

    // detect HFS+
    signature = *((UINT16 *)(sector));
    if (signature == 0x4442) {
        *parttype = 0xaf;
        if (*((UINT16 *)(sector + 0x7c)) == 0x2B48)
            *fsname = STR("HFS Extended (HFS+)");
        else
            *fsname = STR("HFS Standard");
        return 0;
    }
    else {
        if (signature == 0x2B48) {
            *parttype = 0xaf;
            *fsname = STR("HFS Extended (HFS+)");
            return 0;
        }
    }

    // detect ext2/ext3/ext4
    signature = *((UINT16 *)(sector + 56));
    if (signature == 0xEF53) {
        *parttype = 0x83;
        if (*((UINT16 *)(sector + 96)) & 0x02C0 ||
            *((UINT16 *)(sector + 100)) & 0x0078)
            *fsname = STR("ext4");
        else if (*((UINT16 *)(sector + 92)) & 0x0004)
            *fsname = STR("ext3");
        else
            *fsname = STR("ext2");
        return 0;
    }

    // READ sector 128 / offset 64K
    status = read_sector(partlba + 128, sector);
    if (status != 0)
        return status;

    // detect btrfs
    if (CompareMem(sector + 64, "_BHRfS_M", 8) == 0) {
        *parttype = 0x83;
        *fsname = STR("btrfs");
        return 0;
    }

    // detect ReiserFS
    if (CompareMem(sector + 52, "ReIsErFs", 8) == 0 ||
        CompareMem(sector + 52, "ReIsEr2Fs", 9) == 0 ||
        CompareMem(sector + 52, "ReIsEr3Fs", 9) == 0) {
        *parttype = 0x83;
        *fsname = STR("ReiserFS");
        return 0;
    }

    // detect Reiser4
    if (CompareMem(sector, "ReIsEr4", 7) == 0) {
        *parttype = 0x83;
        *fsname = STR("Reiser4");
        return 0;
    }

    // READ sector 64 / offset 32K
    status = read_sector(partlba + 64, sector);
    if (status != 0)
        return status;

    // detect JFS
    if (CompareMem(sector, "JFS1", 4) == 0) {
        *parttype = 0x83;
        *fsname = STR("JFS");
        return 0;
    }

    // READ sector 16 / offset 8K
    status = read_sector(partlba + 16, sector);
    if (status != 0)
        return status;

    // detect ReiserFS
    if (CompareMem(sector + 52, "ReIsErFs", 8) == 0 ||
        CompareMem(sector + 52, "ReIsEr2Fs", 9) == 0 ||
        CompareMem(sector + 52, "ReIsEr3Fs", 9) == 0) {
        *parttype = 0x83;
        *fsname = STR("ReiserFS");
        return 0;
    }

    return 0;
}
