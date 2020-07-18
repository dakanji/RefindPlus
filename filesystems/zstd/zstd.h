/*
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

#ifndef ZSTD_H
#define ZSTD_H

/* ======   Dependency   ======*/


/*-*****************************************************************************
 * Introduction
 *
 * zstd, short for Zstandard, is a fast lossless compression algorithm,
 * targeting real-time compression scenarios at zlib-level and better
 * compression ratios. The zstd compression library provides in-memory
 * compression and decompression functions. The library supports compression
 * levels from 1 up to ZSTD_maxCLevel() which is 22. Levels >= 20, labeled
 * ultra, should be used with caution, as they require more memory.
 * Compression can be done in:
 *  - a single step, reusing a context (described as Explicit memory management)
 *  - unbounded multiple steps (described as Streaming compression)
 * The compression ratio achievable on small data can be highly improved using
 * compression with a dictionary in:
 *  - a single step (described as Simple dictionary API)
 *  - a single step, reusing a dictionary (described as Fast dictionary API)
 ******************************************************************************/

/*======  Helper functions  ======*/

/**
 * enum ZSTD_ErrorCode - zstd error codes
 *
 * Functions that return size_t can be checked for errors using ZSTD_isError()
 * and the ZSTD_ErrorCode can be extracted using ZSTD_getErrorCode().
 */
typedef enum {
	ZSTD_error_no_error,
	ZSTD_error_GENERIC,
	ZSTD_error_prefix_unknown,
	ZSTD_error_frameParameter_unsupported,
	ZSTD_error_frameParameter_windowTooLarge,
	ZSTD_error_dstSize_tooSmall,
	ZSTD_error_srcSize_wrong,
	ZSTD_error_corruption_detected,
	ZSTD_error_checksum_wrong,
	ZSTD_error_tableLog_tooLarge,
	ZSTD_error_maxSymbolValue_tooLarge,
	ZSTD_error_maxSymbolValue_tooSmall,
	ZSTD_error_dictionary_corrupted,
	ZSTD_error_dictionary_wrong,
	ZSTD_error_maxCode
} ZSTD_ErrorCode;

/**
 * ZSTD_maxCLevel() - maximum compression level available
 *
 * Return: Maximum compression level available.
 */
int ZSTD_maxCLevel(void);
/**
 * ZSTD_compressBound() - maximum compressed size in worst case scenario
 * @srcSize: The size of the data to compress.
 *
 * Return:   The maximum compressed size in the worst case scenario.
 */
size_t ZSTD_compressBound(size_t srcSize);
/**
 * ZSTD_isError() - tells if a size_t function result is an error code
 * @code:  The function result to check for error.
 *
 * Return: Non-zero iff the code is an error.
 */
static __attribute__((unused)) unsigned int ZSTD_isError(size_t code)
{
	return code > (size_t)-ZSTD_error_maxCode;
}
/**
 * ZSTD_getErrorCode() - translates an error function result to a ZSTD_ErrorCode
 * @functionResult: The result of a function for which ZSTD_isError() is true.
 *
 * Return:          The ZSTD_ErrorCode corresponding to the functionResult or 0
 *                  if the functionResult isn't an error.
 */
static __attribute__((unused)) ZSTD_ErrorCode ZSTD_getErrorCode(
	size_t functionResult)
{
	if (!ZSTD_isError(functionResult))
		return (ZSTD_ErrorCode)0;
	return (ZSTD_ErrorCode)(0 - functionResult);
}

/**
 * enum ZSTD_strategy - zstd compression search strategy
 *
 * From faster to stronger.
 */
typedef enum {
	ZSTD_fast,
	ZSTD_dfast,
	ZSTD_greedy,
	ZSTD_lazy,
	ZSTD_lazy2,
	ZSTD_btlazy2,
	ZSTD_btopt,
	ZSTD_btopt2
} ZSTD_strategy;

/**
 * struct ZSTD_compressionParameters - zstd compression parameters
 * @windowLog:    Log of the largest match distance. Larger means more
 *                compression, and more memory needed during decompression.
 * @chainLog:     Fully searched segment. Larger means more compression, slower,
 *                and more memory (useless for fast).
 * @hashLog:      Dispatch table. Larger means more compression,
 *                slower, and more memory.
 * @searchLog:    Number of searches. Larger means more compression and slower.
 * @searchLength: Match length searched. Larger means faster decompression,
 *                sometimes less compression.
 * @targetLength: Acceptable match size for optimal parser (only). Larger means
 *                more compression, and slower.
 * @strategy:     The zstd compression strategy.
 */
typedef struct {
	unsigned int windowLog;
	unsigned int chainLog;
	unsigned int hashLog;
	unsigned int searchLog;
	unsigned int searchLength;
	unsigned int targetLength;
	ZSTD_strategy strategy;
} ZSTD_compressionParameters;

/**
 * struct ZSTD_frameParameters - zstd frame parameters
 * @contentSizeFlag: Controls whether content size will be present in the frame
 *                   header (when known).
 * @checksumFlag:    Controls whether a 32-bit checksum is generated at the end
 *                   of the frame for error detection.
 * @noDictIDFlag:    Controls whether dictID will be saved into the frame header
 *                   when using dictionary compression.
 *
 * The default value is all fields set to 0.
 */
typedef struct {
	unsigned int contentSizeFlag;
	unsigned int checksumFlag;
	unsigned int noDictIDFlag;
} ZSTD_frameParameters;

/**
 * struct ZSTD_parameters - zstd parameters
 * @cParams: The compression parameters.
 * @fParams: The frame parameters.
 */
typedef struct {
	ZSTD_compressionParameters cParams;
	ZSTD_frameParameters fParams;
} ZSTD_parameters;

/**
 * ZSTD_getCParams() - returns ZSTD_compressionParameters for selected level
 * @compressionLevel: The compression level from 1 to ZSTD_maxCLevel().
 * @estimatedSrcSize: The estimated source size to compress or 0 if unknown.
 * @dictSize:         The dictionary size or 0 if a dictionary isn't being used.
 *
 * Return:            The selected ZSTD_compressionParameters.
 */
ZSTD_compressionParameters ZSTD_getCParams(int compressionLevel,
	unsigned long long estimatedSrcSize, size_t dictSize);

/**
 * ZSTD_getParams() - returns ZSTD_parameters for selected level
 * @compressionLevel: The compression level from 1 to ZSTD_maxCLevel().
 * @estimatedSrcSize: The estimated source size to compress or 0 if unknown.
 * @dictSize:         The dictionary size or 0 if a dictionary isn't being used.
 *
 * The same as ZSTD_getCParams() except also selects the default frame
 * parameters (all zero).
 *
 * Return:            The selected ZSTD_parameters.
 */
ZSTD_parameters ZSTD_getParams(int compressionLevel,
	unsigned long long estimatedSrcSize, size_t dictSize);

/*-**************************
 * Streaming
 ***************************/

/**
 * struct ZSTD_inBuffer - input buffer for streaming
 * @src:  Start of the input buffer.
 * @size: Size of the input buffer.
 * @pos:  Position where reading stopped. Will be updated.
 *        Necessarily 0 <= pos <= size.
 */
typedef struct ZSTD_inBuffer_s {
	const void *src;
	size_t size;
	size_t pos;
} ZSTD_inBuffer;

/**
 * struct ZSTD_outBuffer - output buffer for streaming
 * @dst:  Start of the output buffer.
 * @size: Size of the output buffer.
 * @pos:  Position where writing stopped. Will be updated.
 *        Necessarily 0 <= pos <= size.
 */
typedef struct ZSTD_outBuffer_s {
	void *dst;
	size_t size;
	size_t pos;
} ZSTD_outBuffer;


/*-*****************************************************************************
 * Streaming decompression - HowTo
 *
 * A ZSTD_DStream object is required to track streaming operations.
 * Use ZSTD_initDStream() to initialize a ZSTD_DStream object.
 * ZSTD_DStream objects can be re-used multiple times.
 *
 * Use ZSTD_decompressStream() repetitively to consume your input.
 * The function will update both `pos` fields.
 * If `input->pos < input->size`, some input has not been consumed.
 * It's up to the caller to present again remaining data.
 * If `output->pos < output->size`, decoder has flushed everything it could.
 * Returns 0 iff a frame is completely decoded and fully flushed.
 * Otherwise it returns a suggested next input size that will never load more
 * than the current frame.
 ******************************************************************************/

/**
 * ZSTD_DStreamWorkspaceBound() - memory needed to initialize a ZSTD_DStream
 * @maxWindowSize: The maximum window size allowed for compressed frames.
 *
 * Return:         A lower bound on the size of the workspace that is passed to
 *                 ZSTD_initDStream().
 */
size_t ZSTD_DStreamWorkspaceBound(size_t maxWindowSize);

/**
 * struct ZSTD_DStream - the zstd streaming decompression context
 */
typedef struct ZSTD_DStream_s ZSTD_DStream;
/*===== ZSTD_DStream management functions =====*/
/**
 * ZSTD_initDStream() - initialize a zstd streaming decompression context
 * @maxWindowSize: The maximum window size allowed for compressed frames.
 * @workspace:     The workspace to emplace the context into. It must outlive
 *                 the returned context.
 * @workspaceSize: The size of workspace.
 *                 Use ZSTD_DStreamWorkspaceBound(maxWindowSize) to determine
 *                 how large the workspace must be.
 *
 * Return:         The zstd streaming decompression context.
 */
ZSTD_DStream *ZSTD_initDStream(size_t maxWindowSize, void *workspace,
	size_t workspaceSize);

/*===== Streaming decompression functions =====*/
/**
 * ZSTD_resetDStream() - reset the context using parameters from creation
 * @zds:   The zstd streaming decompression context to reset.
 *
 * Resets the context using the parameters from creation. Skips dictionary
 * loading, since it can be reused.
 *
 * Return: Zero or an error, which can be checked using ZSTD_isError().
 */
size_t ZSTD_resetDStream(ZSTD_DStream *zds);
/**
 * ZSTD_decompressStream() - streaming decompress some of input into output
 * @zds:    The zstd streaming decompression context.
 * @output: Destination buffer. `output.pos` is updated to indicate how much
 *          decompressed data was written.
 * @input:  Source buffer. `input.pos` is updated to indicate how much data was
 *          read. Note that it may not consume the entire input, in which case
 *          `input.pos < input.size`, and it's up to the caller to present
 *          remaining data again.
 *
 * The `input` and `output` buffers may be any size. Guaranteed to make some
 * forward progress if `input` and `output` are not empty.
 * ZSTD_decompressStream() will not consume the last byte of the frame until
 * the entire frame is flushed.
 *
 * Return:  Returns 0 iff a frame is completely decoded and fully flushed.
 *          Otherwise returns a hint for the number of bytes to use as the input
 *          for the next function call or an error, which can be checked using
 *          ZSTD_isError(). The size hint will never load more than the frame.
 */
size_t ZSTD_decompressStream(ZSTD_DStream *zds, ZSTD_outBuffer *output,
	ZSTD_inBuffer *input);

/**
 * ZSTD_DStreamInSize() - recommended size for the input buffer
 *
 * Return: The recommended size for the input buffer.
 */
size_t ZSTD_DStreamInSize(void);
/**
 * ZSTD_DStreamOutSize() - recommended size for the output buffer
 *
 * When the output buffer is at least this large, it is guaranteed to be large
 * enough to flush at least one complete decompressed block.
 *
 * Return: The recommended size for the output buffer.
 */
size_t ZSTD_DStreamOutSize(void);


/* --- Constants ---*/
#define ZSTD_MAGICNUMBER            0xFD2FB528   /* >= v0.8.0 */
#define ZSTD_MAGIC_SKIPPABLE_START  0x184D2A50U

#define ZSTD_CONTENTSIZE_UNKNOWN (0ULL - 1)
#define ZSTD_CONTENTSIZE_ERROR   (0ULL - 2)

#define ZSTD_WINDOWLOG_MAX_32  27
#define ZSTD_WINDOWLOG_MAX_64  27
#define ZSTD_WINDOWLOG_MAX \
	((unsigned int)(sizeof(size_t) == 4 \
		? ZSTD_WINDOWLOG_MAX_32 \
		: ZSTD_WINDOWLOG_MAX_64))
#define ZSTD_WINDOWLOG_MIN 10
#define ZSTD_HASHLOG_MAX ZSTD_WINDOWLOG_MAX
#define ZSTD_HASHLOG_MIN        6
#define ZSTD_CHAINLOG_MAX     (ZSTD_WINDOWLOG_MAX+1)
#define ZSTD_CHAINLOG_MIN      ZSTD_HASHLOG_MIN
#define ZSTD_HASHLOG3_MAX      17
#define ZSTD_SEARCHLOG_MAX    (ZSTD_WINDOWLOG_MAX-1)
#define ZSTD_SEARCHLOG_MIN      1
/* only for ZSTD_fast, other strategies are limited to 6 */
#define ZSTD_SEARCHLENGTH_MAX   7
/* only for ZSTD_btopt, other strategies are limited to 4 */
#define ZSTD_SEARCHLENGTH_MIN   3
#define ZSTD_TARGETLENGTH_MIN   4
#define ZSTD_TARGETLENGTH_MAX 999

/* for static allocation */
#define ZSTD_FRAMEHEADERSIZE_MAX 18
#define ZSTD_FRAMEHEADERSIZE_MIN  6
static const size_t ZSTD_frameHeaderSize_prefix = 5;
static const size_t ZSTD_frameHeaderSize_min = ZSTD_FRAMEHEADERSIZE_MIN;
static const size_t ZSTD_frameHeaderSize_max = ZSTD_FRAMEHEADERSIZE_MAX;
/* magic number + skippable frame length */
static const size_t ZSTD_skippableHeaderSize = 8;


/*-*************************************
 * Compressed size functions
 **************************************/

/**
 * ZSTD_findFrameCompressedSize() - returns the size of a compressed frame
 * @src:     Source buffer. It should point to the start of a zstd encoded frame
 *           or a skippable frame.
 * @srcSize: The size of the source buffer. It must be at least as large as the
 *           size of the frame.
 *
 * Return:   The compressed size of the frame pointed to by `src` or an error,
 *           which can be check with ZSTD_isError().
 *           Suitable to pass to ZSTD_decompress() or similar functions.
 */
size_t ZSTD_findFrameCompressedSize(const void *src, size_t srcSize);

/*-*************************************
 * Decompressed size functions
 **************************************/
/**
 * struct ZSTD_frameParams - zstd frame parameters stored in the frame header
 * @frameContentSize: The frame content size, or 0 if not present.
 * @windowSize:       The window size, or 0 if the frame is a skippable frame.
 * @dictID:           The dictionary id, or 0 if not present.
 * @checksumFlag:     Whether a checksum was used.
 */
typedef struct {
	unsigned long long frameContentSize;
	unsigned int windowSize;
	unsigned int dictID;
	unsigned int checksumFlag;
} ZSTD_frameParams;

/**
 * ZSTD_getFrameParams() - extracts parameters from a zstd or skippable frame
 * @fparamsPtr: On success the frame parameters are written here.
 * @src:        The source buffer. It must point to a zstd or skippable frame.
 * @srcSize:    The size of the source buffer. `ZSTD_frameHeaderSize_max` is
 *              always large enough to succeed.
 *
 * Return:      0 on success. If more data is required it returns how many bytes
 *              must be provided to make forward progress. Otherwise it returns
 *              an error, which can be checked using ZSTD_isError().
 */
size_t ZSTD_getFrameParams(ZSTD_frameParams *fparamsPtr, const void *src,
	size_t srcSize);

/*-*****************************************************************************
 * Block functions
 *
 * Block functions produce and decode raw zstd blocks, without frame metadata.
 * Frame metadata cost is typically ~18 bytes, which can be non-negligible for
 * very small blocks (< 100 bytes). User will have to take in charge required
 * information to regenerate data, such as compressed and content sizes.
 *
 * A few rules to respect:
 * - Compressing and decompressing require a context structure
 *   + Use ZSTD_initCCtx() and ZSTD_initDCtx()
 * - It is necessary to init context before starting
 *   + compression : ZSTD_compressBegin()
 *   + decompression : ZSTD_decompressBegin()
 *   + variants _usingDict() are also allowed
 *   + copyCCtx() and copyDCtx() work too
 * - Block size is limited, it must be <= ZSTD_getBlockSizeMax()
 *   + If you need to compress more, cut data into multiple blocks
 *   + Consider using the regular ZSTD_compress() instead, as frame metadata
 *     costs become negligible when source size is large.
 * - When a block is considered not compressible enough, ZSTD_compressBlock()
 *   result will be zero. In which case, nothing is produced into `dst`.
 *   + User must test for such outcome and deal directly with uncompressed data
 *   + ZSTD_decompressBlock() doesn't accept uncompressed data as input!!!
 *   + In case of multiple successive blocks, decoder must be informed of
 *     uncompressed block existence to follow proper history. Use
 *     ZSTD_insertBlock() in such a case.
 ******************************************************************************/

/* Define for static allocation */
#define ZSTD_BLOCKSIZE_ABSOLUTEMAX (128 * 1024)

#endif  /* ZSTD_H */
