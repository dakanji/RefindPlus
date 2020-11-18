/** @file
 * AcquireGOP.c
 * Installs GOP by reloading a copy of the GPU's OptionROM from RAM
 *
 * Copyright (c) 2020 Dayo Akanji (dakanji@users.sourceforge.net)
 * Portions Copyright (c) 2020 Joe van Tunen (joevt@shaw.ca)
 * Portions Copyright (c) 2004-2008 The Intel Corporation
 *
 * THIS PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "Platform.h"
#include "../refind/lib.h"
#include "../include/refit_call_wrapper.h"
#include "../../ShellPkg/Include/Library/HandleParsingLib.h"

/**
  @param[in] RomBar       The Rom Base address.
  @param[in] RomSize      The Rom size.
  @param[in] FileName     The file name.

  @retval EFI_SUCCESS             The command completed successfully.
  @retval EFI_INVALID_PARAMETER   Command usage error.
  @retval EFI_UNSUPPORTED         Protocols unsupported.
  @retval EFI_OUT_OF_RESOURCES    Out of memory.
  @retval EFI_VOLUME_CORRUPTED    Inconsistent signatures
  @retval Other value             Unknown error.
**/
EFI_STATUS
LoadGopDriver (
    VOID          *RomBar,
    UINTN         RomSize,
    CONST CHAR16  *FileName
) {
    VOID                          *ImageBuffer;
    VOID                          *DecompressedImageBuffer;
    UINTN                         ImageIndex;
    UINTN                         RomBarOffset;
    UINT8                         *Scratch;
    UINT16                        ImageOffset;
    UINT32                        ImageSize;
    UINT32                        ScratchSize;
    UINT32                        ImageLength;
    UINT32                        DestinationSize;
    UINT32                        InitializationSize;
    CHAR16                        *RomFileName;
    BOOLEAN                       LoadROM;
    EFI_STATUS                    Status;
    EFI_HANDLE                    ImageHandle;
    PCI_DATA_STRUCTURE            *Pcir;
    EFI_DECOMPRESS_PROTOCOL       *Decompress;
    EFI_DEVICE_PATH_PROTOCOL      *FilePath;
    EFI_PCI_EXPANSION_ROM_HEADER  *EfiRomHeader;

    ImageIndex    = 0;
    Status        = EFI_NOT_FOUND;
    RomBarOffset  = (UINTN) RomBar;

    do {
        LoadROM     = FALSE;
        EfiRomHeader = (EFI_PCI_EXPANSION_ROM_HEADER *) (UINTN) RomBarOffset;

        if (EfiRomHeader->Signature != PCI_EXPANSION_ROM_HEADER_SIGNATURE) {
            return EFI_VOLUME_CORRUPTED;
        }

        // If the pointer to the PCI Data Structure is invalid, no further images can be located.
        // The PCI Data Structure must be DWORD aligned.
        if (EfiRomHeader->PcirOffset == 0 ||
            (EfiRomHeader->PcirOffset & 3) != 0 ||
            RomBarOffset - (UINTN) RomBar + EfiRomHeader->PcirOffset + sizeof (PCI_DATA_STRUCTURE) > RomSize
        ) {
            break;
        }

        Pcir = (PCI_DATA_STRUCTURE *) (UINTN) (RomBarOffset + EfiRomHeader->PcirOffset);

        // If a valid signature is not present in the PCI Data Structure, no further images can be located.
        if (Pcir->Signature != PCI_DATA_STRUCTURE_SIGNATURE) {
            break;
        }

        ImageSize = Pcir->ImageLength * 512;

        if (RomBarOffset - (UINTN)RomBar + ImageSize > RomSize) {
            break;
        }

        if ((Pcir->CodeType == PCI_CODE_TYPE_EFI_IMAGE) &&
            (EfiRomHeader->EfiSignature  == EFI_PCI_EXPANSION_ROM_HEADER_EFISIGNATURE) &&
            ((EfiRomHeader->EfiSubsystem == EFI_IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER) ||
            (EfiRomHeader->EfiSubsystem  == EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER))
        ) {
            ImageOffset         = EfiRomHeader->EfiImageHeaderOffset;
            InitializationSize  = EfiRomHeader->InitializationSize * 512;

            if (InitializationSize <= ImageSize && ImageOffset < InitializationSize) {
                ImageBuffer             = (VOID *) (UINTN) (RomBarOffset + ImageOffset);
                ImageLength             = InitializationSize - ImageOffset;
                DecompressedImageBuffer = NULL;

                if (EfiRomHeader->CompressionType == EFI_PCI_EXPANSION_ROM_HEADER_COMPRESSED) {
                    Status = refit_call3_wrapper (
                        gBS->LocateProtocol,
                        &gEfiDecompressProtocolGuid,
                        NULL,
                        (VOID**) &Decompress
                    );

                    if (!EFI_ERROR (Status)) {
                        Status = refit_call5_wrapper (
                            Decompress->GetInfo,
                            Decompress,
                            ImageBuffer,
                            ImageLength,
                            &DestinationSize,
                            &ScratchSize
                        );

                        if (!EFI_ERROR (Status)) {
                            DecompressedImageBuffer = AllocateZeroPool (DestinationSize);

                            if (ImageBuffer != NULL) {
                                Scratch = AllocateZeroPool (ScratchSize);

                                if (Scratch != NULL) {
                                    Status = refit_call7_wrapper (
                                        Decompress->Decompress,
                                        Decompress,
                                        ImageBuffer,
                                        ImageLength,
                                        DecompressedImageBuffer,
                                        DestinationSize,
                                        Scratch,
                                        ScratchSize
                                    );

                                    if (!EFI_ERROR (Status)) {
                                        LoadROM     = TRUE;
                                        ImageBuffer = DecompressedImageBuffer;
                                        ImageLength = DestinationSize;
                                    }

                                     MyFreePool (Scratch);
                                 }
                             }
                         }
                     }
                 }

                 if (LoadROM) {
                     RomFileName = PoolPrint (L"%s[%d]", FileName, ImageIndex);
                     FilePath    = refit_call2_wrapper (FileDevicePath, NULL, RomFileName);
                     Status      = refit_call6_wrapper (
                         gBS->LoadImage,
                         TRUE,
                         gImageHandle,
                         FilePath,
                         ImageBuffer,
                         ImageLength,
                         &ImageHandle
                     );

                     if (EFI_ERROR (Status)) {
                         if (Status == EFI_SECURITY_VIOLATION) {
                             refit_call1_wrapper (gBS->UnloadImage, ImageHandle);
                         }
                     }
                     else {
                        Status = refit_call3_wrapper (gBS->StartImage, ImageHandle, NULL, NULL);
                      }
                  }

                  MyFreePool (DecompressedImageBuffer);
              }
          }

          RomBarOffset = RomBarOffset + ImageSize;
          ImageIndex++;
      } while (((Pcir->Indicator & 0x80) == 0x00) && ((RomBarOffset - (UINTN) RomBar) < RomSize));

      return Status;
}

EFI_STATUS
AcquireGOP (
    VOID
) {
    UINTN                Index               = 0;
    UINTN                HandleIndex         = 0;
    UINTN                HandleArrayCount    = 0;
    UINTN                BindingHandleCount  = 0;
    CHAR16               *RomFileName        = NULL;
    EFI_HANDLE           *HandleArray        = NULL;
    EFI_HANDLE           *BindingHandleBuffer;
    EFI_STATUS           Status;
    EFI_PCI_IO_PROTOCOL  *PciIo;

    Status = refit_call5_wrapper (
        gBS->LocateHandleBuffer,
        ByProtocol,
        &gEfiPciIoProtocolGuid,
        NULL,
        &HandleArrayCount,
        &HandleArray
    );

    if (!EFI_ERROR (Status)) {
        for (Index = 0; Index < HandleArrayCount; Index++) {
            Status = refit_call3_wrapper (
                gBS->HandleProtocol,
                HandleArray[Index],
                &gEfiPciIoProtocolGuid,
                (void **) &PciIo
            );

            if (!EFI_ERROR (Status)) {
                if (PciIo->RomImage && PciIo->RomSize) {
                    BindingHandleBuffer = NULL;
                    refit_call3_wrapper (
                        PARSE_HANDLE_DATABASE_UEFI_DRIVERS,
                        HandleArray[Index],
                        &BindingHandleCount,
                        &BindingHandleBuffer
                    );

                    if (BindingHandleCount == 0) {
                        HandleIndex = refit_call1_wrapper (ConvertHandleToHandleIndex, HandleArray[Index]);
                        RomFileName = PoolPrint (L"Handle%X", HandleIndex);

                        Status = LoadGopDriver (
                            PciIo->RomImage,
                            PciIo->RomSize,
                            RomFileName
                        );
                    } // if BindingHandleCount

                    MyFreePool (BindingHandleBuffer);
                } // if PciIo->RomImage && PciIo->RomSize
            } // if !EFI_ERROR HandleProtocol
        } // for
    } // if !EFI_ERROR LocateHandleBuffer

    return EFI_SUCCESS;
}
