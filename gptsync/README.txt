This directory contains the source code for gptsync, which is a program for
creating hybrid MBRs (http://www.rodsbooks.com/gdisk/hybrid.html).

HYBRID MBRS ARE UGLY AND DANGEROUS HACKS, AND SHOULD NOT BE USED UNLESS
ABSOLUTELY NECESSARY!

Despite their dangers, hybrid MBRs are useful because Windows interprets
hybrid MBR disks as having an MBR partition table, whereas OS X and Linux
interpret such disks as having a GUID partition table (GPT). Since Windows
ties its boot mode to the firmware type (MBR/BIOS and GPT/EFI), a hybrid
MBR enables Windows to boot in BIOS mode from a disk that's primarily a GPT
disk, such as a Macintosh OS X disk.

Unfortunately, Apple uses hybrid MBRs as part of its workaround to enable
Macs to boot Windows in BIOS mode while also supporting a standard EFI-mode
boot of OS X. Many Linux distributions also install in BIOS mode on Macs,
and so use hybrid MBRs; but it's usually possible to add an EFI-mode boot
loader to get Macs to boot Linux in EFI mode, thus obviating the need for a
hybrid MBR. Some Hackintosh installations rely on a hybrid MBR for reasons
similar to those of OS X on a real Mac. Thus, you should use a hybrid MBR
*ONLY* on a Mac that dual-boots with Windows or some other OS in BIOS mode
or in very rare circumstances on other computers.

The version of gptsync provided with rEFInd is heavily modified from the
original rEFIt version of the program. Most notably, it's "smarter" about
creating a hybrid MBR: It prioritizes placement of Windows (FAT and NTFS)
partitions in the MBR side, followed by Linux partitions. Other partitions,
such as OS X's HFS+ partitions, might not appear at all in the hybrid MBR,
whereas they generally do appear in hybrid MBRs created by rEFIt's version
of gptsync. In the rEFIt version of gptsync, OS X partitions can crowd out
FAT or NTFS partitions, particularly on computers with shared FAT or NTFS
partitions, multiple Windows installations, or triple-boots with OS X,
Windows, and Linux. The rEFInd version of gptsync also checks the
firmware's author and warns if you're trying to run the program on anything
but Apple firmware, since in most such cases creating a hybrid MBR is *NOT*
desirable.

Although the Makefile supports building for both EFI (via the "gnuefi" and
"tiano" targets) and Unix/Linux (via the "unix" target), the Unix build is
currently broken; it returns a bogus error about an unknown GPT spec
revision. If you want to create a hybrid MBR in an OS, you're better off
using gdisk (http://www.rodsbooks.com/gdisk/), which provides much better
control of the hybrid MBR creation process. gdisk may also be preferable if
you have an unusual partition layout, many partitions, or specific
requirements that you understand well.
