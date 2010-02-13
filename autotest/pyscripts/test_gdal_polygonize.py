#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test gdal_polygonize.py script
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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
import test_py_scripts

try:
    from osgeo import gdal, gdalconst, ogr
except:
    import gdal
    import gdalconst
    import ogr

###############################################################################
# Test a fairly simple case, with nodata masking.

def test_gdal_polygonize_1():

    #from osgeo import gdal, gdalconst, ogr

    try:
        x = gdal.Polygonize
        gdaltest.have_ng = 1
    except:
        gdaltest.have_ng = 0
        return 'skip'

    script_path = test_py_scripts.get_py_script('gdal_polygonize')
    if script_path is None:
        return 'skip'

    # Create a OGR datasource to put results in. 
    shp_drv = ogr.GetDriverByName( 'ESRI Shapefile' )
    try:
        os.stat('tmp/poly.shp')
        shp_drv.DeleteDataSource( 'tmp/poly.shp' )
    except:
        pass

    shp_ds = shp_drv.CreateDataSource( 'tmp/poly.shp' )

    shp_layer = shp_ds.CreateLayer( 'poly', None, ogr.wkbPolygon )

    fd = ogr.FieldDefn( 'DN', ogr.OFTInteger )
    shp_layer.CreateField( fd )

    shp_ds.Destroy()

    # run the algorithm.
    test_py_scripts.run_py_script(script_path, 'gdal_polygonize', '../alg/data/polygonize_in.grd tmp poly DN')

    # Confirm we get the set of expected features in the output layer.

    shp_ds = ogr.Open( 'tmp' )
    shp_lyr = shp_ds.GetLayerByName('poly')

    expected_feature_number = 13
    if shp_lyr.GetFeatureCount() != expected_feature_number:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of %d' % (mem_layer.GetFeatureCount(), expected_feature_number) )
        return 'fail'

    expect = [ 107, 123, 115, 115, 140, 148, 123, 140, 156,
               100, 101, 102, 103]
    
    tr = ogrtest.check_features_against_list( shp_lyr, 'DN', expect )

    # check at least one geometry.
    if tr:
        shp_lyr.SetAttributeFilter( 'dn = 156' )
        feat_read = shp_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry( feat_read, 'POLYGON ((440720 3751200,440900 3751200,440900 3751020,440720 3751020,440720 3751200),(440780 3751140,440780 3751080,440840 3751080,440840 3751140,440780 3751140))' ) != 0:
            tr = 0
        feat_read.Destroy()

    shp_ds.Destroy()
    # Reload drv because of side effects of run_py_script()
    shp_drv = ogr.GetDriverByName( 'ESRI Shapefile' )
    shp_drv.DeleteDataSource( 'tmp/poly.shp' )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test a simple case without masking.

def test_gdal_polygonize_2():

    if not gdaltest.have_ng:
        return 'skip'

    script_path = test_py_scripts.get_py_script('gdal_polygonize')
    if script_path is None:
        return 'skip'

    shp_drv = ogr.GetDriverByName( 'ESRI Shapefile' )
    try:
        os.stat('tmp/out.shp')
        shp_drv.DeleteDataSource( 'tmp/out.shp' )
    except:
        pass

    # run the algorithm.
    test_py_scripts.run_py_script(script_path, 'gdal_polygonize', '-b 1 -f "ESRI Shapefile" -q -nomask ../alg/data/polygonize_in.grd tmp' )

    # Confirm we get the set of expected features in the output layer.
    shp_ds = ogr.Open( 'tmp' )
    shp_lyr = shp_ds.GetLayerByName('out')

    expected_feature_number = 17
    if shp_lyr.GetFeatureCount() != expected_feature_number:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of %d' % (shp_lyr.GetFeatureCount(), expected_feature_number) )
        return 'fail'

    expect = [ 107, 123, 115, 132, 115, 132, 140, 132, 148, 123, 140,
               132, 156, 100, 101, 102, 103 ]
    
    tr = ogrtest.check_features_against_list( shp_lyr, 'DN', expect )

    shp_ds.Destroy()
    # Reload drv because of side effects of run_py_script()
    shp_drv = ogr.GetDriverByName( 'ESRI Shapefile' )
    shp_drv.DeleteDataSource( 'tmp/out.shp' )

    if tr:
        return 'success'
    else:
        return 'fail'

gdaltest_list = [
    test_gdal_polygonize_1,
    test_gdal_polygonize_2,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_polygonize' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

