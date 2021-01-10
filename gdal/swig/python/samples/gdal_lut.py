#!/usr/bin/env python3
# ******************************************************************************
#  $Id: pct2rgb.py 13087 2007-11-26 20:56:29Z hobu $
#
#  Name:     gdal_lut
#  Project:  GDAL Python Interface
#  Purpose:  Utility to apply a lookup table provided in a text file.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2008, Frank Warmerdam
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
# ******************************************************************************

import sys
import numpy as np

from osgeo import gdal
gdal.TermProgress = gdal.TermProgress_nocb


# =============================================================================
# read_lut()
#
# Read and parse the LUT file.
# =============================================================================


def read_lut(filename):

    lines = open(filename).readlines()

    lut = []
    for line in lines:
        lut.append(int(line))

    return lut

# =============================================================================
# Usage()
# =============================================================================


def Usage():
    print("""
Usage: gdal_lut.py src_file [-srcband] [dst_file] [-dstband] -lutfile filename
                   [-of format] [-co name=value]*

If dst_file is not specified, the result will be applied back to src_file.
The text file specified with -lutfile should have one line per LUT entry
with just the output pixel value.  Thus a LUT file like:

0
5
11
12
12
13

would map input pixel values 0,1,2,3,4,5 to 0,5,11,12,12,13 respectively.
Values not mapped by the lut file (for instance values 6-255 in the above
case) will be left unaltered.  Sixteen bit (UInt16) output values are
supported as well as luts of more than 256 input values.
""")
    return 1


def main(argv):
    frmt = 'GTiff'
    src_filename = None
    dst_filename = None
    src_band_n = 1
    dst_band_n = 1
    lut_filename = None
    create_options = []

    gdal.AllRegister()
    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-of':
            i = i + 1
            frmt = argv[i]

        elif arg == '-co':
            i = i + 1
            create_options.append(argv[i])

        elif arg == '-lutfile':
            i = i + 1
            lut_filename = argv[i]

        elif arg == '-srcband':
            i = i + 1
            src_band_n = int(argv[i])

        elif arg == '-dstband':
            i = i + 1
            dst_band_n = int(argv[i])

        elif src_filename is None:
            src_filename = argv[i]

        elif dst_filename is None:
            dst_filename = argv[i]

        else:
            Usage()

        i = i + 1

    if src_filename is None or lut_filename is None:
        Usage()

    # ----------------------------------------------------------------------------
    # Load the LUT file.

    lut = read_lut(lut_filename)

    max_val = 0
    for entry in lut:
        if entry > max_val:
            max_val = entry

    if max_val > 255:
        tc = np.uint16
        gc = gdal.GDT_UInt16
    else:
        tc = np.uint8
        gc = gdal.GDT_Byte

    # ----------------------------------------------------------------------------
    # Convert the LUT from a normal array to a numpy style array.

    if len(lut) <= 256:
        lookup = np.arange(256)
        for i in range(min(256, len(lut))):
            lookup[i] = lut[i]
    else:
        lookup = np.arange(65536)
        for i in range(min(65536, len(lut))):
            lookup[i] = lut[i]

    lookup = lookup.astype(tc)

    # ----------------------------------------------------------------------------
    # Open source file

    if dst_filename is None:
        src_ds = gdal.Open(src_filename, gdal.GA_Update)
        dst_ds = src_ds
    else:
        src_ds = gdal.Open(src_filename)
        dst_ds = None

    if src_ds is None:
        print('Unable to open ', src_filename)
        return 1

    src_band = src_ds.GetRasterBand(src_band_n)

    # ----------------------------------------------------------------------------
    # Open or create output file.

    dst_driver = gdal.GetDriverByName(frmt)
    if dst_driver is None:
        print('"%s" driver not registered.' % frmt)
        return 1

    if dst_ds is None:
        try:
            dst_ds = gdal.Open(dst_filename, gdal.GA_Update)
        except:
            dst_ds = None

        if dst_ds is None:
            dst_ds = dst_driver.Create(dst_filename,
                                       src_ds.RasterXSize,
                                       src_ds.RasterYSize,
                                       1, gc, options=create_options)
            dst_ds.SetProjection(src_ds.GetProjection())
            dst_ds.SetGeoTransform(src_ds.GetGeoTransform())


    dst_band = dst_ds.GetRasterBand(dst_band_n)

    # ----------------------------------------------------------------------------
    # Do the processing one scanline at a time.

    gdal.TermProgress(0.0)
    for iY in range(src_ds.RasterYSize):
        src_data = src_band.ReadAsArray(0, iY, src_ds.RasterXSize, 1)

        dst_data = np.take(lookup, src_data)
        dst_band.WriteArray(dst_data, 0, iY)

        gdal.TermProgress((iY + 1.0) / src_ds.RasterYSize)

    src_ds = None
    dst_ds = None

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

