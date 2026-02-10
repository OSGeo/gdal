#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR MapML driver functionality.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("MapML")


def test_ogr_mapml_basic():

    filename = "/vsimem/out.mapml"

    # Write a MapML file
    ds = ogr.GetDriverByName("MapML").CreateDataSource(filename)
    assert ds.TestCapability(ogr.ODsCCreateLayer)
    assert not ds.TestCapability("foo")
    lyr = ds.CreateLayer("test")
    assert lyr.GetDataset().GetDescription() == ds.GetDescription()
    lyr.CreateField(ogr.FieldDefn("intfield", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("int64field", ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn("realfield", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("stringfield", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("datetimefield", ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn("datefield", ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn("timefield", ogr.OFTTime))

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["intfield"] = 1
    f["int64field"] = 1
    f["realfield"] = 1
    f["stringfield"] = 1
    f["datetimefield"] = "2020/03/31 12:34:56"
    f["datefield"] = "2020/03/31"
    f["timefield"] = "12:34:56"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    f.SetFID(10)
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["int64field"] = 1234567890123
    f["realfield"] = 1.25
    f["stringfield"] = "x"
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING (1 2,3 4)"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["int64field"] = 1
    f["realfield"] = 1
    f["stringfield"] = 1
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON ((0 0,0 1,1 0,0 0),(0.1 0.1,0.1 0.7,0.7 0.1,0.1 0.1))"
        )
    )
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTIPOINT (0 1,2 3)"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTILINESTRING ((1 2,3 4),(5 6,7 8))"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "MULTIPOLYGON (((0 0,0 1,1 0,0 0)),((10 0,10 1,11 0,10 0)))"
        )
    )
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "GEOMETRYCOLLECTION (POINT (1 2),GEOMETRYCOLLECTION(POINT(3 4)))"
        )
    )
    lyr.CreateFeature(f)

    lyr.ResetReading()
    assert lyr.GetNextFeature() is None
    assert lyr.TestCapability("foo") == 0
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(-1) is None
    assert ds.GetLayer(0) is not None
    assert ds.GetLayer(1) is None

    ds = None

    # Read back the file
    ds = ogr.Open(filename)
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(-1) is None
    assert ds.GetLayer(1) is None
    lyr = ds.GetLayer(0)
    assert lyr.GetDataset().GetDescription() == ds.GetDescription()
    srs = lyr.GetSpatialRef()
    assert srs
    assert srs.GetAuthorityCode(None) == "4326"
    assert lyr.GetGeomType() == ogr.wkbUnknown
    assert lyr.GetName() == "test"
    assert lyr.TestCapability(ogr.OLCStringsAsUTF8)
    assert not lyr.TestCapability(ogr.OLCRandomRead)
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("datetimefield"))
        .GetType()
        == ogr.OFTDateTime
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("datefield"))
        .GetType()
        == ogr.OFTDate
    )
    assert (
        lyr.GetLayerDefn()
        .GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex("timefield"))
        .GetType()
        == ogr.OFTTime
    )

    f = lyr.GetNextFeature()
    assert f.GetFID() == 1

    f = lyr.GetNextFeature()
    assert f.GetFID() == 10
    assert f["intfield"] == 1
    assert f["datetimefield"] == "2020/03/31 12:34:56"
    assert f["datefield"] == "2020/03/31"
    assert f["timefield"] == "12:34:56"
    assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"

    f = lyr.GetNextFeature()
    assert f["int64field"] == 1234567890123
    assert f["realfield"] == 1.25
    assert f["stringfield"] == "x"
    assert f.GetGeometryRef().ExportToWkt() == "LINESTRING (1 2,3 4)"

    f = lyr.GetNextFeature()
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "POLYGON ((0 0,1 0,0 1,0 0),(0.1 0.1,0.1 0.7,0.7 0.1,0.1 0.1))"
    )

    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "MULTIPOINT (0 1,2 3)"

    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "MULTILINESTRING ((1 2,3 4),(5 6,7 8))"

    f = lyr.GetNextFeature()
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "MULTIPOLYGON (((0 0,1 0,0 1,0 0)),((10 0,11 0,10 1,10 0)))"
    )

    f = lyr.GetNextFeature()
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "GEOMETRYCOLLECTION (POINT (1 2),POINT (3 4))"
    )

    assert lyr.GetNextFeature() is None

    ds = None

    gdal.Unlink(filename)


def test_ogr_mapml_multiple_layers():

    filename = "/vsimem/out.mapml"

    # Write a MapML file
    ds = ogr.GetDriverByName("MapML").CreateDataSource(filename)
    lyr1 = ds.CreateLayer("lyr1")
    lyr2 = ds.CreateLayer("lyr2")

    f = ogr.Feature(lyr1.GetLayerDefn())
    lyr1.CreateFeature(f)

    f = ogr.Feature(lyr2.GetLayerDefn())
    lyr2.CreateFeature(f)

    f = ogr.Feature(lyr1.GetLayerDefn())
    lyr1.CreateFeature(f)
    ds = None

    # Read back the file
    ds = ogr.Open(filename)
    assert ds.GetLayerCount() == 2
    assert ds.GetLayer(0).GetFeatureCount() == 2
    assert ds.GetLayer(1).GetFeatureCount() == 1

    ds = None

    gdal.Unlink(filename)


def test_ogr_mapml_creation_options():

    # Write a MapML file
    options = [
        "HEAD=<map-title>My title</map-title>",
        "EXTENT_UNITS=OSMTILE",
        "EXTENT_XMIN=-123456789",
        "EXTENT_YMIN=-234567890",
        "EXTENT_XMAX=123456789",
        "EXTENT_YMAX=234567890",
        "EXTENT_ZOOM=18",
        "EXTENT_ZOOM_MIN=15",
        "EXTENT_ZOOM_MAX=20",
    ]
    filename = "/vsimem/out.mapml"
    ds = ogr.GetDriverByName("MapML").CreateDataSource(filename, options=options)
    lyr = ds.CreateLayer("lyr")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (-180 0)"))
    lyr.CreateFeature(f)
    ds = None

    f = gdal.VSIFOpenL(filename, "rb")
    xml = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)

    assert xml == """<mapml- xmlns="http://www.w3.org/1999/xhtml">
  <map-head>
    <map-title>My title</map-title>
    <map-meta name="projection" content="OSMTILE"></map-meta>
    <map-meta name="cs" content="pcrs"></map-meta>
    <map-meta name="extent" content="top-left-easting=-20037508.34, top-left-northing=0.00, bottom-right-easting=-20037508.34, bottom-right-northing=0.00"></map-meta>
    <map-meta name="zoom" content="min=15,max=20,value=18"></map-meta>
  </map-head>
  <map-body>
    <map-feature id="lyr.1" class="lyr">
      <map-geometry>
        <map-point>
          <map-coordinates>-20037508.34 0.00</map-coordinates>
        </map-point>
      </map-geometry>
    </map-feature>
  </map-body>
</mapml->
"""

    gdal.Unlink(filename)


def test_ogr_mapml_head_links_single():

    options = ['HEAD_LINKS=<map-link type="foo" href="bar"></map-link>']
    filename = "/vsimem/out.mapml"
    ds = ogr.GetDriverByName("MapML").CreateDataSource(filename, options=options)
    lyr = ds.CreateLayer("lyr")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (-180 0)"))
    lyr.CreateFeature(f)
    ds = None

    f = gdal.VSIFOpenL(filename, "rb")
    xml = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)

    assert """<map-link type="foo" href="bar"></map-link>
  </map-head>""" in xml

    gdal.Unlink(filename)


def test_ogr_mapml_head_links_multiple():

    options = [
        'HEAD_LINKS=<map-link type="foo" href="bar"></map-link><map-link type="baz" href="baw"></map-link>'
    ]
    filename = "/vsimem/out.mapml"
    ds = ogr.GetDriverByName("MapML").CreateDataSource(filename, options=options)
    lyr = ds.CreateLayer("lyr")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (-180 0)"))
    lyr.CreateFeature(f)
    ds = None

    f = gdal.VSIFOpenL(filename, "rb")
    xml = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)

    assert """<map-link type="foo" href="bar"></map-link>
    <map-link type="baz" href="baw"></map-link>
  </map-head>""" in xml

    gdal.Unlink(filename)


def test_ogr_mapml_no_class():

    filename = "/vsimem/out.mapml"
    gdal.FileFromMemBuffer(
        filename,
        '<mapml- xmlns="http://www.w3.org/1999/xhtml"><map-body><map-feature><map-geometry><unsupported/></map-geometry></map-feature><map-feature/></map-body></mapml->',
    )

    ds = ogr.Open(filename)
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2
    assert lyr.GetSpatialRef() is None
    ds = None

    gdal.Unlink(filename)


def test_ogr_mapml_errors():

    with pytest.raises(Exception):
        ogr.GetDriverByName("MapML").CreateDataSource("/i_do/not/exists.mapml")

    filename = "/vsimem/out.mapml"
    with pytest.raises(Exception):
        ogr.GetDriverByName("MapML").CreateDataSource(
            filename, options=["EXTENT_UNITS=unsupported"]
        )

    # Invalid XML
    gdal.FileFromMemBuffer(filename, '<mapml- xmlns="http://www.w3.org/1999/xhtml">')
    with pytest.raises(Exception):
        assert ogr.Open(filename) is None

    # Missing <map-body>
    gdal.FileFromMemBuffer(
        filename, '<mapml- xmlns="http://www.w3.org/1999/xhtml"></mapml->'
    )
    with pytest.raises(Exception):
        ogr.Open(filename)

    # No <map-feature>
    gdal.FileFromMemBuffer(
        filename,
        '<mapml- xmlns="http://www.w3.org/1999/xhtml"><map-body></map-body></mapml->',
    )
    with pytest.raises(Exception):
        ogr.Open(filename)

    gdal.Unlink(filename)


def test_ogr_mapml_reprojection_to_wgs84():

    filename = "/vsimem/out.mapml"
    ds = ogr.GetDriverByName("MapML").CreateDataSource(filename)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("lyr", srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (500000 0)"))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint
    srs = lyr.GetSpatialRef()
    assert srs
    assert srs.GetAuthorityCode(None) == "4326"
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "POINT (3 0)"
    ds = None

    gdal.Unlink(filename)


def test_ogr_mapml_layer_srs_is_known():

    filename = "/vsimem/out.mapml"
    ds = ogr.GetDriverByName("MapML").CreateDataSource(filename)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3857)
    lyr = ds.CreateLayer("lyr", srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs
    assert srs.GetAuthorityCode(None) == "3857"
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"
    ds = None

    gdal.Unlink(filename)


wkts = [
    "POINT (1 2)",
    "LINESTRING (1 2,3 4)",
    "POLYGON ((0 0,1 0,1 1,0 0))",
    "MULTIPOINT ((1 2))",
    "MULTILINESTRING ((1 2,3 4))",
    "MULTIPOLYGON (((0 0,1 0,1 1,0 0)))",
    "GEOMETRYCOLLECTION (POINT (1 2))",
]


@pytest.mark.parametrize("wkt", wkts, ids=[x[0 : x.find(" ")].lower() for x in wkts])
def test_ogr_mapml_geomtypes(wkt):

    filename = "/vsimem/out.mapml"
    ds = ogr.GetDriverByName("MapML").CreateDataSource(filename)
    lyr = ds.CreateLayer("lyr")
    f = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt(wkt)
    f.SetGeometry(geom)
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == geom.GetGeometryType()
    ds = None

    gdal.Unlink(filename)


def test_ogr_mapml_ogrsf():

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro data/mapml/poly.mapml"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1
