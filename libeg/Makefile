#
# libeg/Makefile
# Build control file for the libeg library
#

# This program is licensed under the terms of the GNU GPL, version 3,
# or (at your option) any later version.
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

SRCDIR = .

VPATH = $(SRCDIR)

LOCAL_GNUEFI_CFLAGS  = -I$(SRCDIR) -I$(SRCDIR)/../include

OBJS            = nanojpeg.o nanojpeg_xtra.o screen.o image.o text.o load_bmp.o load_icns.o lodepng.o lodepng_xtra.o
TARGET          = libeg.a

all: $(TARGET)

include $(SRCDIR)/../Make.common

clean:
	rm -f $(TARGET) *~ *.so $(OBJS) *.efi *.obj refind_*.txt refind_*.dll *.lib

# EOF
