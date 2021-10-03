/*
 *  BootLog.c
 *
 *  Created by Slice  2011-08.19
 *  Edited by apianti 2012-09-08
 *
 *  Refactored by Dayo Akanji for RefindPlus
 */

#include "../include/tiano_includes.h"
#include "../Library/MemLogLib/MemLogLib.h"
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>
#include <Library/UefiBootServicesTableLib.h>
#include "../BootMaster/global.h"
#include "../BootMaster/lib.h"
#include "../BootMaster/mystrings.h"
#include "../include/refit_call_wrapper.h"

extern  EFI_GUID  gEfiMiscSubClassGuid;

extern  INT16  NowYear;
extern  INT16  NowMonth;
extern  INT16  NowDay;
extern  INT16  NowHour;
extern  INT16  NowMinute;
extern  INT16  NowSecond;

CHAR16  *gLogTemp  = NULL;

BOOLEAN  TimeStamp = TRUE;
BOOLEAN  UseMsgLog = FALSE;

static
CHAR16 * GetAltMonth (VOID) {
    CHAR16 *AltMonth = NULL;

    switch (NowMonth) {
        case 1:  AltMonth = L"b";  break;
        case 2:  AltMonth = L"d";  break;
        case 3:  AltMonth = L"f";  break;
        case 4:  AltMonth = L"h";  break;
        case 5:  AltMonth = L"j";  break;
        case 6:  AltMonth = L"k";  break;
        case 7:  AltMonth = L"n";  break;
        case 8:  AltMonth = L"p";  break;
        case 9:  AltMonth = L"r";  break;
        case 10: AltMonth = L"t";  break;
        case 11: AltMonth = L"v";  break;
        default: AltMonth = L"x";  break;
    } // switch

    return AltMonth;
} // CHAR16 * GetAltMonth()

static
CHAR16 * GetAltHour (VOID) {
    CHAR16 *AltHour = NULL;

    switch (NowHour) {
        case 0:  AltHour = L"a";  break;
        case 1:  AltHour = L"b";  break;
        case 2:  AltHour = L"c";  break;
        case 3:  AltHour = L"d";  break;
        case 4:  AltHour = L"e";  break;
        case 5:  AltHour = L"f";  break;
        case 6:  AltHour = L"g";  break;
        case 7:  AltHour = L"h";  break;
        case 8:  AltHour = L"i";  break;
        case 9:  AltHour = L"j";  break;
        case 10: AltHour = L"k";  break;
        case 11: AltHour = L"m";  break;
        case 12: AltHour = L"n";  break;
        case 13: AltHour = L"p";  break;
        case 14: AltHour = L"q";  break;
        case 15: AltHour = L"r";  break;
        case 16: AltHour = L"s";  break;
        case 17: AltHour = L"t";  break;
        case 18: AltHour = L"u";  break;
        case 19: AltHour = L"v";  break;
        case 20: AltHour = L"w";  break;
        case 21: AltHour = L"x";  break;
        case 22: AltHour = L"y";  break;
        default: AltHour = L"z";  break;
    } // switch

    return AltHour;
} // CHAR16 * GetAltHour()

static
CHAR16 * GetDateString (VOID) {
    static CHAR16 *DateStr = NULL;

    if (DateStr != NULL) {
        return DateStr;
    }

    INT16   ourYear    = (NowYear % 100);
    CHAR16  *ourMonth  = GetAltMonth();
    CHAR16  *ourHour   = GetAltHour();
    DateStr = PoolPrint(
        L"%02d%s%02d%s%02d%02d",
        ourYear, ourMonth,
        NowDay, ourHour,
        NowMinute, NowSecond
    );

    return DateStr;
} // CHAR16 * GetDateString()

static
EFI_FILE_PROTOCOL * GetDebugLogFile (VOID) {
    EFI_STATUS          Status;
    EFI_LOADED_IMAGE    *LoadedImage;
    EFI_FILE_PROTOCOL   *RootDir;
    EFI_FILE_PROTOCOL   *LogFile;
    CHAR16              *ourDebugLog = NULL;

    if (GlobalConfig.LogLevel < 0) {
        return NULL;
    }

    // get RootDir from device we are loaded from
    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol,
        gImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID **) &LoadedImage
    );

    if (EFI_ERROR(Status)) {
        return NULL;
    }

    if (!LoadedImage || !LoadedImage->DeviceHandle) {
        return NULL;
    }

    RootDir = EfiLibOpenRoot (LoadedImage->DeviceHandle);

    if (RootDir == NULL) {
        return NULL;
    }

    CHAR16 *DateStr = GetDateString();
    ourDebugLog     = PoolPrint (L"EFI\\%s.log", DateStr);

    // Open log file from current root
    Status = REFIT_CALL_5_WRAPPER(
        RootDir->Open, RootDir,
        &LogFile, ourDebugLog,
        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0
    );

    // If the log file is not found try to create it
    if (Status == EFI_NOT_FOUND) {
        Status = REFIT_CALL_5_WRAPPER(
            RootDir->Open, RootDir,
            &LogFile, ourDebugLog,
            EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0
        );
    }

    Status  = REFIT_CALL_1_WRAPPER(RootDir->Close, RootDir);
    RootDir = NULL;

    if (EFI_ERROR(Status)) {
        // try on first EFI partition
        Status = egFindESP (&RootDir);
        if (!EFI_ERROR(Status)) {
            Status = REFIT_CALL_5_WRAPPER(
                RootDir->Open, RootDir,
                &LogFile, ourDebugLog,
                EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0
            );

            // If the log file is not found try to create it
            if (Status == EFI_NOT_FOUND) {
                Status = REFIT_CALL_5_WRAPPER(
                    RootDir->Open, RootDir,
                    &LogFile, ourDebugLog,
                    EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0
                );
            }

            Status = REFIT_CALL_1_WRAPPER(RootDir->Close, RootDir);
            RootDir = NULL;
        }
    }

    if (EFI_ERROR(Status)) {
        LogFile = NULL;
    }

    MyFreePool (&ourDebugLog);

    return LogFile;
} // static EFI_FILE_PROTOCOL * GetDebugLogFile()

static
VOID SaveMessageToDebugLogFile (
    IN CHAR8 *LastMessage
) {
    static BOOLEAN           FirstTimeSave = TRUE;
           CHAR8            *MemLogBuffer;
           CHAR8            *Text;
           UINTN             MemLogLen;
           UINTN             TextLen;
           EFI_FILE_HANDLE   LogFile;

    MemLogBuffer = GetMemLogBuffer();
    MemLogLen    = GetMemLogLen();
    Text         = LastMessage;
    TextLen      = AsciiStrLen (LastMessage);
    LogFile      = GetDebugLogFile();

    // Write to the log file
    if (LogFile != NULL) {
        // Advance to the EOF so we append
        EFI_FILE_INFO *Info = EfiLibFileInfo (LogFile);
        if (Info) {
            LogFile->SetPosition (LogFile, Info->FileSize);
            // Write out whole log if we have not had root before this
            if (FirstTimeSave) {
                Text          = MemLogBuffer;
                TextLen       = MemLogLen;
                FirstTimeSave = FALSE;
            }

            // Write out this message
            LogFile->Write (LogFile, &TextLen, Text);
        }

        LogFile->Close (LogFile);
    }
} // static VOID SaveMessageToDebugLogFile()

static
VOID EFIAPI MemLogCallback (
    IN INTN DebugMode,
    IN CHAR8 *LastMessage
) {

    if (GlobalConfig.LogLevel < 0) {
        return;
    }

    // Print message to console
    //if (DebugMode >= 2) {
    //    AsciiPrint (LastMessage);
    //}

    if ( (DebugMode >= 1) ) {
        SaveMessageToDebugLogFile (LastMessage);
    }
} // static VOID EFIAPI MemLogCallback()

VOID EFIAPI DeepLoggger (
    IN INTN     DebugMode,
    IN INTN     level,
    IN INTN     type,
    IN CHAR16 **Msg
) {
    UINTN   Limit     = 225;
    CHAR8  *FormatMsg = NULL;
    CHAR16 *Tmp       = NULL;
    CHAR16 *StoreMsg  = NULL;


#if REFIT_DEBUG < 1
    // FreePool and return in RELEASE builds
    ReleasePtr (*Msg);

    return;
#endif

    // Make sure we are able to write
    if (DebugMode < 1
        || GlobalConfig.LogLevel < level
        || GlobalConfig.LogLevel < 1
        || NativeLogger
        || MuteLogger
        || !(*Msg)
    ) {
        ReleasePtr (*Msg);

        return;
    }

    // Truncate message at Log Levels 4 and lower (if required)
    if (GlobalConfig.LogLevel < 5) {
        BOOLEAN LongStr = TruncateString (*Msg, Limit);

        StoreMsg = StrDuplicate (*Msg);
        ReleasePtr (*Msg);
        *Msg = (LongStr)
            ? PoolPrint (L"%s ... Snipped!!", StoreMsg)
            : StrDuplicate (StoreMsg);
    }

    // Disable Timestamp
    TimeStamp = FALSE;

    switch (type) {
        case LOG_BLANK_LINE_SEP: Tmp = StrDuplicate (L"\n");                                                    break;
        case LOG_STAR_HEAD_SEP:  Tmp = PoolPrint (L"\n                ***[ %s\n",                        *Msg); break;
        case LOG_STAR_SEPARATOR: Tmp = PoolPrint (L"\n* ** ** *** *** ***[ %s ]*** *** *** ** ** *\n\n", *Msg); break;
        case LOG_LINE_SEPARATOR: Tmp = PoolPrint (L"\n===================[ %s ]===================\n",   *Msg); break;
        case LOG_LINE_THIN_SEP:  Tmp = PoolPrint (L"\n-------------------[ %s ]-------------------\n",   *Msg); break;
        case LOG_LINE_DASH_SEP:  Tmp = PoolPrint (L"\n- - - - - - - - - -[ %s ]- - - - - - - - - -\n",   *Msg); break;
        case LOG_THREE_STAR_SEP: Tmp = PoolPrint (L"\n. . . . . . . . ***[ %s ]*** . . . . . . . .\n",   *Msg); break;
        case LOG_THREE_STAR_END: Tmp = PoolPrint (L"                ***[ %s ]***\n\n",                   *Msg); break;
        case LOG_THREE_STAR_MID: Tmp = PoolPrint (L"                ***[ %s\n",                          *Msg); break;
        case LOG_LINE_FORENSIC:  Tmp = PoolPrint (L"            !!! ---[ %s\n",                          *Msg); break;
        case LOG_LINE_SPECIAL:   Tmp = PoolPrint (L"\n                   %s",                            *Msg); break;
        case LOG_LINE_SAME:      Tmp = PoolPrint (L"%s",                                                 *Msg); break;
        default:                 Tmp = PoolPrint (L"%s\n",                                               *Msg);
            // Should be 'LOG_LINE_NORMAL', but use 'default' so as to also catch coding errors
            // Enable Timestamp for this
            TimeStamp = TRUE;
    } // switch

    if (Tmp) {
        FormatMsg = AllocateZeroPool (
            (StrLen (Tmp) + 1) * sizeof (CHAR8)
        );

        if (FormatMsg) {
            // Use Native Logging
            UseMsgLog = TRUE;

            // Write the Message String to File
            UnicodeStrToAsciiStr (Tmp, FormatMsg);
            DebugLog (DebugMode, (const CHAR8 *) FormatMsg);

            // Disable Native Logging
            UseMsgLog = FALSE;
        }
    }

    ReleasePtr (*Msg);
    MyFreePool (&Tmp);
    MyFreePool (&StoreMsg);
    MyFreePool (&FormatMsg);
} // VOID EFIAPI DeepLoggger()

VOID EFIAPI DebugLog (
    IN INTN DebugMode,
    IN const CHAR8 *FormatString,
    ...
) {
#if REFIT_DEBUG < 1
    // Just return in RELEASE builds
    return;
#else
    // Make sure writing is allowed/possible
    if (MuteLogger
        || DebugMode < 1
        || FormatString == NULL
        || GlobalConfig.LogLevel < 0
    ) {
        return;
    }

    // Abort on higher log levels if not forcing
    if (!UseMsgLog) {
        UseMsgLog = NativeLogger;

        if (!UseMsgLog && GlobalConfig.LogLevel > 0) {
            return;
        }
    }

    // Print message to log buffer
    VA_LIST Marker;
    VA_START(Marker, FormatString);
    MemLogVA (TimeStamp, DebugMode, FormatString, Marker);
    VA_END(Marker);

    TimeStamp = TRUE;
#endif
} // VOID EFIAPI DebugLog()

VOID InitBooterLog (VOID) {
    SetMemLogCallback (MemLogCallback);
} // VOID InitBooterLog()
