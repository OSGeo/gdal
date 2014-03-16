#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for NITF driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

import os
import sys
from osgeo import gdal
import array
import string

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test a small GXF sample

def gxf_1():

    tst = gdaltest.GDALTest( 'GXF', 'small.gxf', 1, 90 )

    return tst.testOpen()

###############################################################################
# Test an other GXF sample (with continuous line)

def gxf_2():

    tst = gdaltest.GDALTest( 'GXF', 'small2.gxf', 1, 65042 )
    wkt = """PROJCS["NAD27 / Ohio North",
    GEOGCS["NAD27",
        DATUM["NAD27",
            SPHEROID["NAD27",6378206.4,294.978699815746]],
        PRIMEM["unnamed",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",40.4333333333],
    PARAMETER["standard_parallel_2",41.7],
    PARAMETER["latitude_of_origin",39.6666666667],
    PARAMETER["central_meridian",82.5],
    PARAMETER["false_easting",609601.22],
    UNIT["ftUS",0.3048006096012]]"""

    return tst.testOpen( check_prj = wkt )

###############################################################################
# 
class TestGXF:
    def __init__( self, downloadURL, fileName, checksum, download_size ):
        self.downloadURL = downloadURL
        self.fileName = fileName
        self.checksum = checksum
        self.download_size = download_size

    def test( self ):
        if not gdaltest.download_file(self.downloadURL + '/' + self.fileName, self.fileName, self.download_size):
            return 'skip'

        ds = gdal.Open('tmp/cache/' + self.fileName)

        if ds.GetRasterBand(1).Checksum() != self.checksum:
            gdaltest.post_reason('Bad checksum. Expected %d, got %d' % (self.checksum, ds.GetRasterBand(1).Checksum()))
            return 'failure'

        return 'success'



gdaltest_list = [ gxf_1, gxf_2 ]

gxf_list = [ ('http://download.osgeo.org/gdal/data/gxf', 'SAMPLE.GXF', 24068, -1),
             ('http://download.osgeo.org/gdal/data/gxf', 'gxf_compressed.gxf', 20120, -1),
             ('http://download.osgeo.org/gdal/data/gxf', 'gxf_text.gxf', 20265, -1),
             ('http://download.osgeo.org/gdal/data/gxf', 'gxf_ul_r.gxf', 19930, -1),
             ('http://download.osgeo.org/gdal/data/gxf', 'latlong.gxf', 12243, -1),
             ('http://download.osgeo.org/gdal/data/gxf', 'spif83.gxf', 28752, -1)
           ]

for item in gxf_list:
    ut = TestGXF( item[0], item[1], item[2], item[3] )
    gdaltest_list.append( (ut.test, item[1]) )


if __name__ == '__main__':

    gdaltest.setup_run( 'gxf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

