#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GeoPackage driver functionality.
# Author:   Paul Ramsey <pramsey@boundlessgeom.com>
# 
###############################################################################
# Copyright (c) 2004, Paul Ramsey <pramsey@boundlessgeom.com>
# Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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
import shutil

# Make sure we run from the directory of the script
if os.path.basename(sys.argv[0]) == os.path.basename(__file__):
    if os.path.dirname(sys.argv[0]) != '':
        os.chdir(os.path.dirname(sys.argv[0]))

sys.path.append( '../pymod' )

from osgeo import ogr, osr, gdal
import gdaltest
import ogrtest

###############################################################################
# Create a fresh database.

def ogr_gpkg_1():

    gdaltest.gpkg_ds = None
    gdaltest.gpkg_dr = None

    try:
        gdaltest.gpkg_dr = ogr.GetDriverByName( 'GPKG' )
        if gdaltest.gpkg_dr is None:
            return 'skip'
    except:
        return 'skip'

    try:
        os.remove( 'tmp/gpkg_test.gpkg' )
    except:
        pass

    gdaltest.gpkg_ds = gdaltest.gpkg_dr.CreateDataSource( 'tmp/gpkg_test.gpkg' )

    if gdaltest.gpkg_ds is not None:
        return 'success'
    else:
        return 'fail'

    gdaltest.gpkg_ds.Destroy()


###############################################################################
# Re-open database to test validity

def ogr_gpkg_2():

    if gdaltest.gpkg_dr is None: 
        return 'skip'

    gdaltest.gpkg_ds = gdaltest.gpkg_dr.Open( 'tmp/gpkg_test.gpkg', update = 1 )

    if gdaltest.gpkg_ds is not None:
        return 'success'
    else:
        return 'fail'


###############################################################################
# Create a layer

def ogr_gpkg_3():

    if gdaltest.gpkg_dr is None or gdaltest.gpkg_ds is None:
        return 'skip'

    # Test invalid FORMAT
    #gdal.PushErrorHandler('CPLQuietErrorHandler')
    srs4326 = osr.SpatialReference()
    srs4326.ImportFromEPSG( 4326 )
    lyr = gdaltest.gpkg_ds.CreateLayer( 'first_layer', geom_type = ogr.wkbPoint, srs = srs4326)
    #gdal.PopErrorHandler()
    if lyr is None:
        return 'fail'

    # Test creating a layer with an existing name
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.gpkg_ds.CreateLayer( 'a_layer')
    lyr = gdaltest.gpkg_ds.CreateLayer( 'a_layer' )
    gdal.PopErrorHandler()
    if lyr is not None:
        gdaltest.post_reason('layer creation should have failed')
        return 'fail'

    return 'success'

###############################################################################
# Close and re-open to test the layer registration

def ogr_gpkg_4():

    if gdaltest.gpkg_dr is None or gdaltest.gpkg_ds is None:
        return 'skip'

    gdaltest.gpkg_ds.Destroy()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdaltest.gpkg_ds = gdaltest.gpkg_dr.Open( 'tmp/gpkg_test.gpkg', update = 1 )
    gdal.PopErrorHandler()

    if gdaltest.gpkg_ds is None:
        return 'fail'

    if gdaltest.gpkg_ds.GetLayerCount() != 2:
        gdaltest.post_reason( 'unexpected number of layers' )
        return 'fail'
        
    lyr0 = gdaltest.gpkg_ds.GetLayer(0)
    lyr1 = gdaltest.gpkg_ds.GetLayer(1)

    if lyr0.GetName() != 'first_layer':
        gdaltest.post_reason( 'unexpected layer name for layer 0' )
        return 'fail'

    if lyr1.GetName() != 'a_layer':
        gdaltest.post_reason( 'unexpected layer name for layer 1' )
        return 'fail'
        
    return 'success'


###############################################################################
# Delete a layer

def ogr_gpkg_5():

    if gdaltest.gpkg_dr is None or gdaltest.gpkg_ds is None:
        return 'skip'

    if gdaltest.gpkg_ds.GetLayerCount() != 2:
        gdaltest.post_reason( 'unexpected number of layers' )
        return 'fail'

    if gdaltest.gpkg_ds.DeleteLayer(1) != 0:
        gdaltest.post_reason( 'got error code from DeleteLayer(1)' )
        return 'fail'

    if gdaltest.gpkg_ds.DeleteLayer(0) != 0:
        gdaltest.post_reason( 'got error code from DeleteLayer(0)' )
        return 'fail'

    if gdaltest.gpkg_ds.GetLayerCount() != 0:
        gdaltest.post_reason( 'unexpected number of layers (not 0)' )
        return 'fail'

    return 'success'


###############################################################################
# Add fields 

def ogr_gpkg_6():

    if gdaltest.gpkg_dr is None or gdaltest.gpkg_ds is None:
        return 'skip'

    srs4326 = osr.SpatialReference()
    srs4326.ImportFromEPSG( 4326 )
    lyr = gdaltest.gpkg_ds.CreateLayer( 'field_test_layer', geom_type = ogr.wkbPoint, srs = srs4326)
    if lyr is None:
        return 'fail'

    field_defn = ogr.FieldDefn('dummy', ogr.OFTString)
    ret = lyr.CreateField(field_defn)

    if lyr.GetLayerDefn().GetFieldDefn(0).GetType() != ogr.OFTString:
        gdaltest.post_reason( 'wrong field type' )
        return 'fail'
    
    gdaltest.gpkg_ds.Destroy()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdaltest.gpkg_ds = gdaltest.gpkg_dr.Open( 'tmp/gpkg_test.gpkg', update = 1 )
    gdal.PopErrorHandler()

    if gdaltest.gpkg_ds is None:
        return 'fail'

    if gdaltest.gpkg_ds.GetLayerCount() != 1:
        return 'fail'
        
    lyr = gdaltest.gpkg_ds.GetLayer(0)
    if lyr.GetName() != 'field_test_layer':
        return 'fail'
        
    field_defn_out = lyr.GetLayerDefn().GetFieldDefn(0)
    if field_defn_out.GetType() != ogr.OFTString:
        gdaltest.post_reason( 'wrong field type after reopen' )
        return 'fail'
        
    if field_defn_out.GetName() != 'dummy':
        gdaltest.post_reason( 'wrong field name after reopen' )
        return 'fail'
    
    return 'success'


###############################################################################
# Add a feature / read a feature / delete a feature

def ogr_gpkg_7():

    if gdaltest.gpkg_dr is None or gdaltest.gpkg_ds is None:
        return 'skip'

    lyr = gdaltest.gpkg_ds.GetLayerByName('field_test_layer')
    geom = ogr.CreateGeometryFromWkt('POINT(10 10)')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    feat.SetField('dummy', 'a dummy value')
    if lyr.CreateFeature(feat) != 0:
        gdaltest.post_reason('cannot create feature')
        return 'fail'

    # Read back what we just inserted
    lyr.ResetReading()
    feat_read = lyr.GetNextFeature()
    if feat_read.GetField('dummy') != 'a dummy value':
        gdaltest.post_reason('output does not match input')
        return 'fail'

    # Only inserted one thing, so second feature should return NULL
    feat_read = lyr.GetNextFeature()
    if feat_read is not None:
        gdaltest.post_reason('last call should return NULL')
        return 'fail'

    # Add another feature
    geom = ogr.CreateGeometryFromWkt('POINT(100 100)')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    feat.SetField('dummy', 'who you calling a dummy?')
    if lyr.CreateFeature(feat) != 0:
        gdaltest.post_reason('cannot create feature')
        return 'fail'

    # Random read a feature
    feat_read_random = lyr.GetFeature(feat.GetFID())
    if feat_read_random.GetField('dummy') != 'who you calling a dummy?':
        gdaltest.post_reason('random read output does not match input')
        return 'fail'

    # Random write a feature
    feat.SetField('dummy', 'i am no dummy')
    lyr.SetFeature(feat)
    feat_read_random = lyr.GetFeature(feat.GetFID())
    if feat_read_random.GetField('dummy') != 'i am no dummy':
        gdaltest.post_reason('random read output does not match random write input')
        return 'fail'

    # Delete a feature
    lyr.DeleteFeature(feat.GetFID())
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('delete feature did not delete')
        return 'fail'
        
    # Delete the layer
    if gdaltest.gpkg_ds.DeleteLayer('field_test_layer') != 0:
        gdaltest.post_reason( 'got error code from DeleteLayer(field_test_layer)' )
    
    return 'success'


###############################################################################
# Test a variety of geometry feature types and attribute types

def ogr_gpkg_8():

    # try:
    #     os.remove( 'tmp/gpkg_test.gpkg' )
    # except:
    #     pass
    # gdaltest.gpkg_dr = ogr.GetDriverByName( 'GPKG' )
    # gdaltest.gpkg_ds = gdaltest.gpkg_dr.CreateDataSource( 'tmp/gpkg_test.gpkg' )

    if gdaltest.gpkg_dr is None or gdaltest.gpkg_ds is None:
        return 'skip'

    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4326 )

    lyr = gdaltest.gpkg_ds.CreateLayer( 'tbl_linestring', geom_type = ogr.wkbLineString, srs = srs)
    if lyr is None:
        return 'fail'
    
    ret = lyr.CreateField(ogr.FieldDefn('fld_integer', ogr.OFTInteger))
    ret = lyr.CreateField(ogr.FieldDefn('fld_string', ogr.OFTString))
    ret = lyr.CreateField(ogr.FieldDefn('fld_real', ogr.OFTReal))
    
    geom = ogr.CreateGeometryFromWkt('LINESTRING(5 5,10 5,10 10,5 10)')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    
    for i in range(10):
        feat.SetField('fld_integer', 10 + i)
        feat.SetField('fld_real', 3.14159/(i+1) )
        feat.SetField('fld_string', 'test string %d test' % i)
    
        if lyr.CreateFeature(feat) != 0:
            gdaltest.post_reason('cannot create feature %d' % i)
            return 'fail'
                        
    
    feat = ogr.Feature(lyr.GetLayerDefn())
    if lyr.CreateFeature(feat) != 0:
        gdaltest.post_reason('cannot insert empty')
        return 'fail'
        
    feat.SetFID(2)
    if lyr.SetFeature(feat) != 0:
        gdaltest.post_reason('cannot update with empty')
        return 'fail'

    lyr = gdaltest.gpkg_ds.CreateLayer( 'tbl_polygon', geom_type = ogr.wkbPolygon, srs = srs)
    if lyr is None:
        return 'fail'

    ret = lyr.CreateField(ogr.FieldDefn('fld_datetime', ogr.OFTDateTime))
    ret = lyr.CreateField(ogr.FieldDefn('fld_string', ogr.OFTString))

    geom = ogr.CreateGeometryFromWkt('POLYGON((5 5, 10 5, 10 10, 5 10, 5 5),(6 6, 6 7, 7 7, 7 6, 6 6))')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)

    for i in range(10):
        feat.SetField('fld_string', 'my super string %d' % i)
        feat.SetField('fld_datetime', '2010-01-01' )

        if lyr.CreateFeature(feat) != 0:
            gdaltest.post_reason('cannot create polygon feature %d' % i)
            return 'fail'

    feat = lyr.GetFeature(3)
    geom_read = feat.GetGeometryRef()
    if geom.ExportToWkt() != geom_read.ExportToWkt():
        gdaltest.post_reason('geom output not equal to geom input')
        return 'fail'

    # Test out the 3D support...
    lyr = gdaltest.gpkg_ds.CreateLayer( 'tbl_polygon25d', geom_type = ogr.wkbPolygon25D, srs = srs)
    if lyr is None:
        return 'fail'
        
    ret = lyr.CreateField(ogr.FieldDefn('fld_string', ogr.OFTString))
    geom = ogr.CreateGeometryFromWkt('POLYGON((5 5 1, 10 5 2, 10 10 3, 5 104 , 5 5 1),(6 6 4, 6 7 5, 7 7 6, 7 6 7, 6 6 4))')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    geom_read = feat.GetGeometryRef()
    if geom.ExportToWkt() != geom_read.ExportToWkt():
        gdaltest.post_reason('3d geom output not equal to geom input')
        return 'fail'
    
    
    return 'success'

###############################################################################
# Test support for extents and counts

def ogr_gpkg_9():

    if gdaltest.gpkg_dr is None or gdaltest.gpkg_ds is None:
        return 'skip'

    lyr = gdaltest.gpkg_ds.GetLayerByName('tbl_linestring')
    extent = lyr.GetExtent()
    if extent != (5.0, 10.0, 5.0, 10.0):
        gdaltest.post_reason('got bad extent')
        print(extent)
        return 'fail'
    
    fcount = lyr.GetFeatureCount()
    if fcount != 11:
        gdaltest.post_reason('got bad featurecount')
        print(fcount)
        return 'fail'
    
    
    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_gpkg_10():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    gdaltest.gpkg_ds = None

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/gpkg_test.gpkg')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'
    
###############################################################################
# Remove the test db from the tmp directory

def ogr_gpkg_cleanup():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    gdaltest.gpkg_ds = None

    try:
        os.remove( 'tmp/gpkg_test.gpkg' )
    except:
        pass

    return 'success'

###############################################################################


gdaltest_list = [ 
    ogr_gpkg_1,
    ogr_gpkg_2,
    ogr_gpkg_3,
    ogr_gpkg_4,
    ogr_gpkg_5,
    ogr_gpkg_6,
    ogr_gpkg_7,
    ogr_gpkg_8,
    ogr_gpkg_9,
    ogr_gpkg_10,
    ogr_gpkg_cleanup,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gpkg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

