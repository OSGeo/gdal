#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Some DGN Driver features.
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

import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr

###############################################################################
# Verify we can open the test file.

def ogr_dgn_1():

    gdaltest.dgn_ds = ogr.Open( 'data/smalltest.dgn' )    
    if gdaltest.dgn_ds is None:
        gdaltest.post_reason( 'failed to open test file.' )
        return 'fail'

    gdaltest.dgn_lyr = gdaltest.dgn_ds.GetLayer( 0 )

    return 'success'

###############################################################################
# Check first feature, a text element.

def ogr_dgn_2():
    if gdaltest.dgn_ds is None:
        return 'skip'

    feat = gdaltest.dgn_lyr.GetNextFeature()
    if feat.GetField( 'Type' ) != 17 or feat.GetField( 'Level' ) != 1:
        gdaltest.post_reason( 'feature 1: expected attributes' )
        return 'fail'

    if feat.GetField( 'Text' ) != 'Demo Text': 
        gdaltest.post_reason( 'feature 1: expected text' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POINT (0.73650000 4.21980000)'):
        return 'fail'

    if feat.GetStyleString() != 'LABEL(t:"Demo Text",c:#ffffff,s:1.000g,f:ENGINEERING)':
        gdaltest.post_reason( 'Style string different than expected.' )
        return 'fail'

    return 'success'

###############################################################################
# Check second feature, a circle.

def ogr_dgn_3():
    if gdaltest.dgn_ds is None:
        return 'skip'

    feat = gdaltest.dgn_lyr.GetNextFeature()
    if feat.GetField( 'Type' ) != 15 or feat.GetField( 'Level' ) != 2:
        gdaltest.post_reason( 'feature 2: expected attributes' )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetCoordinateDimension() != 2:
        gdaltest.post_reason( 'expected 2d circle.' )
        return 'fail'
    
    if geom.GetGeometryName() != 'LINESTRING':
        gdaltest.post_reason('Expected circle to be translated as LINESTRING.')
        return 'fail'

    if geom.GetPointCount() < 15:
        gdaltest.post_reason( 'Unexpected small number of circle interpolation points.' )
        return 'fail'

    genvelope = geom.GetEnvelope()
    if genvelope[0] < 0.328593 or genvelope[0] > 0.328594 \
       or genvelope[1] < 9.68780 or genvelope[1] > 9.68781 \
       or genvelope[2] < -0.09611 or genvelope[2] > -0.09610 \
       or genvelope[3] < 9.26310 or genvelope[3] > 9.26311:
        gdaltest.post_reason( 'geometry extents seem odd' )
        return 'fail'

    return 'success'

###############################################################################
# Check third feature, a polygon with fill styling.

def ogr_dgn_4():
    if gdaltest.dgn_ds is None:
        return 'skip'

    feat = gdaltest.dgn_lyr.GetNextFeature()
    if feat.GetField( 'Type' ) != 6 or feat.GetField( 'Level' ) != 2 \
       or feat.GetField( 'ColorIndex' ) != 83:
        gdaltest.post_reason( 'feature 3: expected attributes' )
        return 'fail'

    wkt = 'POLYGON ((4.53550000 3.31700000,4.38320000 2.65170000,4.94410000 2.52350000,4.83200000 3.33310000,4.53550000 3.31700000))'
    
    if ogrtest.check_feature_geometry( feat, wkt):
        return 'fail'
    
    if feat.GetStyleString() != 'BRUSH(fc:#b40000,id:"ogr-brush-0")':
        gdaltest.post_reason( 'Style string different than expected.' )
        return 'fail'

    gdaltest.dgn_lyr.ResetReading()

    return 'success'

###############################################################################
# Use attribute query to pick just the type 15 level 2 object.

def ogr_dgn_5():

    if gdaltest.dgn_ds is None:
        return 'skip'

    gdaltest.dgn_lyr.SetAttributeFilter( 'Type = 15 and Level = 2' )
    tr = ogrtest.check_features_against_list( gdaltest.dgn_lyr, 'Type', [15] )
    gdaltest.dgn_lyr.SetAttributeFilter( None )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Use spatial filter to just pick the big circle.

def ogr_dgn_6():

    if gdaltest.dgn_ds is None:
        return 'skip'

    geom = ogr.CreateGeometryFromWkt( 'LINESTRING(1.0 8.55, 2.5 6.86)' )
    gdaltest.dgn_lyr.SetSpatialFilter( geom )
    geom.Destroy()
    
    tr = ogrtest.check_features_against_list( gdaltest.dgn_lyr, 'Type', [15] )
    gdaltest.dgn_lyr.SetSpatialFilter( None )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Copy our small dgn file to a new dgn file. 

def ogr_dgn_7():

    if gdaltest.dgn_ds is None:
        return 'skip'

    co_opts = [ 'UOR_PER_SUB_UNIT=100', 'SUB_UNITS_PER_MASTER_UNIT=100',
                'ORIGIN=-50,-50,0' ]

    dgn2_ds = ogr.GetDriverByName('DGN').CreateDataSource('tmp/dgn7.dgn',
                                                          options = co_opts )

    dgn2_lyr = dgn2_ds.CreateLayer( 'elements' )

    gdaltest.dgn_lyr.ResetReading()

    dst_feat = ogr.Feature( feature_def = dgn2_lyr.GetLayerDefn() )
    
    feat = gdaltest.dgn_lyr.GetNextFeature()
    while feat is not None:
        dst_feat.SetFrom( feat )
        if dgn2_lyr.CreateFeature( dst_feat ) != 0:
            gdaltest.post_reason('CreateFeature failed.')
            return 'fail'

        feat = gdaltest.dgn_lyr.GetNextFeature()

    dgn2_lyr = None
    dgn2_ds = None

    return 'success'

###############################################################################
# Verify that our copy is pretty similar.
#
# Currently the styling information is not well preserved.  Eventually
# this should be fixed up and the test made more stringent.
#

def ogr_dgn_8():

    if gdaltest.dgn_ds is None:
        return 'skip'

    dgn2_ds = ogr.Open( 'tmp/dgn7.dgn' )

    dgn2_lyr = dgn2_ds.GetLayerByName( 'elements' )

    # Test first first, a text element.
    feat = dgn2_lyr.GetNextFeature()
    if feat.GetField( 'Type' ) != 17 or feat.GetField( 'Level' ) != 1:
        gdaltest.post_reason( 'feature 1: expected attributes' )
        return 'fail'

    if feat.GetField( 'Text' ) != 'Demo Text': 
        gdaltest.post_reason( 'feature 1: expected text' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POINT (0.73650000 4.21980000)'):
        return 'fail'

    if feat.GetStyleString() != 'LABEL(t:"Demo Text",c:#ffffff,s:1.000g,f:ENGINEERING)':
        gdaltest.post_reason( 'feature 1: Style string different than expected.' )
        return 'fail'

    # Check second element, a circle.

    feat = dgn2_lyr.GetNextFeature()
    if feat.GetField( 'Type' ) != 12 or feat.GetField( 'Level' ) != 2:
        gdaltest.post_reason( 'feature 2: expected attributes' )
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom.GetCoordinateDimension() != 2:
        gdaltest.post_reason( 'feature 2: expected 2d circle.' )
        return 'fail'
    
    if geom.GetGeometryName() != 'MULTILINESTRING':
        gdaltest.post_reason('feature 2: Expected MULTILINESTRING.')
        return 'fail'

    genvelope = geom.GetEnvelope()
    if genvelope[0] < 0.3285 or genvelope[0] > 0.3287 \
       or genvelope[1] < 9.6878 or genvelope[1] > 9.6879 \
       or genvelope[2] < -0.0962 or genvelope[2] > -0.0960 \
       or genvelope[3] < 9.26310 or genvelope[3] > 9.2632:
        gdaltest.post_reason( 'feature 2: geometry extents seem odd' )
        print(genvelope)
        return 'fail'

    # Check 3rd feature, a polygon

    feat = dgn2_lyr.GetNextFeature()
    if feat.GetField( 'Type' ) != 6 or feat.GetField( 'Level' ) != 2 \
       or feat.GetField( 'ColorIndex' ) != 83:
        gdaltest.post_reason( 'feature 3: expected attributes' )
        return 'fail'

    wkt = 'POLYGON ((4.53550000 3.31700000,4.38320000 2.65170000,4.94410000 2.52350000,4.83200000 3.33310000,4.53550000 3.31700000))'
    
    if ogrtest.check_feature_geometry( feat, wkt):
        return 'fail'

    # should be: 'BRUSH(fc:#b40000,id:"ogr-brush-0")'
    if feat.GetStyleString() != 'PEN(id:"ogr-pen-0",c:#b40000)':
        gdaltest.post_reason( 'feature 3: Style string different than expected: '+ feat.GetStyleString() )
        return 'fail'

    dgn2_ds = None

    return 'success'

###############################################################################
#  Cleanup

def ogr_dgn_cleanup():

    if gdaltest.dgn_ds is not None:
        gdaltest.dgn_lyr = None
        gdaltest.dgn_ds = None

    gdaltest.clean_tmp()
    return 'success'

gdaltest_list = [
    ogr_dgn_1,
    ogr_dgn_2,
    ogr_dgn_3,
    ogr_dgn_4,
    ogr_dgn_5,
    ogr_dgn_6,
    ogr_dgn_7,
    ogr_dgn_8,
    ogr_dgn_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_dgn' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
