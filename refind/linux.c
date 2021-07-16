/*
 * refind/linux.c
 * Code related specifically to Linux loaders
 *
 * Copyright (c) 2006-2010 Christoph Pfisterer
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
 * Modifications copyright (c) 2012-2020 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
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
#include "config.h"
#include "lib.h"
#include "menu.h"
#include "mystrings.h"
#include "linux.h"
#include "log.h"
#include "scan.h"

// Locate an initrd or initramfs file that matches the kernel specified by LoaderPath.
// The matching file has a name that begins with "init" and includes the same version
// number string as is found in LoaderPath -- but not a longer version number string.
// For instance, if LoaderPath is \EFI\kernels\bzImage-3.3.0.efi, and if \EFI\kernels
// has a file called initramfs-3.3.0.img, this function will return the string
// '\EFI\kernels\initramfs-3.3.0.img'. If the directory ALSO contains the file
// initramfs-3.3.0-rc7.img or initramfs-13.3.0.img, those files will NOT match.
// If more than one initrd file matches the extracted version string, the one
// that matches more characters AFTER (actually, from the start of) the version
// string is used.
// If more than one initrd file matches the extracted version string AND they match
// the same amount of characters, the initrd file with the shortest file name is used.
// If no matching init file can be found, returns NULL.
CHAR16 * FindInitrd(IN CHAR16 *LoaderPath, IN REFIT_VOLUME *Volume) {
    CHAR16              *InitrdName = NULL, *FileName, *KernelVersion, *InitrdVersion, *Path;
    CHAR16              *KernelPostNum, *InitrdPostNum;
    UINTN               MaxSharedChars, SharedChars;
    STRING_LIST         *InitrdNames = NULL, *FinalInitrdName = NULL, *CurrentInitrdName = NULL, *MaxSharedInitrd;
    REFIT_DIR_ITER      DirIter;
    EFI_FILE_INFO       *DirEntry;

    LOG(1, LOG_LINE_NORMAL, L"Searching for an initrd to match '%s' on '%s'", LoaderPath, Volume->VolName);
    FileName = Basename(LoaderPath);
    KernelVersion = FindNumbers(FileName);
    LOG(3, LOG_LINE_NORMAL, L"Kernel version string is '%s'", KernelVersion);
    Path = FindPath(LoaderPath);

    // Add trailing backslash for root directory; necessary on some systems, but must
    // NOT be added to all directories, since on other systems, a trailing backslash on
    // anything but the root directory causes them to flake out!
    if (StrLen(Path) == 0) {
        MergeStrings(&Path, L"\\", 0);
    } // if
    DirIterOpen(Volume->RootDir, Path, &DirIter);
    // Now add a trailing backslash if it was NOT added earlier, for consistency in
    // building the InitrdName later....
    if ((StrLen(Path) > 0) && (Path[StrLen(Path) - 1] != L'\\'))
        MergeStrings(&Path, L"\\", 0);
    while (DirIterNext(&DirIter, 2, L"init*,booster*", &DirEntry)) {
        InitrdVersion = FindNumbers(DirEntry->FileName);
        if (((KernelVersion != NULL) && (MyStriCmp(InitrdVersion, KernelVersion))) ||
            ((KernelVersion == NULL) && (InitrdVersion == NULL))) {
                CurrentInitrdName = AllocateZeroPool(sizeof(STRING_LIST));
                if (InitrdNames == NULL)
                    InitrdNames = FinalInitrdName = CurrentInitrdName;
                if (CurrentInitrdName) {
                    CurrentInitrdName->Value = PoolPrint(L"%s%s", Path, DirEntry->FileName);
                    if (CurrentInitrdName != FinalInitrdName) {
                        FinalInitrdName->Next = CurrentInitrdName;
                        FinalInitrdName = CurrentInitrdName;
                    } // if
                } // if
        } // if
        MyFreePool(InitrdVersion);
    } // while
    if (InitrdNames) {
        if (InitrdNames->Next == NULL) {
            InitrdName = StrDuplicate(InitrdNames -> Value);
        } else {
            MaxSharedInitrd = CurrentInitrdName = InitrdNames;
            MaxSharedChars = 0;
            while (CurrentInitrdName != NULL) {
                KernelPostNum = MyStrStr(LoaderPath, KernelVersion);
                InitrdPostNum = MyStrStr(CurrentInitrdName->Value, KernelVersion);
                SharedChars = NumCharsInCommon(KernelPostNum, InitrdPostNum);
                if (SharedChars > MaxSharedChars || (SharedChars == MaxSharedChars && StrLen(CurrentInitrdName->Value) < StrLen(MaxSharedInitrd->Value))) {
                    MaxSharedChars = SharedChars;
                    MaxSharedInitrd = CurrentInitrdName;
                } // if
                // TODO: Compute number of shared characters & compare with max.
                CurrentInitrdName = CurrentInitrdName->Next;
            }
            if (MaxSharedInitrd)
                InitrdName = StrDuplicate(MaxSharedInitrd->Value);
        } // if/else
    } // if
    DeleteStringList(InitrdNames);

    MyFreePool(KernelVersion);
    MyFreePool(Path);
    MyFreePool(FileName);
    LOG(1, LOG_LINE_NORMAL, L"Located initrd is '%s'", InitrdName);
    return (InitrdName);
} // static CHAR16 * FindInitrd()

// Adds InitrdPath to Options, but only if Options doesn't already include an
// initrd= line or a `%v` variable. Done to enable overriding the default initrd
// selection in a refind_linux.conf file's options list.
// If a `%v` substring/variable is found in Options, it is replaced with the
// initrd version string. This is available to allow for more complex customization
// of initrd options.
// Returns a pointer to a new string. The calling function is responsible for
// freeing its memory.
CHAR16 *AddInitrdToOptions(CHAR16 *Options, CHAR16 *InitrdPath) {
    CHAR16 *NewOptions = NULL;

    if (Options != NULL)
        NewOptions = StrDuplicate(Options);
    
    if (InitrdPath != NULL) {
        if (StriSubCmp(L"%v", Options)) {
            CHAR16 *InitrdVersion = FindNumbers(InitrdPath);
            ReplaceSubstring(&NewOptions, L"%v", InitrdVersion);

            MyFreePool(InitrdVersion);
        } else if (!StriSubCmp(L"initrd=", Options)) {
            MergeStrings(&NewOptions, L"initrd=", L' ');
            MergeStrings(&NewOptions, InitrdPath, 0);
        }
    }
    return NewOptions;
} // CHAR16 *AddInitrdToOptions()

// Returns options for a Linux kernel. Reads them from an options file in the
// kernel's directory; and if present, adds an initrd= option for an initial
// RAM disk file with the same version number as the kernel file.
CHAR16 * GetMainLinuxOptions(IN CHAR16 * LoaderPath, IN REFIT_VOLUME *Volume) {
    CHAR16 *Options = NULL, *InitrdName, *FullOptions = NULL, *KernelVersion;

    Options = GetFirstOptionsFromFile(LoaderPath, Volume);
    InitrdName = FindInitrd(LoaderPath, Volume);
    KernelVersion = FindNumbers(InitrdName);
    ReplaceSubstring(&Options, KERNEL_VERSION, KernelVersion);
    FullOptions = AddInitrdToOptions(Options, InitrdName);

    MyFreePool(Options);
    MyFreePool(InitrdName);
    MyFreePool(KernelVersion);
    return (FullOptions);
} // static CHAR16 * GetMainLinuxOptions()

// Read the specified file and add values of "ID", "NAME", or "DISTRIB_ID" tokens to
// OSIconName list. Intended for adding Linux distribution clues gleaned from
// /etc/lsb-release and /etc/os-release files.
static VOID ParseReleaseFile(CHAR16 **OSIconName, REFIT_VOLUME *Volume, CHAR16 *FileName) {
    UINTN       FileSize = 0;
    REFIT_FILE  File;
    CHAR16      **TokenList;
    UINTN       TokenCount = 0;

    if ((Volume == NULL) || (FileName == NULL) || (OSIconName == NULL) || (*OSIconName == NULL))
        return;

    if (FileExists(Volume->RootDir, FileName) &&
        (ReadFile(Volume->RootDir, FileName, &File, &FileSize) == EFI_SUCCESS)) {
        do {
            TokenCount = ReadTokenLine(&File, &TokenList);
            if ((TokenCount > 1) && (MyStriCmp(TokenList[0], L"ID") ||
                                     MyStriCmp(TokenList[0], L"NAME") ||
                                     MyStriCmp(TokenList[0], L"DISTRIB_ID"))) {
                MergeWords(OSIconName, TokenList[1], L',');
            } // if
            FreeTokenLine(&TokenList, &TokenCount);
        } while (TokenCount > 0);
        MyFreePool(File.Buffer);
    } // if
} // VOID ParseReleaseFile()

// Try to guess the name of the Linux distribution & add that name to
// OSIconName list.
VOID GuessLinuxDistribution(CHAR16 **OSIconName, REFIT_VOLUME *Volume, CHAR16 *LoaderPath) {
    // If on Linux root fs, /etc/os-release or /etc/lsb-release file probably has clues....
    ParseReleaseFile(OSIconName, Volume, L"etc\\lsb-release");
    ParseReleaseFile(OSIconName, Volume, L"etc\\os-release");

    // Search for clues in the kernel's filename....
    if (StriSubCmp(L".fc", LoaderPath))
        MergeStrings(OSIconName, L"fedora", L',');
    if (StriSubCmp(L".el", LoaderPath))
        MergeStrings(OSIconName, L"redhat", L',');
} // VOID GuessLinuxDistribution()

// Add a Linux kernel as a submenu entry for another (pre-existing) Linux kernel entry.
VOID AddKernelToSubmenu(LOADER_ENTRY * TargetLoader, CHAR16 *FileName, REFIT_VOLUME *Volume) {
    REFIT_FILE          *File;
    CHAR16              **TokenList = NULL, *InitrdName, *SubmenuName = NULL, *VolName = NULL;
    CHAR16              *Path = NULL, *Title, *KernelVersion;
    REFIT_MENU_SCREEN   *SubScreen;
    LOADER_ENTRY        *SubEntry;
    UINTN               TokenCount;

    File = ReadLinuxOptionsFile(TargetLoader->LoaderPath, Volume);
    if (File != NULL) {
        SubScreen = TargetLoader->me.SubScreen;
        InitrdName = FindInitrd(FileName, Volume);
        KernelVersion = FindNumbers(FileName);
        while ((TokenCount = ReadTokenLine(File, &TokenList)) > 1) {
            ReplaceSubstring(&(TokenList[1]), KERNEL_VERSION, KernelVersion);
            SubEntry = InitializeLoaderEntry(TargetLoader);
            SplitPathName(FileName, &VolName, &Path, &SubmenuName);
            MergeStrings(&SubmenuName, L": ", '\0');
            MergeStrings(&SubmenuName, TokenList[0] ? StrDuplicate(TokenList[0]) : StrDuplicate(L"Boot Linux"), '\0');
            Title = StrDuplicate(SubmenuName);
            LimitStringLength(Title, MAX_LINE_LENGTH);
            SubEntry->me.Title = Title;
            MyFreePool(SubEntry->LoadOptions);
            SubEntry->LoadOptions = AddInitrdToOptions(TokenList[1], InitrdName);
            MyFreePool(SubEntry->LoaderPath);
            SubEntry->LoaderPath = StrDuplicate(FileName);
            CleanUpPathNameSlashes(SubEntry->LoaderPath);
            SubEntry->Volume = Volume;
            FreeTokenLine(&TokenList, &TokenCount);
            SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;
            AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        } // while
        MyFreePool(VolName);
        MyFreePool(Path);
        MyFreePool(SubmenuName);
        MyFreePool(InitrdName);
        MyFreePool(File);
        MyFreePool(KernelVersion);
    } // if
} // static VOID AddKernelToSubmenu()

// Returns TRUE if a file with the same name as the original but with
// ".efi.signed" is also present in the same directory. Ubuntu is using
// this filename as a signed version of the original unsigned kernel, and
// there's no point in cluttering the display with two kernels that will
// behave identically on non-SB systems, or when one will fail when SB
// is active.
// CAUTION: *FullName MUST be properly cleaned up (via CleanUpPathNameSlashes())
BOOLEAN HasSignedCounterpart(IN REFIT_VOLUME *Volume, IN CHAR16 *FullName) {
    CHAR16 *NewFile = NULL;
    BOOLEAN retval = FALSE;

    MergeStrings(&NewFile, FullName, 0);
    MergeStrings(&NewFile, L".efi.signed", 0);
    if (NewFile != NULL) {
        if (FileExists(Volume->RootDir, NewFile)) {
            LOG(2, LOG_LINE_NORMAL, L"Found signed counterpart to '%s'", FullName);
            retval = TRUE;
        }
        MyFreePool(NewFile);
    } // if

    return retval;
} // BOOLEAN HasSignedCounterpart()
