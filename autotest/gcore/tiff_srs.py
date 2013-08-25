#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write round-tripping of SRS for GeoTIFF format.
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2011, Even Rouault, <even dot rouault at mines dash paris dot org>
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
from osgeo import osr

sys.path.append( '../pymod' )

import gdaltest


###############################################################################
# Write a geotiff and read it back to check its SRS

class TestTiffSRS:
    def __init__( self, epsg_code, use_epsg_code, expected_fail ):
        self.epsg_code = epsg_code
        self.use_epsg_code = use_epsg_code
        self.expected_fail = expected_fail

    def test( self ):
        sr = osr.SpatialReference()
        sr.ImportFromEPSG(self.epsg_code)
        if self.use_epsg_code == 0:
            proj4str = sr.ExportToProj4()
            #print(proj4str)
            sr.SetFromUserInput(proj4str)

        ds = gdal.GetDriverByName('GTiff').Create('/vsimem/TestTiffSRS.tif',1,1)
        ds.SetProjection(sr.ExportToWkt())
        ds = None

        ds = gdal.Open('/vsimem/TestTiffSRS.tif')
        wkt = ds.GetProjectionRef()
        sr2 = osr.SpatialReference()
        sr2.SetFromUserInput(wkt)
        ds = None

        gdal.Unlink('/vsimem/TestTiffSRS.tif')

        if sr.IsSame(sr2) != 1:
            if self.expected_fail:
                print('did not get expected SRS. known to be broken currently. FIXME!')
                #print(sr)
                #print(sr2)
                return 'expected_fail'

            gdaltest.post_reason('did not get expected SRS')
            print(sr)
            print(sr2)
            return 'fail'

        return 'success'

###############################################################################
# Test fix for #4677:
def tiff_srs_without_linear_units():

    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=vandg +datum=WGS84')

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tiff_srs_without_linear_units.tif',1,1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open('/vsimem/tiff_srs_without_linear_units.tif')
    wkt = ds.GetProjectionRef()
    sr2 = osr.SpatialReference()
    sr2.SetFromUserInput(wkt)
    ds = None

    gdal.Unlink('/vsimem/tiff_srs_without_linear_units.tif')

    if sr.IsSame(sr2) != 1:

        gdaltest.post_reason('did not get expected SRS')
        print(sr)
        print(sr2)
        return 'fail'

    return 'success'


gdaltest_list = []

tiff_srs_list = [ 2758, #tmerc
                  2036, #sterea
                  2046, #tmerc
                  [3031, True, False], #stere
                  32661, #stere
                  3035, #laea
                  2062, #lcc
                  [2065, True, True], #krovak
                  [2066, False, True], #cass
                  2964, #aea
                  3410, #cea
                  [3786, True, False], #eqc
                  2934, #merc
                  27200, #nzmg
                  2057, #omerc
                  [29100, True, False], #poly
                  2056, #somerc
                  2027, #utm
                  4326, #longlat
                  26943, #utm
]

for item in tiff_srs_list:
    try:
        epsg_code = item[0]
        epsg_broken = item[1]
        epsg_proj4_broken = item[2]
    except:
        epsg_code = item
        epsg_broken = False
        epsg_proj4_broken = False

    ut = TestTiffSRS( epsg_code, 1, epsg_broken )
    gdaltest_list.append( (ut.test, "tiff_srs_epsg_%d" % epsg_code) )
    ut = TestTiffSRS( epsg_code, 0, epsg_proj4_broken )
    gdaltest_list.append( (ut.test, "tiff_srs_proj4_of_epsg_%d" % epsg_code) )

gdaltest_list.append( tiff_srs_without_linear_units )


if __name__ == '__main__':

    gdaltest.setup_run( 'tiff_srs' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

