/*
 * refind/log.h
 * 
 * Definitions to handle rEFInd's logging facility, activated by setting
 * log_level in refind.conf.
 * 
 */
/*
 * Copyright (c) 2012-2020 Roderick W. Smith
 *
 * Distributed under the terms of the GNU General Public License (GPL)
 * version 3 (GPLv3), a copy of which must be distributed with this source
 * code or binaries made from it.
 *
 */

#ifndef __LOG_H_
#define __LOG_H_

#ifdef __MAKEWITH_GNUEFI
#include "efi.h"
#include "efilib.h"
#define EFI_DEVICE_PATH_PROTOCOL EFI_DEVICE_PATH
#else
#include "../include/tiano_includes.h"
#endif

extern CHAR16           *gLogTemp;

#define LOG_LINE_NORMAL      1
#define LOG_LINE_SEPARATOR   2
#define LOG_LINE_THIN_SEP    3

#define LOGFILE L"refind.log"
#define LOGFILE_OLD L"refind.log-old"

// Note: gLogTemp is freed within WriteToLog()
#define LOG(level, type, ...) \
    if (level <= GlobalConfig.LogLevel) { \
        gLogTemp = PoolPrint(__VA_ARGS__); \
        WriteToLog(&gLogTemp, type); \
    }

EFI_STATUS StartLogging(BOOLEAN Restart);
VOID StopLogging(VOID);
VOID WriteToLog(CHAR16 **Message, UINTN LogLineType);

#endif
