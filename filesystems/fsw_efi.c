/**
 * \file fsw_efi.c
 * EFI host environment code.
 */

/*-
 * Copyright (c) 2006 Christoph Pfisterer
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
 * Changes by Roderick Smith are licensed under the preceding terms.
 */

#include "fsw_efi.h"
#include "fsw_core.h"
#ifdef __MAKEWITH_GNUEFI
#include "edk2/DriverBinding.h"
#include "edk2/ComponentName.h"
#define gMyEfiSimpleFileSystemProtocolGuid FileSystemProtocol
#else
#define REFIND_EFI_DRIVER_BINDING_PROTOCOL EFI_DRIVER_BINDING_PROTOCOL
#define REFIND_EFI_COMPONENT_NAME_PROTOCOL EFI_COMPONENT_NAME_PROTOCOL
#define REFIND_EFI_COMPONENT_NAME_PROTOCOL_GUID EFI_COMPONENT_NAME_PROTOCOL_GUID
#define REFIND_EFI_DRIVER_BINDING_PROTOCOL_GUID EFI_DRIVER_BINDING_PROTOCOL_GUID
#define REFIND_EFI_DEVICE_PATH_PROTOCOL EFI_DEVICE_PATH_PROTOCOL
#define EFI_FILE_SYSTEM_VOLUME_LABEL_INFO_ID    \
    { 0xDB47D7D3,0xFE81, 0x11d3, {0x9A, 0x35, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D} }
#define gMyEfiSimpleFileSystemProtocolGuid gEfiSimpleFileSystemProtocolGuid
#endif

#include "../include/refit_call_wrapper.h"
#include "../include/version.h"

#define DEBUG_LEVEL 0

#ifndef FSTYPE
/** The file system type name to use. */
#define FSTYPE ext2
#endif

EFI_GUID gMyEfiDriverBindingProtocolGuid = REFIND_EFI_DRIVER_BINDING_PROTOCOL_GUID;
EFI_GUID gMyEfiComponentNameProtocolGuid = REFIND_EFI_COMPONENT_NAME_PROTOCOL_GUID;
EFI_GUID gMyEfiDiskIoProtocolGuid = REFIND_EFI_DISK_IO_PROTOCOL_GUID;
EFI_GUID gMyEfiBlockIoProtocolGuid = REFIND_EFI_BLOCK_IO_PROTOCOL_GUID;
EFI_GUID gMyEfiFileInfoGuid = EFI_FILE_INFO_ID;
EFI_GUID gMyEfiFileSystemInfoGuid = EFI_FILE_SYSTEM_INFO_ID;
EFI_GUID gMyEfiFileSystemVolumeLabelInfoIdGuid = EFI_FILE_SYSTEM_VOLUME_LABEL_INFO_ID;

/** Helper macro for stringification. */
#define FSW_EFI_STRINGIFY(x) #x
/** Expands to the EFI driver name given the file system type name. */
#define FSW_EFI_DRIVER_NAME(t) L"rEFInd 0.13.3 " FSW_EFI_STRINGIFY(t) L" File System Driver"

// function prototypes

EFI_STATUS EFIAPI fsw_efi_DriverBinding_Supported(IN REFIND_EFI_DRIVER_BINDING_PROTOCOL  *This,
                                                  IN EFI_HANDLE                          ControllerHandle,
                                                  IN REFIND_EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath);
EFI_STATUS EFIAPI fsw_efi_DriverBinding_Start(IN REFIND_EFI_DRIVER_BINDING_PROTOCOL  *This,
                                              IN EFI_HANDLE                          ControllerHandle,
                                              IN REFIND_EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath);
EFI_STATUS EFIAPI fsw_efi_DriverBinding_Stop(IN REFIND_EFI_DRIVER_BINDING_PROTOCOL  *This,
                                             IN EFI_HANDLE                          ControllerHandle,
                                             IN UINTN                               NumberOfChildren,
                                             IN EFI_HANDLE                          *ChildHandleBuffer);

EFI_STATUS EFIAPI fsw_efi_ComponentName_GetDriverName(IN  REFIND_EFI_COMPONENT_NAME_PROTOCOL  *This,
                                                      IN  CHAR8                               *Language,
                                                      OUT CHAR16                              **DriverName);
EFI_STATUS EFIAPI fsw_efi_ComponentName_GetControllerName(IN  REFIND_EFI_COMPONENT_NAME_PROTOCOL  *This,
                                                          IN  EFI_HANDLE                          ControllerHandle,
                                                          IN  EFI_HANDLE                          ChildHandle  OPTIONAL,
                                                          IN  CHAR8                               *Language,
                                                          OUT CHAR16                              **ControllerName);

void EFIAPI fsw_efi_change_blocksize(struct fsw_volume *vol,
                              fsw_u32 old_phys_blocksize, fsw_u32 old_log_blocksize,
                              fsw_u32 new_phys_blocksize, fsw_u32 new_log_blocksize);
fsw_status_t EFIAPI fsw_efi_read_block(struct fsw_volume *vol, fsw_u64 phys_bno, void *buffer);

EFI_STATUS fsw_efi_map_status(fsw_status_t fsw_status, FSW_VOLUME_DATA *Volume);

EFI_STATUS EFIAPI fsw_efi_FileSystem_OpenVolume(IN EFI_FILE_IO_INTERFACE *This,
                                                OUT EFI_FILE **Root);
EFI_STATUS fsw_efi_dnode_to_FileHandle(IN struct fsw_dnode *dno,
                                       OUT EFI_FILE **NewFileHandle);

EFI_STATUS fsw_efi_file_read(IN FSW_FILE_DATA *File,
                             IN OUT UINTN *BufferSize,
                             OUT VOID *Buffer);
EFI_STATUS fsw_efi_file_getpos(IN FSW_FILE_DATA *File,
                               OUT UINT64 *Position);
EFI_STATUS fsw_efi_file_setpos(IN FSW_FILE_DATA *File,
                               IN UINT64 Position);

EFI_STATUS fsw_efi_dir_open(IN FSW_FILE_DATA *File,
                            OUT EFI_FILE **NewHandle,
                            IN CHAR16 *FileName,
                            IN UINT64 OpenMode,
                            IN UINT64 Attributes);
EFI_STATUS fsw_efi_dir_read(IN FSW_FILE_DATA *File,
                            IN OUT UINTN *BufferSize,
                            OUT VOID *Buffer);
EFI_STATUS fsw_efi_dir_setpos(IN FSW_FILE_DATA *File,
                              IN UINT64 Position);

EFI_STATUS fsw_efi_dnode_getinfo(IN FSW_FILE_DATA *File,
                                 IN EFI_GUID *InformationType,
                                 IN OUT UINTN *BufferSize,
                                 OUT VOID *Buffer);
EFI_STATUS fsw_efi_dnode_fill_FileInfo(IN FSW_VOLUME_DATA *Volume,
                                       IN struct fsw_dnode *dno,
                                       IN OUT UINTN *BufferSize,
                                       OUT VOID *Buffer);

/**
 * Structure for holding disk cache data.
 */

#define CACHE_SIZE 131072 /* 128KiB */
struct cache_data {
   fsw_u8            *Cache;
   fsw_u64           CacheStart;
   BOOLEAN           CacheValid;
   FSW_VOLUME_DATA   *Volume; // NOTE: Do not deallocate; copied here to ID volume
};

#define NUM_CACHES 2 /* Don't increase without modifying fsw_efi_read_block() */
static struct cache_data    Caches[NUM_CACHES];
static int LastRead = -1;

/**
 * Interface structure for the EFI Driver Binding protocol.
 */

REFIND_EFI_DRIVER_BINDING_PROTOCOL fsw_efi_DriverBinding_table = {
    fsw_efi_DriverBinding_Supported,
    fsw_efi_DriverBinding_Start,
    fsw_efi_DriverBinding_Stop,
    0x10,
    NULL,
    NULL
};

/**
 * Interface structure for the EFI Component Name protocol.
 */

REFIND_EFI_COMPONENT_NAME_PROTOCOL fsw_efi_ComponentName_table = {
    fsw_efi_ComponentName_GetDriverName,
    fsw_efi_ComponentName_GetControllerName,
    (CHAR8*) "eng"
};

/**
 * Dispatch table for our FSW host driver.
 */

struct fsw_host_table   fsw_efi_host_table = {
    FSW_STRING_TYPE_UTF16,

    fsw_efi_change_blocksize,
    fsw_efi_read_block
};

extern struct fsw_fstype_table   FSW_FSTYPE_TABLE_NAME(FSTYPE);


VOID EFIAPI fsw_efi_clear_cache(VOID) {
   int i;

   // clear the cache
   for (i = 0; i < NUM_CACHES; i++) {
      if (Caches[i].Cache != NULL) {
         FreePool(Caches[i].Cache);
         Caches[i].Cache = NULL;
      } // if
            Caches[i].CacheStart = 0;
            Caches[i].CacheValid = FALSE;
            Caches[i].Volume = NULL;
   }
   LastRead = -1;
} // VOID EFIAPI fsw_efi_clear_cache();

/**
 * Image entry point. Installs the Driver Binding and Component Name protocols
 * on the image's handle. Actually mounting a file system is initiated through
 * the Driver Binding protocol at the firmware's request.
 */
EFI_STATUS EFIAPI fsw_efi_main(IN EFI_HANDLE         ImageHandle,
                               IN EFI_SYSTEM_TABLE   *SystemTable)
{
    EFI_STATUS  Status;

#ifndef __MAKEWITH_TIANO
    // Not available in EDK2 toolkit
    InitializeLib(ImageHandle, SystemTable);
#endif

    // complete Driver Binding protocol instance
    fsw_efi_DriverBinding_table.ImageHandle          = ImageHandle;
    fsw_efi_DriverBinding_table.DriverBindingHandle  = ImageHandle;
    // install Driver Binding protocol
    Status = refit_call4_wrapper(BS->InstallProtocolInterface, &fsw_efi_DriverBinding_table.DriverBindingHandle,
                                          &gMyEfiDriverBindingProtocolGuid,
                                          EFI_NATIVE_INTERFACE,
                                          &fsw_efi_DriverBinding_table);
    if (EFI_ERROR (Status)) {
        return Status;
    }

    // install Component Name protocol
    Status = refit_call4_wrapper(BS->InstallProtocolInterface, &fsw_efi_DriverBinding_table.DriverBindingHandle,
                                          &gMyEfiComponentNameProtocolGuid,
                                          EFI_NATIVE_INTERFACE,
                                          &fsw_efi_ComponentName_table);
    if (EFI_ERROR (Status)) {
        return Status;
    }

//	OverrideFunctions();
//   Msg = NULL;
//   msgCursor = NULL;
//   Status = BS->LocateProtocol(&gMsgLogProtocolGuid, NULL, (VOID **) &Msg);
//   if (!EFI_ERROR(Status) && (Msg != NULL)) {
//     msgCursor = Msg->Cursor;
//     BootLog("MsgLog installed into VBoxFs\n");
//   }

    return EFI_SUCCESS;
}

#ifdef __MAKEWITH_GNUEFI
EFI_DRIVER_ENTRY_POINT(fsw_efi_main)
#endif

/**
 * Driver Binding EFI protocol, Supported function. This function is called by EFI
 * to test if this driver can handle a certain device. Our implementation only checks
 * if the device is a disk (i.e. that it supports the Block I/O and Disk I/O protocols)
 * and implicitly checks if the disk is already in use by another driver.
 */

EFI_STATUS EFIAPI fsw_efi_DriverBinding_Supported(IN REFIND_EFI_DRIVER_BINDING_PROTOCOL  *This,
                                                  IN EFI_HANDLE                   ControllerHandle,
                                                  IN REFIND_EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath)
{
    EFI_STATUS          Status;
    EFI_DISK_IO         *DiskIo;

    // we check for both DiskIO and BlockIO protocols

    // first, open DiskIO
    Status = refit_call6_wrapper(BS->OpenProtocol, ControllerHandle,
                              &gMyEfiDiskIoProtocolGuid,
                              (VOID **) &DiskIo,
                              This->DriverBindingHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (EFI_ERROR(Status))
        return Status;

    // we were just checking, close it again
    refit_call4_wrapper(BS->CloseProtocol, ControllerHandle,
                      &gMyEfiDiskIoProtocolGuid,
                      This->DriverBindingHandle,
                      ControllerHandle);

    // next, check BlockIO without actually opening it
    Status = refit_call6_wrapper(BS->OpenProtocol, ControllerHandle,
                              &gMyEfiBlockIoProtocolGuid,
                              NULL,
                              This->DriverBindingHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_TEST_PROTOCOL);
    return Status;
}

/**
 * Driver Binding EFI protocol, Start function. This function is called by EFI
 * to start driving the given device. It is still possible at this point to
 * return EFI_UNSUPPORTED, and in fact we will do so if the file system driver
 * cannot find the superblock signature (or equivalent) that it expects.
 *
 * This function allocates memory for a per-volume structure, opens the
 * required protocols (just Disk I/O in our case, Block I/O is only looked
 * at to get the MediaId field), and lets the FSW core mount the file system.
 * If successful, an EFI Simple File System protocol is exported on the
 * device handle.
 */

EFI_STATUS EFIAPI fsw_efi_DriverBinding_Start(IN REFIND_EFI_DRIVER_BINDING_PROTOCOL  *This,
                                              IN EFI_HANDLE                   ControllerHandle,
                                              IN REFIND_EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath)
{
    EFI_STATUS          Status;
    EFI_BLOCK_IO        *BlockIo;
    EFI_DISK_IO         *DiskIo;
    FSW_VOLUME_DATA     *Volume;

#if DEBUG_LEVEL
    Print(L"fsw_efi_DriverBinding_Start\n");
#endif

    // open consumed protocols
    Status = refit_call6_wrapper(BS->OpenProtocol, ControllerHandle,
                              &gMyEfiBlockIoProtocolGuid,
                              (VOID **) &BlockIo,
                              This->DriverBindingHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_GET_PROTOCOL);   // NOTE: we only want to look at the MediaId
    if (EFI_ERROR(Status)) {
//        Print(L"Fsw ERROR: OpenProtocol(BlockIo) returned %x\n", Status);
        return Status;
    }

    Status = refit_call6_wrapper(BS->OpenProtocol, ControllerHandle,
                              &gMyEfiDiskIoProtocolGuid,
                              (VOID **) &DiskIo,
                              This->DriverBindingHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (EFI_ERROR(Status)) {
        Print(L"Fsw ERROR: OpenProtocol(DiskIo) returned %x\n", Status);
        return Status;
    }

    // allocate volume structure
    Volume = AllocateZeroPool(sizeof(FSW_VOLUME_DATA));
    Volume->Signature       = FSW_VOLUME_DATA_SIGNATURE;
    Volume->Handle          = ControllerHandle;
    Volume->DiskIo          = DiskIo;
    Volume->MediaId         = BlockIo->Media->MediaId;
    Volume->LastIOStatus    = EFI_SUCCESS;

    // mount the filesystem
    Status = fsw_efi_map_status(fsw_mount(Volume, &fsw_efi_host_table,
                                          &FSW_FSTYPE_TABLE_NAME(FSTYPE), &Volume->vol),
                                Volume);
    if (!EFI_ERROR(Status)) {
        // register the SimpleFileSystem protocol
        Volume->FileSystem.Revision     = EFI_FILE_IO_INTERFACE_REVISION;
        Volume->FileSystem.OpenVolume   = fsw_efi_FileSystem_OpenVolume;
        Status = refit_call4_wrapper(BS->InstallMultipleProtocolInterfaces, &ControllerHandle,
                                                       &gMyEfiSimpleFileSystemProtocolGuid,
                                                       &Volume->FileSystem,
                                                       NULL);
        if (EFI_ERROR(Status)) {
//            Print(L"Fsw ERROR: InstallMultipleProtocolInterfaces returned %x\n", Status);
        }
    }

    // on errors, close the opened protocols
    if (EFI_ERROR(Status)) {
        if (Volume->vol != NULL)
            fsw_unmount(Volume->vol);
        FreePool(Volume);

        refit_call4_wrapper(BS->CloseProtocol, ControllerHandle,
                          &gMyEfiDiskIoProtocolGuid,
                          This->DriverBindingHandle,
                          ControllerHandle);
    }
    return Status;
}

/**
 * Driver Binding EFI protocol, Stop function. This function is called by EFI
 * to stop the driver on the given device. This translates to an unmount
 * call for the FSW core.
 *
 * We assume that all file handles on the volume have been closed before
 * the driver is stopped. At least with the EFI shell, that is actually the
 * case; it closes all file handles between commands.
 */

EFI_STATUS EFIAPI fsw_efi_DriverBinding_Stop(IN  REFIND_EFI_DRIVER_BINDING_PROTOCOL  *This,
                                             IN  EFI_HANDLE                   ControllerHandle,
                                             IN  UINTN                        NumberOfChildren,
                                             IN  EFI_HANDLE                   *ChildHandleBuffer)
{
    EFI_STATUS          Status;
    EFI_FILE_IO_INTERFACE *FileSystem;
    FSW_VOLUME_DATA     *Volume;

#if DEBUG_LEVEL
    Print(L"fsw_efi_DriverBinding_Stop\n");
#endif

    // get the installed SimpleFileSystem interface
    Status = refit_call6_wrapper(BS->OpenProtocol, ControllerHandle,
                              &gMyEfiSimpleFileSystemProtocolGuid,
                              (VOID **) &FileSystem,
                              This->DriverBindingHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status))
        return EFI_UNSUPPORTED;

    // get private data structure
    Volume = FSW_VOLUME_FROM_FILE_SYSTEM(FileSystem);

    // uninstall Simple File System protocol
    Status = refit_call4_wrapper(BS->UninstallMultipleProtocolInterfaces, ControllerHandle,
                                                     &gMyEfiSimpleFileSystemProtocolGuid, &Volume->FileSystem,
                                                     NULL);
    if (EFI_ERROR(Status)) {
 //       Print(L"Fsw ERROR: UninstallMultipleProtocolInterfaces returned %x\n", Status);
        return Status;
    }
#if DEBUG_LEVEL
    Print(L"fsw_efi_DriverBinding_Stop: protocol uninstalled successfully\n");
#endif

    // release private data structure
    if (Volume->vol != NULL)
        fsw_unmount(Volume->vol);
    FreePool(Volume);

    // close the consumed protocols
    Status = refit_call4_wrapper(BS->CloseProtocol, ControllerHandle,
                               &gMyEfiDiskIoProtocolGuid,
                               This->DriverBindingHandle,
                               ControllerHandle);

    // clear the cache
    fsw_efi_clear_cache();

    return Status;
}

/**
 * Component Name EFI protocol, GetDriverName function. Used by the EFI
 * environment to inquire the name of this driver. The name returned is
 * based on the file system type actually used in compilation.
 */

EFI_STATUS EFIAPI fsw_efi_ComponentName_GetDriverName(IN  REFIND_EFI_COMPONENT_NAME_PROTOCOL  *This,
                                                      IN  CHAR8                        *Language,
                                                      OUT CHAR16                       **DriverName)
{
    if (Language == NULL || DriverName == NULL)
        return EFI_INVALID_PARAMETER;

    if (Language[0] == 'e' && Language[1] == 'n' && Language[2] == 'g' && Language[3] == 0) {
        *DriverName = FSW_EFI_DRIVER_NAME(FSTYPE);
        return EFI_SUCCESS;
    }
    return EFI_UNSUPPORTED;
}

/**
 * Component Name EFI protocol, GetControllerName function. Not implemented
 * because this is not a "bus" driver in the sense of the EFI Driver Model.
 */

EFI_STATUS EFIAPI fsw_efi_ComponentName_GetControllerName(IN  REFIND_EFI_COMPONENT_NAME_PROTOCOL    *This,
                                                          IN  EFI_HANDLE                     ControllerHandle,
                                                          IN  EFI_HANDLE                     ChildHandle  OPTIONAL,
                                                          IN  CHAR8                          *Language,
                                                          OUT CHAR16                         **ControllerName)
{
    return EFI_UNSUPPORTED;
}

/**
 * FSW interface function for block size changes. This function is called by the FSW core
 * when the file system driver changes the block sizes for the volume.
 */

void EFIAPI fsw_efi_change_blocksize(struct fsw_volume *vol,
                              fsw_u32 old_phys_blocksize, fsw_u32 old_log_blocksize,
                              fsw_u32 new_phys_blocksize, fsw_u32 new_log_blocksize)
{
    // nothing to do
}

/**
 * FSW interface function to read data blocks. This function is called by the FSW core
 * to read a block of data from the device. The buffer is allocated by the core code.
 * Two caches are maintained, so as to improve performance on some systems. (VirtualBox
 * is particularly susceptible to performance problems with an uncached driver -- the
 * ext2 driver can take 200 seconds to load a Linux kernel under VirtualBox, whereas
 * the time is more like 3 seconds with a cache!) Two independent caches are maintained
 * because the ext2fs driver tends to alternate between accessing two parts of the
 * disk.
 */

fsw_status_t EFIAPI fsw_efi_read_block(struct fsw_volume *vol, fsw_u64 phys_bno, void *buffer) {
   int              i, ReadCache = -1;
   FSW_VOLUME_DATA  *Volume = (FSW_VOLUME_DATA *)vol->host_data;
   EFI_STATUS       Status = EFI_SUCCESS;
   BOOLEAN          ReadOneBlock = FALSE;
   UINT64           StartRead = (UINT64) phys_bno * (UINT64) vol->phys_blocksize;

   if (buffer == NULL)
      return (fsw_status_t) EFI_BAD_BUFFER_SIZE;

   // Initialize static data structures, if necessary....
   if (LastRead < 0) {
      fsw_efi_clear_cache();
   } // if

   // Look for a cache hit on the current query....
   i = 0;
   do {
      if ((Caches[i].Volume == Volume) &&
          (Caches[i].CacheValid == TRUE) &&
          (StartRead >= Caches[i].CacheStart) &&
          ((StartRead + vol->phys_blocksize) <= (Caches[i].CacheStart + CACHE_SIZE))) {
         ReadCache = i;
      }
      i++;
   } while ((i < NUM_CACHES) && (ReadCache < 0));

   // No cache hit found; load new cache and pass it on....
   if (ReadCache < 0) {
      if (LastRead == -1)
         LastRead = 1;
      ReadCache = 1 - LastRead; // NOTE: If NUM_CACHES > 2, this must become more complex
      Caches[ReadCache].CacheValid = FALSE;
      if (Caches[ReadCache].Cache == NULL)
         Caches[ReadCache].Cache = AllocatePool(CACHE_SIZE);
      if (Caches[ReadCache].Cache != NULL) {
         // TODO: Below call hangs on my 32-bit Mac Mini when compiled with GNU-EFI.
         // The same binary is fine under VirtualBox, and the same call is fine when
         // compiled with Tianocore. Further clue: Omitting "Status =" avoids the
         // hang but produces a failure to mount the filesystem, even when the same
         // change is made to later similar call. Calling Volume->DiskIo->ReadDisk()
         // directly (without refit_call5_wrapper()) changes nothing. Placing Print()
         // statements at the start and end of the function, and before and after the
         // ReadDisk() call, suggests that when it fails, the program is executing
         // code starting mid-function, so there seems to be something messed up in
         // the way the function is being called. FIGURE THIS OUT!
         Status = refit_call5_wrapper(Volume->DiskIo->ReadDisk, Volume->DiskIo, Volume->MediaId,
                                      StartRead, (UINTN) CACHE_SIZE, (VOID*) Caches[ReadCache].Cache);
         if (!EFI_ERROR(Status)) {
            Caches[ReadCache].CacheStart = StartRead;
            Caches[ReadCache].CacheValid = TRUE;
            Caches[ReadCache].Volume = Volume;
            LastRead = ReadCache;
         } else {
            ReadOneBlock = TRUE;
         }
      } else {
         ReadOneBlock = TRUE;
      } // if cache memory allocated
   } // if (ReadCache < 0)

   if (Caches[ReadCache].Cache != NULL && Caches[ReadCache].CacheValid == TRUE && vol->phys_blocksize > 0) {
      CopyMem(buffer, &Caches[ReadCache].Cache[StartRead - Caches[ReadCache].CacheStart], vol->phys_blocksize);
   } else {
      ReadOneBlock = TRUE;
   }

   if (ReadOneBlock) { // Something's failed, so try a simple disk read of one block....
      Status = refit_call5_wrapper(Volume->DiskIo->ReadDisk, Volume->DiskIo, Volume->MediaId,
                                   phys_bno * vol->phys_blocksize,
                                   (UINTN) vol->phys_blocksize,
                                   (VOID*) buffer);
   }
   Volume->LastIOStatus = Status;

   return Status;
} // fsw_status_t *fsw_efi_read_block()

/**
 * Map FSW status codes to EFI status codes. The FSW_IO_ERROR code is only produced
 * by fsw_efi_read_block, so we map it back to the EFI status code remembered from
 * the last I/O operation.
 */

EFI_STATUS fsw_efi_map_status(fsw_status_t fsw_status, FSW_VOLUME_DATA *Volume)
{
    switch (fsw_status) {
        case FSW_SUCCESS:
            return EFI_SUCCESS;
        case FSW_OUT_OF_MEMORY:
            return EFI_VOLUME_CORRUPTED;
        case FSW_IO_ERROR:
            return Volume->LastIOStatus;
        case FSW_UNSUPPORTED:
            return EFI_UNSUPPORTED;
        case FSW_NOT_FOUND:
            return EFI_NOT_FOUND;
        case FSW_VOLUME_CORRUPTED:
            return EFI_VOLUME_CORRUPTED;
        default:
            return EFI_DEVICE_ERROR;
    }
}

/**
 * File System EFI protocol, OpenVolume function. Creates a file handle for
 * the root directory and returns it. Note that this function may be called
 * multiple times and returns a new file handle each time. Each returned
 * handle is closed by the client using it.
 */

EFI_STATUS EFIAPI fsw_efi_FileSystem_OpenVolume(IN EFI_FILE_IO_INTERFACE *This,
                                                OUT EFI_FILE **Root)
{
    EFI_STATUS          Status;
    FSW_VOLUME_DATA     *Volume = FSW_VOLUME_FROM_FILE_SYSTEM(This);

#if DEBUG_LEVEL
    Print(L"fsw_efi_FileSystem_OpenVolume\n");
#endif

    fsw_efi_clear_cache();
    Status = fsw_efi_dnode_to_FileHandle(Volume->vol->root, Root);

    return Status;
}

/**
 * File Handle EFI protocol, Open function. Dispatches the call
 * based on the kind of file handle.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_Open(IN EFI_FILE *This,
                                          OUT EFI_FILE **NewHandle,
                                          IN CHAR16 *FileName,
                                          IN UINT64 OpenMode,
                                          IN UINT64 Attributes)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    if (File->Type == FSW_EFI_FILE_TYPE_DIR)
        return fsw_efi_dir_open(File, NewHandle, FileName, OpenMode, Attributes);
    // not supported for regular files
    return EFI_UNSUPPORTED;
}

/**
 * File Handle EFI protocol, Close function. Closes the FSW shandle
 * and frees the memory used for the structure.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_Close(IN EFI_FILE *This)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

#if DEBUG_LEVEL
    Print(L"fsw_efi_FileHandle_Close\n");
#endif

    fsw_shandle_close(&File->shand);
    FreePool(File);

    return EFI_SUCCESS;
}

/**
 * File Handle EFI protocol, Delete function. Calls through to Close
 * and returns a warning because this driver is read-only.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_Delete(IN EFI_FILE *This)
{
    EFI_STATUS          Status;

    Status = refit_call1_wrapper(This->Close, This);
    if (Status == EFI_SUCCESS) {
        // this driver is read-only
        Status = EFI_WARN_DELETE_FAILURE;
    }

    return Status;
}

/**
 * File Handle EFI protocol, Read function. Dispatches the call
 * based on the kind of file handle.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_Read(IN EFI_FILE *This,
                                          IN OUT UINTN *BufferSize,
                                          OUT VOID *Buffer)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    if (File->Type == FSW_EFI_FILE_TYPE_FILE)
        return fsw_efi_file_read(File, BufferSize, Buffer);
    else if (File->Type == FSW_EFI_FILE_TYPE_DIR)
        return fsw_efi_dir_read(File, BufferSize, Buffer);
    return EFI_UNSUPPORTED;
}

/**
 * File Handle EFI protocol, Write function. Returns unsupported status
 * because this driver is read-only.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_Write(IN EFI_FILE *This,
                                           IN OUT UINTN *BufferSize,
                                           IN VOID *Buffer)
{
    // this driver is read-only
    return EFI_WRITE_PROTECTED;
}

/**
 * File Handle EFI protocol, GetPosition function. Dispatches the call
 * based on the kind of file handle.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_GetPosition(IN EFI_FILE *This,
                                                 OUT UINT64 *Position)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    if (File->Type == FSW_EFI_FILE_TYPE_FILE)
        return fsw_efi_file_getpos(File, Position);
    // not defined for directories
    return EFI_UNSUPPORTED;
}

/**
 * File Handle EFI protocol, SetPosition function. Dispatches the call
 * based on the kind of file handle.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_SetPosition(IN EFI_FILE *This,
                                                 IN UINT64 Position)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    if (File->Type == FSW_EFI_FILE_TYPE_FILE)
        return fsw_efi_file_setpos(File, Position);
    else if (File->Type == FSW_EFI_FILE_TYPE_DIR)
        return fsw_efi_dir_setpos(File, Position);
    return EFI_UNSUPPORTED;
}

/**
 * File Handle EFI protocol, GetInfo function. Dispatches to the common
 * function implementing this.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_GetInfo(IN EFI_FILE *This,
                                             IN EFI_GUID *InformationType,
                                             IN OUT UINTN *BufferSize,
                                             OUT VOID *Buffer)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    return fsw_efi_dnode_getinfo(File, InformationType, BufferSize, Buffer);
}

/**
 * File Handle EFI protocol, SetInfo function. Returns unsupported status
 * because this driver is read-only.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_SetInfo(IN EFI_FILE *This,
                                             IN EFI_GUID *InformationType,
                                             IN UINTN BufferSize,
                                             IN VOID *Buffer)
{
    // this driver is read-only
    return EFI_WRITE_PROTECTED;
}

/**
 * File Handle EFI protocol, Flush function. Returns unsupported status
 * because this driver is read-only.
 */

EFI_STATUS EFIAPI fsw_efi_FileHandle_Flush(IN EFI_FILE *This)
{
    // this driver is read-only
    return EFI_WRITE_PROTECTED;
}

/**
 * Set up a file handle for a dnode. This function allocates a data structure
 * for a file handle, opens a FSW shandle and populates the EFI_FILE structure
 * with the interface functions.
 */

EFI_STATUS fsw_efi_dnode_to_FileHandle(IN struct fsw_dnode *dno,
                                       OUT EFI_FILE **NewFileHandle)
{
    EFI_STATUS          Status;
    FSW_FILE_DATA       *File;

    // make sure the dnode has complete info
    Status = fsw_efi_map_status(fsw_dnode_fill(dno), (FSW_VOLUME_DATA *)dno->vol->host_data);
    if (EFI_ERROR(Status))
        return Status;

    // check type
    if (dno->type != FSW_DNODE_TYPE_FILE && dno->type != FSW_DNODE_TYPE_DIR)
        return EFI_UNSUPPORTED;

    // allocate file structure
    File = AllocateZeroPool(sizeof(FSW_FILE_DATA));
    File->Signature = FSW_FILE_DATA_SIGNATURE;
    if (dno->type == FSW_DNODE_TYPE_FILE)
        File->Type = FSW_EFI_FILE_TYPE_FILE;
    else if (dno->type == FSW_DNODE_TYPE_DIR)
        File->Type = FSW_EFI_FILE_TYPE_DIR;

    // open shandle
    Status = fsw_efi_map_status(fsw_shandle_open(dno, &File->shand),
                                (FSW_VOLUME_DATA *)dno->vol->host_data);
    if (EFI_ERROR(Status)) {
        FreePool(File);
        return Status;
    }

    // populate the file handle
    File->FileHandle.Revision    = EFI_FILE_HANDLE_REVISION;
    File->FileHandle.Open        = fsw_efi_FileHandle_Open;
    File->FileHandle.Close       = fsw_efi_FileHandle_Close;
    File->FileHandle.Delete      = fsw_efi_FileHandle_Delete;
    File->FileHandle.Read        = fsw_efi_FileHandle_Read;
    File->FileHandle.Write       = fsw_efi_FileHandle_Write;
    File->FileHandle.GetPosition = fsw_efi_FileHandle_GetPosition;
    File->FileHandle.SetPosition = fsw_efi_FileHandle_SetPosition;
    File->FileHandle.GetInfo     = fsw_efi_FileHandle_GetInfo;
    File->FileHandle.SetInfo     = fsw_efi_FileHandle_SetInfo;
    File->FileHandle.Flush       = fsw_efi_FileHandle_Flush;

    *NewFileHandle = &File->FileHandle;
    return EFI_SUCCESS;
}

/**
 * Data read function for regular files. Calls through to fsw_shandle_read.
 */

EFI_STATUS fsw_efi_file_read(IN FSW_FILE_DATA *File,
                             IN OUT UINTN *BufferSize,
                             OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    fsw_u32             buffer_size;

#if DEBUG_LEVEL
    Print(L"fsw_efi_file_read %d bytes\n", *BufferSize);
#endif

    buffer_size = (fsw_u32)*BufferSize;
    Status = fsw_efi_map_status(fsw_shandle_read(&File->shand, &buffer_size, Buffer),
                                (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data);
    *BufferSize = buffer_size;

    return Status;
}

/**
 * Get file position for regular files.
 */

EFI_STATUS fsw_efi_file_getpos(IN FSW_FILE_DATA *File,
                               OUT UINT64 *Position)
{
    *Position = File->shand.pos;
    return EFI_SUCCESS;
}

/**
 * Set file position for regular files. EFI specifies the all-ones value
 * to be a special value for the end of the file.
 */

EFI_STATUS fsw_efi_file_setpos(IN FSW_FILE_DATA *File, IN UINT64 Position)
{
    if (Position == 0xFFFFFFFFFFFFFFFFULL)
        File->shand.pos = File->shand.dnode->size;
    else
        File->shand.pos = Position;
    return EFI_SUCCESS;
}

/**
 * Open function used to open new file handles relative to a directory.
 * In EFI, the "open file" function is implemented by directory file handles
 * and is passed a relative or volume-absolute path to the file or directory
 * to open. We use fsw_dnode_lookup_path to find the node plus an additional
 * call to fsw_dnode_resolve because EFI has no concept of symbolic links.
 */

EFI_STATUS fsw_efi_dir_open(IN FSW_FILE_DATA *File,
                            OUT EFI_FILE **NewHandle,
                            IN CHAR16 *FileName,
                            IN UINT64 OpenMode,
                            IN UINT64 Attributes)
{
    EFI_STATUS          Status;
    FSW_VOLUME_DATA     *Volume = (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data;
    struct fsw_dnode    *dno;
    struct fsw_dnode    *target_dno;
    struct fsw_string   lookup_path;

#if DEBUG_LEVEL
    Print(L"fsw_efi_dir_open: '%s'\n", FileName);
#endif

    if (OpenMode != EFI_FILE_MODE_READ)
        return EFI_WRITE_PROTECTED;

    lookup_path.type = FSW_STRING_TYPE_UTF16;
    lookup_path.len  = (int)StrLen(FileName);
    lookup_path.size = lookup_path.len * sizeof(fsw_u16);
    lookup_path.data = FileName;

    // resolve the path (symlinks along the way are automatically resolved)
    Status = fsw_efi_map_status(fsw_dnode_lookup_path(File->shand.dnode, &lookup_path, '\\', &dno), Volume);
    if (EFI_ERROR(Status))
        return Status;

    // if the final node is a symlink, also resolve it
    Status = fsw_efi_map_status(fsw_dnode_resolve(dno, &target_dno), Volume);
    fsw_dnode_release(dno);
    if (EFI_ERROR(Status))
        return Status;
    dno = target_dno;

    // make a new EFI handle for the target dnode
    Status = fsw_efi_dnode_to_FileHandle(dno, NewHandle);
    fsw_dnode_release(dno);
    return Status;
}

/**
 * Read function for directories. A file handle read on a directory retrieves
 * the next directory entry.
 */

EFI_STATUS fsw_efi_dir_read(IN FSW_FILE_DATA *File,
                            IN OUT UINTN *BufferSize,
                            OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    FSW_VOLUME_DATA     *Volume = (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data;
    struct fsw_dnode    *dno;

#if DEBUG_LEVEL
    Print(L"fsw_efi_dir_read...\n");
#endif

    // read the next entry
    Status = fsw_efi_map_status(fsw_dnode_dir_read(&File->shand, &dno), Volume);
    if (Status == EFI_NOT_FOUND) {
        // end of directory
        *BufferSize = 0;
#if DEBUG_LEVEL
        Print(L"...no more entries\n");
#endif
        return EFI_SUCCESS;
    }
    if (EFI_ERROR(Status))
        return Status;

    // get info into buffer
    Status = fsw_efi_dnode_fill_FileInfo(Volume, dno, BufferSize, Buffer);
    fsw_dnode_release(dno);
    return Status;
}

/**
 * Set file position for directories. The only allowed set position operation
 * for directories is to rewind the directory completely by setting the
 * position to zero.
 */

EFI_STATUS fsw_efi_dir_setpos(IN FSW_FILE_DATA *File, IN UINT64 Position)
{
    if (Position == 0) {
        File->shand.pos = 0;
        return EFI_SUCCESS;
    } else {
        // directories can only rewind to the start
        return EFI_UNSUPPORTED;
    }
}

/**
 * Get file or volume information. This function implements the GetInfo call
 * for all file handles. Control is dispatched according to the type of information
 * requested by the caller.
 */

EFI_STATUS fsw_efi_dnode_getinfo(IN FSW_FILE_DATA *File,
                                 IN EFI_GUID *InformationType,
                                 IN OUT UINTN *BufferSize,
                                 OUT VOID *Buffer)
{
    EFI_STATUS            Status;
    FSW_VOLUME_DATA       *Volume = (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data;
    EFI_FILE_SYSTEM_INFO  *FSInfo;
    UINTN                 RequiredSize;
    struct fsw_volume_stat vsb;


    if (CompareGuid(InformationType, &gMyEfiFileInfoGuid)) {
#if DEBUG_LEVEL
        Print(L"fsw_efi_dnode_getinfo: FILE_INFO\n");
#endif

        Status = fsw_efi_dnode_fill_FileInfo(Volume, File->shand.dnode, BufferSize, Buffer);

    } else if (CompareGuid(InformationType, &gMyEfiFileSystemInfoGuid)) {
#if DEBUG_LEVEL
        Print(L"fsw_efi_dnode_getinfo: FILE_SYSTEM_INFO\n");
#endif

        // check buffer size
        RequiredSize = SIZE_OF_EFI_FILE_SYSTEM_INFO + fsw_efi_strsize(&Volume->vol->label);
        if (*BufferSize < RequiredSize) {
            *BufferSize = RequiredSize;
            return EFI_BUFFER_TOO_SMALL;
        }

        // fill structure
        FSInfo = (EFI_FILE_SYSTEM_INFO *)Buffer;
        FSInfo->Size        = RequiredSize;
        FSInfo->ReadOnly    = TRUE;
        FSInfo->BlockSize   = Volume->vol->log_blocksize;
        fsw_efi_strcpy(FSInfo->VolumeLabel, &Volume->vol->label);

        // get the missing info from the fs driver
        ZeroMem(&vsb, sizeof(struct fsw_volume_stat));
        Status = fsw_efi_map_status(fsw_volume_stat(Volume->vol, &vsb), Volume);
        if (EFI_ERROR(Status))
            return Status;
        FSInfo->VolumeSize  = vsb.total_bytes;
        FSInfo->FreeSpace   = vsb.free_bytes;

        // prepare for return
        *BufferSize = RequiredSize;
        Status = EFI_SUCCESS;

    } else if (CompareGuid(InformationType, &gMyEfiFileSystemVolumeLabelInfoIdGuid)) {
#if DEBUG_LEVEL
        Print(L"fsw_efi_dnode_getinfo: FILE_SYSTEM_VOLUME_LABEL\n");
#endif

        // check buffer size
        RequiredSize = SIZE_OF_EFI_FILE_SYSTEM_VOLUME_LABEL_INFO + fsw_efi_strsize(&Volume->vol->label);
        if (*BufferSize < RequiredSize) {
            *BufferSize = RequiredSize;
            return EFI_BUFFER_TOO_SMALL;
        }

        // copy volume label
        fsw_efi_strcpy(((EFI_FILE_SYSTEM_VOLUME_LABEL_INFO *)Buffer)->VolumeLabel, &Volume->vol->label);

        // prepare for return
        *BufferSize = RequiredSize;
        Status = EFI_SUCCESS;

    } else {
        Status = EFI_UNSUPPORTED;
    }

    return Status;
}

/**
 * Time mapping callback for the fsw_dnode_stat call. This function converts
 * a Posix style timestamp into an EFI_TIME structure and writes it to the
 * appropriate member of the EFI_FILE_INFO structure that we're filling.
 */

void fsw_store_time_posix(struct fsw_dnode_stat *sb, int which, fsw_u32 posix_time)
{
    EFI_FILE_INFO       *FileInfo = (EFI_FILE_INFO *)sb->host_data;

    if (which == FSW_DNODE_STAT_CTIME)
        fsw_efi_decode_time(&FileInfo->CreateTime,       posix_time);
    else if (which == FSW_DNODE_STAT_MTIME)
        fsw_efi_decode_time(&FileInfo->ModificationTime, posix_time);
    else if (which == FSW_DNODE_STAT_ATIME)
        fsw_efi_decode_time(&FileInfo->LastAccessTime,   posix_time);
}

/**
 * Mode mapping callback for the fsw_dnode_stat call. This function looks at
 * the Posix mode passed by the file system driver and makes appropriate
 * adjustments to the EFI_FILE_INFO structure that we're filling.
 */

void fsw_store_attr_posix(struct fsw_dnode_stat *sb, fsw_u16 posix_mode)
{
    EFI_FILE_INFO       *FileInfo = (EFI_FILE_INFO *)sb->host_data;

    if ((posix_mode & S_IWUSR) == 0)
        FileInfo->Attribute |= EFI_FILE_READ_ONLY;
}

void fsw_store_attr_efi(struct fsw_dnode_stat *sb, fsw_u16 attr)
{
    EFI_FILE_INFO       *FileInfo = (EFI_FILE_INFO *)sb->host_data;

    FileInfo->Attribute |= attr;
}

/**
 * Common function to fill an EFI_FILE_INFO with information about a dnode.
 */

EFI_STATUS fsw_efi_dnode_fill_FileInfo(IN FSW_VOLUME_DATA *Volume,
                                       IN struct fsw_dnode *dno,
                                       IN OUT UINTN *BufferSize,
                                       OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    EFI_FILE_INFO       *FileInfo;
    UINTN               RequiredSize;
    struct fsw_dnode_stat sb;

    // make sure the dnode has complete info
    Status = fsw_efi_map_status(fsw_dnode_fill(dno), Volume);
    if (EFI_ERROR(Status))
        return Status;

    // TODO: check/assert that the dno's name is in UTF16

    // check buffer size
    RequiredSize = SIZE_OF_EFI_FILE_INFO + fsw_efi_strsize(&dno->name);
    if (*BufferSize < RequiredSize) {
        // TODO: wind back the directory in this case

#if DEBUG_LEVEL
        Print(L"...BUFFER TOO SMALL\n");
#endif
        *BufferSize = RequiredSize;
        return EFI_BUFFER_TOO_SMALL;
    }

    // fill structure
    ZeroMem(Buffer, RequiredSize);
    FileInfo = (EFI_FILE_INFO *)Buffer;
    FileInfo->Size = RequiredSize;
    FileInfo->FileSize          = dno->size;
    FileInfo->Attribute         = 0;
    if (dno->type == FSW_DNODE_TYPE_DIR)
        FileInfo->Attribute    |= EFI_FILE_DIRECTORY;
    fsw_efi_strcpy(FileInfo->FileName, &dno->name);

    // get the missing info from the fs driver
    ZeroMem(&sb, sizeof(struct fsw_dnode_stat));
    sb.host_data = FileInfo;
    Status = fsw_efi_map_status(fsw_dnode_stat(dno, &sb), Volume);
    if (EFI_ERROR(Status))
        return Status;
    FileInfo->PhysicalSize      = sb.used_bytes;

    // prepare for return
    *BufferSize = RequiredSize;
#if DEBUG_LEVEL
    Print(L"...returning '%s'\n", FileInfo->FileName);
#endif
    return EFI_SUCCESS;
}

// EOF
