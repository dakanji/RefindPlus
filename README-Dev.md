[![Latest Release](https://img.shields.io/github/release/dakanji/RefindPlus.svg?flat=1&label=current)](https://github.com/dakanji/RefindPlus/releases) [![Release date](https://img.shields.io/github/release-date/dakanji/RefindPlus.svg?flat=1&color=informational&label=when)](https://github.com/dakanji/RefindPlus/releases) [![Scan Status](https://scan.coverity.com/projects/22695/badge.svg?flat=1)](https://scan.coverity.com/projects/22695)

# RefindPlus
## Overview
RefindPlus is a boot manager for Mac and PC.

It is a fork of the [rEFInd Boot Manager](https://www.rodsbooks.com/refind) incorporating several fixes and additional features.

The main development focus is on the following units:
- **MacPro3,1**: Early 2008 Mac Pro
- **MacPro4,1**: Early 2009 Mac Pro
- **MacPro5,1**: Mid 2010 and Mid 2012 Mac Pro
- **Xserve2,1**: Early 2008 Xserve
- **Xserve3,1**: Early 2009 Xserve

However, the enhancements and fixes provided by RefindPlus are not limited in scope to those units and include several other Apple Mac as well as multiple UEFI-PC related items that may be of interest to anyone requiring a capable and flexible boot manager.

## Headline Features
- Maintains feature and configuration parity with the base upstream version.
- Protects against damage to Mac nvRAM when booting UEFI Windows.
- Provides Pre-Boot Configuration Screen on units running GPUs without native EFI on Macs.
- Provides UGADraw on modern GOP based GPUs to permit booting legacy EFI Boot operating systems.
- Provides improved support for languages that use unicode text.
- Emulates UEFI 2.x on EFI 1.x units to permit running UEFI 2.x utilities on such units.
- Extensive memory management improvements with associated speed and stability gains.
- Adds a debug (DBG) binary that provides extensive logging.
  * The release (REL) binary is an optimised build for day to day use.
- Fixes upstream inability to print to screen on some Macs.
  * This prevented receiving program messages as well as leveraging advanced features such as uEFI shell.
- Provides NVMe capability, if required, via an inbuilt NvmExpress driver.
  * Removes the need to add NVMe drivers on units without NVMe support.
  * Basically allows working as if NVMe is natively supported by the firmware.
    - Removes the need for a risky `firmware flash` operation on units such as the MacPro3,1.
- Provides APFS filesystem capability, if required, via an inbuilt APFS JumpStart driver.
  * Removes the need to add APFS drivers to run recent macOS releases on units without APFS support.
  * Additionally, this ensures that matching APFS drivers for specific macOS releases are used.
  * Basically allows working as if APFS is natively supported by the firmware.
    - Removes the need for a risky `firmware flash` operation on units such as the MacPro3,1.
- Fully supports APFS filesystem requirements.
  * This allows booting macOS 11.x (Big Sur) or later from single named volumes on the main screen.
    - As opposed to generic and difficult to distinguish `PreBoot` volumes.
    - Avoids potentially compromising system integrity by otherwise requiring SIP to be disabled.
  * This also allows booting FileVault encrypted volumes from single named volumes on the main screen.
    - As opposed to generic and difficult to distinguish `PreBoot` volumes.

## Installation
[MyBootMgr](https://www.dakanji.com/creations/index.html) is recommended to automate installing RefindPlus on macOS. Alternatively, as the RefindPlus efi file can function as a drop-in replacement for the upstream efi file, the [rEFInd package](https://www.rodsbooks.com/refind/installing.html) can be installed first and its efi file replaced with the RefindPlus efi file. (Ensure the RefindPlus efi file is renamed to match). This manual process allows installing RefindPlus on other operating systems supported upstream. On macOS, MyBootMgr can optionally be used to set a RefindPlus|OpenCore chain-loading arrangement up on MacPro3,1 to MacPro5,1 as well as on Xserve2,1 and Xserve3,1.

Users may also want to replace upstream filesystem drivers with those packaged with RefindPlus as these are always either exactly the same as upstream versions or have had fixes applied.

RefindPlus will function with the upstream configuration file, `refind.conf`, but users may wish to replace this with the RefindPlus configuration file, `config.conf`, to configure the additional options provided by RefindPlus. A sample RefindPlus configuration file is available here: [config.conf-sample-Dev](https://github.com/dakanji/RefindPlus/blob/GOPFix/config.conf-sample-Dev). RefindPlus-specific options can also be added to an upstream configuration file and used that way if preferred.

Note that if RefindPlus is run without activating the additional options, as will be the case if using an unmodified upstream configuration file, a RefindPlus run will be equivalent to running the upstream version it is based on, currently v0.14.1. That is, the additional options provided in RefindPlus must be actively enabled if they are required. This equivalence is subject to a few divergent items in RefindPlus as outlined under the [Divergence](https://github.com/dakanji/RefindPlus#divergence) section below. NB: Upstream post-release code updates are typically ported to RefindPlus as they happen and as such, RefindPlus releases are actually at the state of the base upstream release version plus any such updates and typically include updates for subsequent upstream release versions since the base version.

## Additional Functionality
RefindPlus-specific funtionality can be configured by adding the tokens below to the upstream configuration file.
Additional information is provided in the sample RefindPlus configuration file.

Token | Functionality
----- | -----
continue_on_warning   |Proceed as if a key was pressed after screen warnings (for unattended boot)
csr_dynamic           |Actively enables or disables the SIP Policy on Macs
csr_normalise         |Removes the `APPLE_INTERNAL` bit, when present, to permit OTA updates
decline_help_icon     |Disables feature that may improve loading speed by preferring generic icons
decline_help_scan     |Disables feature that skips showing misc typically unwanted loaders
decline_help_size     |Disables feature that sets additional UI scaling for very high DPI screens
decline_help_text     |Disables complementary text colours if not required
decouple_key_f10      |Unmaps the `F10` key from native screenshots (the `\` key remains mapped)
disable_apfs_load     |Disables inbuilt provision of APFS filesystem capability
disable_apfs_sync     |Disables feature allowing direct APFS/FileVault boot (Without "PreBoot")
disable_check_amfi    |Disables AMFI Checks on macOS if required
disable_check_compat  |Disables Mac version compatibility checks if required
disable_pass_gop_thru |Disables feature that provides GOP instance on UGA for some loading screens
disable_legacy_sync   |Disables detailed indentification of Mac Legacy BIOS Boot capability
disable_nvram_paniclog|Disables logging macOS kernel panics to nvRAM
disable_nvram_protect |Disables blocking of potentially harmful write attempts to Legacy Mac nvRAM
disable_reload_gop    |Disables reinstallation of UEFI 2.x GOP drivers on EFI 1.x units
disable_rescan_dxe    |Disables scanning for newly revealed DXE drivers when connecting handles
disable_set_applefb   |Disables provision, under some circumstances, of missing Apple Framebuffers
disable_set_consolegop|Disables feature that fixes some issues with GOP graphics on legacy units
enable_esp_filter     |Prevents other ESPs other than the RefindPlus ESP being scanned for loaders
force_trim            |Forces `TRIM` on Third-Party SSDs on Macs if required
handle_ventoy         |Rationalises the binaries displayed for `Ventoy` instances
hidden_icons_external |Allows scanning for `.VolumeIcon` icons on external volumes
hidden_icons_ignore   |Disables scanning for `.VolumeIcon` image icons if not required
hidden_icons_prefer   |Prioritises `.VolumeIcon` and `.VolumeBadge` image icons when available
icon_row_move         |Repositions the main screen icon rows (vertically)
icon_row_tune         |Fine tunes the resulting `icon_row_move` outcome
mitigate_primed_buffer|Allows enhanced intervention to handle apparent primed keystroke buffers
nvram_protect_ex      |Extends `NvramProtect`, if set, to macOS and `unknown` UEFI boots
nvram_variable_limit  |Limits nvRAM write attempts to the specified variable size
pass_uga_through      |Provides UGA instance on GOP to permit EFI Boot with modern GPUs
persist_boot_args     |Overrides using vRAM, and not nvRAM, for macOS boot argument items
prefer_uga            |Prefers UGA use (when available) regardless of GOP availability
ransom_drives         |Frees partitions locked by how certain firmware load inbuilt drivers
renderer_direct_gop   |Provides a potentially improved GOP instance for certain GPUs
renderer_text         |Provides a text renderer for text output when otherwise unavailable
scale_ui              |Provides control of UI element scaling
screen_rgb            |Allows setting arbitrary screen background colours
set_boot_args         |Allows setting arbitrary macOS boot arguments
supply_nvme           |Enables an inbuilt NvmExpress driver
supply_uefi           |Enables feature that emulates UEFI 2.x support on EFI 1.x units
sync_nvram            |Resets nvRAM settings, such as BlueTooth, on some boot types if required
transient_boot        |Disables selection of the last booted loader if not required
unicode_collation     |Provides fine tuned support for languages that use unicode text

## Modified Functionality
In addition to the new functionality listed above, the following upstream tokens have been modified:
- **"use_graphics_for" Token:** OpenCore and Clover added as options that can be set to boot in graphics mode.
- **"showtools" Token:** Additional tool added:
  - `clean_nvram` : Allows resetting nvram directly from RefindPlus.
    - When run on Apple firmware, RefindPlus will additionally trigger nvRAM garbage collection
- **"csr_values" Token:** A value of `0` can be set as the `Enabled` value to ensure `Over The Air` (OTA) updates when running macOS 11.x (Big Sur), or later, with SIP enabled.
  - This is equivalent to activating the `csr_normalise` token.
- **"timeout" Token:** The default is no timeout unless explicitly set.
- **"screensaver" Token:** The RefindPlus screensaver cycles through a set of colours as opposed to a single grey colour.
- **"use_nvram" Token:** RefindPlus variables are written to the file system and not the motherboard's nvRAM unless explicitly set to do so by activating this configuration token.
- **"log_level" Token:** Controls the native log format and an implementation of the upstream format.
  * Only active on `DEBUG` and `NOOPT` builds while `RELEASE` builds remain optimised for day-to-day use.
  * Level 0 does not switch logging off but activates the native summary format.
  * Levels 1 and 2 output logs similar to the detailed upstream format.
    - Level 1 is broadly equivalent to upstream Level 4 (upstream Levels 1 to 3 were dispensed with)
    - Level 2 is only exposed on `NOOPT` builds and outputs logs at a very detailed level
      * Create `NOOPT` builds by passing `ALL` as a second parameter to the RefindPlus build script
      * The first parameter is the build branch, which also needs to be specified in such instances
    - When Level 2 is not exposed, selected levels above `1` will be capped at Level 1
    - When exposed, selected levels above `2` will be capped at Level 2
- **"resolution" Token:** The `max` setting is redundant in RefindPlus which always defaults to the maximum available resolution whenever the resolution is not set or is otherwise not available.

## Divergence
Significant visible implementation differences vis-a-vis the upstream base are:
- **GZipped Loaders:** RefindPlus only provides stub support for handling GZipped loaders as this is largely only relevant for units on the ARM architecture. This stub support is only used for debug logging in RefindPlus and can be activated using the same `support_gzipped_loaders` configuration token as upstream.
- **Screenshots:** These are saved in the PNG format with a significantly smaller file size. Additionally, the file naming is slightly different and the files are always saved to the same ESP as the RefindPlus efi file.
- **UI Flags:** RefindPlus requires that any desired previously set `hideui` configuration token options are explicitly defined in supplementary/theme configuration files; as whenever the token is found in such files, the token setting is reset by RefindPlus to the specified option(s). This is consistent with how other configuration tokens in such files are handled. The upstream implementation effectively adds new settings to any previously existing ones for this configuration token instead.
- **UI Scaling:** WQHD monitors are correctly determined not to be HiDPI monitors and UI elements are not scaled up on such monitors when the RefindPlus-specific `scale_ui` configuration token is set to automatically detect the screen resolution. RefindPlus also takes vertically orientated screens into account and additionally scales UI elements down when low resolution screens (less than 1025px on the longest edge) are detected.
- **Loader Icons:** RefindPlus defaults to preferring generic icons for loaders ahead of the slower to load custom icons where possible. The upstream icon search implementation involves only loading such icons after a search for custom icons has not turned anything up. Users can activate the RefindPlus-specific `decline_help_icon` configuration token to use the upstream icon search implementation instead of the RefindPlus default.
- **GOP Driver Provision:** RefindPlus attempts to ensure that UEFI 2.x GOP drivers are available on EFI 1.x units by attempting to reload such drivers when it detects an absence of GOP on such units to permit the use of modern GPUs on legacy units. Users that wish to disable this feature can activate the RefindPlus-specific `disable_reload_gop` configuration token to switch it off.
- **Apple Framebuffer Provision:** RefindPlus defaults to always providing Apple framebuffers on Macs, when not available under certain circumstances. This is done using an inbuilt `SetAppleFB` feature. Users that wish to disable this feature can activate the RefindPlus-specific `disable_set_applefb` configuration token to switch it off.
- **APFS Filesystem Provision:** RefindPlus defaults to always providing APFS Filesystem capability, when not available but is required, without a need to load an APFS driver. This is done using an inbuilt `SupplyAPFS` feature. Users that wish to disable this feature can activate the RefindPlus-specific `disable_apfs_load` configuration token to switch it off.
- **APFS PreBoot Volumes:** RefindPlus always synchronises APFS System and PreBoot partitions transparently such that the Preboot partitions of APFS volumes are always used to boot APFS formatted macOS. Hence, a single option for booting macOS on APFS volumes is presented in RefindPlus to provide maximum APFS compatibility. Users that wish to disable this feature can activate the RefindPlus-specific `disable_apfs_sync` configuration token to switch it off.
- **Mac nvRAM Protection:** RefindPlus always prevents UEFI Windows Secure Boot from saving certificates to Mac nvRAM as this can result in damage and an inability to boot. Blocking these certificates does not impact the operation of UEFI Windows on Macs. This filtering only happens when Mac firmware is detected and is not applied to other types of firmware. Users that wish to disable this feature can activate the RefindPlus-specific `disable_nvram_protect` configuration token to switch it off.
- **Mac Legacy BIOS Boot:** RefindPlus originally assumed all Macs were capable of legacy BIOS boot based on code that went in upstream back in 2012 when this was a reasonable default. However, some later Intel Macs do not support legacy BIOS boot and RefindPlus now attempts to categorise Macs to enable/disable legacy boot accordingly. Users can activate the RefindPlus-specific `disable_legacy_sync` configuration token to base legacy BIOS boot availability on the old assumption.
- **Secondary Configuration Files:** While the upstream documentation prohibits including tertiary configuration files from secondary configuration files, there is no mechanism enforcing this prohibition. Hence, tertiary, quaternary, quinary, and more, configuration files can in fact be included. RefindPlus enforces a limitation to secondary configuration files.
- **Disabled Manual Stanzas:** The processing of a user configured boot stanza is halted, and the `Entry` object immediately discarded, once a `Disabled` setting is encountered. The outcome is the same as upstream, which always continues to create and return a fully built object in such cases to be discarded later. The approach adopted in RefindPlus allows for an optimised loading process particularly when such `Disabled` tokens are placed immediately after the `menuentry` line (see examples near the bottom of the `config.conf-sample-Dev` file). This also applies to `submenuentry` items which can be enabled or disabled separately.
- **Pointer Device Priority:** The upstream implementation of pointer device priority is based on how the `enable_mouse` and `enable_touch` pointer device control tokens appear in the configuration file(s) when both are active. The last pointer device control token read in the main configuration file and/or any supplementary/override configuration file will be used and the other disregarded. In RefindPlus however, `enable_touch` always takes priority when both tokens are active without regard to the order of appearance in the configuration file(s). This means that to use a mouse in RefindPlus, `enable_touch` must be disabled (default) in addition to enabling `enable_mouse`.

## Roll Your Own
Refer to [BUILDING.md](https://github.com/dakanji/RefindPlus/blob/GOPFix/BUILDING.md) for build instructions (x86_64 Only).

[CLICK HERE](https://github.com/dakanji/RefindPlus/blob/GOPFix/README-Dev.md) for the ReadMe File related to the current (work in progress) code base.
