/*
 * BootMaster/scan.h
 * Headers related to scanning for boot loaders
 *
 * Copyright (c) 2006-2010 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Modifications for rEFInd Copyright (c) 2012-2020 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 *
 */
/*
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
 */
 /*
  * Modified for RefindPlus
  * Copyright (c) 2021-2024 Dayo Akanji (sf.net/u/dakanji/profile)
  *
  * Modifications distributed under the preceding terms.
  */

#ifndef __SCAN_H_
#define __SCAN_H_

#define LABEL_BOOTORDER     L"Manage BootOrder"
#define LABEL_FIRMWARE      L"Firmware Reboot"
#define LABEL_INSTALL       L"Install RefindPlus"
#define LABEL_NETBOOT       L"Net Boot"
#define LABEL_REBOOT        L"System Restart"
#define LABEL_SHUTDOWN      L"System Shutdown"
#define LABEL_CSR_ROTATE    L"Rotate CSR"
#define LABEL_ABOUT         L"About RefindPlus"
#define LABEL_MOK           L"MOK Protocol"
#define LABEL_EXIT          L"Exit RefindPlus"
#define LABEL_SHELL         L"uEFI Shell"
#define LABEL_HIDDEN        L"Hidden Items"
#define LABEL_MEMTEST       L"MemTest Tool"
#define LABEL_GDISK         L"GDisk Tool"
#define LABEL_GPTSYNC       L"GPTsync Tool"
#define LABEL_FWUPDATE      L"Firmware Update"
#define LABEL_CLEAN_NVRAM   L"Clean nvRAM"
#define LABEL_RECOVERY_MAC  L"Recovery (Mac)"
#define LABEL_RECOVERY_WIN  L"Recovery (Win)"

#if defined (EFIX64)
#   define MEMTEST_FILES \
L"bootx64.efi,memtest86.efi,memtest.efi"
#   define MEMTEST_FILES_MORE \
L"memtest86_x64.efi,memtest86x64.efi,memtest86+x64.efi,memtest_x64.efi"
#   define MEMTEST_FILES_EXTRA \
L"memtest86p.efi,memtest86p_x64.efi,memtest86px64.efi"
#   define SKIPNAME_PATTERNS       L"*ia32*.efi,*aa64*.efi,*mips*.efi"
#   define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootx64.efi"
#   define FALLBACK_BASENAME       L"bootx64.efi"
#   define NETBOOT_FILES           L"ipxe.efi,ipxe_x64.efi,ipxex64.efi"
#   define GPTSYNC_FILES           L"gptsync.efi,gptsync_x64.efi,gptsyncx64.efi"
#   define GDISK_FILES             L"gdisk.efi,gdisk_x64.efi,gdisk_x64.efi"
#   define SHELL_FILES             L"shell.efi,shell_x64.efi,shellx64.efi"
#   define NVRAMCLEAN_FILES        L"CleanNvram.efi,CleanNvram_x64.efi,CleanNvramx64.efi"
#elif defined (EFI32)
#   define MEMTEST_FILES \
L"bootia32.efi,memtest86.efi,memtest.efi"
#   define MEMTEST_FILES_MORE \
L"memtest86_ia32.efi,memtest86ia32.efi,memtest86+ia32.efi,memtest_ia32.efi"
#   define MEMTEST_FILES_EXTRA \
L"memtest86p.efi,memtest86p_ia32.efi,memtest86pia32.efi"
#   define SKIPNAME_PATTERNS       L"*x64*.efi,*aa64*.efi,*mips*.efi"
#   define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootia32.efi"
#   define FALLBACK_BASENAME       L"bootia32.efi"
#   define NETBOOT_FILES           L"ipxe.efi,ipxe_ia32.efi,ipxeia32.efi"
#   define GPTSYNC_FILES           L"gptsync.efi,gptsync_ia32.efi,gptsyncia32.efi"
#   define GDISK_FILES             L"gdisk.efi,gdisk_ia32.efi,gdiskia32.efi"
#   define SHELL_FILES             L"shell.efi,shell_ia32.efi,shellia32.efi"
#   define NVRAMCLEAN_FILES        L"CleanNvram.efi,CleanNvram_ia32.efi,CleanNvramia32.efi"
#elif defined (EFIAARCH64)
#   define MEMTEST_FILES \
L"bootaa64.efi,memtest86.efi,memtest.efi"
#   define MEMTEST_FILES_MORE \
L"memtest86_aa64.efi,memtest86aa64.efi,memtest86+aa64.efi,memtest_aa64.efi"
#   define MEMTEST_FILES_EXTRA \
L"memtest86p.efi,memtest86p_aa64.efi,memtest86paa64.efi"
#   define SKIPNAME_PATTERNS       L"*x64*.efi,*ia32*.efi,*mips*.efi"
#   define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootaa64.efi"
#   define FALLBACK_BASENAME       L"bootaa64.efi"
#   define NETBOOT_FILES           L"ipxe.efi,ipxe_aa64.efi,ipxeaa64.efi"
#   define GPTSYNC_FILES           L"gptsync.efi,gptsync_aa64.efi,gptsyncaa64.efi"
#   define GDISK_FILES             L"gdisk.efi,gdisk_aa64.efi,gdiskaa64.efi"
#   define SHELL_FILES             L"shell.efi,shell_aa64.efi,shellaa64.efi"
#   define NVRAMCLEAN_FILES        L"CleanNvram.efi,CleanNvram_aa64.efi,CleanNvramaa64.efi"
#else
#   define MEMTEST_FILES           L"boot.efi"
#   define MEMTEST_FILES_MORE      L"memtest.efi"
#   define MEMTEST_FILES_EXTRA     L"memtest86.efi"
#   define SKIPNAME_PATTERNS       L"*x64*.efi,*ia32*.efi,*aa64*.efi,*mips*.efi"
#   define FALLBACK_FULLNAME       L"EFI\\BOOT\\boot.efi" // Not really correct
#   define FALLBACK_BASENAME       L"boot.efi"            // Not really correct
#   define NETBOOT_FILES           L"ipxe.efi"
#   define GPTSYNC_FILES           L"gptsync.efi"
#   define GDISK_FILES             L"gdisk.efi"
#   define SHELL_FILES             L"shell.efi"
#   define NVRAMCLEAN_FILES        L"CleanNvram.efi"
#endif

#define BASE_LINUX_DISTROS \
L"Arch,Artful,Bionic,CachyOS,Centos,Chakra,Crunchbang,Debian,Deepin,Devuan,\
Elementary,EndeavourOS,Fedora,Frugalware,Gentoo,LinuxMint,\
Mageia,Mandriva,Manjaro,OpenSUSE,Redhat,Slackware,SUSE,\
Kubuntu,Lubuntu,Xubuntu,Ubuntu,Void,Zorin"

#define MAIN_LINUX_DISTROS \
L"Arch,CachyOS,Debian,Deepin,Elementary,EndeavourOS,Fedora,Gentoo,\
LinuxMint,Manjaro,OpenSUSE,Redhat,Slackware,SUSE,Ubuntu,Zorin"


EG_IMAGE * GetDiskBadge (IN UINTN DiskType);

LOADER_ENTRY * InitializeLoaderEntry (IN LOADER_ENTRY *Entry);
LOADER_ENTRY * CopyLoaderEntry (IN LOADER_ENTRY *Entry);

REFIT_MENU_ENTRY * CopyMenuEntry (REFIT_MENU_ENTRY *Entry);

REFIT_MENU_SCREEN * CopyMenuScreen (REFIT_MENU_SCREEN *Entry);
REFIT_MENU_SCREEN * InitializeSubScreen (IN LOADER_ENTRY *Entry);

VOID ScanForTools (VOID);
VOID ScanForBootloaders (VOID);
VOID SetLoaderDefaults (
    IN LOADER_ENTRY *Entry,
    IN CHAR16       *LoaderPath,
    IN REFIT_VOLUME *Volume
);
VOID GenerateSubScreen (
    IN OUT LOADER_ENTRY *Entry,
    IN     REFIT_VOLUME *Volume,
    IN     BOOLEAN       GenerateReturn
);

CHAR16 * SetVolJoin (
    IN CHAR16  *InstanceName,
    IN BOOLEAN  ForBoot
);
CHAR16 * SetVolKind (
    IN CHAR16 *InstanceName,
    IN CHAR16 *VolumeName,
    IN UINT32  VolumeFSType
);
CHAR16 * SetVolFlag (
    IN CHAR16 *InstanceName,
    IN CHAR16 *VolumeName
);
CHAR16 * SetVolType (
    IN CHAR16 *InstanceName OPTIONAL,
    IN CHAR16 *VolumeName,
    IN UINT32  VolumeFSType
);
CHAR16 * GetVolumeGroupName (
    IN CHAR16       *LoaderPath,
    IN REFIT_VOLUME *Volume
);

BOOLEAN ShouldScan (
    REFIT_VOLUME *Volume,
    CHAR16       *Path
);

#endif

/* EOF */
