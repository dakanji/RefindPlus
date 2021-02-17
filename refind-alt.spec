Name: refind
Version: 0.6.4
Release: alt1

Summary: EFI boot manager software
License: GPLv3
Group: System/Base

Url: http://www.rodsbooks.com/refind/
Source0: refind-src-%version.zip
Source1: os_altlinux.icns
Packager: Michael Shigorin <mike@altlinux.org>

BuildRequires: gnu-efi unzip
BuildRequires: rpm-macros-uefi sbsigntools alt-uefi-keys-private
Requires: efibootmgr
Provides: refind-signed

%define refind_lib %_efi_bindir
%define refind_data %_datadir/%name

%ifarch x86_64
%define _efi_arch x64
%endif
%ifarch %ix86
%define _efi_arch ia32
%endif

%description
A graphical boot manager for EFI- and UEFI-based computers, such as all
Intel-based Macs and recent (most 2011 and later) PCs. rEFInd presents a
boot menu showing all the EFI boot loaders on the EFI-accessible
partitions, and optionally BIOS-bootable partitions on Macs. EFI-compatbile
OSes, including Linux, provide boot loaders that rEFInd can detect and
launch. rEFInd can launch Linux EFI boot loaders such as ELILO, GRUB
Legacy, GRUB 2, and 3.3.0 and later kernels with EFI stub support. EFI
filesystem drivers for ext2/3/4fs, ReiserFS, HFS+, and ISO-9660 enable
rEFInd to read boot loaders from these filesystems, too. rEFInd's ability
to detect boot loaders at runtime makes it very easy to use, particularly
when paired with Linux kernels that provide EFI stub support.

%prep
%setup

%build
make gnuefi
make fs_gnuefi

%install
mkdir -p %buildroot{%refind_lib{,/drivers_%_efi_arch},%refind_data}

%ifarch x86_64
for file in refind/refind*.efi; do
	sbsign --key %_efi_keydir/altlinux.key --cert %_efi_keydir/altlinux.crt \
		--output %buildroot%_efi_bindir/"`basename "$file"`" "$file"
done
for file in drivers_%_efi_arch/*_x64.efi; do
	sbsign --key %_efi_keydir/altlinux.key --cert %_efi_keydir/altlinux.crt \
		--output %buildroot%refind_lib/"$file" "$file"
done
%endif

%ifarch %ix86
install -pm644 refind/refind*.efi %buildroot%refind_lib/
cp -a drivers_%_efi_arch/*.efi %buildroot%refind_lib/drivers_%_efi_arch/
%endif

cp -a icons/ %buildroot%refind_data/
cp -a %SOURCE1 %buildroot%refind_data/icons/

%files
%doc docs/*
%doc NEWS.txt COPYING.txt LICENSE.txt README.txt CREDITS.txt
%doc install.sh mkrlconf.sh mvrefind.sh
%refind_lib
%refind_data

# TODO:
# - create separate signing helper
# - move off hardwired sbsign to that
# NB:
# - macros get expanded too early for shell loops

%changelog
* Sat Jan 12 2013 Michael Shigorin <mike@altlinux.org> 0.6.4-alt1
- initial build for ALT Linux Sisyphus
- stripped upstream installation helpers (too smart for a package)
- added os_altlinux icon

* Sun Jan 6 2013 R Smith <rodsmith@rodsbooks.com> - 0.6.3-2
- Fixed accidental inclusion of "env" as part of installation script

* Sun Jan 6 2013 R Smith <rodsmith@rodsbooks.com> - 0.6.3
- Created spec file for 0.6.3 release
