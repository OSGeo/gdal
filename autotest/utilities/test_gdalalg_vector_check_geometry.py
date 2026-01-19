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
import ogrtest
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
            "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))",  # square
            "POLYGON ((0 0, 10 0, 0 10, 10 10, 0 0))",  # bowtie
            "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0), (15 15, 15 20, 20 15, 15 15))",  # hole outside shell
        ],
        geom_type=ogr.wkbPolygon,
        epsg=32145,
    )


@pytest.fixture()
def lines():

    return gdaltest.wkt_ds(
        [
            "LINESTRING (0 0, 10 0, 10 10, 0 10, 0 0)",  # square
            "LINESTRING (0 0, 10 0, 0 10, 10 10, 0 0)",  # bowtie
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
    assert dst_lyr.GetName() == "error_location"
    assert dst_lyr.GetLayerDefn().GetGeomType() == ogr.wkbPoint

    errors = [f for f in dst_lyr]

    assert len(errors) == 2
    assert errors[0]["error"] == "Self-intersection"
    assert errors[0].GetGeometryRef().ExportToWkt() == "POINT (5 5)"
    assert errors[0].GetFID() == 2

    assert errors[1]["error"] == "Hole lies outside shell"
    assert errors[1].GetGeometryRef().ExportToWkt() == "POINT (15 15)"
    assert errors[1].GetFID() == 3

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
    assert out[0].GetFID() == 1

    assert alg.Finalize()


def test_gdalalg_vector_check_geometry_linestring(alg, lines):

    alg["input"] = lines
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 1

    out = [f for f in dst_lyr]

    assert out[0]["error"] == "self-intersection"
    if ogrtest.have_geos() and (
        ogr.GetGEOSVersionMajor(),
        ogr.GetGEOSVersionMinor(),
    ) >= (3, 14):
        assert out[0].GetGeometryRef().ExportToWkt() == "POINT (5 5)"
    assert out[0].GetFID() == 2

    assert alg.Finalize()


def test_gdalalg_vector_check_geometry_linestring_multiple_self_intersections(alg):

    alg["input"] = gdaltest.wkt_ds("LINESTRING (2 1, 0 0, 2 2, 1 2, 1 0)")

    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 1

    out = [f for f in dst_lyr]

    assert out[0]["error"] == "self-intersection"

    if ogrtest.have_geos() and (
        ogr.GetGEOSVersionMajor(),
        ogr.GetGEOSVersionMinor(),
    ) >= (3, 14):
        assert (
            out[0]
            .GetGeometryRef()
            .Equals(ogr.CreateGeometryFromWkt("MULTIPOINT ((1 0.5), (1 1))"))
        )
    assert out[0].GetFID() == 1


def test_gdalalg_vector_check_geometry_curvepolygon(alg):

    alg["input"] = gdaltest.wkt_ds(
        "CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (0 0, 1 1, 2 0), (2 0, 1 1.1, 0 0)))"
    )

    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 1

    out = [f for f in dst_lyr]

    assert out[0].GetFID() == 1


def test_gdalalg_vector_check_geometry_compoundcurve(alg):

    alg["input"] = gdaltest.wkt_ds(
        "COMPOUNDCURVE (CIRCULARSTRING (0 0, 1 1, 2 0), (2 0, 1 1, 0 0))"
    )

    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == 1

    out = [f for f in dst_lyr]

    assert out[0].GetFID() == 1
    assert out[0]["error"] == "self-intersection"

    if ogrtest.have_geos() and (
        ogr.GetGEOSVersionMajor(),
        ogr.GetGEOSVersionMinor(),
    ) >= (3, 14):
        expected = ogr.CreateGeometryFromWkt("POINT (1 1)")
        assert out[0].GetGeometryRef().Distance(expected) < 1e-3


def test_gdalalg_vector_check_geometry_single_point_linestring(alg):

    alg["input"] = gdaltest.wkt_ds("LINESTRING (3 2)")

    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 1

    out = [f for f in dst_lyr]

    assert out[0]["error"] == "point array must contain 0 or >1 elements"


def test_gdalalg_vector_check_geometry_point(alg):

    alg["input"] = gdaltest.wkt_ds("POINT (3 8)")

    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 0


def test_gdalalg_vector_check_geometry_geometry_collection(alg):

    # valid polygon + non-simple linestring

    alg["input"] = gdaltest.wkt_ds(
        "GEOMETRYCOLLECTION ("
        "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0)),"
        "LINESTRING (0 0, 10 0, 0 10, 10 10, 0 0))"
    )

    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)
    assert dst_lyr.GetFeatureCount() == 1

    out = [f for f in dst_lyr]
    assert out[0].GetFID() == 1
    assert out[0]["error"] == "self-intersection"
    if ogrtest.have_geos() and (
        ogr.GetGEOSVersionMajor(),
        ogr.GetGEOSVersionMinor(),
    ) >= (3, 14):
        assert out[0].GetGeometryRef().ExportToWkt() == "POINT (5 5)"


def test_gdalalg_vector_check_geometry_non_closed_polygon_ring(alg):

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


def test_gdalalg_vector_check_geometry_single_point_polygon(alg):

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


def test_gdalalg_vector_check_geometry_two_point_polygon(alg):

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


def test_gdalalg_vector_check_geometry_empty_shell_polygon_with_hole(alg):

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


def test_gdalalg_vector_check_geometry_multiple_layers(alg):

    ds = gdal.GetDriverByName("MEM").CreateVector("")

    ds.CreateLayer("source1")
    ds.CreateLayer("source2")

    alg["input"] = ds
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    assert dst_ds.GetLayer(0).GetName() == "error_location_source1"
    assert dst_ds.GetLayer(1).GetName() == "error_location_source2"


def test_gdalalg_vector_check_geometry_no_geometry_field(alg):

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    ds.CreateLayer("source", geom_type=ogr.wkbNone)

    alg["input"] = ds
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    assert dst_ds.GetLayerCount() == 0

    alg["input"] = ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["input-layer"] = "source"

    with pytest.raises(
        Exception, match="Specified layer 'source' has no geometry field"
    ):
        alg.Run()


def test_gdalalg_vector_check_geometry_include_field(alg):

    alg["input"] = "../ogr/data/poly.shp"
    alg["include-field"] = ["EAS_ID", "AREA"]
    alg["output-format"] = "stream"
    alg["include-valid"] = True

    assert alg.Run()
    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)
    dst_defn = dst_lyr.GetLayerDefn()
    assert dst_defn.GetFieldDefn(0).GetName() == "EAS_ID"
    assert dst_defn.GetFieldDefn(1).GetName() == "AREA"

    f = dst_lyr.GetNextFeature()
    assert f["EAS_ID"] == 168
    assert f["AREA"] == 215229.266


def test_gdalalg_vector_check_geometry_include_field_error(alg):

    alg["input"] = "../ogr/data/poly.shp"
    alg["include-field"] = "does_not_exist"
    alg["output-format"] = "stream"

    with pytest.raises(Exception, match="Specified field .* does not exist"):
        alg.Run()
