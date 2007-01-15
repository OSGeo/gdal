#!/usr/bin/env python
###############################################################################
# $Id: numpy_rw_ng.py,v 1.2 2006/11/30 04:17:28 hobu Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic integration with Numeric Python.
# Author:   Frank Warmerdam, warmerdam@pobox.com
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
# 
#  $Log: numpy_rw_ng.py,v $
#  Revision 1.2  2006/11/30 04:17:28  hobu
#  don't always fail
#
#  Revision 1.1  2006/11/15 23:58:05  hobu
#  tests for ng numpy stuff
#
#  Revision 1.4  2006/10/27 04:31:51  fwarmerdam
#  corrected licenses
#
#  Revision 1.3  2006/04/28 03:40:05  fwarmerdam
#  Just skip if we can't import gdalnumeric.
#
#  Revision 1.2  2004/11/11 04:21:44  fwarmerdam
#  converted to unix text format, added allregister
#
#  Revision 1.1  2003/09/26 15:55:18  warmerda
#  New
#
#

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import gdal

###############################################################################
# verify that we can load Numeric python, and find the Numpy driver.

def numpy_rw_1():
    
    gdaltest.numpy_drv = None
    try:
        import numpy
        import gdalnumeric
        import _gdal_array
    except ImportError:
        return 'skip'

    #import _gdal
    gdal.AllRegister()
    _gdal_array.GDALRegister_NUMPY()

    gdaltest.numpy_drv = gdal.GetDriverByName( 'NUMPY' )
    if gdaltest.numpy_drv is None:
        gdaltest.post_reason( 'NUMPY driver not found!' )
        return 'fail'
    
    return 'success'

###############################################################################
# Load a test file into a memory Numpy array, and verify the checksum.

def numpy_rw_2():

    if gdaltest.numpy_drv is None:
        return 'skip'

    import gdalnumeric

    array = gdalnumeric.LoadFile( 'data/utmsmall.tif' )
    if array is None:
        gdaltest.post_reason( 'Failed to load utmsmall.tif into array')
        return 'fail'

    ds = gdalnumeric.OpenArray( array )
    if ds is None:
        gdaltest.post_reason( 'Failed to open memory array as dataset.' )
        return 'fail'

    bnd = ds.GetRasterBand(1)
    if bnd.Checksum() != 50054:
        gdaltest.post_reason( 'Didnt get expected checksum on reopened file')
        return 'fail'
    ds = None

    return 'success'


def numpy_rw_cleanup():
    gdaltest.numpy_drv = None

    return 'success'

gdaltest_list = [
    numpy_rw_1,
    numpy_rw_2,
    numpy_rw_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'numpy_rw' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

