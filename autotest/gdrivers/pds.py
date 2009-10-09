#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for PDS driver.
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
import array
import string

sys.path.append( '../pymod' )

import gdaltest
import osr

###############################################################################
# Read a truncated and modified version of http://download.osgeo.org/gdal/data/pds/mc02.img

def pds_1():

    tst = gdaltest.GDALTest( 'PDS', 'mc02_truncated.img', 1, 47151 )
    expected_prj = """PROJCS["SIMPLE_CYLINDRICAL "MARS"",GEOGCS["GCS_"MARS"",DATUM["D_"MARS"",SPHEROID[""MARS"",3396000,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Equirectangular"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",0],PARAMETER["false_easting",0],PARAMETER["false_northing",0],PARAMETER["pseudo_standard_parallel_1",0]]"""
    expected_gt = (-10668384.903788566589355,926.115274429321289,0,3852176.483988761901855,0,-926.115274429321289)
    return tst.testOpen( check_prj = expected_prj,
                         check_gt = expected_gt )


###############################################################################
# Read a truncated and modified version of ftp://pdsimage2.wr.usgs.gov/cdroms/magellan/mg_1103/fl78n018/fl73n003.img

def pds_2():

    tst = gdaltest.GDALTest( 'PDS', 'fl73n003_truncated.img', 1, 34962 )
    expected_prj = """PROJCS["SINUSOIDAL VENUS",
    GEOGCS["GCS_VENUS",
        DATUM["D_VENUS",
            SPHEROID["VENUS",6051000,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Sinusoidal"],
    PARAMETER["longitude_of_center",18],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0]]"""
    expected_gt = (587861.55900404998, 75.000002980232239, 0.0, -7815243.4746123618, 0.0, -75.000002980232239)
    if tst.testOpen( check_prj = expected_prj,
                     check_gt = expected_gt ) != 'success':
        return 'fail'

    ds = gdal.Open('data/fl73n003_truncated.img')
    if ds.GetRasterBand(1).GetNoDataValue() != 0:
        return 'fail'
    if ds.GetRasterBand(1).GetScale() != 0.2:
        return 'fail'
    if ds.GetRasterBand(1).GetOffset() != -20.2:
        return 'fail'

    return 'success'

###############################################################################
# Read a truncated and modified version of ftp://pdsimage2.wr.usgs.gov/cdroms/messenger/MSGRMDS_1001/DATA/2004_232/EN0001426030M.IMG
# 16bits image

def pds_3():

    # Shut down warning about missing projection
    gdal.PushErrorHandler('CPLQuietErrorHandler')

    tst = gdaltest.GDALTest( 'PDS', 'EN0001426030M_truncated.IMG', 1, 1367 )
    if tst.testOpen( ) != 'success':
        return 'fail'

    ds = gdal.Open('data/EN0001426030M_truncated.IMG')
    if ds.GetRasterBand(1).GetNoDataValue() != -32768:
        return 'fail'

    gdal.PopErrorHandler()

    return 'success'

###############################################################################
# Read a hacked example of reading a detached file with an offset per #3177.

def pds_4():

    tst = gdaltest.GDALTest( 'PDS', 'pds_3177.lbl', 1, 4028 )
    return tst.testOpen()

gdaltest_list = [
    pds_1,
    pds_2,
    pds_3,
    pds_4 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'pds' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

