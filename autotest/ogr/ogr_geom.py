#!/usr/bin/env python
###############################################################################
# $Id: ogr_geom.py,v 1.2 2004/09/17 14:46:41 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Misc. OGRGeometry operations.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
#  $Log: ogr_geom.py,v $
#  Revision 1.2  2004/09/17 14:46:41  fwarmerdam
#  use more complex multipolygon with holes in a polygon
#
#  Revision 1.1  2004/09/17 14:41:28  fwarmerdam
#  New
#
#

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr

###############################################################################
# Test Area calculation for a MultiPolygon (which excersises lower level
# get_Area() methods as well).

def ogr_geom_area():

    geom_wkt = 'MULTIPOLYGON( ((0 0,1 1,1 0,0 0)),((0 0,10 0, 10 10, 0 10),(1 1,1 2,2 2,2 1)) )'
    geom = ogr.CreateGeometryFromWkt( geom_wkt )

    area = geom.GetArea()
    if abs(area-99.5) > 0.00000000001:
        gdaltest.post_reason( 'Area result wrong, got %g.' % area )
        return 'fail'

    geom.Destroy()
    
    return 'success'

###############################################################################
# cleanup

def ogr_geom_cleanup():
    return 'success'

gdaltest_list = [ 
    ogr_geom_area,
    ogr_geom_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_geom' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

