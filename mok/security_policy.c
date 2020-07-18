/*
 * Copyright 2012 <James.Bottomley@HansenPartnership.com>
 *
 * see COPYING file
 *
 * Install and remove a platform security2 override policy
 */

#include <global.h>

#include "guid.h"
#include "../refind/lib.h"
#include "simple_file.h"
#include "../include/refit_call_wrapper.h"
#include "mok.h"

#include <security_policy.h>

/*
 * See the UEFI Platform Initialization manual (Vol2: DXE) for this
 */
struct _EFI_SECURITY2_PROTOCOL;
struct _EFI_SECURITY_PROTOCOL;
struct _EFI_DEVICE_PATH_PROTOCOL;
typedef struct _EFI_SECURITY2_PROTOCOL EFI_SECURITY2_PROTOCOL;
typedef struct _EFI_SECURITY_PROTOCOL EFI_SECURITY_PROTOCOL;

#if defined(EFIX64)
#define MSABI __attribute__((ms_abi))
#else
#define MSABI
#endif

typedef EFI_STATUS (MSABI *EFI_SECURITY_FILE_AUTHENTICATION_STATE) (
         const EFI_SECURITY_PROTOCOL *This,
         UINT32 AuthenticationStatus,
         const EFI_DEVICE_PATH_PROTOCOL *File
                             );
typedef EFI_STATUS (MSABI *EFI_SECURITY2_FILE_AUTHENTICATION) (
         const EFI_SECURITY2_PROTOCOL *This,
         const EFI_DEVICE_PATH_PROTOCOL *DevicePath,
         VOID *FileBuffer,
         UINTN FileSize,
         BOOLEAN  BootPolicy
                             );

struct _EFI_SECURITY2_PROTOCOL {
   EFI_SECURITY2_FILE_AUTHENTICATION FileAuthentication;
};

struct _EFI_SECURITY_PROTOCOL {
   EFI_SECURITY_FILE_AUTHENTICATION_STATE  FileAuthenticationState;
};


static EFI_SECURITY_FILE_AUTHENTICATION_STATE esfas = NULL;
static EFI_SECURITY2_FILE_AUTHENTICATION es2fa = NULL;

// Perform shim/MOK and Secure Boot authentication on a binary that's already been
// loaded into memory. This function does the platform SB authentication first
// but preserves its return value in case of its failure, so that it can be
// returned in case of a shim/MOK authentication failure. This is done because
// the SB failure code seems to vary from one implementation to another, and I
// don't want to interfere with that at this time.
static MSABI EFI_STATUS
security2_policy_authentication (
   const EFI_SECURITY2_PROTOCOL *This,
   const EFI_DEVICE_PATH_PROTOCOL *DevicePath,
   VOID *FileBuffer,
   UINTN FileSize,
   BOOLEAN  BootPolicy
             )
{
   EFI_STATUS Status;

   /* Chain original security policy */
   Status = uefi_call_wrapper(es2fa, 5, This, DevicePath, FileBuffer, FileSize, BootPolicy);

   /* if OK, don't bother with MOK check */
   if (Status == EFI_SUCCESS)
      return Status;

   if (ShimValidate(FileBuffer, FileSize)) {
      return EFI_SUCCESS;
   } else {
      return Status;
   }
} // EFI_STATUS security2_policy_authentication()

// Perform both shim/MOK and platform Secure Boot authentication. This function loads
// the file and performs shim/MOK authentication first simply to avoid double loads
// of Linux kernels, which are much more likely to be shim/MOK-signed than platform-signed,
// since kernels are big and can take several seconds to load on some computers and
// filesystems. This also has the effect of returning whatever the platform code is for
// authentication failure, be it EFI_ACCESS_DENIED, EFI_SECURITY_VIOLATION, or something
// else. (This seems to vary between implementations.)
static MSABI EFI_STATUS
security_policy_authentication (
   const EFI_SECURITY_PROTOCOL *This,
   UINT32 AuthenticationStatus,
   const EFI_DEVICE_PATH_PROTOCOL *DevicePathConst
   )
{
   EFI_STATUS        Status;
   EFI_DEVICE_PATH   *DevPath, *OrigDevPath;
   EFI_HANDLE        h;
   EFI_FILE          *f;
   VOID              *FileBuffer;
   UINTN             FileSize;
   CHAR16            *DevPathStr;

   if (DevicePathConst == NULL) {
      return EFI_INVALID_PARAMETER;
   } else {
      DevPath = OrigDevPath = DuplicateDevicePath((EFI_DEVICE_PATH *)DevicePathConst);
   }

   Status = refit_call3_wrapper(BS->LocateDevicePath, &SIMPLE_FS_PROTOCOL, &DevPath, &h);
   if (Status != EFI_SUCCESS)
      goto out;

   DevPathStr = DevicePathToStr(DevPath);

   Status = simple_file_open_by_handle(h, DevPathStr, &f, EFI_FILE_MODE_READ);
   MyFreePool(DevPathStr);
   if (Status != EFI_SUCCESS)
      goto out;

   Status = simple_file_read_all(f, &FileSize, &FileBuffer);
   simple_file_close(f);
   if (Status != EFI_SUCCESS)
      goto out;

   if (ShimValidate(FileBuffer, FileSize)) {
      Status = EFI_SUCCESS;
   } else {
      // Try using the platform's native policy....
      Status = uefi_call_wrapper(esfas, 3, This, AuthenticationStatus, DevicePathConst);
   }
   FreePool(FileBuffer);

 out:
   MyFreePool(OrigDevPath);
   return Status;
} // EFI_STATUS security_policy_authentication()

EFI_STATUS
security_policy_install(void)
{
   EFI_SECURITY_PROTOCOL *security_protocol;
   EFI_SECURITY2_PROTOCOL *security2_protocol = NULL;
   EFI_STATUS status;

   if (esfas) {
      /* Already Installed */
      return EFI_ALREADY_STARTED;
   }

   /* Don't bother with status here.  The call is allowed
    * to fail, since SECURITY2 was introduced in PI 1.2.1
    * If it fails, use security2_protocol == NULL as indicator */
   uefi_call_wrapper(BS->LocateProtocol, 3, &SECURITY2_PROTOCOL_GUID, NULL, (VOID**) &security2_protocol);

   status = uefi_call_wrapper(BS->LocateProtocol, 3, &SECURITY_PROTOCOL_GUID, NULL, (VOID**) &security_protocol);
   if (status != EFI_SUCCESS)
      /* This one is mandatory, so there's a serious problem */
      return status;

   if (security2_protocol) {
      es2fa = security2_protocol->FileAuthentication;
      security2_protocol->FileAuthentication = security2_policy_authentication;
   }

   esfas = security_protocol->FileAuthenticationState;
   security_protocol->FileAuthenticationState = security_policy_authentication;
   return EFI_SUCCESS;
}

EFI_STATUS
security_policy_uninstall(void)
{
   EFI_STATUS status;

   if (esfas) {
      EFI_SECURITY_PROTOCOL *security_protocol;

      status = uefi_call_wrapper(BS->LocateProtocol, 3, &SECURITY_PROTOCOL_GUID, NULL, (VOID**) &security_protocol);

      if (status != EFI_SUCCESS)
         return status;

      security_protocol->FileAuthenticationState = esfas;
      esfas = NULL;
   } else {
      /* nothing installed */
      return EFI_NOT_STARTED;
   }

   if (es2fa) {
      EFI_SECURITY2_PROTOCOL *security2_protocol;

      status = uefi_call_wrapper(BS->LocateProtocol, 3, &SECURITY2_PROTOCOL_GUID, NULL, (VOID**) &security2_protocol);

      if (status != EFI_SUCCESS)
         return status;

      security2_protocol->FileAuthentication = es2fa;
      es2fa = NULL;
   }

   return EFI_SUCCESS;
}
