/** @file
    Forward Declaration for Refind Integration.
**/

#ifndef __OCREFIT_LIB_H__
    #define __OCREFIT_LIB_H__

    #ifndef STATIC_ASSERT
    #define STATIC_ASSERT _Static_assert
    #endif

    #ifndef MAX_INT8
    #define MAX_INT8    ((INT8)0x7F)
    #endif

    #ifndef MAX_UINT8
    #define MAX_UINT8   ((UINT8)0xFF)
    #endif

    #ifndef MAX_INT16
    #define MAX_INT16   ((INT16)0x7FFF)
    #endif

    #ifndef MAX_UINT16
    #define MAX_UINT16  ((UINT16)0xFFFF)
    #endif

    #ifndef MAX_INT32
    #define MAX_INT32   ((INT32)0x7FFFFFFF)
    #endif

    #ifndef MAX_UINT32
    #define MAX_UINT32  ((UINT32)0xFFFFFFFF)
    #endif

    #ifndef MAX_INT64
    #define MAX_INT64   ((INT64)0x7FFFFFFFFFFFFFFFULL)
    #endif

    #ifndef MAX_UINT64
    #define MAX_UINT64  ((UINT64)0xFFFFFFFFFFFFFFFFULL)
    #endif

    #ifndef MIN_INT8
    #define MIN_INT8   (((INT8)  -127) - 1)
    #endif

    #ifndef MIN_INT16
    #define MIN_INT16  (((INT16) -32767) - 1)
    #endif

    #ifndef MIN_INT32
    #define MIN_INT32  (((INT32) -2147483647) - 1)
    #endif

    #ifndef MIN_INT64
    #define MIN_INT64  (((INT64) -9223372036854775807LL) - 1)
    #endif

    CHAR16 EFIAPI CharToUpper (IN CHAR16 Char);
#endif // __OCREFIT_LIB_H__
