/*
 * BootMaster/mystrings.c
 * String-manipulation functions
 *
 * Copyright (c) 2012-2020 Roderick W. Smith
 *
 * Distributed under the terms of the GNU General Public License (GPL)
 * version 3 (GPLv3), or (at your option) any later version.
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2024 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "mystrings.h"
#include "lib.h"
#include "screenmgt.h"
#include "../include/refit_call_wrapper.h"

BOOLEAN NestedStrStr = FALSE;


/*++
 *
 * Routine Description:
 *
 *  Return the substring after a supplied delimiter.
 *
 * Arguments:
 *
 *  Delimiter  - Null-terminated string to search for as delimiter.
 *  String     - Null-terminated string to search for a Substring.
 *
 * Returns:
 *  The address of the matching substring after the delimiter
 *  or the original string if the delimiter was not found.
 * --*/
CHAR16 * GetSubStrAfter (
    IN CHAR16 *InputDelimiter,
    IN CHAR16 *String
) {
    CHAR16 *Substring;
    CHAR16 *Delimiter;


    if (String == NULL) {
        return NULL;
    }

    // Handle Deprecated 'INITIAL_STRING_DELIM' = L" @@ "
    if (!MyStriCmp (InputDelimiter, DEFAULT_STRING_DELIM)) {
        Delimiter = InputDelimiter;
    }
    else if (MyStrStr (String, INITIAL_STRING_DELIM)) {
        Delimiter = INITIAL_STRING_DELIM;
    }
    else {
        Delimiter = DEFAULT_STRING_DELIM;
    }

    Substring = MyStrStr (String, Delimiter);
    if (Substring == NULL) {
        // Return original string ... Delimiter not found
        return String;
    }

    // Move past delimiter
    Substring += StrLen (Delimiter);
    if (*Substring == L'\0') {
        // Return original string ... Delimiter is at end
        return String;
    }

    return Substring;
} // CHAR16 * GetSubStrAfter()


/*++
 *
 * Routine Description:
 *
 *  Return the substring before a supplied delimiter.
 *  Note that the calling function is responsible for freeing
 *  the memory associated with the returned string pointer.
 *
 * Arguments:
 *
 *  Delimiter  - Null-terminated string to search for as delimiter.
 *  String     - Null-terminated string to search for a Substring.
 *
 * Returns:
 *  The address of the matching substring before the delimiter
 *  or the original string if the delimiter was not found.
 * --*/
CHAR16 * GetSubStrBefore (
    IN CHAR16 *InputDelimiter,
    IN CHAR16 *String
) {
    UINTN   Length;
    CHAR16 *Result;
    CHAR16 *Substring;
    CHAR16 *Delimiter;


    if (String == NULL) {
        return NULL;
    }

    // Handle Deprecated 'INITIAL_STRING_DELIM' = L" @@ "
    if (!MyStriCmp (InputDelimiter, DEFAULT_STRING_DELIM)) {
        Delimiter = InputDelimiter;
    }
    else if (MyStrStr (String, INITIAL_STRING_DELIM)) {
        Delimiter = INITIAL_STRING_DELIM;
    }
    else {
        Delimiter = DEFAULT_STRING_DELIM;
    }

    Substring = MyStrStr (String, Delimiter);
    if (Substring == NULL) {
        // Return original string ... Delimiter not found
        return String;
    }

    if (MyStriCmp (Substring, String)) {
        // Return original string ... Delimiter is at start
        return String;
    }

    Length = StrLen (String) - StrLen (Substring);
    Result = AllocateZeroPool ((Length + 1) * sizeof (CHAR16));
    if (Result == NULL) {
        // Return original string ... Memory exhausted
        return String;
    }

    REFIT_CALL_3_WRAPPER(
        gBS->CopyMem, Result,
        String, sizeof (CHAR16) * Length
    );
    Result[Length] = L'\0'; // Null-terminate result

    return Result;
} // CHAR16 * GetSubStrBefore()

BOOLEAN StriSubCmp (
    IN CHAR16 *SmallStr,
    IN CHAR16 *BigStr
) {
    UINTN   BigStart;
    UINTN   BigIndex;
    UINTN   SmallIndex;
    BOOLEAN Terminate;
    BOOLEAN Found;

    if (SmallStr == NULL || BigStr == NULL) {
        return FALSE;
    }

    Found = Terminate = FALSE;
    BigIndex = SmallIndex = BigStart = 0;
    while (!Terminate) {
        if (BigStr[BigIndex] == '\0') {
            Terminate = TRUE;
        }

        if (SmallStr[SmallIndex] == '\0') {
            Found     = TRUE;
            Terminate = TRUE;
        }

        if ((SmallStr[SmallIndex] & ~0x20) == (BigStr[BigIndex] & ~0x20)) {
            SmallIndex++;
            BigIndex++;
        }
        else {
            SmallIndex = 0;
            BigStart++;
            BigIndex = BigStart;
        }
    } // while

    return Found;
} // BOOLEAN StriSubCmp()

// Performs a case-insensitive string comparison. This function is necesary
// because some EFIs have buggy StriCmp() functions that actually perform
// case-sensitive comparisons.
// Returns TRUE if strings are identical, FALSE otherwise.
BOOLEAN MyStriCmp (
    IN const CHAR16 *String1,
    IN const CHAR16 *String2
) {
    if (String1 == NULL || String2 == NULL) {
        return FALSE;
    }

    while ((*String1 != L'\0') &&
        ((*String1 & ~0x20) == (*String2 & ~0x20))
    ) {
        String1++;
        String2++;
    } // while

    return (*String1 == *String2);
} // BOOLEAN MyStriCmp()

// As MyStriCmp but only checks whether String2 starts with String1
// Returns TRUE on match, FALSE otherwise.
BOOLEAN MyStrBegins (
    IN const CHAR16 *String1,
    IN const CHAR16 *String2
) {
    BOOLEAN StrBegins;

    if (String1 == NULL || String2 == NULL) {
        return FALSE;
    }

    StrBegins = FALSE;
    while (*String1 != L'\0') {
        if ((*String1 & ~0x20) != (*String2 & ~0x20)) {
            StrBegins = FALSE;

            break;
        }

        String1++;
        String2++;
        StrBegins = TRUE;
    } // while

    return StrBegins;
} // BOOLEAN MyStrBegins()


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
CHAR16 * MyStrStr (
    IN CHAR16  *String,
    IN CHAR16  *StrCharSet
) {
    CHAR16 *Src;
    CHAR16 *Sub;


    if (!NestedStrStr) LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START:- Find '%s' in '%s'", __func__,
        StrCharSet ? StrCharSet : L"NULL",
        String     ? String     : L"NULL"
    );
    if (String == NULL || StrCharSet == NULL) {
        BREAD_CRUMB(L"%a:  return 'NULL'", __func__);
        LOG_DECREMENT();
        if (!NestedStrStr) LOG_SEP(L"X");
        return NULL;
    }

    //BREAD_CRUMB(L"%a:  2", __func__);
    Src = String;
    Sub = StrCharSet;

    //BREAD_CRUMB(L"%a:  3 - WHILE LOOP:- START", __func__);
    while ((*String != L'\0') && (*StrCharSet != L'\0')) {
        if (*String++ == *StrCharSet) {
            StrCharSet++;
        }
        else {
            String     = ++Src;
            StrCharSet = Sub;
        }
    } // while
    //BREAD_CRUMB(L"%a:  4 - WHILE LOOP:- END", __func__);

    if (*StrCharSet == L'\0') {
        BREAD_CRUMB(L"%a:  4a - END:- return CHAR16 *Src (Substring Found)", __func__);
        LOG_DECREMENT();
        if (!NestedStrStr) LOG_SEP(L"X");
        return Src;
    }

    BREAD_CRUMB(L"%a:  5 - END:- return NULL (Substring *NOT* Found)", __func__);
    LOG_DECREMENT();
    if (!NestedStrStr) LOG_SEP(L"X");

    return NULL;
} // CHAR16 * MyStrStr()

/*++
 *
 * Routine Description:
 *
 *  As 'MyStrStr' but case insensitive and returns a BOOLEAN.
 *
 * Arguments:
 *
 *  RawString      - Null-terminated string to search.
 *  RawStrCharSet  - Null-terminated string to search for.
 *
 * Returns:
 *  TRUE if successful, or FALSE otherwise.
 * --*/
BOOLEAN FindSubStr (
    IN CHAR16  *RawString,
    IN CHAR16  *RawStrCharSet
) {
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START:- Find '%s' in '%s'", __func__,
        RawStrCharSet ? RawStrCharSet : L"NULL",
        RawString     ? RawString     : L"NULL"
    );
    if (RawString == NULL || RawStrCharSet == NULL) {
        BREAD_CRUMB(L"%a:  return 'FALSE'", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return FALSE;
    }

    CHAR16  *String     = StrDuplicate (RawString);
    CHAR16  *StrCharSet = StrDuplicate (RawStrCharSet);
    BOOLEAN  FoundStr   = FALSE;

    //BREAD_CRUMB(L"%a:  2", __func__);
    ToLower (String);
    ToLower (StrCharSet);

    //BREAD_CRUMB(L"%a:  3", __func__);
    NestedStrStr = TRUE;
    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    MY_MUTELOGGER_SET;
    #endif
    if (MyStrStr (String, StrCharSet)) {
        //BREAD_CRUMB(L"%a:  3a 1", __func__);
        FoundStr = TRUE;
    }
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif
    NestedStrStr = FALSE;

    //BREAD_CRUMB(L"%a:  3", __func__);
    MY_FREE_POOL(String);
    MY_FREE_POOL(StrCharSet);

    BREAD_CRUMB(L"%a:  4 - END:- return BOOLEAN FoundStr = '%s'", __func__,
        FoundStr ? L"TRUE" : L"FALSE"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return FoundStr;
} // BOOLEAN FindSubStr()


/**
  Returns the first occurrence of a Null-terminated ASCII sub-string
  in a Null-terminated ASCII string.

  This function scans the contents of the ASCII string specified by String
  and returns the first occurrence of SearchString. If SearchString is not
  found in String, then NULL is returned. If the length of SearchString is zero,
  then String is returned.

  If String is NULL, then ASSERT().
  If SearchString is NULL, then ASSERT().

  If PcdMaximumAsciiStringLength is not zero, and SearchString or
  String contains more than PcdMaximumAsciiStringLength Unicode characters
  not including the Null-terminator, then ASSERT().

  @param  String          A pointer to a Null-terminated ASCII string.
  @param  SearchString    A pointer to a Null-terminated ASCII string to search for.

  @retval NULL            If the SearchString does not appear in String.
  @retval others          If there is a match return the first occurrence of SearchingString.
                          If the length of SearchString is zero,return String.

**/
CHAR8 * MyAsciiStrStr (
    IN const CHAR8 *String,
    IN const CHAR8 *SearchString
) {
    const CHAR8 *FirstMatch;
    const CHAR8 *SearchStringTmp;

    //
    // ASSERT both strings are shorter than PcdMaximumAsciiStringLength
    //
    ASSERT (AsciiStrSize (String) != 0);
    ASSERT (AsciiStrSize (SearchString) != 0);

    if (*SearchString == '\0') {
        return (CHAR8 *) String;
    }

    while (*String != '\0') {
        SearchStringTmp = SearchString;
        FirstMatch = String;

        while (*String == *SearchStringTmp && *String != '\0') {
            String++;
            SearchStringTmp++;
        }

        if (*SearchStringTmp == '\0') {
            return (CHAR8 *) FirstMatch;
        }

        if (*String == '\0') {
            return NULL;
        }

        String = FirstMatch + 1;
    } // while

    return NULL;
} // CHAR16 * MyAsciiStrStr()

// Convert input string to all-lowercase.
// DO NOT USE the standard StrLwr() function, as it is broken on some EFIs!
VOID ToLower (
    IN OUT CHAR16 *MyString
) {
    UINTN i;

    if (MyString == NULL) {
        return;
    }

    i = 0;
    while (MyString[i] != L'\0') {
        if ((MyString[i] >= L'A') && (MyString[i] <= L'Z')) {
            MyString[i] = MyString[i] - L'A' + L'a';
        }
        i++;
    } // while
} // VOID ToLower()

// Convert input string to all-uppercase.
VOID ToUpper (
    IN OUT CHAR16 *MyString
) {
    UINTN i;

    if (MyString == NULL) {
        return;
    }

    i = 0;
    while (MyString[i] != L'\0') {
        if ((MyString[i] >= L'a') && (MyString[i] <= L'z')) {
            MyString[i] = MyString[i] - L'a' + L'A';
        }
        i++;
    } // while
} // VOID ToUpper()

// Merges two strings, creating a new one and returning a pointer to it.
// If AddChar != 0, the specified character is placed between the two original
// strings (unless the first string is NULL or empty). The original input
// string *First is de-allocated and replaced by the new merged string.
// This is similar to StrCat, but safer and more flexible because
// MergeStrings allocates memory that is the correct size for the
// new merged string, so it can take a NULL *First and it cleans
// up the old memory. It should *NOT* be used with a constant
// *First, though.
VOID MergeStrings (
    IN OUT CHAR16 **First,
    IN     CHAR16  *Second,
    IN     CHAR16   AddChar
) {
    UINTN           Length1;
    UINTN           Length2;
    CHAR16         *NewString;

    #if REFIT_DEBUG > 1
    CHAR16         *MsgStr;


    MsgStr = PoolPrint (
        L"Add '%s' to the end of '%s' (%s Separator)",
        Second  ? Second  : L"NULL",
        *First  ? *First  : L"NULL",
        AddChar ? L"After a" : L"With no"
    );
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START:- %s", __func__, MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    if (*First == NULL) {
        *First = StrDuplicate (Second);
        BREAD_CRUMB(L"%a:  1a 1 - END:- NULL INPUT - Out String = '%s'", __func__,
            *First
        );
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return;
    }

    //BREAD_CRUMB(L"%a:  2", __func__);
    Length1 = StrLen (*First);

    //BREAD_CRUMB(L"%a:  3", __func__);
    Length2 = (Second != NULL) ? StrLen (Second) : 0;

    //BREAD_CRUMB(L"%a:  4", __func__);
    NewString = AllocatePool (sizeof (CHAR16) * (Length1 + Length2 + 2));

    //BREAD_CRUMB(L"%a:  5", __func__);
    if (NewString) {
        //BREAD_CRUMB(L"%a:  5a 1", __func__);
        if (*First != NULL && Length1 == 0) {
            //BREAD_CRUMB(L"%a:  5a 1a 1", __func__);
            MY_FREE_POOL(*First);
        }

        //BREAD_CRUMB(L"%a:  5a 2", __func__);
        NewString[0] = L'\0';

        //BREAD_CRUMB(L"%a:  5a 3", __func__);
        if (*First != NULL) {
            //BREAD_CRUMB(L"%a:  5a 3a 1", __func__);
            StrCat (NewString, *First);

            //BREAD_CRUMB(L"%a:  5a 3a 2", __func__);
            if (AddChar) {
                //BREAD_CRUMB(L"%a:  5a 3a 2a 1", __func__);
                NewString[Length1] = AddChar;
                NewString[Length1 + 1] = '\0';
            }
            //BREAD_CRUMB(L"%a:  5a 3a 3", __func__);
        }

        //BREAD_CRUMB(L"%a:  5a 4", __func__);
        if (Second != NULL) {
            //BREAD_CRUMB(L"%a:  5a 4a 1", __func__);
            StrCat (NewString, Second);
        }

        //BREAD_CRUMB(L"%a:  5a 5", __func__);
        MY_FREE_POOL(*First);
        *First = NewString;
    }
    BREAD_CRUMB(L"%a:  6 - END:- Out String = '%s'", __func__,
        *First
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID MergeStrings()

// As MergeStrings but does not repeat substrings.
VOID MergeUniqueStrings (
    IN OUT CHAR16 **First,
    IN     CHAR16  *Second,
    IN     CHAR16   AddChar
) {
    UINTN    i;
    UINTN    Length1;
    UINTN    Length2;
    CHAR16  *TestStr;
    CHAR16  *NewString;
    BOOLEAN  SkipMerge;

    #if REFIT_DEBUG > 1
    CHAR16 *MsgStr;


    MsgStr = PoolPrint (
        L"If not already present as a substring, add '%s' to the end of '%s' (%s Separator)",
        Second  ? Second  : L"NULL",
        *First  ? *First  : L"NULL",
        AddChar ? L"After a" : L"With no"
    );
    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START:- %s", __func__, MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    if (*First == NULL) {
        *First = StrDuplicate (Second);
        BREAD_CRUMB(L"%a:  1a 1 - END:- NULL Input - Out String = '%s'", __func__,
            *First
        );
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return;
    }

    //BREAD_CRUMB(L"%a:  2", __func__);
    Length1 = StrLen (*First);

    //BREAD_CRUMB(L"%a:  3", __func__);
    if (Second == NULL) {
        //BREAD_CRUMB(L"%a:  3a 1", __func__);
        Length2 = 0;
    }
    else {
        //BREAD_CRUMB(L"%a:  3b 1", __func__);
        Length2 = StrLen (Second);
    }

    //BREAD_CRUMB(L"%a:  4", __func__);
    NewString = AllocatePool (sizeof (CHAR16) * (Length1 + Length2 + 2));
    if (NewString == NULL) {
        BREAD_CRUMB(L"%a:  4a 1 - END:- OUT OF MEMERY - Out String = '%s'", __func__,
            *First
        );
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return;
    }

    //BREAD_CRUMB(L"%a:  5", __func__);
    if (*First != NULL && Length1 == 0) {
        //BREAD_CRUMB(L"%a:  5a 1", __func__);
        MY_FREE_POOL(*First);
    }

    //BREAD_CRUMB(L"%a:  6", __func__);
    NewString[0] = L'\0';

    //BREAD_CRUMB(L"%a:  7", __func__);
    if (*First != NULL) {
        //BREAD_CRUMB(L"%a:  7a 1", __func__);
        StrCat (NewString, *First);

        //BREAD_CRUMB(L"%a:  7a 2", __func__);
        if (AddChar) {
            //BREAD_CRUMB(L"%a:  7a 2a 1", __func__);
            NewString[Length1] = AddChar;
            NewString[Length1 + 1] = '\0';
        }
    }

    //BREAD_CRUMB(L"%a:  8", __func__);
    if (Second != NULL) {
        //BREAD_CRUMB(L"%a:  8a 1", __func__);
        SkipMerge = FALSE;

        //BREAD_CRUMB(L"%a:  8a 2", __func__);
        if (AddChar) {
            //BREAD_CRUMB(L"%a:  8a 2a 1", __func__);
            i = 0;
            TestStr = NULL;

            while (
                !SkipMerge &&
                (TestStr = FindCommaDelimited (NewString, i++)) != NULL
            ) {
                //BREAD_CRUMB(L"%a:  8a 2a 1a 1 - WHILE LOOP:- START", __func__);
                NestedStrStr = TRUE;
                if (MyStriCmp (TestStr, Second)) {
                    SkipMerge = TRUE;
                }
                NestedStrStr = FALSE;

                MY_FREE_POOL(TestStr);
                //BREAD_CRUMB(L"%a:  8a 2a 1a 2 - WHILE LOOP:- END", __func__);
            } // while
            //BREAD_CRUMB(L"%a:  8a 2a 2", __func__);
        }

        //BREAD_CRUMB(L"%a:  8a 3", __func__);
        if (!SkipMerge) {
            //BREAD_CRUMB(L"%a:  8a 3a 1", __func__);
            StrCat (NewString, Second);
        }
        else if (AddChar) {
            //BREAD_CRUMB(L"%a:  8a 3b 1", __func__);
            // Remove AddChar if not merging this item
            NewString[Length1] = '\0';
        }
        //BREAD_CRUMB(L"%a:  8a 4", __func__);
    }

    //BREAD_CRUMB(L"%a:  9", __func__);
    MY_FREE_POOL(*First);
    *First = NewString;

    BREAD_CRUMB(L"%a:  10 - END:- Out String = '%s'", __func__,
        *First
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");
} // VOID MergeUniqueStrings()

// Similar to MergeStrings, but breaks the input string into word chunks and
// merges each word separately. Words are defined as string fragments separated
// by ' ', ':', '_', or '-'.
VOID MergeWords (
    CHAR16 **MergeTo,
    CHAR16  *InString,
    CHAR16   AddChar
) {
    CHAR16  *Temp, *Word, *p;
    BOOLEAN  LineFinished;

    if (InString == NULL) {
        return;
    }

    Temp = Word = p = StrDuplicate (InString);
    if (Temp) {
        LineFinished = FALSE;

        while (!LineFinished) {
            if ((*p == L' ') ||
                (*p == L':') ||
                (*p == L'_') ||
                (*p == L'-') ||
                (*p == L'\0')
            ) {
                if (*p == L'\0') {
                    LineFinished = TRUE;
                }

                *p = L'\0';

                if (*Word != L'\0') {
                    MergeStrings (MergeTo, Word, AddChar);
                }

                Word = p + 1;
            }

            p++;
        } // while

        MY_FREE_POOL(Temp);
    }
} // VOID MergeWords()

// As MergeWords, but only unique words are merged
VOID MergeUniqueWords (
    CHAR16 **MergeTo,
    CHAR16  *InString,
    CHAR16   AddChar
) {
    CHAR16 *Temp, *Word, *p;
    BOOLEAN LineFinished;

    if (InString == NULL) {
        return;
    }

    Temp = Word = p = StrDuplicate (InString);
    if (Temp) {
        LineFinished = FALSE;

        while (!LineFinished) {
            if ((*p == L' ') ||
                (*p == L':') ||
                (*p == L'_') ||
                (*p == L'-') ||
                (*p == L'\0')
            ) {
                if (*p == L'\0') {
                    LineFinished = TRUE;
                }

                *p = L'\0';

                if (*Word != L'\0') {
                    MergeUniqueStrings (MergeTo, Word, AddChar);
                }

                Word = p + 1;
            }

            p++;
        } // while

        MY_FREE_POOL(Temp);
    }
} // VOID MergeUniqueWords()

// As MergeUniqueWords, but items are separated by ','
VOID MergeUniqueItems (
    CHAR16 **MergeTo,
    CHAR16  *InString,
    CHAR16   AddChar
) {
    UINTN   i;
    CHAR16 *Item;

    if (InString == NULL) {
        return;
    }

    i = 0;
    while ((Item = FindCommaDelimited (InString, i++)) != NULL) {
        MergeUniqueStrings (MergeTo, Item, AddChar);
        MY_FREE_POOL(Item);
    } // while
} // VOID MergeUniqueItems()

// Replaces special characters in the input string with a space.
CHAR16 * SanitiseString (
    CHAR16  *InString
) {
    CHAR16  *Temp, *Word, *p;
    CHAR16  *OutString;
    BOOLEAN  LineFinished;

    if (InString == NULL) {
        return NULL;
    }

    OutString = NULL;
    Temp = Word = p = StrDuplicate (InString);
    if (Temp) {
        LineFinished = FALSE;

        while (!LineFinished) {
            if (
                (*p != L' ') &&
                (*p != L'_') &&
                (*p != L'-') &&
                !('a' <= *p && 'z' >= *p) &&
                !('A' <= *p && 'Z' >= *p) &&
                !('0' <= *p && '9' >= *p)
            ) {
                if (*p == L'\0') {
                    LineFinished = TRUE;
                }

                *p = L'\0';

                if (*Word != L'\0') {
                    MergeStrings (&OutString, Word, L' ');
                }

                Word = p + 1;
            }

            p++;
        } // while

        MY_FREE_POOL(Temp);
    }

    if (OutString == NULL) {
        OutString = StrDuplicate (InString);
    }

    return OutString;
} // CHAR16 * SanitiseString()

// Restrict 'TheString' to no more than 'Limit' characters.
// Does this in two steps:
//   - Compresses blocks of two or more spaces down to one.
//   - Truncates 'TheString' if still longer than 'Limit'.
// Returns TRUE if changes were made, FALSE otherwise
BOOLEAN LimitStringLength (
    CHAR16 *TheString,
    UINTN    Limit
) {
    UINTN     i;
    UINTN     DestSize;
    CHAR16   *SubString;
    CHAR16   *TempString;
    BOOLEAN   HasChanged;
    BOOLEAN   WasTruncated;


    if (TheString == NULL) {
        return FALSE;
    }

    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START", __func__);

    if (StrLen (TheString) < Limit) {
        BREAD_CRUMB(L"%a:  1a 1 - END:- return BOOLEAN HasChanged = 'FALSE'", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return FALSE;
    }

    //BREAD_CRUMB(L"%a:  2 - WHILE LOOP:- START/ENTER", __func__);
    HasChanged = FALSE;
    // SubString will be NULL or point WITHIN TheString
    SubString = MyStrStr (TheString, L"  ");

    //BREAD_CRUMB(L"%a:  3 - WHILE LOOP:- START/ENTER", __func__);
    while (SubString != NULL) {
        i = 0;
        while (SubString[i] == L' ') {
            i++;
        }

        if (i >= StrLen (SubString)) {
            SubString[0] = '\0';
        }
        else {
            TempString = StrDuplicate (&SubString[i]);
            if (TempString == NULL) {
                // Memory Allocation Problem ... abort to avoid potential infinite loop!
                break;
            }

            DestSize = StrSize (&SubString[1]) / sizeof (CHAR16);
            StrCpyS (&SubString[1], DestSize, TempString);
            MY_FREE_POOL(TempString);
        }

        HasChanged = TRUE;
        SubString = MyStrStr (TheString, L"  ");
    } // while
    //BREAD_CRUMB(L"%a:  4 - WHILE LOOP:- END/EXIT", __func__);

    // Truncate if still too long.
    WasTruncated = TruncateString (TheString, Limit);

    //BREAD_CRUMB(L"%a:  5", __func__);
    if (!HasChanged) {
        //BREAD_CRUMB(L"%a:  5a 1", __func__);
        HasChanged = WasTruncated;
    }

    BREAD_CRUMB(L"%a:  6 - END:- return BOOLEAN HasChanged = '%s'", __func__,
        HasChanged ? L"TRUE" : L"FALSE"
    );
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return HasChanged;
} // BOOLEAN LimitStringLength()

// Truncate 'TheString' to 'Limit' characters if longer.
// Returns TRUE if truncated or FALSE otherwise
BOOLEAN TruncateString (
    CHAR16 *TheString,
    UINTN   Limit
) {
    BOOLEAN WasTruncated;

    if (StrLen (TheString) <= Limit) {
        WasTruncated = FALSE;
    }
    else {
        TheString[Limit] = '\0';
        WasTruncated     = TRUE;
    }

    return WasTruncated;
} // BOOLEAN TruncateString()

// Returns all the digits in the input string, including intervening
// non-digit characters. For instance, if InString is "foo-3.3.4-7.img",
// this function returns "3.3.4-7". The GlobalConfig.ExtraKernelVersionStrings
// variable specifies extra strings that may be treated as numbers. If InString
// contains no digits or ExtraKernelVersionStrings, the return value is NULL.
CHAR16 * FindNumbers (
    IN CHAR16 *InString
) {
    UINTN   i, EndOfElement, StartOfElement, CopyLength;
    CHAR16 *Found, *ExtraFound, *LookFor;

    if (InString == NULL) {
        return NULL;
    }

    StartOfElement = StrLen (InString);

    // Find extra_kernel_version_strings
    EndOfElement = i = 0;
    ExtraFound = NULL;
    while (
        ExtraFound == NULL &&
        (LookFor = FindCommaDelimited (GlobalConfig.ExtraKernelVersionStrings, i++)) != NULL
    ) {
        ExtraFound = MyStrStr (InString, LookFor);
        if (ExtraFound != NULL) {
            StartOfElement = ExtraFound - InString;
            EndOfElement   = StartOfElement + StrLen (LookFor) - 1;
        }

        MY_FREE_POOL(LookFor);
    } // while

    // Find start & end of target element
    for (i = 0; InString[i] != L'\0'; i++) {
        if ((InString[i] >= L'0') && (InString[i] <= L'9')) {
            if (StartOfElement > i) {
                StartOfElement = i;
            }

            if (EndOfElement < i) {
                EndOfElement = i;
            }
        }
    } // for

    // Extract the target element
    Found = NULL;
    if (EndOfElement > 0) {
        if (EndOfElement >= StartOfElement) {
            CopyLength = EndOfElement - StartOfElement + 1;

            Found = StrDuplicate (&InString[StartOfElement]);
            if (Found != NULL) {
                Found[CopyLength] = 0;
            }
        }
    }

    return (Found);
} // CHAR16 *FindNumbers()

// Returns the number of characters that are in common between
// String1 and String2 before they diverge. For instance, if
// String1 is "FooBar" and String2 is "FoodiesBar", this function
// will return "3", since they both start with "Foo".
UINTN NumCharsInCommon (
    IN CHAR16 *String1,
    IN CHAR16 *String2
) {
    UINTN Count;


    if (String1 == NULL || String2 == NULL) {
        return 0;
    }

    Count = 0;
    while (
        String1[Count] != L'\0'      &&
        String2[Count] != L'\0'      &&
        String1[Count] == String2[Count]
    ) {
        Count++;
    } // while

    return Count;
} // UINTN NumCharsInCommon()

// Find the #Index element (numbered from 0) in a comma-delimited string.
// Returns the found element, or NULL if Index is out of range or InString
// is NULL. Note that the calling function is responsible for freeing the
// memory associated with the returned string pointer.
//
// DA-TAG: Updated for 'ABC, 123, XYZ, 456'
//         That is, ignores leading spaces
//         'Internal' spaces not affected
//         'A B C, XYZ' = 'A B C' & 'XYZ'
CHAR16 * FindCommaDelimited (
    IN CHAR16 *InString,
    IN UINTN   Index
) {
    UINTN     CurPos;
    UINTN     StartPos;
    UINTN     InLength;
    BOOLEAN   Found;
    BOOLEAN   LeadingSpace;
    CHAR16   *FoundString;

    if (InString == NULL) {
        return NULL;
    }

    StartPos = CurPos = 0;
    InLength = StrLen (InString);

    // After while() loop, StartPos marks start of item #Index
    while (Index > 0 && CurPos < InLength) {
        if (InString[CurPos] == L',') {
            Index--;
            StartPos = CurPos + 1;
        }

        CurPos++;
    } // while

    Found        = FALSE;
    LeadingSpace =  TRUE;

    // After while() loop, CurPos is one past the end of the element
    while (!Found && CurPos < InLength) {
        if (InString[CurPos] == L',') {
            Found = TRUE;
        }
        else {
            // Move Current Position
            CurPos++;

            if (LeadingSpace) {
                if (InString[CurPos] == L' ') {
                    // Ignore Leading Space ... Move Start Position
                    ++StartPos;
                }
                else {
                    // No Leading Space
                    LeadingSpace = FALSE;
                }
            }
        }
    } // while

    FoundString = NULL;
    if (Index == 0)  {
        FoundString = StrDuplicate (&InString[StartPos]);
    }

    if (FoundString != NULL) {
        FoundString[CurPos - StartPos] = 0;
    }

    return FoundString;
} // CHAR16 * FindCommaDelimited()

// Delete an element from a list of comma separated values.
// Modifies the *List string, but not the *ToDelete string!
// Returns TRUE if the item was deleted, FALSE otherwise.
BOOLEAN DeleteItemFromCsvList (
    CHAR16  *ToDelete,
    CHAR16  *List
) {
    UINTN    DestSize;
    CHAR16  *Found;
    CHAR16  *Comma;
    BOOLEAN  Retval;

    if (ToDelete == NULL || List == NULL) {
        return FALSE;
    }

    Retval = FALSE;
    Found = MyStrStr (List, ToDelete);
    if (Found != NULL) {
        Comma = MyStrStr (Found, L",");
        if (Comma != NULL) {
            // 'Found' is NOT the final element
            // Calculate the remaining buffer size in characters
            DestSize = StrSize (Found) / sizeof (CHAR16);
            StrCpyS (Found, DestSize, &Comma[1]);
        }
        else {
            // 'Found' is final element
            if (Found == List) {
                // 'Found' is ONLY element
                List[0] = L'\0';
            }
            else {
                // Delete the comma preceding 'Found'.
                Found--;
                Found[0] = L'\0';
            }
        }

        Retval = TRUE;
    }

    return Retval;
} // BOOLEAN DeleteItemFromCsvList()

// Replaced by IsListItem.
// Kept for upstream compatibility.
BOOLEAN IsIn (
    IN CHAR16 *SmallString,
    IN CHAR16 *List
) {
    if (SmallString == NULL || List == NULL) {
        return FALSE;
    }

    return IsListItem (SmallString, List);
} // BOOLEAN IsIn()

// Replaced by IsListItemSubstringIn.
// Kept for upstream compatibility.
BOOLEAN IsInSubstring (
    IN CHAR16 *BigString,
    IN CHAR16 *List
) {
    if (BigString == NULL || List == NULL) {
        return FALSE;
    }

    return IsListItemSubstringIn (BigString, List);
} // BOOLEAN IsInSubstring()

// Returns TRUE if TestString matches a pattern in the comma-delimited List,
// FALSE otherwise.
BOOLEAN IsListMatch (
    IN CHAR16 *TestString,
    IN CHAR16 *List
) {
    UINTN     i;
    BOOLEAN   Found;
    CHAR16   *OnePattern;


    if (TestString == NULL || List == NULL) {
        return FALSE;
    }

    i     =     0;
    Found = FALSE;
    while (
        !Found &&
        (OnePattern = FindCommaDelimited (List, i++)) != NULL
    ) {
        if (RefitMetaiMatch (TestString, OnePattern)) {
            Found = TRUE;
        }
        MY_FREE_POOL(OnePattern);
    } // while

   return Found;
} // BOOLEAN IsListMatch()

// Returns TRUE if SmallString is an element in the comma-delimited List,
// FALSE otherwise. Performs comparison case-insensitively.
BOOLEAN IsListItem (
    IN CHAR16 *SmallString,
    IN CHAR16 *List
) {
    UINTN     i;
    BOOLEAN   Found;
    CHAR16   *OneElement;

    if (SmallString == NULL || List == NULL) {
        return FALSE;
    }

    i = 0;
    Found = FALSE;
    while (
        !Found &&
        (OneElement = FindCommaDelimited (List, i++)) != NULL
    ) {
        if (MyStriCmp (OneElement, SmallString)) {
            Found = TRUE;
        }

        MY_FREE_POOL(OneElement);
    } // while

   return Found;
} // BOOLEAN IsListItem()

// Returns TRUE if any element of List can be found as a substring of
// BigString, FALSE otherwise. Performs comparisons case-insensitively.
BOOLEAN IsListItemSubstringIn (
    IN CHAR16 *BigString,
    IN CHAR16 *List
) {
    BOOLEAN  Found;
    UINTN    ElementLength, i;
    CHAR16  *OneElement;

    if (BigString == NULL || List == NULL) {
        return FALSE;
    }

    i = 0;
    Found = FALSE;
    while (
        !Found &&
        (OneElement = FindCommaDelimited (List, i++)) != NULL
    ) {
        ElementLength = StrLen (OneElement);
        if (ElementLength > 0                   &&
            ElementLength <= StrLen (BigString) &&
            StriSubCmp (OneElement, BigString)
        ) {
            Found = TRUE;
        }

        if (!Found) {
            if (ElementLength <= StrLen (BigString) &&
                StriSubCmp (OneElement, BigString)
            ) {
                Found = TRUE;
            }
        }
        MY_FREE_POOL(OneElement);
    } // while

    return Found;
} // BOOLEAN IsListItemSubstringIn()

// Replace *SearchString in **MainString with *ReplString -- but if *SearchString
// is preceded by "%", instead remove that character.
// Returns TRUE if replacement was done, FALSE otherwise.
BOOLEAN ReplaceSubstring (
    IN OUT CHAR16 **MainString,
    IN     CHAR16  *SearchString,
    IN     CHAR16  *ReplString
) {
    UINTN   DestSize;
    CHAR16 *EndString;
    CHAR16 *NewString;
    CHAR16 *FoundSearchString;


    LOG_SEP(L"X");
    LOG_INCREMENT();
    BREAD_CRUMB(L"%a:  1 - START:- Replace '%s' with '%s' in '%s'", __func__,
        SearchString ? SearchString : L"NULL",
        ReplString   ? ReplString   : L"NULL",
        *MainString  ? *MainString  : L"NULL"
    );
    if (*MainString == NULL || SearchString == NULL || ReplString == NULL) {
        BREAD_CRUMB(L"%a:  1a - END:- return BOOLEAN 'FALSE' ... NULL Input!!", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");

        return FALSE;
    }

    BREAD_CRUMB(L"%a:  2", __func__);
    FoundSearchString = MyStrStr (*MainString, SearchString);
    NestedStrStr      = FALSE;

    BREAD_CRUMB(L"%a:  3", __func__);
    if (FoundSearchString == NULL) {
        BREAD_CRUMB(L"%a:  3a - END:- return BOOLEAN 'FALSE' ... SearchString *NOT* Found!!", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return FALSE;
    }

    BREAD_CRUMB(L"%a:  4", __func__);
    DestSize = StrLen (*MainString) + 1;
    NewString = AllocateZeroPool (DestSize * sizeof (CHAR16));
    if (NewString == NULL) {
        BREAD_CRUMB(L"%a:  4a - END:- return BOOLEAN 'FALSE' ... Out of Resources!!", __func__);
        LOG_DECREMENT();
        LOG_SEP(L"X");
        return FALSE;
    }

    BREAD_CRUMB(L"%a:  5", __func__);
    EndString = &(FoundSearchString[StrLen (SearchString)]);
    FoundSearchString[0] = L'\0';

    BREAD_CRUMB(L"%a:  6", __func__);
    // "FoundSearchString > *MainString" is required to make sure:
    // "FoundSearchString" is within "*MainString" in terms of memory address
    // "FoundSearchString" is not at the start of "*MainString" for the "-1" index
    if ((FoundSearchString > *MainString) &&
        (FoundSearchString[-1] == L'%')
    ) {
        BREAD_CRUMB(L"%a:  6a 1", __func__);
        FoundSearchString[-1] = L'\0';
        ReplString = SearchString;
    }

    BREAD_CRUMB(L"%a:  7", __func__);
    StrCpyS (NewString, DestSize, *MainString);

    BREAD_CRUMB(L"%a:  8", __func__);
    MergeStrings (&NewString, ReplString, L'\0');

    BREAD_CRUMB(L"%a:  9", __func__);
    MergeStrings (&NewString, EndString, L'\0');

    BREAD_CRUMB(L"%a:  10", __func__);
    MY_FREE_POOL(*MainString);
    *MainString = NewString;

    BREAD_CRUMB(L"%a:  11 - END:- return BOOLEAN 'TRUE'", __func__);
    LOG_DECREMENT();
    LOG_SEP(L"X");

    return TRUE;
} // BOOLEAN ReplaceSubstring()

// Returns TRUE if *Input contains nothing but valid hexadecimal characters,
// FALSE otherwise. Note that a leading "0x" is NOT acceptable in the input!
BOOLEAN IsValidHex (
    CHAR16 *Input
) {
    UINTN   i;
    BOOLEAN IsHex;

    i = 0;
    IsHex = TRUE;
    while (IsHex && (Input[i] != L'\0')) {
        if (
            !(
                ((Input[i] >= L'0') && (Input[i] <= L'9')) ||
                ((Input[i] >= L'A') && (Input[i] <= L'F')) ||
                ((Input[i] >= L'a') && (Input[i] <= L'f'))
            )
        ) {
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
UINT64 StrToHex (
    CHAR16 *Input,
    UINTN   Pos,
    UINTN   NumChars
) {
    UINTN  NumDone, InputLength;
    UINT64 retval;
    CHAR16 a;

    if (Input == NULL ||
        NumChars == 0 ||
        NumChars > 16
    ) {
        return 0;
    }

    NumDone = 0;
    retval = 0x00;
    InputLength = StrLen (Input);
    while (Pos <= InputLength && NumDone < NumChars) {
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
    } // while

    return retval;
} // StrToHex()

// Returns TRUE if UnknownString can be interpreted as a GUID, FALSE otherwise.
// Note that the input string must have no extraneous spaces and must be
// conventionally formatted as a 36-character GUID, complete with dashes in
// appropriate places.
BOOLEAN IsGuid (
    CHAR16 *UnknownString
) {
    UINTN   Length, i;
    CHAR16  a;
    BOOLEAN retval;

    if (UnknownString == NULL) {
        return FALSE;
    }

    Length = StrLen (UnknownString);
    if (Length != 36) {
        return FALSE;
    }

    retval = TRUE;
    for (i = 0; i < Length; i++) {
        a = UnknownString[i];
        if (i ==  8 ||
            i == 13 ||
            i == 18 ||
            i == 23
        ) {
            if (a != L'-') {
                retval = FALSE;
                break;
            }
        }
        // DA-TAG: Investigate This
        //         Condition below can apparently never be met (coverity scan)
        //         Comment out until review
        //else if (
        //    ((a < L'a') || (a > L'f')) &&
        //    ((a < L'A') || (a > L'F')) &&
        //    ((a < L'0') && (a > L'9'))
        //) {
        //    retval = FALSE;
        //    break;
        //}
    } // for

    return retval;
} // BOOLEAN IsGuid()

// Return the GUID as a string, suitable for display to the user. Note that the calling
// function is responsible for freeing the allocated memory.
CHAR16 * GuidAsString (
    EFI_GUID *GuidData
) {
    CHAR16 *TheString;

    if (GuidData == NULL) {
        // Early Return
        return NULL;
    }

    TheString = AllocatePool (sizeof (CHAR16) * 37);
    if (TheString == NULL) {
        // Early Return
        return NULL;
    }

    SPrint (
        TheString,
        sizeof (CHAR16) * 37,
        L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        (UINTN) GuidData->Data1,
        (UINTN) GuidData->Data2,
        (UINTN) GuidData->Data3,
        (UINTN) GuidData->Data4[0],
        (UINTN) GuidData->Data4[1],
        (UINTN) GuidData->Data4[2],
        (UINTN) GuidData->Data4[3],
        (UINTN) GuidData->Data4[4],
        (UINTN) GuidData->Data4[5],
        (UINTN) GuidData->Data4[6],
        (UINTN) GuidData->Data4[7]
    );

    return TheString;
} // CHAR16 * GuidAsString()

EFI_GUID StringAsGuid (
    CHAR16 *InString
) {
    EFI_GUID  Guid = NULL_GUID_VALUE;

    if (!IsGuid (InString)) {
        return Guid;
    }

    Guid.Data1    = (UINT32) StrToHex (InString,  0, 8);
    Guid.Data2    = (UINT16) StrToHex (InString,  9, 4);
    Guid.Data3    = (UINT16) StrToHex (InString, 14, 4);
    Guid.Data4[0] =  (UINT8) StrToHex (InString, 19, 2);
    Guid.Data4[1] =  (UINT8) StrToHex (InString, 21, 2);
    Guid.Data4[2] =  (UINT8) StrToHex (InString, 23, 2);
    Guid.Data4[3] =  (UINT8) StrToHex (InString, 26, 2);
    Guid.Data4[4] =  (UINT8) StrToHex (InString, 28, 2);
    Guid.Data4[5] =  (UINT8) StrToHex (InString, 30, 2);
    Guid.Data4[6] =  (UINT8) StrToHex (InString, 32, 2);
    Guid.Data4[7] =  (UINT8) StrToHex (InString, 34, 2);

    return Guid;
} // EFI_GUID StringAsGuid()

// Returns the current time as a string in 24-hour format; e.g., 14:03:17.
// Discards date portion, since for our purposes, we really do not care.
// Calling function is responsible for releasing returned string.
CHAR16 * GetTimeString (VOID) {
    EFI_STATUS  Status;
    EFI_TIME    CurrentTime;
    CHAR16     *TimeStr;

    Status  = REFIT_CALL_2_WRAPPER(gST->RuntimeServices->GetTime, &CurrentTime, NULL);
    TimeStr = EFI_ERROR(Status)
        ? StrDuplicate (L"Unknown Time")
        : PoolPrint (
            L"%02d:%02d:%02d",
            CurrentTime.Hour,
            CurrentTime.Minute,
            CurrentTime.Second
        );

    return TimeStr;
} // CHAR16 *GetTimeString()

// Delete the STRING_LIST pointed to by *StringList.
VOID DeleteStringList (
    STRING_LIST *StringList
) {
    STRING_LIST *Current, *Previous;

    if (StringList == NULL) {
        return;
    }

    Current = StringList;
    while (Current != NULL) {
        MY_FREE_POOL(Current->Value);
        Previous = Current;
        Current  = Current->Next;
        MY_FREE_POOL(Previous);
    }
} // VOID DeleteStringList()

/** Convert null terminated ascii string to unicode.

  @param[in]  String1  A pointer to the ascii string to convert to unicode.
  @param[in]  Length   Length or 0 to calculate the length of the ascii string to convert.

  @retval  A pointer to the converted unicode string allocated from pool.
**/
CHAR16 * MyAsciiStrCopyToUnicode (
    IN  CHAR8   *AsciiString,
    IN  UINTN    Length
) {
    CHAR16  *UnicodeString;
    CHAR16  *UnicodeStringWalker;
    UINTN    UnicodeStringSize;

    if (AsciiString == NULL) {
        return NULL;
    }

    if (Length == 0) {
        Length = AsciiStrLen (AsciiString);
    }

    UnicodeStringSize = (Length + 1) * sizeof (CHAR16);
    UnicodeString = AllocatePool (UnicodeStringSize);

    if (UnicodeString != NULL) {
        UnicodeStringWalker = UnicodeString;
        while (*AsciiString != '\0' && Length--) {
            *(UnicodeStringWalker++) = *(AsciiString++);
        } // while
        *UnicodeStringWalker = L'\0';
    }

    return UnicodeString;
} // CHAR16 * MyAsciiStrCopyToUnicode()

VOID MyUnicodeFilterString (
    IN OUT CHAR16   *String,
    IN     BOOLEAN   SingleLine
) {
    while (*String != L'\0') {
        if ((*String & 0x7FU) != *String) {
            // Remove all unicode characters.
            *String = L'_';
        }
        else if (
            SingleLine &&
            (
                *String == L'\r' ||
                *String == L'\n'
            )
        ) {
            // Stop after printing one line.
            *String = L'\0';

            break;
        }
        else if (
            *String < 0x20 ||
            *String == 0x7F
        ) {
            // Drop all unprintable spaces but space including tabs.
            *String = L'_';
        }

        ++String;
    }
} // VOID MyUnicodeFilterString()
