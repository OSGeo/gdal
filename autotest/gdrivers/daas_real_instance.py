#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  DAAS driver test suite.
# Author:   Even Rouault, even.rouault at spatialys.com
#
###############################################################################
# Copyright (c) 2021, Airbus DS Intelligence
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

import pytest

pytestmark = pytest.mark.require_driver('DAAS')

###############################################################################
# Test accessing a real DAAS asset

def test_daas_real_instance():

    if gdal.GetConfigOption('GDAL_DAAS_ACCESS_TOKEN') is None and \
       gdal.GetConfigOption('GDAL_DAAS_API_KEY') is None and \
       gdal.GetConfigOption('GDAL_DAAS_CLIENT_ID') is None:
           pytest.skip('Missing authentication elements: GDAL_DAAS_ACCESS_TOKEN or GDAL_DAAS_API_KEY+GDAL_DAAS_CLIENT_ID')

    md_url = gdal.GetConfigOption('GDAL_AUTOTEST_DAAS_GET_METADATA_URL')
    if md_url is None:
        pytest.skip('Missing GDAL_AUTOTEST_DAAS_GET_METADATA_URL with a URL to the GetMetadata URL of the scene to test')

    gdal.ErrorReset()
    ds = gdal.Open('DAAS:' + md_url)
    assert ds is not None
    req_xsize = min(1024, ds.RasterXSize)
    req_ysize = min(1024, ds.RasterYSize)
    assert ds.ReadRaster(0, 0, req_xsize, req_ysize) is not None
    assert ds.GetLastErrorMsg() == ''
