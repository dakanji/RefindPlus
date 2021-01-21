# RefindPlus
## Overview
Enhanced version of the amazing rEFInd Boot Manager that incorporates various fixes and additional features.

The development focus is on Classic MacPros, MacPro3,1 to MacPro5,1 (and equivalent Xserve), but should be useful for all users of rEFInd.

## Headline Features
- Protects against damage to Mac NVRAM when booting UEFI Windows.
- Adds a debug version that provides extensive logging.
- Provides Pre-Boot Configuration Screen on units running GPUs without Native EFI on Macs.
- Provides UGADraw to permit booting legacy operating systems using EFIBoot.
- Fixes inability of rEFInd to print to screen on Macs
  * This prevented receiving program messages as well as leveraging advanced features such as EFI Shell.
- Allows disabling SIP to a high level
  * Values such as `87F`, `977`, or the maximum `FFF`, are possible to allow unsigned DMG packages needed to run Mac OS v11.x (Big Sur) in unsupported environments.
- Provides built-in APFS driver activation.
  * Removes the need to add APFS drivers to run recent Mac OS releases on units without APFS support.


## Installation
[MyBootMgr](https://forums.macrumors.com/threads/thread.2231693), an automated preconfigured implementation of a RefindPlus/OpenCore chain-loading arrangement is recommended for implementation on MacPro3,1, MacPro4,1, MacPro5,1 and Xserve3,1. However, the RefindPlus efi file can work as a drop-in replacement for the default rEFInd efi file. Hence, to install, you can get the [default rEFInd package](https://www.rodsbooks.com/refind/getting.html) and [install this](https://www.rodsbooks.com/refind/installing.html) as normal.

Once rEFInd is installed, replace the rEFInd efi file with the RefindPlus efi file. (Ensure that you rename the RefindPlus efi file to match the rEFInd efi file name).

While RefindPlus will function with the rEFInd configuration file, refind.conf, this should be replaced with the RefindPlus configuration file, config.conf, to configure the additonal options provided by RefindPlus. A sample file is provided: [config.conf-sample](https://github.com/dakanji/RefindPlus/blob/GOPFix/config.conf-sample).

## Additional Configurable Functionality
- text_renderer
- uga_pass_through
- provide_console_gop
- direct_gop_renderer
- continue_on_warning
- trim_force
- disable_mac_compat_check
- disable_amfi
- enable_apfs
- suppress_verbose_apfs
- protect_mac_nvram
- set_mac_boot_args
- scale_ui

## Roll Your Own
Refer to [BUILDING.md](https://github.com/dakanji/RefindPlus/blob/GOPFix/BUILDING.md) for build instructions (x64 Only).
