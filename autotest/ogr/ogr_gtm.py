#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GTM driver functionality.
# Author:   Leonardo de Paula Rosa Piga <leonardo dot piga at gmail dot com>
# 
###############################################################################
# Copyright (c) 2009, Leonardo de P. R. Piga <leonardo dot piga at gmail dot com>
# Copyright (c) 2009, Even Rouault <even dot rouault at mines-paris dot org>
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

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr

def ogr_gtm_init():
    gdaltest.gtm_ds = None

    try:
        gdaltest.gtm_ds = ogr.Open( 'data/samplemap.gtm' )
    except:
        gdaltest.gtm_ds = None

    if gdaltest.gtm_ds is None:
        gdaltest.have_gtm = 0
    else:
        gdaltest.have_gtm = 1

    if not gdaltest.have_gtm:
        return 'skip'

    if gdaltest.gtm_ds.GetLayerCount() != 2:
        gdaltest.post_reason( 'wrong number of layers' )
        return 'fail'

    return 'success'

###############################################################################
# Test waypoints gtm layer.

def ogr_gtm_read_1():
    if not gdaltest.have_gtm:
        return 'skip'
    
    if gdaltest.gtm_ds is None:
        return 'fail'

    lyr = gdaltest.gtm_ds.GetLayerByName( 'samplemap_waypoints' )

    if lyr.GetFeatureCount() != 3:
        gdaltest.post_reason( 'wrong number of features' )
        return 'fail'
    
    # Test 1st feature
    feat = lyr.GetNextFeature()

    if feat.GetField('name') != 'WAY6':
        gdaltest.post_reason( 'Wrong name field value' )
        return 'fail'

    if feat.GetField('comment') != 'Santa Cruz Stadium':
        gdaltest.post_reason( 'Wrong comment field value' )
        return 'fail'

    if feat.GetField('icon') != 92:
        gdaltest.post_reason( 'Wrong icon field value' )
        return 'fail'

    if feat.GetField('time') != '2009/12/18 17:32:41':
        gdaltest.post_reason( 'Wrong time field value' )
        return 'fail'

    wkt = 'POINT (-47.789974212646484 -21.201919555664062)'
    if ogrtest.check_feature_geometry( feat, wkt):
        gdaltest.post_reason( 'Unexpected geometry' )
        return 'fail'
    
    # Test 2nd feature
    feat = lyr.GetNextFeature()

    if feat.GetField('name') != 'WAY6':
        gdaltest.post_reason( 'Wrong name field value' )
        return 'fail'

    if feat.GetField('comment') != 'Joe\'s Goalkeeper Pub':
        gdaltest.post_reason( 'Wrong comment field value' )
        return 'fail'


    if feat.GetField('icon') != 4:
        gdaltest.post_reason( 'Wrong icon field value' )
        return 'fail'

    if feat.GetField('time') != '2009/12/18 17:34:46':
        gdaltest.post_reason( 'Wrong time field value' )
        return 'fail'

    wkt = 'POINT (-47.909481048583984 -21.294229507446289)'
    if ogrtest.check_feature_geometry( feat, wkt):
        gdaltest.post_reason( 'Unexpected geometry' )
        return 'fail'

    # Test 3rd feature
    feat = lyr.GetNextFeature()

    if feat.GetField('name') != '33543400':
        gdaltest.post_reason( 'Wrong name field value' )
        return 'fail'

    if feat.GetField('comment') != 'City Hall':
        gdaltest.post_reason( 'Wrong comment field value' )
        return 'fail'

    if feat.GetField('icon') != 61:
        gdaltest.post_reason( 'Wrong icon field value' )
        return 'fail'

    if feat.GetField('time') != None:
        gdaltest.post_reason( 'Wrong time field value' )
        return 'fail'

    wkt = 'POINT (-47.806097491943362 -21.176849600708007)'
    if ogrtest.check_feature_geometry( feat, wkt):
        gdaltest.post_reason( 'Unexpected geometry' )
        return 'fail'

    return 'success'

###############################################################################
# Test tracks gtm layer.
def ogr_gtm_read_2():
    if not gdaltest.have_gtm:
        return 'skip'
    
    if gdaltest.gtm_ds is None:
        return 'fail'

    lyr = gdaltest.gtm_ds.GetLayerByName( 'samplemap_tracks' )

    if lyr.GetFeatureCount() != 3:
        gdaltest.post_reason( 'wrong number of features' )
        return 'fail'
    
    # Test 1st feature
    feat = lyr.GetNextFeature()

    if feat.GetField('name') != 'San Sebastian Street':
        gdaltest.post_reason( 'Wrong name field value' )
        return 'fail'

    if feat.GetField('type') != 2:
        gdaltest.post_reason( 'Wrong type field value' )
        return 'fail'

    if feat.GetField('color') != 0:
        gdaltest.post_reason( 'Wrong color field value' )
        return 'fail'

    #if feat.GetField('time') != None:
    #    gdaltest.post_reason( 'Wrong time field value' )
    #    return 'fail'

    wkt = 'LINESTRING (-47.807481607448054 -21.177795963939211,' + \
          '-47.808151245117188 -21.177299499511719,' + \
          '-47.809136624130645 -21.176562836150087,' + \
          '-47.809931418108405 -21.175971104366582)'
    if ogrtest.check_feature_geometry( feat, wkt):
        gdaltest.post_reason( 'Unexpected geometry' )
        return 'fail'

    # Test 2nd feature
    feat = lyr.GetNextFeature()

    if feat.GetField('name') != 'Barao do Amazonas Street':
        gdaltest.post_reason( 'Wrong name field value' )
        return 'fail'

    if feat.GetField('type') != 1:
        gdaltest.post_reason( 'Wrong type field value' )
        return 'fail'

    if feat.GetField('color') != 0:
        gdaltest.post_reason( 'Wrong color field value' )
        return 'fail'

    #if feat.GetField('time') != None:
    #    gdaltest.post_reason( 'Wrong time field value' )
    #    return 'fail'

    wkt = 'LINESTRING (-47.808751751608561 -21.178029550275486,' + \
                       '-47.808151245117188 -21.177299499511719,' + \
                       '-47.807561550927701 -21.176617693474089,' + \
                       '-47.806959118447779 -21.175900153727685)'
    if ogrtest.check_feature_geometry( feat, wkt):
        return 'fail'
    
    # Test 3rd feature
    feat = lyr.GetNextFeature()

    if feat.GetField('name') != 'Curupira Park':
        gdaltest.post_reason( 'Wrong name field value' )
        return 'fail'

    if feat.GetField('type') != 17:
        gdaltest.post_reason( 'Wrong type field value' )
        return 'fail'

    if feat.GetField('color') != 46848:
        gdaltest.post_reason( 'Wrong color field value' )
        return 'fail'

    #if feat.GetField('time') != None:
    #    gdaltest.post_reason( 'Wrong time field value' )
    #    return 'fail'

    wkt = 'LINESTRING (-47.7894287109375 -21.194473266601562,' + \
                      '-47.793514591064451 -21.197530536743162,' + \
                      '-47.797027587890625 -21.19483757019043,' + \
                      '-47.794818878173828 -21.192028045654297,' + \
                      '-47.794120788574219 -21.193340301513672,' + \
                      '-47.792263031005859 -21.194267272949219,' + \
                      '-47.7894287109375 -21.194473266601562)'

    if ogrtest.check_feature_geometry( feat, wkt):
        gdaltest.post_reason( 'Unexpected geometry' )
        return 'fail'
    
    return 'success'


###############################################################################
# Write test

###############################################################################
# Waypoint write
def ogr_gtm_write_1():

    ds = ogr.GetDriverByName('GPSTrackMaker').CreateDataSource('tmp/gtm.gtm')
    lyr = ds.CreateLayer('gtm_waypoints', geom_type = ogr.wkbPoint)

    dst_feat = ogr.Feature( lyr.GetLayerDefn() )
    dst_feat.SetField('name', 'WAY0000000')
    dst_feat.SetField('comment', 'Waypoint 0')
    dst_feat.SetField('icon', 10)
    dst_feat.SetField('time', '2009/12/23 14:25:46')
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (-21 -47)'))
    if lyr.CreateFeature( dst_feat ) != 0:
        gdaltest.post_reason('CreateFeature failed.')
        return 'fail'
    dst_feat.Destroy()

    dst_feat = ogr.Feature( lyr.GetLayerDefn() )
    dst_feat.SetField('name', 'WAY0000001')
    dst_feat.SetField('comment', 'Waypoint 1')
    dst_feat.SetField('icon', 31)
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (-21.123 -47.231 800)'))
    if lyr.CreateFeature( dst_feat ) != 0:
        gdaltest.post_reason('CreateFeature failed.')
        return 'fail'
    dst_feat.Destroy()

    lyr = ds.CreateLayer('gtm_tracks', geom_type = ogr.wkbLineString)

    dst_feat = ogr.Feature( lyr.GetLayerDefn() )
    dst_feat.SetField('name', '1st Street')
    dst_feat.SetField('type', 2)
    dst_feat.SetField('color', 0x0000FF)
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING (-21.1 -47.1, -21.2 -47.2, -21.3 -47.3, -21.4 -47.4)'))
    if lyr.CreateFeature( dst_feat ) != 0:
        gdaltest.post_reason('CreateFeature failed.')
        return 'fail'
    dst_feat.Destroy()

    dst_feat = ogr.Feature( lyr.GetLayerDefn() )
    dst_feat.SetField('name', '2nd Street')
    dst_feat.SetField('type', 1)
    dst_feat.SetField('color', 0x000000)
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING (-21.11 -47.1, -21.21 -47.2, -21.31 -47.3, -21.41 -47.4)'))
    if lyr.CreateFeature( dst_feat ) != 0:
        gdaltest.post_reason('CreateFeature failed.')
        return 'fail'
    dst_feat.Destroy()

    dst_feat = ogr.Feature( lyr.GetLayerDefn() )
    dst_feat.SetField('name', '3rd Street')
    dst_feat.SetField('type', 2)
    dst_feat.SetField('color', 0x000000)
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING ((-21.12 -47.1, -21.22 -47.2, -21.32 -47.3, -21.42 -47.4),' + \
                                                   '(-21.12 -47.1, -21.02 -47.0, -20.92 -46.9))'))
    if lyr.CreateFeature( dst_feat ) != 0:
        gdaltest.post_reason('CreateFeature failed.')
        return 'fail'
    dst_feat.Destroy()

    ds.Destroy()

    return 'success'

###############################################################################
# Check previous test
def ogr_gtm_check_write_1():
    if not gdaltest.have_gtm:
        return 'skip'
    
    ds = ogr.Open('tmp/gtm.gtm')
    lyr = ds.GetLayerByName('gtm_waypoints')
    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('Bad feature count.')
        return 'fail'

    # Test 1st waypoint
    feat = lyr.GetNextFeature()

    if feat.GetField('name') != 'WAY0000000':
        gdaltest.post_reason( 'Wrong name field value' )
        return 'fail'

    if feat.GetField('comment') != 'Waypoint 0':
        gdaltest.post_reason( 'Wrong comment field value' )
        return 'fail'

    if feat.GetField('icon') != 10:
        gdaltest.post_reason( 'Wrong icon field value' )
        return 'fail'

    if feat.GetField('time') != '2009/12/23 14:25:46':
        gdaltest.post_reason( 'Wrong time field value' )
        return 'fail'

    wkt = 'POINT (-21 -47)'
    if ogrtest.check_feature_geometry( feat, wkt):
        gdaltest.post_reason( 'Unexpected geometry' )
        return 'fail'
    feat.Destroy()
    
    # Test 2nd waypoint
    feat = lyr.GetNextFeature()

    if feat.GetField('name') != 'WAY0000001':
        gdaltest.post_reason( 'Wrong name field value' )
        return 'fail'

    if feat.GetField('comment') != 'Waypoint 1':
        gdaltest.post_reason( 'Wrong comment field value' )
        return 'fail'

    if feat.GetField('icon') != 31:
        gdaltest.post_reason( 'Wrong icon field value' )
        return 'fail'

    if feat.GetField('time') != None:
        gdaltest.post_reason( 'Wrong time field value' )
        return 'fail'

    wkt = 'POINT (-21.123 -47.231 800)'
    if ogrtest.check_feature_geometry( feat, wkt):
        gdaltest.post_reason( 'Unexpected geometry' )
        return 'fail'
    feat.Destroy()

    # Test tracks
    lyr = ds.GetLayerByName('gtm_tracks')
    if lyr.GetFeatureCount() != 4: # The driver converts multiline
                                   # into one line for each tuple
                                   # element
        gdaltest.post_reason('Bad feature count.')
        return 'fail'

    # Test 1st track
    feat = lyr.GetNextFeature()

    if feat.GetField('name') != '1st Street':
        gdaltest.post_reason( 'Wrong name field value' )
        return 'fail'

    if feat.GetField('type') != 2:
        gdaltest.post_reason( 'Wrong type field value' )
        return 'fail'

    if feat.GetField('color') != 0x0000FF:
        gdaltest.post_reason( 'Wrong color field value' )
        return 'fail'

    wkt = 'LINESTRING (-21.1 -47.1, -21.2 -47.2, -21.3 -47.3, -21.4 -47.4)'
    if ogrtest.check_feature_geometry( feat, wkt):
        gdaltest.post_reason( 'Unexpected geometry' )
        return 'fail'
    feat.Destroy()

    # Test 2nd track
    feat = lyr.GetNextFeature()

    if feat.GetField('name') != '2nd Street':
        gdaltest.post_reason( 'Wrong name field value' )
        return 'fail'

    if feat.GetField('type') != 1:
        gdaltest.post_reason( 'Wrong type field value' )
        return 'fail'

    if feat.GetField('color') != 0x000000:
        gdaltest.post_reason( 'Wrong color field value' )
        return 'fail'

    wkt = 'LINESTRING (-21.11 -47.1, -21.21 -47.2, -21.31 -47.3, -21.41 -47.4)'
    if ogrtest.check_feature_geometry( feat, wkt):
        gdaltest.post_reason( 'Unexpected geometry' )
        return 'fail'
    feat.Destroy()

    # Test 3rd track
    feat = lyr.GetNextFeature()

    if feat.GetField('name') != '3rd Street':
        gdaltest.post_reason( 'Wrong name field value' )
        return 'fail'

    if feat.GetField('type') != 2:
        gdaltest.post_reason( 'Wrong type field value' )
        return 'fail'

    if feat.GetField('color') != 0x000000:
        gdaltest.post_reason( 'Wrong color field value' )
        return 'fail'

    wkt = 'LINESTRING (-21.12 -47.1, -21.22 -47.2, -21.32 -47.3, -21.42 -47.4)'
    if ogrtest.check_feature_geometry( feat, wkt):
        gdaltest.post_reason( 'Unexpected geometry' )
        return 'fail'
    feat.Destroy()

    # Test 4th track
    feat = lyr.GetNextFeature()

    if feat.GetField('name') != '3rd Street':
        gdaltest.post_reason( 'Wrong name field value' )
        return 'fail'

    if feat.GetField('type') != 2:
        gdaltest.post_reason( 'Wrong type field value' )
        return 'fail'

    if feat.GetField('color') != 0x000000:
        gdaltest.post_reason( 'Wrong color field value' )
        return 'fail'

    wkt = 'LINESTRING (-21.12 -47.1, -21.02 -47.0, -20.92 -46.9)'
    if ogrtest.check_feature_geometry( feat, wkt):
        gdaltest.post_reason( 'Unexpected geometry' )
        return 'fail'
    feat.Destroy()

    return 'success'

    
###############################################################################
# 

def ogr_gtm_cleanup():

    if gdaltest.gtm_ds is not None:
        gdaltest.gtm_ds.Destroy()
    gdaltest.gtm_ds = None
    os.remove('tmp/gtm.gtm')
    return 'success'

gdaltest_list = [ 
    ogr_gtm_init,
    ogr_gtm_read_1,
    ogr_gtm_read_2,
    ogr_gtm_write_1,
    ogr_gtm_check_write_1,
    ogr_gtm_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gtm' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

