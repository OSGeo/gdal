#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for MrSID driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
# Read a simple byte file, checking projections and geotransform.

def mrsid_1():

    gdaltest.mrsid_drv = gdal.GetDriverByName( 'MrSID' )
    if gdaltest.mrsid_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'MrSID', 'mercator.sid', 1, 54132 )

    gt = (-15436.385771224039, 60.0, 0.0, 3321987.8617962394, 0.0, -60.0)
    #
    # Old, internally generated.
    # 
    prj = """PROJCS["MER         E000|",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982138982,
                AUTHORITY["EPSG","7008"]],
            AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4267"]],
    PROJECTION["Mercator_1SP"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]"""
    #
    # MrSID SDK getWKT() method.
    # 
    prj = """PROJCS["MER         E000|",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982139006,
                AUTHORITY["EPSG","7008"]],
            AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4267"]],
    PROJECTION["Mercator_1SP"],
    PARAMETER["latitude_of_origin",1],
    PARAMETER["central_meridian",1],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",1],
    PARAMETER["false_northing",1],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]"""
    
    return tst.testOpen( check_gt = gt, check_prj = prj, \
        check_stat = (0.0, 255.0, 103.319, 55.153), \
        check_approx_stat = (2.0, 243.0, 103.131, 43.978) )

###############################################################################
# Do a direct IO to read the image at a resolution for which there is no
# builtin overview.  Checks for the bug Steve L found in the optimized
# RasterIO implementation.

def mrsid_2():

    if gdaltest.mrsid_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/mercator.sid' )

    try:
        data = ds.ReadRaster( 0, 0, 515, 515, buf_xsize = 10, buf_ysize = 10 )
    except:
        gdaltest.post_reason( 'Small overview read failed: ' + gdal.GetLastErrorMsg() )
        return 'fail'

    ds = None
    
    # check that we got roughly the right values by checking mean.
    sum = 0
    for i in range(len(data)):
        sum = sum + ord(data[i])

    mean = float(sum) / len(data)

    if mean < 95 or mean > 105:
        gdaltest.post_reason( 'image mean out of range.' )
        return 'fail'
    
    return 'success'

###############################################################################
# Test overview reading.

def mrsid_3():

    if gdaltest.mrsid_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/mercator.sid' )

    band = ds.GetRasterBand(1)
    if band.GetOverviewCount() != 4:
        gdaltest.post_reason( 'did not get expected overview count' )
        return 'fail'

    cs = band.GetOverview(3).Checksum()
    if cs != 12816:
        gdaltest.post_reason( 'wrong overview checksum (%d)' % cs )
        return 'fail'

    return 'success'

###############################################################################
# Check a new (V3) file which uses a different form for coordinate sys.

def mrsid_4():

    if gdaltest.mrsid_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'MrSID', 'mercator_new.sid', 1, 26456 )

    gt = (-15436.385771224039, 60.0, 0.0, 3321987.8617962394, 0.0, -60.0)
    prj = """PROJCS["MER         E000",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982138982,
                AUTHORITY["EPSG","7008"]],
            AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4267"]],
    PROJECTION["Mercator_1SP"],
    PARAMETER["latitude_of_origin",33.76446202777777],
    PARAMETER["central_meridian",-117.4745428888889],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]]]"""
    
    return tst.testOpen( check_gt = gt, check_prj = prj, \
        check_stat = (0.0, 255.0, 103.112, 52.477), \
        check_approx_stat = (0.0, 255.0, 102.684, 51.614) )

###############################################################################
# Cleanup.

def mrsid_cleanup():
    gdaltest.mrsid_drv = None

    return 'success'

gdaltest_list = [
    mrsid_1,
    mrsid_2,
    mrsid_3,
    mrsid_4,
    mrsid_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'mrsid' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

