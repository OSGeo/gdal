#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector filter' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr


def get_filter_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["filter"]


def test_gdalalg_vector_filter_no_filter(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    assert filter_alg.ParseCommandLineArguments(["../ogr/data/poly.shp", out_filename])
    assert filter_alg.Run()
    ds = filter_alg["output"].GetDataset()
    assert ds.GetLayer(0).GetFeatureCount() == 10
    assert filter_alg.Finalize()
    ds = None

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_filter_bbox(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    assert filter_alg.ParseRunAndFinalize(
        ["--bbox=479867,4762909,479868,4762910", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 1


def test_gdalalg_vector_filter_where_discard_all(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    assert filter_alg.ParseRunAndFinalize(
        ["--where=0=1", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 0


def test_gdalalg_vector_filter_where_accept_all(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    assert filter_alg.ParseRunAndFinalize(
        ["--where=1=1", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_filter_where_error(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    with pytest.raises(
        Exception, match='"invalid" not recognised as an available field.'
    ):
        filter_alg.ParseRunAndFinalize(
            ["--where=invalid", "../ogr/data/poly.shp", out_filename]
        )


def test_gdalalg_vector_fields(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    assert filter_alg.ParseRunAndFinalize(
        ["--fields=EAS_ID,_ogr_geometry_", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetFieldCount() == 1
        assert lyr.GetLayerDefn().GetGeomFieldCount() == 1
        assert lyr.GetFeatureCount() == 10
        f = lyr.GetNextFeature()
        assert f["EAS_ID"] == 168
        assert f.GetGeometryRef() is not None
        lyr.ResetReading()
        assert len([f for f in lyr]) == 10
        f = lyr.GetFeature(0)
        assert f["EAS_ID"] == 168
        with pytest.raises(Exception):
            lyr.GetFeature(10)
        assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1
        assert lyr.TestCapability(ogr.OLCRandomWrite) == 0
        assert lyr.GetExtent() == (478315.53125, 481645.3125, 4762880.5, 4765610.5)
        assert lyr.GetExtent(0) == (478315.53125, 481645.3125, 4762880.5, 4765610.5)


def test_gdalalg_vector_fields_geom_named(tmp_vsimem):

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbNone, srs=None)
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("geom_field"))
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("geom_field2"))
    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    filter_alg.GetArg("input").Get().SetDataset(src_ds)
    filter_alg.GetArg("output").Set(out_filename)
    assert filter_alg.ParseCommandLineArguments(
        ["--of", "Memory", "--fields=geom_field2"]
    )
    assert filter_alg.Run()

    ds = filter_alg.GetArg("output").Get().GetDataset()
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetGeomFieldCount() == 1
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetName() == "geom_field2"


def test_gdalalg_vector_fields_non_existing(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    with pytest.raises(
        Exception,
        match="Field 'i_do_not_exist' does not exist in layer 'poly'.",
    ):
        filter_alg.ParseRunAndFinalize(
            ["--fields=EAS_ID,i_do_not_exist", "../ogr/data/poly.shp", out_filename]
        )
