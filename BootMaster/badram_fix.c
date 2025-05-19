/**
 * BootMaster/badram_fix.c
 *
 * Copyright (c) 2025 Dayo Akanji (sf.net/u/dakanji/profile)
 * Released under the MIT License
**/

#include "badram_fix.h"
#include "global.h"
#include "lib.h"
#include "mystrings.h"
#include "../include/refit_call_wrapper.h"


static
EFI_STATUS MarkPageReserved (
    EFI_PHYSICAL_ADDRESS PageAddress
) {
    return REFIT_CALL_4_WRAPPER(
        gBS->AllocatePages, AllocateAddress,
        EfiReservedMemoryType, 1, &PageAddress
    );
} // static FI_STATUS MarkPageReserved()

static
EFI_STATUS MarkPageUnusable (
    EFI_PHYSICAL_ADDRESS PageAddress
) {
    return REFIT_CALL_4_WRAPPER(
        gBS->AllocatePages, AllocateAddress,
        EfiUnusableMemory, 1, &PageAddress
    );
} // static EFI_STATUS MarkPageUnusable()

static
EFI_STATUS GetMemoryMapWithBuffer (
    EFI_MEMORY_DESCRIPTOR **MemoryMap,
    UINTN                  *MemoryMapSize,
    UINTN                  *DescriptorSize
) {
    EFI_STATUS              Status;
    UINT32                  DescriptorVersion;
    UINTN                   MapKey;


    // Initial call to get correct memory map size
    *MemoryMapSize = 0;
    Status = REFIT_CALL_5_WRAPPER(
        gBS->GetMemoryMap, MemoryMapSize,
        NULL, &MapKey,
        DescriptorSize, &DescriptorVersion
    );
    if (Status != EFI_BUFFER_TOO_SMALL) {
        return EFI_LOAD_ERROR;
    }

    // Allocate required memory map size
    Status = REFIT_CALL_3_WRAPPER(
        gBS->AllocatePool, EfiBootServicesData,
        *MemoryMapSize, (void **)MemoryMap
    );
    if (!EFI_ERROR(Status)) {
        // Second call to get actual memory map data
        Status = REFIT_CALL_5_WRAPPER(
            gBS->GetMemoryMap, MemoryMapSize,
            *MemoryMap, &MapKey,
            DescriptorSize, &DescriptorVersion
        );
    }

    return Status;
} // static EFI_STATUS GetMemoryMapWithBuffer()

static
BOOLEAN IsPageInUse (
    EFI_PHYSICAL_ADDRESS   PageAddress,
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    UINTN                  MemoryMapSize,
    UINTN                  DescriptorSize,
    BOOLEAN                OurFixWide
) {
    BOOLEAN               IsInUse;
    EFI_MEMORY_DESCRIPTOR *MemoryMapEntry;


    IsInUse = FALSE;
    for (MemoryMapEntry = MemoryMap;
         (UINT8 *) MemoryMapEntry < (UINT8 *) MemoryMap + MemoryMapSize;
         MemoryMapEntry = (EFI_MEMORY_DESCRIPTOR *) ((UINT8 *)MemoryMapEntry + DescriptorSize)
     ) {
        if (PageAddress >= MemoryMapEntry->PhysicalStart &&
            PageAddress <  MemoryMapEntry->PhysicalStart + (MemoryMapEntry->NumberOfPages * EFI_PAGE_SIZE)
        ) {
            IsInUse = TRUE;

            break;
        }
    } // for

    return IsInUse;
} // static BOOLEAN IsPageInUse()

static
BOOLEAN IsQualifyingTarget (
    EFI_MEMORY_TYPE Type,
    BOOLEAN         OurFixWide
) {
    BOOLEAN         ValidType;

    if (OurFixWide) {
        // Force 'validity'
        ValidType = TRUE;
    }
    else {
        ValidType = (
            Type == EfiConventionalMemory  ||
            Type == EfiRuntimeServicesCode ||
            Type == EfiRuntimeServicesData ||
            Type == EfiBootServicesCode    ||
            Type == EfiBootServicesData    ||
            Type == EfiLoaderCode          ||
            Type == EfiLoaderData
        );
    }

    return ValidType;
} // static BOOLEAN IsQualifyingTarget()

static
BOOLEAN IsPageEmpty (
    EFI_PHYSICAL_ADDRESS  Address
) {
    UINT64               *Ptr;
    UINTN                 i;


    Ptr = (UINT64 *) Address;
    for (i = 0; i < (EFI_PAGE_SIZE / sizeof(UINT64)); i++) {
        if (Ptr[i] != 0) {
            return FALSE;
        }
    } // for

    return TRUE;
} // static BOOLEAN IsPageEmpty()

static
BOOLEAN TestMemory (
    EFI_PHYSICAL_ADDRESS  Address
) {
    UINT64               *Ptr;
    UINT64                TestPattern;
    BOOLEAN               isFaulty;


    if (!IsPageEmpty (Address)) {
        // Consider "good" as in use and not empty
        return TRUE;
    }

    Ptr = (UINT64 *) Address;
    TestPattern = 0x5555555555555555ULL;
    *Ptr = TestPattern;

    isFaulty = (*Ptr != TestPattern);

    // Restore to zero (assumes page was previously empty)
    *Ptr = 0;

    return !isFaulty;
} // static BOOLEAN TestMemory()

static
EFI_MEMORY_TYPE GetMemoryTypeForAddress (
    EFI_PHYSICAL_ADDRESS   PageAddress,
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    UINTN                  MemoryMapSize,
    UINTN                  DescriptorSize
) {
    EFI_MEMORY_DESCRIPTOR *MemoryMapEntry;


    for (MemoryMapEntry = MemoryMap;
         (UINT8 *) MemoryMapEntry < (UINT8 *) MemoryMap + MemoryMapSize;
         MemoryMapEntry = (EFI_MEMORY_DESCRIPTOR *) ((UINT8 *) MemoryMapEntry + DescriptorSize)
    ) {
        if (PageAddress >= MemoryMapEntry->PhysicalStart &&
            PageAddress <  MemoryMapEntry->PhysicalStart + (MemoryMapEntry->NumberOfPages * EFI_PAGE_SIZE)
        ) {
            return MemoryMapEntry->Type;
        }
    } // for

    // Return non-qualifying memory type if not found in map
    return EfiReservedMemoryType;
} // static EFI_MEMORY_TYPE GetMemoryTypeForAddress()

static
VOID GetBadRamInfo (
    IN OUT CHAR16               **BadRamData  OPTIONAL,
    IN     EFI_PHYSICAL_ADDRESS   PageAddress,
    IN     EFI_PHYSICAL_ADDRESS   RangeStart
) {
    CHAR16                       *BadRamTemp;


    if (BadRamData == NULL) {
        BadRamTemp = PoolPrint (
            L"0x%lx:0x%lx",
            RangeStart,
            PageAddress - EFI_PAGE_SIZE
        );
    }
    else {
        BadRamTemp = PoolPrint (
            L"%s,0x%lx:0x%lx",
            BadRamData, RangeStart,
            PageAddress - EFI_PAGE_SIZE
        );

        MY_FREE_POOL(*BadRamData);
    }

    *BadRamData = BadRamTemp;
} // static VOID GetBadRamInfo()

EFI_STATUS ScanRAM (
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    UINTN                  MemoryMapSize,
    UINTN                  DescriptorSize,
    INTN                   OurFixType,
    BOOLEAN                OurFixWide,
    BOOLEAN                RecordOnly
) {
    #if REFIT_DEBUG > 0
    BOOLEAN                CheckMute = FALSE;
    #endif

    EFI_STATUS             Status;
    EFI_STATUS             XStatus;
    UINTN                  i;
    UINTN                  ListSize;
    CHAR16                *BadRamInfo;
    BOOLEAN                ValidTarget;
    BOOLEAN                CheckMemory;
    BOOLEAN                InBadRange;
    BOOLEAN                PageInUse;
    EFI_PHYSICAL_ADDRESS   PageAddress;
    EFI_PHYSICAL_ADDRESS   RangeStart;
    EFI_MEMORY_DESCRIPTOR *MemoryMapEntry;


    // Scan for, handle bad RAM and store addresses found
    RangeStart = 0;
    BadRamInfo = NULL;
    InBadRange = FALSE;
    Status     = EFI_SUCCESS;
    XStatus    = EFI_NOT_READY;

    for (MemoryMapEntry = MemoryMap;
         (UINT8 *) MemoryMapEntry < (UINT8 *) MemoryMap + MemoryMapSize;
         MemoryMapEntry = (EFI_MEMORY_DESCRIPTOR *) ((UINT8 *) MemoryMapEntry + DescriptorSize)
    ) {
        PageAddress = 0;
        ValidTarget = IsQualifyingTarget (MemoryMapEntry->Type, OurFixWide);
        if (ValidTarget) {
            if (XStatus == EFI_NOT_READY) {
                XStatus = EFI_SUCCESS;
            }

            for (i = 0; i < MemoryMapEntry->NumberOfPages; i++) {
                PageAddress = MemoryMapEntry->PhysicalStart + (i * EFI_PAGE_SIZE);
                CheckMemory = TestMemory (PageAddress);
                PageInUse = IsPageInUse (
                    PageAddress, MemoryMap,
                    MemoryMapSize, DescriptorSize, OurFixWide
                );

                if (!CheckMemory && !PageInUse) {
                    // Start of new bad range
                    if (!InBadRange) {
                        InBadRange = TRUE;
                        RangeStart = PageAddress;
                    }

                    if (!RecordOnly) {
                        // Mark page as unusable
                        Status = MarkPageUnusable (PageAddress);
                    }
                }
                else {
                    if (InBadRange) {
                        // End of bad range ... reset
                        InBadRange = FALSE;

                        GetBadRamInfo (
                            &BadRamInfo, RangeStart, PageAddress
                        );
                    }

                    if (OurFixType == 8 && !RecordOnly) {
                        // Mark page as reserved
                        Status = MarkPageReserved (PageAddress);
                    }
                }

                if (!EFI_ERROR(XStatus) && EFI_ERROR(Status)) {
                    XStatus = Status;
                }
            } // for i

            if (InBadRange) {
                // Handle any remaining bad range
                InBadRange = FALSE;

                GetBadRamInfo (
                    &BadRamInfo, RangeStart, PageAddress
                );
            }
        }
    } // for

    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_SET;
    #endif
    if (BadRamInfo == NULL) {
        BadRamInfo = StrDuplicate (L"AllGood");
    }

    ListSize = (StrLen (BadRamInfo) + 1) * sizeof (CHAR16);
    EfivarSetRaw (
        &RefindPlusGuid, L"BadRamInfo",
        BadRamInfo, ListSize, TRUE
    );
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    MY_FREE_POOL(MemoryMap);

    return XStatus;
} // EFI_STATUS ScanRAM()

static
EFI_STATUS ProcessPages (
    EFI_PHYSICAL_ADDRESS   StartAddress,
    UINTN                  NumPages,
    INTN                   OurFixType,
    BOOLEAN                OurFixWide
) {
    EFI_STATUS             Status;
    EFI_STATUS             XStatus;
    UINTN                  i;
    UINTN                  MemoryMapSize;
    UINTN                  DescriptorSize;
    BOOLEAN                GotUsedPages;
    BOOLEAN                ValidTarget;
    BOOLEAN                PageInUse;
    EFI_MEMORY_TYPE        MemoryType;
    EFI_PHYSICAL_ADDRESS   PageAddress;
    EFI_MEMORY_DESCRIPTOR *MemoryMap;


    MemoryMap = NULL;
    MemoryMapSize = 0;

    Status = GetMemoryMapWithBuffer (
        &MemoryMap, &MemoryMapSize, &DescriptorSize
    );
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (OurFixType == 7 || OurFixType == 8) {
        // Types 7 and 8
        // Previous nvRAM check blank
        XStatus = ScanRAM (
            MemoryMap, MemoryMapSize, DescriptorSize,
            OurFixType, OurFixWide, FALSE
        );
    }
    else {
        XStatus = EFI_SUCCESS;

        for (i = 0; i < NumPages; i++) {
            PageAddress = StartAddress + (EFI_PAGE_SIZE * i);
            PageInUse = IsPageInUse (
                PageAddress, MemoryMap,
                MemoryMapSize, DescriptorSize, OurFixWide
            );

            if (OurFixType == 1) {
                Status = MarkPageUnusable (PageAddress);
            }
            else if (OurFixType == 2) {
                Status = MarkPageReserved (PageAddress);
            }
            else { // Types 3 to 6
                MemoryType = GetMemoryTypeForAddress (
                    PageAddress, MemoryMap,
                    MemoryMapSize, DescriptorSize
                );
                ValidTarget = IsQualifyingTarget (MemoryType, OurFixWide);

                if (OurFixType == 3 ||
                    OurFixType == 4
                ) {
                    GotUsedPages = (PageInUse || !ValidTarget) ? TRUE : FALSE;

                    if (OurFixType == 3 && GotUsedPages) {
                        XStatus = EFI_BAD_BUFFER_SIZE;
                    }
                    else {
                        for (i = 0; i < NumPages; i++) {
                            PageAddress = StartAddress + (EFI_PAGE_SIZE * i);

                            if (GotUsedPages) {
                                Status = MarkPageReserved (PageAddress);
                            }
                            else {
                                Status = MarkPageUnusable (PageAddress);
                            }

                            if (EFI_ERROR(Status)) {
                                XStatus = Status;
                            }
                        } // for
                    }

                    break; // Outer loop
                }
                else if (
                    OurFixType == 5 ||
                    OurFixType == 6
                ) {
                    if (!PageInUse && ValidTarget) {
                        Status = MarkPageUnusable (PageAddress);
                    }
                    else if (OurFixType == 6) {
                        Status = MarkPageReserved (PageAddress);
                    }
                }
            }

            if (EFI_ERROR(Status)) {
                XStatus = Status;
            }
        } // for i
    }

    MY_FREE_POOL(MemoryMap);

    return XStatus;
} // static EFI_STATUS ProcessPages()

EFI_STATUS ManageBadRam (
    CHAR16                *OurFixList,
    INTN                   OurFixType,
    BOOLEAN                OurFixWide
) {
    #if REFIT_DEBUG > 0
    BOOLEAN                CheckMute = FALSE;
    #endif

    EFI_STATUS             Status;
    EFI_STATUS             XStatus;
    UINTN                  i;
    UINTN                  NumPages;
    CHAR16                *AddressPair;
    CHAR16                *AddressOne;   // Do *NOT* Free
    CHAR16                *AddressTwo;   // Do *NOT* Free
    CHAR16                *BadRamInfo;
    EFI_PHYSICAL_ADDRESS   TopBadRAM;
    EFI_PHYSICAL_ADDRESS   EndBadRAM;


    XStatus = EFI_NOT_READY;

    if (OurFixType < 0 || OurFixType > 8) {
        // Disable for invalid settings
        OurFixType = 0;
    }
    else if (OurFixType == 7 || OurFixType == 8) {
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif
        Status = EfivarGetRaw (
            &RefindPlusGuid, L"BadRamInfo",
            (VOID **) &BadRamInfo, NULL
        );
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif

        if (!EFI_ERROR(Status)) {
            // Previously scanned RAM and saved results to nvRAM
            if (MyStriCmp (BadRamInfo, L":0x")) {
                OurFixList = BadRamInfo;
                // Bad RAM Addresses were found on previous scan
                // Set Type to 5 or 6 for actual execution
                OurFixType -= 2;
            }
            else {
                // Bad RAM Addresses were *NOT* found on previous scan
                // Free variable and set Type to 0 (skip execution)
                MY_FREE_POOL(BadRamInfo);
                OurFixType = 0;

                XStatus = EFI_ALREADY_STARTED;
            }
        }
    }

    do {
        if (OurFixType == 0) {
            if (XStatus != EFI_ALREADY_STARTED) {
                XStatus  = EFI_NOT_STARTED;
            }

            break;
        }

        i = 0;
        while (1) {
            AddressPair = FindCommaDelimited (
                OurFixList, i++
            );
            if (AddressPair == NULL) break;

            if (!MyStrStr (AddressPair, L":0x")) {
                XStatus = EFI_INVALID_PARAMETER;
            }
            else {
                AddressOne = GetSubStrBefore (L":", AddressPair);
                AddressTwo = GetSubStrAfter  (L":", AddressPair);

                TopBadRAM = StrToHex (AddressOne, 0, 16);
                if (TopBadRAM == 0) {
                    XStatus = EFI_INVALID_PARAMETER;
                }
                else {
                    EndBadRAM = StrToHex (AddressTwo, 0, 16);
                    if (EndBadRAM < TopBadRAM) {
                        XStatus = EFI_INVALID_PARAMETER;
                    }
                    else {
                        if (XStatus == EFI_NOT_READY) {
                            XStatus = EFI_SUCCESS;
                        }

                        // Convert the addresses to pages and calculate the total number of pages
                        TopBadRAM /= EFI_PAGE_SIZE;              // Convert top address to page number
                        EndBadRAM /= EFI_PAGE_SIZE;              // Convert end address to page number
                        NumPages   = EndBadRAM - TopBadRAM + 1;  // Calculate total pages (start to end)
                        TopBadRAM *= EFI_PAGE_SIZE;              // Change page number to physical address
                                                                 // Must not be higher than 0xfffffffffffff000
/**
                        EndBadRAM += 1;                          // Change page number to physical address Step 1 ... Not used
                        EndBadRAM *= EFI_PAGE_SIZE;              // Change page number to physical address Step 2 ... Not used
                        EndBadRAM -= 1;                          // Change page number to physical address Step 3 ... Not used
**/
                        // Handle defined region
                        Status = ProcessPages (
                            TopBadRAM, NumPages,
                            OurFixType, OurFixWide
                        );

                        if (EFI_ERROR(Status)) {
                            XStatus = Status;
                        }
                    }
                }
            }

            MY_FREE_POOL(AddressPair);
        } // while {Infinite}
    } while (0); // This 'loop' only runs once

    #if REFIT_DEBUG > 0
    LOG_MSG("M A R K   D E F E C T I V E   M E M O R Y");
    LOG_MSG("\n");
    LOG_MSG("INFO: Tag Bad RAM Regions ... %r", XStatus);
    LOG_MSG("\n\n");
    #endif

    return XStatus;
} // EFI_STATUS ManageBadRam()
