This directory contains PNGs built from a few open source fonts: the sans
serif Liberation Mono Regular and Ubuntu mono and the serif Nimbus Mono,
all in 12-, 14, and 24-point versions. All of these font files have
anti-aliasing (aka font smoothing) applied.

New fonts can be created on Linux as follows:

1. Type "convert -list font | less" to obtain a list of fonts. Search for
   monospace fonts.

2. Type "./mkfont.sh {fontname} {point-size} {offset} {filename}", where:
   * {fontname} is the font's name, as shown in the "convert" output.
   * {point-size} is the point size, such as 14 or 20.
   * {offset} is a positive (to shift down) or negative (to shift up)
     integer value by which the font's vertical position is adjusted when
     rendered. You'll need to determine this value by trial and error, but
     in my experience, most fonts require a -1 or -2 value.
   * {filename} is the output filename for a PNG file.
   For instance, I used the following command to create the large Liberation
   Mono font file:
   "./mkfont.sh Liberation-Mono 28 -2 liberation-mono-regular-28.png"

3. Repeat step #2 as necessary for additional fonts or to try different
   {offset} values until you find one that works well for you.

NOTE TO DEVELOPERS:
-------------------

When embedding a font in the rEFInd binary as libeg/egemb_font.h or
libeg/egemb_font_large.h, the images/mkegemb.py script is used. This script
is likely to complain of an unsupported format on fonts prepared in this
way. The solution is:

1. Convert the font to NOT use an alpha layer. (I use xv to load and
   re-save the font for this purpose.)
2. Rename the font to font.png or font_large.png (or anything else that
   begins with "font", although then additional editing of the resulting
   header files will be required).
3. Run mkegemb.py. This causes the script to treat white as if it were
   alpha transparency.
