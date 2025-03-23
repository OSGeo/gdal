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
@pytest.mark.parametrize(
    "options, polygonize, expected_elev_values",
    [
        (
            ["--interval", "10", "--min-name", "ELEV_MIN", "--max-name", "ELEV_MAX"],
            True,
            [(4, 10), (10, 20), (20, 30), (30, 36)],
        ),
        (["--interval", "10", "--elevation-name", "ELEV"], False, [10.0, 20.0, 30.0]),
        (
            ["--interval", "10", "--elevation-name", "ELEV", "--offset", "5"],
            False,
            [5.0, 15.0, 25.0, 35.0],
        ),
        (
            ["--interval", "10", "--elevation-name", "ELEV", "--srcnodata", "4"],
            False,
            [20.0, 30.0],
        ),
        (
            ["--levels", "10", "--levels", "20", "--elevation-name", "ELEV"],
            False,
            [10.0, 20.0],
        ),
        (
            ["--levels", "10,20", "--elevation-name", "ELEV"],
            False,
            [10.0, 20.0],
        ),
        (
            ["--levels", "10", "--min-name", "ELEV_MIN", "--max-name", "ELEV_MAX"],
            True,
            [(10, 20)],
        ),
        (
            ["--exp-base", "10", "--min-name", "ELEV_MIN", "--max-name", "ELEV_MAX"],
            True,
            [(4, 10), (10, 36)],
        ),
        (
            [
                "--levels",
                "MIN,15,MAX",
                "--min-name",
                "ELEV_MIN",
                "--max-name",
                "ELEV_MAX",
            ],
            True,
            [(4, 15), (15, 36)],
        ),
        (
            [
                "--exp-base",
                "10",
                "--levels",
                "10",
                "--min-name",
                "ELEV_MIN",
                "--max-name",
                "ELEV_MAX",
                "--group-transactions",
                "1",
            ],
            True,
            "contour: Argument 'exp-base' is mutually exclusive with 'levels'.",
        ),
        (
            [
                "--interval",
                "10",
                "--levels",
                "10",
                "--min-name",
                "ELEV_MIN",
                "--max-name",
                "ELEV_MAX",
                "--group-transactions",
                "1",
            ],
            True,
            "contour: Argument 'levels' is mutually exclusive with 'interval'.",
        ),
    ],
)
def test_gdalalg_raster_contour(tmp_vsimem, options, polygonize, expected_elev_values):

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
    alg_options = [
        tmp_filename,
        tmp_out_filename,
    ]
    alg_options.extend(options)

    if polygonize:
        alg_options.append("--polygonize")

    if isinstance(expected_elev_values, str):
        with pytest.raises(RuntimeError):
            assert pipeline.ParseRunAndFinalize(alg_options)
        assert gdal.GetLastErrorMsg() == expected_elev_values

    else:
        assert pipeline.ParseRunAndFinalize(alg_options)

        with gdal.OpenEx(tmp_out_filename) as ds:
            lyr = ds.GetLayer()
            for i, feat in enumerate(lyr):
                geom = feat.GetGeometryRef()
                if polygonize:
                    assert geom.GetGeometryType() == ogr.wkbPolygon
                    assert feat.GetField("ELEV_MIN") == expected_elev_values[i][0]
                    assert feat.GetField("ELEV_MAX") == expected_elev_values[i][1]
                else:
                    assert geom.GetGeometryType() == ogr.wkbLineString
                    assert feat.GetField("ELEV") == expected_elev_values[i]
            lyr = None
