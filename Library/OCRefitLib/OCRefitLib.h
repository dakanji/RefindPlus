/** @file
    Provides simple log services to memory buffer.
**/

#ifndef __OCREFIT_LIB_H__
#define __OCREFIT_LIB_H__


/// Forward Declaration for Refind Integration
#define STATIC_ASSERT _Static_assert
CHAR16 EFIAPI CharToUpper (IN CHAR16 Char);
#define MAX_INT8    ((INT8)0x7F)
#define MAX_UINT8   ((UINT8)0xFF)
#define MAX_INT16   ((INT16)0x7FFF)
#define MAX_UINT16  ((UINT16)0xFFFF)
#define MAX_INT32   ((INT32)0x7FFFFFFF)
#define MAX_UINT32  ((UINT32)0xFFFFFFFF)
#define MAX_INT64   ((INT64)0x7FFFFFFFFFFFFFFFULL)
#define MAX_UINT64  ((UINT64)0xFFFFFFFFFFFFFFFFULL)
#define MIN_INT8   (((INT8)  -127) - 1)
#define MIN_INT16  (((INT16) -32767) - 1)
#define MIN_INT32  (((INT32) -2147483647) - 1)
#define MIN_INT64  (((INT64) -9223372036854775807LL) - 1)


#endif // __OCREFIT_LIB_H__
