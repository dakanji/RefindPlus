/*
 * refind/mystrings.c
 * String-manipulation functions
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


#include "mystrings.h"
#include "lib.h"
#include "screen.h"
#include "../include/refit_call_wrapper.h"

BOOLEAN StriSubCmp(IN CHAR16 *SmallStr, IN CHAR16 *BigStr) {
    BOOLEAN Found = 0, Terminate = 0;
    UINTN BigIndex = 0, SmallIndex = 0, BigStart = 0;

    if (SmallStr && BigStr) {
        while (!Terminate) {
            if (BigStr[BigIndex] == '\0') {
                Terminate = 1;
            }
            if (SmallStr[SmallIndex] == '\0') {
                Found = 1;
                Terminate = 1;
            }
            if ((SmallStr[SmallIndex] & ~0x20) == (BigStr[BigIndex] & ~0x20)) {
                SmallIndex++;
                BigIndex++;
            } else {
                SmallIndex = 0;
                BigStart++;
                BigIndex = BigStart;
            }
        } // while
    } // if
    return Found;
} // BOOLEAN StriSubCmp()

// Performs a case-insensitive string comparison. This function is necesary
// because some EFIs have buggy StriCmp() functions that actually perform
// case-sensitive comparisons.
// Returns TRUE if strings are identical, FALSE otherwise.
BOOLEAN MyStriCmp(IN CONST CHAR16 *FirstString, IN CONST CHAR16 *SecondString) {
    if (FirstString && SecondString) {
        while ((*FirstString != L'\0') && ((*FirstString & ~0x20) == (*SecondString & ~0x20))) {
                FirstString++;
                SecondString++;
        }
        return (*FirstString == *SecondString);
    } else {
        return FALSE;
    }
} // BOOLEAN MyStriCmp()

/*++
 * 
 * Routine Description:
 *
 *  Find a substring.
 *
 * Arguments: 
 *
 *  String      - Null-terminated string to search.
 *  StrCharSet  - Null-terminated string to search for.
 *
 * Returns:
 *  The address of the first occurrence of the matching substring if successful, or NULL otherwise.
 * --*/
CHAR16* MyStrStr (IN CHAR16  *String, IN CHAR16  *StrCharSet)
{
    CHAR16 *Src;
    CHAR16 *Sub;

    if ((String == NULL) || (StrCharSet == NULL))
        return NULL;

    Src = String;
    Sub = StrCharSet;

    while ((*String != L'\0') && (*StrCharSet != L'\0')) {
        if (*String++ != *StrCharSet) {
            String = ++Src;
            StrCharSet = Sub;
        } else {
            StrCharSet++;
        }
    }
    if (*StrCharSet == L'\0') {
        return Src;
    } else {
        return NULL;
    }
} // CHAR16 *MyStrStr()

// Convert input string to all-lowercase.
// DO NOT USE the standard StrLwr() function, since it's broken on some EFIs!
VOID ToLower(CHAR16 * MyString) {
    UINTN i = 0;

    if (MyString) {
        while (MyString[i] != L'\0') {
            if ((MyString[i] >= L'A') && (MyString[i] <= L'Z'))
                MyString[i] = MyString[i] - L'A' + L'a';
            i++;
        } // while
    } // if
} // VOID ToLower()

// Merges two strings, creating a new one and returning a pointer to it.
// If AddChar != 0, the specified character is placed between the two original
// strings (unless the first string is NULL or empty). The original input
// string *First is de-allocated and replaced by the new merged string.
// This is similar to StrCat, but safer and more flexible because
// MergeStrings allocates memory that's the correct size for the
// new merged string, so it can take a NULL *First and it cleans
// up the old memory. It should *NOT* be used with a constant
// *First, though....
VOID MergeStrings(IN OUT CHAR16 **First, IN CHAR16 *Second, CHAR16 AddChar) {
    UINTN Length1 = 0, Length2 = 0;
    CHAR16* NewString;

    if (*First != NULL)
        Length1 = StrLen(*First);
    if (Second != NULL)
        Length2 = StrLen(Second);
    NewString = AllocatePool(sizeof(CHAR16) * (Length1 + Length2 + 2));
    if (NewString != NULL) {
        if ((*First != NULL) && (Length1 == 0)) {
            MyFreePool(*First);
            *First = NULL;
        }
        NewString[0] = L'\0';
        if (*First != NULL) {
            StrCat(NewString, *First);
            if (AddChar) {
                NewString[Length1] = AddChar;
                NewString[Length1 + 1] = '\0';
            } // if (AddChar)
        } // if (*First != NULL)
        if (Second != NULL)
            StrCat(NewString, Second);
        MyFreePool(*First);
        *First = NewString;
    } else {
        Print(L"Error! Unable to allocate memory in MergeStrings()!\n");
    } // if/else
} // VOID MergeStrings()

// Similar to MergeStrings, but breaks the input string into word chunks and
// merges each word separately. Words are defined as string fragments separated
// by ' ', ':', '_', or '-'.
VOID MergeWords(CHAR16 **MergeTo, CHAR16 *SourceString, CHAR16 AddChar) {
    CHAR16 *Temp, *Word, *p;
    BOOLEAN LineFinished = FALSE;

    if (SourceString) {
        Temp = Word = p = StrDuplicate(SourceString);
        if (Temp) {
            while (!LineFinished) {
                if ((*p == L' ') || (*p == L':') || (*p == L'_') || (*p == L'-') || (*p == L'\0')) {
                    if (*p == L'\0')
                        LineFinished = TRUE;
                    *p = L'\0';
                    if (*Word != L'\0')
                        MergeStrings(MergeTo, Word, AddChar);
                    Word = p + 1;
                } // if
                p++;
            } // while
            MyFreePool(Temp);
        } else {
            Print(L"Error! Unable to allocate memory in MergeWords()!\n");
        } // if/else
    } // if
} // VOID MergeWords()

// Restrict TheString to at most Limit characters.
// Does this in two ways:
// - Locates stretches of two or more spaces and compresses
//   them down to one space.
// - Truncates TheString
// Returns TRUE if changes were made, FALSE otherwise
BOOLEAN LimitStringLength(CHAR16 *TheString, UINTN Limit) {
    CHAR16    *SubString, *TempString;
    UINTN     i;
    BOOLEAN   HasChanged = FALSE;

    // SubString will be NULL or point WITHIN TheString
    SubString = MyStrStr(TheString, L"  ");
    while (SubString != NULL) {
        i = 0;
        while (SubString[i] == L' ')
            i++;
        if (i >= StrLen(SubString)) {
            SubString[0] = '\0';
            HasChanged = TRUE;
        } else {
            TempString = StrDuplicate(&SubString[i]);
            if (TempString != NULL) {
                StrCpy(&SubString[1], TempString);
                MyFreePool(TempString);
                HasChanged = TRUE;
            } else {
                // memory allocation problem; abort to avoid potentially infinite loop!
                break;
            } // if/else
        } // if/else
        SubString = MyStrStr(TheString, L"  ");
    } // while

    // If the string is still too long, truncate it....
    if (StrLen(TheString) > Limit) {
        TheString[Limit] = '\0';
        HasChanged = TRUE;
    } // if

    return HasChanged;
} // BOOLEAN LimitStringLength()

// Returns all the digits in the input string, including intervening
// non-digit characters. For instance, if InString is "foo-3.3.4-7.img",
// this function returns "3.3.4-7". The GlobalConfig.ExtraKernelVersionStrings
// variable specifies extra strings that may be treated as numbers. If
// InString contains no digits or ExtraKernelVersionStrings, the return value
// is NULL.
CHAR16 *FindNumbers(IN CHAR16 *InString) {
    UINTN i = 0, StartOfElement, EndOfElement = 0, CopyLength;
    CHAR16 *Found = NULL, *ExtraFound = NULL, *LookFor;

    if (InString == NULL)
        return NULL;

    StartOfElement = StrLen(InString);

    // Find extra_kernel_version_strings
    while ((ExtraFound == NULL) && (LookFor = FindCommaDelimited(GlobalConfig.ExtraKernelVersionStrings, i++))) {
        if ((ExtraFound = MyStrStr(InString, LookFor))) {
            StartOfElement = ExtraFound - InString;
            EndOfElement = StartOfElement + StrLen(LookFor) - 1;
        } // if
        MyFreePool(LookFor);
    } // while

    // Find start & end of target element
    for (i = 0; InString[i] != L'\0'; i++) {
        if ((InString[i] >= L'0') && (InString[i] <= L'9')) {
            if (StartOfElement > i)
                StartOfElement = i;
            if (EndOfElement < i)
                EndOfElement = i;
        } // if
    } // for
    // Extract the target element
    if (EndOfElement > 0) {
        if (EndOfElement >= StartOfElement) {
            CopyLength = EndOfElement - StartOfElement + 1;
            Found = StrDuplicate(&InString[StartOfElement]);
            if (Found != NULL)
                Found[CopyLength] = 0;
        } // if (EndOfElement >= StartOfElement)
    } // if (EndOfElement > 0)
    return (Found);
} // CHAR16 *FindNumbers()

// Returns the number of characters that are in common between
// String1 and String2 before they diverge. For instance, if
// String1 is "FooBar" and String2 is "FoodiesBar", this function
// will return "3", since they both start with "Foo".
UINTN NumCharsInCommon(IN CHAR16* String1, IN CHAR16* String2) {
    UINTN Count = 0;
    if ((String1 == NULL) || (String2 == NULL))
        return 0;
    while ((String1[Count] != L'\0') && (String2[Count] != L'\0') && (String1[Count] == String2[Count]))
        Count++;
    return Count;
} // UINTN NumCharsInCommon()

// Find the #Index element (numbered from 0) in a comma-delimited string
// of elements.
// Returns the found element, or NULL if Index is out of range or InString
// is NULL. Note that the calling function is responsible for freeing the
// memory associated with the returned string pointer.
CHAR16 *FindCommaDelimited(IN CHAR16 *InString, IN UINTN Index) {
    UINTN    StartPos = 0, CurPos = 0, InLength;
    BOOLEAN  Found = FALSE;
    CHAR16   *FoundString = NULL;

    if (InString != NULL) {
        InLength = StrLen(InString);
        // After while() loop, StartPos marks start of item #Index
        while ((Index > 0) && (CurPos < InLength)) {
            if (InString[CurPos] == L',') {
                Index--;
                StartPos = CurPos + 1;
            } // if
            CurPos++;
        } // while
        // After while() loop, CurPos is one past the end of the element
        while ((CurPos < InLength) && (!Found)) {
            if (InString[CurPos] == L',')
                Found = TRUE;
            else
                CurPos++;
        } // while
        if (Index == 0)
            FoundString = StrDuplicate(&InString[StartPos]);
        if (FoundString != NULL)
            FoundString[CurPos - StartPos] = 0;
    } // if
    return (FoundString);
} // CHAR16 *FindCommaDelimited()

// Delete an individual element from a comma-separated value list.
// This function modifies the original *List string, but not the
// *ToDelete string!
// Returns TRUE if the item was deleted, FALSE otherwise.
BOOLEAN DeleteItemFromCsvList(CHAR16 *ToDelete, CHAR16 *List) {
    CHAR16 *Found, *Comma;

    if ((ToDelete == NULL) || (List == NULL))
        return FALSE;

    if ((Found = MyStrStr(List, ToDelete)) != NULL) {
        if ((Comma = MyStrStr(Found, L",")) == NULL) {
            // Found is final element
            if (Found == List) { // Found is ONLY element
                List[0] = L'\0';
            } else { // Delete the comma preceding Found....
                Found--;
                Found[0] = L'\0';
            } // if/else
        } else { // Found is NOT final element
            StrCpy(Found, &Comma[1]);
        } // if/else
        return TRUE;
    } else {
        return FALSE;
    } // if/else
} // BOOLEAN DeleteItemFromCsvList()

// Returns TRUE if SmallString is an element in the comma-delimited List,
// FALSE otherwise. Performs comparison case-insensitively.
BOOLEAN IsIn(IN CHAR16 *SmallString, IN CHAR16 *List) {
    UINTN     i = 0;
    BOOLEAN   Found = FALSE;
    CHAR16    *OneElement;

    if (SmallString && List) {
        while (!Found && (OneElement = FindCommaDelimited(List, i++))) {
            if (MyStriCmp(OneElement, SmallString))
                Found = TRUE;
            MyFreePool(OneElement);
        } // while
    } // if
    return Found;
} // BOOLEAN IsIn()

// Returns TRUE if any element of List can be found as a substring of
// BigString, FALSE otherwise. Performs comparisons case-insensitively.
BOOLEAN IsInSubstring(IN CHAR16 *BigString, IN CHAR16 *List) {
    UINTN   i = 0, ElementLength;
    BOOLEAN Found = FALSE;
    CHAR16  *OneElement;

    if (BigString && List) {
        while (!Found && (OneElement = FindCommaDelimited(List, i++))) {
            ElementLength = StrLen(OneElement);
            if ((ElementLength <= StrLen(BigString)) &&
                 (ElementLength > 0) &&
                 (StriSubCmp(OneElement, BigString))) {
                    Found = TRUE;
            } // if
            MyFreePool(OneElement);
        } // while
    } // if
    return Found;
} // BOOLEAN IsSubstringIn()

// Replace *SearchString in **MainString with *ReplString -- but if *SearchString
// is preceded by "%", instead remove that character.
// Returns TRUE if replacement was done, FALSE otherwise.
BOOLEAN ReplaceSubstring(IN OUT CHAR16 **MainString, IN CHAR16 *SearchString, IN CHAR16 *ReplString) {
    BOOLEAN WasReplaced = FALSE;
    CHAR16 *FoundSearchString, *NewString, *EndString;

    FoundSearchString = MyStrStr(*MainString, SearchString);
    if (FoundSearchString) {
        NewString = AllocateZeroPool(sizeof(CHAR16) * StrLen(*MainString));
        if (NewString) {
            EndString = &(FoundSearchString[StrLen(SearchString)]);
            FoundSearchString[0] = L'\0';
            if ((FoundSearchString > *MainString) && (FoundSearchString[-1] == L'%')) {
                FoundSearchString[-1] = L'\0';
                ReplString = SearchString;
            } // if
            StrCpy(NewString, *MainString);
            MergeStrings(&NewString, ReplString, L'\0');
            MergeStrings(&NewString, EndString, L'\0');
            MyFreePool(MainString);
            *MainString = NewString;
            WasReplaced = TRUE;
        } // if
    } // if
    return WasReplaced;
} // BOOLEAN ReplaceSubstring()

// Returns TRUE if *Input contains nothing but valid hexadecimal characters,
// FALSE otherwise. Note that a leading "0x" is NOT acceptable in the input!
BOOLEAN IsValidHex(CHAR16 *Input) {
    BOOLEAN IsHex = TRUE;
    UINTN i = 0;

    while ((Input[i] != L'\0') && IsHex) {
        if (!(((Input[i] >= L'0') && (Input[i] <= L'9')) ||
              ((Input[i] >= L'A') && (Input[i] <= L'F')) ||
              ((Input[i] >= L'a') && (Input[i] <= L'f')))) {
                IsHex = FALSE;
        }
        i++;
    } // while
    return IsHex;
} // BOOLEAN IsValidHex()

// Converts consecutive characters in the input string into a
// number, interpreting the string as a hexadecimal number, starting
// at the specified position and continuing for the specified number
// of characters or until the end of the string, whichever is first.
// NumChars must be between 1 and 16. Ignores invalid characters.
UINT64 StrToHex(CHAR16 *Input, UINTN Pos, UINTN NumChars) {
    UINT64 retval = 0x00;
    UINTN  NumDone = 0, InputLength;
    CHAR16 a;

    if ((Input == NULL) || (NumChars == 0) || (NumChars > 16)) {
        return 0;
    }

    InputLength = StrLen(Input);
    while ((Pos <= InputLength) && (NumDone < NumChars)) {
        a = Input[Pos];
        if ((a >= '0') && (a <= '9')) {
            retval *= 0x10;
            retval += (a - '0');
            NumDone++;
        }
        if ((a >= 'a') && (a <= 'f')) {
            retval *= 0x10;
            retval += (a - 'a' + 0x0a);
            NumDone++;
        }
        if ((a >= 'A') && (a <= 'F')) {
            retval *= 0x10;
            retval += (a - 'A' + 0x0a);
            NumDone++;
        }
        Pos++;
    } // while()
    return retval;
} // StrToHex()

// Returns TRUE if UnknownString can be interpreted as a GUID, FALSE otherwise.
// Note that the input string must have no extraneous spaces and must be
// conventionally formatted as a 36-character GUID, complete with dashes in
// appropriate places.
BOOLEAN IsGuid(CHAR16 *UnknownString) {
    UINTN   Length, i;
    BOOLEAN retval = TRUE;
    CHAR16  a;

    if (UnknownString == NULL)
        return FALSE;

    Length = StrLen(UnknownString);
    if (Length != 36)
        return FALSE;

    for (i = 0; i < Length; i++) {
        a = UnknownString[i];
        if ((i == 8) || (i == 13) || (i == 18) || (i == 23)) {
            if (a != L'-')
                retval = FALSE;
        } else if (((a < L'a') || (a > L'f')) &&
                   ((a < L'A') || (a > L'F')) &&
                   ((a < L'0') && (a > L'9'))) {
            retval = FALSE;
        } // if/else if
    } // for
    return retval;
} // BOOLEAN IsGuid()

// Return the GUID as a string, suitable for display to the user. Note that the calling
// function is responsible for freeing the allocated memory.
CHAR16 * GuidAsString(EFI_GUID *GuidData) {
    CHAR16 *TheString;

    TheString = AllocateZeroPool(42 * sizeof(CHAR16));
    if (GuidData && (TheString != 0)) {
        SPrint (TheString, 82, L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                (UINTN)GuidData->Data1, (UINTN)GuidData->Data2, (UINTN)GuidData->Data3,
                (UINTN)GuidData->Data4[0], (UINTN)GuidData->Data4[1], (UINTN)GuidData->Data4[2],
                (UINTN)GuidData->Data4[3], (UINTN)GuidData->Data4[4], (UINTN)GuidData->Data4[5],
                (UINTN)GuidData->Data4[6], (UINTN)GuidData->Data4[7]);
    }
    return TheString;
} // GuidAsString(EFI_GUID *GuidData)

EFI_GUID StringAsGuid(CHAR16 * InString) {
    EFI_GUID  Guid = NULL_GUID_VALUE;

    if (!IsGuid(InString)) {
        return Guid;
    }

    Guid.Data1 = (UINT32) StrToHex(InString, 0, 8);
    Guid.Data2 = (UINT16) StrToHex(InString, 9, 4);
    Guid.Data3 = (UINT16) StrToHex(InString, 14, 4);
    Guid.Data4[0] = (UINT8) StrToHex(InString, 19, 2);
    Guid.Data4[1] = (UINT8) StrToHex(InString, 21, 2);
    Guid.Data4[2] = (UINT8) StrToHex(InString, 23, 2);
    Guid.Data4[3] = (UINT8) StrToHex(InString, 26, 2);
    Guid.Data4[4] = (UINT8) StrToHex(InString, 28, 2);
    Guid.Data4[5] = (UINT8) StrToHex(InString, 30, 2);
    Guid.Data4[6] = (UINT8) StrToHex(InString, 32, 2);
    Guid.Data4[7] = (UINT8) StrToHex(InString, 34, 2);

    return Guid;
} // EFI_GUID StringAsGuid()

// Returns the current time as a string in 24-hour format; e.g., 14:03:17.
// Discards date portion, since for our purposes, we really don't care.
// Calling function is responsible for releasing returned string.
CHAR16 *GetTimeString(VOID) {
    CHAR16  *TimeStr = NULL;
    EFI_TIME CurrentTime;
    EFI_STATUS Status = EFI_SUCCESS;

    Status = refit_call2_wrapper(ST->RuntimeServices->GetTime, &CurrentTime, NULL);
    if (EFI_ERROR(Status)) {
        TimeStr = PoolPrint(L"unknown time");
    } else {
        TimeStr = PoolPrint(L"%02d:%02d:%02d",
                            CurrentTime.Hour,
                            CurrentTime.Minute,
                            CurrentTime.Second);
    }
    return TimeStr;
} // CHAR16 *GetTimeString()

// Delete the STRING_LIST pointed to by *StringList.
VOID DeleteStringList(STRING_LIST *StringList) {
    STRING_LIST *Current = StringList, *Previous;

    while (Current != NULL) {
        MyFreePool(Current->Value);
        Previous = Current;
        Current = Current->Next;
        MyFreePool(Previous);
    }
} // VOID DeleteStringList()
