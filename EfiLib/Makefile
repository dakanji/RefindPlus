#
# EfiLib/Makefile
#

# This program is licensed under the terms of the GNU GPL, version 3,
# or (at your option) any later version.
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

SRCDIR = .

VPATH = $(SRCDIR)

LOCAL_GNUEFI_CFLAGS  = -I$(SRCDIR) -I$(SRCDIR)/../include

OBJS            = gnuefi-helper.o legacy.o BdsHelper.o BdsTianoCore.o
TARGET          = libEfiLib.a

all: $(TARGET)

include $(SRCDIR)/../Make.common

clean:
	rm -f *.o *.obj *~ *.lib *.a
