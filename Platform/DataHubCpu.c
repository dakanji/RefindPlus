//
/// @file rEFIt_UEFI/Platform/DataHubCpu.c
///
/// VirtualBox CPU descriptors
///
/// VirtualBox CPU descriptors also used to set OS X-used NVRAM variables and DataHub data
///

// Copyright(C) 2009-2010 Oracle Corporation
//
// This file is part of VirtualBox Open Source Edition(OSE), as
// available from http://www.virtualbox.org. This file is free software;
// you can redistribute it and/or modify it under the terms of the GNU
// General Public License(GPL) as published by the Free Software
// Foundation, in version 2 as it comes in the "COPYING" file of the
// VirtualBox OSE distribution. VirtualBox OSE is distributed in the
// hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

//
// CHANGELOG:
//
// 2014/11/20
// Download-Fritz
// Removed commented out code in rev 2963 and 2965
//

#ifndef DEBUG_ALL
#define DEBUG_DH 1
#else
#define DEBUG_DH DEBUG_ALL
#endif

#if DEBUG_DH == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_DH, __VA_ARGS__)	
#endif


#include "Platform.h"
//#include "Version.h"

#include <Guid/DataHubRecords.h>

#define EFI_CPU_DATA_MAXIMUM_LENGTH 0x100

// gDataHub
/// A pointer to the DataHubProtocol
EFI_DATA_HUB_PROTOCOL     *gDataHub;

EFI_SUBCLASS_TYPE1_HEADER mCpuDataRecordHeader = {
  EFI_PROCESSOR_SUBCLASS_VERSION,       // Version
  sizeof(EFI_SUBCLASS_TYPE1_HEADER),    // Header Size
  0,                                    // Instance (initialize later)
  EFI_SUBCLASS_INSTANCE_NON_APPLICABLE, // SubInstance
  0                                     // RecordType (initialize later)
};

// gDataHubPlatformGuid
/// The GUID of the DataHubProtocol
EFI_GUID gDataHubPlatformGuid = {
  0x64517cc8, 0x6561, 0x4051, { 0xb0, 0x3c, 0x59, 0x64, 0xb6, 0x0f, 0x4c, 0x7a }
};

extern EFI_GUID gDataHubPlatformGuid;

typedef union {
  EFI_CPU_DATA_RECORD *DataRecord;
  UINT8               *Raw;
} EFI_CPU_DATA_RECORD_BUFFER;

// PLATFORM_DATA
/// The struct passed to "LogDataHub" holing key and value to be added
#pragma pack(1)
typedef struct {
  EFI_SUBCLASS_TYPE1_HEADER Hdr;     /// 0x48
  UINT32                    NameLen; /// 0x58 (in bytes)
  UINT32                    ValLen;  /// 0x5c
  UINT8                     Data[1]; /// 0x60 Name Value
} PLATFORM_DATA;
#pragma pack()

// CopyRecord
/// Copy the data provided in arguments into a PLATFORM_DATA buffer
///
/// @param Rec    The buffer the data should be copied into
/// @param Name   The value for the member "name"
/// @param Val    The data the object should have
/// @param ValLen The length of the parameter "Val"
///
/// @return The size of the new PLATFORM_DATA object is returned
UINT32 EFIAPI
CopyRecord(IN        PLATFORM_DATA *Rec,
           IN  CONST CHAR16        *Name,
           IN        VOID          *Val,
           IN        UINT32        ValLen)
{
  CopyMem(&Rec->Hdr, &mCpuDataRecordHeader, sizeof(EFI_SUBCLASS_TYPE1_HEADER));
  Rec->NameLen = (UINT32)StrLen(Name) * sizeof(CHAR16);
  Rec->ValLen  = ValLen;
  CopyMem(Rec->Data,                Name, Rec->NameLen);
  CopyMem(Rec->Data + Rec->NameLen, Val,  ValLen);

  return (sizeof(EFI_SUBCLASS_TYPE1_HEADER) + 8 + Rec->NameLen + Rec->ValLen);
}

// LogDataHub
/// Adds a key-value-pair to the DataHubProtocol
EFI_STATUS EFIAPI
LogDataHub(IN  EFI_GUID *TypeGuid,
           IN  CHAR16   *Name,
           IN  VOID     *Data,
           IN  UINT32   DataSize)
{
  UINT32        RecordSize;
  EFI_STATUS    Status;
  PLATFORM_DATA *PlatformData;
  
  PlatformData = (PLATFORM_DATA*)AllocatePool(sizeof(PLATFORM_DATA) + DataSize + EFI_CPU_DATA_MAXIMUM_LENGTH);
  if (PlatformData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  
  RecordSize = CopyRecord(PlatformData, Name, Data, DataSize);
  Status     = gDataHub->LogData(gDataHub,
                                 TypeGuid,                   // DataRecordGuid				
                                 &gDataHubPlatformGuid,      // ProducerName (always)
                                 EFI_DATA_RECORD_CLASS_DATA,
                                 PlatformData,
                                 RecordSize);
  
  FreePool(PlatformData);
  return Status;
}
