/* mok/mok.c
 *
 * Based mostly on shim.c by Matthew J. Garrett/Red Hat (see below
 * copyright notice).
 *
 * Code to perform Secure Boot verification of boot loader programs
 * using the Shim program and its Machine Owner Keys (MOKs), to
 * supplement standard Secure Boot checks performed by the firmware.
 *
 */

/*
 * shim - trivial UEFI first-stage bootloader
 *
 * Copyright 2012 Red Hat, Inc <mjg@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Significant portions of this code are derived from Tianocore
 * (http://tianocore.sf.net) and are Copyright 2009-2012 Intel
 * Corporation.
 */

#include "global.h"
#include "mok.h"
#include "../include/refit_call_wrapper.h"
#include "../BootMaster/lib.h"
#include "../BootMaster/screenmgt.h"


/*
 * Check whether we are in Secure Boot and user mode
 */
BOOLEAN secure_mode (VOID) {
    EFI_STATUS  status;
    EFI_GUID    global_var = EFI_GLOBAL_VARIABLE;
    UINTN       charsize;
    UINT8      *sb        = NULL;
    UINT8      *setupmode = NULL;

    static BOOLEAN DoneOnce   = FALSE;
    static BOOLEAN SecureMode = FALSE;

    if (DoneOnce) {
        return SecureMode;
    }

    status = EfivarGetRaw (
        &global_var,
        L"SecureBoot",
        (VOID **) &sb,
        &charsize
    );

    /* FIXME - more paranoia here? */
    if (status != EFI_SUCCESS ||
        charsize != sizeof (CHAR8) ||
        *sb != 1
    ) {
        SecureMode = FALSE;
    }
    else {
        status = EfivarGetRaw (
            &global_var,
            L"SetupMode",
            (VOID **) &setupmode,
            &charsize
        );

        if (status == EFI_SUCCESS &&
            charsize == sizeof (CHAR8) &&
            *setupmode == 1
        ) {
            SecureMode = FALSE;
        }
        else {
            SecureMode = TRUE;
        }
    }

    DoneOnce = TRUE;

    MyFreePool (&sb);
    MyFreePool (&setupmode);

    return SecureMode;
} // secure_mode()

// Returns TRUE if the shim program is available to verify binaries,
// FALSE if not
BOOLEAN ShimLoaded (VOID) {
    SHIM_LOCK   *shim_lock;
    EFI_GUID    ShimLockGuid = SHIM_LOCK_GUID;

    return (
        REFIT_CALL_3_WRAPPER(
            gBS->LocateProtocol,
            &ShimLockGuid,
            NULL,
            (VOID **) &shim_lock
        ) == EFI_SUCCESS
    );
} // ShimLoaded()

// The following is based on the grub_linuxefi_secure_validate() function in Fedora's
// version of GRUB 2.
// Returns TRUE if the specified data is validated by Shim's MOK, FALSE otherwise
BOOLEAN ShimValidate (
    VOID *data,
    UINT32 size
) {
    SHIM_LOCK   *shim_lock;
    EFI_GUID    ShimLockGuid = SHIM_LOCK_GUID;

    if ((data != NULL) &&
        (REFIT_CALL_3_WRAPPER(
            gBS->LocateProtocol,
            &ShimLockGuid,
            NULL,
            (VOID **) &shim_lock
        ) == EFI_SUCCESS)
    ) {
        if (!shim_lock) {
            return FALSE;
        }

        if (shim_lock->shim_verify (data, size) == EFI_SUCCESS) {
            return TRUE;
        }
    }

    return FALSE;
} // BOOLEAN ShimValidate()
