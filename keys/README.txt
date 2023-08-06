This directory contains known public keys for Linux distributions and from
other parties that sign boot loaders and kernels that should be verifiable
by shim. I'm providing these keys as a convenience to enable easy
installation of keys should you replace your distribution's version of shim
with another one and therefore require adding its public key as a machine
owner key (MOK).

Files come with three extensions. A filename ending in .crt is a
certificate file that can be used by sbverify to verify the authenticity of
a key, as in:

$ sbverify --cert keys/refind.crt refind/refind_x64.efi

The .cer and .der filename extensions are equivalent, and are public key
files similar to .crt files, but in a different form. The MokManager
utility expects its input public keys in this form, so these are the files
you would use to add a key to the MOK list maintained by MokManager and
used by shim.

The files in this directory are, in alphabetical order:

- altlinux.cer -- The public key for ALT Linux (http://www.altlinux.com).
  Taken from the alt-uefi-certs package
  (http://www.sisyphus.ru/br/srpm/Sisyphus/alt-uefi-certs/spec).

- canonical-uefi-ca.crt & canonical-uefi-ca.cer -- Canonical's public key,
  matched to the one used to sign Ubuntu boot loaders and kernels.

- centossecurebootca2.cer & centossecurebootca2.crt -- Public keys used to
  authenticate CentOS binaries, taken from shim-15-15.el8_2.src.rpm. Note
  that these are new files for CentOS 8; CentOS 7 used a now-expired key.

- centossecureboot201.cer & centossecureboot201.crt -- I'm not entirely sure
  what this is, but it comes from the same shim-15-15.el8_2.src.rpm package
  as the preceding files.

- debian.cer -- Debian's public key, obtained from
  https://dsa.debian.org/secure-boot-ca.

- fedora-ca.cer & fedora-ca.crt -- Fedora's public key, matched to the one
  used used to sign Fedora's shim 0.8 binary.

- microsoft-kekca-public.cer -- Microsoft's key exchange key (KEK), which
  is present on most UEFI systems with Secure Boot. The purpose of
  Microsoft's KEK is to enable Microsoft tools to update Secure Boot
  variables. There is no reason to add it to your MOK list.

- microsoft-pca-public.cer -- A Microsoft public key, matched to the one
  used to sign Microsoft's own boot loader. You might include this key in
  your MOK list if you replace the keys that came with your computer with
  your own key but still want to boot Windows. There's no reason to add it
  to your MOK list if your computer came this key pre-installed and you did
  not replace the default keys.

- microsoft-uefica-public.cer -- A Microsoft public key, matched to the one
  Microsoft uses to sign third-party applications and drivers. If you
  remove your default keys, adding this one to your MOK list will enable
  you to launch third-party boot loaders and other tools signed by
  Microsoft. There's no reason to add it to your MOK list if your computer
  came this key pre-installed and you did not replace the default keys.

- openSUSE-UEFI-CA-Certificate.cer, openSUSE-UEFI-CA-Certificate.crt,
  openSUSE-UEFI-CA-Certificate-4096.cer, &
  openSUSE-UEFI-CA-Certificate-4096.crt -- Public keys matched to the ones
  used to sign OpenSUSE; taken from openSUSE's shim 0.7.318.81ee56d
  package.

- redhatsecureboot*.cer & redhatsecureboot*.crt -- Keys associated with the
  Red Hat kernel. (They do NOT seem to be used to sign Fedora's kernels or
  GRUB binaries, though.)

- refind.cer & refind.crt -- My own (Roderick W. Smith's) public key,
  matched to the one used to sign refind_x64.efi and the 64-bit rEFInd
  drivers.

- SLES-UEFI-CA-Certificate.cer & SLES-UEFI-CA-Certificate.crt -- The Public
  key for SUSE Linux Enterprise Server; taken from openSUSE's shim
  0.7.318.81ee56d package.

The refind.cer and refind.crt files are my creations and are distributed
under the terms of the BSD 2-clause license. The rest of the files are
distributed on the assumption that doing so constitutes fair use. Certainly
they're all easily obtained on the Internet from other sources.
