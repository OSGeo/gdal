#!/usr/bin/env python3
###############################################################################
# $Id$
#
# Project:  GDAL Python samples
# Purpose:  Script to perform forward and inverse two-dimensional fast
#           Fourier transform.
# Author:   Andrey Kiselev, dron@remotesensing.org
#
###############################################################################
# Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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

import sys

import FFT
from osgeo import gdal

# =============================================================================


def Usage():
    print('Usage: fft.py [-inv] [-of out_format] [-ot out_type] infile outfile')
    print('')
    return 1


def ParseType(typ):
    if typ == 'Byte':
        return gdal.GDT_Byte
    elif typ == 'Int16':
        return gdal.GDT_Int16
    elif typ == 'UInt16':
        return gdal.GDT_UInt16
    elif typ == 'Int32':
        return gdal.GDT_Int32
    elif typ == 'UInt32':
        return gdal.GDT_UInt32
    elif typ == 'Float32':
        return gdal.GDT_Float32
    elif typ == 'Float64':
        return gdal.GDT_Float64
    elif typ == 'CInt16':
        return gdal.GDT_CInt16
    elif typ == 'CInt32':
        return gdal.GDT_CInt32
    elif typ == 'CFloat32':
        return gdal.GDT_CFloat32
    elif typ == 'CFloat64':
        return gdal.GDT_CFloat64
    return gdal.GDT_Byte


def main(argv):
    infile = None
    outfile = None
    driver_name = 'GTiff'
    typ = None
    transformation = 'forward'

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-inv':
            transformation = 'inverse'
            if typ is None:
                typ = gdal.GDT_Float32

        elif arg == '-of':
            i = i + 1
            driver_name = argv[i]

        elif arg == '-ot':
            i = i + 1
            typ = ParseType(argv[i])
            # set_type = 'yes'

        elif infile is None:
            infile = arg

        elif outfile is None:
            outfile = arg

        else:
            return Usage()

        i = i + 1

    if infile is None:
        return Usage()
    if outfile is None:
        return Usage()

    if typ is None:
        typ = gdal.GDT_CFloat32

    indataset = gdal.Open(infile, gdal.GA_ReadOnly)

    out_driver = gdal.GetDriverByName(driver_name)
    outdataset = out_driver.Create(outfile, indataset.RasterXSize, indataset.RasterYSize, indataset.RasterCount, typ)

    for iBand in range(1, indataset.RasterCount + 1):
        inband = indataset.GetRasterBand(iBand)
        outband = outdataset.GetRasterBand(iBand)

        data = inband.ReadAsArray(0, 0)
        if transformation == 'forward':
            data_tr = FFT.fft2d(data)
        else:
            data_tr = FFT.inverse_fft2d(data)
        outband.WriteArray(data_tr)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
