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
import gdal

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

    lyr = gdaltest.csv_ds.GetLayerByName( 'prime_meridian' )

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

        feat.Destroy()
        feat = srclyr.GetNextFeature()

    dst_feat.Destroy()

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

    err = gdaltest.csv_tmpds.DeleteLayer(0)

    if err != 0:
        gdaltest.post_reason( 'got error code from DeleteLayer' )
        return 'fail'

    if gdaltest.csv_tmpds.GetLayerCount() != 1 \
       or gdaltest.csv_tmpds.GetLayer(0).GetName() != 'pm2':
        gdaltest.post_reason( 'Layer not destroyed properly?' )
        return 'fail'

    gdaltest.csv_tmpds.Destroy()
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

    feat.Destroy()

    gdaltest.csv_tmpds.Destroy()
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
        gdaltest.post_reason( 'CSV files dont efficiently support random reading.')
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
    feat.Destroy()

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('DATETIME') != '':
        print(feat.GetFieldAsString('DATETIME'))
        return 'fail'
    feat.Destroy()

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('DATE') != '2008/12/25':
        print(feat.GetFieldAsString('DATE'))
        return 'fail'
    feat.Destroy()

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('DATE') != '':
        print(feat.GetFieldAsString('DATE'))
        return 'fail'
    feat.Destroy()

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('TIME') != '11:22:33':
        print(feat.GetFieldAsString('TIME'))
        return 'fail'
    feat.Destroy()

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('TIME') != '':
        print(feat.GetFieldAsString('TIME'))
        return 'fail'
    feat.Destroy()

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

    gdaltest.csv_ds.Destroy()
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

        feat.Destroy()
        feat = srclyr.GetNextFeature()

    dst_feat.Destroy()

    #######################################################
    # Closes everything and reopen
    gdaltest.csv_tmpds.Destroy()
    gdaltest.csv_tmpds = None

    gdaltest.csv_ds.Destroy()
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
    options = ['GEOMETRY=AS_WKT',]
    lyr = gdaltest.csv_tmpds.CreateLayer( 'as_wkt', options = options )

    field_defn = ogr.FieldDefn( 'ADATA', ogr.OFTString )
    lyr.CreateField(field_defn)
    field_defn.Destroy()

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    # AS_XY
    options = ['GEOMETRY=AS_XY','CREATE_CSVT=YES']
    lyr = gdaltest.csv_tmpds.CreateLayer( 'as_xy', options = options )

    field_defn = ogr.FieldDefn( 'ADATA', ogr.OFTString )
    lyr.CreateField(field_defn)
    field_defn.Destroy()

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    # Nothing will be written in the x or y field
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(1 2,3 4)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    # AS_YX
    options = ['GEOMETRY=AS_YX','CREATE_CSVT=YES']
    lyr = gdaltest.csv_tmpds.CreateLayer( 'as_yx', options = options )

    field_defn = ogr.FieldDefn( 'ADATA', ogr.OFTString )
    lyr.CreateField(field_defn)
    field_defn.Destroy()

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    # AS_XYZ
    options = ['GEOMETRY=AS_XYZ','CREATE_CSVT=YES']
    lyr = gdaltest.csv_tmpds.CreateLayer( 'as_xyz', options = options )

    field_defn = ogr.FieldDefn( 'ADATA', ogr.OFTString )
    lyr.CreateField(field_defn)
    field_defn.Destroy()

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2 3)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    #######################################################
    # Closes everything and reopen
    gdaltest.csv_tmpds.Destroy()
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
    gdaltest.csv_ds.Destroy()
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

    gdaltest.csv_tmpds.Destroy()
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

    feat.Destroy()

    feat = csv_lyr.GetNextFeature()
    feat.Destroy()

    feat = csv_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry( feat, 'POLYGON((1.001 1.001,3.999 3.999,3.2 1.6,1.001 1.001))'):
        return 'fail'

    feat.Destroy()

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

    gdaltest.csv_ds.Destroy()
    gdaltest.csv_ds = None
    gdaltest.csv_ds = ogr.Open( 'data/testnull.csv' )

    if gdaltest.csv_ds is None:
        return 'fail'

    lyr = gdaltest.csv_ds.GetLayerByName( 'testnull' )

    lyr.ResetReading()
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

    gdaltest.csv_ds.Destroy()
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

    gdaltest.csv_ds.Destroy()
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

    gdaltest.csv_ds.Destroy()
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

    gdaltest.csv_ds.Destroy()
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
    if data[:6] != '\xef\xbb\xbfWKT':
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

    data = open('tmp/csvwrk/newlines.csv', 'rb').read()
    if data != EXPECTED:
        gdaltest.post_reason("Newlines changed:\n\texpected=%s\n\tgot=     %s" % (repr(EXPECTED), repr(data)))
        return 'fail'

    return 'success'

###############################################################################
#

def ogr_csv_cleanup():

    if gdaltest.csv_ds is None:
        return 'skip'

    gdaltest.csv_ds.Destroy()
    gdaltest.csv_ds = None

    try:
        gdaltest.csv_lyr1 = None
        gdaltest.csv_lyr2 = None
        gdaltest.csv_tmpds.Destroy()
        gdaltest.csv_tmpds = None
    except:
        pass

    try:
        gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        ogr.GetDriverByName('CSV').DeleteDataSource( 'tmp/csvwrk' )
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
    ogr_csv_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_csv' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

