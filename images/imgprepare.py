#!/usr/bin/python

import sys
import Image

def enc_backbuffer(backbuffer):
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

def packbits(rawdata):
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


for filename in sys.argv[1:]:

    origimage = Image.open(filename)

    (width, height) = origimage.size
    mode = origimage.mode
    data = origimage.getdata()

    print "%s: %d x %d %s" % (filename, width, height, mode)

    basename = filename[:-4]
    identname = basename.replace("-", "_")

    planecount = 1
    imgmode = 0
    rawdata = []

    if mode == "RGB" or mode == "RGBA":
        planes = [ [], [], [] ]
        for pixcount in range(0, width*height):
            pixeldata = data[pixcount]
            planes[0].append(pixeldata[2])
            planes[1].append(pixeldata[1])
            planes[2].append(pixeldata[0])

        if planes[0] == planes[1] and planes[0] == planes[2]:
            print " encoding as greyscale"
            planecount = 1
            rawdata.extend(planes[0])

            if basename[0:4] == "font":
                print " font detected, using alpha-only mode"
                imgmode = 1
                # invert all values
                rawdata = map(lambda x: 255-x, rawdata)

        else:
            print " encoding as true color"
            planecount = 3
            rawdata.extend(planes[0])
            rawdata.extend(planes[1])
            rawdata.extend(planes[2])

    else:
        print " Mode not supported!"
        continue

    rawlen = len(rawdata)
    compdata = packbits(rawdata)
    complen = len(compdata)
    print " compressed %d to %d" % (rawlen, complen)

    output = """static UINT8 image_%s_compdata[] = {
""" % identname
    for i in range(0, len(compdata)):
        output = output + " 0x%02x," % compdata[i]
        if (i % 12) == 11:
            output = output + "\n"
    output = output + """
};
static BUILTIN_IMAGE image_%s = { NULL, %d, %d, %d, %d, image_%s_compdata, %d };
""" % (identname, width, height, imgmode, planecount, identname, len(compdata))

    f = file("image_%s.h" % identname, "w")
    f.write(output)
    f.close()

print "Done!"
