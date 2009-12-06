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

sys.path.append( '../pymod' )

import gdaltest
import ogr

###############################################################################

class wkb_wkt_unit:
    def __init__(self, unit):
        self.unit = unit

    def wkbwkt_geom( self ):
        raw_wkb = open('data/wkb_wkt/' + self.unit + '.wkb','rb').read()
        raw_wkt = open('data/wkb_wkt/' + self.unit + '.wkt').read()

        ######################################################################
        # Compare the WKT derived from the WKB file to the WKT provided
        # but reformatted (normalized).

        geom_wkb = ogr.CreateGeometryFromWkb( raw_wkb )
        wkb_wkt = geom_wkb.ExportToWkt()

        geom_wkt = ogr.CreateGeometryFromWkt( raw_wkt )
        normal_wkt = geom_wkt.ExportToWkt()

        if wkb_wkt != normal_wkt:
            gdaltest.post_reason( 'WKT from WKB (%s) does not match clean WKT (%s).' % (wkb_wkt, normal_wkt) )
            return 'fail'

        ######################################################################
        # Verify that the geometries appear to be the same.   This is
        # intended to catch problems with the encoding too WKT that might
        # cause passes above but that are mistaken.
        if geom_wkb.GetCoordinateDimension() != geom_wkt.GetCoordinateDimension():
            gdaltest.post_reason( 'Coordinate dimension differs!' )
            return 'fail'
        
        if geom_wkb.GetGeometryType() != geom_wkt.GetGeometryType():
            gdaltest.post_reason( 'Geometry type differs!' )
            return 'fail'
        
        if geom_wkb.GetGeometryName() != geom_wkt.GetGeometryName():
            gdaltest.post_reason( 'Geometry name differs!' )
            return 'fail'

 # It turns out this test is too picky about coordinate precision. skip.
 #       if geom_wkb.Equal( geom_wkt ) == 0:
 #           gdaltest.post_reason( 'Geometries not equal!' )
 #           print geom_wkb.ExportToWkt()
 #           print geom_wkt.ExportToWkt()
 #           return 'fail'

        geom_wkb.Destroy()
        
        ######################################################################
        # Convert geometry to WKB and back to verify that WKB encoding is
        # working smoothly.

        wkb_xdr = geom_wkt.ExportToWkb( ogr.wkbXDR )
        geom_wkb = ogr.CreateGeometryFromWkb( wkb_xdr )

        if str(geom_wkb) != str(geom_wkt):
            print(geom_wkb)
            print(geom_wkt)
            gdaltest.post_reason( 'XDR WKB encoding/decoding failure.' )
            return 'fail'

        geom_wkb.Destroy()

        wkb_ndr = geom_wkt.ExportToWkb( ogr.wkbNDR )
        geom_wkb = ogr.CreateGeometryFromWkb( wkb_ndr )

        if str(geom_wkb) != str(geom_wkt):
            gdaltest.post_reason( 'NDR WKB encoding/decoding failure.' )
            return 'fail'

        geom_wkb.Destroy()

        geom_wkt.Destroy()

        return 'success'
        
###############################################################################
# Test geometry with very large exponents of coordiantes values

def ogr_wkbwkt_geom_bigexponents():

    bigx = -1.79769313486e+308
    bigy = -1.12345678901e+308

    geom = ogr.Geometry( ogr.wkbPoint )
    geom.SetPoint( 0, bigx, bigy )

    expect = 'POINT (0 0 0)'
    wkt = geom.ExportToWkt()

    if str(wkt) != str(expect):
            gdaltest.post_reason( 'trimming long float numbers failed.' )
            return 'fail'

    return 'success'

###############################################################################
# When imported build a list of units based on the files available.

#print 'hit enter'
#sys.stdin.readline()

gdaltest_list = []

files = os.listdir('data/wkb_wkt')
for filename in files:
    if filename[-4:] == '.wkb':
        ut = wkb_wkt_unit( filename[:-4] )
        gdaltest_list.append( (ut.wkbwkt_geom, ut.unit) )

gdaltest_list.append( ogr_wkbwkt_geom_bigexponents )

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_wkbwkt_geom' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

