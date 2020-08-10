# Refind-GOPFix
## Overview
rEFInd Boot Manager with focus on fixes to provide Pre-Boot Configuration Screens (Boot Screens) on units running Custom GPUs (GPUs without Mac EFI).

The development focus is on Classic MacPros (3,1 to 5,1) but the features should be useful for all.

## Other Features
- Fixes inability of rEFInd to print to screen on Macs which prevents receiving program messages as well as leveraging advanced features such as running EFI Shell.
- Adds a debug version that provides extensive logging.
- Allows disabling SIP to a high level
  - The new default level is `877` compared to the previous `77`
  - Values such as `977`, or the maximum `FFF`, are possible to allow unsigned DMG packages needed to run Mac OS v11.x (Big Sur) in unsupported environments.
- Provides UGADraw to permit booting legacy operating systems using EFIBoot.

## Installation
The Refind-GOPFix efi file is a drop-in replacement (x64 Only) for the default rEFInd efi file. Hence, to install, get the [default rEFInd package](www.rodsbooks.com/refind/getting.html) and [install this](www.rodsbooks.com/refind/installing.html) as normal.

Once done, replace the rEFInd efi file with one from Refind-GOPFix. Ensure that you rename to match. Also replace the default rEFInd configuration file with one from Refind-GOPFix to configure the additonal options provided.

Alternatively, you can use [MyBootMgr](https://forums.macrumors.com/threads/thread.2231693), a preconfigured rEFInd-GOPFix/OpenCore chainloading package.

## Roll Your Own
Refer to [BUILDING.md](https://github.com/dakanji/Refind-GOPFix/blob/GOPFix/BUILDING.md) for build instructions (x64 Only).
