#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test functioning of the IdentifyDriver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal

###############################################################################
# Simple try of identify driver on a tiff file.

def identify_1():

    file_list = gdal.ReadDir( 'data' )

    dr = gdal.IdentifyDriver( 'data/byte.tif', file_list )
    if dr is None or dr.GetDescription() != 'GTiff':
        gdaltest.post_reason( 'Got wrong driver for byte.tif' )
        return 'fail'

    return 'success'

###############################################################################
# Test a file that won't be recognised.

def identify_2():

    file_list = gdal.ReadDir( 'data' )

    dr = gdal.IdentifyDriver( 'data/byte.pnm.aux.xml', file_list )
    if dr is not None:
        gdaltest.post_reason( 'Got a driver for byte.pnm.aux.xml!' )
        return 'fail'

    return 'success'

###############################################################################
# Try identify on a directory.

def identify_3():

    dr = gdal.IdentifyDriver( 'data' )
    if dr is not None:
        gdaltest.post_reason( 'Got a driver for data directory!' )
        return 'fail'

    return 'success'

###############################################################################
# Try IdentifyDriverEx

def identify_4():

    dr = gdal.IdentifyDriverEx( 'data/byte.tif' )
    if dr is None or dr.GetDescription() != 'GTiff':
        gdaltest.post_reason( 'Got wrong driver for byte.tif' )
        return 'fail'

    dr = gdal.IdentifyDriverEx( 'data/byte.tif', gdal.OF_RASTER )
    if dr is None or dr.GetDescription() != 'GTiff':
        gdaltest.post_reason( 'Got wrong driver for byte.tif' )
        return 'fail'

    dr = gdal.IdentifyDriverEx( 'data/byte.tif', gdal.OF_VECTOR )
    if dr is not None:
        gdaltest.post_reason( 'Got wrong driver for byte.tif' )
        return 'fail'

    dr = gdal.IdentifyDriverEx( 'data/byte.tif', allowed_drivers = [ 'HFA' ] )
    if dr is not None:
        gdaltest.post_reason( 'Got wrong driver for byte.tif' )
        return 'fail'

    dr = gdal.IdentifyDriverEx( '../gdrivers/data/aea.dat', sibling_files = [ 'aea.dat'] )
    if dr is not None:
        gdaltest.post_reason( 'Got a driver, which was not expected!' )
        return 'fail'

    dr = gdal.IdentifyDriverEx( '../gdrivers/data/aea.dat', sibling_files = [ 'aea.dat', 'aea.hdr' ] )
    if dr is None:
        gdaltest.post_reason( 'Did not get a driver!' )
        return 'fail'

    return 'success'

gdaltest_list = [
    identify_1,
    identify_2,
    identify_3,
    identify_4 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'identify' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

