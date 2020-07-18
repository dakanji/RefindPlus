# meta-Makefile for gptsync program; controls use of EFI build using
# GNU-EFI vs. TianoCore EDK2 or build for Unix/Linux.
#
# Most of the functionality is in Make.tiano, Make.gnuefi, and
# Make.unix; this Makefile just dispatches based on options
# passed to it....

TEXTFILES = gptsync.txt

TARGET = tiano

all:	$(TARGET)

gnuefi:
	+make -f Make.gnuefi

tiano:
	+make -f Make.tiano

# TODO: Fix Make.unix; currently broken....
unix:
	+make -f Make.unix

# utility rules

clean:
	rm -f *~ *.bak *.o *.obj *.so *.efi *.dll err.txt gptsync_*.txt gptsync showpart $(TEXTFILES)


# DO NOT DELETE
