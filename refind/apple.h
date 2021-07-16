/*
 * refind/apple.h
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

#ifndef __APPLE_H_
#define __APPLE_H_

// The constants related to Apple's System Integrity Protection (SIP)....
#define CSR_GUID { 0x7c436110, 0xab2a, 0x4bbb, { 0xa8, 0x80, 0xfe, 0x41, 0x99, 0x5c, 0x9f, 0x82 } };
// These codes are returned in the first two bytes of the csr-active-config variable
#define CSR_ALLOW_UNTRUSTED_KEXTS            0x01
#define CSR_ALLOW_UNRESTRICTED_FS            0x02
#define CSR_ALLOW_TASK_FOR_PID               0x04
#define CSR_ALLOW_KERNEL_DEBUGGER            0x08
#define CSR_ALLOW_APPLE_INTERNAL             0x10
#define CSR_ALLOW_UNRESTRICTED_DTRACE        0x20
#define CSR_ALLOW_UNRESTRICTED_NVRAM         0x40
#define CSR_ALLOW_DEVICE_CONFIGURATION       0x80
#define CSR_ALLOW_ANY_RECOVERY_OS            0x100
#define CSR_ALLOW_UNAPPROVED_KEXTS           0x200
#define CSR_ALLOW_EXECUTABLE_POLICY_OVERRIDE 0x400
#define CSR_ALLOW_UNAUTHENTICATED_ROOT       0x800
#define CSR_END_OF_LIST                      0xFFFFFFFF
// Some summaries....
#define SIP_ENABLED  CSR_ALLOW_APPLE_INTERNAL
// SIP was disabled with a value of 0x77 in macOS 10.x....
#define SIP_DISABLED_10 (CSR_ALLOW_UNRESTRICTED_NVRAM | \
                         CSR_ALLOW_UNRESTRICTED_DTRACE | \
                         CSR_ALLOW_APPLE_INTERNAL | \
                         CSR_ALLOW_TASK_FOR_PID | \
                         CSR_ALLOW_UNRESTRICTED_FS | \
                         CSR_ALLOW_UNTRUSTED_KEXTS)
// Starting with macOS 11.0, SIP requires 0x877 to be fully disabled....
#define SIP_DISABLED_11 (CSR_ALLOW_UNRESTRICTED_NVRAM | \
                         CSR_ALLOW_UNRESTRICTED_DTRACE | \
                         CSR_ALLOW_APPLE_INTERNAL | \
                         CSR_ALLOW_TASK_FOR_PID | \
                         CSR_ALLOW_UNRESTRICTED_FS | \
                         CSR_ALLOW_UNTRUSTED_KEXTS | \
                         CSR_ALLOW_UNAUTHENTICATED_ROOT)
#define CSR_MAX_LEGAL_VALUE (CSR_ALLOW_UNTRUSTED_KEXTS | \
                             CSR_ALLOW_UNRESTRICTED_FS | \
                             CSR_ALLOW_TASK_FOR_PID | \
                             CSR_ALLOW_KERNEL_DEBUGGER | \
                             CSR_ALLOW_APPLE_INTERNAL | \
                             CSR_ALLOW_UNRESTRICTED_DTRACE | \
                             CSR_ALLOW_UNRESTRICTED_NVRAM | \
                             CSR_ALLOW_DEVICE_CONFIGURATION | \
                             CSR_ALLOW_ANY_RECOVERY_OS | \
                             CSR_ALLOW_UNAPPROVED_KEXTS | \
                             CSR_ALLOW_EXECUTABLE_POLICY_OVERRIDE | \
                             CSR_ALLOW_UNAUTHENTICATED_ROOT)

extern CHAR16 gCsrStatus[512];

EFI_STATUS GetCsrStatus(UINT32 *CsrValue);
VOID RecordgCsrStatus(UINT32 CsrStatus, BOOLEAN DisplayMessage);
VOID RotateCsrValue(VOID);
EFI_STATUS SetAppleOSInfo();

#endif
