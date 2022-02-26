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



#ifndef APPLE_APFS_INFO_H
#define APPLE_APFS_INFO_H

#define APPLE_APFS_PARTITION_TYPE_GUID  \
{ \
  0x7C3457EF, 0x0000, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}  \
}


#define APPLE_APFS_CONTAINER_INFO_GUID  \
{ \
  0x3533CF0D, 0x685F, 0x5EBF, {0x8D, 0xC6, 0x73, 0x93, 0x48, 0x5B, 0xAF, 0xA2}  \
}
typedef struct {
    UINT32     Always1;
    EFI_GUID   Uuid;
} APPLE_APFS_CONTAINER_INFO;


#define APPLE_APFS_VOLUME_ROLE_UNDEFINED  (0x00) // Early APFS ... Combined System/Data Volume
#define APPLE_APFS_VOLUME_ROLE_SYSTEM     (0x01) // Later APFS ... Separate System Volume
#define APPLE_APFS_VOLUME_ROLE_RECOVERY   (0x04)
#define APPLE_APFS_VOLUME_ROLE_VM         (0x08)
#define APPLE_APFS_VOLUME_ROLE_PREBOOT    (0x10)
#define APPLE_APFS_VOLUME_ROLE_DATA       (0x40)
#define APPLE_APFS_VOLUME_ROLE_UPDATE     (0xC0)
#define APPLE_APFS_VOLUME_ROLE_UNKNOWN    (0xFF)
typedef UINT32 APPLE_APFS_VOLUME_ROLE;


#define APPLE_APFS_VOLUME_INFO_GUID  \
{ \
  0x900C7693, 0x8C14, 0x58BA, {0xB4, 0x4E, 0x97, 0x45, 0x15, 0xD2, 0x7C, 0x78}  \
}
typedef struct {
    UINT32                 Always1;
    EFI_GUID               Uuid;
    APPLE_APFS_VOLUME_ROLE Role;
} APPLE_APFS_VOLUME_INFO;


#ifdef __MAKEWITH_TIANO
// DA-TAG: Limit to TianoCore - START
EFI_STATUS RP_GetApfsVolumeInfo (
    IN  EFI_HANDLE               Device,
    OUT EFI_GUID                *ContainerGuid OPTIONAL,
    OUT EFI_GUID                *VolumeGuid    OPTIONAL,
    OUT APPLE_APFS_VOLUME_ROLE  *VolumeRole    OPTIONAL
);
CHAR16 * RP_GetAppleDiskLabel (
    IN  REFIT_VOLUME *Volume
);
// DA-TAG: Limit to TianoCore - END
#endif

#endif // APPLE_APFS_INFO_H




// Apple's GUID
#define APPLE_GUID \
{ \
  0x7c436110, 0xab2a, 0x4bbb, {0xa8, 0x80, 0xfe, 0x41, 0x99, 0x5c, 0x9f, 0x82}  \
}

// Apple's NVRAM ACCESS FLAGS
#define APPLE_FLAGS     EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS|EFI_VARIABLE_NON_VOLATILE;


// These codes are returned with the csr-active-config NVRAM variable
#define CSR_CLEAR_SETTING                      0x0000        // RefindPlus Custom Code
#define CSR_ALLOW_UNTRUSTED_KEXTS              0x0001        // Introduced in MacOS 10.11 El Capitan
#define CSR_ALLOW_UNRESTRICTED_FS              0x0002        //      Ditto
#define CSR_ALLOW_TASK_FOR_PID                 0x0004        //      Ditto
#define CSR_ALLOW_KERNEL_DEBUGGER              0x0008        //      Ditto
#define CSR_ALLOW_APPLE_INTERNAL               0x0010        //      Ditto
#define CSR_ALLOW_UNRESTRICTED_DTRACE          0x0020        //      Ditto
#define CSR_ALLOW_UNRESTRICTED_NVRAM           0x0040        //      Ditto
#define CSR_ALLOW_DEVICE_CONFIGURATION         0x0080        //      Ditto
#define CSR_ALLOW_ANY_RECOVERY_OS              0x0100        // Introduced in MacOS 10.12 Sierra
#define CSR_ALLOW_UNAPPROVED_KEXTS             0x0200        // Introduced in MacOS 10.13 High Sierra
#define CSR_ALLOW_EXECUTABLE_POLICY_OVERRIDE   0x0400        // Introduced in MacOS 10.14 Mojave
#define CSR_ALLOW_UNAUTHENTICATED_ROOT         0x0800        // Introduced in MacOS 11.00 Big Sur
#define CSR_END_OF_LIST                        0xFFFFFFFF

// Clear CSR for MacOS 11.00 Big Sur (Custom)
#define SIP_ENABLED_EX (CSR_CLEAR_SETTING)                                                 // 0x000

// SIP/SSV "Enabled" Setting
#define SIP_ENABLED  (CSR_ALLOW_APPLE_INTERNAL)                                            // 0x010

// SIP "Disabled" Setting (MacOS 10.11+)
#define SIP_DISABLED (CSR_ALLOW_UNTRUSTED_KEXTS | CSR_ALLOW_UNRESTRICTED_FS | \
    CSR_ALLOW_TASK_FOR_PID | CSR_ALLOW_APPLE_INTERNAL | \
    CSR_ALLOW_UNRESTRICTED_DTRACE | CSR_ALLOW_UNRESTRICTED_NVRAM)                          // 0x077

// SIP "Disabled" Setting (MacOS 11.00+)
#define SIP_DISABLED_B (SIP_DISABLED | CSR_ALLOW_KERNEL_DEBUGGER)                          // 0x07F
#define SIP_DISABLED_EX (SIP_DISABLED & ~CSR_ALLOW_APPLE_INTERNAL)                         // 0x067
#define SIP_DISABLED_DBG (SIP_DISABLED_EX | CSR_ALLOW_KERNEL_DEBUGGER)                     // 0x06F
#define SIP_DISABLED_KEXT (SIP_DISABLED_EX | CSR_ALLOW_UNAPPROVED_KEXTS)                   // 0x267
#define SIP_DISABLED_EXTRA (SIP_DISABLED_KEXT | CSR_ALLOW_KERNEL_DEBUGGER)                 // 0x26F

// SSV "Disabled" Settings (MacOS 11.00+)
#define SSV_DISABLED (SIP_DISABLED | CSR_ALLOW_UNAUTHENTICATED_ROOT)                       // 0x877
#define SSV_DISABLED_B (SSV_DISABLED | CSR_ALLOW_KERNEL_DEBUGGER)                          // 0x87F
#define SSV_DISABLED_EX (SSV_DISABLED_B & ~CSR_ALLOW_APPLE_INTERNAL)                       // 0x86F

// Recognised Custom SSV "Disabled" Settings
#define SSV_DISABLED_ANY (SSV_DISABLED | CSR_ALLOW_ANY_RECOVERY_OS)                        // 0x977
#define SSV_DISABLED_ANY_EX (SSV_DISABLED_B | CSR_ALLOW_ANY_RECOVERY_OS)                   // 0x97F
#define SSV_DISABLED_KEXT (SSV_DISABLED_B | CSR_ALLOW_UNAPPROVED_KEXTS)                    // 0xA7F
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
EFI_STATUS GetCsrStatus (IN OUT UINT32 *CsrValue);
EFI_STATUS NormaliseCSR (VOID);

VOID ClearRecoveryBootFlag (VOID);
VOID RecordgCsrStatus (UINT32 CsrStatus, BOOLEAN DisplayMessage);
VOID RotateCsrValue (VOID);
#endif
