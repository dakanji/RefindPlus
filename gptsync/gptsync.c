/*
 * gptsync/gptsync.c
 * Platform-independent code for syncing GPT and MBR
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
/* Changes copyright (c) 2013 Roderick W. Smith */

#include "gptsync.h"
#include "../include/version.h"

#include "../include/syslinux_mbr.h"
#define memcpy(a, b, c) CopyMem(a, b, c)

//
// MBR functions
//

static UINTN check_mbr(VOID)
{
    UINTN       i, k;
    BOOLEAN     found = FALSE;

    // check each entry
    for (i = 0; i < mbr_part_count; i++) {
        // check for overlap
        for (k = 0; k < mbr_part_count; k++) {
            if (k != i && !(mbr_parts[i].start_lba > mbr_parts[k].end_lba || mbr_parts[k].start_lba > mbr_parts[i].end_lba)) {
                Print(L"Status: MBR partition table is invalid, partitions overlap.\n");
                return EFI_UNSUPPORTED;
            }
        }

        // check for extended partitions
        if (mbr_parts[i].mbr_type == 0x05 || mbr_parts[i].mbr_type == 0x0f || mbr_parts[i].mbr_type == 0x85) {
            Print(L"Status: Extended partition found in MBR table, will not touch this disk.\n",
                  gpt_parts[i].gpt_parttype->name);
            return EFI_UNSUPPORTED;
        }

        // Check for matching GPT partitition; if not found, flag error
        if ((mbr_parts[i].mbr_type != 0xEE) && (mbr_parts[i].mbr_type != 0x00)) {
            found = FALSE;
            for (k = 0; (k < gpt_part_count) && !found; k++) {
               if ((mbr_parts[i].start_lba == gpt_parts[k].start_lba) && (mbr_parts[i].end_lba == gpt_parts[k].end_lba)) {
                  found = TRUE;
               } // if
            } // for
            if (!found) {
               Print(L"Status: Found MBR partition with no matching GPT partition. Re-syncing could\n");
               Print(L"destroy data; will not touch this disk.\n");
               return EFI_UNSUPPORTED;
            } // if
        } // if

    } // for

    return 0;
} // UINTN check_mbr()

static UINTN write_mbr(VOID)
{
    UINTN               status;
    UINTN               i, k;
    UINT8               active;
    UINT64              lba;
    MBR_PART_INFO       *table;
    BOOLEAN             have_bootcode;

    Print(L"\nWriting new MBR...\n");

    // read MBR data
    status = read_sector(0, sector);
    if (status != 0)
        return status;

    // write partition table
    *((UINT16 *)(sector + 510)) = 0xaa55;

    table = (MBR_PART_INFO *)(sector + 446);
    active = 0x80;
    for (i = 0; i < 4; i++) {
        for (k = 0; k < new_mbr_part_count; k++) {
            if (new_mbr_parts[k].index == i)
                break;
        }
        if (k >= new_mbr_part_count) {
            // unused entry
            table[i].flags        = 0;
            table[i].start_chs[0] = 0;
            table[i].start_chs[1] = 0;
            table[i].start_chs[2] = 0;
            table[i].type         = 0;
            table[i].end_chs[0]   = 0;
            table[i].end_chs[1]   = 0;
            table[i].end_chs[2]   = 0;
            table[i].start_lba    = 0;
            table[i].size         = 0;
        } else {
            if (new_mbr_parts[k].active) {
                table[i].flags        = active;
                active = 0x00;
            } else
                table[i].flags        = 0x00;
            table[i].start_chs[0] = 0xfe;
            table[i].start_chs[1] = 0xff;
            table[i].start_chs[2] = 0xff;
            table[i].type         = new_mbr_parts[k].mbr_type;
            table[i].end_chs[0]   = 0xfe;
            table[i].end_chs[1]   = 0xff;
            table[i].end_chs[2]   = 0xff;

            lba = new_mbr_parts[k].start_lba;
            if (lba > MAX_MBR_LBA) {
                Print(L"Warning: Partition %d starts beyond 2 TiB limit\n", i+1);
                lba = MAX_MBR_LBA;
            }
            table[i].start_lba    = (UINT32)lba;

            lba = new_mbr_parts[k].end_lba + 1 - new_mbr_parts[k].start_lba;
            if (lba > MAX_MBR_LBA) {
                Print(L"Warning: Partition %d extends beyond 2 TiB limit\n", i+1);
                lba = MAX_MBR_LBA;
            }
            table[i].size         = (UINT32)lba;
        }
    }

    // add boot code if necessary
    have_bootcode = FALSE;
    for (i = 0; i < MBR_BOOTCODE_SIZE; i++) {
        if (sector[i] != 0) {
            have_bootcode = TRUE;
            break;
        }
    }
    if (!have_bootcode) {
        // no boot code found in the MBR, add the syslinux MBR code
        SetMem(sector, MBR_BOOTCODE_SIZE, 0);
        CopyMem(sector, syslinux_mbr, SYSLINUX_MBR_SIZE);
    }

    // write MBR data
    status = write_sector(0, sector);
    if (status != 0)
        return status;

    Print(L"MBR updated successfully!\n");

    return 0;
}

//
// GPT functions
//

static UINTN check_gpt(VOID)
{
    UINTN       i, k;

    if (gpt_part_count == 0) {
        Print(L"Status: No GPT partition table, no need to sync.\n");
        return EFI_UNSUPPORTED;
    }

    // check each entry
    for (i = 0; i < gpt_part_count; i++) {
        // check sanity
        if (gpt_parts[i].end_lba < gpt_parts[i].start_lba) {
            Print(L"Status: GPT partition table is invalid.\n");
            return EFI_UNSUPPORTED;
        }
        // check for overlap
        for (k = 0; k < gpt_part_count; k++) {
            if (k != i && !(gpt_parts[i].start_lba > gpt_parts[k].end_lba || gpt_parts[k].start_lba > gpt_parts[i].end_lba)) {
                Print(L"Status: GPT partition table is invalid, partitions overlap.\n");
                return EFI_UNSUPPORTED;
            }
        }

        // check for partitions kind
        if (gpt_parts[i].gpt_parttype->kind == GPT_KIND_FATAL) {
            Print(L"Status: GPT partition of type '%s' found, will not touch this disk.\n",
                  gpt_parts[i].gpt_parttype->name);
            return EFI_UNSUPPORTED;
        }
    }

    return 0;
} // VOID check_gpt()

//
// compare GPT and MBR tables
//

#define ACTION_NONE        (0)
#define ACTION_NOP         (1)
#define ACTION_REWRITE     (2)

// Copy a single GPT entry to the new_mbr_parts array.
static VOID copy_gpt_to_new_mbr(UINTN gpt_num, UINTN mbr_num) {
   new_mbr_parts[mbr_num].index     = mbr_num;
   new_mbr_parts[mbr_num].start_lba = gpt_parts[gpt_num].start_lba;
   new_mbr_parts[mbr_num].end_lba   = gpt_parts[gpt_num].end_lba;
   new_mbr_parts[mbr_num].mbr_type  = gpt_parts[gpt_num].mbr_type;
   new_mbr_parts[mbr_num].active    = FALSE;
} // VOID copy_gpt_to_new_mbr()

// A simple bubble sort for the MBR partitions.
static VOID sort_mbr(PARTITION_INFO *parts) {
   PARTITION_INFO one_part;
   int c, d;

   if (parts == NULL)
      return;

   for (c = 0 ; c < 3; c++) {
      for (d = 1 ; d < 3 - c; d++) {
         if ((parts[d].start_lba > parts[d + 1].start_lba) && (parts[d].start_lba > 0) && (parts[d + 1].start_lba > 0)) {
            one_part           = parts[d];
            parts[d]           = parts[d + 1];
            parts[d + 1]       = one_part;
            parts[d].index     = d;
            parts[d + 1].index = d + 1;
         } // if
      } // for
   } // for
} // VOID sort_mbr()

// Generate a hybrid MBR based on the current GPT. Stores the result in the
// new_mbr_parts[] array.
static VOID generate_hybrid_mbr(VOID) {
    UINTN i, k, iter, count_active;
    UINT64 first_used_lba;

    new_mbr_part_count = 1;
    first_used_lba = (UINT64) MAX_MBR_LBA + (UINT64) 1;

    // Copy partitions in three passes....
    // First, do FAT and NTFS partitions....
    i = 0;
    do {
        if ((gpt_parts[i].start_lba > 0) && (gpt_parts[i].end_lba > 0) &&
            (gpt_parts[i].end_lba <= MAX_MBR_LBA) &&                    /* Within MBR limits */
            (gpt_parts[i].gpt_parttype->kind == GPT_KIND_BASIC_DATA) && /* MS Basic Data GPT type code */
            (gpt_parts[i].mbr_type != 0x83)) {                          /* Not containing Linux filesystem */
           copy_gpt_to_new_mbr(i, new_mbr_part_count);
           if (new_mbr_parts[new_mbr_part_count].start_lba < first_used_lba)
              first_used_lba = new_mbr_parts[new_mbr_part_count].start_lba;

           new_mbr_part_count++;
        }
        i++;
    } while (i < gpt_part_count && new_mbr_part_count <= 3);

    // Second, do Linux partitions. Note that we start from the END of the
    // partition list, so as to maximize the space covered by the 0xEE
    // partition if there are several Linux partitions before other hybridized
    // partitions.
    i = gpt_part_count - 1; // Note that gpt_part_count can't be 0; filtered by check_gpt()
    while (i < gpt_part_count && new_mbr_part_count <= 3) { // if too few GPT partitions, i loops around to a huge value
        if ((gpt_parts[i].start_lba > 0) && (gpt_parts[i].end_lba > 0) &&
            (gpt_parts[i].end_lba <= MAX_MBR_LBA) &&
            ((gpt_parts[i].gpt_parttype->kind == GPT_KIND_DATA) || (gpt_parts[i].gpt_parttype->kind == GPT_KIND_BASIC_DATA)) &&
            (gpt_parts[i].mbr_type == 0x83)) {
           copy_gpt_to_new_mbr(i, new_mbr_part_count);
           if (new_mbr_parts[new_mbr_part_count].start_lba < first_used_lba)
              first_used_lba = new_mbr_parts[new_mbr_part_count].start_lba;

           new_mbr_part_count++;
       }
       i--;
    } // while

    // Third, do anything that's left to cover uncovered spaces; but this requires
    // first creating the EFI protective entry, since we don't want to bother with
    // anything already covered by this entry....
    new_mbr_parts[0].index     = 0;
    new_mbr_parts[0].start_lba = 1;
    new_mbr_parts[0].end_lba   = (disk_size() > first_used_lba) ? (first_used_lba - 1) : disk_size() - 1;
    if (new_mbr_parts[0].end_lba > MAX_MBR_LBA)
       new_mbr_parts[0].end_lba = MAX_MBR_LBA;
    new_mbr_parts[0].mbr_type  = 0xEE;
    i = 0;
    while (i < gpt_part_count && new_mbr_part_count <= 3) {
       if ((gpt_parts[i].start_lba > new_mbr_parts[0].end_lba) && (gpt_parts[i].end_lba > 0) &&
           (gpt_parts[i].end_lba <= MAX_MBR_LBA) &&
           (gpt_parts[i].gpt_parttype->kind != GPT_KIND_BASIC_DATA) &&
           (gpt_parts[i].mbr_type != 0x83)) {
          copy_gpt_to_new_mbr(i, new_mbr_part_count);
          new_mbr_part_count++;
       }
       i++;
    } // while

    // find matching partitions in the old MBR table, copy undetected details....
    for (i = 1; i < new_mbr_part_count; i++) {
       for (k = 0; k < mbr_part_count; k++) {
           if (mbr_parts[k].start_lba == new_mbr_parts[i].start_lba) {
               // keep type if not detected
               if (new_mbr_parts[i].mbr_type == 0)
                   new_mbr_parts[i].mbr_type = mbr_parts[k].mbr_type;
               // keep active flag
               new_mbr_parts[i].active = mbr_parts[k].active;
               break;
           } // if
       } // for (k...)
       if (new_mbr_parts[i].mbr_type == 0) {
          // final fallback: set to a (hopefully) unused type
          new_mbr_parts[i].mbr_type = 0xc0;
       } // if
    } // for (i...)

    sort_mbr(new_mbr_parts);

    // make sure there's exactly one active partition
    for (iter = 0; iter < 3; iter++) {
        // check
        count_active = 0;
        for (i = 0; i < new_mbr_part_count; i++)
            if (new_mbr_parts[i].active)
                count_active++;
        if (count_active == 1)
            break;

        // set active on the first matching partition
        if (count_active == 0) {
            for (i = 0; i < new_mbr_part_count; i++) {
                if (((new_mbr_parts[i].mbr_type == 0x07 ||    // NTFS
                      new_mbr_parts[i].mbr_type == 0x0b ||    // FAT32
                      new_mbr_parts[i].mbr_type == 0x0c)) ||  // FAT32 (LBA)
                    (iter >= 1 && (new_mbr_parts[i].mbr_type == 0x83)) ||  // Linux
                    (iter >= 2 && i > 0)) {
                    new_mbr_parts[i].active = TRUE;
                    break;
                }
            }
        } else if (count_active > 1 && iter == 0) {
            // too many active partitions, try deactivating the ESP / EFI Protective entry
            if ((new_mbr_parts[0].mbr_type == 0xee || new_mbr_parts[0].mbr_type == 0xef) &&
                new_mbr_parts[0].active) {
                new_mbr_parts[0].active = FALSE;
            }
        } else if (count_active > 1 && iter > 0) {
            // too many active partitions, deactivate all but the first one
            count_active = 0;
            for (i = 0; i < new_mbr_part_count; i++)
                if (new_mbr_parts[i].active) {
                    if (count_active > 0)
                        new_mbr_parts[i].active = FALSE;
                    count_active++;
                }
        }
    } // for
} // VOID generate_hybrid_mbr()

// Examine partitions and decide whether a rewrite is in order.
// Note that this function MAY ask user for advice.
// Note that this function assumes the new hybrid MBR has already
// been computed and stored in new_mbr_parts[].
static BOOLEAN should_rewrite(VOID) {
   BOOLEAN retval = TRUE, all_identical = TRUE, invalid;
   UINTN i, num_existing_hybrid = 0, num_new_hybrid = 0;

   // Check to see if the proposed table is identical to the current one;
   // if so, synchronizing is pointless....
   for (i = 0; i < 4; i++) {
      if ((new_mbr_parts[i].mbr_type != 0xEE) && (mbr_parts[i].mbr_type != 0xEE) &&
          ((new_mbr_parts[i].active != mbr_parts[i].active) ||
           (new_mbr_parts[i].start_lba != mbr_parts[i].start_lba) ||
           (new_mbr_parts[i].end_lba != mbr_parts[i].end_lba) ||
           (new_mbr_parts[i].mbr_type != mbr_parts[i].mbr_type)))
         all_identical = FALSE;

      // while we're looping, count the number of old & new hybrid partitions....
      if ((mbr_parts[i].mbr_type != 0x00) && (mbr_parts[i].mbr_type != 0xEE))
         num_existing_hybrid++;
      if ((new_mbr_parts[i].mbr_type != 0x00) && (new_mbr_parts[i].mbr_type != 0xEE))
         num_new_hybrid++;
   } // for

   if (all_identical) {
      Print(L"Tables are synchronized, no need to sync.\n");
      return FALSE;
   }

   // If there's nothing to hybridize, but an existing hybrid MBR exists, offer to replace
   // the hybrid MBR with a protective MBR.
   if ((num_new_hybrid == 0) && (num_existing_hybrid > 0)) {
      Print(L"Found no partitions that could be hybridized, but an existing hybrid MBR exists.\n");
      Print(L"If you proceed, a fresh protective MBR will be created. Do you want to create\n");
      invalid = input_boolean(STR("this new protective MBR, erasing the hybrid MBR? [y/N] "), &retval);
      if (invalid)
         retval = FALSE;
   } // if

   // If existing hybrid MBR that's NOT identical to the new one, ask the user
   // before overwriting the old one.
   if ((num_new_hybrid > 0) && (num_existing_hybrid > 0)) {
      Print(L"Existing hybrid MBR detected, but it's not identical to what this program\n");
      Print(L"would generate. Do you want to see the hybrid MBR that this program would\n");
      invalid = input_boolean(STR("generate? [y/N] "), &retval);
      if (invalid)
         retval = FALSE;
   } // if

   return retval;
} // BOOLEAN should_rewrite()

static UINTN analyze(VOID)
{
    UINTN   i, detected_parttype;
    CHARN   *fsname;
    UINTN   status;

    new_mbr_part_count = 0;

    // determine correct MBR types for GPT partitions
    if (gpt_part_count == 0) {
        Print(L"Status: No GPT partitions defined, nothing to sync.\n");
        return 0;
    }
    for (i = 0; i < gpt_part_count; i++) {
        gpt_parts[i].mbr_type = gpt_parts[i].gpt_parttype->mbr_type;
        if (gpt_parts[i].gpt_parttype->kind == GPT_KIND_BASIC_DATA) {
            // Basic Data: need to look at data in the partition
            status = detect_mbrtype_fs(gpt_parts[i].start_lba, &detected_parttype, &fsname);
            if (status != 0)
               Print(L"Warning: Error %d when detecting filesystem type!\n", status);
            if (detected_parttype)
                gpt_parts[i].mbr_type = detected_parttype;
            else
                gpt_parts[i].mbr_type = 0x0b;  // fallback: FAT32
        }
        // NOTE: mbr_type may still be 0 if content detection fails for exotic GPT types or file systems
    } // for

    // generate the new table
    generate_hybrid_mbr();
    if (!should_rewrite())
       return EFI_ABORTED;

    // display table
    Print(L"\nProposed new MBR partition table:\n");
    Print(L" # A    Start LBA      End LBA  Type\n");
    for (i = 0; i < new_mbr_part_count; i++) {
        Print(L" %d %s %12lld %12lld  %02x  %s\n",
              new_mbr_parts[i].index + 1,
              new_mbr_parts[i].active ? STR("*") : STR(" "),
              new_mbr_parts[i].start_lba,
              new_mbr_parts[i].end_lba,
              new_mbr_parts[i].mbr_type,
              mbr_parttype_name(new_mbr_parts[i].mbr_type));
    }

    return 0;
} // UINTN analyze()

//
// sync algorithm entry point
//

UINTN gptsync(VOID)
{
    UINTN   status = 0;
    UINTN   status_gpt, status_mbr;
    BOOLEAN proceed = FALSE;

    Print(L"gptsync version %s\ncopyright (c) 2006-2007 Christoph Pfisterer & 2013 Roderick W. Smith\n", REFIND_VERSION);

    // get full information from disk
    status_gpt = read_gpt();
    status_mbr = read_mbr();
    if (status_gpt != 0 || status_mbr != 0)
        return (status_gpt || status_mbr);

    // cross-check current situation
    Print(L"\n");
    status = check_gpt();   // check GPT for consistency
    if (status != 0)
        return status;
    status = check_mbr();   // check MBR for consistency
    if (status != 0)
        return status;
    status = analyze();     // analyze the situation & compose new MBR table
    if (status != 0)
        return status;
    if (new_mbr_part_count == 0)
        return status;

    // offer user the choice what to do
    status = input_boolean(STR("\nMay I update the MBR as printed above? [y/N] "), &proceed);
    if (status != 0 || proceed != TRUE)
        return status;

    // adjust the MBR and write it back
    status = write_mbr();
    if (status != 0)
        return status;

    return status;
}
