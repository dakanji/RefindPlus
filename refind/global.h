/*
 * refit/global.h
 * Global header file
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
 * Modifications copyright (c) 2012-2021 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), a copy of which must be distributed
 * with this source code or binaries made from it.
 *
 */

#ifndef __GLOBAL_H_
#define __GLOBAL_H_

#ifdef __MAKEWITH_GNUEFI
#include <efi.h>
#include <efilib.h>
#else
#include "../include/tiano_includes.h"
#endif
#include "../EfiLib/GenericBdsLib.h"

#include "../libeg/libeg.h"

#define REFIT_DEBUG (0)

// Tag classifications; used in various ways.
#define TAG_ABOUT            (1)
#define TAG_REBOOT           (2)
#define TAG_SHUTDOWN         (3)
#define TAG_TOOL             (4)
#define TAG_LOADER           (5)
#define TAG_LEGACY           (6)
#define TAG_FIRMWARE_LOADER  (7)
#define TAG_EXIT             (8)
#define TAG_SHELL            (9)
#define TAG_GPTSYNC          (10)
#define TAG_LEGACY_UEFI      (11)
#define TAG_APPLE_RECOVERY   (12)
#define TAG_WINDOWS_RECOVERY (13)
#define TAG_MOK_TOOL         (14)
#define TAG_FIRMWARE         (15)
#define TAG_MEMTEST          (16)
#define TAG_GDISK            (17)
#define TAG_NETBOOT          (18)
#define TAG_CSR_ROTATE       (19)
#define TAG_FWUPDATE_TOOL    (20)
#define TAG_HIDDEN           (21)
#define TAG_INSTALL          (22)
#define TAG_BOOTORDER        (23)
#define NUM_TOOLS            (24)

#define NUM_SCAN_OPTIONS 11

#define DEFAULT_ICONS_DIR L"icons"

// OS bit codes; used in GlobalConfig.GraphicsOn
#define GRAPHICS_FOR_OSX        1
#define GRAPHICS_FOR_LINUX      2
#define GRAPHICS_FOR_ELILO      4
#define GRAPHICS_FOR_GRUB       8
#define GRAPHICS_FOR_WINDOWS   16

// Type of legacy (BIOS) boot support detected
#define LEGACY_TYPE_NONE 0
#define LEGACY_TYPE_MAC  1
#define LEGACY_TYPE_UEFI 2

// How was a loader added to the menu?
#define DISCOVERY_TYPE_UNKNOWN  0
#define DISCOVERY_TYPE_AUTO     1
#define DISCOVERY_TYPE_MANUAL   2

#ifdef __MAKEWITH_GNUEFI
//
// define BBS Device Types
//
#define BBS_FLOPPY        0x01
#define BBS_HARDDISK      0x02
#define BBS_CDROM         0x03
#define BBS_PCMCIA        0x04
#define BBS_USB           0x05
#define BBS_EMBED_NETWORK 0x06
#define BBS_BEV_DEVICE    0x80
#define BBS_UNKNOWN       0xff
#endif

// BIOS Boot Specification (BBS) device types, as returned in DevicePath->Type field
#define DEVICE_TYPE_HW         0x01
#define DEVICE_TYPE_ACPI       0x02 /* returned by UEFI boot loader on USB */
#define DEVICE_TYPE_MESSAGING  0x03
#define DEVICE_TYPE_MEDIA      0x04 /* returned by EFI boot loaders on hard disk */
#define DEVICE_TYPE_BIOS       0x05 /* returned by legacy (BIOS) boot loaders */
#define DEVICE_TYPE_END        0x75 /* end of path */

// Filesystem type identifiers. Not all are yet used....
#define FS_TYPE_UNKNOWN        0
#define FS_TYPE_WHOLEDISK      1
#define FS_TYPE_FAT            2
#define FS_TYPE_EXFAT          3
#define FS_TYPE_NTFS           4
#define FS_TYPE_EXT2           5
#define FS_TYPE_EXT3           6
#define FS_TYPE_EXT4           7
#define FS_TYPE_HFSPLUS        8
#define FS_TYPE_REISERFS       9
#define FS_TYPE_BTRFS          10
#define FS_TYPE_XFS            11
#define FS_TYPE_JFS            12
#define FS_TYPE_ISO9660        13
#define NUM_FS_TYPES           14

// How to scale banner images
#define BANNER_NOSCALE         0
#define BANNER_FILLSCREEN      1

// Sizes of the default icons; badges are 1/4 the big icon size
#define DEFAULT_SMALL_ICON_SIZE 48
#define DEFAULT_BIG_ICON_SIZE   128
#define DEFAULT_MOUSE_SIZE      16

// Codes for types of icon sizes; used for indexing into GlobalConfig.IconSizes[]
#define ICON_SIZE_BADGE 0
#define ICON_SIZE_SMALL 1
#define ICON_SIZE_BIG   2
#define ICON_SIZE_MOUSE 3

// Minimum horizontal resolution for a screen to be consider high-DPI
#define HIDPI_MIN 1921

#ifndef EFI_OS_INDICATIONS_BOOT_TO_FW_UI
#define EFI_OS_INDICATIONS_BOOT_TO_FW_UI 0x0000000000000001ULL
#endif

// Names of binaries that can manage MOKs....
#if defined (EFIX64)
#define MOK_NAMES               L"MokManager.efi,HashTool.efi,HashTool-signed.efi,KeyTool.efi,KeyTool-signed.efi,mmx64.efi"
#elif defined(EFI32)
#define MOK_NAMES               L"MokManager.efi,HashTool.efi,HashTool-signed.efi,KeyTool.efi,KeyTool-signed.efi,mmia32.efi"
#elif defined(EFIAARCH64)
#define MOK_NAMES               L"MokManager.efi,HashTool.efi,HashTool-signed.efi,KeyTool.efi,KeyTool-signed.efi,mmaa64.efi"
#else
#define MOK_NAMES               L"MokManager.efi,HashTool.efi,HashTool-signed.efi,KeyTool.efi,KeyTool-signed.efi"
#endif
// Names of binaries that can update firmware....
#if defined (EFIX64)
#define FWUPDATE_NAMES          L"fwupx64.efi"
#elif defined(EFI32)
#define FWUPDATE_NAMES          L"fwupia32.efi"
#elif defined(EFIAARCH64)
#define FWUPDATE_NAMES          L"fwupaa64.efi"
#else
#define FWUPDATE_NAMES          L"fwup.efi"
#endif
// Directories to search for these MOK-managing programs. Note that SelfDir is
// searched in addition to these locations....
#define MOK_LOCATIONS           L"\\,EFI\\tools,EFI\\fedora,EFI\\redhat,EFI\\ubuntu,EFI\\suse,EFI\\opensuse,EFI\\altlinux"
// Directories to search for memtest86....
#define MEMTEST_LOCATIONS       L"EFI\\tools,EFI\\tools\\memtest86,EFI\\tools\\memtest,EFI\\memtest86,EFI\\memtest"
// Files that may be Windows recovery files
#define WINDOWS_RECOVERY_FILES  L"EFI\\Microsoft\\Boot\\LrsBootmgr.efi,Recovery:\\EFI\\BOOT\\bootx64.efi,Recovery:\\EFI\\BOOT\\bootia32.efi,\\EFI\\OEM\\Boot\\bootmgfw.efi"
// Files that may be macOS recovery files
#define MACOS_RECOVERY_FILES    L"com.apple.recovery.boot\\boot.efi"

// Filename patterns that identify EFI boot loaders. Note that a single case (either L"*.efi" or
// L"*.EFI") is fine for most systems; but Gigabyte's buggy Hybrid EFI does a case-sensitive
// comparison when it should do a case-insensitive comparison, so I'm doubling this up. It does
// no harm on other computers, AFAIK. In theory, every case variation should be done for
// completeness, but that's ridiculous....
#define LOADER_MATCH_PATTERNS   L"*.efi,*.EFI"

// Definitions for the "hideui" option in refind.conf
#define HIDEUI_FLAG_NONE       (0x0000)
#define HIDEUI_FLAG_BANNER     (0x0001)
#define HIDEUI_FLAG_LABEL      (0x0002)
#define HIDEUI_FLAG_SINGLEUSER (0x0004)
#define HIDEUI_FLAG_HWTEST     (0x0008)
#define HIDEUI_FLAG_ARROWS     (0x0010)
#define HIDEUI_FLAG_HINTS      (0x0020)
#define HIDEUI_FLAG_EDITOR     (0x0040)
#define HIDEUI_FLAG_SAFEMODE   (0x0080)
#define HIDEUI_FLAG_BADGES     (0x0100)
#define HIDEUI_FLAG_ALL       ((0xffff))

// Default hint text for program-launch submenus
#define SUBSCREEN_HINT1            L"Use arrow keys to move cursor; Enter to boot;"
#define SUBSCREEN_HINT2            L"Insert or F2 to edit options; Esc to return to main menu"
#define SUBSCREEN_HINT2_NO_EDITOR  L"Esc to return to main menu"

#define NULL_GUID_VALUE { 0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
#define REFIND_GUID_VALUE { 0x36D08FA7, 0xCF0B, 0x42F5, {0x8F, 0x14, 0x68, 0xDF, 0x73, 0xED, 0x37, 0x40} };
#define ESP_GUID_VALUE { 0xc12a7328, 0xf81f, 0x11d2, { 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b } };
#define SYSTEMD_GUID_VALUE { 0x4a67b082, 0x0a4c, 0x41cf, { 0xb6, 0xc7, 0x44, 0x0b, 0x29, 0xbb, 0x8c, 0x4f }};


// Configuration file variables
#define KERNEL_VERSION L"%v"
#define MAX_RES_CODE 2147483647 /* 2^31 - 1 */

//
// global definitions
//

// global types

typedef struct _uint32_list {
    UINT32               Value;
    struct _uint32_list  *Next;
} UINT32_LIST;

typedef struct {
   UINT8 Flags;
   UINT8 StartCHS1;
   UINT8 StartCHS2;
   UINT8 StartCHS3;
   UINT8 Type;
   UINT8 EndCHS1;
   UINT8 EndCHS2;
   UINT8 EndCHS3;
   UINT32 StartLBA;
   UINT32 Size;
} MBR_PARTITION_INFO;

typedef struct {
   EFI_DEVICE_PATH     *DevicePath;
   EFI_HANDLE          DeviceHandle;
   EFI_FILE            *RootDir;
   CHAR16              *PartName; // GPT partition name
   CHAR16              *FsName;   // Filesystem name
   CHAR16              *VolName;  // One of the two above OR fs description (e.g., "2 GiB FAT volume")
   EFI_GUID            VolUuid;
   EFI_GUID            PartGuid;
   EFI_GUID            PartTypeGuid;
   BOOLEAN             IsMarkedReadOnly;
   EG_IMAGE            *VolIconImage;
   EG_IMAGE            *VolBadgeImage;
   UINTN               DiskKind;
   BOOLEAN             HasBootCode;
   CHAR16              *OSIconName;
   CHAR16              *OSName;
   BOOLEAN             IsMbrPartition;
   UINTN               MbrPartitionIndex;
   EFI_BLOCK_IO        *BlockIO;
   UINT64              BlockIOOffset;
   EFI_BLOCK_IO        *WholeDiskBlockIO;
   EFI_DEVICE_PATH     *WholeDiskDevicePath;
   MBR_PARTITION_INFO  *MbrPartitionTable;
   BOOLEAN             IsReadable;
   UINT32              FSType;
} REFIT_VOLUME;

typedef struct _refit_menu_entry {
   CHAR16      *Title;
   UINTN       Tag;
   UINTN       Row;
   CHAR16      ShortcutDigit;
   CHAR16      ShortcutLetter;
   EG_IMAGE    *Image;
   EG_IMAGE    *BadgeImage;
   struct _refit_menu_screen *SubScreen;
} REFIT_MENU_ENTRY;

typedef struct _refit_menu_screen {
   CHAR16      *Title; // For EFI firmware entry, this includes "Reboot to" prefix
   EG_IMAGE    *TitleImage;
   UINTN       InfoLineCount;
   CHAR16      **InfoLines;
   UINTN       EntryCount;     // total number of entries registered
   REFIT_MENU_ENTRY **Entries;
   UINTN       TimeoutSeconds;
   CHAR16      *TimeoutText;
   CHAR16      *Hint1;
   CHAR16      *Hint2;
} REFIT_MENU_SCREEN;

typedef struct {
   REFIT_MENU_ENTRY me;
   CHAR16           *Title; // For EFI firmware entry, this is "raw" title
   CHAR16           *LoaderPath;
   REFIT_VOLUME     *Volume;
   BOOLEAN          UseGraphicsMode;
   BOOLEAN          Enabled;
   CHAR16           *LoadOptions;
   CHAR16           *InitrdPath; // Linux stub loader only
   CHAR8            OSType;
   UINTN            DiscoveryType;
   EFI_DEVICE_PATH  *EfiLoaderPath; // path to NVRAM-defined loader
   UINT16           EfiBootNum; // Boot#### number for NVRAM-defined loader
} LOADER_ENTRY;

typedef struct {
   REFIT_MENU_ENTRY  me;
   REFIT_VOLUME      *Volume;
   BDS_COMMON_OPTION *BdsOption;
   CHAR16            *LoadOptions;
   BOOLEAN           Enabled;
} LEGACY_ENTRY;

typedef struct {
   BOOLEAN          TextOnly;
   BOOLEAN          ScanAllLinux;
   BOOLEAN          DeepLegacyScan;
   BOOLEAN          EnableAndLockVMX;
   BOOLEAN          FoldLinuxKernels;
   BOOLEAN          EnableMouse;
   BOOLEAN          EnableTouch;
   BOOLEAN          HiddenTags;
   BOOLEAN          UseNvram;
   BOOLEAN          ShutdownAfterTimeout;
   BOOLEAN          Install;
   BOOLEAN          WriteSystemdVars;
   UINTN            RequestedScreenWidth;
   UINTN            RequestedScreenHeight;
   UINTN            BannerBottomEdge;
   UINTN            RequestedTextMode;
   UINTN            Timeout;
   UINTN            HideUIFlags;
   UINTN            MaxTags;     // max. number of OS entries to show simultaneously in graphics mode
   UINTN            GraphicsFor;
   UINTN            LegacyType;
   UINTN            ScanDelay;
   UINTN            ScreensaverTime;
   UINTN            MouseSpeed;
   UINTN            IconSizes[4];
   UINTN            BannerScale;
   UINTN            LogLevel;
   REFIT_VOLUME     *DiscoveredRoot;
   EFI_DEVICE_PATH  *SelfDevicePath;
   CHAR16           *BannerFileName;
   EG_IMAGE         *ScreenBackground;
   CHAR16           *ConfigFilename;
   CHAR16           *SelectionSmallFileName;
   CHAR16           *SelectionBigFileName;
   CHAR16           *DefaultSelection;
   CHAR16           *AlsoScan;
   CHAR16           *DontScanVolumes;
   CHAR16           *DontScanDirs;
   CHAR16           *DontScanFiles;
   CHAR16           *DontScanTools;
   CHAR16           *DontScanFirmware;
   CHAR16           *WindowsRecoveryFiles;
   CHAR16           *MacOSRecoveryFiles;
   CHAR16           *DriverDirs;
   CHAR16           *IconsDir;
   CHAR16           *ExtraKernelVersionStrings;
   CHAR16           *SpoofOSXVersion;
   UINT32_LIST      *CsrValues;
   UINTN            ShowTools[NUM_TOOLS];
   CHAR8            ScanFor[NUM_SCAN_OPTIONS]; // codes of types of loaders for which to scan
} REFIT_CONFIG;

// Global variables

extern EFI_HANDLE       SelfImageHandle;
extern EFI_LOADED_IMAGE *SelfLoadedImage;
extern EFI_FILE         *SelfRootDir;
extern EFI_FILE         *SelfDir;
extern CHAR16           *SelfDirPath;

extern REFIT_VOLUME     *SelfVolume;
extern REFIT_VOLUME     **Volumes;
extern UINTN            VolumesCount;

extern REFIT_CONFIG     GlobalConfig;
extern CHAR16           *gHiddenTools;

extern EFI_GUID gEfiLegacyBootProtocolGuid;
extern EFI_GUID gEfiGlobalVariableGuid;

extern BOOLEAN HaveResized;

extern EFI_GUID GlobalGuid;
extern EFI_GUID RefindGuid;

extern REFIT_MENU_SCREEN MainMenu;
extern REFIT_MENU_ENTRY MenuEntryReturn;

// Global function definitions....

VOID AboutrEFInd(VOID);
EG_IMAGE * GetDiskBadge(IN UINTN DiskType);
LOADER_ENTRY * MakeGenericLoaderEntry(VOID);
VOID StoreLoaderName(IN CHAR16 *Name);
VOID RescanAll(BOOLEAN DisplayMessage, BOOLEAN Reconnect);

#endif

/* EOF */
