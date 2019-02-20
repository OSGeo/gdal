#!/usr/bin/env pytest
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

from osgeo import gdal


import gdaltest
import pytest

###############################################################################
# Read truncated VICAR file


def test_vicar_1():

    tst = gdaltest.GDALTest('VICAR', 'test_vicar_truncated.bin', 1, 0)
    expected_prj = """PROJCS["SINUSOIDAL MARS",
    GEOGCS["GCS_MARS",
        DATUM["D_MARS",
            SPHEROID["MARS",3396000,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Sinusoidal"],
    PARAMETER["longitude_of_center",137],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["meter",1]]]"""
    tst.testOpen(check_prj=expected_prj, skip_checksum=True)

    ds = gdal.Open('data/test_vicar_truncated.bin')
    expected_gt = (-53985.0, 25.0, 0.0, -200805.0, 0.0, -25.0)
    got_gt = ds.GetGeoTransform()
    for i in range(6):
        assert abs(got_gt[i] - expected_gt[i]) <= 1e-8

    assert ds.GetRasterBand(1).GetNoDataValue() == 0
    assert abs(ds.GetRasterBand(1).GetScale() - 2.34) <= 1e-5
    assert abs(ds.GetRasterBand(1).GetOffset() - 4.56) <= 1e-5

    expected_md = {'DLRTO8.REFLECTANCE_OFFSET': '4.56', 'PRODUCT_TYPE': 'IMAGE', 'PIXEL-SHIFT-BUG': 'CORRECTED', 'M94_ORBIT.STOP_TIME': 'stop_time', 'FILE.EVENT_TYPE': 'EVENT_TYPE', 'M94_CAMERAS.MACROPIXEL_SIZE': '1', 'M94_INSTRUMENT.DETECTOR_ID': 'MEX_HRSC_NADIR', 'HRORTHO.SPICE_FILE_NAME': 'SPICE_FILE_NAME', 'DLRTO8.RADIANCE_SCALING_FACTOR': '1.23', 'CONVERSION_DETAILS': 'http://www.lpi.usra.edu/meetings/lpsc2014/pdf/1088.pdf', 'HRORTHO.GEOMETRIC_CALIB_FILE_NAME': 'calib_file_name', 'HRORTHO.EXTORI_FILE_NAME': "extori'_file_name", 'M94_INSTRUMENT.MISSION_PHASE_NAME': 'MISSION_PHASE_NAME', 'HRCONVER.MISSING_FRAMES': '0', 'DLRTO8.RADIANCE_OFFSET': '1.23', 'HRCONVER.OVERFLOW_FRAMES': '0', 'SPACECRAFT_NAME': 'MARS EXPRESS', 'HRFOOT.BEST_GROUND_SAMPLING_DISTANCE': '1.23', 'M94_ORBIT.START_TIME': 'start_time', 'HRORTHO.DTM_NAME': 'dtm_name', 'DLRTO8.REFLECTANCE_SCALING_FACTOR': '2.34', 'HRCONVER.ERROR_FRAMES': '1'}
    md = ds.GetMetadata()
    if len(md) != len(expected_md):
        print(sorted(md.keys()))
        pytest.fail(sorted(expected_md.keys()))
    for key in expected_md:
        assert md[key] == expected_md[key]

    


