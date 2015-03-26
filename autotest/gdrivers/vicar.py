#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for PDS driver.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
# 
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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
# Read truncated VICAR file

def vicar_1():

    tst = gdaltest.GDALTest( 'VICAR', 'test_vicar_truncated.bin', 1, 0 )
    expected_prj = """PROJCS["SINUSOIDAL MARS",
    GEOGCS["GCS_MARS",
        DATUM["D_MARS",
            SPHEROID["MARS",3396000,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Sinusoidal"],
    PARAMETER["longitude_of_center",137],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0]]"""
    expected_gt = (-53960.0, 25.0, 0.0, -200830.0, 0.0, -25.0)
    if tst.testOpen( check_prj = expected_prj,
                     check_gt = expected_gt, skip_checksum = True ) != 'success':
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.Open('data/test_vicar_truncated.bin')
    if ds.GetRasterBand(1).GetNoDataValue() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if abs(ds.GetRasterBand(1).GetScale() - 2.34) > 1e-5:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetScale())
        return 'fail'
    if abs(ds.GetRasterBand(1).GetOffset() - 4.56) > 1e-5:
        gdaltest.post_reason('fail')
        return 'fail'

    expected_md = {'DLRTO8.RADIANCE_SCALING_FACTOR': '1.23', 'PRODUCT_TYPE': 'IMAGE', 'CONVERSION_DETAILS': 'http://www.lpi.usra.edu/meetings/lpsc2014/pdf/1088.pdf', 'HRORTHO.DTM_NAME': 'dtm_name', 'DLRTO8.REFLECTANCE_SCALING_FACTOR': '2.34', 'HRORTHO.GEOMETRIC_CALIB_FILE_NAME': 'calib_file_name', 'HRORTHO.EXTORI_FILE_NAME': 'extori_file_name', 'DLRTO8.REFLECTANCE_OFFSET': '4.56', 'HRFOOT.BEST_GROUND_SAMPLING_DISTANCE': '1.23', 'M94_INSTRUMENT.MISSION_PHASE_NAME': 'MISSION_PHASE_NAME', 'HRCONVER.MISSING_FRAMES': '0', 'DLRTO8.RADIANCE_OFFSET': '1.23', 'SPACECRAFT_NAME': 'MARS EXPRESS', 'M94_ORBIT.STOP_TIME': 'stop_time', 'FILE.EVENT_TYPE': 'EVENT_TYPE', 'HRCONVER.OVERFLOW_FRAMES': '0', 'M94_CAMERAS.MACROPIXEL_SIZE': '1', 'HRCONVER.ERROR_FRAMES': '1', 'M94_INSTRUMENT.DETECTOR_ID': 'MEX_HRSC_NADIR', 'M94_ORBIT.START_TIME': 'start_time', 'HRORTHO.SPICE_FILE_NAME': 'SPICE_FILE_NAME'}
    md = ds.GetMetadata()
    if len(md) != len(expected_md):
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    for key in expected_md:
        if md[key] != expected_md[key]:
            gdaltest.post_reason('fail')
            print(md)
            return 'fail'

    return 'success'

gdaltest_list = [
    vicar_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vicar' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

