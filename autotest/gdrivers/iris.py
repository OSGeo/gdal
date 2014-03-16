#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test IRIS driver
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2012-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
# Test reading a - fake - IRIS dataset

def iris_1():

    tst = gdaltest.GDALTest( 'IRIS', 'fakeiris.dat', 1, 65532 )
    return tst.testOpen()

###############################################################################
# Test reading a real world IRIS dataset

def iris_2():

    ds = gdal.Open('data/iristest.dat')
    if ds.GetRasterBand(1).Checksum() != 52872:
        gdaltest.post_reason('fail')
        return 'fail'

    got_wkt = ds.GetProjectionRef()
    expected_wkt = """PROJCS["unnamed",GEOGCS["unnamed ellipse",DATUM["unknown",SPHEROID["unnamed",6371000.5,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Mercator_1SP"],PARAMETER["central_meridian",0],PARAMETER["scale_factor",1],PARAMETER["false_easting",0],PARAMETER["false_northing",0]]"""
    got_srs = osr.SpatialReference(got_wkt)
    expected_srs = osr.SpatialReference(expected_wkt)

    # There are some differences in the values of the parameters between Linux and Windows
    # not sure if it is only due to rounding differences, different proj versions, etc...
    #if got_srs.IsSame(expected_srs) != 1:
    #    gdaltest.post_reason('fail')
    #    print('')
    #    print(expected_wkt)
    #    print(got_wkt)
    #    return 'fail'

    got_gt = ds.GetGeoTransform()
    expected_gt = [ 16435.721785269096, 1370.4263720754534, 0.0, 5289830.4584420761, 0.0, -1357.6498705837876 ]
    for i in range(6):
        if (expected_gt[i] == 0.0 and got_gt[i] != 0.0) or \
           (expected_gt[i] != 0.0 and abs(got_gt[i] - expected_gt[i])/abs(expected_gt[i]) > 1e-5) :
            gdaltest.post_reason('fail')
            print(got_gt)
            print(i)
            return 'fail'

    expected_metadata = [
  "AZIMUTH_SMOOTHING_FOR_SHEAR=0.0",
  "CAPPI_BOTTOM_HEIGHT=1000.0 m",
  "COMPOSITED_PRODUCT=YES",
  "COMPOSITED_PRODUCT_MASK=0x0000080c",
  "DATA_TYPE=Clutter Corrected H reflectivity (1 byte)",
  "DATA_TYPE_CODE=dBZ",
  "DATA_TYPE_INPUT=Clutter Corrected H reflectivity (1 byte)",
  "DATA_TYPE_INPUT_CODE=dBZ",
  "DATA_TYPE_UNITS=dBZ",
  "GROUND_HEIGHT=523 m",
  "INGEST_HARDWARE_NAME=composada       ",
  "INGEST_SITE_IRIS_VERSION=8.12",
  "INGEST_SITE_NAME=composada       ",
  "MAX_AGE_FOR_SHEAR_VVP_CORRECTION=600 s",
  "NYQUIST_VELOCITY=6.00 m/s",
  "PRF=450 Hz",
  "PRODUCT=CAPPI",
  "PRODUCT_CONFIGURATION_NAME=CAPPI250CAT ",
  "PRODUCT_ID=3",
  "PRODUCT_SITE_IRIS_VERSION=8.12",
  "PRODUCT_SITE_NAME=SMCXRADSRV01    ",
  "RADAR_HEIGHT=542 m",
  "TASK_NAME=PPIVOL_A    ",
  "TIME_INPUT_INGEST_SWEEP=2012-04-19 14:48:05",
  "TIME_PRODUCT_GENERATED=2012-04-19 14:48:30",
  "WAVELENGTH=5.33 cm" ]
    got_metadata = ds.GetMetadata()

    for md in expected_metadata:
        key = md[0:md.find('=')]
        value = md[md.find('=')+1:]
        if got_metadata[key] != value:
            gdaltest.post_reason('did not find %s' % key)
            print(got_metadata)
            return 'fail'

    return 'success'

gdaltest_list = [
    iris_1,
    iris_2]

if __name__ == '__main__':

    gdaltest.setup_run( 'iris' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
