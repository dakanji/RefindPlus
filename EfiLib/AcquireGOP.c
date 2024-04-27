/** @file
 * AcquireGOP.c
 * Installs GOP by reloading a copy of the GPU's OptionROM from RAM
 *
 * Copyright (c) 2020-2024 Dayo Akanji (sf.net/u/dakanji/profile)
 * Portions Copyright (c) 2020 Joe van Tunen (joevt@shaw.ca)
 * Portions Copyright (c) 2004-2008 The Intel Corporation
 *
 * THIS PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef __MAKEWITH_TIANO

/**
  @retval EFI_INCOMPATIBLE_VERSION  Not running on compatible TianoCore compiled version
**/
EFI_STATUS AcquireGOP (VOID) {
    // NOOP if not compiled using EDK II
    return EFI_INCOMPATIBLE_VERSION;
}

/**
  @retval EFI_INCOMPATIBLE_VERSION  Not running on compatible TianoCore compiled version
**/
EFI_STATUS ReissueGOP (VOID) {
    // NOOP if not compiled using EDK II
    return EFI_INCOMPATIBLE_VERSION;
}

#else

#include "Platform.h"
#include "../BootMaster/lib.h"
#include "../include/refit_call_wrapper.h"
#include "../../ShellPkg/Include/Library/HandleParsingLib.h"

/**
  @param[in] RomBar       The Rom Base address.
  @param[in] RomSize      The Rom size.
  @param[in] FileName     The file name.

  @retval EFI_SUCCESS               The command completed successfully.
  @retval EFI_INVALID_PARAMETER     Command usage error.
  @retval EFI_UNSUPPORTED           Protocols unsupported.
  @retval EFI_OUT_OF_RESOURCES      Out of memory.
  @retval EFI_VOLUME_CORRUPTED      Inconsistent signatures.
  @retval EFI_NOT_FOUND             Failed to Locate Suitable Option ROM.
  @retval Other value               Unknown error.
**/
static
EFI_STATUS ReloadOptionROM (
    IN       VOID    *RomBar,
    IN       UINT64   RomSize,
    IN const CHAR16  *FileName
) {
    VOID                          *ImageBuffer;
    VOID                          *DecompressedImageBuffer;
    UINTN                          ImageIndex;
    UINTN                          RomBarOffset;
    UINT8                         *Scratch;
    UINT16                         ImageOffset;
    UINT32                         ImageSize;
    UINT32                         ScratchSize;
    UINT32                         ImageLength;
    UINT32                         DestinationSize;
    UINT32                         InitializationSize;
    CHAR16                        *RomFileName;
    BOOLEAN                        LoadROM;
    EFI_STATUS                     Status;
    EFI_STATUS                     ReturnStatus;
    EFI_HANDLE                     ImageHandle;
    PCI_DATA_STRUCTURE            *Pcir;
    EFI_DECOMPRESS_PROTOCOL       *Decompress;
    EFI_DEVICE_PATH_PROTOCOL      *FilePath;
    EFI_PCI_EXPANSION_ROM_HEADER  *EfiRomHeader;

    ImageIndex   = 0;
    ReturnStatus = Status = EFI_NOT_FOUND;
    RomBarOffset = (UINTN) RomBar;

    do {
        LoadROM      = FALSE;
        EfiRomHeader = (EFI_PCI_EXPANSION_ROM_HEADER *) (UINTN) RomBarOffset;

        if (EfiRomHeader->Signature != PCI_EXPANSION_ROM_HEADER_SIGNATURE) {
            return EFI_VOLUME_CORRUPTED;
        }

        // If the pointer to the PCI Data Structure is invalid, no further images can be located.
        // The PCI Data Structure must be DWORD aligned.
        if ((EfiRomHeader->PcirOffset == 0)     ||
            (EfiRomHeader->PcirOffset & 3) != 0 ||
            (
                RomBarOffset                  -
                (UINTN) RomBar                +
                EfiRomHeader->PcirOffset      +
                sizeof (PCI_DATA_STRUCTURE)
            ) > RomSize
        ) {
            break;
        }

        Pcir = (PCI_DATA_STRUCTURE *) (UINTN) (RomBarOffset + EfiRomHeader->PcirOffset);

        // If a valid signature is not present in the PCI Data Structure, no further images can be located.
        if (Pcir->Signature != PCI_DATA_STRUCTURE_SIGNATURE) {
            break;
        }

        ImageSize = Pcir->ImageLength * 512;

        if ((RomBarOffset - (UINTN) RomBar + ImageSize) > RomSize) {
            break;
        }

        if (Pcir->CodeType == PCI_CODE_TYPE_EFI_IMAGE &&
            EfiRomHeader->EfiSignature  == EFI_PCI_EXPANSION_ROM_HEADER_EFISIGNATURE &&
            (
                EfiRomHeader->EfiSubsystem == EFI_IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER ||
                EfiRomHeader->EfiSubsystem == EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER
            )
        ) {
            ImageOffset        = EfiRomHeader->EfiImageHeaderOffset;
            InitializationSize = EfiRomHeader->InitializationSize * 512;

            if (InitializationSize <= ImageSize && ImageOffset < InitializationSize) {
                ImageBuffer             = (VOID *) (UINTN) (RomBarOffset + ImageOffset);
                ImageLength             = InitializationSize - ImageOffset;
                DecompressedImageBuffer = NULL;
                DestinationSize         = 0; // DA-TAG: Redundant for Infer

                if (EfiRomHeader->CompressionType != EFI_PCI_EXPANSION_ROM_HEADER_COMPRESSED) {
                    // Uncompressed image ... Tag as Success to load 'as is'
                    Status = EFI_SUCCESS;
                }
                else {
                    // Compressed image ... Decompress before loading
                    Status = REFIT_CALL_3_WRAPPER(
                        gBS->LocateProtocol, &gEfiDecompressProtocolGuid,
                        NULL, (VOID **) &Decompress
                    );
                    if (!EFI_ERROR(Status)) {
                        Status = REFIT_CALL_5_WRAPPER(
                            Decompress->GetInfo, Decompress,
                            ImageBuffer, ImageLength,
                            &DestinationSize, &ScratchSize
                        );
                        if (!EFI_ERROR(Status)) {
                            DecompressedImageBuffer = AllocateZeroPool (DestinationSize);
                            if (DecompressedImageBuffer == NULL) {
                                MY_FREE_POOL(ImageBuffer);

                                return EFI_OUT_OF_RESOURCES;
                            }

                            if (ImageBuffer != NULL) {
                                Scratch = AllocateZeroPool (ScratchSize);
                                if (Scratch == NULL) {
                                    MY_FREE_POOL(ImageBuffer);
                                    MY_FREE_POOL(DecompressedImageBuffer);

                                    return EFI_OUT_OF_RESOURCES;
                                }

                                Status = REFIT_CALL_7_WRAPPER(
                                    Decompress->Decompress, Decompress,
                                    ImageBuffer, ImageLength,
                                    DecompressedImageBuffer, DestinationSize,
                                    Scratch, ScratchSize
                                );
                                if (!EFI_ERROR(Status)) {
                                    LoadROM = TRUE;
                                }

                                MY_FREE_POOL(Scratch);
                            }
                        } // if !EFI_ERROR Status = REFIT_CALL_5_WRAPPER
                    } // if !EFI_ERROR Status = REFIT_CALL_3_WRAPPER
                } // if EfiRomHeader

                if (LoadROM) {
                    MY_FREE_POOL(ImageBuffer);
                    ImageBuffer = DecompressedImageBuffer;
                    ImageLength = DestinationSize;
                }

                if (!EFI_ERROR(Status)) {
                    RomFileName = PoolPrint (L"%s[%d]", FileName, ImageIndex);
                    FilePath = REFIT_CALL_2_WRAPPER(FileDevicePath, NULL, RomFileName);
                    Status = REFIT_CALL_6_WRAPPER(
                        gBS->LoadImage, TRUE,
                        gImageHandle, FilePath,
                        ImageBuffer, ImageLength, &ImageHandle
                    );
                    if (EFI_ERROR(Status)) {
                        if (Status == EFI_SECURITY_VIOLATION) {
                            REFIT_CALL_1_WRAPPER(gBS->UnloadImage, ImageHandle);
                        }
                    }
                    else {
                        Status = REFIT_CALL_3_WRAPPER(
                            gBS->StartImage, ImageHandle,
                            NULL, NULL
                        );
                    }

                     MY_FREE_POOL(RomFileName);
                }

                MY_FREE_POOL(ImageBuffer);
            } // if InitializationSize
        } // if Pcir->CodeType

        RomBarOffset = RomBarOffset + ImageSize;
        ImageIndex++;

        if (EFI_ERROR(ReturnStatus)) {
            ReturnStatus = Status;
        }
    } while (
        (Pcir->Indicator & 0x80) == 0x00 &&
        (RomBarOffset - (UINTN) RomBar) < RomSize
    );

    return ReturnStatus;
} // EFI_STATUS ReloadOptionROM()

/**
  @retval EFI_SUCCESS               The command completed successfully.
  @retval EFI_INVALID_PARAMETER     Command usage error.
  @retval EFI_UNSUPPORTED           Protocols unsupported.
  @retval EFI_OUT_OF_RESOURCES      Out of memory.
  @retval EFI_VOLUME_CORRUPTED      Inconsistent signatures.
  @retval EFI_PROTOCOL_ERROR        PciIoProtocolGuid not found.
  @retval EFI_LOAD_ERROR            Failed to get PciIoProtocolGuid handle.
  @retval EFI_NO_MAPPING            Invalid Binding Handle Count.
  @retval EFI_NOT_FOUND             Failed to Locate Suitable Option ROM.
  @retval Other value               Unknown error.
**/
EFI_STATUS ReissueGOP (VOID) {
    UINTN                 Index;
    UINTN                 HandleIndex;
    UINTN                 HandleArrayCount;
    UINTN                 BindingHandleCount;
    CHAR16               *RomFileName;
    EFI_HANDLE           *HandleArray;
    EFI_HANDLE           *BindingHandleBuffer;
    EFI_STATUS            ReturnStatus;
    EFI_STATUS            Status;
    EFI_PCI_IO_PROTOCOL  *PciIo;

    HandleArrayCount = 0;
    HandleArray = NULL;
    Status = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, ByProtocol,
        &gEfiPciIoProtocolGuid, NULL,
        &HandleArrayCount, &HandleArray
    );
    if (EFI_ERROR(Status)) {
        // Early Return
        return EFI_PROTOCOL_ERROR;
    }

    ReturnStatus = EFI_LOAD_ERROR;
    for (Index = 0; Index < HandleArrayCount; Index++) {
        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol, HandleArray[Index],
            &gEfiPciIoProtocolGuid, (void **) &PciIo
        );
        if (EFI_ERROR(Status)) {
            if (EFI_ERROR(ReturnStatus)) {
                ReturnStatus = Status;
            }

            continue;
        }

        if (PciIo->RomImage == NULL || PciIo->RomSize == 0) {
            if (EFI_ERROR(ReturnStatus)) {
                ReturnStatus = EFI_NOT_FOUND;
            }

            continue;
        }

        BindingHandleCount = 0;
        BindingHandleBuffer = NULL;
        REFIT_CALL_3_WRAPPER(
            PARSE_HANDLE_DATABASE_UEFI_DRIVERS, HandleArray[Index],
            &BindingHandleCount, &BindingHandleBuffer
        );
        if (BindingHandleCount != 0) {
            MY_FREE_POOL(BindingHandleBuffer);

            if (EFI_ERROR(ReturnStatus)) {
                ReturnStatus = EFI_NO_MAPPING;
            }

            continue;
        }

        HandleIndex = ConvertHandleToHandleIndex (HandleArray[Index]);
        RomFileName = PoolPrint (L"Handle%X", HandleIndex);

        Status = ReloadOptionROM (
            PciIo->RomImage,
            PciIo->RomSize,
            (const CHAR16 *) RomFileName
        );
        if (EFI_ERROR(ReturnStatus)) {
            ReturnStatus = Status;
        }

        MY_FREE_POOL(RomFileName);
        MY_FREE_POOL(BindingHandleBuffer);
    } // for

    MY_FREE_POOL(HandleArray);

    return ReturnStatus;
} // EFI_STATUS ReissueGOP()

/**
  @retval EFI_SUCCESS               The command completed successfully.
  @retval EFI_INVALID_PARAMETER     Command usage error.
  @retval EFI_UNSUPPORTED           Protocols unsupported.
  @retval EFI_OUT_OF_RESOURCES      Out of memory.
  @retval EFI_VOLUME_CORRUPTED      Inconsistent signatures.
  @retval EFI_PROTOCOL_ERROR        PciIoProtocolGuid not found.
  @retval EFI_LOAD_ERROR            Failed to get PciIoProtocolGuid handle.
  @retval EFI_NO_MAPPING            Invalid Binding Handle Count.
  @retval EFI_NOT_FOUND             Failed to Locate Suitable Option ROM.
  @retval Other value               Unknown error.
**/
EFI_STATUS AcquireGOP (VOID) {
    UINTN                 Index;
    UINTN                 HandleArrayCount;
    UINTN                 BindingHandleCount;
    BOOLEAN               FirstLoop;
    EFI_HANDLE           *HandleArray;
    EFI_HANDLE           *BindingHandleBuffer;
    EFI_STATUS            ReturnStatus;
    EFI_STATUS            Status;
    EFI_PCI_IO_PROTOCOL  *PciIo;

    HandleArrayCount = 0;
    HandleArray = NULL;
    Status = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, ByProtocol,
        &gEfiPciIoProtocolGuid, NULL,
        &HandleArrayCount, &HandleArray
    );
    if (EFI_ERROR(Status)) {
        // Early Return
        return EFI_PROTOCOL_ERROR;
    }

    FirstLoop = TRUE;
    BindingHandleBuffer = NULL;
    ReturnStatus = EFI_LOAD_ERROR;
    for (Index = 0; Index < HandleArrayCount; Index++) {
        do {
            if (FirstLoop == TRUE) {
                // Initialise on First Loop
                BindingHandleBuffer = NULL;
            }

            Status = REFIT_CALL_3_WRAPPER(
                gBS->HandleProtocol, HandleArray[Index],
                &gEfiPciIoProtocolGuid, (void **) &PciIo
            );
            if (EFI_ERROR(Status)) {
                break;
            }

            if (PciIo->RomImage == NULL || PciIo->RomSize == 0) {
                if (EFI_ERROR(ReturnStatus)) {
                    ReturnStatus = EFI_NOT_FOUND;
                }

                break;
            }

            BindingHandleCount = 0;
            REFIT_CALL_3_WRAPPER(
                PARSE_HANDLE_DATABASE_UEFI_DRIVERS, HandleArray[Index],
                &BindingHandleCount, &BindingHandleBuffer
            );
            if (BindingHandleCount != 0) {
                if (EFI_ERROR(ReturnStatus)) {
                    ReturnStatus = EFI_NO_MAPPING;
                }

                break;
            }

            ReturnStatus = EFI_SUCCESS;
        } while (0); // This 'loop' only runs once

        FirstLoop = FALSE;

        MY_FREE_POOL(BindingHandleBuffer);

        if (!EFI_ERROR(ReturnStatus)) {
            break;
        }
    } // for

    MY_FREE_POOL(HandleArray);

    return ReturnStatus;
} // EFI_STATUS AcquireGOP()

#endif
