#!/usr/bin/env python
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
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal

###############################################################################

class gml_geom_unit:
    def __init__(self, unit):
        self.unit = unit

    def gml_geom( self ):
        raw_wkt = open('data/wkb_wkt/' + self.unit + '.wkt').read()

        ######################################################################
        # Convert WKT to GML.

        geom_wkt = ogr.CreateGeometryFromWkt( raw_wkt )

        gml = geom_wkt.ExportToGML()

        if gml is None or len(gml) == 0:
            gdaltest.post_reason( 'Conversion to GML failed.')
            return 'fail'

        ######################################################################
        # Create geometry from GML. 
        
        geom_gml = ogr.CreateGeometryFromGML( gml )

        if ogrtest.check_feature_geometry(geom_wkt, geom_gml, 0.0000000000001) == 1:
            clean_wkt = geom_wkt.ExportToWkt();
            gml_wkt = geom_gml.ExportToWkt()
            gdaltest.post_reason( 'WKT from GML (%s) does not match clean WKT (%s).\ngml was (%s)' % (gml_wkt, clean_wkt, gml) )
            return 'fail'

        geom_wkt.Destroy()
        geom_gml.Destroy()

        return 'success'
        
###############################################################################
# Test geometries with extra spaces at the end, as sometimes are generated
# by ESRI WFS software.

def gml_space_test():
    gml = '<gml:LineString xmlns:foo="http://bar"><gml:coordinates xmlns:foo="http://bar" decimal="." cs="," ts=" ">189999.99995605,624999.99998375 200000.00005735,624999.99998375 200000.00005735,612499.99997125 195791.3593843,612499.99997125 193327.3749823,612499.99997125 189999.99995605,612499.99997125 189999.99995605,619462.31247125 189999.99995605,624999.99998375 \n</gml:coordinates></gml:LineString>'
    geom = ogr.CreateGeometryFromGML( gml )
    if geom is None or geom.GetGeometryType() is not ogr.wkbLineString \
       or geom.GetPointCount() != 8:
        gdaltest.post_reason( 'GML not correctly parsed' )
        return 'fail'

    geom.Destroy()

    return 'success'

###############################################################################
# Test GML 3.x "pos" element for a point.

def gml_pos_point():

    gml = '<gml:Point xmlns:foo="http://bar"><gml:pos>31 29 16</gml:pos></gml:Point>'

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'POINT (31 29 16)':
        gdaltest.post_reason( '<gml:pos> not correctly parsed' )
        return 'fail'

    return 'success'

###############################################################################
# Test GML 3.1.1 "pos" element for a polygon. (ticket #3244)

def gml_pos_polygon():

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

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'POLYGON ((0 0,4 0,4 4,0 4,0 0),(1 1,2 1,2 2,1 2,1 1))':
        gdaltest.post_reason( '<gml:Polygon> not correctly parsed' )
        return 'fail'

    return 'success'
    
###############################################################################
# Test GML 3.x "posList" element for a linestring.

def gml_posList_line():

    gml = '<LineString xmlns:foo="http://bar"><posList xmlns:foo="http://bar">31 42 53 64 55 76</posList></LineString>'

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'LINESTRING (31 42,53 64,55 76)':
        gdaltest.post_reason( '<gml:posList> not correctly parsed' )
        return 'fail'

    return 'success'


###############################################################################
# Test GML 3.x "posList" element for a 3D linestring.

def gml_posList_line3d():

    gml = '<LineString><posList xmlns:foo="http://bar" srsDimension="3">31 42 1 53 64 2 55 76 3</posList></LineString>'

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'LINESTRING (31 42 1,53 64 2,55 76 3)':
        gdaltest.post_reason( '<gml:posList> not correctly parsed' )
        return 'fail'

    return 'success'

###############################################################################
# Test GML 3.x "posList" element for a 3D linestring, but with srsDimension
# set on LineString, not posList

def gml_posList_line3d_2():

    gml = '<LineString srsDimension="3"><posList>31 42 1 53 64 2 55 76 3</posList></LineString>'

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'LINESTRING (31 42 1,53 64 2,55 76 3)':
        gdaltest.post_reason( '<gml:posList> not correctly parsed' )
        return 'fail'

    return 'success'
    
###############################################################################
# Test GML 3.x "polygon" element for a point.

def gml_polygon():

    gml = '<Polygon><exterior><LinearRing><posList>0 0 4 0 4 4 0 4 0 0</posList></LinearRing></exterior><interior xmlns:foo="http://bar"><LinearRing><posList xmlns:foo="http://bar">1 1 2 1 2 2 1 2 1 1</posList></LinearRing></interior></Polygon>'
    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'POLYGON ((0 0,4 0,4 4,0 4,0 0),(1 1,2 1,2 2,1 2,1 1))':
        gdaltest.post_reason( '<gml:Polygon> not correctly parsed' )
        return 'fail'

    return 'success'

###############################################################################
# Private utility function to conver WKT to GML with assigned WGS 84 as SRS

def _CreateGMLWithSRSFromWkt(wkt, epsg):

    geom = ogr.CreateGeometryFromWkt( wkt )

    if geom is None:
        gdaltest.post_reason( 'Import geometry from WKT failed' )
        return None

    # Assign SRS from given EPSG code
    srs = osr.SpatialReference()
    srs.ImportFromEPSG( epsg )

    if srs is None:
        gdaltest.post_reason( 'SRS import from EPSG failed' )
        return None

    geom.AssignSpatialReference( srs )

    return geom.ExportToGML()

###############################################################################
# Test of Point geometry with SRS assigned

def gml_out_point_srs():

    wkt = 'POINT(21.675 53.763)'

    gml = _CreateGMLWithSRSFromWkt( wkt, 4326 )

    if gml is None or len(gml) == 0:
        gdaltest.post_reason( 'Conversion to GML failed.')
        return 'fail'
    
    if gml[0:31] != '<gml:Point srsName="EPSG:4326">':
        gdaltest.post_reason( 'No srsName attribute in GML output')
        return 'fail'

    return 'success'

###############################################################################
# Test of Point 3D geometry with SRS assigned

def gml_out_point3d_srs():

    wkt = 'POINT(21.675 53.763 100)'

    gml = _CreateGMLWithSRSFromWkt( wkt, 4326 )

    if gml is None or len(gml) == 0:
        gdaltest.post_reason( 'Conversion to GML failed.')
        return 'fail'
    
    if gml[0:31] != '<gml:Point srsName="EPSG:4326">':
        gdaltest.post_reason( 'No srsName attribute in GML output')
        return 'fail'

    return 'success'

###############################################################################
# Test of LineString geometry with SRS assigned

def gml_out_linestring_srs():
 
    wkt = open('data/wkb_wkt/5.wkt').read()

    gml = _CreateGMLWithSRSFromWkt( wkt, 4326 )

    if gml is None or len(gml) == 0:
        gdaltest.post_reason( 'Conversion to GML failed.')
        return 'fail'
    
    if gml[0:36] != '<gml:LineString srsName="EPSG:4326">':
        gdaltest.post_reason( 'No srsName attribute in GML output')
        return 'fail'

    return 'success'

###############################################################################
# Test of Polygon geometry with SRS assigned

def gml_out_polygon_srs():
 
    wkt = open('data/wkb_wkt/6.wkt').read()

    gml = _CreateGMLWithSRSFromWkt( wkt, 4326 )

    if gml is None or len(gml) == 0:
        gdaltest.post_reason( 'Conversion to GML failed.')
        return 'fail'

    if gml[0:33] != '<gml:Polygon srsName="EPSG:4326">':
        gdaltest.post_reason( 'No srsName attribute in GML output')
        return 'fail'

    return 'success'

###############################################################################
# Test of MultiPoint geometry with SRS assigned

def gml_out_multipoint_srs():
 
    wkt = open('data/wkb_wkt/11.wkt').read()

    gml = _CreateGMLWithSRSFromWkt( wkt, 4326 )

    if gml is None or len(gml) == 0:
        gdaltest.post_reason( 'Conversion to GML failed.')
        return 'fail'

    if gml[0:36] != '<gml:MultiPoint srsName="EPSG:4326">':
        gdaltest.post_reason( 'No srsName attribute in GML output')
        return 'fail'

    return 'success'

###############################################################################
# Test of MultiLineString geometry with SRS assigned

def gml_out_multilinestring_srs():
 
    wkt = open('data/wkb_wkt/2.wkt').read()

    gml = _CreateGMLWithSRSFromWkt( wkt, 4326 )

    if gml is None or len(gml) == 0:
        gdaltest.post_reason( 'Conversion to GML failed.')
        return 'fail'

    if gml[0:41] != '<gml:MultiLineString srsName="EPSG:4326">':
        gdaltest.post_reason( 'No srsName attribute in GML output')
        return 'fail'

    return 'success'

###############################################################################
# Test of MultiPolygon geometry with SRS assigned

def gml_out_multipolygon_srs():
 
    wkt = open('data/wkb_wkt/4.wkt').read()

    gml = _CreateGMLWithSRSFromWkt( wkt, 4326 )

    if gml is None or len(gml) == 0:
        gdaltest.post_reason( 'Conversion to GML failed.')
        return 'fail'

    if gml[0:38] != '<gml:MultiPolygon srsName="EPSG:4326">':
        gdaltest.post_reason( 'No srsName attribute in GML output')
        print(gml)
        return 'fail'

    # Verify we have no other srsName's on subelements.
    if gml[39:].find('srsName') != -1:
        gdaltest.post_reason( 'Got extra srsName attributes on subelements.' )
        print(gml)
        return 'fail'

    return 'success'

###############################################################################
# Test of GeometryCollection with SRS assigned

def gml_out_geometrycollection_srs():
 
    wkt = open('data/wkb_wkt/3.wkt').read()

    gml = _CreateGMLWithSRSFromWkt( wkt, 4326 )

    if gml is None or len(gml) == 0:
        gdaltest.post_reason( 'Conversion to GML failed.')
        return 'fail'

    if gml[0:39] != '<gml:MultiGeometry srsName="EPSG:4326">':
        gdaltest.post_reason( 'No srsName attribute in GML output')
        print(gml)
        return 'fail'

    return 'success'

###############################################################################
# Test GML Box

def gml_Box():

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

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'POLYGON ((1 2 0,3 2 0,3 4 0,1 4 0,1 2 0))':
        gdaltest.post_reason( '<gml:Box> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test GML Envelope

def gml_Envelope():

    gml = """<gml:Envelope xmlns:gml="http://www.opengis.net/gml" srsName="foo">
    <gml:lowerCorner>1 2</gml:lowerCorner>
    <gml:upperCorner>3 4</gml:upperCorner>
</gml:Envelope>"""

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'POLYGON ((1 2,3 2,3 4,1 4,1 2))':
        gdaltest.post_reason( '<gml:Envelope> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test GML Curve

def gml_Curve():

    gml = """<gml:Curve xmlns:gml="http://www.opengis.net/gml" srsName="foo">
    <gml:segments xmlns:foo="http://bar">
        <gml:LineStringSegment xmlns:foo="http://bar">
            <gml:posList>1 2 3 4</gml:posList>
        </gml:LineStringSegment>
    </gml:segments>
</gml:Curve>"""

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'LINESTRING (1 2,3 4)':
        gdaltest.post_reason( '<gml:Curve> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test GML Curve with pointProperty elements

def gml_Curve_with_pointProperty():

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

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'LINESTRING (1 2,3 4)':
        gdaltest.post_reason( '<gml:Curve> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'
    
###############################################################################
# Test GML MultiCurve

def gml_MultiCurve():

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

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'MULTILINESTRING ((1 2,2 3),(3 4,4 5))':
        gdaltest.post_reason( '<gml:MultiCurve> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test GML MultiSurface with PolygonPatch

def gml_MultiSurface():

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

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'MULTIPOLYGON (((1 2,3 4,5 6,1 2),(2 3,4 5,6 7,2 3),(3 4,5 6,7 8,3 4)),((4 5,6 7,8 9,4 5)))':
        gdaltest.post_reason( '<gml:MultiSurface> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'


###############################################################################
# Test GML MultiSurface with surfaceMembers

def gml_MultiSurface_surfaceMembers():

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

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'MULTIPOLYGON (((1 2,3 4,5 6,1 2),(2 3,4 5,6 7,2 3)),((3 4,5 6,7 8,3 4)),((30 40,50 60,70 80,30 40)))':
        gdaltest.post_reason( '<gml:MultiSurface> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'


###############################################################################
# Test GML MultiCurve with curveMembers

def gml_MultiCurve_curveMembers():

    gml = """<gml:MultiCurve xmlns:foo="http://bar">
          <gml:curveMembers xmlns:foo="http://bar">
            <gml:LineString xmlns:foo="http://bar">
                <gml:posList xmlns:foo="http://bar" srsDimension="2">0 0 1 1</gml:posList>
              </gml:LineString>
            </gml:curveMembers>
          </gml:MultiCurve>"""

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'MULTILINESTRING ((0 0,1 1))':
        gdaltest.post_reason( '<gml:MultiCurve> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test GML MultiPoint with pointMembers

def gml_MultiCurve_pointMembers():

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

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'MULTIPOINT (0 0,1 1)':
        gdaltest.post_reason( '<gml:MultiPoint> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'
###############################################################################
# Test GML Solid

def gml_Solid():

    gml = """<gml:Solid xmlns:foo="http://bar" gml:id="UUID_cc5a9513-2d85-4f1a-869a-620400182e1f">
          <gml:exterior xmlns:foo="http://bar">
            <gml:CompositeSurface xmlns:foo="http://bar" gml:id="UUID_2c83341e-a9ce-4abe-9c40-b5208eed5588">
              <gml:surfaceMember xmlns:foo="http://bar">
                <gml:Polygon xmlns:foo="http://bar" gml:id="UUID_d8e4b04b-ce0a-441e-b940-5ab99fcf6112">
                  <gml:exterior xmlns:foo="http://bar">
                    <gml:LinearRing xmlns:foo="http://bar" gml:id="UUID_d8e4b04b-ce0a-441e-b940-5ab99fcf6112_0">
                      <gml:posList xmlns:foo="http://bar" srsDimension="3">1 2 0 3 4 0 5 6 0 1 2 0</gml:posList>
                    </gml:LinearRing>
                  </gml:exterior>
                </gml:Polygon>
              </gml:surfaceMember>
            </gml:CompositeSurface>
          </gml:exterior>
        </gml:Solid>"""

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'MULTIPOLYGON (((1 2 0,3 4 0,5 6 0,1 2 0)))':
        gdaltest.post_reason( '<gml:Solid> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test GML OrientableSurface

def gml_OrientableSurface():

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

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'POLYGON ((-213.475 24.989 0,-213.475 24.989 8,-215.704 25.077 8,-215.704 25.077 0,-213.475 24.989 0))':
        gdaltest.post_reason( '<gml:OrientableSurface> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test GML Triangle

def gml_Triangle():

    gml = """<gml:Triangle xmlns:foo="http://bar">
                <gml:exterior>
                    <gml:LinearRing>
                        <gml:posList>0 0 0 1 1 1 0 0</gml:posList>
                     </gml:LinearRing>
                </gml:exterior>
             </gml:Triangle>"""

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,0 0))':
        gdaltest.post_reason( '<gml:Triangle> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test GML Rectangle

def gml_Rectangle():

    gml = """<gml:Rectangle xmlns:foo="http://bar">
                <gml:exterior>
                    <gml:LinearRing>
                        <gml:posList>0 0 0 1 1 1 1 0 0 0</gml:posList>
                     </gml:LinearRing>
                </gml:exterior>
             </gml:Rectangle>"""

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        gdaltest.post_reason( '<gml:Rectangle> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test GML Tin

def gml_Tin():

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

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'POLYGON ((0 0 1,0 1 1,1 1 1,1 0 1,0 0 1))':
        gdaltest.post_reason( '<gml:Tin> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test GML Arc

def gml_Arc():

    poslist_list = [
("1 0 0 1 -1 0", 'LINESTRING (1 0,0.707106781186548 0.707106781186547,0.0 1.0,-0.707106781186547 0.707106781186548,-1.0 0.0)' ),
("1 0 0 -1 -1 0", 'LINESTRING (1 0,0.707106781186548 -0.707106781186547,0.0 -1.0,-0.707106781186547 -0.707106781186548,-1.0 -0.0)' ),
("1 0 -1 0 0 -1 ", 'LINESTRING (1 0,0.707106781186548 0.707106781186547,0.0 1.0,-0.707106781186547 0.707106781186548,-1.0 0.0,-0.707106781186548 -0.707106781186547,-0.0 -1.0)' ),
("1 0 -1 0 0 1 ", 'LINESTRING (1 0,0.707106781186548 -0.707106781186547,0.0 -1.0,-0.707106781186547 -0.707106781186548,-1.0 -0.0,-0.707106781186548 0.707106781186547,-0.0 1.0)' ),
("1 0  0 0 -1 0 ", 'LINESTRING (1 0,0 0,-1 0)' ),
("0 1 1 0 0 -1", 'LINESTRING (0.0 1.0,0.707106781186548 0.707106781186547,1 0,0.707106781186548 -0.707106781186547,0.0 -1.0)' ),
("0 1 -1 0 0 -1", 'LINESTRING (0.0 1.0,-0.707106781186547 0.707106781186548,-1.0 0.0,-0.707106781186548 -0.707106781186547,-0.0 -1.0)' ),
("-1 0 0 1 1 0", 'LINESTRING (-1.0 0.0,-0.707106781186547 0.707106781186548,0.0 1.0,0.707106781186548 0.707106781186547,1 0)' ),
("-1 0 0 1 -0.707106781186547 -0.707106781186548", 'LINESTRING (-1 0,-0.707106781186547 0.707106781186548,0 1,0.923879532511287 0.38268343236509,0.923879532511287 -0.38268343236509,0.38268343236509 -0.923879532511287,-0.707106781186547 -0.707106781186548)' )
    ]

    for (poslist, expected_wkt) in poslist_list:

        gml = "<gml:Arc><gml:posList>%s</gml:posList></gml:Arc>" % poslist
        gdal.SetConfigOption('OGR_ARC_STEPSIZE','45')
        geom = ogr.CreateGeometryFromGML( gml )
        gdal.SetConfigOption('OGR_ARC_STEPSIZE',None)

        if ogrtest.check_feature_geometry(geom, ogr.CreateGeometryFromWkt(expected_wkt)) != 0:
            print(gml)
            print(geom)
            return 'fail'

    return 'success'

###############################################################################
# Test GML Circle

def gml_Circle():

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

    gdal.SetConfigOption('OGR_ARC_STEPSIZE','45')
    geom = ogr.CreateGeometryFromGML( gml )
    gdal.SetConfigOption('OGR_ARC_STEPSIZE',None)

    expected_wkt = 'POLYGON ((-1 0,-0.707106781186547 0.707106781186548,0 1,0.923879532511287 0.38268343236509,0.923879532511287 -0.38268343236509,0.38268343236509 -0.923879532511287,-0.707106781186547 -0.707106781186548,-1.0 -0.0,-1 0))'
    if ogrtest.check_feature_geometry(geom, ogr.CreateGeometryFromWkt(expected_wkt)) != 0:
        print(geom)
        return 'fail'

    return 'success'

###############################################################################
# Test concatenated sections (#4451)

def gml_ConcatenatedDeduplication():

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

    gdal.SetConfigOption('OGR_ARC_STEPSIZE','45')
    geom = ogr.CreateGeometryFromGML( gml )
    gdal.SetConfigOption('OGR_ARC_STEPSIZE',None)

    expected_wkt = 'POLYGON ((0 -1,0 1,0.707106781186548 0.707106781186547,1 0,0.707106781186548 -0.707106781186547,0.0 -1.0))'
    if ogrtest.check_feature_geometry(geom, ogr.CreateGeometryFromWkt(expected_wkt)) != 0:
        print(geom)
        return 'fail'

    if ogrtest.have_geos() and not geom.IsValid():
        gdaltest.post_reason( 'geometry not valid' )
        return 'fail'

    return 'success'

###############################################################################
# Test OGRFormatDouble() to check for rounding errors (would also apply for KML output, or ogrinfo output)

def gml_out_precision():
    
    geom = ogr.CreateGeometryFromWkt('POINT(93538.15 1.23456789)')
    expected_gml = '<gml:Point><gml:coordinates>93538.15,1.23456789</gml:coordinates></gml:Point>'
    got_gml = geom.ExportToGML()
    if got_gml != expected_gml:
        gdaltest.post_reason('did not get expected gml')
        print(got_gml)
        return 'fail'

    geom = ogr.CreateGeometryFromWkt('POINT(93538.55 1234567.89)')
    expected_gml = '<gml:Point><gml:coordinates>93538.55,1234567.89</gml:coordinates></gml:Point>'
    got_gml = geom.ExportToGML()
    if got_gml != expected_gml:
        gdaltest.post_reason('did not get expected gml')
        print(got_gml)
        return 'fail'

    return 'success'

###############################################################################
# Test various error cases of gml2ogrgeometry.cpp

def gml_invalid_geoms():

    gml_expected_wkt_list = [
        ('<foo/>', None),
        ('<gml:Point><gml:pos>31 29 16</gml:pos><gml:pos>31 29 16</gml:pos></gml:Point>', None),
        ('<gml:Point><gml:coordinates/></gml:Point>', 'POINT EMPTY'), # This is valid GML actually
        ('<gml:Point><gml:coordinates>0</gml:coordinates></gml:Point>', None),
        ('<gml:Point><gml:coordinates>0 1</gml:coordinates></gml:Point>', 'POINT (0 1)'), # Support for uncommon formatting of coordinates
        ('<gml:Point><gml:coordinates>0 1 2</gml:coordinates></gml:Point>', 'POINT (0 1 2)'), # Support for uncommon formatting of coordinates
        ('<gml:Point><gml:coordinates>0,1 2,3</gml:coordinates></gml:Point>', None),
        ('<gml:Point><gml:pos>0</gml:pos></gml:Point>', None),
        ('<gml:Point><gml:pos/></gml:Point>', 'POINT EMPTY'), # This is valid GML actually
        ('<gml:Point/>', None),
        ('<gml:Point><foo/></gml:Point>', None),
        ('<gml:LineString/>', None),
        ('<gml:LineString><foo/></gml:LineString>', None),
        ('<gml:LineString><gml:posList></gml:posList></gml:LineString>', 'LINESTRING EMPTY'), # This is valid GML actually
        ('<gml:LineString><gml:posList>0</gml:posList></gml:LineString>', None),
        ('<gml:LineString><gml:posList srsDimension="4">0 1 2 3</gml:posList></gml:LineString>', None),
        ('<gml:LineString><gml:posList srsDimension="3">0 1 2 3</gml:posList></gml:LineString>', None),
        ('<gml:Point><gml:coord></gml:coord></gml:Point>', None),
        ('<gml:Point><gml:coord><gml:X/><gml:Y/></gml:coord></gml:Point>', None),
        ('<gml:Point><gml:coord><gml:X>0</gml:X></gml:coord></gml:Point>', None),
        ('<gml:Polygon/>', 'POLYGON EMPTY'), # valid GML3, but invalid GML2. Be tolerant
        ('<gml:Polygon><gml:outerBoundaryIs/></gml:Polygon>', 'POLYGON EMPTY'), # valid GML2
        ('<gml:Polygon><gml:outerBoundaryIs><foo/></gml:outerBoundaryIs></gml:Polygon>', None),
        ('<gml:Polygon><gml:outerBoundaryIs><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:outerBoundaryIs></gml:Polygon>', None),
        ('<gml:Polygon><gml:outerBoundaryIs><gml:LinearRing><gml:posList>0 1 2 3 4 5 0 1</gml:posList></gml:LinearRing></gml:outerBoundaryIs><gml:innerBoundaryIs/></gml:Polygon>', None),
        ('<gml:Polygon><gml:outerBoundaryIs><gml:LinearRing/></gml:outerBoundaryIs><gml:innerBoundaryIs/></gml:Polygon>', None),
        ('<gml:Polygon><gml:outerBoundaryIs><gml:LinearRing><gml:posList>0 1 2 3 4 5 0 1</gml:posList></gml:LinearRing></gml:outerBoundaryIs><gml:innerBoundaryIs><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:innerBoundaryIs></gml:Polygon>', None),
        ('<gml:Ring/>', 'LINEARRING EMPTY'), # Probably illegal GML
        ('<gml:Ring><foo/></gml:Ring>', 'LINEARRING EMPTY'), # Probably illegal GML
        ('<gml:Ring><gml:curveMember/></gml:Ring>', None),
        ('<gml:Ring><gml:curveMember><foo/></gml:curveMember></gml:Ring>', None),
        ('<gml:Ring><gml:curveMember><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:curveMember></gml:Ring>', None),
        ('<gml:Box/>', None),
        ('<gml:Box><gml:pos>31 29 16</gml:pos></gml:Box>', None),
        ('<gml:MultiPolygon/>', 'MULTIPOLYGON EMPTY'), # valid GML3, but invalid GML2. Be tolerant
        ('<gml:MultiPolygon><foo/></gml:MultiPolygon>', 'MULTIPOLYGON EMPTY'), # illegal GML, but we are tolerant
        ('<gml:MultiPolygon><gml:polygonMember/></gml:MultiPolygon>', None),
        ('<gml:MultiPolygon><gml:polygonMember><foo/></gml:polygonMember></gml:MultiPolygon>', None),
        ('<gml:MultiPolygon><gml:polygonMember><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:polygonMember></gml:MultiPolygon>', None),
        ('<gml:MultiSurface><gml:surfaceMembers/></gml:MultiSurface>', 'MULTIPOLYGON EMPTY'), # valid GML
        ('<gml:MultiSurface><gml:surfaceMembers><foo/></gml:surfaceMembers></gml:MultiSurface>', 'MULTIPOLYGON EMPTY'), # illegal GML, but we are tolerant
        ('<gml:MultiSurface><gml:surfaceMembers><gml:Polygon/></gml:surfaceMembers></gml:MultiSurface>', 'MULTIPOLYGON EMPTY'), # valid GML3
        ('<gml:MultiPoint/>', 'MULTIPOINT EMPTY'),
        ('<gml:MultiPoint><foo/></gml:MultiPoint>', 'MULTIPOINT EMPTY'),
        ('<gml:MultiPoint><gml:pointMember/></gml:MultiPoint>', None),
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
        ('<gml:MultiCurve><gml:curveMember/></gml:MultiCurve>', None),
        ('<gml:MultiCurve><gml:curveMember><foo/></gml:curveMember></gml:MultiCurve>', None),
        ('<gml:MultiCurve><gml:curveMember><gml:Curve/></gml:curveMember></gml:MultiCurve>', None),
        ('<gml:MultiCurve><gml:curveMember><gml:Curve><foo/></gml:Curve></gml:curveMember></gml:MultiCurve>', None),
        ('<gml:MultiCurve><gml:curveMember><gml:Curve><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:Curve></gml:curveMember></gml:MultiCurve>', None),
        ('<gml:MultiCurve><gml:curveMembers></gml:curveMembers></gml:MultiCurve>', 'MULTILINESTRING EMPTY'),
        ('<gml:MultiCurve><gml:curveMembers><foo/></gml:curveMembers></gml:MultiCurve>', 'MULTILINESTRING EMPTY'),
        ('<gml:MultiCurve><gml:curveMembers><gml:LineString/></gml:curveMembers></gml:MultiCurve>', None),
        ('<gml:Curve/>', None),
        ('<gml:Curve><foo/></gml:Curve>', None),
        ('<gml:Curve><gml:segments/></gml:Curve>', 'LINESTRING EMPTY'),
        ('<gml:Curve><gml:segments><foo/></gml:segments></gml:Curve>', 'LINESTRING EMPTY'),
        ('<gml:Curve><gml:segments><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:segments></gml:Curve>', 'LINESTRING EMPTY'),
        ('<gml:Arc/>', None),
        ('<gml:Arc><gml:posList>0 0 0 1</gml:posList></gml:Arc>', None),
        ('<gml:segments/>', 'LINESTRING EMPTY'),
        ('<gml:segments><foo/></gml:segments>', 'LINESTRING EMPTY'),
        ('<gml:segments><gml:LineStringSegment/></gml:segments>', 'LINESTRING EMPTY'),
        ('<gml:segments><gml:LineStringSegment><foo/></gml:LineStringSegment></gml:segments>', 'LINESTRING EMPTY'),
        ('<gml:segments><gml:LineStringSegment><gml:Point><gml:pos>31 29 16</gml:pos></gml:Point></gml:LineStringSegment></gml:segments>', 'LINESTRING EMPTY'),
        ('<gml:MultiGeometry/>', 'GEOMETRYCOLLECTION EMPTY'),
        ('<gml:MultiGeometry><foo/></gml:MultiGeometry>', 'GEOMETRYCOLLECTION EMPTY'),
        ('<gml:MultiGeometry><gml:geometryMember/></gml:MultiGeometry>', None),
        ('<gml:MultiGeometry><gml:geometryMember><foo/></gml:geometryMember></gml:MultiGeometry>', None),
        ('<gml:Surface/>', 'POLYGON EMPTY'), # valid GML3
        ('<gml:Surface><foo/></gml:Surface>', 'POLYGON EMPTY'), # invalid GML3, but we are tolerant
        ('<gml:Surface><gml:patches/></gml:Surface>', 'POLYGON EMPTY'), # valid GML3
        ('<gml:Surface><gml:patches><foo/></gml:patches></gml:Surface>', None),
        ('<gml:Surface><gml:patches><gml:PolygonPatch/></gml:patches></gml:Surface>', 'POLYGON EMPTY'), # valid GML3
        ('<gml:Solid/>', 'POLYGON EMPTY'), # valid GML3
        ('<gml:Solid><foo/></gml:Solid>', 'POLYGON EMPTY'), # invalid GML3, but we are tolerant
        ('<gml:Solid><gml:exterior/></gml:Solid>', 'POLYGON EMPTY'), # valid GML3
        ('<gml:Solid><gml:exterior><foo/></gml:exterior></gml:Solid>', None),
        ('<gml:Solid><gml:exterior><Polygon><exterior><LinearRing><posList>0 0 4 0 4 4 0 4 0 0</posList></LinearRing></exterior></Polygon></gml:exterior><gml:interior/></gml:Solid>', 'POLYGON ((0 0,4 0,4 4,0 4,0 0))'),
        ('<gml:OrientableSurface/>', None),
        ('<gml:OrientableSurface><foo/></gml:OrientableSurface>', None),
        ('<gml:OrientableSurface><gml:baseSurface/></gml:OrientableSurface>', None),
        ('<gml:OrientableSurface><gml:baseSurface><foo/></gml:baseSurface></gml:OrientableSurface>', None),
        ('<gmlce:SimplePolygon/>',None), #invalid
        ('<gmlce:SimplePolygon><foo/></gmlce:SimplePolygon>',None), # invalid GML3, but we are tolerant
        ('<gmlce:SimplePolygon><gml:posList/></gmlce:SimplePolygon>','POLYGON EMPTY'), # validates the schema
        ('<gmlce:SimpleMultiPoint/>',None), #invalid
        ('<gmlce:SimpleMultiPoint><foo/></gmlce:SimpleMultiPoint>',None), # invalid GML3, but we are tolerant
        ('<gmlce:SimpleMultiPoint><gml:posList/></gmlce:SimpleMultiPoint>','MULTIPOINT EMPTY'), # validates the schema
        ('<gml:Envelope/>',None),
        ('<gml:Envelope><gml:lowerCorner/><gml:upperCorner/></gml:Envelope>',None),
        ('<gml:Envelope><gml:lowerCorner>1</gml:lowerCorner><gml:upperCorner>3 4</gml:upperCorner/></gml:Envelope>',None),
    ]

    for (gml, expected_wkt) in gml_expected_wkt_list:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        #print gml
        geom = ogr.CreateGeometryFromGML(gml)
        gdal.PopErrorHandler()
        if geom is None:
            if expected_wkt is not None:
                gdaltest.post_reason('did not get expected result for %s. Got None instead of %s' % (gml, expected_wkt))
                return 'fail'
        else:
            wkt = geom.ExportToWkt()
            if expected_wkt is None:
                gdaltest.post_reason('did not get expected result for %s. Got %s instead of None' % (gml, wkt))
                return 'fail'
            else:
                if wkt != expected_wkt:
                    gdaltest.post_reason('did not get expected result for %s. Got %s instead of %s' % (gml, wkt, expected_wkt))
                    return 'fail'

    return 'success'

###############################################################################
# Test write support for GML3

def gml_write_gml3_geometries():

    gml_list = [ '<gml:Point><gml:pos>2 3</gml:pos></gml:Point>',
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
             '<gml:MultiGeometry><gml:geometryMember><gml:Point><gml:pos>0 1</gml:pos></gml:Point></gml:geometryMember><gml:geometryMember><gml:LineString><gml:posList>2 3 4 5</gml:posList></gml:LineString></gml:geometryMember></gml:MultiGeometry>' ]

    for gml_in in gml_list:
        geom = ogr.CreateGeometryFromGML(gml_in)
        if gml_in.find('<gml:Curve') != -1:
            gml_out = geom.ExportToGML( ['FORMAT=GML3', 'GML3_LINESTRING_ELEMENT=curve'] )
        else:
            gml_out = geom.ExportToGML( ['FORMAT=GML3'] )
        if gml_out != gml_in:
            gdaltest.post_reason('got %s, instead of %s' % (gml_out, gml_in))
            return 'fail'

    return 'success'

###############################################################################
# Test write support for GML3 SRS

def gml_write_gml3_srs():

    sr32631 = osr.SpatialReference()
    sr32631.SetFromUserInput("EPSG:32631")

    srlonglat = osr.SpatialReference()
    srlonglat.SetFromUserInput("EPSG:4326")

    srlatlong = osr.SpatialReference()
    srlatlong.SetFromUserInput("EPSGA:4326")

    geom = ogr.CreateGeometryFromWkt('POINT(500000 4500000)')
    geom.AssignSpatialReference(sr32631)
    gml3 = geom.ExportToGML( options = ['FORMAT=GML3'] )
    expected_gml = '<gml:Point srsName="urn:ogc:def:crs:EPSG::32631"><gml:pos>500000 4500000</gml:pos></gml:Point>'
    if gml3 != expected_gml:
        gdaltest.post_reason('got %s, instead of %s' % (gml3, expected_gml))
        return 'fail'

    # Should perform the needed coordinate order swapping
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    geom.AssignSpatialReference(srlonglat)
    gml3 = geom.ExportToGML( options = ['FORMAT=GML3'] )
    expected_gml = '<gml:Point srsName="urn:ogc:def:crs:EPSG::4326"><gml:pos>49 2</gml:pos></gml:Point>'
    if gml3 != expected_gml:
        gdaltest.post_reason('got %s, instead of %s' % (gml3, expected_gml))
        return 'fail'

    # Shouldn't change the coordinate order
    geom = ogr.CreateGeometryFromWkt('POINT(49 2)')
    geom.AssignSpatialReference(srlatlong)
    gml3 = geom.ExportToGML( options = ['FORMAT=GML3'] )
    expected_gml = '<gml:Point srsName="urn:ogc:def:crs:EPSG::4326"><gml:pos>49 2</gml:pos></gml:Point>'
    if gml3 != expected_gml:
        gdaltest.post_reason('got %s, instead of %s' % (gml3, expected_gml))
        return 'fail'

    # Legacy SRS format
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    geom.AssignSpatialReference(srlonglat)
    gml3 = geom.ExportToGML( options = ['FORMAT=GML3', 'GML3_LONGSRS=NO'] )
    expected_gml = '<gml:Point srsName="EPSG:4326"><gml:pos>2 49</gml:pos></gml:Point>'
    if gml3 != expected_gml:
        gdaltest.post_reason('got %s, instead of %s' % (gml3, expected_gml))
        return 'fail'

    return 'success'

###############################################################################
# Test that importing too nested GML doesn't cause stack overflows

def gml_nested():

    gml = ''
    for i in range(31):
        gml = gml + '<gml:MultiGeometry><gml:geometryMember>'
    gml = gml + '<gml:MultiPolygon></gml:MultiPolygon>'
    for i in range(31):
        gml = gml +  '</gml:geometryMember></gml:MultiGeometry>'

    geom = ogr.CreateGeometryFromGML(gml)
    if geom is None:
        gdaltest.post_reason('expected a geometry')
        return 'fail'

    gml = ''
    for i in range(32):
        gml = gml + '<gml:MultiGeometry><gml:geometryMember>'
    gml = gml + '<gml:MultiPolygon></gml:MultiPolygon>'
    for i in range(32):
        gml = gml +  '</gml:geometryMember></gml:MultiGeometry>'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    geom = ogr.CreateGeometryFromGML(gml)
    gdal.PopErrorHandler()
    if geom is not None:
        gdaltest.post_reason('expected None')
        return 'fail'

    return 'success'

###############################################################################
# Test GML 3.3 SimplePolygon

def gml_SimplePolygon():

    gml = """<gmlce:SimplePolygon><gml:posList>0 0 1 0 1 1 0 1</gml:posList></gmlce:SimplePolygon>"""

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'POLYGON ((0 0,1 0,1 1,0 1,0 0))':
        gdaltest.post_reason( '<gmlce:SimplePolygon> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test GML 3.3 SimpleRectangle

def gml_SimpleRectangle():

    gml = """<gmlce:SimpleRectangle><gml:posList>0 0 1 0 1 1 0 1</gml:posList></gmlce:SimpleRectangle>"""

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'POLYGON ((0 0,1 0,1 1,0 1,0 0))':
        gdaltest.post_reason( '<gmlce:SimpleRectangle> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test GML 3.3 SimpleTriangle

def gml_SimpleTriangle():

    gml = """<gmlce:SimpleTriangle><gml:posList>0 0 1 0 1 1</gml:posList></gmlce:SimpleTriangle>"""

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'POLYGON ((0 0,1 0,1 1,0 0))':
        gdaltest.post_reason( '<gmlce:SimpleTriangle> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test GML 3.3 SimpleMultiPoint

def gml_SimpleMultiPoint():

    gml = """<gmlce:SimpleMultiPoint><gml:posList>0 1 2 3</gml:posList></gmlce:SimpleMultiPoint>"""

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'MULTIPOINT (0 1,2 3)':
        gdaltest.post_reason( '<gmlce:SimpleMultiPoint> not correctly parsed' )
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Test  gml:CompositeCurve> in <gml:Ring>

def gml_CompositeCurveInRing():

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

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,0 0))':
        print(geom.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# When imported build a list of units based on the files available.

#print 'hit enter'
#sys.stdin.readline()

gdaltest_list = []

files = os.listdir('data/wkb_wkt')
for filename in files:
    if filename[-4:] == '.wkt':
        ut = gml_geom_unit( filename[:-4] )
        gdaltest_list.append( (ut.gml_geom, ut.unit) )

gdaltest_list.append( gml_space_test )
gdaltest_list.append( gml_pos_point )
gdaltest_list.append( gml_pos_polygon )
gdaltest_list.append( gml_posList_line )
gdaltest_list.append( gml_posList_line3d )
gdaltest_list.append( gml_posList_line3d_2 )
gdaltest_list.append( gml_polygon )
gdaltest_list.append( gml_out_point_srs )
gdaltest_list.append( gml_out_point3d_srs )
gdaltest_list.append( gml_out_linestring_srs )
gdaltest_list.append( gml_out_polygon_srs )
gdaltest_list.append( gml_out_multipoint_srs )
gdaltest_list.append( gml_out_multilinestring_srs )
gdaltest_list.append( gml_out_multipolygon_srs )
gdaltest_list.append( gml_out_geometrycollection_srs )
gdaltest_list.append( gml_Box )
gdaltest_list.append( gml_Envelope )
gdaltest_list.append( gml_Curve )
gdaltest_list.append( gml_Curve_with_pointProperty )
gdaltest_list.append( gml_MultiCurve )
gdaltest_list.append( gml_MultiSurface )
gdaltest_list.append( gml_MultiSurface_surfaceMembers )
gdaltest_list.append( gml_MultiCurve_curveMembers )
gdaltest_list.append( gml_MultiCurve_pointMembers )
gdaltest_list.append( gml_Solid )
gdaltest_list.append( gml_OrientableSurface )
gdaltest_list.append( gml_Triangle )
gdaltest_list.append( gml_Rectangle )
gdaltest_list.append( gml_Tin )
gdaltest_list.append( gml_Arc )
gdaltest_list.append( gml_Circle )
gdaltest_list.append( gml_ConcatenatedDeduplication )
#gdaltest_list.append( gml_out_precision )
gdaltest_list.append( gml_invalid_geoms )
gdaltest_list.append( gml_write_gml3_geometries )
gdaltest_list.append( gml_write_gml3_srs )
gdaltest_list.append( gml_nested )
gdaltest_list.append( gml_SimplePolygon )
gdaltest_list.append( gml_SimpleRectangle )
gdaltest_list.append( gml_SimpleTriangle )
gdaltest_list.append( gml_SimpleMultiPoint )
gdaltest_list.append( gml_CompositeCurveInRing )

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gml_geom' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

