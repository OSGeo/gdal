#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR CSV driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal

###############################################################################
# Open CSV datasource.

def ogr_csv_1():

    gdaltest.csv_ds = None
    gdaltest.csv_ds = ogr.Open( 'data/prime_meridian.csv' )

    if gdaltest.csv_ds is not None:
        return 'success'
    else:
        return 'fail'


###############################################################################
# Check layer

def ogr_csv_check_layer(lyr, expect_code_as_numeric):

    if expect_code_as_numeric is True:
        expect = [8901, 8902, 8903, 8904 ]
    else:
        expect = ['8901', '8902', '8903', '8904' ]

    tr = ogrtest.check_features_against_list( lyr,'PRIME_MERIDIAN_CODE',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()

    expect = [ '', 'Instituto Geografico e Cadastral; Lisbon',
               'Institut Geographique National (IGN), Paris',
               'Instituto Geografico "Augustin Cadazzi" (IGAC); Bogota' ]

    tr = ogrtest.check_features_against_list( lyr,'INFORMATION_SOURCE',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()

    return 'success'

###############################################################################
# Verify the some attributes read properly.
#

def ogr_csv_2():
    if gdaltest.csv_ds is None:
        return 'skip'

    with gdaltest.error_handler():
        if gdaltest.csv_ds.CreateLayer('foo') is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        if gdaltest.csv_ds.DeleteLayer(0) == 0:
            gdaltest.post_reason('fail')
            return 'fail'

    lyr = gdaltest.csv_ds.GetLayerByName( 'prime_meridian' )

    f = ogr.Feature(lyr.GetLayerDefn())
    with gdaltest.error_handler():
        if lyr.CreateField(ogr.FieldDefn('foo')) == 0:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.CreateFeature(f) == 0:
            gdaltest.post_reason('fail')
            return 'fail'

    return ogr_csv_check_layer(lyr, False)

###############################################################################
# Copy layer

def ogr_csv_copy_layer(layer_name, options):

    #######################################################
    # Create layer (.csv file)
    if options is None:
        new_lyr = gdaltest.csv_tmpds.CreateLayer( layer_name )
    else:
        new_lyr = gdaltest.csv_tmpds.CreateLayer( layer_name, options = options )

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( new_lyr,
                                    [ ('PRIME_MERIDIAN_CODE', ogr.OFTInteger),
                                      ('INFORMATION_SOURCE', ogr.OFTString) ] )

    #######################################################
    # Copy in matching prime meridian fields.

    dst_feat = ogr.Feature( feature_def = new_lyr.GetLayerDefn() )

    srclyr = gdaltest.csv_ds.GetLayerByName( 'prime_meridian' )
    srclyr.ResetReading()

    feat = srclyr.GetNextFeature()

    while feat is not None:

        dst_feat.SetFrom( feat )
        new_lyr.CreateFeature( dst_feat )

        feat = srclyr.GetNextFeature()

    return new_lyr

###############################################################################
# Copy prime_meridian.csv to a new subtree under the tmp directory.

def ogr_csv_3():
    if gdaltest.csv_ds is None:
        return 'skip'

    #######################################################
    # Ensure any old copy of our working datasource is cleaned up
    try:
        gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        ogr.GetDriverByName('CSV').DeleteDataSource( 'tmp/csvwrk' )
        gdal.PopErrorHandler()
    except:
        pass

    #######################################################
    # Create CSV datasource (directory)
    gdaltest.csv_tmpds = \
         ogr.GetDriverByName('CSV').CreateDataSource( 'tmp/csvwrk' )

    #######################################################
    # Create layer (.csv file)
    gdaltest.csv_lyr1 = ogr_csv_copy_layer( 'pm1', None )

    # No longer valid since we have editing capabilities
    if False:
        # Check that we cannot add a new field now
        if gdaltest.csv_lyr1.TestCapability(ogr.OLCCreateField) != 0:
            gdaltest.post_reason('fail')
            return 'fail'
        field_defn = ogr.FieldDefn('dummy', ogr.OFTString)
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ret = gdaltest.csv_lyr1.CreateField(field_defn)
        gdal.PopErrorHandler()
        if ret == 0:
            gdaltest.post_reason('fail')
            return 'fail'

    return 'success'


###############################################################################
# Verify the some attributes read properly.
#
# NOTE: one weird thing is that in this pass the prime_meridian_code field
# is typed as integer instead of string since it is created literally.

def ogr_csv_4():
    if gdaltest.csv_ds is None:
        return 'skip'

    return ogr_csv_check_layer(gdaltest.csv_lyr1, True)

###############################################################################
# Copy prime_meridian.csv again, in CRLF mode.

def ogr_csv_5():
    if gdaltest.csv_ds is None:
        return 'skip'

    #######################################################
    # Create layer (.csv file)
    gdaltest.csv_lyr2 = ogr_csv_copy_layer( 'pm2', ['LINEFORMAT=CRLF',] )

    return 'success'

###############################################################################
# Verify the some attributes read properly.
#

def ogr_csv_6():
    if gdaltest.csv_ds is None:
        return 'skip'

    return ogr_csv_check_layer(gdaltest.csv_lyr2, True)

###############################################################################
# Delete a layer and verify it seems to have worked properly.
#

def ogr_csv_7():
    if gdaltest.csv_ds is None:
        return 'skip'

    lyr = gdaltest.csv_tmpds.GetLayer(0)
    if lyr.GetName() != 'pm1':
        gdaltest.post_reason( 'unexpected name for first layer' )
        return 'fail'

    gdaltest.csv_lyr1 = None
    err = gdaltest.csv_tmpds.DeleteLayer(0)

    if err != 0:
        gdaltest.post_reason( 'got error code from DeleteLayer' )
        return 'fail'

    if gdaltest.csv_tmpds.GetLayerCount() != 1 \
       or gdaltest.csv_tmpds.GetLayer(0).GetName() != 'pm2':
        gdaltest.post_reason( 'Layer not destroyed properly?' )
        return 'fail'

    with gdaltest.error_handler():
        if gdaltest.csv_tmpds.DeleteLayer(-1) == 0:
            gdaltest.post_reason( 'fail' )
            return 'fail'
        if gdaltest.csv_tmpds.DeleteLayer(gdaltest.csv_tmpds.GetLayerCount()) == 0:
            gdaltest.post_reason( 'fail' )
            return 'fail'

    gdaltest.csv_tmpds = None

    return 'success'

###############################################################################
# Reopen and append a record then close.
#

def ogr_csv_8():
    if gdaltest.csv_ds is None:
        return 'skip'

    gdaltest.csv_tmpds = ogr.Open( 'tmp/csvwrk', update = 1 )

    lyr = gdaltest.csv_tmpds.GetLayer(0)

    feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )

    feat.SetField( 'PRIME_MERIDIAN_CODE', '7000' )
    feat.SetField( 'INFORMATION_SOURCE', 'This is a newline test\n' )

    lyr.CreateFeature( feat )

    gdaltest.csv_tmpds = None

    return 'success'

###############################################################################
# Verify the some attributes read properly.
#

def ogr_csv_9():
    if gdaltest.csv_ds is None:
        return 'skip'

    gdaltest.csv_tmpds = ogr.Open( 'tmp/csvwrk', update=1 )

    lyr = gdaltest.csv_tmpds.GetLayer(0)

    expect = [ '8901', '8902', '8903', '8904', '7000' ]

    tr = ogrtest.check_features_against_list( lyr,'PRIME_MERIDIAN_CODE',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()

    expect = [ '', 'Instituto Geografico e Cadastral; Lisbon',
               'Institut Geographique National (IGN), Paris',
               'Instituto Geografico "Augustin Cadazzi" (IGAC); Bogota',
               'This is a newline test\n' ]

    tr = ogrtest.check_features_against_list( lyr,'INFORMATION_SOURCE',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()

    return 'success'

###############################################################################
# Verify some capabilities and related stuff.
#

def ogr_csv_10():
    if gdaltest.csv_ds is None:
        return 'skip'

    lyr = gdaltest.csv_ds.GetLayerByName( 'prime_meridian' )

    if lyr.TestCapability( 'SequentialWrite' ):
        gdaltest.post_reason( 'should not have write access to readonly layer')
        return 'fail'

    if lyr.TestCapability( 'RandomRead' ):
        gdaltest.post_reason( 'CSV files do not efficiently support '
                              'random reading.')
        return 'fail'

    if lyr.TestCapability( 'FastGetExtent' ):
        gdaltest.post_reason( 'CSV files do not support getextent' )
        return 'fail'

    if lyr.TestCapability( 'FastFeatureCount' ):
        gdaltest.post_reason( 'CSV files do not support fast feature count')
        return 'fail'

    if not ogr.GetDriverByName('CSV').TestCapability( 'DeleteDataSource' ):
        gdaltest.post_reason( 'CSV files do support DeleteDataSource' )
        return 'fail'

    if not ogr.GetDriverByName('CSV').TestCapability( 'CreateDataSource' ):
        gdaltest.post_reason( 'CSV files do support CreateDataSource' )
        return 'fail'

    if gdaltest.csv_ds.TestCapability( 'CreateLayer' ):
        gdaltest.post_reason( 'readonly datasource should not CreateLayer' )
        return 'fail'

    if gdaltest.csv_ds.TestCapability( 'DeleteLayer' ):
        gdaltest.post_reason( 'should not have deletelayer on readonly ds.')
        return 'fail'

    lyr = gdaltest.csv_tmpds.GetLayer(0)

    if not lyr.TestCapability( 'SequentialWrite' ):
        gdaltest.post_reason( 'should have write access to updatable layer')
        return 'fail'

    if not gdaltest.csv_tmpds.TestCapability( 'CreateLayer' ):
        gdaltest.post_reason( 'should have createlayer on updatable ds.')
        return 'fail'

    if not gdaltest.csv_tmpds.TestCapability( 'DeleteLayer' ):
        gdaltest.post_reason( 'should have deletelayer on updatable ds.')
        return 'fail'

    return 'success'

###############################################################################
def ogr_csv_check_testcsvt(lyr):

    lyr.ResetReading()

    expect = [ 12, None ]
    tr = ogrtest.check_features_against_list( lyr,'INTCOL',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()

    expect = [ 5.7, None ]
    tr = ogrtest.check_features_against_list( lyr,'REALCOL',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()

    expect = [ 'foo', '' ]
    tr = ogrtest.check_features_against_list( lyr,'STRINGCOL',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('DATETIME') != '2008/12/25 11:22:33':
        print(feat.GetFieldAsString('DATETIME'))
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('DATETIME') != '':
        print(feat.GetFieldAsString('DATETIME'))
        return 'fail'

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('DATE') != '2008/12/25':
        print(feat.GetFieldAsString('DATE'))
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('DATE') != '':
        print(feat.GetFieldAsString('DATE'))
        return 'fail'

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('TIME') != '11:22:33':
        print(feat.GetFieldAsString('TIME'))
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('TIME') != '':
        print(feat.GetFieldAsString('TIME'))
        return 'fail'

    if lyr.GetLayerDefn().GetFieldDefn(0).GetWidth() != 5:
        gdaltest.post_reason( 'Field 0 : expecting width = 5')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldDefn(1).GetWidth() != 10:
        gdaltest.post_reason( 'Field 1 : expecting width = 10')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldDefn(1).GetPrecision() != 7:
        gdaltest.post_reason( 'Field 1 : expecting precision = 7')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldDefn(2).GetWidth() != 15:
        gdaltest.post_reason( 'Field 2 : expecting width = 15')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldDefn(6).GetType() != ogr.OFTDateTime:
        gdaltest.post_reason( 'Field DATETIME : wrong type')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldDefn(7).GetType() != ogr.OFTDate:
        gdaltest.post_reason( 'Field DATETIME : wrong type')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldDefn(8).GetType() != ogr.OFTTime:
        gdaltest.post_reason( 'Field DATETIME : wrong type')
        return 'fail'

    lyr.ResetReading()

    return 'success'


###############################################################################
# Verify handling of csvt with width and precision specified
# Test NULL handling of non string columns too (#2756)

def ogr_csv_11():

    if gdaltest.csv_ds is None:
        return 'skip'

    gdaltest.csv_ds = None
    gdaltest.csv_ds = ogr.Open( 'data/testcsvt.csv' )

    if gdaltest.csv_ds is None:
        return 'fail'

    lyr = gdaltest.csv_ds.GetLayerByName( 'testcsvt' )

    return ogr_csv_check_testcsvt(lyr)

###############################################################################
# Verify CREATE_CSVT=YES option

def ogr_csv_12():

    if gdaltest.csv_ds is None:
        return 'skip'

    srclyr = gdaltest.csv_ds.GetLayerByName( 'testcsvt' )

    #######################################################
    # Create layer (.csv file)
    options = ['CREATE_CSVT=YES',]
    gdaltest.csv_lyr2 = gdaltest.csv_tmpds.CreateLayer( 'testcsvt_copy',
                                                        options = options )

    #######################################################
    # Setup Schema
    for i in range(srclyr.GetLayerDefn().GetFieldCount()):
        field_defn = srclyr.GetLayerDefn().GetFieldDefn(i)
        gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        gdaltest.csv_lyr2.CreateField( field_defn )
        gdal.PopErrorHandler()

    #######################################################
    # Recopy source layer into destination layer
    dst_feat = ogr.Feature( feature_def = gdaltest.csv_lyr2.GetLayerDefn() )

    srclyr.ResetReading()

    feat = srclyr.GetNextFeature()

    while feat is not None:

        dst_feat.SetFrom( feat )
        gdaltest.csv_lyr2.CreateFeature( dst_feat )

        feat = srclyr.GetNextFeature()

    with gdaltest.error_handler():
        if gdaltest.csv_tmpds.CreateLayer( 'testcsvt_copy' ) is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    #######################################################
    # Closes everything and reopen
    gdaltest.csv_tmpds = None

    gdaltest.csv_ds = None
    gdaltest.csv_ds = ogr.Open( 'tmp/csvwrk/testcsvt_copy.csv' )

    #######################################################
    # Checks copy
    if gdaltest.csv_ds is None:
        return 'fail'

    lyr = gdaltest.csv_ds.GetLayerByName( 'testcsvt_copy' )

    return ogr_csv_check_testcsvt(lyr)

###############################################################################
# Verify GEOMETRY=AS_WKT,AS_XY,AS_XYZ,AS_YX options

def ogr_csv_13():

    if gdaltest.csv_ds is None:
        return 'skip'

    gdaltest.csv_tmpds = ogr.Open( 'tmp/csvwrk', update=1 )

    # AS_WKT
    options = ['GEOMETRY=AS_WKT','CREATE_CSVT=YES']
    lyr = gdaltest.csv_tmpds.CreateLayer( 'as_wkt', options = options )

    field_defn = ogr.FieldDefn( 'ADATA', ogr.OFTString )
    lyr.CreateField(field_defn)

    # Some applications expect the WKT column not to be exposed. Check it
    if lyr.GetLayerDefn().GetFieldCount() != 1:
        return 'fail'

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature( dst_feat )

    # AS_WKT but no field
    options = ['GEOMETRY=AS_WKT','CREATE_CSVT=YES']
    lyr = gdaltest.csv_tmpds.CreateLayer( 'as_wkt_no_field', options = options )

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    lyr.CreateFeature( dst_feat )

    # AS_XY
    options = ['GEOMETRY=AS_XY','CREATE_CSVT=YES']
    lyr = gdaltest.csv_tmpds.CreateLayer( 'as_xy', options = options )

    field_defn = ogr.FieldDefn( 'ADATA', ogr.OFTString )
    lyr.CreateField(field_defn)

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature( dst_feat )

    # Nothing will be written in the x or y field
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(1 2,3 4)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature( dst_feat )

    # AS_YX
    options = ['GEOMETRY=AS_YX','CREATE_CSVT=YES']
    lyr = gdaltest.csv_tmpds.CreateLayer( 'as_yx', options = options )

    field_defn = ogr.FieldDefn( 'ADATA', ogr.OFTString )
    lyr.CreateField(field_defn)

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature( dst_feat )

    # AS_XYZ
    options = ['GEOMETRY=AS_XYZ','CREATE_CSVT=YES']
    lyr = gdaltest.csv_tmpds.CreateLayer( 'as_xyz', options = options )

    field_defn = ogr.FieldDefn( 'ADATA', ogr.OFTString )
    lyr.CreateField(field_defn)

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2 3)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature( dst_feat )

    #######################################################
    # Closes everything and reopen
    gdaltest.csv_tmpds = None

    gdaltest.csv_tmpds = ogr.Open( 'tmp/csvwrk' )

    # Test AS_WKT
    lyr = gdaltest.csv_tmpds.GetLayerByName('as_wkt')

    expect = [ 'POINT (1 2)' ]
    tr = ogrtest.check_features_against_list( lyr,'WKT',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()
    expect = [ 'avalue' ]
    tr = ogrtest.check_features_against_list( lyr,'ADATA',expect)
    if not tr:
        return 'fail'

    # Test as_wkt_no_field
    lyr = gdaltest.csv_tmpds.GetLayerByName('as_wkt_no_field')

    expect = [ 'POINT (1 2)' ]
    tr = ogrtest.check_features_against_list( lyr,'WKT',expect)
    if not tr:
        return 'fail'

    # Test AS_XY
    lyr = gdaltest.csv_tmpds.GetLayerByName('as_xy')

    if lyr.GetLayerDefn().GetFieldDefn(0).GetName() != 'X':
        return 'fail'

    expect = [ 1, None ]
    tr = ogrtest.check_features_against_list( lyr,'X',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()
    expect = [ 2, None ]
    tr = ogrtest.check_features_against_list( lyr,'Y',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()
    expect = [ 'avalue','avalue' ]
    tr = ogrtest.check_features_against_list( lyr,'ADATA',expect)
    if not tr:
        return 'fail'

    # Test AS_YX
    lyr = gdaltest.csv_tmpds.GetLayerByName('as_yx')

    if lyr.GetLayerDefn().GetFieldDefn(0).GetName() != 'Y':
        return 'fail'

    expect = [ 1 ]
    tr = ogrtest.check_features_against_list( lyr,'X',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()
    expect = [ 2 ]
    tr = ogrtest.check_features_against_list( lyr,'Y',expect)
    if not tr:
        return 'fail'

    # Test AS_XYZ
    lyr = gdaltest.csv_tmpds.GetLayerByName('as_xyz')

    if lyr.GetLayerDefn().GetFieldDefn(0).GetName() != 'X':
        return 'fail'

    expect = [ 1 ]
    tr = ogrtest.check_features_against_list( lyr,'X',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()
    expect = [ 2 ]
    tr = ogrtest.check_features_against_list( lyr,'Y',expect)
    if not tr:
        return 'fail'

    lyr.ResetReading()
    expect = [ 3 ]
    tr = ogrtest.check_features_against_list( lyr,'Z',expect)
    if not tr:
        return 'fail'

    return 'success'

###############################################################################
# Copy prime_meridian.csv again, with SEMICOLON as separator

def ogr_csv_14():
    if gdaltest.csv_ds is None:
        return 'skip'

    gdaltest.csv_tmpds = ogr.Open( 'tmp/csvwrk', update = 1 )
    gdaltest.csv_ds = None
    gdaltest.csv_ds = ogr.Open( 'data/prime_meridian.csv'  )

    #######################################################
    # Create layer (.csv file)
    gdaltest.csv_lyr1 = ogr_csv_copy_layer( 'pm3', ['SEPARATOR=SEMICOLON',] )

    return 'success'

###############################################################################
# Verify the some attributes read properly.
#

def ogr_csv_15():
    if gdaltest.csv_ds is None:
        return 'skip'

    return ogr_csv_check_layer(gdaltest.csv_lyr1, True)

###############################################################################
# Close the file and check again
#

def ogr_csv_16():
    if gdaltest.csv_ds is None:
        return 'skip'

    gdaltest.csv_tmpds = None
    gdaltest.csv_tmpds = ogr.Open( 'tmp/csvwrk' )
    gdaltest.csv_lyr1 = gdaltest.csv_tmpds.GetLayerByName('pm3')

    return ogr_csv_check_layer(gdaltest.csv_lyr1, False)

###############################################################################
# Verify that WKT field treated as geometry.
#

def ogr_csv_17():
    if gdaltest.csv_ds is None:
        return 'skip'

    csv_ds = ogr.Open( 'data/wkt.csv' )
    csv_lyr = csv_ds.GetLayer(0)

    if csv_lyr.GetLayerDefn().GetGeomType() != ogr.wkbUnknown:
        gdaltest.post_reason( 'did not get wktUnknown for geometry type.' )
        return 'fail'

    feat = csv_lyr.GetNextFeature()
    if feat.GetField( 'WKT' ) != 'POLYGON((6.25 1.25,7.25 1.25,7.25 2.25,6.25 2.25,6.25 1.25))':
        gdaltest.post_reason( 'feature 1: expected wkt value' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POLYGON((6.25 1.25,7.25 1.25,7.25 2.25,6.25 2.25,6.25 1.25))'):
        return 'fail'

    feat = csv_lyr.GetNextFeature()

    feat = csv_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON((1.001 1.001,3.999 3.999,3.2 1.6,1.001 1.001))'):
        return 'fail'

    return 'success'


###############################################################################
# Write to /vsistdout/

def ogr_csv_18():

    ds = ogr.GetDriverByName('CSV').CreateDataSource( '/vsistdout/' )
    lyr = ds.CreateLayer('foo', options = ['GEOMETRY=AS_WKT'])
    lyr.CreateField( ogr.FieldDefn('foo') )
    lyr.CreateField( ogr.FieldDefn('bar') )
    feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    feat.SetField( 'foo', 'bar' )
    feat.SetField( 'bar', 'baz' )
    geom = ogr.CreateGeometryFromWkt('POINT(0 1)')
    feat.SetGeometry(geom)
    lyr.CreateFeature( feat )

    return 'success'

###############################################################################
# Verify handling of non-numeric values in numeric columns

def ogr_csv_19():

    if gdaltest.csv_ds is None:
        return 'skip'

    gdaltest.csv_ds = None
    gdaltest.csv_ds = ogr.Open( 'data/testnull.csv' )

    if gdaltest.csv_ds is None:
        return 'fail'

    lyr = gdaltest.csv_ds.GetLayerByName( 'testnull' )

    lyr.ResetReading()
    with gdaltest.error_handler():
        if not ogrtest.check_features_against_list( lyr,'INTCOL',[12] ):
            return 'fail'
    lyr.ResetReading()
    if not ogrtest.check_features_against_list( lyr,'REALCOL',[5.7] ):
        return 'fail'
    lyr.ResetReading()
    if not ogrtest.check_features_against_list( lyr,'INTCOL2',[None] ):
        return 'fail'
    lyr.ResetReading()
    if not ogrtest.check_features_against_list( lyr,'REALCOL2',[None] ):
        return 'fail'
    lyr.ResetReading()
    if not ogrtest.check_features_against_list( lyr,'STRINGCOL',['foo']):
        return 'fail'

    return 'success'


###############################################################################
# Verify handling of column names with numbers

def ogr_csv_20():

    if gdaltest.csv_ds is None:
        return 'skip'

    gdaltest.csv_ds = None

    gdaltest.csv_ds = ogr.Open( 'data/testnumheader1.csv' )
    if gdaltest.csv_ds is None:
        return 'fail'

    lyr = gdaltest.csv_ds.GetLayerByName( 'testnumheader1' )
    if lyr is None:
        return 'fail'
    lyr.ResetReading()

    expect = ['1 - 2', '2-3']
    got = [lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef(),\
                 lyr.GetLayerDefn().GetFieldDefn(1).GetNameRef()]
    if  got[0]!= expect[0]:
        print('column 0 got name %s expected %s' % (str(got[0]), str(expect[0])) )
        return 'fail'
    if  got[1]!= expect[1]:
        print('column 1 got name %s expected %s' % (str(got[1]), str(expect[1])) )
        return 'fail'

    gdaltest.csv_ds = None

    gdaltest.csv_ds = ogr.Open( 'data/testnumheader2.csv' )
    if gdaltest.csv_ds is None:
        return 'fail'

    lyr = gdaltest.csv_ds.GetLayerByName( 'testnumheader2' )
    if lyr is None:
        return 'fail'
    lyr.ResetReading()

    expect = ['field_1', 'field_2']
    got = [lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef(),\
                 lyr.GetLayerDefn().GetFieldDefn(1).GetNameRef()]
    if  got[0]!= expect[0]:
        print('column 0 got name %s expected %s' % (str(got[0]), str(expect[0])) )
        return 'fail'
    if  got[1]!= expect[1]:
        print('column 1 got name %s expected %s' % (str(got[1]), str(expect[1])) )
        return 'fail'

    return 'success'

###############################################################################
# Verify handling of numeric column names with quotes (bug #4361)

def ogr_csv_21():

    if gdaltest.csv_ds is None:
        return 'skip'

    gdaltest.csv_ds = None

    gdaltest.csv_ds = ogr.Open( 'data/testquoteheader1.csv' )
    if gdaltest.csv_ds is None:
        return 'fail'

    lyr = gdaltest.csv_ds.GetLayerByName( 'testquoteheader1' )
    if lyr is None:
        return 'fail'
    lyr.ResetReading()

    expect = ['test', '2000', '2000.12']
    for i in range(0,3):
        got = lyr.GetLayerDefn().GetFieldDefn(i).GetNameRef()
        if  got!= expect[i]:
            print('column %d got name %s expected %s' % (i,str(got), str(expect[i])) )
            return 'fail'

    gdaltest.csv_ds = None

    gdaltest.csv_ds = ogr.Open( 'data/testquoteheader2.csv' )
    if gdaltest.csv_ds is None:
        return 'fail'

    lyr = gdaltest.csv_ds.GetLayerByName( 'testquoteheader2' )
    if lyr is None:
        return 'fail'
    lyr.ResetReading()

    expect = ['field_1', 'field_2', 'field_3']
    for i in range(0,3):
        got = lyr.GetLayerDefn().GetFieldDefn(i).GetNameRef()
        if  got!= expect[i]:
            print('column %d got name %s expected %s' % (i,str(got), str(expect[i])) )
            return 'fail'

    return 'success'


###############################################################################
# Test handling of UTF8 BOM (bug #4623)

def ogr_csv_22():

    ds = ogr.Open('data/csv_with_utf8_bom.csv')
    lyr = ds.GetLayer(0)
    fld0_name = lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef()

    if fld0_name != 'id':
        gdaltest.post_reason('bad field name')
        print(fld0_name)
        return 'fail'

    return 'success'


def ogr_csv_23():
    # create a CSV file with UTF8 BOM
    ds = ogr.Open('tmp/csvwrk', update=1)
    lyr = ds.CreateLayer('utf8', options=['WRITE_BOM=YES', 'GEOMETRY=AS_WKT'])
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('bar', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', 123)
    feat.SetField('bar', 'baz')
    geom = ogr.CreateGeometryFromWkt('POINT(0 1)')
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = None
    lyr = None
    ds = None

    data = open('tmp/csvwrk/utf8.csv', 'rb').read()
    if sys.version_info >= (3,0,0):
        ogrtest.ret = False
        exec("ogrtest.ret = (data[:6] == b'\\xef\\xbb\\xbfWKT')")
    else:
        ogrtest.ret = (data[:6] == '\xef\xbb\xbfWKT')
    if not ogrtest.ret:
        gdaltest.post_reason("No UTF8 BOM header on output")
        return 'fail'

    # create a CSV file without UTF8 BOM
    ds = ogr.Open('tmp/csvwrk', update=1)
    lyr = ds.CreateLayer('utf8no', options=['WRITE_BOM=YES', 'GEOMETRY=AS_WKT'])
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('bar', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', 123)
    feat.SetField('bar', 'baz')
    geom = ogr.CreateGeometryFromWkt('POINT(0 1)')
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = None
    lyr = None
    ds = None

    data = open('tmp/csvwrk/utf8no.csv', 'rb').read()
    if data[:3] == '\xef\xbb\xbfWKT':
        gdaltest.post_reason("Found UTF8 BOM header on output!")
        return 'fail'

    return 'success'

###############################################################################
# Test single column CSV files

def ogr_csv_24():

    # Create an invalid CSV file
    f = gdal.VSIFOpenL('/vsimem/invalid.csv', 'wb')
    gdal.VSIFCloseL(f)

    # and check that it doesn't prevent from creating a new CSV file (#4824)
    ds = ogr.GetDriverByName('CSV').CreateDataSource( '/vsimem/single.csv' )
    lyr = ds.CreateLayer('single')
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'bar')
    lyr.CreateFeature(feat)
    feat = None
    lyr = None
    ds = None

    ds = ogr.Open( '/vsimem/single.csv' )
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField(0) != '':
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField(0) != 'bar':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/single.csv')
    gdal.Unlink('/vsimem/invalid.csv')

    return 'success'


###############################################################################
# Test newline handling (#4452)

def ogr_csv_25():
    ds = ogr.Open('tmp/csvwrk', update=1)
    lyr = ds.CreateLayer('newlines', options=['LINEFORMAT=LF'])  # just in case tests are run on windows...
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', 'windows newline:\r\nlinux newline:\nend of string:')
    lyr.CreateFeature(feat)

    feat = None
    lyr = None
    ds = None

    EXPECTED = 'foo,\n"windows newline:\r\nlinux newline:\nend of string:"\n'

    data = open('tmp/csvwrk/newlines.csv', 'rb').read().decode('ascii')
    if data != EXPECTED:
        gdaltest.post_reason("Newlines changed:\n\texpected=%s\n\tgot=     %s" % (repr(EXPECTED), repr(data)))
        return 'fail'

    return 'success'


###############################################################################
# Test number padding behaviour (#4469)

def ogr_csv_26():
    ds = ogr.Open('tmp/csvwrk', update=1)
    lyr = ds.CreateLayer('num_padding', options=['LINEFORMAT=LF'])  # just in case tests are run on windows...

    field = ogr.FieldDefn('foo', ogr.OFTReal)
    field.SetWidth(50)
    field.SetPrecision(25)
    lyr.CreateField(field)

    feature = ogr.Feature(lyr.GetLayerDefn())
    feature.SetField('foo', 10.5)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', 10.5)
    lyr.CreateFeature(feat)

    feat = None
    lyr = None
    ds = None

    EXPECTED = 'foo,\n10.5000000000000000000000000\n'

    data = open('tmp/csvwrk/num_padding.csv', 'rb').read().decode('ascii')
    if data != EXPECTED:
        gdaltest.post_reason("expected=%s got= %s" % (repr(EXPECTED), repr(data)))
        return 'fail'

    return 'success'

###############################################################################
# Test Eurostat .TSV files

def ogr_csv_27():

    ds = ogr.Open('data/test_eurostat.tsv')
    lyr = ds.GetLayer(0)
    layer_defn = lyr.GetLayerDefn()
    if layer_defn.GetFieldCount() != 8:
        gdaltest.post_reason('fail')
        return 'fail'

    expected_fields = [ ('unit', ogr.OFTString),
                        ('geo', ogr.OFTString),
                        ('time_2010', ogr.OFTReal),
                        ('time_2010_flag', ogr.OFTString),
                        ('time_2011', ogr.OFTReal),
                        ('time_2011_flag', ogr.OFTString),
                        ('time_2012', ogr.OFTReal),
                        ('time_2012_flag', ogr.OFTString) ]
    i = 0
    for expected_field in expected_fields:
        fld = layer_defn.GetFieldDefn(i)
        if fld.GetName() != expected_field[0]:
            print(fld.GetName())
            print(expected_field[0])
            gdaltest.post_reason('fail')
            return 'fail'
        if fld.GetType() != expected_field[1]:
            print(fld.GetType())
            print(expected_field[1])
            gdaltest.post_reason('fail')
            return 'fail'
        i = i + 1

    feat = lyr.GetNextFeature()
    if feat.GetField('unit') != 'NBR' or \
       feat.GetField('geo') != 'FOO' or \
       feat.IsFieldSet('time_2010') or \
       feat.IsFieldSet('time_2010_flag') or \
       feat.GetField('time_2011') != 1 or \
       feat.GetField('time_2011_flag') != 'u' or \
       feat.GetField('time_2012') != 2.34 or \
       feat.IsFieldSet('time_2012_flag') :
            feat.DumpReadable()
            gdaltest.post_reason('fail')
            return 'fail'

    return 'success'

###############################################################################
# Check that we don't rewrite erroneously a file that has no header (#5161).

def ogr_csv_28():

    f = open('tmp/ogr_csv_28.csv', 'wb')
    f.write('1,2\n'.encode('ascii'))
    f.close()

    ds = ogr.Open('tmp/ogr_csv_28.csv', update = 1)
    del ds

    f = open('tmp/ogr_csv_28.csv', 'rb')
    data = f.read().decode('ascii')
    f.close()

    os.unlink('tmp/ogr_csv_28.csv')

    if data != '1,2\n':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    return 'success'

###############################################################################
# Check multi geometry field support

def ogr_csv_29():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('tmp/ogr_csv_29', options = ['GEOMETRY=AS_WKT'])
    if ds.TestCapability(ogr.ODsCCurveGeometries) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)
    if lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_lyr1_EPSG_4326", ogr.wkbPoint)) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_lyr2_EPSG_32632", ogr.wkbPolygon)) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        if lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_lyr2_EPSG_32632", ogr.wkbPolygon)) == 0:
            gdaltest.post_reason('fail')
            return 'fail'
    ds = None

    ds = ogr.Open('tmp/ogr_csv_29', update = 1)
    lyr = ds.GetLayerByName('test')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeomField(0, ogr.CreateGeometryFromWkt('POINT (1 2)'))
    feat.SetGeomField(1, ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,1 0,0 0))'))
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open('tmp/ogr_csv_29')
    lyr = ds.GetLayerByName('test')
    if lyr.GetLayerDefn().GetGeomFieldCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    srs = lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef()
    if srs.GetAuthorityCode(None) != '4326':
        gdaltest.post_reason('fail')
        return 'fail'
    srs = lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef()
    if srs.GetAuthorityCode(None) != '32632':
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    geom = feat.GetGeomFieldRef('geom__WKT_lyr1_EPSG_4326')
    if geom.ExportToWkt() != 'POINT (1 2)':
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    geom = feat.GetGeomFieldRef('geom__WKT_lyr2_EPSG_32632')
    if geom.ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_csv_30():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/ogr_csv_29')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Read geonames.org allCountries.txt

def ogr_csv_31():

    ds = ogr.Open('data/allCountries.txt')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetField('GEONAMEID') != '3038814' or f.GetField('LATITUDE') != 42.5 or \
       f.GetGeometryRef().ExportToWkt() != 'POINT (1.48333 42.5)':
           gdaltest.post_reason('fail')
           f.DumpReadable()
           return 'fail'

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetField('GEONAMEID') != '3038814':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    if lyr.GetFeatureCount() != 10:
        gdaltest.post_reason('fail')
        print(lyr.GetFeatureCount())
        return 'fail'

    return 'success'

###############################################################################
# Test AUTODETECT_TYPE=YES

def ogr_csv_32():

    # Without limit, everything will be detected as string
    ds = gdal.OpenEx('data/testtypeautodetect.csv', gdal.OF_VECTOR, \
        open_options = ['AUTODETECT_TYPE=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_values = [ '', '1.5', '1', '1.5', '2', '', '2014-09-27 19:01:00', '2014-09-27', '2014-09-27 20:00:00',
                   '2014-09-27', '12:34:56', 'a', 'a', '1', '1', '1.5', '2014-09-27 19:01:00', '2014-09-27', '19:01:00', '2014-09-27T00:00:00Z' ]
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        if lyr.GetLayerDefn().GetFieldDefn(i).GetType() != ogr.OFTString or \
           lyr.GetLayerDefn().GetFieldDefn(i).GetWidth() != 0:
            gdaltest.post_reason('fail')
            print(i)
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetType())
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetWidth())
            return 'fail'
        if f.GetField(i) != col_values[i]:
            gdaltest.post_reason('fail')
            print(i)
            f.DumpReadable()
            return 'fail'

    # Without limit, everything will be detected as string
    ds = gdal.OpenEx('data/testtypeautodetect.csv', gdal.OF_VECTOR, \
        open_options = ['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=0'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        if lyr.GetLayerDefn().GetFieldDefn(i).GetType() != ogr.OFTString or \
           lyr.GetLayerDefn().GetFieldDefn(i).GetWidth() != 0:
            gdaltest.post_reason('fail')
            print(i)
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetType())
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetWidth())
            return 'fail'
        if f.GetField(i) != col_values[i]:
            gdaltest.post_reason('fail')
            print(i)
            f.DumpReadable()
            return 'fail'

    # We limit to the first "1.5" line
    ds = gdal.OpenEx('data/testtypeautodetect.csv', gdal.OF_VECTOR, \
        open_options = ['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=300', 'QUOTED_FIELDS_AS_STRING=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_type = [ ogr.OFTString, ogr.OFTReal, ogr.OFTInteger, ogr.OFTReal, ogr.OFTInteger, ogr.OFTString,
                 ogr.OFTDateTime, ogr.OFTDate, ogr.OFTDateTime, ogr.OFTDate, ogr.OFTTime,
                 ogr.OFTString, ogr.OFTString, ogr.OFTString, ogr.OFTInteger, ogr.OFTReal, ogr.OFTDateTime, ogr.OFTDate, ogr.OFTTime, ogr.OFTDateTime ]
    col_values = [ '', 1.5, 1, 1.5, 2, '', '2014/09/27 19:01:00', '2014/09/27', '2014/09/27 20:00:00',
                   '2014/09/27', '12:34:56', 'a', 'a', '1', 1, 1.5, '2014/09/27 19:01:00', '2014/09/27', '19:01:00', '2014/09/27 00:00:00+00' ]
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        if lyr.GetLayerDefn().GetFieldDefn(i).GetType() != col_type[i] or \
           lyr.GetLayerDefn().GetFieldDefn(i).GetWidth() != 0:
            gdaltest.post_reason('fail')
            print(i)
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetType())
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetWidth())
            return 'fail'
        if f.GetField(i) != col_values[i]:
            gdaltest.post_reason('fail')
            print(i)
            f.DumpReadable()
            return 'fail'

    # Without QUOTED_FIELDS_AS_STRING=YES, str3 will be detected as integer
    ds = gdal.OpenEx('data/testtypeautodetect.csv', gdal.OF_VECTOR, \
        open_options = ['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=300'])
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('str3')).GetType() != ogr.OFTInteger:
        gdaltest.post_reason('fail')
        return 'fail'

    # We limit to the first 2 lines
    ds = gdal.OpenEx('data/testtypeautodetect.csv', gdal.OF_VECTOR, \
        open_options = ['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=350', 'QUOTED_FIELDS_AS_STRING=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_type = [ ogr.OFTString, ogr.OFTReal, ogr.OFTReal, ogr.OFTReal, ogr.OFTInteger, ogr.OFTInteger,
                 ogr.OFTDateTime, ogr.OFTDateTime, ogr.OFTDateTime, ogr.OFTDate, ogr.OFTTime,
                 ogr.OFTString, ogr.OFTString, ogr.OFTString, ogr.OFTString, ogr.OFTString, ogr.OFTString, ogr.OFTString, ogr.OFTString, ogr.OFTDateTime ]
    col_values = [ '', 1.5, 1, 1.5, 2, None, '2014/09/27 19:01:00', '2014/09/27 00:00:00', '2014/09/27 20:00:00',
                   '2014/09/27', '12:34:56', 'a', 'a', '1', '1', '1.5', '2014-09-27 19:01:00', '2014-09-27', '19:01:00', '2014/09/27 00:00:00+00' ]
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        if lyr.GetLayerDefn().GetFieldDefn(i).GetType() != col_type[i] or \
           lyr.GetLayerDefn().GetFieldDefn(i).GetWidth() != 0:
            gdaltest.post_reason('fail')
            print(i)
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetType())
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetWidth())
            return 'fail'
        if f.GetField(i) != col_values[i]:
            gdaltest.post_reason('fail')
            print(i)
            f.DumpReadable()
            return 'fail'

    # Test AUTODETECT_WIDTH=YES
    ds = gdal.OpenEx('data/testtypeautodetect.csv', gdal.OF_VECTOR, \
        open_options = ['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=350', 'AUTODETECT_WIDTH=YES', 'QUOTED_FIELDS_AS_STRING=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_width = [ 0, 3, 3, 3, 1, 1, 0, 0, 0, 0, 0, 1, 2 , 1, 1, 3, 19, 10, 8, 0 ]
    col_precision = [ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ]

    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        if lyr.GetLayerDefn().GetFieldDefn(i).GetType() != col_type[i] or \
           lyr.GetLayerDefn().GetFieldDefn(i).GetWidth() != col_width[i] or \
           lyr.GetLayerDefn().GetFieldDefn(i).GetPrecision() != col_precision[i]:
            gdaltest.post_reason('fail')
            print(i)
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetType())
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetWidth())
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetPrecision())
            return 'fail'
        if f.GetField(i) != col_values[i]:
            gdaltest.post_reason('fail')
            print(i)
            f.DumpReadable()
            return 'fail'

    # Test AUTODETECT_WIDTH=STRING_ONLY
    ds = gdal.OpenEx('data/testtypeautodetect.csv', gdal.OF_VECTOR, \
        open_options = ['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=350', 'AUTODETECT_WIDTH=STRING_ONLY', 'QUOTED_FIELDS_AS_STRING=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_width = [ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2 , 1, 1, 3, 19, 10, 8, 0 ]
    col_precision = [ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ]

    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        if lyr.GetLayerDefn().GetFieldDefn(i).GetType() != col_type[i] or \
           lyr.GetLayerDefn().GetFieldDefn(i).GetWidth() != col_width[i] or \
           lyr.GetLayerDefn().GetFieldDefn(i).GetPrecision() != col_precision[i]:
            gdaltest.post_reason('fail')
            print(i)
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetType())
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetWidth())
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetPrecision())
            return 'fail'
        if f.GetField(i) != col_values[i]:
            gdaltest.post_reason('fail')
            print(i)
            f.DumpReadable()
            return 'fail'

    # Test KEEP_SOURCE_COLUMNS=YES
    ds = gdal.OpenEx('data/testtypeautodetect.csv', gdal.OF_VECTOR, \
        open_options = ['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=350', 'KEEP_SOURCE_COLUMNS=YES', 'QUOTED_FIELDS_AS_STRING=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_values = [ '', 1.5, '1.5', 1, '1', 1.5, '1.5', 2, '2', None, None, \
                   '2014/09/27 19:01:00', '2014-09-27 19:01:00', '2014/09/27 00:00:00', '2014-09-27', '2014/09/27 20:00:00', '2014-09-27 20:00:00',
                   '2014/09/27', '2014-09-27', '12:34:56', '12:34:56', 'a', 'a', '1', '1', '1.5', '2014-09-27 19:01:00', '2014-09-27', '19:01:00',
                   '2014/09/27 00:00:00+00', '2014-09-27T00:00:00Z' ]

    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        if lyr.GetLayerDefn().GetFieldDefn(i).GetType() != ogr.OFTString and \
           lyr.GetLayerDefn().GetFieldDefn(i+1).GetNameRef() != lyr.GetLayerDefn().GetFieldDefn(i).GetNameRef() + '_original':
            gdaltest.post_reason('fail')
            print(i)
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetNameRef())
            print(lyr.GetLayerDefn().GetFieldDefn(i+1).GetNameRef())
            return 'fail'
        if f.GetField(i) != col_values[i]:
            gdaltest.post_reason('fail')
            print(i)
            f.DumpReadable()
            return 'fail'

    # Test warnings
    for fid in [ 3, # string in real field
                 4, # string in int field
                 5, # real in int field
                 6, # string in datetime field
                 7, # Value with a width greater than field width found in record 7 for field int1
                 8, # Value with a width greater than field width found in record 8 for field str1
                 9, # Value with a precision greater than field precision found in record 9 for field real1
               ]:
        ds = gdal.OpenEx('data/testtypeautodetect.csv', gdal.OF_VECTOR, \
            open_options = ['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=350', 'AUTODETECT_WIDTH=YES'])
        lyr = ds.GetLayer(0)
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        lyr.GetFeature(fid)
        gdal.PopErrorHandler()
        if gdal.GetLastErrorType() != gdal.CE_Warning:
            gdaltest.post_reason('fail')
            print(fid)
            f.DumpReadable()
            return 'fail'

    return 'success'

###############################################################################
# Test Boolean, Int16 and Float32 support

def ogr_csv_33():

    ds = gdal.OpenEx('data/testtypeautodetectboolean.csv', gdal.OF_VECTOR, \
        open_options = ['AUTODETECT_TYPE=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_values = [ 1,1,1,1,1,0,0,0,0,0,0,1,'y' ]
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        if (i < 10 and lyr.GetLayerDefn().GetFieldDefn(i).GetSubType() != ogr.OFSTBoolean) or \
           (i >= 10 and lyr.GetLayerDefn().GetFieldDefn(i).GetSubType() == ogr.OFSTBoolean):
            gdaltest.post_reason('fail')
            print(i)
            print(lyr.GetLayerDefn().GetFieldDefn(i).GetSubType())
            return 'fail'
        if f.GetField(i) != col_values[i]:
            gdaltest.post_reason('fail')
            print(i)
            f.DumpReadable()
            return 'fail'
    ds = None

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/subtypes.csv')
    lyr = ds.CreateLayer('test', options = ['CREATE_CSVT=YES'])
    fld = ogr.FieldDefn('b', ogr.OFTInteger)
    fld.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld)
    fld = ogr.FieldDefn('int16', ogr.OFTInteger)
    fld.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld)
    fld = ogr.FieldDefn('float32', ogr.OFTReal)
    fld.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 1)
    f.SetField(1, -32768)
    f.SetField(2, 1.23)
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/subtypes.csv')
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldDefn(0).GetType() != ogr.OFTInteger or \
       lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() != ogr.OFSTBoolean:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(1).GetType() != ogr.OFTInteger or \
       lyr.GetLayerDefn().GetFieldDefn(1).GetSubType() != ogr.OFSTInt16:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(2).GetType() != ogr.OFTReal or \
       lyr.GetLayerDefn().GetFieldDefn(2).GetSubType() != ogr.OFSTFloat32:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetField(0) != 1 or f.GetField(1) != -32768 or f.GetField(2) != 1.23:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/subtypes.csv')
    gdal.Unlink('/vsimem/subtypes.csvt')

    return 'success'

###############################################################################
# Test Integer64 support

def ogr_csv_34():

    ds = gdal.OpenEx('data/testtypeautodetectinteger64.csv', gdal.OF_VECTOR, \
        open_options = ['AUTODETECT_TYPE=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_values = [ 1,10000000000,10000000000,10000000000.0 ]
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        if f.GetField(i) != col_values[i]:
            gdaltest.post_reason('fail')
            print(i)
            f.DumpReadable()
            return 'fail'
    f = lyr.GetNextFeature()
    col_values = [ 10000000000,1,10000000000,1.0 ]
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        if f.GetField(i) != col_values[i]:
            gdaltest.post_reason('fail')
            print(i)
            f.DumpReadable()
            return 'fail'
    ds = None

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/int64.csv')
    lyr = ds.CreateLayer('test', options = ['CREATE_CSVT=YES'])
    fld = ogr.FieldDefn('int64', ogr.OFTInteger64)
    lyr.CreateField(fld)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 10000000000)
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/int64.csv')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetField(0) != 10000000000:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/int64.csv')
    gdal.Unlink('/vsimem/int64.csvt')

    return 'success'

###############################################################################
# Test comma separator

def ogr_csv_35():

    gdal.FileFromMemBuffer('/vsimem/ogr_csv_35.csv',
"""FIELD_1  "FIELD 2" FIELD_3
VAL1   "VAL 2"   "VAL 3"
""")

    ds = gdal.OpenEx('/vsimem/ogr_csv_35.csv', gdal.OF_VECTOR, \
        open_options = ['MERGE_SEPARATOR=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['FIELD_1'] != 'VAL1' or f['FIELD 2'] != 'VAL 2' or f['FIELD_3'] != 'VAL 3':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_35.csv')

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_35.csv')
    lyr = ds.CreateLayer('ogr_csv_35', options = ['SEPARATOR=SPACE'])
    lyr.CreateField(ogr.FieldDefn('FIELD_1', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('FIELD 2', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('FIELD_1', 'VAL1')
    f.SetField('FIELD 2', 'VAL 2')
    lyr.CreateFeature(f)
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_csv_35.csv', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data.find('FIELD_1 "FIELD 2"') < 0 or data.find('VAL1 "VAL 2"') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.Unlink('/vsimem/ogr_csv_35.csv')

    return 'success'

###############################################################################
# Test GEOM_POSSIBLE_NAMES open option

def ogr_csv_36():

    gdal.FileFromMemBuffer('/vsimem/ogr_csv_36.csv',
"""id,mygeometry,format
1,"POINT(1 2)",wkt
2,"{""type"": ""Point"", ""coordinates"" : [2, 49]}",geojson
3,010100000000000000000008400000000000004940,wkb
4,0101000020e610000000000000000008400000000000004940,ewkb
5,something else,something else
""")

    ds = gdal.OpenEx('/vsimem/ogr_csv_36.csv', gdal.OF_VECTOR, \
        open_options = ['GEOM_POSSIBLE_NAMES=mygeometry,another_field'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (1 2)' or f['id'] != '1' or f['mygeometry'] != 'POINT(1 2)' or f['format'] != 'wkt':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    # Test prefix* pattern
    ds = gdal.OpenEx('/vsimem/ogr_csv_36.csv', gdal.OF_VECTOR, \
        open_options = ['GEOM_POSSIBLE_NAMES=mygeom*'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (1 2)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    # Test *suffix pattern
    ds = gdal.OpenEx('/vsimem/ogr_csv_36.csv', gdal.OF_VECTOR, \
        open_options = ['GEOM_POSSIBLE_NAMES=*geometry'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (1 2)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    # Test *middle* pattern
    ds = gdal.OpenEx('/vsimem/ogr_csv_36.csv', gdal.OF_VECTOR, \
        open_options = ['GEOM_POSSIBLE_NAMES=*geom*'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (1 2)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    # Test non matching pattern
    ds = gdal.OpenEx('/vsimem/ogr_csv_36.csv', gdal.OF_VECTOR, \
        open_options = ['GEOM_POSSIBLE_NAMES=bla'])
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetGeomFieldCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    # Check KEEP_GEOM_COLUMNS=NO
    ds = gdal.OpenEx('/vsimem/ogr_csv_36.csv', gdal.OF_VECTOR, \
        open_options = ['GEOM_POSSIBLE_NAMES=mygeometry', 'KEEP_GEOM_COLUMNS=NO'])
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (1 2)' or f['id'] != '1' or f['format'] != 'wkt':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_36.csv')

    return 'success'

###############################################################################
# Test X_POSSIBLE_NAMES, Y_POSSIBLE_NAMES and Z_POSSIBLE_NAMES open options

def ogr_csv_37():

    gdal.FileFromMemBuffer('/vsimem/ogr_csv_37.csv',
"""id,y,other,x,z
1,49,a,2,100
2,50,b,3,
3,49,c,
""")

    ds = gdal.OpenEx('/vsimem/ogr_csv_37.csv', gdal.OF_VECTOR, \
        open_options = ['X_POSSIBLE_NAMES=long,x', 'Y_POSSIBLE_NAMES=lat,y'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)' or f['id'] != '1' or f['x'] != 2 or f['y'] != 49 or f['other'] != 'a' or f['z'] != '100':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    # Check Z_POSSIBLE_NAMES
    ds = gdal.OpenEx('/vsimem/ogr_csv_37.csv', gdal.OF_VECTOR, \
        open_options = ['X_POSSIBLE_NAMES=long,x', 'Y_POSSIBLE_NAMES=lat,y', 'Z_POSSIBLE_NAMES=z'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49 100)' or f['id'] != '1' or f['x'] != 2 or f['y'] != 49 or f['other'] != 'a' or f['z'] != 100:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    # Check KEEP_GEOM_COLUMNS=NO
    ds = gdal.OpenEx('/vsimem/ogr_csv_37.csv', gdal.OF_VECTOR, \
        open_options = ['X_POSSIBLE_NAMES=long,x', 'Y_POSSIBLE_NAMES=lat,y', 'KEEP_GEOM_COLUMNS=NO'])
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldCount() != 3:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)' or f['id'] != '1' or f['other'] != 'a':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_37.csv')

    return 'success'

###############################################################################
# Test GeoCSV WKT type

def ogr_csv_38():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_38.csv')
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:4326')
    lyr = ds.CreateLayer('ogr_csv_38', srs = srs, options = ['GEOMETRY=AS_WKT', 'CREATE_CSVT=YES', 'GEOMETRY_NAME=mygeom'])
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 1)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_38.csv')
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetGeomFieldDefn(0).GetName() != 'mygeom':
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef().ExportToWkt().find('4326') < 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_38.csv')
    gdal.Unlink('/vsimem/ogr_csv_38.csvt')
    gdal.Unlink('/vsimem/ogr_csv_38.prj')

    return 'success'

###############################################################################
# Test GeoCSV CoordX and CoordY types

def ogr_csv_39():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_39.csv')
    lyr = ds.CreateLayer('ogr_csv_38', options = ['GEOMETRY=AS_XY', 'CREATE_CSVT=YES'])
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 1)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_39.csv')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_39.csv')
    gdal.Unlink('/vsimem/ogr_csv_39.csvt')

    return 'success'

###############################################################################
# Test X_POSSIBLE_NAMES, Y_POSSIBLE_NAMES, GEOM_POSSIBLE_NAMES and KEEP_GEOM_COLUMNS=NO together (#6137)

def ogr_csv_40():

    gdal.FileFromMemBuffer('/vsimem/ogr_csv_40.csv',
"""latitude,longitude,the_geom,id
49,2,0101000020E61000004486E281C5C257C068B89DDA998F4640,1
""")

    ds = gdal.OpenEx('/vsimem/ogr_csv_40.csv', gdal.OF_VECTOR, \
        open_options = ['X_POSSIBLE_NAMES=longitude', 'Y_POSSIBLE_NAMES=latitude', 'GEOM_POSSIBLE_NAMES=the_geom', 'KEEP_GEOM_COLUMNS=NO' ])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)' or f['id'] != '1' or f['the_geom'] != '0101000020E61000004486E281C5C257C068B89DDA998F4640':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_40.csv')

    gdal.FileFromMemBuffer('/vsimem/ogr_csv_40.csv',
"""the_geom,latitude,longitude,id
0101000020E61000004486E281C5C257C068B89DDA998F4640,49,2,1
""")

    ds = gdal.OpenEx('/vsimem/ogr_csv_40.csv', gdal.OF_VECTOR, \
        open_options = ['X_POSSIBLE_NAMES=longitude', 'Y_POSSIBLE_NAMES=latitude', 'GEOM_POSSIBLE_NAMES=the_geom', 'KEEP_GEOM_COLUMNS=NO' ])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt().find('POINT (-95.04') < 0 or f['id'] != '1' or f['longitude'] != '2' or f['latitude'] != '49':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_40.csv')

    return 'success'

###############################################################################
# Test GEOM_POSSIBLE_NAMES and KEEP_GEOM_COLUMNS=NO together with empty content in geom column (#6152)

def ogr_csv_41():

    gdal.FileFromMemBuffer('/vsimem/ogr_csv_41.csv',
"""id,the_geom,foo
1,,bar
""")

    ds = gdal.OpenEx('/vsimem/ogr_csv_41.csv', gdal.OF_VECTOR, \
        open_options = ['GEOM_POSSIBLE_NAMES=the_geom', 'KEEP_GEOM_COLUMNS=NO' ])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef() is not None or f['id'] != '1' or f['foo'] != 'bar':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_41.csv')

    return 'success'

###############################################################################
# Test writing field with empty content

def ogr_csv_42():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_42.csv')
    lyr = ds.CreateLayer('ogr_csv_42')
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 1)
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_42.csv')
    return 'success'

###############################################################################
# Test editing capabilities

def ogr_csv_43():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_43.csv')
    lyr = ds.CreateLayer('ogr_csv_43', options = ['GEOMETRY=AS_WKT', 'CREATE_CSVT=YES'])
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 1)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = None
    lyr.SetNextByIndex(0)
    f = lyr.GetNextFeature()
    if f['id'] != 1:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = None

    if lyr.TestCapability(ogr.OLCCreateField) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString)) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        if lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString)) == 0:
            gdaltest.post_reason('fail')
            return 'fail'
    f = lyr.GetFeature(1)
    f.SetField('foo', 'bar')
    if lyr.TestCapability(ogr.OLCRandomWrite) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.SetFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetFeature(1)
    if f['id'] != 1 or f['foo'] != 'bar' or f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f['id'] != 1 or f['foo'] != 'bar' or f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 2)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(3 50)'))
    f.SetField('foo', 'baz')
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if f.GetFID() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetFeature(2)
    if f['id'] != 2 or f['foo'] != 'baz' or f.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.SetNextByIndex(1)
    f = lyr.GetNextFeature()
    if f['id'] != 2:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = None

    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_43.csv', update = 1)
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(2)
    if f['id'] != 2 or f['foo'] != 'baz' or f.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = None
    if lyr.TestCapability(ogr.OLCDeleteField) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        if lyr.DeleteField(-1) == 0:
            gdaltest.post_reason('fail')
            return 'fail'
    if lyr.DeleteField( lyr.GetLayerDefn().GetFieldIndex('foo') ) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.TestCapability(ogr.OLCDeleteFeature) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.DeleteFeature(2) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.DeleteFeature(2) != ogr.OGRERR_NON_EXISTING_FEATURE:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.DeleteFeature(3) != ogr.OGRERR_NON_EXISTING_FEATURE:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetFeature(2) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetFeature(3) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 3
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if f.GetFID() != 3:
        gdaltest.post_reason('fail')
        return 'fail'
    f = None
    if lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString)) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetFeature(1)
    if f['foo'] == 'bar':
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetFeature(3)
    if f.GetFID() != 3:
        gdaltest.post_reason('fail')
        return 'fail'
    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_43.csv', update = 1)
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(2)
    if f['id'] != 3:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = None
    if lyr.DeleteField( lyr.GetLayerDefn().GetFieldIndex('foo') ) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(4 51)'))
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if f.GetFID() != 3:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if lyr.GetExtent() != (2.0, 4.0, 49.0, 51.0):
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.DeleteFeature(f.GetFID()) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        lyr.SetSpatialFilter(-1, None)
    with gdaltest.error_handler():
        lyr.SetSpatialFilter(1, None)
    lyr.SetSpatialFilterRect(0, 0, 100, 100)
    lyr.SetSpatialFilterRect(0, 0, 0, 100, 100)
    lyr.SetSpatialFilter(0, lyr.GetSpatialFilter())
    lyr.SetSpatialFilter(lyr.GetSpatialFilter())
    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetExtent() != (2.0, 2.0, 49.0, 49.0):
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetExtent(geom_field = 0) != (2.0, 2.0, 49.0, 49.0):
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        lyr.GetExtent(geom_field = -1)
    lyr.SetAttributeFilter(None)

    if lyr.TestCapability(ogr.OLCCurveGeometries) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.TestCapability(ogr.OLCTransactions) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.StartTransaction()
    lyr.RollbackTransaction()
    lyr.CommitTransaction()

    if lyr.GetGeometryColumn() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetFIDColumn() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.TestCapability(ogr.OLCReorderFields) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.ReorderFields([0, 1]) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        if lyr.ReorderFields([0, -1]) == 0:
            gdaltest.post_reason('fail')
            return 'fail'

    if lyr.TestCapability(ogr.OLCAlterFieldDefn) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    if lyr.AlterFieldDefn(0, fld_defn, 0) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        if lyr.AlterFieldDefn(-1, fld_defn, 0) == 0:
            gdaltest.post_reason('fail')
            return 'fail'

    f = lyr.GetFeature(2)
    f.SetGeomField(0, ogr.CreateGeometryFromWkt('POINT (1 2)'))
    if lyr.SetFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = None
    if lyr.TestCapability(ogr.OLCCreateGeomField) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.CreateGeomField(ogr.GeomFieldDefn('geom__WKT_2')) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetFeature(2)
    f.SetGeomField(1, ogr.CreateGeometryFromWkt('POINT (3 4)'))
    if lyr.SetFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_43.csv', update = 1)
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(2)
    if f['WKT'] != 'POINT (1 2)' or f['_WKT_2'] != 'POINT (3 4)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    if lyr.SetFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = None
    if lyr.DeleteFeature(2) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetFeature(2) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.DeleteFeature(2) != ogr.OGRERR_NON_EXISTING_FEATURE:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_43.csv')
    return 'success'

###############################################################################
# Test seeking back while creating

def ogr_csv_44():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_44.csv')
    lyr = ds.CreateLayer('ogr_csv_44', options = ['GEOMETRY=AS_WKT'])
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 1)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = None
    f = lyr.GetFeature(1)
    if f['id'] != 1 or f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 2)
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetFeature(2)
    if f['id'] != 2 or f.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_44.csv')
    return 'success'

###############################################################################
# Test QGIS use case that consists in reopening a file just after calling
# CreateField() on the main dataset and assuming that file is already serialized.

def ogr_csv_45():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_45.csv')
    lyr = ds.CreateLayer('ogr_csv_45', options = ['GEOMETRY=AS_WKT'])
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_45.csv', update = 1)
    lyr = ds.GetLayer(0)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTInteger))

    ds2 = ogr.Open('/vsimem/ogr_csv_45.csv')
    lyr2 = ds2.GetLayer(0)
    if lyr2.GetLayerDefn().GetFieldCount() != 3:
        return 'fail'
    ds2 = None

    ds = None

    gdal.Unlink('/vsimem/ogr_csv_45.csv')
    return 'success'

###############################################################################
# Test edition of CSV files with X_POSSIBLE_NAMES, Y_POSSIBLE_NAMES open options

def ogr_csv_46():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_46.csv')
    lyr = ds.CreateLayer('ogr_csv_46')
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('X', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('Y', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('Z', ogr.OFTReal))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 1
    f['X'] = 1
    f['Y'] = 2
    f['Z'] = 3
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = gdal.OpenEx('/vsimem/ogr_csv_46.csv', gdal.OF_VECTOR | gdal.OF_UPDATE, open_options = ['X_POSSIBLE_NAMES=X', 'Y_POSSIBLE_NAMES=Y'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (10 20 30)'))
    lyr.SetFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_46.csv')
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldCount() != 4:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f['id'] != '1' or f['X'] != '10' or f['Y'] != '20' or f['Z'] != '3':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    ds = gdal.OpenEx('/vsimem/ogr_csv_46.csv', gdal.OF_VECTOR | gdal.OF_UPDATE, open_options = ['KEEP_GEOM_COLUMNS=NO', 'X_POSSIBLE_NAMES=X', 'Y_POSSIBLE_NAMES=Y'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (-10 -20 30)'))
    lyr.SetFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_46.csv')
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldCount() != 4:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f['id'] != '1' or f['X'] != '-10' or f['Y'] != '-20' or f['Z'] != '3':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    ds = gdal.OpenEx('/vsimem/ogr_csv_46.csv', gdal.OF_VECTOR | gdal.OF_UPDATE, open_options = ['KEEP_GEOM_COLUMNS=NO', 'X_POSSIBLE_NAMES=X', 'Y_POSSIBLE_NAMES=Y', 'Z_POSSIBLE_NAMES=Z'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (-1 -2 -3)'))
    lyr.SetFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_46.csv')
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldCount() != 4:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f['id'] != '1' or f['X'] != '-1' or f['Y'] != '-2' or f['Z'] != '-3':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_46.csv')

    return 'success'

###############################################################################
# Test writing XYZM

def ogr_csv_47():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_47.csv')
    if ds.TestCapability(ogr.ODsCMeasuredGeometries) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.CreateLayer('ogr_csv_47', options = ['GEOMETRY=AS_WKT'])
    if lyr.TestCapability(ogr.OLCMeasuredGeometries) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 1
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT ZM (1 2 3 4)'))
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_47.csv')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != 'POINT ZM (1 2 3 4)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_47.csv')

    return 'success'

###############################################################################
# Test reading/writing StringList, etc..

def ogr_csv_48():

    gdal.FileFromMemBuffer('/vsimem/ogr_csv_48.csvt', 'JsonStringList,JsonIntegerList,JsonInteger64List,JsonRealList\n')
    gdal.FileFromMemBuffer('/vsimem/ogr_csv_48.csv',
"""stringlist,intlist,int64list,reallist
"[""a"",null]","[1]","[1234567890123]","[0.125]"
""")

    gdal.VectorTranslate('/vsimem/ogr_csv_48_out.csv', '/vsimem/ogr_csv_48.csv', format = 'CSV', layerCreationOptions = ['CREATE_CSVT=YES', 'LINEFORMAT=LF'])

    f = gdal.VSIFOpenL('/vsimem/ogr_csv_48_out.csv', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data.find('stringlist,intlist,int64list,reallist\n"[ ""a"", """" ]",[ 1 ],[ 1234567890123 ],[ 0.125') != 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    f = gdal.VSIFOpenL('/vsimem/ogr_csv_48_out.csvt', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    if data.find('JSonStringList,JSonIntegerList,JSonInteger64List,JSonRealList') != 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    gdal.Unlink('/vsimem/ogr_csv_48.csv')
    gdal.Unlink('/vsimem/ogr_csv_48.csvt')
    gdal.Unlink('/vsimem/ogr_csv_48_out.csv')
    gdal.Unlink('/vsimem/ogr_csv_48_out.csvt')

    return 'success'

###############################################################################
#

def ogr_csv_cleanup():

    if gdaltest.csv_ds is None:
        return 'skip'

    gdaltest.csv_ds = None

    try:
        gdaltest.csv_lyr1 = None
        gdaltest.csv_lyr2 = None
        gdaltest.csv_tmpds = None
    except:
        pass

    try:
        gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        ogr.GetDriverByName('CSV').DeleteDataSource( 'tmp/csvwrk' )
        gdal.PopErrorHandler()
    except:
        pass

    try:
        gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        ogr.GetDriverByName('CSV').DeleteDataSource( 'tmp/ogr_csv_29' )
        gdal.PopErrorHandler()
    except:
        pass

    return 'success'

gdaltest_list = [
    ogr_csv_1,
    ogr_csv_2,
    ogr_csv_3,
    ogr_csv_4,
    ogr_csv_5,
    ogr_csv_6,
    ogr_csv_6,
    ogr_csv_7,
    ogr_csv_8,
    ogr_csv_9,
    ogr_csv_10,
    ogr_csv_11,
    ogr_csv_12,
    ogr_csv_13,
    ogr_csv_14,
    ogr_csv_15,
    ogr_csv_16,
    ogr_csv_17,
    ogr_csv_18,
    ogr_csv_19,
    ogr_csv_20,
    ogr_csv_21,
    ogr_csv_22,
    ogr_csv_23,
    ogr_csv_24,
    ogr_csv_25,
    ogr_csv_26,
    ogr_csv_27,
    ogr_csv_28,
    ogr_csv_29,
    ogr_csv_30,
    ogr_csv_31,
    ogr_csv_32,
    ogr_csv_33,
    ogr_csv_34,
    ogr_csv_35,
    ogr_csv_36,
    ogr_csv_37,
    ogr_csv_38,
    ogr_csv_39,
    ogr_csv_40,
    ogr_csv_41,
    ogr_csv_42,
    ogr_csv_43,
    ogr_csv_44,
    ogr_csv_45,
    ogr_csv_46,
    ogr_csv_47,
    ogr_csv_48,
    ogr_csv_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_csv' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
