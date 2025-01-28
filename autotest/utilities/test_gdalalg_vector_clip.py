#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector clip' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_geos


def get_clip_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    vector = reg.InstantiateAlg("vector")
    return vector.InstantiateSubAlgorithm("clip")


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_clip_general_behavior(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.gpkg")

    clip = get_clip_alg()
    assert clip.ParseRunAndFinalize(
        ["--bbox", "-1e10,-1e10,1e10,1e10", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.OpenEx(out_filename, gdal.OF_UPDATE) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10
        for i in range(10):
            ds.GetLayer(0).DeleteFeature(i + 1)

    clip = get_clip_alg()
    with pytest.raises(
        Exception, match="already exists. Specify the --overwrite option"
    ):
        clip.ParseRunAndFinalize(
            ["--bbox", "-1e10,-1e10,1e10,1e10", "../ogr/data/poly.shp", out_filename]
        )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 0

    clip = get_clip_alg()
    assert clip.ParseRunAndFinalize(
        [
            "--bbox",
            "-1e10,-1e10,1e10,1e10",
            "--overwrite",
            "../ogr/data/poly.shp",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10

    clip = get_clip_alg()
    assert clip.ParseRunAndFinalize(
        [
            "--bbox",
            "-1e10,-1e10,1e10,1e10",
            "--append",
            "../ogr/data/poly.shp",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 20

    clip = get_clip_alg()
    assert clip.ParseRunAndFinalize(
        [
            "--bbox",
            "-1e10,-1e10,1e10,1e10",
            "--update",
            "--nln",
            "layer2",
            "../ogr/data/poly.shp",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayerByName("poly").GetFeatureCount() == 20
        assert ds.GetLayerByName("layer2").GetFeatureCount() == 10

    clip = get_clip_alg()
    assert clip.ParseRunAndFinalize(
        [
            "--bbox",
            "-1e10,-1e10,1e10,1e10",
            "--of",
            "GPKG",
            "--overwrite-layer",
            "--nln",
            "poly",
            "../ogr/data/poly.shp",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayerByName("poly").GetFeatureCount() == 10
        assert ds.GetLayerByName("layer2").GetFeatureCount() == 10


def test_gdalalg_vector_clip_bbox():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("foo"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 -1,-1 -1,-1 0,0 0))"))
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))"))
    src_lyr.CreateFeature(f)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)

    assert clip.ParseCommandLineArguments(
        ["--bbox", "0.2,0.3,0.7,0.8", "--of", "Memory", "--output", "memory_ds"]
    )
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    ogrtest.check_feature_geometry(
        out_f, "POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))"
    )

    assert out_lyr.GetNextFeature() is None


def test_gdalalg_vector_clip_bbox_srs():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs_wgs84 = osr.SpatialReference()
    srs_wgs84.SetFromUserInput("WGS84")
    srs_wgs84.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    src_lyr = src_ds.CreateLayer("test", srs=srs_wgs84)
    src_lyr.CreateField(ogr.FieldDefn("foo"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))"))
    src_lyr.CreateFeature(f)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)

    assert clip.ParseCommandLineArguments(
        [
            "--bbox",
            "22263.8981586547,33395.9998333802,77923.6435552915,89058.4864167096",
            "--bbox-crs",
            "EPSG:3857",
            "--of",
            "Memory",
            "--output",
            "memory_ds",
        ]
    )
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    ogrtest.check_feature_geometry(
        out_f, "POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))"
    )

    assert out_lyr.GetNextFeature() is None


def test_gdalalg_vector_clip_split_multipart():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbPolygon)
    src_lyr.CreateField(ogr.FieldDefn("foo"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON ((0 0,0 1,1 1,1 0,0.8 0,0.8 0.8,0.2 0.8,0.2 0,0 0))"
        )
    )
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "baz"
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POLYGON ((0.4 0.4,0.4 0.6,0.6 0.6,0.6 0.4,0.4 0.4))")
    )
    src_lyr.CreateFeature(f)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)

    assert clip.ParseCommandLineArguments(
        ["--bbox", "-0.1,0.3,1.1,0.7", "--of", "Memory", "--output", "memory_ds"]
    )
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)

    ref1_geom = ogr.CreateGeometryFromWkt(
        "POLYGON ((0.0 0.7,0.2 0.7,0.2 0.3,0.0 0.3,0.0 0.7))"
    )
    ref2_geom = ogr.CreateGeometryFromWkt(
        "POLYGON ((1.0 0.3,0.8 0.3,0.8 0.7,1.0 0.7,1.0 0.3))"
    )

    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    g = out_f.GetGeometryRef()
    was_one = g.Within(ref1_geom) and ref1_geom.Within(g)
    assert was_one or (g.Within(ref2_geom) and ref2_geom.Within(g))

    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    g = out_f.GetGeometryRef()
    if was_one:
        assert g.Within(ref2_geom) and ref2_geom.Within(g)
    else:
        assert g.Within(ref1_geom) and ref1_geom.Within(g)

    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "baz"
    ogrtest.check_feature_geometry(
        out_f, "POLYGON ((0.4 0.4,0.4 0.6,0.6 0.6,0.6 0.4,0.4 0.4))"
    )

    assert out_lyr.GetNextFeature() is None


@pytest.mark.parametrize(
    "clip_geom",
    [
        "POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))",
        '{"type":"Polygon","coordinates":[[[0.2,0.8],[0.7,0.8],[0.7,0.3],[0.2,0.3],[0.2,0.8]]]}',
    ],
)
def test_gdalalg_vector_clip_geom(clip_geom):

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("foo"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 -1,-1 -1,-1 0,0 0))"))
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))"))
    src_lyr.CreateFeature(f)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)

    assert clip.ParseCommandLineArguments(
        ["--geometry", clip_geom, "--of", "Memory", "--output", "memory_ds"]
    )
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    ogrtest.check_feature_geometry(
        out_f, "POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))"
    )

    assert out_lyr.GetNextFeature() is None


@pytest.mark.parametrize(
    "clip_geom",
    [
        "POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))",
        '{"type":"Polygon","coordinates":[[[0.2,0.8],[0.7,0.8],[0.7,0.3],[0.2,0.3],[0.2,0.8]]]}',
    ],
)
def test_gdalalg_vector_clip_geom_srs(clip_geom):

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs_wgs84 = osr.SpatialReference()
    srs_wgs84.SetFromUserInput("WGS84")
    srs_wgs84.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    src_lyr = src_ds.CreateLayer("test", srs=srs_wgs84)
    src_lyr.CreateField(ogr.FieldDefn("foo"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))"))
    src_lyr.CreateFeature(f)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)
    clip_geom = clip_geom.replace("0.2", "22263.8981586547")
    clip_geom = clip_geom.replace("0.3", "33395.9998333802")
    clip_geom = clip_geom.replace("0.7", "77923.6435552915")
    clip_geom = clip_geom.replace("0.8", "89058.4864167096")
    assert clip.ParseCommandLineArguments(
        [
            "--geometry",
            clip_geom,
            "--geometry-crs",
            "EPSG:3857",
            "--of",
            "Memory",
            "--output",
            "memory_ds",
        ]
    )
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromWkt(out_f.GetGeometryRef().ExportToWkt()),
        "POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))",
    )

    assert out_lyr.GetNextFeature() is None


def test_gdalalg_vector_clip_geom_not_rectangle():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("foo"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    # Bounding box of this polygon intersects the bounding box of the clipping
    # geometry, but their full intersection is empty
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POLYGON ((0.4 0.4,0.4 0.5,0.5 0.5,0.4 0.4))")
    )
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((-1 -1,-1 2,2 2,2 -1,-1 -1))"))
    src_lyr.CreateFeature(f)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)

    assert clip.ParseCommandLineArguments(
        [
            "--geometry",
            "POLYGON ((0 0,0 1,1 1,1 0,0.8 0,0.8 0.8,0.2 0.8,0.2 0,0 0))",
            "--of",
            "Memory",
            "--output",
            "memory_ds",
        ]
    )
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    ogrtest.check_feature_geometry(
        out_f, "POLYGON ((0 0,0 1,1 1,1 0,0.8 0,0.8 0.8,0.2 0.8,0.2 0,0 0))"
    )

    assert out_lyr.GetNextFeature() is None


def test_gdalalg_vector_clip_intersection_incompatible_geometry_type():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbLineString)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(1 1,2 2)"))
    src_lyr.CreateFeature(f)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)

    assert clip.ParseCommandLineArguments(
        [
            "--geometry",
            "POLYGON ((0 0,0 1,1 1,1 0,0 0))",
            "--of",
            "Memory",
            "--output",
            "memory_ds",
        ]
    )
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetNextFeature() is None


def test_gdalalg_vector_clip_intersection_promote_simple_type_to_multi():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbMultiPolygon)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "MULTIPOLYGON(((-0.5 -0.5,-0.5 1.5,1.5 1.5,1.5 -0.5,-0.5 -0.5)))"
        )
    )
    src_lyr.CreateFeature(f)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)

    assert clip.ParseCommandLineArguments(
        [
            "--geometry",
            "POLYGON ((0 0,0 1,1 1,1 0,0 0))",
            "--of",
            "Memory",
            "--output",
            "memory_ds",
        ]
    )
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromWkt(out_f.GetGeometryRef().ExportToWkt()),
        "MULTIPOLYGON (((0 1,1 1,1 0,0 0,0 1)))",
    )


def test_gdalalg_vector_clip_like_vector():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))"))
    src_lyr.CreateFeature(f)

    # Create "like" dataset
    like_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    like_lyr = like_ds.CreateLayer("test")
    f = ogr.Feature(like_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))")
    )
    like_lyr.CreateFeature(f)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)
    clip.GetArg("like").Get().SetDataset(like_ds)

    assert clip.ParseCommandLineArguments(["--of", "Memory", "--output", "memory_ds"])
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromWkt(out_f.GetGeometryRef().ExportToWkt()),
        "POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))",
    )

    assert out_lyr.GetNextFeature() is None


def test_gdalalg_vector_clip_like_vector_invalid_geom():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))"))
    src_lyr.CreateFeature(f)

    # Create "like" dataset
    like_ds = gdal.GetDriverByName("Memory").Create(
        "clip_ds", 0, 0, 0, gdal.GDT_Unknown
    )
    like_lyr = like_ds.CreateLayer("test")
    f = ogr.Feature(like_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,1 1,0 1,1 0,0 0))"))
    like_lyr.CreateFeature(f)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)
    clip.GetArg("like").Get().SetDataset(like_ds)

    assert clip.ParseCommandLineArguments(["--of", "Memory", "--output", "memory_ds"])
    with gdal.quiet_errors(), pytest.raises(
        Exception, match="Geometry of feature 0 of clip_ds is invalid"
    ):
        clip.Run()


def test_gdalalg_vector_clip_like_vector_like_layer():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))"))
    src_lyr.CreateFeature(f)

    # Create "like" dataset
    like_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    like_ds.CreateLayer("unused")
    like_lyr = like_ds.CreateLayer("my_layer")
    f = ogr.Feature(like_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))")
    )
    like_lyr.CreateFeature(f)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)
    clip.GetArg("like").Get().SetDataset(like_ds)

    assert clip.ParseCommandLineArguments(
        ["--like-layer", "my_layer", "--of", "Memory", "--output", "memory_ds"]
    )
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromWkt(out_f.GetGeometryRef().ExportToWkt()),
        "POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))",
    )

    assert out_lyr.GetNextFeature() is None


def test_gdalalg_vector_clip_like_vector_like_layer_invalid():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)

    # Create "like" dataset
    like_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    like_ds.CreateLayer("test")

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)
    clip.GetArg("like").Get().SetDataset(like_ds)

    assert clip.ParseCommandLineArguments(
        ["--like-layer", "invalid", "--of", "Memory", "--output", "memory_ds"]
    )
    with pytest.raises(
        Exception, match="Failed to identify source layer from clipping dataset."
    ):
        clip.Run()


def test_gdalalg_vector_clip_like_vector_like_sql():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))"))
    src_lyr.CreateFeature(f)

    # Create "like" dataset
    like_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    like_ds.CreateLayer("unused")
    like_lyr = like_ds.CreateLayer("my_layer")
    f = ogr.Feature(like_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))")
    )
    like_lyr.CreateFeature(f)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)
    clip.GetArg("like").Get().SetDataset(like_ds)

    assert clip.ParseCommandLineArguments(
        [
            "--like-sql",
            "SELECT * FROM my_layer",
            "--of",
            "Memory",
            "--output",
            "memory_ds",
        ]
    )
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromWkt(out_f.GetGeometryRef().ExportToWkt()),
        "POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))",
    )

    assert out_lyr.GetNextFeature() is None


def test_gdalalg_vector_clip_like_vector_like_where():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))"))
    src_lyr.CreateFeature(f)

    # Create "like" dataset
    like_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    like_lyr = like_ds.CreateLayer("my_layer")
    like_lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
    f = ogr.Feature(like_lyr.GetLayerDefn())
    f["id"] = 0
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))"))
    like_lyr.CreateFeature(f)

    f = ogr.Feature(like_lyr.GetLayerDefn())
    f["id"] = 1
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))")
    )
    like_lyr.CreateFeature(f)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)
    clip.GetArg("like").Get().SetDataset(like_ds)

    assert clip.ParseCommandLineArguments(
        ["--like-where", "id=1", "--of", "Memory", "--output", "memory_ds"]
    )
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromWkt(out_f.GetGeometryRef().ExportToWkt()),
        "POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))",
    )

    assert out_lyr.GetNextFeature() is None


def test_gdalalg_vector_clip_like_vector_like_where_empty():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))"))
    src_lyr.CreateFeature(f)

    # Create "like" dataset
    like_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    like_lyr = like_ds.CreateLayer("my_layer")
    like_lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)
    clip.GetArg("like").Get().SetDataset(like_ds)

    assert clip.ParseCommandLineArguments(
        ["--like-where", "id=1", "--of", "Memory", "--output", "memory_ds"]
    )
    with pytest.raises(Exception, match="No clipping geometry found"):
        clip.Run()


def test_gdalalg_vector_clip_like_vector_srs():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs_wgs84 = osr.SpatialReference()
    srs_wgs84.SetFromUserInput("WGS84")
    srs_wgs84.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    src_lyr = src_ds.CreateLayer("test", srs=srs_wgs84)
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))"))
    src_lyr.CreateFeature(f)

    # Create "like" dataset
    like_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs_3857 = osr.SpatialReference()
    srs_3857.SetFromUserInput("EPSG:3857")
    srs_3857.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    like_lyr = like_ds.CreateLayer("test", srs=srs_3857)
    f = ogr.Feature(like_lyr.GetLayerDefn())
    wkt = "POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))"
    wkt = wkt.replace("0.2", "22263.8981586547")
    wkt = wkt.replace("0.3", "33395.9998333802")
    wkt = wkt.replace("0.7", "77923.6435552915")
    wkt = wkt.replace("0.8", "89058.4864167096")
    f.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
    like_lyr.CreateFeature(f)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)
    clip.GetArg("like").Get().SetDataset(like_ds)

    assert clip.ParseCommandLineArguments(["--of", "Memory", "--output", "memory_ds"])
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        ogr.CreateGeometryFromWkt(out_f.GetGeometryRef().ExportToWkt()),
        "POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))",
    )

    assert out_lyr.GetNextFeature() is None


def test_gdalalg_vector_clip_like_raster():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))"))
    src_lyr.CreateFeature(f)

    # Create "like" dataset
    like_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    like_ds.SetGeoTransform([0.2, 0.5, 0, 0.3, 0, 0.5])

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)
    clip.GetArg("like").Get().SetDataset(like_ds)

    assert clip.ParseCommandLineArguments(["--of", "Memory", "--output", "memory_ds"])
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    ogrtest.check_feature_geometry(
        out_f, "POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))"
    )

    assert out_lyr.GetNextFeature() is None


def test_gdalalg_vector_clip_like_raster_srs():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs_wgs84 = osr.SpatialReference()
    srs_wgs84.SetFromUserInput("WGS84")
    srs_wgs84.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    src_lyr = src_ds.CreateLayer("test", srs=srs_wgs84)
    src_lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,1 0,0 0))"))
    src_lyr.CreateFeature(f)

    # Create "like" dataset
    like_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    like_ds.SetGeoTransform(
        [22263.8981586547, 55660.4518654215, 0, 33395.9998333802, 0, 55659.7453966368]
    )
    srs_3857 = osr.SpatialReference()
    srs_3857.SetFromUserInput("EPSG:3857")
    srs_3857.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    like_ds.SetSpatialRef(srs_3857)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)
    clip.GetArg("like").Get().SetDataset(like_ds)

    assert clip.ParseCommandLineArguments(["--of", "Memory", "--output", "memory_ds"])
    assert clip.Run()

    out_ds = clip.GetArg("output").Get().GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    ogrtest.check_feature_geometry(
        out_f, "POLYGON ((0.2 0.8,0.7 0.8,0.7 0.3,0.2 0.3,0.2 0.8))"
    )

    assert out_lyr.GetNextFeature() is None


def test_gdalalg_vector_clip_missing_arg(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    clip = get_clip_alg()
    with pytest.raises(
        Exception, match="clip: --bbox, --geometry or --like must be specified"
    ):
        clip.ParseRunAndFinalize(["../ogr/data/poly.shp", out_filename])


def test_gdalalg_vector_clip_geometry_invalid():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_ds.CreateLayer("test")

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)

    assert clip.ParseCommandLineArguments(
        ["--geometry", "invalid", "--of", "Memory", "--output", "memory_ds"]
    )
    with pytest.raises(
        Exception,
        match="clip: Clipping geometry is neither a valid WKT or GeoJSON geometry",
    ):
        clip.Run()


def test_gdalalg_vector_clip_like_vector_too_many_layers():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_ds.CreateLayer("test")

    # Create "like" dataset
    like_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)
    clip.GetArg("like").Get().SetDataset(like_ds)

    assert clip.ParseCommandLineArguments(["--of", "Memory", "--output", "memory_ds"])
    with pytest.raises(
        Exception,
        match="clip: Dataset '' has no geotransform matrix. Its bounds cannot be established",
    ):
        clip.Run()


def test_gdalalg_vector_clip_like_raster_no_geotransform():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_ds.CreateLayer("test")

    # Create "like" dataset
    like_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    like_ds.CreateLayer("test1")
    like_ds.CreateLayer("test2")

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)
    clip.GetArg("like").Get().SetDataset(like_ds)

    assert clip.ParseCommandLineArguments(["--of", "Memory", "--output", "memory_ds"])
    with pytest.raises(
        Exception, match="clip: Only single layer dataset can be specified with --like"
    ):
        clip.Run()


def test_gdalalg_vector_clip_like_neither_raster_no_vector():

    src_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)

    # Create "like" dataset
    like_ds = gdal.GetDriverByName("Memory").Create("clip", 0, 0, 0, gdal.GDT_Unknown)

    clip = get_clip_alg()
    clip.GetArg("input").Get().SetDataset(src_ds)
    clip.GetArg("like").Get().SetDataset(like_ds)

    assert clip.ParseCommandLineArguments(["--of", "Memory", "--output", "memory_ds"])
    with pytest.raises(
        Exception,
        match="clip: Cannot get extent from clip dataset",
    ):
        clip.Run()
