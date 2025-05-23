#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector make-valid' testing
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

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_geos


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["make-valid"]


def test_gdalalg_vector_make_valid():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    src_lyr = src_ds.CreateLayer("the_layer", srs=srs)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,1 1,1 0,0 1,0 0))"))
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f.GetGeometryRef().GetGeometryType() == ogr.wkbMultiPolygon
    assert (
        out_f.GetGeometryRef().GetSpatialReference().GetAuthorityCode(None) == "32631"
    )
    out_f = out_lyr.GetNextFeature()
    assert out_f.GetGeometryRef().ExportToIsoWkt() == "POINT (1 2)"
    out_f = out_lyr.GetNextFeature()
    assert out_f.GetGeometryRef() is None
    assert out_lyr.GetFeatureCount() == 3
    out_lyr.SetAttributeFilter("0 = 1")
    assert out_lyr.GetFeatureCount() == 0
    out_lyr.SetAttributeFilter(None)
    assert out_lyr.TestCapability(ogr.OLCFastFeatureCount) == 1
    assert out_lyr.TestCapability(ogr.OLCRandomWrite) == 0
    assert out_lyr.GetExtent() == (0.0, 1.0, 0.0, 2.0)
    assert out_lyr.GetFeature(0).GetFID() == 0
    assert out_lyr.GetFeature(-1) is None


def test_gdalalg_vector_make_valid_active_layer():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,1 1,1 0,0 1,0 0))"))
    src_lyr.CreateFeature(f)

    src_lyr = src_ds.CreateLayer("other_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,1 1,1 0,0 1,0 0))"))
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["active-layer"] = "the_layer"

    assert alg.Run()

    out_ds = alg["output"].GetDataset()

    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f.GetGeometryRef().GetGeometryType() == ogr.wkbMultiPolygon

    out_lyr = out_ds.GetLayer(1)
    out_f = out_lyr.GetNextFeature()
    assert out_f.GetGeometryRef().GetGeometryType() == ogr.wkbPolygon


def test_gdalalg_vector_make_valid_active_geometry():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer", geom_type=ogr.wkbNone)
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("a"))
    src_lyr.CreateGeomField(ogr.GeomFieldDefn("b"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("POLYGON ((0 0,1 1,1 0,0 1,0 0))"))
    f.SetGeomField(1, ogr.CreateGeometryFromWkt("POLYGON ((0 0,1 1,1 0,0 1,0 0))"))
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["active-geometry"] = "b"

    assert alg.Run()

    out_ds = alg["output"].GetDataset()

    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f.GetGeomFieldRef(0).GetGeometryType() == ogr.wkbPolygon
    assert out_f.GetGeomFieldRef(1).GetGeometryType() == ogr.wkbMultiPolygon


def test_gdalalg_vector_make_valid_skip_lower_dim():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON ((0 0,0 1,0.5 1,0.5 0.75,0.5 1,1 1,1 0,0 0))"
        )
    )
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f, "POLYGON ((0 0,0 1,0.5 1.0,1 1,1 0,0 0))")


def test_gdalalg_vector_make_valid_keep_lower_dim():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON ((0 0,0 1,0.5 1,0.5 0.75,0.5 1,1 1,1 0,0 0))"
        )
    )
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["keep-lower-dim"] = True

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f.GetGeometryRef().GetGeometryType() == ogr.wkbGeometryCollection


@pytest.mark.require_geos(3, 10, 0)
@pytest.mark.parametrize(
    "input_wkt,options,output_wkt",
    [
        ("LINESTRING (0 0,0 0)", {}, "POINT (0 0)"),
        ("LINESTRING (0 0,0 0)", {"method": "structure"}, "LINESTRING EMPTY"),
        (
            "LINESTRING (0 0,0 0)",
            {"method": "structure", "keep-lower-dim": True},
            "POINT (0 0)",
        ),
    ],
)
def test_gdalalg_vector_make_valid_options(input_wkt, options, output_wkt):

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt(input_wkt))
    src_lyr.CreateFeature(f)

    alg = get_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    for k in options:
        alg[k] = options[k]

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    # print(out_f.GetGeometryRef().ExportToIsoWkt())
    ogrtest.check_feature_geometry(out_f, output_wkt)


@pytest.mark.require_driver("GDALG")
def test_gdalalg_vector_make_valid_test_ogrsf(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    gdalg_filename = tmp_path / "tmp.gdalg.json"
    open(gdalg_filename, "wb").write(
        b'{"type": "gdal_streamed_alg","command_line": "gdal vector make-valid ../ogr/data/poly.shp --output-format=stream foo","relative_paths_relative_to_this_file":false}'
    )

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" -ro {gdalg_filename}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret
    assert "FAILURE" not in ret
