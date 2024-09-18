/*
 * BootMaster/gpt.c
 * Functions related to GPT data structures
 *
 * Copyright (c) 2014-2021 Roderick W. Smith
 * All rights reserved.
 *
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
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2024 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

 #include "gpt.h"
 #include "lib.h"
 #include "crc32.h"
 #include "screenmgt.h"
 #include "../include/refit_call_wrapper.h"

 #ifdef __MAKEWITH_TIANO
 #define BlockIoProtocol gEfiBlockIoProtocolGuid
 #endif

 GPT_DATA *gPartitions = NULL;

// Allocate data for the main GPT_DATA structure, as well as the ProtectiveMBR
// and Header structures it contains. This function does *NOT*, however,
// allocate memory for the Entries data structure, since its size is variable
// and is determined by the contents of Header.
GPT_DATA * AllocateGptData (VOID) {
    GPT_DATA *GptData;

    GptData = AllocateZeroPool (sizeof (GPT_DATA));
    if (GptData == NULL) {
        // Early Return
        return NULL;
    }

    GptData->Header = AllocateZeroPool (sizeof (GPT_HEADER));
    if (GptData->Header == NULL) {
        ClearGptData (GptData);

        // Early Return
        return NULL;
    }

    GptData->ProtectiveMBR = AllocateZeroPool (sizeof (MBR_RECORD));
    if (GptData->ProtectiveMBR == NULL) {
        ClearGptData (GptData);
    }

    return GptData;
 } // GPT_DATA * AllocateGptData()

// Unallocate a single GPT_DATA structure.
// NB: Does *NOT* follow the linked list.
VOID ClearGptData (
    GPT_DATA *Data
) {
    if (Data == NULL) {
        // Early Return
        return;
    }

    MY_FREE_POOL(Data->ProtectiveMBR);
    MY_FREE_POOL(Data->Entries);
    MY_FREE_POOL(Data->Header);
    MY_FREE_POOL(Data);
} // VOID ClearGptData()

// TODO: Make this work on big-endian systems; at the moment, it contains
// little-endian assumptions!
// Returns TRUE if the GPT protective MBR and header data appear valid,
// FALSE otherwise.
static
BOOLEAN GptHeaderValid (
    GPT_DATA *GptData
) {
    BOOLEAN IsValid;
    UINT32  StoredCrcValue;
    UINT32  CrcValue;
    UINTN   HeaderSize;


    if (GptData                == NULL ||
        GptData->Header        == NULL ||
        GptData->ProtectiveMBR == NULL
    ) {
        return FALSE;
    }

    IsValid = (GptData->ProtectiveMBR->MBRSignature == 0xAA55);
    IsValid = IsValid && ((GptData->ProtectiveMBR->partitions[0].type == 0xEE) ||
                          (GptData->ProtectiveMBR->partitions[1].type == 0xEE) ||
                          (GptData->ProtectiveMBR->partitions[2].type == 0xEE) ||
                          (GptData->ProtectiveMBR->partitions[3].type == 0xEE));

    IsValid = IsValid && ((GptData->Header->signature == 0x5452415020494645ULL) &&
                          (GptData->Header->spec_revision == 0x00010000) &&
                          (GptData->Header->entry_size == 128));

    if (!IsValid) {
        // Early Return
        return FALSE;
    }

    StoredCrcValue = GptData->Header->header_crc32;
    GptData->Header->header_crc32 = 0;

    HeaderSize = sizeof (GPT_HEADER);
    if (GptData->Header->header_size < HeaderSize) {
        HeaderSize = GptData->Header->header_size;
    }

    // Looks good so far ... Validate CRC value.
    CrcValue = crc32refit (0x0, GptData->Header, HeaderSize);
    if (CrcValue != StoredCrcValue) {
        IsValid = FALSE;
    }

    GptData->Header->header_crc32 = StoredCrcValue;

    return IsValid;
} // BOOLEAN GptHeaderValid()

// Read GPT data from Volume and store it in *Data. Note that this function
// may be called on a Volume that is not in fact a GPT disk (an MBR disk,
// a partition, etc.), in which case it will return EFI_LOAD_ERROR or some
// other error condition. In this case, *Data will be left alone.
// Note also that this function checks CRCs and does other sanity checks
// on the input data, but does NOT resort to using the backup data if the
// primary data structures are damaged. The intent is that the function
// be very conservative about reading GPT data. Currently (version 0.7.10),
// rEFInd uses the data only to provide access to partition names. This is
// non-critical data, so it is OK to return nothing, but having the program
// hang on reading garbage or return nonsense could be very bad.
EFI_STATUS ReadGptData (
    REFIT_VOLUME  *Volume,
    GPT_DATA     **Data
) {
    EFI_STATUS  Status;
    UINT64      BufferSize;
    UINTN       i;
    GPT_DATA   *GptData; // Temporary storage ... Tansferred to *Data later


    if (Volume == NULL || Data == NULL) {
        // Early Return
        return EFI_INVALID_PARAMETER;
    }

    // Get block i/o
    if (Volume->BlockIO == NULL) {
        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, Volume->DeviceHandle,
            &BlockIoProtocol, (VOID **) &(Volume->BlockIO)
        );
        if (EFI_ERROR(Status)) {
            Volume->BlockIO = NULL;
            Print (L"Warning: Can't get BlockIO protocol in ReadGptData().\n");

            // Early Return
            return EFI_NOT_READY;
        }
    }

    if (!Volume->BlockIO->Media->MediaPresent ||
        Volume->BlockIO->Media->LogicalPartition
    ) {
        // Early Return
        return EFI_NO_MEDIA;
    }

    GptData = AllocateGptData(); // DA-TAG: All but GptData->Entries
    if (GptData == NULL) {
        // Early Return
        return EFI_OUT_OF_RESOURCES;
    }

    // Read the MBR and store it in GptData->ProtectiveMBR.
    Status = REFIT_CALL_5_WRAPPER(
        Volume->BlockIO->ReadBlocks, Volume->BlockIO,
        Volume->BlockIO->Media->MediaId, 0,
        sizeof (MBR_RECORD), (VOID*) GptData->ProtectiveMBR
    );
    if (EFI_ERROR(Status)) {
        ClearGptData (GptData);

        // Early Return
        return Status;
    }

    // Read the GPT header and store it in GptData->Header.
    Status = REFIT_CALL_5_WRAPPER(
        Volume->BlockIO->ReadBlocks, Volume->BlockIO,
        Volume->BlockIO->Media->MediaId, 1,
        sizeof (GPT_HEADER), GptData->Header
    );
    if (EFI_ERROR(Status)) {
        ClearGptData (GptData);

        // Early Return
        return Status;
    }

    // If it looks like a valid protective MBR & GPT header, try to do more with it.
    if (!GptHeaderValid (GptData)) {
        ClearGptData (GptData);

        // Early Return
        return EFI_UNSUPPORTED;
    }

    // Load actual GPT table.
    BufferSize       = (UINT64) (GptData->Header->entry_count) * 128;
    GptData->Entries = AllocatePool (BufferSize);
    if (GptData->Entries == NULL) {
        ClearGptData (GptData);

        // Early Return
        return EFI_OUT_OF_RESOURCES;
    }

    Status = REFIT_CALL_5_WRAPPER(
        Volume->BlockIO->ReadBlocks, Volume->BlockIO,
        Volume->BlockIO->Media->MediaId, GptData->Header->entry_lba,
        BufferSize, GptData->Entries
    );
    if (EFI_ERROR(Status)) {
        ClearGptData (GptData);

        // Early Return
        return Status;
    }

    // Check CRC status of table
    if (crc32refit (
            0x0, GptData->Entries, BufferSize
        ) != GptData->Header->entry_crc32
    ) {
        ClearGptData (GptData);

        // Early Return
        return EFI_CRC_ERROR;
    }

    // Now, ensure that every name is null-terminated.
    for (i = 0; i < GptData->Header->entry_count; i++) {
        GptData->Entries[i].name[35] = '\0';
    }

    // Everything looks OK, so copy it over
    ClearGptData (*Data);
    *Data = GptData;

    return EFI_SUCCESS;
} // EFI_STATUS ReadGptData()

// Look in gPartitions for a partition with the specified Guid. If found, return
// a pointer to that partition's data. If not found, return a NULL pointer.
// The calling function is responsible for freeing the returned memory.
GPT_ENTRY * FindPartWithGuid (
    EFI_GUID *Guid
) {
    UINTN      i;
    GPT_ENTRY *Found;
    GPT_DATA  *GptData;


    if (Guid == NULL || gPartitions == NULL) {
        // Early Return
        return NULL;
    }

    Found   = NULL;
    GptData = gPartitions;
    while (!Found && GptData) {
        i = 0;
        while (!Found && i < GptData->Header->entry_count) {
            if (!GuidsAreEqual ((EFI_GUID *) &(GptData->Entries[i].partition_guid), Guid)) {
                i++;
                continue;
            }

            Found = AllocateZeroPool (sizeof (GPT_ENTRY));
            if (Found == NULL) {
                // Early Return
                return NULL;
            }

            REFIT_CALL_3_WRAPPER(
                gBS->CopyMem, Found,
                &GptData->Entries[i], sizeof (GPT_ENTRY)
            );
        } // while (scanning entries)

        GptData = GptData->NextEntry;
    } // while (scanning GPTs)

    return Found;
} // GPT_ENTRY * FindPartWithGuid()

// Erase the gPartitions linked-list data structure
VOID ForgetPartitionTables (VOID) {
    GPT_DATA  *Next;


    while (gPartitions != NULL) {
        Next = gPartitions->NextEntry;
        ClearGptData (gPartitions);
        gPartitions = Next;
    } // while
} // VOID ForgetPartitionTables()

// If Volume points to a whole disk with a GPT, add it to the gPartitions
// linked list of GPTs.
VOID AddPartitionTable (
    REFIT_VOLUME *Volume
) {
    EFI_STATUS  Status;
    GPT_DATA   *GptList;
    GPT_DATA   *GptData;


    GptData = NULL;
    Status = ReadGptData (Volume, &GptData);
    if (EFI_ERROR(Status)) {
        if (GptData != NULL) {
            ClearGptData (GptData);
        }

        // Early Return
        return;
    }

    if (gPartitions == NULL) {
        gPartitions = GptData;
    }
    else {
        GptList = gPartitions;

        while (GptList->NextEntry != NULL) {
            GptList = GptList->NextEntry;
        } // while

        GptList->NextEntry = GptData;
    }
} // VOID AddPartitionTable()
