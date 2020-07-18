// A boatload of #includes needed to build the software with TianoCore's EDK2/UDK2010
// toolkit. Placed here to maintain my own sanity.

#ifndef _REFIND_TIANO_INCLUDES_
#define _REFIND_TIANO_INCLUDES_

#define ST gST
#define BS gBS
#define RT gRT
#define RS gRS
#define StrDuplicate EfiStrDuplicate
#define LoadedImageProtocol gEfiLoadedImageProtocolGuid
#define LibFileInfo EfiLibFileInfo
#define Atoi StrDecimalToUintn
#define SPrint UnicodeSPrint
#define StrDuplicate EfiStrDuplicate
#define EFI_MAXIMUM_VARIABLE_SIZE           1024

#include <PiDxe.h>
#include <Base.h>
#include <Uefi.h>
#include <FrameworkDxe.h>
// Protocol Includes
#include <Protocol/AbsolutePointer.h>
#include <Protocol/AcpiTable.h>
#include <Protocol/BlockIo.h>
#include <Protocol/BlockIo2.h>
#include <Protocol/Cpu.h>
#include <Protocol/DataHub.h>
#include <Protocol/DebugPort.h>
#include <Protocol/Decompress.h>
#include <Protocol/DevicePath.h>
#include <Protocol/DevicePathFromText.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/DiskIo.h>
#include <Protocol/EdidActive.h>
#include <Protocol/EdidDiscovered.h>
#include <Protocol/FirmwareVolume2.h>
#include <Protocol/FrameworkHii.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/HiiDatabase.h>
#include <Protocol/HiiImage.h>
#include <Protocol/LegacyBios.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/PciIo.h>
#include <Protocol/ScsiIo.h>
#include <Protocol/ScsiPassThru.h>
#include <Protocol/ScsiPassThruExt.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/SimpleNetwork.h>
#include <Protocol/SimplePointer.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/Smbios.h>
#include <Protocol/SmbusHc.h>
#include <Protocol/UgaDraw.h>
#include <Protocol/UgaIo.h>
#include <Protocol/UnicodeCollation.h>
#include <Protocol/UsbIo.h>
#include <Protocol/LegacyBios.h>

// Guid Includes
#include <Guid/Acpi.h>
#include <Guid/ConsoleInDevice.h>
#include <Guid/ConsoleOutDevice.h>
#include <Guid/DataHubRecords.h>
#include <Guid/DxeServices.h>
#include <Guid/EventGroup.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <Guid/FileSystemVolumeLabelInfo.h>
#include <Guid/GlobalVariable.h>
#include <Guid/HobList.h>
#include <Guid/MemoryTypeInformation.h>
#include <Guid/MemoryAllocationHob.h>
#include <Guid/SmBios.h>
#include <Guid/StandardErrorDevice.h>

// Library Includes
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/DxeServicesTableLib.h>
//#include <Library/EblCmdLib.h>
//#include <Library/EblNetworkLib.h>
//#include "EfiFileLib.h"
#include <Library/HiiLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiScsiLib.h>

// IndustryStandard Includes
#include <IndustryStandard/Pci.h>
#include <IndustryStandard/SmBus.h>
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/HighPrecisionEventTimerTable.h>
#include <IndustryStandard/Scsi.h>

#include "../EfiLib/Platform.h"


BOOLEAN CheckError(IN EFI_STATUS Status, IN CHAR16 *where);

//
// BmLib
//
extern EFI_STATUS
EfiLibLocateProtocol (
   IN  EFI_GUID    *ProtocolGuid,
   OUT VOID        **Interface
);


extern EFI_FILE_HANDLE
EfiLibOpenRoot (
   IN EFI_HANDLE                   DeviceHandle
);

extern EFI_FILE_SYSTEM_VOLUME_LABEL *
EfiLibFileSystemVolumeLabelInfo (
   IN EFI_FILE_HANDLE      FHand
);
extern CHAR16 *
EfiStrDuplicate (
   IN CHAR16   *Src
);

extern EFI_FILE_INFO * EfiLibFileInfo (IN EFI_FILE_HANDLE      FHand);
extern EFI_FILE_SYSTEM_INFO * EfiLibFileSystemInfo (IN EFI_FILE_HANDLE   Root);


extern VOID *
EfiReallocatePool (
   IN VOID                 *OldPool,
   IN UINTN                OldSize,
   IN UINTN                NewSize
);

#define PoolPrint(...) CatSPrint(NULL, __VA_ARGS__)

#endif