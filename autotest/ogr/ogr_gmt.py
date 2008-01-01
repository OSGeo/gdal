#!/usr/bin/env python
###############################################################################
# $Id: ogr_mem.py 13026 2007-11-25 19:20:48Z warmerdam $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR GMT driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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

import gdaltest
import ogrtest
import ogr
import gdal

###############################################################################
# Open Memory datasource.

def ogr_gmt_1():

    gmt_drv = ogr.GetDriverByName('GMT')
    gdaltest.gmt_ds = gmt_drv.CreateDataSource( 'tmp/tpoly.gmt' )

    if gdaltest.gmt_ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Create table from data/poly.shp

def ogr_gmt_2():

    #######################################################
    # Create gmtory Layer
    gdaltest.gmt_lyr = gdaltest.gmt_ds.CreateLayer( 'tpoly' )

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.gmt_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString) ] )
    
    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.gmt_lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)
    
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.gmt_lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()

    gdaltest.gmt_lyr = None

    gdaltest.gmt_ds.Destroy()
    gdaltest.gmt_ds = None

    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.

def ogr_gmt_3():

    gdaltest.gmt_ds = ogr.Open( 'tmp/tpoly.gmt' )
    gdaltest.gmt_lyr = gdaltest.gmt_ds.GetLayer(0)
    
    expect = [168, 169, 166, 158, 165]
    
    gdaltest.gmt_lyr.SetAttributeFilter( 'eas_id < 170' )
    tr = ogrtest.check_features_against_list( gdaltest.gmt_lyr,
                                              'eas_id', expect )
    gdaltest.gmt_lyr.SetAttributeFilter( None )

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.gmt_lyr.GetNextFeature()

        if ogrtest.check_feature_geometry(read_feat,orig_feat.GetGeometryRef(),
                                          max_error = 0.000000001 ) != 0:
            return 'fail'

        for fld in range(3):
            if orig_feat.GetField(fld) != read_feat.GetField(fld):
                gdaltest.post_reason( 'Attribute %d does not match' % fld )
                return 'fail'

        read_feat.Destroy()
        orig_feat.Destroy()

    gdaltest.poly_feat = None
    gdaltest.shp_ds.Destroy()

    gdaltest.gmt_lyr = None

    gdaltest.gmt_ds.Destroy()
    gdaltest.gmt_ds = None

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# 

def ogr_gmt_cleanup():

    if gdaltest.gmt_ds is not None:
        gdaltest.gmt_ds.Destroy()
        gdaltest.gmt_ds = None

    gdaltest.clean_tmp()

    return 'success'

gdaltest_list = [ 
    ogr_gmt_1,
    ogr_gmt_2,
    ogr_gmt_3,
    ogr_gmt_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gmt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

