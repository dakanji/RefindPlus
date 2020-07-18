/*
 * refit/config.h
 * General header file
 *
 * Copyright (c) 2006-2009 Christoph Pfisterer
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
 * Modifications copyright (c) 2012-2020 Roderick W. Smith
 * 
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), a copy of which must be distributed
 * with this source code or binaries made from it.
 * 
 */

#ifndef __CONFIG_H_
#define __CONFIG_H_

#ifdef __MAKEWITH_GNUEFI
#include "efi.h"
#else
#include "../include/tiano_includes.h"
#endif
#include "global.h"


//
// config module
//

typedef struct {
    UINT8   *Buffer;
    UINTN   BufferSize;
    UINTN   Encoding;
    CHAR8   *Current8Ptr;
    CHAR8   *End8Ptr;
    CHAR16  *Current16Ptr;
    CHAR16  *End16Ptr;
} REFIT_FILE;

#define CONFIG_FILE_NAME         L"refind.conf"
// Note: Below is combined with MOK_NAMES and FWUPDATE_NAMES to make default
#if defined (EFIX64)
#define DONT_SCAN_FILES L"shim.efi,shim-fedora.efi,shim-centos.efi,shimx64.efi,PreLoader.efi,TextMode.efi,ebounce.efi,GraphicsConsole.efi,bootmgr.efi,fbx64.efi"
#elif defined(EFI32)
#define DONT_SCAN_FILES L"shim.efi,shim-fedora.efi,shim-centos.efi,shimx64.efi,PreLoader.efi,TextMode.efi,ebounce.efi,GraphicsConsole.efi,bootmgr.efi,fbia32.efi"
#elif defined(EFIAARCH64)
#define DONT_SCAN_FILES L"shim.efi,shim-fedora.efi,shim-centos.efi,shimx64.efi,PreLoader.efi,TextMode.efi,ebounce.efi,GraphicsConsole.efi,bootmgr.efi,fbaa64.efi"
#else
#define DONT_SCAN_FILES L"shim.efi,shim-fedora.efi,shim-centos.efi,shimx64.efi,PreLoader.efi,TextMode.efi,ebounce.efi,GraphicsConsole.efi,bootmgr.efi"
#endif
#define DONT_SCAN_VOLUMES L"LRS_ESP"
#define ALSO_SCAN_DIRS L"boot,@/boot"

EFI_STATUS ReadFile(IN EFI_FILE_HANDLE BaseDir, CHAR16 *FileName, REFIT_FILE *File, UINTN *size);
VOID ReadConfig(CHAR16 *FileName);
VOID ScanUserConfigured(CHAR16 *FileName);
UINTN ReadTokenLine(IN REFIT_FILE *File, OUT CHAR16 ***TokenList);
VOID FreeTokenLine(IN OUT CHAR16 ***TokenList, IN OUT UINTN *TokenCount);
REFIT_FILE * ReadLinuxOptionsFile(IN CHAR16 *LoaderPath, IN REFIT_VOLUME *Volume);
CHAR16 * GetFirstOptionsFromFile(IN CHAR16 *LoaderPath, IN REFIT_VOLUME *Volume);

#endif

/* EOF */
