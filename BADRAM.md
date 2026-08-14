<div align="center">

# The BadRamTag Feature

This document describes the implementation and configuration of the memory defect handling system in RefindPlus called `BadRamTag`.

The feature allows masking faulty memory regions to prevent them being used by the EFI environment or subsequent operating systems.

</div>

<br><br>

<table>
 <tr>
   <td style="background-color: LightYellow;">
       <div>
           <h2>Table of Contents</h2>
           <ul>
               <li><a href="#configuration-settings">CONFIGURATION SETTINGS</a></li>
               <li><a href="#technical-constraints">TECHNICAL CONSTRIANTS</a></li>
               <li><a href="#tagging-strategies">TAGGING STRATEGIES</a></li>
               <li><a href="#address-entries">ADDRESS ENTRIES</a></li>
               <li><a href="#troubleshooting">TROUBLESHOOTING</a></li>
           </ul>
       </div>
   </td>
 </tr>
</table>

<br><br>

---

<br><br>

## Configuration Settings

The following tokens in the RefindPlus configuration file control `BadRamTag` behaviour:

### _`badram_tag_list`_
A comma-separated list of start and end Bad RAM address pairs.
* **Format:** `0xStart:0xEnd,0xStart:0xEnd,...,0xStart:0xEnd,0xStart:0xEnd`
* **Example:** `badram_tag_list 0x12345000:0x12346000,0x55000000:0x55001000`

### _`badram_tag_mode`_
Defines the strategy used to tag memory regions.<br/>
Valid values are `0` through `9`.<br/>
However, `-1` can be used to clear a cached auto-scan.

### _`badram_tag_wide`_
When active, the default, RefindPlus will essentially first try the `Mode 1` protocols before `Modes 2to9`.<br/>
When inactive, RefindPlus will directly run the slower `Modes 2to9` protocols from `badram_tag_mode`.<br/>
Attempting the faster `Mode 1` protocols first allows skipping the `Modes 2to9` ones if successful.

> [!CAUTION]
>
> The default `badram_tag_wide` setting is currently `0 (OFF)`. \
> The setting _MUST_ therefore be explicitly activated if wanted. \
> The setting _IS NOT_ active by default as intended/documented. \
> The default has been fixed for the next version of `RefindPlus`.

<br><br>

---

<br><br>

## Technical Constraints

The following limits are enforced for system stability:

* **Max Ranges:** A maximum of `20` defective regions can be specified.
* **Size Limit:** No single defective region can exceed `1.0GB` (0x040000 pages).
* **Automation Limit:** Only `EfiConventionalMemory` type handling can be automated.
* **Max Address:** Ignores addresses beyond the 64-bit page-aligned ceiling (`0xfffffffffffff000`).
* **Address 0:** Never tagged, or even probed, as holds the 'Real Mode' `Interrupt Vector Table (IVT)` on x86.

> The size limit is raised to `4.0GB` when `badram_tag_mode` is set to `1`. \
> `4.0GB` is allowed for initial `Mode 1` attempts on other modes with `badram_tag_wide` active. \
> If an initial `Mode 1` attempt fails, the original mode's `1.0GB` limit applies to the fallback stage.

<br><br>

---

<br><br>

## Tagging Strategies

There are various levels of granularity available.

| Mode  | Source | Strategy          | Handling of 'In-Use' Pages                                                          |
| :---  | :---   | :---              | :---                                                                                |
| **1** | Manual | Forces Allocation | Allocates `EfiRuntimeServicesData` pages over defined regions.                      |
| **2** | Manual | Forces `Unusable` | Tags defined regions (regardless of usage) as: `EfiUnusableMemory`.                 |
| **3** | Manual | Forces `Reserved` | Tags defined regions (regardless of usage) as: `EfiReservedMemoryType`.             |
| **4** | Manual | Conservative      | **Region-Level:** Tags regions as `EfiUnusableMemory` if no pages are in use.       |
| **5** | Manual | Conservative      | **Region-Level:** As with `Mode 4` but tags in-use regions as `Reserved` instead.   |
| **6** | Manual | Hybrid            | **Page-Level:** Tags free pages as `Unusable` and skips pages in use individually.  |
| **7** | Manual | Hybrid            | **Page-Level:** Tags free pages as `Unusable` and tags in-use pages as `Reserved`.  |
| **8** | Auto   | Hybrid            | **Page-Level:** As with `Mode 6` with regions identified by automated memory probe. |
| **9** | Auto   | Hybrid            | **Page-Level:** As with `Mode 7` with regions identified by automated memory probe. |

<br><br>

---

<br><br>

## Address Entries

### Automated Scans

Automated scans (`Modes 8/9`) can probe `EfiConventionalMemory` for faults.<br/>
As scanning large amounts of RAM can be slow, RefindPlus caches the output.

* **Cache Storage:** Results are stored in the `BadRamTag` nvRAM variable.
  - As a `RefindPlus-specific` variable.
* **Cache States:**
  - `ACE`: Indicates automated scan ended successfully.
  - `ERR`: Indicates automated scan met with an error.
  - `XXX`: Indicates automated scan returned empty.

When the cache is error-tagged, RefindPlus will return ***Not Supported*** on subsequent boots.<br/>
This persists until the first-scan error is resolved, the current cache is cleared, and a rescan done.<br/>
Clearing the cache and scanning without resolving the error will return a cache with the previous state.

While the automated scan modes do not cover the same range and depth as dedicated test tools,<br/>
users may still wish to consider these modes for relative speed, RefindPlus compatibilty, basic adequacy<br/>
reasons. Mainly because MemTest output may often need conversion before use as RefindPlus address ranges.

### Dedicated Tools

Tools such as MemTest can be run to carry out exhaustive tests and subsequently output details on faulty memory.<br/>
A FOSS MemTest variant, `MemTest86+`, has the following candidate output types that can be considered as source.

See: https://memtest.org/readme#error-reporting

#### 1. Error Summary

This simply provides the first and the last bad RAM addresses, which can simply be passed to RefindPlus<br/>
if the size of the range is within the defined limits. The range can be split if above defined limits.

> [!CAUTION]
>
> The `Error Summary` range will also include Good RAM in most cases. \
> The larger the size range, the likelier it also includes Good RAM.

#### 2. Misc Others

The `BadRAM Patterns`/`Linux memmap`/`Bad Pages` and similar output types provide ranges that exclude or limit Good RAM.<br/>
These outputs could potentially be passed to `LLM Bots` for conversion to a comma delimited list of start/end addresses.

<br><br>

---

<br><br>

## Troubleshooting

### <ins>Status Report</ins>

The debug log reports the status of the `BadRamTag` operation:

| EFI Status        | Description                                                |
| :---              | :---                                                       |
| Not Started       | Was not configured to tag BadRAM regions.                  |
| Success           | All relevant memory regions successfully tagged.           |
| Not Ready         | Failed to find issues in `Mode 1to7` BadRAM regions.       |
| Already Started   | `Mode 8/9` automated memory scan did not find issues.      |
| Not Supported     | Error state flag in `cached automated memory` scan found.  |
| Access Denied     | `Mode 4/6/8` tag attempt aborted as all ranges are in use. |
| Invalid Parameter | `badram_tag_list` formatting error or `Address 0` attempt. |
| Bad Buffer Size   | Found or defined BadRAM region exceeds defined size limit. |
| Device Error      | Could not allocate memory space for the system memory map. |
| Out of Resources  | Could not allocate memory space for internal buffer needs. |

> RefindPlus will display an on-screen message on boot when errors are encountered. \
> However, `Not Ready`/`Already Started` do not trigger the on-screen message. \
> This on-screen message is shown by all build types (`REL`, `DBG`, and `NPT`).

### <ins>Some Issues and Fixes</ins>

#### ***`Automated Scan "Stuck"`***

**Symptoms**: Status shows `Not Supported` on every boot.<br/>
**Cause**: A previous scan hit an error and tagged the cache.<br/>
**Fix**: Resolve the error that appeared on first scan and clear the cache.

See below for how to clear the cache.
  - Edit `config.conf` and set `badram_tag_mode -1`.
    - Boot once to clear the nvRAM.
    - Revert `badram_tag_mode` to `8`/`9`
	- Reboot RefindPlus to trigger rescan
  - Alternatively, run the `CleanNvram` tool.
    - A rescan will be triggered on reboot into RefindPlus.

> You might need to clear the cache and rescan first if the original error has been forgotten.

#### ***`Manual List "Ignored"`***

**Symptoms**: Debug log always shows `Invalid Parameter`.<br/>
**Cause**: Typically errors in `badram_tag_list`.<br/>
**Fix**: Ensure the list entries look similar to:
  - `badram_tag_list 0x12345000:0x12349000,0x55000000:0x55001000`.

#### ***`Always Apparently "Not Ready"`***

**Symptoms**: Debug log always shows `Not Ready`.<br/>
**Cause**: Typically an unsuitable `badram_tag_mode`.<br/>
**Fix**: Consider a different tag mode.
  - Assumes correctly determined ranges in `badram_tag_list`.
  - Otherwise, disable `BadRamTag` by setting `Mode 0`
