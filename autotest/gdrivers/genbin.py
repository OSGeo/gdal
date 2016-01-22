#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Generic Binary format driver.
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

import os
import sys
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Perform simple read test.

def genbin_1():

    tst = gdaltest.GDALTest( 'GenBin', 'tm4628_96.bil', 1, 5738,
                             0, 0, 500, 1 )

    prj = """PROJCS["NAD27 / Washington South",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke 1866",6378206.4,294.9786982138982,
                AUTHORITY["EPSG","7008"]],
            AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.01745329251994328,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4267"]],
    UNIT["US survey foot",0.3048006096012192,
        AUTHORITY["EPSG","9003"]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",45.83333333333334],
    PARAMETER["standard_parallel_2",47.33333333333334],
    PARAMETER["latitude_of_origin",45.33333333333334],
    PARAMETER["central_meridian",-120.5],
    PARAMETER["false_easting",2000000],
    PARAMETER["false_northing",0],
    AUTHORITY["EPSG","32049"],
    AXIS["X",EAST],
    AXIS["Y",NORTH]]"""

    gt = (1181700.9894981384, 82.021003723042099, 0.0,
          596254.01050186157, 0.0, -82.021003723045894 )
    
    return tst.testOpen( check_prj = prj, check_gt = gt )

gdaltest_list = [
    genbin_1
    ]
  


if __name__ == '__main__':

    gdaltest.setup_run( 'genbin' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

