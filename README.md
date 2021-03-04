# RefindPlus
## Overview
Enhanced version of the amazing rEFInd Boot Manager that incorporates various fixes and additional features.

The development focus is on MacPro3,1 to MacPro5,1 (and equivalent Xserve), but may be useful to other rEFInd users (especially if on a Mac).

## Headline Features
- Maintains feature and configuration parity with the base rEFInd version.
- Protects against damage to Mac NVRAM when booting UEFI Windows.
- Provides Pre-Boot Configuration Screen on units running GPUs without Native EFI on Macs.
- Provides UGADraw on modern GOP based GPUs to permit booting legacy EFIBoot operating systems.
- Adds a debug version that provides extensive logging.
  * The release version is kept as an optimised version for day to day use.
- Fixes inability of rEFInd to print to screen on Macs
  * This prevented receiving program messages as well as leveraging advanced features such as EFI Shell.
- Provides APFS filesystem capability when required.
  * Removes the need to add APFS drivers to run recent Mac OS releases on units without APFS support.
  * Additionally, this ensures that matching APFS drivers for specific Mac OS releases are used.
- Supports Apple's APFS filesystem requirements
  * This allows booting Mac OS v11.0 (Big Sur) from named volumes on the main screen, as opposed to generic 'PreBoot' volumes, without requiring SIP to be disabled (potentially compromising system integrity).
  * This also allows booting FileVault encrypted volumes from named volumes on the main screen, as opposed to generic 'PreBoot' volumes.

## Additional Configurable Functionality
RefindPlus-specific funtionality can be activated by adding the tokens below to a rEFInd config file and passing to RefindPlus.

Config Token| Functionality
--- | ---
continue_on_warning| Proceeds as if a key is pressed after screen warnings (enables remote login).
direct_gop_renderer| Provides a potentially improved GOP instance for certain GPUs.
disable_amfi| Disables AMFI Checks on Mac OS if required.
disable_mac_compat_check| Disables Mac version compatibility checks if required.
force_trim| Forces `TRIM` on non-Apple SSDs if required.
ignore_previous_boot| Disables saving the last booted loader if not required.
protect_mac_nvram| Stops UEFI Windows from saving certificates to Apple NVRAM.
provide_console_gop| Fixes issues with GOP on some Legacy Macs.
reinforce_apfs| Allows directly booting Big Sur as well as FileVault (without needing PreBoot).
scale_ui| Provides control of UI element scaling.
scan_other_esp| Allows other ESPs other than the RefindPlus ESP to be scanned for loaders.
set_mac_boot_args| Allows arbitrary Mac OS boot argument strings.
supply_apfs| Provides APFS file system capability if required.
suppress_verbose_apfs| Supresses verbose APFS text on boot (if required when using `supply_apfs`).
text_renderer| Provides a text renderer that allows text mode when not otherwise available.
uga_pass_through| Provides UGA instance on GOP to permit EFIBoot with modern GPUs.

In addition to the tokens above, two additional tools can be activated using the `showtools` token:
- `clean_nvram` : This allows resetting nvram directly from RefindPlus.
- `show_bootscreen` : This allows compatible GPUs to load the Apple Pre Boot Configuration screen.

## Installation
[MyBootMgr](https://www.dakanji.com/creations/index.html), an automated preconfigured implementation of a RefindPlus/OpenCore chain-loading arrangement is recommended for implementation on MacPro3,1 to MacPro5,1 as well as on Xserve3,1. However, the RefindPlus efi can work as a drop-in replacement for the rEFInd efi. Hence, you can get the [rEFInd package](https://www.rodsbooks.com/refind/getting.html) and [install this](https://www.rodsbooks.com/refind/installing.html) first. Once rEFInd is installed, replace the rEFInd efi with the RefindPlus efi. (Ensure that you rename the RefindPlus efi to match the rEFInd efi name). This permits implementing RefindPlus on other Mac types as well as on other operating systems supported by rEFInd. 

While RefindPlus will function with the rEFInd configuration file, `refind.conf`, this should be replaced with the RefindPlus configuration file, `config.conf`, to configure the additonal options provided by RefindPlus. A sample RefindPlus configuration file is available here: [config.conf-sample](https://github.com/dakanji/RefindPlus/blob/GOPFix/config.conf-sample).

Note that if you run RefindPlus without activating the additonal  options, as will be the case if using an unmodified rEFInd configuration file, a RefindPlus run will be equivalent to running the rEFInd version it is based on, currently v0.13.1. That is, the additonal options provided in RefindPlus must be actively enabled if they are required.

Configuration differences between the rEFInd and RefindPlus implementations as at rEFInd v0.13.1 are:
- `firmware_bootnum`: Manual Stanzas with this token are ignored by RefindPlus due to reliability issues.
- `write_systemd_vars`: Systemd EFI variables are not written unless specifically set to do so by activating this configuration token.
- `use_nvram`: Application variables are written to the `vars` folder on the file system instead of to the motherboard's NVRAM unless specifically set to do so by activating this configuration token.
- `resolution`: The `max` setting is ignored as the maximum available resolution is automatically used by default by RefindPlus when required.
- `log_level`: Ignored by RefindPlus as debug logs are provided by a dedicated debug build.
- rEFInd now scans other ESPs for loaders, in addition to the ESP containing the rEFInd loader. The earlier behaviour, where other ESPs were treated as duplicates and ignored, has been considered an error and changed. This earlier behaviour is preferred and maintained in RefindPlus. Users are however provided an option to override this behaviour, in favour of the new rEFInd behaviour, by activating the RefindPlus-specific `scan_other_esp` configuration token.

## Roll Your Own
Refer to [BUILDING.md](https://github.com/dakanji/RefindPlus/blob/GOPFix/BUILDING.md) for build instructions (x64 Only).
