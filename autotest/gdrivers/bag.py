#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for BAG driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at mines dash paris dot org>,
#                     Frank Warmerdam <warmerdam@pobox.com>
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
import array
import string
import shutil

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test if BAG driver is present

def bag_1():

    try:
        gdaltest.bag_drv = gdal.GetDriverByName( 'BAG' )
    except:
        gdaltest.bag_drv = None
        return 'skip'

    return 'success'

###############################################################################
# Confirm various info on true_n_nominal 1.1 sample file.

def bag_2():

    if gdaltest.bag_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/true_n_nominal.bag' )

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 1072:
        gdaltest.post_reason( 'Wrong checksum on band 1, got %d.' % cs )
        return 'fail'
    
    cs = ds.GetRasterBand(2).Checksum()
    if cs != 150:
        gdaltest.post_reason( 'Wrong checksum on band 2, got %d.' % cs )
        return 'fail'
    
    cs = ds.GetRasterBand(3).Checksum()
    if cs != 1315:
        gdaltest.post_reason( 'Wrong checksum on band 3, got %d.' % cs )
        return 'fail'

    b1 = ds.GetRasterBand(1)
    if abs(b1.GetMinimum()-10) > 0.01:
        gdaltest.post_reason( 'band 1 minimum wrong.' )
        return 'fail'
    
    if abs(b1.GetMaximum()-19.8) > 0.01:
        gdaltest.post_reason( 'band 1 maximum wrong.' )
        return 'fail'
    
    if abs(b1.GetNoDataValue()-1000000.0) > 0.1:
        gdaltest.post_reason( 'band 1 nodata wrong.' )
        return 'fail'

    b2 = ds.GetRasterBand(2)
    if abs(b2.GetNoDataValue()-0.0) > 0.1:
        gdaltest.post_reason( 'band 2 nodata wrong.' )
        return 'fail'

    b3 = ds.GetRasterBand(3)
    if abs(b3.GetNoDataValue()-1000000.0) > 0.1:
        gdaltest.post_reason( 'band 3 nodata wrong.' )
        return 'fail'
    
    # It would be nice to test srs and geotransform but they are
    # pretty much worthless on this dataset.


    # Test the xml:BAG metadata domain
    xmlBag = ds.GetMetadata('xml:BAG')[0]
    if xmlBag.find('<?xml') != 0:
        gdaltest.post_reason( 'did not get xml:BAG metadata' )
        print(xmlBag)
        return 'fail'


    return 'success'

gdaltest_list = [ bag_1,
                  bag_2 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'hdf5' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

