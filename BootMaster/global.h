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
 * Modifications copyright (c) 2012-2023 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), a copy of which must be distributed
 * with this source code or binaries made from it.
 *
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2023 Dayo Akanji (sf.net/u/dakanji/profile)
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

// Tag classifications ... Used in various ways.
#define TAG_GENERIC               (0)
#define TAG_ABOUT                 (1)
#define TAG_REBOOT                (2)
#define TAG_SHUTDOWN              (3)
#define TAG_TOOL                  (4)
#define TAG_LOADER                (5)
#define TAG_LEGACY                (6)
#define TAG_FIRMWARE_LOADER       (7)
#define TAG_EXIT                  (8)
#define TAG_SHELL                 (9)
#define TAG_GPTSYNC              (10)
#define TAG_LEGACY_UEFI          (11)
#define TAG_RECOVERY_APPLE       (12)
#define TAG_RECOVERY_WINDOWS     (13)
#define TAG_MOK_TOOL             (14)
#define TAG_FIRMWARE             (15)
#define TAG_MEMTEST              (16)
#define TAG_GDISK                (17)
#define TAG_NETBOOT              (18)
#define TAG_CSR_ROTATE           (19)
#define TAG_FWUPDATE_TOOL        (20)
#define TAG_HIDDEN               (21)
#define TAG_INSTALL              (22)
#define TAG_BOOTORDER            (23)
#define TAG_INFO_NVRAMCLEAN      (24)
#define TAG_LOAD_NVRAMCLEAN      (25)
#define NUM_TOOLS                (26)

#define NUM_SCAN_OPTIONS         (11)

// OS bit codes (Actual Decimal) ... Used in GlobalConfig.GraphicsFor
#define GRAPHICS_FOR_OSX          (1)
#define GRAPHICS_FOR_LINUX        (2)
#define GRAPHICS_FOR_ELILO        (4)
#define GRAPHICS_FOR_GRUB         (8)
#define GRAPHICS_FOR_WINDOWS     (16)
#define GRAPHICS_FOR_OPENCORE    (32)
#define GRAPHICS_FOR_CLOVER      (64)

// Type of Legacy Boot support detected
#define LEGACY_TYPE_NONE          (0)
#define LEGACY_TYPE_MAC           (1)
#define LEGACY_TYPE_UEFI          (2)

// Flags how a loader entry was added to a menu
#define DISCOVERY_TYPE_UNKNOWN    (0)
#define DISCOVERY_TYPE_AUTO       (1)
#define DISCOVERY_TYPE_MANUAL     (2)

#ifdef __MAKEWITH_GNUEFI

// Define BBS Device Types
#define BBS_FLOPPY             (0x01)
#define BBS_HARDDISK           (0x02)
#define BBS_CDROM              (0x03)
#define BBS_PCMCIA             (0x04)
#define BBS_USB                (0x05)
#define BBS_EMBED_NETWORK      (0x06)
#define BBS_BEV_DEVICE         (0x80)
#define BBS_UNKNOWN            (0xFF)
#endif

// BIOS Boot Specification (BBS) device types, as returned in DevicePath->Type field
#define DEVICE_TYPE_HW         (0x01)
#define DEVICE_TYPE_ACPI       (0x02) /* returned by UEFI boot loader on USB */
#define DEVICE_TYPE_MESSAGING  (0x03)
#define DEVICE_TYPE_MEDIA      (0x04) /* returned by EFI boot loaders on hard disk */
#define DEVICE_TYPE_BIOS       (0x05) /* returned by Legacy Boot loaders */
#define DEVICE_TYPE_END        (0x75) /* end of path */

// Filesystem type identifiers ... Not all used.
#define FS_TYPE_UNKNOWN           (0)
#define FS_TYPE_WHOLEDISK         (1)
#define FS_TYPE_FAT12             (2)
#define FS_TYPE_FAT16             (3)
#define FS_TYPE_FAT32             (4)
#define FS_TYPE_EXFAT             (5)
#define FS_TYPE_NTFS              (6)
#define FS_TYPE_EXT2              (7)
#define FS_TYPE_EXT3              (8)
#define FS_TYPE_EXT4              (9)
#define FS_TYPE_HFSPLUS          (10)
#define FS_TYPE_REISERFS         (11)
#define FS_TYPE_BTRFS            (12)
#define FS_TYPE_XFS              (13)
#define FS_TYPE_JFS              (14)
#define FS_TYPE_ISO9660          (15)
#define FS_TYPE_APFS             (16)
#define NUM_FS_TYPES             (17)

// How to scale banner images
#define BANNER_NOSCALE            (0)
#define BANNER_FILLSCREEN         (1)

// Sizes of the default icons; badges are 1/4 the big icon size
#define DEFAULT_SMALL_ICON_SIZE  (48)
#define DEFAULT_BIG_ICON_SIZE   (128)
#define DEFAULT_MOUSE_SIZE       (16)

// Codes for types of icon sizes; used for indexing into GlobalConfig.IconSizes[]
#define ICON_SIZE_BADGE           (0)
#define ICON_SIZE_SMALL           (1)
#define ICON_SIZE_BIG             (2)
#define ICON_SIZE_MOUSE           (3)

// Minimum resolutions for a screen to be considered High-DPI
#define HIDPI_LONG             (2560)
#define HIDPI_SHORT            (1600)
#define LOREZ_LIMIT            (1024)
#define BASE_REZ                (600)

#ifndef EFI_OS_INDICATIONS_BOOT_TO_FW_UI
#define EFI_OS_INDICATIONS_BOOT_TO_FW_UI 0x0000000000000001ULL
#endif

#define DEFAULT_ICONS_DIR     L"icons"

// NVRAM ACCESS FLAGS
#define ACCESS_FLAGS_FULL   EFI_VARIABLE_NON_VOLATILE|EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS;
#define ACCESS_FLAGS_BOOT   EFI_VARIABLE_NON_VOLATILE|EFI_VARIABLE_BOOTSERVICE_ACCESS;

// Names of binaries that can manage MOKs
#if defined (EFIX64)
#   define MOK_NAMES \
L"MokManager.efi,HashTool.efi,HashTool-signed.efi,\
KeyTool.efi,KeyTool-signed.efi,mm.efi,mmx64.efi"
#elif defined(EFI32)
#   define MOK_NAMES \
L"MokManager.efi,HashTool.efi,HashTool-signed.efi,\
KeyTool.efi,KeyTool-signed.efi,mm.efi,mmia32.efi"
#elif defined(EFIAARCH64)
#   define MOK_NAMES \
L"MokManager.efi,HashTool.efi,HashTool-signed.efi,\
KeyTool.efi,KeyTool-signed.efi,mm.efi,mmaa64.efi"
#else
#   define MOK_NAMES \
L"MokManager.efi,HashTool.efi,HashTool-signed.efi,\
KeyTool.efi,KeyTool-signed.efi,mm.efi"
#endif

// Names of binaries that can update firmware
#if defined (EFIX64)
#   define FWUPDATE_NAMES     L"fwupx64.efi,fwup.efi"
#elif defined(EFI32)
#   define FWUPDATE_NAMES     L"fwupia32.efi,fwup.efi"
#elif defined(EFIAARCH64)
#   define FWUPDATE_NAMES     L"fwupaa64.efi,fwup.efi"
#else
#   define FWUPDATE_NAMES     L"fwup.efi"
#endif

// Directories to search for these MOK-managing programs.
// Note that SelfDir is searched in addition to these locations.
#define MOK_LOCATIONS \
L"\\,EFI\\tools,EFI\\fedora,EFI\\redhat,EFI\\ubuntu,EFI\\suse,EFI\\opensuse,EFI\\altlinux"

// Directories to search for memtest86
#if defined (EFIX64)
#   define MEMTEST_LOCATIONS \
L"EFI\\tools_x64,EFI\\tools,\
EFI\\tools_x64\\memtest86,EFI\\tools_x64\\memtest,EFI\\tools_x64\\memtest86p,\
EFI\\tools\\memtest86,EFI\\tools\\memtest,EFI\\tools\\memtest86p,\
EFI\\BOOT\\tools_x64\\memtest86,EFI\\BOOT\\tools_x64\\memtest,EFI\\BOOT\\tools_x64\\memtest86p,\
EFI\\BOOT\\tools\\memtest86,EFI\\BOOT\\tools\\memtest,EFI\\BOOT\\tools\\memtest86p,\
EFI\\BOOT\\memtest86,EFI\\BOOT\\memtest,EFI\\BOOT\\memtest86p,\
EFI\\memtest86,EFI\\memtest,EFI\\memtest86p"
#elif defined(EFI32)
#   define MEMTEST_LOCATIONS \
L"EFI\\tools_ia32,EFI\\tools,\
EFI\\tools_ia32\\memtest86,EFI\\tools_ia32\\memtest,EFI\\tools_ia32\\memtest86p,\
EFI\\tools\\memtest86,EFI\\tools\\memtest,EFI\\tools\\memtest86p,\
EFI\\BOOT\\tools_ia32\\memtest86,EFI\\BOOT\\tools_ia32\\memtest,EFI\\BOOT\\tools_ia32\\memtest86p,\
EFI\\BOOT\\tools\\memtest86,EFI\\BOOT\\tools\\memtest,EFI\\BOOT\\tools\\memtest86p,\
EFI\\BOOT\\memtest86,EFI\\BOOT\\memtest,EFI\\BOOT\\memtest86p,\
EFI\\memtest86,EFI\\memtest,EFI\\memtest86p"
#elif defined(EFIAARCH64)
#   define MEMTEST_LOCATIONS \
L"EFI\\tools_aa64,EFI\\tools,\
EFI\\tools_aa64\\memtest86,EFI\\tools_aa64\\memtest,EFI\\tools_aa64\\memtest86p,\
EFI\\tools\\memtest86,EFI\\tools\\memtest,EFI\\tools\\memtest86p,\
EFI\\BOOT\\tools_aa64\\memtest86,EFI\\BOOT\\tools_aa64\\memtest,EFI\\BOOT\\tools_aa64\\memtest86p,\
EFI\\BOOT\\tools\\memtest86,EFI\\BOOT\\tools\\memtest,EFI\\BOOT\\tools\\memtest86p,\
EFI\\BOOT\\memtest86,EFI\\BOOT\\memtest,EFI\\BOOT\\memtest86p,\
EFI\\memtest86,EFI\\memtest,EFI\\memtest86p"
#else
#   define MEMTEST_LOCATIONS \
L"EFI\\tools,\
EFI\\tools\\memtest86,EFI\\tools\\memtest,EFI\\tools\\memtest86p,\
EFI\\BOOT\\tools\\memtest86,EFI\\BOOT\\tools\\memtest,EFI\\BOOT\\tools\\memtest86p,\
EFI\\BOOT\\memtest86,EFI\\BOOT\\memtest,EFI\\BOOT\\memtest86p,\
EFI\\memtest86,EFI\\memtest,EFI\\memtest86p"
#endif

// Files that may be Windows recovery files
#if defined (EFIX64)
#   define WINDOWS_RECOVERY_FILES \
L"EFI\\Microsoft\\Boot\\LrsBootmgr.efi,Recovery:\\EFI\\BOOT\\bootx64.efi,\
Recovery:\\EFI\\BOOT\\boot.efi,EFI\\OEM\\Boot\\bootmgfw.efi"
#elif defined(EFI32)
#   define WINDOWS_RECOVERY_FILES \
L"EFI\\Microsoft\\Boot\\LrsBootmgr.efi,Recovery:\\EFI\\BOOT\\bootia32.efi,\
Recovery:\\EFI\\BOOT\\boot.efi,EFI\\OEM\\Boot\\bootmgfw.efi"
#elif defined(EFIAARCH64)
#   define WINDOWS_RECOVERY_FILES \
L"EFI\\Microsoft\\Boot\\LrsBootmgr.efi,Recovery:\\EFI\\BOOT\\bootaa64.efi,\
Recovery:\\EFI\\BOOT\\boot.efi,EFI\\OEM\\Boot\\bootmgfw.efi"
#else
#   define WINDOWS_RECOVERY_FILES \
L"EFI\\Microsoft\\Boot\\LrsBootmgr.efi,Recovery:\\EFI\\BOOT\\boot.efi,\
EFI\\OEM\\Boot\\bootmgfw.efi"
#endif

// Default Ventoy Partitions
#define VENTOY_NAMES          L"VTOYEFI,Ventoy"

// Misc macOS Paths/Files
#define MACOSX_LOADER_DIR     L"System\\Library\\CoreServices"
#define MACOSX_LOADER_PATH    ( MACOSX_LOADER_DIR L"\\boot.efi" )
#define MACOSX_DIAGNOSTICS    ( MACOSX_LOADER_DIR L"\\.diagnostics\\diags.efi" )

// Files that may be macOS recovery files
#define MACOS_RECOVERY_FILES  L"com.apple.recovery.boot\\boot.efi"

// Filename patterns that identify EFI boot loaders. Note that a single case (either L"*.efi" or
// L"*.EFI") is fine for most systems; but Gigabyte's buggy Hybrid EFI does a case-sensitive
// comparison when it should do a case-insensitive comparison, so I'm doubling this up. It does
// no harm on other computers, AFAIK. In theory, every case variation should be done for
// completeness, but that is ridiculous.
#define LOADER_MATCH_PATTERNS L"*.efi,*.EFI"

// Patterns that identify Linux kernels. Added to the loader match pattern when the
// scan_all_linux_kernels option is set in the configuration file. Causes kernels WITHOUT
// a ".efi" extension to be found when scanning for boot loaders.
#if defined(EFIAARCH64)
#define LINUX_PREFIXES        L"vmlinuz,Image,kernel"
#else
#define LINUX_PREFIXES        L"vmlinuz,bzImage,kernel"
#endif

// Definitions for the "hideui" option in config.conf
#define HIDEUI_FLAG_NONE        (0x0000)
#define HIDEUI_FLAG_BANNER      (0x0001)
#define HIDEUI_FLAG_LABEL       (0x0002)
#define HIDEUI_FLAG_SINGLEUSER  (0x0004)
#define HIDEUI_FLAG_HWTEST      (0x0008)
#define HIDEUI_FLAG_ARROWS      (0x0010)
#define HIDEUI_FLAG_HINTS       (0x0020)
#define HIDEUI_FLAG_EDITOR      (0x0040)
#define HIDEUI_FLAG_SAFEMODE    (0x0080)
#define HIDEUI_FLAG_BADGES      (0x0100)
#define HIDEUI_FLAG_ALL         (0x01FF)

// Default hint text for program-launch submenus
#define SUBSCREEN_HINT1            L"Use arrow keys to move selection and press 'Enter' to run selected item"
#define SUBSCREEN_HINT2            L"Press 'Insert' or 'F2' to edit options or press 'Esc' to return to the main screen"
#define SUBSCREEN_HINT2_NO_EDITOR  L"Press 'Esc' to return to the main screen"

// Other default hint texts
#define SELECT_OPTION_HINT         L"Select an option and press 'Enter' to apply the option"
#define RETURN_MAIN_SCREEN_HINT    L"Press 'ESC', 'BackSpace' or 'SpaceBar' to return to the main screen"


#define NULL_GUID_VALUE              {0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
#define MBR_GUID_VALUE               {0x92A6C61F, 0x7130, 0x49B9, {0xB0, 0x5C, 0x8D, 0x7E, 0x7B, 0x03, 0x91, 0x27}};
#define REFINDPLUS_GUID              {0x36D08FA7, 0xCF0B, 0x42F5, {0x8F, 0x14, 0x68, 0xDF, 0x73, 0xED, 0x37, 0x40}};
#define REFINDPLUS_OLD_GUID          {0xF8800DA7, 0xDF1F, 0x4A16, {0x8F, 0xE3, 0x72, 0x43, 0xDB, 0xB7, 0x87, 0xCA}};
#define OPENCORE_VENDOR_GUID         {0x4D1FDA02, 0x38C7, 0x4A6A, {0x9C, 0xC6, 0x4B, 0xCC, 0xA8, 0xB3, 0x01, 0x02}};
#define ESP_GUID_VALUE               {0xC12A7328, 0xF81F, 0x11D2, {0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B}};
#define HFS_GUID_VALUE               {0x48465300, 0x0000, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}};
#define APFS_GUID_VALUE              {0x7C3457EF, 0x0000, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}};
#define APFS_FINGER_PRINT_GUID       {0xBE74FCF7, 0x0B7C, 0x49F3, {0x91, 0x47, 0x01, 0xF4, 0x04, 0x2E, 0x68, 0x42}};
#define APPLE_TV_RECOVERY_GUID       {0x5265636F, 0x7665, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}};
#define MAC_RAID_ON_GUID_VALUE       {0x52414944, 0x0000, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}};
#define MAC_RAID_OFF_GUID_VALUE      {0x52414944, 0x5F4F, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}};
#define CONTAINER_HFS_GUID_VALUE     {0x53746F72, 0x6167, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}};
#define MAC_RECOVERY_HD_GUID_VALUE   {0x426F6F74, 0x0000, 0x11AA, {0xAA, 0x11, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC}};
#define WIN_RECOVERY_ENV_GUID_VALUE  {0xDE94BBA4, 0x06D1, 0x4D40, {0xA1, 0x6A, 0xBF, 0xD5, 0x01, 0x79, 0xD6, 0xAC}};
#define BASIC_DATA_GUID_VALUE        {0xEBD0A0A2, 0xB9E5, 0x4433, {0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7}};
#define MICROSOFT_VENDOR_GUID        {0x77FA9ABD, 0x0359, 0x4D32, {0xBD, 0x60, 0x28, 0xF4, 0xE7, 0x8F, 0x78, 0x4B}};
#define MSFT_RESERVED_GUID_VALUE     {0xE3C9E316, 0x0B5C, 0x4DB8, {0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE}};
#define SYSTEMD_GUID_VALUE           {0x4A67B082, 0x0A4C, 0x41CF, {0xB6, 0xC7, 0x44, 0x0B, 0x29, 0xBB, 0x8C, 0x4F}};
#define LINUX_GUID_VALUE             {0x0FC63DAF, 0x8483, 0x4772, {0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4}};
#define GPT_HOME_GUID_VALUE          {0x933AC7E1, 0x2EB4, 0x4F13, {0xB8, 0x44, 0x0E, 0x14, 0xE2, 0xAE, 0xF9, 0x15}};
#define GPT_SERVER_GUID_VALUE        {0x3B8F8425, 0x20E0, 0x4F3B, {0x90, 0x7F, 0x1A, 0x25, 0xA7, 0x6F, 0x98, 0xE8}};
#define OC_BOOTSTRAP_GUID_VALUE      {0xBA1EB455, 0xB182, 0x4F14, {0x85, 0x21, 0xE4, 0x22, 0xC3, 0x25, 0xDE, 0xF6}};

// Configuration file variables
#define KERNEL_VERSION L"%v"
#define MAX_RES_CODE 2147483647 /* 2^31 - 1 */


#ifdef __MAKEWITH_TIANO
// DA-TAG: Forward Declaration for OpenCore Integration
//         Limit to TianoCore Builds
EFI_STATUS OcProvideConsoleGop (IN BOOLEAN Route);
EFI_STATUS OcProvideUgaPassThrough (VOID);
EFI_STATUS OcUseDirectGop (IN INT32 CacheType);
EFI_STATUS OcUseBuiltinTextOutput (IN EFI_CONSOLE_CONTROL_SCREEN_MODE Mode);
#endif


#define LOGLEVELOFF (-1)
#define MINLOGLEVEL  (0)
#define MAXLOGLEVEL  (1)


//
// Global definitions
//

// global types
typedef struct _uint32_list {
    UINT32                      Value;
    struct _uint32_list        *Next;
} UINT32_LIST;

typedef struct {
    UINT8                      Flags;
    UINT8                      StartCHS1;
    UINT8                      StartCHS2;
    UINT8                      StartCHS3;
    UINT8                      Type;
    UINT8                      EndCHS1;
    UINT8                      EndCHS2;
    UINT8                      EndCHS3;
    UINT32                     StartLBA;
    UINT32                     Size;
} MBR_PARTITION_INFO;

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
    EFI_HANDLE                 DeviceHandle;
    EFI_FILE_PROTOCOL         *RootDir;
    CHAR16                    *PartName;
    CHAR16                    *FsName;   // Filesystem name
    CHAR16                    *VolName;  // One of two above OR fs desc (e.g., "2 GiB FAT volume")
    UINT32                     VolRole;  // APFS_VOLUME_ROLE
    EFI_GUID                   VolUuid;
    EFI_GUID                   PartGuid;
    EFI_GUID                   PartTypeGuid;
    BOOLEAN                    IsMarkedReadOnly;
    EG_IMAGE                  *VolIconImage;
    EG_IMAGE                  *VolBadgeImage;
    UINTN                      DiskKind;
    BOOLEAN                    HasBootCode;
    CHAR16                    *OSIconName;
    CHAR16                    *OSName;
    BOOLEAN                    IsMbrPartition;
    UINTN                      MbrPartitionIndex;
    EFI_BLOCK_IO_PROTOCOL     *BlockIO;
    UINT64                     BlockIOOffset;
    EFI_BLOCK_IO_PROTOCOL     *WholeDiskBlockIO;
    EFI_DEVICE_PATH_PROTOCOL  *WholeDiskDevicePath;
    MBR_PARTITION_INFO        *MbrPartitionTable;
    BOOLEAN                    IsReadable;
    UINT32                     FSType;
} REFIT_VOLUME;

typedef struct _refit_menu_entry {
    CHAR16                    *Title;
    UINTN                      Tag;
    UINTN                      Row;
    CHAR16                     ShortcutDigit;
    CHAR16                     ShortcutLetter;
    EG_IMAGE                  *Image;
    EG_IMAGE                  *BadgeImage;
    struct _refit_menu_screen *SubScreen;
} REFIT_MENU_ENTRY;

typedef struct _refit_menu_screen {
    CHAR16                    *Title;             // EFI firmware entry ... Includes "Reboot to" prefix
    EG_IMAGE                  *TitleImage;
    UINTN                      InfoLineCount;
    CHAR16                   **InfoLines;
    UINTN                      EntryCount;        // Total number of entries registered
    REFIT_MENU_ENTRY         **Entries;
    UINTN                      TimeoutSeconds;
    CHAR16                    *TimeoutText;
    CHAR16                    *Hint1;
    CHAR16                    *Hint2;
} REFIT_MENU_SCREEN;

typedef struct {
    REFIT_MENU_ENTRY            me;
    CHAR16                     *Title;            // EFI firmware entry "raw" title
    CHAR16                     *LoaderPath;
    REFIT_VOLUME               *Volume;
    BOOLEAN                     UseGraphicsMode;
    BOOLEAN                     Enabled;
    CHAR16                     *LoadOptions;
    CHAR16                     *InitrdPath;       // Linux stub loader
    CHAR8                       OSType;
    UINTN                       DiscoveryType;
    EFI_DEVICE_PATH_PROTOCOL   *EfiLoaderPath;    // Path to NVRAM-defined loader
    UINT16                      EfiBootNum;       // Boot#### number for NVRAM-defined loader
} LOADER_ENTRY;

typedef struct {
    REFIT_MENU_ENTRY            me;
    REFIT_VOLUME               *Volume;
    BDS_COMMON_OPTION          *BdsOption;
    CHAR16                     *LoadOptions;
    BOOLEAN                     Enabled;
} LEGACY_ENTRY;

typedef struct {
    BOOLEAN                     DirectBoot;
    BOOLEAN                     CustomScreenBG;
    BOOLEAN                     TextOnly;
    BOOLEAN                     ScanAllLinux;
    BOOLEAN                     DeepLegacyScan;
    BOOLEAN                     RescanDXE;
    BOOLEAN                     RansomDrives;
    BOOLEAN                     EnableAndLockVMX;
    BOOLEAN                     FoldLinuxKernels;
    BOOLEAN                     EnableMouse;
    BOOLEAN                     EnableTouch;
    BOOLEAN                     PersistBootArgs;
    BOOLEAN                     HiddenTags;
    BOOLEAN                     UseNvram;
    BOOLEAN                     TransientBoot;
    BOOLEAN                     HiddenIconsIgnore;
    BOOLEAN                     HiddenIconsExternal;
    BOOLEAN                     HiddenIconsPrefer;
    BOOLEAN                     UseTextRenderer;
    BOOLEAN                     PassUgaThrough;
    BOOLEAN                     SetConsoleGOP;
    BOOLEAN                     ReloadGOP;
    BOOLEAN                     UseDirectGop;
    BOOLEAN                     ContinueOnWarning;
    BOOLEAN                     ForceTRIM;
    BOOLEAN                     DisableCompatCheck;
    BOOLEAN                     DisableNvramPanicLog;
    BOOLEAN                     DecoupleKeyF10;
    BOOLEAN                     DisableAMFI;
    BOOLEAN                     NvramProtectEx;
    BOOLEAN                     FollowSymlinks;
    BOOLEAN                     GzippedLoaders;
    BOOLEAN                     PreferUGA;
    BOOLEAN                     SupplyNVME;
    BOOLEAN                     SupplyAPFS;
    BOOLEAN                     SupplyUEFI;
    BOOLEAN                     SilenceAPFS;
    BOOLEAN                     SyncAPFS;
    BOOLEAN                     NvramProtect;
    BOOLEAN                     ScanAllESP;
    BOOLEAN                     HelpIcon;
    BOOLEAN                     HelpScan;
    BOOLEAN                     HelpTags;
    BOOLEAN                     HelpText;
    BOOLEAN                     NormaliseCSR;
    BOOLEAN                     ShutdownAfterTimeout;
    BOOLEAN                     Install;
    BOOLEAN                     WriteSystemdVars;
    BOOLEAN                     UnicodeCollation;
    BOOLEAN                     SetAppleFB;
    BOOLEAN                     MitigatePrimedBuffer;
    UINTN                       RequestedScreenWidth;
    UINTN                       RequestedScreenHeight;
    UINTN                       BannerBottomEdge;
    UINTN                       RequestedTextMode;
    UINTN                       HideUIFlags;
    UINTN                       MaxTags;
    UINTN                       GraphicsFor;
    UINTN                       LegacyType;
    UINTN                       ScanDelay;
    UINTN                       MouseSpeed;
    UINTN                       IconSizes[4];
    UINTN                       BannerScale;
    UINTN                       NvramVariableLimit;
    INTN                        ScreensaverTime;
    INTN                        Timeout;
    INTN                        ScaleUI;
    INTN                        DynamicCSR;
    INTN                        LogLevel;
    INTN                        IconRowMove;
    INTN                        IconRowTune;
    INTN                        ScreenR;
    INTN                        ScreenG;
    INTN                        ScreenB;
    REFIT_VOLUME               *DiscoveredRoot;
    EFI_DEVICE_PATH_PROTOCOL   *SelfDevicePath;
    EG_IMAGE                   *ScreenBackground;
    CHAR16                     *BannerFileName;
    CHAR16                     *ConfigFilename;
    CHAR16                     *SelectionSmallFileName;
    CHAR16                     *SelectionBigFileName;
    CHAR16                     *DefaultSelection;
    CHAR16                     *AlsoScan;
    CHAR16                     *DontScanVolumes;
    CHAR16                     *DontScanDirs;
    CHAR16                     *DontScanFiles;
    CHAR16                     *DontScanTools;
    CHAR16                     *DontScanFirmware;
    CHAR16                     *WindowsRecoveryFiles;
    CHAR16                     *MacOSRecoveryFiles;
    CHAR16                     *DriverDirs;
    CHAR16                     *IconsDir;
    CHAR16                     *LinuxPrefixes;      // Linux prefixes (e.g., L"vmlinuz,bzImage"
    CHAR16                     *LinuxMatchPatterns; // Linux prefixes PLUS wildcards (e.g., L"vmlinuz*,bzImage*")
    CHAR16                     *SetBootArgs;
    CHAR16                     *ExtraKernelVersionStrings;
    CHAR16                     *SpoofOSXVersion;
    UINT32_LIST                *CsrValues;
    UINTN                       ShowTools[NUM_TOOLS];
    CHAR8                       ScanFor[NUM_SCAN_OPTIONS];
} REFIT_CONFIG;

// Global variables
extern CHAR16                  *OffsetNext;
extern CHAR16                  *SelfDirPath;
extern CHAR16                  *gHiddenTools;

extern UINTN                    PadPosition;
extern UINTN                    VolumesCount;
extern UINTN                    RecoveryVolumesCount;
extern UINTN                    SkipApfsVolumesCount;
extern UINTN                    PreBootVolumesCount;
extern UINTN                    SystemVolumesCount;
extern UINTN                    DataVolumesCount;
extern UINTN                    HfsRecoveryCount;

extern UINT64                   ReadWriteCreate;

extern BOOLEAN                  SingleAPFS;
extern BOOLEAN                  MuteLogger;
extern BOOLEAN                  NativeLogger;
extern BOOLEAN                  AppleFirmware;
extern BOOLEAN                  DevicePresence;

extern EFI_FILE_PROTOCOL       *SelfDir;
extern EFI_FILE_PROTOCOL       *SelfRootDir;

extern EFI_GUID                 GlobalGuid;
extern EFI_GUID                 RefindPlusGuid;
extern EFI_GUID                 AppleBootGuid;
extern EFI_GUID                 gEfiLegacyBootProtocolGuid;

extern EFI_HANDLE               SelfImageHandle;

extern EFI_LOADED_IMAGE_PROTOCOL *SelfLoadedImage;

extern REFIT_VOLUME            *SelfVolume;
extern REFIT_VOLUME           **Volumes;
extern REFIT_VOLUME           **RecoveryVolumes;
extern REFIT_VOLUME           **SkipApfsVolumes;
extern REFIT_VOLUME           **PreBootVolumes;
extern REFIT_VOLUME           **SystemVolumes;
extern REFIT_VOLUME           **DataVolumes;
extern REFIT_VOLUME           **HfsRecovery;

extern REFIT_CONFIG             GlobalConfig;

extern REFIT_MENU_SCREEN       *MainMenu;


VOID StoreLoaderName (IN CHAR16 *Name);
VOID RescanAll (BOOLEAN Reconnect);

EG_IMAGE * GetDiskBadge (IN UINTN DiskType);

LOADER_ENTRY * MakeGenericLoaderEntry (VOID);



/* Misc Extra Items - START */
#define MY_OFFSET_OF(st, m) ((UINTN)((char *) &((st *)0x100000)->m - (char *)0x100000))

#define LOG_BLOCK_SEP         (0)
#define LOG_BLANK_LINE_SEP    (1)
#define LOG_LINE_SPECIAL      (2)
#define LOG_LINE_SAME         (3)
#define LOG_LINE_NORMAL       (4)
#define LOG_LINE_SEPARATOR    (5)
#define LOG_LINE_THIN_SEP     (6)
#define LOG_STAR_SEPARATOR    (7)
#define LOG_LINE_DASH_SEP     (8)
#define LOG_THREE_STAR_SEP    (9)
#define LOG_THREE_STAR_MID   (10)
#define LOG_THREE_STAR_END   (11)
#define LOG_STAR_HEAD_SEP    (12)
#define LOG_LINE_FORENSIC    (13)
#define LOG_LINE_EXIT        (14)
#define LOG_LINE_BASE        (15)

VOID DeepLoggger (
    IN  INTN     level,
    IN  INTN     type,
    IN  CHAR16 **Msg
);

VOID WayPointer (
    IN CHAR16  *Msg
);


// NB: 'gLogTemp' is freed in 'DeepLoggger'
extern CHAR16 *gLogTemp;

#if REFIT_DEBUG > 0
extern VOID LogPadding (BOOLEAN Increment);
extern VOID EFIAPI DebugLog (
    IN const CHAR8 *FormatString,
    ...
);

#   define ALT_LOG(level, type, ...)                                        \
        do {                                                                \
            if (!gKernelStarted) {                                          \
                gLogTemp = PoolPrint (__VA_ARGS__);                         \
                DeepLoggger (level, type, &gLogTemp);                       \
            }                                                               \
        } while (0)
#   define LOG_MSG(...) DebugLog (__VA_ARGS__);
#   define OUT_TAG() WayPointer (L"<<------ * ------>>");
#   define RET_TAG() WayPointer (L"------>> * <<------");
#   define END_TAG() WayPointer (L"<<<      *      >>>");
#else
#   define END_TAG()
#   define RET_TAG()
#   define OUT_TAG()
#   define LOG_MSG(...)
#   define ALT_LOG(...)
#endif

#if REFIT_DEBUG < 1
#   define LOG_INCREMENT(...)
#   define LOG_DECREMENT(...)
#   define BREAD_CRUMB(...)
#   define LOG_SEP(...)
#   define BRK_MAX(...)
#   define BRK_MOD(...)
#   define BRK_MIN(...)
#elif REFIT_DEBUG < 2
#   define BRK_MIN(...)                                                     \
        do {                                                                \
            if (!gKernelStarted && GlobalConfig.LogLevel == MINLOGLEVEL) {  \
                DebugLog (__VA_ARGS__);                                     \
            }                                                               \
        } while (0)
#   define BRK_MOD(...)                                                     \
        do {                                                                \
            if (!gKernelStarted) {                                          \
                DebugLog (__VA_ARGS__);                                     \
            }                                                               \
        } while (0)
#   define BRK_MAX(...)
#   define LOG_SEP(...)
#   define BREAD_CRUMB(...)
#   define LOG_DECREMENT(...)
#   define LOG_INCREMENT(...)
#else
#   define LOG_INCREMENT(...)                                               \
        do {                                                                \
            if (!gKernelStarted) {                                          \
                LogPadding (TRUE);                                          \
            }                                                               \
        } while (0)
#   define LOG_DECREMENT(...)                                               \
        do {                                                                \
            if (!gKernelStarted) {                                          \
                LogPadding (FALSE);                                         \
            }                                                               \
        } while (0)
#   define BREAD_CRUMB(...)                                                 \
        do {                                                                \
            if (!gKernelStarted && GlobalConfig.LogLevel > MAXLOGLEVEL) {   \
                gLogTemp = PoolPrint (__VA_ARGS__);                         \
                DeepLoggger (2, LOG_LINE_FORENSIC, &gLogTemp);              \
            }                                                               \
        } while (0)
#   define LOG_SEP(...)                                                     \
        do {                                                                \
            if (!gKernelStarted && GlobalConfig.LogLevel > MAXLOGLEVEL) {   \
                gLogTemp = PoolPrint (__VA_ARGS__);                         \
                DeepLoggger (2, LOG_BLOCK_SEP, &gLogTemp);                  \
            }                                                               \
        } while (0)
#   define BRK_MAX(...)                                                     \
        do {                                                                \
            if (!gKernelStarted && GlobalConfig.LogLevel > MAXLOGLEVEL) {   \
                DebugLog (__VA_ARGS__);                                     \
            }                                                               \
        } while (0)
#   define BRK_MOD(...)                                                     \
        do {                                                                \
            if (!gKernelStarted && GlobalConfig.LogLevel <= MAXLOGLEVEL) {  \
                DebugLog (__VA_ARGS__);                                     \
            }                                                               \
        } while (0)
#   define BRK_MIN(...)                                                     \
        do {                                                                \
            if (!gKernelStarted && GlobalConfig.LogLevel == MINLOGLEVEL) {  \
                DebugLog (__VA_ARGS__);                                     \
            }                                                               \
        } while (0)
#endif
/* Misc Extra Items - END */


#endif

/* EOF */
