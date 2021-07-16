/*
 * refind/gpt.c
 * Functions related to GPT data structures
 *
 * Copyright (c) 2014-2015 Roderick W. Smith
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

#include "gpt.h"
#include "lib.h"
#include "screen.h"
#include "crc32.h"
#include "../include/refit_call_wrapper.h"

#ifdef __MAKEWITH_TIANO
#define BlockIoProtocol gEfiBlockIoProtocolGuid
#endif

GPT_DATA *gPartitions = NULL;

// Allocate data for the main GPT_DATA structure, as well as the ProtectiveMBR
// and Header structures it contains. This function does *NOT*, however,
// allocate memory for the Entries data structure, since its size is variable
// and is determined by the contents of Header.
GPT_DATA * AllocateGptData(VOID) {
    GPT_DATA *GptData;

    GptData = AllocateZeroPool(sizeof(GPT_DATA));
    if (GptData != NULL) {
        GptData->ProtectiveMBR = AllocateZeroPool(sizeof(MBR_RECORD));
        GptData->Header = AllocateZeroPool(sizeof(GPT_HEADER));
        if ((GptData->ProtectiveMBR == NULL) || (GptData->Header == NULL)) {
            MyFreePool(GptData->ProtectiveMBR);
            MyFreePool(GptData->Header);
            MyFreePool(GptData);
            GptData = NULL;
        } // if
    } // if
    return GptData;
} // GPT_DATA * AllocateGptData()

// Unallocate a single GPT_DATA structure. This does NOT follow the
// linked list, though.
VOID ClearGptData(GPT_DATA *Data) {
    if (Data) {
        if (Data->ProtectiveMBR)
            MyFreePool(Data->ProtectiveMBR);
        if (Data->Header)
            MyFreePool(Data->Header);
        if (Data->Entries)
            MyFreePool(Data->Entries);
        MyFreePool(Data);
    } // if
} // VOID ClearGptData()

// TODO: Make this work on big-endian systems; at the moment, it contains
// little-endian assumptions!
// Returns TRUE if the GPT protective MBR and header data appear valid,
// FALSE otherwise.
static BOOLEAN GptHeaderValid(GPT_DATA *GptData) {
    BOOLEAN IsValid;
    UINT32 CrcValue, StoredCrcValue;
    UINTN HeaderSize = sizeof(GPT_HEADER);

    if ((GptData == NULL) || (GptData->ProtectiveMBR == NULL) || (GptData->Header == NULL))
        return FALSE;

    IsValid = (GptData->ProtectiveMBR->MBRSignature == 0xAA55);
    IsValid = IsValid && ((GptData->ProtectiveMBR->partitions[0].type == 0xEE) ||
                          (GptData->ProtectiveMBR->partitions[1].type == 0xEE) ||
                          (GptData->ProtectiveMBR->partitions[2].type == 0xEE) ||
                          (GptData->ProtectiveMBR->partitions[3].type == 0xEE));

    IsValid = IsValid && ((GptData->Header->signature == 0x5452415020494645ULL) &&
                          (GptData->Header->spec_revision == 0x00010000) &&
                          (GptData->Header->entry_size == 128));

    // Looks good so far; check CRC value....
    if (IsValid) {
        if (GptData->Header->header_size < HeaderSize)
            HeaderSize = GptData->Header->header_size;
        StoredCrcValue = GptData->Header->header_crc32;
        GptData->Header->header_crc32 = 0;
        CrcValue = crc32(0x0, GptData->Header, HeaderSize);
        if (CrcValue != StoredCrcValue)
            IsValid = FALSE;
        GptData->Header->header_crc32 = StoredCrcValue;
    } // if

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
// non-critical data, so it's OK to return nothing, but having the program
// hang on reading garbage or return nonsense could be very bad.
EFI_STATUS ReadGptData(REFIT_VOLUME *Volume, GPT_DATA **Data) {
    EFI_STATUS Status = EFI_SUCCESS;
    UINT64     BufferSize;
    UINTN      i;
    GPT_DATA   *GptData = NULL; // Temporary holding storage; transferred to *Data later

    if ((Volume == NULL) || (Data == NULL))
        return EFI_INVALID_PARAMETER;

    // get block i/o
    if ((Status == EFI_SUCCESS) && (Volume->BlockIO == NULL)) {
        Status = refit_call3_wrapper(BS->HandleProtocol, Volume->DeviceHandle,
                                     &BlockIoProtocol, (VOID **) &(Volume->BlockIO));
        if (EFI_ERROR(Status)) {
            Volume->BlockIO = NULL;
            Print(L"Warning: Can't get BlockIO protocol in ReadGptData().\n");
            Status = EFI_NOT_READY;
        }
    } // if

    if ((Status == EFI_SUCCESS) && ((!Volume->BlockIO->Media->MediaPresent) ||
        (Volume->BlockIO->Media->LogicalPartition)))
            Status = EFI_NO_MEDIA;

    if (Status == EFI_SUCCESS) {
        GptData = AllocateGptData(); // Note: All but GptData->Entries
        if (GptData == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
        } // if
    } // if

    // Read the MBR and store it in GptData->ProtectiveMBR.
    if (Status == EFI_SUCCESS) {
        Status = refit_call5_wrapper(Volume->BlockIO->ReadBlocks, Volume->BlockIO,
                                     Volume->BlockIO->Media->MediaId, 0,
                                     sizeof(MBR_RECORD), (VOID*) GptData->ProtectiveMBR);
    }

    // Read the GPT header and store it in GptData->Header.
    if (Status == EFI_SUCCESS) {
        Status = refit_call5_wrapper(Volume->BlockIO->ReadBlocks, Volume->BlockIO,
                                     Volume->BlockIO->Media->MediaId, 1,
                                     sizeof(GPT_HEADER), GptData->Header);
    }

    // If it looks like a valid protective MBR & GPT header, try to do more with it....
    if (Status == EFI_SUCCESS) {
        if (GptHeaderValid(GptData)) {
            // Load actual GPT table....
            BufferSize = GptData->Header->entry_count * 128;
            GptData->Entries = AllocatePool(BufferSize);
            if (GptData->Entries == NULL)
                Status = EFI_OUT_OF_RESOURCES;

            if (Status == EFI_SUCCESS)
                Status = refit_call5_wrapper(Volume->BlockIO->ReadBlocks, Volume->BlockIO,
                                             Volume->BlockIO->Media->MediaId,
                                             GptData->Header->entry_lba, BufferSize,
                                             GptData->Entries);

            // Check CRC status of table
            if ((Status == EFI_SUCCESS) &&
                (crc32(0x0, GptData->Entries, BufferSize) != GptData->Header->entry_crc32))
                    Status = EFI_CRC_ERROR;

            // Now, ensure that every name is null-terminated....
            if (Status == EFI_SUCCESS) {
                for (i = 0; i < GptData->Header->entry_count; i++)
                    GptData->Entries[i].name[35] = '\0';
            } // if
        } else {
            Status = EFI_UNSUPPORTED;
        } // if/else valid header
    } // if header read OK

    if (Status == EFI_SUCCESS) {
        // Everything looks OK, so copy it over
        ClearGptData(*Data);
        *Data = GptData;
    } else {
        ClearGptData(GptData);
    } // if/else

    return Status;
} // EFI_STATUS ReadGptData()

// Look in gPartitions for a partition with the specified Guid. If found, return
// a pointer to that partition's data. If not found, return a NULL pointer.
// The calling function is responsible for freeing the returned memory.
GPT_ENTRY * FindPartWithGuid(EFI_GUID *Guid) {
    UINTN     i;
    GPT_ENTRY *Found = NULL;
    GPT_DATA  *GptData;

    if ((Guid == NULL) || (gPartitions == NULL))
        return NULL;

    GptData = gPartitions;
    while ((GptData != NULL) && (!Found)) {
        i = 0;
        while ((i < GptData->Header->entry_count) && (!Found)) {
            if (GuidsAreEqual((EFI_GUID*) &(GptData->Entries[i].partition_guid), Guid)) {
                Found = AllocateZeroPool(sizeof(GPT_ENTRY));
                CopyMem(Found, &GptData->Entries[i], sizeof(GPT_ENTRY));
            } else {
                i++;
            } // if/else
        } // while(scanning entries)
        GptData = GptData->NextEntry;
    } // while(scanning GPTs)
    return Found;
} // GPT_ENTRY * FindPartWithGuid()

// Erase the gPartitions linked-list data structure
VOID ForgetPartitionTables(VOID) {
    GPT_DATA  *Next;

    while (gPartitions != NULL) {
        Next = gPartitions->NextEntry;
        ClearGptData(gPartitions);
        gPartitions = Next;
    } // while
} // VOID ForgetPartitionTables()

// If Volume points to a whole disk with a GPT, add it to the gPartitions
// linked list of GPTs.
VOID AddPartitionTable(REFIT_VOLUME *Volume) {
    GPT_DATA    *GptData = NULL, *GptList;
    EFI_STATUS  Status;
    UINTN       NumTables = 1;

    Status = ReadGptData(Volume, &GptData);
    if (Status == EFI_SUCCESS) {
        if (gPartitions == NULL) {
            gPartitions = GptData;
        } else {
            GptList = gPartitions;
            while (GptList->NextEntry != NULL) {
                GptList = GptList->NextEntry;
                NumTables++;
            } // while
            GptList->NextEntry = GptData;
            NumTables++;
        } // if/else
    } else if (GptData != NULL) {
        ClearGptData(GptData);
        NumTables = 0;
    } // if/else
} // VOID AddPartitionTable()

