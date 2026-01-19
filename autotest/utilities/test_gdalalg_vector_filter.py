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


def test_gdalalg_vector_filter_bbox_active_layer():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")
    src_lyr.CreateField(ogr.FieldDefn("foo"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "baz"
    src_lyr.CreateFeature(f)

    src_lyr = src_ds.CreateLayer("other_layer")
    src_lyr.CreateField(ogr.FieldDefn("foo"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "baz"
    src_lyr.CreateFeature(f)

    filter_alg = get_filter_alg()
    filter_alg["input"] = src_ds
    filter_alg["active-layer"] = "the_layer"

    assert filter_alg.ParseCommandLineArguments(
        ["--where", "foo='bar'", "--of", "MEM", "--output", "memory_ds"]
    )
    assert filter_alg.Run()

    out_ds = filter_alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    assert out_lyr.GetNextFeature() is None

    out_lyr = out_ds.GetLayer(1)
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "baz"


def test_gdalalg_vector_filter_update_extent(tmp_vsimem):

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "in"
    ) as src_ds:
        src_lyr = src_ds.CreateLayer("the_layer", geom_type=ogr.wkbPoint25D)
        src_lyr.CreateField(ogr.FieldDefn("foo"))

        f = ogr.Feature(src_lyr.GetLayerDefn())
        f["foo"] = "bar"
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2 10)"))
        src_lyr.CreateFeature(f)

        f = ogr.Feature(src_lyr.GetLayerDefn())
        f["foo"] = "baz"
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(3 4 20)"))
        src_lyr.CreateFeature(f)

        src_lyr = src_ds.CreateLayer(
            "other_layer", geom_type=ogr.wkbPoint25D, options=["AUTO_REPACK=NO"]
        )
        src_lyr.CreateField(ogr.FieldDefn("foo"))

        f = ogr.Feature(src_lyr.GetLayerDefn())
        f["foo"] = "baz"
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(3 4 30)"))
        src_lyr.CreateFeature(f)

        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(5 6 40)"))
        src_lyr.CreateFeature(f)
        src_lyr.DeleteFeature(f.GetFID())

    with gdal.alg.vector.filter(
        input=tmp_vsimem / "in",
        output="",
        output_format="stream",
        where="foo='bar'",
        update_extent=True,
        active_layer="the_layer",
    ) as alg:
        out_ds = alg.Output()
        lyr = out_ds.GetLayerByName("the_layer")
        assert lyr.GetExtent(force=False) == (1, 1, 2, 2)
        assert lyr.GetExtent3D(force=False) == (1, 1, 2, 2, 10, 10)
        assert lyr.GetFeatureCount() == 1
        lyr = out_ds.GetLayerByName("other_layer")
        assert lyr.GetExtent(force=False) == (3, 5, 4, 6)
        assert lyr.GetExtent3D(force=False) == (3, 5, 4, 6, 30, 40)
        assert lyr.GetFeatureCount() == 1
