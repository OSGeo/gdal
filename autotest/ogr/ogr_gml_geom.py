#!/usr/bin/env python
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
import ogr
import osr

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
    gml = '<gml:LineString><gml:coordinates decimal="." cs="," ts=" ">189999.99995605,624999.99998375 200000.00005735,624999.99998375 200000.00005735,612499.99997125 195791.3593843,612499.99997125 193327.3749823,612499.99997125 189999.99995605,612499.99997125 189999.99995605,619462.31247125 189999.99995605,624999.99998375 \n</gml:coordinates></gml:LineString>'
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

    gml = '<gml:Point><gml:pos>31 29 16</gml:pos></gml:Point>'

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'POINT (31 29 16)':
        gdaltest.post_reason( '<gml:pos> not correctly parsed' )
        return 'fail'

    return 'success'

###############################################################################
# Test GML 3.1.1 "pos" element for a polygon. (ticket #3244)

def gml_pos_polygon():

    gml = '''<gml:Polygon>
                <gml:exterior>
                    <gml:LinearRing>
                        <gml:pos>0 0</gml:pos>
                        <gml:pos>4 0</gml:pos>
                        <gml:pos>4 4</gml:pos>
                        <gml:pos>0 4</gml:pos>
                        <gml:pos>0 0</gml:pos>
                    </gml:LinearRing>
                </gml:exterior>
                <gml:interior>
                    <gml:LinearRing>
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

    gml = '<LineString><posList>31 42 53 64 55 76</posList></LineString>'

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'LINESTRING (31 42,53 64,55 76)':
        gdaltest.post_reason( '<gml:posList> not correctly parsed' )
        return 'fail'

    return 'success'


###############################################################################
# Test GML 3.x "posList" element for a 3D linestring.

def gml_posList_line3d():

    gml = '<LineString><posList srsDimension="3">31 42 1 53 64 2 55 76 3</posList></LineString>'

    geom = ogr.CreateGeometryFromGML( gml )

    if geom.ExportToWkt() != 'LINESTRING (31 42 1,53 64 2,55 76 3)':
        gdaltest.post_reason( '<gml:posList> not correctly parsed' )
        return 'fail'

    return 'success'

###############################################################################
# Test GML 3.x "polygon" element for a point.

def gml_polygon():

    gml = '<Polygon><exterior><LinearRing><posList>0 0 4 0 4 4 0 4 0 0</posList></LinearRing></exterior><interior><LinearRing><posList>1 1 2 1 2 2 1 2 1 1</posList></LinearRing></interior></Polygon>'
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

    if gml[0:44] != '<gml:GeometryCollection srsName="EPSG:4326">':
        gdaltest.post_reason( 'No srsName attribute in GML output')
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
# Test GML Curve

def gml_Curve():

    gml = """<gml:Curve xmlns:gml="http://www.opengis.net/gml" srsName="foo">
    <gml:segments>
        <gml:LineStringSegment>
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
# Test GML MultiCurve

def gml_MultiCurve():

    gml = """<gml:MultiCurve xmlns:gml="http://www.opengis.net/gml" srsName="foo">
    <gml:curveMember>
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
    <gml:surfaceMember>
        <gml:Surface>
            <gml:patches>
                <gml:PolygonPatch interpolation="planar">
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
gdaltest_list.append( gml_Curve )
gdaltest_list.append( gml_MultiCurve )
gdaltest_list.append( gml_MultiSurface )

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gml_geom' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

