/*
 * BootMaster/main.c
 * Main code for the boot menu
 *
 * Copyright (c) 2006-2010 Christoph Pfisterer
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
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2023 Dayo Akanji (sf.net/u/dakanji/profile)
 * Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "config.h"
#include "screenmgt.h"
#include "launch_legacy.h"
#include "lib.h"
#include "icns.h"
#include "menu.h"
#include "mok.h"
#include "scan.h"
#include "apple.h"
#include "install.h"
#include "mystrings.h"
#include "launch_efi.h"
#include "driver_support.h"
#include "security_policy.h"
#include "../include/refit_call_wrapper.h"
#include "../libeg/efiConsoleControl.h"
#include "../libeg/efiUgaDraw.h"
#include "../include/version.h"
#include "../libeg/libeg.h"

#ifndef __MAKEWITH_GNUEFI
#define LibLocateProtocol EfiLibLocateProtocol
#endif

INT16 NowYear   = 0;
INT16 NowMonth  = 0;
INT16 NowDay    = 0;
INT16 NowHour   = 0;
INT16 NowMinute = 0;
INT16 NowSecond = 0;

REFIT_MENU_SCREEN *MainMenu = NULL;

REFIT_CONFIG GlobalConfig = {
    /* DirectBoot = */ FALSE,
    /* CustomScreenBG = */ FALSE,
    /* TextOnly = */ FALSE,
    /* ScanAllLinux = */ TRUE,
    /* DeepLegacyScan = */ FALSE,
    /* RescanDXE = */ TRUE,
    /* RansomDrives = */ FALSE,
    /* EnableAndLockVMX = */ FALSE,
    /* FoldLinuxKernels = */ TRUE,
    /* EnableMouse = */ FALSE,
    /* EnableTouch = */ FALSE,
    /* HiddenTags = */ TRUE,
    /* UseNvram = */ FALSE,
    /* TransientBoot = */ FALSE,
    /* HiddenIconsIgnore = */ FALSE,
    /* HiddenIconsExternal = */ FALSE,
    /* HiddenIconsPrefer = */ FALSE,
    /* UseTextRenderer = */ FALSE,
    /* PassUgaThrough = */ FALSE,
    /* ProvideConsoleGOP = */ FALSE,
    /* ReloadGOP = */ TRUE,
    /* UseDirectGop = */ FALSE,
    /* ContinueOnWarning = */ FALSE,
    /* ForceTRIM = */ FALSE,
    /* DisableCompatCheck = */ FALSE,
    /* DisableNvramPanicLog = */ FALSE,
    /* DecoupleKeyF10 = */ FALSE,
    /* DisableAMFI = */ FALSE,
    /* NvramProtectEx = */ FALSE,
    /* FollowSymlinks = */ FALSE,
    /* GzippedLoaders = */ FALSE,
    /* PreferUGA = */ FALSE,
    /* SupplyNVME = */ FALSE,
    /* SupplyAPFS = */ TRUE,
    /* SupplyUEFI = */ FALSE,
    /* SilenceAPFS = */ TRUE,
    /* SyncAPFS = */ TRUE,
    /* NvramProtect = */ TRUE,
    /* ScanAllESP = */ TRUE,
    /* HelpIcon = */ TRUE,
    /* HelpTags = */ TRUE,
    /* HelpText = */ TRUE,
    /* NormaliseCSR = */ FALSE,
    /* ShutdownAfterTimeout = */ FALSE,
    /* Install = */ FALSE,
    /* WriteSystemdVars = */ FALSE,
    /* UnicodeCollation = */ FALSE,
    /* SupplyAppleFB = */ TRUE,
    /* MitigatePrimedBuffer = */ FALSE,
    /* RequestedScreenWidth = */ 0,
    /* RequestedScreenHeight = */ 0,
    /* BannerBottomEdge = */ 0,
    /* RequestedTextMode = */ DONT_CHANGE_TEXT_MODE,
    /* HideUIFlags = */ 0,
    /* MaxTags = */ 0,
    /* GraphicsFor = */ GRAPHICS_FOR_OSX,
    /* LegacyType = */ LEGACY_TYPE_MAC,
    /* ScanDelay = */ 0,
    /* MouseSpeed = */ 4,
    /* IconSizes = */ {
        DEFAULT_BIG_ICON_SIZE / 4,
        DEFAULT_SMALL_ICON_SIZE,
        DEFAULT_BIG_ICON_SIZE,
        DEFAULT_MOUSE_SIZE
    },
    /* BannerScale = */ BANNER_NOSCALE,
    /* NvramVariableLimit = */ 0,
    /* ScreensaverTime = */ 0,
    /* Timeout = */ 0,
    /* ScaleUI = */ 0,
    /* DynamicCSR = */ 0,
    /* LogLevel = */ 0,
    /* IconRowMove = */ 0,
    /* IconRowTune = */ 0,
    /* ScreenR = */ -1,
    /* ScreenG = */ -1,
    /* ScreenB = */ -1,
    /* *DiscoveredRoot = */ NULL,
    /* *SelfDevicePath = */ NULL,
    /* *ScreenBackground = */ NULL,
    /* *BannerFileName = */ NULL,
    /* *ConfigFilename = */ CONFIG_FILE_NAME,
    /* *SelectionSmallFileName = */ NULL,
    /* *SelectionBigFileName = */ NULL,
    /* *DefaultSelection = */ NULL,
    /* *AlsoScan = */ NULL,
    /* *DontScanVolumes = */ NULL,
    /* *DontScanDirs = */ NULL,
    /* *DontScanFiles = */ NULL,
    /* *DontScanTools = */ NULL,
    /* *DontScanFirmware = */ NULL,
    /* *WindowsRecoveryFiles = */ NULL,
    /* *MacOSRecoveryFiles = */ NULL,
    /* *DriverDirs = */ NULL,
    /* *IconsDir = */ NULL,
    /* *LinuxPrefixes = */ NULL,
    /* *LinuxMatchPatterns = */ NULL,
    /* *SetBootArgs = */ NULL,
    /* *ExtraKernelVersionStrings = */ NULL,
    /* *SpoofOSXVersion = */ NULL,
    /* CsrValues = */ NULL,
    /* ShowTools = */ {
        TAG_SHELL,
        TAG_MEMTEST,
        TAG_GDISK,
        TAG_RECOVERY_APPLE,
        TAG_RECOVERY_WINDOWS,
        TAG_MOK_TOOL,
        TAG_ABOUT,
        TAG_HIDDEN,
        TAG_SHUTDOWN,
        TAG_REBOOT,
        TAG_FIRMWARE,
        TAG_FWUPDATE_TOOL,
        0, 0, 0, 0, 0, 0, 0, 0, 0
    }
};

#define NVRAMCLEAN_FILES L"\\EFI\\BOOT\\tools_x64\\x64_CleanNvram.efi,\\EFI\\BOOT\\tools_x64\\CleanNvram_x64.efi,\
\\EFI\\BOOT\\tools_x64\\CleanNvram.efi,\\EFI\\BOOT\\tools\\x64_CleanNvram.efi,\\EFI\\BOOT\\tools\\CleanNvram_x64.efi,\
\\EFI\\BOOT\\tools\\CleanNvram.efi,\\EFI\\tools\\x64_CleanNvram.efi,\\EFI\\tools\\CleanNvram_x64.efi,\
\\EFI\\tools\\CleanNvram.efi,\\EFI\\tools_x64\\x64_CleanNvram.efi,\\EFI\\tools_x64\\CleanNvram_x64.efi,\
\\EFI\\tools_x64\\CleanNvram.efi,\\EFI\\x64_CleanNvram.efi,\\EFI\\CleanNvram_x64.efi,\\EFI\\CleanNvram.efi,\
\\x64_CleanNvram.efi,\\CleanNvram_x64.efi,\\CleanNvram.efi"

#define RP_NVRAM_VARIABLES L"PreviousBoot,HiddenTags,HiddenTools,HiddenLegacy,HiddenFirmware"


UINTN                  AppleFramebuffers    = 0;
UINT32                 AccessFlagsBoot      = ACCESS_FLAGS_BOOT;
UINT32                 AccessFlagsFull      = ACCESS_FLAGS_FULL;
CHAR16                *ArchType             = NULL;
CHAR16                *VendorInfo           = NULL;
CHAR16                *gHiddenTools         = NULL;
BOOLEAN                gKernelStarted       = FALSE;
BOOLEAN                IsBoot               = FALSE;
BOOLEAN                ConfigWarn           = FALSE;
BOOLEAN                OverrideSB           = FALSE;
BOOLEAN                OneMainLoop          = FALSE;
BOOLEAN                BlockRescan          = FALSE;
BOOLEAN                NativeLogger         = FALSE;
BOOLEAN                IconScaleSet         = FALSE;
BOOLEAN                ForceTextOnly        = FALSE;
BOOLEAN                ranCleanNvram        = FALSE;
BOOLEAN                AppleFirmware        = FALSE;
BOOLEAN                FlushFailedTag       = FALSE;
BOOLEAN                FlushFailReset       = FALSE;
BOOLEAN                WarnVersionEFI       = FALSE;
BOOLEAN                WarnRevisionUEFI     = FALSE;
BOOLEAN                WarnMissingQVInfo    = FALSE;
BOOLEAN                SecureBootFailure    = FALSE;
EFI_GUID               RefindPlusGuid       = REFINDPLUS_GUID;
EFI_GUID               RefindPlusOldGuid    = REFINDPLUS_OLD_GUID;
EFI_GUID               OpenCoreVendorGuid   = OPENCORE_VENDOR_GUID;
EFI_SET_VARIABLE       OrigSetVariableRT;
EFI_OPEN_PROTOCOL      OrigOpenProtocolBS;

#define NVRAM_SIZE_THRESHOLD       (1023)

#define BOOT_FIX_STR_01            L"Disable AMFI Check"
#define BOOT_FIX_STR_02            L"Disable Compatibility Check"
#define BOOT_FIX_STR_03            L"Disable Paniclog Writes to NVRAM"

extern VOID              InitBooterLog (VOID);

extern EFI_STATUS        AmendSysTable (VOID);
extern EFI_STATUS        RP_ApfsConnectDevices (VOID);
extern EFI_STATUS EFIAPI NvmExpressLoad (
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
);

#ifdef __MAKEWITH_TIANO
extern VOID              OcUnblockUnmountedPartitions (VOID);
#endif

extern CHAR16                       *StrSelfUUID;

extern EFI_GUID                      AppleVendorOsGuid;

extern EFI_FILE_PROTOCOL            *gVarsDir;

extern BOOLEAN                       HasMacOS;
extern BOOLEAN                       SetSysTab;
extern BOOLEAN                       SelfVolSet;
extern BOOLEAN                       SelfVolRun;
extern BOOLEAN                       SubScreenBoot;
extern BOOLEAN                       ForceRescanDXE;

extern EFI_GRAPHICS_OUTPUT_PROTOCOL *GOPDraw;


//
// Misc functions
//

#if REFIT_DEBUG > 0
static
VOID UnexpectedReturn (
    IN CHAR16 *ItemType
) {
    CHAR16 *MsgStr;

    MsgStr = PoolPrint (L"WARN: Unexpected Return from %s", ItemType);
    ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
    LOG_MSG("\n\n");
    LOG_MSG("*** %s", MsgStr);
    LOG_MSG("\n\n");
    MY_FREE_POOL(MsgStr);
} // static VOID UnexpectedReturn()
#endif

static
VOID InitMainMenu (VOID) {
    REFIT_MENU_SCREEN MainMenuSrc = {
        L"Main Menu",
        NULL, 0,
        NULL, 0,
        NULL, 0,
        L"Automatic Boot",
        SUBSCREEN_HINT1,
        L"Press 'Insert', 'Tab', or 'F2' for more options. Press 'Esc' or 'Backspace' to refresh the screen"
    };

    FreeMenuScreen (&MainMenu);
    MainMenu = CopyMenuScreen (&MainMenuSrc);
    MainMenu->TimeoutSeconds = GlobalConfig.Timeout;
} // static VOID InitMainMenu()

static
EFI_STATUS GetHardwareNvramVariable (
    IN  CHAR16    *VariableName,
    IN  EFI_GUID  *VendorGuid,
    OUT VOID     **VariableData,
    OUT UINTN     *VariableSize  OPTIONAL
) {
    EFI_STATUS   Status;
    UINTN        BufferSize;
    VOID        *TmpBuffer;

    // Pass in a zero-size buffer to find the required buffer size.
    // If the variable exists, the status should be EFI_BUFFER_TOO_SMALL and BufferSize updated.
    // Any other status means the variable does not exist.
    BufferSize = 0;
    TmpBuffer  = NULL;
    Status = REFIT_CALL_5_WRAPPER(
        gRT->GetVariable, VariableName,
        VendorGuid, NULL,
        &BufferSize, TmpBuffer
    );
    if (Status != EFI_BUFFER_TOO_SMALL) {
        return EFI_NOT_FOUND;
    }

    TmpBuffer = AllocateZeroPool (BufferSize);
    if (!TmpBuffer) {
        return EFI_OUT_OF_RESOURCES;
    }

    // Retry with the correct buffer size.
    Status = REFIT_CALL_5_WRAPPER(
        gRT->GetVariable, VariableName,
        VendorGuid, NULL,
        &BufferSize, TmpBuffer
    );
    if (EFI_ERROR(Status)) {
        MY_FREE_POOL(TmpBuffer);
        *VariableData = NULL;
        *VariableSize = 0;

        return EFI_LOAD_ERROR;
    }

    *VariableData = TmpBuffer;
    *VariableSize = (BufferSize) ? BufferSize : 0;

    return EFI_SUCCESS;
} // static EFI_STATUS GetHardwareNvramVariable()

static
EFI_STATUS SetHardwareNvramVariable (
    IN  CHAR16    *VariableName,
    IN  EFI_GUID  *VendorGuid,
    IN  UINT32     Attributes,
    IN  UINTN      VariableSize,
    IN  VOID      *VariableData OPTIONAL
) {
    EFI_STATUS   Status;
    VOID        *OldBuf;
    UINTN        OldSize;
    BOOLEAN      SettingMatch;

    OldSize = 0;
    OldBuf  = NULL;
    Status  = EFI_LOAD_ERROR;

    if (VariableData && VariableSize != 0) {
        Status = GetHardwareNvramVariable (
            VariableName, VendorGuid,
            &OldBuf, &OldSize
        );
        if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
            // Early Return
            return Status;
        }
    }

    SettingMatch = FALSE;
    if (!EFI_ERROR(Status)) {
        if (VariableData && VariableSize != 0) {
            // Check for match
            SettingMatch = (
                VariableSize == OldSize &&
                CompareMem (VariableData, OldBuf, VariableSize) == 0
            );
        }
        MY_FREE_POOL(OldBuf);

        if (SettingMatch) {
            // Early Return
            return EFI_ALREADY_STARTED;
        }
    }

    // Store the new value
    Status = OrigSetVariableRT (
        VariableName, VendorGuid,
        Attributes, VariableSize, VariableData
    );

    return Status;
} // EFI_STATUS SetHardwareNvramVariable()


static
EFI_STATUS EFIAPI gRTSetVariableEx (
    IN  CHAR16    *VariableName,
    IN  EFI_GUID  *VendorGuid,
    IN  UINT32     Attributes,
    IN  UINTN      VariableSize,
    IN  VOID      *VariableData
) {
    if (!VariableName || !VendorGuid || !Attributes) {
        return EFI_INVALID_PARAMETER;
    }

    EFI_STATUS     Status;
    EFI_GUID       VendorMS = MICROSOFT_VENDOR_GUID;

    BOOLEAN IsVendorMS;
    BOOLEAN CurPolicyOEM;
    BOOLEAN BlockCert;
    BOOLEAN BlockUEFI;
    BOOLEAN BlockSize;
    BOOLEAN RevokeVar;
    BOOLEAN HardNVRAM;

    RevokeVar = (!VariableData && VariableSize == 0);
    IsVendorMS = GuidsAreEqual (VendorGuid, &VendorMS);
    CurPolicyOEM = (IsVendorMS && MyStriCmp (VariableName, L"CurrentPolicy"));

    if (!CurPolicyOEM && !RevokeVar) {
        // DA-TAG: Always block UEFI Win stuff on Apple Firmware
        //         That is, without considering storage volatility
        BlockUEFI = (
            AppleFirmware && (IsVendorMS || FindSubStr (VariableName, L"UnlockID"))
        );
    }
    else {
        BlockUEFI = FALSE;
    }

    #if REFIT_DEBUG > 0
    static BOOLEAN  FirstTimeLog = TRUE;
    BOOLEAN         CheckMute   = FALSE;
    BOOLEAN         ForceNative = FALSE;
    CHAR16         *MsgStr;
    CHAR16         *LogStatus;
    CHAR16         *LogNameTmp;
    CHAR16         *LogNameFull;

    MY_MUTELOGGER_SET;
    #endif

    BlockCert = FALSE;
    HardNVRAM = ((Attributes & EFI_VARIABLE_NON_VOLATILE) == EFI_VARIABLE_NON_VOLATILE);
    if (!BlockUEFI && !CurPolicyOEM && !RevokeVar) {
        BlockCert = (
            AppleFirmware && HardNVRAM &&
            (
                (
                    GuidsAreEqual (VendorGuid, &gEfiGlobalVariableGuid) &&
                    (
                        MyStriCmp (VariableName, L"PK" ) || /* EFI_PLATFORM_KEY_NAME     */
                        MyStriCmp (VariableName, L"KEK")    /* EFI_KEY_EXCHANGE_KEY_NAME */
                    )
                ) || (
                    GuidsAreEqual (VendorGuid, &gEfiImageSecurityDatabaseGuid) &&
                    (
                        MyStriCmp (VariableName, L"db" ) || /* EFI_IMAGE_SECURITY_DATABASE0 */
                        MyStriCmp (VariableName, L"dbx") || /* EFI_IMAGE_SECURITY_DATABASE1 */
                        MyStriCmp (VariableName, L"dbt") || /* EFI_IMAGE_SECURITY_DATABASE2 */
                        MyStriCmp (VariableName, L"dbr")    /* EFI_IMAGE_SECURITY_DATABASE3 */
                    )
                )
            )
        );
    }

    if (BlockUEFI || BlockCert) {
        BlockSize = FALSE;
    }
    else {
        BlockSize = (
            AppleFirmware && HardNVRAM &&
            VariableSize > GlobalConfig.NvramVariableLimit &&
            GlobalConfig.NvramVariableLimit > NVRAM_SIZE_THRESHOLD
        );
    }

    Status = (BlockUEFI || BlockCert || BlockSize)
    ? EFI_SUCCESS
    : SetHardwareNvramVariable (
        VariableName, VendorGuid,
        Attributes, VariableSize, VariableData
    );

    #if REFIT_DEBUG > 0
    LogStatus = NULL;
    if (!gKernelStarted) {
        // Log Outcome
        LogStatus = PoolPrint (
            L"%r",
            (BlockUEFI || BlockCert || BlockSize)
                ? EFI_ACCESS_DENIED : Status
        );
        LimitStringLength (LogStatus, 18);
    }

    MY_MUTELOGGER_OFF;
    /* Enable Forced Native Logging */
    MY_NATIVELOGGER_SET;

    if (!gKernelStarted) {
        // Do not free LogNameTmp
        LogNameTmp = NULL;
        if (GuidsAreEqual (VendorGuid, &gEfiImageSecurityDatabaseGuid)) {
            if (0);
            else if (MyStriCmp (VariableName, L"db") ) LogNameTmp = L"EFI_IMAGE_SECURITY_DATABASE0";
            else if (MyStriCmp (VariableName, L"dbx")) LogNameTmp = L"EFI_IMAGE_SECURITY_DATABASE1";
            else if (MyStriCmp (VariableName, L"dbt")) LogNameTmp = L"EFI_IMAGE_SECURITY_DATABASE2";
            else if (MyStriCmp (VariableName, L"dbr")) LogNameTmp = L"EFI_IMAGE_SECURITY_DATABASE3";
            else if (MyStriCmp (VariableName, L"KEK")) LogNameTmp = L"EFI_KEY_EXCHANGE_KEY_NAME"   ;
            else if (MyStriCmp (VariableName, L"PK") ) LogNameTmp = L"EFI_PLATFORM_KEY_NAME"       ;
        }

        LogNameFull = StrDuplicate (
            (LogNameTmp) ? LogNameTmp : VariableName
        );
        LimitStringLength (LogNameFull, 32);

        MsgStr = PoolPrint (
            L"In Hardware NVRAM ... %18s %s:- %s  :::  %-32s  ***  Size: %5d byte%s%s",
            LogStatus,
            NVRAM_LOG_SET,
            (BlockUEFI)
                ? L"WindowsCert"
                : (BlockCert)
                    ? L"GeneralCert"
                    : (BlockSize)
                        ? L"OversizeItem"
                        : (CurPolicyOEM)
                            ? L"MyPolicyOEM"
                            : (HardNVRAM)
                                ? L"RegularItem"
                                : L"NotFiltered",
            LogNameFull,
            VariableSize,
            (VariableSize == 1)
                ? L" "
                : (RevokeVar)
                    ? L"s  ---  Invalidated"
                    : L"s",
            (HardNVRAM)
                ? L"  ***  NonVolatile"
                : L""
        );

        if (!FirstTimeLog) {
            LOG_MSG("\n");
        }
        LOG_MSG("%s", MsgStr);
        FirstTimeLog = FALSE;

        MY_FREE_POOL(MsgStr);
        MY_FREE_POOL(LogStatus);
        MY_FREE_POOL(LogNameFull);
    }

    /* Disable Forced Native Logging */
    MY_NATIVELOGGER_OFF;
    #endif

    // DA-TAG: Do not remove Current OEM Policy
    //         NvramProtect is currently limited to Apple Firmware, so not strictly needed
    //         However, best to add filter now in case that changes in future
    //
    // Clear any previously saved instances of blocked variable
    if (!CurPolicyOEM && (BlockUEFI || BlockCert || BlockSize)) {
        OrigSetVariableRT (
            VariableName, VendorGuid,
            Attributes, 0, NULL
        );
    }

    if (Status == EFI_ALREADY_STARTED) {
        // Return 'Success' if Already Started
        Status = EFI_SUCCESS;
    }

    return Status;
} // EFI_STATUS EFIAPI gRTSetVariableEx()

static
VOID FlagKernelActive (VOID ) {
    // DA-TAG: Flag that the kernel has started
    //         To disable BootServices access
    gKernelStarted = TRUE;
} // static VOID FlagKernelActive()

static
VOID EFIAPI HandleExitBootServicesEvent (
    IN EFI_EVENT   Event,
    IN VOID       *Context
) {
    FlagKernelActive();
} // static VOID EFIAPI HandleExitBootServicesEvent()

static
VOID EFIAPI HandleVirtualAddressChangeEvent (
    IN EFI_EVENT   Event,
    IN VOID       *Context
) {
    gRT->ConvertPointer (EFI_OPTIONAL_PTR, (VOID **) &OrigSetVariableRT);
    FlagKernelActive();
} // static VOID EFIAPI HandleExitBootServicesEvent()

static
VOID SetProtectNvram (
    IN EFI_SYSTEM_TABLE *SystemTable,
    IN BOOLEAN           Activate
) {
    EFI_STATUS        Status;
    static BOOLEAN    ProtectActive         = FALSE;
    static EFI_EVENT  OurBootServicesEvent  =  NULL;
    static EFI_EVENT  OurAddressChangeEvent =  NULL;

    if (!GlobalConfig.NvramProtect) {
        // Early Return
        return;
    }

    if (Activate) {
        if (!ProtectActive) {
            ProtectActive    =             TRUE;
            gRT->SetVariable = gRTSetVariableEx;
            gRT->Hdr.CRC32   =                0;
            REFIT_CALL_3_WRAPPER(
                gBS->CalculateCrc32, gRT,
                gRT->Hdr.HeaderSize, &gRT->Hdr.CRC32
            );

            Status = (OurBootServicesEvent)
                ? REFIT_CALL_1_WRAPPER(gBS->CheckEvent, OurBootServicesEvent)
                : EFI_NOT_FOUND;
            if (EFI_ERROR(Status)) {
                REFIT_CALL_5_WRAPPER(
                    gBS->CreateEvent, EVT_SIGNAL_EXIT_BOOT_SERVICES,
                    TPL_CALLBACK, HandleExitBootServicesEvent,
                    NULL, &OurBootServicesEvent
                );
            }

            Status = (OurAddressChangeEvent)
                ? REFIT_CALL_1_WRAPPER(gBS->CheckEvent, OurAddressChangeEvent)
                : EFI_NOT_FOUND;
            if (EFI_ERROR(Status)) {
                REFIT_CALL_5_WRAPPER(
                    gBS->CreateEvent, EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE,
                    TPL_CALLBACK, HandleVirtualAddressChangeEvent,
                    NULL, &OurAddressChangeEvent
                );
            }
        }
    }
    else {
        if (ProtectActive) {
            ProtectActive    =             FALSE;
            gRT->SetVariable = OrigSetVariableRT;
            gRT->Hdr.CRC32   =                 0;
            REFIT_CALL_3_WRAPPER(
                gBS->CalculateCrc32, gRT,
                gRT->Hdr.HeaderSize, &gRT->Hdr.CRC32
            );

            Status = (OurBootServicesEvent)
                ? REFIT_CALL_1_WRAPPER(gBS->CheckEvent, OurBootServicesEvent)
                : EFI_NOT_FOUND;
            if (!EFI_ERROR(Status)) {
                REFIT_CALL_1_WRAPPER(gBS->CloseEvent, OurBootServicesEvent);
            }

            Status = (OurAddressChangeEvent)
                ? REFIT_CALL_1_WRAPPER(gBS->CheckEvent, OurAddressChangeEvent)
                : EFI_NOT_FOUND;
            if (!EFI_ERROR(Status)) {
                REFIT_CALL_1_WRAPPER(gBS->CloseEvent, OurAddressChangeEvent);
            }
        }
    }
} // static VOID SetProtectNvram()

static
EFI_STATUS FilterCSR (VOID) {
    EFI_STATUS Status;

    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;
    #endif

    Status = (GlobalConfig.NormaliseCSR) ? NormaliseCSR() : EFI_NOT_STARTED;
    #if REFIT_DEBUG > 0
    if (EFI_ERROR(Status)) {
        MsgStr = PoolPrint (
            L"Normalise CSR ... %r",
            Status
        );
        ALT_LOG(1, LOG_THREE_STAR_MID, L"%s", MsgStr);
        LOG_MSG("%s    * %s", OffsetNext, MsgStr);
        MY_FREE_POOL(MsgStr);
    }
    #endif

    return Status;
} // static EFI_STATUS FilterCSR()

static
VOID AlignCSR (VOID) {
    EFI_STATUS Status;
    UINT32     CsrStatus;
    BOOLEAN    CsrEnabled;
    BOOLEAN    RotateCsr;

    #if REFIT_DEBUG > 0
    CHAR16    *TmpStr;
    CHAR16    *MsgStr;
    BOOLEAN    HandleCsr;
    #endif

    if (!GlobalConfig.CsrValues) {
        // Early Exit ... CsrValues not configured
        return;
    }

    if (GlobalConfig.DynamicCSR == 0) {
        // Early Exit ... Configured not to align CSR
        return;
    }

    do {
        #if REFIT_DEBUG > 0
        HandleCsr = FALSE;
        #endif

        if (!HasMacOS) {
            #if REFIT_DEBUG > 0
            Status = EFI_NOT_STARTED;
            #endif

            // Break Early ... No MacOS Instance
            break;
        }

        if (GlobalConfig.DynamicCSR != -1 && GlobalConfig.DynamicCSR != 1) {
            #if REFIT_DEBUG > 0
            Status = EFI_INVALID_PARAMETER;
            #endif

            // Break Early ... Improperly configured
            break;
        }

        // Try to get current CSR status
        Status = GetCsrStatus (&CsrStatus);
        if (EFI_ERROR(Status)) {
            // Break Early ... Invalid CSR Status
            break;
        }

        #if REFIT_DEBUG > 0
        HandleCsr = TRUE;
        #endif

        // Record CSR status in the 'gCsrStatus' variable
        RecordgCsrStatus (CsrStatus, FALSE);

        // Check 'gCsrStatus' variable for 'Enabled' term
        CsrEnabled = (MyStrStr (gCsrStatus, L"Enabled")) ? TRUE : FALSE;

        RotateCsr = FALSE;
        if (GlobalConfig.DynamicCSR == -1) {
            // Configured to Always Disable SIP/SSV
            if (CsrEnabled) {
                // Disable SIP/SSV as currently enabled
                RotateCsr = TRUE;
            }
        }
        else {
            // Configured to Always Enable SIP/SSV
            if (!CsrEnabled) {
                // Enable SIP/SSV as currently disbled
                RotateCsr = TRUE;
            }
        }

        if (RotateCsr) {
            // Rotate SIP/SSV from current setting
            // Do not Unset DynamicCSR
            RotateCsrValue (FALSE);

            // Break Early
            break;
        }

        #if REFIT_DEBUG > 0
        // Set Status to 'Already Started' if we get here
        Status = EFI_ALREADY_STARTED;
        #endif
    } while (0); // This 'loop' only runs once

    #if REFIT_DEBUG > 0
    MsgStr = StrDuplicate (L"D Y N A M I C   C S R   A L I G N M E N T");
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    LOG_MSG("%s", MsgStr);
    MY_FREE_POOL(MsgStr);
    if (!HandleCsr) {
        TmpStr = L"WARN: Did Not Update";
    }
    else {
        TmpStr = (GlobalConfig.DynamicCSR == -1)
            ? L"INFO: Disable"
            : L"INFO: Enable";
    }
    MsgStr = PoolPrint (L"%s SIP/SSV ... %r", TmpStr, Status);
    ALT_LOG(1, LOG_THREE_STAR_END, L"%s", MsgStr);
    LOG_MSG("\n");
    LOG_MSG("%s", MsgStr);
    LOG_MSG("\n\n");
    MY_FREE_POOL(MsgStr);
    #endif
} // static VOID AlignCSR()

#if REFIT_DEBUG > 0
static
VOID LogDisableCheck (
    IN CHAR16     *TypStr,
    IN EFI_STATUS  Result
) {
    CHAR16 *MsgStr;

    MsgStr = PoolPrint (
        L"%s ... %r",
        TypStr, Result
    );
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("%s    * %s", OffsetNext, MsgStr);
    MY_FREE_POOL(MsgStr);
} // static VOID LogDisableCheck()
#endif

static
VOID SetBootArgs (VOID) {
    EFI_STATUS   Status;
    CHAR16      *NameNVRAM;
    CHAR16      *BootArg;
    CHAR8       *DataNVRAM;
    VOID        *VarData;

    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr;
    BOOLEAN  LogDisableAMFI;
    BOOLEAN  LogDisableCompatCheck;
    BOOLEAN  LogDisableNvramPanicLog;
    #endif

    if (!GlobalConfig.SetBootArgs || GlobalConfig.SetBootArgs[0] == L'\0') {
        #if REFIT_DEBUG > 0
        Status = EFI_INVALID_PARAMETER;

        MsgStr = PoolPrint (
            L"Reset Boot Args ... %r",
            Status
        );
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("%s    * %s", OffsetNext, MsgStr);
        MY_FREE_POOL(MsgStr);
        #endif

        // Early Return
        return;
    }

    #if REFIT_DEBUG > 0
    LogDisableAMFI          = FALSE;
    LogDisableCompatCheck   = FALSE;
    LogDisableNvramPanicLog = FALSE;
    #endif

    if (MyStrStr (GlobalConfig.SetBootArgs, L"nvram_paniclog")) {
        #if REFIT_DEBUG > 0
        if (GlobalConfig.DisableNvramPanicLog) {
            // Ensure Logging
            LogDisableNvramPanicLog = TRUE;
        }
        #endif

        // Do not duplicate 'nvram_paniclog=0'
        GlobalConfig.DisableNvramPanicLog = FALSE;
    }

    if (MyStrStr (GlobalConfig.SetBootArgs, L"amfi_get_out_of_my_way")) {
        #if REFIT_DEBUG > 0
        if (GlobalConfig.DisableAMFI) {
            // Ensure Logging
            LogDisableAMFI = TRUE;
        }
        #endif

        // Do not duplicate 'amfi_get_out_of_my_way=1'
        GlobalConfig.DisableAMFI = FALSE;
    }

    if (MyStrStr (GlobalConfig.SetBootArgs, L"-no_compat_check")) {
        #if REFIT_DEBUG > 0
        if (GlobalConfig.DisableCompatCheck) {
            // Ensure Logging
            LogDisableCompatCheck = TRUE;
        }
        #endif

        // Do not duplicate '-no_compat_check'
        GlobalConfig.DisableCompatCheck = FALSE;
    }

    if (GlobalConfig.DisableAMFI &&
        GlobalConfig.DisableCompatCheck &&
        GlobalConfig.DisableNvramPanicLog
    ) {
        // Combine Args with DisableNvramPanicLog and DisableAMFI and DisableCompatCheck
        BootArg = PoolPrint (
            L"%s nvram_paniclog=0 amfi_get_out_of_my_way=1 -no_compat_check",
            GlobalConfig.SetBootArgs
        );
    }
    else if (GlobalConfig.DisableAMFI &&
        GlobalConfig.DisableNvramPanicLog
    ) {
        // Combine Args with DisableNvramPanicLog and DisableAMFI
        BootArg = PoolPrint (
            L"%s nvram_paniclog=0 amfi_get_out_of_my_way=1",
            GlobalConfig.SetBootArgs
        );
    }
    else if (GlobalConfig.DisableCompatCheck &&
        GlobalConfig.DisableNvramPanicLog
    ) {
        // Combine Args with DisableNvramPanicLog and DisableCompatCheck
        BootArg = PoolPrint (
            L"%s nvram_paniclog=0 -no_compat_check",
            GlobalConfig.SetBootArgs
        );
    }
    else if (GlobalConfig.DisableAMFI &&
        GlobalConfig.DisableCompatCheck
    ) {
        // Combine Args with DisableAMFI and DisableCompatCheck
        BootArg = PoolPrint (
            L"%s amfi_get_out_of_my_way=1 -no_compat_check",
            GlobalConfig.SetBootArgs
        );
    }
    else if (GlobalConfig.DisableNvramPanicLog) {
        // Combine Args with DisableNvramPanicLog
        BootArg = PoolPrint (
            L"%s nvram_paniclog=0",
            GlobalConfig.SetBootArgs
        );
    }
    else if (GlobalConfig.DisableAMFI) {
        // Combine Args with DisableAMFI
        BootArg = PoolPrint (
            L"%s amfi_get_out_of_my_way=1",
            GlobalConfig.SetBootArgs
        );
    }
    else if (GlobalConfig.DisableCompatCheck) {
        // Combine Args with DisableCompatCheck
        BootArg = PoolPrint (
            L"%s -no_compat_check",
            GlobalConfig.SetBootArgs
        );
    }
    else {
        // Use Args Alone
        BootArg = StrDuplicate (GlobalConfig.SetBootArgs);
    }

    DataNVRAM = AllocatePool (
        (StrLen (BootArg) + 1) * sizeof (CHAR8)
    );
    if (!DataNVRAM) {
        MY_FREE_POOL(BootArg);

        #if REFIT_DEBUG > 0
        Status = EFI_OUT_OF_RESOURCES;

        MsgStr = PoolPrint (
            L"Reset Boot Args ... %r",
            Status
        );
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("%s    * %s", OffsetNext, MsgStr);
        MY_FREE_POOL(MsgStr);
        #endif

        // Early Return
        return;
    }

    VarData = NULL;
    NameNVRAM = L"boot-args";
    GetHardwareNvramVariable (
        NameNVRAM, &AppleVendorOsGuid,
        &VarData, NULL
    );

    if (VarData && MyStriCmp (BootArg, VarData)) {
        #if REFIT_DEBUG > 0
        Status = EFI_ALREADY_STARTED;

        MsgStr = PoolPrint (
            L"%r When Setting Boot Args:- '%s'",
            Status, BootArg
        );
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("%s    ** %s", OffsetNext, MsgStr);
        MY_FREE_POOL(MsgStr);
        #endif

        MY_FREE_POOL(VarData);
        MY_FREE_POOL(BootArg);

        // Early Return
        return;
    }

    // Convert Unicode String 'BootArg' to Ascii String 'DataNVRAM'
    UnicodeStrToAsciiStr (BootArg, DataNVRAM);

    Status = EfivarSetRaw (
        &AppleVendorOsGuid, NameNVRAM,
        DataNVRAM, AsciiStrSize (DataNVRAM), TRUE
    );

    #if REFIT_DEBUG > 0
    if (LogDisableAMFI || GlobalConfig.DisableAMFI) {
        LogDisableCheck (BOOT_FIX_STR_01, Status);
    }

    if (LogDisableCompatCheck || GlobalConfig.DisableCompatCheck) {
        LogDisableCheck (BOOT_FIX_STR_02, Status);
    }

    if (LogDisableNvramPanicLog || GlobalConfig.DisableNvramPanicLog) {
        LogDisableCheck (BOOT_FIX_STR_03, Status);
    }

    MsgStr = PoolPrint (
        L"%r When Setting Boot Args:- '%s'",
        Status, GlobalConfig.SetBootArgs
    );
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("%s    ** %s", OffsetNext, MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    MY_FREE_POOL(BootArg);
    MY_FREE_POOL(DataNVRAM);
} // static VOID SetBootArgs()

static
EFI_STATUS NoCheckCompat (VOID) {
    EFI_STATUS   Status;
    CHAR16      *NameNVRAM;
    CHAR16      *ArgData;
    CHAR16      *BootArg;
    CHAR8       *DataNVRAM;
    CHAR16      *CurArgs;
    VOID        *VarData;

    if (!GlobalConfig.DisableCompatCheck) {
        // Early Return ... Do Not Log
        return EFI_NOT_STARTED;
    }

    VarData = NULL;
    NameNVRAM = L"boot-args";
    GetHardwareNvramVariable (
        NameNVRAM, &AppleVendorOsGuid,
        &VarData, NULL
    );

    BootArg = NULL;
    Status  = EFI_SUCCESS;
    ArgData = L"-no_compat_check";
    if (!VarData) {
        BootArg = StrDuplicate (ArgData);
    }
    else {
        CurArgs = MyAsciiStrCopyToUnicode ((CHAR8 *) VarData, 0);
        if (FindSubStr (CurArgs, ArgData)) {
            Status = EFI_ALREADY_STARTED;
        }
        else {
            BootArg = PoolPrint (
                L"%s %s",
                CurArgs, ArgData
            );
        }
        MY_FREE_POOL(CurArgs);
    }

    if (!EFI_ERROR(Status)) {
        DataNVRAM = AllocatePool (
            (StrLen (BootArg) + 1) * sizeof (CHAR8)
        );
        if (!DataNVRAM) {
            Status = EFI_OUT_OF_RESOURCES;
        }

        if (!EFI_ERROR(Status)) {
            // Convert Unicode String 'BootArg' to Ascii String 'DataNVRAM'
            UnicodeStrToAsciiStr (BootArg, DataNVRAM);

            Status = EfivarSetRaw (
                &AppleVendorOsGuid, NameNVRAM,
                DataNVRAM, AsciiStrSize (DataNVRAM), TRUE
            );
        }
    }
    MY_FREE_POOL(BootArg);

    #if REFIT_DEBUG > 0
    LogDisableCheck (BOOT_FIX_STR_02, Status);
    #endif

    return Status;
} // static EFI_STATUS NoCheckCompat()

static
EFI_STATUS NoCheckAMFI (VOID) {
    EFI_STATUS   Status;
    CHAR16      *NameNVRAM;
    CHAR16      *ArgData;
    CHAR16      *BootArg;
    CHAR16      *CurArgs;
    CHAR8       *DataNVRAM;
    VOID        *VarData;

    if (!GlobalConfig.DisableAMFI) {
        // Early Return ... Do Not Log
        return EFI_NOT_STARTED;
    }

    VarData = NULL;
    NameNVRAM = L"boot-args";
    GetHardwareNvramVariable (
        NameNVRAM, &AppleVendorOsGuid,
        &VarData, NULL
    );

    BootArg = NULL;
    Status  = EFI_SUCCESS;
    ArgData = L"amfi_get_out_of_my_way";
    if (!VarData) {
        BootArg = PoolPrint (L"%s=1", ArgData);
    }
    else {
        CurArgs = MyAsciiStrCopyToUnicode ((CHAR8 *) VarData, 0);
        BootArg = PoolPrint (L"%s %s=1", CurArgs, ArgData);

        if (FindSubStr (CurArgs, ArgData)) {
            Status = EFI_ALREADY_STARTED;
        }
        MY_FREE_POOL(CurArgs);
    }

    if (!EFI_ERROR(Status)) {
        DataNVRAM = AllocatePool (
            (StrLen (BootArg) + 1) * sizeof (CHAR8)
        );
        if (!DataNVRAM) {
            Status = EFI_OUT_OF_RESOURCES;
        }

        if (!EFI_ERROR(Status)) {
            // Convert Unicode String 'BootArg' to Ascii String 'DataNVRAM'
            UnicodeStrToAsciiStr (BootArg, DataNVRAM);

            Status = EfivarSetRaw (
                &AppleVendorOsGuid, NameNVRAM,
                DataNVRAM, AsciiStrSize (DataNVRAM), TRUE
            );
        }
    }
    MY_FREE_POOL(BootArg);

    #if REFIT_DEBUG > 0
    LogDisableCheck (BOOT_FIX_STR_01, Status);
    #endif

    return Status;
} // static EFI_STATUS NoCheckAMFI()

static
EFI_STATUS NoNvramPanicLog (VOID) {
    EFI_STATUS   Status;
    CHAR16      *NameNVRAM;
    CHAR16      *ArgData;
    CHAR16      *BootArg;
    CHAR16      *CurArgs;
    CHAR8       *DataNVRAM;
    VOID        *VarData;

    if (!GlobalConfig.DisableNvramPanicLog) {
        // Early Return ... Do Not Log
        return EFI_NOT_STARTED;
    }

    VarData = NULL;
    NameNVRAM = L"boot-args";
    GetHardwareNvramVariable (
        NameNVRAM, &AppleVendorOsGuid,
        &VarData, NULL
    );

    BootArg = NULL;
    Status  = EFI_SUCCESS;
    ArgData = L"nvram_paniclog=0";
    if (!VarData) {
        BootArg = StrDuplicate (ArgData);
    }
    else {
        CurArgs = MyAsciiStrCopyToUnicode ((CHAR8 *) VarData, 0);
        if (FindSubStr (CurArgs, ArgData)) {
            Status = EFI_ALREADY_STARTED;
        }
        else {
            BootArg = PoolPrint (
                L"%s %s",
                CurArgs, ArgData
            );
        }
        MY_FREE_POOL(CurArgs);
    }

    if (!EFI_ERROR(Status)) {
        DataNVRAM = AllocatePool (
            (StrLen (BootArg) + 1) * sizeof (CHAR8)
        );
        if (!DataNVRAM) {
            Status = EFI_OUT_OF_RESOURCES;
        }

        if (!EFI_ERROR(Status)) {
            // Convert Unicode String 'BootArg' to Ascii String 'DataNVRAM'
            UnicodeStrToAsciiStr (BootArg, DataNVRAM);

            Status = EfivarSetRaw (
                &AppleVendorOsGuid, NameNVRAM,
                DataNVRAM, AsciiStrSize (DataNVRAM), TRUE
            );
        }
    }
    MY_FREE_POOL(BootArg);

    #if REFIT_DEBUG > 0
    LogDisableCheck (BOOT_FIX_STR_03, Status);
    #endif

    return Status;
} // static EFI_STATUS NoNvramPanicLog()

static
EFI_STATUS TrimCoerce (VOID) {
    EFI_STATUS Status;
    CHAR16    *NameNVRAM;
    CHAR8      DataNVRAM[1] = {0x01};

    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr;
    #endif

    if (!GlobalConfig.ForceTRIM) {
        // Early Return
        return EFI_NOT_STARTED;
    }

    NameNVRAM = L"EnableTRIM";
    Status = EfivarSetRaw (
        &AppleVendorOsGuid, NameNVRAM,
        DataNVRAM, sizeof (DataNVRAM), TRUE
    );

    #if REFIT_DEBUG > 0
    MsgStr = PoolPrint (
        L"Forcefully Enable TRIM ... %r",
        Status
    );
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("%s    * %s", OffsetNext, MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    return Status;
} // static EFI_STATUS TrimCoerce()

// Extended 'OpenProtocol'
// Ensures GOP Interface for Boot Loading Screen
static
EFI_STATUS EFIAPI OpenProtocolEx (
    IN   EFI_HANDLE    Handle,
    IN   EFI_GUID     *Protocol,
    OUT  VOID        **Interface OPTIONAL,
    IN   EFI_HANDLE    AgentHandle,
    IN   EFI_HANDLE    ControllerHandle,
    IN   UINT32        Attributes
) {
    EFI_STATUS   Status;
    UINTN        i;
    UINTN        HandleCount;
    EFI_HANDLE  *HandleBuffer;

    Status = REFIT_CALL_6_WRAPPER(
        OrigOpenProtocolBS, Handle,
        Protocol, Interface,
        AgentHandle, ControllerHandle, Attributes
    );
    if (Status == EFI_UNSUPPORTED) {
        // Early Return
        return EFI_UNSUPPORTED;
    }

    if (!GuidsAreEqual (&gEfiGraphicsOutputProtocolGuid, Protocol)) {
        // Early Return
        return Status;
    }

    if (GOPDraw != NULL) {
        *Interface = GOPDraw;

        // Early Return
        return EFI_SUCCESS;
    }

    HandleCount = 0;
    HandleBuffer = NULL;
    Status = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer, ByProtocol,
        &gEfiGraphicsOutputProtocolGuid, NULL,
        &HandleCount, &HandleBuffer
    );
    if (EFI_ERROR(Status)) {
        // Early Return
        return EFI_UNSUPPORTED;
    }

    for (i = 0; i < HandleCount; i++) {
        if (HandleBuffer[i] != gST->ConsoleOutHandle) {
            Status = REFIT_CALL_6_WRAPPER(
                OrigOpenProtocolBS, HandleBuffer[i],
                &gEfiGraphicsOutputProtocolGuid, *Interface,
                AgentHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
            );
            if (!EFI_ERROR(Status) && *Interface != NULL) {
                break;
            }
        }
    } // for

    if (EFI_ERROR(Status) || *Interface == NULL) {
        Status = EFI_UNSUPPORTED;
    }

    MY_FREE_POOL(HandleBuffer);

    return Status;
} // EFI_STATUS OpenProtocolEx()


// Extended 'HandleProtocol'
// Routes 'HandleProtocol' to 'OpenProtocol'
static
EFI_STATUS EFIAPI HandleProtocolEx (
    IN   EFI_HANDLE   Handle,
    IN   EFI_GUID    *Protocol,
    OUT  VOID       **Interface
) {
    EFI_STATUS Status;

    Status = REFIT_CALL_6_WRAPPER(
        gBS->OpenProtocol, Handle,
        Protocol, Interface,
        gImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );

    return Status;
} // EFI_STATUS HandleProtocolEx()

static
VOID ReMapOpenProtocol (VOID) {
    if (!GOPDraw) {
        // Early Return if GOP is Absent
        return;
    }

    if (AppleFirmware) {
        if (!DevicePresence) {
            // Early Return on Compact Macs
            return;
        }
        if (AppleFramebuffers == 0) {
            // Early Return on Macs without AppleFramebuffers
            return;
        }
    }

    // Amend EFI_BOOT_SERVICES.OpenProtocol
    gBS->OpenProtocol = OpenProtocolEx;
    gBS->Hdr.CRC32    = 0;
    REFIT_CALL_3_WRAPPER(
        gBS->CalculateCrc32, gBS,
        gBS->Hdr.HeaderSize, &gBS->Hdr.CRC32
    );
} // ReMapOpenProtocol()

static
VOID RunMacBootSupportFuncs (VOID) {
    // Set Mac boot args if configured to
    // Disables AMFI if DisableAMFI is active
    // Disables MacOS compatibility check if DisableCompatCheck is active
    // Disables MacOS kernel panic logging to NVRAM if DisableNvramPanicLog is active
    if (GlobalConfig.SetBootArgs && GlobalConfig.SetBootArgs[0] != L'\0') {
        SetBootArgs();
    }

    // Disable AMFI if configured to and not previously disabled
    NoCheckAMFI();

    // Disable MacOS compatibility check if configured to and not previously disabled
    NoCheckCompat();

    // Disable MacOS kernel panic logging to NVRAM if configured to and not previously disabled
    NoNvramPanicLog();

    // Filter out the 'APPLE_INTERNAL' CSR bit if required
    FilterCSR();

    // Enable TRIM on non-Apple SSDs if configured to
    TrimCoerce();

    // Re-Map OpenProtocol
    ReMapOpenProtocol();
} // static VOID RunMacBootSupportFuncs()

static
BOOLEAN ShowCleanNvramInfo (
    CHAR16 *ToolPurpose
) {
    INTN               DefaultEntry;
    UINTN              MenuExit, k;
    CHAR16            *FilePath;
    BOOLEAN            RetVal;
    MENU_STYLE_FUNC    Style;
    REFIT_MENU_ENTRY  *ChosenEntry;
    REFIT_MENU_SCREEN *CleanNvramInfoMenu;

    CleanNvramInfoMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (CleanNvramInfoMenu == NULL) {
        // Early Return
        return FALSE;
    }

    CleanNvramInfoMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_TOOL_NVRAMCLEAN);
    CleanNvramInfoMenu->Title      = StrDuplicate (L"Clean Nvram"             );
    CleanNvramInfoMenu->Hint1      = StrDuplicate (RETURN_MAIN_SCREEN_HINT    );
    CleanNvramInfoMenu->Hint2      = StrDuplicate (L""                        );

    AddMenuInfoLine (CleanNvramInfoMenu, PoolPrint (L"A Tool to %s", ToolPurpose),                     TRUE);
    AddMenuInfoLine (CleanNvramInfoMenu, L"",                                                         FALSE);
    AddMenuInfoLine (CleanNvramInfoMenu, L"The binary must be placed in one of the paths below",      FALSE);
    AddMenuInfoLine (CleanNvramInfoMenu, L" - The first file found in the order listed will be used", FALSE);
    AddMenuInfoLine (CleanNvramInfoMenu, L" - You will be returned to the main menu if not found",    FALSE);
    AddMenuInfoLine (CleanNvramInfoMenu, L"",                                                         FALSE);

    k = 0;
    while ((FilePath = FindCommaDelimited (NVRAMCLEAN_FILES, k++)) != NULL) {
        AddMenuInfoLine (CleanNvramInfoMenu, FilePath, TRUE);
    }

    AddMenuInfoLine (CleanNvramInfoMenu, L"", FALSE);

    REFIT_MENU_ENTRY *MenuEntryCleanNvram = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
    if (MenuEntryCleanNvram == NULL) {
        FreeMenuScreen (&CleanNvramInfoMenu);

        // Early Return
        return FALSE;
    }

    MenuEntryCleanNvram->Title = StrDuplicate (ToolPurpose);
    MenuEntryCleanNvram->Tag   = TAG_GENERIC;
    AddMenuEntry (CleanNvramInfoMenu, MenuEntryCleanNvram);

    RetVal = GetReturnMenuEntry (&CleanNvramInfoMenu);
    if (!RetVal) {
        FreeMenuScreen (&CleanNvramInfoMenu);

        // Early Return
        return FALSE;
    }

    DefaultEntry = 1;
    Style = GraphicsMenuStyle;
    ChosenEntry = NULL;
    MenuExit = RunGenericMenu (CleanNvramInfoMenu, Style, &DefaultEntry, &ChosenEntry);

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'ShowCleanNvramInfo'",
        MenuExit, MenuExitInfo (MenuExit), ChosenEntry->Title
    );
    LOG_MSG("Received User Input:");
    LOG_MSG("%s  - %s", OffsetNext, ChosenEntry->Title);
    #endif

    RetVal = TRUE;
    if (MenuExit != MENU_EXIT_ENTER || ChosenEntry->Tag != TAG_GENERIC) {
        RetVal = FALSE;
    }

    FreeMenuScreen (&CleanNvramInfoMenu);

    return RetVal;
} // static BOOLEAN ShowCleanNvramInfo()

static
VOID AboutRefindPlus (VOID) {
    EFI_STATUS         Status;
    UINT32             CsrStatus;
    CHAR16            *TmpStr;
    BOOLEAN            RetVal;
    REFIT_MENU_SCREEN *AboutMenu;

    AboutMenu = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    if (AboutMenu == NULL) {
        // Early Return
        return;
    }

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;

    ALT_LOG(1, LOG_LINE_THIN_SEP, L"Creating 'About RefindPlus' Screen");
    #endif

    AboutMenu->TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_ABOUT                 );
    AboutMenu->Title      = PoolPrint (L"About RefindPlus %s", REFINDPLUS_VERSION);
    AboutMenu->Hint1      = StrDuplicate (L"Press 'Enter' to Return to Main Menu");
    AboutMenu->Hint2      = StrDuplicate (L""                                    );

    AboutMenu->InfoLines  = (CHAR16 **) AllocateZeroPool (sizeof (CHAR16 *));
    if (AboutMenu->InfoLines == NULL) {
        FreeMenuScreen (&AboutMenu);

        // Early Return
        return;
    }

    AddMenuInfoLine (
        AboutMenu,
        #if defined(__MAKEWITH_TIANO)
            L"Built with TianoCore EDK II",
        #else
            L"Built with GNU-EFI",
        #endif
        FALSE
    );
    // More than ~65 causes empty info page on 800x600 display
    TmpStr = StrDuplicate (VendorInfo);
    LimitStringLength (TmpStr, (MAX_LINE_LENGTH - 16));
    AddMenuInfoLine (AboutMenu, PoolPrint (L"Firmware      : %s", TmpStr),   TRUE);
    AddMenuInfoLine (AboutMenu, PoolPrint (L"Platform      : %s", ArchType), TRUE);
    MY_FREE_POOL(TmpStr);
    AddMenuInfoLine (
        AboutMenu,
        PoolPrint (
            L"EFI Version   : %s %d.%02d%s",
            ((gST->Hdr.Revision >> 16U) > 1) ? L"UEFI" : L"EFI",
            gST->Hdr.Revision >> 16U,
            gST->Hdr.Revision & ((1 << 16) - 1),
            (WarnVersionEFI)
                ? L" (Spoofed by Others)"
                : (SetSysTab)
                    ? L" (Spoofed)"
                    : L""
        ),
        TRUE
    );
    AddMenuInfoLine (
        AboutMenu,
        PoolPrint (
            L"Secure Boot   : %s",
            secure_mode() ? L"Active" : L"Inactive"
        ),
        TRUE
    );
    if (!HasMacOS) {
        TmpStr = StrDuplicate (L"Not Available");
    }
    else {
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif
        if (GlobalConfig.DynamicCSR == 1 || GlobalConfig.DynamicCSR == -1) {
            Status = EFI_SUCCESS;
            MY_FREE_POOL(gCsrStatus);
            gCsrStatus = StrDuplicate (
                (GlobalConfig.DynamicCSR == 1)
                    ? L"Dynamic SIP/SSV Enable"
                    : L"Dynamic SIP/SSV Disable"
            );
        }
        else {
            Status = GetCsrStatus (&CsrStatus);
        }
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif

        TmpStr = (!EFI_ERROR(Status))
            ? StrDuplicate (gCsrStatus)
            : PoolPrint (L"%s ... %r", gCsrStatus, Status);
    }
    LimitStringLength (TmpStr, (MAX_LINE_LENGTH - 16));
    AddMenuInfoLine (AboutMenu, PoolPrint (L"CSR for Mac   : %s", TmpStr), TRUE);
    MY_FREE_POOL(TmpStr);
    TmpStr = egScreenDescription();
    LimitStringLength (TmpStr, (MAX_LINE_LENGTH - 16));
    AddMenuInfoLine (AboutMenu, PoolPrint(L"Screen Output : %s", TmpStr),  TRUE);
    MY_FREE_POOL(TmpStr);

    AddMenuInfoLine (AboutMenu, L"",                                                        FALSE);
    AddMenuInfoLine (AboutMenu, L"Copyright (c) 2020-2023 Dayo Akanji and Others",          FALSE);
    AddMenuInfoLine (AboutMenu, L"Portions Copyright (c) 2012-2023 Roderick W. Smith",      FALSE);
    AddMenuInfoLine (AboutMenu, L"Portions Copyright (c) 2006-2010 Christoph Pfisterer",    FALSE);
    AddMenuInfoLine (AboutMenu, L"Portions Copyright (c) The Intel Corporation and Others", FALSE);
    AddMenuInfoLine (AboutMenu, L"Distributed under the terms of the GNU GPLv3 license",    FALSE);

    RetVal = GetReturnMenuEntry (&AboutMenu);
    if (!RetVal) {
        FreeMenuScreen (&AboutMenu);

        // Early Return
        return;
    }

    RunMenu (AboutMenu, NULL);

    FreeMenuScreen (&AboutMenu);
} // static VOID AboutRefindPlus()

// Record the loader's name/description in the "PreviousBoot" UEFI variable
// if different from what is already stored there.
VOID StoreLoaderName (
    IN CHAR16 *Name
) {
    if (!Name) {
        // Early Return
        return;
    }

    // Clear current PreviousBoot if TransientBoot is active
    if (GlobalConfig.TransientBoot) {
        EfivarSetRaw (
            &RefindPlusGuid, L"PreviousBoot",
            NULL, 0, TRUE
        );

        return;
    }

    EfivarSetRaw (
        &RefindPlusGuid, L"PreviousBoot",
        Name, StrLen (Name) * 2 + 2, TRUE
    );
} // VOID StoreLoaderName()

// Rescan for boot loaders
VOID RescanAll (
    BOOLEAN Reconnect
) {
    BOOLEAN TempRescanDXE;

    #if REFIT_DEBUG > 0
    #if REFIT_DEBUG > 1
    UINTN   ThislogLevel;
    BOOLEAN TempLevelFlip;
    #endif

    BOOLEAN ForceNative = FALSE;

    CHAR16 *MsgStr = L"R E S C A N   A L L   I T E M S";
    ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
    LOG_MSG("I N F O :   %s", MsgStr);
    LOG_MSG("\n\n");

    /* Enable Forced Native Logging */
    MY_NATIVELOGGER_SET;
    #endif

    // Reset MainMenu
    InitMainMenu();

    // ConnectAllDriversToAllControllers() can cause system hangs with some
    // buggy filesystem drivers, so do it only if necessary.
    if (Reconnect) {
        // Always rescan for DXE drivers when connecting drivers here
        TempRescanDXE = GlobalConfig.RescanDXE;
        ForceRescanDXE = TRUE;
        ConnectAllDriversToAllControllers (FALSE);
        ForceRescanDXE = TempRescanDXE;

        #if REFIT_DEBUG > 1
        ThislogLevel  = GlobalConfig.LogLevel;
        TempLevelFlip = FALSE;
        if (ThislogLevel > MAXLOGLEVEL) {
            GlobalConfig.LogLevel = MAXLOGLEVEL;
            TempLevelFlip = TRUE;
        }
        #endif
        ScanVolumes();
        #if REFIT_DEBUG > 1
        if (TempLevelFlip) {
            GlobalConfig.LogLevel = ThislogLevel;
            TempLevelFlip = FALSE;
        }
        #endif
    }

    // Unset Icon Scaled Flag
    IconScaleSet = FALSE;

    // Read Config
    ReadConfig (GlobalConfig.ConfigFilename);

    // Fix Icon Scales
    FixIconScale();

    if (OverrideSB) {
        GlobalConfig.DirectBoot = FALSE;
        GlobalConfig.TextOnly = ForceTextOnly = TRUE;
        GlobalConfig.Timeout = MainMenu->TimeoutSeconds = 0;
    }

    SetVolumeIcons();
    ScanForBootloaders();

    #if REFIT_DEBUG > 0
    /* Disable Forced Native Logging */
    MY_NATIVELOGGER_OFF;
    #endif

    ScanForTools();
} // VOID RescanAll()

#ifdef __MAKEWITH_TIANO
// Minimal initialisation function
static
VOID InitializeLib (
    IN EFI_HANDLE         ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
) {
    EFI_STATUS Status;

    gImageHandle = ImageHandle;
    gST          = SystemTable;
    gBS          = SystemTable->BootServices;
    gRT          = SystemTable->RuntimeServices;

    Status = EfiGetSystemConfigurationTable (
        &gEfiDxeServicesTableGuid,
        (VOID **) &gDS
    );
    if (EFI_ERROR(Status)) {
        // DA-TAG: Always check gDS before use
        gDS = 0;
    }

    // Upgrade EFI_BOOT_SERVICES.HandleProtocol
    gBS->HandleProtocol = HandleProtocolEx;
    gBS->Hdr.CRC32      = 0;
    REFIT_CALL_3_WRAPPER(
        gBS->CalculateCrc32, gBS,
        gBS->Hdr.HeaderSize, &gBS->Hdr.CRC32
    );
} // static VOID InitializeLib()
#endif

// Set up our own Secure Boot extensions.
// Returns TRUE on success, FALSE otherwise
static
BOOLEAN SecureBootSetup (VOID) {
    if (secure_mode() && ShimLoaded()) {
        if (security_policy_install() == EFI_SUCCESS) {
            return TRUE;
        }

        SecureBootFailure = TRUE;

        #if REFIT_DEBUG > 0
        LOG_MSG("** FATAL ERROR: Failed to Install MOK Secure Boot Extensions");
        LOG_MSG("\n\n");
        #endif
    }

    return FALSE;
} // VOID static SecureBootSetup()

// Remove our own Secure Boot extensions.
// Returns TRUE on success, FALSE otherwise
static
BOOLEAN SecureBootUninstall (VOID) {
    EFI_STATUS  Status;
    CHAR16     *MsgStr;
    BOOLEAN     Success;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    Success = TRUE;
    if (secure_mode()) {
        Status = security_policy_uninstall();
        if (EFI_ERROR(Status)) {
            Success = FALSE;
            BeginTextScreen (L"Secure Boot Policy Failure");

            MsgStr = L"Failed to Uninstall MOK Secure Boot Extensions ... Forcing Shutdown in 9 Seconds";

            #if REFIT_DEBUG > 0
            LOG_MSG("%s", MsgStr);
            OUT_TAG();

            #endif

            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_SET;
            #endif
            REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
            PrintUglyText (MsgStr, NEXTLINE);
            REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_OFF;
            #endif

            PauseSeconds (9);

            REFIT_CALL_4_WRAPPER(
                gRT->ResetSystem, EfiResetShutdown,
                EFI_SUCCESS, 0, NULL
            );
        }
    }

    return Success;
} // static BOOLEAN SecureBootUninstall()

// Sets the global configuration filename; will be CONFIG_FILE_NAME unless the
// "-c" command-line option is set, in which case that takes precedence.
// If an error is encountered, leaves the value alone (it should be set to
// CONFIG_FILE_NAME when GlobalConfig is initialized).
static
VOID SetConfigFilename (
    EFI_HANDLE ImageHandle
) {
    EFI_STATUS                  Status;
    CHAR16                     *Options;
    CHAR16                     *FileName;
    CHAR16                     *SubString;
    CHAR16                     *MsgStr;
    EFI_LOADED_IMAGE_PROTOCOL  *Info;

    #if REFIT_DEBUG > 0
    BOOLEAN CheckMute = FALSE;
    #endif

    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol, ImageHandle,
        &LoadedImageProtocol, (VOID **) &Info
    );
    if (!EFI_ERROR(Status) && Info->LoadOptionsSize > 0) {
        Options   = (CHAR16 *) Info->LoadOptions;
        SubString = MyStrStr (Options, L" -c ");
        if (SubString) {
            #if REFIT_DEBUG > 0
            LOG_MSG("Set Config Filename from Command Line Option:");
            LOG_MSG("\n");
            #endif

            FileName = StrDuplicate (&SubString[4]);

            if (FileName) {
                LimitStringLength (FileName, 256);
            }

            if (FileExists (SelfDir, FileName)) {
                GlobalConfig.ConfigFilename = FileName;

                #if REFIT_DEBUG > 0
                LOG_MSG("  - Config File:- '%s'", FileName);
                LOG_MSG("\n\n");
                #endif
            }
            else {
                MsgStr = StrDuplicate (L"** WARN: Specified Config File Not Found");
                #if REFIT_DEBUG > 0
                LOG_MSG("%s", MsgStr);
                LOG_MSG("\n");
                #endif

                #if REFIT_DEBUG > 0
                MY_MUTELOGGER_SET;
                #endif
                REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
                PrintUglyText (MsgStr, NEXTLINE);
                #if REFIT_DEBUG > 0
                MY_MUTELOGGER_OFF;
                #endif

                MY_FREE_POOL(MsgStr);
                MsgStr = StrDuplicate (L"         Try Default:- 'config.conf / refind.conf'");
                #if REFIT_DEBUG > 0
                LOG_MSG("%s", MsgStr);
                LOG_MSG("\n\n");
                #endif

                #if REFIT_DEBUG > 0
                MY_MUTELOGGER_SET;
                #endif
                PrintUglyText (MsgStr, NEXTLINE);
                REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
                #if REFIT_DEBUG > 0
                MY_MUTELOGGER_OFF;
                #endif

                PauseSeconds (9);
                MY_FREE_POOL(MsgStr);
            } // if/else FileExists (SelfDir, FileName

            MY_FREE_POOL(FileName);
        } // if SubString
    } // if !EFI_ERROR(Status) && Info->LoadOptionsSize
} // static VOID SetConfigFilename()

// Adjust the GlobalConfig.DefaultSelection variable: Replace all "+" elements with the
//  PreviousBoot variable, if it is available. If it is not available, delete that element.
static
VOID AdjustDefaultSelection (VOID) {
    EFI_STATUS  Status;
    UINTN       i;
    CHAR16     *Element;
    CHAR16     *NewCommaDelimited;
    CHAR16     *PreviousBoot;
    BOOLEAN     FormatLog;
    BOOLEAN     Ignore;

    #if REFIT_DEBUG > 0
    CHAR16     *MsgStr;
    BOOLEAN     LoggedOnce = FALSE;

    LOG_MSG("U P D A T E   D E F A U L T   S E L E C T I O N");
    #endif

    i = 0;
    PreviousBoot = NewCommaDelimited = NULL;
    while ((Element = FindCommaDelimited (
        GlobalConfig.DefaultSelection, i++
    )) != NULL) {
        Ignore = FormatLog = FALSE;
        if (MyStriCmp (Element, L"+")) {
            if (GlobalConfig.TransientBoot &&
                StrLen (GlobalConfig.DefaultSelection) > 1
            ) {
                Ignore = TRUE;
            }

            if (!Ignore) {
                FormatLog = TRUE;
                MY_FREE_POOL(Element);

                Status = EfivarGetRaw (
                    &RefindPlusGuid, L"PreviousBoot",
                    (VOID **) &PreviousBoot, NULL
                );
                if (!EFI_ERROR(Status)) {
                    Element = PreviousBoot;
                }
            }
        }

        if (!Ignore && Element && StrLen (Element)) {
            #if REFIT_DEBUG > 0
            if (!LoggedOnce) {
                LoggedOnce = TRUE;
                if (FormatLog) {
                    BRK_MIN("\n");
                    MsgStr = StrDuplicate (L"Changed to Previous Selection");
                }
                else {
                    LOG_MSG("\n");
                    MsgStr = StrDuplicate (L"Changed to Preferred Selection");
                }
                LOG_MSG("%s:- '%s'", MsgStr, Element);
            }
            #endif

            MergeStrings (&NewCommaDelimited, Element, L',');
        }

        MY_FREE_POOL(Element);
    } // while

    #if REFIT_DEBUG > 0
    if (!LoggedOnce) {
        BRK_MIN("\n");
        LOG_MSG("INFO: No Change to Default Selection");
    }
    BRK_MOD("\n\n");
    #endif

    MY_FREE_POOL(GlobalConfig.DefaultSelection);
    GlobalConfig.DefaultSelection = NewCommaDelimited;
} // static VOID AdjustDefaultSelection()


#if REFIT_DEBUG > 0
static
VOID LogRevisionInfo (
    EFI_TABLE_HEADER *Hdr,
    CHAR16           *Name,
    UINT16            CompileSize,
    BOOLEAN           DoEFICheck
) {
    static BOOLEAN FirstRun = TRUE;

    if (FirstRun) {
        LOG_MSG("\n\n");
    }
    else {
        LOG_MSG("\n");
    }
    FirstRun = FALSE;

    LOG_MSG(
        "%s:- '%-4s %d.%02d'",
        Name,
        DoEFICheck ? (((Hdr->Revision >> 16U) > 1) ? L"UEFI" : L"EFI") : L"Ver",
        Hdr->Revision >> 16U,
        Hdr->Revision & 0xffff
    );

    if (Hdr->HeaderSize == CompileSize) {
        LOG_MSG(" (HeaderSize %d)", Hdr->HeaderSize);
    }
    else {
        LOG_MSG(" (HeaderSize %d ... %d CompileSize)", Hdr->HeaderSize, CompileSize);
    }
} // static VOID LogRevisionInfo()
#endif

// Log basic information (RefindPlus version, EFI version, etc.) to the log file.
// Also sets some variables that may be needed later
static
VOID LogBasicInfo (VOID) {
    UINTN  EfiMajorVersion;

#if REFIT_DEBUG > 0
    EFI_STATUS  Status;
    CHAR16     *MsgStr;
    UINT64      MaximumVariableSize;
    UINT64      MaximumVariableStorageSize;
    UINT64      RemainingVariableStorageSize;
    EFI_GUID    ConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

    UINTN       CheckSize;
    BOOLEAN     TempBool;
    BOOLEAN     QVInfoSupport;

    LogRevisionInfo (&gST->Hdr, L"    System Table", sizeof (*gST),  TRUE);
    LogRevisionInfo (&gBS->Hdr, L"   Boot Services", sizeof (*gBS),  TRUE);
    LogRevisionInfo (&gRT->Hdr, L"Runtime Services", sizeof (*gRT),  TRUE);
    if (gDS) {
        LogRevisionInfo (&gDS->Hdr, L"    DXE Services", sizeof (*gDS), FALSE);
    }
    else {
        LOG_MSG("\n");
        LOG_MSG("    DXE Services:- 'Not Found' ... Some Functionality Will be Lost!!");
    }
    LOG_MSG("\n\n");
#endif


    // Get AppleFramebuffer Count
    AppleFramebuffers = egCountAppleFramebuffers();

    EfiMajorVersion = (gST->Hdr.Revision >> 16U);
    WarnVersionEFI  = WarnRevisionUEFI = FALSE;
    if (((gST->Hdr.Revision >> 16U) != EfiMajorVersion) ||
        ((gBS->Hdr.Revision >> 16U) != EfiMajorVersion) ||
        ((gRT->Hdr.Revision >> 16U) != EfiMajorVersion)
    ) {
        WarnVersionEFI = TRUE;
    }
    else if (
        ((gST->Hdr.Revision & 0xffff) != (gBS->Hdr.Revision & 0xffff)) ||
        ((gST->Hdr.Revision & 0xffff) != (gRT->Hdr.Revision & 0xffff)) ||
        ((gBS->Hdr.Revision & 0xffff) != (gRT->Hdr.Revision & 0xffff))
    ) {
        WarnRevisionUEFI = TRUE;
    }


/**
 * Function effectively ends here on RELEASE Builds
**/


#if REFIT_DEBUG > 0
    if (WarnVersionEFI || WarnRevisionUEFI) {
        LOG_MSG(
            "** WARN: Inconsistent %s Detected",
            (WarnVersionEFI)
                ? L"EFI Versions"
                : L"UEFI Revisions"
        );
        LOG_MSG("\n\n");
    }

    /* NVRAM Storage Info */
    QVInfoSupport = FALSE;
    LOG_MSG("Non-Volatile Storage:");
    if (((gST->Hdr.Revision >> 16U) > 1) &&
        ((gBS->Hdr.Revision >> 16U) > 1) &&
        ((gRT->Hdr.Revision >> 16U) > 1)
    ) {
        CheckSize = MY_OFFSET_OF(EFI_RUNTIME_SERVICES, QueryVariableInfo) + sizeof(gRT->QueryVariableInfo);
        if (gRT->Hdr.HeaderSize < CheckSize) {
            WarnMissingQVInfo = TRUE;

            LOG_MSG("\n\n");
            LOG_MSG("** WARN: Irregular UEFI 2.x Implementation Detected");
            LOG_MSG("%s         Program Behaviour is *NOT* Defined", OffsetNext);
        }
        else {
            // Flag as 'TRUE' even on error to skip the "Skipped QVI Call" message later
            QVInfoSupport = TRUE;
            Status = REFIT_CALL_4_WRAPPER(
                gRT->QueryVariableInfo, AccessFlagsBoot,
                &MaximumVariableStorageSize, &RemainingVariableStorageSize, &MaximumVariableSize
            );
            if (EFI_ERROR(Status)) {
                LOG_MSG("\n\n");
                LOG_MSG("** WARN: Could Not Retrieve Non-Volatile Storage Details");
            }
            else {
                if (AppleFirmware) {
                    // DA-TAG: Investigate This
                    //         Do not log output on Apple Firmware
                    //         Currently gives inaccurate output
                    LOG_MSG("%s*** Redacted Invalid Output ***", OffsetNext);
                }
                else {
                    LOG_MSG("%s  - Total Storage         : %ld", OffsetNext,   MaximumVariableStorageSize);
                    LOG_MSG("%s  - Remaining Available   : %ld", OffsetNext, RemainingVariableStorageSize);
                    LOG_MSG("%s  - Maximum Variable Size : %ld", OffsetNext,          MaximumVariableSize);
                }
            }
        }
    } // if gRT->Hdr.Revision

    if (!QVInfoSupport) {
        // QueryVariableInfo is not supported EFI 1.x Firmware
        LOG_MSG("%s*** Skipped QueryVariableInfo Call ***", OffsetNext);
    }
    LOG_MSG("\n\n");

    /**
     * Report which video output devices are natively available. We do not actually
     * use them, so just use MsgStr as a throwaway pointer to the protocol.
    **/
    LOG_MSG("ConsoleOut Modes:");

    Status = LibLocateProtocol (&ConsoleControlProtocolGuid, (VOID **) &MsgStr);
    LOG_MSG("%s  - Mode = Text           : %s", OffsetNext, EFI_ERROR(Status) ? L" NO" : L"YES");
    MY_FREE_POOL(MsgStr);

    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol, gST->ConsoleOutHandle,
        &gEfiUgaDrawProtocolGuid, (VOID **) &MsgStr
    );
    LOG_MSG("%s  - Mode = Graphics (UGA) : %s", OffsetNext, EFI_ERROR(Status) ? L" NO" : L"YES");
    MY_FREE_POOL(MsgStr);

    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol, gST->ConsoleOutHandle,
        &gEfiGraphicsOutputProtocolGuid, (VOID **) &MsgStr
    );
    LOG_MSG("%s  - Mode = Graphics (GOP) : %s", OffsetNext, EFI_ERROR(Status) ? L" NO" : L"YES");
    LOG_MSG("\n\n");
    MY_FREE_POOL(MsgStr);

    /* Shim */
    TempBool = ShimLoaded();
    LOG_MSG("Shim:- '%s'", (TempBool) ? L"Present" : L"Absent");
    LOG_MSG("\n");

    /* Secure Boot */
    MuteLogger = TRUE;
    TempBool = secure_mode();
    MuteLogger = FALSE;
    LOG_MSG("Secure Boot:- '%s'", (TempBool) ? L"Active"  : L"Inactive");
    LOG_MSG("\n");

    /* Apple Framebuffers */
    LOG_MSG("Apple Framebuffers:- '%d'", AppleFramebuffers);
    LOG_MSG("\n");

    /* Compat Support Module Type */
    switch (GlobalConfig.LegacyType) {
        case LEGACY_TYPE_MAC:  MsgStr = StrDuplicate (L"Mac-Style");  break;
        case LEGACY_TYPE_UEFI: MsgStr = StrDuplicate (L"UEFI-Style"); break;
        case LEGACY_TYPE_NONE: MsgStr = StrDuplicate (L"Absent");     break;
        default:               MsgStr = StrDuplicate (L"Unknown");    break; // Just in case
    }
    LOG_MSG("Compat Support Module:- '%s'", MsgStr);
    LOG_MSG("\n\n");
    MY_FREE_POOL(MsgStr);
#endif
} // static VOID LogBasicInfo()

VOID RefitStall (
    UINTN StallLoops
) {
    UINTN StallIndex;

    for (StallIndex = 0; StallIndex < StallLoops; ++StallIndex) {
        REFIT_CALL_1_WRAPPER(gBS->Stall, 9999);
    } // for
    // DA-TAG: Add Stall Difference
    REFIT_CALL_1_WRAPPER(gBS->Stall, (StallIndex + 1));
} // VOID RefitStall()

//
// main entry point
//
EFI_STATUS EFIAPI efi_main (
    EFI_HANDLE         ImageHandle,
    EFI_SYSTEM_TABLE  *SystemTable
) {
    EFI_STATUS         Status;
    EFI_TIME           Now;
    UINTN              i, k;
    UINTN              Trigger;
    INTN               MenuExit;
    CHAR16            *MsgStr;
    CHAR16            *PartMsg;
    CHAR16            *TypeStr;
    CHAR16            *FilePath;
    CHAR16            *SelectionName;
    CHAR16            *VarNVRAM;
    CHAR16             KeyAsString[2];
    BOOLEAN            FoundTool;
    BOOLEAN            RunOurTool;
    BOOLEAN            MokProtocol;
    BOOLEAN            MainLoopRunning;
    EG_PIXEL           BGColor = COLOR_LIGHTBLUE;
    LOADER_ENTRY      *ourLoaderEntry;
    LEGACY_ENTRY      *ourLegacyEntry;
    EFI_INPUT_KEY      key;
    REFIT_MENU_ENTRY  *ChosenEntry;


    #if REFIT_DEBUG > 0
    CHAR16            *SelfGUID;
    CHAR16            *CurDateStr;
    CHAR16            *DisplayName;
    BOOLEAN            ForceContinue;

    #if REFIT_DEBUG > 1
    UINTN              ThislogLevel;
    BOOLEAN            TempLevelFlip;
    #endif

    // Different Tpyes
    BOOLEAN    CheckMute     = FALSE;
    BOOLEAN    ForceNative   = FALSE;
    #endif

    /* Init Bootstrap */
    InitializeLib (ImageHandle, SystemTable);
    Status = InitRefitLib (ImageHandle);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    /* Stash SystemTable */
    OrigSetVariableRT  =  gRT->SetVariable;
    OrigOpenProtocolBS = gBS->OpenProtocol;

    /* Remove any recovery boot flags */
    ClearRecoveryBootFlag();

    /* Other Preambles */
    REFIT_CALL_2_WRAPPER(gRT->GetTime, &Now, NULL);
    NowYear   =   Now.Year;
    NowMonth  =  Now.Month;
    NowDay    =    Now.Day;
    NowHour   =   Now.Hour;
    NowMinute = Now.Minute;
    NowSecond = Now.Second;

    if (MyStrStr (gST->FirmwareVendor, L"Apple")) {
        AppleFirmware = TRUE;
        VendorInfo = StrDuplicate (L"Apple");
    }
    else {
        VendorInfo = PoolPrint (
            L"%s %d.%02d",
            gST->FirmwareVendor,
            gST->FirmwareRevision >> 16U,
            gST->FirmwareRevision & ((1 << 16U) - 1)
        );
    }

    /* Set Legacy Boot Type */
    FindLegacyBootType();

    /* Set Default Scan Pattern */
    (GlobalConfig.LegacyType == LEGACY_TYPE_MAC)
        ? CopyMem (GlobalConfig.ScanFor, "ihebocm    ", NUM_SCAN_OPTIONS)
        : CopyMem (GlobalConfig.ScanFor, "ieom       ", NUM_SCAN_OPTIONS);


    /* Init Logging */
    // DA-TAG: Also on RELEASE builds as we need the timer
    //         - Without logging output on RELEASE builds
    InitBooterLog();

    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_SET;
    #endif

    // Clear 'BootNext' if present
    // Just in case ... Should have been cleared by the firmware
    EfivarSetRaw (
        &GlobalGuid, L"BootNext",
        NULL, 0, TRUE
    );

    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;

    /* Enable Forced Native Logging */
    MY_NATIVELOGGER_SET;

    /* Start Logging */
    LOG_MSG("B E G I N   P R O G R A M   B O O T S T R A P");
    LOG_MSG("\n");
    LOG_MSG(
        "Loading RefindPlus %s on %s Firmware",
        REFINDPLUS_VERSION, VendorInfo
    );
    LOG_MSG("\n");
    #endif

    /* Architecture */
#if defined(EFIX64)
    ArchType = L"x86_64 (64 bit)";
#elif defined(EFI32)
    ArchType = L"x86_32 (32 bit)";
#elif defined(EFIAARCH64)
    ArchType = L"ARM_64 (64 bit)";
#else
    ArchType = L"Unknown Arch";
#endif

#if REFIT_DEBUG > 0
// Big REFIT_DEBUG - START
    LOG_MSG("Arch/Type:- '%s'", ArchType);
    LOG_MSG("\n");

    /* Build Engine */
    LOG_MSG("Made With:- ");
#if defined(__MAKEWITH_TIANO)
    LOG_MSG("'TianoCore EDK II'");
#elif defined(__MAKEWITH_GNUEFI)
    LOG_MSG("'GNU-EFI'");
#else
    LOG_MSG("Unknown DevKit");
#endif
    LOG_MSG("\n");

    /* TimeStamp */
    if (Now.TimeZone == EFI_UNSPECIFIED_TIMEZONE) {
        CurDateStr = PoolPrint (
            L"%d-%02d-%02d %02d:%02d:%02d (UTC)",
            NowYear, NowMonth,
            NowDay, NowHour,
            NowMinute, NowSecond
        );
    }
    else {
        CurDateStr = PoolPrint (
            L"%d-%02d-%02d %02d:%02d:%02d (UTC%s%02d:%02d)",
            NowYear, NowMonth,
            NowDay, NowHour,
            NowMinute, NowSecond,
            (Now.TimeZone > 0 ? L"-" : L"+"),
            ((ABS(Now.TimeZone)) / 60),
            ((ABS(Now.TimeZone)) % 60)
        );
    }
    LOG_MSG("Timestamp:- '%s'", CurDateStr);
    MY_FREE_POOL(CurDateStr);
// Big REFIT_DEBUG - END
#endif

    /* Log System Details */
    LogBasicInfo ();

    /* Read Configuration */
    SetConfigFilename (ImageHandle);

    /* Set Secure Boot Up */
    MokProtocol = SecureBootSetup();

    #if REFIT_DEBUG > 0
    Status = (MokProtocol) ? EFI_SUCCESS : EFI_NOT_STARTED;
    LOG_MSG("INFO: MOK Protocol ... %r", Status);
    LOG_MSG("\n\n");
    #endif

    // First scan volumes by calling ScanVolumes() to find "SelfVolume",
    //    SelfVolume is required by LoadDrivers() and ReadConfig();
    // A second call is needed later to enumerate volumes as well as
    //    to register new filesystem(s) accessed by drivers.
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_SET;
    #endif
    ScanVolumes();
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    if (!SelfVolSet) {
        MsgStr = StrDuplicate (L"Could Not Set Self Volume!!");
        ALT_LOG(1, LOG_STAR_HEAD_SEP, L"%s", MsgStr);
        LOG_MSG("** WARN: %s", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
    }
    else {
        StrSelfUUID = GuidAsString (&SelfVolume->VolUuid);
        SelfGUID = GuidAsString (&SelfVolume->PartGuid);
        LOG_MSG("INFO: Self Volume:- '%s  :::  %s  :::  %s'", SelfVolume->VolName, SelfGUID, StrSelfUUID);
        LOG_MSG("%s      Install Dir:- '%s'", OffsetNext, (SelfDirPath) ? SelfDirPath : L"Not Set");
        LOG_MSG("\n\n");
        MY_FREE_POOL(SelfGUID);
        MY_FREE_POOL(StrSelfUUID);
    }
    #endif

    /* Get/Set Config File ... Prefer RefindPlus Configuration File Naame */
    if (!FileExists (SelfDir, GlobalConfig.ConfigFilename)) {
        ConfigWarn = TRUE;

        #if REFIT_DEBUG > 0
        LOG_MSG("** WARN: Could not find RefindPlus configuration file:- 'config.conf'\n");
        LOG_MSG("         Trying rEFInd's configuration file instead:- 'refind.conf'\n"  );
        LOG_MSG("         Provide a 'config.conf' file to silence this warning\n"        );
        LOG_MSG("         You can rename 'refind.conf' as 'config.conf'\n"               );
        LOG_MSG("         NB: Will not contain all RefindPlus settings\n\n"              );
        #endif

        GlobalConfig.ConfigFilename = L"refind.conf";
    }

    /* Load config tokens */
    ReadConfig (GlobalConfig.ConfigFilename);

    /* Unlock partitions if required */
    #ifdef __MAKEWITH_TIANO
    // DA-TAG: Limit to TianoCore
    if (GlobalConfig.RansomDrives) {
        UninitRefitLib();
        OcUnblockUnmountedPartitions();
        ReinitRefitLib();
    }
    #endif

    /* Init MainMenu */
    InitMainMenu();

    /* Adjust Default Menu Selection */
    AdjustDefaultSelection();

    #if REFIT_DEBUG > 0
    #define TAG_ITEM_A(Item) OffsetNext,  Item
    #define TAG_ITEM_B(Item) OffsetNext, (Item) ? L"YES" : L"NO"
    #define TAG_ITEM_C(Item) OffsetNext, (Item) ? L"Active" : L"Inactive"

    LOG_MSG("L I S T   M I S C   S E T T I N G S");
    LOG_MSG("\n");
    LOG_MSG("INFO: RefitDBG:- '%d'",       REFIT_DEBUG                             );
    LOG_MSG("%s      LogLevel:- '%d'",     TAG_ITEM_A(GlobalConfig.LogLevel       ));
    LOG_MSG("%s      ScanDelay:- '%d'",    TAG_ITEM_A(GlobalConfig.ScanDelay      ));
    LOG_MSG("%s      PreferUGA:- '%s'",    TAG_ITEM_B(GlobalConfig.PreferUGA      ));
    LOG_MSG("%s      ReloadGOP:- '%s'",    TAG_ITEM_B(GlobalConfig.ReloadGOP      ));
    LOG_MSG("%s      CheckDXE:- '%s'",     TAG_ITEM_C(GlobalConfig.RescanDXE      ));
    LOG_MSG("%s      SyncAPFS:- '%s'",     TAG_ITEM_C(GlobalConfig.SyncAPFS       ));
    LOG_MSG("%s      HelpTags:- '%s'",     TAG_ITEM_C(GlobalConfig.HelpTags       ));
    LOG_MSG("%s      HelpIcon:- '%s'",     TAG_ITEM_C(GlobalConfig.HelpIcon       ));
    LOG_MSG("%s      AlignCSR:- ",         OffsetNext                              );
    if (GlobalConfig.DynamicCSR == 1) {
        LOG_MSG("'Active ... CSR Enable'"                                          );
    }
    else if (GlobalConfig.DynamicCSR == 0) {
        LOG_MSG("'Inactive'"                                                       );
    }
    else if (GlobalConfig.DynamicCSR == -1) {
        LOG_MSG("'Active ... CSR Disable'"                                         );
    }
    else {
        LOG_MSG("'Error ... Invalid'"                                              );
    }

    LOG_MSG("%s      TextOnly:- ",         OffsetNext                              );
    if (ForceTextOnly) {
        LOG_MSG("'Forced'"                                                         );
    }
    else {
        LOG_MSG("'%s'", GlobalConfig.TextOnly ? L"Active" : L"Inactive"            );
    }

    LOG_MSG("%s      DirectGOP:- '%s'",    TAG_ITEM_C(GlobalConfig.UseDirectGop   ));
    LOG_MSG("%s      DirectBoot:- '%s'",   TAG_ITEM_C(GlobalConfig.DirectBoot     ));
    LOG_MSG("%s      ScanAllESP:- '%s'",   TAG_ITEM_C(GlobalConfig.ScanAllESP     ));

    LOG_MSG(
        "%s      TextRenderer:- '%s'",
        OffsetNext,
        GlobalConfig.UseTextRenderer ? L"Active" : L"Inactive"
    );
    LOG_MSG("%s      ProtectNvram:- ",     OffsetNext                              );
    if (!AppleFirmware) {
        LOG_MSG("'Disabled'"                                                       );
    }
    else {
        LOG_MSG("'%s'", GlobalConfig.NvramProtect ? L"Active" : L"Inactive"        );
    }
    LOG_MSG(
        "%s      NormaliseCSR:- '%s'",
        OffsetNext,
        GlobalConfig.NormaliseCSR ? L"Active" : L"Inactive"
    );
    LOG_MSG("%s      SupplyAppleFB:- ",    OffsetNext                              );
    if (!AppleFirmware) {
        LOG_MSG("'Disabled'"                                                       );
    }
    else {
        LOG_MSG("'%s'", GlobalConfig.SupplyAppleFB ? L"Active" : L"Inactive"       );
    }
    LOG_MSG("%s      RansomDrives:- ",     OffsetNext                              );
    if (AppleFirmware) {
        LOG_MSG("'Disabled'"                                                       );
    }
    else {
        LOG_MSG("'%s'", GlobalConfig.RansomDrives ? L"Active" : L"Inactive"        );
    }
    LOG_MSG(
        "%s      TransientBoot:- '%s'",
        OffsetNext,
        GlobalConfig.TransientBoot ? L"Active" : L"Inactive"
    );
    LOG_MSG("%s      NvramProtectEx:- ",    OffsetNext                             );
    if (!AppleFirmware) {
        LOG_MSG("'Disabled'"                                                       );
    }
    else {
        LOG_MSG("'%s'", GlobalConfig.NvramProtectEx ? L"Active" : L"Inactive"      );
    }
    LOG_MSG("\n\n");

    // DA-TAG: Prime Status for SupplyUEFI
    //         Here to accomodate GNU-EFI
    Status = EFI_NOT_STARTED;
    #endif

    #ifdef __MAKEWITH_TIANO
    // DA-TAG: Limit to TianoCore
    if (GlobalConfig.SupplyUEFI) {
        Status = AmendSysTable();
    }
    #endif

    #if REFIT_DEBUG > 0
    LOG_MSG("C O M M E N C E   B A S E   S U P P O R T");
    LOG_MSG("\n");
    LOG_MSG("INFO: Supply Support:- 'UEFI  :  %r'", Status);

    // DA-TAG: Prime Status for SupplyNVME
    //         Here to accomodate GNU-EFI
    Status = EFI_NOT_STARTED;
    #endif

    #ifdef __MAKEWITH_TIANO
    // DA-TAG: Limit to TianoCore
    if (GlobalConfig.SupplyNVME) {
        Status = NvmExpressLoad (ImageHandle, SystemTable);
    }
    #endif

    #if REFIT_DEBUG > 0
    LOG_MSG("%s      Supply Support:- 'NVME  :  %r'", OffsetNext, Status);
    #endif

    // Load Drivers
    LoadDrivers();

    #if REFIT_DEBUG > 0
    // DA-TAG: Prime Status for SupplyAPFS
    //         Here to accomodate GNU-EFI
    Status = EFI_NOT_STARTED;
    #endif

    #ifdef __MAKEWITH_TIANO
    // DA-TAG: Limit to TianoCore
    if (GlobalConfig.SupplyAPFS) {
        Status = RP_ApfsConnectDevices();
    }
    #endif

    #if REFIT_DEBUG > 0
    LOG_MSG("\n\n");
    LOG_MSG("C O N C L U D E   B A S E   S U P P O R T");
    LOG_MSG("\n");
    LOG_MSG("INFO: Supply Support:- 'APFS  :  %r'", Status);
    #endif

    // Second call to ScanVolumes() to enumerate volumes and
    //   register any new filesystem(s) accessed by drivers.
    #if REFIT_DEBUG > 1
    ThislogLevel  = GlobalConfig.LogLevel;
    TempLevelFlip = FALSE;
    if (ThislogLevel > MAXLOGLEVEL) {
        GlobalConfig.LogLevel = MAXLOGLEVEL;
        TempLevelFlip = TRUE;
    }
    #endif
    ScanVolumes();
    #if REFIT_DEBUG > 1
    if (TempLevelFlip) {
        GlobalConfig.LogLevel = ThislogLevel;
        TempLevelFlip = FALSE;
    }
    #endif

    if (GlobalConfig.SpoofOSXVersion &&
        GlobalConfig.SpoofOSXVersion[0] != L'\0'
    ) {
        Status = SetAppleOSInfo();

        #if REFIT_DEBUG > 0
        LOG_MSG("INFO: Spoof MacOS Version ... %r", Status);
        LOG_MSG("\n\n");
        #endif
    }

    /* Validate DirectBoot */
    if (GlobalConfig.DirectBoot) {
        // Override ScreensaverTime
        if (GlobalConfig.ScreensaverTime != 0) {
            GlobalConfig.ScreensaverTime = 300;
        }

        if (SecureBootFailure || WarnMissingQVInfo) {
            // Override DirectBoot if a warning needs to be shown
            //   for ALL build targets
            OverrideSB                     = TRUE;
            GlobalConfig.ContinueOnWarning = TRUE;
        }

        #if REFIT_DEBUG > 0
        if (WarnMissingQVInfo || ConfigWarn) {
            // Override DirectBoot if a warning needs to be shown
            //   for DBG and NPT build targets
            OverrideSB                     = TRUE;
            GlobalConfig.ContinueOnWarning = TRUE;
        }

        LOG_MSG("C O N F I R M   D I R E C T   B O O T");
        if (OverrideSB) {
            LOG_MSG("\n");
            LOG_MSG("Load Error or Warning Present");
        }
        #endif

        if (!OverrideSB) {
            Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
            if (!EFI_ERROR(Status)) {
                KeyAsString[0] = key.UnicodeChar;
                KeyAsString[1] = 0;

                if (key.ScanCode == SCAN_ESC || key.UnicodeChar == CHAR_BACKSPACE) {
                    OverrideSB = TRUE;
                    MainMenu->TimeoutSeconds = 0;
                }
            }

            #if REFIT_DEBUG > 0
            LOG_MSG("\n");
            LOG_MSG("Read Buffered Keystrokes ... %r", Status);
            #endif
        }

        // DA-TAG: Do not merge with block above
        if (OverrideSB) {
            BlockRescan = TRUE;
            GlobalConfig.Timeout = 0;
            GlobalConfig.DirectBoot = FALSE;

            #if REFIT_DEBUG > 0
            MsgStr = StrDuplicate (L"Overriding");
            #endif
        }
        else {
            #if REFIT_DEBUG > 0
            MsgStr = StrDuplicate (L"Maintaining");
            #endif

            GlobalConfig.TextOnly = ForceTextOnly = TRUE;
        }

        #if REFIT_DEBUG > 0
        LOG_MSG("%s - %s DirectBoot", OffsetNext, MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif
    } // if GlobalConfig.DirectBoot

    #if REFIT_DEBUG > 0
    MsgStr = StrDuplicate (L"I N I T I A L I S E   M O N I T O R   D I S P L A Y");
    ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
    LOG_MSG("%s", MsgStr);
    LOG_MSG("\n");
    MY_FREE_POOL(MsgStr);
    #endif

    InitScreen();

    // Disable the EFI watchdog timer
    REFIT_CALL_4_WRAPPER(
        gBS->SetWatchdogTimer, 0x0000,
        0x0000, 0x0000, NULL
    );

    // Further bootstrap (now with config available)
    SetupScreen();

    WarnIfLegacyProblems();

    #if REFIT_DEBUG > 0
    /* Disable Forced Native Logging */
    MY_NATIVELOGGER_OFF;
    #endif

    // Show Secure Boot Failure Notice and Shut Down
    if (SecureBootFailure) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"Secure Boot Failure");
        ALT_LOG(1, LOG_LINE_SEPARATOR, L"Display %s Warning", MsgStr);
        LOG_MSG("INFO: User Warning:- '%s ... Forcing Shutdown'", MsgStr);
        LOG_MSG("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif

        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_SET;
        #endif
        SwitchToText (FALSE);
        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
        PrintUglyText (L"                                                          ", NEXTLINE);
        PrintUglyText (L"                                                          ", NEXTLINE);
        PrintUglyText (L"              Failure Setting Secure Boot Up              ", NEXTLINE);
        PrintUglyText (L"              Forcing Shutdown in 9 Seconds!              ", NEXTLINE);
        PrintUglyText (L"                                                          ", NEXTLINE);
        PrintUglyText (L"                                                          ", NEXTLINE);
        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
        PauseSeconds (9);
        #if REFIT_DEBUG > 0
        MY_MUTELOGGER_OFF;
        #endif

        REFIT_CALL_4_WRAPPER(
            gRT->ResetSystem, EfiResetShutdown,
            EFI_SUCCESS, 0, NULL
        );
    }

    // Apply Scan Delay if set
    if (GlobalConfig.ScanDelay > 0) {
        MsgStr = StrDuplicate (L"Paused for Scan Delay");

        Trigger = 3;
        if (GlobalConfig.ScanDelay > Trigger) {
            PartMsg = PoolPrint (L"%s ... Please Wait", MsgStr);
            egDisplayMessage (
                PartMsg, &BGColor, CENTER,
                0, NULL
            );

            MY_FREE_POOL(PartMsg);
        }

        #if REFIT_DEBUG > 0
        ALT_LOG(1, LOG_LINE_THIN_SEP, L"Scan Delay");
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("%s:", MsgStr);
        LOG_MSG("\n");
        #endif

        MY_FREE_POOL(MsgStr);

        for (i = 0; i < GlobalConfig.ScanDelay; ++i) {
            #if REFIT_DEBUG > 0
            ALT_LOG(1, LOG_THREE_STAR_MID, L"Loading Paused for 1 Second");
            #endif

            // Wait 1 second
            // DA-TAG: 100 Loops = 1 Sec
            RefitStall (100);
        }

        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (L"Resuming After a %d Second Pause", i);
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        LOG_MSG("  - %s", MsgStr);
        LOG_MSG("\n");
        MY_FREE_POOL(MsgStr);
        #endif

        if (GlobalConfig.ScanDelay > Trigger) {
            BltClearScreen (TRUE);
        }
    }

    // Continue Bootstrap
    SetVolumeIcons();
    ScanForBootloaders();
    ScanForTools();

    if (GlobalConfig.ShutdownAfterTimeout) {
        MainMenu->TimeoutText = StrDuplicate (L"Shutdown");
    }

    // Show Inconsistent UEFI 2.x Implementation Warning
    #if REFIT_DEBUG > 0
    // Prime ForceContinue
    ForceContinue = FALSE;

    if (WarnMissingQVInfo) {
        SwitchToText (FALSE);

        LOG_MSG("D I S P L A Y   U S E R   N O T I C E");
        MsgStr = StrDuplicate (L"Inconsistent UEFI 2.x Implementation");
        ALT_LOG(1, LOG_LINE_SEPARATOR, L"Display %s Warning", MsgStr);
        LOG_MSG("INFO: User Warning:- '%s'", MsgStr);
        MY_FREE_POOL(MsgStr);

        ForceContinue = (GlobalConfig.ContinueOnWarning) ? TRUE : FALSE;
        GlobalConfig.ContinueOnWarning = TRUE;
        MY_MUTELOGGER_SET;
        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
        PrintUglyText (L"                                                          ", NEXTLINE);
        PrintUglyText (L"                                                          ", NEXTLINE);
        PrintUglyText (L"           Inconsistent UEFI 2.x Implementation           ", NEXTLINE);
        PrintUglyText (L"                                                          ", NEXTLINE);

        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
        PrintUglyText (L"                                                          ", NEXTLINE);
        PrintUglyText (L"             Program Behaviour is Undefined!!             ", NEXTLINE);
        PrintUglyText (L"                                                          ", NEXTLINE);
        PrintUglyText (L"                                                          ", NEXTLINE);
        PauseForKey();
        MY_MUTELOGGER_OFF;
        GlobalConfig.ContinueOnWarning = ForceContinue;
        ForceContinue = FALSE;

        MsgStr = StrDuplicate (L"Warning Acknowledged or Timed Out");
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        LOG_MSG("%s      %s ...", OffsetNext, MsgStr);
        MY_FREE_POOL(MsgStr);

        if (AllowGraphicsMode) {
            LOG_MSG("Restore Graphics Mode");

            SwitchToGraphicsAndClear (TRUE);
        }
        else {
            LOG_MSG("Proceeding");

            BltClearScreen (FALSE);
        }

        LOG_MSG("\n\n");

        // Wait 1 second
        // DA-TAG: 100 Loops = 1 Sec
        RefitStall (100);
    } // if WarnMissingQVInfo
    #endif

    // Show Config Mismatch Warning
    // DA-TAG: Separate REFIT_DEBUG sections for flexibility
    #if REFIT_DEBUG > 0
    if (ConfigWarn) {
        SwitchToText (FALSE);

        LOG_MSG("D I S P L A Y   U S E R   N O T I C E");
        MsgStr = StrDuplicate (L"Missing Config File");
        ALT_LOG(1, LOG_LINE_SEPARATOR, L"Display %s Warning", MsgStr);
        LOG_MSG("INFO: User Warning:- '%s'", MsgStr);
        MY_FREE_POOL(MsgStr);

        ForceContinue = (GlobalConfig.ContinueOnWarning) ? TRUE : FALSE;
        GlobalConfig.ContinueOnWarning = TRUE;
        MY_MUTELOGGER_SET;
        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
        PrintUglyText (L"                                                          ", NEXTLINE);
        PrintUglyText (L"                                                          ", NEXTLINE);
        PrintUglyText (L"      Could not find a RefindPlus 'config.conf' file      ", NEXTLINE);
        PrintUglyText (L"    Trying to load a rEFInd 'refind.conf' file instead    ", NEXTLINE);
        PrintUglyText (L"                                                          ", NEXTLINE);

        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
        PrintUglyText (L"                                                          ", NEXTLINE);
        PrintUglyText (L"    Provide a config.conf file to silence this warning    ", NEXTLINE);
        PrintUglyText (L"     You can rename a refind.conf file as config.conf     ", NEXTLINE);
        PrintUglyText (L" * Will not contain all RefindPlus configuration tokens * ", NEXTLINE);
        PrintUglyText (L"                                                          ", NEXTLINE);
        PrintUglyText (L"                                                          ", NEXTLINE);
        PauseForKey();
        MY_MUTELOGGER_OFF;
        GlobalConfig.ContinueOnWarning = ForceContinue;
        ForceContinue = FALSE;

        MsgStr = StrDuplicate (L"Warning Acknowledged or Timed Out");
        ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
        LOG_MSG("%s      %s ...", OffsetNext, MsgStr);
        MY_FREE_POOL(MsgStr);

        if (AllowGraphicsMode) {
            LOG_MSG("Restore Graphics Mode");
            LOG_MSG("\n\n");

            SwitchToGraphicsAndClear (TRUE);
        }
        else {
            LOG_MSG("Proceeding");
            LOG_MSG("\n\n");
        }

        // Wait 0.25 second
        // DA-TAG: 100 Loops = 1 Sec
        RefitStall (25);
    } // if ConfigWarn
    #endif

    // Set CSR if required
    AlignCSR();

    // Init Pointers
    pdInitialize();

    SelectionName = (GlobalConfig.DefaultSelection)
        ? StrDuplicate (GlobalConfig.DefaultSelection) : NULL;

    TypeStr         =  NULL;
    FilePath        =  NULL;
    FoundTool       = FALSE;
    RunOurTool      = FALSE;
    ChosenEntry     =  NULL;
    MainLoopRunning =  TRUE;

    while (MainLoopRunning) {
        // Reset Misc
        IsBoot         = FALSE;
        FoundTool      = FALSE;
        RunOurTool     = FALSE;
        SubScreenBoot  = FALSE;
        MY_FREE_POOL(FilePath);

        MenuExit = RunMainMenu (MainMenu, &SelectionName, &ChosenEntry);

        // The ESC key triggers a rescan ... if allowed
        if (MenuExit == MENU_EXIT_ESCAPE) {
            // Flag at least one loop done
            OneMainLoop = TRUE;

            #if REFIT_DEBUG > 0
            LOG_MSG("Received User Input:");
            LOG_MSG("%s  - Escape Key Pressed ... Rescan All", OffsetNext);
            LOG_MSG("\n\n");
            #endif

            if (GlobalConfig.DirectBoot) {
                OverrideSB = TRUE;
            }
            RescanAll (TRUE);

            continue;
        }
        BlockRescan = FALSE;

        // Ignore MenuExit if FlushFailedTag is set and not previously reset
        if (FlushFailedTag && !FlushFailReset) {
            // Flag at least one loop done
            OneMainLoop = TRUE;

            #if REFIT_DEBUG > 0
            MsgStr = StrDuplicate (L"FlushFailedTag is Set ... Ignore MenuExit");
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
            LOG_MSG("INFO: %s", MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
            #endif

            FlushFailedTag = FALSE;
            FlushFailReset = TRUE;

            continue;
        }

        if (!OneMainLoop &&
            ChosenEntry->Tag == TAG_REBOOT
        ) {
            // Flag at least one loop done
            OneMainLoop = TRUE;

            #if REFIT_DEBUG > 0
            LOG_MSG("INFO: Invalid Post-Load Reboot Call ... Ignoring Reboot Call");
            MsgStr = StrDuplicate (L"Mitigated Potential Persistent Primed Keystroke Buffer");
            ALT_LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
            LOG_MSG("%s      %s", OffsetNext, MsgStr);
            LOG_MSG("\n\n");
            MY_FREE_POOL(MsgStr);
            #endif

            TypeStr = L"Aborted Invalid System Reset Call ... Please Try Again";

            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_SET;
            #endif
            egDisplayMessage (
                TypeStr, &BGColor, CENTER,
                4, L"PauseSeconds"
            );
            #if REFIT_DEBUG > 0
            MY_MUTELOGGER_OFF;
            #endif

            continue;
        }

        if (MenuExit == MENU_EXIT_TIMEOUT &&
            GlobalConfig.ShutdownAfterTimeout
        ) {
            ChosenEntry->Tag = TAG_SHUTDOWN;
        }

        if (GlobalConfig.NvramProtect) {
            // Stop NvramProtect
            SetProtectNvram (SystemTable, FALSE);
        }

        switch (ChosenEntry->Tag) {
            case TAG_INFO_NVRAMCLEAN:    // Clean NVRAM
                TypeStr = L"Clean/Reset NVRAM";

                #if REFIT_DEBUG > 0
                ALT_LOG(1, LOG_LINE_THIN_SEP, L"Creating '%s Info' Screen", TypeStr);

                LOG_MSG("Received User Input:");
                LOG_MSG("%s  - %s", OffsetNext, TypeStr);
                LOG_MSG("\n\n");
                #endif

                RunOurTool = ShowCleanNvramInfo (TypeStr);
                if (RunOurTool == FALSE) {
                    #if REFIT_DEBUG > 0
                    LOG_MSG("%s    ** Not Running Tool to %s", OffsetNext, TypeStr);
                    LOG_MSG("\n\n");
                    #endif

                    // Early Exit
                    break;
                }

                k = 0;
                while (!FoundTool && (FilePath = FindCommaDelimited (NVRAMCLEAN_FILES, k++)) != NULL) {
                    for (i = 0; i < VolumesCount; i++) {
                        if (Volumes[i]->RootDir != NULL &&
                            FileExists (Volumes[i]->RootDir, FilePath)
                        ) {
                            ourLoaderEntry = AllocateZeroPool (sizeof (LOADER_ENTRY));
                            ourLoaderEntry->me.Title        =  StrDuplicate (TypeStr);
                            ourLoaderEntry->me.Tag          =     TAG_LOAD_NVRAMCLEAN;
                            ourLoaderEntry->Volume          = CopyVolume (Volumes[i]);
                            ourLoaderEntry->LoaderPath      = StrDuplicate (FilePath);
                            ourLoaderEntry->UseGraphicsMode =                   FALSE;

                            FoundTool = TRUE;

                            // Break out of 'for' loop
                            break;
                        }
                    } // for

                    if (!FoundTool) {
                        MY_FREE_POOL(FilePath);
                    }
                } // while

                if (!FoundTool) {
                    #if REFIT_DEBUG > 0
                    MsgStr = PoolPrint (L"Could Not Find Tool:- '%s'", TypeStr);
                    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
                    LOG_MSG("\n\n");
                    LOG_MSG("** WARN : %s ... Return to Main Menu", MsgStr);
                    LOG_MSG("\n\n");
                    MY_FREE_POOL(MsgStr);
                    #endif

                    MY_FREE_POOL(FilePath);

                    // Early Exit
                    break;
                }

                #if REFIT_DEBUG > 0
                LOG_MSG("%s  - Load Tool to %s", OffsetNext, TypeStr);
                LOG_MSG("\n\n");
                #endif

                MY_FREE_POOL(FilePath);

                // DA-TAG: Disable Misc Protected OpenCore NVRAM Variables
                //         Initially limited to Apple firmware
                if (AppleFirmware) {
                    SetHardwareNvramVariable ( // "scan-policy"
                        L"scan-policy", &OpenCoreVendorGuid,
                        AccessFlagsBoot, 0, NULL
                    );
                    SetHardwareNvramVariable ( // "boot-redirect" (RequestBootVarRouting)
                        L"boot-redirect", &OpenCoreVendorGuid,
                        AccessFlagsBoot, 0, NULL
                    );
                    SetHardwareNvramVariable ( // "boot-protect"
                        L"boot-protect", &OpenCoreVendorGuid,
                        AccessFlagsFull, 0, NULL
                    );
                }

                // No end dash line ... Handled in 'Reboot' below
                StartTool (ourLoaderEntry);

                #if REFIT_DEBUG > 0
                BOOLEAN CheckMute = FALSE;
                MY_MUTELOGGER_SET;
                #endif

                i = 0;
                VarNVRAM = NULL;
                Status = (gVarsDir) ? EFI_SUCCESS : FindVarsDir();
                while ((VarNVRAM = FindCommaDelimited (
                    RP_NVRAM_VARIABLES, i++
                )) != NULL) {
                    // Clear Emulated NVRAM
                    if (!EFI_ERROR(Status)) {
                        egSaveFile (
                            gVarsDir, VarNVRAM,
                            NULL, 0
                        );
                    }

                    // Clear Hardware NVRAM (RefindPlusGuid)
                    SetHardwareNvramVariable (
                        VarNVRAM, &RefindPlusGuid,
                        AccessFlagsFull, 0, NULL
                    );

                    // Clear Hardware NVRAM (RefindPlusOldGuid)
                    SetHardwareNvramVariable (
                        VarNVRAM, &RefindPlusOldGuid,
                        AccessFlagsFull, 0, NULL
                    );

                    MY_FREE_POOL(VarNVRAM);
                } // while

                // Force nvram garbage collection on Macs
                if (AppleFirmware) {
                    UINT8 ResetNVRam = 1;
                    REFIT_CALL_5_WRAPPER(
                        gRT->SetVariable, L"ResetNVRam",
                        &AppleVendorOsGuid, AccessFlagsFull,
                        sizeof (ResetNVRam), &ResetNVRam
                    );
                }

                #if REFIT_DEBUG > 0
                MY_MUTELOGGER_OFF;

                LOG_MSG("INFO: Cleaned NVRAM");
                LOG_MSG("\n\n");
                #endif

            // No Break
            case TAG_REBOOT:    // Reboot
                TypeStr = L"Running System Restart";

                // If 'RunOurTool' is TRUE, then we have dropped down from 'Clean Nvram' above
                // Do not show confirmation menu in such cases
                if (RunOurTool == FALSE) {
                    // Now set 'RunOurTool' for this tool
                    RunOurTool = ConfirmRestart();
                    if (RunOurTool == FALSE) {
                        #if REFIT_DEBUG > 0
                        MsgStr = PoolPrint (L"Aborted %s", TypeStr);
                        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
                        ALT_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);
                        ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
                        LOG_MSG("INFO: %s", MsgStr);
                        LOG_MSG("\n\n");
                        MY_FREE_POOL(MsgStr);
                        #endif

                        // Early Exit
                        break;
                    }
                }

                // If 'FoundTool' is TRUE, then we have dropped down from 'Clean Nvram' above
                // Only display 'TypeStr' message in such cases
                if (FoundTool == TRUE) {
                    #if REFIT_DEBUG > 0
                    MY_MUTELOGGER_SET;
                    #endif
                    egDisplayMessage (
                        TypeStr, &BGColor, CENTER,
                        3, L"PauseSeconds"
                    );
                    #if REFIT_DEBUG > 0
                    MY_MUTELOGGER_OFF;
                    #endif
                }

                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"R U N   S Y S T E M   R E S T A R T");
                ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
                ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
                ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
                LOG_MSG("%s", MsgStr);
                LOG_MSG("\n");
                MY_FREE_POOL(MsgStr);

                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", TypeStr);
                LOG_MSG("%s", TypeStr);
                OUT_TAG();
                #endif

                // Terminate Screen
                TerminateScreen();

                REFIT_CALL_4_WRAPPER(
                    gRT->ResetSystem, EfiResetCold,
                    EFI_SUCCESS, 0, NULL
                );

                // Just in case we get this far
                MainLoopRunning = FALSE;

                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"System Reset Failed");
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
                LOG_MSG("WARN: %s", MsgStr);
                MY_FREE_POOL(MsgStr);
                #endif

            break;
            case TAG_SHUTDOWN: // Shut Down
                TypeStr = L"Running System Shutdown";

                RunOurTool = ConfirmShutdown();
                if (RunOurTool == FALSE) {
                    #if REFIT_DEBUG > 0
                    MsgStr = PoolPrint (L"Aborted %s", TypeStr);
                    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
                    ALT_LOG(1, LOG_THREE_STAR_SEP, L"%s", MsgStr);
                    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
                    LOG_MSG("INFO: %s", MsgStr);
                    LOG_MSG("\n\n");
                    MY_FREE_POOL(MsgStr);
                    #endif

                    // Early Exit
                    break;
                }

                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"R U N   S Y S T E M   S H U T D O W N");
                ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
                ALT_LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
                ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
                LOG_MSG("%s", MsgStr);
                LOG_MSG("\n");
                MY_FREE_POOL(MsgStr);

                ALT_LOG(1, LOG_LINE_NORMAL, L"%s", TypeStr);
                LOG_MSG("%s", TypeStr);
                OUT_TAG();
                #endif

                // Terminate Screen
                TerminateScreen();

                REFIT_CALL_4_WRAPPER(
                    gRT->ResetSystem, EfiResetShutdown,
                    EFI_SUCCESS, 0, NULL
                );

                // Just in case we get this far
                MainLoopRunning = FALSE;

                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"System Shutdown Failed");
                ALT_LOG(1, LOG_LINE_NORMAL, L"%s!!", MsgStr);
                LOG_MSG("WARN: %s", MsgStr);
                MY_FREE_POOL(MsgStr);
                #endif

            break;
            case TAG_ABOUT:    // About RefindPlus
                #if REFIT_DEBUG > 0
                LOG_MSG("Received User Input:");
                LOG_MSG("%s  - Show 'About RefindPlus'", OffsetNext);
                LOG_MSG("\n\n");
                #endif

                // No end dash line ... Expected to return
                AboutRefindPlus();

                #if REFIT_DEBUG > 0
                LOG_MSG("Received User Input:");
                LOG_MSG("%s  - Exit 'About RefindPlus'", OffsetNext);
                LOG_MSG("\n\n");
                #endif

            break;
            case TAG_LOADER:   // Boot OS via *.efi File
                ourLoaderEntry = (LOADER_ENTRY *) ChosenEntry;

                // Fix undetected MacOS
                if (!FindSubStr (ourLoaderEntry->Title, L"MacOS")                          &&
                    !FindSubStr (ourLoaderEntry->Title, L"Mac OS")                         &&
                    !FindSubStr (ourLoaderEntry->Title, L"Install")                        &&
                    FindSubStr (ourLoaderEntry->LoaderPath, L"System\\Library\\CoreServices")
                ) {
                    ourLoaderEntry->Title = (FindSubStr (ourLoaderEntry->Volume->VolName, L"PreBoot"))
                        ? L"MacOS" : L"RefindPlus";
                }

                // Fix undetected Windows
                if (!FindSubStr (ourLoaderEntry->Title, L"Windows") &&
                    FindSubStr (ourLoaderEntry->LoaderPath, L"EFI\\Microsoft\\Boot")
                ) {
                    ourLoaderEntry->Title = L"Windows (UEFI)";
                }

                if (
                    (
                        FindSubStr (ourLoaderEntry->Title, L"OS X Install")
                    ) || (
                        FindSubStr (ourLoaderEntry->Title, L"MacOS Install")
                    ) || (
                        FindSubStr (ourLoaderEntry->Title, L"Mac OS Install")
                    ) || (
                        FindSubStr (ourLoaderEntry->Title, L"com.apple.install")
                    ) || (
                        FindSubStr (ourLoaderEntry->LoaderPath, L"OS X Install")
                    ) || (
                        FindSubStr (ourLoaderEntry->LoaderPath, L"MacOS Install")
                    ) || (
                        FindSubStr (ourLoaderEntry->LoaderPath, L"Mac OS Install")
                    ) || (
                        FindSubStr (ourLoaderEntry->LoaderPath, L"com.apple.install")
                    ) || (
                        ourLoaderEntry->Volume->VolName &&
                        FindSubStr (ourLoaderEntry->Volume->VolName, L"OS X Install")
                    ) || (
                        ourLoaderEntry->Volume->VolName &&
                        FindSubStr (ourLoaderEntry->Volume->VolName, L"Mac OS Install")
                    ) || (
                        ourLoaderEntry->Volume->VolName &&
                        FindSubStr (ourLoaderEntry->Volume->VolName, L"com.apple.install")
                    )
                ) {
                    #if REFIT_DEBUG > 0
                    // DA-TAG: Using separate instances of 'Received User Input:'
                    LOG_MSG("Received User Input:");
                    MsgStr = StrDuplicate (L"Load MacOS Installer");
                    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                    LOG_MSG(
                        "%s  - %s%s '%s'",
                        OffsetNext,
                        MsgStr,
                        (ourLoaderEntry->Volume->VolName)
                            ? L" on" : L":-",
                        (ourLoaderEntry->Volume->VolName)
                            ? ourLoaderEntry->Volume->VolName : ourLoaderEntry->LoaderPath
                    );
                    MY_FREE_POOL(MsgStr);
                    #endif

                    RunMacBootSupportFuncs();
                }
                else if (
                    (
                        SubScreenBoot && FindSubStr (SelectionName, L"OpenCore")
                    )
                    || FindSubStr (ourLoaderEntry->Title, L"OpenCore")
                    || FindSubStr (ourLoaderEntry->LoaderPath, L"\\OC\\")
                    || FindSubStr (ourLoaderEntry->LoaderPath, L"\\OpenCore")
                ) {
                    if (!ourLoaderEntry->UseGraphicsMode) {
                        ourLoaderEntry->UseGraphicsMode = (
                            (GlobalConfig.GraphicsFor & GRAPHICS_FOR_OPENCORE) == GRAPHICS_FOR_OPENCORE
                        );
                    }

                    #if REFIT_DEBUG > 0
                    // DA-TAG: Using separate instances of 'Received User Input:'
                    LOG_MSG("Received User Input:");
                    MsgStr = StrDuplicate (L"Load Instance: OpenCore");
                    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                    LOG_MSG(
                        "%s  - %s:- '%s'",
                        OffsetNext,
                        MsgStr,
                        ourLoaderEntry->LoaderPath
                    );
                    MY_FREE_POOL(MsgStr);
                    #endif
                }
                else if (
                    (
                        SubScreenBoot && FindSubStr (SelectionName, L"Clover")
                    )
                    || FindSubStr (ourLoaderEntry->Title, L"Clover")
                    || FindSubStr (ourLoaderEntry->LoaderPath, L"\\Clover")
                ) {
                    if (!ourLoaderEntry->UseGraphicsMode) {
                        ourLoaderEntry->UseGraphicsMode = (
                            (GlobalConfig.GraphicsFor & GRAPHICS_FOR_CLOVER) == GRAPHICS_FOR_CLOVER
                        );
                    }

                    #if REFIT_DEBUG > 0
                    // DA-TAG: Using separate instances of 'Received User Input:'
                    LOG_MSG("Received User Input:");
                    MsgStr = StrDuplicate (L"Load Instance: Clover");
                    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                    LOG_MSG(
                        "%s  - %s:- '%s'",
                        OffsetNext,
                        MsgStr,
                        ourLoaderEntry->LoaderPath
                    );
                    MY_FREE_POOL(MsgStr);
                    #endif
                }
                else if (
                    (
                        SubScreenBoot && FindSubStr (SelectionName, L"MacOS")
                    )
                    || ourLoaderEntry->OSType == 'M'
                    || FindSubStr (ourLoaderEntry->Title, L"MacOS")
                ) {
                    #if REFIT_DEBUG > 0
                    // DA-TAG: Using separate instances of 'Received User Input:'
                    LOG_MSG("Received User Input:");
                    MsgStr = StrDuplicate (L"Load Instance: MacOS");
                    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                    LOG_MSG("%s  - %s", OffsetNext, MsgStr);

                    DisplayName = NULL;
                    if (!ourLoaderEntry->Volume->VolName) {
                        LOG_MSG(":- '%s'", ourLoaderEntry->LoaderPath);
                    }
                    else {
                        if (ourLoaderEntry->Volume->FSType == FS_TYPE_APFS && GlobalConfig.SyncAPFS) {
                            APPLE_APFS_VOLUME_ROLE VolumeRole = 0;

                            // DA-TAG: Limit to TianoCore
                            #ifndef __MAKEWITH_TIANO
                            Status = EFI_NOT_STARTED;
                            #else
                            Status = RP_GetApfsVolumeInfo (
                                ourLoaderEntry->Volume->DeviceHandle,
                                NULL, NULL,
                                &VolumeRole
                            );
                            #endif

                            if (!EFI_ERROR(Status)) {
                                if (VolumeRole == APPLE_APFS_VOLUME_ROLE_PREBOOT) {
                                    DisplayName = GetVolumeGroupName (
                                        ourLoaderEntry->LoaderPath,
                                        ourLoaderEntry->Volume
                                    );
                                }
                            }
                        }

                        LOG_MSG(
                            " on '%s'",
                            (DisplayName)
                                ? DisplayName
                                : ourLoaderEntry->Volume->VolName
                        );
                    }
                    MY_FREE_POOL(MsgStr);
                    MY_FREE_POOL(DisplayName);
                    #endif

                    RunMacBootSupportFuncs();

                    if (GlobalConfig.NvramProtectEx) {
                        if (GlobalConfig.NvramProtect) {
                            // Start NvramProtect
                            SetProtectNvram (SystemTable, TRUE);
                        }
                    }
                }
                else if (
                    (
                        SubScreenBoot && FindSubStr (SelectionName, L"Windows")
                    )
                    || ourLoaderEntry->OSType == 'W'
                    || FindSubStr (ourLoaderEntry->Title, L"Windows")
                ) {
                    #if REFIT_DEBUG > 0
                    // DA-TAG: Using separate instances of 'Received User Input:'
                    LOG_MSG("Received User Input:");
                    MsgStr = StrDuplicate (L"Load Instance: Windows (UEFI)");
                    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                    LOG_MSG(
                        "%s  - %s%s '%s'",
                        OffsetNext,
                        MsgStr,
                        (ourLoaderEntry->Volume->VolName)
                            ? L" on" : L":-",
                        (ourLoaderEntry->Volume->VolName)
                            ? ourLoaderEntry->Volume->VolName : ourLoaderEntry->LoaderPath
                    );
                    MY_FREE_POOL(MsgStr);

                    MsgStr = PoolPrint (
                        L"NVRAM Filter:- '%s'",
                        (GlobalConfig.NvramProtect) ? L"Active" : L"Inactive"
                    );
                    LOG_MSG("%s    * %s", OffsetNext, MsgStr);
                    MY_FREE_POOL(MsgStr);
                    #endif

                    if (GlobalConfig.NvramProtect) {
                        // Start NvramProtect
                        SetProtectNvram (SystemTable, TRUE);
                    }
                }
                else if (
                    (
                        SubScreenBoot && FindSubStr (SelectionName, L"Grub")
                    )
                    || ourLoaderEntry->OSType == 'G'
                    || FindSubStr (ourLoaderEntry->Title, L"Grub")
                ) {
                    #if REFIT_DEBUG > 0
                    MsgStr = StrDuplicate (L"Load Instance: Linux (Grub)");
                    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                    // DA-TAG: Using separate instances of 'Received User Input:'
                    LOG_MSG("Received User Input:");
                    LOG_MSG(
                        "%s  - %s:- '%s'",
                        OffsetNext, MsgStr,
                        ourLoaderEntry->LoaderPath
                    );

                    MY_FREE_POOL(MsgStr);
                    #endif
                }
                else if (FindSubStr (ourLoaderEntry->LoaderPath, L"vmlinuz")) {
                    #if REFIT_DEBUG > 0
                    MsgStr = StrDuplicate (L"Load Instance: Linux (VMLinuz)");
                    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                    // DA-TAG: Using separate instances of 'Received User Input:'
                    LOG_MSG("Received User Input:");
                    LOG_MSG(
                        "%s  - %s:- '%s'",
                        OffsetNext, MsgStr,
                        ourLoaderEntry->LoaderPath
                    );

                    MY_FREE_POOL(MsgStr);
                    #endif
                }
                else if (FindSubStr (ourLoaderEntry->LoaderPath, L"bzImage")) {
                    #if REFIT_DEBUG > 0
                    MsgStr = StrDuplicate (L"Load Instance: Linux (BZImage)");
                    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                    // DA-TAG: Using separate instances of 'Received User Input:'
                    LOG_MSG("Received User Input:");
                    LOG_MSG(
                        "%s  - %s:- '%s'",
                        OffsetNext, MsgStr,
                        ourLoaderEntry->LoaderPath
                    );

                    MY_FREE_POOL(MsgStr);
                    #endif
                }
                else if (FindSubStr (ourLoaderEntry->LoaderPath, L"kernel")) {
                    #if REFIT_DEBUG > 0
                    MsgStr = StrDuplicate (L"Load Instance: Linux (Kernel)");
                    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                    // DA-TAG: Using separate instances of 'Received User Input:'
                    LOG_MSG("Received User Input:");
                    LOG_MSG(
                        "%s  - %s:- '%s'",
                        OffsetNext, MsgStr,
                        ourLoaderEntry->LoaderPath
                    );

                    MY_FREE_POOL(MsgStr);
                    #endif
                }
                else if (
                    (
                        SubScreenBoot && FindSubStr (SelectionName, L"Linux")
                    )
                    || ourLoaderEntry->OSType == 'L'
                    || FindSubStr (ourLoaderEntry->Title, L"Linux")
                ) {
                    #if REFIT_DEBUG > 0
                    // DA-TAG: Using separate instances of 'Received User Input:'
                    LOG_MSG("Received User Input:");
                    MsgStr = StrDuplicate (L"Load Instance: Linux");
                    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                    LOG_MSG("%s  - %s", OffsetNext, MsgStr);
                    if (ourLoaderEntry->Volume->VolName) {
                        LOG_MSG(" on '%s'", ourLoaderEntry->Volume->VolName);
                    }
                    else {
                        LOG_MSG(":- '%s'", ourLoaderEntry->LoaderPath);
                    }
                    MY_FREE_POOL(MsgStr);
                    #endif
                }
                else if (ourLoaderEntry->OSType == 'R') {
                    if (!ourLoaderEntry->UseGraphicsMode && AllowGraphicsMode) {
                        ourLoaderEntry->UseGraphicsMode = TRUE;
                    }

                    #if REFIT_DEBUG > 0
                    MsgStr = StrDuplicate (L"Load Instance: rEFIt Variant");
                    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                    // DA-TAG: Using separate instances of 'Received User Input:'
                    LOG_MSG("Received User Input:");
                    LOG_MSG(
                        "%s  - %s:- '%s'",
                        OffsetNext, MsgStr,
                        ourLoaderEntry->LoaderPath
                    );

                    MY_FREE_POOL(MsgStr);
                    #endif
                }
                else {
                    #if REFIT_DEBUG > 0
                    MsgStr = StrDuplicate (L"Load EFI File");
                    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                    // DA-TAG: Using separate instances of 'Received User Input:'
                    LOG_MSG("Received User Input:");
                    LOG_MSG(
                        "%s  - %s:- '%s'",
                        OffsetNext, MsgStr,
                        ourLoaderEntry->LoaderPath
                    );

                    MY_FREE_POOL(MsgStr);
                    #endif

                    if (GlobalConfig.NvramProtectEx) {
                        if (GlobalConfig.NvramProtect) {
                            // Some UEFI Windows installers/updaters may not be in the standard path
                            // So, activate NvramProtect (if set and allowed) on unidentified loaders
                            SetProtectNvram (SystemTable, TRUE);
                        }
                    }
                }

                // No end dash line ... Added in 'IsValidLoader'
                StartLoader (ourLoaderEntry, SelectionName);

                #if REFIT_DEBUG > 0
                UnexpectedReturn (L"OS Loader");
                #endif

            break;
            case TAG_LEGACY:   // Boot legacy OS
                ourLegacyEntry = (LEGACY_ENTRY *) ChosenEntry;

                #if REFIT_DEBUG > 0
                LOG_MSG("Received User Input:");
                #endif

                if (MyStrStr (ourLegacyEntry->Volume->OSName, L"Windows")) {
                    #if REFIT_DEBUG > 0
                    MsgStr = PoolPrint (
                        L"Load %s%s%s%s%s",
                        ourLegacyEntry->Volume->OSName,
                        SetVolJoin (ourLegacyEntry->Volume->OSName),
                        SetVolKind (ourLegacyEntry->Volume->OSName, ourLegacyEntry->Volume->VolName),
                        SetVolFlag (ourLegacyEntry->Volume->OSName, ourLegacyEntry->Volume->VolName),
                        SetVolType (ourLegacyEntry->Volume->OSName, ourLegacyEntry->Volume->VolName)
                    );
                    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                    LOG_MSG("%s  - %s", OffsetNext, MsgStr);
                    MY_FREE_POOL(MsgStr);
                    #endif
                }
                else {
                    #if REFIT_DEBUG > 0
                    MsgStr = StrDuplicate (L"Load 'Mac-Style' Legacy Bootcode");
                    ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                    LOG_MSG(
                        "%s  - %s:- '%s'",
                        OffsetNext, MsgStr,
                        (ourLegacyEntry->Volume)
                            ? ourLegacyEntry->Volume->OSName
                            : L"NULL Volume Label"
                    );
                    MY_FREE_POOL(MsgStr);
                    #endif
                }

                // No end dash line ... Added in 'StartLegacyImageList'
                StartLegacy (ourLegacyEntry, SelectionName);

                #if REFIT_DEBUG > 0
                UnexpectedReturn (L"OS Loader");
                #endif

            break;
            case TAG_LEGACY_UEFI: // Boot a legacy OS on a non-Mac
                ourLegacyEntry = (LEGACY_ENTRY *) ChosenEntry;

                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"Load 'UEFI-Style' Legacy Bootcode");
                ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                LOG_MSG("Received User Input:");
                LOG_MSG(
                    "%s  - %s:- '%s'",
                    OffsetNext, MsgStr,
                    (ourLegacyEntry->Volume)
                        ? ourLegacyEntry->Volume->OSName
                        : L"NULL Volume Label"
                );
                MY_FREE_POOL(MsgStr);
                #endif

                // No end dash line ... Added in 'BdsLibDoLegacyBoot'
                StartLegacyUEFI (ourLegacyEntry, SelectionName);

                #if REFIT_DEBUG > 0
                UnexpectedReturn (L"OS Loader");
                #endif

            break;
            case TAG_TOOL:     // Start a UEFI tool
                ourLoaderEntry = (LOADER_ENTRY *) ChosenEntry;

                #if REFIT_DEBUG > 0
                MsgStr = StrDuplicate (L"Start UEFI Tool");
                ALT_LOG(1, LOG_LINE_THIN_SEP, L"%s", MsgStr);
                LOG_MSG("Received User Input:");
                LOG_MSG("%s  - %s:- '%s'", OffsetNext, MsgStr, ourLoaderEntry->LoaderPath);
                MY_FREE_POOL(MsgStr);
                #endif

                // No end dash line ... Expected to return
                StartTool (ourLoaderEntry);

            break;
            case TAG_FIRMWARE_LOADER:  // Reboot to a loader defined in the NVRAM
                ourLoaderEntry = (LOADER_ENTRY *) ChosenEntry;

                #if REFIT_DEBUG > 0
                LOG_MSG("Received User Input:");
                LOG_MSG("%s  - Reboot into Firmware Loader", OffsetNext);
                #endif

                // No end dash line ... Added in 'RebootIntoLoader'
                RebootIntoLoader (ourLoaderEntry);

                #if REFIT_DEBUG > 0
                UnexpectedReturn (L"Firmware Reboot");
                #endif

            break;
            case TAG_HIDDEN:  // Manage hidden tag entries

                #if REFIT_DEBUG > 0
                LOG_MSG("Received User Input:");
                LOG_MSG("%s  - Restore Tags", OffsetNext);
                LOG_MSG("\n\n");
                #endif

                // No end dash line ... Expected to return
                ManageHiddenTags();

                #if REFIT_DEBUG > 0
                LOG_MSG("Received User Input:");
                LOG_MSG("%s  - Exit 'Restore Tags'", OffsetNext);
                LOG_MSG("\n\n");
                #endif

            break;
            case TAG_EXIT:    // Exit RefindPlus
                #if REFIT_DEBUG > 0
                LOG_MSG("Received User Input:");
                LOG_MSG("%s  - Close RefindPlus", OffsetNext);
                OUT_TAG();
                #endif

                if ((MokProtocol) && !SecureBootUninstall()) {
                    // Just in case we get this far
                    MainLoopRunning = FALSE;
                }
                else {
                   BeginTextScreen (L" ");
                   return EFI_SUCCESS;
                }

            break;
            case TAG_FIRMWARE: // Reboot into firmware's user interface
                #if REFIT_DEBUG > 0
                LOG_MSG("Received User Input:");
                LOG_MSG("%s  - Reboot into Firmware", OffsetNext);
                #endif

                // No end dash line ... Added in 'RebootIntoFirmware'
                RebootIntoFirmware();

            break;
            case TAG_CSR_ROTATE:
                #if REFIT_DEBUG > 0
                LOG_MSG("Received User Input:");
                LOG_MSG("%s  - Rotate CSR", OffsetNext);
                #endif

                // No end dash line ... Expected to return
                // Unset DynamicCSR if active
                RotateCsrValue (TRUE);

            break;
            case TAG_INSTALL:
                #if REFIT_DEBUG > 0
                LOG_MSG("Received User Input:");
                LOG_MSG("%s  - Install RefindPlus", OffsetNext);
                LOG_MSG("\n\n");
                #endif

                // No end dash line ... Expected to return
                InstallRefindPlus();

            break;
            case TAG_BOOTORDER:
                #if REFIT_DEBUG > 0
                LOG_MSG("Received User Input:");
                LOG_MSG("%s  - Manage Boot Order", OffsetNext);
                LOG_MSG("\n\n");
                #endif

                // No end dash line ... Expected to return
                ManageBootorder();

                #if REFIT_DEBUG > 0
                LOG_MSG("Received User Input:");
                LOG_MSG("%s  - Exit 'Boot Order'", OffsetNext);
                LOG_MSG("\n\n");
                #endif

            break;
        } // switch

        // Disable DirectBoot if still active after first loop
        if (GlobalConfig.DirectBoot) {
            GlobalConfig.DirectBoot = FALSE;
            MainMenu->TimeoutSeconds = GlobalConfig.Timeout = 0;
            DrawScreenHeader (MainMenu->Title);
        }

        // Flag at least one loop done
        OneMainLoop = TRUE;
    } // while

    // Things have gone wrong if we end up here ... Try to reboot.
    // Go into a dead loop if the reboot attempt fails.
    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"Unexpected Main Loop Exit ... Try to Reboot!!");

    LOG_MSG("Fallback: System Restart...");
    LOG_MSG("\n");
    LOG_MSG("Screen Termination:");
    LOG_MSG("\n");
    #endif

    TerminateScreen();

    #if REFIT_DEBUG > 0
    LOG_MSG("System Reset:");
    LOG_MSG("\n\n");
    #endif

    REFIT_CALL_4_WRAPPER(
        gRT->ResetSystem, EfiResetCold,
        EFI_SUCCESS, 0, NULL
    );

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_THREE_STAR_SEP, L"Reset After Unexpected Main Loop Exit:- 'FAILED!!'");
    #endif

    SwitchToText (FALSE);

    MsgStr = StrDuplicate (L"E N T E R I N G   D E A D   L O O P");

    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_SET;
    #endif
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText (MsgStr, NEXTLINE);
    REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    #if REFIT_DEBUG > 0
    MY_MUTELOGGER_OFF;
    #endif

    #if REFIT_DEBUG > 0
    ALT_LOG(1, LOG_BLANK_LINE_SEP, L"X");
    ALT_LOG(1, LOG_LINE_NORMAL, L"%s", MsgStr);
    LOG_MSG("INFO: %s", MsgStr);
    OUT_TAG();
    #endif

    MY_FREE_POOL(MsgStr);

    PauseForKey();
    RefitDeadLoop();

    return EFI_SUCCESS;
} // EFI_STATUS EFIAPI efi_main()
