/*
 * BootMaster/global.h
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
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
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
#include "globalExtra.h"

// Menu entry types - depends on the tag of the menu entry
typedef enum {
    EntryTypeRefitMenuEntry,
    EntryTypeLoaderEntry,
    EntryTypeLegacyEntry,
} ENTRY_TYPE;

// Tag classifications; used in various ways.
#define TAG_ABOUT               (1)
#define TAG_REBOOT              (2)
#define TAG_SHUTDOWN            (3)
#define TAG_TOOL                (4)
#define TAG_LOADER              (5)
#define TAG_LEGACY              (6)
#define TAG_FIRMWARE_LOADER     (7)
#define TAG_EXIT                (8)
#define TAG_SHELL               (9)
#define TAG_GPTSYNC            (10)
#define TAG_LEGACY_UEFI        (11)
#define TAG_RECOVERY_APPLE     (12)
#define TAG_RECOVERY_WINDOWS   (13)
#define TAG_MOK_TOOL           (14)
#define TAG_FIRMWARE           (15)
#define TAG_MEMTEST            (16)
#define TAG_GDISK              (17)
#define TAG_NETBOOT            (18)
#define TAG_CSR_ROTATE         (19)
#define TAG_FWUPDATE_TOOL      (20)
#define TAG_HIDDEN             (21)
#define TAG_INSTALL            (22)
#define TAG_BOOTORDER          (23)
#define TAG_INFO_BOOTKICKER    (24)
#define TAG_LOAD_BOOTKICKER    (25)
#define TAG_INFO_NVRAMCLEAN    (26)
#define TAG_LOAD_NVRAMCLEAN    (27)
#define NUM_TOOLS              (28)

#define NUM_SCAN_OPTIONS 11

#define DEFAULT_ICONS_DIR L"icons"

// OS bit codes (Actual Decimal); used in GlobalConfig.GraphicsFor
#define GRAPHICS_FOR_OSX         1
#define GRAPHICS_FOR_LINUX       2
#define GRAPHICS_FOR_ELILO       4
#define GRAPHICS_FOR_GRUB        8
#define GRAPHICS_FOR_WINDOWS    16
#define GRAPHICS_FOR_OPENCORE   32
#define GRAPHICS_FOR_CLOVER     64

// Type of legacy (BIOS) boot support detected
#define LEGACY_TYPE_NONE         0
#define LEGACY_TYPE_MAC          1
#define LEGACY_TYPE_UEFI         2

// How was a loader added to the menu?
#define DISCOVERY_TYPE_UNKNOWN   0
#define DISCOVERY_TYPE_AUTO      1
#define DISCOVERY_TYPE_MANUAL    2

#ifdef __MAKEWITH_GNUEFI

// define BBS Device Types
#define BBS_FLOPPY             0x01
#define BBS_HARDDISK           0x02
#define BBS_CDROM              0x03
#define BBS_PCMCIA             0x04
#define BBS_USB                0x05
#define BBS_EMBED_NETWORK      0x06
#define BBS_BEV_DEVICE         0x80
#define BBS_UNKNOWN            0xff
#endif

// BIOS Boot Specification (BBS) device types, as returned in DevicePath->Type field
#define DEVICE_TYPE_HW         0x01
#define DEVICE_TYPE_ACPI       0x02 /* returned by UEFI boot loader on USB */
#define DEVICE_TYPE_MESSAGING  0x03
#define DEVICE_TYPE_MEDIA      0x04 /* returned by EFI boot loaders on hard disk */
#define DEVICE_TYPE_BIOS       0x05 /* returned by legacy (BIOS) boot loaders */
#define DEVICE_TYPE_END        0x75 /* end of path */

// Filesystem type identifiers ... Not all are used.
#define FS_TYPE_UNKNOWN           0
#define FS_TYPE_WHOLEDISK         1
#define FS_TYPE_FAT               2
#define FS_TYPE_EXFAT             3
#define FS_TYPE_NTFS              4
#define FS_TYPE_EXT2              5
#define FS_TYPE_EXT3              6
#define FS_TYPE_EXT4              7
#define FS_TYPE_HFSPLUS           8
#define FS_TYPE_REISERFS          9
#define FS_TYPE_BTRFS            10
#define FS_TYPE_XFS              11
#define FS_TYPE_JFS              12
#define FS_TYPE_ISO9660          13
#define FS_TYPE_APFS             14
#define NUM_FS_TYPES             15

// How to scale banner images
#define BANNER_NOSCALE            0
#define BANNER_FILLSCREEN         1

// Sizes of the default icons; badges are 1/4 the big icon size
#define DEFAULT_SMALL_ICON_SIZE  48
#define DEFAULT_BIG_ICON_SIZE   128
#define DEFAULT_MOUSE_SIZE       16

// Codes for types of icon sizes; used for indexing into GlobalConfig.IconSizes[]
#define ICON_SIZE_BADGE           0
#define ICON_SIZE_SMALL           1
#define ICON_SIZE_BIG             2
#define ICON_SIZE_MOUSE           3

// Minimum resolutions for a screen to be considered High-DPI
#define HIDPI_LONG 2560
#define HIDPI_SHORT 1600

#ifndef EFI_OS_INDICATIONS_BOOT_TO_FW_UI
#define EFI_OS_INDICATIONS_BOOT_TO_FW_UI 0x0000000000000001ULL
#endif

// Names of binaries that can manage MOKs.
#if defined (EFIX64)
    #define MOK_NAMES L"MokManager.efi,HashTool.efi,HashTool-signed.efi,KeyTool.efi,KeyTool-signed.efi,mmx64.efi"
#elif defined(EFI32)
    #define MOK_NAMES L"MokManager.efi,HashTool.efi,HashTool-signed.efi,KeyTool.efi,KeyTool-signed.efi,mmia32.efi"
#elif defined(EFIAARCH64)
    #define MOK_NAMES L"MokManager.efi,HashTool.efi,HashTool-signed.efi,KeyTool.efi,KeyTool-signed.efi,mmaa64.efi"
#else
    #define MOK_NAMES L"MokManager.efi,HashTool.efi,HashTool-signed.efi,KeyTool.efi,KeyTool-signed.efi"
#endif

// Names of binaries that can update firmware.
#if defined (EFIX64)
    #define FWUPDATE_NAMES          L"fwupx64.efi"
#elif defined(EFI32)
    #define FWUPDATE_NAMES          L"fwupia32.efi"
#elif defined(EFIAARCH64)
    #define FWUPDATE_NAMES          L"fwupaa64.efi"
#else
    #define FWUPDATE_NAMES          L"fwup.efi"
#endif

// Directories to search for these MOK-managing programs.
// Note that SelfDir is searched in addition to these locations.
#define MOK_LOCATIONS \
L"\\EFI\\tools,\\EFI\\fedora,\\EFI\\redhat,\\EFI\\ubuntu,\\EFI\\suse,\\EFI\\opensuse,\\EFI\\altlinux"

// Directories to search for memtest86.
#define MEMTEST_LOCATIONS \
L"\\EFI\\tools\\memtest86,\\EFI\\tools\\memtest,\\EFI\\memtest86,\\EFI\\memtest,\
\\EFI\\BOOT\\tools,\\EFI\\BOOT\\tools_x64,\\EFI\\tools_x64,\\EFI\\tools,\\EFI"

// Files that may be Windows recovery files
#if defined (EFIX64)
    #define WINDOWS_RECOVERY_FILES \
L"\\EFI\\Microsoft\\Boot\\LrsBootmgr.efi,Recovery:\\EFI\\BOOT\\bootx64.efi,\
Recovery:\\EFI\\BOOT\\boot.efi,\\EFI\\OEM\\Boot\\bootmgfw.efi"
#elif defined(EFI32)
    #define WINDOWS_RECOVERY_FILES \
L"\\EFI\\Microsoft\\Boot\\LrsBootmgr.efi,Recovery:\\EFI\\BOOT\\bootia32.efi,\
Recovery:\\EFI\\BOOT\\boot.efi,\\EFI\\OEM\\Boot\\bootmgfw.efi"
#elif defined(EFIAARCH64)
    #define WINDOWS_RECOVERY_FILES \
L"\\EFI\\Microsoft\\Boot\\LrsBootmgr.efi,Recovery:\\EFI\\BOOT\\bootaa64.efi,\
Recovery:\\EFI\\BOOT\\boot.efi,\\EFI\\OEM\\Boot\\bootmgfw.efi"
#else
    #define WINDOWS_RECOVERY_FILES \
L"\\EFI\\Microsoft\\Boot\\LrsBootmgr.efi,Recovery:\\EFI\\BOOT\\boot.efi,\
\\EFI\\OEM\\Boot\\bootmgfw.efi"
#endif

// Files that may be Mac OS recovery files
#define MACOS_RECOVERY_FILES    L"com.apple.recovery.boot\\boot.efi"
#define MACOSX_LOADER_DIR       L"System\\Library\\CoreServices"
#define MACOSX_LOADER_PATH      ( MACOSX_LOADER_DIR L"\\boot.efi" )


// Filename patterns that identify EFI boot loaders. Note that a single case (either L"*.efi" or
// L"*.EFI") is fine for most systems; but Gigabyte's buggy Hybrid EFI does a case-sensitive
// comparison when it should do a case-insensitive comparison, so I'm doubling this up. It does
// no harm on other computers, AFAIK. In theory, every case variation should be done for
// completeness, but that is ridiculous.
#define LOADER_MATCH_PATTERNS   L"*.efi,*.EFI"

// Definitions for the "hideui" option in config.conf
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
#define HIDEUI_FLAG_ALL        (0x01ff)

// Default hint text for program-launch submenus
#define SUBSCREEN_HINT1            L"Use arrow keys to move cursor; 'Enter' to boot;"
#define SUBSCREEN_HINT2            L"'Insert' or 'F2' to edit options; 'Esc' to return to main menu"
#define SUBSCREEN_HINT2_NO_EDITOR  L"'Esc' to return to main menu"

#define NULL_GUID_VALUE               {0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
#define REFINDPLUS_GUID               {0xF8800DA7, 0xDF1F, 0x4A16, {0x8F, 0xE3, 0x72, 0x43, 0xDB, 0xB7, 0x87, 0xCA}};
#define ESP_GUID_VALUE                {0xC12A7328, 0xF81F, 0x11D2, {0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B}};
#define HFS_GUID_VALUE                {0x48465300, 0x0000, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}};
#define APFS_GUID_VALUE               {0x7C3457EF, 0x0000, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}};
#define APPLE_TV_RECOVERY_GUID        {0x426F6F74, 0x0000, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}};
#define MAC_RAID_ON_GUID_VALUE        {0x52414944, 0x0000, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}};
#define MAC_RAID_OFF_GUID_VALUE       {0x52414944, 0x5F4F, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}};
#define CORE_STORAGE_GUID_VALUE       {0x53746F72, 0x6167, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}};
#define MAC_RECOVERYHD_GUID_VALUE     {0x426F6F74, 0x0000, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}};
#define MICROSOFT_VENDOR_GUID         {0x77FA9ABD, 0x0359, 0x4D32, {0xBD, 0x60, 0x28, 0xF4, 0xE7, 0x8F, 0x78, 0x4B}};
#define SYSTEMD_GUID_VALUE            {0x4a67b082, 0x0a4c, 0x41cf, {0xb6, 0xc7, 0x44, 0x0b, 0x29, 0xbb, 0x8c, 0x4f}};

// Define EFI Certificate GUIDs. Others are in mok/guid.c
#define EFI_CERT_SHA1_GUID            {0x826ca512, 0xcf10, 0x4ac9, {0xb1, 0x87, 0xbe, 0x01, 0x49, 0x66, 0x31, 0xbd}};
#define EFI_CERT_SHA224_GUID          {0x0b6e5233, 0xa65c, 0x44c9, {0x94, 0x07, 0xd9, 0xab, 0x83, 0xbf, 0xc8, 0xbd}};
#define EFI_CERT_SHA384_GUID          {0xff3e5307, 0x9fd0, 0x48c9, {0x85, 0xf1, 0x8a, 0xd5, 0x6c, 0x70, 0x1e, 0x01}};
#define EFI_CERT_SHA512_GUID          {0x093e0fae, 0xa6c4, 0x4f50, {0x9f, 0x1b, 0xd4, 0x1e, 0x2b, 0x89, 0xc1, 0x9a}};
#define EFI_CERT_TYPE_PKCS7_GUID      {0x4aafd29d, 0x68df, 0x49ee, {0x8a, 0xa9, 0x34, 0x7d, 0x37, 0x56, 0x65, 0xa7}};
#define EFI_CERT_RSA2048_SHA1_GUID    {0x67f8444f, 0x8743, 0x48f1, {0xa3, 0x28, 0x1e, 0xaa, 0xb8, 0x73, 0x60, 0x80}};
#define EFI_CERT_RSA2048_SHA256_GUID  {0xe2b36190, 0x879b, 0x4a3d, {0xad, 0x8d, 0xf2, 0xe7, 0xbb, 0xa3, 0x27, 0x84}};

// Configuration file variables
#define KERNEL_VERSION L"%v"
#define MAX_RES_CODE 2147483647 /* 2^31 - 1 */


#ifdef __MAKEWITH_TIANO
// DA-TAG: Forward Declaration for OpenCore Integration
//         Limit to TianoCore Builds
EFI_STATUS OcProvideConsoleGop (IN BOOLEAN Route);
EFI_STATUS OcProvideUgaPassThrough (VOID);
EFI_STATUS OcUseDirectGop (IN INT32 CacheType);
EFI_STATUS OcUseBuiltinTextOutput (IN EFI_CONSOLE_CONTROL_SCREEN_MODE  Mode);
#endif

/* DA-TAG: Add Macros */
#define MY_OFFSET_OF(st, m) ((UINTN)((char *) &((st *)0)->m - (char *)0))


//
// global definitions
//

// global types
typedef struct _uint32_list {
    UINT32               Value;
    struct _uint32_list  *Next;
} UINT32_LIST;

typedef struct {
    UINT8  Flags;
    UINT8  StartCHS1;
    UINT8  StartCHS2;
    UINT8  StartCHS3;
    UINT8  Type;
    UINT8  EndCHS1;
    UINT8  EndCHS2;
    UINT8  EndCHS3;
    UINT32 StartLBA;
    UINT32 Size;
} MBR_PARTITION_INFO;

typedef struct {
    EFI_DEVICE_PATH     *DevicePath;
    EFI_HANDLE           DeviceHandle;
    EFI_FILE            *RootDir;
    CHAR16              *PartName;
    CHAR16              *FsName;   // Filesystem name
    CHAR16              *VolName;  // One of the two above OR fs description (e.g., "2 GiB FAT volume")
    EFI_GUID             VolUuid;
    EFI_GUID             PartGuid;
    EFI_GUID             PartTypeGuid;
    BOOLEAN              IsMarkedReadOnly;
    EG_IMAGE            *VolIconImage;
    EG_IMAGE            *VolBadgeImage;
    UINTN                DiskKind;
    BOOLEAN              HasBootCode;
    CHAR16              *OSIconName;
    CHAR16              *OSName;
    BOOLEAN              IsMbrPartition;
    UINTN                MbrPartitionIndex;
    EFI_BLOCK_IO        *BlockIO;
    UINT64               BlockIOOffset;
    EFI_BLOCK_IO        *WholeDiskBlockIO;
    EFI_DEVICE_PATH     *WholeDiskDevicePath;
    MBR_PARTITION_INFO  *MbrPartitionTable;
    BOOLEAN              IsReadable;
    UINT32               FSType;
} REFIT_VOLUME;

typedef struct _refit_menu_entry {
    CHAR16      *Title;
    UINTN        Tag;
    UINTN        Row;
    CHAR16       ShortcutDigit;
    CHAR16       ShortcutLetter;
    EG_IMAGE    *Image;
    EG_IMAGE    *BadgeImage;
    struct _refit_menu_screen *SubScreen;
} REFIT_MENU_ENTRY;

typedef struct _refit_menu_screen {
    CHAR16            *Title;          // For EFI firmware entry, this includes "Reboot to" prefix
    EG_IMAGE          *TitleImage;
    UINTN              InfoLineCount;
    CHAR16           **InfoLines;
    UINTN              EntryCount;     // total number of entries registered
    REFIT_MENU_ENTRY **Entries;
    UINTN              TimeoutSeconds;
    CHAR16            *TimeoutText;
    CHAR16            *Hint1;
    CHAR16            *Hint2;
} REFIT_MENU_SCREEN;

typedef struct {
    REFIT_MENU_ENTRY  me;
    CHAR16           *Title;            // For EFI firmware entry, this is "raw" title
    CHAR16           *LoaderPath;
    REFIT_VOLUME     *Volume;
    BOOLEAN           UseGraphicsMode;
    BOOLEAN           Enabled;
    CHAR16           *LoadOptions;
    CHAR16           *InitrdPath;       // Linux stub loader only
    CHAR8             OSType;
    UINTN             DiscoveryType;
    EFI_DEVICE_PATH  *EfiLoaderPath;    // path to NVRAM-defined loader
    UINT16            EfiBootNum;       // Boot#### number for NVRAM-defined loader
} LOADER_ENTRY;

typedef struct {
    REFIT_MENU_ENTRY   me;
    REFIT_VOLUME      *Volume;
    BDS_COMMON_OPTION *BdsOption;
    CHAR16            *LoadOptions;
    BOOLEAN            Enabled;
} LEGACY_ENTRY;

typedef struct {
    BOOLEAN           CustomScreenBG;
    BOOLEAN           TextOnly;
    BOOLEAN           ScanAllLinux;
    BOOLEAN           DeepLegacyScan;
    BOOLEAN           EnableAndLockVMX;
    BOOLEAN           FoldLinuxKernels;
    BOOLEAN           EnableMouse;
    BOOLEAN           EnableTouch;
    BOOLEAN           HiddenTags;
    BOOLEAN           UseNvram;
    BOOLEAN           IgnorePreviousBoot;
    BOOLEAN           IgnoreHiddenIcons;
    BOOLEAN           ExternalHiddenIcons;
    BOOLEAN           PreferHiddenIcons;
    BOOLEAN           TextRenderer;
    BOOLEAN           UgaPassThrough;
    BOOLEAN           ProvideConsoleGOP;
    BOOLEAN           ReloadGOP;
    BOOLEAN           UseDirectGop;
    BOOLEAN           ContinueOnWarning;
    BOOLEAN           ForceTRIM;
    BOOLEAN           DisableCompatCheck;
    BOOLEAN           DisableAMFI;
    BOOLEAN           SupplyNVME;
    BOOLEAN           SupplyAPFS;
    BOOLEAN           SilenceAPFS;
    BOOLEAN           SyncAPFS;
    BOOLEAN           ProtectNVRAM;
    BOOLEAN           ScanAllESP;
    BOOLEAN           TagsHelp;
    BOOLEAN           NormaliseCSR;
    BOOLEAN           ShutdownAfterTimeout;
    BOOLEAN           Install;
    BOOLEAN           WriteSystemdVars;
    UINTN             RequestedScreenWidth;
    UINTN             RequestedScreenHeight;
    UINTN             BannerBottomEdge;
    UINTN             RequestedTextMode;
    UINTN             HideUIFlags;
    UINTN             MaxTags;
    UINTN             GraphicsFor;
    UINTN             LegacyType;
    UINTN             ScanDelay;
    UINTN             MouseSpeed;
    UINTN             IconSizes[4];
    UINTN             BannerScale;
    INTN              ScreensaverTime;
    INTN              Timeout;
    INTN              ScaleUI;
    INTN              ActiveCSR;
    INTN              LogLevel;
    INTN              ScreenR;
    INTN              ScreenG;
    INTN              ScreenB;
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
    CHAR16           *SetBootArgs;
    CHAR16           *ExtraKernelVersionStrings;
    CHAR16           *SpoofOSXVersion;
    UINT32_LIST      *CsrValues;
    UINTN             ShowTools[NUM_TOOLS];
    CHAR8             ScanFor[NUM_SCAN_OPTIONS];
} REFIT_CONFIG;

// Global variables
extern CHAR16              *SelfDirPath;
extern CHAR16              *gHiddenTools;

extern UINTN                VolumesCount;
extern UINTN                RecoveryVolumesCount;
extern UINTN                SkipApfsVolumesCount;
extern UINTN                PreBootVolumesCount;
extern UINTN                SystemVolumesCount;
extern UINTN                DataVolumesCount;

extern BOOLEAN              SetSysTab;
extern BOOLEAN              MuteLogger;
extern BOOLEAN              DetectedDevices;
extern BOOLEAN              NativeLogger;

extern EFI_FILE            *SelfDir;
extern EFI_FILE            *SelfRootDir;

extern EFI_GUID             GlobalGuid;
extern EFI_GUID             RefindPlusGuid;
extern EFI_GUID             gEfiGlobalVariableGuid;
extern EFI_GUID             gEfiLegacyBootProtocolGuid;

extern EFI_HANDLE           SelfImageHandle;

extern EFI_LOADED_IMAGE    *SelfLoadedImage;

extern REFIT_VOLUME        *SelfVolume;
extern REFIT_VOLUME       **Volumes;
extern REFIT_VOLUME       **RecoveryVolumes;
extern REFIT_VOLUME       **SkipApfsVolumes;
extern REFIT_VOLUME       **PreBootVolumes;
extern REFIT_VOLUME       **SystemVolumes;
extern REFIT_VOLUME       **DataVolumes;

extern REFIT_CONFIG         GlobalConfig;

extern REFIT_MENU_SCREEN   *MainMenu;


VOID AboutRefindPlus (VOID);
VOID StoreLoaderName (IN CHAR16 *Name);
VOID RescanAll (BOOLEAN DisplayMessage, BOOLEAN Reconnect);

EG_IMAGE * GetDiskBadge (IN UINTN DiskType);

LOADER_ENTRY * MakeGenericLoaderEntry (VOID);

#endif

/* EOF */
