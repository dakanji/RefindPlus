/*
 * refind/config.c
 * Configuration file functions
 *
 * Copyright (c) 2006 Christoph Pfisterer
 * All rights reserved.
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

/*
 * Modifications copyright (c) 2012-2021 Roderick W. Smith
 * 
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3) or (at your option) any later version.
 * 
 */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "global.h"
#include "lib.h"
#include "icns.h"
#include "log.h"
#include "menu.h"
#include "config.h"
#include "screen.h"
#include "apple.h"
#include "mystrings.h"
#include "scan.h"
#include "../include/refit_call_wrapper.h"
#include "../mok/mok.h"

// constants

#define LINUX_OPTIONS_FILENAMES  L"refind_linux.conf,refind-linux.conf"
#define MAXCONFIGFILESIZE        (128*1024)

#define ENCODING_ISO8859_1  (0)
#define ENCODING_UTF8       (1)
#define ENCODING_UTF16_LE   (2)

#define GetTime ST->RuntimeServices->GetTime
#define LAST_MINUTE 1439 /* Last minute of a day */

// extern REFIT_MENU_ENTRY MenuEntryReturn;
//static REFIT_MENU_ENTRY MenuEntryReturn   = { L"Return to Main Menu", TAG_RETURN, 0, 0, 0, NULL, NULL, NULL };

//
// read a file into a buffer
//

EFI_STATUS ReadFile(IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName, IN OUT REFIT_FILE *File, OUT UINTN *size)
{
    EFI_STATUS      Status;
    EFI_FILE_HANDLE FileHandle;
    EFI_FILE_INFO   *FileInfo;
    UINT64          ReadSize;
    CHAR16          *Message;

    File->Buffer = NULL;
    File->BufferSize = 0;

    // read the file, allocating a buffer on the way
    Status = refit_call5_wrapper(BaseDir->Open, BaseDir, &FileHandle, FileName, EFI_FILE_MODE_READ, 0);
    Message = PoolPrint(L"while loading the file '%s'", FileName);
    if (CheckError(Status, Message)) {
        MyFreePool(Message);
        return Status;
    }

    FileInfo = LibFileInfo(FileHandle);
    if (FileInfo == NULL) {
        // TODO: print and register the error
        refit_call1_wrapper(FileHandle->Close, FileHandle);
        return EFI_LOAD_ERROR;
    }
    ReadSize = FileInfo->FileSize;
    FreePool(FileInfo);

    File->BufferSize = (UINTN)ReadSize;
    File->Buffer = AllocatePool(File->BufferSize);
    if (File->Buffer == NULL) {
       size = 0;
       return EFI_OUT_OF_RESOURCES;
    } else {
       *size = File->BufferSize;
    } // if/else
    Status = refit_call3_wrapper(FileHandle->Read, FileHandle, &File->BufferSize, File->Buffer);
    if (CheckError(Status, Message)) {
        MyFreePool(File->Buffer);
        MyFreePool(Message);
        File->Buffer = NULL;
        refit_call1_wrapper(FileHandle->Close, FileHandle);
        return Status;
    }
    Status = refit_call1_wrapper(FileHandle->Close, FileHandle);

    // setup for reading
    File->Current8Ptr  = (CHAR8 *)File->Buffer;
    File->End8Ptr      = File->Current8Ptr + File->BufferSize;
    File->Current16Ptr = (CHAR16 *)File->Buffer;
    File->End16Ptr     = File->Current16Ptr + (File->BufferSize >> 1);

    // detect encoding
    File->Encoding = ENCODING_ISO8859_1;   // default: 1:1 translation of CHAR8 to CHAR16
    if (File->BufferSize >= 4) {
        if (File->Buffer[0] == 0xFF && File->Buffer[1] == 0xFE) {
            // BOM in UTF-16 little endian (or UTF-32 little endian)
            File->Encoding = ENCODING_UTF16_LE;   // use CHAR16 as is
            File->Current16Ptr++;
        } else if (File->Buffer[0] == 0xEF && File->Buffer[1] == 0xBB && File->Buffer[2] == 0xBF) {
            // BOM in UTF-8
            File->Encoding = ENCODING_UTF8;       // translate from UTF-8 to UTF-16
            File->Current8Ptr += 3;
        } else if (File->Buffer[1] == 0 && File->Buffer[3] == 0) {
            File->Encoding = ENCODING_UTF16_LE;   // use CHAR16 as is
        }
        // TODO: detect other encodings as they are implemented
    }

    return EFI_SUCCESS;
}

//
// get a single line of text from a file
//

static CHAR16 *ReadLine(REFIT_FILE *File)
{
    CHAR16  *Line, *q;
    UINTN   LineLength;

    if (File->Buffer == NULL)
        return NULL;

    if (File->Encoding == ENCODING_ISO8859_1 || File->Encoding == ENCODING_UTF8) {

        CHAR8 *p, *LineStart, *LineEnd;

        p = File->Current8Ptr;
        if (p >= File->End8Ptr)
            return NULL;

        LineStart = p;
        for (; p < File->End8Ptr; p++)
            if (*p == 13 || *p == 10)
                break;
        LineEnd = p;
        for (; p < File->End8Ptr; p++)
            if (*p != 13 && *p != 10)
                break;
        File->Current8Ptr = p;

        LineLength = (UINTN)(LineEnd - LineStart) + 1;
        Line = AllocatePool(LineLength * sizeof(CHAR16));
        if (Line == NULL)
            return NULL;

        q = Line;
        if (File->Encoding == ENCODING_ISO8859_1) {
            for (p = LineStart; p < LineEnd; )
                *q++ = *p++;
        } else if (File->Encoding == ENCODING_UTF8) {
            // TODO: actually handle UTF-8
            for (p = LineStart; p < LineEnd; )
                *q++ = *p++;
        }
        *q = 0;

    } else if (File->Encoding == ENCODING_UTF16_LE) {

        CHAR16 *p, *LineStart, *LineEnd;

        p = File->Current16Ptr;
        if (p >= File->End16Ptr)
            return NULL;

        LineStart = p;
        for (; p < File->End16Ptr; p++)
            if (*p == 13 || *p == 10)
                break;
        LineEnd = p;
        for (; p < File->End16Ptr; p++)
            if (*p != 13 && *p != 10)
                break;
        File->Current16Ptr = p;

        LineLength = (UINTN)(LineEnd - LineStart) + 1;
        Line = AllocatePool(LineLength * sizeof(CHAR16));
        if (Line == NULL)
            return NULL;

        for (p = LineStart, q = Line; p < LineEnd; )
            *q++ = *p++;
        *q = 0;

    } else
        return NULL;   // unsupported encoding

    return Line;
}

// Returns FALSE if *p points to the end of a token, TRUE otherwise.
// Also modifies *p **IF** the first and second characters are both
// quotes ('"'); it deletes one of them.
static BOOLEAN KeepReading(IN OUT CHAR16 *p, IN OUT BOOLEAN *IsQuoted) {
    BOOLEAN MoreToRead = FALSE;
    CHAR16  *Temp = NULL;

    if ((p == NULL) || (IsQuoted == NULL))
        return FALSE;

    if (*p == L'\0')
        return FALSE;

    if ((*p != ' ' && *p != '\t' && *p != '=' && *p != '#' && *p != ',') || *IsQuoted) {
        MoreToRead = TRUE;
    }
    if (*p == L'"') {
        if (p[1] == L'"') {
            Temp = StrDuplicate(&p[1]);
            if (Temp != NULL) {
                StrCpy(p, Temp);
                FreePool(Temp);
            }
            MoreToRead = TRUE;
        } else {
            *IsQuoted = !(*IsQuoted);
            MoreToRead = FALSE;
        } // if/else second character is a quote
    } // if first character is a quote

    return MoreToRead;
} // BOOLEAN KeepReading()

//
// get a line of tokens from a file
//
UINTN ReadTokenLine(IN REFIT_FILE *File, OUT CHAR16 ***TokenList)
{
    BOOLEAN         LineFinished, IsQuoted = FALSE;
    CHAR16          *Line, *Token, *p;
    UINTN           TokenCount = 0;

    *TokenList = NULL;

    while (TokenCount == 0) {
        Line = ReadLine(File);
        if (Line == NULL)
            return(0);

        p = Line;
        LineFinished = FALSE;
        while (!LineFinished) {
            // skip whitespace & find start of token
            while ((*p == ' ' || *p == '\t' || *p == '=' || *p == ',') && !IsQuoted)
                p++;
            if (*p == 0 || *p == '#')
                break;

            if (*p == '"') {
               IsQuoted = !IsQuoted;
               p++;
            } // if
            Token = p;

            // find end of token
            while (KeepReading(p, &IsQuoted)) {
               if ((*p == L'/') && !IsQuoted) // Switch Unix-style to DOS-style directory separators
                  *p = L'\\';
               p++;
            } // while
            if (*p == L'\0' || *p == L'#')
                LineFinished = TRUE;
            *p++ = 0;

            AddListElement((VOID ***)TokenList, &TokenCount, (VOID *)StrDuplicate(Token));
        }

        FreePool(Line);
    }
    return (TokenCount);
} /* ReadTokenLine() */

VOID FreeTokenLine(IN OUT CHAR16 ***TokenList, IN OUT UINTN *TokenCount)
{
    // TODO: also free the items
    FreeList((VOID ***)TokenList, TokenCount);
}

// handle a parameter with a single integer argument
static VOID HandleInt(IN CHAR16 **TokenList, IN UINTN TokenCount, OUT UINTN *Value)
{
    if (TokenCount == 2) {
       if (StrCmp(TokenList[1], L"-1") == 0)
          *Value = -1;
       else
          *Value = Atoi(TokenList[1]);
    }
}

// handle a parameter with a single string argument
static VOID HandleString(IN CHAR16 **TokenList, IN UINTN TokenCount, OUT CHAR16 **Target) {
    if ((TokenCount == 2) && Target) {
        MyFreePool(*Target);
        *Target = StrDuplicate(TokenList[1]);
    } // if
} // static VOID HandleString()

// Handle a parameter with a series of string arguments, to replace or be added to a
// comma-delimited list. Passes each token through the CleanUpPathNameSlashes() function
// to ensure consistency in subsequent comparisons of filenames. If the first
// non-keyword token is "+", the list is added to the existing target string; otherwise,
// the tokens replace the current string.
static VOID HandleStrings(IN CHAR16 **TokenList, IN UINTN TokenCount, OUT CHAR16 **Target) {
    UINTN i;
    BOOLEAN AddMode = FALSE;

    if ((TokenCount > 2) && (StrCmp(TokenList[1], L"+") == 0)) {
        AddMode = TRUE;
    }

    if ((*Target != NULL) && !AddMode) {
        FreePool(*Target);
        *Target = NULL;
    } // if
    for (i = 1; i < TokenCount; i++) {
        if ((i != 1) || !AddMode) {
            CleanUpPathNameSlashes(TokenList[i]);
            MergeStrings(Target, TokenList[i], L',');
        } // if
    } // for
} // static VOID HandleStrings()

// Handle a parameter with a series of hexadecimal arguments, to replace or be added to a
// linked list of UINT32 values. Any item with a non-hexadecimal value is discarded, as is
// any value that exceeds MaxValue. If the first non-keyword token is "+", the new list is
// added to the existing Target; otherwise, the interpreted tokens replace the current
// Target.
static VOID HandleHexes(IN CHAR16 **TokenList,
                        IN UINTN TokenCount,
                        IN UINTN MaxValue,
                        OUT UINT32_LIST **Target) {
    UINTN       InputIndex = 1, i;
    UINT32      Value;
    UINT32_LIST *EndOfList = NULL;
    UINT32_LIST *NewEntry;

    if ((TokenCount > 2) && (StrCmp(TokenList[1], L"+") == 0)) {
        InputIndex = 2;
        EndOfList = *Target;
        while (EndOfList && (EndOfList->Next != NULL)) {
            EndOfList = EndOfList->Next;
        }
    } else {
        EraseUint32List(Target);
    }

    for (i = InputIndex; i < TokenCount; i++) {
        if (IsValidHex(TokenList[i])) {
            Value = (UINT32) StrToHex(TokenList[i], 0, 8);
            if (Value <= MaxValue) {
                NewEntry = AllocatePool(sizeof(UINT32_LIST));
                if (NewEntry) {
                    NewEntry->Value = Value;
                    NewEntry->Next = NULL;
                    if (EndOfList == NULL) {
                        EndOfList = NewEntry;
                        *Target = NewEntry;
                    } else {
                        EndOfList->Next = NewEntry;
                        EndOfList = NewEntry;
                    } // if/else
                } // if allocated memory for NewEntry
            } // if (Value < MaxValue)
        } // if is valid hex value
    } // for
} // static VOID HandleHexes()

// Convert TimeString (in "HH:MM" format) to a pure-minute format. Values should be
// in the range from 0 (for 00:00, or midnight) to 1439 (for 23:59; aka LAST_MINUTE).
// Any value outside that range denotes an error in the specification. Note that if
// the input is a number that includes no colon, this function will return the original
// number in UINTN form.
static UINTN HandleTime(IN CHAR16 *TimeString) {
    UINTN Hour = 0, Minute = 0, TimeLength, i = 0;

    TimeLength = StrLen(TimeString);
    while (i < TimeLength) {
        if (TimeString[i] == L':') {
            Hour = Minute;
            Minute = 0;
        } // if
        if ((TimeString[i] >= L'0') && (TimeString[i] <= '9')) {
            Minute *= 10;
            Minute += (TimeString[i] - L'0');
        } // if
        i++;
    } // while
    return (Hour * 60 + Minute);
} // BOOLEAN HandleTime()

static BOOLEAN HandleBoolean(IN CHAR16 **TokenList, IN UINTN TokenCount) {
    BOOLEAN TruthValue = TRUE;

    if ((TokenCount >= 2) && ((StrCmp(TokenList[1], L"0") == 0) ||
                              MyStriCmp(TokenList[1], L"false") ||
                              MyStriCmp(TokenList[1], L"off"))) {
        TruthValue = FALSE;
    } // if

    return TruthValue;
} // BOOLEAN HandleBoolean

// Sets the default boot loader IF the current time is within the bounds
// defined by the third and fourth tokens in the TokenList.
static VOID SetDefaultByTime(IN CHAR16 **TokenList, OUT CHAR16 **Default) {
    EFI_STATUS            Status;
    EFI_TIME              CurrentTime;
    UINTN                 StartTime, EndTime, Now;
    BOOLEAN               SetIt = FALSE;

    StartTime = HandleTime(TokenList[2]);
    EndTime = HandleTime(TokenList[3]);

    if ((StartTime <= LAST_MINUTE) && (EndTime <= LAST_MINUTE)) {
        Status = refit_call2_wrapper(GetTime, &CurrentTime, NULL);
        if (Status != EFI_SUCCESS)
            return;
        Now = CurrentTime.Hour * 60 + CurrentTime.Minute;

        if (Now > LAST_MINUTE) { // Shouldn't happen; just being paranoid
            Print(L"Warning: Impossible system time: %d:%d\n", CurrentTime.Hour, CurrentTime.Minute);
            return;
        } // if impossible time

        if (StartTime < EndTime) { // Time range does NOT cross midnight
            if ((Now >= StartTime) && (Now <= EndTime))
                SetIt = TRUE;
        } else { // Time range DOES cross midnight
            if ((Now >= StartTime) && (Now <= EndTime))
                SetIt = TRUE;
        } // if/else time range crosses midnight

        if (SetIt) {
            MyFreePool(*Default);
            *Default = StrDuplicate(TokenList[1]);
        } // if (SetIt)
    } // if ((StartTime <= LAST_MINUTE) && (EndTime <= LAST_MINUTE))
} // VOID SetDefaultByTime()

static LOADER_ENTRY * AddPreparedLoaderEntry(LOADER_ENTRY *Entry) {
    AddMenuEntry(&MainMenu, (REFIT_MENU_ENTRY *)Entry);

    return(Entry);
} // LOADER_ENTRY * AddPreparedLoaderEntry()

// read config file
VOID ReadConfig(CHAR16 *FileName)
{
    EFI_STATUS      Status;
    REFIT_FILE      File;
    CHAR16          **TokenList;
    CHAR16          *FlagName;
    CHAR16          *TempStr = NULL;
    UINTN           TokenCount, i;

    // Set a few defaults only if we're loading the default file.
    if (MyStriCmp(FileName, GlobalConfig.ConfigFilename)) {
        MyFreePool(GlobalConfig.AlsoScan);
        GlobalConfig.AlsoScan = StrDuplicate(ALSO_SCAN_DIRS);
        MyFreePool(GlobalConfig.DontScanDirs);
        if (SelfVolume)
            TempStr = GuidAsString(&(SelfVolume->PartGuid));
        MergeStrings(&TempStr, SelfDirPath, L':');
        MergeStrings(&TempStr, MEMTEST_LOCATIONS, L',');
        GlobalConfig.DontScanDirs = TempStr;
        MyFreePool(GlobalConfig.DontScanFiles);
        GlobalConfig.DontScanFiles = StrDuplicate(DONT_SCAN_FILES);
        MyFreePool(GlobalConfig.DontScanTools);
        GlobalConfig.DontScanTools = NULL;
        MyFreePool(GlobalConfig.DontScanFirmware);
        GlobalConfig.DontScanFirmware = NULL;
        MergeStrings(&(GlobalConfig.DontScanFiles), MOK_NAMES, L',');
        MergeStrings(&(GlobalConfig.DontScanFiles), FWUPDATE_NAMES, L',');
        MyFreePool(GlobalConfig.DontScanVolumes);
        GlobalConfig.DontScanVolumes = StrDuplicate(DONT_SCAN_VOLUMES);
        GlobalConfig.WindowsRecoveryFiles = StrDuplicate(WINDOWS_RECOVERY_FILES);
        GlobalConfig.MacOSRecoveryFiles = StrDuplicate(MACOS_RECOVERY_FILES);
        MyFreePool(GlobalConfig.DefaultSelection);
        GlobalConfig.DefaultSelection = StrDuplicate(L"+");
    } // if

    if (!FileExists(SelfDir, FileName)) {
        Print(L"Configuration file '%s' missing!\n", FileName);
        if (!FileExists(SelfDir, L"icons")) {
            Print(L"Icons directory doesn't exist; setting textonly = TRUE!\n");
            GlobalConfig.TextOnly = TRUE;
        }
        return;
    }

    Status = ReadFile(SelfDir, FileName, &File, &i);
    if (EFI_ERROR(Status))
        return;

    for (;;) {
        TokenCount = ReadTokenLine(&File, &TokenList);
        if (TokenCount == 0)
            break;

        if (MyStriCmp(TokenList[0], L"timeout")) {
            HandleInt(TokenList, TokenCount, &(GlobalConfig.Timeout));

        } else if (MyStriCmp(TokenList[0], L"shutdown_after_timeout")) {
           GlobalConfig.ShutdownAfterTimeout = HandleBoolean(TokenList, TokenCount);

        } else if (MyStriCmp(TokenList[0], L"hideui")) {
            for (i = 1; i < TokenCount; i++) {
                FlagName = TokenList[i];
                if (MyStriCmp(FlagName, L"banner")) {
                    GlobalConfig.HideUIFlags |= HIDEUI_FLAG_BANNER;
                } else if (MyStriCmp(FlagName, L"label")) {
                    GlobalConfig.HideUIFlags |= HIDEUI_FLAG_LABEL;
                } else if (MyStriCmp(FlagName, L"singleuser")) {
                    GlobalConfig.HideUIFlags |= HIDEUI_FLAG_SINGLEUSER;
                } else if (MyStriCmp(FlagName, L"hwtest")) {
                    GlobalConfig.HideUIFlags |= HIDEUI_FLAG_HWTEST;
                } else if (MyStriCmp(FlagName, L"arrows")) {
                    GlobalConfig.HideUIFlags |= HIDEUI_FLAG_ARROWS;
                } else if (MyStriCmp(FlagName, L"hints")) {
                    GlobalConfig.HideUIFlags |= HIDEUI_FLAG_HINTS;
                } else if (MyStriCmp(FlagName, L"editor")) {
                    GlobalConfig.HideUIFlags |= HIDEUI_FLAG_EDITOR;
                } else if (MyStriCmp(FlagName, L"safemode")) {
                    GlobalConfig.HideUIFlags |= HIDEUI_FLAG_SAFEMODE;
                } else if (MyStriCmp(FlagName, L"badges")) {
                    GlobalConfig.HideUIFlags |= HIDEUI_FLAG_BADGES;
                } else if (MyStriCmp(FlagName, L"all")) {
                    GlobalConfig.HideUIFlags = HIDEUI_FLAG_ALL;
                } else {
                    Print(L" unknown hideui flag: '%s'\n", FlagName);
                }
            }

        } else if (MyStriCmp(TokenList[0], L"icons_dir")) {
            HandleString(TokenList, TokenCount, &(GlobalConfig.IconsDir));

        } else if (MyStriCmp(TokenList[0], L"scanfor")) {
            for (i = 0; i < NUM_SCAN_OPTIONS; i++) {
                if (i < TokenCount)
                    GlobalConfig.ScanFor[i] = TokenList[i][0];
                else
                    GlobalConfig.ScanFor[i] = ' ';
            }

        } else if (MyStriCmp(TokenList[0], L"use_nvram")) {
            GlobalConfig.UseNvram = HandleBoolean(TokenList, TokenCount);

        } else if (MyStriCmp(TokenList[0], L"uefi_deep_legacy_scan")) {
            GlobalConfig.DeepLegacyScan = HandleBoolean(TokenList, TokenCount);

        } else if (MyStriCmp(TokenList[0], L"scan_delay") && (TokenCount == 2)) {
            HandleInt(TokenList, TokenCount, &(GlobalConfig.ScanDelay));

        } else if (MyStriCmp(TokenList[0], L"log_level") && (TokenCount == 2)) {
            HandleInt(TokenList, TokenCount, &(GlobalConfig.LogLevel));

        } else if (MyStriCmp(TokenList[0], L"also_scan_dirs")) {
             HandleStrings(TokenList, TokenCount, &(GlobalConfig.AlsoScan));

        } else if (MyStriCmp(TokenList[0], L"don't_scan_volumes") ||
                   MyStriCmp(TokenList[0], L"dont_scan_volumes")) {
            // Note: Don't use HandleStrings() because it modifies slashes,
            // which might be present in volume name
            MyFreePool(GlobalConfig.DontScanVolumes);
            GlobalConfig.DontScanVolumes = NULL;
            for (i = 1; i < TokenCount; i++) {
                MergeStrings(&GlobalConfig.DontScanVolumes, TokenList[i], L',');
            }

        } else if (MyStriCmp(TokenList[0], L"don't_scan_dirs") ||
                   MyStriCmp(TokenList[0], L"dont_scan_dirs")) {
            HandleStrings(TokenList, TokenCount, &(GlobalConfig.DontScanDirs));

        } else if (MyStriCmp(TokenList[0], L"don't_scan_files") ||
                   MyStriCmp(TokenList[0], L"dont_scan_files")) {
            HandleStrings(TokenList, TokenCount, &(GlobalConfig.DontScanFiles));

        } else if (MyStriCmp(TokenList[0], L"don't_scan_firmware") ||
                   MyStriCmp(TokenList[0], L"dont_scan_firmware")) {
            HandleStrings(TokenList, TokenCount, &(GlobalConfig.DontScanFirmware));

        } else if (MyStriCmp(TokenList[0], L"don't_scan_tools") ||
                   MyStriCmp(TokenList[0], L"dont_scan_tools")) {
            HandleStrings(TokenList, TokenCount, &(GlobalConfig.DontScanTools));

        } else if (MyStriCmp(TokenList[0], L"windows_recovery_files")) {
            HandleStrings(TokenList, TokenCount, &(GlobalConfig.WindowsRecoveryFiles));

        } else if (MyStriCmp(TokenList[0], L"scan_driver_dirs")) {
            HandleStrings(TokenList, TokenCount, &(GlobalConfig.DriverDirs));

        } else if (MyStriCmp(TokenList[0], L"showtools")) {
            SetMem(GlobalConfig.ShowTools, NUM_TOOLS * sizeof(UINTN), 0);
            GlobalConfig.HiddenTags = FALSE;
            for (i = 1; (i < TokenCount) && (i < NUM_TOOLS); i++) {
                FlagName = TokenList[i];
                if (MyStriCmp(FlagName, L"shell")) {
                    GlobalConfig.ShowTools[i - 1] = TAG_SHELL;
                } else if (MyStriCmp(FlagName, L"gptsync")) {
                    GlobalConfig.ShowTools[i - 1] = TAG_GPTSYNC;
                } else if (MyStriCmp(FlagName, L"gdisk")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_GDISK;
                } else if (MyStriCmp(FlagName, L"about")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_ABOUT;
                } else if (MyStriCmp(FlagName, L"exit")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_EXIT;
                } else if (MyStriCmp(FlagName, L"reboot")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_REBOOT;
                } else if (MyStriCmp(FlagName, L"shutdown")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_SHUTDOWN;
                } else if (MyStriCmp(FlagName, L"install")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_INSTALL;
                } else if (MyStriCmp(FlagName, L"bootorder")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_BOOTORDER;
                } else if (MyStriCmp(FlagName, L"apple_recovery")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_APPLE_RECOVERY;
                } else if (MyStriCmp(FlagName, L"windows_recovery")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_WINDOWS_RECOVERY;
                } else if (MyStriCmp(FlagName, L"mok_tool")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_MOK_TOOL;
                } else if (MyStriCmp(FlagName, L"fwupdate")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_FWUPDATE_TOOL;
                } else if (MyStriCmp(FlagName, L"csr_rotate")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_CSR_ROTATE;
                } else if (MyStriCmp(FlagName, L"firmware")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_FIRMWARE;
                } else if (MyStriCmp(FlagName, L"memtest86") || MyStriCmp(FlagName, L"memtest")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_MEMTEST;
                } else if (MyStriCmp(FlagName, L"netboot")) {
                   GlobalConfig.ShowTools[i - 1] = TAG_NETBOOT;
                } else if (MyStriCmp(FlagName, L"hidden_tags")) {
                    GlobalConfig.ShowTools[i - 1] = TAG_HIDDEN;
                    GlobalConfig.HiddenTags = TRUE;
                } else {
                   Print(L" unknown showtools flag: '%s'\n", FlagName);
                }
            } // showtools options

        } else if (MyStriCmp(TokenList[0], L"banner")) {
            HandleString(TokenList, TokenCount, &(GlobalConfig.BannerFileName));

        } else if (MyStriCmp(TokenList[0], L"banner_scale") && (TokenCount == 2)) {
            if (MyStriCmp(TokenList[1], L"noscale")) {
                GlobalConfig.BannerScale = BANNER_NOSCALE;
            } else if (MyStriCmp(TokenList[1], L"fillscreen") || MyStriCmp(TokenList[1], L"fullscreen")) {
                GlobalConfig.BannerScale = BANNER_FILLSCREEN;
            } else {
                Print(L" unknown banner_type flag: '%s'\n", TokenList[1]);
            } // if/else

        } else if (MyStriCmp(TokenList[0], L"small_icon_size") && (TokenCount == 2)) {
            HandleInt(TokenList, TokenCount, &i);
            if (i >= 32) {
                GlobalConfig.IconSizes[ICON_SIZE_SMALL] = i;
                HaveResized = TRUE;
            }

        } else if (MyStriCmp(TokenList[0], L"big_icon_size") && (TokenCount == 2)) {
            HandleInt(TokenList, TokenCount, &i);
            if (i >= 32) {
                GlobalConfig.IconSizes[ICON_SIZE_BIG] = i;
                GlobalConfig.IconSizes[ICON_SIZE_BADGE] = i / 4;
                HaveResized = TRUE;
            }

        } else if (MyStriCmp(TokenList[0], L"mouse_size") && (TokenCount == 2)) {
            HandleInt(TokenList, TokenCount, &i);
            if (i >= DEFAULT_MOUSE_SIZE) {
                GlobalConfig.IconSizes[ICON_SIZE_MOUSE] = i;
            }

        } else if (MyStriCmp(TokenList[0], L"selection_small")) {
            HandleString(TokenList, TokenCount, &(GlobalConfig.SelectionSmallFileName));

        } else if (MyStriCmp(TokenList[0], L"selection_big")) {
            HandleString(TokenList, TokenCount, &(GlobalConfig.SelectionBigFileName));

        } else if (MyStriCmp(TokenList[0], L"default_selection")) {
            if (TokenCount == 4) {
                SetDefaultByTime(TokenList, &(GlobalConfig.DefaultSelection));
            } else {
                HandleString(TokenList, TokenCount, &(GlobalConfig.DefaultSelection));
            }

        } else if (MyStriCmp(TokenList[0], L"textonly")) {
            GlobalConfig.TextOnly = HandleBoolean(TokenList, TokenCount);

        } else if (MyStriCmp(TokenList[0], L"textmode")) {
            HandleInt(TokenList, TokenCount, &(GlobalConfig.RequestedTextMode));

        } else if (MyStriCmp(TokenList[0], L"resolution") && ((TokenCount == 2) || (TokenCount == 3))) {
            if (MyStriCmp(TokenList[1], L"max")) {
                GlobalConfig.RequestedScreenWidth = MAX_RES_CODE;
                GlobalConfig.RequestedScreenHeight = MAX_RES_CODE;
            } else {
                GlobalConfig.RequestedScreenWidth = Atoi(TokenList[1]);
                if (TokenCount == 3)
                    GlobalConfig.RequestedScreenHeight = Atoi(TokenList[2]);
                else
                    GlobalConfig.RequestedScreenHeight = 0;
            }

        } else if (MyStriCmp(TokenList[0], L"screensaver")) {
            HandleInt(TokenList, TokenCount, &(GlobalConfig.ScreensaverTime));

        } else if (MyStriCmp(TokenList[0], L"use_graphics_for")) {
            if ((TokenCount == 2) || ((TokenCount > 2) && (!MyStriCmp(TokenList[1], L"+"))))
                GlobalConfig.GraphicsFor = 0;
            for (i = 1; i < TokenCount; i++) {
                if (MyStriCmp(TokenList[i], L"osx")) {
                    GlobalConfig.GraphicsFor |= GRAPHICS_FOR_OSX;
                } else if (MyStriCmp(TokenList[i], L"linux")) {
                    GlobalConfig.GraphicsFor |= GRAPHICS_FOR_LINUX;
                } else if (MyStriCmp(TokenList[i], L"elilo")) {
                    GlobalConfig.GraphicsFor |= GRAPHICS_FOR_ELILO;
                } else if (MyStriCmp(TokenList[i], L"grub")) {
                    GlobalConfig.GraphicsFor |= GRAPHICS_FOR_GRUB;
                } else if (MyStriCmp(TokenList[i], L"windows")) {
                    GlobalConfig.GraphicsFor |= GRAPHICS_FOR_WINDOWS;
                }
            } // for (graphics_on tokens)

        } else if (MyStriCmp(TokenList[0], L"font") && (TokenCount == 2)) {
            egLoadFont(TokenList[1]);

        } else if (MyStriCmp(TokenList[0], L"scan_all_linux_kernels")) {
            GlobalConfig.ScanAllLinux = HandleBoolean(TokenList, TokenCount);

        } else if (MyStriCmp(TokenList[0], L"fold_linux_kernels")) {
            GlobalConfig.FoldLinuxKernels = HandleBoolean(TokenList, TokenCount);

        } else if (MyStriCmp(TokenList[0], L"extra_kernel_version_strings")) {
            HandleStrings(TokenList, TokenCount, &(GlobalConfig.ExtraKernelVersionStrings));

        } else if (MyStriCmp(TokenList[0], L"max_tags")) {
            HandleInt(TokenList, TokenCount, &(GlobalConfig.MaxTags));

        } else if (MyStriCmp(TokenList[0], L"enable_and_lock_vmx")) {
            GlobalConfig.EnableAndLockVMX = HandleBoolean(TokenList, TokenCount);

        } else if (MyStriCmp(TokenList[0], L"spoof_osx_version")) {
            HandleString(TokenList, TokenCount, &(GlobalConfig.SpoofOSXVersion));

        } else if (MyStriCmp(TokenList[0], L"csr_values")) {
            HandleHexes(TokenList, TokenCount, CSR_MAX_LEGAL_VALUE, &(GlobalConfig.CsrValues));

        } else if (MyStriCmp(TokenList[0], L"write_systemd_vars")) {
            GlobalConfig.WriteSystemdVars = HandleBoolean(TokenList, TokenCount);

        } else if (MyStriCmp(TokenList[0], L"include") && (TokenCount == 2) &&
                   MyStriCmp(FileName, GlobalConfig.ConfigFilename)) {
            if (!MyStriCmp(TokenList[1], FileName)) {
                ReadConfig(TokenList[1]);
            }

        } else if (MyStriCmp(TokenList[0], L"enable_mouse")) {
            GlobalConfig.EnableMouse = HandleBoolean(TokenList, TokenCount);
            if (GlobalConfig.EnableMouse) {
                GlobalConfig.EnableTouch = FALSE;
            }
        
        } else if (MyStriCmp(TokenList[0], L"enable_touch")) {
            GlobalConfig.EnableTouch = HandleBoolean(TokenList, TokenCount);
            if (GlobalConfig.EnableTouch) {
                GlobalConfig.EnableMouse = FALSE;
            }
           
        } else if (MyStriCmp(TokenList[0], L"mouse_speed") && (TokenCount == 2)) {
            HandleInt(TokenList, TokenCount, &i);
            if (i < 1)
                i = 1;
            if (i > 32)
                i = 32;
            GlobalConfig.MouseSpeed = i;
        }

        FreeTokenLine(&TokenList, &TokenCount);
    }
    if ((GlobalConfig.DontScanFiles) && (GlobalConfig.WindowsRecoveryFiles))
        MergeStrings(&(GlobalConfig.DontScanFiles), GlobalConfig.WindowsRecoveryFiles, L',');
    MyFreePool(File.Buffer);

    if (!FileExists(SelfDir, L"icons") && !FileExists(SelfDir, GlobalConfig.IconsDir)) {
        Print(L"Icons directory doesn't exist; setting textonly = TRUE!\n");
        GlobalConfig.TextOnly = TRUE;
    }
} /* VOID ReadConfig() */

static VOID AddSubmenu(LOADER_ENTRY *Entry, REFIT_FILE *File, REFIT_VOLUME *Volume, CHAR16 *Title) {
    REFIT_MENU_SCREEN  *SubScreen;
    LOADER_ENTRY       *SubEntry;
    UINTN              TokenCount;
    CHAR16             **TokenList;

    SubScreen = InitializeSubScreen(Entry);

    // Set defaults for the new entry; will be modified based on lines read from the config. file....
    SubEntry = InitializeLoaderEntry(Entry);

    if ((SubEntry == NULL) || (SubScreen == NULL))
        return;
    SubEntry->me.Title        = StrDuplicate(Title);

    while (((TokenCount = ReadTokenLine(File, &TokenList)) > 0) && (StrCmp(TokenList[0], L"}") != 0)) {

        if (MyStriCmp(TokenList[0], L"loader") && (TokenCount > 1)) { // set the boot loader filename
            MyFreePool(SubEntry->LoaderPath);
            SubEntry->LoaderPath = StrDuplicate(TokenList[1]);
            SubEntry->Volume = Volume;

        } else if (MyStriCmp(TokenList[0], L"volume") && (TokenCount > 1)) {
            if (FindVolume(&Volume, TokenList[1])) {
                if ((Volume != NULL) && (Volume->IsReadable) && (Volume->RootDir)) {
                    MyFreePool(SubEntry->me.Title);
                    SubEntry->me.Title      = PoolPrint(L"Boot %s from %s",
                                                        (Title != NULL) ? Title : L"Unknown",
                                                        Volume->VolName);
                    SubEntry->me.BadgeImage = Volume->VolBadgeImage;
                    SubEntry->Volume        = Volume;
                } // if volume is readable
            } // if match found

        } else if (MyStriCmp(TokenList[0], L"initrd")) {
            MyFreePool(SubEntry->InitrdPath);
            SubEntry->InitrdPath = NULL;
            if (TokenCount > 1) {
                SubEntry->InitrdPath = StrDuplicate(TokenList[1]);
            }

        } else if (MyStriCmp(TokenList[0], L"options")) {
            MyFreePool(SubEntry->LoadOptions);
            SubEntry->LoadOptions = NULL;
            if (TokenCount > 1) {
                SubEntry->LoadOptions = StrDuplicate(TokenList[1]);
            } // if/else

        } else if (MyStriCmp(TokenList[0], L"add_options") && (TokenCount > 1)) {
            MergeStrings(&SubEntry->LoadOptions, TokenList[1], L' ');

        } else if (MyStriCmp(TokenList[0], L"graphics") && (TokenCount > 1)) {
            SubEntry->UseGraphicsMode = MyStriCmp(TokenList[1], L"on");

        } else if (MyStriCmp(TokenList[0], L"disabled")) {
            SubEntry->Enabled = FALSE;
        } // ief/elseif

        FreeTokenLine(&TokenList, &TokenCount);
    } // while()

    if (SubEntry->InitrdPath != NULL) {
        MergeStrings(&SubEntry->LoadOptions, L"initrd=", L' ');
        MergeStrings(&SubEntry->LoadOptions, SubEntry->InitrdPath, 0);
        MyFreePool(SubEntry->InitrdPath);
        SubEntry->InitrdPath = NULL;
    } // if
    if (SubEntry->Enabled == TRUE) {
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
    }
    Entry->me.SubScreen = SubScreen;
} // VOID AddSubmenu()

// Adds the options from a SINGLE refind.conf stanza to a new loader entry and returns
// that entry. The calling function is then responsible for adding the entry to the
// list of entries.
static LOADER_ENTRY * AddStanzaEntries(REFIT_FILE *File, REFIT_VOLUME *Volume, CHAR16 *Title) {
    CHAR16       **TokenList;
    UINTN        TokenCount;
    LOADER_ENTRY *Entry;
    BOOLEAN      DefaultsSet = FALSE, AddedSubmenu = FALSE;
    REFIT_VOLUME *CurrentVolume = Volume, *PreviousVolume;

    // prepare the menu entry
    Entry = InitializeLoaderEntry(NULL);
    if (Entry == NULL)
        return NULL;

    Entry->Title           = StrDuplicate(Title);
    Entry->me.Title        = AllocateZeroPool(256 * sizeof(CHAR16));
    Entry->me.Title        = PoolPrint(L"Boot %s from %s",
                                       (Title != NULL) ? Title : L"Unknown",
                                       CurrentVolume->VolName);
    Entry->me.Row          = 0;
    Entry->me.BadgeImage   = CurrentVolume->VolBadgeImage;
    Entry->Volume          = CurrentVolume;
    Entry->DiscoveryType   = DISCOVERY_TYPE_MANUAL;

    // Parse the config file to add options for a single stanza, terminating when the token
    // is "}" or when the end of file is reached.
    while (((TokenCount = ReadTokenLine(File, &TokenList)) > 0) && (StrCmp(TokenList[0], L"}") != 0)) {
        if (MyStriCmp(TokenList[0], L"loader") && (TokenCount > 1)) { // set the boot loader filename
            Entry->LoaderPath = StrDuplicate(TokenList[1]);
            LOG(1, LOG_LINE_NORMAL, L"Adding manual loader for '%s'", Entry->LoaderPath);
            SetLoaderDefaults(Entry, TokenList[1], CurrentVolume);
            MyFreePool(Entry->LoadOptions);
            Entry->LoadOptions = NULL; // Discard default options, if any
            DefaultsSet = TRUE;

        } else if (MyStriCmp(TokenList[0], L"volume") && (TokenCount > 1)) {
            PreviousVolume = CurrentVolume;
            if (FindVolume(&CurrentVolume, TokenList[1])) {
                if ((CurrentVolume != NULL) && (CurrentVolume->IsReadable) && (CurrentVolume->RootDir)) {
                    MyFreePool(Entry->me.Title);
                    Entry->me.Title        = PoolPrint(L"Boot %s from %s",
                                                       (Title != NULL) ? Title : L"Unknown",
                                                       CurrentVolume->VolName);
                    Entry->me.BadgeImage   = CurrentVolume->VolBadgeImage;
                    Entry->Volume          = CurrentVolume;
                } else { // It won't work out; reset to previous working volume....
                    CurrentVolume = PreviousVolume;
                } // if/else volume is readable
            } // if match found

        } else if (MyStriCmp(TokenList[0], L"icon") && (TokenCount > 1)) {
            MyFreePool(Entry->me.Image);
            Entry->me.Image = egLoadIcon(CurrentVolume->RootDir, TokenList[1], GlobalConfig.IconSizes[ICON_SIZE_BIG]);
            if (Entry->me.Image == NULL) {
                Entry->me.Image = DummyImage(GlobalConfig.IconSizes[ICON_SIZE_BIG]);
            }

        } else if (MyStriCmp(TokenList[0], L"initrd") && (TokenCount > 1)) {
            MyFreePool(Entry->InitrdPath);
            Entry->InitrdPath = StrDuplicate(TokenList[1]);

        } else if (MyStriCmp(TokenList[0], L"options") && (TokenCount > 1)) {
            MyFreePool(Entry->LoadOptions);
            Entry->LoadOptions = StrDuplicate(TokenList[1]);

        } else if (MyStriCmp(TokenList[0], L"ostype") && (TokenCount > 1)) {
            if (TokenCount > 1) {
                Entry->OSType = TokenList[1][0];
            }

        } else if (MyStriCmp(TokenList[0], L"graphics") && (TokenCount > 1)) {
            Entry->UseGraphicsMode = MyStriCmp(TokenList[1], L"on");

        } else if (MyStriCmp(TokenList[0], L"disabled")) {
            LOG(1, LOG_LINE_NORMAL, L"Entry is disabled");
            Entry->Enabled = FALSE;

        } else if (MyStriCmp(TokenList[0], L"firmware_bootnum") && (TokenCount > 1)) {
            Entry->EfiBootNum = StrToHex(TokenList[1], 0, 16);
            Entry->EfiLoaderPath = NULL;
            Entry->LoaderPath = NULL;
            MyFreePool(Entry->me.Title);
            Entry->me.Title = StrDuplicate(Title);
            Entry->me.BadgeImage = BuiltinIcon(BUILTIN_ICON_VOL_EFI);
            Entry->me.Tag = TAG_FIRMWARE_LOADER;

        } else if (MyStriCmp(TokenList[0], L"submenuentry") && (TokenCount > 1)) {
            AddSubmenu(Entry, File, CurrentVolume, TokenList[1]);
            AddedSubmenu = TRUE;

        } // set options to pass to the loader program
        FreeTokenLine(&TokenList, &TokenCount);
    } // while()

    if (AddedSubmenu)
         AddMenuEntry(Entry->me.SubScreen, &MenuEntryReturn);

    if (Entry->InitrdPath) {
        MergeStrings(&Entry->LoadOptions, L"initrd=", L' ');
        MergeStrings(&Entry->LoadOptions, Entry->InitrdPath, 0);
        MyFreePool(Entry->InitrdPath);
        Entry->InitrdPath = NULL;
    } // if

    if (!DefaultsSet) {
        // user included no "loader" line; use bogus one
        SetLoaderDefaults(Entry, L"\\EFI\\BOOT\\nemo.efi", CurrentVolume);
    }

    return(Entry);
} // static VOID AddStanzaEntries()

// Read the user-configured menu entries from refind.conf and add or delete
// entries based on the contents of that file....
VOID ScanUserConfigured(CHAR16 *FileName)
{
    EFI_STATUS        Status;
    REFIT_FILE        File;
    REFIT_VOLUME      *Volume;
    CHAR16            **TokenList;
    UINTN             TokenCount, size;
    LOADER_ENTRY      *Entry;

    LOG(1, LOG_LINE_THIN_SEP, L"Scanning for user-configured boot stanzas");
    if (FileExists(SelfDir, FileName)) {
        Status = ReadFile(SelfDir, FileName, &File, &size);
        if (EFI_ERROR(Status))
            return;

        Volume = SelfVolume;

        while ((TokenCount = ReadTokenLine(&File, &TokenList)) > 0) {
            if (MyStriCmp(TokenList[0], L"menuentry") && (TokenCount > 1)) {
                Entry = AddStanzaEntries(&File, Volume, TokenList[1]);
                if ((Entry) && (Entry->Enabled)) {
                    if (Entry->me.SubScreen == NULL)
                        GenerateSubScreen(Entry, Volume, TRUE);
                    AddPreparedLoaderEntry(Entry);
                } else {
                    MyFreePool(Entry);
                } // if/else

            } else if (MyStriCmp(TokenList[0], L"include") && (TokenCount == 2) &&
                       MyStriCmp(FileName, GlobalConfig.ConfigFilename)) {
                if (!MyStriCmp(TokenList[1], FileName)) {
                    ScanUserConfigured(TokenList[1]);
                }

            } // if/else if...
            FreeTokenLine(&TokenList, &TokenCount);
        } // while()
    } // if()
} // VOID ScanUserConfigured()

// Create an options file based on /etc/fstab. The resulting file has two options
// lines, one of which boots the system with "ro root={rootfs}" and the other of
// which boots the system with "ro root={rootfs} single", where "{rootfs}" is the
// filesystem identifier associated with the "/" line in /etc/fstab.
static REFIT_FILE * GenerateOptionsFromEtcFstab(REFIT_VOLUME *Volume) {
    UINTN        TokenCount, i;
    REFIT_FILE   *Options = NULL, *Fstab = NULL;
    EFI_STATUS   Status;
    CHAR16       **TokenList, *Line, *Root = NULL;

    if (FileExists(Volume->RootDir, L"\\etc\\fstab")) {
        Options = AllocateZeroPool(sizeof(REFIT_FILE));
        Fstab = AllocateZeroPool(sizeof(REFIT_FILE));
        Status = ReadFile(Volume->RootDir, L"\\etc\\fstab", Fstab, &i);
        if (CheckError(Status, L"while reading /etc/fstab")) {
            if (Options != NULL)
                FreePool(Options);
            if (Fstab != NULL)
                FreePool(Fstab);
            Options = NULL;
            Fstab = NULL;
        } else { // File read; locate root fs and create entries
            Options->Encoding = ENCODING_UTF16_LE;
            while ((TokenCount = ReadTokenLine(Fstab, &TokenList)) > 0) {
                LOG(3, LOG_LINE_NORMAL, L"Read line from /etc/fstab holding %d tokens", TokenCount);
                if (TokenCount > 2) {
                    if (StrCmp(TokenList[1], L"\\") == 0) {
                        Root = PoolPrint(L"%s", TokenList[0]);
                    } else if (StrCmp(TokenList[2], L"\\") == 0) {
                        Root = PoolPrint(L"%s=%s", TokenList[0], TokenList[1]);
                    } // if/elseif/elseif
                    if (Root && (Root[0] != L'\0')) {
                        for (i = 0; i < StrLen(Root); i++)
                            if (Root[i] == '\\')
                                Root[i] = '/';
                        Line = PoolPrint(L"\"Boot with normal options\"    \"ro root=%s\"\n", Root);
                        MergeStrings((CHAR16 **) &(Options->Buffer), Line, 0);
                        MyFreePool(Line);
                        Line = PoolPrint(L"\"Boot into single-user mode\"  \"ro root=%s single\"\n", Root);
                        MergeStrings((CHAR16**) &(Options->Buffer), Line, 0);
                        MyFreePool(Line);
                        Options->BufferSize = StrLen((CHAR16*) Options->Buffer) * sizeof(CHAR16);
                    } // if
                    MyFreePool(Root);
                    Root = NULL;
                 } // if
                 FreeTokenLine(&TokenList, &TokenCount);
            } // while

            if (Options->Buffer) {
                Options->Current8Ptr  = (CHAR8 *)Options->Buffer;
                Options->End8Ptr      = Options->Current8Ptr + Options->BufferSize;
                Options->Current16Ptr = (CHAR16 *)Options->Buffer;
                Options->End16Ptr     = Options->Current16Ptr + (Options->BufferSize >> 1);
            } else {
                MyFreePool(Options);
                Options = NULL;
            }

            MyFreePool(Fstab->Buffer);
            MyFreePool(Fstab);
        } // if/else file read error
    } // if /etc/fstab exists
    return Options;
} // GenerateOptionsFromEtcFstab()

// Create options from partition type codes. Specifically, if the earlier
// partition scan found a partition with a type code corresponding to a root
// filesystem according to the Freedesktop.org Discoverable Partitions Spec
// (http://www.freedesktop.org/wiki/Specifications/DiscoverablePartitionsSpec/),
// this function returns an appropriate file with two lines, one with
// "ro root=/dev/disk/by-partuuid/{GUID}" and the other with that plus "single".
// Note that this function returns the LAST partition found with the
// appropriate type code, so this will work poorly on dual-boot systems or
// if the type code is set incorrectly.
static REFIT_FILE * GenerateOptionsFromPartTypes(VOID) {
    REFIT_FILE   *Options = NULL;
    CHAR16       *Line, *GuidString, *WriteStatus;

    if (GlobalConfig.DiscoveredRoot) {
        Options = AllocateZeroPool(sizeof(REFIT_FILE));
        if (Options) {
            Options->Encoding = ENCODING_UTF16_LE;
            GuidString = GuidAsString(&(GlobalConfig.DiscoveredRoot->PartGuid));
            WriteStatus = GlobalConfig.DiscoveredRoot->IsMarkedReadOnly ? L"ro" : L"rw";
            ToLower(GuidString);
            if (GuidString) {
                Line = PoolPrint(L"\"Boot with normal options\"    \"%s root=/dev/disk/by-partuuid/%s\"\n",
                                 WriteStatus, GuidString);
                MergeStrings((CHAR16 **) &(Options->Buffer), Line, 0);
                MyFreePool(Line);
                Line = PoolPrint(L"\"Boot into single-user mode\"  \"%s root=/dev/disk/by-partuuid/%s single\"\n",
                                 WriteStatus, GuidString);
                MergeStrings((CHAR16**) &(Options->Buffer), Line, 0);
                MyFreePool(Line);
                MyFreePool(GuidString);
            } // if (GuidString)
            Options->BufferSize = StrLen((CHAR16*) Options->Buffer) * sizeof(CHAR16);

            Options->Current8Ptr  = (CHAR8 *)Options->Buffer;
            Options->End8Ptr      = Options->Current8Ptr + Options->BufferSize;
            Options->Current16Ptr = (CHAR16 *)Options->Buffer;
            Options->End16Ptr     = Options->Current16Ptr + (Options->BufferSize >> 1);
        } // if (Options allocated OK)
    } // if (partition has root GUID)
    return Options;
} // REFIT_FILE * GenerateOptionsFromPartTypes()

// Read a Linux kernel options file for a Linux boot loader into memory. The LoaderPath
// and Volume variables identify the location of the options file, but not its name --
// you pass this function the filename of the Linux kernel, initial RAM disk, or other
// file in the target directory, and this function finds the file with a name in the
// comma-delimited list of names specified by LINUX_OPTIONS_FILENAMES within that
// directory and loads it. If a rEFInd options file can't be found, try to generate
// minimal options from /etc/fstab on the same volume as the kernel. This typically
// works only if the kernel is being read from the Linux root filesystem.
//
// The return value is a pointer to the REFIT_FILE handle for the file, or NULL if
// it wasn't found.
REFIT_FILE * ReadLinuxOptionsFile(IN CHAR16 *LoaderPath, IN REFIT_VOLUME *Volume) {
    CHAR16       *OptionsFilename, *FullFilename;
    BOOLEAN      GoOn = TRUE, FileFound = FALSE;
    UINTN        i = 0, size;
    REFIT_FILE   *File = NULL;
    EFI_STATUS   Status;

    do {
        OptionsFilename = FindCommaDelimited(LINUX_OPTIONS_FILENAMES, i++);
        FullFilename = FindPath(LoaderPath);
        if ((OptionsFilename != NULL) && (FullFilename != NULL)) {
            MergeStrings(&FullFilename, OptionsFilename, '\\');
            if (FileExists(Volume->RootDir, FullFilename)) {
                File = AllocateZeroPool(sizeof(REFIT_FILE));
                Status = ReadFile(Volume->RootDir, FullFilename, File, &size);
                if (CheckError(Status, L"while loading the Linux options file")) {
                    if (File != NULL)
                        FreePool(File);
                    File = NULL;
                } else {
                    GoOn = FALSE;
                    FileFound = TRUE;
                } // if/else error
            } // if file exists
        } else { // a filename string is NULL
            GoOn = FALSE;
        } // if/else
        MyFreePool(OptionsFilename);
        MyFreePool(FullFilename);
        OptionsFilename = FullFilename = NULL;
    } while (GoOn);
    if (!FileFound) {
        // No refind_linux.conf file; look for /etc/fstab and try to pull values from there....
        File = GenerateOptionsFromEtcFstab(Volume);
        // If still no joy, try to use Freedesktop.org Discoverable Partitions Spec....
        if (!File)
            File = GenerateOptionsFromPartTypes();
    } // if
    return (File);
} // static REFIT_FILE * ReadLinuxOptionsFile()

// Retrieve a single line of options from a Linux kernel options file
CHAR16 * GetFirstOptionsFromFile(IN CHAR16 *LoaderPath, IN REFIT_VOLUME *Volume) {
    UINTN        TokenCount;
    CHAR16       *Options = NULL;
    CHAR16       **TokenList;
    REFIT_FILE   *File;

    File = ReadLinuxOptionsFile(LoaderPath, Volume);
    if (File != NULL) {
        TokenCount = ReadTokenLine(File, &TokenList);
        if (TokenCount > 1)
            Options = StrDuplicate(TokenList[1]);
        FreeTokenLine(&TokenList, &TokenCount);
        FreePool(File);
    } // if
    return Options;
} // static CHAR16 * GetOptionsFile()

