#!/usr/bin/env python3
###############################################################################
#
# Project:  GDAL Python samples
# Purpose:  Script to write out ASCII GRD rasters (used in Golden Software
#           Surfer)
#           from any source supported by GDAL.
# Author:   Andrey Kiselev, dron@remotesensing.org
#
###############################################################################
# Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import sys

from osgeo import gdal

gdal.TermProgress = gdal.TermProgress_nocb

# =============================================================================


def Usage():
    print("Usage: gdal2grd.py [-b band] [-quiet] infile outfile")
    print("Write out ASCII GRD rasters (used in Golden Software Surfer)")
    print("")
    print("  -b band	    Select a band number to convert (1 based)")
    print("  -quiet	    Do not report any diagnostic information")
    print("  infile	    Name of the input GDAL supported file")
    print("  outfile	    Name of the output GRD file")
    print("")
    return 2


def main(argv=sys.argv):
    infile = None
    outfile = None
    iBand = 1
    quiet = 0

    # Parse command line arguments.
    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]

        if arg == "-b":
            i = i + 1
            iBand = int(sys.argv[i])

        elif arg == "-quiet":
            quiet = 1

        elif infile is None:
            infile = arg

        elif outfile is None:
            outfile = arg

        else:
            Usage()

        i = i + 1

    if infile is None:
        return Usage()
    if outfile is None:
        return Usage()

    indataset = gdal.Open(infile, gdal.GA_ReadOnly)
    if infile is None:
        print("Cannot open", infile)
        return 2
    geotransform = indataset.GetGeoTransform()
    band = indataset.GetRasterBand(iBand)
    if band is None:
        print("Cannot load band", iBand, "from the", infile)
        return 2

    if not quiet:
        print(
            "Size is ",
            indataset.RasterXSize,
            "x",
            indataset.RasterYSize,
            "x",
            indataset.RasterCount,
        )
        print("Projection is ", indataset.GetProjection())
        print("Origin = (", geotransform[0], ",", geotransform[3], ")")
        print("Pixel Size = (", geotransform[1], ",", geotransform[5], ")")
        print(
            "Converting band number",
            iBand,
            "with type",
            gdal.GetDataTypeName(band.DataType),
        )

    # Header printing
    fpout = open(outfile, "wt")
    fpout.write("DSAA\n")
    fpout.write(str(band.XSize) + " " + str(band.YSize) + "\n")
    fpout.write(
        str(geotransform[0] + geotransform[1] / 2)
        + " "
        + str(geotransform[0] + geotransform[1] * (band.XSize - 0.5))
        + "\n"
    )
    if geotransform[5] < 0:
        fpout.write(
            str(geotransform[3] + geotransform[5] * (band.YSize - 0.5))
            + " "
            + str(geotransform[3] + geotransform[5] / 2)
            + "\n"
        )
    else:
        fpout.write(
            str(geotransform[3] + geotransform[5] / 2)
            + " "
            + str(geotransform[3] + geotransform[5] * (band.YSize - 0.5))
            + "\n"
        )
    fpout.write(
        str(band.ComputeRasterMinMax(0)[0])
        + " "
        + str(band.ComputeRasterMinMax(0)[1])
        + "\n"
    )

    for i in range(band.YSize - 1, -1, -1):
        scanline = band.ReadAsArray(0, i, band.XSize, 1, band.XSize, 1)
        j = 0
        while j < band.XSize:
            fpout.write(str(scanline[0, j]))
            j = j + 1
            if j % 10:  # Print no more than 10 values per line
                fpout.write(" ")
            else:
                fpout.write("\n")
        fpout.write("\n")

        # Display progress report on terminal
        if not quiet:
            gdal.TermProgress(float(band.YSize - i) / band.YSize)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
