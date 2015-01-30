#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL Python samples
# Purpose:  Script to write out ASCII GRD rasters (used in Golden Software
#	    Surfer)
#           from any source supported by GDAL.
# Author:   Andrey Kiselev, dron@remotesensing.org
#
###############################################################################
# Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
# Copyright (c) 2009, Even Rouault <even dot rouault at mines-paris dot org>
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

try:
    from osgeo import gdal
    gdal.TermProgress = gdal.TermProgress_nocb
except ImportError:
    import gdal

import sys

# =============================================================================
def Usage():
    print('Usage: gdal2grd.py [-b band] [-quiet] infile outfile')
    print('Write out ASCII GRD rasters (used in Golden Software Surfer)')
    print('')
    print('  -b band	    Select a band number to convert (1 based)')
    print('  -quiet	    Do not report any diagnostic information')
    print('  infile	    Name of the input GDAL supported file')
    print('  outfile	    Name of the output GRD file')
    print('')
    sys.exit(1)

# =============================================================================

infile = None
outfile = None
iBand = 1
quiet = 0

# Parse command line arguments.
i = 1
while i < len(sys.argv):
    arg = sys.argv[i]

    if arg == '-b':
        i = i + 1
        iBand = int(sys.argv[i])

    elif arg == '-quiet':
        quiet = 1

    elif infile is None:
        infile = arg

    elif outfile is None:
        outfile = arg

    else:
        Usage()

    i = i + 1

if infile is None:
    Usage()
if  outfile is None:
    Usage()

indataset = gdal.Open(infile, gdal.GA_ReadOnly)
if infile == None:
    print('Cannot open', infile)
    sys.exit(2)
geotransform = indataset.GetGeoTransform()
band = indataset.GetRasterBand(iBand)
if band == None:
    print('Cannot load band', iBand, 'from the', infile)
    sys.exit(2)

if not quiet:
    print('Size is ',indataset.RasterXSize,'x',indataset.RasterYSize,'x',indataset.RasterCount)
    print('Projection is ',indataset.GetProjection())
    print('Origin = (',geotransform[0], ',',geotransform[3],')')
    print('Pixel Size = (',geotransform[1], ',',geotransform[5],')')
    print('Converting band number',iBand,'with type',gdal.GetDataTypeName(band.DataType))

# Header printing
fpout = open(outfile, "wt")
fpout.write("DSAA\n")
fpout.write(str(band.XSize) + " " + str(band.YSize) + "\n")
fpout.write(str(geotransform[0] + geotransform[1] / 2) + " " +
    str(geotransform[0] + geotransform[1] * (band.XSize - 0.5)) + "\n")
if geotransform[5] < 0:
    fpout.write(str(geotransform[3] + geotransform[5] * (band.YSize - 0.5)) + " " +
    str(geotransform[3] + geotransform[5] / 2) + "\n")
else:
    fpout.write(str(geotransform[3] + geotransform[5] / 2) + " " +
    str(geotransform[3] + geotransform[5] * (band.YSize - 0.5)) + "\n")
fpout.write(str(band.ComputeRasterMinMax(0)[0]) + " " +
    str(band.ComputeRasterMinMax(0)[1]) + "\n")

for i in range(band.YSize - 1, -1, -1):
    scanline = band.ReadAsArray(0, i, band.XSize, 1, band.XSize, 1)
    j = 0
    while j < band.XSize:
        fpout.write(str(scanline[0, j]))
        j = j + 1
        if j % 10:	    # Print no more than 10 values per line
            fpout.write(" ")
        else:
            fpout.write("\n")
    fpout.write("\n")

    # Display progress report on terminal
    if not quiet:
        gdal.TermProgress(float(band.YSize - i) / band.YSize)

