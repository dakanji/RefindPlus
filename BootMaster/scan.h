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

#define LABEL_GDISK           L"GDisk Tool"
#define LABEL_GPTSYNC         L"GPTsync Tool"
#define LABEL_CLEAN_NVRAM     L"Clean nvRAM"
#define LABEL_MEMTEST         L"MemTest Tool"
#define LABEL_HIDDEN          L"Restore Entries"

#if defined (EFIX64)
#   define SHELL_NAMES \
L"EFI\\tools_x64\\shell_x64.efi,EFI\\tools_x64\\shell.efi,EFI\\tools_x64\\x64_shell.efi,EFI\\tools_x64\\shellx64.efi,\
EFI\\tools\\shell_x64.efi,EFI\\tools\\shell.efi,EFI\\tools\\x64_shell.efi,EFI\\tools\\shellx64.efi,\
EFI\\x64_tools\\shell_x64.efi,EFI\\x64_tools\\shell.efi,EFI\\x64_tools\\x64_shell.efi,EFI\\x64_tools\\shellx64.efi,\
EFI\\BOOT\\tools\\shell_x64.efi,EFI\\BOOT\\tools\\shell.efi,EFI\\BOOT\\tools\\x64_shell.efi,EFI\\BOOT\\tools\\shellx64.efi,\
EFI\\BOOT\\tools_x64\\shell_x64.efi,EFI\\BOOT\\tools_x64\\shell.efi,EFI\\BOOT\\tools_x64\\x64_shell.efi,EFI\\BOOT\\tools_x64\\shellx64.efi,\
EFI\\BOOT\\x64_tools\\shell_x64.efi,EFI\\BOOT\\x64_tools\\shell.efi,EFI\\BOOT\\x64_tools\\x64_shell.efi,EFI\\BOOT\\x64_tools\\shellx64.efi,\
EFI\\shell_x64.efi,EFI\\shell.efi,EFI\\x64_shell.efi,EFI\\shellx64.efi,\
\\shell_x64.efi,\\shell.efi,\\x64_shell.efi,\\shellx64.efi"
#   define GPTSYNC_NAMES \
L"EFI\\tools_x64\\gptsync_x64.efi,EFI\\tools_x64\\gptsync.efi,EFI\\tools_x64\\x64_gptsync.efi,EFI\\tools_x64\\gptsyncx64.efi,\
EFI\\tools\\gptsync_x64.efi,EFI\\tools\\gptsync.efi,EFI\\tools\\x64_gptsync.efi,EFI\\tools\\gptsyncx64.efi,\
EFI\\x64_tools\\gptsync_x64.efi,EFI\\x64_tools\\gptsync.efi,EFI\\x64_tools\\x64_gptsync.efi,EFI\\x64_tools\\gptsyncx64.efi,\
EFI\\BOOT\\tools\\gptsync_x64.efi,EFI\\BOOT\\tools\\gptsync.efi,EFI\\BOOT\\tools\\x64_gptsync.efi,EFI\\BOOT\\tools\\gptsyncx64.efi,\
EFI\\BOOT\\tools_x64\\gptsync_x64.efi,EFI\\BOOT\\tools_x64\\gptsync.efi,EFI\\BOOT\\tools_x64\\x64_gptsync.efi,EFI\\BOOT\\tools_x64\\gptsyncx64.efi,\
EFI\\BOOT\\x64_tools\\gptsync_x64.efi,EFI\\BOOT\\x64_tools\\gptsync.efi,EFI\\BOOT\\x64_tools\\x64_gptsync.efi,EFI\\BOOT\\x64_tools\\gptsyncx64.efi,\
EFI\\gptsync_x64.efi,EFI\\gptsync.efi,EFI\\x64_gptsync.efi,EFI\\gptsyncx64.efi,\
\\gptsync_x64.efi,\\gptsync.efi,\\x64_gptsync.efi,\\gptsyncx64.efi"
#   define GDISK_NAMES \
L"EFI\\tools_x64\\gdisk_x64.efi,EFI\\tools_x64\\gdisk.efi,EFI\\tools_x64\\x64_gdisk.efi,EFI\\tools_x64\\gdiskx64.efi,\
EFI\\tools\\gdisk_x64.efi,EFI\\tools\\gdisk.efi,EFI\\tools\\x64_gdisk.efi,EFI\\tools\\gdiskx64.efi,\
EFI\\x64_tools\\gdisk_x64.efi,EFI\\x64_tools\\gdisk.efi,EFI\\x64_tools\\x64_gdisk.efi,EFI\\x64_tools\\gdiskx64.efi,\
EFI\\BOOT\\tools\\gdisk_x64.efi,EFI\\BOOT\\tools\\gdisk.efi,EFI\\BOOT\\tools\\x64_gdisk.efi,EFI\\BOOT\\tools\\gdiskx64.efi,\
EFI\\BOOT\\tools_x64\\gdisk_x64.efi,EFI\\BOOT\\tools_x64\\gdisk.efi,EFI\\BOOT\\tools_x64\\x64_gdisk.efi,EFI\\BOOT\\tools_x64\\gdiskx64.efi,\
EFI\\BOOT\\x64_tools\\gdisk_x64.efi,EFI\\BOOT\\x64_tools\\gdisk.efi,EFI\\BOOT\\x64_tools\\x64_gdisk.efi,EFI\\BOOT\\x64_tools\\gdiskx64.efi,\
EFI\\gdisk_x64.efi,EFI\\gdisk.efi,EFI\\x64_gdisk.efi,EFI\\gdiskx64.efi,\
\\gdisk_x64.efi,\\gdisk.efi,\\x64_gdisk.efi,\\gdiskx64.efi"
#   define NETBOOT_NAMES \
L"EFI\\tools_x64\\ipxe_x64.efi,EFI\\tools_x64\\ipxe.efi,EFI\\tools_x64\\x64_ipxe.efi,,EFI\\tools_x64\\ipxex64.efi,\
EFI\\tools\\ipxe_x64.efi,EFI\\tools\\ipxe.efi,EFI\\tools\\x64_ipxe.efi,EFI\\tools\\ipxex64.efi,\
EFI\\x64_tools\\ipxe_x64.efi,EFI\\x64_tools\\ipxe.efi,EFI\\x64_tools\\x64_ipxe.efi,EFI\\x64_tools\\ipxex64.efi,\
EFI\\BOOT\\tools\\ipxe_x64.efi,EFI\\BOOT\\tools\\ipxe.efi,EFI\\BOOT\\tools\\x64_ipxe.efi,EFI\\BOOT\\tools\\ipxex64.efi,\
EFI\\BOOT\\tools_x64\\ipxe_x64.efi,EFI\\BOOT\\tools_x64\\ipxe.efi,EFI\\BOOT\\tools_x64\\x64_ipxe.efi,EFI\\BOOT\\tools_x64\\ipxex64.efi,\
EFI\\BOOT\\x64_tools\\ipxe_x64.efi,EFI\\BOOT\\x64_tools\\ipxe.efi,EFI\\BOOT\\x64_tools\\x64_ipxe.efi,EFI\\BOOT\\x64_tools\\ipxex64.efi,\
EFI\\ipxe_x64.efi,EFI\\ipxe.efi,EFI\\x64_ipxe.efi,EFI\\ipxex64.efi,\
\\ipxe_x64.efi,\\ipxe.efi,\\x64_ipxe.efi,\\ipxex64.efi"
#   define MEMTEST_NAMES \
L"memtest.efi,memtest_x64.efi,x64_memtest.efi,memtestx64.efi,memtest+x64.efi,\
memtest86.efi,memtest86_x64.efi,x64_memtest86.efi,memtest86x64.efi,memtest86+x64.efi,bootx64.efi"
#   define FALLBACK_SKIPNAME       L"bootia32.efi,bootaa64.efi,bootmips.efi"
#   define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootx64.efi"
#   define FALLBACK_BASENAME       L"bootx64.efi"
#   define NETBOOT_FILES           L"ipxe_x64.efi,ipxe.efi,x64_ipxe.efi,ipxex64.efi"
#   define GPTSYNC_FILES           L"gptsync_x64.efi,gptsync.efi,x64_gptsync.efi,gptsyncx64.efi"
#   define GDISK_FILES             L"gdisk_x64.efi,gdisk.efi,x64_gdisk.efiefi,gdiskx64.efi"
#   define SHELL_FILES             L"shell_x64.efi,shell.efi,x64_shell.efi,shellx64.efi"
#   define NVRAMCLEAN_FILES        L"CleanNvram_x64.efi,CleanNvram.efi,x64_CleanNvram.efi,CleanNvramx64.efi"
#elif defined (EFI32)
#   define SHELL_NAMES \
L"EFI\\tools_ia32\\shell_ia32.efi,EFI\\tools_ia32\\shell.efi,EFI\\tools_ia32\\ia32_shell.efi,EFI\\tools_ia32\\shellia32.efi,\
EFI\\tools\\shell_ia32.efi,EFI\\tools\\shell.efi,EFI\\tools\\ia32_shell.efi,EFI\\tools\\shellia32.efi,\
EFI\\ia32_tools\\shell_ia32.efi,EFI\\ia32_tools\\shell.efi,EFI\\ia32_tools\\ia32_shell.efi,EFI\\ia32_tools\\shellia32.efi,\
EFI\\BOOT\\tools\\shell_ia32.efi,EFI\\BOOT\\tools\\shell.efi,EFI\\BOOT\\tools\\ia32_shell.efi,EFI\\BOOT\\tools\\shellia32.efi,\
EFI\\BOOT\\tools_ia32\\shell_ia32.efi,EFI\\BOOT\\tools_ia32\\shell.efi,EFI\\BOOT\\tools_ia32\\ia32_shell.efi,EFI\\BOOT\\tools_ia32\\shellia32.efi,\
EFI\\BOOT\\ia32_tools\\shell_ia32.efi,EFI\\BOOT\\ia32_tools\\shell.efi,EFI\\BOOT\\ia32_tools\\ia32_shell.efi,EFI\\BOOT\\ia32_tools\\shellia32.efi,\
EFI\\shell_ia32.efi,EFI\\shell.efi,EFI\\ia32_shell.efi,EFI\\shellia32.efi,\
\\shell_ia32.efi,\\shell.efi,\\ia32_shell.efi,\\shellia32.efi"
#   define GPTSYNC_NAMES \
L"EFI\\tools_ia32\\gptsync_ia32.efi,EFI\\tools_ia32\\gptsync.efi,EFI\\tools_ia32\\ia32_gptsync.efi,EFI\\tools_ia32\\gptsyncia32.efi,\
EFI\\tools\\gptsync_ia32.efi,EFI\\tools\\gptsync.efi,EFI\\tools\\ia32_gptsync.efi,EFI\\tools\\gptsyncia32.efi,\
EFI\\ia32_tools\\gptsync_ia32.efi,EFI\\ia32_tools\\gptsync.efi,EFI\\ia32_tools\\ia32_gptsync.efi,EFI\\ia32_tools\\gptsyncia32.efi,\
EFI\\BOOT\\tools\\gptsync_ia32.efi,EFI\\BOOT\\tools\\gptsync.efi,EFI\\BOOT\\tools\\ia32_gptsync.efi,EFI\\BOOT\\tools\\gptsyncia32.efi,\
EFI\\BOOT\\tools_ia32\\gptsync_ia32.efi,EFI\\BOOT\\tools_ia32\\gptsync.efi,EFI\\BOOT\\tools_ia32\\ia32_gptsync.efi,EFI\\BOOT\\tools_ia32\\gptsyncia32.efi,\
EFI\\BOOT\\ia32_tools\\gptsync_ia32.efi,EFI\\BOOT\\ia32_tools\\gptsync.efi,EFI\\BOOT\\ia32_tools\\ia32_gptsync.efi,EFI\\BOOT\\ia32_tools\\gptsyncia32.efi,\
EFI\\gptsync_ia32.efi,EFI\\gptsync.efi,EFI\\ia32_gptsync.efi,EFI\\gptsyncia32.efi,\
\\gptsync_ia32.efi,\\gptsync.efi,\\ia32_gptsync.efi,\\gptsyncia32.efi"
#   define GDISK_NAMES \
L"EFI\\tools_ia32\\gdisk_ia32.efi,EFI\\tools_ia32\\gdisk.efi,EFI\\tools_ia32\\ia32_gdisk.efi,EFI\\tools_ia32\\gdiskia32.efi,\
EFI\\tools\\gdisk_ia32.efi,EFI\\tools\\gdisk.efi,EFI\\tools\\ia32_gdisk.efi,EFI\\tools\\gdiskia32.efi,\
EFI\\ia32_tools\\gdisk_ia32.efi,EFI\\ia32_tools\\gdisk.efi,EFI\\ia32_tools\\ia32_gdisk.efi,EFI\\ia32_tools\\gdiskia32.efi,\
EFI\\BOOT\\tools\\gdisk_ia32.efi,EFI\\BOOT\\tools\\gdisk.efi,EFI\\BOOT\\tools\\ia32_gdisk.efi,EFI\\BOOT\\tools\\gdiskia32.efi,\
EFI\\BOOT\\tools_ia32\\gdisk_ia32.efi,EFI\\BOOT\\tools_ia32\\gdisk.efi,EFI\\BOOT\\tools_ia32\\ia32_gdisk.efi,EFI\\BOOT\\tools_ia32\\gdiskia32.efi,\
EFI\\BOOT\\ia32_tools\\gdisk_ia32.efi,EFI\\BOOT\\ia32_tools\\gdisk.efi,EFI\\BOOT\\ia32_tools\\ia32_gdisk.efi,EFI\\BOOT\\ia32_tools\\gdiskia32.efi,\
EFI\\gdisk_ia32.efi,EFI\\gdisk.efi,EFI\\ia32_gdisk.efi,EFI\\gdiskia32.efi,\
\\gdisk_ia32.efi,\\gdisk.efi,\\ia32_gdisk.efi,\\gdiskia32.efi"
#   define NETBOOT_NAMES \
L"EFI\\tools_ia32\\ipxe_ia32.efi,EFI\\tools_ia32\\ipxe.efi,EFI\\tools_ia32\\ia32_ipxe.efi,,EFI\\tools_ia32\\ipxeia32.efi,\
EFI\\tools\\ipxe_ia32.efi,EFI\\tools\\ipxe.efi,EFI\\tools\\ia32_ipxe.efi,EFI\\tools\\ipxeia32.efi,\
EFI\\ia32_tools\\ipxe_ia32.efi,EFI\\ia32_tools\\ipxe.efi,EFI\\ia32_tools\\ia32_ipxe.efi,EFI\\ia32_tools\\ipxeia32.efi,\
EFI\\BOOT\\tools\\ipxe_ia32.efi,EFI\\BOOT\\tools\\ipxe.efi,EFI\\BOOT\\tools\\ia32_ipxe.efi,EFI\\BOOT\\tools\\ipxeia32.efi,\
EFI\\BOOT\\tools_ia32\\ipxe_ia32.efi,EFI\\BOOT\\tools_ia32\\ipxe.efi,EFI\\BOOT\\tools_ia32\\ia32_ipxe.efi,EFI\\BOOT\\tools_ia32\\ipxeia32.efi,\
EFI\\BOOT\\ia32_tools\\ipxe_ia32.efi,EFI\\BOOT\\ia32_tools\\ipxe.efi,EFI\\BOOT\\ia32_tools\\ia32_ipxe.efi,EFI\\BOOT\\ia32_tools\\ipxeia32.efi,\
EFI\\ipxe_ia32.efi,EFI\\ipxe.efi,EFI\\ia32_ipxe.efi,EFI\\ipxeia32.efi,\
\\ipxe_ia32.efi,\\ipxe.efi,\\ia32_ipxe.efi,\\ipxeia32.efi"
#   define MEMTEST_NAMES \
L"memtest.efi,memtest_ia32.efi,ia32_memtest.efi,memtestia32.efi,memtest+ia32.efi,\
memtest86.efi,memtest86_ia32.efi,ia32_memtest86.efi,memtest8ia32.efi,memtest86+ia32.efi,bootia32.efi"
#   define FALLBACK_SKIPNAME       L"bootx64.efi,bootaa64.efi,bootmips.efi"
L"memtest.efi,memtest_ia32.efi,ia32_memtest.efi,memtestia32.efi,memtest+ia32.efi,\
memtest86.efi,memtest86_ia32.efi,ia32_memtest86.efi,memtest86ia32.efi,memtest86+ia32.efi,bootia32.efi"
#   define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootia32.efi"
#   define FALLBACK_BASENAME       L"bootia32.efi"
#   define NETBOOT_FILES           L"ipxe_ia32.efi,ipxe.efi,ia32_ipxe.efi,ipxeia32.efi"
#   define GPTSYNC_FILES           L"gptsync_ia32.efi,gptsync.efi,ia32_gptsync.efi,gptsyncia32.efi"
#   define GDISK_FILES             L"gdisk_ia32.efi,gdisk.efi,ia32_gdisk.efiefi,gdiskia32.efi"
#   define SHELL_FILES             L"shell_ia32.efi,shell.efi,ia32_shell.efi,shellia32.efi"
#   define NVRAMCLEAN_FILES        L"CleanNvram_ia32.efi,CleanNvram.efi,ia32_CleanNvram.efi,CleanNvramia32.efi"
#elif defined (EFIAARCH64)
#   define SHELL_NAMES \
L"EFI\\tools_aa64\\shell_aa64.efi,EFI\\tools_aa64\\shell.efi,EFI\\tools_aa64\\aa64_shell.efi,EFI\\tools_aa64\\shellaa64.efi,\
EFI\\tools\\shell_aa64.efi,EFI\\tools\\shell.efi,EFI\\tools\\aa64_shell.efi,EFI\\tools\\shellaa64.efi,\
EFI\\aa64_tools\\shell_aa64.efi,EFI\\aa64_tools\\shell.efi,EFI\\aa64_tools\\aa64_shell.efi,EFI\\aa64_tools\\shellaa64.efi,\
EFI\\BOOT\\tools\\shell_aa64.efi,EFI\\BOOT\\tools\\shell.efi,EFI\\BOOT\\tools\\aa64_shell.efi,EFI\\BOOT\\tools\\shellaa64.efi,\
EFI\\BOOT\\tools_aa64\\shell_aa64.efi,EFI\\BOOT\\tools_aa64\\shell.efi,EFI\\BOOT\\tools_aa64\\aa64_shell.efi,EFI\\BOOT\\tools_aa64\\shellaa64.efi,\
EFI\\BOOT\\aa64_tools\\shell_aa64.efi,EFI\\BOOT\\aa64_tools\\shell.efi,EFI\\BOOT\\aa64_tools\\aa64_shell.efi,EFI\\BOOT\\aa64_tools\\shellaa64.efi,\
EFI\\shell_aa64.efi,EFI\\shell.efi,EFI\\aa64_shell.efi,EFI\\shellaa64.efi,\
\\shell_aa64.efi,\\shell.efi,\\aa64_shell.efi,\\shellaa64.efi"
#   define GPTSYNC_NAMES \
L"EFI\\tools_aa64\\gptsync_aa64.efi,EFI\\tools_aa64\\gptsync.efi,EFI\\tools_aa64\\aa64_gptsync.efi,EFI\\tools_aa64\\gptsyncaa64.efi,\
EFI\\tools\\gptsync_aa64.efi,EFI\\tools\\gptsync.efi,EFI\\tools\\aa64_gptsync.efi,EFI\\tools\\gptsyncaa64.efi,\
EFI\\aa64_tools\\gptsync_aa64.efi,EFI\\aa64_tools\\gptsync.efi,EFI\\aa64_tools\\aa64_gptsync.efi,EFI\\aa64_tools\\gptsyncaa64.efi,\
EFI\\BOOT\\tools\\gptsync_aa64.efi,EFI\\BOOT\\tools\\gptsync.efi,EFI\\BOOT\\tools\\aa64_gptsync.efi,EFI\\BOOT\\tools\\gptsyncaa64.efi,\
EFI\\BOOT\\tools_aa64\\gptsync_aa64.efi,EFI\\BOOT\\tools_aa64\\gptsync.efi,EFI\\BOOT\\tools_aa64\\aa64_gptsync.efi,EFI\\BOOT\\tools_aa64\\gptsyncaa64.efi,\
EFI\\BOOT\\aa64_tools\\gptsync_aa64.efi,EFI\\BOOT\\aa64_tools\\gptsync.efi,EFI\\BOOT\\aa64_tools\\aa64_gptsync.efi,EFI\\BOOT\\aa64_tools\\gptsyncaa64.efi,\
EFI\\gptsync_aa64.efi,EFI\\gptsync.efi,EFI\\aa64_gptsync.efi,EFI\\gptsyncaa64.efi,\
\\gptsync_aa64.efi,\\gptsync.efi,\\aa64_gptsync.efi,\\gptsyncaa64.efi"
#   define GDISK_NAMES \
L"EFI\\tools_aa64\\gdisk_aa64.efi,EFI\\tools_aa64\\gdisk.efi,EFI\\tools_aa64\\aa64_gdisk.efi,EFI\\tools_aa64\\gdiskaa64.efi,\
EFI\\tools\\gdisk_aa64.efi,EFI\\tools\\gdisk.efi,EFI\\tools\\aa64_gdisk.efi,EFI\\tools\\gdiskaa64.efi,\
EFI\\aa64_tools\\gdisk_aa64.efi,EFI\\aa64_tools\\gdisk.efi,EFI\\aa64_tools\\aa64_gdisk.efi,EFI\\aa64_tools\\gdiskaa64.efi,\
EFI\\BOOT\\tools\\gdisk_aa64.efi,EFI\\BOOT\\tools\\gdisk.efi,EFI\\BOOT\\tools\\aa64_gdisk.efi,EFI\\BOOT\\tools\\gdiskaa64.efi,\
EFI\\BOOT\\tools_aa64\\gdisk_aa64.efi,EFI\\BOOT\\tools_aa64\\gdisk.efi,EFI\\BOOT\\tools_aa64\\aa64_gdisk.efi,EFI\\BOOT\\tools_aa64\\gdiskaa64.efi,\
EFI\\BOOT\\aa64_tools\\gdisk_aa64.efi,EFI\\BOOT\\aa64_tools\\gdisk.efi,EFI\\BOOT\\aa64_tools\\aa64_gdisk.efi,EFI\\BOOT\\aa64_tools\\gdiskaa64.efi,\
EFI\\gdisk_aa64.efi,EFI\\gdisk.efi,EFI\\aa64_gdisk.efi,EFI\\gdiskaa64.efi,\
\\gdisk_aa64.efi,\\gdisk.efi,\\aa64_gdisk.efi,\\gdiskaa64.efi"
#   define NETBOOT_NAMES \
L"EFI\\tools_aa64\\ipxe_aa64.efi,EFI\\tools_aa64\\ipxe.efi,EFI\\tools_aa64\\aa64_ipxe.efi,,EFI\\tools_aa64\\ipxeaa64.efi,\
EFI\\tools\\ipxe_aa64.efi,EFI\\tools\\ipxe.efi,EFI\\tools\\aa64_ipxe.efi,EFI\\tools\\ipxeaa64.efi,\
EFI\\aa64_tools\\ipxe_aa64.efi,EFI\\aa64_tools\\ipxe.efi,EFI\\aa64_tools\\aa64_ipxe.efi,EFI\\aa64_tools\\ipxeaa64.efi,\
EFI\\BOOT\\tools\\ipxe_aa64.efi,EFI\\BOOT\\tools\\ipxe.efi,EFI\\BOOT\\tools\\aa64_ipxe.efi,EFI\\BOOT\\tools\\ipxeaa64.efi,\
EFI\\BOOT\\tools_aa64\\ipxe_aa64.efi,EFI\\BOOT\\tools_aa64\\ipxe.efi,EFI\\BOOT\\tools_aa64\\aa64_ipxe.efi,EFI\\BOOT\\tools_aa64\\ipxeaa64.efi,\
EFI\\BOOT\\aa64_tools\\ipxe_aa64.efi,EFI\\BOOT\\aa64_tools\\ipxe.efi,EFI\\BOOT\\aa64_tools\\aa64_ipxe.efi,EFI\\BOOT\\aa64_tools\\ipxeaa64.efi,\
EFI\\ipxe_aa64.efi,EFI\\ipxe.efi,EFI\\aa64_ipxe.efi,EFI\\ipxeaa64.efi,\
\\ipxe_aa64.efi,\\ipxe.efi,\\aa64_ipxe.efi,\\ipxeaa64.efi"
#   define MEMTEST_NAMES \
L"memtest.efi,memtest_aa64.efi,aa64_memtest.efi,memtestaa64.efi,memtest+aa64.efi,\
memtest86.efi,memtest86_aa64.efi,aa64_memtest86.efi,memtest8aa64.efi,memtest86+aa64.efi,bootaa64.efi"
#   define FALLBACK_SKIPNAME       L"bootx64.efi,bootia32.efi,bootmips.efi"
L"memtest.efi,memtest_aa64.efi,aa64_memtest.efi,memtestaa64.efi,memtest+aa64.efi,\
memtest86.efi,memtest86_aa64.efi,aa64_memtest86.efi,memtest86aa64.efi,memtest86+aa64.efi,bootaa64.efi"
#   define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootaa64.efi"
#   define FALLBACK_BASENAME       L"bootaa64.efi"
#   define NETBOOT_FILES           L"ipxe_aa64.efi,ipxe.efi,aa64_ipxe.efi,ipxeaa64.efi"
#   define GPTSYNC_FILES           L"gptsync_aa64.efi,gptsync.efi,aa64_gptsync.efi,gptsyncaa64.efi"
#   define GDISK_FILES             L"gdisk_aa64.efi,gdisk.efi,aa64_gdisk.efiefi,gdiskaa64.efi"
#   define SHELL_FILES             L"shell_aa64.efi,shell.efi,aa64_shell.efi,shellaa64.efi"
#   define NVRAMCLEAN_FILES        L"CleanNvram_aa64.efi,CleanNvram.efi,aa64_CleanNvram.efi,CleanNvramaa64.efi"
#else
#   define SHELL_NAMES \
L"EFI\\tools\\shell.efi,EFI\\BOOT\\tools\\shell.efi,EFI\\shell.efi,\\shell.efi"
#   define GPTSYNC_NAMES \
L"EFI\\tools\\gptsync.efi,EFI\\BOOT\\tools\\gptsync.efi,EFI\\gptsync.efi,\\gptsync.efi"
#   define GDISK_NAMES \
L"EFI\\tools\\gdisk.efi,EFI\\BOOT\\tools\\gdisk.efi,EFI\\gdisk.efi,\\gdisk.efi"
#   define NETBOOT_NAMES \
L"EFI\\tools\\ipxe.efi,EFI\\BOOT\\tools\\ipxe.efi,EFI\\ipxe.efi,\\ipxe.efi"
#   define MEMTEST_NAMES           L"memtest.efi,memtest86.efi,boot.efi"
#   define FALLBACK_SKIPNAME       L"bootx64.efi,bootia32.efi,bootaa64.efi,bootmips.efi"
#   define FALLBACK_FULLNAME       L"EFI\\BOOT\\boot.efi" // Not really correct
#   define FALLBACK_BASENAME       L"boot.efi"            // Not really correct
#   define NETBOOT_FILES           L"ipxe.efi"
#   define GPTSYNC_FILES           L"gptsync.efi"
#   define GDISK_FILES             L"gdisk.efi"
#   define SHELL_FILES             L"shell.efi"
#   define NVRAMCLEAN_FILES        L"CleanNvram.efi"
#endif

#define BASE_LINUX_DISTROS \
L"Alpine,Arch,Artful,Bionic,Cachy,Centos,Chakra,Crunchbang,Debian,Deepin,\
Devuan,Elementary,Endeavour,Fedora,Frugalware,Gentoo,Gummiboot,LinuxMint,\
Mageia,Mandriva,Manjaro,NixOS,OpenSUSE,Redhat,Slackware,SUSE,\
Trusty,Kubuntu,Lubuntu,Xubuntu,Ubuntu,Void,Xenial,Zesty"

#define MAIN_LINUX_DISTROS \
L"Arch,Debian,Deepin,Elementary,Endeavour,Fedora,Gentoo,\
LinuxMint,Manjaro,NixOS,OpenSUSE,Redhat,Slackware,Ubuntu,Zorin"

LOADER_ENTRY * InitializeLoaderEntry (IN LOADER_ENTRY *Entry);

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

BOOLEAN ShouldScan (REFIT_VOLUME *Volume, CHAR16 *Path);

#endif

/* EOF */
