/**
 * BootMaster/badram_fix.h
 *
 * Copyright (c) 2025 Dayo Akanji (sf.net/u/dakanji/profile)
 * Released under the MIT License
**/

#ifndef __REFINDPLUS_BADRAM_H_
#define __REFINDPLUS_BADRAM_H_

EFI_STATUS ManageBadRam (
    CHAR16                     *OurBadRamFixList,
    INTN                        OurBadRamFixType,
    BOOLEAN                     OurBadRamFixWide
);

EFI_STATUS ScanRAM (
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    UINTN                  MemoryMapSize,
    UINTN                  DescriptorSize,
    INTN                   OurFixType,
    BOOLEAN                OurFixWide,
    BOOLEAN                RecordOnly
);

#endif
