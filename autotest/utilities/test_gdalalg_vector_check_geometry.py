#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector check-geometry' testing
# Author:   Daniel Baston
#
###############################################################################
# Copyright (c) 2025, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_geos()


@pytest.fixture()
def alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["check-geometry"]


@pytest.fixture()
def polys():

    return gdaltest.wkt_ds(
        [
            "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",
            "POLYGON ((0 0, 10 0, 0 10, 10 10, 0 0))",
            "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0), (15 15, 15 20, 20 15, 15 15))",
        ],
        geom_type=ogr.wkbPolygon,
        epsg=32145,
    )


@pytest.fixture()
def lines():

    return gdaltest.wkt_ds(
        [
            "LINESTRING (0 0, 10 0, 10 10, 0 10, 0 0)",
            "LINESTRING (0 0, 10 0, 0 10, 10 10, 0 0)",
        ],
        geom_type=ogr.wkbLineString,
        epsg=32145,
    )


def test_gdalalg_vector_check_geometry(alg, polys):

    src_lyr = polys.GetLayer(0)

    alg["input"] = polys
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    errors = [f for f in dst_lyr]

    assert len(errors) == 2
    assert errors[0]["error"] == "Self-intersection"
    assert errors[0].GetGeometryRef().ExportToWkt() == "POINT (5 5)"

    assert errors[1]["error"] == "Hole lies outside shell"
    assert errors[1].GetGeometryRef().ExportToWkt() == "POINT (15 15)"

    assert dst_lyr.GetFeatureCount() == 2
    assert dst_lyr.GetSpatialRef().IsSame(src_lyr.GetSpatialRef())

    assert alg.Finalize()


def test_gdalalg_vector_check_geometry_include_valid(alg, polys):

    src_lyr = polys.GetLayer(0)

    alg["input"] = polys
    alg["include-valid"] = True
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == src_lyr.GetFeatureCount()

    out = [f for f in dst_lyr]

    assert out[0]["error"] is None
    assert out[0].GetGeometryRef() is None

    assert alg.Finalize()


def test_gdalalg_vector_geometry_non_polygon(alg, lines):

    alg["input"] = lines
    alg["output"] = ""
    alg["output-format"] = "stream"

    # TODO: this warning is printed, why is it not captured?
    # with gdaltest.error_raised(gdal.CE_Warning, "always valid"):
    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    errors = [f for f in dst_lyr]
    assert not errors

    assert alg.Finalize()


def test_gdalalg_vector_geometry_non_closed_polygon_ring(alg):

    alg["input"] = gdaltest.wkt_ds("POLYGON ((7 3, 10 0, 10 10))")

    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    errors = [f for f in dst_lyr]

    assert (
        errors[0]["error"].lower()
        == "points of linearring do not form a closed linestring"
    )
    assert errors[0].GetGeometryRef().ExportToWkt() == "POINT (7 3)"

    assert alg.Finalize()


def test_gdalalg_vector_geometry_single_point_polygon(alg):

    alg["input"] = gdaltest.wkt_ds("POLYGON ((7 3))")

    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    errors = [f for f in dst_lyr]

    assert errors[0]["error"].lower() == "point array must contain 0 or >1 elements"
    assert errors[0].GetGeometryRef().ExportToWkt() == "POINT (7 3)"

    assert alg.Finalize()


def test_gdalalg_vector_geometry_two_point_polygon(alg):

    alg["input"] = gdaltest.wkt_ds("POLYGON ((7 3, 7 3))")

    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    errors = [f for f in dst_lyr]

    assert "invalid number of points in linearring" in errors[0]["error"].lower()
    assert errors[0].GetGeometryRef().ExportToWkt() == "POINT (7 3)"

    assert alg.Finalize()


def test_gdalalg_vector_geometry_empty_shell_polygon_with_hole(alg):

    ds = gdal.GetDriverByName("MEM").CreateVector("")

    lyr = ds.CreateLayer("src")
    f = ogr.Feature(lyr.GetLayerDefn())

    # This polygon cannot be created from WKT
    g = ogr.Geometry(ogr.wkbPolygon)
    shell = ogr.Geometry(ogr.wkbLinearRing)
    hole = ogr.Geometry(ogr.wkbLinearRing)
    hole.AddPoint(5, 5)
    hole.AddPoint(10, 5)
    hole.AddPoint(10, 10)
    hole.AddPoint(5, 5)

    g.AddGeometry(shell)
    g.AddGeometry(hole)

    f.SetGeometry(g)

    lyr.CreateFeature(f)

    alg["input"] = ds

    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    errors = [f for f in dst_lyr]

    assert "shell is empty but holes are not" in errors[0]["error"].lower()
    # assert errors[0].GetGeometryRef().ExportToWkt() == "POINT (5 5)"

    assert alg.Finalize()


def test_gdalalg_vector_check_geometry_invalid_layer(alg, polys):

    alg["input"] = polys
    alg["input-layer"] = "does_not_exist"
    alg["output"] = ""
    alg["output-format"] = "stream"

    with pytest.raises(RuntimeError, match="Cannot find source layer"):
        assert alg.Run()


def test_gdalalg_vector_check_geometry_two_layers(alg, polys, lines):

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    ds.CopyLayer(lines.GetLayer(0), "lines2")
    ds.CopyLayer(polys.GetLayer(0), "poly2")

    alg["input"] = ds
    alg["input-layer"] = "poly2"
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 2

    assert alg.Finalize()


def test_gdalalg_vector_check_geometry_invalid_geometry_name(alg, polys):

    alg["input"] = polys
    alg["geometry-field"] = "does_not_exist"
    alg["output"] = ""
    alg["output-format"] = "stream"

    with pytest.raises(
        RuntimeError, match="Specified geometry field .* does not exist"
    ):
        assert alg.Run()


def test_gdalalg_vector_check_geometry_multiple_geometry_fields(alg):

    ds = gdal.GetDriverByName("MEM").CreateVector("")

    lyr = ds.CreateLayer("source", geom_type=ogr.wkbPoint)
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom2", ogr.wkbPolygon))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeomField(0, ogr.CreateGeometryFromWkt("POINT (3 7)"))
    feat.SetGeomField(
        1, ogr.CreateGeometryFromWkt("POLYGON ((0 0, 10 0, 0 10, 10 10, 0 0))")
    )

    lyr.CreateFeature(feat)

    alg["input"] = ds
    alg["geometry-field"] = "geom2"
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 1

    assert alg.Finalize()
