#!/usr/bin/env python
###############################################################################
# $Id: ogr_mem.py 13026 2007-11-25 19:20:48Z warmerdam $
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

    gdaltest.xplane_apt_ds = ogr.Open( 'data/apt.dat' )

    if gdaltest.xplane_apt_ds is None:
        return 'fail'

    layers = [ ( 'APT'                  , 6 ),
               ( 'RunwayPolygon'        , 12 ),
               ( 'RunwayThreshold'      , 27 ),
               ( 'WaterRunwayPolygon'   , 1 ),
               ( 'WaterRunwayThreshold' , 2 ),
               ( 'Helipad'              , 1 ), 
               ( 'HelipadPolygon'       , 1 ),
               ( 'TaxiwayRectangle'     , 437 ),
               ( 'Pavement'             , 11 ),
               ( 'APTBoundary'          , 1 ),
               ( 'APTLinearFeature'     , 45 ),
               ( 'ATCFreq'              , 42 ),
               ( 'StartupLocation'      , 110 ),
               ( 'APTLightBeacon'       , 3 ),
               ( 'APTWindsock'          , 25 ),
               ( 'TaxiwaySign'          , 17 ),
               ( 'VASI_PAPI_WIGWAG'     , 24 )
             ]

    for layer in layers:
        lyr = gdaltest.xplane_apt_ds.GetLayerByName( layer[0] )
        if lyr.GetFeatureCount() != layer[1] :
            gdaltest.post_reason( 'wrong number of features for layer %s : %d. %d were expected ' % (layer[0], lyr.GetFeatureCount(), layer[1]) )
            return 'fail'

    return 'success'


###############################################################################
# Test nav.dat reading

def ogr_xplane_nav_dat():

    gdaltest.xplane_nav_ds = ogr.Open( 'data/nav.dat' )

    if gdaltest.xplane_nav_ds is None:
        return 'fail'

    layers = [ ( 'ILS'                  , 6 ),
               ( 'VOR'                  , 3 ),
               ( 'NDB'                  , 4 ),
               ( 'GS'                   , 1 ),
               ( 'Marker'               , 3 ),
               ( 'DME'                  , 6 ),
               ( 'DMEILS'               , 1 )
             ]

    for layer in layers:
        lyr = gdaltest.xplane_nav_ds.GetLayerByName( layer[0] )
        if lyr.GetFeatureCount() != layer[1] :
            gdaltest.post_reason( 'wrong number of features for layer %s : %d. %d were expected ' % (layer[0], lyr.GetFeatureCount(), layer[1]) )
            return 'fail'

    return 'success'


###############################################################################
# Test awy.dat reading

def ogr_xplane_awy_dat():

    gdaltest.xplane_awy_ds = ogr.Open( 'data/awy.dat' )

    if gdaltest.xplane_awy_ds is None:
        return 'fail'

    layers = [ ( 'AirwaySegment'        , 11 ),
               ( 'AirwayIntersection'   , 14 )
             ]

    for layer in layers:
        lyr = gdaltest.xplane_awy_ds.GetLayerByName( layer[0] )
        if lyr.GetFeatureCount() != layer[1] :
            gdaltest.post_reason( 'wrong number of features for layer %s : %d. %d were expected ' % (layer[0], lyr.GetFeatureCount(), layer[1]) )
            return 'fail'

    return 'success'

###############################################################################
# Test fix.dat reading

def ogr_xplane_fix_dat():

    gdaltest.xplane_fix_ds = ogr.Open( 'data/fix.dat' )

    if gdaltest.xplane_fix_ds is None:
        return 'fail'

    layers = [ ( 'FIX'                  , 1 )
             ]

    for layer in layers:
        lyr = gdaltest.xplane_fix_ds.GetLayerByName( layer[0] )
        if lyr.GetFeatureCount() != layer[1] :
            gdaltest.post_reason( 'wrong number of features for layer %s : %d. %d were expected ' % (layer[0], lyr.GetFeatureCount(), layer[1]) )
            return 'fail'

    return 'success'

###############################################################################
# 

gdaltest_list = [ 
    ogr_xplane_apt_dat,
    ogr_xplane_nav_dat,
    ogr_xplane_awy_dat,
    ogr_xplane_fix_dat ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_xplane' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

