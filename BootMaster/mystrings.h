/*
 * BootMaster/mystrings.h
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
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2024 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
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

// DA-TAG: See here for more if needed:
//         https://www.virtualbox.org/svn/vbox/trunk/src/VBox/Devices/EFI/Firmware/MdePkg/Library/BaseLib/String.c
BOOLEAN IsValidHex (CHAR16 *Input);
BOOLEAN IsGuid (CHAR16 *UnknownString);
BOOLEAN IsIn (IN CHAR16 *SmallString, IN CHAR16 *List);
BOOLEAN TruncateString (CHAR16 *TheString, UINTN Limit);
BOOLEAN LimitStringLength (CHAR16 *TheString, UINTN Limit);
BOOLEAN StriSubCmp (IN CHAR16 *TargetStr, IN CHAR16 *BigStr);
BOOLEAN IsListItem (IN CHAR16 *SmallString, IN CHAR16 *List);
BOOLEAN IsListMatch (IN CHAR16 *TestString, IN CHAR16 *List);
BOOLEAN IsInSubstring (IN CHAR16 *BigString, IN CHAR16 *List);
BOOLEAN DeleteItemFromCsvList (CHAR16 *ToDelete, CHAR16 **List);
BOOLEAN FindSubStr (
    IN CHAR16 *RawString,
    IN CHAR16 *RawStrCharSet
);
BOOLEAN IsListItemSubstringIn (
    IN CHAR16 *BigString,
    IN CHAR16 *List
);
BOOLEAN ReplaceSubstring (
    IN OUT CHAR16 **MainString,
    IN     CHAR16  *SearchString,
    IN     CHAR16  *ReplString
);
BOOLEAN MyStriCmp (
    IN const CHAR16 *String1,
    IN const CHAR16 *String2
);
BOOLEAN MyStrBegins (
    IN const CHAR16 *String1,
    IN const CHAR16 *String2
);

CHAR16 * GetTimeString (VOID);
CHAR16 * FindNumbers (IN CHAR16 *InString);
CHAR16 * GuidAsString (EFI_GUID *GuidData);
CHAR16 * SanitiseString (CHAR16 *InString);
CHAR16 * MyStrStr (
    IN CHAR16 *String,
    IN CHAR16 *StrCharSet
);
CHAR16 * FindCommaDelimited (
    IN CHAR16 *InString,
    IN UINTN   Index
);
CHAR16 * MyAsciiStrCopyToUnicode (
    IN  CHAR8   *AsciiString,
    IN  UINTN    Length
);
CHAR16 * GetSubStrAfter (
    IN CHAR16 *InputDelimiter,
    IN CHAR16 *String
);
CHAR16 * GetSubStrBefore (
    IN CHAR16 *InputDelimiter,
    IN CHAR16 *String
);

VOID ToUpper (IN OUT CHAR16 *MyString);
VOID ToLower (IN OUT CHAR16 *MyString);
VOID DeleteStringList (STRING_LIST *StringList);
VOID MergeWords (
    IN OUT CHAR16 **MergeTo,
    IN     CHAR16  *InString,
    IN     CHAR16   AddChar
);
VOID MergeUniqueWords (
    IN OUT CHAR16 **MergeTo,
    IN     CHAR16  *InString,
    IN     CHAR16   AddChar
);
VOID MergeUniqueItems (
    IN OUT CHAR16 **MergeTo,
    IN     CHAR16  *InString,
    IN     CHAR16   AddChar
);
VOID MergeStrings (
    IN OUT CHAR16 **First,
    IN     CHAR16  *Second,
    IN     CHAR16   AddChar
);
VOID MergeUniqueStrings (
    IN OUT CHAR16 **First,
    IN     CHAR16  *Second,
    IN     CHAR16   AddChar
);
VOID MyUnicodeFilterString (
    IN OUT CHAR16   *String,
    IN     BOOLEAN   SingleLine
);

CHAR8 * MyAsciiStrStr (IN const CHAR8 *String, IN const CHAR8 *SearchString);

UINTN NumCharsInCommon (IN CHAR16 *String1, IN CHAR16 *String2);

UINT64 StrToHex (CHAR16 *Input, UINTN Position, UINTN NumChars);

EFI_GUID StringAsGuid (CHAR16 *InString);
#endif
