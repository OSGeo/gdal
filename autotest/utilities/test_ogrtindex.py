#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogrtindex testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import ogrtest
import pytest
import test_cli_utilities

from osgeo import ogr, osr

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_ogrtindex_path() is None, reason="ogrtindex not available"
)


@pytest.fixture()
def ogrtindex_path():
    return test_cli_utilities.get_ogrtindex_path()


###############################################################################
# Simple test


@pytest.mark.parametrize("srs", (None, 4326))
def test_ogrtindex_1(ogrtindex_path, tmp_path, srs):

    if srs:
        srs_obj = osr.SpatialReference()
        srs_obj.ImportFromEPSG(srs)
        srs = srs_obj

    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    with shape_drv.CreateDataSource(str(tmp_path)) as shape_ds:
        shape_lyr = shape_ds.CreateLayer("point1", srs=srs)
        dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
        dst_feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(49 2)"))
        shape_lyr.CreateFeature(dst_feat)

        shape_lyr = shape_ds.CreateLayer("point2", srs=srs)
        dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
        dst_feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(49 3)"))
        shape_lyr.CreateFeature(dst_feat)

        shape_lyr = shape_ds.CreateLayer("point3", srs=srs)
        dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
        dst_feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(48 2)"))
        shape_lyr.CreateFeature(dst_feat)

        shape_lyr = shape_ds.CreateLayer("point4", srs=srs)
        dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
        dst_feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(48 3)"))
        shape_lyr.CreateFeature(dst_feat)

    _, err = gdaltest.runexternal_out_and_err(
        f"{ogrtindex_path} -skip_different_projection {tmp_path}/tileindex.shp {tmp_path}/point1.shp {tmp_path}/point2.shp {tmp_path}/point3.shp {tmp_path}/point4.shp"
    )
    assert err is None or err == "", "got error/warning"

    ds = ogr.Open(f"{tmp_path}/tileindex.shp")
    assert ds.GetLayer(0).GetFeatureCount() == 4, "did not get expected feature count"

    if srs is not None:
        assert ds.GetLayer(0).GetSpatialRef() is not None and ds.GetLayer(
            0
        ).GetSpatialRef().IsSame(
            srs, options=["IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES"]
        ), "did not get expected spatial ref"
    else:
        assert (
            ds.GetLayer(0).GetSpatialRef() is None
        ), "did not get expected spatial ref"

    expected_wkts = [
        "POLYGON ((49 2,49 2,49 2,49 2,49 2))",
        "POLYGON ((49 3,49 3,49 3,49 3,49 3))",
        "POLYGON ((48 2,48 2,48 2,48 2,48 2))",
        "POLYGON ((48 3,48 3,48 3,48 3,48 3))",
    ]

    for i, feat in enumerate(ds.GetLayer(0)):
        assert (
            feat.GetGeometryRef().ExportToWkt() == expected_wkts[i]
        ), "i=%d, wkt=%s" % (i, feat.GetGeometryRef().ExportToWkt())


###############################################################################
# Test -src_srs_name, -src_srs_format and -t_srs


def epsg_to_wkt(srid):
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(srid)
    return srs.ExportToWkt()


@pytest.mark.parametrize(
    "src_srs_format,expected_srss",
    [
        ("", ["EPSG:4326", "EPSG:32631"]),
        ("-src_srs_format AUTO", ["EPSG:4326", "EPSG:32631"]),
        ("-src_srs_format EPSG", ["EPSG:4326", "EPSG:32631"]),
        (
            "-src_srs_format PROJ",
            [
                "+proj=longlat +datum=WGS84 +no_defs",
                "+proj=utm +zone=31 +datum=WGS84 +units=m +no_defs",
            ],
        ),
        ("-src_srs_format WKT", [epsg_to_wkt(4326), epsg_to_wkt(32631)]),
    ],
)
def test_ogrtindex_3(ogrtindex_path, tmp_path, src_srs_format, expected_srss):

    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    shape_ds = shape_drv.CreateDataSource(str(tmp_path))

    srs_4326 = osr.SpatialReference()
    srs_4326.ImportFromEPSG(4326)

    srs_32631 = osr.SpatialReference()
    srs_32631.ImportFromEPSG(32631)

    shape_lyr = shape_ds.CreateLayer("point1", srs=srs_4326)
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 49)"))
    shape_lyr.CreateFeature(dst_feat)

    shape_lyr = shape_ds.CreateLayer("point2", srs=srs_32631)
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(500000 5538630.70286887)"))
    shape_lyr.CreateFeature(dst_feat)
    shape_ds = None

    output_filename = str(tmp_path / "tileindex.shp")
    output_format = ""
    if src_srs_format == "-src_srs_format WKT":
        if ogr.GetDriverByName("SQLite") is None:
            pytest.skip("SQLite driver not available")
        output_filename = str(tmp_path / "tileindex.db")
        output_format = " -f SQLite"

    _, err = gdaltest.runexternal_out_and_err(
        ogrtindex_path
        + " -src_srs_name src_srs -t_srs EPSG:4326 "
        + output_filename
        + f" {tmp_path}/point1.shp {tmp_path}/point2.shp "
        + src_srs_format
        + output_format
    )

    assert src_srs_format == "-src_srs_format WKT" or (
        err is None or err == ""
    ), "got error/warning"

    ds = ogr.Open(output_filename)
    assert ds.GetLayer(0).GetFeatureCount() == 2, "did not get expected feature count"

    assert (
        ds.GetLayer(0).GetSpatialRef().GetAuthorityCode(None) == "4326"
    ), "did not get expected spatial ref"

    expected_wkts = [
        "POLYGON ((2 49,2 49,2 49,2 49,2 49))",
        "POLYGON ((3 50,3 50,3 50,3 50,3 50))",
    ]

    for i, feat in enumerate(ds.GetLayer(0)):
        assert feat.GetField("src_srs") == expected_srss[i]
        ogrtest.check_feature_geometry(feat, expected_wkts[i], context=f"i={i}")

    ds = None


###############################################################################
# More options


@pytest.mark.require_driver("GPKG")
def test_ogrtindex_options(ogrtindex_path, tmp_path):

    _, err = gdaltest.runexternal_out_and_err(
        f"{ogrtindex_path} -f GPKG -lnum 0 -lname poly -tileindex my_loc -accept_different_schemas {tmp_path}/out.gpkg ../ogr/data/poly.shp"
    )
    assert err is None or err == ""
    ds = ogr.Open(tmp_path / "out.gpkg")
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == "tileindex"
    f = lyr.GetNextFeature()
    assert f["my_loc"] == "../ogr/data/poly.shp,0"
