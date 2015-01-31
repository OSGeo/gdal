#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SNODAS driver
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test a fake SNODAS dataset

def snodas_1():

    tst = gdaltest.GDALTest( 'SNODAS', 'fake_snodas.hdr', 1, 0 )
    expected_gt = [ -124.733749999995, 0.0083333333333330643, 0.0, 52.874583333331302, 0.0, -0.0083333333333330054 ]
    expected_srs = """GEOGCS["WGS 84",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        TOWGS84[0,0,0,0,0,0,0],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["degree",0.0174532925199433,
        AUTHORITY["EPSG","9108"]],
    AUTHORITY["EPSG","4326"]]"""
    ret = tst.testOpen(check_gt = expected_gt, check_prj = expected_srs, skip_checksum = True)

    if ret is 'success':
        ds = gdal.Open('data/fake_snodas.hdr')
        ds.GetFileList()
        if ds.GetRasterBand(1).GetNoDataValue() != -9999:
            print(ds.GetRasterBand(1).GetNoDataValue())
            return 'fail'
        if ds.GetRasterBand(1).GetMinimum() != 0:
            print(ds.GetRasterBand(1).GetMinimum())
            return 'fail'
        if ds.GetRasterBand(1).GetMaximum() != 429:
            print(ds.GetRasterBand(1).GetMaximum())
            return 'fail'

    return ret

gdaltest_list = [
    snodas_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'snodas' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

