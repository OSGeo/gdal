#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SQLite driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

# Make sure we run from the directory of the script
if os.path.basename(sys.argv[0]) == os.path.basename(__file__):
    if os.path.dirname(sys.argv[0]) != '':
        os.chdir(os.path.dirname(sys.argv[0]))

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import osr
import gdal

###############################################################################
# Create a fresh database.

def ogr_sqlite_1():

    gdaltest.sl_ds = None
    gdaltest.has_spatialite = False

    try:
        sqlite_dr = ogr.GetDriverByName( 'SQLite' )
        if sqlite_dr is None:
            return 'skip'
    except:
        return 'skip'

    try:
        os.remove( 'tmp/sqlite_test.db' )
    except:
        pass

    # This is to speed-up the runtime of tests on EXT4 filesystems
    # Do not use this for production environment if you care about data safety
    # w.r.t system/OS crashes, unless you know what you are doing.
    gdal.SetConfigOption('OGR_SQLITE_SYNCHRONOUS', 'OFF')

    gdaltest.sl_ds = sqlite_dr.CreateDataSource( 'tmp/sqlite_test.db' )

    if gdaltest.sl_ds is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Create table from data/poly.shp

def ogr_sqlite_2():

    if gdaltest.sl_ds is None:
        return 'skip'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.sl_ds.ExecuteSQL( 'DELLAYER:tpoly' )
    gdal.PopErrorHandler()

    ######################################################
    # Create Layer
    gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer( 'tpoly' )

    ######################################################
    # Setup Schema
    
    fields = [ ('AREA', ogr.OFTReal),
               ('EAS_ID', ogr.OFTInteger),
               ('PRFEDEA', ogr.OFTString),
               ('BINCONTENT', ogr.OFTBinary)]
    
    ogrtest.quick_create_layer_def( gdaltest.sl_lyr,
                                    fields )
                                      
    ######################################################
    # Reopen database to be sure that the data types are properly read
    # even if no record are written
    
    gdaltest.sl_ds.Destroy()
    gdaltest.sl_ds = ogr.Open( 'tmp/sqlite_test.db', update = 1  )
    gdaltest.sl_lyr = gdaltest.sl_ds.GetLayerByName( 'tpoly')
    
    for field_desc in fields:
        feature_def = gdaltest.sl_lyr.GetLayerDefn()
        field_defn = feature_def.GetFieldDefn(feature_def.GetFieldIndex(field_desc[0]))
        if field_defn.GetType() != field_desc[1]:
            print('Expected type for %s is %s, not %s' % \
                (field_desc[0], field_defn.GetFieldTypeName(field_defn.GetType()), \
                 field_defn.GetFieldTypeName(field_desc[1])))

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def )

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)
    
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    gdaltest.sl_lyr.StartTransaction()
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.sl_lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()
    
    gdaltest.sl_lyr.CommitTransaction()
        
    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.

def ogr_sqlite_3():
    if gdaltest.sl_ds is None:
        return 'skip'

    if gdaltest.sl_lyr.GetFeatureCount() != 10:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 10' % gdaltest.sl_lyr.GetFeatureCount() )
        return 'fail'

    expect = [168, 169, 166, 158, 165]

    gdaltest.sl_lyr.SetAttributeFilter( 'eas_id < 170' )
    tr = ogrtest.check_features_against_list( gdaltest.sl_lyr,
                                              'eas_id', expect )

    if gdaltest.sl_lyr.GetFeatureCount() != 5:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 5' % gdaltest.sl_lyr.GetFeatureCount() )
        return 'fail'

    gdaltest.sl_lyr.SetAttributeFilter( None )

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.sl_lyr.GetNextFeature()

        if read_feat is None:
            gdaltest.post_reason( 'Did not get as many features as expected.')
            return 'fail'

        if ogrtest.check_feature_geometry(read_feat,orig_feat.GetGeometryRef(),
                                          max_error = 0.001 ) != 0:
            return 'fail'

        for fld in range(3):
            if orig_feat.GetField(fld) != read_feat.GetField(fld):
                gdaltest.post_reason( 'Attribute %d does not match' % fld )
                return 'fail'

        read_feat.Destroy()
        orig_feat.Destroy()

    gdaltest.poly_feat = None
    gdaltest.shp_ds.Destroy()

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Write more features with a bunch of different geometries, and verify the
# geometries are still OK.

def ogr_sqlite_4():

    if gdaltest.sl_ds is None:
        return 'skip'

    dst_feat = ogr.Feature( feature_def = gdaltest.sl_lyr.GetLayerDefn() )
    wkt_list = [ '10', '2', '1', '3d_1', '4', '5', '6' ]

    for item in wkt_list:

        wkt = open( 'data/wkb_wkt/'+item+'.wkt' ).read()
        geom = ogr.CreateGeometryFromWkt( wkt )
        
        ######################################################################
        # Write geometry as a new feature.
    
        dst_feat.SetGeometryDirectly( geom )
        dst_feat.SetField( 'PRFEDEA', item )
        dst_feat.SetFID( -1 )
        gdaltest.sl_lyr.CreateFeature( dst_feat )
        
        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.sl_lyr.SetAttributeFilter( "PRFEDEA = '%s'" % item )
        feat_read = gdaltest.sl_lyr.GetNextFeature()

        if feat_read is None:
            gdaltest.post_reason( 'Did not get as many features as expected.')
            return 'fail'

        geom_read = feat_read.GetGeometryRef()

        if ogrtest.check_feature_geometry( feat_read, geom ) != 0:
            return 'fail'

        feat_read.Destroy()

    dst_feat.Destroy()
    
    return 'success'
    
###############################################################################
# Test ExecuteSQL() results layers without geometry.

def ogr_sqlite_5():

    if gdaltest.sl_ds is None:
        return 'skip'

    expect = [ 179, 173, 172, 171, 170, 169, 168, 166, 165, 158, None ]
    
    sql_lyr = gdaltest.sl_ds.ExecuteSQL( 'select distinct eas_id from tpoly order by eas_id desc' )

    if sql_lyr.GetFeatureCount() != 11:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 11' % sql_lyr.GetFeatureCount() )
        return 'fail'

    tr = ogrtest.check_features_against_list( sql_lyr, 'eas_id', expect )

    gdaltest.sl_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test ExecuteSQL() results layers with geometry.

def ogr_sqlite_6():

    if gdaltest.sl_ds is None:
        return 'skip'

    sql_lyr = gdaltest.sl_ds.ExecuteSQL( "select * from tpoly where prfedea = '2'" )

    tr = ogrtest.check_features_against_list( sql_lyr, 'prfedea', [ '2' ] )
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry( feat_read, 'MULTILINESTRING ((5.00121349 2.99853132,5.00121349 1.99853133),(5.00121349 1.99853133,5.00121349 0.99853133),(3.00121351 1.99853127,5.00121349 1.99853133),(5.00121349 1.99853133,6.00121348 1.99853135))' ) != 0:
            tr = 0
        feat_read.Destroy()
        
    gdaltest.sl_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test spatial filtering. 

def ogr_sqlite_7():

    if gdaltest.sl_ds is None:
        return 'skip'

    gdaltest.sl_lyr.SetAttributeFilter( None )
    
    geom = ogr.CreateGeometryFromWkt( \
        'LINESTRING(479505 4763195,480526 4762819)' )
    gdaltest.sl_lyr.SetSpatialFilter( geom )
    geom.Destroy()

    if gdaltest.sl_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 1' % gdaltest.sl_lyr.GetFeatureCount() )
        return 'fail'

    tr = ogrtest.check_features_against_list( gdaltest.sl_lyr, 'eas_id',
                                              [ 158 ] )

    gdaltest.sl_lyr.SetAttributeFilter( 'eas_id = 158' )

    if gdaltest.sl_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 1' % gdaltest.sl_lyr.GetFeatureCount() )
        return 'fail'

    gdaltest.sl_lyr.SetAttributeFilter( None )

    gdaltest.sl_lyr.SetSpatialFilter( None )
    
    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test transactions with rollback.  

def ogr_sqlite_8():

    if gdaltest.sl_ds is None:
        return 'skip'

    ######################################################################
    # Prepare working feature.
    
    dst_feat = ogr.Feature( feature_def = gdaltest.sl_lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POINT(10 20)' ))

    dst_feat.SetField( 'PRFEDEA', 'rollbacktest' )

    ######################################################################
    # Create it, but rollback the transaction.
    
    gdaltest.sl_lyr.StartTransaction()
    gdaltest.sl_lyr.CreateFeature( dst_feat )
    gdaltest.sl_lyr.RollbackTransaction()

    ######################################################################
    # Verify that it is not in the layer.

    gdaltest.sl_lyr.SetAttributeFilter( "PRFEDEA = 'rollbacktest'" )
    feat_read = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter( None )

    if feat_read is not None:
        gdaltest.post_reason( 'Unexpectedly got rollbacktest feature.' )
        return 'fail'

    ######################################################################
    # Create it, and commit the transaction.
    
    gdaltest.sl_lyr.StartTransaction()
    gdaltest.sl_lyr.CreateFeature( dst_feat )
    gdaltest.sl_lyr.CommitTransaction()

    ######################################################################
    # Verify that it is not in the layer.

    gdaltest.sl_lyr.SetAttributeFilter( "PRFEDEA = 'rollbacktest'" )
    feat_read = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter( None )

    if feat_read is None:
        gdaltest.post_reason( 'Failed to get committed feature.' )
        return 'fail'

    feat_read.Destroy()
    dst_feat.Destroy()

    return 'success'
    
###############################################################################
# Test SetFeature()

def ogr_sqlite_9():

    if gdaltest.sl_ds is None:
        return 'skip'

    ######################################################################
    # Read feature with EAS_ID 158. 

    gdaltest.sl_lyr.SetAttributeFilter( "eas_id = 158" )
    feat_read = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter( None )

    if feat_read is None:
        gdaltest.post_reason( 'did not find eas_id 158!' )
        return 'fail'

    ######################################################################
    # Modify the PRFEDEA value, and reset it. 

    feat_read.SetField( 'PRFEDEA', 'SetWorked' )
    err = gdaltest.sl_lyr.SetFeature( feat_read )
    if err != 0:
        gdaltest.post_reason( 'SetFeature() reported error %d' % err )
        return 'fail'

    ######################################################################
    # Read feature with EAS_ID 158 and check that PRFEDEA was altered.

    gdaltest.sl_lyr.SetAttributeFilter( "eas_id = 158" )
    feat_read_2 = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter( None )

    if feat_read_2 is None:
        gdaltest.post_reason( 'did not find eas_id 158!' )
        return 'fail'

    if feat_read_2.GetField('PRFEDEA') != 'SetWorked':
        feat_read_2.DumpReadable()
        gdaltest.post_reason( 'PRFEDEA apparently not reset as expected.' )
        return 'fail'

    feat_read.Destroy()
    feat_read_2.Destroy()

    return 'success'
    
###############################################################################
# Test GetFeature()

def ogr_sqlite_10():

    if gdaltest.sl_ds is None:
        return 'skip'

    ######################################################################
    # Read feature with EAS_ID 158. 

    gdaltest.sl_lyr.SetAttributeFilter( "eas_id = 158" )
    feat_read = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter( None )

    if feat_read is None:
        gdaltest.post_reason( 'did not find eas_id 158!' )
        return 'fail'

    ######################################################################
    # Now read the feature by FID.

    feat_read_2 = gdaltest.sl_lyr.GetFeature( feat_read.GetFID() )

    if feat_read_2 is None:
        gdaltest.post_reason( 'did not find FID %d' % feat_read.GetFID() )
        return 'fail'

    if feat_read_2.GetField('PRFEDEA') != feat_read.GetField('PRFEDEA'):
        feat_read.DumpReadable()
        feat_read_2.DumpReadable()
        gdaltest.post_reason( 'GetFeature() result seems to not match expected.' )
        return 'fail'

    feat_read.Destroy()
    feat_read_2.Destroy()

    return 'success'
    
###############################################################################
# Test FORMAT=WKB creation option

def ogr_sqlite_11():

    if gdaltest.sl_ds is None:
        return 'skip'

    ######################################################
    # Create Layer with WKB geometry
    gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer( 'geomwkb', options = [ 'FORMAT=WKB' ] )

    geom = ogr.CreateGeometryFromWkt( 'POINT(0 1)' )
    dst_feat = ogr.Feature( feature_def = gdaltest.sl_lyr.GetLayerDefn() )
    dst_feat.SetGeometry( geom )
    gdaltest.sl_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    # Test adding a column to see if geometry is preserved (#3471)
    gdaltest.sl_lyr.CreateField( ogr.FieldDefn("foo", ogr.OFTString) )

    ######################################################
    # Reopen DB
    gdaltest.sl_ds.Destroy()
    gdaltest.sl_ds = ogr.Open( 'tmp/sqlite_test.db', update = 1  )
    gdaltest.sl_lyr = gdaltest.sl_ds.GetLayerByName('geomwkb')

    feat_read = gdaltest.sl_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat_read,geom,max_error = 0.001 ) != 0:
        return 'fail'
    feat_read.Destroy()

    gdaltest.sl_lyr.ResetReading()

    return 'success'

###############################################################################
# Test FORMAT=WKT creation option

def ogr_sqlite_12():

    if gdaltest.sl_ds is None:
        return 'skip'

    ######################################################
    # Create Layer with WKT geometry
    gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer( 'geomwkt', options = [ 'FORMAT=WKT' ] )

    geom = ogr.CreateGeometryFromWkt( 'POINT(0 1)' )
    dst_feat = ogr.Feature( feature_def = gdaltest.sl_lyr.GetLayerDefn() )
    dst_feat.SetGeometry( geom )
    gdaltest.sl_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    # Test adding a column to see if geometry is preserved (#3471)
    gdaltest.sl_lyr.CreateField( ogr.FieldDefn("foo", ogr.OFTString) )

    ######################################################
    # Reopen DB
    gdaltest.sl_ds.Destroy()
    gdaltest.sl_ds = ogr.Open( 'tmp/sqlite_test.db', update = 1  )
    gdaltest.sl_lyr = gdaltest.sl_ds.GetLayerByName('geomwkt')

    feat_read = gdaltest.sl_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat_read,geom,max_error = 0.001 ) != 0:
        return 'fail'
    feat_read.Destroy()

    gdaltest.sl_lyr.ResetReading()

    sql_lyr = gdaltest.sl_ds.ExecuteSQL( "select * from geomwkt" )

    feat_read = sql_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat_read,geom,max_error = 0.001 ) != 0:
        return 'fail'
    feat_read.Destroy()

    feat_read = sql_lyr.GetFeature(0)
    if ogrtest.check_feature_geometry(feat_read,geom,max_error = 0.001 ) != 0:
        return 'fail'
    feat_read.Destroy()

    gdaltest.sl_ds.ReleaseResultSet( sql_lyr )

    return 'success'

###############################################################################
# Test SRID support

def ogr_sqlite_13():

    if gdaltest.sl_ds is None:
        return 'skip'

    ######################################################
    # Create Layer with EPSG:4326
    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4326 )
    gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer( 'wgs84layer', srs = srs )

    ######################################################
    # Reopen DB
    gdaltest.sl_ds.Destroy()
    gdaltest.sl_ds = ogr.Open( 'tmp/sqlite_test.db', update = 1  )
    gdaltest.sl_lyr = gdaltest.sl_ds.GetLayerByName('wgs84layer')

    if not gdaltest.sl_lyr.GetSpatialRef().IsSame(srs):
        gdaltest.post_reason('SRS is not the one expected.')
        return 'fail'

    ######################################################
    # Create second layer with very approximative EPSG:4326
    srs = osr.SpatialReference()
    srs.SetFromUserInput('GEOGCS["WGS 84",AUTHORITY["EPSG","4326"]]')
    gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer( 'wgs84layer_approx', srs = srs )

    # Must still be 1
    sql_lyr = gdaltest.sl_ds.ExecuteSQL("SELECT COUNT(*) AS count FROM spatial_ref_sys");
    feat = sql_lyr.GetNextFeature()
    if  feat.GetFieldAsInteger('count') != 1:
        return 'fail'
    gdaltest.sl_ds.ReleaseResultSet(sql_lyr)

    return 'success'


###############################################################################
# Test all column types

def ogr_sqlite_14():

    if gdaltest.sl_ds is None:
        return 'skip'

    gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer( 'testtypes' )
    ogrtest.quick_create_layer_def( gdaltest.sl_lyr,
                                    [ ('INTEGER', ogr.OFTInteger),
                                      ('FLOAT', ogr.OFTReal),
                                      ('STRING', ogr.OFTString),
                                      ('BLOB', ogr.OFTBinary),
                                      ('BLOB2', ogr.OFTBinary) ] )

    dst_feat = ogr.Feature( feature_def = gdaltest.sl_lyr.GetLayerDefn() )

    dst_feat.SetField('INTEGER', 1)
    dst_feat.SetField('FLOAT', 1.2)
    dst_feat.SetField('STRING', 'myString\'a')

    gdaltest.sl_lyr.CreateFeature( dst_feat )

    dst_feat.Destroy()

    # Set the BLOB attribute via SQL UPDATE instructions as there's no binding
    # for OGR_F_SetFieldBinary
    gdaltest.sl_ds.ExecuteSQL("UPDATE testtypes SET BLOB = x'0001FF' WHERE OGC_FID = 1")

    ######################################################
    # Reopen DB
    gdaltest.sl_ds.Destroy()
    gdaltest.sl_ds = ogr.Open( 'tmp/sqlite_test.db', update = 1  )
    gdaltest.sl_lyr = gdaltest.sl_ds.GetLayerByName('testtypes')

    # Duplicate the first record
    dst_feat = ogr.Feature( feature_def = gdaltest.sl_lyr.GetLayerDefn() )
    feat_read = gdaltest.sl_lyr.GetNextFeature()
    dst_feat.SetFrom(feat_read)
    gdaltest.sl_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    # Check the 2 records
    gdaltest.sl_lyr.ResetReading()
    for i in range(2):
        feat_read = gdaltest.sl_lyr.GetNextFeature()
        if feat_read.GetField('INTEGER') != 1 or \
           feat_read.GetField('FLOAT') != 1.2 or \
           feat_read.GetField('STRING') != 'myString\'a' or \
           feat_read.GetFieldAsString('BLOB') != '0001FF':
            return 'fail'

    gdaltest.sl_lyr.ResetReading()

    return 'success'

###############################################################################
# Test FORMAT=SPATIALITE layer creation option

def ogr_sqlite_15():

    if gdaltest.sl_ds is None:
        return 'skip'

    ######################################################
    # Create Layer with SPATIALITE geometry
    gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer( 'geomspatialite', options = [ 'FORMAT=SPATIALITE' ] )

    geoms = [ ogr.CreateGeometryFromWkt( 'POINT(0 1)' ),
              ogr.CreateGeometryFromWkt( 'MULTIPOINT EMPTY' ),
              ogr.CreateGeometryFromWkt( 'MULTIPOINT (0 1,2 3)' ),
              ogr.CreateGeometryFromWkt( 'LINESTRING EMPTY' ),
              ogr.CreateGeometryFromWkt( 'LINESTRING (1 2,3 4)' ),
              ogr.CreateGeometryFromWkt( 'MULTILINESTRING EMPTY' ),
              ogr.CreateGeometryFromWkt( 'MULTILINESTRING ((1 2,3 4),(5 6,7 8))' ),
              ogr.CreateGeometryFromWkt( 'POLYGON EMPTY' ),
              ogr.CreateGeometryFromWkt( 'POLYGON ((1 2,3 4))' ),
              ogr.CreateGeometryFromWkt( 'POLYGON ((1 2,3 4),(5 6,7 8))' ),
              ogr.CreateGeometryFromWkt( 'MULTIPOLYGON EMPTY' ),
              ogr.CreateGeometryFromWkt( 'MULTIPOLYGON (((1 2,3 4)),((5 6,7 8)))' ),
              ogr.CreateGeometryFromWkt( 'GEOMETRYCOLLECTION EMPTY' ),
              ogr.CreateGeometryFromWkt( 'GEOMETRYCOLLECTION (POLYGON ((1 2,3 4)),POLYGON ((5 6,7 8)))' ),
              ogr.CreateGeometryFromWkt( 'GEOMETRYCOLLECTION (POLYGON ((1 2,3 4)),POINT(0 1))' ) ]

    gdaltest.sl_lyr.StartTransaction()
    
    for geom in geoms:
        dst_feat = ogr.Feature( feature_def = gdaltest.sl_lyr.GetLayerDefn() )
        #print(geom)
        dst_feat.SetGeometry( geom )
        gdaltest.sl_lyr.CreateFeature( dst_feat )
        dst_feat.Destroy()

    gdaltest.sl_lyr.CommitTransaction()
    
    ######################################################
    # Reopen DB
    gdaltest.sl_ds.Destroy()
    gdaltest.sl_ds = ogr.Open( 'tmp/sqlite_test.db'  )
    gdaltest.sl_lyr = gdaltest.sl_ds.GetLayerByName('geomspatialite')

    for geom in geoms:
        feat_read = gdaltest.sl_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry(feat_read,geom,max_error = 0.001 ) != 0:
            return 'fail'
        feat_read.Destroy()

    gdaltest.sl_lyr.ResetReading()

    sql_lyr = gdaltest.sl_ds.ExecuteSQL( "select * from geomspatialite" )

    feat_read = sql_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat_read,geoms[0],max_error = 0.001 ) != 0:
        return 'fail'
    feat_read.Destroy()

    feat_read = sql_lyr.GetFeature(0)
    if ogrtest.check_feature_geometry(feat_read,geoms[0],max_error = 0.001 ) != 0:
        return 'fail'
    feat_read.Destroy()

    gdaltest.sl_ds.ReleaseResultSet( sql_lyr )

    return 'success'


###############################################################################
# Test reading geometries in FGF (FDO Geometry Format) binary representation.

def ogr_sqlite_16():

    if gdaltest.sl_ds is None:
        return 'skip'

    ######################################################
    # Reopen DB in update
    gdaltest.sl_ds.Destroy()
    gdaltest.sl_ds = ogr.Open( 'tmp/sqlite_test.db', update = 1  )

    # Hand create a table with FGF geometry
    gdaltest.sl_ds.ExecuteSQL( "INSERT INTO geometry_columns (f_table_name, f_geometry_column, geometry_type, coord_dimension, geometry_format) VALUES ('fgf_table', 'GEOMETRY', 0, 2, 'FGF')" )
    gdaltest.sl_ds.ExecuteSQL( "CREATE TABLE fgf_table (OGC_FID INTEGER PRIMARY KEY, GEOMETRY BLOB)")
    gdaltest.sl_ds.ExecuteSQL( "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (1, X'0100000000000000000000000000F03F0000000000000040')" )
    gdaltest.sl_ds.ExecuteSQL( "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (2, X'020000000000000000000000')" )
    gdaltest.sl_ds.ExecuteSQL( "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (3, X'020000000000000002000000000000000000F03F000000000000004000000000000008400000000000001040')" )
    gdaltest.sl_ds.ExecuteSQL( "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (4, X'030000000000000000000000')" )
    gdaltest.sl_ds.ExecuteSQL( "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (5, X'03000000000000000200000002000000000000000000F03F00000000000000400000000000000840000000000000104000000000')" )
    gdaltest.sl_ds.ExecuteSQL( "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (6, X'0700000000000000')" )
    gdaltest.sl_ds.ExecuteSQL( "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (7, X'070000000200000003000000000000000200000002000000000000000000F03F0000000000000040000000000000084000000000000010400000000003000000000000000200000002000000000000000000F03F00000000000000400000000000000840000000000000104000000000')" )
    gdaltest.sl_ds.ExecuteSQL( "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (8, X'0100000001000000000000000000F03F00000000000000400000000000000840')" )
    
    # invalid geometries
    gdaltest.sl_ds.ExecuteSQL( "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (9, X'0700000001000000')" )
    gdaltest.sl_ds.ExecuteSQL( "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (10,X'060000000100000001')" )
    gdaltest.sl_ds.ExecuteSQL( "INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (11,X'06000000010000000100000000000000000000000000F03F0000000000000040')" )

    ######################################################
    # Reopen DB
    gdaltest.sl_ds.Destroy()
    gdaltest.sl_ds = ogr.Open( 'tmp/sqlite_test.db', update = 1 )
    gdaltest.sl_lyr = gdaltest.sl_ds.GetLayerByName('fgf_table')

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (1 2)':
        return 'fail'
    feat.Destroy()

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'LINESTRING EMPTY':
        return 'fail'
    feat.Destroy()

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'LINESTRING (1 2,3 4)':
        return 'fail'
    feat.Destroy()

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POLYGON EMPTY':
        return 'fail'
    feat.Destroy()

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POLYGON ((1 2,3 4))':
        return 'fail'
    feat.Destroy()

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'GEOMETRYCOLLECTION EMPTY':
        return 'fail'
    feat.Destroy()

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'GEOMETRYCOLLECTION (POLYGON ((1 2,3 4)),POLYGON ((1 2,3 4)))':
        return 'fail'
    feat.Destroy()

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (1 2 3)':
        return 'fail'
    feat.Destroy()
    
    # Test invalid geometries
    for i in range(3):
        feat = gdaltest.sl_lyr.GetNextFeature()
        geom = feat.GetGeometryRef()
        if geom is not None:
            return 'fail'
        
    gdaltest.sl_lyr.ResetReading()

    return 'success'

###############################################################################
# Test SPATIALITE dataset creation option

def ogr_sqlite_17():

    if gdaltest.sl_ds is None:
        return 'skip'

    if gdaltest.has_spatialite == False:
        return 'skip'

    ######################################################
    # Create dataset with SPATIALITE geometry

    ds = ogr.GetDriverByName( 'SQLite' ).CreateDataSource( 'tmp/spatialite_test.db', options = ['SPATIALITE=YES'] )
    srs = osr.SpatialReference()
    srs.SetFromUserInput("""GEOGCS["GCS_WGS_1984",DATUM["WGS_1984",SPHEROID["WGS_1984",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]]""")
    lyr = ds.CreateLayer( 'geomspatialite', srs = srs )

    geom = ogr.CreateGeometryFromWkt( 'POINT(0 1)' )

    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometry( geom )
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    ######################################################
    # Reopen DB
    ds.Destroy()
    ds = ogr.Open( 'tmp/spatialite_test.db'  )
    lyr =ds.GetLayerByName('geomspatialite')

    feat_read = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat_read,geom,max_error = 0.001 ) != 0:
        return 'fail'
    feat_read.Destroy()

    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    if wkt.find('4326') == -1:
        gdaltest.post_reason('did not identify correctly SRS')
        print(wkt)
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Create a layer with a non EPSG SRS into a SPATIALITE DB (#3506)

def ogr_sqlite_18():

    if gdaltest.sl_ds is None:
        return 'skip'

    if gdaltest.has_spatialite == False:
        return 'skip'

    ds = ogr.Open( 'tmp/spatialite_test.db', update = 1  )
    srs = osr.SpatialReference()
    srs.SetFromUserInput('+proj=vandg')
    lyr = ds.CreateLayer( 'nonepsgsrs', srs = srs )

    ######################################################
    # Reopen DB
    ds.Destroy()
    ds = ogr.Open( 'tmp/spatialite_test.db'  )

    lyr =ds.GetLayerByName('nonepsgsrs')
    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    if wkt.find('VanDerGrinten') == -1:
        gdaltest.post_reason('did not identify correctly SRS')
        print(wkt)
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Create a SpatiaLite DB with INIT_WITH_EPSG=YES

def ogr_sqlite_19():

    if gdaltest.sl_ds is None:
        return 'skip'

    if int(gdal.VersionInfo('VERSION_NUM')) < 1800:
        return 'skip'

    if gdaltest.has_spatialite == False:
        return 'skip'

    if gdaltest.spatialite_version != '2.3.1':
        return 'skip'

    ds = ogr.GetDriverByName( 'SQLite' ).CreateDataSource( 'tmp/spatialite_test_with_epsg.db', options = ['SPATIALITE=YES', 'INIT_WITH_EPSG=YES'] )

    # EPSG:26632 has a ' character in it's WKT representation
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:26632')
    lyr = ds.CreateLayer( 'test', srs = srs )

    ds.Destroy()
    ds = ogr.Open('tmp/spatialite_test_with_epsg.db')

    sql_lyr = ds.ExecuteSQL( "select count(*) from spatial_ref_sys" )
    feat = sql_lyr.GetNextFeature()
    nb_srs = feat.GetFieldAsInteger(0)
    ds.ReleaseResultSet( sql_lyr )

    ds.Destroy()

    # Currently the injection of the EPSG DB as proj.4 strings adds 3915 entries
    if nb_srs < 3915:
        gdaltest.post_reason('did not get expected SRS count')
        print(nb_srs)
        return 'fail'

    return 'success'

###############################################################################
# Create a regular DB with INIT_WITH_EPSG=YES

def ogr_sqlite_20():

    if gdaltest.sl_ds is None:
        return 'skip'

    ds = ogr.GetDriverByName( 'SQLite' ).CreateDataSource( 'tmp/non_spatialite_test_with_epsg.db', options = ['INIT_WITH_EPSG=YES'] )

    # EPSG:26632 has a ' character in it's WKT representation
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:26632')
    lyr = ds.CreateLayer( 'test', srs = srs )

    ds.Destroy()
    ds = ogr.Open('tmp/non_spatialite_test_with_epsg.db')

    sql_lyr = ds.ExecuteSQL( "select count(*) from spatial_ref_sys" )
    feat = sql_lyr.GetNextFeature()
    nb_srs = feat.GetFieldAsInteger(0)
    ds.ReleaseResultSet( sql_lyr )

    ds.Destroy()

    # Currently the injection of the EPSG DB as proj.4 strings adds 3945 entries
    if nb_srs < 3945:
        gdaltest.post_reason('did not get expected SRS count')
        print(nb_srs)
        return 'fail'

    return 'success'

###############################################################################
# Test CopyLayer() from a table layer (#3617)

def ogr_sqlite_21():

    if gdaltest.sl_ds is None:
        return 'skip'

    src_lyr = gdaltest.sl_ds.GetLayerByName('tpoly')
    copy_lyr = gdaltest.sl_ds.CopyLayer(src_lyr, 'tpoly_2')
    
    src_lyr_count = src_lyr.GetFeatureCount()
    copy_lyr_count = copy_lyr.GetFeatureCount()
    if src_lyr_count != copy_lyr_count:
        gdaltest.post_reason('did not get same number of features')
        print(src_lyr_count)
        print(copy_lyr_count)
        return 'fail'

    return 'success'

###############################################################################
# Test CopyLayer() from a result layer (#3617)

def ogr_sqlite_22():

    if gdaltest.sl_ds is None:
        return 'skip'

    src_lyr = gdaltest.sl_ds.ExecuteSQL('select * from tpoly')
    copy_lyr = gdaltest.sl_ds.CopyLayer(src_lyr, 'tpoly_3')
    
    src_lyr_count = src_lyr.GetFeatureCount()
    copy_lyr_count = copy_lyr.GetFeatureCount()
    if src_lyr_count != copy_lyr_count:
        gdaltest.post_reason('did not get same number of features')
        print(src_lyr_count)
        print(copy_lyr_count)
        return 'fail'

    gdaltest.sl_ds.ReleaseResultSet(src_lyr)

    return 'success'

###############################################################################
# Test ignored fields works ok

def ogr_sqlite_23():

    if gdaltest.sl_ds is None:
        return 'skip'

    shp_layer = gdaltest.sl_ds.GetLayerByName('tpoly')
    shp_layer.SetIgnoredFields( ['AREA'] )

    feat = shp_layer.GetNextFeature()

    if feat.IsFieldSet( 'AREA' ):
        gdaltest.post_reason( 'got area despite request to ignore it.' )
        return 'fail'

    if feat.GetFieldAsInteger('EAS_ID') != 168:
        gdaltest.post_reason( 'missing or wrong eas_id' )
        return 'fail'

    wkt = 'POLYGON ((479819.84375 4765180.5,479690.1875 4765259.5,479647.0 4765369.5,479730.375 4765400.5,480039.03125 4765539.5,480035.34375 4765558.5,480159.78125 4765610.5,480202.28125 4765482.0,480365.0 4765015.5,480389.6875 4764950.0,480133.96875 4764856.5,480080.28125 4764979.5,480082.96875 4765049.5,480088.8125 4765139.5,480059.90625 4765239.5,480019.71875 4765319.5,479980.21875 4765409.5,479909.875 4765370.0,479859.875 4765270.0,479819.84375 4765180.5))'
    if ogrtest.check_feature_geometry(feat, wkt,
                                      max_error = 0.00000001 ) != 0:
        return 'fail'

    fd = shp_layer.GetLayerDefn()
    fld = fd.GetFieldDefn(0) # area
    if not fld.IsIgnored():
        gdaltest.post_reason( 'AREA unexpectedly not marked as ignored.' )
        return 'fail'

    fld = fd.GetFieldDefn(1) # eas_id
    if fld.IsIgnored():
        gdaltest.post_reason( 'EASI unexpectedly marked as ignored.' )
        return 'fail'

    if fd.IsGeometryIgnored():
        gdaltest.post_reason( 'geometry unexpectedly ignored.' )
        return 'fail'

    if fd.IsStyleIgnored():
        gdaltest.post_reason( 'style unexpectedly ignored.' )
        return 'fail'

    fd.SetGeometryIgnored( 1 )

    if not fd.IsGeometryIgnored():
        gdaltest.post_reason( 'geometry unexpectedly not ignored.' )
        return 'fail'

    feat = shp_layer.GetNextFeature()

    if feat.GetGeometryRef() != None:
        gdaltest.post_reason( 'Unexpectedly got a geometry on feature 2.' )
        return 'fail'

    if feat.IsFieldSet( 'AREA' ):
        gdaltest.post_reason( 'got area despite request to ignore it.' )
        return 'fail'

    if feat.GetFieldAsInteger('EAS_ID') != 179:
        gdaltest.post_reason( 'missing or wrong eas_id' )
        return 'fail'

    return 'success'

###############################################################################
# Test that ExecuteSQL() with OGRSQL dialect doesn't forward the where clause to sqlite (#4022)

def ogr_sqlite_24():

    if gdaltest.sl_ds is None:
        return 'skip'

    try:
        os.remove('tmp/test24.sqlite')
    except:
        pass
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/test24.sqlite')
    lyr = ds.CreateLayer('test')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 1)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(2 3)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((4 5,6 7))'))
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open('tmp/test24.sqlite')

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.ExecuteSQL('select OGR_GEOMETRY from test')
    gdal.PopErrorHandler()
    if lyr is not None:
        gdaltest.post_reason('this should not work (1)')
        ds.ReleaseResultSet(lyr)
        return 'fail'

    lyr = ds.ExecuteSQL('select * from test')
    lyr.SetAttributeFilter("OGR_GEOMETRY = 'POLYGON'")
    feat = lyr.GetNextFeature()
    ds.ReleaseResultSet(lyr)
    if feat is None:
        gdaltest.post_reason('a feature was expected (2)')
        return 'fail'

    lyr = ds.GetLayerByName('test')
    lyr.SetAttributeFilter("OGR_GEOMETRY = 'POLYGON'")
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    feat = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    if feat is not None:
        gdaltest.post_reason('a feature was not expected (3)')
        return 'fail'

    lyr = ds.ExecuteSQL('select OGR_GEOMETRY from test', dialect = 'OGRSQL')
    lyr.SetAttributeFilter("OGR_GEOMETRY = 'POLYGON'")
    feat = lyr.GetNextFeature()
    ds.ReleaseResultSet(lyr)
    if feat is None:
        gdaltest.post_reason('a feature was expected (4)')
        return 'fail'

    lyr = ds.ExecuteSQL("select OGR_GEOMETRY from test WHERE OGR_GEOMETRY = 'POLYGON'", dialect = 'OGRSQL')
    feat = lyr.GetNextFeature()
    ds.ReleaseResultSet(lyr)
    if feat is None:
        gdaltest.post_reason('a feature was expected (5)')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test opening a /vsicurl/ DB

def ogr_sqlite_25():

    if gdaltest.sl_ds is None:
        return 'skip'

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    sql_lyr = gdaltest.sl_ds.ExecuteSQL("SELECT sqlite_version()")
    feat = sql_lyr.GetNextFeature()
    print('SQLite version : %s' % feat.GetFieldAsString(0))
    feat = None
    gdaltest.sl_ds.ReleaseResultSet(sql_lyr)

    # Check that we have SQLite VFS support
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_sqlite_25.db')
    gdal.PopErrorHandler()
    if ds is None:
        return 'skip'
    ds = None
    gdal.Unlink('/vsimem/ogr_sqlite_25.db')

    ds = ogr.Open('/vsicurl/http://download.osgeo.org/gdal/data/sqlite3/polygon.db')
    if ds is None:
        if gdaltest.gdalurlopen('http://download.osgeo.org/gdal/data/sqlite3/polygon.db') is None:
            print('cannot open URL')
            return 'skip'
        return 'fail'

    return 'success'

###############################################################################
# Test creating a :memory: DB

def ogr_sqlite_26():

    if gdaltest.sl_ds is None:
        return 'skip'

    ds = ogr.GetDriverByName('SQLite').CreateDataSource(':memory:')
    sql_lyr = ds.ExecuteSQL('select count(*) from geometry_columns')
    if sql_lyr is None:
        gdaltest.post_reason('expected existing geometry_columns')
        return 'fail'

    count = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    if count != 1:
        gdaltest.post_reason('expected existing geometry_columns')
        return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_sqlite_27():

    if gdaltest.sl_ds is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f SQLite tmp/ogr_sqlite_27.sqlite data/poly.shp')

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/ogr_sqlite_27.sqlite')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test if SpatiaLite is available

def ogr_spatialite_1():

    if gdaltest.sl_ds is None:
        return 'skip'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = gdaltest.sl_ds.ExecuteSQL("SELECT spatialite_version()")
    gdal.PopErrorHandler()
    if sql_lyr is None:
        res = 'skip'
    else:
        feat = sql_lyr.GetNextFeature()
        print('Spatialite : %s' % feat.GetFieldAsString(0))
        gdaltest.spatialite_version = feat.GetFieldAsString(0)
        gdaltest.has_spatialite = True
        gdaltest.sl_ds.ReleaseResultSet(sql_lyr)
        res = 'success'

    return res

###############################################################################
# Test spatial filter when SpatiaLite is available

def ogr_spatialite_2():

    if gdaltest.has_spatialite == False:
        return 'skip'

    ds = ogr.Open( 'tmp/spatialite_test.db', update = 1 )
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:4326')
    lyr = ds.CreateLayer( 'test_spatialfilter', srs = srs)
    
    lyr.StartTransaction()

    for i in range(10):
        for j in range(10):
            geom = ogr.CreateGeometryFromWkt( 'POINT(%d %d)' % (i, j))
            dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
            dst_feat.SetGeometry( geom )
            lyr.CreateFeature( dst_feat )
            dst_feat.Destroy()

    geom = ogr.CreateGeometryFromWkt( 'POLYGON((0 0,0 3,3 3,3 0,0 0))' )
    dst_feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    dst_feat.SetGeometry( geom )
    lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    lyr.CommitTransaction()

    ds.Destroy()

    # Test OLCFastFeatureCount with spatial index (created by default)
    ds = ogr.Open( 'tmp/spatialite_test.db', update = 1 )
    lyr = ds.GetLayerByName('test_spatialfilter')

    geom = ogr.CreateGeometryFromWkt( \
        'POLYGON((2 2,2 8,8 8,8 2,2 2))' )
    lyr.SetSpatialFilter( geom )
    geom.Destroy()

    if lyr.TestCapability(ogr.OLCFastFeatureCount) != True:
        return 'fail'
    if lyr.TestCapability(ogr.OLCFastSpatialFilter) != True:
        return 'fail'

    if lyr.GetFeatureCount() != 50:
        gdaltest.post_reason('did not get expected feature count(1)')
        print(lyr.GetFeatureCount())
        return 'fail'

    # Remove spatial index
    sql_lyr = ds.ExecuteSQL("SELECT DisableSpatialIndex('test_spatialfilter', 'Geometry')")
    sql_lyr.GetFeatureCount()
    feat = sql_lyr.GetNextFeature()
    ret = feat.GetFieldAsInteger(0)
    ds.ReleaseResultSet(sql_lyr)

    if ret != 1:
        gdaltest.post_reason('DisableSpatialIndex failed')
        return 'fail'

    ds.ExecuteSQL("VACUUM")

    ds.Destroy()

    # Test OLCFastFeatureCount without spatial index
    ds = ogr.Open( 'tmp/spatialite_test.db' )
    lyr = ds.GetLayerByName('test_spatialfilter')

    geom = ogr.CreateGeometryFromWkt( \
        'POLYGON((2 2,2 8,8 8,8 2,2 2))' )
    lyr.SetSpatialFilter( geom )
    geom.Destroy()

    if lyr.TestCapability(ogr.OLCFastFeatureCount) != False:
        return 'fail'
    if lyr.TestCapability(ogr.OLCFastSpatialFilter) != False:
        return 'fail'

    if lyr.GetFeatureCount() != 50:
        print(lyr.GetFeatureCount())
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Test VirtualShape feature of SpatiaLite

def ogr_spatialite_3():

    if gdaltest.has_spatialite == False:
        return 'skip'

    ds = ogr.Open( 'tmp/spatialite_test.db', update = 1 )
    ds.ExecuteSQL( 'CREATE VIRTUAL TABLE testpoly USING VirtualShape(data/testpoly, CP1252, -1)')
    ds.Destroy()

    ds = ogr.Open( 'tmp/spatialite_test.db'  )
    lyr = ds.GetLayerByName('testpoly')
    if lyr is None:
        return 'fail'

    lyr.SetSpatialFilterRect( -400, 22, -120, 400 )

    tr = ogrtest.check_features_against_list( lyr, 'FID',
                                              [ 0, 4, 8 ] )

    ds.Destroy()

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test updating a spatialite DB (#3471 and #3474)

def ogr_spatialite_4():

    if gdaltest.has_spatialite == False:
        return 'skip'

    ds = ogr.Open( 'tmp/spatialite_test.db', update = 1  )

    lyr = ds.ExecuteSQL('SELECT * FROM sqlite_master')
    nb_sqlite_master_objects_before = lyr.GetFeatureCount()
    ds.ReleaseResultSet(lyr)

    lyr = ds.ExecuteSQL('SELECT * FROM idx_geomspatialite_GEOMETRY')
    nb_idx_before = lyr.GetFeatureCount()
    ds.ReleaseResultSet(lyr)

    lyr = ds.GetLayerByName('geomspatialite')
    lyr.CreateField( ogr.FieldDefn("foo", ogr.OFTString) )

    lyr = ds.ExecuteSQL('SELECT * FROM geomspatialite')
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom is None or geom.ExportToWkt() != 'POINT (0 1)':
        print(geom)
        return 'fail'
    feat.Destroy()
    ds.ReleaseResultSet(lyr)

    # Check that triggers and index are restored (#3474)
    lyr = ds.ExecuteSQL('SELECT * FROM sqlite_master')
    nb_sqlite_master_objects_after = lyr.GetFeatureCount()
    ds.ReleaseResultSet(lyr)

    if nb_sqlite_master_objects_before != nb_sqlite_master_objects_after:
        print('nb_sqlite_master_objects_before=%d, nb_sqlite_master_objects_after=%d' % (nb_sqlite_master_objects_before, nb_sqlite_master_objects_after))
        return 'fail'

    # Add new feature
    lyr = ds.GetLayerByName('geomspatialite')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(100 -100)'))
    lyr.CreateFeature(feat)
    feat.Destroy()

    # Check that the trigger is functionnal (#3474)
    lyr = ds.ExecuteSQL('SELECT * FROM idx_geomspatialite_GEOMETRY')
    nb_idx_after = lyr.GetFeatureCount()
    ds.ReleaseResultSet(lyr)

    if nb_idx_before + 1 != nb_idx_after:
        print('nb_idx_before=%d, nb_idx_after=%d' % (nb_idx_before, nb_idx_after))
        return 'fail'

    return 'success'

###############################################################################
# Test writing and reading back spatialite geometries (#4092)

def ogr_spatialite_5(bUseComprGeom = False):

    if gdaltest.has_spatialite == False:
        return 'skip'

    try:
        os.remove('tmp/ogr_spatialite_5.sqlite')
    except:
        pass
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_spatialite_5.sqlite', options = ['SPATIALITE=YES'] )

    geometries = [
        #'POINT EMPTY',
        'POINT (1 2)',
        'POINT (1 2 3)',
        'LINESTRING EMPTY',
        'LINESTRING (1 2)',
        'LINESTRING (1 2,3 4)',
        'LINESTRING (1 2,3 4,5 6)',
        'LINESTRING (1 2 3,4 5 6)',
        'LINESTRING (1 2 3,4 5 6,7 8 9)',
        'POLYGON EMPTY',
        'POLYGON ((1 2,1 3,2 3,2 2,1 2))',
        'POLYGON ((1 2 10,1 3 -10,2 3 20,2 2 -20,1 2 10))',
        'POLYGON ((1 2,1 3,2 3,2 2,1 2),(1.25 2.25,1.25 2.75,1.75 2.75,1.75 2.25,1.25 2.25))',
        'POLYGON ((1 2 10,1 3 -10,2 3 20,2 2 -20,1 2 10))',
        'MULTIPOINT EMPTY',
        'MULTIPOINT (1 2,3 4)',
        'MULTIPOINT (1 2 3,4 5 6)',
        'MULTILINESTRING EMPTY',
        'MULTILINESTRING ((1 2,3 4),(5 6,7 8))',
        'MULTILINESTRING ((1 2 3,4 5 6),(7 8 9,10 11 12))',
        'MULTIPOLYGON EMPTY',
        'MULTIPOLYGON (((1 2,1 3,2 3,2 2,1 2)),((-1 -2,-1 -3,-2 -3,-2 -2,-1 -2)))',
        'MULTIPOLYGON (((1 2,1 3,2 3,2 2,1 2),(1.25 2.25,1.25 2.75,1.75 2.75,1.75 2.25,1.25 2.25)),((-1 -2,-1 -3,-2 -3,-2 -2,-1 -2)))',
        'MULTIPOLYGON (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2 0,-1 -3 0,-2 -3 0,-2 -2 0,-1 -2 0)))',
        'GEOMETRYCOLLECTION EMPTY',
        #'GEOMETRYCOLLECTION (GEOMETRYCOLLECTION EMPTY)',
        'GEOMETRYCOLLECTION (POINT (1 2))',
        'GEOMETRYCOLLECTION (POINT (1 2 3))',
        'GEOMETRYCOLLECTION (LINESTRING (1 2,3 4))',
        'GEOMETRYCOLLECTION (LINESTRING (1 2 3,4 5 6))',
        'GEOMETRYCOLLECTION (POLYGON ((1 2,1 3,2 3,2 2,1 2)))',
        'GEOMETRYCOLLECTION (POLYGON ((1 2 10,1 3 -10,2 3 20,2 2 -20,1 2 10)))',
        'GEOMETRYCOLLECTION (POINT (1 2),LINESTRING (1 2,3 4),POLYGON ((1 2,1 3,2 3,2 2,1 2)))',
        'GEOMETRYCOLLECTION (POINT (1 2 3),LINESTRING (1 2 3,4 5 6),POLYGON ((1 2 10,1 3 -10,2 3 20,2 2 -20,1 2 10)))',
    ]

    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4326 )

    num_layer = 0
    for wkt in geometries:
        geom = ogr.CreateGeometryFromWkt(wkt)
        if gdaltest.spatialite_version == '2.3.1' and (geom.GetGeometryType() & ogr.wkb25DBit) != 0:
            continue
        if bUseComprGeom:
            options = ['COMPRESS_GEOM=YES']
        else:
            options = []
        lyr = ds.CreateLayer('test%d' % num_layer, geom_type = geom.GetGeometryType(), srs = srs, options = options)
        feat = ogr.Feature(lyr.GetLayerDefn())
        #print(geom)
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)
        num_layer = num_layer + 1

    ds = None

    ds = ogr.Open('tmp/ogr_spatialite_5.sqlite')
    num_layer = 0
    for wkt in geometries:
        geom = ogr.CreateGeometryFromWkt(wkt)
        if gdaltest.spatialite_version == '2.3.1' and (geom.GetGeometryType() & ogr.wkb25DBit) != 0:
            continue
        lyr = ds.GetLayer(num_layer)
        feat = lyr.GetNextFeature()
        got_wkt = feat.GetGeometryRef().ExportToWkt()
        if got_wkt != wkt:
            gdaltest.post_reason('got %s, expected %s' % (got_wkt, wkt))
            return 'fail'

        num_layer = num_layer + 1

    if bUseComprGeom:
        num_layer = 0
        for wkt in geometries:
            if wkt.find('EMPTY') == -1 and wkt.find('POINT') == -1:
                sql_lyr = ds.ExecuteSQL("SELECT GEOMETRY == CompressGeometry(GEOMETRY) FROM test%d" % num_layer)
                feat = sql_lyr.GetNextFeature()
                val = feat.GetFieldAsInteger(0)
                if wkt != 'LINESTRING (1 2)':
                    if val != 1:
                        gdaltest.post_reason('did not get expected compressed geometry')
                        print(wkt)
                        print(val)
                        ds.ReleaseResultSet(sql_lyr)
                        return 'fail'
                else:
                    if val != 0:
                        print(wkt)
                        print(val)
                        ds.ReleaseResultSet(sql_lyr)
                        return 'fail'
                feat = None
                ds.ReleaseResultSet(sql_lyr)
            num_layer = num_layer + 1

    ds = None

    return 'success'


###############################################################################
# Test writing and reading back spatialite geometries in compressed form

def ogr_spatialite_compressed_geom_5():

    if gdaltest.has_spatialite == False:
        return 'skip'

    if gdaltest.spatialite_version == '2.3.1':
        return 'skip'

    return ogr_spatialite_5(bUseComprGeom = True)

###############################################################################
# Test spatialite spatial views

def ogr_spatialite_6():

    if gdaltest.has_spatialite == False:
        return 'skip'

    if gdaltest.spatialite_version.find('2.3') == 0:
        return 'skip'

    try:
        os.remove('tmp/ogr_spatialite_6.sqlite')
    except:
        pass
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_spatialite_6.sqlite', options = ['SPATIALITE=YES'])

    # Create regular layer
    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4326 )
    lyr = ds.CreateLayer('regular_layer', geom_type = ogr.wkbPoint, srs = srs)
    lyr.CreateField(ogr.FieldDefn("intcol", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("realcol", ogr.OFTReal))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 12)
    feat.SetField(1, 34.56)
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    feat.SetGeometryDirectly(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 12)
    feat.SetField(1, 34.56)
    geom = ogr.CreateGeometryFromWkt('POINT(3 50)')
    feat.SetGeometryDirectly(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 34)
    feat.SetField(1, 56.78)
    geom = ogr.CreateGeometryFromWkt('POINT(-30000 -50000)')
    feat.SetGeometryDirectly(geom)
    lyr.CreateFeature(feat)
    geom = ogr.CreateGeometryFromWkt('POINT(3 50)')
    feat.SetGeometryDirectly(geom)
    lyr.SetFeature(feat)

    # Create spatial view
    ds.ExecuteSQL('CREATE VIEW view_of_regular_layer AS SELECT OGC_FID AS pk_id, GEOMETRY AS the_geom, intcol, realcol FROM regular_layer')
    ds.ExecuteSQL("INSERT INTO views_geometry_columns(view_name, view_geometry, view_rowid, f_table_name, f_geometry_column) VALUES " + \
                  "('view_of_regular_layer', 'the_geom', 'pk_id', 'regular_layer', 'GEOMETRY')")

    ds = None

    # Test spatial view
    ds = ogr.Open('tmp/ogr_spatialite_6.sqlite')
    lyr = ds.GetLayerByName('regular_layer')
    view_lyr = ds.GetLayerByName('view_of_regular_layer')
    if view_lyr.GetGeomType() != lyr.GetGeomType():
        gdaltest.post_reason('failed')
        return 'fail'
    if view_lyr.GetFeatureCount() != lyr.GetFeatureCount():
        gdaltest.post_reason('failed')
        return 'fail'
    if view_lyr.GetSpatialRef().IsSame(lyr.GetSpatialRef()) != 1:
        gdaltest.post_reason('failed')
        return 'fail'
    feat = view_lyr.GetFeature(3)
    if feat.GetFieldAsDouble(1) != 56.78:
        gdaltest.post_reason('failed')
        feat.DumpReadable()
        return 'fail'
    view_lyr.SetAttributeFilter('intcol = 34')
    view_lyr.SetSpatialFilterRect(2.5,49.5,3.5,50.5)
    feat = view_lyr.GetNextFeature()
    if feat.GetFID() != 3:
        gdaltest.post_reason('failed')
        feat.DumpReadable()
        return 'fail'
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
        gdaltest.post_reason('failed')
        feat.DumpReadable()
        return 'fail'
    ds = None

    # Remove spatial index
    ds = ogr.Open('tmp/ogr_spatialite_6.sqlite', update = 1)
    sql_lyr = ds.ExecuteSQL("SELECT DisableSpatialIndex('regular_layer', 'GEOMETRY')")
    ds.ReleaseResultSet(sql_lyr)
    ds.ExecuteSQL("DROP TABLE idx_regular_layer_GEOMETRY")
    ds = None

    # Test spatial view again
    ds = ogr.Open('tmp/ogr_spatialite_6.sqlite')
    view_lyr = ds.GetLayerByName('view_of_regular_layer')
    view_lyr.SetAttributeFilter('intcol = 34')
    view_lyr.SetSpatialFilterRect(2.5,49.5,3.5,50.5)
    feat = view_lyr.GetNextFeature()
    if feat.GetFID() != 3:
        gdaltest.post_reason('failed')
        feat.DumpReadable()
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test VirtualShape:xxx.shp

def ogr_spatialite_7():

    if gdaltest.has_spatialite == False:
        return 'skip'

    ds = ogr.Open('VirtualShape:data/poly.shp')
    if ds is None:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr = ds.GetLayerByName('poly')
    if lyr is None:
        gdaltest.post_reason('failed')
        return 'fail'

    return 'success'

###############################################################################
# 

def ogr_sqlite_cleanup():

    if gdaltest.sl_ds is None:
        return 'skip'

    gdaltest.sl_ds.ExecuteSQL( 'DELLAYER:tpoly' )
    gdaltest.sl_ds.ExecuteSQL( 'DELLAYER:tpoly_2' )
    gdaltest.sl_ds.ExecuteSQL( 'DELLAYER:tpoly_3' )
    gdaltest.sl_ds.ExecuteSQL( 'DELLAYER:geomwkb' )
    gdaltest.sl_ds.ExecuteSQL( 'DELLAYER:geomwkt' )
    gdaltest.sl_ds.ExecuteSQL( 'DELLAYER:geomspatialite' )
    gdaltest.sl_ds.ExecuteSQL( 'DELLAYER:wgs84layer' )
    gdaltest.sl_ds.ExecuteSQL( 'DELLAYER:wgs84layer_approx' )
    gdaltest.sl_ds.ExecuteSQL( 'DELLAYER:testtypes' )
    gdaltest.sl_ds.ExecuteSQL( 'DELLAYER:fgf_table' )

    gdaltest.sl_ds.Destroy()
    gdaltest.sl_ds = None

    try:
        os.remove( 'tmp/sqlite_test.db' )
    except:
        pass

    try:
        os.remove( 'tmp/spatialite_test.db' )
    except:
        pass

    try:
        os.remove( 'tmp/spatialite_test_with_epsg.db' )
    except:
        pass

    try:
        os.remove( 'tmp/non_spatialite_test_with_epsg.db' )
    except:
        pass
    try:
        os.remove( 'tmp/test24.sqlite' )
    except:
        pass

    try:
        os.remove( 'tmp/ogr_spatialite_5.sqlite' )
    except:
        pass

    try:
        os.remove( 'tmp/ogr_spatialite_6.sqlite' )
    except:
        pass

    try:
        os.remove( 'tmp/ogr_sqlite_27.sqlite' )
    except:
        pass

    return 'success'

gdaltest_list = [ 
    ogr_sqlite_1,
    ogr_sqlite_2,
    ogr_sqlite_3,
    ogr_sqlite_4,
    ogr_sqlite_5,
    ogr_sqlite_6,
    ogr_sqlite_7,
    ogr_sqlite_8,
    ogr_sqlite_9,
    ogr_sqlite_10,
    ogr_sqlite_11,
    ogr_sqlite_12,
    ogr_sqlite_13,
    ogr_sqlite_14,
    ogr_sqlite_15,
    ogr_sqlite_16,
    ogr_sqlite_20,
    ogr_sqlite_21,
    ogr_sqlite_22,
    ogr_sqlite_23,
    ogr_sqlite_24,
    ogr_sqlite_25,
    ogr_sqlite_26,
    ogr_sqlite_27,
    ogr_spatialite_1,
    ogr_sqlite_17,
    ogr_sqlite_18,
    ogr_sqlite_19,
    ogr_spatialite_2,
    ogr_spatialite_3,
    ogr_spatialite_4,
    ogr_spatialite_5,
    ogr_spatialite_compressed_geom_5,
    ogr_spatialite_6,
    ogr_spatialite_7,
    ogr_sqlite_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_sqlite' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

