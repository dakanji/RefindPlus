/*
 * BootMaster/config.c
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
 * Modifications for rEFInd Copyright (c) 2012-2023 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3) or (at your option) any later version.
 *
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2024 Dayo Akanji (sf.net/u/dakanji/profile)
 * Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "lib.h"
#include "icns.h"
#include "menu.h"
#include "scan.h"
#include "apple.h"
#include "config.h"
#include "screenmgt.h"
#include "mystrings.h"
#include "../mok/mok.h"
#include "../include/refit_call_wrapper.h"

// Constants

#define LINUX_OPTIONS_FILENAMES \
L"refind_linux.conf,refind-linux.conf,\
refindplus_linux.conf,refindplus-linux.conf"


#define ENCODING_ISO8859_1                  (0)
#define ENCODING_UTF8                       (1)
#define ENCODING_UTF16_LE                   (2)

#define LAST_MINUTE                      (1439) /* Last minute of a day */

INTN                   LogLevelConfig  =     0;

UINTN                  TotalEntryCount =     0;
UINTN                  ValidEntryCount =     0;

BOOLEAN                OuterLoop       =  TRUE;
BOOLEAN                SetShowTools    = FALSE;
BOOLEAN                ManualInclude   = FALSE;

#if REFIT_DEBUG > 0
BOOLEAN                FoundFontImage  =  TRUE;
#endif

// Control Forensic Logging
#if REFIT_DEBUG > 1
    BOOLEAN            ForensicLogging =  TRUE;
#else
    BOOLEAN            ForensicLogging = FALSE;
#endif

extern BOOLEAN         ForceTextOnly;

#if REFIT_DEBUG > 0
static
BOOLEAN LogUpdate (
    CHAR16   *TokenName,
    BOOLEAN   MuteFlagOn,
    BOOLEAN   UseTypeReset
) {
    if (MuteFlagOn) MuteLogger = FALSE;
    if (UseTypeReset) {
        LOG_MSG("%s  - Reset:- '%s'", OffsetNext, TokenName);
    }
    else {
        LOG_MSG("%s ** Avoid:- '%s'", OffsetNext, TokenName);
    }
    if (MuteFlagOn) MuteLogger = TRUE;

    return TRUE;
} // static BOOLEAN LogUpdate()
#endif

// Sets GlobalConfig.LinuxMatchPatterns based on the input comma-delimited set
// of prefixes. An asterisk ("*") is added to each of the input prefixes and
// GlobalConfig.LinuxMatchPatterns is set to the resulting comma-delimited
// string.
static
VOID SetLinuxMatchPatterns (
    CHAR16 *Prefixes
) {
    UINTN   i;
    CHAR16 *Pattern;
    CHAR16 *PatternSet;

    i = 0;
    PatternSet = NULL;
    while ((Pattern = FindCommaDelimited (Prefixes, i++)) != NULL) {
        MergeStrings (&Pattern, L"*", 0);
        MergeStrings (&PatternSet, Pattern, L',');
        MY_FREE_POOL(Pattern);
    }

    MY_FREE_POOL(GlobalConfig.LinuxMatchPatterns);
    GlobalConfig.LinuxMatchPatterns = PatternSet;
} // static VOID SetLinuxMatchPatterns()

static
VOID SyncLinuxPrefixes (VOID) {
    if (GlobalConfig.LinuxPrefixes == NULL) {
        GlobalConfig.LinuxPrefixes = StrDuplicate (
            LINUX_PREFIXES
        );
    }
    else if (GlobalConfig.HelpScan) {
        MergeUniqueItems (
            &GlobalConfig.LinuxPrefixes,
            LINUX_PREFIXES, L','
        );
    }

    SetLinuxMatchPatterns (GlobalConfig.LinuxPrefixes);
} // static VOID SyncLinuxPrefixes()

static
VOID SyncAlsoScan (VOID) {
    if (GlobalConfig.AlsoScan == NULL) {
        GlobalConfig.AlsoScan = StrDuplicate (
            ALSO_SCAN_DIRS
        );
    }
    else if (GlobalConfig.HelpScan) {
        MergeUniqueItems (
            &GlobalConfig.AlsoScan,
            ALSO_SCAN_DIRS, L','
        );
    }
} // static VOID SyncAlsoScan()

static
VOID SyncDontScanDirs (VOID) {
    CHAR16 *GuidString;


    if (SelfVolume  == NULL ||
        SelfDirPath == NULL
    ) {
        return;
    }

    if (GuidsAreEqual (&(SelfVolume->PartGuid), &GuidNull)) {
        return;
    }

    GuidString = GuidAsString (&(SelfVolume->PartGuid));
    if (GuidString == NULL) {
        return;
    }

    MergeStrings (
        &GlobalConfig.DontScanDirs,
        GuidString, L','
    );
    MergeStrings (
        &GlobalConfig.DontScanDirs,
        SelfDirPath, L':'
    );
    MY_FREE_POOL(GuidString);
} // static VOID SyncDontScanDirs()

static
VOID SyncDontScanFiles (VOID) {
    if (GlobalConfig.DontScanFiles == NULL) {
        GlobalConfig.DontScanFiles = StrDuplicate (
            DONT_SCAN_FILES
        );
    }
    else if (GlobalConfig.HelpScan) {
        MergeUniqueItems (
            &GlobalConfig.DontScanFiles,
            DONT_SCAN_FILES, L','
        );
    }

    if (!GlobalConfig.HelpScan) {
        return;
    }

    // Handle MEMTEST_NAMES in 'ScanLoaderDir' and not here
    // to accomodate fallback loaders in the list.

    MergeUniqueItems (
        &GlobalConfig.DontScanFiles,
        SHELL_FILES, L','
    );
    MergeUniqueItems (
        &GlobalConfig.DontScanFiles,
        GDISK_FILES, L','
    );
    MergeUniqueItems (
        &GlobalConfig.DontScanFiles,
        GPTSYNC_FILES, L','
    );
    MergeUniqueItems (
        &GlobalConfig.DontScanFiles,
        NETBOOT_FILES, L','
    );
    MergeUniqueItems (
        &GlobalConfig.DontScanFiles,
        FWUPDATE_NAMES, L','
    );
    MergeUniqueItems (
        &GlobalConfig.DontScanFiles,
        MOK_NAMES, L','
    );
    MergeUniqueItems (
        &GlobalConfig.DontScanFiles,
        NVRAMCLEAN_FILES, L','
    );
    MergeUniqueItems (
        &GlobalConfig.DontScanFiles,
        FALLBACK_SKIPNAME, L','
    );
    MergeUniqueItems (
        &GlobalConfig.DontScanFiles,
        GlobalConfig.WindowsRecoveryFiles, L','
    );
    MergeUniqueItems (
        &GlobalConfig.DontScanFiles,
        GlobalConfig.MacOSRecoveryFiles, L','
    );
} // static VOID SyncDontScanFiles()

static
VOID SyncShowTools (VOID) {
    if (SetShowTools) {
        return;
    }

    SetShowTools               =              TRUE;
    GlobalConfig.ShowTools[0]  =         TAG_SHELL;
    GlobalConfig.ShowTools[1]  =       TAG_MEMTEST;
    GlobalConfig.ShowTools[2]  =         TAG_GDISK;
    GlobalConfig.ShowTools[3]  =  TAG_RECOVERY_MAC;
    GlobalConfig.ShowTools[4]  =  TAG_RECOVERY_WIN;
    GlobalConfig.ShowTools[5]  =      TAG_MOK_TOOL;
    GlobalConfig.ShowTools[6]  =         TAG_ABOUT;
    GlobalConfig.ShowTools[7]  =        TAG_HIDDEN;
    GlobalConfig.ShowTools[8]  =      TAG_SHUTDOWN;
    GlobalConfig.ShowTools[9]  =        TAG_REBOOT;
    GlobalConfig.ShowTools[10] =      TAG_FIRMWARE;
    GlobalConfig.ShowTools[11] = TAG_FWUPDATE_TOOL;
} // static VOID SyncShowTools()

// Get a single line of text from a file
static
CHAR16 * ReadLine (
    REFIT_FILE *File
) {
    CHAR16  *Line;
    CHAR16  *qChar16;
    CHAR16  *pChar16;
    CHAR16  *LineEndChar16;
    CHAR16  *LineStartChar16;

    CHAR8   *pChar08;
    CHAR8   *LineEndChar08;
    CHAR8   *LineStartChar08;
    UINTN    LineLength;


    if (File->Buffer == NULL) {
        // Early Return
        return NULL;
    }

    if (File->Encoding != ENCODING_UTF8      &&
        File->Encoding != ENCODING_UTF16_LE  &&
        File->Encoding != ENCODING_ISO8859_1
    ) {
        // Early Return ... Unsupported encoding
        return NULL;
    }

    if (File->Encoding == ENCODING_UTF8 ||
        File->Encoding == ENCODING_ISO8859_1
    ) {
        pChar08 = File->Current8Ptr;
        if (pChar08 >= File->End8Ptr) {
            // Early Return
            return NULL;
        }

        LineStartChar08 = pChar08;
        for (; pChar08 < File->End8Ptr; pChar08++) {
            if (*pChar08 == 13 || *pChar08 == 10) {
                break;
            }
        }
        LineEndChar08 = pChar08;
        for (; pChar08 < File->End8Ptr; pChar08++) {
            if (*pChar08 != 13 && *pChar08 != 10) {
                break;
            }
        }
        File->Current8Ptr = pChar08;

        LineLength = (UINTN) (LineEndChar08 - LineStartChar08) + 1;
        Line = AllocatePool (sizeof (CHAR16) * LineLength);
        if (Line == NULL) {
            // Early Return
            return NULL;
        }

        qChar16 = Line;
        if (File->Encoding == ENCODING_ISO8859_1) {
            for (pChar08 = LineStartChar08; pChar08 < LineEndChar08; ) {
                *qChar16++ = *pChar08++;
            }
        }
        else if (File->Encoding == ENCODING_UTF8) {
            // DA-TAG: Investigate This
            //         Actually handle UTF-8
            //         Currently just duplicates previous block
            for (pChar08 = LineStartChar08; pChar08 < LineEndChar08; ) {
                *qChar16++ = *pChar08++;
            }
        }
        *qChar16 = 0;

        return Line;
    }

    // Encoding is ENCODING_UTF16_LE
    pChar16 = File->Current16Ptr;
    if (pChar16 >= File->End16Ptr) {
        // Early Return
        return NULL;
    }

    LineStartChar16 = pChar16;
    for (; pChar16 < File->End16Ptr; pChar16++) {
        if (*pChar16 == 13 || *pChar16 == 10) {
            break;
        }
    }
    LineEndChar16 = pChar16;
    for (; pChar16 < File->End16Ptr; pChar16++) {
        if (*pChar16 != 13 && *pChar16 != 10) {
            break;
        }
    }
    File->Current16Ptr = pChar16;

    LineLength = (UINTN) (LineEndChar16 - LineStartChar16) + 1;
    Line = AllocatePool (sizeof (CHAR16) * LineLength);
    if (Line == NULL) {
        // Early Return
        return NULL;
    }

    for (pChar16 = LineStartChar16, qChar16 = Line; pChar16 < LineEndChar16; ) {
        *qChar16++ = *pChar16++;
    }
    *qChar16 = 0;

    return Line;
} // static CHAR16 * ReadLine

// Returns FALSE if *p points to the end of a token, TRUE otherwise.
// Also modifies *p **IF** the first and second characters are both
// quotes ('"'); it deletes one of them.
static
BOOLEAN KeepReading (
    IN OUT CHAR16  *InString,
    IN OUT BOOLEAN *IsQuoted
) {
    CHAR16  *Temp;
    UINTN    DestSize;
    BOOLEAN  MoreToRead;

    // Check if pointers are NULL or if the string pointed to by 'InString' is empty
    if (IsQuoted == NULL || InString == NULL || *InString == L'\0') {
        return FALSE;
    }

    if ((
        *InString != ' '  &&
        *InString != '\t' &&
        *InString != '='  &&
        *InString != '#'  &&
        *InString != ','
    ) || *IsQuoted) {
        MoreToRead = TRUE;
    }
    else {
        MoreToRead = FALSE;
    }

    if (*InString == L'"') {
        if (InString[1] != L'"') {
            *IsQuoted  = !(*IsQuoted);
            MoreToRead = FALSE;
        }
        else {
            Temp = StrDuplicate (&InString[1]);
            if (Temp != NULL) {
                DestSize = StrSize (InString) / sizeof (CHAR16);
                StrCpyS (InString, DestSize, Temp);
                MY_FREE_POOL(Temp);
            }
            MoreToRead = TRUE;
        }
    } // if first character is a quote

    return MoreToRead;
} // static BOOLEAN KeepReading()

// Handle a parameter with a single integer argument (signed)
static
VOID HandleSignedInt (
    IN  CHAR16 **TokenList,
    IN  UINTN    TokenCount,
    OUT INTN    *Value
) {
    if (TokenCount == 2) {
        *Value = (TokenList[1][0] == '-')
            ? Atoi(TokenList[1]+1) * -1
            : Atoi(TokenList[1]);
    }
} // static VOID HandleSignedInt()

// Handle a parameter with a single integer argument (unsigned)
static
VOID HandleUnsignedInt (
    IN  CHAR16 **TokenList,
    IN  UINTN    TokenCount,
    OUT UINTN   *Value
) {
    if (TokenCount == 2) {
        *Value = Atoi(TokenList[1]);
    }
} // static VOID HandleUnsignedInt()

// Handle a parameter with a single string argument
static
VOID HandleString (
    IN  CHAR16  **TokenList,
    IN  UINTN     TokenCount,
    OUT CHAR16  **Target
) {
    if (TokenCount == 2 && Target != NULL) {
        MY_FREE_POOL(*Target);
        *Target = StrDuplicate (TokenList[1]);
    }
} // static VOID HandleString()

// Handle a parameter with a series of string arguments, to replace or be added
// to a comma-delimited list. Passes each token through the CleanUpPathNameSlashes()
// function to ensure consistency in subsequent comparisons of filenames. If the
// first non-keyword token is "+", the list is added to the existing target
// string; otherwise, the tokens replace the current string.
static
VOID HandleStrings (
    IN  CHAR16 **TokenList,
    IN  UINTN    TokenCount,
    OUT CHAR16 **Target
) {
    UINTN   i;
    BOOLEAN AddMode;

    if (Target == NULL) {
        return;
    }

    if (TokenCount > 2 && MyStriCmp (TokenList[1], L"+")) {
        AddMode = TRUE;
    }
    else {
        AddMode = FALSE;
    }

    if (!AddMode && *Target != NULL) {
        MY_FREE_POOL(*Target);
    }

    for (i = 1; i < TokenCount; i++) {
        if ((i != 1) || !AddMode) {
            CleanUpPathNameSlashes (TokenList[i]);
            MergeStrings (Target, TokenList[i], L',');
        }
    }
} // static VOID HandleStrings()

// Handle a parameter with a series of hexadecimal arguments, to replace
// or be added to a linked list of UINT32 values. Any item with a non-hexadecimal
// value is discarded, as is any value that exceeds MaxValue. If the first
// non-keyword token is "+", the new list is added to the existing Target;
// otherwise, the interpreted tokens replace the current Target.
static
VOID HandleHexes (
    IN  CHAR16       **TokenList,
    IN  UINTN          TokenCount,
    IN  UINTN          MaxValue,
    OUT UINT32_LIST  **Target
) {
    UINTN        i;
    UINTN        InputIndex;
    UINT32       Value;
    UINT32_LIST *EndOfList;
    UINT32_LIST *NewEntry;

    if (TokenCount > 2 && MyStriCmp (TokenList[1], L"+")) {
        InputIndex = 2;
        EndOfList  = *Target;
        while (
            EndOfList       != NULL &&
            EndOfList->Next != NULL
        ) {
            EndOfList = EndOfList->Next;
        }
    }
    else {
        InputIndex =    1;
        EndOfList  = NULL;
        EraseUint32List (Target);
    }

    for (i = InputIndex; i < TokenCount; i++) {
        if (!IsValidHex (TokenList[i])) {
            continue;
        }

        Value = (UINT32) StrToHex (TokenList[i], 0, 8);
        if (Value > MaxValue) {
            continue;
        }

        NewEntry = AllocatePool (sizeof (UINT32_LIST));
        if (NewEntry == NULL) {
            return;
        }

        NewEntry->Value = Value;
        NewEntry->Next = NULL;
        if (EndOfList == NULL) {
            EndOfList = NewEntry;
            *Target = NewEntry;
        }
        else {
            EndOfList->Next = NewEntry;
            EndOfList       = NewEntry;
        }
    } // for
} // static VOID HandleHexes()

// Convert TimeString (in "HH:MM" format) to a pure-minute format. Values should be
// in the range from 0 (for 00:00, or midnight) to 1439 (for 23:59; aka LAST_MINUTE).
// Any value outside that range denotes an error in the specification. Note that if
// the input is a number that includes no colon, this function will return the original
// number in UINTN form.
static
UINTN HandleTime (
    IN CHAR16 *TimeString
) {
    UINTN i, Hour, Minute, TimeLength;

    TimeLength = StrLen (TimeString);
    i = Hour = Minute = 0;
    while (i < TimeLength) {
        if (TimeString[i] == L':') {
            Hour = Minute;
            Minute = 0;
        }

        if ((TimeString[i] >= L'0') && (TimeString[i] <= '9')) {
            Minute *= 10;
            Minute += (TimeString[i] - L'0');
        }

        i++;
    } // while

    return (Hour * 60 + Minute);
} // static BOOLEAN HandleTime()

static
BOOLEAN HandleBoolean (
    IN CHAR16 **TokenList,
    IN UINTN    TokenCount
) {
    BOOLEAN TruthValue;

    TruthValue = TRUE;
    if (TokenCount >= 2 &&
        (
            MyStriCmp (TokenList[1], L"0")   ||
            MyStriCmp (TokenList[1], L"off") ||
            MyStriCmp (TokenList[1], L"false")
        )
    ) {
        TruthValue = FALSE;
    }

    return TruthValue;
} // static BOOLEAN HandleBoolean()

// Sets the default boot loader IF the current time is within the bounds
// defined by the third and fourth tokens in the TokenList.
static
VOID SetDefaultByTime (
    IN  CHAR16 **TokenList,
    OUT CHAR16 **Default
) {
    EFI_STATUS            Status;
    UINTN                 Now;
    UINTN                 EndTime;
    UINTN                 StartTime;
    CHAR16               *MsgStr;
    EFI_TIME              CurrentTime;
    BOOLEAN               SetIt;

    StartTime = HandleTime (TokenList[2]);
    EndTime   = HandleTime (TokenList[3]);

    if (StartTime <= LAST_MINUTE &&
        EndTime   <= LAST_MINUTE
    ) {
        Status = REFIT_CALL_2_WRAPPER(gRT->GetTime, &CurrentTime, NULL);
        if (EFI_ERROR(Status)) {
            return;
        }

        Now = CurrentTime.Hour * 60 + CurrentTime.Minute;
        if (Now > LAST_MINUTE) {
            // Should not happen ... Just being paranoid
            MsgStr = PoolPrint (
                L"ERROR: Impossible System Time:- %d:%d",
                CurrentTime.Hour, CurrentTime.Minute
            );

            #if REFIT_DEBUG > 0
            LOG_MSG("  - %s", MsgStr);
            LOG_MSG("\n");
            #endif

            Print (L"%s\n", MsgStr);
            MY_FREE_POOL(MsgStr);

            // Early Return
            return;
        }

        SetIt = FALSE;
        if (StartTime < EndTime) {
            // Time range does NOT cross midnight
            if (Now >= StartTime && Now <= EndTime) {
                SetIt = TRUE;
            }
        }
        else {
            // Time range DOES cross midnight
            if (Now >= StartTime || Now <= EndTime) {
                SetIt = TRUE;
            }
        }

        if (SetIt) {
            MY_FREE_POOL(*Default);
            *Default = StrDuplicate (TokenList[1]);
        }
    } // if ((StartTime <= LAST_MINUTE) && (EndTime <= LAST_MINUTE))
} // static VOID SetDefaultByTime()

static
LOADER_ENTRY * AddPreparedLoaderEntry (
    LOADER_ENTRY *Entry
) {
    AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *) Entry);

    return Entry;
} // static LOADER_ENTRY * AddPreparedLoaderEntry()

static
VOID AddSubmenu (
    LOADER_ENTRY *Entry,
    REFIT_FILE   *File,
    REFIT_VOLUME *Volume,
    CHAR16       *Title
) {
    REFIT_MENU_SCREEN   *SubScreen;
    LOADER_ENTRY        *SubEntry;
    UINTN                TokenCount;
    CHAR16              *TmpName;
    CHAR16             **TokenList;
    BOOLEAN              TitleVolume;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    SubScreen = InitializeSubScreen (Entry);
    BREAD_CRUMB(L"%a:  2", __func__);
    if (SubScreen == NULL) {
        BREAD_CRUMB(L"%a:  1a 1 - END:- VOID", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return;
    }

    // Set defaults for the new entry
    // Will be modified based on lines read from the config file
    BREAD_CRUMB(L"%a:  3", __func__);
    SubEntry = CopyLoaderEntry (Entry);
    BREAD_CRUMB(L"%a:  4", __func__);
    if (SubEntry == NULL) {
        BREAD_CRUMB(L"%a:  4a 1", __func__);
        FreeMenuScreen (&SubScreen);

        BREAD_CRUMB(L"%a:  4a 2 - END:- VOID", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return;
    }

    BREAD_CRUMB(L"%a:  5", __func__);
    SubEntry->Enabled = TRUE;
    TitleVolume = FALSE;

    while (
        SubEntry->Enabled &&
        (TokenCount = ReadTokenLine (File, &TokenList)) > 0 &&
        StrCmp (TokenList[0], L"}") != 0
    ) {
        LOG_SEP(L"X");
        BREAD_CRUMB(L"%a:  5a 1 - WHILE LOOP:- START", __func__);
        if (MyStriCmp (TokenList[0], L"disabled")) {
            BREAD_CRUMB(L"%a:  5a 1a", __func__);
            SubEntry->Enabled = FALSE;
        }
        else if (
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"loader")
        ) {
            BREAD_CRUMB(L"%a:  5a 1b", __func__);

            // Set the boot loader filename
            MY_FREE_POOL(SubEntry->LoaderPath);
            SubEntry->LoaderPath = StrDuplicate (TokenList[1]);
            SubEntry->Volume     = Volume;
        }
        else if (
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"volume")
        ) {
            BREAD_CRUMB(L"%a:  5a 1c", __func__);

            if (FindVolume (&Volume, TokenList[1])) {
                if (Volume          != NULL &&
                    Volume->RootDir != NULL &&
                    Volume->IsReadable
                ) {
                    TitleVolume = TRUE;
                    MY_FREE_IMAGE(SubEntry->me.BadgeImage);

                    SetVolumeBadgeIcon (Volume);
                    SubEntry->Volume        = Volume;
                    SubEntry->me.BadgeImage = egCopyImage (
                        Volume->VolBadgeImage
                    );
                }
            }
        }
        else if (
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"initrd")
        ) {
            BREAD_CRUMB(L"%a:  5a 1d", __func__);
            MY_FREE_POOL(SubEntry->InitrdPath);
            SubEntry->InitrdPath = StrDuplicate (TokenList[1]);
        }
        else if (
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"options")
        ) {
            BREAD_CRUMB(L"%a:  5a 1e", __func__);
            MY_FREE_POOL(SubEntry->LoadOptions);
            SubEntry->LoadOptions = StrDuplicate (TokenList[1]);
        }
        else if (
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"add_options")
        ) {
            BREAD_CRUMB(L"%a:  5a 1f", __func__);

            MergeStrings (&SubEntry->LoadOptions, TokenList[1], L' ');
        }
        else if (
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"graphics")
        ) {
            BREAD_CRUMB(L"%a:  5a 1g", __func__);

            SubEntry->UseGraphicsMode = MyStriCmp (TokenList[1], L"on");
        }
        else {
            BREAD_CRUMB(L"%a:  5a 1h - WARN ... ''%s' Token is Invalid!!", __func__, TokenList[0]);
        }

        BREAD_CRUMB(L"%a:  5a 2", __func__);
        FreeTokenLine (&TokenList, &TokenCount);

        BREAD_CRUMB(L"%a:  5a 3 - WHILE LOOP:- END", __func__);
        LOG_SEP(L"X");
    } // while

    BREAD_CRUMB(L"%a:  6", __func__);
    if (!SubEntry->Enabled) {
        BREAD_CRUMB(L"%a:  6a 1", __func__);
        FreeMenuEntry ((REFIT_MENU_ENTRY **) SubEntry);

        BREAD_CRUMB(L"%a:  6a 2 - END:- VOID", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_STAR_HEAD_SEP, L"SubEntry is Disabled");
        #endif

        return;
    }

    BREAD_CRUMB(L"%a:  7", __func__);
    MY_FREE_POOL(SubEntry->me.Title);
    if (!TitleVolume) {
        BREAD_CRUMB(L"%a:  7a 1", __func__);

        SubEntry->me.Title = StrDuplicate (
            (Title != NULL) ? Title : L"Instance: Unknown"
        );
    }
    else {
        BREAD_CRUMB(L"%a:  7b 1", __func__);

        TmpName = (Title != NULL)
            ? Title : L"Instance: Unknown";

        SubEntry->me.Title = PoolPrint (
            L"Load %s%s%s%s%s",
            TmpName,
            SetVolJoin (TmpName, TRUE                           ),
            SetVolKind (TmpName, Volume->VolName, Volume->FSType),
            SetVolFlag (TmpName, Volume->VolName                ),
            SetVolType (TmpName, Volume->VolName, Volume->FSType)
        );
    }

    BREAD_CRUMB(L"%a:  8", __func__);
    if (SubEntry->InitrdPath != NULL) {
        BREAD_CRUMB(L"%a:  8a 1", __func__);
        MergeStrings (&SubEntry->LoadOptions, L"initrd=", L' ');
        MergeStrings (&SubEntry->LoadOptions, SubEntry->InitrdPath, 0);
        MY_FREE_POOL(SubEntry->InitrdPath);
        BREAD_CRUMB(L"%a:  8a 2", __func__);
    }

    BREAD_CRUMB(L"%a:  9", __func__);
    AddSubMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);

    // DA-TAG: Investigate This
    //         Freeing the SubScreen below causes a hang
    //BREAD_CRUMB(L"%a:  10", __func__);
    //FreeMenuScreen (&Entry->me.SubScreen);

    BREAD_CRUMB(L"%a:  10", __func__);
    Entry->me.SubScreen = SubScreen;

    BREAD_CRUMB(L"%a:  11 - END:- VOID", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // static VOID AddSubmenu()

// Adds the options from a single config.conf stanza to a new loader entry and returns
// that entry. The calling function is then responsible for adding the entry to the
// list of entries.
static
LOADER_ENTRY * AddStanzaEntries (
    REFIT_FILE   *File,
    REFIT_VOLUME *Volume,
    CHAR16       *Title
) {
    UINTN           TokenCount;
    CHAR16        **TokenList;
    CHAR16         *OurEfiBootNumber;
    CHAR16         *LoadOptions;
    CHAR16         *LoaderToken;
    BOOLEAN         RetVal;
    BOOLEAN         HasPath;
    BOOLEAN         DoneIcon;
    BOOLEAN         DoneLoader;
    BOOLEAN         DefaultsSet;
    BOOLEAN         AddedSubmenu;
    BOOLEAN         FirmwareBootNum;
    REFIT_VOLUME   *CurrentVolume;
    REFIT_VOLUME   *PreviousVolume;
    LOADER_ENTRY   *Entry;

    #if REFIT_DEBUG > 0
    static BOOLEAN  OtherCall = FALSE;
    #endif

    // Prepare the menu entry
    Entry = InitializeLoaderEntry (NULL);
    if (Entry == NULL) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_STAR_SEPARATOR,
            L"Could *NOT* Initialise Loader Entry for User Defined Stanza"
        );
        #endif

        return NULL;
    }

    Entry->Title = (Title != NULL)
        ? PoolPrint (L"Manual Stanza: %s", Title)
        : StrDuplicate (L"Manual Stanza: Title *NOT* Found");
    Entry->me.Row          = 0;
    Entry->Enabled         = TRUE;
    Entry->Volume          = Volume;
    Entry->DiscoveryType   = DISCOVERY_TYPE_MANUAL;

    // Parse the config file to add options for a single stanza,
    // terminating when the token is "}" or when the end of file is reached.
    #if REFIT_DEBUG > 0
    /* Exception for LOG_LINE_THIN_SEP */
    ALT_LOG(1, LOG_LINE_THIN_SEP,
        L"%s",
        (!OtherCall) ? L"FIRST STANZA" : L"NEXT STANZA"
    );
    OtherCall = TRUE;
    #endif

    CurrentVolume   = Volume;
    LoaderToken     = LoadOptions  = OurEfiBootNumber =  NULL;
    FirmwareBootNum = AddedSubmenu = DefaultsSet      = FALSE;
    DoneLoader      = DoneIcon                        = FALSE;

    while (
        Entry->Enabled &&
        (TokenCount = ReadTokenLine (File, &TokenList)) > 0
    ) {
        if (MyStriCmp (TokenList[0], L"}")) {
            FreeTokenLine (&TokenList, &TokenCount);
            break;
        }

        // Set options to pass to the loader program - START
        if (TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"graphics")
        ) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- 'graphics'");
            #endif

            Entry->UseGraphicsMode = MyStriCmp (TokenList[1], L"on");
        }
        else if (
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"ostype")
        ) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- 'ostype'");
            #endif

            Entry->OSType = TokenList[1][0];
        }
        else if (
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"icon")
        ) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- 'icon'");
            #endif

            if (AllowGraphicsMode) {
                // DA-TAG: Avoid Memory Leak
                MY_FREE_IMAGE(Entry->me.Image);
                Entry->me.Image = egLoadIcon (
                    CurrentVolume->RootDir, TokenList[1],
                    GlobalConfig.IconSizes[ICON_SIZE_BIG]
                );

                if (Entry->me.Image == NULL) {
                    // Set dummy image if icon was not found
                    Entry->me.Image = DummyImage (
                        GlobalConfig.IconSizes[ICON_SIZE_BIG]
                    );
                }
            }

            DoneIcon = TRUE;
        }
        else if (
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"loader")
        ) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- 'loader'");
            #endif

            HasPath = (TokenList[1] && StrLen (TokenList[1]) > 0);
            if (HasPath) {
                if (!DoneIcon) {
                    MY_FREE_POOL(LoaderToken);
                    LoaderToken = StrDuplicate (TokenList[1]);
                }
                else {
                    // Set the boot loader filename
                    // DA-TAG: Avoid Memory Leak
                    MY_FREE_POOL(Entry->LoaderPath);
                    Entry->LoaderPath = StrDuplicate (TokenList[1]);

                    HasPath = (
                        Entry->LoaderPath &&
                        StrLen (Entry->LoaderPath) > 0
                    );

                    if (HasPath) {
                        #if REFIT_DEBUG > 0
                        ALT_LOG(1, LOG_LINE_NORMAL,
                            L"Add Loader Path:- '%s'",
                            Entry->LoaderPath
                        );
                        #endif

                        SetLoaderDefaults (
                            Entry, TokenList[1], CurrentVolume
                        );

                        DefaultsSet = TRUE;
                    }

                    DoneLoader = TRUE;
                }
            }
        }
        else if (
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"volume")
        ) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- 'volume'");
            #endif

            PreviousVolume = CurrentVolume;
            if (!FindVolume (&CurrentVolume, TokenList[1])) {
                #if REFIT_DEBUG > 0
                ALT_LOG(1, LOG_THREE_STAR_MID,
                    L"Could *NOT* Find Volume:- '%s'",
                    TokenList[1]
                );
                #endif
            }
            else {
                if (CurrentVolume          != NULL &&
                    CurrentVolume->RootDir != NULL &&
                    CurrentVolume->IsReadable
                ) {
                    Entry->Volume = CurrentVolume;

                    // DA-TAG: Avoid Memory Leak
                    MY_FREE_IMAGE(Entry->me.BadgeImage);
                    Entry->me.BadgeImage = egCopyImage (
                        CurrentVolume->VolBadgeImage
                    );
                }
                else {
                    #if REFIT_DEBUG > 0
                    ALT_LOG(1, LOG_THREE_STAR_MID,
                        L"Could *NOT* Add Volume ... Revert to Previous:- '%s'",
                        PreviousVolume->VolName
                    );
                    #endif

                    // Invalid ... Reset to previous working volume
                    CurrentVolume = PreviousVolume;
                }
            } // if/else !FindVolume
        }
        else if (
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"initrd")
        ) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- 'initrd'");
            #endif

            // DA-TAG: Avoid Memory Leak
            MY_FREE_POOL(Entry->InitrdPath);
            Entry->InitrdPath = StrDuplicate (TokenList[1]);
        }
        else if (
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"options")
        ) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_THREE_STAR_MID, L"Handle Token:- 'options'");
            #endif

            // DA-TAG: Avoid Memory Leak
            MY_FREE_POOL(LoadOptions);
            LoadOptions = StrDuplicate (TokenList[1]);
        }
        else if (
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"firmware_bootnum")
        ) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_THREE_STAR_MID,
                L"Handle Token:- 'firmware_bootnum'"
            );
            #endif

            Entry->me.Tag = TAG_FIRMWARE_LOADER;

            Entry->me.BadgeImage = BuiltinIcon (BUILTIN_ICON_VOL_EFI);
            if (Entry->me.BadgeImage == NULL) {
                // Set dummy image if badge was not found
                Entry->me.BadgeImage = DummyImage (
                    GlobalConfig.IconSizes[ICON_SIZE_BADGE]
                );
            }

            DefaultsSet     = TRUE;
            FirmwareBootNum = TRUE;

            MY_FREE_POOL(OurEfiBootNumber);
            OurEfiBootNumber = StrDuplicate (TokenList[1]);
        }
        else if (
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"submenuentry")
        ) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
            ALT_LOG(1, LOG_LINE_SPECIAL, L"***[ Add SubMenu Item(s) ]***");
            #endif

            AddSubmenu (Entry, File, CurrentVolume, TokenList[1]);
            AddedSubmenu = TRUE;
        }
        else if (MyStriCmp (TokenList[0], L"disabled")) {
            Entry->Enabled = FALSE;
        } // Set options to pass to the loader program - End

        FreeTokenLine (&TokenList, &TokenCount);
    } // while Entry->Enabled

    if (!Entry->Enabled) {
        FreeMenuEntry ((REFIT_MENU_ENTRY **) Entry);
        MY_FREE_POOL(LoadOptions     );
        MY_FREE_POOL(OurEfiBootNumber);

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_STAR_HEAD_SEP, L"Entry is Disabled");
        #endif

        return NULL;
    }

    if (!DoneLoader && LoaderToken && StrLen (LoaderToken) > 0) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_NORMAL, L"Add Loader Path:- '%s'", LoaderToken);
        #endif

        // Set the boot loader filename
        // DA-TAG: Avoid Memory Leak
        MY_FREE_POOL(Entry->LoaderPath);
        Entry->LoaderPath = StrDuplicate (LoaderToken);

        SetLoaderDefaults (Entry, LoaderToken, CurrentVolume);
        MY_FREE_POOL(LoaderToken);

        DefaultsSet = TRUE;
    }

    // Set Screen Title
    if (!FirmwareBootNum && Entry->Volume->VolName != NULL) {
        Entry->me.Title = PoolPrint (
            L"Load %s%s%s%s%s",
            Entry->Title,
            SetVolJoin (Entry->Title, TRUE                           ),
            SetVolKind (Entry->Title, Volume->VolName, Volume->FSType),
            SetVolFlag (Entry->Title, Volume->VolName                ),
            SetVolType (Entry->Title, Volume->VolName, Volume->FSType)
        );
    }
    else {
        if (!FirmwareBootNum) {
            Entry->me.Title = PoolPrint (L"Load %s", Entry->Title);
        }
        else {
            // Clear potentially wrongly set items
            MY_FREE_POOL(Entry->InitrdPath   );
            MY_FREE_POOL(Entry->LoaderPath   );
            MY_FREE_POOL(Entry->EfiLoaderPath);

            Entry->me.Title = PoolPrint (
                L"Load %s ... [Firmware Boot Number]",
                Entry->Title
            );

            Entry->EfiBootNum = StrToHex (OurEfiBootNumber, 0, 16);
        }
    }

    // Set load options, if any
    // DA-TAG: Remove any previously set values first
    MY_FREE_POOL(Entry->LoadOptions);
    if (LoadOptions != NULL && StrLen (LoadOptions) > 0) {
        Entry->LoadOptions = StrDuplicate (LoadOptions);
    }

    if (AddedSubmenu) {
        RetVal = GetMenuEntryReturn (&Entry->me.SubScreen);
        if (!RetVal) {
            FreeMenuScreen (&Entry->me.SubScreen);
        }
    }

    if (Entry->InitrdPath != NULL   &&
        StrLen (Entry->InitrdPath) > 0
    ) {
        if (Entry->LoadOptions != NULL   &&
            StrLen (Entry->LoadOptions) > 0
        ) {
            MergeStrings (&Entry->LoadOptions, L"initrd=", L' '    );
            MergeStrings (&Entry->LoadOptions, Entry->InitrdPath, 0);
        }
        else {
            if (Entry->LoadOptions != NULL    &&
                StrLen (Entry->LoadOptions) == 0
            ) {
                MY_FREE_POOL(Entry->LoadOptions);
            }

            Entry->LoadOptions = PoolPrint (
                L"initrd=%s",
                Entry->InitrdPath
            );
        }

        MY_FREE_POOL(Entry->InitrdPath);
    }

    if (!DefaultsSet) {
        // No "loader" line ... use bogus one
        SetLoaderDefaults (
            Entry, L"\\EFI\\BOOT\\nemo.efi", CurrentVolume
        );
    }

    if (AllowGraphicsMode && Entry->me.Image == NULL) {
        // Still no icon ... set dummy image
        Entry->me.Image = DummyImage (
            GlobalConfig.IconSizes[ICON_SIZE_BIG]
        );
    }

    MY_FREE_POOL(LoadOptions     );
    MY_FREE_POOL(OurEfiBootNumber);

    return Entry;
} // static VOID AddStanzaEntries()

// Create an options file based on /etc/fstab. The resulting file has two options
// lines, one of which boots the system with "ro root={rootfs}" and the other of
// which boots the system with "ro root={rootfs} single", where "{rootfs}" is
// the filesystem identifier associated with the "/" line in /etc/fstab.
static
REFIT_FILE * GenerateOptionsFromEtcFstab (
    REFIT_VOLUME *Volume
) {
    EFI_STATUS    Status;
    UINTN         i;
    UINTN         TokenCount;
    CHAR16      **TokenList;
    CHAR16       *Line;
    CHAR16       *Root;
    REFIT_FILE   *Fstab;
    REFIT_FILE   *Options;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    if (!FileExists (Volume->RootDir, L"\\etc\\fstab")) {
        BREAD_CRUMB(L"%a:  1a 1 - END:- return NULL - '\\etc\\fstab' *DOES NOT* Exist", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return NULL;
    }

    BREAD_CRUMB(L"%a:  2", __func__);
    Options = AllocateZeroPool (sizeof (REFIT_FILE));
    if (Options == NULL) {
        BREAD_CRUMB(L"%a:  2a 1 - END:- return NULL - OUT OF MEMORY!!", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        // Early Return
        return NULL;
    }

    BREAD_CRUMB(L"%a:  3", __func__);
    Fstab = AllocateZeroPool (sizeof (REFIT_FILE));
    if (Fstab == NULL) {
        BREAD_CRUMB(L"%a:  3a 1 - END:- return NULL - OUT OF MEMORY!!", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        MY_FREE_FILE(Options);

        // Early Return
        return NULL;
    }

    BREAD_CRUMB(L"%a:  4", __func__);
    Status = RefitReadFile (Volume->RootDir, L"\\etc\\fstab", Fstab, &i);

    BREAD_CRUMB(L"%a:  5", __func__);
    if (EFI_ERROR(Status)) {
        CheckError (Status, L"while reading /etc/fstab");
        BREAD_CRUMB(L"%a:  5a 1 - END:- return NULL - '\\etc\\fstab' is Unreadable", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        MY_FREE_FILE(Options);
        MY_FREE_FILE(Fstab);

        // Early Return
        return NULL;
    }

    BREAD_CRUMB(L"%a:  6", __func__);
    // File read; locate root fs and create entries
    Options->Encoding = ENCODING_UTF16_LE;

    BREAD_CRUMB(L"%a:  7", __func__);
    while ((TokenCount = ReadTokenLine (Fstab, &TokenList)) > 0) {
        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_THREE_STAR_MID,
            L"Read Line Holding %d Token%s from '/etc/fstab'",
            TokenCount,
            (TokenCount == 1) ? L"" : L"s"
        );
        #endif

        LOG_SEP(L"X");
        BREAD_CRUMB(L"%a:  7a 1 - WHILE LOOP:- START", __func__);
        if (TokenCount > 2) {
            BREAD_CRUMB(L"%a:  7a 1a 1", __func__);
            if (StrCmp (TokenList[1], L"\\") == 0) {
                BREAD_CRUMB(L"%a:  7a 1a 1a 1", __func__);
                Root = PoolPrint (L"%s", TokenList[0]);
            }
            else if (StrCmp (TokenList[2], L"\\") == 0) {
                BREAD_CRUMB(L"%a:  7a 1a 1b 1", __func__);
                Root = PoolPrint (L"%s=%s", TokenList[0], TokenList[1]);
            }
            else {
                BREAD_CRUMB(L"%a:  7a 1a 1c 1", __func__);
                Root = NULL;
            }

            BREAD_CRUMB(L"%a:  7a 1a 2", __func__);
            if (Root != NULL &&
                Root[0] != L'\0'
            ) {
                BREAD_CRUMB(L"%a:  7a 1a 2a 1", __func__);
                for (i = 0; i < StrLen (Root); i++) {
                    LOG_SEP(L"X");
                    BREAD_CRUMB(L"%a:  7a 1a 2a 1a 1 - FOR LOOP:- START", __func__);
                    if (Root[i] == '\\') {
                        BREAD_CRUMB(L"%a:  7a 1a 2a 1a 1a 1 - Flip Slash", __func__);
                        Root[i] = '/';
                    }
                    BREAD_CRUMB(L"%a:  7a 1a 2a 1a 2 - FOR LOOP:- END", __func__);
                    LOG_SEP(L"X");
                }

                BREAD_CRUMB(L"%a:  7a 1a 2a 2", __func__);
                Line = PoolPrint (
                    L"\"Boot with Normal Options\"    \"ro root=%s\"\n",
                    Root
                );

                BREAD_CRUMB(L"%a:  7a 1a 2a 3", __func__);
                MergeStrings ((CHAR16 **) &(Options->Buffer), Line, 0);

                BREAD_CRUMB(L"%a:  7a 1a 2a 4", __func__);
                MY_FREE_POOL(Line);

                BREAD_CRUMB(L"%a:  7a 1a 2a 5", __func__);
                Line = PoolPrint (
                    L"\"Boot into Single User Mode\"  \"ro root=%s single\"\n",
                    Root
                );

                BREAD_CRUMB(L"%a:  7a 1a 2a 6", __func__);
                MergeStrings ((CHAR16**) &(Options->Buffer), Line, 0);

                BREAD_CRUMB(L"%a:  7a 1a 2a 7", __func__);
                MY_FREE_POOL(Line);

                BREAD_CRUMB(L"%a:  7a 1a 2a 8", __func__);
                Options->BufferSize = sizeof (CHAR16) * (
                    StrLen ((CHAR16 *) Options->Buffer) + 1
                );
            } // if

            BREAD_CRUMB(L"%a:  7a 1a 3", __func__);
            MY_FREE_POOL(Root);
        } // if

        BREAD_CRUMB(L"%a:  7a 2", __func__);
        FreeTokenLine (&TokenList, &TokenCount);

        BREAD_CRUMB(L"%a:  7a 3 - WHILE LOOP:- END", __func__);
        LOG_SEP(L"X");
    } // while

    BREAD_CRUMB(L"%a:  8", __func__);
    if (Options->Buffer == NULL) {
        BREAD_CRUMB(L"%a:  8a 1", __func__);
        MY_FREE_POOL(Options);
    }
    else {
        BREAD_CRUMB(L"%a:  8b 1", __func__);
        Options->Current8Ptr  = (CHAR8  *) Options->Buffer;
        Options->Current16Ptr = (CHAR16 *) Options->Buffer;
        Options->End8Ptr      = Options->Current8Ptr + Options->BufferSize;
        Options->End16Ptr     = Options->Current16Ptr + (Options->BufferSize >> 1);
        BREAD_CRUMB(L"%a:  8b 2", __func__);
    }

    BREAD_CRUMB(L"%a:  9", __func__);
    MY_FREE_FILE(Fstab);

    BREAD_CRUMB(L"%a:  10 - END:- return REFIT_FILE *Options", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return Options;
} // static REFIT_FILE * GenerateOptionsFromEtcFstab()

// Create options from partition type codes. Specifically, if the earlier
// partition scan found a partition with a type code corresponding to a root
// filesystem according to the Freedesktop.org Discoverable Partitions Spec
// (http://www.freedesktop.org/wiki/Specifications/DiscoverablePartitionsSpec/),
// this function returns an appropriate file with two lines, one with
// "ro root=/dev/disk/by-partuuid/{GUID}" and the other with that plus "single".
// Note that this function returns the LAST partition found with the
// appropriate type code, so this will work poorly on dual-boot systems or
// if the type code is set incorrectly.
static
REFIT_FILE * GenerateOptionsFromPartTypes (VOID) {
    REFIT_FILE   *Options;
    CHAR16       *Line, *GuidString, *WriteStatus;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    if (GlobalConfig.DiscoveredRoot == NULL) {
        BREAD_CRUMB(L"%a:  1a 1 - END:- !GlobalConfig.DiscoveredRoot return NULL", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return NULL;
    }
    WriteStatus = GlobalConfig.DiscoveredRoot->IsMarkedReadOnly ? L"ro" : L"rw";

    BREAD_CRUMB(L"%a:  2", __func__);
    Options = AllocateZeroPool (sizeof (REFIT_FILE));
    if (Options == NULL) {
        BREAD_CRUMB(L"%a:  2a 1 - END:- !Options return NULL", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return NULL;
    }

    BREAD_CRUMB(L"%a:  3", __func__);
    GuidString = GuidAsString (&(GlobalConfig.DiscoveredRoot->PartGuid));
    if (GuidString != NULL) {
        BREAD_CRUMB(L"%a:  3a 1", __func__);
        ToLower (GuidString);

        BREAD_CRUMB(L"%a:  3a 2", __func__);
        Line = PoolPrint (
            L"\"Boot with Normal Options\"    \"%s root=/dev/disk/by-partuuid/%s\"\n",
            WriteStatus, GuidString
        );
        MergeStrings ((CHAR16 **) &(Options->Buffer), Line, 0);
        MY_FREE_POOL(Line);

        BREAD_CRUMB(L"%a:  3a 3", __func__);
        Line = PoolPrint (
            L"\"Boot into Single User Mode\"  \"%s root=/dev/disk/by-partuuid/%s single\"\n",
            WriteStatus, GuidString
        );
        MergeStrings ((CHAR16 **) &(Options->Buffer), Line, 0);
        MY_FREE_POOL(Line);
        MY_FREE_POOL(GuidString);
    } // if (GuidString)

    BREAD_CRUMB(L"%a:  4", __func__);
    Options->Encoding     = ENCODING_UTF16_LE;
    Options->Current8Ptr  = (CHAR8  *) Options->Buffer;
    Options->Current16Ptr = (CHAR16 *) Options->Buffer;
    Options->BufferSize   = sizeof (CHAR16) * (StrLen ((CHAR16 *) Options->Buffer) + 1);
    Options->End16Ptr     = Options->Current16Ptr + (Options->BufferSize >> 1);
    Options->End8Ptr      = Options->Current8Ptr  +  Options->BufferSize;

    BREAD_CRUMB(L"%a:  5 - END:- return REFIT_FILE *Options", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return Options;
} // static REFIT_FILE * GenerateOptionsFromPartTypes()

static
#if REFIT_DEBUG < 1
VOID ExitOuter (VOID) {
#else
VOID ExitOuter (
    BOOLEAN    ValidInclude,
    BOOLEAN    NotRunBefore
) {
    EFI_STATUS Status;
#endif


    // Set a few defaults if required
    if (GlobalConfig.DontScanVolumes == NULL) {
        GlobalConfig.DontScanVolumes = StrDuplicate (
            DONT_SCAN_VOLUMES
        );
    }
    if (GlobalConfig.WindowsRecoveryFiles == NULL) {
        GlobalConfig.WindowsRecoveryFiles = StrDuplicate (
            WINDOWS_RECOVERY_FILES
        );
    }
    if (GlobalConfig.MacOSRecoveryFiles == NULL) {
        GlobalConfig.MacOSRecoveryFiles = StrDuplicate (
            MACOS_RECOVERY_FILES
        );
    }
    if (GlobalConfig.DefaultSelection == NULL) {
        GlobalConfig.DefaultSelection = StrDuplicate (L"+");
    }

    SyncShowTools();
    SyncAlsoScan();
    SyncDontScanDirs();
    SyncDontScanFiles();
    SyncLinuxPrefixes();

    // Forced Default Settings
    if (GlobalConfig.EnableTouch) {
        GlobalConfig.EnableMouse = FALSE;

        if (!AppleFirmware) {
            // DA-TAG: Force 'RescanDXE' on UEFI-PC
            //         Update other instances if changing
            GlobalConfig.RescanDXE = TRUE;
        }
    }
    if (AppleFirmware) {
        GlobalConfig.RescanDXE    = FALSE;
        GlobalConfig.RansomDrives = FALSE;
    }
    else {
        GlobalConfig.SetAppleFB     = FALSE;
        GlobalConfig.NvramProtect   = FALSE;
        GlobalConfig.NvramProtectEx = FALSE;

        if (GlobalConfig.EnableMouse) {
            // DA-TAG: Force 'RescanDXE' on UEFI-PC
            //         Update other instances if changing
            GlobalConfig.RescanDXE = TRUE;
        }
    }
    if (GlobalConfig.SyncTrust != ENFORCE_TRUST_NONE) {
        GlobalConfig.DirectBoot = FALSE;
        GlobalConfig.Timeout    =     0;
    }

    #if REFIT_DEBUG > 0
    if (NotRunBefore) MuteLogger = FALSE;
    #endif

    if (!GlobalConfig.TextOnly                   &&
        !FileExists (SelfDir, L"icons")          &&
        !FileExists (SelfDir, GlobalConfig.IconsDir)
    ) {
        #if REFIT_DEBUG > 0
        LOG_MSG(
            "%s  - WARN: Could *NOT* Find Icons Folder ... Use Text-Only Mode",
            OffsetNext
        );
        #endif

        GlobalConfig.TextOnly = ForceTextOnly = TRUE;
    }

    #if REFIT_DEBUG > 0
    if (!FoundFontImage) {
        FoundFontImage = TRUE;

        LOG_MSG(
            "%s  - WARN: Defined Font File *IS NOT* Valid ... Use Default Font",
            OffsetNext
        );
    }

    LOG_MSG("\n");
    Status = (ValidInclude) ? EFI_SUCCESS : EFI_WARN_STALE_DATA;
    LOG_MSG("Process Configuration Options ... %r", Status);
    LOG_MSG("\n\n");
    #endif
} // static VOID ExitOuter()

EFI_STATUS RefitReadFile (
    IN     EFI_FILE_HANDLE  BaseDir,
    IN     CHAR16          *FileName,
    IN OUT REFIT_FILE      *File,
    OUT    UINTN           *size
) {
    EFI_STATUS       Status;
    EFI_FILE_HANDLE  FileHandle;
    EFI_FILE_INFO   *FileInfo;
    CHAR16          *Message;
    UINT64           ReadSize;

    File->Buffer     = NULL;
    File->BufferSize =    0;
    *size            =    0;

    // Read the file and allocating a buffer
    Status = REFIT_CALL_5_WRAPPER(
        BaseDir->Open, BaseDir,
        &FileHandle, FileName,
        EFI_FILE_MODE_READ, 0
    );
    if (EFI_ERROR(Status)) {
        Message = PoolPrint (L"While Loading File:- '%s'", FileName);
        CheckError (Status, Message);
        MY_FREE_POOL(Message);

        // Early Return
        return Status;
    }

    FileInfo = LibFileInfo (FileHandle);
    if (FileInfo == NULL) {
        // DA-TAG: Invesigate This
        //         Print and register the error
        REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);

        // Early Return
        return EFI_LOAD_ERROR;
    }
    ReadSize = FileInfo->FileSize;
    MY_FREE_POOL(FileInfo);

    File->BufferSize = (UINTN) ReadSize;

    File->Buffer = AllocatePool (File->BufferSize);
    if (File->Buffer == NULL) {
       // DA-TAG: Invesigate This
       //         Print and register the error
       REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);

       // Early Return
       return EFI_OUT_OF_RESOURCES;
    }

    Status = REFIT_CALL_3_WRAPPER(
        FileHandle->Read, FileHandle,
        &File->BufferSize, File->Buffer
    );
    if (EFI_ERROR(Status)) {
        Message = PoolPrint (L"While Loading File:- '%s'", FileName);
        CheckError (Status, Message);
        MY_FREE_POOL(Message);

        // DA-TAG: Invesigate This
        //         Print and register the error
        REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);

        MY_FREE_POOL(File->Buffer);

        // Early Return
        return Status;
    }

    *size = File->BufferSize;

    REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);

    // Setup for reading
    File->Current8Ptr  = (CHAR8  *) File->Buffer;
    File->Current16Ptr = (CHAR16 *) File->Buffer;
    File->End8Ptr      = File->Current8Ptr  + File->BufferSize;
    File->End16Ptr     = File->Current16Ptr + (File->BufferSize >> 1);

    // DA_TAG: Investigate This
    //        Detect other encodings
    //        Some are also implemented
    //
    // Detect Encoding
    File->Encoding = ENCODING_ISO8859_1; // Default: Translate CHAR8 to CHAR16 1:1
    if (File->BufferSize >= 4) {
        if (File->Buffer[0] == 0xFF &&
            File->Buffer[1] == 0xFE
        ) {
            // BOM in UTF-16 little endian (or UTF-32 little endian)
            File->Encoding = ENCODING_UTF16_LE; // Use CHAR16 as is
            File->Current16Ptr++;
        }
        else if (
            File->Buffer[0] == 0xEF &&
            File->Buffer[1] == 0xBB &&
            File->Buffer[2] == 0xBF
        ) {
            // BOM in UTF-8
            File->Encoding = ENCODING_UTF8; // Translate from UTF-8 to UTF-16
            File->Current8Ptr += 3;
        }
        else if (
            File->Buffer[1] == 0 &&
            File->Buffer[3] == 0
        ) {
            File->Encoding = ENCODING_UTF16_LE; // Use CHAR16 as is
        }
    }

    return EFI_SUCCESS;
} // EFI_STATUS RefitReadFile()

//
// Get a line of tokens from a file
//
UINTN ReadTokenLine (
    IN  REFIT_FILE   *File,
    OUT CHAR16     ***TokenList
) {
    BOOLEAN  LineFinished;
    BOOLEAN  IsQuoted;
    CHAR16  *Line, *Token, *p;
    UINTN    TokenCount;

    *TokenList = NULL;

    IsQuoted = FALSE;
    TokenCount = 0;
    while (TokenCount == 0) {
        Line = ReadLine (File);
        if (Line == NULL) {
            return 0;
        }

        p = Line;
        LineFinished = FALSE;
        while (!LineFinished) {
            // Skip whitespace and find start of token
            while (!IsQuoted &&
                (
                    *p == ' '  ||
                    *p == '\t' ||
                    *p == '='  ||
                    *p == ','
                )
            ) {
                p++;
            } // while

            if (*p == 0 || *p == '#') {
                break;
            }

            if (*p == '"') {
               IsQuoted = !IsQuoted;
               p++;
            }

            Token = p;

            // Find end of token
            while (KeepReading (p, &IsQuoted)) {
               if ((*p == L'/') && !IsQuoted) {
                   // Switch 'Unix style' to 'DOS style' directory separators
                   *p = L'\\';
               }
               p++;
            } // while

            if (*p == L'\0' || *p == L'#') {
                LineFinished = TRUE;
            }
            *p++ = 0;

            AddListElement (
                (VOID ***) TokenList,
                &TokenCount,
                (VOID *) StrDuplicate (Token)
            );
        } // while !LineFinished

        MY_FREE_POOL(Line);
    } // while TokenCount == 0

    return TokenCount;
} // UINTN ReadTokenLine()

VOID FreeTokenLine (
    IN OUT CHAR16 ***TokenList,
    IN OUT UINTN    *TokenCount
) {
    // DA-TAG: Investigate this
    //         Also free the items
    FreeList ((VOID ***) TokenList, TokenCount);
} // VOID FreeTokenLine()

// Read the user-configured menu entries from config.conf
// and add/delete entries based on file contents.
VOID ScanUserConfigured (
    CHAR16 *FileName
) {
    EFI_STATUS         Status;
    REFIT_FILE         File;
    CHAR16           **TokenList;
    UINTN              size;
    UINTN              TokenCount;
    LOADER_ENTRY      *Entry;

    #if REFIT_DEBUG > 0
    CHAR16             *TmpName;
    CHAR16             *CountStr;
    UINTN               LogLineType;
    #endif


    if (!ManualInclude) {
        LOG_SEP(L"X");
        LOG_INCREMENT();
        BREAD_CRUMB(L"%a:  A - START", __func__);

        TotalEntryCount = ValidEntryCount = 0;
    }

    if (FileExists (SelfDir, FileName)) {
        Status = RefitReadFile (SelfDir, FileName, &File, &size);
        if (!EFI_ERROR(Status)) {
            while ((TokenCount = ReadTokenLine (&File, &TokenList)) > 0) {
                if (TokenCount > 1 &&
                    MyStriCmp (TokenList[0], L"menuentry")
                ) {
                    TotalEntryCount = TotalEntryCount + 1;

                    Entry = AddStanzaEntries (
                        &File, SelfVolume, TokenList[1]
                    );
                    if (Entry == NULL) {
                        FreeTokenLine (&TokenList, &TokenCount);
                        continue;
                    }

                    ValidEntryCount = ValidEntryCount + 1;
                    #if REFIT_DEBUG > 0
                    TmpName = (SelfVolume->VolName != NULL)
                        ? SelfVolume->VolName : Entry->LoaderPath;
                    LOG_MSG(
                        "%s  - Found %s%s%s%s%s",
                        OffsetNext,
                        Entry->Title,
                        SetVolJoin (Entry->Title, FALSE     ),
                        SetVolKind (Entry->Title, TmpName, 0),
                        SetVolFlag (Entry->Title, TmpName   ),
                        SetVolType (Entry->Title, TmpName, 0)
                    );
                    #endif

                    if (Entry->me.SubScreen == NULL) {
                        GenerateSubScreen (Entry, SelfVolume, TRUE);
                    }

                    AddPreparedLoaderEntry (Entry);
                }
                else if (
                    !ManualInclude &&
                    TokenCount == 2 &&
                    MyStriCmp (TokenList[0], L"include") &&
                    MyStriCmp (FileName, GlobalConfig.ConfigFilename)
                ) {
                    if (!MyStriCmp (TokenList[1], FileName)) {
                        // Scan manual stanza include file
                        #if REFIT_DEBUG > 0
                        #if REFIT_DEBUG < 2
                        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
                        ALT_LOG(1, LOG_THREE_STAR_MID,
                            L"Process Include File for Manual Stanzas"
                        );
                        #else
                        LOG_SEP(L"X");
                        BREAD_CRUMB(L"%a:  A1 - INCLUDE FILE (%s): START", __func__, TokenList[1]);
                        #endif
                        #endif

                        ManualInclude =  TRUE;
                        ScanUserConfigured (TokenList[1]);
                        ManualInclude = FALSE;

                        #if REFIT_DEBUG > 0
                        #if REFIT_DEBUG < 2
                        ALT_LOG(1, LOG_THREE_STAR_MID,
                            L"Scanned Include File for Manual Stanzas"
                        );
                        #else
                        BREAD_CRUMB(L"%a:  A2 - INCLUDE FILE (%s): END", __func__, TokenList[1]);
                        LOG_SEP(L"X");
                        #endif
                        #endif
                    }
                }

                FreeTokenLine (&TokenList, &TokenCount);
            } // while
        }

        MY_FREE_POOL(File.Buffer);
    } // if FileExists

    #if REFIT_DEBUG > 0
    CountStr = (ValidEntryCount > 0)
        ? PoolPrint (L"%d", ValidEntryCount) : NULL;

    if (ManualInclude) {
        LogLineType = LOG_THREE_STAR_MID;
    }
    else {
        LogLineType = LOG_STAR_HEAD_SEP;
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    }

    ALT_LOG(1, LogLineType,
        L"Processed %d Manual Stanza%s in '%s'%s%s%s%s",
        TotalEntryCount,
        (TotalEntryCount == 1) ? L""      : L"s",
        FileName,
        (TotalEntryCount == 0) ? L""      : L" ... Found ",
        (ValidEntryCount  > 0) ? CountStr : (TotalEntryCount == 0) ? L"" : L"0",
        (TotalEntryCount == 0) ? L""      : L" Valid/Active Stanza",
        (ValidEntryCount == 1) ? L""      : (TotalEntryCount == 0) ? L"" : L"s"
    );
    MY_FREE_POOL(CountStr);
    #endif

    if (ManualInclude) {
        ManualInclude = FALSE;
    }
    else {
        BREAD_CRUMB(L"%a:  Z - END:- VOID", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
    }
} // VOID ScanUserConfigured()

// Read a Linux kernel options file for a Linux boot loader into memory.
// The LoaderPath and Volume variables identify the location of the options file,
// but not its name -- you pass this function the filename of the Linux kernel,
// initial RAM disk, or other file in the target directory, and this function
// finds the file with a name in the comma-delimited list of names specified by
// LINUX_OPTIONS_FILENAMES within that directory and loads it. If a RefindPlus
// options file can't be found, try to generate minimal options from /etc/fstab
// on the same volume as the kernel. This typically works only if the kernel is
// being read from the Linux root filesystem.
//
// The return value is a pointer to the REFIT_FILE handle for the file,
// or NULL if it was not found.
REFIT_FILE * ReadLinuxOptionsFile (
    IN CHAR16       *LoaderPath,
    IN REFIT_VOLUME *Volume
) {
    EFI_STATUS   Status;
    CHAR16      *OptionsFilename;
    CHAR16      *FullFilename;
    CHAR16      *BaseFilename;
    UINTN        size;
    UINTN        i;
    BOOLEAN      GoOn;
    BOOLEAN      FileFound;
    REFIT_FILE  *File;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    BREAD_CRUMB(L"%a:  2", __func__);
    File         =  NULL;
    GoOn         =  TRUE;
    FileFound    = FALSE;
    BaseFilename =  NULL;
    FullFilename = FindPath (LoaderPath);

    i = 0;
    while (
        GoOn                 &&
        FullFilename != NULL &&
        (OptionsFilename = FindCommaDelimited (LINUX_OPTIONS_FILENAMES, i++)) != NULL
    ) {
        LOG_SEP(L"X");
        BaseFilename = StrDuplicate (FullFilename);

        BREAD_CRUMB(L"%a:  2a 1 - WHILE LOOP:- START", __func__);
        MergeStrings (&BaseFilename, OptionsFilename, '\\');

        BREAD_CRUMB(L"%a:  2a 2", __func__);
        if (!FileExists (Volume->RootDir, BaseFilename)) {
            BREAD_CRUMB(L"%a:  2a 2a 1 - OptionsFile *NOT* Found", __func__);
        }
        else {
            BREAD_CRUMB(L"%a:  2a 2b 1 - Seek OptionsFile ... Success", __func__);
            MY_FREE_FILE(File);
            File = AllocateZeroPool (sizeof (REFIT_FILE));
            if (File == NULL) {
                MY_FREE_POOL(OptionsFilename);
                MY_FREE_POOL(FullFilename);
                MY_FREE_POOL(BaseFilename);

                BREAD_CRUMB(L"%a:  2a 2b 1a 1 - WHILE LOOP:- END - OUT OF MEMORY return NULL", __func__);
                LOG_DECREMENT();
                LOG_SEP(L"X");

                return NULL;
            }
            BREAD_CRUMB(L"%a:  2a 2b 2", __func__);

            Status = RefitReadFile (
                Volume->RootDir,
                BaseFilename, File, &size
            );
            BREAD_CRUMB(L"%a:  2a 2b 3", __func__);
            if (EFI_ERROR(Status)) {
                BREAD_CRUMB(L"%a:  2a 2b 3a 1", __func__);
                CheckError (Status, L"While Loading the Linux Options File");
            }
            else {
                BREAD_CRUMB(L"%a:  2a 2b 3b 1", __func__);
                GoOn      = FALSE;
                FileFound =  TRUE;
            }
            BREAD_CRUMB(L"%a:  2a 2b 4", __func__);
        }

        BREAD_CRUMB(L"%a:  2a 3", __func__);
        MY_FREE_POOL(OptionsFilename);
        MY_FREE_POOL(BaseFilename);

        BREAD_CRUMB(L"%a:  2a 4 - WHILE LOOP:- END", __func__);
        LOG_SEP(L"X");
    } // while;
    MY_FREE_POOL(FullFilename);

    BREAD_CRUMB(L"%a:  3", __func__);
    if (!FileFound) {
        BREAD_CRUMB(L"%a:  3a 1", __func__);
        // No refindplus_linux.conf or refind_linux.conf file,
        // try to pull values from /etc/fstab
        MY_FREE_FILE(File);
        File = GenerateOptionsFromEtcFstab (Volume);

        BREAD_CRUMB(L"%a:  3a 2", __func__);
        // If still NULL, try Freedesktop.org Discoverable Partitions Spec
        if (File == NULL) {
            BREAD_CRUMB(L"%a:  3a 2a 1", __func__);
            File = GenerateOptionsFromPartTypes();
        }
        BREAD_CRUMB(L"%a:  3a 3", __func__);
    } // if

    BREAD_CRUMB(L"%a:  4 - END:- return REFIT_FILE *File", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");
    return File;
} // REFIT_FILE * ReadLinuxOptionsFile()

// Retrieve a single line of options from a Linux kernel options file
CHAR16 * GetFirstOptionsFromFile (
    IN CHAR16       *LoaderPath,
    IN REFIT_VOLUME *Volume
) {
    UINTN         TokenCount;
    CHAR16       *Options;
    CHAR16      **TokenList;
    REFIT_FILE   *File;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);
    File = ReadLinuxOptionsFile (LoaderPath, Volume);

    BREAD_CRUMB(L"%a:  2", __func__);
    Options = NULL;
    if (File != NULL) {
        BREAD_CRUMB(L"%a:  2a 1", __func__);
        TokenCount = ReadTokenLine(File, &TokenList);

        BREAD_CRUMB(L"%a:  2a 2", __func__);
        if (TokenCount > 1) {
            BREAD_CRUMB(L"%a:  2a 2a 1", __func__);
            Options = StrDuplicate (TokenList[1]);
        }

        BREAD_CRUMB(L"%a:  2a 3", __func__);
        FreeTokenLine (&TokenList, &TokenCount);

        BREAD_CRUMB(L"%a:  2a 4", __func__);
        MY_FREE_FILE(File);
    }

    BREAD_CRUMB(L"%a:  3 - END:- return CHAR16 *Options = '%s'", __func__,
        Options ? Options : L"NULL"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return Options;
} // CHAR16 * GetOptionsFile()

// Read Config File
VOID ReadConfig (
    CHAR16 *FileName
) {
    EFI_STATUS        Status;
    REFIT_FILE        File;
    BOOLEAN           DoneTool;
    BOOLEAN           DoneManual;
    BOOLEAN           CheckManual;
    BOOLEAN           GotHideuiAll;
    BOOLEAN           GotNoneHideui;
    BOOLEAN           OutLoopHideui;
    BOOLEAN           GotSyncTrustAll;
    BOOLEAN           GotNoneSyncTrust;
    BOOLEAN           OutLoopSyncTrust;
    BOOLEAN           GotGraphicsForAll;
    BOOLEAN           GotNoneGraphicsFor;
    BOOLEAN           OutLoopGraphicsFor;
    BOOLEAN           DeclineSetting;
    CHAR16          **TokenList;
    CHAR16           *MsgStr;
    CHAR16           *Flag; // Do Not Free
    UINTN             i, j;
    UINTN             TokenCount;
    UINTN             InvalidEntries;
    INTN              MaxLogLevel;

    static UINTN      ReadLoops    =     0;

    #if REFIT_DEBUG > 0
    INTN             RealLogLevel;
    INTN             HighLogLevel;
    BOOLEAN          UpdatedToken;

    static BOOLEAN   NotRunBefore = TRUE;
    static BOOLEAN   ValidInclude = TRUE;
    static BOOLEAN   FirstInclude = TRUE;
    #endif


    // Control 'Include' Depth
    if (ReadLoops > 1) {
        ReadLoops = ReadLoops - 1;

        #if REFIT_DEBUG > 0
        if (NotRunBefore) MuteLogger = FALSE;
        LOG_MSG(
            "%s  ** Ignore Tertiary Config ... %s",
            OffsetNext, FileName
        );
        ValidInclude = FALSE;
        // DA-TAG: No 'TRUE' Flag
        #endif

        return;
    }
    ReadLoops = ReadLoops + 1;

    #if REFIT_DEBUG > 0
    if (NotRunBefore) MuteLogger = FALSE;
    if (!OuterLoop) {
        UpdatedToken = FALSE;
    }
    else {
        LOG_MSG("R E A D   C O N F I G   T O K E N S");
    }
    if (NotRunBefore) MuteLogger =  TRUE;
    #endif

    if (!FileExists (SelfDir, FileName)) {
        #if REFIT_DEBUG > 0
        ValidInclude = FALSE;
        if (NotRunBefore) MuteLogger = FALSE;
        LOG_MSG("%s", OffsetNext);
        if (!OuterLoop) {
            LOG_MSG("  - ");
            Flag = L"";
        }
        else {
            LOG_MSG("*** ");
            Flag = L"Configuration";
        }
        LOG_MSG("WARN: %sFile *NOT* Found", Flag);
        // DA-TAG: No 'TRUE' Flag
        #endif

        if (!OuterLoop) {
            ReadLoops = ReadLoops - 1;
        }
        else {
            #if REFIT_DEBUG > 0
            LOG_MSG(" ... Use Default Settings ***");
            LOG_MSG("\n\n");
            #endif

            ExitOuter (
                #if REFIT_DEBUG > 0
                ValidInclude, NotRunBefore
                #endif
            );
            ReadLoops = 0;

            #if REFIT_DEBUG > 0
            // Reset Misc Flags
            NotRunBefore =                FALSE;
            FirstInclude = ValidInclude =  TRUE;
            #endif
        }

        return;
    }

    Status = RefitReadFile (SelfDir, FileName, &File, &i);
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        if (NotRunBefore) MuteLogger = FALSE;
        LOG_MSG("%s", OffsetNext);
        if (!OuterLoop) {
            ValidInclude = FALSE;
            LOG_MSG("  - ");
        }
        else {
            LOG_MSG("*** ");
        }
        LOG_MSG(
            "WARN: Invalid Configuration File ... Abort File Load",
            OffsetNext
        );
        // DA-TAG: No 'TRUE' Flag
        #endif

        if (!OuterLoop) {
            ReadLoops = ReadLoops - 1;
        }
        else {
            #if REFIT_DEBUG > 0
            LOG_MSG(" ... Use Default Settings ***");
            LOG_MSG("\n\n");
            #endif

            ExitOuter (
                #if REFIT_DEBUG > 0
                ValidInclude, NotRunBefore
                #endif
            );
            ReadLoops = 0;

            #if REFIT_DEBUG > 0
            // Reset Misc Flags
            NotRunBefore =                FALSE;
            FirstInclude = ValidInclude =  TRUE;
            #endif
        }

        return;
    }

    CheckManual        = DoneManual         = FALSE;
    GotNoneHideui      = OutLoopHideui      = FALSE;
    GotNoneSyncTrust   = OutLoopSyncTrust   = FALSE;
    GotNoneGraphicsFor = OutLoopGraphicsFor = FALSE;
    #if REFIT_DEBUG > 0
    if (!OuterLoop) {
        CheckManual = TRUE;
    }
    #endif

    MaxLogLevel = (ForensicLogging) ? LOGLEVELMAX + 1 : LOGLEVELMAX;
    while ((TokenCount = ReadTokenLine (&File, &TokenList)) > 0) {
        if (MyStriCmp (TokenList[0], L"timeout")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA-TAG: Signed integer as can have negative value
            HandleSignedInt (
                TokenList, TokenCount,
                &(GlobalConfig.Timeout)
            );

            GlobalConfig.DirectBoot = (GlobalConfig.Timeout < 0)
                ? TRUE : FALSE;
        }
        else if (
            !GotNoneHideui &&
            MyStriCmp (TokenList[0], L"hideui")
        ) {
            if (!OuterLoop && !OutLoopHideui) {
                #if REFIT_DEBUG > 0
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
                #endif

                // DA-TAG: Allows reset/override in 'included' config files
                OutLoopHideui            = TRUE;
                GlobalConfig.HideUIFlags = HIDEUI_FLAG_NONE;
            }

            GotHideuiAll = FALSE;
            for (i = 1; i < TokenCount; i++) {
                Flag = TokenList[i];
                if (MyStriCmp (Flag, L"none")) {
                    // DA-TAG: Required despite earlier reset
                    //         This will always be used if in token list
                    GotNoneHideui            = TRUE;
                    GlobalConfig.HideUIFlags = HIDEUI_FLAG_NONE;
                    break;
                }
                else if (!GotHideuiAll) {
                    // DA-TAG: Arranged as so to prioritise 'none' above
                    if (MyStriCmp (Flag, L"all")) {
                        GotHideuiAll             = TRUE;
                        GlobalConfig.HideUIFlags = HIDEUI_FLAG_ALL;
                    }
                    else {
                        if (0);
                        else if (MyStriCmp (Flag, L"label"     )) GlobalConfig.HideUIFlags |= HIDEUI_FLAG_LABEL;
                        else if (MyStriCmp (Flag, L"hints"     )) GlobalConfig.HideUIFlags |= HIDEUI_FLAG_HINTS;
                        else if (MyStriCmp (Flag, L"banner"    )) GlobalConfig.HideUIFlags |= HIDEUI_FLAG_BANNER;
                        else if (MyStriCmp (Flag, L"hwtest"    )) GlobalConfig.HideUIFlags |= HIDEUI_FLAG_HWTEST;
                        else if (MyStriCmp (Flag, L"arrows"    )) GlobalConfig.HideUIFlags |= HIDEUI_FLAG_ARROWS;
                        else if (MyStriCmp (Flag, L"editor"    )) GlobalConfig.HideUIFlags |= HIDEUI_FLAG_EDITOR;
                        else if (MyStriCmp (Flag, L"badges"    )) GlobalConfig.HideUIFlags |= HIDEUI_FLAG_BADGES;
                        else if (MyStriCmp (Flag, L"safemode"  )) GlobalConfig.HideUIFlags |= HIDEUI_FLAG_SAFEMODE;
                        else if (MyStriCmp (Flag, L"singleuser")) GlobalConfig.HideUIFlags |= HIDEUI_FLAG_SINGLEUSER;
                        else {
                            MsgStr = PoolPrint (
                                L"WARN: Invalid 'hideui' Token:- '%s'", Flag
                            );

                            #if REFIT_DEBUG > 0
                            if (NotRunBefore) MuteLogger = FALSE;
                            LOG_MSG("%s  - %s", OffsetNext, MsgStr);
                            if (NotRunBefore) MuteLogger = TRUE;
                            #endif

                            SwitchToText (FALSE);
                            PrintUglyText (MsgStr, NEXTLINE);
                            PauseForKey();
                            MY_FREE_POOL(MsgStr);
                        }
                    }
                }
            } // for
        }
        else if (
            !GotNoneGraphicsFor &&
            MyStriCmp (TokenList[0], L"use_graphics_for")
        ) {
            if (!OuterLoop && !OutLoopGraphicsFor) {
                #if REFIT_DEBUG > 0
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
                #endif

                // DA-TAG: Allows reset/override in 'included' config files
                OutLoopGraphicsFor       = TRUE;
                GlobalConfig.GraphicsFor = GRAPHICS_FOR_NONE;
            }

            if (TokenCount == 2 ||
                (
                    TokenCount > 2 &&
                    !MyStriCmp (TokenList[1], L"+")
                )
            ) {
                GlobalConfig.GraphicsFor = GRAPHICS_FOR_NONE;
            }

            if (TokenCount > 1) {
                GotGraphicsForAll = FALSE;
            }
            else {
                GotGraphicsForAll        = TRUE;
                GlobalConfig.GraphicsFor = GRAPHICS_FOR_EVERYTHING;
            }

            for (i = 1; i < TokenCount; i++) {
                Flag = TokenList[i];
                if (MyStriCmp (Flag, L"none")) {
                    // DA-TAG: Required despite earlier reset
                    //         This will always be used if in token list
                    GotNoneGraphicsFor        = TRUE;
                    GlobalConfig.GraphicsFor = GRAPHICS_FOR_NONE;
                    break;
                }
                else if (!GotGraphicsForAll) {
                    // DA-TAG: Arranged as so to prioritise 'none' above
                    if (MyStriCmp (Flag, L"everything")) {
                        GotGraphicsForAll        = TRUE;
                        GlobalConfig.GraphicsFor = GRAPHICS_FOR_EVERYTHING;
                    }
                    else {
                        if (0);
                        else if (MyStriCmp (TokenList[i], L"osx"     )) GlobalConfig.GraphicsFor |= GRAPHICS_FOR_OSX;
                        else if (MyStriCmp (TokenList[i], L"grub"    )) GlobalConfig.GraphicsFor |= GRAPHICS_FOR_GRUB;
                        else if (MyStriCmp (TokenList[i], L"linux"   )) GlobalConfig.GraphicsFor |= GRAPHICS_FOR_LINUX;
                        else if (MyStriCmp (TokenList[i], L"elilo"   )) GlobalConfig.GraphicsFor |= GRAPHICS_FOR_ELILO;
                        else if (MyStriCmp (TokenList[i], L"clover"  )) GlobalConfig.GraphicsFor |= GRAPHICS_FOR_CLOVER;
                        else if (MyStriCmp (TokenList[i], L"windows" )) GlobalConfig.GraphicsFor |= GRAPHICS_FOR_WINDOWS;
                        else if (MyStriCmp (TokenList[i], L"opencore")) GlobalConfig.GraphicsFor |= GRAPHICS_FOR_OPENCORE;
                    }
                }
            } // for
        }
        else if (
            !GotNoneSyncTrust &&
            MyStriCmp (TokenList[0], L"sync_trust")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop && !OutLoopSyncTrust) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GotSyncTrustAll = FALSE;
            if (!OuterLoop && !OutLoopSyncTrust) {
                // DA-TAG: Allows reset/override in 'included' config files
                OutLoopSyncTrust       = TRUE;
                GlobalConfig.SyncTrust = ENFORCE_TRUST_NONE;
            }

            for (i = 1; i < TokenCount; i++) {
                Flag = TokenList[i];
                if (MyStriCmp (Flag, L"none")) {
                    // DA-TAG: Required despite earlier reset
                    //         This will always be used if in token list
                    GotNoneSyncTrust       = TRUE;
                    GlobalConfig.SyncTrust = ENFORCE_TRUST_NONE;
                    break;
                }
                else if (!GotSyncTrustAll) {
                    // DA-TAG: Arranged as so to prioritise 'none' above
                    if (MyStriCmp (Flag, L"every")) {
                        GotSyncTrustAll        = TRUE;
                        GlobalConfig.SyncTrust = ENFORCE_TRUST_EVERY;
                    }
                    else {
                        if (0);
                        else if (MyStriCmp (Flag, L"macos"   )) GlobalConfig.SyncTrust |= ENFORCE_TRUST_MACOS;
                        else if (MyStriCmp (Flag, L"linux"   )) GlobalConfig.SyncTrust |= ENFORCE_TRUST_LINUX;
                        else if (MyStriCmp (Flag, L"windows" )) GlobalConfig.SyncTrust |= ENFORCE_TRUST_WINDOWS;
                        else if (MyStriCmp (Flag, L"opencore")) GlobalConfig.SyncTrust |= ENFORCE_TRUST_OPENCORE;
                        else if (MyStriCmp (Flag, L"clover"  )) GlobalConfig.SyncTrust |= ENFORCE_TRUST_CLOVER;
                        else if (MyStriCmp (Flag, L"similar" )) GlobalConfig.SyncTrust |= ENFORCE_TRUST_OTHERS;
                        else if (MyStriCmp (Flag, L"verify"  )) GlobalConfig.SyncTrust |= REQUIRE_TRUST_VERIFY;
                        else {
                            MsgStr = PoolPrint (
                                L"WARN: Invalid 'sync_trust' Token:- '%s'", Flag
                            );

                            #if REFIT_DEBUG > 0
                            if (NotRunBefore) MuteLogger = FALSE;
                            LOG_MSG("%s  - %s", OffsetNext, MsgStr);
                            if (NotRunBefore) MuteLogger = TRUE;
                            #endif

                            SwitchToText (FALSE);
                            PrintUglyText (MsgStr, NEXTLINE);
                            PauseForKey();
                            MY_FREE_POOL(MsgStr);
                        }
                    }
                }
            } // for
        }
        else if (MyStriCmp (TokenList[0], L"icons_dir")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleString (
                TokenList, TokenCount,
                &(GlobalConfig.IconsDir)
            );
        }
        else if (MyStriCmp (TokenList[0], L"scanfor")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            for (i = 0; i < NUM_SCAN_OPTIONS; i++) {
                GlobalConfig.ScanFor[i] = (i < TokenCount)
                    ? TokenList[i][0] : ' ';
            } // for
        }
        else if (
            TokenCount == 2 &&
            MyStriCmp (TokenList[0], L"log_level")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA-TAG: Signed integer as *MAY* have negative value input
            HandleSignedInt (
                TokenList, TokenCount,
                &LogLevelConfig
            );

            GlobalConfig.LogLevel = LogLevelConfig;

            // Sanitise levels
            if (0);
            else if (GlobalConfig.LogLevel < LOGLEVELOFF) GlobalConfig.LogLevel = LOGLEVELOFF;
            else if (GlobalConfig.LogLevel > MaxLogLevel) GlobalConfig.LogLevel = MaxLogLevel;
        }
        else if (MyStriCmp (TokenList[0], L"also_scan_dirs")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleStrings (
                TokenList, TokenCount,
                &(GlobalConfig.AlsoScan)
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"dont_scan_volumes" ) ||
            MyStriCmp (TokenList[0], L"don't_scan_volumes")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // Note: Do not use HandleStrings() because it modifies slashes.
            //       However, This might be present in the volume name.
            MY_FREE_POOL(GlobalConfig.DontScanVolumes);
            for (i = 1; i < TokenCount; i++) {
                MergeStrings (
                    &GlobalConfig.DontScanVolumes,
                    TokenList[i], L','
                );
            }
        }
        else if (
            MyStriCmp (TokenList[0], L"dont_scan_files" ) ||
            MyStriCmp (TokenList[0], L"don't_scan_files")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA-TAG: Synced With Defaults Later
            HandleStrings (
                TokenList, TokenCount,
                &(GlobalConfig.DontScanFiles)
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"dont_scan_dirs" ) ||
            MyStriCmp (TokenList[0], L"don't_scan_dirs")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA-TAG: Synced With Defaults Later
            HandleStrings (
                TokenList, TokenCount,
                &(GlobalConfig.DontScanDirs)
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"dont_scan_tools" ) ||
            MyStriCmp (TokenList[0], L"don't_scan_tools")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleStrings (
                TokenList, TokenCount,
                &(GlobalConfig.DontScanTools)
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"dont_scan_firmware" ) ||
            MyStriCmp (TokenList[0], L"don't_scan_firmware")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleStrings (
                TokenList, TokenCount,
                &(GlobalConfig.DontScanFirmware)
            );
        }
        else if (MyStriCmp (TokenList[0], L"use_nvram")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.UseNvram = HandleBoolean (TokenList, TokenCount);
        }
        else if (MyStriCmp (TokenList[0], L"disable_rescan_dxe")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            DeclineSetting = HandleBoolean (TokenList, TokenCount);
            GlobalConfig.RescanDXE = (DeclineSetting) ? FALSE : TRUE;
        }
        else if (
            TokenCount == 2 &&
            MyStriCmp (TokenList[0], L"sync_nvram")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleUnsignedInt (
                TokenList, TokenCount,
                &(GlobalConfig.SyncNVram)
            );
        }
        else if (MyStriCmp (TokenList[0], L"scan_driver_dirs")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleStrings (
                TokenList, TokenCount,
                &(GlobalConfig.DriverDirs)
            );
        }
        else if (MyStriCmp (TokenList[0], L"showtools")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            if (SetShowTools) {
                // Clear Showtools List
                for (j = 0; j < NUM_TOOLS; j++) {
                    GlobalConfig.ShowTools[j] = TAG_BASE;
                } // for
            }

            // DA-TAG: Resetting HiddenTags here looks strange but is valid
            //         Artificial default of 'TRUE' was a misconfig exit option
            //         This sets the real default of 'FALSE' if 'showtools' is set
            if (GlobalConfig.HiddenTags) {
                GlobalConfig.HiddenTags = FALSE;
            }

            DoneTool = FALSE;
            InvalidEntries = 0;
            i = j = 0;
            for (;;) {
                // DA-TAG: Start Index is 1 Here ('i' for NUM_TOOLS/TokenList)
                ++i;

                if (i >= TokenCount ||
                    i >= (NUM_TOOLS + InvalidEntries)
                ) {
                    // Break Loop
                    break;
                }

                // Set Showtools Index
                j = (DoneTool) ? j + 1 : 0;

                Flag = TokenList[i];
                if (0);
                else if (MyStrBegins (L"exit",             Flag)) GlobalConfig.ShowTools[j] = TAG_EXIT;
                else if (MyStrBegins (L"about",            Flag)) GlobalConfig.ShowTools[j] = TAG_ABOUT;
                else if (MyStrBegins (L"shell",            Flag)) GlobalConfig.ShowTools[j] = TAG_SHELL;
                else if (MyStrBegins (L"gdisk",            Flag)) GlobalConfig.ShowTools[j] = TAG_GDISK;
                else if (MyStrBegins (L"reboot",           Flag)) GlobalConfig.ShowTools[j] = TAG_REBOOT;
                else if (MyStrBegins (L"gptsync",          Flag)) GlobalConfig.ShowTools[j] = TAG_GPTSYNC;
                else if (MyStrBegins (L"memtest",          Flag)) GlobalConfig.ShowTools[j] = TAG_MEMTEST;
                else if (MyStrBegins (L"install",          Flag)) GlobalConfig.ShowTools[j] = TAG_INSTALL;
                else if (MyStrBegins (L"netboot",          Flag)) GlobalConfig.ShowTools[j] = TAG_NETBOOT;
                else if (MyStrBegins (L"shutdown",         Flag)) GlobalConfig.ShowTools[j] = TAG_SHUTDOWN;
                else if (MyStrBegins (L"mok_tool",         Flag)) GlobalConfig.ShowTools[j] = TAG_MOK_TOOL;
                else if (MyStrBegins (L"firmware",         Flag)) GlobalConfig.ShowTools[j] = TAG_FIRMWARE;
                else if (MyStrBegins (L"bootorder",        Flag)) GlobalConfig.ShowTools[j] = TAG_BOOTORDER;
                else if (MyStrBegins (L"csr_rotate",       Flag)) GlobalConfig.ShowTools[j] = TAG_CSR_ROTATE;
                else if (MyStrBegins (L"clean_nvram",      Flag)) GlobalConfig.ShowTools[j] = TAG_CLEAN_NVRAM;
                else if (MyStrBegins (L"windows_recovery", Flag)) GlobalConfig.ShowTools[j] = TAG_RECOVERY_WIN;
                else if (MyStrBegins (L"apple_recovery",   Flag)) GlobalConfig.ShowTools[j] = TAG_RECOVERY_MAC;
                else if (MyStrBegins (L"fwupdate",         Flag)) GlobalConfig.ShowTools[j] = TAG_FWUPDATE_TOOL;
                else if (MyStrBegins (L"hidden_tags",      Flag)) {
                    GlobalConfig.ShowTools[j] = TAG_HIDDEN;
                    GlobalConfig.HiddenTags = TRUE;
                }
                else {
                    #if REFIT_DEBUG > 0
                    if (NotRunBefore) MuteLogger = FALSE;
                    ALT_LOG(1, LOG_THREE_STAR_MID,
                        L"Invalid Config Entry in 'showtools' List:- '%s'!!",
                        Flag
                    );
                    if (NotRunBefore) MuteLogger = TRUE;
                    #endif

                    // Handle Showtools Index
                    j = (DoneTool) ? j - 1 : 0;

                    // Increment Invalid Entry Count
                    ++InvalidEntries;

                    // Skip 'DoneTool' Update
                    // In case this is the first entry
                    continue;
                }

                // Update 'DoneTool' if false
                if (!DoneTool) {
                    DoneTool = TRUE;
                }

                // Update 'SetShowTools' if false
                if (!SetShowTools) {
                    SetShowTools = TRUE;
                }
            } // for ;;
        }
        else if (MyStriCmp (TokenList[0], L"banner")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleString (
                TokenList, TokenCount,
                &(GlobalConfig.BannerFileName)
            );
        }
        else if (
            TokenCount == 2 &&
            MyStriCmp (TokenList[0], L"banner_scale")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            if (MyStriCmp (TokenList[1], L"noscale")) {
                GlobalConfig.BannerScale = BANNER_NOSCALE;
            }
            else if (
                MyStriCmp (TokenList[1], L"fillscreen") ||
                MyStriCmp (TokenList[1], L"fullscreen")
            ) {
                GlobalConfig.BannerScale = BANNER_FILLSCREEN;
            }
            else {
                MsgStr = PoolPrint (
                    L"  - WARN: Invalid 'banner_type' Flag:- '%s'",
                    TokenList[1]
                );
                PrintUglyText (MsgStr, NEXTLINE);

                #if REFIT_DEBUG > 0
                if (NotRunBefore) MuteLogger = FALSE;
                LOG_MSG("%s%s", OffsetNext, MsgStr);
                if (NotRunBefore) MuteLogger = TRUE;
                #endif

                PauseForKey();
                MY_FREE_POOL(MsgStr);
            } // if/else MyStriCmp TokenList[0]
        }
        else if (
            TokenCount == 2 &&
            MyStriCmp (TokenList[0], L"small_icon_size")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleUnsignedInt (
                TokenList, TokenCount, &i
            );
            if (i >= 32) {
                GlobalConfig.IconSizes[ICON_SIZE_SMALL] = i;
            }
        }
        else if (
            TokenCount == 2 &&
            MyStriCmp (TokenList[0], L"big_icon_size")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleUnsignedInt (
                TokenList, TokenCount, &i
            );
            if (i >= 32) {
                GlobalConfig.IconSizes[ICON_SIZE_BIG] = i;
                GlobalConfig.IconSizes[ICON_SIZE_BADGE] = i / 4;
            }
        }
        else if (MyStriCmp (TokenList[0], L"selection_small")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleString (
                TokenList, TokenCount,
                &(GlobalConfig.SelectionSmallFileName)
            );
        }
        else if (MyStriCmp (TokenList[0], L"selection_big")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleString (
                TokenList, TokenCount,
                &(GlobalConfig.SelectionBigFileName)
            );
        }
        else if (MyStriCmp (TokenList[0], L"default_selection")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            MY_FREE_POOL(GlobalConfig.DefaultSelection);
            if (TokenCount == 4) {
                SetDefaultByTime (
                    TokenList, &(GlobalConfig.DefaultSelection)
                );
            }
            else {
                HandleString (
                    TokenList, TokenCount,
                    &(GlobalConfig.DefaultSelection)
                );
            }
        }
        else if (
            MyStriCmp (TokenList[0], L"resolution") &&
            (TokenCount == 2 || TokenCount == 3)
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            if (MyStriCmp(TokenList[1], L"max")) {
                // DA-TAG: Has been set to 0 so as to ignore the 'max' setting
                //GlobalConfig.RequestedScreenWidth  = MAX_RES_CODE;
                //GlobalConfig.RequestedScreenHeight = MAX_RES_CODE;
                GlobalConfig.RequestedScreenWidth  = 0;
                GlobalConfig.RequestedScreenHeight = 0;
            }
            else {
                GlobalConfig.RequestedScreenWidth  = Atoi(TokenList[1]);
                GlobalConfig.RequestedScreenHeight = (TokenCount == 3)
                    ? Atoi(TokenList[2]) : 0;
            }
        }
        else if (MyStriCmp (TokenList[0], L"screensaver")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA-TAG: Signed integer as can have negative value
            HandleSignedInt (
                TokenList, TokenCount,
                &(GlobalConfig.ScreensaverTime)
            );
        }
        else if (
            TokenCount == 2 &&
            MyStriCmp (TokenList[0], L"font")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            egLoadFont (TokenList[1]);
        }
        else if (MyStriCmp (TokenList[0], L"textonly")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.TextOnly = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (MyStriCmp (TokenList[0], L"textmode")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleUnsignedInt (
                TokenList, TokenCount,
                &(GlobalConfig.RequestedTextMode)
            );
        }
        else if (MyStriCmp (TokenList[0], L"scan_all_linux_kernels")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.ScanAllLinux = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (MyStriCmp (TokenList[0], L"fold_linux_kernels")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.FoldLinuxKernels = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (MyStriCmp (TokenList[0], L"linux_prefixes")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleStrings (
                TokenList, TokenCount,
                &(GlobalConfig.LinuxPrefixes)
            );
        }
        else if (MyStriCmp (TokenList[0], L"csr_values")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleHexes (
                TokenList, TokenCount,
                CSR_MAX_LEGAL_VALUE, &(GlobalConfig.CsrValues)
            );
        }
        else if (
            TokenCount == 4 &&
            MyStriCmp (TokenList[0], L"screen_rgb")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA-TAG: Consider handling hex input?
            //         KISS ... Stick with integers
            GlobalConfig.ScreenR = Atoi(TokenList[1]);
            GlobalConfig.ScreenG = Atoi(TokenList[2]);
            GlobalConfig.ScreenB = Atoi(TokenList[3]);

            // Record whether a valid custom screen BG is specified
            GlobalConfig.CustomScreenBG = (
                GlobalConfig.ScreenR >= 0 && GlobalConfig.ScreenR <= 255 &&
                GlobalConfig.ScreenG >= 0 && GlobalConfig.ScreenG <= 255 &&
                GlobalConfig.ScreenB >= 0 && GlobalConfig.ScreenB <= 255
            );
        }
        else if (MyStriCmp (TokenList[0], L"enable_mouse")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.EnableMouse = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (MyStriCmp (TokenList[0], L"enable_touch")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.EnableTouch = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (MyStriCmp (TokenList[0], L"persist_boot_args")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.PersistBootArgs = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"disable_set_consolegop") ||
            MyStriCmp (TokenList[0], L"provide_console_gop"   )
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            DeclineSetting = HandleBoolean (TokenList, TokenCount);
            if (MyStriCmp (TokenList[0], L"disable_set_consolegop")) {
                GlobalConfig.SetConsoleGOP = (DeclineSetting) ? FALSE : TRUE;
            }
            else {
                // DA_TAG: Duplication Purely to Accomodate Deprecation
                //         Change top level 'substring' check when dropped
                GlobalConfig.SetConsoleGOP = DeclineSetting;
            }
        }
        else if (
            MyStriCmp (TokenList[0], L"transient_boot"      ) ||
            MyStriCmp (TokenList[0], L"ignore_previous_boot")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA_TAG: Accomodate Deprecation
            GlobalConfig.TransientBoot = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"hidden_icons_ignore") ||
            MyStriCmp (TokenList[0], L"ignore_hidden_icons")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA_TAG: Accomodate Deprecation
            GlobalConfig.HiddenIconsIgnore = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"hidden_icons_external") ||
            MyStriCmp (TokenList[0], L"external_hidden_icons")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA_TAG: Accomodate Deprecation
            GlobalConfig.HiddenIconsExternal = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"hidden_icons_prefer") ||
            MyStriCmp (TokenList[0], L"prefer_hidden_icons")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA_TAG: Accomodate Deprecation
            GlobalConfig.HiddenIconsPrefer = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"renderer_text") ||
            MyStriCmp (TokenList[0], L"text_renderer")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA_TAG: Accomodate Deprecation
            GlobalConfig.UseTextRenderer = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"pass_uga_through") ||
            MyStriCmp (TokenList[0], L"uga_pass_through")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA_TAG: Accomodate Deprecation
            GlobalConfig.PassUgaThrough = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"disable_reload_gop") ||
            MyStriCmp (TokenList[0], L"decline_reload_gop") ||
            MyStriCmp (TokenList[0], L"decline_reloadgop" )
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA_TAG: Accomodate Deprecation
            DeclineSetting = HandleBoolean (TokenList, TokenCount);
            GlobalConfig.ReloadGOP = (DeclineSetting) ? FALSE : TRUE;
        }
        else if (
            MyStriCmp (TokenList[0], L"disable_apfs_load") ||
            MyStriCmp (TokenList[0], L"decline_apfs_load") ||
            MyStriCmp (TokenList[0], L"decline_apfsload" )
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA_TAG: Accomodate Deprecation
            DeclineSetting = HandleBoolean (TokenList, TokenCount);
            GlobalConfig.SupplyAPFS = (DeclineSetting) ? FALSE : TRUE;
        }
        else if (
            MyStriCmp (TokenList[0], L"disable_apfs_sync") ||
            MyStriCmp (TokenList[0], L"decline_apfs_sync") ||
            MyStriCmp (TokenList[0], L"decline_apfssync" )
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA_TAG: Accomodate Deprecation
            DeclineSetting = HandleBoolean (TokenList, TokenCount);
            GlobalConfig.SyncAPFS = (DeclineSetting) ? FALSE : TRUE;
        }
        else if (
            MyStriCmp (TokenList[0], L"disable_set_applefb") ||
            MyStriCmp (TokenList[0], L"disable_provide_fb" ) ||
            MyStriCmp (TokenList[0], L"decline_apple_fb"   ) ||
            MyStriCmp (TokenList[0], L"decline_applefb"    )
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore,
                    (AppleFirmware) ? TRUE : FALSE
                );
            }
            #endif

            // DA_TAG: Skip on UEFI-PC ... Default Always Used
            if (AppleFirmware) {
                // DA_TAG: Accomodate Deprecation
                DeclineSetting = HandleBoolean (TokenList, TokenCount);
                GlobalConfig.SetAppleFB = (DeclineSetting) ? FALSE : TRUE;
            }
        }
        else if (MyStriCmp (TokenList[0], L"handle_ventoy")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.HandleVentoy = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (MyStriCmp (TokenList[0], L"decline_help_icon")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            DeclineSetting = HandleBoolean (TokenList, TokenCount);
            GlobalConfig.HelpIcon = (DeclineSetting) ? FALSE : TRUE;
        }
        else if (
            MyStriCmp (TokenList[0], L"decline_help_text") ||
            MyStriCmp (TokenList[0], L"decline_text_help") ||
            MyStriCmp (TokenList[0], L"decline_texthelp" )
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA_TAG: Accomodate Deprecation
            DeclineSetting = HandleBoolean (TokenList, TokenCount);
            GlobalConfig.HelpText = (DeclineSetting) ? FALSE : TRUE;
        }
        else if (MyStriCmp (TokenList[0], L"decline_help_scan")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            DeclineSetting = HandleBoolean (TokenList, TokenCount);
            GlobalConfig.HelpScan = (DeclineSetting) ? FALSE : TRUE;
        }
        else if (MyStriCmp (TokenList[0], L"decline_help_size")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            DeclineSetting = HandleBoolean (TokenList, TokenCount);
            GlobalConfig.HelpSize = (DeclineSetting) ? FALSE : TRUE;
        }
        else if (MyStriCmp (TokenList[0], L"disable_legacy_sync")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            DeclineSetting = HandleBoolean (TokenList, TokenCount);
            GlobalConfig.LegacySync = (DeclineSetting) ? FALSE : TRUE;
        }
        else if (MyStriCmp (TokenList[0], L"follow_symlinks")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.FollowSymlinks = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"csr_normalise") ||
            MyStriCmp (TokenList[0], L"normalise_csr")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA_TAG: Accomodate Deprecation
            GlobalConfig.NormaliseCSR = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"csr_dynamic") ||
            MyStriCmp (TokenList[0], L"active_csr" )
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA_TAG: Accomodate Deprecation
            // DA-TAG: Signed integer as can have negative value
            HandleSignedInt (
                TokenList, TokenCount,
                &(GlobalConfig.DynamicCSR)
            );
        }
        else if (MyStriCmp (TokenList[0], L"disable_nvram_paniclog")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.DisableNvramPanicLog = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"disable_check_compat") ||
            MyStriCmp (TokenList[0], L"disable_compat_check")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.DisableCheckCompat = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"disable_check_amfi") ||
            MyStriCmp (TokenList[0], L"disable_amfi")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.DisableCheckAMFI = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (MyStriCmp (TokenList[0], L"supply_nvme")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.SupplyNVME = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (MyStriCmp (TokenList[0], L"supply_uefi")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.SupplyUEFI = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            StriSubCmp (L"esp_filter", TokenList[0]) ||
            StriSubCmp (L"espfilter",  TokenList[0])
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            DeclineSetting = HandleBoolean (TokenList, TokenCount);
            if (MyStriCmp (TokenList[0], L"enable_esp_filter")) {
                GlobalConfig.ScanAllESP = (DeclineSetting) ? FALSE : TRUE;
            }
            else if (
                MyStriCmp (TokenList[0], L"disable_esp_filter") ||
                MyStriCmp (TokenList[0], L"disable_espfilter" )
            ) {
                // DA_TAG: Duplication Purely to Accomodate Deprecation
                //         Change top level 'substring' check when dropped
                GlobalConfig.ScanAllESP = DeclineSetting;
            }
        }
        else if (MyStriCmp (TokenList[0], L"scale_ui")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA-TAG: Signed integer as can have negative value
            HandleSignedInt (
                TokenList, TokenCount,
                &(GlobalConfig.ScaleUI)
            );
        }
        else if (
            CheckManual    &&
            !DoneManual    &&
            TokenCount > 1 &&
            MyStriCmp (TokenList[0], L"menuentry")
        ) {
            // DA-TAG: Do not log this or set 'UpdatedToken'
            DoneManual = TRUE;
        }
        else if (
            MyStriCmp (TokenList[0], L"disable_nvram_protect") ||
            MyStriCmp (TokenList[0], L"decline_nvram_protect") ||
            MyStriCmp (TokenList[0], L"decline_nvramprotect" )
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore,
                    (AppleFirmware) ? TRUE : FALSE
                );
            }
            #endif

            // DA_TAG: Skip on UEFI-PC ... Default Always Used
            if (AppleFirmware) {
                // DA_TAG: Accomodate Deprecation
                DeclineSetting = HandleBoolean (TokenList, TokenCount);
                GlobalConfig.NvramProtect = (DeclineSetting) ? FALSE : TRUE;
            }
        }
        else if (MyStriCmp (TokenList[0], L"disable_pass_gop_thru")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            DeclineSetting = HandleBoolean (TokenList, TokenCount);
            GlobalConfig.PassGopThrough = (DeclineSetting) ? FALSE : TRUE;
        }
        else if (
            MyStriCmp (TokenList[0], L"renderer_direct_gop") ||
            MyStriCmp (TokenList[0], L"direct_gop_renderer")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA_TAG: Accomodate Deprecation
            GlobalConfig.UseDirectGop = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            MyStriCmp (TokenList[0], L"force_trim") ||
            MyStriCmp (TokenList[0], L"trim_force")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA_TAG: Accomodate Deprecation
            GlobalConfig.ForceTRIM = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            TokenCount == 2 &&
            MyStriCmp (TokenList[0], L"mouse_size")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleUnsignedInt (
                TokenList, TokenCount, &i
            );
            if (i >= DEFAULT_MOUSE_SIZE) {
                GlobalConfig.IconSizes[ICON_SIZE_MOUSE] = i;
            }
        }
        else if (
            TokenCount == 2 &&
            MyStriCmp (TokenList[0], L"mouse_speed")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleUnsignedInt (
                TokenList, TokenCount, &i
            );

            if (i < 1) {
                i = 1;
            }
            else if (i > 32) {
                i = 32;
            }
            GlobalConfig.MouseSpeed = i;
        }
        else if (MyStriCmp (TokenList[0], L"continue_on_warning")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.ContinueOnWarning = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (MyStriCmp (TokenList[0], L"decouple_key_f10")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.DecoupleKeyF10 = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            TokenCount == 2 &&
            MyStriCmp (TokenList[0], L"icon_row_move")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA-TAG: Signed integer as *MAY* have negative value input
            HandleSignedInt (
                TokenList, TokenCount,
                &(GlobalConfig.IconRowMove)
            );
        }
        else if (
            TokenCount == 2 &&
            MyStriCmp (TokenList[0], L"icon_row_tune")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            // DA-TAG: Signed integer as *MAY* have negative value input
            HandleSignedInt (
                TokenList, TokenCount,
                &(GlobalConfig.IconRowTune)
            );

            // Store as opposite number
            GlobalConfig.IconRowTune *= -1;
        }
        else if (
            TokenCount == 2 &&
            MyStriCmp (TokenList[0], L"scan_delay")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleUnsignedInt (
                TokenList, TokenCount,
                &(GlobalConfig.ScanDelay)
            );
        }
        else if (MyStriCmp (TokenList[0], L"uefi_deep_legacy_scan")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.DeepLegacyScan = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (MyStriCmp (TokenList[0], L"ransom_drives")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore,
                    (!AppleFirmware) ? TRUE : FALSE
                );
            }
            #endif

            // DA_TAG: Skip on Apple Mac ... Default Always Used
            if (!AppleFirmware) {
                GlobalConfig.RansomDrives = HandleBoolean (
                    TokenList, TokenCount
                );
            }
        }
        else if (MyStriCmp (TokenList[0], L"prefer_uga")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.PreferUGA = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (MyStriCmp (TokenList[0], L"windows_recovery_files")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleStrings (
                TokenList, TokenCount,
                &(GlobalConfig.WindowsRecoveryFiles)
            );
        }
        else if (MyStriCmp (TokenList[0], L"shutdown_after_timeout")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.ShutdownAfterTimeout = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (MyStriCmp (TokenList[0], L"set_boot_args")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleString (
                TokenList, TokenCount,
                &(GlobalConfig.SetBootArgs)
            );

            if (MyStriCmp (GlobalConfig.SetBootArgs, L"-none")) {
                MY_FREE_POOL(GlobalConfig.SetBootArgs);
            }
        }
        else if (MyStriCmp (TokenList[0], L"nvram_protect_ex")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore,
                    (AppleFirmware) ? TRUE : FALSE
                );
            }
            #endif

            // DA_TAG: Skip on UEFI-PC ... Default Always Used
            if (AppleFirmware) {
                GlobalConfig.NvramProtectEx = HandleBoolean (
                    TokenList, TokenCount
                );
            }
        }
        else if (MyStriCmp (TokenList[0], L"write_systemd_vars")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.WriteSystemdVars = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (MyStriCmp (TokenList[0], L"extra_kernel_version_strings")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleStrings (
                TokenList, TokenCount,
                &(GlobalConfig.ExtraKernelVersionStrings)
            );
        }
        else if (MyStriCmp (TokenList[0], L"max_tags")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleUnsignedInt (
                TokenList, TokenCount,
                &(GlobalConfig.MaxTags)
            );
        }
        else if (MyStriCmp (TokenList[0], L"enable_and_lock_vmx")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.EnableAndLockVMX = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (MyStriCmp (TokenList[0], L"spoof_osx_version")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleString (
                TokenList, TokenCount,
                &(GlobalConfig.SpoofOSXVersion)
            );
        }
        else if (MyStriCmp (TokenList[0], L"support_gzipped_loaders")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.GzippedLoaders = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            TokenCount == 2 &&
            MyStriCmp (TokenList[0], L"nvram_variable_limit")
        ) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            HandleUnsignedInt (
                TokenList, TokenCount,
                &(GlobalConfig.NvramVariableLimit)
            );
        }
        else if (MyStriCmp (TokenList[0], L"unicode_collation")) {
            #if REFIT_DEBUG > 0
            if (!OuterLoop) {
                UpdatedToken = LogUpdate (
                    TokenList[0], NotRunBefore, TRUE
                );
            }
            #endif

            GlobalConfig.UnicodeCollation = HandleBoolean (
                TokenList, TokenCount
            );
        }
        else if (
            OuterLoop                                     &&
            TokenCount == 2                               &&
            MyStriCmp (TokenList[0], L"include")          &&
            MyStriCmp (FileName, GlobalConfig.ConfigFilename)
        ) {
            if (!MyStriCmp (TokenList[1], FileName)) {
                #if REFIT_DEBUG > 0
                // DA-TAG: Always log this in case LogLevel is overriden
                RealLogLevel = 0;
                HighLogLevel = MaxLogLevel * 10;
                if (GlobalConfig.LogLevel < LOGLEVELMIN) {
                    RealLogLevel = GlobalConfig.LogLevel;
                    GlobalConfig.LogLevel = HighLogLevel;
                }

                if (NotRunBefore) MuteLogger = FALSE;
                if (FirstInclude) {
                    LOG_MSG("\n");
                    LOG_MSG("Detected Override File(s) - L O A D   C O N F I G   O V E R R I D E S");
                }
                LOG_MSG(
                    "%s%s* Supplementary Configuration ... %s",
                    (FirstInclude) ? L"" : L"\n",
                    OffsetNext,
                    TokenList[1]
                );
                FirstInclude = FALSE;
                LOG_MSG("%s*** Examine Included File ***", OffsetNext);
                if (NotRunBefore) MuteLogger = TRUE; /* Explicit For FB Infer */
                #endif

                // Set 'OuterLoop' to 'false' to break any 'include' chains
                OuterLoop = FALSE;
                ReadConfig (TokenList[1]);
                OuterLoop = TRUE;
                // Reset 'OuterLoop' to accomodate multiple instances in main file

                #if REFIT_DEBUG > 0
                if (NotRunBefore) MuteLogger = TRUE; /* Explicit For FB Infer */

                // DA-TAG: Restore the RealLogLevel
                if (GlobalConfig.LogLevel == HighLogLevel) {
                    GlobalConfig.LogLevel  = RealLogLevel;
                }
                #endif
            }
        }

        FreeTokenLine (&TokenList, &TokenCount);
    } // for ;;

    MY_FREE_POOL(File.Buffer);

    if (OuterLoop) {
        ExitOuter (
            #if REFIT_DEBUG > 0
            ValidInclude, NotRunBefore
            #endif
        );
        ReadLoops = 0;

        #if REFIT_DEBUG > 0
        // Reset Misc Flags
        NotRunBefore =                FALSE;
        FirstInclude = ValidInclude =  TRUE;
        #endif
    }
    else {
        // Reset Loop Count
        ReadLoops = ReadLoops - 1;

        #if REFIT_DEBUG > 0
        if (NotRunBefore) MuteLogger = FALSE;
        if (!UpdatedToken) {
            if (!DoneManual) {
                LOG_MSG("%s  - Active Tokens *NOT* Found", OffsetNext);
            }
            else {
                LOG_MSG("%s  - Only Got Manual Stanza(s) ... Handle Later", OffsetNext);
            }
        }
        LOG_MSG("%s*** Handled Included File ***", OffsetNext);
        // DA-TAG: No 'TRUE' Flag
        #endif
    }
} // VOID ReadConfig()
