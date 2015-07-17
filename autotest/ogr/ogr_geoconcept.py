#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR Geoconcept driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import osr

###############################################################################
# Simple read test of known file.

def ogr_gxt_1():

    gdaltest.gxt_ds = ogr.Open('data/expected_000_GRD.gxt' )

    if gdaltest.gxt_ds is None:
        return 'fail'

    if gdaltest.gxt_ds.GetLayerCount() != 1:
        gdaltest.post_reason( 'Got wrong layer count.' )
        return 'fail'

    lyr = gdaltest.gxt_ds.GetLayer(0)
    if lyr.GetName() != '000_GRD.000_GRD':
        gdaltest.post_reason( 'got unexpected layer name.' )
        return 'fail'

    if lyr.GetFeatureCount() != 10:
        gdaltest.post_reason( 'got wrong feature count.' )
        return 'fail'

    expect = [ '000-2007-0050-7130-LAMB93',
               '000-2007-0595-7130-LAMB93',
               '000-2007-0595-6585-LAMB93',
               '000-2007-1145-6250-LAMB93',
               '000-2007-0050-6585-LAMB93',
               '000-2007-0050-7130-LAMB93',
               '000-2007-0595-7130-LAMB93',
               '000-2007-0595-6585-LAMB93',
               '000-2007-1145-6250-LAMB93',
               '000-2007-0050-6585-LAMB93' ]
    
    tr = ogrtest.check_features_against_list( lyr, 'idSel', expect )
    if not tr:
        return 'fail'

    lyr.ResetReading()

    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat,
          'MULTIPOLYGON (((50000 7130000,600000 7130000,600000 6580000,50000 6580000,50000 7130000)))',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'
    
    srs = osr.SpatialReference()
    srs.SetFromUserInput('PROJCS["Lambert 93",GEOGCS["unnamed",DATUM["ITRS-89",SPHEROID["GRS 80",6378137,298.257222099657],TOWGS84[0,0,0,0,0,0,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",44],PARAMETER["standard_parallel_2",49],PARAMETER["latitude_of_origin",46.5],PARAMETER["central_meridian",3],PARAMETER["false_easting",700000],PARAMETER["false_northing",6600000]]')
    
    if not lyr.GetSpatialRef().IsSame(srs):
        gdaltest.post_reason('SRS is not the one expected.')
        return 'fail'

    return 'success'

###############################################################################
# Similar test than previous one with TAB separator.

def ogr_gxt_2():

    gdaltest.gxt_ds = ogr.Open('data/expected_000_GRD_TAB.txt' )

    if gdaltest.gxt_ds is None:
        return 'fail'

    if gdaltest.gxt_ds.GetLayerCount() != 1:
        gdaltest.post_reason( 'Got wrong layer count.' )
        return 'fail'

    lyr = gdaltest.gxt_ds.GetLayer(0)
    if lyr.GetName() != '000_GRD.000_GRD':
        gdaltest.post_reason( 'got unexpected layer name.' )
        return 'fail'

    if lyr.GetFeatureCount() != 5:
        gdaltest.post_reason( 'got wrong feature count.' )
        return 'fail'

    expect = [ '000-2007-0050-7130-LAMB93',
               '000-2007-0595-7130-LAMB93',
               '000-2007-0595-6585-LAMB93',
               '000-2007-1145-6250-LAMB93',
               '000-2007-0050-6585-LAMB93' ]
    
    tr = ogrtest.check_features_against_list( lyr, 'idSel', expect )
    if not tr:
        return 'fail'

    lyr.ResetReading()

    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat,
          'MULTIPOLYGON (((50000 7130000,600000 7130000,600000 6580000,50000 6580000,50000 7130000)))',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    return 'success'

###############################################################################
# Read a GXT file containing 2 points, duplicate it, and check the newly written file

def ogr_gxt_3():

    gdaltest.gxt_ds = None

    src_ds = ogr.Open( 'data/points.gxt' )

    try:
        os.remove ('tmp/tmp.gxt')
    except:
        pass

    # Duplicate all the points from the source GXT
    src_lyr = src_ds.GetLayerByName( 'points.points' )

    gdaltest.gxt_ds = ogr.GetDriverByName('Geoconcept').CreateDataSource('tmp/tmp.gxt')

    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS( 'WGS84' )

    gxt_lyr = gdaltest.gxt_ds.CreateLayer( 'points', srs, geom_type = ogr.wkbPoint )

    src_lyr.ResetReading()

    for i in range(src_lyr.GetLayerDefn().GetFieldCount()):
        field_defn = src_lyr.GetLayerDefn().GetFieldDefn(i)
        gxt_lyr.CreateField( field_defn )

    dst_feat = ogr.Feature( feature_def = gxt_lyr.GetLayerDefn() )

    feat = src_lyr.GetNextFeature()
    while feat is not None:
        dst_feat.SetFrom( feat )
        if gxt_lyr.CreateFeature( dst_feat ) != 0:
            gdaltest.post_reason('CreateFeature failed.')
            return 'fail'

        feat = src_lyr.GetNextFeature()

    gdaltest.gxt_ds = None


    # Read the newly written GXT file and check its features and geometries
    gdaltest.gxt_ds = ogr.Open('tmp/tmp.gxt')
    gxt_lyr = gdaltest.gxt_ds.GetLayerByName( 'points.points' )

    if not gxt_lyr.GetSpatialRef().IsSame(srs):
        gdaltest.post_reason('Output SRS is not the one expected.')
        return 'fail'

    expect = ['PID1', 'PID2']

    tr = ogrtest.check_features_against_list( gxt_lyr, 'Primary_ID', expect )
    if not tr:
        return 'fail'

    gxt_lyr.ResetReading()

    expect = ['SID1', 'SID2']

    tr = ogrtest.check_features_against_list( gxt_lyr, 'Secondary_ID', expect )
    if not tr:
        return 'fail'

    gxt_lyr.ResetReading()

    expect = ['TID1', None]

    tr = ogrtest.check_features_against_list( gxt_lyr, 'Third_ID', expect )
    if not tr:
        return 'fail'

    gxt_lyr.ResetReading()

    feat = gxt_lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat,'POINT(0 1)',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    feat = gxt_lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat,'POINT(2 3)',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    return 'success'


###############################################################################
#

def ogr_gxt_cleanup():

    gdaltest.gxt_ds = None
    try:
        os.remove ('tmp/tmp.gxt')
    except:
        pass
    return 'success'


gdaltest_list = [
    ogr_gxt_1,
    ogr_gxt_2,
    ogr_gxt_3,
    ogr_gxt_cleanup,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gxt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
