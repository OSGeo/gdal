#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Terrage Testing.
# Author:   Even Rouault, <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2017, Even Rouault, <even.rouault at spatialys.com>
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



from osgeo import gdal
import gdaltest

###############################################################################


def test_terragen_1():

    tst = gdaltest.GDALTest('terragen', 'terragen/float32.ter', 1, 1128)

    return tst.testOpen()

###############################################################################
# Write


def test_terragen_2():

    gdal.Translate('/vsimem/out.ter', 'data/float32.tif', options='-of TERRAGEN -co MINUSERPIXELVALUE=74 -co MAXUSERPIXELVALUE=255')
    gdal.Translate('/vsimem/out.tif', '/vsimem/out.ter', options='-unscale')
    ds = gdal.Open('/vsimem/out.tif')
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None
    gdal.GetDriverByName('TERRAGEN').Delete('/vsimem/out.ter')
    gdal.GetDriverByName('TERRAGEN').Delete('/vsimem/out.tif')




