/*
 * refind/log.c
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

#include "log.h"
#include "global.h"
#include "install.h"
#include "lib.h"
#include "mystrings.h"
#include "../include/refit_call_wrapper.h"
#include "screen.h"

EFI_FILE_HANDLE  gLogHandle;
CHAR16           *gLogTemp = NULL;
BOOLEAN          gLogActive = FALSE;


EFI_STATUS DeleteFile(IN EFI_FILE *BaseDir, CHAR16 *FileName) {
    EFI_FILE_HANDLE FileHandle;
    UINT64          FileMode = EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE;;
    EFI_STATUS      Status;

    Status = refit_call5_wrapper(BaseDir->Open, BaseDir, &FileHandle,
                                 FileName, FileMode, 0);
    if (Status == 0) {
        Status = refit_call1_wrapper(FileHandle->Delete, FileHandle);
    }
    return Status;
} // EFI_STATUS DeleteFile()

// Open the logging file (refind.log).
// Sets the global gLogHandle variable to point to the file.
// Returns EFI_STATUS of file open operation. This might error out if rEFInd
// is installed on a read-only filesystem, for instance.
// If Restart == TRUE, then begin logging at the end of the file;
// if Restart == FALSE, then delete the file and start a new one.
EFI_STATUS StartLogging(BOOLEAN Restart) {
    EFI_STATUS      Status = EFI_SUCCESS;
    UINT64          FileMode;
    UINTN           BufferSize;
    EFI_FILE_HANDLE FoundEsp;
    EFI_FILE_INFO   *FileInfo;
    UINT8           Utf16[2]; // String to hold ID for UTF-16 file start

    if (GlobalConfig.LogLevel > 0) {
        if (Restart) {
            FileMode = EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE;
        } else {
            FileMode = EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE;
            if (FileExists(SelfDir, LOGFILE)) {
                if (FileExists(SelfDir, LOGFILE_OLD))
                    DeleteFile(SelfDir, LOGFILE_OLD);
                BackupOldFile(SelfDir, LOGFILE);
            }
        }
        Status = refit_call5_wrapper(SelfDir->Open, SelfDir, &gLogHandle, LOGFILE,
                                     FileMode, 0);
        if (EFI_ERROR(Status)) {
            Status = egFindESP(&FoundEsp);
            if (!EFI_ERROR(Status)) {
                if (!Restart && (FileExists(FoundEsp, LOGFILE))) {
                    if (FileExists(FoundEsp, LOGFILE_OLD))
                        DeleteFile(FoundEsp, LOGFILE_OLD);
                    BackupOldFile(FoundEsp, LOGFILE);
                }
                Status = refit_call5_wrapper(FoundEsp->Open, FoundEsp,
                                             &gLogHandle, LOGFILE,
                                             FileMode, 0);
            }
        }
        if (EFI_ERROR(Status)) {
            GlobalConfig.LogLevel = 0;
            PrintUglyText(L"Unable to open log file!", CENTER);
            PauseForKey();
        } else {
            // File has been opened; however, if it already exists, then rEFInd
            // will end up writing into the existing file, so it could end up
            // containing remnants of the earlier file if it was larger than
            // this one needs to be. To prevent that, check the file size. If
            // it's bigger than 0, delete it and start again....
            FileInfo = LibFileInfo(gLogHandle);
            if ((FileInfo != NULL) && (FileInfo->FileSize > 0)) {
                if (!Restart) {
                    Status = refit_call1_wrapper(gLogHandle->Delete, gLogHandle);
                    StartLogging(FALSE);
                }
            } else {
                // UTF-16 files begin with these two bytes, so write them....
                Utf16[0] = 0xFF;
                Utf16[1] = 0xFE;
                BufferSize = 2;
                refit_call3_wrapper(gLogHandle->Write, gLogHandle, &BufferSize, Utf16);
                gLogActive = TRUE;
                LOG(1, LOG_LINE_SEPARATOR, L"Beginning logging");
            } // if/else
            if (Restart) {
                refit_call2_wrapper(gLogHandle->SetPosition, gLogHandle, 0xFFFFFFFFFFFFFFFF);
            }
            gLogActive = TRUE;
        } // if/else
    } // if
    return Status;
} // EFI_STATUS StartLogging()

VOID StopLogging(VOID) {
    if (GlobalConfig.LogLevel > 0)
        refit_call1_wrapper(gLogHandle->Close, gLogHandle); // close logging file
    gLogActive = FALSE;
} // VOID StopLogging()

// Write a message (*Message) to the log file. (This pointer is freed
// and set to NULL by this function, the point being to keep these
// operations outside of the macro that calls this function.)
// LogLineType specifies the type of the log line, as specified by the
// LOG_LINE_* constants defined in log.h.
VOID WriteToLog(CHAR16 **Message, UINTN LogLineType) {
    CHAR16   *TimeStr;
    CHAR16   *FinalMessage = NULL;
    UINTN    BufferSize;

    if (gLogActive) {
        switch (LogLineType) {
            case LOG_LINE_SEPARATOR:
                FinalMessage = PoolPrint(L"\n==========%s==========\n", *Message);
                break;
            case LOG_LINE_THIN_SEP:
                FinalMessage = PoolPrint(L"\n----------%s----------\n", *Message);
                break;
            default: /* Normally LOG_LINE_NORMAL, but if there's a coding error, use this.... */
                TimeStr = GetTimeString();
                FinalMessage = PoolPrint(L"%s - %s\n", TimeStr, *Message);
                if (TimeStr)
                    FreePool(TimeStr);
                break;
        } // switch

        if (FinalMessage) {
            BufferSize = StrLen(FinalMessage) * 2;
            refit_call3_wrapper(gLogHandle->Write, gLogHandle, &BufferSize, FinalMessage);
            refit_call1_wrapper(gLogHandle->Flush, gLogHandle);
            FreePool(FinalMessage);
        }
    }
    if (*Message) {
        FreePool(*Message);
        *Message = NULL;
    }
} // VOID WriteToLog()
