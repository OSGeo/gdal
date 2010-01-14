#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR XPlane driver functionality.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at mines dash paris dot org>
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
import string

sys.path.append( '../pymod' )

import ogrtest
import gdaltest
import ogr

###############################################################################
# Test apt.dat reading

def ogr_xplane_apt_dat():

    xplane_apt_ds = ogr.Open( 'data/apt.dat' )

    if xplane_apt_ds is None:
        return 'fail'

    layers = [ ( 'APT'                  , 8,   [ ('apt_icao', 'E46') ] ),
               ( 'RunwayPolygon'        , 19,  [ ('apt_icao', 'E46') ] ),
               ( 'RunwayThreshold'      , 44,  [ ('apt_icao', 'E46') ] ),
               ( 'WaterRunwayPolygon'   , 1,   [ ('apt_icao', 'I38') ] ),
               ( 'WaterRunwayThreshold' , 2,   [ ('apt_icao', 'I38') ] ),
               ( 'Helipad'              , 2,   [ ('apt_icao', 'CYXX') ] ), 
               ( 'HelipadPolygon'       , 2,   [ ('apt_icao', 'CYXX') ]  ),
               ( 'TaxiwayRectangle'     , 437, [ ('apt_icao', 'LFPG') ] ),
               ( 'Pavement'             , 11,  [ ('apt_icao', 'CYXX') ] ),
               ( 'APTBoundary'          , 1,   [ ('apt_icao', 'VTX2') ] ),
               ( 'APTLinearFeature'     , 45,  [ ('apt_icao', 'CYXX') ] ),
               ( 'ATCFreq'              , 42,  [ ('apt_icao', 'CYXX') ] ),
               ( 'StartupLocation'      , 110, [ ('apt_icao', 'CYXX') ] ),
               ( 'APTLightBeacon'       , 3,   [ ('apt_icao', 'CYXX') ] ),
               ( 'APTWindsock'          , 25,  [ ('apt_icao', 'E46') ] ),
               ( 'TaxiwaySign'          , 17,  [ ('apt_icao', 'CYXX') ] ),
               ( 'VASI_PAPI_WIGWAG'     , 30,  [ ('apt_icao', 'CYXX') ] ),
               ( 'Stopway'              , 6,   [ ('apt_icao', 'LFPM') ] ), 
             ]

    for layer in layers:
        lyr = xplane_apt_ds.GetLayerByName( layer[0] )
        if lyr.GetFeatureCount() != layer[1] :
            gdaltest.post_reason( 'wrong number of features for layer %s : %d. %d were expected ' % (layer[0], lyr.GetFeatureCount(), layer[1]) )
            return 'fail'
        feat_read = lyr.GetNextFeature()
        for item in layer[2]:
            if feat_read.GetField(item[0]) != item[1]:
                print(layer[0])
                print(item[0])
                print(feat_read.GetField(item[0]))
                return 'fail'

    return 'success'


###############################################################################
# Test apt.dat v810 reading

def ogr_xplane_apt_v810_dat():

    xplane_apt_ds = ogr.Open( 'data/apt810/apt.dat' )

    if xplane_apt_ds is None:
        return 'fail'

    layers = [ ( 'APT'                  , 6,   [ ('apt_icao', 'UHP1') ] ),
               ( 'RunwayPolygon'        , 6,   [ ('apt_icao', 'UHP1') ] ),
               ( 'RunwayThreshold'      , 13,   [ ('apt_icao', 'UHP1') ] ),
               ( 'WaterRunwayPolygon'   , 2,   [ ('apt_icao', '6MA8') ] ),
               ( 'WaterRunwayThreshold' , 4,   [ ('apt_icao', '6MA8') ] ),
               ( 'Helipad'              , 1,   [ ('apt_icao', '9FD6') ] ), 
               ( 'HelipadPolygon'       , 1,   [ ('apt_icao', '9FD6') ] ),
               ( 'TaxiwayRectangle'     , 54,  [ ('apt_icao', 'UHP1') ] ),
               ( 'Pavement'             , 0,   [ ] ),
               ( 'APTBoundary'          , 0,   [ ] ),
               ( 'APTLinearFeature'     , 0,   [ ] ),
               ( 'ATCFreq'              , 10,  [ ('apt_icao', 'EHVB') ] ),
               ( 'StartupLocation'      , 0,   [ ] ),
               ( 'APTLightBeacon'       , 2,   [ ('apt_icao', '7I6') ] ),
               ( 'APTWindsock'          , 9,   [ ('apt_icao', 'UHP1') ] ),
               ( 'TaxiwaySign'          , 0,   [ ] ),
               ( 'VASI_PAPI_WIGWAG'     , 12,  [ ('apt_icao', 'UHP1') ] ),
               ( 'Stopway'              , 4,   [ ('apt_icao', 'EKYT' ) ] ), 
             ]

    for layer in layers:
        lyr = xplane_apt_ds.GetLayerByName( layer[0] )
        if lyr.GetFeatureCount() != layer[1] :
            gdaltest.post_reason( 'wrong number of features for layer %s : %d. %d were expected ' % (layer[0], lyr.GetFeatureCount(), layer[1]) )
            return 'fail'
        feat_read = lyr.GetNextFeature()
        for item in layer[2]:
            if feat_read.GetField(item[0]) != item[1]:
                print(layer[0])
                print(item[0])
                print(feat_read.GetField(item[0]))
                return 'fail'

    return 'success'

###############################################################################
# Test nav.dat reading

def ogr_xplane_nav_dat():

    xplane_nav_ds = ogr.Open( 'data/nav.dat' )

    if xplane_nav_ds is None:
        return 'fail'

    layers = [ ( 'ILS'                  , 6, [ ('navaid_id', 'IMQS') ] ),
               ( 'VOR'                  , 3, [ ('navaid_id', 'AAL') ] ),
               ( 'NDB'                  , 4, [ ('navaid_id', 'APH') ] ),
               ( 'GS'                   , 1, [ ('navaid_id', 'IMQS') ] ),
               ( 'Marker'               , 3, [ ('apt_icao', '40N') ] ),
               ( 'DME'                  , 6, [ ('navaid_id', 'AAL') ] ),
               ( 'DMEILS'               , 1, [ ('navaid_id', 'IWG') ] )
             ]

    for layer in layers:
        lyr = xplane_nav_ds.GetLayerByName( layer[0] )
        if lyr.GetFeatureCount() != layer[1] :
            gdaltest.post_reason( 'wrong number of features for layer %s : %d. %d were expected ' % (layer[0], lyr.GetFeatureCount(), layer[1]) )
            return 'fail'
        feat_read = lyr.GetNextFeature()
        for item in layer[2]:
            if feat_read.GetField(item[0]) != item[1]:
                print(layer[0])
                print(item[0])
                print(feat_read.GetField(item[0]))
                return 'fail'

    xplane_nav_ds = None

    return 'success'


###############################################################################
# Test awy.dat reading

def ogr_xplane_awy_dat():

    xplane_awy_ds = ogr.Open( 'data/awy.dat' )

    if xplane_awy_ds is None:
        return 'fail'

    layers = [ ( 'AirwaySegment'        , 11, [ ('segment_name', 'R464') ] ),
               ( 'AirwayIntersection'   , 14, [ ('name', '00MKK') ] )
             ]

    for layer in layers:
        lyr = xplane_awy_ds.GetLayerByName( layer[0] )
        if lyr.GetFeatureCount() != layer[1] :
            gdaltest.post_reason( 'wrong number of features for layer %s : %d. %d were expected ' % (layer[0], lyr.GetFeatureCount(), layer[1]) )
            return 'fail'
        feat_read = lyr.GetNextFeature()
        for item in layer[2]:
            if feat_read.GetField(item[0]) != item[1]:
                print(layer[0])
                print(item[0])
                print(feat_read.GetField(item[0]))
                return 'fail'

    return 'success'

###############################################################################
# Test fix.dat reading

def ogr_xplane_fix_dat():

    xplane_fix_ds = ogr.Open( 'data/fix.dat' )

    if xplane_fix_ds is None:
        return 'fail'

    layers = [ ( 'FIX'                  , 1, [ ('fix_name', '00MKK') ] )
             ]

    for layer in layers:
        lyr = xplane_fix_ds.GetLayerByName( layer[0] )
        if lyr.GetFeatureCount() != layer[1] :
            gdaltest.post_reason( 'wrong number of features for layer %s : %d. %d were expected ' % (layer[0], lyr.GetFeatureCount(), layer[1]) )
            return 'fail'
        feat_read = lyr.GetNextFeature()
        for item in layer[2]:
            if feat_read.GetField(item[0]) != item[1]:
                print(layer[0])
                print(item[0])
                print(feat_read.GetField(item[0]))
                return 'fail'

    return 'success'

###############################################################################
# 

gdaltest_list = [ 
    ogr_xplane_apt_dat,
    ogr_xplane_apt_v810_dat,
    ogr_xplane_nav_dat,
    ogr_xplane_awy_dat,
    ogr_xplane_fix_dat ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_xplane' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

