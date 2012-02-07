#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OZI projection and datum support
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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
from osgeo import osr

###############################################################################
# Test with WGS 84 datum

def osr_ozi_1():

    srs = osr.SpatialReference()
    srs.ImportFromOzi("WGS 84,WGS 84,   0.0000,   0.0000,WGS 84",
                      "Map Projection,Lambert Conformal Conic,PolyCal,No,AutoCalOnly,No,BSBUseWPX,No",
                      "Projection Setup,     4.000000000,    10.000000000,,,,    40.000000000,    56.000000000,,,")

    expected = 'PROJCS["unnamed",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",40],PARAMETER["standard_parallel_2",56],PARAMETER["latitude_of_origin",4],PARAMETER["central_meridian",10],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["Meter",1]]'

    if not gdaltest.equal_srs_from_wkt( expected, srs.ExportToWkt() ):
        return 'fail'

    return 'success'

###############################################################################
# Test with another datum known by OZI and whose EPSG code is known

def osr_ozi_2():

    srs = osr.SpatialReference()
    srs.ImportFromOzi("Tokyo,",
                      "Map Projection,Latitude/Longitude,,,,,,",
                      "Projection Setup,,,,,,,,,,")

    srs_ref = osr.SpatialReference()
    srs_ref.ImportFromEPSG(4301)

    if not gdaltest.equal_srs_from_wkt( srs_ref.ExportToWkt(), srs.ExportToWkt() ):
        return 'fail'

    return 'success'

###############################################################################
# Test with another datum known by OZI and whose EPSG code is unknown

def osr_ozi_3():

    srs = osr.SpatialReference()
    srs.ImportFromOzi("European 1950 (Mean France),",
                      "Map Projection,Latitude/Longitude,,,,,,",
                      "Projection Setup,,,,,,,,,,")

    expected = 'GEOGCS["European 1950 (Mean France)",DATUM["European 1950 (Mean France)",SPHEROID["International 1924",6378388,297],TOWGS84[-87,-96,-120,0,0,0,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]'

    if not gdaltest.equal_srs_from_wkt( expected, srs.ExportToWkt() ):
        return 'fail'

    return 'success'


gdaltest_list = [ 
    osr_ozi_1,
    osr_ozi_2,
    osr_ozi_3,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_ozi' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

