#!/usr/bin/python

import sys
import Image

def enc_backbuffer(backbuffer):
    """Helper function for RLE compression, encodes a string of uncompressable data."""
    compdata = []
    if len(backbuffer) == 0:
        return compdata
    while len(backbuffer) > 128:
        compdata.append(127)
        compdata.extend(backbuffer[0:128])
        backbuffer = backbuffer[128:]
    compdata.append(len(backbuffer)-1)
    compdata.extend(backbuffer)
    return compdata

def compress_rle(rawdata):
    """Compresses the string using a RLE scheme."""
    compdata = []
    backbuffer = []

    while len(rawdata) >= 3:
        c = rawdata[0]
        if rawdata[1] == c and rawdata[2] == c:
            runlength = 3
            while runlength < 130 and len(rawdata) > runlength:
                if rawdata[runlength] == c:
                    runlength = runlength + 1
                else:
                    break
            compdata.extend(enc_backbuffer(backbuffer))
            backbuffer = []
            compdata.append(runlength + 125)
            compdata.append(c)
            rawdata = rawdata[runlength:]

        else:
            backbuffer.append(c)
            rawdata = rawdata[1:]

    backbuffer.extend(rawdata)
    compdata.extend(enc_backbuffer(backbuffer))

    return compdata

def encode_plane(rawdata, identname, planename):
    """Encodes the data of a single plane."""

    rawlen = len(rawdata)
    compdata = compress_rle(rawdata)
    complen = len(compdata)
    print "  plane %s: compressed %d to %d (%.1f%%)" % (planename, rawlen, complen, float(complen) / float(rawlen) * 100.0)

    output = """static const UINT8 eei_%s_planedata_%s[%d] = {
""" % (identname, planename, complen)
    for i in range(0, len(compdata)):
        output = output + " 0x%02x," % compdata[i]
        if (i % 12) == 11:
            output = output + "\n"
    output = output + """
};
"""
    return (output, "eei_%s_planedata_%s, %d" % (identname, planename, complen))


### main loop

print "mkeei 0.1, Copyright (c) 2006 Christoph Pfisterer"

planenames = ( "blue", "green", "red", "alpha", "grey" )

for filename in sys.argv[1:]:

    origimage = Image.open(filename)

    (width, height) = origimage.size
    mode = origimage.mode
    data = origimage.getdata()

    print "%s: %d x %d %s" % (filename, width, height, mode)

    basename = filename[:-4]   # TODO!!!!!!
    identname = basename.replace("-", "_")

    planes = [ [], [], [], [] ]

    if mode == "RGB":
        for pixcount in range(0, width*height):
            pixeldata = data[pixcount]
            planes[0].append(pixeldata[2])
            planes[1].append(pixeldata[1])
            planes[2].append(pixeldata[0])

    elif mode == "RGBA":
        for pixcount in range(0, width*height):
            pixeldata = data[pixcount]
            planes[0].append(pixeldata[2])
            planes[1].append(pixeldata[1])
            planes[2].append(pixeldata[0])
            planes[3].append(pixeldata[3])

    elif mode == "L":
        for pixcount in range(0, width*height):
            pixeldata = data[pixcount]
            planes[0].append(pixeldata)
            planes[1].append(pixeldata)
            planes[2].append(pixeldata)

    else:
        print " Error: Mode not supported!"
        continue

    # special treatment for fonts

    if basename[0:4] == "font":
        if planes[0] != planes[1] or planes[0] != planes[2]:
            print " Error: Font detected, but it is not greyscale!"
            continue
        print " font detected, encoding as alpha-only"
        # invert greyscale values for use as alpha
        planes[3] = map(lambda x: 255-x, planes[0])
        planes[0] = []
        planes[1] = []
        planes[2] = []

    # generate optimal output

    output = ""
    planeinfo = [ "NULL, 0", "NULL, 0", "NULL, 0", "NULL, 0" ]

    if len(planes[0]) > 0 and planes[0] == planes[1] and planes[0] == planes[2]:
        print " encoding as greyscale"
        (output_part, planeinfo[0]) = encode_plane(planes[0], identname, planenames[4])
        output = output + output_part
        planeinfo[1] = planeinfo[0]
        planeinfo[2] = planeinfo[0]

    elif len(planes[0]) > 0:
        print " encoding as true color"

        (output_part, planeinfo[0]) = encode_plane(planes[0], identname, planenames[0])
        output = output + output_part

        if planes[1] == planes[0]:
            print " encoding plane 1 is a copy of plane 0"
            planeinfo[1] = planeinfo[0]
        else:
            (output_part, planeinfo[1]) = encode_plane(planes[1], identname, planenames[1])
            output = output + output_part

        if planes[2] == planes[0]:
            print " encoding plane 2 is a copy of plane 0"
            planeinfo[2] = planeinfo[0]
        elif planes[2] == planes[1]:
            print " encoding plane 2 is a copy of plane 1"
            planeinfo[2] = planeinfo[1]
        else:
            (output_part, planeinfo[2]) = encode_plane(planes[2], identname, planenames[2])
            output = output + output_part

    if len(planes[3]) > 0:
        if reduce(lambda x,y: x+y, planes[3]) == 0:
            print " skipping alpha plane because it is empty"
        else:
            (output_part, planeinfo[3]) = encode_plane(planes[3], identname, planenames[3])
            output = output + output_part

    output = output + "static EEI_IMAGE eei_%s = { %d, %d, NULL, {\n" % (identname, width, height)
    for i in range(0,4):
        output = output + "    { %s },\n" % planeinfo[i]
    output = output + "} };\n"

    f = file("eei_%s.h" % identname, "w")
    f.write(output)
    f.close()

print "Done!"
