#!/usr/bin/env python
###############################################################################
# $Id: ogr_sqlite.py 11065 2007-03-24 09:35:32Z mloskot $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SQLite driver functionality.
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
# Test points bna layer.

def ogr_bna_1():

    gdaltest.bna_ds = ogr.Open( 'data/test.bna' )

    lyr = gdaltest.bna_ds.GetLayerByName( 'test_points' )

    expect = ['PID5', 'PID4']

    tr = ogrtest.check_features_against_list( lyr, 'Primary ID', expect )

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT (573.736 476.563)',
                                       max_error = 0.0001 ) != 0:
        return 'fail'
    feat.Destroy()
    
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POINT (532.991 429.121)',
                                       max_error = 0.0001 ) != 0:
        return 'fail'
    feat.Destroy()
    
    return 'success'

###############################################################################
# Test lines bna layer.

def ogr_bna_2():

    gdaltest.bna_ds = ogr.Open( 'data/test.bna' )

    lyr = gdaltest.bna_ds.GetLayerByName( 'test_lines' )

    expect = ['PID3']

    tr = ogrtest.check_features_against_list( lyr, 'Primary ID', expect )

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'LINESTRING (224.598 307.425,333.043 341.461,396.629 304.952)', max_error = 0.0001 ) != 0:
        return 'fail'
    feat.Destroy()
    
    return 'success'

###############################################################################
# 

def ogr_bna_cleanup():

    gdaltest.bna_ds.Destroy()
    gdaltest.bna_ds = None
    return 'success'

gdaltest_list = [ 
    ogr_bna_1,
    ogr_bna_2,
    ogr_bna_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_bna' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

