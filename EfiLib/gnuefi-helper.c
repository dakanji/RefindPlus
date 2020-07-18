/*
 * EfiLib/gnuefi-helper.c
 * GNU-EFI support functions
 *
 * Borrowed from the TianoCore EDK II, with modifications by Rod Smith
 *
 * Copyright (c) 2004 - 2011, Intel Corporation. All rights reserved.<BR>
 * This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the BSD License
 * which accompanies this distribution.  The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php
 * 
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 *
 */

#include "gnuefi-helper.h"
#include "DevicePathUtilities.h"
#include "refit_call_wrapper.h"
#include "LegacyBios.h"

EFI_GUID gEfiDevicePathUtilitiesProtocolGuid = { 0x09576E91, 0x6D3F, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }};
EFI_GUID gEfiLegacyBiosProtocolGuid = { 0xdb9a1e3d, 0x45cb, 0x4abb, { 0x85, 0x3b, 0xe5, 0x38, 0x7f, 0xdb, 0x2e, 0x2d }};

/**
  Convert a Null-terminated Unicode string to a Null-terminated 
  ASCII string and returns the ASCII string.

  This function converts the content of the Unicode string Source 
  to the ASCII string Destination by copying the lower 8 bits of 
  each Unicode character. It returns Destination. The function terminates 
  the ASCII string Destination  by appending a Null-terminator character 
  at the end. The caller is responsible to make sure Destination points 
  to a buffer with size equal or greater than (StrLen (Source) + 1) in bytes.

  If Destination is NULL, then ASSERT().
  If Source is NULL, then ASSERT().
  If Source is not aligned on a 16-bit boundary, then ASSERT().
  If Source and Destination overlap, then ASSERT().

  If any Unicode characters in Source contain non-zero value in 
  the upper 8 bits, then ASSERT().

  If PcdMaximumUnicodeStringLength is not zero, and Source contains 
  more than PcdMaximumUnicodeStringLength Unicode characters not including 
  the Null-terminator, then ASSERT().

  If PcdMaximumAsciiStringLength is not zero, and Source contains more 
  than PcdMaximumAsciiStringLength Unicode characters not including the 
  Null-terminator, then ASSERT().

  @param  Source        Pointer to a Null-terminated Unicode string.
  @param  Destination   Pointer to a Null-terminated ASCII string.

  @reture Destination

**/
CHAR8 *
UnicodeStrToAsciiStr (
  IN      CHAR16              *Source,
  OUT     CHAR8               *Destination
  )
{
  ASSERT (Destination != NULL);
  ASSERT (Source != NULL);
  ASSERT (((UINTN) Source & 0x01) == 0);

  //
  // Source and Destination should not overlap
  //
  ASSERT ((UINTN) ((CHAR16 *) Destination -  Source) > StrLen (Source));
  ASSERT ((UINTN) ((CHAR8 *) Source - Destination) > StrLen (Source));

//   //
//   // If PcdMaximumUnicodeStringLength is not zero,
//   // length of Source should not more than PcdMaximumUnicodeStringLength
//   //
//   if (PcdGet32 (PcdMaximumUnicodeStringLength) != 0) {
//     ASSERT (StrLen (Source) < PcdGet32 (PcdMaximumUnicodeStringLength));
//   }

  while (*Source != '\0') {
    //
    // If any Unicode characters in Source contain 
    // non-zero value in the upper 8 bits, then ASSERT().
    //
    ASSERT (*Source < 0x100);
    *(Destination++) = (CHAR8) *(Source++);
  }

  *Destination = '\0';

  return Destination;
}

/**
  Returns the length of a Null-terminated ASCII string.

  This function returns the number of ASCII characters in the Null-terminated
  ASCII string specified by String.

  If String is NULL, then ASSERT().
  If PcdMaximumAsciiStringLength is not zero and String contains more than
  PcdMaximumAsciiStringLength ASCII characters not including the Null-terminator,
  then ASSERT().

  @param  String  Pointer to a Null-terminated ASCII string.

  @return The length of String.

**/
UINTN
AsciiStrLen (
  IN      CONST CHAR8               *String
  )
{
  UINTN                             Length;

  ASSERT (String != NULL);

  for (Length = 0; *String != '\0'; String++, Length++) {
//     //
//     // If PcdMaximumUnicodeStringLength is not zero,
//     // length should not more than PcdMaximumUnicodeStringLength
//     //
//     if (PcdGet32 (PcdMaximumAsciiStringLength) != 0) {
//       ASSERT (Length < PcdGet32 (PcdMaximumAsciiStringLength));
//     }
  }
  return Length;
}

/**
  Determine whether a given device path is valid.
  If DevicePath is NULL, then ASSERT().

  @param  DevicePath  A pointer to a device path data structure.
  @param  MaxSize     The maximum size of the device path data structure.

  @retval TRUE        DevicePath is valid.
  @retval FALSE       The length of any node node in the DevicePath is less
                      than sizeof (EFI_DEVICE_PATH_PROTOCOL).
  @retval FALSE       If MaxSize is not zero, the size of the DevicePath
                      exceeds MaxSize.
  @retval FALSE       If PcdMaximumDevicePathNodeCount is not zero, the node
                      count of the DevicePath exceeds PcdMaximumDevicePathNodeCount.
**/
BOOLEAN
EFIAPI
IsDevicePathValid (
  IN CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath,
  IN       UINTN                    MaxSize
  )
{
//  UINTN Count;
  UINTN Size;
  UINTN NodeLength;

  ASSERT (DevicePath != NULL);

  for (/* Count = 0, */ Size = 0; !IsDevicePathEnd (DevicePath); DevicePath = NextDevicePathNode (DevicePath)) {
    NodeLength = DevicePathNodeLength (DevicePath);
    if (NodeLength < sizeof (EFI_DEVICE_PATH_PROTOCOL)) {
      return FALSE;
    }

    if (MaxSize > 0) {
      Size += NodeLength;
      if (Size + END_DEVICE_PATH_LENGTH > MaxSize) {
        return FALSE;
      }
    }

//     if (PcdGet32 (PcdMaximumDevicePathNodeCount) > 0) {
//       Count++;
//       if (Count >= PcdGet32 (PcdMaximumDevicePathNodeCount)) {
//         return FALSE;
//       }
//     }
  }

  //
  // Only return TRUE when the End Device Path node is valid.
  //
  return (BOOLEAN) (DevicePathNodeLength (DevicePath) == END_DEVICE_PATH_LENGTH);
}

/**
  Returns the size of a device path in bytes.

  This function returns the size, in bytes, of the device path data structure 
  specified by DevicePath including the end of device path node.
  If DevicePath is NULL or invalid, then 0 is returned.

  @param  DevicePath  A pointer to a device path data structure.

  @retval 0           If DevicePath is NULL or invalid.
  @retval Others      The size of a device path in bytes.

**/
UINTN
EFIAPI
GetDevicePathSize (
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  CONST EFI_DEVICE_PATH_PROTOCOL  *Start;

  if (DevicePath == NULL) {
    return 0;
  }

  if (!IsDevicePathValid (DevicePath, 0)) {
    return 0;
  }

  //
  // Search for the end of the device path structure
  //
  Start = DevicePath;
  while (!IsDevicePathEnd (DevicePath)) {
    DevicePath = NextDevicePathNode (DevicePath);
  }

  //
  // Compute the size and add back in the size of the end device path structure
  //
  return ((UINTN) DevicePath - (UINTN) Start) + DevicePathNodeLength (DevicePath);
}

/**
  Creates a copy of the current device path instance and returns a pointer to the next device path
  instance.

  This function creates a copy of the current device path instance. It also updates 
  DevicePath to point to the next device path instance in the device path (or NULL 
  if no more) and updates Size to hold the size of the device path instance copy.
  If DevicePath is NULL, then NULL is returned.
  If DevicePath points to a invalid device path, then NULL is returned.
  If there is not enough memory to allocate space for the new device path, then 
  NULL is returned.  
  The memory is allocated from EFI boot services memory. It is the responsibility 
  of the caller to free the memory allocated.
  If Size is NULL, then ASSERT().
 
  @param  DevicePath                 On input, this holds the pointer to the current 
                                     device path instance. On output, this holds 
                                     the pointer to the next device path instance 
                                     or NULL if there are no more device path
                                     instances in the device path pointer to a 
                                     device path data structure.
  @param  Size                       On output, this holds the size of the device 
                                     path instance, in bytes or zero, if DevicePath 
                                     is NULL.

  @return A pointer to the current device path instance.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
GetNextDevicePathInstance (
  IN OUT EFI_DEVICE_PATH_PROTOCOL    **DevicePath,
  OUT UINTN                          *Size
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *DevPath;
  EFI_DEVICE_PATH_PROTOCOL  *ReturnValue;
  UINT8                     Temp;

  ASSERT (Size != NULL);

  if (DevicePath == NULL || *DevicePath == NULL) {
    *Size = 0;
    return NULL;
  }

  if (!IsDevicePathValid (*DevicePath, 0)) {
    return NULL;
  }

  //
  // Find the end of the device path instance
  //
  DevPath = *DevicePath;
  while (!IsDevicePathEndType (DevPath)) {
    DevPath = NextDevicePathNode (DevPath);
  }

  //
  // Compute the size of the device path instance
  //
  *Size = ((UINTN) DevPath - (UINTN) (*DevicePath)) + sizeof (EFI_DEVICE_PATH_PROTOCOL);
 
  //
  // Make a copy and return the device path instance
  //
  Temp              = DevPath->SubType;
  DevPath->SubType  = END_ENTIRE_DEVICE_PATH_SUBTYPE;
  ReturnValue       = DuplicateDevicePath (*DevicePath);
  DevPath->SubType  = Temp;

  //
  // If DevPath is the end of an entire device path, then another instance
  // does not follow, so *DevicePath is set to NULL.
  //
  if (DevicePathSubType (DevPath) == END_ENTIRE_DEVICE_PATH_SUBTYPE) {
    *DevicePath = NULL;
  } else {
    *DevicePath = NextDevicePathNode (DevPath);
  }

  return ReturnValue;
}
