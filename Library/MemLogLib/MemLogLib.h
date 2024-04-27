/** @file
    Provides simple log services to memory buffer.
**/
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2023 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * THIS PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 */

#ifndef __MEMLOG_LIB_H__
#define __MEMLOG_LIB_H__


//
// Mem log sizes
//
#define MEM_LOG_INITIAL_SIZE    (128 * 1024)
#define MEM_LOG_MAX_SIZE        (10 * 1024 * 1024)
#define MEM_LOG_MAX_LINE_SIZE   1024


/** Callback that can be installed to be called when some message is printed with MemLog() or MemLogVA(). **/
typedef VOID (EFIAPI *MEM_LOG_CALLBACK) (IN INTN DebugMode, IN CHAR8 *LastMessage);


/**
  Prints a log message to memory buffer.

  @param  Timing      TRUE to prepend timing to log.
  @param  DebugMode   DebugMode will be passed to Callback function if it is set.
  @param  Format      The format string for the debug message to print.
  @param  Marker      VA_LIST with variable arguments for Format.

**/
VOID EFIAPI MemLogVA (
  IN  const BOOLEAN Timing,
  IN  const INTN    DebugMode,
  IN  const CHAR8   *Format,
  IN  VA_LIST       Marker
);

/**
  Prints a log message to memory buffer.

  If Format is NULL, then does nothing.

  @param  Timing      TRUE to prepend timing to log.
  @param  DebugMode   DebugMode will be passed to Callback function if it is set.
  @param  Format      The format string for the debug message to print.
  @param  ...         The variable argument list whose contents are accessed
                      based on the format string specified by Format.

**/
VOID EFIAPI MemLog (
  IN  const BOOLEAN Timing,
  IN  const INTN    DebugMode,
  IN  const CHAR8   *Format,
  ...
);


/**
  Returns pointer to MemLog buffer.
**/
CHAR8 * EFIAPI GetMemLogBuffer (VOID);


/**
  Returns the length of log (number of chars written) in mem buffer.
 **/
UINTN EFIAPI GetMemLogLen (VOID);


/**
  Sets callback that will be called when message is added to mem log.
 **/
VOID EFIAPI SetMemLogCallback (
  MEM_LOG_CALLBACK  Callback
);


/**
  Returns TSC ticks per second.
 **/
UINT64 EFIAPI GetMemLogTscTicksPerSecond (VOID);

UINT64 GetCurrentMS (VOID);

#if REFIT_DEBUG > 0
VOID EFIAPI DebugLog (
    IN const CHAR8 *FormatString,
    ...
);
#endif

#endif // __MEMLOG_LIB_H__
