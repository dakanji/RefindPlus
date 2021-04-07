/*
 *  BootLog.c
 *
 *
 *  Created by Slice on 19.08.11.
 *  Edited by apianti 2012-09-08
 *  Initial idea from Kabyl
 */

#include <global.h>
#include "../include/tiano_includes.h"
#include "../Library/MemLogLib/MemLogLib.h"
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>
#include <Library/UefiBootServicesTableLib.h>
#include "../MainLoader/lib.h"
#include "../MainLoader/mystrings.h"

extern  EFI_GUID  gEfiMiscSubClassGuid;

extern  INT16  NowYear;
extern  INT16  NowMonth;
extern  INT16  NowDay;
extern  INT16  NowHour;
extern  INT16  NowMinute;
extern  INT16  NowSecond;

CHAR16  *gLogTemp       = NULL;

BOOLEAN  TimeStamp      = TRUE;
BOOLEAN  DeepLoggging   = FALSE;


CHAR16
*GetAltMonth(
    VOID
) {
    CHAR16  *AltMonth = NULL;

    if (NowMonth == 1) {
        AltMonth = L"b";
    }
    else if (NowMonth == 2) {
        AltMonth = L"d";
    }
    else if (NowMonth == 3) {
        AltMonth = L"f";
    }
    else if (NowMonth == 4) {
        AltMonth = L"h";
    }
    else if (NowMonth == 5) {
        AltMonth = L"j";
    }
    else if (NowMonth == 6) {
        AltMonth = L"k";
    }
    else if (NowMonth == 7) {
        AltMonth = L"n";
    }
    else if (NowMonth == 8) {
        AltMonth = L"p";
    }
    else if (NowMonth == 9) {
        AltMonth = L"r";
    }
    else if (NowMonth == 10) {
        AltMonth = L"t";
    }
    else if (NowMonth == 11) {
        AltMonth = L"v";
    }
    else {
        AltMonth = L"x";
    }

    return AltMonth;
}


CHAR16
*GetAltHour(
    VOID
) {
    CHAR16  *AltHour = NULL;

    if (NowHour == 0) {
        AltHour = L"a";
    }
    else if (NowHour == 1) {
        AltHour = L"b";
    }
    else if (NowHour == 2) {
        AltHour = L"c";
    }
    else if (NowHour == 3) {
        AltHour = L"d";
    }
    else if (NowHour == 4) {
        AltHour = L"e";
    }
    else if (NowHour == 5) {
        AltHour = L"f";
    }
    else if (NowHour == 6) {
        AltHour = L"g";
    }
    else if (NowHour == 7) {
        AltHour = L"h";
    }
    else if (NowHour == 8) {
        AltHour = L"i";
    }
    else if (NowHour == 9) {
        AltHour = L"j";
    }
    else if (NowHour == 10) {
        AltHour = L"k";
    }
    else if (NowHour == 11) {
        AltHour = L"m";
    }
    else if (NowHour == 12) {
        AltHour = L"n";
    }
    else if (NowHour == 13) {
        AltHour = L"p";
    }
    else if (NowHour == 14) {
        AltHour = L"q";
    }
    else if (NowHour == 17) {
        AltHour = L"r";
    }
    else if (NowHour == 16) {
        AltHour = L"s";
    }
    else if (NowHour == 17) {
        AltHour = L"t";
    }
    else if (NowHour == 18) {
        AltHour = L"u";
    }
    else if (NowHour == 19) {
        AltHour = L"v";
    }
    else if (NowHour == 20) {
        AltHour = L"w";
    }
    else if (NowHour == 21) {
        AltHour = L"x";
    }
    else if (NowHour == 22) {
        AltHour = L"y";
    }
    else {
        AltHour = L"z";
    }

    return AltHour;
}


CHAR16
*GetDateString(
    VOID
) {
    STATIC CHAR16  *DateStr = NULL;

    if (DateStr != NULL) {
        return DateStr;
    }

    INT16   ourYear    = (NowYear % 100);
    CHAR16  *ourMonth  = GetAltMonth();
    CHAR16  *ourHour   = GetAltHour();
    DateStr = PoolPrint(
        L"%02d%s%02d%s%02d%02d",
        ourYear,
        ourMonth,
        NowDay,
        ourHour,
        NowMinute,
        NowSecond
    );

    return DateStr;
}


EFI_FILE_PROTOCOL* GetDebugLogFile()
{
  EFI_STATUS          Status;
  EFI_LOADED_IMAGE    *LoadedImage;
  EFI_FILE_PROTOCOL   *RootDir;
  EFI_FILE_PROTOCOL   *LogFile;
  CHAR16              *ourDebugLog = NULL;

  // get RootDir from device we are loaded from
  Status = gBS->HandleProtocol(
      gImageHandle,
      &gEfiLoadedImageProtocolGuid,
      (VOID **) &LoadedImage
  );
  if (EFI_ERROR (Status)) {
    return NULL;
  }
  RootDir = EfiLibOpenRoot(LoadedImage->DeviceHandle);
  if (RootDir == NULL) {
    return NULL;
  }

  CHAR16 *DateStr = GetDateString();

  ourDebugLog = PoolPrint(
      L"EFI\\%s.log",
      DateStr
  );

  // Open log file from current root
  Status = RootDir->Open(
      RootDir,
      &LogFile,
      ourDebugLog,
      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
      0
  );

  // If the log file is not found try to create it
  if (Status == EFI_NOT_FOUND) {
    Status = RootDir->Open(
        RootDir,
        &LogFile,
        ourDebugLog,
        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
        0
    );
  }
  RootDir->Close(RootDir);
  RootDir = NULL;

  if (EFI_ERROR (Status)) {
    // try on first EFI partition
    Status = egFindESP(&RootDir);
    if (!EFI_ERROR (Status)) {
      Status = RootDir->Open(
          RootDir,
          &LogFile,
          ourDebugLog,
          EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
          0
      );
      // If the log file is not found try to create it
      if (Status == EFI_NOT_FOUND) {
        Status = RootDir->Open(
            RootDir,
            &LogFile,
            ourDebugLog,
            EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
            0
        );
      }
      RootDir->Close(RootDir);
      RootDir = NULL;
    }
  }

  if (EFI_ERROR (Status)) {
    LogFile = NULL;
  }

  MyFreePool (ourDebugLog);

  return LogFile;
}


VOID SaveMessageToDebugLogFile(IN CHAR8 *LastMessage)
{
  STATIC BOOLEAN           FirstTimeSave = TRUE;
  CHAR8                   *MemLogBuffer;
  UINTN                    MemLogLen;
  CHAR8                   *Text;
  UINTN                    TextLen;
  EFI_FILE_HANDLE          LogFile;

  MemLogBuffer = GetMemLogBuffer();
  MemLogLen    = GetMemLogLen();
  Text         = LastMessage;
  TextLen      = AsciiStrLen(LastMessage);
  LogFile      = GetDebugLogFile();

  // Write to the log file
  if (LogFile != NULL) {
    // Advance to the EOF so we append
    EFI_FILE_INFO *Info = EfiLibFileInfo(LogFile);
    if (Info) {
      LogFile->SetPosition(LogFile, Info->FileSize);
      // Write out whole log if we have not had root before this
      if (FirstTimeSave) {
        Text          = MemLogBuffer;
        TextLen       = MemLogLen;
        FirstTimeSave = FALSE;
      }
      // Write out this message
      LogFile->Write(LogFile, &TextLen, Text);
    }
    LogFile->Close(LogFile);
  }
}


VOID
EFIAPI
MemLogCallback (
    IN INTN DebugMode,
    IN CHAR8 *LastMessage
) {
    // Print message to console
    if (DebugMode >= 2) {
        AsciiPrint (LastMessage);
    }

    if ( (DebugMode >= 1) ) {
        SaveMessageToDebugLogFile (LastMessage);
    }
}

VOID
EFIAPI
DeepLoggger (
    IN INTN     DebugMode,
    IN INTN     level,
    IN INTN     type,
    IN CHAR16 **Message
) {
#if REFIT_DEBUG < 1
    // FreePool and return in RELEASE builds
    if (*Message) {
        FreePool (*Message);
        *Message = NULL;
    }
#else
    CHAR8   FormatString[255];
    CHAR16 *FinalMessage = NULL;

    // Make sure we are able to write
    if (DebugMode < 1 ||
        GlobalConfig.LogLevel < 1 ||
        GlobalConfig.LogLevel < level ||
        !(*Message)
    ) {
        if (*Message) {
            FreePool (*Message);
            *Message = NULL;
        }

        return;
    }

    // Disable Timestamp
    TimeStamp = FALSE;

    switch (type) {
        case LOG_LINE_SEPARATOR:
            FinalMessage = PoolPrint (L"\n=================[ %s ]=================\n", *Message);
            break;
        case LOG_LINE_THIN_SEP:
            FinalMessage = PoolPrint (L"\n-----------------[ %s ]-----------------\n", *Message);
            break;
        case LOG_THREE_STAR_SEP:
            FinalMessage = PoolPrint (L"\n              ***[ %s ]***\n", *Message);
            break;
        case LOG_THREE_STAR_MID:
            FinalMessage = PoolPrint (L"              ***[ %s ]***\n", *Message);
            break;
        case LOG_THREE_STAR_END:
            FinalMessage = PoolPrint (L"              ***[ %s ]***\n\n", *Message);
            break;
        default:
            // Normally 'LOG_LINE_NORMAL', but use this default to also catch coding errors
            // Enable Timestamp
            TimeStamp    = TRUE;
            FinalMessage = PoolPrint (L"%s\n", *Message);
    } // switch

    if (FinalMessage) {
        // Enable Forced Logging
        DeepLoggging = TRUE;
        // Convert Unicode Message String to Ascii
        MyUnicodeStrToAsciiStr (FinalMessage, FormatString);
        // Write the Message String
        DebugLog (DebugMode, (CONST CHAR8 *) FormatString);
        // Disable Forced Logging
        DeepLoggging = FALSE;
    }

    MyFreePool (*Message);
    MyFreePool (FinalMessage);
#endif
}


VOID
EFIAPI
DebugLog(
    IN INTN DebugMode,
    IN CONST CHAR8 *FormatString, ...
) {
#if REFIT_DEBUG < 1
    // Just return in RELEASE builds
    return;
#else
    VA_LIST Marker;

    // Make sure the buffer is intact for writing
    if (FormatString == NULL || DebugMode < 0) {
      return;
    }

    // Abort on higher log levels if not forcing
    if (!DeepLoggging && GlobalConfig.LogLevel > 0) {
      return;
    }

    // Print message to log buffer
    VA_START(Marker, FormatString);
    MemLogVA(TimeStamp, DebugMode, FormatString, Marker);
    VA_END(Marker);

    TimeStamp = TRUE;
#endif
}

VOID InitBooterLog(
    VOID
) {
  SetMemLogCallback(MemLogCallback);
}
