#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for ISIS2 driver.
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
from osgeo import gdal, osr
import array
import string

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Read a truncated and modified version of arvidson_original.cub from
# ftp://ftpflag.wr.usgs.gov/dist/pigpen/venus/venustopo_download/ovda_dtm.zip

def isis2_1():

    tst = gdaltest.GDALTest( 'ISIS2', 'arvidson_original_truncated.cub', 1, 382 )
    expected_prj = """PROJCS["SIMPLE_CYLINDRICAL VENUS",
    GEOGCS["GCS_VENUS",
        DATUM["D_VENUS",
            SPHEROID["VENUS",6051000,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Equirectangular"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",0],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0]],
    PARAMETER["standard_parallel_1",-6.5]"""
    expected_gt = (10157400.403618813, 1200.0000476837158, 0.0, -585000.02324581146, 0.0, -1200.0000476837158)
    return tst.testOpen( check_prj = expected_prj,
                         check_gt = expected_gt )


###############################################################################
# Test simple creation on disk.

def isis2_2():

    tst = gdaltest.GDALTest( 'ISIS2', 'byte.tif', 1, 4672 )

    return tst.testCreate()

###############################################################################
# Test a different data type with some options.

def isis2_3():

    tst = gdaltest.GDALTest( 'ISIS2', 'float32.tif', 1, 4672,
                             options = ['LABELING_METHOD=DETACHED', 'IMAGE_EXTENSION=qub'] )

    return tst.testCreateCopy( vsimem=1 )

gdaltest_list = [
    isis2_1,
    isis2_2,
    isis2_3 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'isis2' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

