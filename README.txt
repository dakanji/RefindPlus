Brief Installation Instructions (Binary Package)
================================================

This is rEFInd, an EFI boot manager. The binary package includes the
following files and subdirectories:

   File                             Description
   -----------------------------    -----------------------------
   refind/refind_ia32.efi           The main IA32 rEFInd binary
   refind/refind_x64.efi            The main x86-64 rEFInd binary
   refind/refind.conf-sample        A sample configuration file
   refind/icons/                    Subdirectory containing icons
   refind/drivers_ia32/             Subdirectory containing IA32 drivers
   refind/drivers_x64/              Subdirectory containing x86-64 drivers
   keys/                            Subdirectory containing MOKs
   refind-install                   Linux/MacOS installation script
   refind-mkdefault                 Script to make rEFInd the default
   mkrlconf                         A script to create refind_linux.conf
   mvrefind                         A script to move a rEFInd installation
   README.txt                       This file
   NEWS.txt                         A summary of program changes
   LICENSE.txt                      The original rEFIt license
   COPYING.txt                      The rEFInd license
   CREDITS.txt                      Acknowledgments of code sources
   docs/                            Documentation in HTML format

The easiest way of installing rEFInd is generally to use the refind-install
script; however, you must be running under Linux or OS X to do this. If
you're using either of those OSes, simply typing "./refind-install" will
generally install rEFInd. If you have problems with this method, though,
you'll have to do a manual installation. The refind-install script supports
a number of options that you might want to use; consult the
docs/refind/installing.html file for details.

To install the binary package manually, you must first access your EFI
System Partition (ESP). You can then place the files from the refind
subdirectory in a subdirectory of the ESP's EFI directory. You may omit the
.efi binary for the type of computer you're NOT using, and you may
optionally rename the .efi file for the binary you are using. If this is an
initial installation, you should rename refind.conf-sample to refind.conf;
but if you're replacing an existing installation, you should leave your
existing refind.conf intact. The end result might include the following
files on the ESP:

 EFI/refind/refind_x64.efi
 EFI/refind/refind.conf
 EFI/refind/icons/

Unfortunately, dropping the files in the ESP is not sufficient; as
described in the docs/refind/installing.html file, you must also tell your
EFI about rEFInd. Precisely how to do this varies with your OS or, if you
choose to do it through the EFI, your EFI implementation. In some cases you
may need to rename the EFI/refind directory as EFI/boot, and rename
refind_x86.efi to bootx64.efi (or refind_ia32.efi to bootia32.efi on 32-bit
systems). Consult the installing.html file for full details.

If you want to use any of the filesystem drivers, you must install them,
too. Creating a subdirectory of the rEFInd binary directory called
drivers_x64 (for x86-64 systems), drivers_ia32 (for x86 systems), or
drivers (for any architecture) and copying the drivers you want to this
location should do the trick. When you next launch it, rEFInd should load
the drivers, giving you access to the relevant filesystems.

Brief Installation Instructions (Source Package)
================================================

rEFInd source code can be obtained from
https://sourceforge.net/projects/refind/. Consult the BUILDING.txt file in
the source code package for build instructions. Once  you've built the
source code, you can use the refind-install script to install the binaries
you've built. Alternatively, you can duplicate the directory tree described
above by copying the individual files and the icons directory to the ESP.
