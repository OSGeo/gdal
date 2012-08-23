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

###############################################################################
# Tests that don't involve geometry

def ogr_sql_sqlite_1():

    if ogr.GetDriverByName('SQLite') is None:
        return 'skip'

    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")

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

        # Test INSERT
        sql_lyr = ds.ExecuteSQL( "INSERT INTO my_layer (intfield, nullablefield, doublefield, strfield, binaryfield) VALUES (1,NULL,2.34,'foo',x'0001FF')", dialect = 'SQLite' )
        ds.ReleaseResultSet( sql_lyr )

        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        if feat.GetField('intfield') != 1 or \
           feat.GetField('nullablefield') is not None or \
           feat.GetField('doublefield') != 2.34 or \
           feat.GetField('strfield') != 'foo' or \
           feat.GetField('binaryfield') != '0001FF':
            gdaltest.post_reason('failure')
            feat.DumpReadable()
            return 'fail'
        feat = None

        # Test UPDATE
        sql_lyr = ds.ExecuteSQL( "UPDATE my_layer SET intfield = 2, doublefield = 3.45, strfield = 'bar' WHERE ROWID = 0", dialect = 'SQLite' )
        ds.ReleaseResultSet( sql_lyr )

        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        if feat.GetField('intfield') != 2 or \
        feat.GetField('doublefield') != 3.45 or \
        feat.GetField('strfield') != 'bar' :
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
           feat.GetField('strfield') != 'bar' :
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
                        'intfield = 2 AND doublefield = 3.45', 'ROWID = 0']:
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
                        'intfield = 2 AND doublefield = 0', 'ROWID = 10000']:
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

    if ogr.GetDriverByName('SQLite') is None:
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

    ds.DeleteLayer(0)

    return 'success'

###############################################################################
# Test that involves a join

def ogr_sql_sqlite_3():

    if ogr.GetDriverByName('SQLite') is None:
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

    if ogr.GetDriverByName('SQLite') is None:
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

    if ogr.GetDriverByName('SQLite') is None:
        return 'skip'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/foo.db', options = ['SPATIALITE=YES'])
    ogrtest.has_spatialite = ds is not None
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

    if ogr.GetDriverByName('SQLite') is None:
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

    if ogr.GetDriverByName('SQLite') is None:
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

    if ogr.GetDriverByName('SQLite') is None:
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

gdaltest_list = [
    ogr_sql_sqlite_1,
    ogr_sql_sqlite_2,
    ogr_sql_sqlite_3,
    ogr_sql_sqlite_4,
    ogr_sql_sqlite_5,
    ogr_sql_sqlite_6,
    ogr_sql_sqlite_7,
    ogr_sql_sqlite_8,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_sql_sqlite' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

