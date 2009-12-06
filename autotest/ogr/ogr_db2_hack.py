#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test DB2 V7.2 WKB support. DB2 7.2 had a corrupt WKB format
#           and OGR supports reading and writing it for compatibility (done
#           on behalf of Safe Software).
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
import gdal

###############################################################################
# Create a point in DB2 format, and verify the byte order flag. 

def ogr_db2_hack_1():

    if ogr.SetGenerate_DB2_V72_BYTE_ORDER( 1 ) != 0:
        return 'skip'

    # XDR Case. 
    geom = ogr.CreateGeometryFromWkt( 'POINT(10 20)' )
    wkb = geom.ExportToWkb( byte_order = ogr.wkbXDR ).decode('latin1')
    geom.Destroy()

    if wkb[0] != '0':
        gdaltest.post_reason('WKB wkbXDR point geometry has wrong byte order')
        return 'fail'

    # NDR Case. 
    geom = ogr.CreateGeometryFromWkt( 'POINT(10 20)' )
    wkb = geom.ExportToWkb( byte_order = ogr.wkbNDR ).decode('latin1')
    geom.Destroy()
    
    if wkb[0] != '1':
        gdaltest.post_reason('WKB wkbNDR point geometry has wrong byte order')
        return 'fail'

    return 'success'

###############################################################################
# Verify that we can turn DB2 V7.2 mode back off!

def ogr_db2_hack_2():

    if ogr.SetGenerate_DB2_V72_BYTE_ORDER( 0 ) != 0:
        gdaltest.post_reason( 'SetGenerate to turn off hack failed!' )
        return 'fail'

    # XDR Case. 
    geom = ogr.CreateGeometryFromWkt( 'POINT(10 20)' )
    wkb = geom.ExportToWkb( byte_order = ogr.wkbXDR ).decode('latin1')
    geom.Destroy()

    if wkb[0] != chr(0):
        gdaltest.post_reason('WKB wkbXDR point geometry has wrong byte order')
        return 'fail'

    # NDR Case. 
    geom = ogr.CreateGeometryFromWkt( 'POINT(10 20)' )
    wkb = geom.ExportToWkb( byte_order = ogr.wkbNDR ).decode('latin1')
    geom.Destroy()
    
    if wkb[0] != chr(1):
        gdaltest.post_reason('WKB wkbNDR point geometry has wrong byte order')
        return 'fail'

    return 'success'


###############################################################################
# Try a more complex geometry, and verify we can read it back.

def ogr_db2_hack_3():

    if ogr.SetGenerate_DB2_V72_BYTE_ORDER( 1 ) != 0:
        return 'skip'

    wkt = 'MULTIPOLYGON (((10.00121344 2.99853145,10.00121344 1.99853145,11.00121343 1.99853148,11.00121343 2.99853148)),((10.00121344 2.99853145,10.00121344 3.99853145,9.00121345 3.99853143,9.00121345 2.99853143)))'

    geom = ogr.CreateGeometryFromWkt( wkt )
    wkb = geom.ExportToWkb()
    geom.Destroy()

    # Check primary byte order value.
    if wkb.decode('latin1')[0] != '0' and wkb.decode('latin1')[0] != '1':
        gdaltest.post_reason( 'corrupt primary geometry byte order' )
        return 'fail'

    # Check component geometry byte order
    if wkb.decode('latin1')[9] != '0' and wkb.decode('latin1')[9] != '1':
        gdaltest.post_reason( 'corrupt sub-geometry byte order' )
        return 'fail'

    geom = ogr.CreateGeometryFromWkb( wkb )
    if geom.ExportToWkt() != wkt:
        gdaltest.post_reason( 'Convertion to/from DB2 format seems to have corrupted geometry.')
        return 'fail'

    geom.Destroy()

    ogr.SetGenerate_DB2_V72_BYTE_ORDER( 0 )

    return 'success'

gdaltest_list = [ 
    ogr_db2_hack_1,
    ogr_db2_hack_2,
    ogr_db2_hack_3
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_db2_hack' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

