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
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */


#include "mystrings.h"
#include "lib.h"
#include "screenmgt.h"
#include "../include/refit_call_wrapper.h"


BOOLEAN StriSubCmp (
    IN CHAR16 *SmallStr,
    IN CHAR16 *BigStr
) {
    BOOLEAN Found = FALSE, Terminate = FALSE;
    UINTN BigIndex = 0, SmallIndex = 0, BigStart = 0;

    if (!SmallStr || !BigStr) {
        return FALSE;
    }

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
    IN const CHAR16 *FirstString,
    IN const CHAR16 *SecondString
) {
    if (!FirstString || !SecondString) {
        return FALSE;
    }

    while ((*FirstString != L'\0') &&
        ((*FirstString & ~0x20) == (*SecondString & ~0x20))
    ) {
        FirstString++;
        SecondString++;
    } // while

    return (*FirstString == *SecondString);
} // BOOLEAN MyStriCmp()

// As MyStriCmp but only checks whether SecondString starts with FirstString
// Returns TRUE on match, FALSE otherwise.
BOOLEAN MyStrBegins (
    IN CHAR16 *FirstString,
    IN CHAR16 *SecondString
) {
    BOOLEAN StrBegins = FALSE;

    if (!FirstString || !SecondString) {
        return FALSE;
    }

    while (*FirstString != L'\0') {
        if ((*FirstString & ~0x20) == (*SecondString & ~0x20)) {
            StrBegins = TRUE;
            FirstString++;
            SecondString++;
        }
        else {
            StrBegins = FALSE;
            break;
        }
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

    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    LOG(5, LOG_LINE_FORENSIC,
        L"In MyStrStr ... 1 - START:- Find '%s' in '%s'",
        StrCharSet ? StrCharSet : L"NULL",
        String     ? String     : L"NULL"
    );
    if ((String == NULL) || (StrCharSet == NULL)) {
        LOG(5, LOG_LINE_FORENSIC, L"In MyStrStr ... 1a - END:- NULL INPUT");
        LOG(5, LOG_BLANK_LINE_SEP, L"X");
        return NULL;
    }

    //LOG(5, LOG_LINE_FORENSIC, L"In MyStrStr ... 2");
    Src = String;
    Sub = StrCharSet;

    //LOG(5, LOG_LINE_FORENSIC, L"In MyStrStr ... 3 - WHILE LOOP:- START/ENTER");
    while ((*String != L'\0') && (*StrCharSet != L'\0')) {
        if (*String++ != *StrCharSet) {
            String     = ++Src;
            StrCharSet = Sub;
        }
        else {
            StrCharSet++;
        }
    } // while
    //LOG(5, LOG_LINE_FORENSIC, L"In MyStrStr ... 4 - WHILE LOOP:- END/EXIT");

    if (*StrCharSet == L'\0') {
        LOG(5, LOG_LINE_FORENSIC, L"In MyStrStr ... 4a - END:- return CHAR16 *Src (Substring Found)");
        LOG(5, LOG_BLANK_LINE_SEP, L"X");
        return Src;
    }

    LOG(5, LOG_LINE_FORENSIC, L"In MyStrStr ... 5 - END:- return NULL (Substring not Found)");
    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    return NULL;
} // CHAR16 * MyStrStr()

/*++
 *
 * Routine Description:
 *
 *  As 'MyStrStr' but case insensitive.
 *
 * Arguments:
 *
 *  RawString      - Null-terminated string to search.
 *  RawStrCharSet  - Null-terminated string to search for.
 *
 * Returns:
 *  The address of the first occurrence of the matching substring if successful, or NULL otherwise.
 * --*/
CHAR16 * MyStrStrIns (
    IN CHAR16  *RawString,
    IN CHAR16  *RawStrCharSet
) {
    if ((RawString == NULL) || (RawStrCharSet == NULL)) {
        return NULL;
    }

    CHAR16 *Src;
    CHAR16 *Sub;
    CHAR16 *String     = StrDuplicate (RawString);
    CHAR16 *StrCharSet = StrDuplicate (RawStrCharSet);

    ToLower (String);
    ToLower (StrCharSet);

    Src = String;
    Sub = StrCharSet;

    while ((*String != L'\0') && (*StrCharSet != L'\0')) {
        if (*String++ != *StrCharSet) {
            String = ++Src;
            StrCharSet = Sub;
        }
        else {
            StrCharSet++;
        }
    }

    if (*StrCharSet == L'\0') {
        MyFreePool (&String);
        MyFreePool (&StrCharSet);

        return Src;
    }

    MyFreePool (&String);
    MyFreePool (&StrCharSet);

    return NULL;
} // CHAR16 * MyStrStrIns()


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
    // ASSERT both strings are less long than PcdMaximumAsciiStringLength
    //
    ASSERT (AsciiStrSize (String) != 0);
    ASSERT (AsciiStrSize (SearchString) != 0);

    if (*SearchString == '\0') {
        return (CHAR8 *) String;
    }

    while (*String != '\0') {
        SearchStringTmp = SearchString;
        FirstMatch = String;

        while ((*String == *SearchStringTmp) && (*String != '\0')) {
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
    CHAR16 *MyString
) {
    UINTN i = 0;

    if (!MyString) {
        return;
    }

    while (MyString[i] != L'\0') {
        if ((MyString[i] >= L'A') && (MyString[i] <= L'Z')) {
            MyString[i] = MyString[i] - L'A' + L'a';
        }
        i++;
    } // while
} // VOID ToLower()

// Convert input string to all-uppercase.
VOID ToUpper (
    CHAR16 *MyString
) {
    UINTN i = 0;

    if (!MyString) {
        return;
    }

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
    IN CHAR16      *Second,
    CHAR16          AddChar
) {
    UINTN Length1 = 0, Length2 = 0;
    CHAR16* NewString;

    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 1 - START:- Merge '%s' into '%s'",
        Second ? Second : L"NULL",
        *First ? *First : L"NULL"
    );

    if (*First == NULL) {
        *First = StrDuplicate (Second);
        LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 1a 1 - END:- VOID ... Out String = '%s'", *First);
        LOG(5, LOG_BLANK_LINE_SEP, L"X");

        return;
    }

    //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 2");
    Length1 = StrLen (*First);

    //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 3");
    if (Second != NULL) {
        //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 3a 1");
        Length2 = StrLen (Second);

        //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 3a 2");
    }

    //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 4");
    NewString = AllocatePool (sizeof (CHAR16) * (Length1 + Length2 + 2));

    //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5");
    if (NewString) {
        //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 1");
        if ((*First != NULL) && (Length1 == 0)) {
            //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 1a 1");
            ReleasePtr (*First);
            *First = NULL;

            //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 1a 2");
        }

        //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 2");
        NewString[0] = L'\0';

        //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 3");
        if (*First != NULL) {
            //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 3a 1");
            StrCat (NewString, *First);

            //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 3a 2");
            if (AddChar) {
                //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 3a 2a 1");
                NewString[Length1] = AddChar;
                NewString[Length1 + 1] = '\0';

                //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 3a 2a 2");
            }

            //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 3a 3");
        }

        //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 4");
        if (Second != NULL) {
            //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 4a 1");
            StrCat (NewString, Second);

            //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 4a 2");
        }

        //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 5");
        ReleasePtr (*First);
        *First = NewString;

        //LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 5a 6");
    }
    LOG(5, LOG_LINE_FORENSIC, L"In MergeStrings ... 6 - END:- VOID ... Out String = '%s'", *First);
    LOG(5, LOG_BLANK_LINE_SEP, L"X");
} // VOID MergeStrings()

// As MergeStrings but does not repeat substrings.
VOID MergeUniqueStrings (
    IN OUT CHAR16 **First,
    IN CHAR16      *Second,
    CHAR16          AddChar
) {
    UINTN Length1 = 0, Length2 = 0;
    CHAR16* NewString;

    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 1 - START:- Merge '%s' into '%s'",
        Second ? Second : L"NULL",
        *First ? *First : L"NULL"
    );
    if (*First == NULL) {
        *First = StrDuplicate (Second);
        LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 1a 1 - END:- VOID ... Out String = '%s'", *First);
        LOG(5, LOG_BLANK_LINE_SEP, L"X");

        return;
    }

    //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 2");
    Length1 = StrLen (*First);

    //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 3");
    if (Second != NULL) {
        //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 3a 1");
        Length2 = StrLen (Second);

        //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 3a 2");
    }

    //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 4");
    NewString = AllocatePool (sizeof (CHAR16) * (Length1 + Length2 + 2));

    //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5");
    if (NewString) {
        //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 1");
        if ((*First != NULL) && (Length1 == 0)) {
            //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 1a 1");
            ReleasePtr (*First);
            *First = NULL;

            //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 1a 2");
        }

        //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 2");
        NewString[0] = L'\0';

        //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 3");
        if (*First != NULL) {
            //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 3a 1");
            StrCat (NewString, *First);

            //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 3a 2");
            if (AddChar) {
                //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 3a 2a 1");
                NewString[Length1] = AddChar;
                NewString[Length1 + 1] = '\0';

                //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 3a 2a 2");
            }

            //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 3a 3");
        }

        //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 4");
        if (Second != NULL) {
            //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 4a 1");
            BOOLEAN SkipMerge = FALSE;

            //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 4a 2");
            if (AddChar) {
                UINTN   i       = 0;
                CHAR16 *TestStr = NULL;

                LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 4a 2a 1 - WHILE LOOP:- START/ENTER");
                while (!SkipMerge &&
                    (TestStr = FindCommaDelimited (NewString, i++)) != NULL
                ) {
                    if (MyStrStr (TestStr, Second) != NULL) {
                        SkipMerge = TRUE;
                    }

                    MyFreePool (&TestStr);
                } // while
                LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 4a 2a 2 - WHILE LOOP:- END/EXIT");
            }

            //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 4a 3");
            if (!SkipMerge) {
                //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 4a 3a 1");
                StrCat (NewString, Second);

                //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 4a 3a 2");
            }
            else if (AddChar) {
                //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 4a 3b 1");
                // Remove AddChar if not merging this item
                NewString[Length1] = '\0';

                //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 4a 3b 2");
            }

            //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 4a 4");
        }

        //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 5");
        ReleasePtr (*First);
        *First = NewString;

        //LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 5a 6");
    }

    LOG(5, LOG_LINE_FORENSIC, L"In MergeUniqueStrings ... 6 - END:- VOID ... Out String = '%s'", *First);
    LOG(5, LOG_BLANK_LINE_SEP, L"X");
} // VOID MergeUniqueStrings()

// Similar to MergeStrings, but breaks the input string into word chunks and
// merges each word separately. Words are defined as string fragments separated
// by ' ', ':', '_', or '-'.
VOID MergeWords (
    CHAR16 **MergeTo,
    CHAR16  *InString,
    CHAR16   AddChar
) {
    CHAR16 *Temp, *Word, *p;
    BOOLEAN LineFinished = FALSE;

    if (!InString) {
        return;
    }

    Temp = Word = p = StrDuplicate (InString);
    if (Temp) {
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

        MyFreePool (&Temp);
    }
} // VOID MergeWords()

// As MergeWords, but only unique words are merged
VOID MergeUniqueWords (
    CHAR16 **MergeTo,
    CHAR16  *InString,
    CHAR16   AddChar
) {
    CHAR16 *Temp, *Word, *p;
    BOOLEAN LineFinished = FALSE;

    if (!InString) {
        return;
    }

    Temp = Word = p = StrDuplicate (InString);
    if (Temp) {
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

        MyFreePool (&Temp);
    }
} // VOID MergeUniqueWords()

// Replaces special characters in the input string with a space.
// '.', ',', '*', ':', '_', '-', '&', '#', '?', '(', ')', ';', '[', ']', '$', '{', '}', '+', '@', and '%'
CHAR16 * SanitiseString (
    CHAR16  *InString
) {
    CHAR16  *Temp, *Word, *p;
    CHAR16  *OutString    = NULL;
    BOOLEAN  LineFinished = FALSE;

    if (!InString) {
        return NULL;
    }

    Temp = Word = p = StrDuplicate (InString);
    if (Temp) {
        while (!LineFinished) {
            if ((*p == L'.') || (*p == L',') || (*p == L'*') ||
                (*p == L':') || (*p == L'_') || (*p == L'-') ||
                (*p == L'&') || (*p == L'#') || (*p == L'?') ||
                (*p == L'(') || (*p == L')') || (*p == L';') ||
                (*p == L'[') || (*p == L']') || (*p == L'$') ||
                (*p == L'{') || (*p == L'}') || (*p == L'+') ||
                (*p == L'@') || (*p == L'%') || (*p == L'\0')
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

        MyFreePool (&Temp);
    }

    if (!OutString) {
        OutString = StrDuplicate (InString);
    }

    return OutString;
} // CHAR16 * SanitiseString()

// Restrict 'TheString' to at most 'Limit' characters.
// Does this in two ways:
// - Locates stretches of two or more spaces and compresses
//   them down to one space.
// - Truncates TheString
// Returns TRUE if changes were made, FALSE otherwise
BOOLEAN LimitStringLength (
    CHAR16 *TheString,
    UINTN    Limit
) {
    UINTN     i;
    CHAR16    *SubString, *TempString;
    BOOLEAN   HasChanged   = FALSE;
    BOOLEAN   WasTruncated = FALSE;

    // SubString will be NULL or point WITHIN TheString
    SubString = MyStrStr (TheString, L"  ");
    while (SubString != NULL) {
        i = 0;
        while (SubString[i] == L' ')
            i++;
        if (i >= StrLen (SubString)) {
            SubString[0] = '\0';
            HasChanged = TRUE;
        }
        else {
            TempString = StrDuplicate (&SubString[i]);
            if (TempString == NULL) {
                // memory allocation problem ... abort to avoid potentially infinite loop!
                break;
            }
            else {
                StrCpy (&SubString[1], TempString);
                MyFreePool (&TempString);
                HasChanged = TRUE;
            }
        }

        SubString = MyStrStr (TheString, L"  ");
    } // while

    // Truncate if still too long.
    WasTruncated = TruncateString (TheString, Limit);
    if (!HasChanged) {
        HasChanged = WasTruncated;
    }

    return HasChanged;
} // BOOLEAN LimitStringLength()

// Truncate 'TheString' to 'Limit' characters if longer.
// Returns TRUE if truncated or FALSE otherwise
BOOLEAN TruncateString (
    CHAR16 *TheString,
    UINTN   Limit
) {
    BOOLEAN WasTruncated = FALSE;

    if (StrLen (TheString) > Limit) {
        TheString[Limit] = '\0';
        WasTruncated = TRUE;
    }

    return WasTruncated;
} // BOOLEAN TruncateString()

// Returns all the digits in the input string, including intervening
// non-digit characters. For instance, if InString is "foo-3.3.4-7.img",
// this function returns "3.3.4-7". The GlobalConfig.ExtraKernelVersionStrings
// variable specifies extra strings that may be treated as numbers. If
// InString contains no digits or ExtraKernelVersionStrings, the return value
// is NULL.
CHAR16 * FindNumbers (
    IN CHAR16 *InString
) {
    UINTN i = 0, StartOfElement, EndOfElement = 0, CopyLength;
    CHAR16 *Found = NULL, *ExtraFound = NULL, *LookFor;

    if (InString == NULL) {
        return NULL;
    }

    StartOfElement = StrLen (InString);

    // Find extra_kernel_version_strings
    while ((ExtraFound == NULL) &&
        (LookFor = FindCommaDelimited (GlobalConfig.ExtraKernelVersionStrings, i++))
    ) {
        if ((ExtraFound = MyStrStr (InString, LookFor))) {
            StartOfElement = ExtraFound - InString;
            EndOfElement = StartOfElement + StrLen (LookFor) - 1;
        }

        MyFreePool (&LookFor);
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
    IN CHAR16* String1,
    IN CHAR16* String2
) {
    UINTN Count = 0;
    if ((String1 == NULL) || (String2 == NULL)) {
        return 0;
    }

    while ((String1[Count] != L'\0') &&
        (String2[Count] != L'\0') &&
        (String1[Count] == String2[Count])
    ) {
        Count++;
    }

    return Count;
} // UINTN NumCharsInCommon()

// Find the #Index element (numbered from 0) in a comma-delimited string
// of elements.
// Returns the found element, or NULL if Index is out of range or InString
// is NULL. Note that the calling function is responsible for freeing the
// memory associated with the returned string pointer.
CHAR16 * FindCommaDelimited (
    IN CHAR16 *InString,
    IN UINTN   Index
) {
    UINTN    StartPos = 0, CurPos = 0, InLength;
    BOOLEAN  Found = FALSE;
    CHAR16   *FoundString = NULL;

    if (InString == NULL) {
        return NULL;
    }

    InLength = StrLen (InString);
    // After while() loop, StartPos marks start of item #Index
    while ((Index > 0) && (CurPos < InLength)) {
        if (InString[CurPos] == L',') {
            Index--;
            StartPos = CurPos + 1;
        }

        CurPos++;
    } // while

    // After while() loop, CurPos is one past the end of the element
    while ((CurPos < InLength) && (!Found)) {
        if (InString[CurPos] == L',') {
            Found = TRUE;
        }
        else {
            CurPos++;
        }
    } // while

    if (Index == 0) {
        FoundString = StrDuplicate (&InString[StartPos]);
    }

    if (FoundString != NULL) {
        FoundString[CurPos - StartPos] = 0;
    }

    return FoundString;
} // CHAR16 *FindCommaDelimited()

// Delete an individual element from a comma-separated value list.
// This function modifies the original *List string, but not the
// *ToDelete string!
// Returns TRUE if the item was deleted, FALSE otherwise.
BOOLEAN DeleteItemFromCsvList (
    CHAR16 *ToDelete,
    CHAR16 *List
) {
    CHAR16 *Found, *Comma;

    if ((ToDelete == NULL) || (List == NULL)) {
        return FALSE;
    }

    if ((Found = MyStrStr (List, ToDelete)) != NULL) {
        if ((Comma = MyStrStr (Found, L",")) == NULL) {
            // Found is final element
            if (Found == List) { // Found is ONLY element
                List[0] = L'\0';
            }
            else { // Delete the comma preceding Found.
                Found--;
                Found[0] = L'\0';
            } // if/else
        }
        else { // Found is NOT final element
            StrCpy (Found, &Comma[1]);
        }

        return TRUE;
    }

    return FALSE;
} // BOOLEAN DeleteItemFromCsvList()

// Returns TRUE if SmallString is an element in the comma-delimited List,
// FALSE otherwise. Performs comparison case-insensitively.
BOOLEAN IsIn (
    IN CHAR16 *SmallString,
    IN CHAR16 *List
) {
   UINTN     i = 0;
   BOOLEAN   Found = FALSE;
   CHAR16    *OneElement;

   if (!SmallString || !List) {
       return FALSE;
   }

    while (!Found && (OneElement = FindCommaDelimited (List, i++))) {
        if (MyStriCmp (OneElement, SmallString)) {
            Found = TRUE;
        }
        MyFreePool (&OneElement);
    } // while

   return Found;
} // BOOLEAN IsIn()

// Returns TRUE if any element of List can be found as a substring of
// BigString, FALSE otherwise. Performs comparisons case-insensitively.
BOOLEAN IsInSubstring (
    IN CHAR16 *BigString,
    IN CHAR16 *List
) {
    UINTN   i = 0, ElementLength;
    BOOLEAN Found = FALSE;
    CHAR16  *OneElement;

    if (!BigString || !List) {
        return FALSE;
    }

    while (!Found && (OneElement = FindCommaDelimited (List, i++))) {
        ElementLength = StrLen(OneElement);
        if ((ElementLength <= StrLen(BigString)) &&
            (ElementLength > 0) &&
            (StriSubCmp (OneElement, BigString))
        ) {
            Found = TRUE;
        } // if

        if ((ElementLength <= StrLen (BigString)) &&
            (StriSubCmp (OneElement, BigString))
        ) {
            Found = TRUE;
        }
        MyFreePool (&OneElement);
    } // while

    return Found;
} // BOOLEAN IsSubstringIn()

// Replace *SearchString in **MainString with *ReplString -- but if *SearchString
// is preceded by "%", instead remove that character.
// Returns TRUE if replacement was done, FALSE otherwise.
BOOLEAN ReplaceSubstring (
    IN OUT CHAR16 **MainString,
    IN     CHAR16  *SearchString,
    IN     CHAR16  *ReplString
) {
    BOOLEAN WasReplaced = FALSE;
    CHAR16 *FoundSearchString, *NewString, *EndString;

    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    LOG(5, LOG_LINE_FORENSIC,
        L"In ReplaceSubstring ... 1 - START:- Replace '%s' with '%s' in '%s'",
        SearchString ? SearchString : L"NULL",
        ReplString   ? ReplString   : L"NULL",
        *MainString  ? *MainString  : L"NULL"
    );
    if ((*MainString == NULL) || (SearchString == NULL) || (ReplString == NULL)) {
        LOG(5, LOG_LINE_FORENSIC, L"In ReplaceSubstring ... 1a - END:- NULL INPUT");
        LOG(5, LOG_BLANK_LINE_SEP, L"X");
        return FALSE;
    }
    FoundSearchString = MyStrStr (*MainString, SearchString);

    //LOG(5, LOG_LINE_FORENSIC, L"In ReplaceSubstring ... 2");
    if (FoundSearchString) {
        //LOG(5, LOG_LINE_FORENSIC, L"In ReplaceSubstring ... 2a 1");
        NewString = AllocateZeroPool (sizeof (CHAR16) * StrLen(*MainString));

        //LOG(5, LOG_LINE_FORENSIC, L"In ReplaceSubstring ... 2a 2");
        if (NewString) {
            //LOG(5, LOG_LINE_FORENSIC, L"In ReplaceSubstring ... 2a 2a 1");
            EndString = &(FoundSearchString[StrLen (SearchString)]);
            FoundSearchString[0] = L'\0';

            //LOG(5, LOG_LINE_FORENSIC, L"In ReplaceSubstring ... 2a 2a 2");
            if ((FoundSearchString > *MainString) && (FoundSearchString[-1] == L'%')) {
                //LOG(5, LOG_LINE_FORENSIC, L"In ReplaceSubstring ... 2a 2a 2a 1");
                FoundSearchString[-1] = L'\0';
                ReplString = SearchString;
            }

            //LOG(5, LOG_LINE_FORENSIC, L"In ReplaceSubstring ... 2a 2a 3");
            StrCpy (NewString, *MainString);

            //LOG(5, LOG_LINE_FORENSIC, L"In ReplaceSubstring ... 2a 2a 4");
            MergeStrings (&NewString, ReplString, L'\0');

            //LOG(5, LOG_LINE_FORENSIC, L"In ReplaceSubstring ... 2a 2a 5");
            MergeStrings (&NewString, EndString, L'\0');

            //LOG(5, LOG_LINE_FORENSIC, L"In ReplaceSubstring ... 2a 2a 6");
            MyFreePool (&MainString);
            *MainString = NewString;

            //LOG(5, LOG_LINE_FORENSIC, L"In ReplaceSubstring ... 2a 2a 7 - WasReplaced = TRUE");
            WasReplaced = TRUE;
        }
        //LOG(5, LOG_LINE_FORENSIC, L"In ReplaceSubstring ... 2a 3");
    }

    LOG(5, LOG_LINE_FORENSIC,
        L"In ReplaceSubstring ... 3 END - return BOOLEAN WasReplaced:- '%s'",
        WasReplaced ? L"TRUE" : L"FALSE"
    );
    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    return WasReplaced;
} // BOOLEAN ReplaceSubstring()

// Returns TRUE if *Input contains nothing but valid hexadecimal characters,
// FALSE otherwise. Note that a leading "0x" is NOT acceptable in the input!
BOOLEAN IsValidHex (
    CHAR16 *Input
) {
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
UINT64 StrToHex (
    CHAR16 *Input,
    UINTN   Pos,
    UINTN   NumChars
) {
    UINT64 retval = 0x00;
    UINTN  NumDone = 0, InputLength;
    CHAR16 a;

    if ((Input == NULL) || (NumChars == 0) || (NumChars > 16)) {
        return 0;
    }

    InputLength = StrLen (Input);
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
    BOOLEAN retval = TRUE;
    CHAR16  a;

    if (UnknownString == NULL) {
        return FALSE;
    }

    Length = StrLen (UnknownString);
    if (Length != 36) {
        retval = FALSE;
    }
    else {
        for (i = 0; i < Length; i++) {
            a = UnknownString[i];
            if ((i == 8) || (i == 13) || (i == 18) || (i == 23)) {
                if (a != L'-') {
                    retval = FALSE;
                    break;
                }
            }
            // DA-TAG: Condition below can never be met
            //         Comment out until review
            //else if (((a < L'a') || (a > L'f')) &&
            //    ((a < L'A') || (a > L'F')) &&
            //    ((a < L'0') && (a > L'9'))
            //) {
            //    retval = FALSE;
            //    break;
            //}
        } // for
    }

    return retval;
} // BOOLEAN IsGuid()

// Return the GUID as a string, suitable for display to the user. Note that the calling
// function is responsible for freeing the allocated memory.
CHAR16 * GuidAsString (
    EFI_GUID *GuidData
) {
    CHAR16 *TheString;

    TheString = AllocateZeroPool(42 * sizeof (CHAR16));

    if (GuidData && (TheString != 0)) {
        SPrint (
            TheString, 82, L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            (UINTN) GuidData->Data1,    (UINTN) GuidData->Data2,    (UINTN) GuidData->Data3,
            (UINTN) GuidData->Data4[0], (UINTN) GuidData->Data4[1], (UINTN )GuidData->Data4[2],
            (UINTN) GuidData->Data4[3], (UINTN) GuidData->Data4[4], (UINTN) GuidData->Data4[5],
            (UINTN) GuidData->Data4[6], (UINTN) GuidData->Data4[7]
        );
    }

    return TheString;
} // GuidAsString(EFI_GUID *GuidData)

EFI_GUID StringAsGuid (
    CHAR16 *InString
) {
    EFI_GUID  Guid = NULL_GUID_VALUE;

    if (!IsGuid(InString)) {
        return Guid;
    }

    Guid.Data1    = (UINT32) StrToHex(InString,  0, 8);
    Guid.Data2    = (UINT16) StrToHex(InString,  9, 4);
    Guid.Data3    = (UINT16) StrToHex(InString, 14, 4);
    Guid.Data4[0] =  (UINT8) StrToHex(InString, 19, 2);
    Guid.Data4[1] =  (UINT8) StrToHex(InString, 21, 2);
    Guid.Data4[2] =  (UINT8) StrToHex(InString, 23, 2);
    Guid.Data4[3] =  (UINT8) StrToHex(InString, 26, 2);
    Guid.Data4[4] =  (UINT8) StrToHex(InString, 28, 2);
    Guid.Data4[5] =  (UINT8) StrToHex(InString, 30, 2);
    Guid.Data4[6] =  (UINT8) StrToHex(InString, 32, 2);
    Guid.Data4[7] =  (UINT8) StrToHex(InString, 34, 2);

    return Guid;
} // EFI_GUID StringAsGuid()

// Returns the current time as a string in 24-hour format; e.g., 14:03:17.
// Discards date portion, since for our purposes, we really do not care.
// Calling function is responsible for releasing returned string.
CHAR16 * GetTimeString(VOID) {
    CHAR16     *TimeStr = NULL;
    EFI_TIME    CurrentTime;
    EFI_STATUS  Status = EFI_SUCCESS;

    Status = REFIT_CALL_2_WRAPPER(ST->RuntimeServices->GetTime, &CurrentTime, NULL);
    if (EFI_ERROR(Status)) {
        TimeStr = PoolPrint(L"unknown time");
    }
    else {
        TimeStr = PoolPrint(
            L"%02d:%02d:%02d",
            CurrentTime.Hour,
            CurrentTime.Minute,
            CurrentTime.Second
        );
    }
    return TimeStr;
} // CHAR16 *GetTimeString()

// Delete the STRING_LIST pointed to by *StringList.
VOID DeleteStringList (
    STRING_LIST *StringList
) {
    STRING_LIST *Previous = NULL;
    STRING_LIST *Current  = StringList;

    while (Current != NULL) {
        MyFreePool (&(Current->Value));
        Previous = Current;
        Current  = Current->Next;
        MyFreePool (&Previous);
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
    UINTN   UnicodeStringSize;

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
        }
        *UnicodeStringWalker = L'\0';
    }

    return UnicodeString;
} // CHAR16 * MyAsciiStrCopyToUnicode()


// Convert Unicode String To Ascii String
VOID MyUnicodeStrToAsciiStr (
    IN  CHAR16  *StrCHAR16,
    OUT CHAR8    ArrCHAR8[256]
) {
    UINTN k = -1;
    UINTN i = -1;

    // Get the number of characters (plus null terminator) in StrCHAR16
    do {
        // increment index
        k = k + 1;
    } while (StrCHAR16[k] != L'\0');

    // Move StrCHAR16 characters to ArrCHAR8
    do {
        // increment index
        i = i + 1;

        if (i > 254) {
            // prevent overflow
            ArrCHAR8[i]  = L'\0';
            break;
        }
        else {
            // convert to single byte character and assign to array
            CHAR8 character  = StrCHAR16[i];
            ArrCHAR8[i]      = character;
        }
    } while (i < k);
} // VOID MyUnicodeStrToAsciiStr()

VOID MyUnicodeFilterString (
    IN OUT CHAR16   *String,
    IN     BOOLEAN   SingleLine
) {
    while (*String != L'\0') {
        if ((*String & 0x7FU) != *String) {
            // Remove all unicode characters.
            *String = L'_';
        }
        else if (SingleLine && (*String == L'\r' || *String == L'\n')) {
            // Stop after printing one line.
            *String = L'\0';

            break;
        }
        else if (*String < 0x20 || *String == 0x7F) {
            // Drop all unprintable spaces but space including tabs.
            *String = L'_';
        }

        ++String;
    }
} // VOID MyUnicodeFilterString()
