[Defines]
  PLATFORM_NAME                  = RefindPlus
  PLATFORM_GUID                  = cf424b08-2e9d-4e77-a6dd-71e0c8de60f3
  PLATFORM_VERSION               = 4.5.0
  DSC_SPECIFICATION              = 0x00010006
  SUPPORTED_ARCHITECTURES        = IA32|IPF|X64|EBC|ARM|AARCH64
  BUILD_TARGETS                  = DEBUG
  SKUID_IDENTIFIER               = DEFAULT

[LibraryClasses]
  #
  # Entry point
  #
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  SynchronizationLib|MdePkg/Library/BaseSynchronizationLib/BaseSynchronizationLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  PciLib|MdePkg/Library/BasePciLibCf8/BasePciLibCf8.inf
  PciCf8Lib|MdePkg/Library/BasePciCf8Lib/BasePciCf8Lib.inf
  CacheMaintenanceLib|MdePkg/Library/BaseCacheMaintenanceLib/BaseCacheMaintenanceLib.inf
  PeCoffLib|MdePkg/Library/BasePeCoffLib/BasePeCoffLib.inf
  PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
  #
  # UEFI & PI
  #
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  UefiDecompressLib|MdePkg/Library/BaseUefiDecompressLib/BaseUefiDecompressLib.inf
  PeiServicesTablePointerLib|MdePkg/Library/PeiServicesTablePointerLib/PeiServicesTablePointerLib.inf
  PeiServicesLib|MdePkg/Library/PeiServicesLib/PeiServicesLib.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  #
  # Generic Modules
  #
  UefiUsbLib|MdePkg/Library/UefiUsbLib/UefiUsbLib.inf
  UefiScsiLib|MdePkg/Library/UefiScsiLib/UefiScsiLib.inf
  NetLib|MdeModulePkg/Library/DxeNetLib/DxeNetLib.inf
  IpIoLib|MdeModulePkg/Library/DxeIpIoLib/DxeIpIoLib.inf
  UdpIoLib|MdeModulePkg/Library/DxeUdpIoLib/DxeUdpIoLib.inf
  TcpIoLib|MdeModulePkg/Library/DxeTcpIoLib/DxeTcpIoLib.inf
  DpcLib|MdeModulePkg/Library/DxeDpcLib/DxeDpcLib.inf
  SecurityManagementLib|MdeModulePkg/Library/DxeSecurityManagementLib/DxeSecurityManagementLib.inf
  TimerLib|MdePkg/Library/BaseTimerLibNullTemplate/BaseTimerLibNullTemplate.inf
  SerialPortLib|MdePkg/Library/BaseSerialPortLibNull/BaseSerialPortLibNull.inf
  CapsuleLib|MdeModulePkg/Library/DxeCapsuleLibNull/DxeCapsuleLibNull.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  #
  # Misc
  #
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  ReportStatusCodeLib|MdePkg/Library/BaseReportStatusCodeLibNull/BaseReportStatusCodeLibNull.inf
  PeCoffExtraActionLib|MdePkg/Library/BasePeCoffExtraActionLibNull/BasePeCoffExtraActionLibNull.inf
  PerformanceLib|MdePkg/Library/BasePerformanceLibNull/BasePerformanceLibNull.inf
  DebugAgentLib|MdeModulePkg/Library/DebugAgentLibNull/DebugAgentLibNull.inf
  PlatformHookLib|MdeModulePkg/Library/BasePlatformHookLibNull/BasePlatformHookLibNull.inf
  ResetSystemLib|MdeModulePkg/Library/BaseResetSystemLibNull/BaseResetSystemLibNull.inf
  SmbusLib|MdePkg/Library/DxeSmbusLib/DxeSmbusLib.inf
  S3BootScriptLib|MdeModulePkg/Library/PiDxeS3BootScriptLib/DxeS3BootScriptLib.inf
  CpuExceptionHandlerLib|MdeModulePkg/Library/CpuExceptionHandlerLibNull/CpuExceptionHandlerLibNull.inf

  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  BaseStackCheckLib|MdePkg/Library/BaseStackCheckLib/BaseStackCheckLib.inf

  MemLogLib|RefindPlusPkg/Library/MemLogLib/MemLogLib.inf

  # Added by dakanji (sf.net/u/dakanji/profile) from OpenCore
    OcConsoleLib|OpenCorePkg/Library/OcConsoleLib/OcConsoleLib.inf
    FrameBufferBltLib|MdeModulePkg/Library/FrameBufferBltLib/FrameBufferBltLib.inf
    MtrrLib|UefiCpuPkg/Library/MtrrLib/MtrrLib.inf
    CpuLib|MdePkg/Library/BaseCpuLib/BaseCpuLib.inf
    OcMiscLib|OpenCorePkg/Library/OcMiscLib/OcMiscLib.inf
    OcFileLib|OpenCorePkg/Library/OcFileLib/OcFileLib.inf
    OcGuardLib|OpenCorePkg/Library/OcGuardLib/OcGuardLib.inf
    OcStringLib|OpenCorePkg/Library/OcStringLib/OcStringLib.inf
    OcDevicePathLib|OpenCorePkg/Library/OcDevicePathLib/OcDevicePathLib.inf
    RP_ApfsLib|RefindPlusPkg/Library/RP_ApfsLib/RP_ApfsLib.inf
    OcDriverConnectionLib|OpenCorePkg/Library/OcDriverConnectionLib/OcDriverConnectionLib.inf

  # Added by dakanji (sf.net/u/dakanji/profile) for AcquireGOP
    HandleParsingLib|ShellPkg/Library/UefiHandleParsingLib/UefiHandleParsingLib.inf
    FileHandleLib|MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
    SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf

  # Added by dakanji (sf.net/u/dakanji/profile) for SupplyNVME
    NvmExpressLib|RefindPlusPkg/Library/NvmExpressLib/NvmExpressLib.inf

[LibraryClasses.AARCH64]
  CompilerIntrinsicsLib|ArmPkg/Library/CompilerIntrinsicsLib/CompilerIntrinsicsLib.inf

[Components]
  RefindPlusPkg/RefindPlus.inf
  RefindPlusPkg/filesystems/ext2.inf
  RefindPlusPkg/filesystems/ext4.inf
  RefindPlusPkg/filesystems/reiserfs.inf
  RefindPlusPkg/filesystems/hfs.inf
  RefindPlusPkg/filesystems/iso9660.inf
  RefindPlusPkg/filesystems/ntfs.inf
  RefindPlusPkg/gptsync/gptsync.inf

[PcdsFixedAtBuild]
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x1D
  gEfiMdePkgTokenSpaceGuid.PcdDebugClearMemoryValue|0x00
