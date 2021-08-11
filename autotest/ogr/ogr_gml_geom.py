#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic OGR translation of WKT and WKB geometries.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os

import pytest

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal

###############################################################################


@pytest.mark.parametrize(
    'filename',
    [
        f for f in os.listdir(os.path.join(os.path.dirname(__file__), 'data/wkb_wkt'))
        if f[-4:] == '.wkt'
    ]
)
def test_gml_geom(filename):
    raw_wkt = open('data/wkb_wkt/' + filename).read()

    ######################################################################
    # Convert WKT to GML.

    geom_wkt = ogr.CreateGeometryFromWkt(raw_wkt)

    gml = geom_wkt.ExportToGML()

    assert gml is not None and gml, 'Conversion to GML failed.'

    ######################################################################
    # Create geometry from GML.

    geom_gml = ogr.CreateGeometryFromGML(gml)

    if ogrtest.check_feature_geometry(geom_wkt, geom_gml, 0.0000000000001) == 1:
        clean_wkt = geom_wkt.ExportToWkt()
        gml_wkt = geom_gml.ExportToWkt()
        pytest.fail('WKT from GML (%s) does not match clean WKT (%s).\ngml was (%s)' % (gml_wkt, clean_wkt, gml))


###############################################################################
# Test geometries with extra spaces at the end, as sometimes are generated
# by ESRI WFS software.


def test_gml_space_test():
    gml = '<gml:LineString xmlns:foo="http://bar"><gml:coordinates xmlns:foo="http://bar" decimal="." cs="," ts=" ">189999.99995605,624999.99998375 200000.00005735,624999.99998375 200000.00005735,612499.99997125 195791.3593843,612499.99997125 193327.3749823,612499.99997125 189999.99995605,612499.99997125 189999.99995605,619462.31247125 189999.99995605,624999.99998375 \n</gml:coordinates></gml:LineString>'
    geom = ogr.CreateGeometryFromGML(gml)
    assert geom is not None and geom.GetGeometryType() is ogr.wkbLineString and geom.GetPointCount() == 8, \
        'GML not correctly parsed'

###############################################################################
# Test GML 3.x "pos" element for a point.


def test_gml_pos_point():

    gml = '<gml:Point xmlns:foo="http://bar"><gml:pos>31 29 16</gml:pos></gml:Point>'

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'POINT (31 29 16)', '<gml:pos> not correctly parsed'

###############################################################################
# Test GML 3.1.1 "pos" element for a polygon. (ticket #3244)


def test_gml_pos_polygon():

    gml = '''<gml:Polygon xmlns:foo="http://bar">
                <gml:exterior xmlns:foo="http://bar">
                    <gml:LinearRing xmlns:foo="http://bar">
                        <gml:pos xmlns:foo="http://bar">0 0</gml:pos>
                        <gml:pos>4 0</gml:pos>
                        <gml:pos>4 4</gml:pos>
                        <gml:pos>0 4</gml:pos>
                        <gml:pos>0 0</gml:pos>
                    </gml:LinearRing>
                </gml:exterior>
                <gml:interior xmlns:foo="http://bar">
                    <gml:LinearRing xmlns:foo="http://bar">
                        <gml:pos>1 1</gml:pos>
                        <gml:pos>2 1</gml:pos>
                        <gml:pos>2 2</gml:pos>
                        <gml:pos>1 2</gml:pos>
                        <gml:pos>1 1</gml:pos>
                    </gml:LinearRing>
                </gml:interior>
            </gml:Polygon>'''

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'POLYGON ((0 0,4 0,4 4,0 4,0 0),(1 1,2 1,2 2,1 2,1 1))', \
        '<gml:Polygon> not correctly parsed'

###############################################################################
# Test GML 3.x "posList" element for a linestring.


def test_gml_posList_line():

    gml = '<LineString xmlns:foo="http://bar"><posList xmlns:foo="http://bar">31 42 53 64 55 76</posList></LineString>'

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'LINESTRING (31 42,53 64,55 76)', \
        '<gml:posList> not correctly parsed'


###############################################################################
# Test GML 3.x "posList" element for a 3D linestring.

def test_gml_posList_line3d():

    gml = '<LineString><posList xmlns:foo="http://bar" srsDimension="3">31 42 1 53 64 2 55 76 3</posList></LineString>'

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'LINESTRING (31 42 1,53 64 2,55 76 3)', \
        '<gml:posList> not correctly parsed'

###############################################################################
# Test GML 3.x "posList" element for a 3D linestring, but with srsDimension
# set on LineString, not posList


def test_gml_posList_line3d_2():

    gml = '<LineString srsDimension="3"><posList>31 42 1 53 64 2 55 76 3</posList></LineString>'

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'LINESTRING (31 42 1,53 64 2,55 76 3)', \
        '<gml:posList> not correctly parsed'

###############################################################################
# Test GML 3.x "polygon" element for a point.


def test_gml_polygon():

    gml = '<Polygon><exterior><LinearRing><posList>0 0 4 0 4 4 0 4 0 0</posList></LinearRing></exterior><interior xmlns:foo="http://bar"><LinearRing><posList xmlns:foo="http://bar">1 1 2 1 2 2 1 2 1 1</posList></LinearRing></interior></Polygon>'
    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'POLYGON ((0 0,4 0,4 4,0 4,0 0),(1 1,2 1,2 2,1 2,1 1))', \
        '<gml:Polygon> not correctly parsed'

###############################################################################
# Private utility function to convert WKT to GML with assigned WGS 84 as SRS.


def _CreateGMLWithSRSFromWkt(wkt, epsg):

    geom = ogr.CreateGeometryFromWkt(wkt)

    if geom is None:
        gdaltest.post_reason('Import geometry from WKT failed')
        return None

    # Assign SRS from given EPSG code
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(epsg)

    if srs is None:
        gdaltest.post_reason('SRS import from EPSG failed')
        return None

    geom.AssignSpatialReference(srs)

    return geom.ExportToGML()

###############################################################################
# Test of Point geometry with SRS assigned


def test_gml_out_point_srs():

    wkt = 'POINT(21.675 53.763)'

    gml = _CreateGMLWithSRSFromWkt(wkt, 4326)

    assert gml is not None and gml, 'Conversion to GML failed.'

    assert gml[0:31] == '<gml:Point srsName="EPSG:4326">', \
        'No srsName attribute in GML output'

###############################################################################
# Test of Point 3D geometry with SRS assigned


def test_gml_out_point3d_srs():

    wkt = 'POINT(21.675 53.763 100)'

    gml = _CreateGMLWithSRSFromWkt(wkt, 4326)

    assert gml is not None and gml, 'Conversion to GML failed.'

    assert gml[0:31] == '<gml:Point srsName="EPSG:4326">', \
        'No srsName attribute in GML output'

###############################################################################
# Test of LineString geometry with SRS assigned


def test_gml_out_linestring_srs():

    wkt = open('data/wkb_wkt/5.wkt').read()

    gml = _CreateGMLWithSRSFromWkt(wkt, 4326)

    assert gml is not None and gml, 'Conversion to GML failed.'

    assert gml[0:36] == '<gml:LineString srsName="EPSG:4326">', \
        'No srsName attribute in GML output'

###############################################################################
# Test of Polygon geometry with SRS assigned


def test_gml_out_polygon_srs():

    wkt = open('data/wkb_wkt/6.wkt').read()

    gml = _CreateGMLWithSRSFromWkt(wkt, 4326)

    assert gml is not None and gml, 'Conversion to GML failed.'

    assert gml[0:33] == '<gml:Polygon srsName="EPSG:4326">', \
        'No srsName attribute in GML output'

###############################################################################
# Test of MultiPoint geometry with SRS assigned


def test_gml_out_multipoint_srs():

    wkt = open('data/wkb_wkt/11.wkt').read()

    gml = _CreateGMLWithSRSFromWkt(wkt, 4326)

    assert gml is not None and gml, 'Conversion to GML failed.'

    assert gml[0:36] == '<gml:MultiPoint srsName="EPSG:4326">', \
        'No srsName attribute in GML output'

###############################################################################
# Test of MultiLineString geometry with SRS assigned


def test_gml_out_multilinestring_srs():

    wkt = open('data/wkb_wkt/2.wkt').read()

    gml = _CreateGMLWithSRSFromWkt(wkt, 4326)

    assert gml is not None and gml, 'Conversion to GML failed.'

    assert gml[0:41] == '<gml:MultiLineString srsName="EPSG:4326">', \
        'No srsName attribute in GML output'

###############################################################################
# Test of MultiPolygon geometry with SRS assigned


def test_gml_out_multipolygon_srs():

    wkt = open('data/wkb_wkt/4.wkt').read()

    gml = _CreateGMLWithSRSFromWkt(wkt, 4326)

    assert gml is not None and gml, 'Conversion to GML failed.'

    assert gml[0:38] == '<gml:MultiPolygon srsName="EPSG:4326">', \
        'No srsName attribute in GML output'

    # Verify we have no other srsName's on subelements.
    assert gml[39:].find('srsName') == -1, \
        'Got extra srsName attributes on subelements.'

###############################################################################
# Test of GeometryCollection with SRS assigned


def test_gml_out_geometrycollection_srs():

    wkt = open('data/wkb_wkt/3.wkt').read()

    gml = _CreateGMLWithSRSFromWkt(wkt, 4326)

    assert gml is not None and gml, 'Conversion to GML failed.'

    assert gml[0:39] == '<gml:MultiGeometry srsName="EPSG:4326">', \
        'No srsName attribute in GML output'

###############################################################################
# Test GML Box


def test_gml_Box():

    gml = """<gml:Box xmlns:gml="http://www.opengis.net/gml" srsName="foo">
  <gml:coord>
    <gml:X>1</gml:X>
    <gml:Y>2</gml:Y>
  </gml:coord>
  <gml:coord>
    <gml:X>3</gml:X>
    <gml:Y>4</gml:Y>
  </gml:coord>
</gml:Box>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'POLYGON ((1 2 0,3 2 0,3 4 0,1 4 0,1 2 0))', \
        '<gml:Box> not correctly parsed'

###############################################################################
# Test GML Envelope


def test_gml_Envelope():

    gml = """<gml:Envelope xmlns:gml="http://www.opengis.net/gml" srsName="foo">
    <gml:lowerCorner>1 2</gml:lowerCorner>
    <gml:upperCorner>3 4</gml:upperCorner>
</gml:Envelope>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'POLYGON ((1 2,3 2,3 4,1 4,1 2))', \
        '<gml:Envelope> not correctly parsed'

###############################################################################
# Test GML Curve


def test_gml_Curve():

    gml = """<gml:Curve xmlns:gml="http://www.opengis.net/gml" srsName="foo">
    <gml:segments xmlns:foo="http://bar">
        <gml:LineStringSegment xmlns:foo="http://bar">
            <gml:posList>1 2 3 4</gml:posList>
        </gml:LineStringSegment>
    </gml:segments>
</gml:Curve>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'LINESTRING (1 2,3 4)', \
        '<gml:Curve> not correctly parsed'

###############################################################################
# Test GML Curve with pointProperty elements


def test_gml_Curve_with_pointProperty():

    gml = """<gml:Curve xmlns:gml="http://www.opengis.net/gml" srsName="foo">
    <gml:segments>
        <gml:LineStringSegment>
            <gml:pos>1 2</gml:pos>
            <gml:pointProperty>
                <gml:Point>
                    <gml:pos>3 4</gml:pos>
                </gml:Point>
            </gml:pointProperty>
        </gml:LineStringSegment>
    </gml:segments>
</gml:Curve>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'LINESTRING (1 2,3 4)', \
        '<gml:Curve> not correctly parsed'

###############################################################################
# Test GML MultiCurve


def test_gml_MultiCurve():

    gml = """<gml:MultiCurve xmlns:foo="http://bar" xmlns:gml="http://www.opengis.net/gml" srsName="foo">
    <gml:curveMember xmlns:foo="http://bar">
        <gml:LineString>
            <gml:posList>1 2 2 3</gml:posList>
        </gml:LineString>
    </gml:curveMember>
    <gml:curveMember>
        <gml:LineString>
            <gml:posList>3 4 4 5</gml:posList>
        </gml:LineString>
    </gml:curveMember>
</gml:MultiCurve>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'MULTILINESTRING ((1 2,2 3),(3 4,4 5))', \
        '<gml:MultiCurve> not correctly parsed'

###############################################################################
# Test GML MultiSurface with PolygonPatch


def test_gml_MultiSurface():

    gml = """<gml:MultiSurface xmlns:gml="http://www.opengis.net/gml" srsName="foo">
    <gml:surfaceMember xmlns:foo="http://bar" xlink:role="main">
        <gml:Surface gml:id="id1">
            <gml:patches xmlns:foo="http://bar">
                <gml:PolygonPatch xmlns:foo="http://bar" interpolation="planar">
                    <gml:exterior>
                        <gml:LinearRing>
                            <gml:posList>1 2 3 4 5 6 1 2</gml:posList>
                        </gml:LinearRing>
                    </gml:exterior>
                    <gml:interior>
                        <gml:LinearRing>
                            <gml:posList>2 3 4 5 6 7 2 3</gml:posList>
                        </gml:LinearRing>
                    </gml:interior>
                    <gml:interior>
                        <gml:LinearRing>
                            <gml:posList>3 4 5 6 7 8 3 4</gml:posList>
                        </gml:LinearRing>
                    </gml:interior>
                </gml:PolygonPatch>
            </gml:patches>
        </gml:Surface>
    </gml:surfaceMember>
    <gml:surfaceMember>
        <gml:Surface>
            <gml:patches>
                <gml:PolygonPatch interpolation="planar">
                    <gml:exterior>
                        <gml:Ring>
                            <gml:curveMember>
                                <gml:Curve>
                                    <gml:segments>
                                        <gml:LineStringSegment>
                                            <gml:pos>4 5</gml:pos>
                                            <gml:pos>6 7</gml:pos>
                                        </gml:LineStringSegment>
                                    </gml:segments>
                                </gml:Curve>
                            </gml:curveMember>
                            <gml:curveMember>
                                <gml:Curve>
                                    <gml:segments>
                                        <gml:LineStringSegment>
                                            <gml:pos>6 7</gml:pos>
                                            <gml:pos>8 9</gml:pos>
                                        </gml:LineStringSegment>
                                    </gml:segments>
                                </gml:Curve>
                            </gml:curveMember>
                            <gml:curveMember>
                                <gml:Curve>
                                    <gml:segments>
                                        <gml:LineStringSegment>
                                            <gml:pos>8 9</gml:pos>
                                            <gml:pos>4 5</gml:pos>
                                        </gml:LineStringSegment>
                                    </gml:segments>
                                </gml:Curve>
                            </gml:curveMember>
                        </gml:Ring>
                    </gml:exterior>
                </gml:PolygonPatch>
            </gml:patches>
        </gml:Surface>
    </gml:surfaceMember>
</gml:MultiSurface>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'MULTIPOLYGON (((1 2,3 4,5 6,1 2),(2 3,4 5,6 7,2 3),(3 4,5 6,7 8,3 4)),((4 5,6 7,8 9,4 5)))', \
        '<gml:MultiSurface> not correctly parsed'


###############################################################################
# Test GML MultiSurface with surfaceMembers

def test_gml_MultiSurface_surfaceMembers():

    gml = """<gml:MultiSurface xmlns:foo="http://bar">
          <gml:surfaceMembers xmlns:foo="http://bar">
            <gml:Surface xmlns:foo="http://bar">
              <gml:patches xmlns:foo="http://bar">
                <gml:PolygonPatch xmlns:foo="http://bar">
                  <gml:exterior xmlns:foo="http://bar">
                      <gml:LinearRing xmlns:foo="http://bar">
                          <gml:posList>1 2 3 4 5 6 1 2</gml:posList>
                      </gml:LinearRing>
                    </gml:exterior>
                    <gml:interior>
                      <gml:LinearRing>
                          <gml:posList>2 3 4 5 6 7 2 3</gml:posList>
                      </gml:LinearRing>
                    </gml:interior>
                </gml:PolygonPatch>
              </gml:patches>
            </gml:Surface>
            <gml:Surface>
              <gml:patches>
                <gml:PolygonPatch>
                  <gml:exterior>
                    <gml:LinearRing>
                      <gml:posList>3 4 5 6 7 8 3 4</gml:posList>
                    </gml:LinearRing>
                  </gml:exterior>
                </gml:PolygonPatch>
                <gml:PolygonPatch>
                  <gml:exterior>
                    <gml:LinearRing>
                      <gml:posList>30 40 50 60 70 80 30 40</gml:posList>
                    </gml:LinearRing>
                  </gml:exterior>
                </gml:PolygonPatch>
              </gml:patches>
            </gml:Surface>
          </gml:surfaceMembers>
        </gml:MultiSurface>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'MULTIPOLYGON (((1 2,3 4,5 6,1 2),(2 3,4 5,6 7,2 3)),((3 4,5 6,7 8,3 4)),((30 40,50 60,70 80,30 40)))', \
        '<gml:MultiSurface> not correctly parsed'


###############################################################################
# Test GML MultiCurve with curveMembers

def test_gml_MultiCurve_curveMembers():

    gml = """<gml:MultiCurve xmlns:foo="http://bar">
          <gml:curveMembers xmlns:foo="http://bar">
            <gml:LineString xmlns:foo="http://bar">
                <gml:posList xmlns:foo="http://bar" srsDimension="2">0 0 1 1</gml:posList>
              </gml:LineString>
            </gml:curveMembers>
          </gml:MultiCurve>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'MULTILINESTRING ((0 0,1 1))', \
        '<gml:MultiCurve> not correctly parsed'

###############################################################################
# Test GML MultiGeometry with geometryMembers

def test_gml_MultiGeometry_geometryMembers():

    gml = """<gml:MultiGeometry xmlns:foo="http://bar">
          <gml:geometryMembers xmlns:foo="http://bar">
            <gml:LineString xmlns:foo="http://bar">
                <gml:posList xmlns:foo="http://bar" srsDimension="2">0 0 1 1</gml:posList>
              </gml:LineString>
            </gml:geometryMembers>
          </gml:MultiGeometry>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'GEOMETRYCOLLECTION (LINESTRING (0 0,1 1))', \
        '<gml:MultiGeometry> not correctly parsed'

###############################################################################
# Test GML CompositeCurve with curveMembers


def test_gml_CompositeCurve_curveMembers():

    gml = """<gml:CompositeCurve xmlns:foo="http://bar">
          <gml:curveMembers xmlns:foo="http://bar">
            <gml:LineString xmlns:foo="http://bar">
                <gml:posList xmlns:foo="http://bar" srsDimension="2">0 0 1 1</gml:posList>
            </gml:LineString>
            <gml:LineString xmlns:foo="http://bar">
                <gml:posList xmlns:foo="http://bar" srsDimension="2">1 1 2 2</gml:posList>
            </gml:LineString>
          </gml:curveMembers>
        </gml:CompositeCurve>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'LINESTRING (0 0,1 1,2 2)', \
        '<gml:CompositeCurve> not correctly parsed'

###############################################################################
# Test GML MultiPoint with pointMembers


def test_gml_MultiCurve_pointMembers():

    gml = """<gml:MultiPoint xmlns:foo="http://bar">
          <gml:pointMembers xmlns:foo="http://bar">
              <gml:Point xmlns:foo="http://bar">
                <gml:pos xmlns:foo="http://bar">0 0</gml:pos>
              </gml:Point>
              <gml:Point>
                <gml:pos>1 1</gml:pos>
              </gml:Point>
            </gml:pointMembers>
          </gml:MultiPoint>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'MULTIPOINT (0 0,1 1)', \
        '<gml:MultiPoint> not correctly parsed'
###############################################################################
# Test GML Solid


def test_gml_Solid():

    gml = """<gml:Solid xmlns:foo="http://bar" gml:id="UUID_cc5a9513-2d85-4f1a-869a-620400182e1f">
          <gml:exterior xmlns:foo="http://bar">
            <gml:CompositeSurface xmlns:foo="http://bar" gml:id="UUID_2c83341e-a9ce-4abe-9c40-b5208eed5588">
              <gml:surfaceMember xmlns:foo="http://bar">
                <gml:Polygon xmlns:foo="http://bar" gml:id="UUID_d8e4b04b-ce0a-441e-b940-5ab99fcf6112">
                  <gml:exterior xmlns:foo="http://bar">
                    <gml:LinearRing xmlns:foo="http://bar" gml:id="UUID_d8e4b04b-ce0a-441e-b940-5ab99fcf6112_0">
                      <gml:posList xmlns:foo="http://bar">1 2 0 3 4 0 5 6 0 1 2 0</gml:posList>
                    </gml:LinearRing>
                  </gml:exterior>
                </gml:Polygon>
              </gml:surfaceMember>
            </gml:CompositeSurface>
          </gml:exterior>
        </gml:Solid>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'POLYHEDRALSURFACE Z (((1 2 0,3 4 0,5 6 0,1 2 0)))', \
        '<gml:Solid> not correctly parsed'

###############################################################################
# Test GML OrientableSurface


def test_gml_OrientableSurface():

    gml = """<gml:OrientableSurface xmlns:foo="http://bar" orientation="+">
                                            <gml:baseSurface xmlns:foo="http://bar">
                                                <gml:Polygon>
                                                  <gml:exterior>
                                                  <gml:LinearRing>
                                                  <gml:posList srsDimension="3">-213.475 24.989 0
                                                  -213.475 24.989 8.0 -215.704 25.077 8.0 -215.704
                                                  25.077 0 -213.475 24.989 0 </gml:posList>
                                                  </gml:LinearRing>
                                                  </gml:exterior>
                                                </gml:Polygon>
                                            </gml:baseSurface>
                                        </gml:OrientableSurface>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'POLYGON ((-213.475 24.989 0,-213.475 24.989 8,-215.704 25.077 8,-215.704 25.077 0,-213.475 24.989 0))', \
        '<gml:OrientableSurface> not correctly parsed'

###############################################################################
# Test GML Triangle


def test_gml_Triangle():

    gml = """<gml:Triangle xmlns:foo="http://bar">
                <gml:exterior>
                    <gml:LinearRing>
                        <gml:posList>0 0 0 1 1 1 0 0</gml:posList>
                     </gml:LinearRing>
                </gml:exterior>
             </gml:Triangle>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'TRIANGLE ((0 0,0 1,1 1,0 0))', \
        '<gml:Triangle> not correctly parsed'

    # check the conversion of Triangle from OGR -> GML
    wkt_original = 'TRIANGLE ((0 0,0 1,0 1,0 0))'
    triangle = ogr.CreateGeometryFromWkt(wkt_original)
    opts = ["FORMAT=GML3"]
    gml_string = triangle.ExportToGML(opts)

    if gml_string != '<gml:Triangle><gml:exterior><gml:LinearRing><gml:posList>0 0 0 1 0 1 0 0</gml:posList></gml:LinearRing></gml:exterior></gml:Triangle>':
        print(geom.ExportToWkt())
        pytest.fail('incorrect conversion from OGR -> GML for OGRTriangle')


###############################################################################
# Test GML Rectangle


def test_gml_Rectangle():

    gml = """<gml:Rectangle xmlns:foo="http://bar">
                <gml:exterior>
                    <gml:LinearRing>
                        <gml:posList>0 0 0 1 1 1 1 0 0 0</gml:posList>
                     </gml:LinearRing>
                </gml:exterior>
             </gml:Rectangle>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'POLYGON ((0 0,0 1,1 1,1 0,0 0))', \
        '<gml:Rectangle> not correctly parsed'

###############################################################################
# Test GML PolyhedralSurface


def test_gml_PolyhedralSurface():

    # Conversion from GML -> OGR

    # 2 patches and 2 rings
    gml = """<gml:PolyhedralSurface>
                <gml:polygonPatches>
                    <gml:PolygonPatch>
                        <gml:exterior>
                            <gml:LinearRing>
                                <gml:posList srsDimension="3">1 2 3 4 5 6 7 8 9 1 2 3</gml:posList>
                            </gml:LinearRing>
                        </gml:exterior>
                    </gml:PolygonPatch>
                    <gml:PolygonPatch>
                        <gml:exterior>
                            <gml:LinearRing>
                                <gml:posList srsDimension="3">10 11 12 13 14 15 16 17 18 10 11 12</gml:posList>
                            </gml:LinearRing>
                        </gml:exterior>
                        <gml:interior>
                            <gml:LinearRing>
                                <gml:posList srsDimension="3">19 20 21 22 23 24 25 26 27 19 20 21</gml:posList>
                            </gml:LinearRing>
                        </gml:interior>
                    </gml:PolygonPatch>
                </gml:polygonPatches>
            </gml:PolyhedralSurface>"""
    geom = ogr.CreateGeometryFromGML(gml)

    # NOTE - this is actually an invalid PolyhedralSurface
    # need to assert geom.IsValid() == True to determine the validity of the geometry
    assert geom.ExportToWkt() == 'POLYHEDRALSURFACE Z (((1 2 3,4 5 6,7 8 9,1 2 3)),((10 11 12,13 14 15,16 17 18,10 11 12),(19 20 21,22 23 24,25 26 27,19 20 21)))', \
        '<gml:PolyhedralSurface> not correctly parsed'

    # 1 patch and 2 rings
    gml = """<gml:PolyhedralSurface>
                <gml:polygonPatches>
                    <gml:PolygonPatch>
                        <gml:exterior>
                            <gml:LinearRing>
                                <gml:posList srsDimension="3">1 2 3 4 5 6 7 8 9 1 2 3</gml:posList>
                            </gml:LinearRing></gml:exterior>
                        </gml:PolygonPatch>
                    <gml:PolygonPatch>
                        <gml:exterior>
                            <gml:LinearRing>
                                <gml:posList srsDimension="3">10 11 12 13 14 15 16 17 18 10 11 12</gml:posList>
                            </gml:LinearRing>
                        </gml:exterior>
                    </gml:PolygonPatch>
            </gml:polygonPatches>
            </gml:PolyhedralSurface>"""

    geom = ogr.CreateGeometryFromGML(gml)

    # NOTE - this is actually an invalid PolyhedralSurface
    # need to assert geom.IsValid() == True to determine the validity of the geometry
    assert geom.ExportToWkt() == 'POLYHEDRALSURFACE Z (((1 2 3,4 5 6,7 8 9,1 2 3)),((10 11 12,13 14 15,16 17 18,10 11 12)))', \
        '<gml:PolyhedralSurface> not correctly parsed'

    # Variations of empty PolyhedralSurface
    gml_strings = ['<gml:PolyhedralSurface></gml:PolyhedralSurface>',
                   '<gml:PolyhedralSurface><gml:polygonPatches></gml:polygonPatches></gml:PolyhedralSurface>',
                   """<gml:PolyhedralSurface>
                           <gml:polygonPatches>
                               <gml:PolygonPatch>
                                   <gml:exterior>
                                   </gml:exterior>
                               </gml:PolygonPatch>
                           </gml:polygonPatches>
                       </gml:PolyhedralSurface>"""]

    for string in gml_strings:
        geom = ogr.CreateGeometryFromGML(string)
        assert geom.ExportToWkt() == 'POLYHEDRALSURFACE EMPTY', \
            'Empty <gml:PolyhedralSurface> not correctly parsed'

    # Conversion from OGR -> GML
    wkt_original = 'POLYHEDRALSURFACE Z (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)),\
((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0)),\
((0 0 0,1 0 0,1 0 1,0 0 1,0 0 0)),\
((1 1 0,1 1 1,1 0 1,1 0 0,1 1 0)),\
((0 1 0,0 1 1,1 1 1,1 1 0,0 1 0)),\
((0 0 1,1 0 1,1 1 1,0 1 1,0 0 1)))'
    ps = ogr.CreateGeometryFromWkt(wkt_original)
    opts = ["FORMAT=GML3"]
    string = ps.ExportToGML(opts)
    if string != '<gml:PolyhedralSurface><gml:polygonPatches><gml:PolygonPatch><gml:exterior><gml:LinearRing><gml:posList srsDimension="3">0 0 0 0 0 1 0 1 1 0 1 0 0 0 0</gml:posList></gml:LinearRing></gml:exterior></gml:PolygonPatch><gml:PolygonPatch><gml:exterior><gml:LinearRing><gml:posList srsDimension="3">0 0 0 0 1 0 1 1 0 1 0 0 0 0 0</gml:posList></gml:LinearRing></gml:exterior></gml:PolygonPatch><gml:PolygonPatch><gml:exterior><gml:LinearRing><gml:posList srsDimension="3">0 0 0 1 0 0 1 0 1 0 0 1 0 0 0</gml:posList></gml:LinearRing></gml:exterior></gml:PolygonPatch><gml:PolygonPatch><gml:exterior><gml:LinearRing><gml:posList srsDimension="3">1 1 0 1 1 1 1 0 1 1 0 0 1 1 0</gml:posList></gml:LinearRing></gml:exterior></gml:PolygonPatch><gml:PolygonPatch><gml:exterior><gml:LinearRing><gml:posList srsDimension="3">0 1 0 0 1 1 1 1 1 1 1 0 0 1 0</gml:posList></gml:LinearRing></gml:exterior></gml:PolygonPatch><gml:PolygonPatch><gml:exterior><gml:LinearRing><gml:posList srsDimension="3">0 0 1 1 0 1 1 1 1 0 1 1 0 0 1</gml:posList></gml:LinearRing></gml:exterior></gml:PolygonPatch></gml:polygonPatches></gml:PolyhedralSurface>':
        print(geom.ExportToWkt())
        pytest.fail('incorrect parsing of OGR -> GML for PolyhedralSurface')

    g2 = ogr.CreateGeometryFromGML(string)
    if g2.Equals(ps) != 1:
        print(geom.ExportToWkt())
        pytest.fail('incorrect round-tripping')

    # empty geometry
    wkt_original = 'POLYHEDRALSURFACE EMPTY'
    ps = ogr.CreateGeometryFromWkt(wkt_original)
    opts = ["FORMAT=GML3"]
    string = ps.ExportToGML(opts)
    if string != '<gml:PolyhedralSurface><gml:polygonPatches></gml:polygonPatches></gml:PolyhedralSurface>':
        print(geom.ExportToWkt())
        pytest.fail('incorrect parsing of OGR -> GML for empty PolyhedralSurface')

    # several polygon patches (and test that non elements such as comments are parsed OK)
    gml = """<gml:PolyhedralSurface>
                <gml:polygonPatches>
                    <gml:PolygonPatch>
                        <gml:exterior>
                            <gml:LinearRing>
                                <gml:posList srsDimension="3">1 2 3 4 5 6 7 8 9 1 2 3</gml:posList>
                            </gml:LinearRing>
                        </gml:exterior>
                    </gml:PolygonPatch>
                    <!--- --->
                </gml:polygonPatches>
                <!--- --->
                <gml:polygonPatches>
                    <gml:PolygonPatch>
                        <gml:exterior>
                            <gml:LinearRing>
                                <gml:posList srsDimension="3">1 2 3 4 5 6 7 8 9 1 2 3</gml:posList>
                            </gml:LinearRing>
                        </gml:exterior>
                    </gml:PolygonPatch>
                </gml:polygonPatches>
            </gml:PolyhedralSurface>"""

    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'GEOMETRYCOLLECTION (POLYHEDRALSURFACE (((1 2 3,4 5 6,7 8 9,1 2 3))),POLYHEDRALSURFACE (((1 2 3,4 5 6,7 8 9,1 2 3))))', \
        '<gml:PolyhedralSurface> not correctly parsed'

    # Test PolyhedralSurface with curve section (which we linearize since the SF PolyhedralSurface doesn't support curves)
    gml = """<gml:PolyhedralSurface>
                <gml:polygonPatches>
                    <gml:PolygonPatch interpolation="planar">
                        <gml:exterior>
                            <gml:Ring>
                                <gml:curveMember>
                                    <gml:Curve>
                                        <gml:segments>
                                            <gml:LineStringSegment interpolation="linear">
                                                <gml:pos>0 -1</gml:pos>
                                                <gml:pos>0 1</gml:pos>
                                            </gml:LineStringSegment>
                                            <gml:Arc interpolation="circularArc3Points" numArc="1">
                                                <gml:pos>0 1</gml:pos>
                                                <gml:pos>1 0</gml:pos>
                                                <gml:pos>0 -1</gml:pos>
                                            </gml:Arc>
                                        </gml:segments>
                                    </gml:Curve>
                                </gml:curveMember>
                            </gml:Ring>
                        </gml:exterior>
                    </gml:PolygonPatch>
                </gml:polygonPatches>
            </gml:PolyhedralSurface>"""

    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt().find('POLYHEDRALSURFACE (((0 -1,0 1,') >= 0, \
        '<gml:PolyhedralSurface> not correctly parsed'


###############################################################################
# Test GML Tin

def test_gml_Tin():

    gml = """<gml:Tin xmlns:foo="http://bar">
                <gml:patches xmlns:foo="http://bar">
                    <gml:Triangle xmlns:foo="http://bar">
                        <gml:exterior xmlns:foo="http://bar">
                            <gml:LinearRing xmlns:foo="http://bar">
                                <gml:posList srsDimension="3">0 0 1 0 1 1 1 1 1 1 0 1 0 0 1 </gml:posList>
                            </gml:LinearRing>
                        </gml:exterior>
                     </gml:Triangle>
                 </gml:patches>
             </gml:Tin>"""

    geom = ogr.CreateGeometryFromGML(gml)

    # NOTE - this is actually an invalid TIN surface, as the triangle is incorrect
    # need to assert geom.IsValid() == True to determine the validity of the geometry
    assert geom.ExportToWkt() == 'TIN Z (((0 0 1,0 1 1,1 1 1,1 0 1,0 0 1)))', \
        '<gml:Tin> not correctly parsed'

    # Test for gml:TriangulatedSurface
    gml = """<gml:TriangulatedSurface>
                <gml:patches>
                    <gml:Triangle>
                        <gml:exterior>
                            <gml:LinearRing>
                                <gml:posList srsDimension="3">0 0 0 0 0 1 0 1 0 0 0 0</gml:posList>
                            </gml:LinearRing>
                        </gml:exterior>
                    </gml:Triangle>
                    <gml:Triangle>
                        <gml:exterior>
                            <gml:LinearRing>
                                <gml:posList srsDimension="3">0 0 0 0 1 0 1 1 0 0 0 0</gml:posList>
                            </gml:LinearRing>
                        </gml:exterior>
                    </gml:Triangle>
                </gml:patches>
            </gml:TriangulatedSurface>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'TIN Z (((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,0 0 0)))', \
        '<gml:Tin> not correctly parsed'

    # substituting gml:trianglePatches for gml:patches
    gml = """<gml:TriangulatedSurface>
                <gml:trianglePatches>
                    <gml:Triangle>
                        <gml:exterior>
                            <gml:LinearRing>
                                <gml:posList srsDimension="3">0 0 0 0 0 1 0 1 0 0 0 0</gml:posList>
                            </gml:LinearRing>
                        </gml:exterior>
                    </gml:Triangle>
                    <gml:Triangle>
                        <gml:exterior>
                            <gml:LinearRing>
                                <gml:posList srsDimension="3">0 0 0 0 1 0 1 1 0 0 0 0</gml:posList>
                            </gml:LinearRing>
                        </gml:exterior>
                    </gml:Triangle>
                </gml:trianglePatches>
            </gml:TriangulatedSurface>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'TIN Z (((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,0 0 0)))', \
        '<gml:Tin> not correctly parsed'

    # Part 2 - Create GML File from OGR Geometries
    wkt_original = 'TIN Z (((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,0 0 0)))'

    tin = ogr.CreateGeometryFromWkt(wkt_original)
    opts = ["FORMAT=GML3"]
    gml_string = tin.ExportToGML(opts)

    if gml_string != '<gml:TriangulatedSurface><gml:patches><gml:Triangle><gml:exterior><gml:LinearRing><gml:posList srsDimension="3">0 0 0 0 0 1 0 1 0 0 0 0</gml:posList></gml:LinearRing></gml:exterior></gml:Triangle><gml:Triangle><gml:exterior><gml:LinearRing><gml:posList srsDimension="3">0 0 0 0 1 0 1 1 0 0 0 0</gml:posList></gml:LinearRing></gml:exterior></gml:Triangle></gml:patches></gml:TriangulatedSurface>':
        print(geom.ExportToWkt())
        pytest.fail('OGRGeometry::TriangulatedSurface incorrectly converted')


###############################################################################
# Test concatenated sections (#4451)


def test_gml_ConcatenatedDeduplication():

    gml = """<gml:Surface>
       <gml:patches>
        <gml:PolygonPatch interpolation="planar">
         <gml:exterior>
          <gml:Ring>
           <gml:curveMember>
            <gml:Curve>
             <gml:segments>
              <gml:LineStringSegment interpolation="linear">
               <gml:pos>0 -1</gml:pos>
               <gml:pos>0 1</gml:pos>
              </gml:LineStringSegment>
              <gml:Arc interpolation="circularArc3Points" numArc="1">
               <gml:pos>0 1</gml:pos>
               <gml:pos>1 0</gml:pos>
               <gml:pos>0 -1</gml:pos>
              </gml:Arc>
             </gml:segments>
            </gml:Curve>
           </gml:curveMember>
          </gml:Ring></gml:exterior>
         </gml:PolygonPatch>
        </gml:patches>
       </gml:Surface>"""

    geom = ogr.CreateGeometryFromGML(gml)

    expected_wkt = 'CURVEPOLYGON (COMPOUNDCURVE ((0 -1,0 1),CIRCULARSTRING (0 1,1 0,0 -1)))'
    assert ogrtest.check_feature_geometry(geom, ogr.CreateGeometryFromWkt(expected_wkt)) == 0

    assert not ogrtest.have_geos() or geom.IsValid(), 'geometry not valid'

###############################################################################
# Test OGRFormatDouble() to check for rounding errors (would also apply for KML output, or ogrinfo output)


def gml_out_precision():

    geom = ogr.CreateGeometryFromWkt('POINT(93538.15 1.23456789)')
    expected_gml = '<gml:Point><gml:coordinates>93538.15,1.23456789</gml:coordinates></gml:Point>'
    got_gml = geom.ExportToGML()
    assert got_gml == expected_gml, 'did not get expected gml'

    geom = ogr.CreateGeometryFromWkt('POINT(93538.55 1234567.89)')
    expected_gml = '<gml:Point><gml:coordinates>93538.55,1234567.89</gml:coordinates></gml:Point>'
    got_gml = geom.ExportToGML()
    assert got_gml == expected_gml, 'did not get expected gml'

###############################################################################
# Test various error cases of gml2ogrgeometry.cpp


def test_gml_invalid_geoms():

    gml_expected_wkt_list = [
        ('<?xml version="1.0" encoding="UTF-8"?>', None),
        ('<!-- bla -->', None),
        ('<foo/>', None),
        ('<gml:Point><gml:pos>31 29 16</gml:pos><gml:pos>31 29 16</gml:pos></gml:Point>', None),
        ('<gml:Point><gml:coordinates/></gml:Point>', 'POINT EMPTY'),  # This is valid GML actually
        ('<gml:Point><gml:coordinates>0</gml:coordinates></gml:Point>', None),
        ('<gml:Point><gml:coordinates>0 1</gml:coordinates></gml:Point>', 'POINT (0 1)'),  # Support for uncommon formatting of coordinates
        ('<gml:Point><gml:coordinates>0 1 2</gml:coordinates></gml:Point>', 'POINT (0 1 2)'),  # Support for uncommon formatting of coordinates
        ('<gml:Point><gml:coordinates>0,1 2,3</gml:coordinates></gml:Point>', None),
        ('<gml:Point><gml:pos>0</gml:pos></gml:Point>', None),
        ('<gml:Point><gml:pos/></gml:Point>', 'POINT EMPTY'),  # This is valid GML actually
        ('<gml:Point/>', None),
        ('<gml:Point><foo/></gml:Point>', None),
        ('<gml:LineString/>', None),
        ('<gml:LineString><foo/></gml:LineString>', None),
        ('<gml:LineString><gml:posList></gml:posList></gml:LineString>', 'LINESTRING EMPTY'),  # This is valid GML actually
        ('<gml:LineString><gml:posList>0</gml:posList></gml:LineString>', None),
        ('<gml:LineString><gml:posList srsDimension="4">0 1 2 3</gml:posList></gml:LineString>', None),
        ('<gml:LineString><gml:posList srsDimension="3">0 1 2 3</gml:posList></gml:LineString>', None),
        ('<gml:Point><gml:coord></gml:coord></gml:Point>', None),
        ('<gml:Point><gml:coord><gml:X/><gml:Y/></gml:coord></gml:Point>', None),
        ('<gml:Point><gml:coord><gml:X>0</gml:X></gml:coord></gml:Point>', None),
        ('<gml:Polygon/>', 'POLYGON EMPTY'),  # valid GML3, but invalid GML2. Be tolerant
        ('<gml:Polygon><gml:outerBoundaryIs/></gml:Polygon>', 'POLYGON EMPTY'),  # valid GML2
        ('<gml:Polygon><gml:outerBoundaryIs><foo/></gml:outerBoundaryIs></gml:Polygon>', None),
        ('<gml:Polygon><gml:outerBoundaryIs><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:outerBoundaryIs></gml:Polygon>', None),
        ('<gml:Polygon><gml:outerBoundaryIs><gml:LinearRing><gml:posList>0 1 2 3 4 5 0 1</gml:posList></gml:LinearRing></gml:outerBoundaryIs><gml:innerBoundaryIs/></gml:Polygon>', None),
        ('<gml:Polygon><gml:outerBoundaryIs><gml:LinearRing/></gml:outerBoundaryIs><gml:innerBoundaryIs/></gml:Polygon>', None),
        ('<gml:Polygon><gml:outerBoundaryIs><gml:LinearRing><gml:posList>0 1 2 3 4 5 0 1</gml:posList></gml:LinearRing></gml:outerBoundaryIs><gml:innerBoundaryIs><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:innerBoundaryIs></gml:Polygon>', None),
        ('<gml:Polygon><gml:exterior><gml:CompositeCurve/></gml:exterior></gml:Polygon>', None),
        ('<gml:Polygon><gml:exterior><gml:LinearRing><gml:posList>0 0 4 0 4 4 0 4 0 0<gml:/posList><gml:/LinearRing><gml:/exterior><gml:interior><gml:CompositeCurve/></gml:interior></gml:Polygon>', None),
        ('<gml:Ring/>', None),
        ('<gml:Ring><foo/></gml:Ring>', None),
        ('<gml:Ring><gml:curveMember/></gml:Ring>', None),
        ('<gml:Ring><gml:curveMember><foo/></gml:curveMember></gml:Ring>', None),
        ('<gml:Ring><gml:curveMember><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:curveMember></gml:Ring>', None),
        ('<gml:Box/>', None),
        ('<gml:Box><gml:pos>31 29 16</gml:pos></gml:Box>', None),
        ('<gml:MultiPolygon/>', 'MULTIPOLYGON EMPTY'),  # valid GML3, but invalid GML2. Be tolerant
        ('<gml:MultiPolygon><foo/></gml:MultiPolygon>', 'MULTIPOLYGON EMPTY'),  # illegal GML, but we are tolerant
        ('<gml:MultiPolygon><gml:polygonMember/></gml:MultiPolygon>', 'MULTIPOLYGON EMPTY'),  # valid in GML3 (accepted by PostGIS too)
        ('<gml:MultiPolygon><gml:polygonMember><foo/></gml:polygonMember></gml:MultiPolygon>', None),
        ('<gml:MultiPolygon><gml:polygonMember><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:polygonMember></gml:MultiPolygon>', None),
        ('<gml:MultiSurface><gml:surfaceMembers/></gml:MultiSurface>', 'MULTIPOLYGON EMPTY'),  # valid GML
        ('<gml:MultiSurface><gml:surfaceMembers><foo/></gml:surfaceMembers></gml:MultiSurface>', 'MULTIPOLYGON EMPTY'),  # illegal GML, but we are tolerant
        ('<gml:MultiSurface><gml:surfaceMembers><gml:Polygon/></gml:surfaceMembers></gml:MultiSurface>', 'MULTIPOLYGON EMPTY'),  # valid GML3
        ('<gml:MultiPoint/>', 'MULTIPOINT EMPTY'),
        ('<gml:MultiPoint><foo/></gml:MultiPoint>', 'MULTIPOINT EMPTY'),
        ('<gml:MultiPoint><gml:pointMember/></gml:MultiPoint>', 'MULTIPOINT EMPTY'),  # valid in GML3 (accepted by PostGIS too)
        ('<gml:MultiPoint><gml:pointMember><gml:LineString><gml:posList>0 1 2 3</gml:posList></gml:LineString></gml:pointMember></gml:MultiPoint>', None),
        ('<gml:MultiPoint><gml:pointMembers></gml:pointMembers></gml:MultiPoint>', 'MULTIPOINT EMPTY'),
        ('<gml:MultiPoint><gml:pointMembers><foo/></gml:pointMembers></gml:MultiPoint>', 'MULTIPOINT EMPTY'),
        ('<gml:MultiPoint><gml:pointMembers><gml:Point/></gml:pointMembers></gml:MultiPoint>', None),
        ('<gml:MultiLineString/>', 'MULTILINESTRING EMPTY'),
        ('<gml:MultiLineString><foo/></gml:MultiLineString>', 'MULTILINESTRING EMPTY'),
        ('<gml:MultiLineString><gml:lineStringMember/></gml:MultiLineString>', None),
        ('<gml:MultiLineString><gml:lineStringMember><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:lineStringMember></gml:MultiLineString>', None),
        ('<gml:MultiCurve/>', 'MULTILINESTRING EMPTY'),
        ('<gml:MultiCurve><foo/></gml:MultiCurve>', 'MULTILINESTRING EMPTY'),
        ('<gml:MultiCurve><gml:curveMember/></gml:MultiCurve>', 'MULTILINESTRING EMPTY'),  # valid in GML3 (accepted by PostGIS too)
        ('<gml:MultiCurve><gml:curveMember><foo/></gml:curveMember></gml:MultiCurve>', None),
        ('<gml:MultiCurve><gml:curveMember><gml:Curve/></gml:curveMember></gml:MultiCurve>', None),
        ('<gml:MultiCurve><gml:curveMember><gml:Curve><foo/></gml:Curve></gml:curveMember></gml:MultiCurve>', None),
        ('<gml:MultiCurve><gml:curveMember><gml:Curve><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:Curve></gml:curveMember></gml:MultiCurve>', None),
        ('<gml:MultiCurve><gml:curveMembers></gml:curveMembers></gml:MultiCurve>', 'MULTILINESTRING EMPTY'),
        ('<gml:MultiCurve><gml:curveMembers><foo/></gml:curveMembers></gml:MultiCurve>', None),
        ('<gml:MultiCurve><gml:curveMembers><gml:LineString/></gml:curveMembers></gml:MultiCurve>', None),
        ('<gml:Curve/>', None),
        ('<gml:Curve><foo/></gml:Curve>', None),
        ('<gml:Curve><gml:segments/></gml:Curve>', None),
        ('<gml:Curve><gml:segments><foo/></gml:segments></gml:Curve>', None),
        ('<gml:Curve><gml:segments><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:segments></gml:Curve>', None),
        ('<gml:Arc/>', None),
        ('<gml:Arc><gml:posList>0 0 0 1</gml:posList></gml:Arc>', None),
        ('<gml:Arc><gml:posList>0 0 0 1 1 0 2 0</gml:posList></gml:Arc>', None),
        ('<gml:ArcString/>', None),
        ('<gml:ArcString><gml:posList>0 0 0 1</gml:posList></gml:ArcString>', None),
        ('<gml:ArcString><gml:posList>0 0 0 1 1 0 2 0</gml:posList></gml:ArcString>', None),
        ('<gml:Circle/>', None),
        ('<gml:Circle><gml:posList>0 0 0 1</gml:posList></gml:Circle>', None),
        ('<gml:segments/>', None),
        ('<gml:segments><foo/></gml:segments>', None),
        ('<gml:segments><gml:LineStringSegment/></gml:segments>', None),
        ('<gml:segments><gml:LineStringSegment><foo/></gml:LineStringSegment></gml:segments>', None),
        ('<gml:segments><gml:LineStringSegment><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:LineStringSegment></gml:segments>', None),
        ('<gml:MultiGeometry/>', 'GEOMETRYCOLLECTION EMPTY'),
        ('<gml:MultiGeometry><foo/></gml:MultiGeometry>', 'GEOMETRYCOLLECTION EMPTY'),
        ('<gml:MultiGeometry><gml:geometryMember/></gml:MultiGeometry>', 'GEOMETRYCOLLECTION EMPTY'),  # valid in GML3 (accepted by PostGIS too)
        ('<gml:MultiGeometry><gml:geometryMember><foo/></gml:geometryMember></gml:MultiGeometry>', None),
        ('<gml:MultiGeometry><gml:geometryMembers><foo/></gml:geometryMembers></gml:MultiGeometry>', None),
        ('<gml:Surface/>', 'POLYGON EMPTY'),  # valid GML3
        ('<gml:Surface><foo/></gml:Surface>', 'POLYGON EMPTY'),  # invalid GML3, but we are tolerant
        ('<gml:Surface><gml:patches/></gml:Surface>', 'POLYGON EMPTY'),  # valid GML3
        ('<gml:Surface><gml:patches><foo/></gml:patches></gml:Surface>', None),
        ('<gml:Surface><gml:patches><gml:PolygonPatch/></gml:patches></gml:Surface>', 'POLYGON EMPTY'),  # valid GML3
        ('<gml:Solid/>', 'POLYHEDRALSURFACE EMPTY'),  # valid GML3
        ('<gml:Solid><foo/></gml:Solid>', 'POLYHEDRALSURFACE EMPTY'),  # invalid GML3, but we are tolerant
        ('<gml:Solid><gml:exterior/></gml:Solid>', 'POLYHEDRALSURFACE EMPTY'),  # valid GML3
        ('<gml:Solid><gml:exterior><foo/></gml:exterior></gml:Solid>', None),
        ('<gml:Solid><gml:exterior><Polygon><exterior><LinearRing><posList srsDimension="2">0 0 4 0 4 4 0 4 0 0</posList></LinearRing></exterior></Polygon></gml:exterior><gml:interior/></gml:Solid>', 'POLYGON ((0 0,4 0,4 4,0 4,0 0))'),
        ('<gml:OrientableSurface/>', None),
        ('<gml:OrientableSurface><foo/></gml:OrientableSurface>', None),
        ('<gml:OrientableSurface><gml:baseSurface/></gml:OrientableSurface>', None),
        ('<gml:OrientableSurface><gml:baseSurface><foo/></gml:baseSurface></gml:OrientableSurface>', None),
        ('<gmlce:SimplePolygon/>', None),  # invalid
        ('<gmlce:SimplePolygon><foo/></gmlce:SimplePolygon>', None),  # invalid GML3, but we are tolerant
        ('<gmlce:SimplePolygon><gml:posList/></gmlce:SimplePolygon>', 'POLYGON EMPTY'),  # validates the schema
        ('<gmlce:SimpleMultiPoint/>', None),  # invalid
        ('<gmlce:SimpleMultiPoint><foo/></gmlce:SimpleMultiPoint>', None),  # invalid GML3, but we are tolerant
        ('<gmlce:SimpleMultiPoint><gml:posList/></gmlce:SimpleMultiPoint>', 'MULTIPOINT EMPTY'),  # validates the schema
        ('<gml:Envelope/>', None),
        ('<gml:Envelope><gml:lowerCorner/><gml:upperCorner/></gml:Envelope>', None),
        ('<gml:Envelope><gml:lowerCorner>1</gml:lowerCorner><gml:upperCorner>3 4</gml:upperCorner/></gml:Envelope>', None),
        ('<gml:Point><gml:coordinates cs="bla">1bla2</gml:coordinates></gml:Point>', None),
        ('<gml:Point><gml:coordinates cs="">1bla2</gml:coordinates></gml:Point>', None),
        ('<gml:Point><gml:coordinates cs="0">1bla2</gml:coordinates></gml:Point>', None),
        ('<gml:Point><gml:coordinates ts="bla">1,2</gml:coordinates></gml:Point>', None),
        ('<gml:Point><gml:coordinates ts="">1,2</gml:coordinates></gml:Point>', None),
        ('<gml:Point><gml:coordinates ts="0">1,2</gml:coordinates></gml:Point>', None),
        ('<gml:Point><gml:coordinates decimal="bla">1,2</gml:coordinates></gml:Point>', None),
        ('<gml:Point><gml:coordinates decimal="">1,2</gml:coordinates></gml:Point>', None),
        ('<gml:Point><gml:coordinates decimal="0">1,2</gml:coordinates></gml:Point>', None),
        ("""<gml:Curve><gml:segments>
            <gml:Arc><gml:posList>0 0 1 0 0 0</gml:posList></gml:Arc>
            <gml:LineStringSegment><gml:posList>-10 0 -1 0 0 0</gml:posList></gml:LineStringSegment>
            </gml:segments></gml:Curve>""", "COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0),(0 0,-1 0,-10 0))"),  # non contiguous segments
        ("""<gml:CompositeCurve>
            <gml:curveMember><gml:Curve><gml:segments><gml:LineString><gml:posList>0 0 1 0 0 0</gml:posList></gml:LineString></gml:segments></gml:Curve></gml:curveMember>
            <gml:curveMember>
                    <gml:Curve><gml:segments>
                        <gml:Arc><gml:posList>-10 0 1 0 0 0</gml:posList></gml:Arc>
                        <gml:LineStringSegment><gml:posList>0 0 -1 0 0 0</gml:posList></gml:LineStringSegment>
                    </gml:segments></gml:Curve>
            </gml:curveMember>
            </gml:CompositeCurve>""", None),  # non contiguous segments
        ("<gml:ArcByBulge><gml:bulge>2</gml:bulge><gml:normal>-1</gml:normal></gml:ArcByBulge>", None),
        ("<gml:ArcByBulge><gml:posList>2 0</gml:posList><gml:bulge>2</gml:bulge><gml:normal>-1</gml:normal></gml:ArcByBulge>", None),
        ("<gml:ArcByBulge><gml:posList>2 0 -2 0</gml:posList><gml:normal>-1</gml:normal></gml:ArcByBulge>", None),
        ("<gml:ArcByBulge><gml:posList>2 0 -2 0</gml:posList><gml:normal>-1</gml:normal></gml:ArcByBulge>", None),
        ("<ArcByBulge><bulge/></ArcByBulge>", None),
        ("<gml:ArcByCenterPoint><gml:radius>2</gml:radius><gml:startAngle>90</gml:startAngle><gml:endAngle>270</gml:endAngle></gml:ArcByCenterPoint>", None),
        ("<gml:ArcByCenterPoint><gml:pos>1 2</gml:pos><gml:startAngle>90</gml:startAngle><gml:endAngle>270</gml:endAngle></gml:ArcByCenterPoint>", None),
        ("<gml:ArcByCenterPoint><gml:pos>1 2</gml:pos><gml:radius>2</gml:radius<gml:endAngle>270</gml:endAngle></gml:ArcByCenterPoint>", None),
        ("<gml:ArcByCenterPoint><gml:pos>1 2</gml:pos><gml:radius>2</gml:radius><gml:startAngle>90</gml:startAngle></gml:ArcByCenterPoint>", None),
        ("<gml:CircleByCenterPoint><gml:radius>2</gml:radius></gml:CircleByCenterPoint>", None),
        ("<gml:CircleByCenterPoint><gml:pos>1 2</gml:pos></gml:CircleByCenterPoint>", None),
        ('<gml:PolyhedralSurface><foo/></gml:PolyhedralSurface>', None),
        ('<gml:PolyhedralSurface><gml:polygonPatches><foo/></gml:polygonPatches></gml:PolyhedralSurface>', None),
        ('<gml:PolyhedralSurface><gml:polygonPatches><gml:PolygonPatch><gml:exterior><foo/></gml:exterior></gml:PolygonPatch></gml:polygonPatches></gml:PolyhedralSurface>', None),
        ('<gml:Triangle><gml:exterior><gml:CompositeCurve/></gml:exterior></gml:Triangle>', None),
    ]

    for (gml, expected_wkt) in gml_expected_wkt_list:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        # print gml
        geom = ogr.CreateGeometryFromGML(gml)
        gdal.PopErrorHandler()
        if geom is None:
            assert expected_wkt is None, \
                ('did not get expected result for %s. Got None instead of %s' % (gml, expected_wkt))
        else:
            wkt = geom.ExportToWkt()
            if expected_wkt is None:
                pytest.fail('did not get expected result for %s. Got %s instead of None' % (gml, wkt))
            else:
                assert wkt == expected_wkt, \
                    ('did not get expected result for %s. Got %s instead of %s' % (gml, wkt, expected_wkt))


###############################################################################
# Test write support for GML3


def test_gml_write_gml3_geometries():

    gml_list = ['<gml:Point><gml:pos>2 3</gml:pos></gml:Point>',
                '<gml:Point><gml:pos>2 3 4</gml:pos></gml:Point>',
                '<gml:LineString><gml:posList>2 3 4 5</gml:posList></gml:LineString>',
                '<gml:Curve><gml:segments><gml:LineStringSegment><gml:posList>2 3 4 5</gml:posList></gml:LineStringSegment></gml:segments></gml:Curve>',
                '<gml:LineString><gml:posList srsDimension="3">2 3 10 4 5 20</gml:posList></gml:LineString>',
                '<gml:Curve><gml:segments><gml:LineStringSegment><gml:posList srsDimension="3">2 3 10 4 5 20</gml:posList></gml:LineStringSegment></gml:segments></gml:Curve>',
                '<gml:Polygon><gml:exterior><gml:LinearRing><gml:posList>0 0 0 1 1 1 1 0 0 0</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon>',
                '<gml:Polygon><gml:exterior><gml:LinearRing><gml:posList>0 0 0 1 1 1 1 0 0 0</gml:posList></gml:LinearRing></gml:exterior><gml:interior><gml:LinearRing><gml:posList>10 10 10 11 11 11 10 10</gml:posList></gml:LinearRing></gml:interior></gml:Polygon>',
                '<gml:MultiPoint><gml:pointMember><gml:Point><gml:pos>2 3</gml:pos></gml:Point></gml:pointMember><gml:pointMember><gml:Point><gml:pos>4 5</gml:pos></gml:Point></gml:pointMember></gml:MultiPoint>',
                '<gml:MultiCurve><gml:curveMember><gml:LineString><gml:posList>0 1 2 3 4 5</gml:posList></gml:LineString></gml:curveMember><gml:curveMember><gml:LineString><gml:posList>6 7 8 9 10 11</gml:posList></gml:LineString></gml:curveMember></gml:MultiCurve>',
                '<gml:MultiCurve><gml:curveMember><gml:Curve><gml:segments><gml:LineStringSegment><gml:posList>0 1 2 3 4 5</gml:posList></gml:LineStringSegment></gml:segments></gml:Curve></gml:curveMember><gml:curveMember><gml:Curve><gml:segments><gml:LineStringSegment><gml:posList>6 7 8 9 10 11</gml:posList></gml:LineStringSegment></gml:segments></gml:Curve></gml:curveMember></gml:MultiCurve>',
                '<gml:MultiSurface><gml:surfaceMember><gml:Polygon><gml:exterior><gml:LinearRing><gml:posList>0 1 2 3 4 5 0 1</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon></gml:surfaceMember><gml:surfaceMember><gml:Polygon><gml:exterior><gml:LinearRing><gml:posList>6 7 8 9 10 11 6 7</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon></gml:surfaceMember></gml:MultiSurface>',
                '<gml:MultiGeometry><gml:geometryMember><gml:Point><gml:pos>0 1</gml:pos></gml:Point></gml:geometryMember><gml:geometryMember><gml:LineString><gml:posList>2 3 4 5</gml:posList></gml:LineString></gml:geometryMember></gml:MultiGeometry>']

    for gml_in in gml_list:
        geom = ogr.CreateGeometryFromGML(gml_in)
        if gml_in.find('<gml:Curve') != -1:
            gml_out = geom.ExportToGML(['FORMAT=GML3', 'GML3_LINESTRING_ELEMENT=curve'])
        else:
            gml_out = geom.ExportToGML(['FORMAT=GML3'])
        assert gml_out == gml_in, ('got %s, instead of %s' % (gml_out, gml_in))


###############################################################################
# Test write support for GML3 SRS


def test_gml_write_gml3_srs():

    sr32631 = osr.SpatialReference()
    sr32631.SetFromUserInput("EPSG:32631")

    srlonglat = osr.SpatialReference()
    srlonglat.SetFromUserInput("EPSG:4326")
    srlonglat.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    srlatlong = osr.SpatialReference()
    srlatlong.SetFromUserInput("EPSG:4326")

    geom = ogr.CreateGeometryFromWkt('POINT(500000 4500000)')
    geom.AssignSpatialReference(sr32631)
    gml3 = geom.ExportToGML(options=['FORMAT=GML3'])
    expected_gml = '<gml:Point srsName="urn:ogc:def:crs:EPSG::32631"><gml:pos>500000 4500000</gml:pos></gml:Point>'
    assert gml3 == expected_gml, ('got %s, instead of %s' % (gml3, expected_gml))

    # Should perform the needed coordinate order swapping
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    geom.AssignSpatialReference(srlonglat)
    gml3 = geom.ExportToGML(options=['FORMAT=GML3'])
    expected_gml = '<gml:Point srsName="urn:ogc:def:crs:EPSG::4326"><gml:pos>49 2</gml:pos></gml:Point>'
    assert gml3 == expected_gml, ('got %s, instead of %s' % (gml3, expected_gml))

    # Should not change the coordinate order.
    geom = ogr.CreateGeometryFromWkt('POINT(49 2)')
    geom.AssignSpatialReference(srlatlong)
    gml3 = geom.ExportToGML(options=['FORMAT=GML3'])
    expected_gml = '<gml:Point srsName="urn:ogc:def:crs:EPSG::4326"><gml:pos>49 2</gml:pos></gml:Point>'
    assert gml3 == expected_gml, ('got %s, instead of %s' % (gml3, expected_gml))

    # Legacy SRS format
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    geom.AssignSpatialReference(srlonglat)
    gml3 = geom.ExportToGML(options=['FORMAT=GML3', 'GML3_LONGSRS=NO'])
    expected_gml = '<gml:Point srsName="EPSG:4326"><gml:pos>2 49</gml:pos></gml:Point>'
    assert gml3 == expected_gml, ('got %s, instead of %s' % (gml3, expected_gml))

    # Test SRSNAME_FORMAT=SHORT
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    geom.AssignSpatialReference(srlonglat)
    gml3 = geom.ExportToGML(options=['FORMAT=GML3', 'SRSNAME_FORMAT=SHORT'])
    expected_gml = '<gml:Point srsName="EPSG:4326"><gml:pos>2 49</gml:pos></gml:Point>'
    assert gml3 == expected_gml, ('got %s, instead of %s' % (gml3, expected_gml))

    # Test SRSNAME_FORMAT=SRSNAME_FORMAT
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    geom.AssignSpatialReference(srlonglat)
    gml3 = geom.ExportToGML(options=['FORMAT=GML3', 'SRSNAME_FORMAT=OGC_URN'])
    expected_gml = '<gml:Point srsName="urn:ogc:def:crs:EPSG::4326"><gml:pos>49 2</gml:pos></gml:Point>'
    assert gml3 == expected_gml, ('got %s, instead of %s' % (gml3, expected_gml))

    # Test SRSNAME_FORMAT=OGC_URL
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    geom.AssignSpatialReference(srlonglat)
    gml3 = geom.ExportToGML(options=['FORMAT=GML3', 'SRSNAME_FORMAT=OGC_URL'])
    expected_gml = '<gml:Point srsName="http://www.opengis.net/def/crs/EPSG/0/4326"><gml:pos>49 2</gml:pos></gml:Point>'
    assert gml3 == expected_gml, ('got %s, instead of %s' % (gml3, expected_gml))

###############################################################################
# Test that importing too nested GML doesn't cause stack overflows


def test_gml_nested():

    gml = '<gml:MultiGeometry><gml:geometryMember>' * 31
    gml += '<gml:MultiPolygon></gml:MultiPolygon>'
    gml += '</gml:geometryMember></gml:MultiGeometry>' * 31

    geom = ogr.CreateGeometryFromGML(gml)
    assert geom is not None, 'expected a geometry'

    gml = '<gml:MultiGeometry><gml:geometryMember>' * 32
    gml += '<gml:MultiPolygon></gml:MultiPolygon>'
    gml += '</gml:geometryMember></gml:MultiGeometry>' * 32

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    geom = ogr.CreateGeometryFromGML(gml)
    gdal.PopErrorHandler()
    assert geom is None, 'expected None'

###############################################################################
# Test GML 3.3 SimplePolygon


def test_gml_SimplePolygon():

    gml = """<gmlce:SimplePolygon><gml:posList>0 0 1 0 1 1 0 1</gml:posList></gmlce:SimplePolygon>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'POLYGON ((0 0,1 0,1 1,0 1,0 0))', \
        '<gmlce:SimplePolygon> not correctly parsed'

###############################################################################
# Test GML 3.3 SimpleRectangle


def test_gml_SimpleRectangle():

    gml = """<gmlce:SimpleRectangle><gml:posList>0 0 1 0 1 1 0 1</gml:posList></gmlce:SimpleRectangle>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'POLYGON ((0 0,1 0,1 1,0 1,0 0))', \
        '<gmlce:SimpleRectangle> not correctly parsed'

###############################################################################
# Test GML 3.3 SimpleTriangle


def test_gml_SimpleTriangle():

    gml = """<gmlce:SimpleTriangle><gml:posList>0 0 1 0 1 1</gml:posList></gmlce:SimpleTriangle>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'TRIANGLE ((0 0,1 0,1 1,0 0))', \
        '<gmlce:SimpleTriangle> not correctly parsed'

###############################################################################
# Test GML 3.3 SimpleMultiPoint


def test_gml_SimpleMultiPoint():

    gml = """<gmlce:SimpleMultiPoint><gml:posList>0 1 2 3</gml:posList></gmlce:SimpleMultiPoint>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'MULTIPOINT (0 1,2 3)', \
        '<gmlce:SimpleMultiPoint> not correctly parsed'

###############################################################################
# Test  gml:CompositeCurve> in <gml:Ring>


def test_gml_CompositeCurveInRing():

    gml = """<gml:Surface>
    <gml:patches>
        <gml:PolygonPatch interpolation="planar">
            <gml:exterior>
                <gml:Ring>
                    <gml:curveMember>
                        <gml:CompositeCurve>
                            <gml:curveMember>
                                <gml:Curve>
                                    <gml:segments>
                                        <gml:LineStringSegment>
                                            <gml:pos>0 0</gml:pos>
                                            <gml:pos>0 1</gml:pos>
                                        </gml:LineStringSegment>
                                    </gml:segments>
                                </gml:Curve>
                            </gml:curveMember>
                            <gml:curveMember>
                                <gml:Curve>
                                    <gml:segments>
                                        <gml:LineStringSegment>
                                            <gml:pos>0 1</gml:pos>
                                            <gml:pos>1 1</gml:pos>
                                        </gml:LineStringSegment>
                                    </gml:segments>
                                </gml:Curve>
                            </gml:curveMember>
                            <gml:curveMember>
                                <gml:Curve>
                                    <gml:segments>
                                        <gml:LineStringSegment>
                                            <gml:pos>1 1</gml:pos>
                                            <gml:pos>0 0</gml:pos>
                                        </gml:LineStringSegment>
                                    </gml:segments>
                                </gml:Curve>
                            </gml:curveMember>
                        </gml:CompositeCurve>
                    </gml:curveMember>
                </gml:Ring>
            </gml:exterior>
        </gml:PolygonPatch>
    </gml:patches>
</gml:Surface>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'POLYGON ((0 0,0 1,1 1,0 0))'

###############################################################################
# Test <gml:CompositeSurface> in <gml:surfaceMembers> (#5369)


def test_gml_CompositeSurface_in_surfaceMembers():

    gml = """<gml:MultiSurface>
          <gml:surfaceMembers>
            <gml:CompositeSurface>
              <gml:surfaceMember>
                <gml:Polygon>
                  <gml:exterior>
                    <gml:LinearRing>
                      <gml:posList>0 0 0 1 1 1 1 0 0 0</gml:posList>
                    </gml:LinearRing>
                  </gml:exterior>
                </gml:Polygon>
              </gml:surfaceMember>
              <gml:surfaceMember>
                <gml:Polygon>
                  <gml:exterior>
                    <gml:LinearRing>
                      <gml:posList>2 0 2 1 3 1 3 0 2 0</gml:posList>
                    </gml:LinearRing>
                  </gml:exterior>
                </gml:Polygon>
              </gml:surfaceMember>
            </gml:CompositeSurface>
          </gml:surfaceMembers>
        </gml:MultiSurface>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)),((2 0,2 1,3 1,3 0,2 0)))'

###############################################################################
# Test <gml:PolygonPatch> with only Interior ring (#5421)


def test_gml_MultiSurfaceOfSurfaceOfPolygonPatchWithInteriorRing():

    gml = """<gml:MultiSurface>
               <gml:surfaceMember>
                 <gml:Surface>
                    <gml:patches>
                      <gml:PolygonPatch>
                        <gml:exterior>
                          <gml:LinearRing>
                            <gml:posList>0 0 0 1 1 1 1 0 0 0</gml:posList>
                          </gml:LinearRing>
                        </gml:exterior>
                      </gml:PolygonPatch>
                    </gml:patches>
                  </gml:Surface>
                </gml:surfaceMember>
                <gml:surfaceMember>
                  <gml:Surface>
                    <gml:patches>
                      <gml:PolygonPatch>
                        <gml:interior>
                          <gml:LinearRing>
                            <gml:posList>0.25 0.25 0.25 0.75 0.75 0.75 0.75 0.25 0.25 0.25</gml:posList>
                          </gml:LinearRing>
                        </gml:interior>
                      </gml:PolygonPatch>
                    </gml:patches>
                  </gml:Surface>
                </gml:surfaceMember>
               <gml:surfaceMember>
                 <gml:Surface>
                    <gml:patches>
                      <gml:PolygonPatch>
                        <gml:exterior>
                          <gml:LinearRing>
                            <gml:posList>0 0 0 -1 -1 -1 -1 0 0 0</gml:posList>
                          </gml:LinearRing>
                        </gml:exterior>
                      </gml:PolygonPatch>
                    </gml:patches>
                  </gml:Surface>
                </gml:surfaceMember>
              </gml:MultiSurface>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.75 0.25,0.25 0.25)),((0 0,0 -1,-1 -1,-1 0,0 0)))'

###############################################################################
# Test ts, cs and decimal attributes of gml:coordinates


def test_gml_Coordinates_ts_cs_decimal():

    gml_expected_wkt_list = [
        ('<gml:Point><gml:coordinates>1,2</gml:coordinates></gml:Point>', 'POINT (1 2)'),  # default values
        ('<gml:Point><gml:coordinates cs="," ts=" " decimal=".">1,2</gml:coordinates></gml:Point>', 'POINT (1 2)'),  # default values
        ('<gml:Point><gml:coordinates cs="," ts=" " decimal=".">1,2,3</gml:coordinates></gml:Point>', 'POINT (1 2 3)'),  # default values
        ('<gml:Point><gml:coordinates cs="," ts=" " decimal=".">  1,2  </gml:coordinates></gml:Point>', 'POINT (1 2)'),  # we accept that...
        ('<gml:Point><gml:coordinates>1 2</gml:coordinates></gml:Point>', 'POINT (1 2)'),  # this is completely out of specification ! but we accept that too !
        ('<gml:Point><gml:coordinates cs=";">1;2</gml:coordinates></gml:Point>', 'POINT (1 2)'),
        ('<gml:Point><gml:coordinates decimal="," cs=";">1,2;3,4</gml:coordinates></gml:Point>', 'POINT (1.2 3.4)'),
        ('<gml:Point><gml:coordinates decimal="," cs=";">1,2;3,4;5,6</gml:coordinates></gml:Point>', 'POINT (1.2 3.4 5.6)'),
        ('<gml:LineString><gml:coordinates>1,2 3,4</gml:coordinates></gml:LineString>', 'LINESTRING (1 2,3 4)'),  # default values
        ('<gml:LineString><gml:coordinates cs="," ts=" " decimal=".">1,2 3,4</gml:coordinates></gml:LineString>', 'LINESTRING (1 2,3 4)'),  # default values
        ('<gml:LineString><gml:coordinates cs="," ts=" " decimal=".">1,2,2.5 3,4</gml:coordinates></gml:LineString>', 'LINESTRING (1 2 2.5,3 4 0)'),  # default values
        ('<gml:LineString><gml:coordinates ts="-">1,2-3,4</gml:coordinates></gml:LineString>', 'LINESTRING (1 2,3 4)'),
        ('<gml:LineString><gml:coordinates cs=" " ts=",">1 2,3 4</gml:coordinates></gml:LineString>', 'LINESTRING (1 2,3 4)'),
        ('<gml:LineString><gml:coordinates cs=" " ts=",">1 2 2.5,3 4</gml:coordinates></gml:LineString>', 'LINESTRING (1 2 2.5,3 4 0)'),
    ]

    for (gml, expected_wkt) in gml_expected_wkt_list:
        geom = ogr.CreateGeometryFromGML(gml)
        wkt = geom.ExportToWkt()
        if expected_wkt is None:
            pytest.fail('did not get expected result for %s. Got %s instead of None' % (gml, wkt))
        else:
            assert wkt == expected_wkt, \
                ('did not get expected result for %s. Got %s instead of %s' % (gml, wkt, expected_wkt))


###############################################################################
# Test gml with XML header and comments


def test_gml_with_xml_header_and_comments():

    gml_expected_wkt_list = [
        ('<?xml version="1.0" encoding="UTF-8"?><!-- comment --><gml:Point> <!-- comment --> <gml:coordinates>1,2</gml:coordinates></gml:Point>', 'POINT (1 2)'),
        ("""<gml:MultiSurface><!-- comment -->
               <gml:surfaceMember><!-- comment -->
                 <gml:Surface><!-- comment -->
                    <gml:patches><!-- comment -->
                      <gml:PolygonPatch><!-- comment -->
                        <gml:exterior><!-- comment -->
                          <gml:LinearRing><!-- comment -->
                            <gml:posList>0 0 0 1 1 1 1 0 0 0</gml:posList>
                          </gml:LinearRing>
                        </gml:exterior>
                        <!-- comment -->
                        <gml:interior><!-- comment -->
                          <gml:LinearRing><!-- comment -->
                            <gml:posList>0.25 0.25 0.25 0.75 0.75 0.75 0.75 0.25 0.25 0.25</gml:posList>
                          </gml:LinearRing>
                        </gml:interior>
                      </gml:PolygonPatch>
                    </gml:patches>
                  </gml:Surface>
                </gml:surfaceMember>
              </gml:MultiSurface>""", 'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0),(0.25 0.25,0.25 0.75,0.75 0.75,0.75 0.25,0.25 0.25)))'),
    ]

    for (gml, expected_wkt) in gml_expected_wkt_list:
        geom = ogr.CreateGeometryFromGML(gml)
        wkt = geom.ExportToWkt()
        if expected_wkt is None:
            pytest.fail('did not get expected result for %s. Got %s instead of None' % (gml, wkt))
        else:
            assert wkt == expected_wkt, \
                ('did not get expected result for %s. Got %s instead of %s' % (gml, wkt, expected_wkt))


###############################################################################
# Test srsDimension attribute on top-level geometry and not on posList (#5606)


def test_gml_srsDimension_topgeometry():

    gml = """<gml:Surface srsName="EPSG:25832" srsDimension="3">
    <gml:patches>
        <gml:PolygonPatch>
            <gml:exterior>
                <gml:LinearRing>
                    <gml:posList>
                        0 0 10 0 1 10 1 1 10 1 0 10 0 0 10
                    </gml:posList>
                </gml:LinearRing>
            </gml:exterior>
        </gml:PolygonPatch>
    </gml:patches>
</gml:Surface>"""

    geom = ogr.CreateGeometryFromGML(gml)

    assert geom.ExportToWkt() == 'POLYGON ((0 0 10,0 1 10,1 1 10,1 0 10,0 0 10))'

###############################################################################
# Test GML Arc


def test_gml_Arc():

    gml = "<gml:Arc><gml:posList>1 0 0 1 -1 0</gml:posList></gml:Arc>"
    geom = ogr.CreateGeometryFromGML(gml)

    assert ogrtest.check_feature_geometry(geom, ogr.CreateGeometryFromWkt('CIRCULARSTRING (1 0,0 1,-1 0)')) == 0

    gml2 = geom.ExportToGML(['FORMAT=GML3'])
    expected_gml2 = '<gml:Curve><gml:segments><gml:ArcString><gml:posList>1 0 0 1 -1 0</gml:posList></gml:ArcString></gml:segments></gml:Curve>'
    assert gml2 == expected_gml2

    geom2 = ogr.CreateGeometryFromGML(gml2)
    assert geom.Equals(geom2)

###############################################################################
# Test GML ArcByBulge


def test_gml_ArcByBulge():

    gml = "<gml:ArcByBulge><gml:posList>2 0 -2 0</gml:posList><gml:bulge>2</gml:bulge><gml:normal>-1</gml:normal></gml:ArcByBulge>"
    geom = ogr.CreateGeometryFromGML(gml)

    assert ogrtest.check_feature_geometry(geom, ogr.CreateGeometryFromWkt('CIRCULARSTRING (2 0,0 2,-2 0)')) == 0

###############################################################################
# Test GML ArcByCenterPoint


def test_gml_ArcByCenterPoint():

    gml = "<gml:ArcByCenterPoint><gml:pos>1 2</gml:pos><gml:radius>2</gml:radius><gml:startAngle>90</gml:startAngle><gml:endAngle>270</gml:endAngle></gml:ArcByCenterPoint>"
    geom = ogr.CreateGeometryFromGML(gml)

    assert ogrtest.check_feature_geometry(geom, ogr.CreateGeometryFromWkt('CIRCULARSTRING (1 4,-1 2,1 0)')) == 0


###############################################################################
# Test compound curve of ArcByCenterPoint whose ends don't exactly match
# with ends of neighbouring curves, as found in some AIXM files

def test_gml_CompoundCurve_of_ArcByCenterPoint():

    gml = """
<Surface gml:id="Surface1" srsDimension="2" srsName="URN:OGC:DEF:CRS:OGC:1.3:CRS84">
  <patches>
    <PolygonPatch>
      <exterior>
        <Ring>
          <curveMember>
            <Curve ns:id="Curve1">
              <segments>
                <LineStringSegment>
                  <pos>-80.40 33.86</pos>
                  <pos>-80.27 33.63</pos>
                </LineStringSegment>
              </segments>
            </Curve>
          </curveMember>
          <curveMember>
            <Curve ns:id="Curve2">
              <segments>
                <ArcByCenterPoint numArc="1">
                  <pointProperty>
                    <Point ns:id="Point1">
                      <pos>-80.47 33.98</pos>
                    </Point>
                  </pointProperty>
                  <radius uom="NM">23</radius>
                  <startAngle uom="deg">295</startAngle>
                  <endAngle uom="deg">249</endAngle>
                </ArcByCenterPoint>
              </segments>
            </Curve>
          </curveMember>
          <curveMember>
            <Curve ns:id="Curve3">
              <segments>
                <LineStringSegment>
                  <pos>-80.63 33.62</pos>
                  <pos>-80.39 33.85</pos>
                </LineStringSegment>
              </segments>
            </Curve>
          </curveMember>
        </Ring>
      </exterior>
    </PolygonPatch>
  </patches>
</Surface>"""
    geom = ogr.CreateGeometryFromGML(gml)

    assert ogrtest.check_feature_geometry(geom, ogr.CreateGeometryFromWkt('POLYGON ((-80.4 33.86,-80.27 33.63,-80.305028054229538 33.622017309598967,-80.335422529369936 33.613343178471617,-80.366464292754429 33.606448070493634,-80.398003921948742 33.601365147653873,-80.429889693662162 33.598118851265042,-80.461968286017793 33.596724788982847,-80.494085487001527 33.597189662699385,-80.52608690656875 33.599511237590342,-80.557818688893789 33.603678352435914,-80.589128223167393 33.609670971175497,-80.619864849221443 33.617460275496377,-80.63 33.62,-80.39 33.85))')) == 0

###############################################################################
# Test GML CircleByCenterPoint


def test_gml_CircleByCenterPoint():

    gml = "<gml:CircleByCenterPoint><gml:pos>1 2</gml:pos><gml:radius>2</gml:radius></gml:CircleByCenterPoint>"
    geom = ogr.CreateGeometryFromGML(gml)

    assert ogrtest.check_feature_geometry(geom, ogr.CreateGeometryFromWkt('CIRCULARSTRING (-1 2,3 2,-1 2)')) == 0

###############################################################################
# Test GML CircleByCenterPoint with uom="m" and uom="km"


def test_gml_CircleByCenterPoint_srs_geog_uom_m_km():

    gml = '<gml:CircleByCenterPoint srsName="urn:ogc:def:crs:EPSG::4326"><gml:pos>49 2</gml:pos><gml:radius uom="m">2000</gml:radius></gml:CircleByCenterPoint>'
    geom1 = ogr.CreateGeometryFromGML(gml)
    geom1.SwapXY()

    gml = '<gml:CircleByCenterPoint srsName="URN:OGC:DEF:CRS:OGC:1.3:CRS84"><gml:pos>2 49</gml:pos><gml:radius uom="km">2</gml:radius></gml:CircleByCenterPoint>'
    geom2 = ogr.CreateGeometryFromGML(gml)

    assert ogrtest.check_feature_geometry(geom1, geom2) == 0


###############################################################################
# Test compound curve of ArcByCenterPoint whose ends don't exactly match
# with ends of neighbouring curves, as found in some AIXM files
# with all curves in the same <segment> element as found in #2356)

def test_gml_CompoundCurve_of_ArcByCenterPoint_curve_in_same_segments():

    geom = ogr.CreateGeometryFromGML("""
        <aixm:Surface srsName="urn:ogc:def:crs:EPSG::4326" gml:id="ID_249">
            <gml:patches>
                <gml:PolygonPatch>
                    <gml:exterior>
                        <gml:Ring>
                            <gml:curveMember>
                                <gml:Curve gml:id="ID_250">
                                    <gml:segments>
                                        <gml:GeodesicString>
                                            <gml:posList>55.233333333333334
                                            -36.166666666666664 55.23116372807667
                                            -36.89437337916484</gml:posList>
                                        </gml:GeodesicString>
                                        <gml:ArcByCenterPoint numArc="1">
                                            <gml:posList>55.2333333333333
                                            -36.166666666666664</gml:posList>
                                            <gml:radius uom="NM">25.0</gml:radius>
                                            <gml:startAngle uom="deg">270.0</gml:startAngle>
                                            <gml:endAngle uom="deg">497.0</gml:endAngle>
                                        </gml:ArcByCenterPoint>
                                        <gml:GeodesicString>
                                            <gml:posList>54.92816350530716 -35.674116070018954
                                            55.233333333333334
                                            -36.166666666666664</gml:posList>
                                        </gml:GeodesicString>
                                    </gml:segments>
                                </gml:Curve>
                            </gml:curveMember>
                        </gml:Ring>
                    </gml:exterior>
                </gml:PolygonPatch>
            </gml:patches>
            <aixm:horizontalAccuracy uom="KM">2.0</aixm:horizontalAccuracy>
        </aixm:Surface>""")

    assert ogrtest.check_feature_geometry(geom, ogr.CreateGeometryFromWkt('POLYGON ((55.2333333333333 -36.1666666666667,55.2311637280767 -36.8943733791648,55.2602248071013 -36.8960852160185,55.2891782700249 -36.8912782655051,55.3178697537514 -36.88292675789,55.3461587637071 -36.8710639413776,55.3739064765608 -36.8557405708675,55.4009764350458 -36.8370248014709,55.4272352367262 -36.8150019876212,55.4525532129231 -36.7897743859994,55.476805093957 -36.7614607612323,55.4998706568286 -36.7301958939182,55.5216353514589 -36.6961299915025,55.5419909016414 -36.6594280032268,55.5608358769213 -36.6202688413925,55.5780762317212 -36.5788445119267,55.5936258081681 -36.5353591583435,55.6074067992521 -36.4900280239174,55.6193501691593 -36.4430763379496,55.6293960278662 -36.3947381326996,55.6374939573672 -36.3452549984946,55.6436032872147 -36.2948747851727,55.6476933173936 -36.2438502586493,55.6497434869178 -36.19243772209,55.6497434869178 -36.1408956112433,55.6476933173936 -36.089483074684,55.6436032872147 -36.0384585481606,55.6374939573672 -35.9880783348387,55.6293960278662 -35.9385952006337,55.6193501691593 -35.8902569953837,55.6074067992521 -35.8433053094159,55.5936258081681 -35.7979741749898,55.5780762317212 -35.7544888214066,55.5608358769213 -35.7130644919408,55.5419909016414 -35.6739053301065,55.5216353514589 -35.6372033418322,55.4998706568286 -35.6031374394151,55.476805093957 -35.571872572101,55.4525532129231 -35.5435589473339,55.4272352367262 -35.5183313457121,55.4009764350458 -35.4963085318624,55.3739064765608 -35.4775927624659,55.3461587637071 -35.4622693919557,55.3178697537514 -35.4504065754433,55.2891782700249 -35.4420550678282,55.2602248071013 -35.4372481173149,55.2311508336186 -35.4360014509317,55.2020980963355 -35.4383133491681,55.1732079288891 -35.4441648063421,55.1446205685763 -35.4535197729773,55.1164744843283 -35.4663254761286,55.0889057188812 -35.4825128133848,55.0620472479757 -35.5019968161103,55.0360283592393 -35.524677177381,55.0109740532301 -35.5504388400636,54.9870044689367 -35.5791526404379,54.9642343358562 -35.6106760028906,54.9427724545944 -35.6448536812344,54.9281635053072 -35.674116070019,55.2333333333333 -36.1666666666667))')) == 0

###############################################################################
# Test Ring starting with ArcByCenterPoint

def test_gml_Ring_starting_with_ArcByCenterPoint():

    geom = ogr.CreateGeometryFromGML("""<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<ns3:Surface xmlns:ns1="http://www.opengis.net/gml/3.2" xmlns:ns2="http://www.w3.org/1999/xlink" xmlns:ns3="http://www.aixm.aero/schema/5.1" xmlns:ns4="http://www.isotc211.org/2005/gco" xmlns:ns5="http://www.isotc211.org/2005/gmd" xmlns:ns6="http://www.isotc211.org/2005/gts" xmlns:ns7="http://www.aixm.aero/schema/5.1/message" srsName="urn:ogc:def:crs:EPSG::4326" ns1:id="Ase_Proj_1561541">
    <ns1:patches>
        <ns1:PolygonPatch>
            <ns1:exterior>
                <ns1:Ring>
                    <ns1:curveMember>
                        <ns1:Curve ns1:id="Ase_Curv_1561541">
                            <ns1:segments>
<ns1:ArcByCenterPoint interpolation="circularArcCenterPointWithRadius" numArc="1">
    <ns1:posList srsName="urn:ogc:def:crs:EPSG::4326">46.5875 0.3066666666666666</ns1:posList>
    <ns1:radius uom="[nmi_i]">9.5</ns1:radius>
    <ns1:startAngle uom="deg">6.295729688631284</ns1:startAngle>
    <ns1:endAngle uom="deg">67.38797951888118</ns1:endAngle>
</ns1:ArcByCenterPoint>
<ns1:GeodesicString interpolation="geodesic">
    <ns1:posList srsName="urn:ogc:def:crs:EPSG::4326">46.64833333333333 0.5194444444444445 46.43861111111111 0.33805555555555555 46.42305555555555 0.28944444444444445</ns1:posList>
</ns1:GeodesicString>
<ns1:ArcByCenterPoint interpolation="circularArcCenterPointWithRadius" numArc="1">
    <ns1:posList srsName="urn:ogc:def:crs:EPSG::4326">46.581388888888895 0.2980555555555556</ns1:posList>
    <ns1:radius uom="[nmi_i]">9.5</ns1:radius>
    <ns1:startAngle uom="deg">-177.84615335400528</ns1:startAngle>
    <ns1:endAngle uom="deg">-120.68835384474265</ns1:endAngle>
</ns1:ArcByCenterPoint>
<ns1:GeodesicString interpolation="geodesic">
    <ns1:posList srsName="urn:ogc:def:crs:EPSG::4326">46.500277777777775 0.10055555555555556 46.54083333333333 0.10555555555555556 46.575 0.225 46.59444444444445 0.25833333333333336 46.65833333333333 0.2833333333333333 46.69555555555555 0.25555555555555554 46.745 0.33194444444444443</ns1:posList>
</ns1:GeodesicString>
                            </ns1:segments>
                        </ns1:Curve>
                    </ns1:curveMember>
                </ns1:Ring>
            </ns1:exterior>
        </ns1:PolygonPatch>
    </ns1:patches>
</ns3:Surface>""")
    #print(g)

    assert ogrtest.check_feature_geometry(geom, ogr.CreateGeometryFromWkt('POLYGON ((46.745 0.331944444444444,46.7432764927165 0.347962462754535,46.7409162535525 0.363717281627594,46.7378065348593 0.3791917846605,46.7339626150092 0.394310000163572,46.7294033759096 0.408997750371246,46.7241512079629 0.423183023082575,46.7182318974714 0.436796331344972,46.7116744971061 0.449771059387196,46.7045111801497 0.462043792829896,46.6967770793052 0.473554631563425,46.6885101109471 0.484247483569652,46.6797507857586 0.494070338341013,46.6705420067727 0.502975518511247,46.6609288558889 0.5109199085045,46.6509583699943 0.517865159176619,46.6483333333333 0.519444444444444,46.4386111111111 0.338055555555556,46.4230555555555 0.289444444444444,46.4239652827541 0.273432507330975,46.4255282239253 0.25756088338307,46.4278483647801 0.241884857622964,46.4309144976449 0.226480183207377,46.4347118090807 0.21142133567082,46.4392219496614 0.196781161237034,46.4444231204746 0.18263053185483,46.450290175977 0.169038008752016,46.4567947427662 0.156069515749848,46.4639053537626 0.143788023916768,46.4715875972233 0.132253248900933,46.4798042799442 0.121521362391196,46.4885156039408 0.111644719041597,46.4976793558324 0.102671600158084,46.5002777777778 0.100555555555556,46.5408333333333 0.105555555555556,46.575 0.225,46.5944444444444 0.258333333333333,46.6583333333333 0.283333333333333,46.6955555555556 0.255555555555556,46.745 0.331944444444444))')) == 0


###############################################################################
# Test GML Circle


def test_gml_Circle():

    gml = """<gml:Curve><gml:segments><gml:Circle>
             <gml:posList>-1 0 0 1 -0.707106781186547 -0.707106781186548</gml:posList>
             </gml:Circle></gml:segments></gml:Curve>"""

    geom = ogr.CreateGeometryFromGML(gml)

    expected_wkt = 'CIRCULARSTRING (-1 0,0 1,-0.707106781186547 -0.707106781186548,-0.923879532511287 -0.38268343236509,-1 0)'
    assert ogrtest.check_feature_geometry(geom, ogr.CreateGeometryFromWkt(expected_wkt)) == 0

    geom = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0,2 0,0 0)')
    gml2 = geom.ExportToGML(['FORMAT=GML3'])
    expected_gml2 = '<gml:Curve><gml:segments><gml:Circle><gml:posList>0 0 1 1 2 0</gml:posList></gml:Circle></gml:segments></gml:Curve>'
    assert gml2 == expected_gml2

    geom2 = ogr.CreateGeometryFromGML(gml2)
    assert geom2.ExportToWkt() == 'CIRCULARSTRING (0 0,1 1,2 0,1 -1,0 0)'

    geom = ogr.CreateGeometryFromWkt('CIRCULARSTRING (0 0 10,2 0 10,0 0 10)')
    gml2 = geom.ExportToGML(['FORMAT=GML3'])
    expected_gml2 = '<gml:Curve><gml:segments><gml:Circle><gml:posList srsDimension="3">0 0 10 1 1 10 2 0 10</gml:posList></gml:Circle></gml:segments></gml:Curve>'
    assert gml2 == expected_gml2

    gml = """<gml:PolygonPatch>
                <gml:exterior>
                    <gml:Ring>
                        <gml:curveMember>
                            <gml:Curve>
                                <gml:segments>
                                    <gml:Circle>
                                        <gml:posList>-1 0 0 1 -0.707106781186547 -0.707106781186548</gml:posList>
                                    </gml:Circle>
                                </gml:segments>
                            </gml:Curve>
                        </gml:curveMember>
                    </gml:Ring>
                </gml:exterior>
            </gml:PolygonPatch>"""

    geom = ogr.CreateGeometryFromGML(gml)

    expected_wkt = 'CURVEPOLYGON ( CIRCULARSTRING (-1 0,0 1,-0.707106781186547 -0.707106781186548,-0.923879532511287 -0.38268343236509,-1 0))'
    assert ogrtest.check_feature_geometry(geom, ogr.CreateGeometryFromWkt(expected_wkt)) == 0

###############################################################################
# Test ArcString


def test_gml_ArcString():

    gml = """<gml:ArcString><gml:posList srsDimension="2">-2 0 -1 -1 0 0</gml:posList></gml:ArcString>"""
    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'CIRCULARSTRING (-2 0,-1 -1,0 0)'

    gml = """<gml:ArcString><gml:posList srsDimension="2">-2 0 -1 -1 0 0 1 -1 2 0 0 2 -2 0</gml:posList></gml:ArcString>"""
    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'CIRCULARSTRING (-2 0,-1 -1,0 0,1 -1,2 0,0 2,-2 0)'

    gml2 = geom.ExportToGML(['FORMAT=GML3'])
    expected_gml2 = '<gml:Curve><gml:segments><gml:ArcString><gml:posList>-2 0 -1 -1 0 0 1 -1 2 0 0 2 -2 0</gml:posList></gml:ArcString></gml:segments></gml:Curve>'
    assert gml2 == expected_gml2

    geom2 = ogr.CreateGeometryFromGML(gml2)
    assert geom2.ExportToWkt() == 'CIRCULARSTRING (-2 0,-1 -1,0 0,1 -1,2 0,0 2,-2 0)'

###############################################################################
# Test OGRCompoundCurve


def test_gml_OGRCompoundCurve():

    wkt = 'COMPOUNDCURVE ((0 0,1 1,2 0))'
    geom = ogr.CreateGeometryFromWkt(wkt)
    gml = geom.ExportToGML(['FORMAT=GML3'])
    expected_gml = '<gml:CompositeCurve><gml:curveMember><gml:LineString><gml:posList>0 0 1 1 2 0</gml:posList></gml:LineString></gml:curveMember></gml:CompositeCurve>'
    assert gml == expected_gml

    # CompositeCurve of LineStringSegment
    gml = expected_gml
    geom = ogr.CreateGeometryFromGML(gml)
    # We simplify it in LINESTRING
    assert geom.ExportToWkt() == 'LINESTRING (0 0,1 1,2 0)'

    # CompositeCurve of Arc
    gml = """<gml:CompositeCurve><gml:curveMember><gml:Curve><gml:segments><gml:ArcString><gml:posList>0 0 1 1 2 0</gml:posList></gml:ArcString></gml:segments></gml:Curve></gml:curveMember></gml:CompositeCurve>"""
    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0))'

    gml2 = geom.ExportToGML(['FORMAT=GML3'])
    assert gml2 == gml

    # CompositeCurve of 3 arcs
    gml = """<gml:CompositeCurve>
    <gml:curveMember><gml:Curve><gml:segments><gml:Arc><gml:posList>0 0 1 1 2 0</gml:posList></gml:Arc></gml:segments></gml:Curve></gml:curveMember>
    <gml:curveMember><gml:Curve><gml:segments><gml:Arc><gml:posList>2 0 3 1 4 0</gml:posList></gml:Arc></gml:segments></gml:Curve></gml:curveMember>
    <gml:curveMember><gml:Curve><gml:segments><gml:Arc><gml:posList>4 0 5 1 6 0</gml:posList></gml:Arc></gml:segments></gml:Curve></gml:curveMember>
    </gml:CompositeCurve>"""
    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0),CIRCULARSTRING (2 0,3 1,4 0),CIRCULARSTRING (4 0,5 1,6 0))'

    # Alternative syntax : Curve with 3 Arc segments
    gml = """<gml:Curve><gml:segments>
            <gml:Arc><gml:posList>0 0 1 1 2 0</gml:posList></gml:Arc>
            <gml:Arc><gml:posList>2 0 3 1 4 0</gml:posList></gml:Arc>
            <gml:Arc><gml:posList>4 0 5 1 6 0</gml:posList></gml:Arc>
            </gml:segments></gml:Curve>"""
    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0),CIRCULARSTRING (2 0,3 1,4 0),CIRCULARSTRING (4 0,5 1,6 0))'

    # Curve with LineStringSegment and Arc segments
    gml = """<gml:Curve><gml:segments>
            <gml:Arc><gml:posList>0 0 1 0 0 0</gml:posList></gml:Arc>
            <gml:LineStringSegment><gml:posList>0 0 -1 0 0 0</gml:posList></gml:LineStringSegment>
            </gml:segments></gml:Curve>"""
    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0),(0 0,-1 0,0 0))'

    # Composite curve of a LineString and a (Composite) Curve with an Arc and a LineString
    gml = """<gml:CompositeCurve>
    <gml:curveMember><gml:Curve><gml:segments><gml:LineString><gml:posList>0 0 1 0 0 0</gml:posList></gml:LineString></gml:segments></gml:Curve></gml:curveMember>
    <gml:curveMember>
            <gml:Curve><gml:segments>
                <gml:Arc><gml:posList>0 0 1 0 0 0</gml:posList></gml:Arc>
                <gml:LineStringSegment><gml:posList>0 0 -1 0 0 0</gml:posList></gml:LineStringSegment>
            </gml:segments></gml:Curve>
    </gml:curveMember>
    </gml:CompositeCurve>"""
    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'COMPOUNDCURVE ((0 0,1 0,0 0),CIRCULARSTRING (0 0,1 0,0 0),(0 0,-1 0,0 0))'

###############################################################################
# Test OGRCurvePolygon


def test_gml_OGRCurvePolygon():

    # Test one CircularString
    gml = """<gml:Polygon><gml:exterior><gml:Ring><gml:curveMember><gml:Arc><gml:posList>0 0 1 0 0 0</gml:posList></gml:Arc></gml:curveMember></gml:Ring></gml:exterior></gml:Polygon>"""
    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0))'

    gml2 = geom.ExportToGML(['FORMAT=GML3'])
    expected_gml2 = '<gml:Polygon><gml:exterior><gml:Curve><gml:segments><gml:Circle><gml:posList>0 0 0.5 0.5 1 0</gml:posList></gml:Circle></gml:segments></gml:Curve></gml:exterior></gml:Polygon>'
    assert gml2 == expected_gml2

    # Test two CircularString
    gml = """<gml:Polygon><gml:exterior><gml:Ring><gml:curveMember><gml:Arc><gml:posList>0 0 1 0 0 0</gml:posList></gml:Arc></gml:curveMember></gml:Ring></gml:exterior><gml:interior><gml:Ring><gml:curveMember><gml:Arc><gml:posList>0.25 0 0.75 0 0.25 0</gml:posList></gml:Arc></gml:curveMember></gml:Ring></gml:interior></gml:Polygon>"""
    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0),CIRCULARSTRING (0.25 0.0,0.75 0.0,0.25 0.0))'

    # Test a LinearRing followed by a CircularString
    gml = """<gml:Polygon><gml:exterior><gml:LinearRing><gml:posList>-2 -2 -2 2 2 2 2 -2 -2 -2</gml:posList></gml:LinearRing></gml:exterior><gml:interior><gml:Ring><gml:curveMember><gml:Arc><gml:posList>0.25 0 0.75 0 0.25 0</gml:posList></gml:Arc></gml:curveMember></gml:Ring></gml:interior></gml:Polygon>"""
    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'CURVEPOLYGON ((-2 -2,-2 2,2 2,2 -2,-2 -2),CIRCULARSTRING (0.25 0.0,0.75 0.0,0.25 0.0))'

    # Test a CircularString followed by a LinearRing
    gml = """<gml:Polygon><gml:exterior><gml:Ring><gml:curveMember><gml:Circle><gml:posList>-1 0 1 2 3 0</gml:posList></gml:Circle></gml:curveMember></gml:Ring></gml:exterior><gml:interior><gml:LinearRing><gml:posList>-2 -2 -2 2 2 2 2 -2 -2 -2</gml:posList></gml:LinearRing></gml:interior></gml:Polygon>"""
    geom = ogr.CreateGeometryFromGML(gml)
    assert ogrtest.check_feature_geometry(geom, 'CURVEPOLYGON (CIRCULARSTRING (-1 0,1 2,3 0,1.0 -2.0,-1 0),(-2 -2,-2 2,2 2,2 -2,-2 -2))') == 0, \
        geom.ExportToWkt()

    gml2 = geom.ExportToGML(['FORMAT=GML3'])
    geom2 = ogr.CreateGeometryFromGML(gml)
    expected_gml2 = '<gml:Polygon><gml:exterior><gml:Curve><gml:segments><gml:ArcString><gml:posList>-1 0 1 2 3 0 1 -2 -1 0</gml:posList></gml:ArcString></gml:segments></gml:Curve></gml:exterior><gml:interior><gml:LineString><gml:posList>-2 -2 -2 2 2 2 2 -2 -2 -2</gml:posList></gml:LineString></gml:interior></gml:Polygon>'
    expected_geom2 = ogr.CreateGeometryFromGML(expected_gml2)
    assert ogrtest.check_feature_geometry(geom2, expected_geom2) == 0, gml2

###############################################################################
# Test OGRMultiSurface


def test_gml_OGRMultiSurface():

    # MultiSurface of CurvePolygon
    gml = """<gml:MultiSurface>
    <gml:surfaceMember>
        <gml:Surface>
            <gml:patches>
                <gml:PolygonPatch>
                    <gml:exterior>
                        <gml:Ring><gml:curveMember><gml:Curve><gml:segments><gml:Circle><gml:posList>0 0 1 1 1 -1</gml:posList></gml:Circle></gml:segments></gml:Curve></gml:curveMember></gml:Ring>
                    </gml:exterior>
                </gml:PolygonPatch>
            </gml:patches>
        </gml:Surface>
    </gml:surfaceMember>
</gml:MultiSurface>"""

    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'MULTISURFACE (CURVEPOLYGON (CIRCULARSTRING (0 0,1 1,1 -1,0.292893218813453 -0.707106781186548,0 0)))', \
        '<gml:MultiSurface> not correctly parsed'

    # MultiSurface of Polygon and CurvePolygon
    gml = """<gml:MultiSurface>
    <gml:surfaceMember>
        <gml:Polygon>
            <gml:exterior>
                <gml:LinearRing><gml:posList>0 0 0 1 1 1 0 0</gml:posList></gml:LinearRing>
            </gml:exterior>
        </gml:Polygon>
    </gml:surfaceMember>
    <gml:surfaceMember>
        <gml:Polygon>
            <gml:exterior>
                <gml:Ring><gml:curveMember><gml:Curve><gml:segments><gml:Circle><gml:posList>0 0 1 1 1 -1</gml:posList></gml:Circle></gml:segments></gml:Curve></gml:curveMember></gml:Ring>
            </gml:exterior>
        </gml:Polygon>
    </gml:surfaceMember>
</gml:MultiSurface>"""

    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'MULTISURFACE (((0 0,0 1,1 1,0 0)),CURVEPOLYGON (CIRCULARSTRING (0 0,1 1,1 -1,0.292893218813453 -0.707106781186548,0 0)))', \
        '<gml:MultiSurface> not correctly parsed'

    # MultiSurface of CurvePolygon and Polygon
    gml = """<gml:MultiSurface>
    <gml:surfaceMember>
        <gml:Polygon>
            <gml:exterior>
                <gml:Ring><gml:curveMember><gml:Curve><gml:segments><gml:Circle><gml:posList>0 0 1 1 1 -1</gml:posList></gml:Circle></gml:segments></gml:Curve></gml:curveMember></gml:Ring>
            </gml:exterior>
        </gml:Polygon>
    </gml:surfaceMember>
    <gml:surfaceMember>
        <gml:Polygon>
            <gml:exterior>
                <gml:LinearRing><gml:posList>0 0 0 1 1 1 0 0</gml:posList></gml:LinearRing>
            </gml:exterior>
        </gml:Polygon>
    </gml:surfaceMember>
</gml:MultiSurface>"""

    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'MULTISURFACE (CURVEPOLYGON (CIRCULARSTRING (0 0,1 1,1 -1,0.292893218813453 -0.707106781186548,0 0)),((0 0,0 1,1 1,0 0)))', \
        '<gml:MultiSurface> not correctly parsed'

    geom = ogr.CreateGeometryFromWkt('MULTISURFACE (CURVEPOLYGON((0 0,0 1,1 1,1 0,0 0)))')
    gml2 = geom.ExportToGML(['FORMAT=GML3'])
    expected_gml2 = '<gml:MultiSurface><gml:surfaceMember><gml:Polygon><gml:exterior><gml:LinearRing><gml:posList>0 0 0 1 1 1 1 0 0 0</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon></gml:surfaceMember></gml:MultiSurface>'
    assert gml2 == expected_gml2

###############################################################################
# Test OGRMultiCurve


def test_gml_OGRMultiCurve():

    # MultiCurve of Arc
    gml = """<gml:MultiCurve><gml:curveMember><gml:Curve><gml:segments><gml:ArcString><gml:posList>0 0 1 1 1 -1</gml:posList></gml:ArcString></gml:segments></gml:Curve></gml:curveMember></gml:MultiCurve>"""

    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'MULTICURVE (CIRCULARSTRING (0 0,1 1,1 -1))', \
        '<gml:MultiCurve> not correctly parsed'

    gml2 = geom.ExportToGML(['FORMAT=GML3'])
    assert gml2 == gml

    # MultiCurve of LineString and Arc
    gml = """<gml:MultiCurve>
    <gml:curveMember>
        <gml:LineString><gml:posList>0 0 1 1 1 -1</gml:posList></gml:LineString>
    </gml:curveMember>
    <gml:curveMember>
        <gml:Curve>
            <gml:segments><gml:Arc><gml:posList>0 0 1 1 1 -1</gml:posList></gml:Arc></gml:segments>
        </gml:Curve>
    </gml:curveMember>
</gml:MultiCurve>"""

    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'MULTICURVE ((0 0,1 1,1 -1),CIRCULARSTRING (0 0,1 1,1 -1))', \
        '<gml:MultiCurve> not correctly parsed'

    # MultiCurve of Arc and LineString
    gml = """<gml:MultiCurve>
    <gml:curveMember>
        <gml:Curve>
            <gml:segments><gml:Arc><gml:posList>0 0 1 1 1 -1</gml:posList></gml:Arc></gml:segments>
        </gml:Curve>
    </gml:curveMember>
    <gml:curveMember>
        <gml:LineString><gml:posList>0 0 1 1 1 -1</gml:posList></gml:LineString>
    </gml:curveMember>
</gml:MultiCurve>"""

    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'MULTICURVE (CIRCULARSTRING (0 0,1 1,1 -1),(0 0,1 1,1 -1))', \
        '<gml:MultiCurve> not correctly parsed'

    # MultiCurve of CompositeCurve
    gml = """<gml:MultiCurve>
    <gml:curveMember>
        <gml:CompositeCurve>
            <gml:curveMember>
                <gml:Curve>
                    <gml:segments><gml:Arc><gml:posList>0 0 1 1 1 -1</gml:posList></gml:Arc></gml:segments>
                </gml:Curve>
            </gml:curveMember>
            <gml:curveMember>
                <gml:LineString><gml:posList>1 -1 1 1 1 -1</gml:posList></gml:LineString>
            </gml:curveMember>
        </gml:CompositeCurve>
    </gml:curveMember>
</gml:MultiCurve>"""

    geom = ogr.CreateGeometryFromGML(gml)
    assert geom.ExportToWkt() == 'MULTICURVE (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 -1),(1 -1,1 1,1 -1)))', \
        '<gml:MultiCurve> not correctly parsed'

###############################################################################
# Test write support for GML namespace declaration


def test_gml_write_gml_ns():

    geom = ogr.CreateGeometryFromWkt('POINT(500000 4500000)')
    gml = geom.ExportToGML(options=['NAMESPACE_DECL=YES'])
    expected_gml = '<gml:Point xmlns:gml="http://www.opengis.net/gml"><gml:coordinates>500000,4500000</gml:coordinates></gml:Point>'
    assert gml == expected_gml, ('got %s, instead of %s' % (gml, expected_gml))

    geom = ogr.CreateGeometryFromWkt('POINT(500000 4500000)')
    gml = geom.ExportToGML(options=['FORMAT=GML3', 'NAMESPACE_DECL=YES'])
    expected_gml = '<gml:Point xmlns:gml="http://www.opengis.net/gml"><gml:pos>500000 4500000</gml:pos></gml:Point>'
    assert gml == expected_gml, ('got %s, instead of %s' % (gml, expected_gml))

    geom = ogr.CreateGeometryFromWkt('POINT(500000 4500000)')
    gml = geom.ExportToGML(options=['FORMAT=GML32', 'GMLID=foo', 'NAMESPACE_DECL=YES'])
    expected_gml = '<gml:Point xmlns:gml="http://www.opengis.net/gml/3.2" gml:id="foo"><gml:pos>500000 4500000</gml:pos></gml:Point>'
    assert gml == expected_gml, ('got %s, instead of %s' % (gml, expected_gml))

###############################################################################
# Test reading geometry from https://github.com/OSGeo/gdal/issues/4155


def test_gml_read_gml_ArcByCenterPoint_projected_crs_northing_easting():

    g = ogr.CreateGeometryFromGML("""<gml:Surface srsName="EPSG:2326" srsDimension="2">
    <gml:patches>
    <gml:PolygonPatch>
    <gml:exterior>
    <gml:Ring>
    <gml:curveMember>
    <gml:Curve>
    <gml:segments>
    <gml:LineStringSegment>
    <gml:posList>821502.753690919 838825.332031005 821194.727830006 839043.611480001</gml:posList>
    </gml:LineStringSegment>
    <gml:ArcByCenterPoint numArc="1">
    <gml:posList>821194.396688321 839052.616490606</gml:posList>
    <gml:radius uom="EPSG:2326">9.01109709191771</gml:radius>
    <gml:startAngle uom="degree">177.894008505116</gml:startAngle>
    <gml:endAngle uom="degree">250.98396509322</gml:endAngle>
    </gml:ArcByCenterPoint>
    <gml:LineStringSegment>
    <gml:posList>821185.877350006 839049.680380003 821502.753690919 838825.332031005</gml:posList>
    </gml:LineStringSegment>
    </gml:segments>
    </gml:Curve>
    </gml:curveMember>
    </gml:Ring>
    </gml:exterior>
    </gml:PolygonPatch>
    </gml:patches>
    </gml:Surface>""")
    assert g is not None
