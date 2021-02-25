/*
 * refit/mystrings.h
 * String functions header file
 *
 * Copyright (c) 2012-2020 Roderick W. Smith
 *
 * Distributed under the terms of the GNU General Public License (GPL)
 * version 3 (GPLv3), or (at your option) any later version.
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

#ifndef __MYSTRINGS_H_
#define __MYSTRINGS_H_

#ifdef __MAKEWITH_GNUEFI
#include <efi.h>
#include <efilib.h>
#else
#include "../include/tiano_includes.h"
#endif
#include "../EfiLib/GenericBdsLib.h"

typedef struct _string_list {
    CHAR16               *Value;
    struct _string_list  *Next;
} STRING_LIST;

BOOLEAN StriSubCmp(IN CHAR16 *TargetStr, IN CHAR16 *BigStr);
BOOLEAN MyStriCmp(IN CONST CHAR16 *String1, IN CONST CHAR16 *String2);
CHAR16* MyStrStr (IN CHAR16  *String, IN CHAR16  *StrCharSet);
VOID ToLower(CHAR16 * MyString);
VOID MergeStrings(IN OUT CHAR16 **First, IN CHAR16 *Second, CHAR16 AddChar);
VOID MergeWords(CHAR16 **MergeTo, CHAR16 *InString, CHAR16 AddChar);
BOOLEAN LimitStringLength(CHAR16 *TheString, UINTN Limit);
CHAR16 *FindNumbers(IN CHAR16 *InString);
UINTN NumCharsInCommon(IN CHAR16* String1, IN CHAR16* String2);
CHAR16 *FindCommaDelimited(IN CHAR16 *InString, IN UINTN Index);
BOOLEAN DeleteItemFromCsvList(CHAR16 *ToDelete, CHAR16 *List);
BOOLEAN IsIn(IN CHAR16 *SmallString, IN CHAR16 *List);
BOOLEAN IsInSubstring(IN CHAR16 *BigString, IN CHAR16 *List);
BOOLEAN ReplaceSubstring(IN OUT CHAR16 **MainString, IN CHAR16 *SearchString, IN CHAR16 *ReplString);

BOOLEAN IsValidHex(CHAR16 *Input);
UINT64 StrToHex(CHAR16 *Input, UINTN Position, UINTN NumChars);
BOOLEAN IsGuid(CHAR16 *UnknownString);
CHAR16 * GuidAsString(EFI_GUID *GuidData);
EFI_GUID StringAsGuid(CHAR16 * InString);

CHAR16 *GetTimeString(VOID);
VOID DeleteStringList(STRING_LIST *StringList);

#endif
