/**
 * \file fsw_efi.h
 * EFI host environment header.
*/

/*-
  * Copyright (c) 2006 Christoph Pfisterer
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

#ifndef _FSW_EFI_H_
#define _FSW_EFI_H_

#include "fsw_core.h"

#ifdef __MAKEWITH_GNUEFI
#define CompareGuid(a, b) CompareGuid(a, b)==0
#endif

#define REFIND_EFI_DISK_IO_PROTOCOL_GUID \
  { \
    0xce345171, 0xba0b, 0x11d2, {0x8e, 0x4f, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

#define REFIND_EFI_BLOCK_IO_PROTOCOL_GUID \
  { \
    0x964e5b21, 0x6459, 0x11d2, {0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

/**
 * EFI Host: Private per-volume structure.
 */

typedef struct {
    UINT64                      Signature;      //!< Used to identify this structure

    EFI_FILE_IO_INTERFACE       FileSystem;     //!< Published EFI protocol interface structure

    EFI_HANDLE                  Handle;         //!< The device handle the protocol is attached to
    EFI_DISK_IO                 *DiskIo;        //!< The Disk I/O protocol we use for disk access
    UINT32                      MediaId;        //!< The media ID from the Block I/O protocol
    EFI_STATUS                  LastIOStatus;   //!< Last status from Disk I/O

    struct fsw_volume           *vol;           //!< FSW volume structure

} FSW_VOLUME_DATA;

/** Signature for the volume structure. */
#define FSW_VOLUME_DATA_SIGNATURE  EFI_SIGNATURE_32 ('f', 's', 'w', 'V')
/** Access macro for the volume structure. */
#define FSW_VOLUME_FROM_FILE_SYSTEM(a)  CR (a, FSW_VOLUME_DATA, FileSystem, FSW_VOLUME_DATA_SIGNATURE)

/**
 * EFI Host: Private structure for a EFI_FILE_PROTOCOL interface.
 */

typedef struct {
    UINT64                      Signature;      //!< Used to identify this structure

    EFI_FILE_PROTOCOL           FileHandle;     //!< Published EFI protocol interface structure

    UINT64                       Type;           //!< File type used for dispatching
    struct fsw_shandle          shand;          //!< FSW handle for this file

} FSW_FILE_DATA;

/** File type: regular file. */
#define FSW_EFI_FILE_TYPE_FILE  (0)
/** File type: directory. */
#define FSW_EFI_FILE_TYPE_DIR   (1)

/** Signature for the file handle structure. */
#define FSW_FILE_DATA_SIGNATURE    EFI_SIGNATURE_32 ('f', 's', 'w', 'F')
/** Access macro for the file handle structure. */
#define FSW_FILE_FROM_FILE_HANDLE(a)  CR (a, FSW_FILE_DATA, FileHandle, FSW_FILE_DATA_SIGNATURE)


//
// Library functions
//

VOID fsw_efi_decode_time(OUT EFI_TIME *EfiTime, IN UINT32 UnixTime);

UINTN fsw_efi_strsize(struct fsw_string *s);
VOID fsw_efi_strcpy(CHAR16 *Dest, struct fsw_string *src);
VOID EFIAPI fsw_efi_clear_cache(VOID);

#endif
