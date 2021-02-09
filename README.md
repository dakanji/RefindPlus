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
  * Values such as `87F`, `977`, or the maximum `FFF`, are possible to allow unsigned DMG packages needed to run Mac OS v11.0 (Big Sur) in unsupported environments.
- Provides built-in APFS driver activation.
  * Removes the need to add APFS drivers to run recent Mac OS releases on units without APFS support.


## Installation
[MyBootMgr](https://www.dakanji.com/creations/index.html), an automated preconfigured implementation of a RefindPlus/OpenCore chain-loading arrangement is recommended for implementation on MacPro3,1 to MacPro5,1 as well as on Xserve3,1. However, the RefindPlus efi can work as a drop-in replacement for the rEFInd efi. Hence, you can get the [rEFInd package](https://www.rodsbooks.com/refind/getting.html) and [install this](https://www.rodsbooks.com/refind/installing.html) first. This permits implementing RefindPlus on other Mac types as well as on other operating systems supported by rEFInd.

Once rEFInd is installed, replace the rEFInd efi with the RefindPlus efi. (Ensure that you rename the RefindPlus efi to match the rEFInd efi name).

While RefindPlus will function with the rEFInd configuration file, `refind.conf`, this should be replaced with the RefindPlus configuration file, `config.conf`, to configure the additonal options provided by RefindPlus.

Note that if you run RefindPlus without activating the additonal RefindPlus options, as will be the case if using an unmodified rEFInd configuration file, RefindPlus will behave exactly as if you are running rEFInd. That is, the additonal RefindPlus options must be actively enabled if they are required.

A sample RefindPlus configuration file is provided here: [config.conf-sample](https://github.com/dakanji/RefindPlus/blob/GOPFix/config.conf-sample).

## Additional Configurable Functionality
- text_renderer
- uga_pass_through
- provide_console_gop
- direct_gop_renderer
- continue_on_warning
- trim_force
- disable_mac_compat_check
- disable_amfi
- supply_apfs
- suppress_verbose_apfs
- enforce_apfs
- protect_mac_nvram
- set_mac_boot_args
- scale_ui

## Roll Your Own
Refer to [BUILDING.md](https://github.com/dakanji/RefindPlus/blob/GOPFix/BUILDING.md) for build instructions (x64 Only).
