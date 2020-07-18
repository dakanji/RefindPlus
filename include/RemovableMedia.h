/*
 * Copyright (c) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Module Name:
 *
 *	RemovableMedia.h
 *
 * Abstract:
 *
 *	Protocol interface for any device that supports removable media (i.e. optical drives).
 *

Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple
Computer, Inc. ("Apple") in consideration of your agreement to the
following terms, and your use, installation, modification or
redistribution of this Apple software constitutes acceptance of these
terms.  If you do not agree with these terms, please do not use,
install, modify or redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and
subject to these terms, Apple grants you a personal, non-exclusive
license, under Apple's copyrights in this original Apple software (the
"Apple Software"), to use, reproduce, modify and redistribute the Apple
Software, with or without modifications, in source and/or binary forms;
provided that if you redistribute the Apple Software in its entirety and
without modifications, you must retain this notice and the following
text and disclaimers in all such redistributions of the Apple Software. 
Neither the name, trademarks, service marks or logos of Apple Computer,
Inc. may be used to endorse or promote products derived from the Apple
Software without specific prior written permission from Apple.  Except
as expressly stated in this notice, no other rights or licenses, express
or implied, are granted by Apple herein, including but not limited to
any patent rights that may be infringed by your derivative works or by
other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

Copyright © 2006 Apple Computer, Inc., All Rights Reserved

 */

#ifndef _APPLE_REMOVABLE_MEDIA_H
#define _APPLE_REMOVABLE_MEDIA_H

//
// Global Id for Removable_Media Interface
//
// {2EA9743A-23D9-425e-872C-F615AA195788}
#define APPLE_REMOVABLE_MEDIA_PROTOCOL_GUID \
  { \
    0x2ea9743a, 0x23d9, 0x425e, { 0x87, 0x2c, 0xf6, 0x15, 0xaa, 0x19, 0x57, 0x88 } \
  }

#define	APPLE_REMOVABLE_MEDIA_PROTOCOL_REVISION		0x00000001

// EFI_FORWARD_DECLARATION (APPLE_REMOVABLE_MEDIA_PROTOCOL);
typedef struct _APPLE_REMOVABLE_MEDIA_PROTOCOL APPLE_REMOVABLE_MEDIA_PROTOCOL;

//
// Eject
//
typedef
EFI_STATUS
(EFIAPI *APPLE_REMOVABLE_MEDIA_EJECT) (
  IN APPLE_REMOVABLE_MEDIA_PROTOCOL				* This
  )
/*++

  Routine Description:
    Eject removable media from drive (such as a CD/DVD).

  Arguments:
    This                 - Protocol instance pointer.

  Returns:
    EFI_SUCCESS          - The media was ejected.

--*/
;

//
// Inject
//
typedef
EFI_STATUS
(EFIAPI *APPLE_REMOVABLE_MEDIA_INJECT) (
  IN APPLE_REMOVABLE_MEDIA_PROTOCOL				* This
  )
/*++

  Routine Description:
    Inject removable media into drive (such as a CD/DVD).

  Arguments:
    This                 - Protocol instance pointer.

  Returns:
    EFI_SUCCESS          - The media was injected.

--*/
;

//
// Allow media to be ejected
//
typedef
EFI_STATUS
(EFIAPI *APPLE_REMOVABLE_MEDIA_ALLOW_REMOVAL) (
  IN APPLE_REMOVABLE_MEDIA_PROTOCOL				* This
  )
/*++

  Routine Description:
    Allow the media to be removed from the drive.

  Arguments:
    This                 - Protocol instance pointer.

  Returns:
    EFI_SUCCESS          - The media can now be removed.
	EFI_UNSUPPORTED      - The media cannot be removed.

--*/
;

//
// Prevent media from being ejected
//
typedef
EFI_STATUS
(EFIAPI *APPLE_REMOVABLE_MEDIA_PREVENT_REMOVAL) (
  IN APPLE_REMOVABLE_MEDIA_PROTOCOL				* This
  )
/*++

  Routine Description:
    Prevent the media from being removed from the drive.

  Arguments:
    This                 - Protocol instance pointer.

  Returns:
    EFI_SUCCESS          - The drive is locked, and the media cannot be removed.
	EFI_UNSUPPORTED      - The drive cannot be locked.

--*/
;

//
// Detect state of removable media tray
//
typedef
EFI_STATUS
(EFIAPI *APPLE_REMOVABLE_MEDIA_DETECT_TRAY_STATE) (
  IN APPLE_REMOVABLE_MEDIA_PROTOCOL				* This,
  OUT BOOLEAN									* TrayOpen
  )
/*++

  Routine Description:
    Get the status of the drive tray.

  Arguments:
    This                 - Protocol instance pointer.
	TrayOpen             - Status of the drive tray.

  Returns:
    EFI_SUCCESS          - The status has been returned in TrayOpen.

--*/
;

//
// Protocol definition
//
struct _APPLE_REMOVABLE_MEDIA_PROTOCOL {
  UINT32										Revision;
  BOOLEAN										RemovalAllowed;
  APPLE_REMOVABLE_MEDIA_EJECT					Eject;
  APPLE_REMOVABLE_MEDIA_INJECT					Inject;
  APPLE_REMOVABLE_MEDIA_ALLOW_REMOVAL			AllowRemoval;
  APPLE_REMOVABLE_MEDIA_PREVENT_REMOVAL			PreventRemoval;
  APPLE_REMOVABLE_MEDIA_DETECT_TRAY_STATE		DetectTrayState;

};

//extern EFI_GUID gAppleRemovableMediaProtocolGuid;

#endif

