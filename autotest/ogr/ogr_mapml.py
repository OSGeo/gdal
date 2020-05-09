#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR MapML driver functionality.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even dot rouault at spatialys dot com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

from osgeo import gdal, ogr, osr
import gdaltest
import pytest


def test_ogr_mapml_basic():

    filename = '/vsimem/out.mapml'

    # Write a MapML file
    ds = ogr.GetDriverByName('MapML').CreateDataSource(filename)
    assert ds.TestCapability(ogr.ODsCCreateLayer)
    assert not ds.TestCapability('foo')
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('int64field', ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn('realfield', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('stringfield', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('datetimefield', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('datefield', ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn('timefield', ogr.OFTTime))

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f['intfield'] = 1
    f['int64field'] = 1
    f['realfield'] = 1
    f['stringfield'] = 1
    f['datetimefield'] = '2020/03/31 12:34:56'
    f['datefield'] = '2020/03/31'
    f['timefield'] = '12:34:56'
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    f.SetFID(10)
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f['int64field'] = 1234567890123
    f['realfield'] = 1.25
    f['stringfield'] = 'x'
    f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING (1 2,3 4)'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f['int64field'] = 1
    f['realfield'] = 1
    f['stringfield'] = 1
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 0,0 0),(0.1 0.1,0.1 0.7,0.7 0.1,0.1 0.1))'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOINT (0 1,2 3)'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING ((1 2,3 4),(5 6,7 8))'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON (((0 0,0 1,1 0,0 0)),((10 0,10 1,11 0,10 0)))'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (POINT (1 2),GEOMETRYCOLLECTION(POINT(3 4)))'))
    lyr.CreateFeature(f)

    lyr.ResetReading()
    assert lyr.GetNextFeature() is None
    assert lyr.TestCapability('foo') == 0
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
    srs = lyr.GetSpatialRef()
    assert srs
    assert srs.GetAuthorityCode(None) == '4326'
    assert lyr.GetGeomType() == ogr.wkbUnknown
    assert lyr.GetName() == 'test'
    assert lyr.TestCapability(ogr.OLCStringsAsUTF8)
    assert not lyr.TestCapability(ogr.OLCRandomRead)
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('datetimefield')).GetType() == ogr.OFTDateTime
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('datefield')).GetType() == ogr.OFTDate
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('timefield')).GetType() == ogr.OFTTime

    f = lyr.GetNextFeature()
    assert f.GetFID() == 1

    f = lyr.GetNextFeature()
    assert f.GetFID() == 10
    assert f['intfield'] == 1
    assert f['datetimefield'] == '2020/03/31 12:34:56'
    assert f['datefield'] == '2020/03/31'
    assert f['timefield'] == '12:34:56'
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (1 2)'

    f = lyr.GetNextFeature()
    assert f['int64field'] == 1234567890123
    assert f['realfield'] == 1.25
    assert f['stringfield'] == 'x'
    assert f.GetGeometryRef().ExportToWkt() == 'LINESTRING (1 2,3 4)'

    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'POLYGON ((0 0,1 0,0 1,0 0),(0.1 0.1,0.1 0.7,0.7 0.1,0.1 0.1))'

    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'MULTIPOINT (0 1,2 3)'

    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'MULTILINESTRING ((1 2,3 4),(5 6,7 8))'

    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'MULTIPOLYGON (((0 0,1 0,0 1,0 0)),((10 0,11 0,10 1,10 0)))'

    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'GEOMETRYCOLLECTION (POINT (1 2),POINT (3 4))'

    assert lyr.GetNextFeature() is None

    ds = None

    gdal.Unlink(filename)


def test_ogr_mapml_multiple_layers():

    filename = '/vsimem/out.mapml'

    # Write a MapML file
    ds = ogr.GetDriverByName('MapML').CreateDataSource(filename)
    lyr1 = ds.CreateLayer('lyr1')
    lyr2 = ds.CreateLayer('lyr2')

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
        "HEAD=<title>My title</title>",
        "EXTENT_UNITS=OSMTILE",
        "EXTENT_ACTION=action",
        "EXTENT_XMIN=-123456789",
        "EXTENT_YMIN=-234567890",
        "EXTENT_XMAX=123456789",
        "EXTENT_YMAX=234567890",
        "EXTENT_XMIN_MIN=0",
        "EXTENT_XMIN_MAX=1",
        "EXTENT_YMIN_MIN=2",
        "EXTENT_YMIN_MAX=3",
        "EXTENT_XMAX_MIN=4",
        "EXTENT_XMAX_MAX=5",
        "EXTENT_YMAX_MIN=6",
        "EXTENT_YMAX_MAX=7",
        "EXTENT_ZOOM=18",
        "EXTENT_ZOOM_MIN=15",
        "EXTENT_ZOOM_MAX=20",
        "EXTENT_EXTRA=<foo/>",
    ]
    filename = '/vsimem/out.mapml'
    ds = ogr.GetDriverByName('MapML').CreateDataSource(filename, options=options)
    lyr = ds.CreateLayer('lyr')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (-180 0)'))
    lyr.CreateFeature(f)
    ds = None

    f = gdal.VSIFOpenL(filename, "rb")
    xml = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert xml == """<mapml>
  <head>
    <title>My title</title>
  </head>
  <body>
    <extent action="action" units="OSMTILE">
      <input name="xmin" type="location" units="pcrs" axis="x" position="top-left" value="-123456789" min="0" max="1" />
      <input name="ymin" type="location" units="pcrs" axis="y" position="bottom-right" value="-234567890" min="2" max="3" />
      <input name="xmax" type="location" units="pcrs" axis="x" position="bottom-right" value="123456789" min="4" max="5" />
      <input name="ymax" type="location" units="pcrs" axis="y" position="top-left" value="234567890" min="6" max="7" />
      <input name="projection" type="hidden" value="OSMTILE" />
      <input name="zoom" type="zoom" value="18" min="15" max="20" />
      <foo />
    </extent>
    <feature id="lyr.1" class="lyr">
      <geometry>
        <point>
          <coordinates>-20037508.34 0.00</coordinates>
        </point>
      </geometry>
    </feature>
  </body>
</mapml>
"""

    gdal.Unlink(filename)


def test_ogr_mapml_body_links_single():

    options = [
        'BODY_LINKS=<link type="foo" href="bar"/>'
    ]
    filename = '/vsimem/out.mapml'
    ds = ogr.GetDriverByName('MapML').CreateDataSource(filename, options=options)
    lyr = ds.CreateLayer('lyr')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (-180 0)'))
    lyr.CreateFeature(f)
    ds = None

    f = gdal.VSIFOpenL(filename, "rb")
    xml = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert """</extent>
    <link type="foo" href="bar" />
    <feature id="lyr.1" class="lyr">""" in xml

    gdal.Unlink(filename)


def test_ogr_mapml_body_links_multiple():

    options = [
        'BODY_LINKS=<link type="foo" href="bar"/><link type="baz" href="baw"/>'
    ]
    filename = '/vsimem/out.mapml'
    ds = ogr.GetDriverByName('MapML').CreateDataSource(filename, options=options)
    lyr = ds.CreateLayer('lyr')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (-180 0)'))
    lyr.CreateFeature(f)
    ds = None

    f = gdal.VSIFOpenL(filename, "rb")
    xml = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert """</extent>
    <link type="foo" href="bar" />
    <link type="baz" href="baw" />
    <feature id="lyr.1" class="lyr">""" in xml

    gdal.Unlink(filename)


def test_ogr_mapml_no_class():

    filename = '/vsimem/out.mapml'
    gdal.FileFromMemBuffer(filename, "<mapml><body><feature><geometry><unsupported/></geometry></feature><feature/></body></mapml>")

    ds = ogr.Open(filename)
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2
    assert lyr.GetSpatialRef() is None
    ds = None

    gdal.Unlink(filename)


def test_ogr_mapml_errors():

    with gdaltest.error_handler():
        assert ogr.GetDriverByName('MapML').CreateDataSource("/i_do/not/exists.mapml") is None

    filename = '/vsimem/out.mapml'
    with gdaltest.error_handler():
        assert ogr.GetDriverByName('MapML').CreateDataSource(filename, options=['EXTENT_UNITS=unsupported']) is None

    # Invalid XML
    gdal.FileFromMemBuffer(filename, "<mapml>")
    with gdaltest.error_handler():
        assert ogr.Open(filename) is None

    # Missing <body>
    gdal.FileFromMemBuffer(filename, "<mapml></mapml>")
    with gdaltest.error_handler():
        assert ogr.Open(filename) is None

    # No <feature>
    gdal.FileFromMemBuffer(filename, "<mapml><body></body></mapml>")
    with gdaltest.error_handler():
        assert ogr.Open(filename) is None

    gdal.Unlink(filename)


def test_ogr_mapml_reprojection_to_wgs84():

    filename = '/vsimem/out.mapml'
    ds = ogr.GetDriverByName('MapML').CreateDataSource(filename)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    lyr = ds.CreateLayer('lyr', srs = sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (500000 0)'))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint
    srs = lyr.GetSpatialRef()
    assert srs
    assert srs.GetAuthorityCode(None) == '4326'
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (3 0)'
    ds = None

    gdal.Unlink(filename)


def test_ogr_mapml_layer_srs_is_known():

    filename = '/vsimem/out.mapml'
    ds = ogr.GetDriverByName('MapML').CreateDataSource(filename)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3857)
    lyr = ds.CreateLayer('lyr', srs = sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs
    assert srs.GetAuthorityCode(None) == '3857'
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (1 2)'
    ds = None

    gdal.Unlink(filename)


wkts = [ 'POINT (1 2)',
         'LINESTRING (1 2,3 4)',
         'POLYGON ((0 0,1 0,1 1,0 0))',
         'MULTIPOINT ((1 2))',
         'MULTILINESTRING ((1 2,3 4))',
         'MULTIPOLYGON (((0 0,1 0,1 1,0 0)))',
         'GEOMETRYCOLLECTION (POINT (1 2))',
]
@pytest.mark.parametrize(
    'wkt',
    wkts,
    ids=[ x[0:x.find(' ')].lower() for x in wkts ]
)
def test_ogr_mapml_geomtypes(wkt):

    filename = '/vsimem/out.mapml'
    ds = ogr.GetDriverByName('MapML').CreateDataSource(filename)
    lyr = ds.CreateLayer('lyr')
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

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/mapml/poly.mapml')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1
