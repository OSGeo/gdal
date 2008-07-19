#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for FITS driver.
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
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# 
def fits_init():
    try:
        gdaltest.fitsDriver = gdal.GetDriverByName('FITS')
    except:
        gdaltest.fitsDriver = None

    if gdaltest.fitsDriver is None:
        return 'skip'

    return 'success'

###############################################################################
# 
class TestFITS:
    def __init__( self, fileName ):
        self.fileName = fileName

    def test( self ):
        if gdaltest.fitsDriver is None:
            return 'skip'

        ds = gdal.Open('../gcore/data/' + self.fileName + '.tif')
        gdaltest.fitsDriver.CreateCopy('tmp/' + self.fileName + '.fits', ds, options = [ 'PAGESIZE=2,2' ] )

        ds2 = gdal.Open('tmp/' + self.fileName + '.fits')
        if ds2.GetRasterBand(1).Checksum() != ds.GetRasterBand(1).Checksum():
            return 'fail'

        if ds2.GetRasterBand(1).DataType != ds.GetRasterBand(1).DataType:
            return 'fail'

        ds2 = None
        gdaltest.fitsDriver.Delete('tmp/' + self.fileName + '.fits')
        return 'success'

###############################################################################
# 
def fits_metadata():
    if gdaltest.fitsDriver is None:
        return 'skip'

    ds = gdal.Open('../gcore/data/byte.tif')
    ds2 = gdaltest.fitsDriver.CreateCopy('tmp/byte.fits', ds )
    md = { 'TEST' : 'test_value' }
    ds2.SetMetadata(md)
    ds2 = None
    try:
        os.unlink('tmp/byte.fits.aux.xml')
    except:
        pass

    ds2 = gdal.Open('tmp/byte.fits')
    md = ds2.GetMetadata()
    ds2 = None

    if md['TEST'] != 'test_value':
        return 'fail'

    ds2 = gdal.Open('tmp/byte.fits', gdal.GA_Update)
    md = { 'TEST2' : 'test_value2' }
    ds2.SetMetadata(md)
    ds2 = None
    try:
        os.unlink('tmp/byte.fits.aux.xml')
    except:
        pass

    ds2 = gdal.Open('tmp/byte.fits')
    md = ds2.GetMetadata()
    ds2 = None

    if md['TEST2'] != 'test_value2':
        return 'fail'

    gdaltest.fitsDriver.Delete('tmp/byte.fits' )

    return 'success'

###############################################################################
#
gdaltest_list = [ fits_init ]

fits_list = [ 'byte', 'int16', 'int32', 'float32', 'float64' ]

for item in fits_list:
    ut = TestFITS( item )
    gdaltest_list.append( (ut.test, item) )

gdaltest_list.append(fits_metadata)

if __name__ == '__main__':

    gdaltest.setup_run( 'fits' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
