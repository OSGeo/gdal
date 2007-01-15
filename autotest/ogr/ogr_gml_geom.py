#!/usr/bin/env python
###############################################################################
# $Id: ogr_gml_geom.py,v 1.5 2006/06/23 00:36:59 mloskot Exp $
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
# 
#  $Log: ogr_gml_geom.py,v $
#  Revision 1.5  2006/06/23 00:36:59  mloskot
#  Added new test cases to ogr/ogr_gml_geom.py. New cases test export of GML 2 srsName.
#
#  Revision 1.4  2005/07/29 04:14:15  fwarmerdam
#  fix up test names for better reporting
#
#  Revision 1.3  2005/03/08 19:50:50  fwarmerdam
#  Added gml:pos parsing test.
#
#  Revision 1.2  2004/01/06 18:30:25  warmerda
#  added test with newline and spaces in it
#
#  Revision 1.1  2003/09/22 03:11:59  warmerda
#  New
#
#  Revision 1.3  2003/03/19 16:04:26  warmerda
#  read wkb files in binary mode
#
#  Revision 1.2  2003/03/13 15:33:30  warmerda
#  removed cruft
#
#  Revision 1.1  2003/03/07 21:31:57  warmerda
#  New
#
#

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
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
        clean_wkt = geom_wkt.ExportToWkt();

        gml = geom_wkt.ExportToGML()

        if gml is None or len(gml) == 0:
            gdaltest.post_reason( 'Conversion to GML failed.')
            return 'fail'

        geom_wkt.Destroy()
            
        ######################################################################
        # Convert back to WKT. 
        
        geom_gml = ogr.CreateGeometryFromGML( gml )
        gml_wkt = geom_gml.ExportToWkt()

        if gml_wkt != clean_wkt:
            gdaltest.post_reason( 'WKT from GML (%s) does not match clean WKT (%s).\ngml was (%s)' % (gml_wkt, clean_wkt, gml) )
            return 'fail'

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
gdaltest_list.append( gml_out_point_srs )
gdaltest_list.append( gml_out_point3d_srs )
gdaltest_list.append( gml_out_linestring_srs )
gdaltest_list.append( gml_out_polygon_srs )
gdaltest_list.append( gml_out_multipoint_srs )
gdaltest_list.append( gml_out_multilinestring_srs )
gdaltest_list.append( gml_out_multipolygon_srs )
gdaltest_list.append( gml_out_geometrycollection_srs )

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gml_geom' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

