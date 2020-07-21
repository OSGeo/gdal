#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for DOQ2 driver.
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
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



###############################################################################
# Read a truncated and modified version of C3607614.NWS
# downloaded from http://edcftp.cr.usgs.gov/pub/data/samples/doq-clr-native.tar.gz


def test_doq2_1():

    ds = gdal.Open('data/doq2/C3607614_truncated.NWS')

    mem_ds = gdal.GetDriverByName('MEM').Create('mem_1.mem', 500, 1, gdal.GDT_Byte, 1)

    mem_ds.GetRasterBand(1).WriteRaster(0, 0, 500, 1, ds.GetRasterBand(1).ReadRaster(0, 0, 500, 1))
    assert mem_ds.GetRasterBand(1).Checksum() == 4201, 'wrong checksum for band 1'

    mem_ds.GetRasterBand(1).WriteRaster(0, 0, 500, 1, ds.GetRasterBand(2).ReadRaster(0, 0, 500, 1))
    assert mem_ds.GetRasterBand(1).Checksum() == 4010, 'wrong checksum for band 2'

    mem_ds.GetRasterBand(1).WriteRaster(0, 0, 500, 1, ds.GetRasterBand(3).ReadRaster(0, 0, 500, 1))
    assert mem_ds.GetRasterBand(1).Checksum() == 5820, 'wrong checksum for band 3'

    assert ds.GetGeoTransform() == (377054, 1, 0, 4082205, 0, -1), 'wrong geotransform'

    md = ds.GetMetadata()
    assert md['QUADRANGLE_NAME'] == 'NORFOLK SOUTH 3.45 or 7.5-min. name*', \
        'wrong metadata'

    mem_ds = None
    ds = None



