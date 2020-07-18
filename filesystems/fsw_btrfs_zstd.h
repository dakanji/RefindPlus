// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 */

#define PAGE_SIZE 4096
#define uint64_t fsw_u64
#define uint32_t fsw_u32
#define uint16_t fsw_u16
#define int16_t fsw_s16
#define uintptr_t unsigned long
#define sys_memmove fsw_memcpy

static inline uint16_t get_unaligned_le16(const void *s)
{
	const unsigned char *p = (const unsigned char *)s;
	return p[0]+ (p[1]<<8);
}

static inline uint32_t get_unaligned_le32(const void *s)
{
	const unsigned char *p = (const unsigned char *)s;
	return p[0]+ (p[1]<<8) + (p[2]<<16) + (p[3]<<24);
}

static inline uint64_t get_unaligned_le64(const void *s)
{
	const unsigned char *p = (const unsigned char *)s;
	uint64_t v0 = get_unaligned_le32(p);
	uint64_t v1 = get_unaligned_le32(p+4);
	return v0 + (v1<<32);
}

static inline void put_unaligned_le16(uint16_t v, void *s)
{
	unsigned char *p = (unsigned char *)s;
	p[0] = v & 0xff;
	p[1] = v >> 8;
}

#define UP_U32(a)	(((a)+3) >> 2)

#include "zstd/xxhash64.c"
#include "zstd/zstd_decompress.c"
#include "zstd/fse_decompress.c"
#include "zstd/huf_decompress.c"

#define ZSTD_BTRFS_MAX_WINDOWLOG 17
#define ZSTD_BTRFS_MAX_INPUT (1 << ZSTD_BTRFS_MAX_WINDOWLOG)


static fsw_ssize_t zstd_decompress(char *data_in, fsw_size_t srclen,
		grub_off_t start_byte,
		char *data_out, fsw_size_t destlen)
{
	ZSTD_DStream *stream;
	ZSTD_inBuffer in_buf;
	ZSTD_outBuffer out_buf;
	fsw_ssize_t ret = 0;
	size_t ret2;

	size_t workspace_size = ZSTD_DStreamWorkspaceBound(ZSTD_BTRFS_MAX_INPUT);
	void * workspace;
	
	in_buf.src = data_in;
	in_buf.pos = 0;
	in_buf.size = srclen;

	out_buf.dst = NULL;

	workspace = AllocatePool(workspace_size);

	if(!workspace) {
		ret = -FSW_OUT_OF_MEMORY;
		goto finish;
	}

	stream = ZSTD_initDStream(ZSTD_BTRFS_MAX_INPUT, workspace, workspace_size);
	if (!stream) {
		DPRINT(L"BTRFS: ZSTD_initDStream failed\n");
		ret = -FSW_OUT_OF_MEMORY;
		goto finish;
	}

	while(start_byte > 0) {
	    out_buf.size = start_byte < PAGE_SIZE ? start_byte : PAGE_SIZE;
	    if(out_buf.dst == NULL)
		out_buf.dst = AllocatePool(out_buf.size);
	    out_buf.pos = 0;

	    ret2 = ZSTD_decompressStream(stream, &out_buf, &in_buf);
	    if (ZSTD_isError(ret2)) {
		DPRINT(L"BTRFS: ZSTD_decompressStream returned %d\n", ZSTD_getErrorCode(ret2));
		ret = -FSW_VOLUME_CORRUPTED;
		goto finish;
	    }

	    if(out_buf.pos == 0 && in_buf.pos == in_buf.size) {
		DPRINT(L"BTRFS: ZSTD_decompressStream ended early\n");
		ret = -FSW_VOLUME_CORRUPTED;
		goto finish;
	    }

	    start_byte -= out_buf.pos;
	}

	if(out_buf.dst)
		FreePool(out_buf.dst);

	out_buf.dst = data_out;
	out_buf.size = destlen;
	out_buf.pos = 0;

	ret2 = ZSTD_decompressStream(stream, &out_buf, &in_buf);
	if (ZSTD_isError(ret2)) {
	    DPRINT(L"BTRFS: ZSTD_decompressStream returned %d\n", ZSTD_getErrorCode(ret2));
	    ret = -FSW_VOLUME_CORRUPTED;
	    goto finish;
	}

	ret = destlen;
finish:
	if(out_buf.dst && out_buf.dst != data_out)
		FreePool(out_buf.dst);
	if(workspace)
		FreePool(workspace);
	if (out_buf.pos < destlen)
		memset(data_out + out_buf.pos, 0, destlen - out_buf.pos);
	return ret;
}
