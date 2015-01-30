#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL Python samples
# Purpose:  Script to replace specified values from the input raster file
#           with the new ones. May be useful in cases when you don't like
#           value, used for NoData indication and want replace it with other
#           value. Input file remains unchanged, results stored in other file.
# Author:   Andrey Kiselev, dron@remotesensing.org
#
###############################################################################
# Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
# Copyright (c) 2009-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

try:
    import numpy
except ImportError:
    import Numeric as numpy


import sys

# =============================================================================
def Usage():
    print('Usage: val_repl.py -innd in_nodata_value -outnd out_nodata_value')
    print('                   [-of out_format] [-ot out_type] infile outfile')
    print('')
    sys.exit( 1 )

# =============================================================================

# =============================================================================
def ParseType(type):
    gdal_dt = gdal.GetDataTypeByName(type)
    if gdal_dt is gdal.GDT_Unknown:
        gdal_dt = gdal.GDT_Byte
    return gdal_dt

# =============================================================================

inNoData = None
outNoData = None
infile = None
outfile = None
format = 'GTiff'
type = gdal.GDT_Byte

# Parse command line arguments.
i = 1
while i < len(sys.argv):
    arg = sys.argv[i]

    if arg == '-innd':
        i = i + 1
        inNoData = float(sys.argv[i])

    elif arg == '-outnd':
        i = i + 1
        outNoData = float(sys.argv[i])

    elif arg == '-of':
        i = i + 1
        format = sys.argv[i]

    elif arg == '-ot':
        i = i + 1
        type = ParseType(sys.argv[i])

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
if inNoData is None:
    Usage()
if outNoData is None:
    Usage()

indataset = gdal.Open( infile, gdal.GA_ReadOnly )

out_driver = gdal.GetDriverByName(format)
outdataset = out_driver.Create(outfile, indataset.RasterXSize, indataset.RasterYSize, indataset.RasterCount, type)

gt = indataset.GetGeoTransform()
if gt is not None and gt != (0.0, 1.0, 0.0, 0.0, 0.0, 1.0):
    outdataset.SetGeoTransform(gt)

prj = indataset.GetProjectionRef()
if prj is not None and len(prj) > 0:
    outdataset.SetProjection(prj)

for iBand in range(1, indataset.RasterCount + 1):
    inband = indataset.GetRasterBand(iBand)
    outband = outdataset.GetRasterBand(iBand)

    for i in range(inband.YSize - 1, -1, -1):
        scanline = inband.ReadAsArray(0, i, inband.XSize, 1, inband.XSize, 1)
        scanline = numpy.choose( numpy.equal( scanline, inNoData),
                                       (scanline, outNoData) )
        outband.WriteArray(scanline, 0, i)

