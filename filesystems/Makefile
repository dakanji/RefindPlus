# meta-Makefile for rEFInd filesystem drivers
#
# Most of the functionality is in Make.tiano; this Makefile merely
# deletes critical temporary files and calls Make.tiano with the
# name of the driver to be built. This is done because of a dependency
# in the fsw_efi.c file on the filesystem type; this file must be
# recompiled for each new filesystem built.

# This program is licensed under the terms of the GNU GPL, version 3,
# or (at your option) any later version.
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

INSTALL_DIR = /boot/efi/EFI/refind/drivers

FILESYSTEMS = ext2 ext4 reiserfs iso9660 hfs btrfs
FILESYSTEMS_GNUEFI = ext2_gnuefi ext4_gnuefi reiserfs_gnuefi iso9660_gnuefi hfs_gnuefi btrfs_gnuefi
TEXTFILES = $(FILESYSTEMS:=*.txt)

# Build the drivers with TianoCore EDK2.....

all:	$(FILESYSTEMS)

ext2:
	rm -f fsw_efi.obj
	+make DRIVERNAME=ext2 -f Make.tiano

ext4:
	rm -f fsw_efi.obj
	+make DRIVERNAME=ext4 -f Make.tiano

reiserfs:
	rm -f fsw_efi.obj
	+make DRIVERNAME=reiserfs -f Make.tiano

iso9660:
	rm -f fsw_efi.obj
	+make DRIVERNAME=iso9660 -f Make.tiano

hfs:
	rm -f fsw_efi.obj
	+make DRIVERNAME=hfs -f Make.tiano

btrfs:
	rm -f fsw_efi.obj
	+make DRIVERNAME=btrfs -f Make.tiano

ntfs:
	rm -f fsw_efi.obj
	+make DRIVERNAME=ntfs -f Make.tiano

# Build the drivers with GNU-EFI....

gnuefi: $(FILESYSTEMS_GNUEFI)

all_gnuefi:	$(FILESYSTEMS_GNUEFI)

ext2_gnuefi:
	rm -f fsw_efi.o
	+make DRIVERNAME=ext2 -f Make.gnuefi

ext4_gnuefi:
	rm -f fsw_efi.o
	+make DRIVERNAME=ext4 -f Make.gnuefi

reiserfs_gnuefi:
	rm -f fsw_efi.o
	+make DRIVERNAME=reiserfs -f Make.gnuefi

iso9660_gnuefi:
	rm -f fsw_efi.o
	+make DRIVERNAME=iso9660 -f Make.gnuefi

hfs_gnuefi:
	rm -f fsw_efi.o
	+make DRIVERNAME=hfs -f Make.gnuefi

btrfs_gnuefi:
	rm -f fsw_efi.o
	+make DRIVERNAME=btrfs -f Make.gnuefi

ntfs_gnuefi:
	rm -f fsw_efi.o
	+make DRIVERNAME=ntfs -f Make.gnuefi

# utility rules

clean:
	rm -f *~ *.bak *.o *.obj *.so *.efi *.dll err.txt $(TEXTFILES)
	+make -C test clean


install:
	mkdir -p $(INSTALL_DIR)
	cp *.efi $(INSTALL_DIR)

# DO NOT DELETE
