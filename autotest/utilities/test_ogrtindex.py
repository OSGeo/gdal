#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogrtindex testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault @ mines-paris dot org>
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

import sys
import os

sys.path.append( '../pymod' )

from osgeo import ogr
from osgeo import osr
import gdaltest
import test_cli_utilities

###############################################################################
# Simple test

def test_ogrtindex_1(srs = None):
    if test_cli_utilities.get_ogrtindex_path() is None:
        return 'skip'

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    
    for basename in ['tileindex', 'point1', 'point2', 'point3', 'point4']:
        for extension in ['shp', 'dbf', 'shx', 'prj']:
            try:
                os.remove('tmp/%s.%s' % (basename, extension))
            except:
                pass

    shape_ds = shape_drv.CreateDataSource( 'tmp' )

    shape_lyr = shape_ds.CreateLayer( 'point1', srs = srs)
    dst_feat = ogr.Feature( feature_def = shape_lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(49 2)'))
    shape_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    shape_lyr = shape_ds.CreateLayer( 'point2', srs = srs )
    dst_feat = ogr.Feature( feature_def = shape_lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(49 3)'))
    shape_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    shape_lyr = shape_ds.CreateLayer( 'point3', srs = srs )
    dst_feat = ogr.Feature( feature_def = shape_lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(48 2)'))
    shape_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    shape_lyr = shape_ds.CreateLayer( 'point4', srs = srs )
    dst_feat = ogr.Feature( feature_def = shape_lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(48 3)'))
    shape_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    shape_ds.Destroy()

    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrtindex_path() + ' -skip_different_projection tmp/tileindex.shp tmp/point1.shp tmp/point2.shp tmp/point3.shp tmp/point4.shp')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

    ds = ogr.Open('tmp/tileindex.shp')
    if ds.GetLayer(0).GetFeatureCount() != 4:
        gdaltest.post_reason('did not get expected feature count')
        return 'fail'
        
    if srs is not None:
        if ds.GetLayer(0).GetSpatialRef() == None or not ds.GetLayer(0).GetSpatialRef().IsSame(srs):
            gdaltest.post_reason('did not get expected spatial ref')
            return 'fail'
    else:
        if ds.GetLayer(0).GetSpatialRef() is not None:
            gdaltest.post_reason('did not get expected spatial ref')
            return 'fail'

    expected_wkts =['POLYGON ((49 2,49 2,49 2,49 2,49 2))',
                    'POLYGON ((49 3,49 3,49 3,49 3,49 3))',
                    'POLYGON ((48 2,48 2,48 2,48 2,48 2))',
                    'POLYGON ((48 3,48 3,48 3,48 3,48 3))' ]
    i = 0
    feat = ds.GetLayer(0).GetNextFeature()
    while feat is not None:
        if feat.GetGeometryRef().ExportToWkt() != expected_wkts[i]:
            print('i=%d, wkt=%s' % (i, feat.GetGeometryRef().ExportToWkt()))
            return 'fail'
        i = i + 1
        feat = ds.GetLayer(0).GetNextFeature()
    ds.Destroy()

    return 'success'

###############################################################################
# Same test but with a SRS set on the different tiles to index

def test_ogrtindex_2():
    
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    return test_ogrtindex_1(srs)

###############################################################################
# Cleanup

def test_ogrtindex_cleanup():

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    shape_drv.DeleteDataSource('tmp/tileindex.shp')
    shape_drv.DeleteDataSource('tmp/point1.shp')
    shape_drv.DeleteDataSource('tmp/point2.shp')
    shape_drv.DeleteDataSource('tmp/point3.shp')
    shape_drv.DeleteDataSource('tmp/point4.shp')

    return 'success'


gdaltest_list = [
    test_ogrtindex_1,
    test_ogrtindex_2,
    test_ogrtindex_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_ogrtindex' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()





