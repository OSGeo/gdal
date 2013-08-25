#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for FIT driver.
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault @ mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# 
class TestFIT:
    def __init__( self, fileName ):
        self.fileName = fileName
        self.fitDriver = gdal.GetDriverByName('FIT')

    def test( self ):

        ds = gdal.Open('../gcore/data/' + self.fileName + '.tif')
        self.fitDriver.CreateCopy('tmp/' + self.fileName + '.fit', ds, options = [ 'PAGESIZE=2,2' ] )

        ds2 = gdal.Open('tmp/' + self.fileName + '.fit')
        if ds2.GetRasterBand(1).Checksum() != ds.GetRasterBand(1).Checksum():
            return 'fail'

        if ds2.GetRasterBand(1).DataType != ds.GetRasterBand(1).DataType:
            return 'fail'

        ds2 = None
        self.fitDriver.Delete('tmp/' + self.fileName + '.fit')
        return 'success'


gdaltest_list = [ ]

fit_list = [ 'byte', 'int16', 'uint16', 'int32', 'uint32', 'float32', 'float64' ]

for item in fit_list:
    ut = TestFIT( item )
    gdaltest_list.append( (ut.test, item) )


if __name__ == '__main__':

    gdaltest.setup_run( 'fit' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
