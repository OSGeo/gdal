#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for SDTS driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest

from osgeo import gdal, osr

###############################################################################
# Test a truncated version of an SDTS DEM downloaded at
# http://thor-f5.er.usgs.gov/sdts/datasets/raster/dem/dem_oct_2001/1107834.dem.sdts.tar.gz


def test_sdts_1():

    tst = gdaltest.GDALTest("SDTS", "STDS_1107834_truncated/1107CATD.DDF", 1, 61672)
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("NAD27")
    srs.SetUTM(16)
    tst.testOpen(
        check_prj=srs.ExportToWkt(),
        check_gt=(666015, 30, 0, 5040735, 0, -30),
        check_filelist=False,
    )

    ds = gdal.Open("data/STDS_1107834_truncated/1107CATD.DDF")
    md = ds.GetMetadata()

    assert md["TITLE"] == "ALANSON, MI-24000"
