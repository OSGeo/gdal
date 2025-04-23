#!/usr/bin/env python3
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Application to checksum a GDAL image file.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
# ******************************************************************************

import sys

from osgeo import gdal


def Usage():
    print("Usage: gdalchksum.py [-b band] [-srcwin xoff yoff xsize ysize] file")
    return 2


def main(argv=sys.argv):
    srcwin = None
    bands = []

    filename = None

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == "-b":
            i = i + 1
            bands.append(int(argv[i]))

        elif arg == "-srcwin":
            srcwin = [
                int(argv[i + 1]),
                int(argv[i + 2]),
                int(argv[i + 3]),
                int(argv[i + 3]),
            ]
            i = i + 4

        elif filename is None:
            filename = argv[i]

        else:
            return Usage()

        i = i + 1

    if filename is None:
        return Usage()

    # Open source file

    ds = gdal.Open(filename)
    if ds is None:
        print("Unable to open %s" % filename)
        return 1

    # Default values

    if srcwin is None:
        srcwin = [0, 0, ds.RasterXSize, ds.RasterYSize]

    if not bands:
        bands = list(range(1, (ds.RasterCount + 1)))

    # Generate checksums

    for band_num in bands:
        oBand = ds.GetRasterBand(band_num)
        result = oBand.Checksum(srcwin[0], srcwin[1], srcwin[2], srcwin[3])
        print(result)

    ds = None


if __name__ == "__main__":
    sys.exit(main(sys.argv))
