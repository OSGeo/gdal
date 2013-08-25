#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for L1B driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at mines dash paris dot org>
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
# 
class TestL1B:
    def __init__( self, downloadURL, fileName, checksum, download_size, gcpNumber ):
        self.downloadURL = downloadURL
        self.fileName = fileName
        self.checksum = checksum
        self.download_size = download_size
        self.gcpNumber = gcpNumber

    def test( self ):
        if not gdaltest.download_file(self.downloadURL + '/' + self.fileName, self.fileName, self.download_size):
            return 'skip'

        ds = gdal.Open('tmp/cache/' + self.fileName)

        if ds.GetRasterBand(1).Checksum() != self.checksum:
            gdaltest.post_reason('Bad checksum. Expected %d, got %d' % (self.checksum, ds.GetRasterBand(1).Checksum()))
            return 'failure'

        if len(ds.GetGCPs()) != self.gcpNumber:
            gdaltest.post_reason('Bad GCP number. Expected %d, got %d' % (self.gcpNumber, len(ds.GetGCPs())))
            return 'failure'

        return 'success'



gdaltest_list = []

l1b_list = [ ('http://download.osgeo.org/gdal/data/l1b', 'n12gac8bit.l1b', 51754, -1, 220),
             ('http://download.osgeo.org/gdal/data/l1b', 'n12gac10bit.l1b', 46039, -1, 220),
             ('http://download.osgeo.org/gdal/data/l1b', 'n12gac10bit_ebcdic.l1b', 46039, -1, 220), # 2848
             ('http://download.osgeo.org/gdal/data/l1b', 'n14gac16bit.l1b', 42286, -1, 220),
             ('http://download.osgeo.org/gdal/data/l1b', 'n15gac8bit.l1b', 55772, -1, 220),
             ('http://download.osgeo.org/gdal/data/l1b', 'n16gac10bit.l1b', 6749, -1, 220),
             ('http://download.osgeo.org/gdal/data/l1b', 'n17gac16bit.l1b', 61561, -1, 220),
             ('http://www2.ncdc.noaa.gov/docs/podug/data/avhrr', 'frang.1b', 33700, 30000, 220),  # 10 bit guess
             ('http://www2.ncdc.noaa.gov/docs/podug/data/avhrr', 'franh.1b', 56702, 100000, 220), # 10 bit guess
             ('http://www2.ncdc.noaa.gov/docs/podug/data/avhrr', 'calfirel.1b', 55071, 30000, 220), # 16 bit guess
             ('http://www2.ncdc.noaa.gov/docs/podug/data/avhrr', 'rapnzg.1b', 58084, 30000, 220), # 16 bit guess
             ('ftp://ftp.sat.dundee.ac.uk/misc/testdata/new_noaa/new_klm_format', 'noaa18.n1b', 50229, 50000, 220),
             ('ftp://ftp.sat.dundee.ac.uk/misc/testdata/metop', 'noaa1b', 62411, 150000, 220)
           ]

for item in l1b_list:
    ut = TestL1B( item[0], item[1], item[2], item[3], item[4] )
    gdaltest_list.append( (ut.test, item[1]) )


if __name__ == '__main__':

    gdaltest.setup_run( 'l1b' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

