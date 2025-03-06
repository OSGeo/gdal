#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster contour' testing
# Author:   Alessandro Pasotti, <elpaso at itopen dot it>
#
###############################################################################
# Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr


def get_contour_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("contour")


@pytest.mark.require_driver("AAIGRID")
def test_gdalalg_raster_contour(tmp_vsimem):

    tmp_out_filename = str(tmp_vsimem / "out.shp")
    tmp_filename = str(tmp_vsimem / "tmp.asc")
    dem = """ncols        2
nrows        2
xllcorner    0
yllcorner    0
cellsize     1
4 15
25 36"""

    gdal.FileFromMemBuffer(tmp_filename, dem.encode("ascii"))

    pipeline = get_contour_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            tmp_filename,
            tmp_out_filename,
            "--interval",
            "10",
            "--min-name",
            "ELEV_MIN",
            "--max-name",
            "ELEV_MAX",
            "-p",
        ]
    )

    expected_elev_values = [(4, 10), (10, 20), (20, 30), (30, 36)]

    with gdal.OpenEx(tmp_out_filename) as ds:
        lyr = ds.GetLayer()
        assert lyr.GetFeatureCount() == 4
        for i, feat in enumerate(lyr):
            geom = feat.GetGeometryRef()
            assert geom.GetGeometryType() == ogr.wkbPolygon
            assert feat.GetField("ELEV_MIN") == expected_elev_values[i][0]
            assert feat.GetField("ELEV_MAX") == expected_elev_values[i][1]
            feat = None
        lyr = None
