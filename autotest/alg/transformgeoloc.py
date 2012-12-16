#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test TransformGeoloc algorithm.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2012, Frank Warmerdam <warmerdam@pobox.com>
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

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal, osr

###############################################################################
# Test a fairly default case.

def transformgeoloc_1():

    try:
        from osgeo import gdalnumeric
        gdalnumeric.zeros
    except:
        try:
            import osgeo.gdal_array as gdalnumeric
        except ImportError:
            return 'skip'

    # Setup 2x2 geolocation arrays in a memory dataset with lat/long values.

    drv = gdal.GetDriverByName('MEM')
    geoloc_ds = drv.Create('geoloc_1',2,2,3,gdal.GDT_Float64)

    lon_array = gdalnumeric.asarray([[-117.0,-116.0],
                                     [-116.5, -115.5]])
    lat_array = gdalnumeric.asarray([[45.0, 45.5],
                                     [44.0, 44.5]])
    
    geoloc_ds.GetRasterBand(1).WriteArray(lon_array)
    geoloc_ds.GetRasterBand(2).WriteArray(lat_array)
    # Z left as default zero.

    # Create a wgs84 to utm transformer.
    
    wgs84_wkt = osr.GetUserInputAsWKT('WGS84')
    utm_wkt = osr.GetUserInputAsWKT('+proj=utm +zone=11 +datum=WGS84')
    
    ll_utm_transformer = gdal.Transformer(None, None,
                                          ['SRC_SRS='+wgs84_wkt,
                                           'DST_SRS='+utm_wkt] )

    # transform the geoloc dataset in place.
    status = ll_utm_transformer.TransformGeolocations(
        geoloc_ds.GetRasterBand(1),
        geoloc_ds.GetRasterBand(2),
        geoloc_ds.GetRasterBand(3))

    print(status)

    print(geoloc_ds.ReadAsArray())

    return 'success' 


gdaltest_list = [
    transformgeoloc_1,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'transform_geoloc' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

