#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  SQLite SQL dialect testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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

from osgeo import ogr
from osgeo import osr
from osgeo import gdal
import gdaltest
import ogrtest
import webserver

###############################################################################
# Tests that don't involve geometry

def ogr_sql_sqlite_1():

    ogrtest.has_sqlite_dialect = False
    if ogr.GetDriverByName('SQLite') is None:
        return 'skip'

    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
    sql_lyr = ds.ExecuteSQL( "SELECT * FROM sqlite_master", dialect = 'SQLite' )
    ds.ReleaseResultSet( sql_lyr )
    if sql_lyr is None:
        return 'skip'
    ogrtest.has_sqlite_dialect = True

    for geom in [ ogr.wkbNone, ogr.wkbUnknown ]:
        lyr = ds.CreateLayer( "my_layer", geom_type = geom)
        field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('doublefield', ogr.OFTReal)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('strfield', ogr.OFTString)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('binaryfield', ogr.OFTBinary)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('nullablefield', ogr.OFTInteger)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('datetimefield', ogr.OFTDateTime)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('datefield', ogr.OFTDate)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('timefield', ogr.OFTTime)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('from', ogr.OFTString)
        lyr.CreateField(field_defn)

        # Test INSERT
        sql_lyr = ds.ExecuteSQL( "INSERT INTO my_layer (intfield, nullablefield, doublefield, strfield, binaryfield, datetimefield, datefield, timefield, \"from\") VALUES (1,NULL,2.34,'foo',x'0001FF', '2012-08-23 21:24', '2012-08-23', '21:24', 'from_val')", dialect = 'SQLite' )
        ds.ReleaseResultSet( sql_lyr )

        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        if feat.GetField('intfield') != 1 or \
           feat.GetField('nullablefield') is not None or \
           feat.GetField('doublefield') != 2.34 or \
           feat.GetField('strfield') != 'foo' or \
           feat.GetField('binaryfield') != '0001FF' or \
           feat.GetField('datetimefield') != '2012/08/23 21:24:00' or \
           feat.GetField('datefield') != '2012/08/23' or \
           feat.GetField('timefield') != '21:24:00' or \
           feat.GetField('from') != 'from_val':
            gdaltest.post_reason('failure')
            feat.DumpReadable()
            return 'fail'
        feat = None

        # Test UPDATE
        sql_lyr = ds.ExecuteSQL( "UPDATE my_layer SET intfield = 2, doublefield = 3.45, strfield = 'bar', timefield = '12:34' WHERE ROWID = 0", dialect = 'SQLite' )
        ds.ReleaseResultSet( sql_lyr )

        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        if feat.GetField('intfield') != 2 or \
           feat.GetField('doublefield') != 3.45 or \
           feat.GetField('strfield') != 'bar' or \
           feat.GetField('datetimefield') != '2012/08/23 21:24:00' or \
           feat.GetField('datefield') != '2012/08/23' or \
           feat.GetField('timefield') != '12:34:00':
            gdaltest.post_reason('failure')
            feat.DumpReadable()
            return 'fail'

        feat.SetStyleString('cool_style')
        lyr.SetFeature(feat)

        feat = None

        # Test SELECT
        sql_lyr = ds.ExecuteSQL( "SELECT * FROM my_layer", dialect = 'SQLite' )
        feat = sql_lyr.GetNextFeature()
        if feat.GetField('intfield') != 2 or \
           feat.GetField('nullablefield') is not None or \
           feat.GetField('doublefield') != 3.45 or \
           feat.GetField('strfield') != 'bar' or \
           feat.GetField('datetimefield') != '2012/08/23 21:24:00' or \
           feat.GetField('datefield') != '2012/08/23' or \
           feat.GetField('timefield') != '12:34:00':
            gdaltest.post_reason('failure')
            feat.DumpReadable()
            return 'fail'
        feat = None
        ds.ReleaseResultSet( sql_lyr )

        # Test SELECT with OGR_STYLE
        sql_lyr = ds.ExecuteSQL( "SELECT *, OGR_STYLE FROM my_layer", dialect = 'SQLite' )
        feat = sql_lyr.GetNextFeature()
        if feat.GetField('intfield') != 2 or \
           feat.GetField('nullablefield') is not None or \
           feat.GetField('doublefield') != 3.45 or \
           feat.GetField('strfield') != 'bar' or \
           feat.GetStyleString() != 'cool_style' :
            gdaltest.post_reason('failure')
            feat.DumpReadable()
            return 'fail'
        feat = None
        ds.ReleaseResultSet( sql_lyr )
        
        # Test SELECT with filters

        # Success filters
        for cond in [ 'intfield = 2', 'intfield > 1', 'intfield >= 2', 'intfield < 3', 'intfield <= 2',
                        'doublefield = 3.45', 'doublefield > 3', 'doublefield >= 3.45', 'doublefield < 3.46', 'doublefield <= 3.45',
                        "strfield = 'bar'", "strfield > 'baq'", "strfield >= 'bar'", "strfield < 'bas'", "strfield <= 'bar'",
                        'nullablefield IS NULL',
                        "binaryfield = x'0001FF'",
                        "OGR_STYLE = 'cool_style'",
                        'intfield = 2 AND doublefield = 3.45',
                        'ROWID = 0',
                        "\"from\" = 'from_val'"]:
            sql_lyr = ds.ExecuteSQL( "SELECT * FROM my_layer WHERE " + cond, dialect = 'SQLite' )
            feat = sql_lyr.GetNextFeature()
            if feat is None:
                gdaltest.post_reason('failure')
                return 'fail'
            feat = None
            ds.ReleaseResultSet( sql_lyr )

        # Failed filters
        for cond in [ 'intfield = 0', 'intfield > 3', 'intfield >= 3', 'intfield < 0', 'intfield <= 0',
                        'doublefield = 0', 'doublefield > 3.46', 'doublefield >= 3.46', 'doublefield < 3.45', 'doublefield <= 0',
                        "strfield = 'XXX'", "strfield > 'bas'", "strfield >= 'bas'", "strfield < 'bar'", "strfield <= 'baq'",
                        'intfield = 2 AND doublefield = 0',
                        'ROWID = 10000',
                        "\"from\" = 'other_val'"]:
            sql_lyr = ds.ExecuteSQL( "SELECT * FROM my_layer WHERE " + cond, dialect = 'SQLite' )
            feat = sql_lyr.GetNextFeature()
            if feat is not None:
                gdaltest.post_reason('failure')
                return 'fail'
            feat = None
            ds.ReleaseResultSet( sql_lyr )

        if geom != ogr.wkbNone:
            # Test a filter on geometry, to check that we won't try to optimize that
            sql_lyr = ds.ExecuteSQL( "SELECT * FROM my_layer WHERE GEOMETRY = x'00'", dialect = 'SQLite' )
            ds.ReleaseResultSet( sql_lyr )


        # Test INSERT with specified ROWID/FID
        sql_lyr = ds.ExecuteSQL( "INSERT INTO my_layer (intfield, ROWID) VALUES (100, 1000)", dialect = 'SQLite' )
        ds.ReleaseResultSet( sql_lyr )

        feat = lyr.GetFeature(1000)
        if feat.GetField('intfield') != 100:
            gdaltest.post_reason('failure')
            feat.DumpReadable()
            return 'fail'
        feat = None

        # Test DELETE
        sql_lyr = ds.ExecuteSQL( "DELETE FROM my_layer WHERE intfield = 2", dialect = 'SQLite' )
        ds.ReleaseResultSet( sql_lyr )
        sql_lyr = ds.ExecuteSQL( "DELETE FROM my_layer WHERE ROWID = 1000", dialect = 'SQLite' )
        ds.ReleaseResultSet( sql_lyr )

        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        if feat is not None:
            gdaltest.post_reason('failure')
            feat.DumpReadable()
            return 'fail'
        feat = None


        ds.DeleteLayer(0)

    ds = None

    return 'success'

###############################################################################
# Tests that involve geometry  (but without needing Spatialite)

def ogr_sql_sqlite_2():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    lyr = ds.CreateLayer( "my_layer", srs = srs )
    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('doublefield', ogr.OFTReal)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('strfield', ogr.OFTString)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('intfield', 1)
    feat.SetField('doublefield', 2.34)
    feat.SetField('strfield', 'foo')
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 1)'))
    feat.SetStyleString('cool_style')
    lyr.CreateFeature(feat)
    feat = None

    # Test UPDATE
    sql_lyr = ds.ExecuteSQL( "UPDATE my_layer SET intfield = 2, doublefield = 3.45, strfield = 'bar' WHERE ROWID = 0", dialect = 'SQLite' )
    ds.ReleaseResultSet( sql_lyr )

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetField('intfield') != 2 or \
       feat.GetField('doublefield') != 3.45 or \
       feat.GetField('strfield') != 'bar' or \
       feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
        gdaltest.post_reason('failure')
        feat.DumpReadable()
        return 'fail'
    feat = None

    # Test SELECT
    sql_lyr = ds.ExecuteSQL( "SELECT * FROM my_layer", dialect = 'SQLite' )
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('intfield') != 2 or \
       feat.GetField('doublefield') != 3.45 or \
       feat.GetField('strfield') != 'bar' or \
       feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
        gdaltest.post_reason('failure')
        feat.DumpReadable()
        return 'fail'
    got_srs = feat.GetGeometryRef().GetSpatialReference()
    if got_srs is None or srs.IsSame(got_srs) == 0:
        gdaltest.post_reason('failure')
        print(srs)
        print(got_srs)
        return 'fail'
    feat = None
    ds.ReleaseResultSet( sql_lyr )

    # Test SELECT with OGR_STYLE
    sql_lyr = ds.ExecuteSQL( "SELECT *, OGR_STYLE FROM my_layer", dialect = 'SQLite' )
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('intfield') != 2 or \
       feat.GetField('doublefield') != 3.45 or \
       feat.GetField('strfield') != 'bar' or \
       feat.GetStyleString() != 'cool_style' or \
       feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
        gdaltest.post_reason('failure')
        feat.DumpReadable()
        return 'fail'
    got_srs = feat.GetGeometryRef().GetSpatialReference()
    if got_srs is None or srs.IsSame(got_srs) == 0:
        gdaltest.post_reason('failure')
        print(srs)
        print(got_srs)
        return 'fail'
    feat = None
    ds.ReleaseResultSet( sql_lyr )

    # Test with a custom SRS
    srs = osr.SpatialReference()
    srs.SetFromUserInput("""LOCAL_CS["foo"]""")
    lyr = ds.CreateLayer( "my_layer2", srs = srs )

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 1)'))
    lyr.CreateFeature(feat)
    feat = None

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 1)'))
    lyr.CreateFeature(feat)
    feat = None

    # Test SELECT
    sql_lyr = ds.ExecuteSQL( "SELECT * FROM my_layer2", dialect = 'SQLite' )

    layer_srs = sql_lyr.GetSpatialRef()
    if layer_srs is None or srs.IsSame(layer_srs) == 0:
        gdaltest.post_reason('failure')
        print(srs)
        print(layer_srs)
        return 'fail'

    for i in range(2):
        feat = sql_lyr.GetNextFeature()
        if  feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
            gdaltest.post_reason('failure')
            feat.DumpReadable()
            return 'fail'
        got_srs = feat.GetGeometryRef().GetSpatialReference()
        if got_srs is None or srs.IsSame(got_srs) == 0:
            gdaltest.post_reason('failure')
            print(srs)
            print(got_srs)
            return 'fail'
        feat = None

    ds.ReleaseResultSet( sql_lyr )

    return 'success'

###############################################################################
# Test that involves a join

def ogr_sql_sqlite_3():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    ds = ogr.Open('data')

    sql_lyr = ds.ExecuteSQL( "SELECT p.*, idlink.* FROM poly p LEFT JOIN idlink USING (EAS_ID) ORDER BY EAS_ID", dialect = 'SQLite' )
    count = sql_lyr.GetFeatureCount()
    sql_lyr.ResetReading()
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('NAME') != '_158_':
        feat.DumpReadable()
        return 'fail'
    if ogrtest.check_feature_geometry(feat, "POLYGON ((480701.0625 4764738.0,480761.46875 4764778.0,480824.96875 4764820.0,480922.03125 4764850.5,480930.71875 4764852.0,480984.25 4764875.0,481088.1875 4764936.0,481136.84375 4764994.5,481281.3125 4764876.5,481291.09375 4764810.0,481465.90625 4764872.5,481457.375 4764937.0,481509.65625 4764967.0,481538.90625 4764982.5,481575.0 4764999.5,481602.125 4764915.5,481629.84375 4764829.5,481645.3125 4764797.5,481635.96875 4764795.5,481235.3125 4764650.0,481209.8125 4764633.5,481199.21875 4764623.5,481185.5 4764607.0,481159.9375 4764580.0,481140.46875 4764510.5,481141.625 4764480.5,481199.84375 4764180.0,481143.4375 4764010.5,481130.3125 4763979.5,481039.9375 4763889.5,480882.6875 4763670.0,480826.0625 4763650.5,480745.1875 4763628.5,480654.4375 4763627.5,480599.8125 4763660.0,480281.9375 4763576.5,480221.5 4763533.5,480199.6875 4763509.0,480195.09375 4763430.0,480273.6875 4763305.5,480309.6875 4763063.5,480201.84375 4762962.5,479855.3125 4762880.5,479848.53125 4762897.0,479728.875 4763217.5,479492.6875 4763850.0,479550.0625 4763919.5,480120.21875 4764188.5,480192.3125 4764183.0,480358.53125 4764277.0,480500.40625 4764326.5,480584.375 4764353.0,480655.34375 4764397.5,480677.84375 4764439.0,480692.1875 4764453.5,480731.65625 4764561.5,480515.125 4764695.0,480540.71875 4764741.0,480588.59375 4764740.5,480710.25 4764690.5,480701.0625 4764738.0))") != 0:
        feat.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet( sql_lyr )

    if count != 10:
        print(count)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test that involves a self-join (to check that we can open twice the same table)

def ogr_sql_sqlite_4():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    ds = ogr.Open('data')

    sql_lyr = ds.ExecuteSQL( "SELECT p.* FROM poly p JOIN poly USING (EAS_ID)", dialect = 'SQLite' )
    count = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet( sql_lyr )

    if count != 10:
        print(count)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test that involves spatialite

def ogr_sql_sqlite_5():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/foo.db', options = ['SPATIALITE=YES'])
    ogrtest.has_spatialite = ds is not None
    if ogrtest.has_spatialite:
        sql_lyr = ds.ExecuteSQL("SELECT spatialite_version()")
        feat = sql_lyr.GetNextFeature()
        gdaltest.spatialite_version = feat.GetFieldAsString(0)
        ds.ReleaseResultSet(sql_lyr)
    ds = None
    gdal.Unlink('/vsimem/foo.db')
    gdal.PopErrorHandler()

    if ogrtest.has_spatialite is False:
        return 'skip'

    ds = ogr.Open('data')

    sql_lyr = ds.ExecuteSQL( "SELECT MAX(ST_Length(GEOMETRY)) FROM POLY", dialect = 'SQLite' )
    count = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet( sql_lyr )

    ds = None

    if count != 1:
        print(count)
        return 'fail'

    return 'success'

###############################################################################
# If Spatialite available, retry some tests without it, to check that
# we are fully compatible with regular SQLite

def ogr_sql_sqlite_6():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    if ogrtest.has_spatialite is False:
        return 'skip'

    gdal.SetConfigOption('OGR_SQLITE_DIALECT_USE_SPATIALITE', 'NO')

    ret = ogr_sql_sqlite_1()
    if ret != 'success':
        return ret

    ret = ogr_sql_sqlite_2()
    if ret != 'success':
        return ret

    ret = ogr_sql_sqlite_4()
    if ret != 'success':
        return ret

    gdal.SetConfigOption('OGR_SQLITE_DIALECT_USE_SPATIALITE', None)

    return 'success'

###############################################################################
# Test if there's a text column called GEOMETRY already in the table

def ogr_sql_sqlite_7():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")


    lyr = ds.CreateLayer( "my_layer" )
    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('geometry', ogr.OFTString)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('intfield', 1)
    feat.SetField('geometry', 'BLA')
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 1)'))
    lyr.CreateFeature(feat)
    feat = None

    # Test SELECT
    sql_lyr = ds.ExecuteSQL( "SELECT * FROM my_layer", dialect = 'SQLite' )

    if sql_lyr.GetGeometryColumn() != 'GEOMETRY2':
        gdaltest.post_reason('failure')
        return 'fail'

    feat = sql_lyr.GetNextFeature()
    if feat.GetField('intfield') != 1 or \
       feat.GetField('geometry') != 'BLA' or \
       feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
        gdaltest.post_reason('failure')
        feat.DumpReadable()
        return 'fail'
    feat = None
    ds.ReleaseResultSet( sql_lyr )

    # Test SELECT
    sql_lyr = ds.ExecuteSQL( "SELECT GEOMETRY2 FROM my_layer", dialect = 'SQLite' )
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
        gdaltest.post_reason('failure')
        feat.DumpReadable()
        return 'fail'
    feat = None
    ds.ReleaseResultSet( sql_lyr )

    return 'success'

###############################################################################
# Test join with an external datasource

def ogr_sql_sqlite_8():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    ds = ogr.Open('data')

    expect = [ 171, 172, 173, 179 ]

    sql_lyr = ds.ExecuteSQL(   \
        'SELECT p.*, il.name FROM poly p ' \
        + 'LEFT JOIN "data/idlink.dbf".idlink il USING (eas_id) ' \
        + 'WHERE eas_id > 170 ORDER BY eas_id', dialect = 'SQLite' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'eas_id', expect )

    ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Check parsing of sub-selects

def ogr_sql_sqlite_9():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    ds = ogr.Open('data')

    sql_lyr = ds.ExecuteSQL( "SELECT count(*) as cnt FROM (SELECT * FROM (SELECT * FROM\n'data'.poly my_alias))p,(SELECT * FROM 'data'.idlink) il WHERE p.EAS_ID = il.EAS_id", dialect = 'SQLite' )

    feat = sql_lyr.GetNextFeature()
    cnt = feat.GetField('cnt')
    feat = None

    ds.ReleaseResultSet( sql_lyr )

    if cnt != 7:
        return' fail'

    return 'success'

###############################################################################
# Test optimized count(*)

def ogr_sql_sqlite_10():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    ds = ogr.Open('data')

    sql_lyr = ds.ExecuteSQL( "SELECT count(*) as cnt FROM poly", dialect = 'SQLite' )

    feat = sql_lyr.GetNextFeature()
    cnt = feat.GetField('cnt')
    feat = None

    ds.ReleaseResultSet( sql_lyr )

    if cnt != 10:
        return' fail'

    return 'success'

###############################################################################
# Test correct parsing of litterals

def ogr_sql_sqlite_11():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")

    lyr = ds.CreateLayer( "my_layer" )
    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('intfield', 1)
    lyr.CreateFeature(feat)
    feat = None

    sql_lyr = ds.ExecuteSQL( "SELECT 'a' FROM \"my_layer\"", dialect = 'SQLite' )
    cnt = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet( sql_lyr )

    ds = None

    if cnt != 1:
        return' fail'

    return 'success'

###############################################################################
# Test various error conditions

def ogr_sql_sqlite_12():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")

    # Invalid SQL
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL( "qdfdfdf", dialect = 'SQLite' )
    gdal.PopErrorHandler()
    ds.ReleaseResultSet( sql_lyr )

    # Non existing external datasource
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL( "SELECT * FROM 'foo'.'bar'", dialect = 'SQLite' )
    gdal.PopErrorHandler()
    ds.ReleaseResultSet( sql_lyr )

    # Non existing layer in existing external datasource
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL( "SELECT * FROM 'data'.'azertyuio'", dialect = 'SQLite' )
    gdal.PopErrorHandler()
    ds.ReleaseResultSet( sql_lyr )

    ds = None

    return 'success'

###############################################################################
# Test ogr_layer_Extent(), ogr_layer_SRID() and ogr_layer_GeometryType()

def ogr_sql_sqlite_13():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    lyr = ds.CreateLayer( "non_spatial", geom_type = ogr.wkbNone )

    lyr = ds.CreateLayer( "my_layer", geom_type = ogr.wkbLineString, srs = srs )
    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (0 1,2 3)'))
    lyr.CreateFeature(feat)
    feat = None

    # Test with invalid parameter
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL( "SELECT ogr_layer_Extent(12)", dialect = 'SQLite' )
    gdal.PopErrorHandler()
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    ds.ReleaseResultSet( sql_lyr )

    if geom is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test on non existing layer
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL( "SELECT ogr_layer_Extent('foo')", dialect = 'SQLite' )
    gdal.PopErrorHandler()
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    ds.ReleaseResultSet( sql_lyr )

    if geom is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test ogr_layer_Extent()
    sql_lyr = ds.ExecuteSQL( "SELECT ogr_layer_Extent('my_layer')", dialect = 'SQLite' )
    feat = sql_lyr.GetNextFeature()
    geom_wkt = feat.GetGeometryRef().ExportToWkt()
    feat = None
    ds.ReleaseResultSet( sql_lyr )

    if geom_wkt != 'POLYGON ((0 1,2 1,2 3,0 3,0 1))':
        gdaltest.post_reason('fail')
        print(geom_wkt)
        return 'fail'

    # Test ogr_layer_Extent() on a non spatial layer
    sql_lyr = ds.ExecuteSQL( "SELECT ogr_layer_Extent('non_spatial')", dialect = 'SQLite' )
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    ds.ReleaseResultSet( sql_lyr )

    if geom is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test ogr_layer_SRID()
    sql_lyr = ds.ExecuteSQL( "SELECT ogr_layer_SRID('my_layer') AS the_srid", dialect = 'SQLite' )
    feat = sql_lyr.GetNextFeature()
    the_srid = feat.GetField('the_srid')
    feat = None
    ds.ReleaseResultSet( sql_lyr )

    if the_srid != 4326:
        gdaltest.post_reason('fail')
        print(the_srid)
        return 'fail'

    # Test ogr_layer_SRID() on a non spatial layer
    sql_lyr = ds.ExecuteSQL( "SELECT ogr_layer_SRID('non_spatial') AS the_srid", dialect = 'SQLite' )
    feat = sql_lyr.GetNextFeature()
    the_srid = feat.GetField('the_srid')
    feat = None
    ds.ReleaseResultSet( sql_lyr )

    if the_srid is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test ogr_layer_GeometryType()
    sql_lyr = ds.ExecuteSQL( "SELECT ogr_layer_GeometryType('my_layer') AS the_geometrytype", dialect = 'SQLite' )
    feat = sql_lyr.GetNextFeature()
    the_geometrytype = feat.GetField('the_geometrytype')
    feat = None
    ds.ReleaseResultSet( sql_lyr )

    if the_geometrytype != 'LINESTRING':
        gdaltest.post_reason('fail')
        print(the_geometrytype)
        return 'fail'

    # Test ogr_layer_GeometryType() on a non spatial layer
    sql_lyr = ds.ExecuteSQL( "SELECT ogr_layer_GeometryType('non_spatial') AS the_geometrytype", dialect = 'SQLite' )
    feat = sql_lyr.GetNextFeature()
    the_geometrytype = feat.GetField('the_geometrytype')
    feat = None
    ds.ReleaseResultSet( sql_lyr )

    if the_geometrytype is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test on a external virtual table
    ds_shape = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource('/vsimem/ogr_sql_sqlite_13.shp')
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds_shape.CreateLayer('ogr_sql_sqlite_13', srs = srs)
    ds_shape = None

    sql_lyr = ds.ExecuteSQL( "SELECT ogr_layer_SRID('/vsimem/ogr_sql_sqlite_13.shp'.ogr_sql_sqlite_13) AS the_srid_shp", dialect = 'SQLite' )
    feat = sql_lyr.GetNextFeature()
    the_srid_shp = feat.GetField('the_srid_shp')
    feat = None
    ds.ReleaseResultSet( sql_lyr )

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource('/vsimem/ogr_sql_sqlite_13.shp')

    if the_srid_shp != 32631:
        gdaltest.post_reason('fail')
        print(the_srid_shp)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
#

def ogr_sql_sqlite_14_and_15(sql):

    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    lyr = ds.CreateLayer( "my_layer", geom_type = ogr.wkbLineString, srs = srs )
    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1)
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (0 0,1 1)'))
    lyr.CreateFeature(feat)
    feat = None

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 2)
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (10 0,11 1)'))
    lyr.CreateFeature(feat)
    feat = None

    lyr2 = ds.CreateLayer( "my_layer2", geom_type = ogr.wkbLineString, srs = srs )
    field_defn = ogr.FieldDefn('intfield2', ogr.OFTInteger)
    lyr2.CreateField(field_defn)

    feat = ogr.Feature(lyr2.GetLayerDefn())
    feat.SetField(0, 11)
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (10 1,11 0)'))
    lyr2.CreateFeature(feat)
    feat = None

    feat = ogr.Feature(lyr2.GetLayerDefn())
    feat.SetField(0, 12)
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (0 1,1 0)'))
    lyr2.CreateFeature(feat)
    feat = None

    got_one = False
    got_two = False

    sql_lyr = ds.ExecuteSQL( sql, dialect = 'SQLite' )
    for i in range(2):
        feat = sql_lyr.GetNextFeature()
        i1 = feat.GetField('intfield')
        i2 = feat.GetField('intfield2')
        if (i1 == 1 and i2 == 12 ):
            got_one = True
        if (i1 == 2 and i2 == 11 ):
            got_two = True
        feat = None

    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        return 'fail'

    ds.ReleaseResultSet( sql_lyr )

    if not (got_one and got_two):
        return 'fail'

    return 'success'

###############################################################################
# Test 'idx_layername_geometryname' spatial index recognition

def ogr_sql_sqlite_14():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    if ogrtest.has_spatialite is False:
        return 'skip'

    sql = "SELECT intfield, intfield2 FROM my_layer, my_layer2 WHERE " + \
    "my_layer2.rowid IN (SELECT pkid FROM idx_my_layer2_geometry WHERE " + \
    "xmax > MbrMinX(my_layer.geometry) AND xmin < MbrMaxX(my_layer.geometry) AND " + \
    "ymax >= MbrMinY(my_layer.geometry) AND ymin <= MbrMaxY(my_layer.geometry) )"

    return ogr_sql_sqlite_14_and_15(sql)

###############################################################################
# Test 'SpatialIndex' spatial index recognition

def ogr_sql_sqlite_15():

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    if ogrtest.has_spatialite is False:
        return 'skip'

    if int(gdaltest.spatialite_version[0:gdaltest.spatialite_version.find('.')]) < 3:
        return 'skip'

    sql = "SELECT intfield, intfield2 FROM my_layer, my_layer2 WHERE " + \
    "my_layer2.rowid IN (SELECT ROWID FROM SpatialIndex WHERE f_table_name = 'my_layer2' AND search_frame = my_layer.geometry)"

    return ogr_sql_sqlite_14_and_15(sql)

###############################################################################
def ogr_sql_sqlite_start_webserver():

    ogrtest.webserver_process = None
    ogrtest.webserver_port = 0

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    (ogrtest.webserver_process, ogrtest.webserver_port) = webserver.launch()
    if ogrtest.webserver_port == 0:
        return 'skip'

    return 'success'

###############################################################################
# Test ogr_geocode()

def ogr_sql_sqlite_16(service = None, template = 'http://127.0.0.1:%d/geocoding?q=%%s'):

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    if ogrtest.webserver_port == 0:
        return 'skip'

    gdal.SetConfigOption('OGR_GEOCODE_APPLICATION', 'GDAL/OGR autotest suite')
    gdal.SetConfigOption('OGR_GEOCODE_EMAIL', 'foo@bar')
    gdal.SetConfigOption('OGR_GEOCODE_QUERY_TEMPLATE', template % ogrtest.webserver_port)
    gdal.SetConfigOption('OGR_GEOCODE_DELAY', '0.1')
    gdal.SetConfigOption('OGR_GEOCODE_SERVICE', service)

    ret = 'success'

    for cache_filename in ['tmp/ogr_geocode_cache.sqlite', 'tmp/ogr_geocode_cache.csv']:

        try:
            os.unlink(cache_filename)
        except:
            pass

        gdal.SetConfigOption('OGR_GEOCODE_CACHE_FILE', cache_filename)

        ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")

        for sql in ["SELECT ogr_geocode('Paris')",
                    "SELECT ogr_geocode('Paris', 'geometry')",
                    "SELECT ogr_geocode('Paris', 'display_name') AS display_name",
                    "SELECT ogr_geocode('Paris', 'raw') AS raw"]:

            sql_lyr = ds.ExecuteSQL(sql, dialect = 'SQLite')
            feat = sql_lyr.GetNextFeature()
            if feat is None:
                gdaltest.post_reason('fail')
                print(sql)
                ret = 'fail'
                ds.ReleaseResultSet(sql_lyr)
                break

            if ((sql == "SELECT ogr_geocode('Paris')" or \
                sql == "SELECT ogr_geocode('Paris', 'geometry')") and feat.GetGeometryRef() is None) or \
            (sql == "SELECT ogr_geocode('Paris', 'display_name')" and not feat.IsFieldSet('display_name')) or \
            (sql == "SELECT ogr_geocode('Paris', 'raw')" and not feat.IsFieldSet('raw')):
                feat.DumpReadable()
                gdaltest.post_reason('fail')
                print(sql)
                ret = 'fail'
                ds.ReleaseResultSet(sql_lyr)
                break

            ds.ReleaseResultSet(sql_lyr)

        if ret == 'success':
            for sql in ["SELECT ogr_geocode('NonExistingPlace')", "SELECT ogr_geocode('Error')"]:
                sql_lyr = ds.ExecuteSQL(sql, dialect = 'SQLite')
                feat = sql_lyr.GetNextFeature()
                if feat is None:
                    gdaltest.post_reason('fail')
                    ret = 'fail'
                    ds.ReleaseResultSet(sql_lyr)
                    break

                if feat.GetGeometryRef() is not None:
                    feat.DumpReadable()
                    gdaltest.post_reason('fail')
                    ret = 'fail'
                    ds.ReleaseResultSet(sql_lyr)
                    break

                ds.ReleaseResultSet(sql_lyr)

        # Test various syntax errors
        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode()", dialect = 'SQLite')
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode(5)", dialect = 'SQLite')
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode('Paris', 5)", dialect = 'SQLite')
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode('Paris', 'geometry', 5)", dialect = 'SQLite')
        ds.ReleaseResultSet(sql_lyr)

        ds = None

        # Check cache existence
        cache_ds = ogr.Open(cache_filename)
        if cache_ds is None:
            gdaltest.post_reason('fail')
            ret = 'fail'
        cache_ds = None

        try:
            os.unlink(cache_filename)
        except:
            pass

        ds = None

        if ret != 'success':
            break

    gdal.SetConfigOption('OGR_GEOCODE_CACHE_FILE', None)
    gdal.SetConfigOption('OGR_GEOCODE_APPLICATION', None)
    gdal.SetConfigOption('OGR_GEOCODE_EMAIL', None)
    gdal.SetConfigOption('OGR_GEOCODE_QUERY_TEMPLATE', None)
    gdal.SetConfigOption('OGR_GEOCODE_DELAY', None)
    gdal.SetConfigOption('OGR_GEOCODE_SERVICE', None)

    return ret

###############################################################################
# Test ogr_geocode_reverse()

def ogr_sql_sqlite_17(service = None, template = 'http://127.0.0.1:%d/reversegeocoding?lon={lon}&lat={lat}'):

    if not ogrtest.has_sqlite_dialect:
        return 'skip'

    if ogrtest.webserver_port == 0:
        return 'skip'

    gdal.SetConfigOption('OGR_GEOCODE_APPLICATION', 'GDAL/OGR autotest suite')
    gdal.SetConfigOption('OGR_GEOCODE_EMAIL', 'foo@bar')
    gdal.SetConfigOption('OGR_GEOCODE_REVERSE_QUERY_TEMPLATE', template % ogrtest.webserver_port)
    gdal.SetConfigOption('OGR_GEOCODE_DELAY', '0.1')
    gdal.SetConfigOption('OGR_GEOCODE_SERVICE', service)

    ret = 'success'

    for cache_filename in ['tmp/ogr_geocode_cache.sqlite', 'tmp/ogr_geocode_cache.csv']:

        try:
            os.unlink(cache_filename)
        except:
            pass

        gdal.SetConfigOption('OGR_GEOCODE_CACHE_FILE', cache_filename)

        ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")

        sql_list = [ "SELECT ogr_geocode_reverse(2,49,'display_name') AS display_name",
                     "SELECT ogr_geocode_reverse(2,49,'display_name','zoom=12') AS display_name",
                     "SELECT ogr_geocode_reverse(2.0,49.0,'display_name') AS display_name",
                     "SELECT ogr_geocode_reverse(2.0,49.0,'raw') AS raw" ]
        if ogrtest.has_spatialite:
            sql_list.append("SELECT ogr_geocode_reverse(MakePoint(2,49),'display_name') AS display_name")
            sql_list.append("SELECT ogr_geocode_reverse(MakePoint(2,49),'display_name','zoom=12') AS display_name")

        for sql in sql_list:

            sql_lyr = ds.ExecuteSQL(sql, dialect = 'SQLite')
            feat = sql_lyr.GetNextFeature()
            if feat is None:
                gdaltest.post_reason('fail')
                print(sql)
                ret = 'fail'
                ds.ReleaseResultSet(sql_lyr)
                break

            if sql.find('raw') != -1:
                field_to_test = 'raw'
            else:
                field_to_test = 'display_name'
            if not feat.IsFieldSet(field_to_test):
                feat.DumpReadable()
                gdaltest.post_reason('fail')
                print(sql)
                ret = 'fail'
                ds.ReleaseResultSet(sql_lyr)
                break

            ds.ReleaseResultSet(sql_lyr)

        # Test various syntax errors
        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode_reverse()", dialect = 'SQLite')
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode_reverse(2)", dialect = 'SQLite')
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode_reverse(2, 'foo')", dialect = 'SQLite')
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode_reverse(2, 49)", dialect = 'SQLite')
        ds.ReleaseResultSet(sql_lyr)

        if ogrtest.has_spatialite:
            sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode_reverse(MakePoint(2,49))", dialect = 'SQLite')
            ds.ReleaseResultSet(sql_lyr)

            sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode_reverse(MakePoint(2,49), 5)", dialect = 'SQLite')
            ds.ReleaseResultSet(sql_lyr)

        ds = None

        # Check cache existence
        cache_ds = ogr.Open(cache_filename)
        if cache_ds is None:
            gdaltest.post_reason('fail')
            ret = 'fail'
        cache_ds = None

        try:
            os.unlink(cache_filename)
        except:
            pass

        ds = None

        if ret != 'success':
            break

    gdal.SetConfigOption('OGR_GEOCODE_CACHE_FILE', None)
    gdal.SetConfigOption('OGR_GEOCODE_APPLICATION', None)
    gdal.SetConfigOption('OGR_GEOCODE_EMAIL', None)
    gdal.SetConfigOption('OGR_GEOCODE_REVERSE_QUERY_TEMPLATE', None)
    gdal.SetConfigOption('OGR_GEOCODE_DELAY', None)
    gdal.SetConfigOption('OGR_GEOCODE_SERVICE', None)

    return ret

###############################################################################
# Test ogr_geocode() with Yahoo geocoding service

def ogr_sql_sqlite_18():

    return ogr_sql_sqlite_16('YAHOO', 'http://127.0.0.1:%d/yahoogeocoding?q=%%s')

###############################################################################
# Test ogr_geocode_reverse() with Yahoo geocoding service

def ogr_sql_sqlite_19():

    return ogr_sql_sqlite_17('YAHOO', 'http://127.0.0.1:%d/yahooreversegeocoding?q={lat},{lon}&gflags=R')

###############################################################################
def ogr_sql_sqlite_stop_webserver():

    if ogrtest.webserver_port == 0:
        return 'skip'

    webserver.server_stop(ogrtest.webserver_process, ogrtest.webserver_port)

    return 'success'

gdaltest_list = [
    ogr_sql_sqlite_1,
    ogr_sql_sqlite_2,
    ogr_sql_sqlite_3,
    ogr_sql_sqlite_4,
    ogr_sql_sqlite_5,
    ogr_sql_sqlite_6,
    ogr_sql_sqlite_7,
    ogr_sql_sqlite_8,
    ogr_sql_sqlite_9,
    ogr_sql_sqlite_10,
    ogr_sql_sqlite_11,
    ogr_sql_sqlite_12,
    ogr_sql_sqlite_13,
    ogr_sql_sqlite_14,
    ogr_sql_sqlite_15,
    ogr_sql_sqlite_start_webserver,
    ogr_sql_sqlite_16,
    ogr_sql_sqlite_17,
    ogr_sql_sqlite_18,
    ogr_sql_sqlite_19,
    ogr_sql_sqlite_stop_webserver,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_sql_sqlite' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

