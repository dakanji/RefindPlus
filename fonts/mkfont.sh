#!/usr/bin/env bash
#
# copyright (c) 2013 by Roderick W. Smith
#
# This program is licensed under the terms of the GNU GPL, version 3,
# or (at your option) any later version.
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

# Program to generate a PNG file suitable for use as a rEFInd font
# To obtain a list of available font names, type:
#
# convert -list font | less
#
# The font used MUST be a monospaced font; searching for the string
# "Mono" will turn up most suitable candidates.
#
# Usage:
# ./mkfont.sh font-name font-size font-Y-offset bitmap-filename.png
#
# This script is part of the rEFInd package. Version numbers refer to
# the rEFInd version with which the script was released.
#
# Version history:
#
#  0.6.6  -  Initial release

if [[ $# != 4 ]] ; then
   echo "Usage: $0 font-name font-size y-offset bitmap-filename.png"
   echo "   font-name: Name of font (use 'convert -list font | less' to get list)"
   echo "              NOTE: Font MUST be monospaced!"
   echo "   font-size: Font size in points"
   echo "   y-offset: pixels font is shifted (may be negative)"
   echo "   bitmap-filename.png: output filename"
   echo ""
   exit 1
fi

Convert="$(command -v convert 2> /dev/null)"
if [[ ! -x $Convert ]] ; then
   echo "The 'convert' program is required but could not be found. It's part of the"
   echo "ImagMagick program, usually installed in the 'imagemagick' package."
   echo ""
   exit 1
fi

Height=$2
let CellWidth=(${Height}*6+5)/10
#let CellWidth=(${Height}*5)/10
let Width=${CellWidth}*96
echo "Creating ${Width}x${Height} font bitmap...."
$Convert -size ${Width}x${Height} xc:transparent -gravity NorthWest -font $1 -pointsize $2 \
      -draw "text 0,$3 ' !\"#\$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_\`abcdefghijklmnopqrstuvwxyz{|}~?'" $4
