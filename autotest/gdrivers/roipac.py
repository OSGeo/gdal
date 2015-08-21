#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ROI_PAC format driver.
# Author:   Matthieu Volat <matthieu.volat@ujf-grenoble.fr>
#
###############################################################################
# Copyright (c) 2014, Matthieu Volat <matthieu.volat@ujf-grenoble.fr>
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
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Perform simple read test.
def roipac_1():

    tst = gdaltest.GDALTest( 'roi_pac', 'srtm.dem', 1, 64074 )

    prj = """GEOGCS["WGS 84",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        TOWGS84[0,0,0,0,0,0,0],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["degree",0.0174532925199433,
        AUTHORITY["EPSG","9108"]],
    AUTHORITY["EPSG","4326"]]"""
    
    return tst.testOpen( check_prj = prj,
                         check_gt = (-180.0083333, 0.0083333333, 0.0,
                                     -59.9916667, 0.0, -0.0083333333) )

###############################################################################
# Test reading of metadata from the ROI_PAC metadata domain 

def roipac_2():

    ds = gdal.Open( 'data/srtm.dem' )
    val = ds.GetMetadataItem( 'Z_SCALE', 'ROI_PAC' )
    if val != '1':
        return 'fail'

    return 'success'

###############################################################################
# Verify this can be exported losslessly.

def roipac_3():

    tst = gdaltest.GDALTest( 'roi_pac', 'srtm.dem', 1, 64074 )
    return tst.testCreateCopy( check_gt = 1, new_filename = 'strm.tst.dem' )

###############################################################################
# Verify VSIF*L capacity

def roipac_4():

    tst = gdaltest.GDALTest( 'roi_pac', 'srtm.dem', 1, 64074 )
    return tst.testCreateCopy( check_gt = 1, new_filename = 'strm.tst.dem', vsimem = 1 )

gdaltest_list = [
    roipac_1,
    roipac_2,
    roipac_3,
    roipac_4,
    ]
  


if __name__ == '__main__':

    gdaltest.setup_run( 'roipac' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

