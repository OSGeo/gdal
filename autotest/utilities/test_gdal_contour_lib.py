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
    "options, polygonize, expected_elev_values",
    [
        (["-i", "10"], True, [(4, 10), (10, 20), (20, 30), (30, 36)]),
        (["-i", "10"], False, [10.0, 20.0, 30.0]),
    ],
)
@pytest.mark.require_driver("AAIGRID")
def test_contour_1(
    tmp_vsimem, create_output, options, polygonize, expected_elev_values
):

    content = """ncols        2
nrows        2
xllcorner    0
yllcorner    0
cellsize     1
4 15
25 36"""

    options_new = options.copy()

    srcfilename = str(tmp_vsimem / "test_contour_1.asc")
    dstfilename = str(tmp_vsimem / "test_contour_1.shp")

    ogr_ds = None
    lyr = None

    if create_output:
        ogr_ds = ogr.GetDriverByName("Memory").CreateDataSource("")
        assert ogr_ds is not None
        lyr = ogr_ds.CreateLayer(
            "contour",
            geom_type=ogr.wkbMultiPolygon if polygonize else ogr.wkbLineString,
        )
        lyr.CreateField(ogr.FieldDefn("ID", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("ELEV_MIN", ogr.OFTReal))
        lyr.CreateField(ogr.FieldDefn("ELEV_MAX", ogr.OFTReal))
        assert lyr is not None

    with gdaltest.tempfile(srcfilename, content):

        src_ds = gdal.Open(srcfilename)
        assert src_ds is not None

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

        # Get all elev values
        elev_values = []
        for f in lyr:
            if polygonize:
                elev_values.append((f["ELEV_MIN"], f["ELEV_MAX"]))
            else:
                elev_values.append(f["ELEV"])

        assert elev_values == expected_elev_values, (elev_values, expected_elev_values)
