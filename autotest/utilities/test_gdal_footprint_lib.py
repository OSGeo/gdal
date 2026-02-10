#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_footprint testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import collections
import os
import pathlib

import ogrtest
import pytest

from osgeo import gdal, osr

pytestmark = pytest.mark.require_geos


###############################################################################
# Simple test


def test_gdal_footprint_lib_basic():

    out_ds = gdal.Footprint("", "../gcore/data/byte.tif", format="MEM")
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert lyr.GetFeatureCount() == 1
    f = lyr.GetNextFeature()
    assert f["location"] == "../gcore/data/byte.tif"
    ogrtest.check_feature_geometry(
        f,
        "MULTIPOLYGON (((440720 3751320,440720 3750120,441920 3750120,441920 3751320,440720 3751320)))",
    )


###############################################################################
#


def test_gdal_footprint_lib_targetCoordinateSystem_pixel():

    out_ds = gdal.Footprint(
        "", "../gcore/data/byte.tif", format="MEM", targetCoordinateSystem="pixel"
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
        "", "../gcore/data/byte.tif", format="MEM", targetCoordinateSystem="georef"
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
        "", "../gcore/data/byte.tif", format="MEM", dstSRS="EPSG:4267"
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


@pytest.mark.require_driver("GeoJSON")
def test_gdal_footprint_lib_inline_geojson():

    ret = gdal.Footprint("", "../gcore/data/byte.tif", format="GeoJSON")
    assert isinstance(ret, dict)
    assert ret["crs"]["properties"]["name"] == "urn:ogc:def:crs:OGC:1.3:CRS84"


###############################################################################
#


@pytest.mark.require_driver("GeoJSON")
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
        format="MEM",
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
        src_ds.GetRasterBand(i + 1).WriteRaster(1, 1, 1, 1, b"\xff")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="MEM",
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
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, b"\xff")
    src_ds.GetRasterBand(1).WriteRaster(2, 0, 1, 1, b"\xff")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="MEM",
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
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, b"\xff")
    src_ds.GetRasterBand(1).WriteRaster(2, 0, 1, 1, b"\xff")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="MEM",
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
    src_ds.GetRasterBand(1).WriteRaster(1, 0, 1, 1, b"\xff")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="MEM",
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
    src_ds.GetRasterBand(1).WriteRaster(1, 0, 1, 1, b"\xff")
    src_ds.GetRasterBand(1).WriteRaster(1, 1, 1, 1, b"\xff")
    src_ds.GetRasterBand(1).WriteRaster(2, 1, 1, 1, b"\xff")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="MEM",
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
    src_ds.GetRasterBand(1).WriteRaster(1, 0, 1, 1, b"\xff")
    src_ds.GetRasterBand(1).WriteRaster(1, 1, 1, 1, b"\xff")
    src_ds.GetRasterBand(1).WriteRaster(2, 1, 1, 1, b"\xff")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="MEM",
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
    src_ds.GetRasterBand(1).WriteRaster(1, 0, 1, 1, b"\xff")
    src_ds.GetRasterBand(1).WriteRaster(1, 1, 1, 1, b"\xff")
    src_ds.GetRasterBand(1).WriteRaster(2, 1, 1, 1, b"\xff")
    src_ds.BuildOverviews("NONE", [2])
    src_ds.GetRasterBand(1).GetOverview(0).WriteRaster(0, 0, 1, 1, b"\xff")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="MEM",
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
    src_ds.GetRasterBand(1).WriteRaster(1, 0, 1, 1, b"\xff")
    src_ds.GetRasterBand(1).WriteRaster(1, 1, 1, 1, b"\xff")
    src_ds.GetRasterBand(1).WriteRaster(2, 1, 1, 1, b"\xff")
    src_ds.BuildOverviews("NONE", [2])
    src_ds.GetRasterBand(1).GetOverview(0).WriteRaster(0, 0, 1, 1, b"\xff")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="MEM",
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


def test_gdal_footprint_footprint_dict_arguments():

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


###############################################################################
# Test footprint, RGBA and overviews


def test_gdal_footprint_footprint_rgba_overviews():

    src_ds = gdal.GetDriverByName("MEM").Create("", 6, 6, 4)
    for i in range(4):
        src_ds.GetRasterBand(i + 1).SetColorInterpretation(gdal.GCI_RedBand + i)
    src_ds.BuildOverviews("NONE", [2])
    src_ds.GetRasterBand(4).GetOverview(0).WriteRaster(1, 1, 1, 1, b"\xff")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="MEM",
        targetCoordinateSystem="pixel",
        ovr=0,
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "MULTIPOLYGON (((2 2,2 4,4 4,4 2,2 2)))")


###############################################################################
def test_gdal_footprint_lib_union():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, 2)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, b"\xff")
    src_ds.GetRasterBand(2).SetNoDataValue(0)
    src_ds.GetRasterBand(2).WriteRaster(1, 0, 1, 1, b"\xff")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="MEM",
        targetCoordinateSystem="pixel",
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "MULTIPOLYGON (((0 0,0 1,2 1,2 0,0 0)))")


###############################################################################
def test_gdal_footprint_lib_intersection_none():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, b"\xff")
    src_ds.GetRasterBand(2).SetNoDataValue(0)
    src_ds.GetRasterBand(2).WriteRaster(1, 0, 1, 1, b"\xff")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="MEM",
        targetCoordinateSystem="pixel",
        combineBands="intersection",
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is None


###############################################################################
def test_gdal_footprint_lib_intersection_partial():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, 2)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\xff\xff")
    src_ds.GetRasterBand(2).SetNoDataValue(0)
    src_ds.GetRasterBand(2).WriteRaster(1, 0, 1, 1, b"\xff")
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="MEM",
        targetCoordinateSystem="pixel",
        combineBands="intersection",
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "MULTIPOLYGON (((1 0,1 1,2 1,2 0,1 0)))")


###############################################################################
def test_gdal_footprint_layerName():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).Fill(255)
    out_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)

    with pytest.raises(Exception, match="Cannot find layer non_existing"):
        gdal.Footprint(out_ds, src_ds, layerName="non_existing")

    out_ds.CreateLayer("a")
    layer_b = out_ds.CreateLayer("b")

    gdal.Footprint(out_ds, src_ds, layerName="b")
    assert layer_b.GetFeatureCount() == 1


###############################################################################
def test_gdal_footprint_wrong_output_format():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)

    # Non existing output driver
    with pytest.raises(Exception, match="Output driver `non_existing' not recognised"):
        gdal.Footprint("", src_ds, format="non_existing")

    # Raster-only output driver
    with pytest.raises(Exception, match="Output driver `GTiff' not recognised"):
        gdal.Footprint("", src_ds, format="GTiff")

    with pytest.raises(
        Exception, match="Cannot guess driver for /vsimem/out.unknown_ext"
    ):
        gdal.Footprint(
            "/vsimem/out.unknown_ext",
            src_ds,
        )


###############################################################################
def test_gdal_footprint_output_layer_has_crs_but_input_not():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    out_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    out_ds.CreateLayer("out_lyr", srs=srs)

    with pytest.raises(
        Exception, match="Output layer has CRS, but input is not georeferenced"
    ):
        gdal.Footprint(out_ds, src_ds, layerName="out_lyr")


###############################################################################
def test_gdal_footprint_wrong_number_nodata_values():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    with pytest.raises(
        Exception,
        match="Number of values in -srcnodata should be 1 or the number of bands",
    ):
        gdal.Footprint("", src_ds, format="MEM", srcNodata=[1, 2])


###############################################################################
def test_gdal_footprint_wrong_bands():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    with pytest.raises(Exception, match="Invalid band number: 2"):
        gdal.Footprint("", src_ds, format="MEM", bands=[2])


###############################################################################
def test_gdal_footprint_wrong_ovr_on_band_with_nodata():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 1)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    with pytest.raises(
        Exception,
        match="Overview index 0 invalid for this dataset. Bands of this dataset have no precomputed overviews",
    ):
        gdal.Footprint(
            "",
            src_ds,
            format="MEM",
            ovr=0,
        )
    src_ds.BuildOverviews("NEAR", [2])
    with pytest.raises(
        Exception,
        match=r"Overview index 1 invalid for this dataset. Value should be in \[0,0\] range",
    ):
        gdal.Footprint(
            "",
            src_ds,
            format="MEM",
            ovr=1,
        )


###############################################################################
def test_gdal_footprint_wrong_ovr_on_band_with_alpha():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 2)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    with pytest.raises(
        Exception,
        match="Overview index 0 invalid for this dataset. Mask bands of this dataset have no precomputed overviews",
    ):
        gdal.Footprint(
            "",
            src_ds,
            format="MEM",
            ovr=0,
        )
    src_ds.BuildOverviews("NEAR", [2])
    with pytest.raises(
        Exception,
        match=r"Overview index 1 invalid for this dataset. Value should be in \[0,0\] range",
    ):
        gdal.Footprint(
            "",
            src_ds,
            format="MEM",
            ovr=1,
        )


###############################################################################
#


def test_gdal_footprint_lib_targetCoordinateSystem_georef_error():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    with pytest.raises(
        Exception,
        match="Georeferenced coordinates requested, but input dataset has no geotransform.",
    ):
        gdal.Footprint(
            "",
            src_ds,
            format="MEM",
            targetCoordinateSystem="georef",
        )


###############################################################################
#


def test_gdal_footprint_lib_minRingArea():

    # footprint area above minRingArea
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="MEM",
        targetCoordinateSystem="pixel",
        minRingArea="0.5",
    )
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetFeatureCount() == 1

    # footprint area below minRingArea
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    out_ds = gdal.Footprint(
        "",
        src_ds,
        format="MEM",
        targetCoordinateSystem="pixel",
        minRingArea="1.5",
    )
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetFeatureCount() == 0


###############################################################################
#


def test_gdal_footprint_lib_destSRS_and_targetCoordinateSystem_pixel_mutually_exclusive():

    with pytest.raises(
        Exception, match="-t_cs pixel and -t_srs are mutually exclusive"
    ):
        gdal.Footprint(
            "",
            "../gcore/data/byte.tif",
            format="MEM",
            dstSRS="EPSG:4267",
            targetCoordinateSystem="pixel",
        )


###############################################################################
#


def test_gdal_footprint_lib_srcNodata_and_ovr_mutually_exclusive():

    with pytest.raises(
        Exception,
        match=r"-srcnodata \"<value>\[ <value>\]...\"' not allowed with '-ovr <index>",
    ):
        gdal.Footprint("", "../gcore/data/byte.tif", format="MEM", srcNodata=0, ovr=0)


###############################################################################
# Test locationFieldName=None


def test_gdal_footprint_lib_no_location():

    out_ds = gdal.Footprint(
        "", "../gcore/data/byte.tif", format="MEM", locationFieldName=None
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 0


###############################################################################
# Test writeAbsolutePath=True


def test_gdal_footprint_lib_writeAbsolutePath():

    out_ds = gdal.Footprint(
        "", "../gcore/data/byte.tif", format="MEM", writeAbsolutePath=True
    )
    assert out_ds is not None
    lyr = out_ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert os.path.isabs(f["location"])
