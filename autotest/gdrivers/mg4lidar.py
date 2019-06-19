#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  MG4Lidar Reading Driver testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
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


import gdaltest
from osgeo import gdal
import pytest

###############################################################################
# Test reading a MG4Lidar file
#


def test_mg4lidar_1():

    drv = gdal.GetDriverByName('MG4Lidar')
    if drv is None:
        pytest.skip()

    if not gdaltest.download_file('http://home.gdal.org/tmp/GDAL_MG4Lidar_Src.zip', 'GDAL_MG4Lidar_Src.zip'):
        pytest.skip()

    try:
        os.stat('tmp/cache/GDAL_MG4Lidar_Src')
    except OSError:
        try:
            gdaltest.unzip('tmp/cache', 'tmp/cache/GDAL_MG4Lidar_Src.zip')
            try:
                os.stat('tmp/cache/GDAL_MG4Lidar_Src')
            except OSError:
                pytest.skip()
        except OSError:
            pytest.skip()

    ds = gdal.Open('tmp/cache/GDAL_MG4Lidar_Src/Tetons_200k.view')
    assert ds is not None, 'could not open dataset'

    prj = ds.GetProjectionRef()
    if prj.find('NAD83 / UTM zone 12N') == -1:
        gdaltest.post_reason('did not get expected projection')
        print(prj)
        return

    gt = ds.GetGeoTransform()
    ref_gt = (504489.919999999983702, 3.078227571115974, 0, 4795848.389999999664724, 0, -3.078259860787739)
    for i in range(6):
        assert abs(gt[i] - ref_gt[i]) <= 1e-6, 'did not get expected geotransform'

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 13216:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs != 64099:
        gdaltest.post_reason('did not get expected overview checksum')
        print(cs)
        return

    ds = None



