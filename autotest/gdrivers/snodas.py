#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SNODAS driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("SNODAS")

###############################################################################
# Test a fake SNODAS dataset


def test_snodas_1():

    tst = gdaltest.GDALTest("SNODAS", "snodas/fake_snodas.hdr", 1, 0)
    expected_gt = [
        -124.733749999995,
        0.0083333333333330643,
        0.0,
        52.874583333331302,
        0.0,
        -0.0083333333333330054,
    ]
    expected_srs = """GEOGCS["WGS 84",
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
    tst.testOpen(check_gt=expected_gt, check_prj=expected_srs, skip_checksum=True)

    ds = gdal.Open("data/snodas/fake_snodas.hdr")
    ds.GetFileList()
    assert ds.GetRasterBand(1).GetNoDataValue() == -9999
    assert ds.GetRasterBand(1).GetMinimum() == 0
    assert ds.GetRasterBand(1).GetMaximum() == 429
