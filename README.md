# RefindPlus
## Overview
Enhanced version of the amazing rEFInd Boot Manager that incorporates various fixes and additional features.

The development focus is on Classic MacPros (3,1 to 5,1) but should be useful for all users of rEFInd.

The aim is for most, if not all, of the fixes and features to be merged upsteam into rEFInd.

## Headline Features
- Protects against damage to Mac NVRAM when booting UEFI Windows.
- Provides Pre-Boot Configuration Screen on units running GPUs without Native EFI on Macs.
- Fixes inability of rEFInd to print to screen on Macs
  - This prevented receiving program messages as well as leveraging advanced features such as EFI Shell.
- Adds a debug version that provides extensive logging.
- Allows disabling SIP to a high level
  - The new default level is `877` compared to the previous `77`
  - Values such as `977`, or the maximum `FFF`, are possible to allow unsigned DMG packages needed to run Mac OS v11.x (Big Sur) in unsupported environments.
- Provides UGADraw to permit booting legacy operating systems using EFIBoot.
- Misc Code Optimisation.

## Installation
The RefindPlus efi file can work as a drop-in replacement for the default rEFInd efi file. Hence, to install, you can get the [default rEFInd package](https://www.rodsbooks.com/refind/getting.html) and [install this](https://www.rodsbooks.com/refind/installing.html) as normal.

Once rEFInd is installed, replace the rEFInd efi file with the RefindPlus efi file. (Ensure that you rename the RefindPlus efi file to match the rEFInd efi file name).

Consider replacing the rEFInd configuration file, refind.conf, with the RefindPlus configuration file, config.conf ... see config.conf-sample file, to configure the additonal options provided by RefindPlus.

Alternatively, you can use [MyBootMgr](https://forums.macrumors.com/threads/thread.2231693), a preconfigured RefindPlus/OpenCore chain-loading package. **(Recommended for MacPro3,1, MacPro4,1 and MacPro5,1)**

## Additional Configurable Functionality
- text_renderer
- uga_pass_through
- provide_console_gop
- direct_gop_renderer
- continue_on_warning
- force_trim
- disable_mac_compat_check
- disable_amfi
- protect_mac_nvram
- set_mac_boot_args
- scale_ui

## Roll Your Own
Refer to [BUILDING.md](https://github.com/dakanji/RefindPlus/blob/GOPFix/BUILDING.md) for build instructions (x64 Only).
