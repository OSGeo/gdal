#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGRGeoJSONSeq driver.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2018, Even Rouault <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("GeoJSONSeq")


def _ogr_geojsonseq_create(filename, lco, expect_rs):

    ds = ogr.GetDriverByName("GeoJSONSeq").CreateDataSource(filename)
    sr = osr.SpatialReference()
    sr.SetFromUserInput("WGS84")
    lyr = ds.CreateLayer("test", srs=sr, options=lco)
    assert lyr.GetDataset().GetDescription() == ds.GetDescription()
    assert lyr.CreateField(ogr.FieldDefn("foo")) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = 'bar"d'
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = "baz"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(3 4)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    assert lyr.GetFeatureCount() == 2

    ds = None

    f = gdal.VSIFOpenL(filename, "rb")
    first = gdal.VSIFReadL(1, 1, f).decode("ascii")
    gdal.VSIFCloseL(f)
    if expect_rs:
        assert first == "\x1e"
    else:
        assert first == "{"

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetDataset().GetDescription() == ds.GetDescription()
    assert not ds.TestCapability(ogr.ODsCCreateLayer)
    assert ds.CreateLayer("foo") is None
    assert not lyr.TestCapability(ogr.OLCCreateField)
    assert lyr.CreateField(ogr.FieldDefn("bar")) == ogr.OGRERR_FAILURE
    assert not lyr.TestCapability(ogr.OLCSequentialWrite)
    assert lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn())) == ogr.OGRERR_FAILURE

    f = lyr.GetNextFeature()
    assert f["foo"] == 'bar"d'
    assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"
    f = lyr.GetNextFeature()
    assert f["foo"] == "baz"
    assert f.GetGeometryRef().ExportToWkt() == "POINT (3 4)"
    assert lyr.GetNextFeature() is None
    ds = None

    # Test update mode on existing layer
    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCCreateField)
    assert lyr.CreateField(ogr.FieldDefn("bar")) == ogr.OGRERR_NONE
    f = ogr.Feature(lyr.GetLayerDefn())
    f["bar"] = "baz"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(5 6)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == 3
    assert len([f for f in lyr]) == 3
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f["foo"] == 'bar"d'
    assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(7 8)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == 4
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["foo"] == 'bar"d'
    assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"
    f = lyr.GetNextFeature()
    assert f["foo"] == "baz"
    assert f.GetGeometryRef().ExportToWkt() == "POINT (3 4)"
    f = lyr.GetNextFeature()
    assert f["bar"] == "baz"
    assert f.GetGeometryRef().ExportToWkt() == "POINT (5 6)"
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "POINT (7 8)"
    assert lyr.GetNextFeature() is None
    ds = None

    # Test update mode with a new layer
    ds = ogr.Open(filename, update=1)
    assert ds.TestCapability(ogr.ODsCCreateLayer)
    lyr = ds.CreateLayer("new", srs=sr)
    assert lyr.TestCapability(ogr.OLCCreateField)
    assert lyr.CreateField(ogr.FieldDefn("foo")) == ogr.OGRERR_NONE
    assert lyr.CreateField(ogr.FieldDefn("baz")) == ogr.OGRERR_NONE
    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = "foo"
    f["baz"] = "baw"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(9 10)"))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == 1
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["foo"] == 'bar"d'
    assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"
    f = lyr.GetNextFeature()
    assert f["foo"] == "baz"
    assert f.GetGeometryRef().ExportToWkt() == "POINT (3 4)"
    f = lyr.GetNextFeature()
    assert f["bar"] == "baz"
    assert f.GetGeometryRef().ExportToWkt() == "POINT (5 6)"
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "POINT (7 8)"
    f = lyr.GetNextFeature()
    assert f["foo"] == "foo"
    assert f["baz"] == "baw"
    assert f.GetGeometryRef().ExportToWkt() == "POINT (9 10)"
    assert lyr.GetNextFeature() is None
    ds = None

    f = gdal.VSIFOpenL(filename, "rb")
    content = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    if expect_rs:
        assert b"\x1e" in content
        assert b"}\n{" not in content
    else:
        assert b"\x1e" not in content
        assert b"\n" in content

    ogr.GetDriverByName("GeoJSONSeq").DeleteDataSource(filename)


@gdaltest.disable_exceptions()
def test_ogr_geojsonseq_lf():
    return _ogr_geojsonseq_create("/vsimem/test", [], False)


@gdaltest.disable_exceptions()
def test_ogr_geojsonseq_rs():
    return _ogr_geojsonseq_create("/vsimem/test", ["RS=YES"], True)


@gdaltest.disable_exceptions()
def test_ogr_geojsonseq_rs_auto():
    return _ogr_geojsonseq_create("/vsimem/test.geojsons", [], True)


def test_ogr_geojsonseq_inline():

    ds = ogr.Open("""{"type":"Feature","properties":{},"geometry":null}
{"type":"Feature","properties":{},"geometry":null}""")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2


def test_ogr_geojsonseq_prefix():

    ds = ogr.Open("""GeoJSONSeq:data/geojsonseq/test.geojsonl""")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2


def test_ogr_geojsonseq_seq_geometries():

    with gdaltest.config_option("OGR_GEOJSONSEQ_CHUNK_SIZE", "10"):
        ds = ogr.Open("""{"type":"Point","coordinates":[2,49]}
    {"type":"Point","coordinates":[3,50]}""")
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2
        f = lyr.GetNextFeature()
        if f.GetGeometryRef().ExportToWkt() != "POINT (2 49)":
            f.DumpReadable()
            pytest.fail()


@gdaltest.disable_exceptions()
def test_ogr_geojsonseq_seq_geometries_with_errors():

    with gdal.quiet_errors():
        ds = ogr.Open("""{"type":"Point","coordinates":[2,49]}
    {"type":"Point","coordinates":[3,50]}
    foo
    "bar"
    null

    {"type":"Point","coordinates":[3,51]}""")
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 3
        f = lyr.GetNextFeature()
        if f.GetGeometryRef().ExportToWkt() != "POINT (2 49)":
            f.DumpReadable()
            pytest.fail()
        f = lyr.GetNextFeature()
        if f.GetGeometryRef().ExportToWkt() != "POINT (3 50)":
            f.DumpReadable()
            pytest.fail()
        f = lyr.GetNextFeature()
        if f.GetGeometryRef().ExportToWkt() != "POINT (3 51)":
            f.DumpReadable()
            pytest.fail()


def test_ogr_geojsonseq_reprojection():

    filename = "/vsimem/ogr_geojsonseq_reprojection.geojsonl"
    ds = ogr.GetDriverByName("GeoJSONSeq").CreateDataSource(filename)
    sr = osr.SpatialReference()
    sr.SetFromUserInput("+proj=merc +datum=WGS84")
    lyr = ds.CreateLayer("test", srs=sr)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(222638.981586547 6242595.9999532)"))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != "POINT (2 49)":
        f.DumpReadable()
        pytest.fail()
    ds = None

    ogr.GetDriverByName("GeoJSONSeq").DeleteDataSource(filename)


def test_ogr_geojsonseq_read_rs_json_pretty():

    ds = ogr.Open("data/geojsonseq/test.geojsons")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f["foo"] != "bar" or f.GetGeometryRef().ExportToWkt() != "POINT (1 2)":
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f["foo"] != "baz" or f.GetGeometryRef().ExportToWkt() != "POINT (3 4)":
        f.DumpReadable()
        pytest.fail()
    assert lyr.GetNextFeature() is None


def test_ogr_geojsonseq_test_ogrsf():

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro data/geojsonseq/test.geojsonl"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Test effect of OGR_GEOJSON_MAX_OBJ_SIZE


@gdaltest.disable_exceptions()
def test_ogr_geojsonseq_feature_large():

    filename = "/vsimem/test_ogr_geojson_feature_large.geojsonl"
    feature = (
        '{"type":"Feature","properties":{},"geometry":{"type":"LineString","coordinates":[%s]}}'
        % ",".join(["[0,0]" for _ in range(20 * 1024)])
    )
    gdal.FileFromMemBuffer(filename, feature + "\n" + feature)
    assert ogr.Open(filename) is not None
    with gdaltest.config_option("OGR_GEOJSON_MAX_OBJ_SIZE", "0"):
        assert ogr.Open(filename) is not None
    with gdaltest.config_option("OGR_GEOJSON_MAX_OBJ_SIZE", "0.1"):
        with gdal.quiet_errors():
            assert ogr.Open(filename) is None
    gdal.Unlink(filename)


###############################################################################
# Test bugfix for #3892


def test_ogr_geojsonseq_feature_starting_with_big_properties():

    filename = "/vsimem/test_ogr_geojsonseq_feature_starting_with_big_properties"
    s = "\n".join(
        [
            '{"properties":{"foo":"%s"},"type":"Feature","geometry":null}'
            % ("x" * 10000)
            for i in range(2)
        ]
    )
    gdal.FileFromMemBuffer(
        filename,
        s,
    )
    ds = ogr.Open(filename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2
    gdal.Unlink(filename)


###############################################################################
# Test output on /vsistdout/


def test_ogr_geojsonseq_vsistdout():

    filename = "/vsimem/test_ogr_geojsonseq_vsistdout.geojsonl"
    gdal.ErrorReset()
    ds = ogr.GetDriverByName("GeoJSONSeq").CreateDataSource(
        "/vsistdout_redirect/" + filename
    )
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    lyr = ds.CreateLayer("test", srs=sr)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 49)"))
    lyr.CreateFeature(f)
    ds = None
    assert gdal.GetLastErrorMsg() == ""

    ds = ogr.Open(filename)
    assert ds.GetLayer(0).GetFeatureCount() == 1
    ds = None

    gdal.Unlink(filename)


###############################################################################
# Test output on /vsigzip/


def test_ogr_geojsonseq_vsigzip():

    filename = "/vsimem/test_ogr_geojsonseq_vsigzip.geojsonl.gz"
    gdal.ErrorReset()
    ds = ogr.GetDriverByName("GeoJSONSeq").CreateDataSource("/vsigzip/" + filename)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    lyr = ds.CreateLayer("test", srs=sr)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 49)"))
    lyr.CreateFeature(f)
    ds = None
    assert gdal.GetLastErrorMsg() == ""

    ds = ogr.Open("/vsigzip/" + filename)
    assert ds.GetLayer(0).GetFeatureCount() == 1
    ds = None

    gdal.Unlink(filename)


###############################################################################
# Test COORDINATE_PRECISION option


def test_ogr_geojsonseq_COORDINATE_PRECISION(tmp_vsimem):

    filename = str(tmp_vsimem / "test.geojsonl")
    ds = gdal.GetDriverByName("GeoJSONSeq").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds.CreateLayer("test", options=["COORDINATE_PRECISION=3"])
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 1e-3
    assert prec.GetZResolution() == 1e-3
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1.23456789 2.34567891 9.87654321)"))
    lyr.CreateFeature(f)
    ds.Close()

    with gdal.VSIFile(filename, "rb") as f:
        assert b'"coordinates":[1.235,2.346,9.877]' in f.read()


###############################################################################
# Test geometry coordinate precision support


def test_ogr_geojsonseq_geom_coord_precision_already_4326(tmp_vsimem):

    filename = str(tmp_vsimem / "test.geojsonl")
    ds = gdal.GetDriverByName("GeoJSONSeq").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    geom_fld = ogr.GeomFieldDefn("geometry", ogr.wkbUnknown)
    prec = ogr.CreateGeomCoordinatePrecision()
    prec.Set(1e-5, 1e-3, 0)
    geom_fld.SetCoordinatePrecision(prec)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    geom_fld.SetSpatialRef(srs)
    lyr = ds.CreateLayerFromGeomFieldDefn("test", geom_fld)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == 1e-5
    assert prec.GetZResolution() == 1e-3
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1.23456789 2.34567891 9.87654321)"))
    lyr.CreateFeature(f)
    ds.Close()

    with gdal.VSIFile(filename, "rb") as f:
        assert b'"coordinates":[1.23457,2.34568,9.877]' in f.read()


###############################################################################
# Test geometry coordinate precision support


def test_ogr_geojsonseq_geom_coord_precision_not_4326(tmp_vsimem):

    filename = str(tmp_vsimem / "test.geojsonl")
    ds = gdal.GetDriverByName("GeoJSONSeq").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    geom_fld = ogr.GeomFieldDefn("geometry", ogr.wkbUnknown)
    prec = ogr.CreateGeomCoordinatePrecision()
    prec.Set(1, 1e-3, 0)
    geom_fld.SetCoordinatePrecision(prec)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    geom_fld.SetSpatialRef(srs)
    lyr = ds.CreateLayerFromGeomFieldDefn("test", geom_fld)
    geom_fld = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    prec = geom_fld.GetCoordinatePrecision()
    assert prec.GetXYResolution() == pytest.approx(8.983152841195214e-06)
    assert prec.GetZResolution() == 1e-3
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(450000 5000000 9.87654321)"))
    lyr.CreateFeature(f)
    ds.Close()

    with gdal.VSIFile(filename, "rb") as f:
        assert b'"coordinates":[2.363925,45.151706,9.877]' in f.read()


###############################################################################
# Test force opening a GeoJSONSeq file


def test_ogr_geojsonseq_force_opening(tmp_vsimem):

    filename = str(tmp_vsimem / "test.json")

    with gdaltest.vsi_open(filename, "wb") as f:
        f.write(
            b"{"
            + b" " * (1000 * 1000)
            + b' "type": "Feature", "properties":{},"geometry":null}\n'
        )

    with pytest.raises(Exception):
        gdal.OpenEx(filename)

    ds = gdal.OpenEx(filename, allowed_drivers=["GeoJSONSeq"])
    assert ds.GetDriver().GetDescription() == "GeoJSONSeq"

    drv = gdal.IdentifyDriverEx("http://example.com", allowed_drivers=["GeoJSONSeq"])
    assert drv.GetDescription() == "GeoJSONSeq"


###############################################################################
# Test WRITE_BBOX option


def test_ogr_geojsonseq_WRITE_BBOX(tmp_vsimem):

    filename = str(tmp_vsimem / "test.geojsonl")
    ds = gdal.GetDriverByName("GeoJSONSeq").Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds.CreateLayer("test", options=["WRITE_BBOX=YES"])
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(2 49,3 50)"))
    lyr.CreateFeature(f)
    ds.Close()

    with gdal.VSIFile(filename, "rb") as f:
        assert b'"bbox":[2.0,49.0,3.0,50.0]' in f.read()
