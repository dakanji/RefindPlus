#
# refind/Makefile
# Build control file for the rEFInd boot menu
#
# This program is licensed under the terms of the GNU GPL, version 3,
# or (at your option) any later version.
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

SRCDIR = .

VPATH = $(SRCDIR)

ARCH            = $(shell uname -m | sed s,i[3456789]86,ia32,)

TARGET	= refind.efi

ifeq ($(ARCH),ia32)
  LIBEG = build32
  TARGET = refind_ia32.efi
endif

ifeq ($(ARCH),x86_64)
  LIBEG = build64
  TARGET = refind_x64.efi
endif

ifeq ($(ARCH),aarch64)
  LIBEG = build
  TARGET = refind_aa64.efi
endif

LOCAL_GNUEFI_CFLAGS  = -I$(SRCDIR) -I$(SRCDIR)/../include \
                       -I$(SRCDIR)/../libeg -I$(SRCDIR)/../mok
LOCAL_LDFLAGS   = -L$(SRCDIR)/../libeg/ -L$(SRCDIR)/../mok/ \
                  -L$(SRCDIR)/../EfiLib/
LOCAL_LIBS      = -leg -lmok -lEfiLib

OBJS            = apple.o config.o crc32.o driver_support.o gpt.o icns.o \
                  install.o launch_efi.o launch_legacy.o lib.o line_edit.o \
                  linux.o log.o main.o menu.o mystrings.o pointer.o scan.o \
                  screen.o

include $(SRCDIR)/../Make.common

all: $(TARGET)

$(SHLIB_TARGET): $(OBJS)
	$(LD) $(LOCAL_LDFLAGS) $(GNUEFI_LDFLAGS) $(SUBSYSTEM_LDFLAG) $(OBJS) \
	      -o $@ $(LOCAL_LIBS) $(GNUEFI_LIBS)

$(TARGET): $(SHLIB_TARGET)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel \
	           -j .rela -j .rel.* -j .rela.* -j .rel* -j .rela* \
		   -j .reloc $(FORMAT) $< $@
	chmod a-x $(TARGET)

clean:
	rm -f $(TARGET) *~ *.so $(OBJS) *.efi *.obj refind_*.txt \
		refind_*.dll *.lib

