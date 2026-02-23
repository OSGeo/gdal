#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector combine' testing
# Author:   Daniel Baston
#
###############################################################################
# Copyright (c) 2025-2026, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import string

import gdaltest
import pytest

from osgeo import gdal, ogr, osr


@pytest.fixture()
def alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["combine"]


def test_gdalalg_vector_combine(alg):

    src_ds = gdal.OpenEx("../ogr/data/poly.shp")

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()

    assert dst_ds.GetLayerCount() == 1
    assert dst_ds.GetSpatialRef().IsSame(src_ds.GetSpatialRef())

    src_lyr = src_ds.GetLayer(0)
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetName() == src_lyr.GetName()
    assert dst_lyr.GetSpatialRef().IsSame(src_lyr.GetSpatialRef())
    assert dst_lyr.GetFeatureCount() == 1
    assert dst_lyr.GetLayerDefn().GetGeomFieldCount() == 1
    assert dst_lyr.GetLayerDefn().GetGeomType() == ogr.wkbMultiPolygon

    src_area = sum(f.GetGeometryRef().GetArea() for f in src_lyr)
    src_parts = sum(f.GetGeometryRef().GetGeometryCount() for f in src_lyr)

    f = dst_lyr.GetNextFeature()

    assert f.GetGeometryRef().GetGeometryCount() == src_parts
    assert f.GetGeometryRef().GetArea() == src_area


@pytest.mark.parametrize(
    "group_by", ["int_field", "str_field", ["str_field", "int_field"]]
)
def test_gdalalg_vector_combine_group_by(alg, group_by):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("layer", geom_type=ogr.wkbPoint)

    src_lyr.CreateField(ogr.FieldDefn("int_field", ogr.OFTInteger))
    src_lyr.CreateField(ogr.FieldDefn("str_field", ogr.OFTString))

    f = ogr.Feature(src_lyr.GetLayerDefn())

    for i in range(7):
        f["int_field"] = i % 2
        f["str_field"] = string.ascii_letters[i % 3]
        f.SetGeometry(ogr.CreateGeometryFromWkt(f"POINT ({i} {2 * i})"))
        src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["group-by"] = group_by

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer()

    features = [f for f in dst_lyr]

    if group_by == "int_field":
        assert len(features) == 2
        assert dst_lyr.GetLayerDefn().GetFieldCount() == 1
        assert dst_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "int_field"
        assert dst_lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger

        assert features[0]["int_field"] == 0
        assert (
            features[0]
            .GetGeometryRef()
            .Equals(
                ogr.CreateGeometryFromWkt("MULTIPOINT ((0 0), (2 4), (4 8), (6 12))")
            )
        )

        assert features[1]["int_field"] == 1
        assert (
            features[1]
            .GetGeometryRef()
            .Equals(ogr.CreateGeometryFromWkt("MULTIPOINT ((1 2), (3 6), (5 10))"))
        )
    elif group_by == "str_field":
        assert len(features) == 3
        assert dst_lyr.GetLayerDefn().GetFieldCount() == 1
        assert dst_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "str_field"
        assert dst_lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString

        assert features[0]["str_field"] == "a"
        assert (
            features[0]
            .GetGeometryRef()
            .Equals(ogr.CreateGeometryFromWkt("MULTIPOINT ((0 0), (3 6), (6 12)))"))
        )

        assert features[1]["str_field"] == "b"
        assert (
            features[1]
            .GetGeometryRef()
            .Equals(ogr.CreateGeometryFromWkt("MULTIPOINT ((1 2), (4 8))"))
        )

        assert features[2]["str_field"] == "c"
        assert (
            features[2]
            .GetGeometryRef()
            .Equals(ogr.CreateGeometryFromWkt("MULTIPOINT ((2 4), (5 10))"))
        )
    elif group_by == ["str_field", "int_field"]:
        assert len(features) == 6
        assert dst_lyr.GetLayerDefn().GetFieldCount() == 2
        assert dst_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "str_field"
        assert dst_lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
        assert dst_lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "int_field"
        assert dst_lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTInteger

        assert features[0]["int_field"] == 0
        assert features[0]["str_field"] == "a"
        assert (
            features[0]
            .GetGeometryRef()
            .Equals(ogr.CreateGeometryFromWkt("MULTIPOINT ((0 0), (6 12))"))
        )

        assert features[1]["int_field"] == 1
        assert features[1]["str_field"] == "a"
        assert (
            features[1]
            .GetGeometryRef()
            .Equals(ogr.CreateGeometryFromWkt("MULTIPOINT ((3 6))"))
        )

        assert features[2]["int_field"] == 0
        assert features[2]["str_field"] == "b"
        assert (
            features[2]
            .GetGeometryRef()
            .Equals(ogr.CreateGeometryFromWkt("MULTIPOINT ((4 8))"))
        )

        assert features[3]["int_field"] == 1
        assert features[3]["str_field"] == "b"
        assert (
            features[3]
            .GetGeometryRef()
            .Equals(ogr.CreateGeometryFromWkt("MULTIPOINT ((1 2))"))
        )

        assert features[4]["int_field"] == 0
        assert features[4]["str_field"] == "c"
        assert (
            features[4]
            .GetGeometryRef()
            .Equals(ogr.CreateGeometryFromWkt("MULTIPOINT ((2 4))"))
        )

        assert features[5]["int_field"] == 1
        assert features[5]["str_field"] == "c"
        assert (
            features[5]
            .GetGeometryRef()
            .Equals(ogr.CreateGeometryFromWkt("MULTIPOINT ((5 10))"))
        )
    else:
        pytest.fail(f"Unhandled value of group-by: : {group_by}")


@pytest.mark.parametrize("geoms", [["POINT (3 7)", None], [None, "POINT (3 7)"]])
def test_gdalalg_vector_combine_null_geometry(alg, geoms):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("layer", geom_type=ogr.wkbPoint)

    f = ogr.Feature(src_lyr.GetLayerDefn())

    expect_empty = True
    for wkt in geoms:
        if wkt is None:
            f.SetGeometry(None)
        else:
            f.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
            expect_empty = False
        src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 1
    f = dst_lyr.GetNextFeature()
    geom = f.GetGeometryRef()
    if expect_empty:
        assert geom.ExportToWkt() == "MULTIPOINT EMPTY"
    else:
        assert geom.Equals(ogr.CreateGeometryFromWkt("MULTIPOINT ((3 7))"))


def test_gdalalg_vector_combine_multiple_geom_fields(alg):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer(
        "layer", srs=osr.SpatialReference(epsg=4326), geom_type=ogr.wkbPoint
    )

    line_geom_defn = ogr.GeomFieldDefn("line_geom", ogr.wkbLineStringM)
    line_geom_defn.SetSpatialRef(osr.SpatialReference(epsg=32145))

    src_lyr.CreateGeomField(line_geom_defn)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("POINT (3 7)"))
    f.SetGeomField(1, ogr.CreateGeometryFromWkt("LINESTRING M (1 2 3)"))

    src_lyr.CreateFeature(f)

    f.SetGeomField(0, ogr.CreateGeometryFromWkt("POINT (2 8)"))
    f.SetGeomField(1, ogr.CreateGeometryFromWkt("LINESTRING M (4 5 6)"))

    src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 1

    f = dst_lyr.GetNextFeature()
    assert f.GetGeomFieldRef(0).ExportToWkt() == "MULTIPOINT (3 7,2 8)"
    assert f.GetGeomFieldRef(0).GetSpatialReference().IsSame(src_lyr.GetSpatialRef())

    assert (
        f.GetGeomFieldRef(1).ExportToIsoWkt() == "MULTILINESTRING M ((1 2 3),(4 5 6))"
    )
    assert (
        f.GetGeomFieldRef(1)
        .GetSpatialReference()
        .IsSame(line_geom_defn.GetSpatialRef())
    )


def test_gdalalg_vector_combine_group_by_invalid(alg):

    alg["input"] = "../ogr/data/poly.shp"
    alg["group-by"] = "does_not_exist"
    alg["output"] = ""
    alg["output-format"] = "stream"

    with pytest.raises(Exception, match="attribute field .* does not exist"):
        alg.Run()

    with pytest.raises(Exception, match="must be a list of unique field names"):
        alg["group-by"] = ["EAS_ID", "AREA", "EAS_ID"]


@pytest.mark.parametrize(
    "wkt_in,type_in,expected_type,expected_nested_type",
    [
        (
            (
                "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
                "MULTIPOLYGON (((1 1, 2 1, 2 2, 1 2, 1 1)), ((2 2, 3 2, 3 3, 2 3, 2 2)))",
            ),
            ogr.wkbPolygon,
            ogr.wkbMultiPolygon,
            ogr.wkbGeometryCollection,
        ),  # lie about geometry type, like the Shapefile driver does
        (
            ("MULTILINESTRING ((1 1, 2 2)), POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))"),
            ogr.wkbUnknown,
            ogr.wkbGeometryCollection,
            ogr.wkbGeometryCollection,
        ),
        (
            ("POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))", "POINT M (2 4 6)"),
            ogr.wkbUnknown,
            ogr.wkbGeometryCollectionM,
            ogr.wkbGeometryCollectionM,
        ),
        (
            ("POINT (2 4)", "POINT M (6 4 4)", "POINT Z (3 4 9)"),
            ogr.wkbPoint,
            ogr.wkbMultiPointZM,
            ogr.wkbMultiPointZM,
        ),
        (
            ("POINT (2 4)", "MULTIPOINT M (6 4 4)", "POINT Z (3 4 9)"),
            ogr.wkbPoint,
            ogr.wkbMultiPointZM,
            ogr.wkbGeometryCollectionZM,
        ),
    ],
)
@pytest.mark.parametrize("keep_nested", (True, False))
def test_gdalalg_vector_combine_keep_nested_mixed_geometry_types(
    alg, wkt_in, type_in, expected_type, expected_nested_type, keep_nested
):

    src_ds = gdaltest.wkt_ds(wkt_in, geom_type=type_in)

    alg["input"] = src_ds
    if keep_nested:
        alg["keep-nested"] = True
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert (
        dst_lyr.GetLayerDefn().GetGeomType() == expected_nested_type
        if keep_nested
        else expected_type
    )

    f = dst_lyr.GetNextFeature()
    g = f.GetGeometryRef()

    assert g.GetGeometryType() == expected_nested_type if keep_nested else expected_type


@pytest.mark.require_driver("GDALG")
def test_gdalalg_vector_combine_test_ogrsf(tmp_path):
    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_path / "src.shp"
    ) as src_ds:
        src_lyr = src_ds.CreateLayer("poly")

        src_lyr.CreateField(ogr.FieldDefn("GRP", ogr.OFTInteger))

        with gdal.OpenEx("../ogr/data/poly.shp", gdal.OF_VECTOR) as poly_ds:
            for i, src_feature in enumerate(poly_ds.GetLayer(0)):
                dst_feature = ogr.Feature(src_lyr.GetLayerDefn())
                dst_feature["GRP"] = i % 3
                dst_feature.SetGeometry(src_feature.GetGeometryRef())
                src_lyr.CreateFeature(dst_feature)

    gdalg_filename = tmp_path / "tmp.gdalg.json"

    gdalg_contents = {
        "type": "gdal_streamed_alg",
        "command_line": f"gdal vector combine {tmp_path}/src.shp --group-by GRP --output-format=stream dummy_dataset_name",
        "relative_paths_relative_to_this_file": False,
    }

    with open(gdalg_filename, "w") as gdalg_file:
        json.dump(gdalg_contents, gdalg_file)

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" -ro {gdalg_filename}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret
    assert "FAILURE" not in ret
