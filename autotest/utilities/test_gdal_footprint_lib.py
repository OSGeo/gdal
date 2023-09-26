#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_footprint testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
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

import collections
import pathlib

import ogrtest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_geos


###############################################################################
# Simple test


def test_gdal_footprint_lib_basic():

    out_ds = gdal.Footprint("", "../gcore/data/byte.tif", format="Memory")
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 1
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTIPOLYGON (((440720 3751320,440720 3750120,441920 3750120,441920 3751320,440720 3751320)))",
    )


###############################################################################
#


def test_gdal_footprint_lib_targetCoordinateSystem_pixel():

    out_ds = gdal.Footprint(
        "", "../gcore/data/byte.tif", format="Memory", targetCoordinateSystem="pixel"
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    assert lyr.GetSpatialRef() is None
    assert lyr.GetFeatureCount() == 1
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTIPOLYGON (((0 0,0 20,20 20,20 0,0 0)))",
    )


###############################################################################
#


def test_gdal_footprint_lib_targetCoordinateSystem_georef():

    out_ds = gdal.Footprint(
        "", "../gcore/data/byte.tif", format="Memory", targetCoordinateSystem="georef"
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 1
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTIPOLYGON (((440720 3751320,440720 3750120,441920 3750120,441920 3751320,440720 3751320)))",
    )


###############################################################################
#


def test_gdal_footprint_lib_destSRS():

    out_ds = gdal.Footprint(
        "", "../gcore/data/byte.tif", format="Memory", dstSRS="EPSG:4267"
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4267"
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTIPOLYGON (((-117.641168620797 33.9023526904272,-117.641087629972 33.8915301685907,-117.628110837847 33.8915970129623,-117.628190189534 33.9024195619211,-117.641168620797 33.9023526904272)))",
    )


###############################################################################
#


def test_gdal_footprint_lib_inline_geojson():

    ret = gdal.Footprint("", "../gcore/data/byte.tif", format="GeoJSON")
    assert type(ret) == dict
    assert ret["crs"]["properties"]["name"] == "urn:ogc:def:crs:OGC:1.3:CRS84"


###############################################################################
#


def test_gdal_footprint_lib_inline_wkt():

    ret = gdal.Footprint("", "../gcore/data/byte.tif", format="WKT")
    assert ret.startswith(
        "MULTIPOLYGON (((440720 3751320,440720 3750120,441920 3750120,441920 3751320,440720 3751320)))"
    )


###############################################################################
#


def test_gdal_footprint_lib_srcNodata():

    out_ds = gdal.Footprint(
        "",
        "../gcore/data/byte.tif",
        format="Memory",
        targetCoordinateSystem="pixel",
        srcNodata=74,
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "MULTIPOLYGON (((0 0,0 20,20 20,20 0,0 0),(9 17,10 17,10 18,9 18,9 17)))"
    )


###############################################################################
#


def test_gdal_footprint_lib_alpha_band():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3, 4)
    src_ds.GetRasterBand(4).SetColorInterpretation(gdal.GCI_AlphaBand)
    for i in range(4):
        src_ds.GetRasterBand(i + 1).WriteRaster(1, 1, 1, 1, b"\xFF")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="Memory",
        targetCoordinateSystem="pixel",
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTIPOLYGON (((1 1,1 2,2 2,2 1,1 1)))",
    )


###############################################################################
#


def test_gdal_footprint_lib_splitPolys():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, 1)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, b"\xFF")
    src_ds.GetRasterBand(1).WriteRaster(2, 0, 1, 1, b"\xFF")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="Memory",
        targetCoordinateSystem="pixel",
        splitPolys=True,
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    f1 = lyr.GetNextFeature()
    f2 = lyr.GetNextFeature()
    try:
        ogrtest.check_feature_geometry(f1, "POLYGON ((0 0,0 1,1 1,1 0,0 0))")
        ogrtest.check_feature_geometry(f2, "POLYGON ((2 0,2 1,3 1,3 0,2 0))")
    except AssertionError:
        ogrtest.check_feature_geometry(f2, "POLYGON ((0 0,0 1,1 1,1 0,0 0))")
        ogrtest.check_feature_geometry(f1, "POLYGON ((2 0,2 1,3 1,3 0,2 0))")


###############################################################################
#


def test_gdal_footprint_lib_convexHull():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, 1)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, b"\xFF")
    src_ds.GetRasterBand(1).WriteRaster(2, 0, 1, 1, b"\xFF")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="Memory",
        targetCoordinateSystem="pixel",
        convexHull=True,
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "MULTIPOLYGON (((0 0,0 1,3 1,3 0,0 0)))")


###############################################################################
#


def test_gdal_footprint_lib_densify():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, 1)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(1, 0, 1, 1, b"\xFF")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="Memory",
        targetCoordinateSystem="pixel",
        densify=0.5,
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "MULTIPOLYGON (((1 0,1.0 0.5,1 1,1.5 1.0,2 1,2.0 0.5,2 0,1.5 0.0,1 0)))"
    )


###############################################################################
#


def test_gdal_footprint_lib_simplify():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 2, 1)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(1, 0, 1, 1, b"\xFF")
    src_ds.GetRasterBand(1).WriteRaster(1, 1, 1, 1, b"\xFF")
    src_ds.GetRasterBand(1).WriteRaster(2, 1, 1, 1, b"\xFF")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="Memory",
        targetCoordinateSystem="pixel",
        simplify=1,
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "MULTIPOLYGON (((1 0,1 2,3 2,1 0)))")


###############################################################################
#


def test_gdal_footprint_lib_maxPoints():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 2, 1)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(1, 0, 1, 1, b"\xFF")
    src_ds.GetRasterBand(1).WriteRaster(1, 1, 1, 1, b"\xFF")
    src_ds.GetRasterBand(1).WriteRaster(2, 1, 1, 1, b"\xFF")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="Memory",
        targetCoordinateSystem="pixel",
        maxPoints=4,
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "MULTIPOLYGON (((1 0,1 2,3 2,1 0)))")


###############################################################################
#


def test_gdal_footprint_lib_ovr():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 2, 1)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(1, 0, 1, 1, b"\xFF")
    src_ds.GetRasterBand(1).WriteRaster(1, 1, 1, 1, b"\xFF")
    src_ds.GetRasterBand(1).WriteRaster(2, 1, 1, 1, b"\xFF")
    src_ds.BuildOverviews("NONE", [2])
    src_ds.GetRasterBand(1).GetOverview(0).WriteRaster(0, 0, 1, 1, b"\xFF")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="Memory",
        targetCoordinateSystem="pixel",
        ovr=0,
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "MULTIPOLYGON (((0 0,0 2,1.5 2.0,1.5 0.0,0 0)))")


###############################################################################
#


def test_gdal_footprint_lib_ovr_georef():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 2, 1)
    src_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(1, 0, 1, 1, b"\xFF")
    src_ds.GetRasterBand(1).WriteRaster(1, 1, 1, 1, b"\xFF")
    src_ds.GetRasterBand(1).WriteRaster(2, 1, 1, 1, b"\xFF")
    src_ds.BuildOverviews("NONE", [2])
    src_ds.GetRasterBand(1).GetOverview(0).WriteRaster(0, 0, 1, 1, b"\xFF")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="Memory",
        ovr=0,
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "MULTIPOLYGON (((2 49,2 47,3.5 47.0,3.5 49.0,2 49)))"
    )


###############################################################################
#


@pytest.mark.require_driver("GPKG")
def test_gdal_footprint_lib_dsco_lco(tmp_vsimem):

    out_filename = tmp_vsimem / "out.gpkg"
    out_ds = gdal.Footprint(
        out_filename,
        pathlib.Path("../gcore/data/byte.tif"),
        format="GPKG",
        datasetCreationOptions=["ADD_GPKG_OGR_CONTENTS=NO"],
        layerCreationOptions=["GEOMETRY_NAME=my_geom"],
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    assert lyr.GetGeometryColumn() == "my_geom"
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTIPOLYGON (((440720 3751320,440720 3750120,441920 3750120,441920 3751320,440720 3751320)))",
    )
    with out_ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE name = 'gpkg_ogr_contents'"
    ) as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 0
    gdal.Unlink(out_filename)


###############################################################################
# Test option argument handling


def test_gdaldem_footprint_dict_arguments():

    opt = gdal.FootprintOptions(
        "__RETURN_OPTION_LIST__",
        datasetCreationOptions=collections.OrderedDict(
            (("GEOMETRY_ENCODING", "WKT"), ("FORMAT", "NC4"))
        ),
        layerCreationOptions=collections.OrderedDict(
            (("RECORD_DIM_NAME", "record"), ("STRING_DEFAULT_WIDTH", 10))
        ),
    )

    dsco_idx = opt.index("-dsco")

    assert opt[dsco_idx : dsco_idx + 4] == [
        "-dsco",
        "GEOMETRY_ENCODING=WKT",
        "-dsco",
        "FORMAT=NC4",
    ]

    lco_idx = opt.index("-lco")

    assert opt[lco_idx : lco_idx + 4] == [
        "-lco",
        "RECORD_DIM_NAME=record",
        "-lco",
        "STRING_DEFAULT_WIDTH=10",
    ]
