#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector explode-collections' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import ogrtest
import pytest
import test_cli_utilities

from osgeo import gdal, ogr


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["explode-collections"]


def _src_ds_two_fields():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer", geom_type=ogr.wkbNone)
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("a", ogr.wkbMultiPoint))
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("b", ogr.wkbMultiLineString))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("MULTIPOINT (1 2,3 4)"))
    f.SetGeomField(
        1, ogr.CreateGeometryFromWkt("MULTILINESTRING ((5 6,7 8),(9 10,11 12))")
    )
    f.SetFID(1)
    src_lyr.CreateFeature(f)

    return src_ds


def test_gdalalg_vector_explode_collections():

    alg = get_alg()
    alg["input"] = _src_ds_two_fields()
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbPoint
    assert out_lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() == ogr.wkbLineString

    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(0), "POINT (1 2)")
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(1), "LINESTRING (5 6,7 8)")
    assert out_f.GetFID() == 1

    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(0), "POINT (1 2)")
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(1), "LINESTRING (9 10,11 12)")
    assert out_f.GetFID() == 2

    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(0), "POINT (3 4)")
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(1), "LINESTRING (5 6,7 8)")
    assert out_f.GetFID() == 3

    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(0), "POINT (3 4)")
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(1), "LINESTRING (9 10,11 12)")
    assert out_f.GetFID() == 4

    assert out_lyr.GetFeatureCount() == 4
    assert out_lyr.TestCapability(ogr.OLCStringsAsUTF8) == 0
    assert out_lyr.TestCapability(ogr.OLCFastFeatureCount) == 0
    assert out_lyr.GetExtent() == (1, 3, 2, 4)

    got = []
    out_ds.ResetReading()
    while True:
        f, _ = out_ds.GetNextFeature()
        if not f:
            break
        got.append(str(f))
        assert f.GetFID() == len(got)
    assert len(got) == 4


def test_gdalalg_vector_explode_collections_active_geometry_a():

    alg = get_alg()
    alg["input"] = _src_ds_two_fields()
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["active-geometry"] = "a"

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbPoint
    assert (
        out_lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() == ogr.wkbMultiLineString
    )

    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(0), "POINT (1 2)")
    ogrtest.check_feature_geometry(
        out_f.GetGeomFieldRef(1), "MULTILINESTRING ((5 6,7 8),(9 10,11 12))"
    )
    assert out_f.GetFID() == 1

    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(0), "POINT (3 4)")
    ogrtest.check_feature_geometry(
        out_f.GetGeomFieldRef(1), "MULTILINESTRING ((5 6,7 8),(9 10,11 12))"
    )
    assert out_f.GetFID() == 2

    assert out_lyr.GetFeatureCount() == 2


def test_gdalalg_vector_explode_collections_active_geometry_b():

    alg = get_alg()
    alg["input"] = _src_ds_two_fields()
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["active-geometry"] = "b"

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbMultiPoint
    assert out_lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() == ogr.wkbLineString

    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(0), "MULTIPOINT (1 2,3 4)")
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(1), "LINESTRING (5 6,7 8)")
    assert out_f.GetFID() == 1

    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(0), "MULTIPOINT (1 2,3 4)")
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(1), "LINESTRING (9 10,11 12)")
    assert out_f.GetFID() == 2

    assert out_lyr.GetFeatureCount() == 2


def test_gdalalg_vector_explode_collections_active_layer():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)

    src_lyr = src_ds.CreateLayer("the_layer", geom_type=ogr.wkbMultiPoint)
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("MULTIPOINT (1 2,3 4)"))
    src_lyr.CreateFeature(f)

    src_lyr = src_ds.CreateLayer("other_layer", geom_type=ogr.wkbUnknown)
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("MULTIPOINT (5 6,7 8)"))
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["active-layer"] = "the_layer"

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


def test_gdalalg_vector_explode_collections_geometry_type():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTIPOINT (1 2,3 4)"))
    f.SetFID(1)
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["geometry-type"] = "POINTZ"

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetGeomType() == ogr.wkbPoint25D
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f, "POINT Z (1 2 0)")
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f, "POINT Z (3 4 0)")


def test_gdalalg_vector_explode_collections_geometry_type_skip():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "GEOMETRYCOLLECTION (POINT(1 2),LINESTRING(3 4,5 6),POINT(7 8))"
        )
    )
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(9 10))"))
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["geometry-type"] = "POINT"
    alg["skip-on-type-mismatch"] = True

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetGeomType() == ogr.wkbPoint
    out_f = out_lyr.GetNextFeature()
    assert out_f.GetGeometryRef() is None
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f, "POINT (1 2)")
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f, "POINT (7 8)")
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f, "POINT (9 10)")


def test_gdalalg_vector_explode_collections_type_invalid():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["geometry-type"] = "INVALID"

    with pytest.raises(
        Exception, match="explode-collections: Invalid geometry type 'INVALID'"
    ):
        alg.Run()


def test_gdalalg_vector_explode_collections_type_autocomplete():

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary not available")

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal vector explode-collections --geometry-type"
    ).split(" ")
    assert out == [
        "GEOMETRY",
        "GEOMETRYZ",
        "GEOMETRYM",
        "GEOMETRYZM",
        "POINT",
        "POINTZ",
        "POINTM",
        "POINTZM",
        "LINESTRING",
        "LINESTRINGZ",
        "LINESTRINGM",
        "LINESTRINGZM",
        "POLYGON",
        "POLYGONZ",
        "POLYGONM",
        "POLYGONZM",
        "CIRCULARSTRING",
        "CIRCULARSTRINGZ",
        "CIRCULARSTRINGM",
        "CIRCULARSTRINGZM",
        "COMPOUNDCURVE",
        "COMPOUNDCURVEZ",
        "COMPOUNDCURVEM",
        "COMPOUNDCURVEZM",
        "CURVEPOLYGON",
        "CURVEPOLYGONZ",
        "CURVEPOLYGONM",
        "CURVEPOLYGONZM",
        "POLYHEDRALSURFACE",
        "POLYHEDRALSURFACEZ",
        "POLYHEDRALSURFACEM",
        "POLYHEDRALSURFACEZM",
        "TIN",
        "TINZ",
        "TINM",
        "TINZM",
    ]

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal vector explode-collections --geometry-type COMPOUNDCURVE"
    ).split(" ")
    assert len(out) == 4


@pytest.mark.require_driver("GDALG")
def test_gdalalg_vector_explode_collections_test_ogrsf(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    gdalg_filename = tmp_path / "tmp.gdalg.json"
    open(gdalg_filename, "wb").write(
        b'{"type": "gdal_streamed_alg","command_line": "gdal vector explode-collections ../ogr/data/poly.shp --output-format=stream foo","relative_paths_relative_to_this_file":false}'
    )

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" -ro {gdalg_filename}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret
    assert "FAILURE" not in ret
