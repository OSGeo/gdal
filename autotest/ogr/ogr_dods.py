#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR DODS driver.
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

import os
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import osr
import gdal

###############################################################################
# Open DODS datasource.

def ogr_dods_1():
    gdaltest.dods_ds = None
    try:
        ogrtest.dods_drv = ogr.GetDriverByName( 'DODS' )
    except:
        ogrtest.dods_drv = None
        return 'skip'

    if ogrtest.dods_drv is None:
        return 'skip'

    gdal.SetConfigOption( 'DODS_AIS_FILE', 'data/ais.xml' )

    srv = 'http://www.epic.noaa.gov:10100/dods/wod2001/natl_prof_bot.cdp?&_id=1'
    if gdaltest.gdalurlopen(srv) is None:
        gdaltest.dods_ds = None
        return 'skip'

    gdaltest.dods_ds = ogr.Open( 'DODS:' + srv )

    if gdaltest.dods_ds is None:
        return 'fail'

    try:
        gdaltest.dods_profiles = gdaltest.dods_ds.GetLayerByName( 'profiles' )
        gdaltest.dods_normalized =gdaltest.dods_ds.GetLayerByName('normalized')
        gdaltest.dods_lines = gdaltest.dods_ds.GetLayerByName( 'lines' )
    except:
        gdaltest.dods_profiles = None
        gdaltest.dods_normalized = None
        
    if gdaltest.dods_profiles is None:
        gdaltest.dods_ds = None
        gdaltest.post_reason('profiles layer missing, likely AIS stuff not working.' )
        return 'fail'
    else:
        return 'success'

###############################################################################
# Read a single feature from the profiles layer and verify a few things.
#

def ogr_dods_2():

    if gdaltest.dods_ds is None:
        return 'skip'

    gdaltest.dods_profiles.ResetReading()
    feat = gdaltest.dods_profiles.GetNextFeature()

    if feat.GetField('time') != -1936483200000:
        gdaltest.post_reason( 'time wrong' )
        return 'fail'
    
    if feat.GetField('profile.depth') != [0,10,20,30,39]:
        gdaltest.post_reason( 'depth wrong' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POINT (4.30000019 5.36999989)')\
           != 0:
        return 'fail'

    feat.Destroy()

    feat = gdaltest.dods_profiles.GetNextFeature()
    if feat is not None:
        feat.Destroy()
        gdaltest.post_reason( 'got more than expected number of features.' )
        return 'fail'

    return 'success'

###############################################################################
# Read the normalized form of the same profile, and verify some values.
#

def ogr_dods_3():

    if gdaltest.dods_ds is None:
        return 'skip'

    gdaltest.dods_normalized.ResetReading()
    expect = [0,10,20,30,39]
    tr = ogrtest.check_features_against_list( gdaltest.dods_normalized,
                                              'depth', expect )
    if tr == 0:
        return 'fail'

    expected = [14.8100004196167,14.8100004196167,14.8100004196167,14.60999965667725,14.60999965667725]
    
    gdaltest.dods_normalized.ResetReading()
    for i in range(5):
        feat = gdaltest.dods_normalized.GetNextFeature()

        if feat.GetField('time') != -1936483200000:
            gdaltest.post_reason( 'time wrong' )
            return 'fail'
    
        if abs(feat.GetField('T_20')-expected[i]) > 0.001:
            gdaltest.post_reason( 'T_20 wrong' )
            return 'fail'
        
        if ogrtest.check_feature_geometry( feat, 'POINT (4.30000019 5.36999989)')\
               != 0:
            return 'fail'
        
        feat.Destroy()
        feat = None

    feat = gdaltest.dods_normalized.GetNextFeature()
    if feat is not None:
        feat.Destroy()
        gdaltest.post_reason( 'got more than expected number of features.' )
        return 'fail'

    return 'success'

###############################################################################
# Read the "lines" from from the same server and verify some values.
#

def ogr_dods_4():

    if gdaltest.dods_ds is None:
        return 'skip'

    gdaltest.dods_lines.ResetReading()
    feat = gdaltest.dods_lines.GetNextFeature()

    if feat.GetField('time') != -1936483200000:
        gdaltest.post_reason( 'time wrong' )
        return 'fail'
    
    if feat.GetField('profile.depth') != [0,10,20,30,39]:
        gdaltest.post_reason( 'depth wrong' )
        return 'fail'

    wkt_geom = 'LINESTRING (0.00000000 14.81000042,10.00000000 14.81000042,20.00000000 14.81000042,30.00000000 14.60999966,39.00000000 14.60999966)'

    if ogrtest.check_feature_geometry( feat, wkt_geom ) != 0:
        print(feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    feat.Destroy()

    feat = gdaltest.dods_lines.GetNextFeature()
    if feat is not None:
        feat.Destroy()
        gdaltest.post_reason( 'got more than expected number of features.' )
        return 'fail'

    return 'success'


###############################################################################
# Simple 1D Grid.
#

def ogr_dods_5():

    if ogrtest.dods_drv is None:
        return 'skip'
    
    srv = 'http://uhslc1.soest.hawaii.edu/cgi-bin/nph-nc/fast/m004.nc.dds'
    if gdaltest.gdalurlopen(srv) is None:
        return 'skip'

    grid_ds = ogr.Open( 'DODS:' + srv )
    if grid_ds is None:
        return 'fail'

    lat_lyr = grid_ds.GetLayerByName( 'latitude' )

    expect = [-0.53166663646698]
    tr = ogrtest.check_features_against_list( lat_lyr, 'latitude', expect )
    if tr == 0:
        return 'fail'

    return 'success'

###############################################################################
# 

def ogr_dods_cleanup():

    if gdaltest.dods_ds is None:
        return 'skip'

    gdaltest.dods_profiles = None
    gdaltest.dods_lines = None
    gdaltest.dods_normalized = None
    gdaltest.dods_ds.Destroy()
    gdaltest.dods_ds = None

    return 'success'

gdaltest_list = []

manual_gdaltest_list = [ 
    ogr_dods_1,
    ogr_dods_2,
    ogr_dods_3,
    ogr_dods_4,
    ogr_dods_5,
    ogr_dods_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_dods' )

    gdaltest.run_tests( manual_gdaltest_list )

    gdaltest.summarize()

