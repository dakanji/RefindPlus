<div align="center">

# RefindPlus

A Boot Manager for Mac and PC

<br>

[![Release Version](https://img.shields.io/github/v/release/RefindPlusRepo/RefindPlus?style=for-the-badge&color=informational&label=current)](https://github.com/RefindPlusRepo/RefindPlus/releases)[![Release Date](https://img.shields.io/github/release-date/RefindPlusRepo/RefindPlus.svg?display_date=published_at&style=for-the-badge&color=informational&label=)](https://github.com/RefindPlusRepo/RefindPlus/releases)

[![Coverity Scan](https://scan.coverity.com/projects/22695/badge.svg?flat=1)](https://scan.coverity.com/projects/22695)&nbsp;&nbsp;&nbsp;[![Codacy Grade](https://img.shields.io/codacy/grade/3d486c33f276471cbe95735bd28ea3e9?label=codacy)](https://app.codacy.com/gh/RefindPlusRepo/RefindPlus/dashboard)

[![License Type](https://img.shields.io/badge/GPL%203.0/Later-blue?label=copies)](https://github.com/RefindPlusRepo/RefindPlus/blob/GOPFix/INFO.txt)

</div>

<br><br>

## Overview

RefindPlus is a fork of _`rEFInd`_ that provides extended functionality via enhancements and fixes that include several Apple Mac and UEFI-PC related items that may be of interest to anyone requiring a boot manager for Mac and PC.

RefindPlus is particularly useful for those with additional configuration needs or that require advanced or otherwise non-standard (hence typically unavailable) options for running operating systems and uEFI utilities on Mac and PC.

> `RefindPlus` is a case-sensitive single word.
>
> > Standard: **RefindPlus** \
> > Lowercase: **refindplus** \
> > Uppercase: **REFINDPLUS**
>
> It _MUST NOT_ be split or mutated. \
> It _MAY_ be abbreviated as '**RP**' or '**rp**'.

Some RefindPlus Features:
- Maintains feature and configuration parity with `Upstream v0.14.2` base.
- Provides options to [tag faulty RAM regions](https://github.com/RefindPlusRepo/RefindPlus/blob/GOPFix/BADRAM.md) as `unusable` to extend device life.
- Provides protection against damage to vulnerable Mac nvRAM by UEFI Windows.
- Emulates UEFI 2.x on EFI 1.x units to permit running UEFI 2.x utilities on such units.
- Provides mitigation against boot failures and related issues on T2/TPM chipped units.
- Provides Pre-Boot Configuration Screen on Apple Macs running GPUs without native EFI.
- Extensive memory management improvements with associated speed and stability gains.
- Provides improved text display support for languages that require unicode text.
- Identifies and automatically handles `Ventoy` instances if present.
  - Rationalises binaries displayed.
  - Displays an `os_ventoy` icon if available.
- Includes troubleshooting (`DBG`/`NPT`) binaries for debugging.
  - The standard release (`REL`) binary is for day to day use.
- Fixes inability to print to screen on some Macs.
  - This prevented receiving program messages or using utilities such as uEFI shell.
- Provides NVMe capability, if required, via an inbuilt `NvmExpress` driver.
  - Removes the need to load external drivers on units without native NVMe support.
  - Basically allows working as if NVMe is natively supported by the firmware.
- Provides APFS filesystem capability, if required, via an inbuilt `APFS JumpStart` driver.
  - Removes the need to load external drivers on units without native APFS support.
  - Additionally ensures matching APFS drivers for specific Mac OS versions are used.
  - Basically allows working as if APFS is natively supported by the firmware.
- Fully supports APFS filesystem requirements.
  - This allows booting recent Mac OS versions from single named volumes.
    - As opposed to generic and difficult to distinguish `PreBoot` volumes.
    - Avoids compromising system integrity by otherwise requiring SIP to be disabled.
  - This also allows booting `FileVault` encrypted volumes from single named volumes.
    - As opposed to generic and difficult to distinguish `PreBoot` volumes.

## Installation

A simple and direct way is to manually make the RefindPlus efi file a `UEFI Fallback File` by naming it accordingly, `BOOTx64.efi`, and placing this in the `UEFI Fallback Path` of a disk, `/EFI/BOOT`. The configuration file should be placed next to the RefindPlus efi file along with optional `drivers`, `tools`, and/or `icons` folders as required.

> Only `x86_64` builds of RefindPlus are currently distributed and supported.
>
> External ports can be slower than internal by an order of magnitude. \
> HDD disks are slower than SSD/NVMe drives by an order of magnitude. \
> Loading RefindPlus from external ports or HDD disks results in longer start times. \
> Booting with external ports or HDD disks active typically results in longer start times.

[MyBootMgr](https://www.dakanji.com/creations/index.html) is recommended to automate installing RefindPlus when running Mac OS on Intel-based Macs. Alternatively, as the RefindPlus efi file can function as a drop-in replacement for the upstream efi file, [rEFInd](https://www.rodsbooks.com/refind/installing.html) can be installed first and its efi file replaced with the RefindPlus efi file (rename RefindPlus file to match). This allows installing RefindPlus on other compatible operating systems supported upstream. See the [Divergence](https://github.com/RefindPlusRepo/RefindPlus#divergence) section for how to enable `UEFI Secure Boot` (if required).

> [!NOTE]
>
> MyBootMgr can optionally also be used to create a flexible and powerful `RefindPlus|OpenCore` chain-loading arrangement for MacPro3,1 to MacPro5,1 as well as for Xserve2,1 and Xserve3,1.

Upstream post-release code updates are typically ported to RefindPlus as they happen and as such, the RefindPlus code base is usually at the state of the base upstream release version plus any such updates. The code base typically also includes updates for subsequent upstream release versions since the base version.

> [!TIP]
>
> Consider replacing upstream filesystem drivers with those packaged with RefindPlus as these are always either exactly the same as upstream versions or have had fixes applied.

RefindPlus will function with the upstream configuration file, `refind.conf`, but users may wish to replace this with the RefindPlus configuration file, `config.conf`, to configure the additional options provided by RefindPlus. A sample RefindPlus configuration file is available here: [config.conf-sample](https://github.com/RefindPlusRepo/RefindPlus/blob/GOPFix/config.conf-sample).

> [!TIP]
>
> RefindPlus-specific options can also simply be added to upstream configuration files.

When run without activating RefindPlus-specific configuration options, as will be the case with unmodified upstream configuration files, a RefindPlus run will be equivalent to running the upstream version it is based on. That is, the additional options provided in RefindPlus must be actively enabled if they are required.

> [!NOTE]
>
> This equivalence is subject to some differences such as outlined under the [Divergence Section](https://github.com/RefindPlusRepo/RefindPlus#divergence) below.

## Additional Functionality

RefindPlus-specific funtionality can be configured using the tokens below. \
Additional information is provided in the sample RefindPlus configuration file. \
These tokens are included in `Section 1` of the sample RefindPlus configuration file.

Token | Functionality
----- | -----
badram_tag_list        |Allows providing a list of faulty memory regions to be marked as `unusable`
badram_tag_mode        |Controls whether and how faulty memory regions are managed by the program
badram_tag_wide        |Allows first trying a simpler, and faster, process for some tag modes
continue_on_warning    |Proceed as if a key was pressed after screen warnings (for unattended boot)
csr_dynamic            |Actively sets or unsets Apple's `Configurable Security Restrictions (CSR)`
csr_normalise          |Removes the `APPLE_INTERNAL` CSR bit, when present, to permit OTA updates
decline_help_icon      |Disables feature that may improve loading speed by preferring generic icons
decline_help_size      |Disables feature that sets additional UI scaling for very high DPI screens
decline_help_text      |Disables feature that sets screen text to complementary colours
decouple_key_f10       |Unmaps the `F10` key from native screenshots (the `\` key remains mapped)
disable_apfs_load      |Disables inbuilt provision of APFS filesystem capability
disable_apfs_sync      |Disables feature allowing direct APFS/FileVault boot (without "PreBoot")
disable_check_amfi     |Disables AMFI checks on Mac OS
disable_check_compat   |Disables Mac OS version compatibility checks
disable_exitlogo_clear |Disables clearing displayed exit logo images on exit screens
disable_exitlogo_image |Disables display of exit logo images on exit screens
disable_exitlogo_scale |Disables scaling displayed exit logo images on exit screens
disable_pass_gop_thru  |Disables feature that provides GOP instance on UGA for some loading screens
disable_legacy_sync    |Disables detailed indentification of Mac legacy BIOS boot capability
disable_nvram_paniclog |Disables logging Mac OS kernel panics to nvRAM
disable_nvram_protect  |Disables blocking of potentially harmful write attempts to Legacy Mac nvRAM
disable_reload_gop     |Disables UEFI 2.x GOP OptionROM activation fix for EFI 1.x units
disable_rescan_dxe     |Disables scanning for newly revealed DXE drivers when connecting handles
disable_set_applefb    |Disables conditional provision of missing Apple framebuffers on Macs
disable_set_consolegop |Disables feature that fixes some issues with GOP graphics on legacy units
enable_esp_filter      |Prevents other ESPs other than the RefindPlus ESP being scanned for loaders
force_trim             |Allows forcing `TRIM` on Third-Party SSDs on Macs
hidden_icons_external  |Allows scanning for `.VolumeIcon` icons on external volumes
hidden_icons_ignore    |Disables scanning for `.VolumeIcon` image icons if not required
hidden_icons_prefer    |Prioritises `.VolumeIcon` and `.VolumeBadge` image icons when available
icon_row_move          |Repositions the main screen icon rows (vertically)
icon_row_tune          |Fine tunes the resulting `icon_row_move` outcome
mitigate_primed_buffer |Allows enhanced intervention to handle apparent primed keystroke buffers
nvram_protect_ex       |Extends `NvramProtect`, if set, to Mac OS and `unknown` UEFI boots
nvram_variable_limit   |Limits nvRAM write attempts to the specified variable size
pass_uga_through       |Provides UGA instance on GOP to permit EFI Boot with modern GPUs
persist_boot_args      |Overrides using vRAM (instead of nvRAM) for Mac OS boot argument items
prefer_uga             |Prefers UGA use (when available) regardless of GOP availability
ransom_drives          |Frees partitions locked by how certain firmware load inbuilt drivers
renderer_direct_gop    |Provides a potentially improved GOP instance for certain GPUs
renderer_text          |Provides a text renderer for text output when otherwise unavailable
scale_ui               |Provides control of UI element scaling
screen_rgb             |Allows setting arbitrary screen background colours
set_boot_args          |Allows setting arbitrary Mac OS boot arguments
supply_nvme            |Enables an inbuilt NvmExpress driver
supply_uefi            |Enables feature that emulates UEFI 2.x support on EFI 1.x units
sync_nvram             |Resets nvRAM settings, such as BlueTooth, on some boot types if required
sync_trust             |Works around some `Boot Chain of Trust` issues with T2/TPM chipped units
transient_boot         |Disables feature that selects the last booted loader by default
unicode_collation      |Provides fine tuned support for languages that require unicode text

## Modified Functionality

In addition to the new functionality listed above, the following upstream tokens have been modified:
- **"timeout":** The RefindPlus default is no timeout unless explicitly set.
- **"use_nvram":** Variable storage is on the filesystem, not the nvRAM chip, unless explicitly set.
- **"use_graphics_for":** Additional options added:
  - `tools` option to _enable_ graphics mode loading for such.
  - `none` option to _disable_ graphics mode loading for everything.
  - `everything` option to _enable_ graphics mode loading for everything.
  - `SystemD`, `OpenCore`, and `Clover` can be set to load in graphics mode.
- **"showtools":** Default setting changed and additional tool added:
  - `clean_nvram` : Allows resetting nvRAM directly from RefindPlus.
    - When run on Apple firmware, RefindPlus will additionally trigger nvRAM garbage collection.
- **menuentry:** Additional `OSType` options added for manual stanzas:
  - `RefitVariant`, `SystemD`, `OpenCore`, and `Clover` can be additionally defined.
- **"follow_symlinks":** Accepts optional additional parameters:
    - `follow_symlinks`: Symlinks always followed.
    - `follow_symlinks ON`: Symlinks always followed.
    - `follow_symlinks OFF`: Symlinks never followed.
    - `follow_symlinks OFF "Vol_1,Vol_2"`: Symlinks followed unless on list.
    - `follow_symlinks ON "Vol_9,Vol_10"`: Symlinks followed only if listed.
- **"csr_values":** A value of `0` can be set as the `Enabled` value to allow `Over The Air (OTA)` updates on Mac OS 11.x (Big Sur) or newer with SIP enabled.
  - This is equivalent to activating the `csr_normalise` token.
- **"log_level":** Controls the native log format and an implementation of the upstream format.
  - Levels 0, 1, or 2 can be specified.
    - Level 0 activates the succinct native log format.
    - Level 1 is broadly equivalent to the verbose upstream Level 4 format
      - Upstream Levels 1 to 3 were dispensed with
    - Level 2 is only exposed on `NOOPT` builds and outputs logs at a very detailed level
      - The RefindPlus build script will create `NOOPT` builds when passed `ALL` or `NPT` via the `--build-type=XYZ` parameter
        - Setting `ALL` adds an `NPT` file to the standard `REL` and `DBG` files created
        - Setting `NPT` creates only that build file
          - Applies to only setting `REL` or `DBG`
        - Pass `--help` to the build script for guidance on options
    - When Level 2 is not exposed, selected levels above `1` will be capped at Level 1
    - When exposed, selected levels above `2` will be capped at Level 2
  - Logging is never active on `REL` files (for day-to-day use).
  - Logging is always active on `DBG` and `NPT` files (for trouble shooting).
- **"resolution":** The `max` setting is redundant in RefindPlus, which always defaults to the maximum available resolution when a resolution is not specified or is not available for any other reason.
- **"screensaver":** The screensaver cycles through a set of colours as opposed to a single grey colour.

## Divergence

Significant visible implementation differences vis-a-vis the upstream base are:
- **UEFI Secure Boot:** RefindPlus binaries as from v0.14.2.AD include the `Secure Boot Advanced Targeting (SBAT)` section required by Shim v15.3/newer for secure boot support but require users to self-sign the binaries and to self-enrol the associated certificate.
  - > The process [outlined upstream](https://www.rodsbooks.com/refind/secureboot.html#installation) for self-signing can be followed to enable support.
  - > An adaptation of the process for RefindPlus is [provided here](https://github.com/RefindPlusRepo/RefindPlus/discussions/190#discussioncomment-10130431). Modify for newer releases as required.
  - > Refer to [this summation](https://forum.manjaro.org/t/howto-enable-secure-boot-with-refind/121403/6) for futher insight.
- **GZipped Loaders:** RefindPlus only provides stub support for handling GZipped loaders as this is largely only relevant for units with ARM chips.
  - > This stub support is only used for debug logging in RefindPlus and can be activated using the same `support_gzipped_loaders` setting as upstream.
- **Screenshots:** These are saved in the PNG format with a significantly smaller file size.
  - > Additionally, the file naming is different and files are always saved to the same ESP as RefindPlus.
- **UI Flags:** RefindPlus requires that any desired previously set `hideui` setting options are explicitly defined in supplementary/theme configuration files; as whenever the token is found in such files, the token setting is reset by RefindPlus to the specified option(s). The upstream implementation effectively adds new settings to any previously existing ones for this token instead.
  - > RefindPlus maintains consistency with how other tokens are handled.
- **UI Scaling:** WQHD monitors are correctly determined not to be HiDPI monitors and UI elements are not scaled up on such monitors when the RefindPlus-specific `scale_ui` setting is set to automatically detect the screen resolution. RefindPlus also scales UI elements down when low resolution screens (less than 1025px on the longest edge) are detected.
  - > Additionally, UI elements on extremely high resolution screens (greater than 5999px on the longest edge) receive a `4X scaling` as opposed to the `2X scaling` applied for standard HiDPI screens.
- **Loader Icons:** RefindPlus prefers `os_windows`/`boot_windows` icon files, if present, over `os_win`/`boot_win` files (and `win8` variants). Separately, RefindPlus prefers generic OS icons over custom icons by default (improves load speed). The upstream icon search implementation involves loading generic OS icons only if a search for custom icons has returned empty.
  - > Activate the RefindPlus-specific `decline_help_icon` setting to keep the upstream implementation.
- **GOP OptionROM Provision:** RefindPlus attempts to ensure that GOP is available, to permit using modern GPUs on on EFI 1.x units, by amending the `UEFI System Table` and reloading the GOP OptionROM from the GPU. This is via an inbuilt `ReloadGOP` feature.
  - > Activate the RefindPlus-specific `disable_reload_gop` setting to switch this feature off.
- **Apple Framebuffer Provision:** RefindPlus defaults to always providing Apple framebuffers on Macs, when not available under certain circumstances. This is via an inbuilt `SetAppleFB` feature.
  - > Activate the RefindPlus-specific `disable_set_applefb` setting to switch this feature off.
- **APFS Filesystem Provision:** RefindPlus defaults to always providing APFS filesystem capability, when not available but is required, without a need to load an APFS driver. This is via an inbuilt `SupplyAPFS` feature.
  - > Activate the RefindPlus-specific `disable_apfs_load` setting to switch this feature off.
- **APFS PreBoot Volumes:** RefindPlus always synchronises APFS System and PreBoot partitions transparently such that the Preboot partitions of APFS volumes are always used to boot APFS formatted Mac OS. Hence, a single option for booting Mac OS on APFS volumes is presented in RefindPlus to provide maximum APFS compatibility. This is via an inbuilt `SyncAPFS` feature.
  - > Activate the RefindPlus-specific `disable_apfs_sync` setting to switch this feature off.
- **Mac nvRAM Protection:** RefindPlus always prevents UEFI Windows from saving certificates to Mac nvRAM as this can result in damage and, ultimately, an inability to boot anything on some Macs (typically Pre 2013 Vintage). Blocking these certificates does not impact the operation of UEFI Windows on such Macs. This filtering only happens when Apple firmware is detected and is not applied to other types of firmware. This is via an inbuilt `ProtectNVRAM` feature.
  - > Activate the RefindPlus-specific `disable_nvram_protect` setting to switch this feature off.
- **Mac Legacy BIOS Boot:** RefindPlus originally assumed all Macs were capable of legacy BIOS boot based on code that went in upstream back in 2012 when this was a reasonable default. However, some later Intel Macs do not support legacy BIOS boot and RefindPlus now attempts to categorise Macs to enable/disable legacy BIOS boot accordingly.
  - > Activate the RefindPlus-specific `disable_legacy_sync` setting to keep the old assumption.
- **Secondary Configuration Files:** While the upstream documentation prohibits including tertiary configuration files from secondary configuration files, there is no apparent mechanism enforcing this prohibition. Hence, tertiary and more configuration files can in fact be included.
  - > RefindPlus enforces the limitation for inclusion to secondary configuration files.
- **Shortcut Keys:** RefindPlus does not allocate shortcut keys based on the operating system type/name as there is no way of knowing what would actually be loaded in many cases.
  - > Keys are allocated based on display position in the order of `Key 1` to `Key Z`.
  - > Alphabetic `Keys I and O` are not used, while Numeric `Key 0` is reserved for internal use.
  - > Keys are not allocated to `Tools` apart from `Key A` for `About Refindplus` and `Key Z` for `System Shutdown`.
- **Disabled Manual Stanzas:** The processing of a user configured boot stanza is halted, and the `Entry` object immediately discarded, once a `Disabled` setting is encountered. The outcome is the same as upstream, which always continues to create and return a fully built object that is later discarded in such cases. The approach adopted in RefindPlus allows for an optimised loading process particularly when such `Disabled` tokens are placed immediately after the `menuentry` line (see examples near the bottom of the `config.conf-sample` file).
  - > This also applies to `submenuentry` items which can be enabled or disabled separately.
- **Pointer Device Priority:** The upstream implementation of pointer device priority is based on how the `enable_mouse` and `enable_touch` pointer device control tokens appear in the configuration file(s) when both are active. The last pointer device control token read in the main configuration file and/or any supplementary/override configuration file will be used and the other disregarded. In RefindPlus however, `enable_touch` always takes priority when both tokens are active without regard to the order of appearance in the configuration file(s).
  - > Keep `enable_touch` disabled, in addition to setting `enable_mouse`, to use a mouse in RefindPlus.

## Roll Your Own

Refer to [BUILDING.md](https://github.com/RefindPlusRepo/RefindPlus/blob/GOPFix/BUILDING.md) for build instructions.

[CLICK HERE](https://github.com/RefindPlusRepo/RefindPlus/blob/GOPFix/README.md) for the ReadMe file related to the current (work in progress) code base.
