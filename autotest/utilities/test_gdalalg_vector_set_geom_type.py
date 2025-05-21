#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector set-geom-type' testing
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
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["set-geom-type"]


def test_gdalalg_vector_set_geom_type_geometry_type():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 0)"))
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
    ogrtest.check_feature_geometry(out_f, "POINT Z (3 0 0)")
    assert out_lyr.GetFeatureCount() == 1
    assert out_lyr.TestCapability(ogr.OLCFastFeatureCount) == 1
    assert out_lyr.TestCapability(ogr.OLCRandomWrite) == 0
    assert out_lyr.GetExtent() == (3, 3, 0, 0)
    assert out_lyr.GetFeature(0).GetFID() == 0
    assert out_lyr.GetFeature(-1) is None


def test_gdalalg_vector_set_geom_type_geometry_type_invalid():

    alg = get_alg()
    with pytest.raises(
        Exception, match="set-geom-type: Invalid geometry type 'invalid'"
    ):
        alg["geometry-type"] = "invalid"


@pytest.mark.parametrize("other_option", ["multi", "single", "linear", "curve", "dim"])
def test_gdalalg_vector_set_geom_type_geometry_type_exclusive_with_other_option(
    other_option,
):

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["geometry-type"] = "POINT"
    alg[other_option] = "XY" if other_option == "dim" else True

    with pytest.raises(Exception, match="cannot be used with any of"):
        alg.Run()


def test_gdalalg_vector_set_geom_type_geometry_type_failed_no_skip():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 0)"))
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["geometry-type"] = "LINESTRING"

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetGeomType() == ogr.wkbLineString
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f, "POINT (3 0)")
    assert out_lyr.GetFeatureCount() == 1
    assert out_lyr.TestCapability(ogr.OLCFastFeatureCount) == 1


def test_gdalalg_vector_set_geom_type_geometry_type_failed_skip():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 0)"))
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING Z (3 0 1,4 0 1)"))
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["geometry-type"] = "LINESTRING"
    alg["skip"] = True

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetGeomType() == ogr.wkbLineString
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f, "LINESTRING (3 0,4 0)")
    out_f = out_lyr.GetNextFeature()
    assert out_f is None
    assert out_lyr.GetFeatureCount() == 1
    assert out_lyr.TestCapability(ogr.OLCFastFeatureCount) == 0


def test_gdalalg_vector_set_geom_type_geometry_type_layer_only():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 0)"))
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["geometry-type"] = "POINTZ"
    alg["layer-only"] = True

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetGeomType() == ogr.wkbPoint25D
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f, "POINT (3 0)")
    assert out_lyr.GetFeatureCount() == 1
    assert out_lyr.TestCapability(ogr.OLCFastFeatureCount) == 1


def test_gdalalg_vector_set_geom_type_geometry_type_feature_only():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 0)"))
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["geometry-type"] = "POINTZ"
    alg["feature-only"] = True

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetGeomType() == ogr.wkbUnknown
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f, "POINT Z (3 0 0)")
    assert out_lyr.GetFeatureCount() == 1
    assert out_lyr.TestCapability(ogr.OLCFastFeatureCount) == 1


@pytest.mark.parametrize(
    "modifier,in_wkt,out_wkt",
    [
        (("dim", "xy"), "POINT Z (1 2 3)", "POINT (1 2)"),
        (("dim", "xyz"), "POINT (1 2)", "POINT Z (1 2 0)"),
        (("dim", "xym"), "POINT (1 2 3 4)", "POINT M (1 2 4)"),
        (("dim", "xyzm"), "POINT (1 2)", "POINT ZM (1 2 0 0)"),
        (("dim", "xyzm"), "POINT ZM (1 2 3 4)", "POINT ZM (1 2 3 4)"),
        ("multi", "POINT Z (1 2 3)", "MULTIPOINT Z ((1 2 3))"),
        ("single", "MULTIPOINT Z ((1 2 3))", "POINT Z (1 2 3)"),
        ("single", "MULTILINESTRING ((1 2,3 4))", "LINESTRING (1 2,3 4)"),
        ("single", "MULTIPOLYGON (((0 0,0 1,1 1,0 0)))", "POLYGON ((0 0,0 1,1 1,0 0))"),
        ("single", "MULTICURVE ((1 2,3 4))", "COMPOUNDCURVE ((1 2,3 4))"),
        (
            "single",
            "MULTISURFACE (((0 0,0 1,1 1,0 0)))",
            "CURVEPOLYGON ((0 0,0 1,1 1,0 0))",
        ),
        (
            "linear",
            "CIRCULARSTRING(0 0,1 1,2 0)",
            "LINESTRING (0 0,0.002435949740176 0.069756473744125,0.009731931258429 0.139173100960061,0.021852399266194 0.20791169081776,0.038738304061681 0.275637355817011,0.060307379214091 0.342020143325669,0.086454542357401 0.406736643075803,0.117052407141074 0.469471562785898,0.151951903843575 0.529919264233229,0.190983005625057 0.587785252292491,0.233955556881021 0.642787609686564,0.280660199661355 0.694658370459024,0.330869393641151 0.743144825477401,0.384338524674348 0.788010753606727,0.440807096529255 0.829037572555052,0.5 0.866025403784448,0.561628853210948 0.898794046299173,0.625393406584095 0.927183854566806,0.690983005625071 0.951056516295154,0.75807810440034 0.970295726276021,0.826351822333095 0.984807753012234,0.895471536732373 0.99452189536828,0.965100503297521 0.999390827019113,1.03489949670251 0.999390827019113,1.10452846326768 0.99452189536828,1.17364817766696 0.984807753012234,1.24192189559972 0.970295726276021,1.30901699437499 0.951056516295182,1.37460659341593 0.927183854566806,1.43837114678911 0.898794046299173,1.50000000000006 0.866025403784448,1.55919290347077 0.829037572555052,1.61566147532568 0.788010753606727,1.66913060635886 0.743144825477401,1.71933980033867 0.694658370459024,1.76604444311903 0.642787609686564,1.80901699437499 0.587785252292491,1.84804809615645 0.529919264233229,1.88294759285895 0.469471562785898,1.91354545764261 0.406736643075803,1.93969262078593 0.342020143325669,1.96126169593833 0.275637355817011,1.97814760073385 0.20791169081776,1.99026806874161 0.139173100960068,1.99756405025983 0.069756473744128,2 0)",
        ),
        ("curve", "LINESTRING (0 0,1 1)", "COMPOUNDCURVE((0 0,1 1))"),
    ],
)
def test_gdalalg_vector_set_geom_type_other_modifiers(modifier, in_wkt, out_wkt):

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt(in_wkt))
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"

    if type(modifier) is str:
        alg[modifier] = True
    else:
        alg[modifier[0]] = modifier[1]

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f, out_wkt)
    assert out_lyr.GetFeatureCount() == 1
    assert out_lyr.TestCapability(ogr.OLCFastFeatureCount) == 1


def test_gdalalg_vector_geom_active_layer_active_geometry():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)

    src_lyr = src_ds.CreateLayer("the_layer", geom_type=ogr.wkbNone)
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("a"))
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("b"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("POINT (1 2)"))
    f.SetGeomField(1, ogr.CreateGeometryFromWkt("POINT (3 4)"))
    src_lyr.CreateFeature(f)

    src_lyr = src_ds.CreateLayer("other_layer", geom_type=ogr.wkbNone)
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("a"))
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("b"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("POINT (5 6)"))
    f.SetGeomField(1, ogr.CreateGeometryFromWkt("POINT (7 8)"))
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["geometry-type"] = "POINTZ"
    alg["active-layer"] = "the_layer"
    alg["active-geometry"] = "b"

    assert alg.Run()

    out_ds = alg["output"].GetDataset()

    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbUnknown
    assert out_lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() == ogr.wkbPoint25D
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(0), "POINT (1 2)")
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(1), "POINT Z (3 4 0)")

    out_lyr = out_ds.GetLayer(1)
    assert out_lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbUnknown
    assert out_lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() == ogr.wkbUnknown
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(0), "POINT (5 6)")
    ogrtest.check_feature_geometry(out_f.GetGeomFieldRef(1), "POINT (7 8)")


def test_gdalalg_vector_set_geom_type_type_autocomplete():

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary not available")

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal vector set-geom-type --geometry-type"
    ).split(" ")
    assert "GEOMETRY" in out
    assert "GEOMETRYZ" in out
    assert "GEOMETRYM" in out
    assert "GEOMETRYZM" in out
    assert "POINT" in out
    assert "MULTIPOINT" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal vector set-geom-type --geometry-type GEOMETRYC"
    ).split(" ")
    assert len(out) == 4


@pytest.mark.require_driver("GDALG")
def test_gdalalg_vector_set_geom_type_test_ogrsf(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    gdalg_filename = tmp_path / "tmp.gdalg.json"
    open(gdalg_filename, "wb").write(
        b'{"type": "gdal_streamed_alg","command_line": "gdal vector set-geom-type --geometry-type=MULTIPOLYGON ../ogr/data/poly.shp --output-format=stream foo","relative_paths_relative_to_this_file":false}'
    )

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" -ro {gdalg_filename}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret
    assert "FAILURE" not in ret
