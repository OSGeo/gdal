#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  DAAS driver test suite.
# Author:   Even Rouault, even.rouault at spatialys.com
#
###############################################################################
# Copyright (c) 2021, Airbus DS Intelligence
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("DAAS")

###############################################################################
# Test accessing a real DAAS asset


def test_daas_real_instance():

    if (
        gdal.GetConfigOption("GDAL_DAAS_ACCESS_TOKEN") is None
        and gdal.GetConfigOption("GDAL_DAAS_API_KEY") is None
        and gdal.GetConfigOption("GDAL_DAAS_CLIENT_ID") is None
    ):
        pytest.skip(
            "Missing authentication elements: GDAL_DAAS_ACCESS_TOKEN or GDAL_DAAS_API_KEY+GDAL_DAAS_CLIENT_ID"
        )

    md_url = gdal.GetConfigOption("GDAL_AUTOTEST_DAAS_GET_METADATA_URL")
    if md_url is None:
        pytest.skip(
            "Missing GDAL_AUTOTEST_DAAS_GET_METADATA_URL with a URL to the GetMetadata URL of the scene to test"
        )

    gdal.ErrorReset()
    ds = gdal.Open("DAAS:" + md_url)
    assert ds is not None
    req_xsize = min(1024, ds.RasterXSize)
    req_ysize = min(1024, ds.RasterYSize)
    assert ds.ReadRaster(0, 0, req_xsize, req_ysize) is not None
    assert ds.GetLastErrorMsg() == ""
