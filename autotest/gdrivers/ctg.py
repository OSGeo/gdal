#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test CTG driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest

from osgeo import gdal

###############################################################################
# Test a fake CTG dataset


def test_ctg_1():

    tst = gdaltest.GDALTest("CTG", "ctg/fake_grid_cell", 1, 21)
    expected_gt = [421000.0, 200.0, 0.0, 5094400.0, 0.0, -200.0]
    expected_srs = """PROJCS["WGS 84 / UTM zone 14N",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4326"]],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-99],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    AUTHORITY["EPSG","32614"],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]"""
    tst.testOpen(check_gt=expected_gt, check_prj=expected_srs)

    ds = gdal.Open("data/ctg/fake_grid_cell")
    lst = ds.GetRasterBand(1).GetCategoryNames()
    assert lst is not None and lst, "expected non empty category names for band 1"
    lst = ds.GetRasterBand(2).GetCategoryNames()
    assert lst is None, "expected empty category names for band 2"
    assert (
        ds.GetRasterBand(1).GetNoDataValue() == 0
    ), "did not get expected nodata value"
