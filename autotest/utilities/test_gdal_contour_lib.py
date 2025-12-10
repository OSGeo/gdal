#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_contour testing
# Author:   Alessandro Pasotti, <elpaso at itopen dot it>
#
###############################################################################
# Copyright (c) 2025, Alessandro Pasotti<elpaso at itopen dot it>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

from osgeo import gdal, ogr


@pytest.mark.parametrize("create_output", [False, True])
@pytest.mark.parametrize(
    "options, options_args, polygonize, expected_elev_values",
    [
        (
            ["-i", "10"],
            None,
            True,
            [(4.0, 10.0), (10.0, 20.0), (20.0, 30.0), (30.0, 36.0)],
        ),
        (["-i", "10"], None, False, [10.0, 20.0, 30.0]),
        (None, {"interval": 10}, False, [10.0, 20.0, 30.0]),
        (None, {"fixedLevels": [10, 20]}, False, [10.0, 20.0]),
        (None, {"interval": 10, "offset": 5}, False, [5.0, 15.0, 25.0, 35.0]),
        (None, {"interval": 10, "with3d": True}, False, [10.0, 20.0, 30.0]),
    ],
)
@pytest.mark.require_driver("AAIGRID")
def test_contour_1(
    tmp_vsimem, create_output, options, options_args, polygonize, expected_elev_values
):

    content = """ncols        2
nrows        2
xllcorner    0
yllcorner    0
cellsize     1
4 15
25 36"""

    options_new = options.copy() if options is not None else None

    srcfilename = str(tmp_vsimem / "test_contour_1.asc")
    dstfilename = str(tmp_vsimem / "test_contour_1.shp")

    ogr_ds = None
    lyr = None

    has_z = options_args and "with3d" in options_args and options_args["with3d"]

    if create_output:
        ogr_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
        assert ogr_ds is not None

        if polygonize:
            if has_z:
                geom_type = ogr.wkbMultiPolygon25D
            else:
                geom_type = ogr.wkbMultiPolygon
        else:
            if has_z:
                geom_type = ogr.wkbLineString25D
            else:
                geom_type = ogr.wkbLineString

        lyr = ogr_ds.CreateLayer("contour", geom_type=geom_type)
        lyr.CreateField(ogr.FieldDefn("ID", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("ELEV_MIN", ogr.OFTReal))
        lyr.CreateField(ogr.FieldDefn("ELEV_MAX", ogr.OFTReal))
        assert lyr is not None

    with gdaltest.tempfile(srcfilename, content):

        src_ds = gdal.Open(srcfilename)
        assert src_ds is not None

        if options_new is not None:
            if polygonize:
                options_new.extend(
                    [
                        "-amin",
                        "ELEV_MIN",
                        "-amax",
                        "ELEV_MAX",
                        "-p",
                    ]
                )
            else:
                options_new.extend(["-a", "ELEV"])
            if create_output:
                assert gdal.Contour(ogr_ds, src_ds, options=options_new)
            else:
                ogr_ds = gdal.Contour(dstfilename, src_ds, options=options_new)
                assert ogr_ds is not None
                lyr = ogr_ds.GetLayer(0)
                assert lyr is not None
        else:

            if polygonize:
                options_args.update(
                    {
                        "minName": "ELEV_MIN",
                        "maxName": "ELEV_MAX",
                        "polygonize": True,
                    }
                )
            else:
                options_args.update({"elevationName": "ELEV"})

            if create_output:
                assert gdal.Contour(ogr_ds, src_ds, **options_args)
            else:
                ogr_ds = gdal.Contour(dstfilename, src_ds, **options_args)
                assert ogr_ds is not None
                lyr = ogr_ds.GetLayer(0)
                assert lyr is not None

        # Get all elev values
        elev_values = []
        for f in lyr:
            geom = f.GetGeometryRef()
            assert geom is not None
            if has_z:
                assert geom.GetZ() != 0
            if polygonize:
                elev_values.append((f["ELEV_MIN"], f["ELEV_MAX"]))
            else:
                elev_values.append(f["ELEV"])

        assert elev_values == expected_elev_values, (elev_values, expected_elev_values)


@pytest.mark.require_driver("AAIGRID")
@pytest.mark.parametrize(
    "first_value, expected_values",
    [
        (
            0,
            {
                10.0: [
                    "LINESTRING (3.49999990000002 0.0,3.49999990000002 0.5,3.5 0.50000009999998,4.0 0.50000009999998)"
                ]
            },
        ),
        (
            -1,
            {
                0: [
                    "LINESTRING (0.0 2.500000999998,0.5 2.500000999998,1.499999000002 3.5,1.499999000002 4.0)"
                ],
                10.0: [
                    "LINESTRING (3.49999990000002 0.0,3.49999990000002 0.5,3.5 0.50000009999998,4.0 0.50000009999998)"
                ],
            },
        ),
        (
            0.001,
            {
                10.0: [
                    "LINESTRING (3.49999990000002 0.0,3.49999990000002 0.5,3.5 0.50000009999998,4.0 0.50000009999998)"
                ]
            },
        ),
    ],
)
def test_contour_min_value(tmp_vsimem, first_value, expected_values):

    content = f"""ncols        4
nrows        4
xllcorner    0
yllcorner    0
cellsize     1
{first_value} 0   0   0
0   0    0   0.001
0   0    0   0
0   0    0   10"""

    srcfilename = str(tmp_vsimem / "test_contour_1.asc")

    with gdal.VSIFile(srcfilename, "wt+") as f:
        f.write(content)

    dstfilename = str(tmp_vsimem / "test_contour_1.shp")

    options_args = {
        "elevationName": "ELEV",
        "interval": 10,
    }

    ogr_ds = gdal.Contour(dstfilename, srcfilename, **options_args)
    assert ogr_ds is not None
    lyr = ogr_ds.GetLayer(0)
    assert lyr is not None

    results = {}
    elev_values = set()
    for feat in lyr:
        geom = feat.GetGeometryRef()
        assert geom is not None
        elev = feat.GetFieldAsDouble("ELEV")
        elev_values.add(elev)
        try:
            results[elev].append(geom.ExportToWkt())
        except KeyError:
            results[elev] = [geom.ExportToWkt()]

    assert results.keys() == expected_values.keys()

    # Compare actual lines with expected ones, allow for some small numerical differences
    for elev in expected_values.keys():
        expected_contours = expected_values[elev]
        actual_contours = results[elev]
        assert len(expected_contours) == len(actual_contours)
        for i in range(len(expected_contours)):
            expected_contour = expected_contours[i]
            actual_contour = actual_contours[i]
            actual_xy = []
            for xy in actual_contour[12:-1].split(","):
                actual_xy.extend([float(k) for k in xy.split(" ")])
            expected_xy = []
            for xy in expected_contour[12:-1].split(","):
                expected_xy.extend([float(k) for k in xy.split(" ")])
            assert pytest.approx(actual_xy, rel=1e-8) == expected_xy
