# Makefile for rEFInd

# This program is licensed under the terms of the GNU GPL, version 3,
# or (at your option) any later version.
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

include Make.common

SHELL=/bin/bash
LOADER_DIR=refind
FS_DIR=filesystems
LIBEG_DIR=libeg
MOK_DIR=mok
GPTSYNC_DIR=gptsync
EFILIB_DIR=EfiLib
# Two possible locations for TianoCore toolkit:
# TIANOBASE is used with "tiano" targets and
# EDK2BASE is used with "edk2" targets
export TIANOBASE=/usr/local/UDK2014/MyWorkSpace
export EDK2BASE=/usr/local/edk2-vUDK2018
# NOTE: Below is overridden for "tiano" targets
-include $(EDK2BASE)/Conf/target.txt
THISDIR=$(shell pwd)
EDK2_BUILDLOC=$(EDK2BASE)/Build/Refind/$(TARGET)_$(TOOL_CHAIN_TAG)/$(UC_ARCH)
EDK2_PROGRAM_BASENAMES=refind gptsync
EDK2_PROGRAMS=$(EDK2_PROGRAM_BASENAMES:=.efi)
EDK2_DRIVER_BASENAMES=btrfs ext4 ext2 hfs iso9660 reiserfs
EDK2_DRIVERS=$(EDK2_DRIVER_BASENAMES:=.efi)
EDK2_ALL_BASENAMES=$(EDK2_PROGRAM_BASENAMES) $(EDK2_DRIVER_BASENAMES)
EDK2_ALL_FILES=$(EDK2_ALL_BASENAMES:=.efi)

# The "all" target builds with the TianoCore library if possible, but falls
# back on the more easily-installed GNU-EFI library if TianoCore isn't
# installed at $(EDK2BASE) or $(TIANOBASE)
all:
ifneq ($(wildcard $(EDK2BASE)/*),)
	@echo "Found $(EDK2BASE); building with TianoCore EDK2"
	+make edk2
else ifneq ($(wildcard $(TIANOBASE)/*),)
	@echo "Found $(TIANOBASE); building with TianoCore UDK2014"
	+make tiano
else
	@echo "Did not find $(EDK2BASE) or $(TIANOBASE); building with GNU-EFI"
	+make gnuefi
endif

# The "fs" target, like "all," attempts to build with TianoCore but falls
# back to GNU-EFI.
fs:
ifneq ($(wildcard $(EDK2BASE)/*),)
	@echo "Found $(EDK2BASE); building with TianoCore EDK2"
	+make fs_edk2
else ifneq ($(wildcard $(TIANOBASE)/*),)
	@echo "Found $(TIANOBASE); building with TianoCore UDK2014"
	+make fs_tiano
else
	@echo "Did not find $(EDK2BASE) or $(TIANOBASE); building with GNU-EFI"
	+make fs_gnuefi
endif

# Likewise for GPTsync....
gptsync:
ifneq ($(wildcard $(EDK2BASE)/*),)
	@echo "Found $(EDK2BASE); building with TianoCore EDK2"
	+make gptsync_edk2
else ifneq ($(wildcard $(TIANOBASE)/*),)
	@echo "Found $(TIANOBASE); building with TianoCore UDK2014"
	+make gptsync_tiano
else
	@echo "Did not find $(EDK2BASE) or $(TIANOBASE); building with GNU-EFI"
	+make gptsync_gnuefi
endif

###########################################################################
#
# GNU-EFI build rules; should work with any CPU architecture and GNU-EFI
# version 3.0u or later, but cross-compiling requires adding odd
# components and is untested....
#
###########################################################################

gnuefi:
	+make MAKEWITH=GNUEFI -C $(LIBEG_DIR)
	+make MAKEWITH=GNUEFI -C $(MOK_DIR)
	+make MAKEWITH=GNUEFI -C $(EFILIB_DIR)
	+make MAKEWITH=GNUEFI -C $(LOADER_DIR)
	+make MAKEWITH=GNUEFI -C $(GPTSYNC_DIR) gnuefi

all_gnuefi: gnuefi fs_gnuefi

gptsync_gnuefi:
	+make MAKEWITH=GNUEFI -C $(GPTSYNC_DIR) gnuefi

fs_gnuefi:
	+make MAKEWITH=GNUEFI -C $(FS_DIR) all_gnuefi

###########################################################################
#
# Old-style TianoCore build rules; useful only with UDK2014 development
# kit....
#
###########################################################################

# Don't build gptsync under TianoCore/AARCH64 by default because it errors out
# when using a cross-compiler on an x86-64 system. Because gptsync is pretty
# useless on ARM64, skipping it is no big deal....
tiano:
	-include $(TIANOBASE)/Conf/target.txt
	+make MAKEWITH=TIANO AR_TARGET=EfiLib -C $(EFILIB_DIR) -f Make.tiano
	+make MAKEWITH=TIANO AR_TARGET=libeg -C $(LIBEG_DIR) -f Make.tiano
	+make MAKEWITH=TIANO AR_TARGET=mok -C $(MOK_DIR) -f Make.tiano
	+make MAKEWITH=TIANO BUILDME=refind DLL_TARGET=refind -C $(LOADER_DIR) -f Make.tiano
ifneq ($(ARCH),aarch64)
	+make MAKEWITH=TIANO -C $(GPTSYNC_DIR) -f Make.tiano
endif
#	+make MAKEWITH=TIANO -C $(FS_DIR)

all_tiano: tiano fs_tiano

gptsync_tiano:
	-include $(TIANOBASE)/Conf/target.txt
	+make MAKEWITH=TIANO -C $(GPTSYNC_DIR) -f Make.tiano

fs_tiano:
	-include $(TIANOBASE)/Conf/target.txt
	+make MAKEWITH=TIANO -C $(FS_DIR)

###########################################################################
#
# New-style build rules for use with TianoCore via its own "build" tool.
# Works with UDK2014 and late-March EDK2; but UDK2014 builds fail with
# AARCH64 unless .inf files are modified to eliminate references to
# CompilerIntrinsicsLib....
#
# NOTE: Unlike other build rules, these build everything (rEFInd, gptsync,
# and filesystem drivers), even if only one component is requested; but
# only the requested components are copied to their final destinations.
# This is done because of the heavy overhead in building just one
# component in the TianoCore "build" tool.
#
###########################################################################

# Build process for TianoCore using TianoCore-standard build process rather
# than my own custom Makefiles (except this top-level one)
edk2: build_edk2
	cp $(EDK2_BUILDLOC)/refind.efi ./refind/refind_$(FILENAME_CODE).efi
	cp $(EDK2_BUILDLOC)/gptsync.efi ./gptsync/gptsync_$(FILENAME_CODE).efi

all_edk2: build_edk2 fs_edk2
	cp $(EDK2_BUILDLOC)/refind.efi ./refind/refind_$(FILENAME_CODE).efi
	cp $(EDK2_BUILDLOC)/gptsync.efi ./gptsync/gptsync_$(FILENAME_CODE).efi

gptsync_edk2: build_edk2
	cp $(EDK2_BUILDLOC)/gptsync.efi ./gptsync/gptsync_$(FILENAME_CODE).efi

fs_edk2: build_edk2
	for BASENAME in $(EDK2_DRIVER_BASENAMES) ; do \
		echo "Copying $$BASENAME""_$(FILENAME_CODE).efi" ; \
		cp "$(EDK2_BUILDLOC)/$$BASENAME.efi" ./drivers_$(FILENAME_CODE)/$$BASENAME\_$(FILENAME_CODE).efi ; \
	done

build_edk2: $(EDK2BASE)/RefindPkg
	cd $(EDK2BASE) && \
	. ./edksetup.sh BaseTools && \
	build -a $(UC_ARCH) -p RefindPkg/RefindPkg.dsc
	mkdir -p ./drivers_$(FILENAME_CODE)

$(EDK2BASE)/RefindPkg:
	ln -s $(THISDIR) $(EDK2BASE)/RefindPkg

###########################################################################
#
# Build rules that are not dependent on the toolkit....
#
###########################################################################

# NOTE: This "clean" rule cleans intermediate components for all three
# build styles (tiano, edk2, and gnuefi).
clean:
	make -C $(LIBEG_DIR) clean
	make -C $(MOK_DIR) clean
	make -C $(LOADER_DIR) clean
	make -C $(EFILIB_DIR) clean
	make -C $(FS_DIR) clean
	make -C $(GPTSYNC_DIR) clean
	rm -f include/*~
	rm -rf $(EDK2BASE)/Build/Refind
	rm -rf drivers_$(FILENAME_CODE)/*
	[ ! -L $(EDK2BASE)/RefindPkg ] || rm -v $(EDK2BASE)/RefindPkg

# NOTE TO DISTRIBUTION MAINTAINERS:
# The "install" target installs the program directly to the ESP
# and it modifies the *CURRENT COMPUTER's* NVRAM. Thus, you should
# *NOT* use this target as part of the build process for your
# binary packages (RPMs, Debian packages, etc.). (Gentoo could
# use it in an ebuild, though....) You COULD, however, copy the
# files to a directory somewhere (/usr/share/refind or whatever)
# and then call refind-install as part of the binary package
# installation process.

install:
	./refind-install

# DO NOT DELETE
