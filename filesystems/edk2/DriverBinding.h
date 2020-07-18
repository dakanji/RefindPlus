/*++

Copyright (c) 2004, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:

    DriverBinding.h
    
Abstract:

    EFI ControllerHandle Driver Protocol

Revision History

--*/

/*
 * rEFInd NOTE: This file is included only when compiling with GNU-EFI,
 * which has not traditionally provided the definitions supplied here.
 * Unfortunately, recent (ca. 3.0.5) versions of GNU-EFI have added
 * SOME of these functions to an existing header file, creating problems
 * when trying to maintain compatibility with multiple GNU-EFI versions.
 * I've therefore renamed the relevant defines, types, and functions,
 * both here and in fsw_efi.c; and included a define to match the only
 * used name (REFIND_EFI_DRIVER_BINDING_PROTOCOL) to the traditional
 * name (EFI_DRIVER_BINDING_PROTOCOL) in fsw_efi.c for compiling with
 * TianoCore.
 */

#ifndef _EFI_DRIVER_BINDING_H_
#define _EFI_DRIVER_BINDING_H_

#include <efidevp.h>

#define REFIND_EFI_DRIVER_BINDING_PROTOCOL_GUID \
  { \
    0x18a031ab, 0xb443, 0x4d1a, {0xa5, 0xc0, 0xc, 0x9, 0x26, 0x1e, 0x9f, 0x71} \
  }

#define EFI_FORWARD_DECLARATION(x) typedef struct _##x x

EFI_FORWARD_DECLARATION (REFIND_EFI_DRIVER_BINDING_PROTOCOL);

// Begin included from DevicePath.h....

///
/// Device Path guid definition for backward-compatible with EFI1.1.
///
//#define DEVICE_PATH_PROTOCOL  EFI_DEVICE_PATH_PROTOCOL_GUID

#pragma pack(1)

/**
  This protocol can be used on any device handle to obtain generic path/location
  information concerning the physical device or logical device. If the handle does
  not logically map to a physical device, the handle may not necessarily support
  the device path protocol. The device path describes the location of the device
  the handle is for. The size of the Device Path can be determined from the structures
  that make up the Device Path.
**/
typedef struct {
  UINT8 Type;       ///< 0x01 Hardware Device Path.
                    ///< 0x02 ACPI Device Path.
                    ///< 0x03 Messaging Device Path.
                    ///< 0x04 Media Device Path.
                    ///< 0x05 BIOS Boot Specification Device Path.
                    ///< 0x7F End of Hardware Device Path.

  UINT8 SubType;    ///< Varies by Type
                    ///< 0xFF End Entire Device Path, or
                    ///< 0x01 End This Instance of a Device Path and start a new
                    ///< Device Path.

  UINT8 Length[2];  ///< Specific Device Path data. Type and Sub-Type define
                    ///< type of data. Size of data is included in Length.

} REFIND_EFI_DEVICE_PATH_PROTOCOL;

#pragma pack()


// End included from DevicePath.h

typedef
EFI_STATUS
(EFI_FUNCTION EFIAPI *EFI_DRIVER_BINDING_SUPPORTED) (
  IN REFIND_EFI_DRIVER_BINDING_PROTOCOL            * This,
  IN EFI_HANDLE                             ControllerHandle,
  IN REFIND_EFI_DEVICE_PATH_PROTOCOL               * RemainingDevicePath OPTIONAL
  )
/*++

  Routine Description:
    Test to see if this driver supports ControllerHandle. 

  Arguments:
    This                - Protocol instance pointer.
    ControllerHandle    - Handle of device to test
    RemainingDevicePath - Optional parameter use to pick a specific child 
                          device to start.

  Returns:
    EFI_SUCCESS         - This driver supports this device
    EFI_ALREADY_STARTED - This driver is already running on this device
    other               - This driver does not support this device

--*/
;

typedef
EFI_STATUS
(EFI_FUNCTION EFIAPI *EFI_DRIVER_BINDING_START) (
  IN REFIND_EFI_DRIVER_BINDING_PROTOCOL            * This,
  IN EFI_HANDLE                             ControllerHandle,
  IN REFIND_EFI_DEVICE_PATH_PROTOCOL               * RemainingDevicePath OPTIONAL
  )
/*++

  Routine Description:
    Start this driver on ControllerHandle.

  Arguments:
    This                - Protocol instance pointer.
    ControllerHandle    - Handle of device to bind driver to
    RemainingDevicePath - Optional parameter use to pick a specific child 
                          device to start.

  Returns:
    EFI_SUCCESS         - This driver is added to ControllerHandle
    EFI_ALREADY_STARTED - This driver is already running on ControllerHandle
    other               - This driver does not support this device

--*/
;

typedef
EFI_STATUS
(EFI_FUNCTION EFIAPI *EFI_DRIVER_BINDING_STOP) (
  IN REFIND_EFI_DRIVER_BINDING_PROTOCOL            * This,
  IN  EFI_HANDLE                            ControllerHandle,
  IN  UINTN                                 NumberOfChildren,
  IN  EFI_HANDLE                            * ChildHandleBuffer
  )
/*++

  Routine Description:
    Stop this driver on ControllerHandle.

  Arguments:
    This              - Protocol instance pointer.
    ControllerHandle  - Handle of device to stop driver on 
    NumberOfChildren  - Number of Handles in ChildHandleBuffer. If number of 
                        children is zero stop the entire bus driver.
    ChildHandleBuffer - List of Child Handles to Stop.

  Returns:
    EFI_SUCCESS         - This driver is removed ControllerHandle
    other               - This driver was not removed from this device

--*/
;

//
// Interface structure for the ControllerHandle Driver Protocol
//
struct _REFIND_EFI_DRIVER_BINDING_PROTOCOL {
  EFI_DRIVER_BINDING_SUPPORTED  Supported;
  EFI_DRIVER_BINDING_START      Start;
  EFI_DRIVER_BINDING_STOP       Stop;
  UINT32                        Version;
  EFI_HANDLE                    ImageHandle;
  EFI_HANDLE                    DriverBindingHandle;
};

extern EFI_GUID gEfiDriverBindingProtocolGuid;

#endif
