#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector explode' testing
# Author:   Dan Baston
#
###############################################################################
# Copyright (c) 2026, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import string
import sys

import gdaltest
import ogrtest
import pytest
import test_cli_utilities

from osgeo import gdal, ogr, osr


@pytest.fixture()
def alg():
    return gdal.Algorithm("vector", "explode")


@pytest.fixture()
def source_with_arrays():

    np = pytest.importorskip("numpy")

    nfeat = 3
    array_length = 3

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer(
        "test", geom_type=ogr.wkbPoint, srs=osr.SpatialReference(epsg=4326)
    )
    src_lyr.CreateField(ogr.FieldDefn("name", ogr.OFTString))
    src_lyr.CreateField(ogr.FieldDefn("my_real_list", ogr.OFTRealList))
    src_lyr.CreateField(ogr.FieldDefn("my_int_list", ogr.OFTIntegerList))
    src_lyr.CreateField(ogr.FieldDefn("my_int64_list", ogr.OFTInteger64List))
    src_lyr.CreateField(ogr.FieldDefn("my_string_list", ogr.OFTStringList))

    feature = ogr.Feature(src_lyr.GetLayerDefn())
    for i in range(nfeat):
        feature["name"] = f"feat_{i}"
        feature["my_real_list"] = (i ** (np.arange(1, 1 + array_length))).tolist()
        feature["my_int_list"] = ((i + 1) * np.arange(array_length)).tolist()
        if sys.maxsize < (1 << 63) - 1:
            feature["my_int64_list"] = feature["my_int_list"]
        else:
            feature["my_int64_list"] = (2**32 + np.arange(array_length)).tolist()
        feature["my_string_list"] = list(string.ascii_letters[i : i + array_length])
        feature.SetGeometry(ogr.CreateGeometryFromWkt(f"POINT ({i} {2 * i})"))
        src_lyr.CreateFeature(feature)

    return src_ds


def test_gdalalg_vector_explode_basic(alg, source_with_arrays):

    alg["input"] = source_with_arrays
    alg["field"] = [
        "name",
        "my_real_list",
        "my_int_list",
        "my_int64_list",
        "my_string_list",
    ]
    alg["index-field"] = "index"
    alg["output-format"] = "MEM"

    assert alg.Run()

    out_ds = alg.Output()
    assert out_ds.GetLayerCount() == 1

    src_lyr = source_with_arrays.GetLayer(0)
    out_lyr = out_ds.GetLayer(0)

    assert out_lyr.GetName() == "test"

    assert out_lyr.GetSpatialRef().IsSame(src_lyr.GetSpatialRef())
    assert out_lyr.GetFeatureCount() == 9

    out_defn = out_lyr.GetLayerDefn()
    out_fields = [
        out_defn.GetFieldDefn(i).GetName() for i in range(out_defn.GetFieldCount())
    ]

    assert out_fields == [
        "index",
        "name",
        "my_real_list",
        "my_int_list",
        "my_int64_list",
        "my_string_list",
    ]

    for i, src_feat in enumerate(src_lyr):
        for j in range(3):
            dst_feat = out_lyr.GetNextFeature()

            assert dst_feat["index"] == j
            assert dst_feat["name"] == src_feat["name"]
            assert dst_feat["my_real_list"] == src_feat["my_real_list"][j]
            assert dst_feat["my_int_list"] == src_feat["my_int_list"][j]
            assert dst_feat["my_int64_list"] == src_feat["my_int64_list"][j]
            assert dst_feat["my_string_list"] == src_feat["my_string_list"][j]

            assert dst_feat.GetGeometryRef().ExportToWkt() == f"POINT ({i} {2 * i})"


@pytest.mark.parametrize(
    "field_type",
    (ogr.OFTIntegerList, ogr.OFTInteger64List, ogr.OFTRealList, ogr.OFTStringList),
)
def test_gdalalg_vector_explode_arrays_unequal_length(alg, field_type):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbNone)
    src_lyr.CreateField(ogr.FieldDefn("field_a", field_type))
    src_lyr.CreateField(ogr.FieldDefn("field_b", field_type))

    f = ogr.Feature(src_lyr.GetLayerDefn())

    f["field_a"] = [1, 2, 3]
    f["field_b"] = [4, 5, 6]
    src_lyr.CreateFeature(f)

    f["field_a"] = [7, 8, 9]
    f["field_b"] = [10, 11]
    src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["field"] = ["field_a", "field_b"]
    alg["output-format"] = "MEM"

    with pytest.raises(
        Exception,
        match="Field 'field_b' of source feature 1 does not have enough elements",
    ):
        alg.Run()


def test_gdalalg_vector_explode_invalid_field(alg):

    alg["input"] = "../ogr/data/poly.shp"
    alg["field"] = "does_not_exist"
    alg["output-format"] = "MEM"

    with pytest.raises(Exception, match="Field 'does_not_exist' not found"):
        alg.Run()


def test_gdalalg_vector_explode_geometry(alg):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer(
        "test", geom_type=ogr.wkbMultiPoint, srs=osr.SpatialReference(epsg=4326)
    )

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTIPOINT (3 2, 4 7, 1 9)"))
    src_lyr.CreateFeature(f)

    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTIPOINT (2 4, 6 1, 0 6)"))
    src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["geometry"] = True
    alg["output-format"] = "MEM"

    assert alg.Run()

    dst_ds = alg.Output()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 6

    assert dst_lyr.GetNextFeature().GetGeometryRef().ExportToWkt() == "POINT (3 2)"
    assert dst_lyr.GetNextFeature().GetGeometryRef().ExportToWkt() == "POINT (4 7)"
    assert dst_lyr.GetNextFeature().GetGeometryRef().ExportToWkt() == "POINT (1 9)"
    assert dst_lyr.GetNextFeature().GetGeometryRef().ExportToWkt() == "POINT (2 4)"
    assert dst_lyr.GetNextFeature().GetGeometryRef().ExportToWkt() == "POINT (6 1)"
    assert dst_lyr.GetNextFeature().GetGeometryRef().ExportToWkt() == "POINT (0 6)"


def test_gdalalg_vector_explode_field_and_geometry(alg):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer(
        "test", geom_type=ogr.wkbMultiPoint, srs=osr.SpatialReference(epsg=4326)
    )
    src_lyr.CreateField(ogr.FieldDefn("ints", ogr.OFTIntegerList))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTIPOINT (3 2, 4 7, 1 9)"))
    f["ints"] = [6, 3, 2]
    src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["field"] = "ints"
    alg["geometry-field"] = "_ogr_geometry_"
    alg["output-format"] = "MEM"

    assert alg.Run()

    dst_ds = alg.Output()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 3

    features = [f for f in dst_lyr]

    assert features[0].GetGeometryRef().ExportToWkt() == "POINT (3 2)"
    assert features[0]["ints"] == 6

    assert features[1].GetGeometryRef().ExportToWkt() == "POINT (4 7)"
    assert features[1]["ints"] == 3

    assert features[2].GetGeometryRef().ExportToWkt() == "POINT (1 9)"
    assert features[2]["ints"] == 2


@pytest.mark.require_driver("GeoJSON")
def test_gdalalg_vector_explode_geometry_limit_type_with_pipeline(alg, tmp_vsimem):

    src_fname = tmp_vsimem / "src.json"
    dst_fname = tmp_vsimem / "dst.json"

    with gdal.GetDriverByName("GeoJSON").CreateVector(src_fname) as src_ds:
        src_lyr = src_ds.CreateLayer(
            "test",
            geom_type=ogr.wkbGeometryCollection,
            srs=osr.SpatialReference(epsg=4326),
        )

        f = ogr.Feature(src_lyr.GetLayerDefn())

        f.SetGeometry(
            ogr.CreateGeometryFromWkt(
                "GEOMETRYCOLLECTION (POINT (6 6), MULTIPOINT (2 1, 2 8), LINESTRING (3 3, 7 6))"
            )
        )
        src_lyr.CreateFeature(f)

        f.SetGeometry(
            ogr.CreateGeometryFromWkt(
                "GEOMETRYCOLLECTION (LINESTRING (0 0, 2 1), POINT (2 2), LINESTRING (6 0, 2 1))"
            )
        )
        src_lyr.CreateFeature(f)

    gdal.alg.vector.pipeline(
        pipeline=f'read {src_fname} ! explode --geometry-field 0"" ! set-geom-type --geometry-type LINESTRING --skip ! write -f GeoJSON {dst_fname}'
    )

    with gdal.Open(dst_fname) as dst_ds:
        assert dst_ds.GetLayerCount() == 1

        dst_lyr = dst_ds.GetLayer(0)

        assert dst_lyr.GetFeatureCount() == 3

        wkt = [f.GetGeometryRef().ExportToWkt() for f in dst_lyr]

        assert wkt[0] == "LINESTRING (3 3,7 6)"
        assert wkt[1] == "LINESTRING (0 0,2 1)"
        assert wkt[2] == "LINESTRING (6 0,2 1)"


def test_gdalalg_vector_explode_geometry_multiple(alg):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer(
        "test", geom_type=ogr.wkbMultiPoint, srs=osr.SpatialReference(epsg=4326)
    )
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("geom2", ogr.wkbGeometryCollection))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("MULTIPOINT (3 2, 4 7, 1 9)"))
    f.SetGeomField(
        1,
        ogr.CreateGeometryFromWkt(
            "GEOMETRYCOLLECTION (POINT (6 6), MULTIPOINT (2 1, 2 8), LINESTRING (3 3, 7 6))"
        ),
    )

    src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["geometry-field"] = ["_ogr_geometry_", "geom2"]
    alg["output-format"] = "MEM"

    assert alg.Run()

    dst_ds = alg.Output()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 3

    features = [f for f in dst_lyr]

    assert features[0].GetGeomFieldRef(0).ExportToWkt() == "POINT (3 2)"
    assert features[0].GetGeomFieldRef(1).ExportToWkt() == "POINT (6 6)"

    assert features[1].GetGeomFieldRef(0).ExportToWkt() == "POINT (4 7)"
    assert features[1].GetGeomFieldRef(1).ExportToWkt() == "MULTIPOINT (2 1,2 8)"

    assert features[2].GetGeomFieldRef(0).ExportToWkt() == "POINT (1 9)"
    assert features[2].GetGeomFieldRef(1).ExportToWkt() == "LINESTRING (3 3,7 6)"


def test_gdalalg_vector_explode_geometry_multiple_unmatched(alg):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer(
        "test", geom_type=ogr.wkbMultiPoint, srs=osr.SpatialReference(epsg=4326)
    )
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("geom2", ogr.wkbGeometryCollection))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("MULTIPOINT (3 2, 4 7, 1 9)"))
    f.SetGeomField(
        1,
        ogr.CreateGeometryFromWkt("MULTIPOINT (4 3, 2 2)"),
    )

    src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["geometry-field"] = ["_ogr_geometry_", "geom2"]
    alg["output-format"] = "MEM"

    with pytest.raises(Exception, match="Geometry field 'geom2' .* has 2 elements"):
        alg.Run()


def test_gdalalg_vector_explode_geometry_multiple_multipart_and_singlepart(alg):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer(
        "test", geom_type=ogr.wkbMultiPoint, srs=osr.SpatialReference(epsg=4326)
    )
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("geom2", ogr.wkbLineString))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("MULTIPOINT (3 2, 4 7, 1 9)"))
    f.SetGeomField(
        1,
        ogr.CreateGeometryFromWkt("LINESTRING (2 2, 3 3)"),
    )

    src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["geometry-field"] = ["_ogr_geometry_", "geom2"]
    alg["output-format"] = "MEM"

    with pytest.raises(Exception, match="geom2.* is not a collection"):
        assert alg.Run()


def test_gdalalg_vector_explode_geometry_multiple_multipart_and_null(alg):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer(
        "test", geom_type=ogr.wkbMultiPoint, srs=osr.SpatialReference(epsg=4326)
    )
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("geom2", ogr.wkbMultiLineString))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("MULTIPOINT (3 2, 4 7, 1 9)"))

    src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["geometry-field"] = ["_ogr_geometry_", "geom2"]
    alg["output-format"] = "MEM"

    assert alg.Run()

    dst_ds = alg.Output()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 3

    features = [f for f in dst_lyr]

    assert features[0].GetGeomFieldRef(0).ExportToWkt() == "POINT (3 2)"
    assert features[0].GetGeomFieldRef(1) is None

    assert features[1].GetGeomFieldRef(0).ExportToWkt() == "POINT (4 7)"
    assert features[1].GetGeomFieldRef(1) is None

    assert features[2].GetGeomFieldRef(0).ExportToWkt() == "POINT (1 9)"
    assert features[2].GetGeomFieldRef(1) is None


def test_gdalalg_vector_explode_geometry_null(alg):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer(
        "test", geom_type=ogr.wkbMultiPoint, srs=osr.SpatialReference(epsg=4326)
    )
    src_lyr.CreateField(ogr.FieldDefn("int_array", ogr.OFTIntegerList))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["int_array"] = [1, 2]
    src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["field"] = "ALL"
    alg["geometry-field"] = "ALL"
    alg["output-format"] = "MEM"

    assert alg.Run()

    dst_ds = alg.Output()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 2

    features = [f for f in dst_lyr]

    assert features[0]["int_array"] == 1
    assert features[0].GetGeometryRef() is None

    assert features[1]["int_array"] == 2
    assert features[1].GetGeometryRef() is None


@pytest.mark.require_driver("GML")
def test_gdalalg_vector_explode_geometry_multiple_cartesian_product_using_pipeline(
    alg, tmp_vsimem
):

    src_fname = tmp_vsimem / "in.gml"
    dst_fname = tmp_vsimem / "out.gml"

    with gdal.GetDriverByName("GML").CreateVector(src_fname) as src_ds:
        src_lyr = src_ds.CreateLayer(
            "test", geom_type=ogr.wkbNone, srs=osr.SpatialReference(epsg=4326)
        )
        src_lyr.CreateGeomField(ogr.GeomFieldDefn("geom1", ogr.wkbPoint))
        src_lyr.CreateGeomField(ogr.GeomFieldDefn("geom2", ogr.wkbPoint))

        f = ogr.Feature(src_lyr.GetLayerDefn())
        f.SetGeomField(0, ogr.CreateGeometryFromWkt("MULTIPOINT (3 2, 4 7, 1 9)"))
        f.SetGeomField(
            1,
            ogr.CreateGeometryFromWkt("MULTIPOINT (4 3, 2 2)"),
        )

        src_lyr.CreateFeature(f)

    gdal.alg.vector.pipeline(
        pipeline=f"read {src_fname} ! explode --geometry ! explode --geometry-field 1 ! write {dst_fname} --of GML"
    )

    with gdal.Open(dst_fname) as dst_ds:
        assert dst_ds.GetLayerCount() == 1

        dst_lyr = dst_ds.GetLayer(0)

        assert dst_lyr.GetFeatureCount() == 6

        wkt = [
            (f.GetGeomFieldRef(0).ExportToWkt(), f.GetGeomFieldRef(1).ExportToWkt())
            for f in dst_lyr
        ]

        assert wkt[0] == ("POINT (3 2)", "POINT (4 3)")
        assert wkt[1] == ("POINT (3 2)", "POINT (2 2)")
        assert wkt[2] == ("POINT (4 7)", "POINT (4 3)")
        assert wkt[3] == ("POINT (4 7)", "POINT (2 2)")
        assert wkt[4] == ("POINT (1 9)", "POINT (4 3)")
        assert wkt[5] == ("POINT (1 9)", "POINT (2 2)")


def test_gdalalg_vector_explode_geometry_with_singlepart(alg):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer(
        "test", geom_type=ogr.wkbPoint, srs=osr.SpatialReference(epsg=4326)
    )
    src_lyr.CreateField(ogr.FieldDefn("int_array", ogr.OFTIntegerList))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["int_array"] = [5]
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("POINT (8 2)"))

    src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["field"] = "ALL"
    alg["geometry-field"] = "ALL"
    alg["output-format"] = "MEM"

    assert alg.Run()

    dst_ds = alg.Output()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 1

    f = dst_lyr.GetNextFeature()

    assert f["int_array"] == 5
    assert f.GetGeometryRef().ExportToWkt() == "POINT (8 2)"


def test_gdalalg_vector_explode_active_layer(alg):

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)

    src_lyr = src_ds.CreateLayer("the_layer", geom_type=ogr.wkbMultiPoint)
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("MULTIPOINT (1 2,3 4)"))
    src_lyr.CreateFeature(f)

    src_lyr = src_ds.CreateLayer("other_layer", geom_type=ogr.wkbUnknown)
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("MULTIPOINT (5 6,7 8)"))
    src_lyr.CreateFeature(f)

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["active-layer"] = "the_layer"
    alg["geometry"] = True

    assert alg.Run()

    out_ds = alg["output"].GetDataset()

    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbPoint

    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(0), "POINT (1 2)")
    assert out_f.GetFID() == 1

    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(0), "POINT (3 4)")
    assert out_f.GetFID() == 2

    out_lyr = out_ds.GetLayer(1)
    assert out_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbUnknown

    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(0), "MULTIPOINT (5 6,7 8)")
    assert out_f.GetFID() == 0


@pytest.mark.require_driver("GeoJSON")
def test_gdalalg_vector_explode_autocomplete(tmp_path):

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary not available")

    src_fname = tmp_path / "src.geojson"

    with gdal.GetDriverByName("GeoJSON").CreateVector(src_fname) as ds:
        lyr = ds.CreateLayer("layer")
        lyr.CreateField(ogr.FieldDefn("field1", ogr.OFTReal))
        lyr.CreateField(ogr.FieldDefn("field2", ogr.OFTIntegerList))

        f = ogr.Feature(lyr.GetLayerDefn())
        f["field1"] = 8.02
        f["field2"] = [8, 0, 2]

        lyr.CreateFeature(f)

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal vector explode {src_fname} --field last_word_is_complete=true"
    )

    assert out == ["ALL", "field2"]


@pytest.mark.require_driver("GeoJSON")
def test_gdalalg_vector_explode_ogrsf(alg, source_with_arrays, tmp_path):

    src_fname = tmp_path / "in.geojson"
    gdal.VectorTranslate(src_fname, source_with_arrays)

    alg["input"] = src_fname
    alg["field"] = ["name", "my_int_list"]

    gdaltest.algorithm_check_ogrsf(alg, tmp_path)


@pytest.mark.require_driver("OSM")
def test_gdalalg_vector_explode_pipeline_layer_interleaved(tmp_vsimem):

    with gdal.alg.vector.pipeline(
        input="../ogr/data/osm/test.pbf",
        pipeline='read --layer lines  ! explode --geometry ! filter --where "highway IS NOT NULL" ! write --format=MEM --output=""',
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1
        f = lyr.GetNextFeature()
        assert f["osm_id"] == "1"
        assert f["highway"] == "motorway"
