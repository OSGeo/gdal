#!/usr/bin/env pytest
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



from osgeo import gdal

###############################################################################
# Simple try of identify driver on a tiff file.


def test_identify_1():

    file_list = gdal.ReadDir('data')

    dr = gdal.IdentifyDriver('data/byte.tif', file_list)
    assert dr is not None and dr.GetDescription() == 'GTiff', \
        'Got wrong driver for byte.tif'

###############################################################################
# Test a file that won't be recognised.


def test_identify_2():

    file_list = gdal.ReadDir('data')

    dr = gdal.IdentifyDriver('data/byte.pnm.aux.xml', file_list)
    assert dr is None, 'Got a driver for byte.pnm.aux.xml!'

###############################################################################
# Try identify on a directory.


def test_identify_3():

    dr = gdal.IdentifyDriver('data')
    assert dr is None, 'Got a driver for data directory!'

###############################################################################
# Try IdentifyDriverEx


def test_identify_4():

    dr = gdal.IdentifyDriverEx('data/byte.tif')
    assert dr is not None and dr.GetDescription() == 'GTiff', \
        'Got wrong driver for byte.tif'

    dr = gdal.IdentifyDriverEx('data/byte.tif', gdal.OF_RASTER)
    assert dr is not None and dr.GetDescription() == 'GTiff', \
        'Got wrong driver for byte.tif'

    dr = gdal.IdentifyDriverEx('data/byte.tif', gdal.OF_VECTOR)
    assert dr is None, 'Got wrong driver for byte.tif'

    dr = gdal.IdentifyDriverEx('data/byte.tif', allowed_drivers=['HFA'])
    assert dr is None, 'Got wrong driver for byte.tif'

    dr = gdal.IdentifyDriverEx('../gdrivers/data/envi/aea.dat', sibling_files=['aea.dat'])
    assert dr is None, 'Got a driver, which was not expected!'

    dr = gdal.IdentifyDriverEx('../gdrivers/data/envi/aea.dat', sibling_files=['aea.dat', 'aea.hdr'])
    assert dr is not None, 'Did not get a driver!'



