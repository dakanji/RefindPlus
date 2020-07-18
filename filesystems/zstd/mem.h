/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of https://github.com/facebook/zstd.
 * An additional grant of patent rights can be found in the PATENTS file in the
 * same directory.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation. This program is dual-licensed; you may select
 * either version 2 of the GNU General Public License ("GPL") or BSD license
 * ("BSD").
 */

#ifndef MEM_H_MODULE
#define MEM_H_MODULE

/*-****************************************
*  Dependencies
******************************************/

/*-****************************************
*  Compiler specifics
******************************************/
#define ZSTD_STATIC static __inline __attribute__((unused))

/*-**************************************************************
*  Basic Types
*****************************************************************/
typedef uint8_t BYTE;
typedef uint16_t U16;
typedef int16_t S16;
typedef uint32_t U32;
typedef int32_t S32;
typedef uint64_t U64;
typedef int64_t S64;
typedef ptrdiff_t iPtrDiff;
typedef uintptr_t uPtrDiff;

/*-**************************************************************
*  Memory I/O
*****************************************************************/
ZSTD_STATIC unsigned ZSTD_32bits(void) { return sizeof(size_t) == 4; }
ZSTD_STATIC unsigned ZSTD_64bits(void) { return sizeof(size_t) == 8; }


/*=== Little endian r/w ===*/

ZSTD_STATIC U16 ZSTD_readLE16(const void *memPtr) { return get_unaligned_le16(memPtr); }

ZSTD_STATIC void ZSTD_writeLE16(void *memPtr, U16 val) { put_unaligned_le16(val, memPtr); }

ZSTD_STATIC U32 ZSTD_readLE24(const void *memPtr) { return ZSTD_readLE16(memPtr) + (((const BYTE *)memPtr)[2] << 16); }

ZSTD_STATIC U32 ZSTD_readLE32(const void *memPtr) { return get_unaligned_le32(memPtr); }

ZSTD_STATIC U64 ZSTD_readLE64(const void *memPtr) { return get_unaligned_le64(memPtr); }

ZSTD_STATIC size_t ZSTD_readLEST(const void *memPtr)
{
	if (ZSTD_32bits())
		return (size_t)ZSTD_readLE32(memPtr);
	else
		return (size_t)ZSTD_readLE64(memPtr);
}

#endif /* MEM_H_MODULE */
