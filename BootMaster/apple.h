/*
 * BootMaster/apple.h
 *
 * Copyright (c) 2015 Roderick W. Smith
 *
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
 *
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#ifndef __APPLE_H_
#define __APPLE_H_

#ifndef APPLE_FRAMEBUFFER_INFO_H
#define APPLE_FRAMEBUFFER_INFO_H

// Apple Framebuffer Info GUID
#define APPLE_FRAMEBUFFER_INFO_PROTOCOL_GUID \
    { 0xE316E100, 0x0751, 0x4C49, \
    { 0x90, 0x56, 0x48, 0x6C, 0x7E, 0x47, 0x29, 0x03 } }

typedef struct APPLE_FRAMEBUFFER_INFO_PROTOCOL_ APPLE_FRAMEBUFFER_INFO_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *APPLE_FRAMEBUFFER_INFO_GET_INFO) (
    IN   APPLE_FRAMEBUFFER_INFO_PROTOCOL  *This,
    OUT  EFI_PHYSICAL_ADDRESS             *FramebufferBase,
    OUT  UINT32                           *FramebufferSize,
    OUT  UINT32                           *ScreenRowBytes,
    OUT  UINT32                           *ScreenWidth,
    OUT  UINT32                           *ScreenHeight,
    OUT  UINT32                           *ScreenDepth
);

struct APPLE_FRAMEBUFFER_INFO_PROTOCOL_ {
    APPLE_FRAMEBUFFER_INFO_GET_INFO     GetInfo;
};

#endif // APPLE_FRAMEBUFFER_INFO_H


// Apple's GUID
#define APPLE_GUID \
    { 0x7c436110, 0xab2a, 0x4bbb, \
    { 0xa8, 0x80, 0xfe, 0x41, 0x99, 0x5c, 0x9f, 0x82 } };

// Apple's NVRAM ACCESS FLAGS
#define APPLE_FLAGS EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS|EFI_VARIABLE_NON_VOLATILE;

// These codes are returned with the csr-active-config NVRAM variable
#define CSR_ALLOW_UNTRUSTED_KEXTS              0x0001        // Introduced in Mac OS 10.11 El Capitan
#define CSR_ALLOW_UNRESTRICTED_FS              0x0002        //               Ditto
#define CSR_ALLOW_TASK_FOR_PID                 0x0004        //               Ditto
#define CSR_ALLOW_KERNEL_DEBUGGER              0x0008        //               Ditto
#define CSR_ALLOW_APPLE_INTERNAL               0x0010        //               Ditto
#define CSR_ALLOW_UNRESTRICTED_DTRACE          0x0020        //               Ditto
#define CSR_ALLOW_UNRESTRICTED_NVRAM           0x0040        //               Ditto
#define CSR_ALLOW_DEVICE_CONFIGURATION         0x0080        //               Ditto
#define CSR_ALLOW_ANY_RECOVERY_OS              0x0100        // Introduced in Mac OS 10.12 Sierra
#define CSR_ALLOW_UNAPPROVED_KEXTS             0x0200        // Introduced in Mac OS 10.13 High Sierra
#define CSR_ALLOW_EXECUTABLE_POLICY_OVERRIDE   0x0400        // Introduced in Mac OS 10.14 Mojave
#define CSR_ALLOW_UNAUTHENTICATED_ROOT         0x0800        // Introduced in Mac OS 11.00 Big Sur
#define CSR_END_OF_LIST                        0xFFFFFFFF

// SIP/SSV "Enabled" Setting
#define SIP_ENABLED  (CSR_ALLOW_APPLE_INTERNAL)                                            // 0x010

// SIP "Disabled" Setting (Mac OS 10.11+)
#define SIP_DISABLED (CSR_ALLOW_UNTRUSTED_KEXTS | CSR_ALLOW_UNRESTRICTED_FS | \
    CSR_ALLOW_TASK_FOR_PID | CSR_ALLOW_APPLE_INTERNAL | \
    CSR_ALLOW_UNRESTRICTED_DTRACE | CSR_ALLOW_UNRESTRICTED_NVRAM)                          // 0x077

// SSV "Disabled" Settings (Mac OS 11.00+)
#define SSV_DISABLED (SIP_DISABLED | CSR_ALLOW_UNAUTHENTICATED_ROOT)                       // 0x877
#define SSV_DISABLED_EX (SSV_DISABLED | CSR_ALLOW_KERNEL_DEBUGGER)                         // 0x87F

// Recognised Custom SSV "Disabled" Settings
#define SSV_DISABLED_ANY (SSV_DISABLED | CSR_ALLOW_ANY_RECOVERY_OS)                        // 0x977
#define SSV_DISABLED_ANY_EX (SSV_DISABLED_EX | CSR_ALLOW_ANY_RECOVERY_OS)                  // 0x97F
#define SSV_DISABLED_KEXT (SSV_DISABLED_EX | CSR_ALLOW_UNAPPROVED_KEXTS)                   // 0xA7F
#define SSV_DISABLED_WIDE_OPEN (CSR_ALLOW_UNTRUSTED_KEXTS | \
    CSR_ALLOW_UNRESTRICTED_FS | CSR_ALLOW_TASK_FOR_PID | \
    CSR_ALLOW_KERNEL_DEBUGGER | CSR_ALLOW_UNRESTRICTED_DTRACE | \
    CSR_ALLOW_UNRESTRICTED_NVRAM | CSR_ALLOW_DEVICE_CONFIGURATION | \
    CSR_ALLOW_ANY_RECOVERY_OS | CSR_ALLOW_UNAPPROVED_KEXTS | \
    CSR_ALLOW_EXECUTABLE_POLICY_OVERRIDE | CSR_ALLOW_UNAUTHENTICATED_ROOT)                  // 0xFEF


// Max Legal CSR "Disabled" Setting
#define CSR_MAX_LEGAL_VALUE (CSR_ALLOW_UNTRUSTED_KEXTS | CSR_ALLOW_UNRESTRICTED_FS | \
    CSR_ALLOW_TASK_FOR_PID | CSR_ALLOW_KERNEL_DEBUGGER | \
    CSR_ALLOW_APPLE_INTERNAL | CSR_ALLOW_UNRESTRICTED_DTRACE | \
    CSR_ALLOW_UNRESTRICTED_NVRAM | CSR_ALLOW_DEVICE_CONFIGURATION | \
    CSR_ALLOW_ANY_RECOVERY_OS | CSR_ALLOW_UNAPPROVED_KEXTS | \
    CSR_ALLOW_EXECUTABLE_POLICY_OVERRIDE | CSR_ALLOW_UNAUTHENTICATED_ROOT)                  // 0xFFF

extern CHAR16 *gCsrStatus;

EFI_STATUS SetAppleOSInfo();
EFI_STATUS GetCsrStatus (UINT32 *CsrValue);

VOID RecordgCsrStatus (UINT32 CsrStatus, BOOLEAN DisplayMessage);
VOID RotateCsrValue (VOID);
VOID ForceTRIM (VOID);
VOID DisableCompatCheck (VOID);
VOID DisableAMFI (VOID);
#endif
