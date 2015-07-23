#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test various OGR SQL support options.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

import sys

sys.path.append( '../pymod' )

from osgeo import gdal
import gdaltest
from osgeo import ogr
import ogrtest

###############################################################################
# Test a simple query with a where clause.

def ogr_sql_1():
    gdaltest.ds = ogr.Open( 'data' )
    gdaltest.lyr = gdaltest.ds.GetLayerByName( 'poly' )

    gdaltest.lyr.SetAttributeFilter( 'eas_id < 167' )

    count = gdaltest.lyr.GetFeatureCount()
    if count != 3:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 3' % count )
        return 'fail'

    gdaltest.lyr.SetAttributeFilter( '' )
    count = gdaltest.lyr.GetFeatureCount()
    if count != 10:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 10' % count )
        return 'fail'

    return 'success'

###############################################################################
# Test DISTINCT handling 

def ogr_sql_2():

    expect = [168, 169, 166, 158, 165]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 'select distinct eas_id from poly where eas_id < 170' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'eas_id', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test ORDER BY handling 

def ogr_sql_3():

    expect = [158, 165, 166, 168, 169]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 'select distinct eas_id from poly where eas_id < 170 order by eas_id' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'eas_id', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test ORDER BY DESC handling 

def ogr_sql_3_desc():

    expect = [169, 168, 166, 165, 158]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 'select distinct eas_id from poly where eas_id < 170 order by eas_id desc' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'eas_id', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test DISTINCT and ORDER BY on strings.

def ogr_sql_4():

    expect = ['_158_', '_165_', '_166_', '_168_', '_170_', '_171_', '_179_']
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 'select distinct name from idlink order by name asc' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'name', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test column functions.

def ogr_sql_5():

    sql_lyr = gdaltest.ds.ExecuteSQL( 'select max(eas_id), min(eas_id), avg(eas_id), sum(eas_id), count(eas_id) from idlink' )
    feat = sql_lyr.GetNextFeature()
    if feat['max_eas_id'] != 179:
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    if feat['min_eas_id'] != 158:
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    if abs(feat['avg_eas_id'] - 168.142857142857) > 1e-12:
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    if feat['count_eas_id'] != 7:
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    if feat['sum_eas_id'] != 1177:
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    gdaltest.ds.ReleaseResultSet( sql_lyr )

    return 'success'

###############################################################################
# Test simple COUNT() function.

def ogr_sql_6():

    expect = [ 10 ]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 'select count(*) from poly' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'count_*', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Verify that selecting the FID works properly.

def ogr_sql_7():

    expect = [ 7, 8 ]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 'select eas_id, fid from poly where eas_id in (158,165)' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'fid', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Verify that wildcard expansion works properly.

def ogr_sql_8():

    expect = [ '35043369', '35043408' ]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 'select * from poly where eas_id in (158,165)' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'PRFEDEA', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Verify that quoted table names work.

def ogr_sql_9():

    expect = [ '35043369', '35043408' ]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 'select * from "poly" where eas_id in (158,165)' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'PRFEDEA', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test the ILIKE operator. 

def ogr_sql_10():

    expect = [170]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( "select eas_id from poly where prfedea ilike '%413'" )

    tr = ogrtest.check_features_against_list( sql_lyr, 'eas_id', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test MAX() on empty dataset.

def ogr_sql_11():

    expect = [None]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( "select max(eas_id) from empty" )

    tr = ogrtest.check_features_against_list( sql_lyr, 'max_eas_id', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test DISTINCT on empty dataset.

def ogr_sql_12():

    expect = []
    
    sql_lyr = gdaltest.ds.ExecuteSQL( "select distinct eas_id from empty" )

    tr = ogrtest.check_features_against_list( sql_lyr, 'eas_id', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Verify selection of, and on ogr_geometry. 

def ogr_sql_13():

    expect = ['POLYGON','POLYGON','POLYGON','POLYGON','POLYGON',
              'POLYGON','POLYGON','POLYGON','POLYGON','POLYGON']
    
    sql_lyr = gdaltest.ds.ExecuteSQL( "select ogr_geometry from poly where ogr_geometry = 'POLYGON'" )

    tr = ogrtest.check_features_against_list( sql_lyr, 'ogr_geometry', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Verify selection of, and on ogr_style and ogr_geom_wkt.

def ogr_sql_14():

    expect = [
        'BRUSH(fc:#000000,bc:#ffffff,id:"mapinfo-brush-1,ogr-brush-1");PEN(w:1px,c:#000000,id:"mapinfo-pen-2,ogr-pen-0")',
        'BRUSH(fc:#000000,bc:#ffffff,id:"mapinfo-brush-1,ogr-brush-1");PEN(w:1px,c:#000000,id:"mapinfo-pen-2,ogr-pen-0")' ]

    ds = ogr.Open( 'data/small.mif' )
    sql_lyr = ds.ExecuteSQL( "select ogr_style from small where ogr_geom_wkt LIKE 'POLYGON%'" )

    tr = ogrtest.check_features_against_list( sql_lyr, 'ogr_style', expect )

    ds.ReleaseResultSet( sql_lyr )
    ds = None

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Verify that selecting with filtering by FID works properly.

def ogr_sql_15():

    expect = [ 7 ]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 'select fid,eas_id,prfedea from poly where fid = %d' % expect[0]  )

    tr = ogrtest.check_features_against_list( sql_lyr, 'fid', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################

def ogr_sql_16():

    expect = [ 2 ]

    ds = ogr.Open( 'data/small.mif' )
    sql_lyr = ds.ExecuteSQL( "select fid from small where owner < 'H'" )

    tr = ogrtest.check_features_against_list( sql_lyr, 'fid', expect )

    ds.ReleaseResultSet( sql_lyr )
    ds = None

    if tr:
        return 'success'
    else:
        return 'fail'


###############################################################################
# Test the RFC 21 CAST operator.
#
def ogr_sql_17():

    expect = [ '1', '2' ]

    ds = ogr.Open( 'data/small.mif' )
    sql_lyr = ds.ExecuteSQL( "select CAST(fid as CHARACTER(10)), CAST(data as numeric(7,3)) from small" )

    fld_def = sql_lyr.GetLayerDefn().GetFieldDefn(0)

    if fld_def.GetName() != 'fid':
        gdaltest.post_reason( 'got wrong fid field name' )
        print(fld_def.GetName())
        return 'fail'

    if fld_def.GetType() != ogr.OFTString:
        gdaltest.post_reason( 'got wrong fid field type' )
        print(fld_def.GetType())
        
    if fld_def.GetWidth() != 10:
        gdaltest.post_reason( 'got wrong fid field width' )
        print(fld_def.GetWidth())
        
    fld_def = sql_lyr.GetLayerDefn().GetFieldDefn(1)

    if fld_def.GetName() != 'data':
        gdaltest.post_reason( 'got wrong data field name' )
        print(fld_def.GetName())
        return 'fail'

    if fld_def.GetType() != ogr.OFTReal:
        gdaltest.post_reason( 'got wrong data field type' )
        print(fld_def.GetType())
        
    if fld_def.GetWidth() != 7:
        gdaltest.post_reason( 'got wrong data field width' )
        print(fld_def.GetWidth())
        
    if fld_def.GetPrecision() != 3:
        gdaltest.post_reason( 'got wrong data field precision' )
        print(fld_def.GetPrecision())
        
    tr = ogrtest.check_features_against_list( sql_lyr, 'fid', expect )

    ds.ReleaseResultSet( sql_lyr )
    ds = None

    if tr:
        return 'success'
    else:
        return 'fail'


###############################################################################
# Test extended character set

def ogr_sql_18():

    if sys.version_info >= (3,0,0):
        return 'skip'

    name = 'data/departs.vrt'

    ds = ogr.Open( name )
    if ds is None:
        return 'fail'
    
    sql = 'select * from D\303\251parts'
    sql_lyr = ds.ExecuteSQL( sql )
    if sql_lyr is None:
        ds = None
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat is None:
        return 'fail'
    feat = None

    ds.ReleaseResultSet( sql_lyr )

    # Test #2221
    sql = 'select NOMd\303\251PART from D\303\251parts'
    sql_lyr = ds.ExecuteSQL( sql )
    if sql_lyr is None:
        ds = None
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat is None:
        return 'fail'
    feat = None

    ds.ReleaseResultSet( sql_lyr )
    ds = None

    return 'success'

###############################################################################
# Test empty request string

def ogr_sql_19():

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    sql_lyr = gdaltest.ds.ExecuteSQL( '' )
    gdal.PopErrorHandler()

    if sql_lyr is not None:
        return 'fail'

    return 'success'

###############################################################################
# Test query "SELECT * from my_layer" on layer without any field (#2788)

def ogr_sql_20():

    mem_ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
    mem_lyr = mem_ds.CreateLayer( "my_layer")

    feat = ogr.Feature(mem_lyr.GetLayerDefn() )
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    mem_lyr.CreateFeature( feat )

    feat = ogr.Feature(mem_lyr.GetLayerDefn() )
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 3)"))
    mem_lyr.CreateFeature( feat )

    sql_lyr = mem_ds.ExecuteSQL("SELECT * from my_layer")
    if sql_lyr.GetFeatureCount() != 2:
        return 'fail'
    mem_ds.ReleaseResultSet(sql_lyr)
    mem_ds = None

    return 'success'

###############################################################################
# Test query "SELECT *, fid from my_layer" on layer without any field (#2788)

def ogr_sql_21():

    mem_ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
    mem_ds.CreateLayer( "my_layer")

    sql_lyr = mem_ds.ExecuteSQL("SELECT *, fid from my_layer")
    if sql_lyr.GetLayerDefn().GetFieldCount() != 1:
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(0).GetName() != 'fid':
        return 'fail'
    mem_ds.ReleaseResultSet(sql_lyr)
    mem_ds = None

    return 'success'

###############################################################################
# Test multiple expansion of '*' as in "SELECT *, fid, *, my_layer.* from my_layer" (#2788)

def ogr_sql_22():

    mem_ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
    mem_lyr = mem_ds.CreateLayer( "my_layer")
    mem_lyr.CreateField( ogr.FieldDefn("test", ogr.OFTString) )

    feat = ogr.Feature(mem_lyr.GetLayerDefn() )
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    mem_lyr.CreateFeature( feat )

    sql_lyr = mem_ds.ExecuteSQL("SELECT *, fid, *, my_layer.* from my_layer")
    if sql_lyr.GetLayerDefn().GetFieldCount() != 4:
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(0).GetName() != 'test':
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(1).GetName() != 'fid':
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(2).GetName() != 'test':
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(3).GetName() != 'my_layer.test':
        return 'fail'
    mem_ds.ReleaseResultSet(sql_lyr)
    mem_ds = None

    return 'success'

###############################################################################
# Test query "SELECT DISTINCT test from my_layer" (#2788)

def ogr_sql_23():

    mem_ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
    mem_lyr = mem_ds.CreateLayer( "my_layer")
    mem_lyr.CreateField( ogr.FieldDefn("test", ogr.OFTString) )

    feat = ogr.Feature(mem_lyr.GetLayerDefn() )
    feat.SetField("test", 0)
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    mem_lyr.CreateFeature( feat )

    feat = ogr.Feature(mem_lyr.GetLayerDefn() )
    feat.SetField("test", 1)
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(2 3)"))
    mem_lyr.CreateFeature( feat )

    sql_lyr = mem_ds.ExecuteSQL("SELECT DISTINCT test from my_layer")
    if sql_lyr.GetFeatureCount() != 2:
        return 'fail'
    mem_ds.ReleaseResultSet(sql_lyr)
    mem_ds = None

    return 'success'

###############################################################################
# Test that style strings get carried with OGR SQL SELECT results. (#2808)

def ogr_sql_24():

    result = 'success'
    
    ds = ogr.Open( 'data/smalltest.dgn' )

    sql_layer = ds.ExecuteSQL( 'SELECT * from elements where colorindex=83 and type=3' )

    feat = sql_layer.GetNextFeature()
    if len(feat.GetStyleString()) < 10:
        print(feat.GetStyleString())
        gdaltest.post_reason( 'style string apparently not propagated to OGR SQL results.' )
        result = 'fail'
    feat = None
    ds.ReleaseResultSet( sql_layer )
    ds = None

    return result

###############################################################################
# Test for OGR_GEOM_AREA special field (#2949)

def ogr_sql_25():

    mem_ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
    mem_lyr = mem_ds.CreateLayer( "my_layer")
    mem_lyr.CreateField( ogr.FieldDefn("test", ogr.OFTString) )

    feat = ogr.Feature(mem_lyr.GetLayerDefn() )
    feat.SetField("test", 0)
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,1 0,0 0))"))
    mem_lyr.CreateFeature( feat )

    feat = ogr.Feature(mem_lyr.GetLayerDefn() )
    feat.SetField("test", 1)
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 0.5,0.5 0.5,0.5 0,0 0))"))
    mem_lyr.CreateFeature( feat )

    sql_lyr = mem_ds.ExecuteSQL("SELECT test, OGR_GEOM_AREA from my_layer WHERE OGR_GEOM_AREA > 0.9")
    if sql_lyr.GetFeatureCount() != 1:
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat.GetFieldAsDouble('OGR_GEOM_AREA') != 1.0:
        return 'fail'
    if feat.GetFieldAsString('test') != '0':
        return 'fail'
    mem_ds.ReleaseResultSet(sql_lyr)

    mem_ds = None

    return 'success'

###############################################################################
# Test query 'SELECT 'literal_value' AS column_name FROM a_table'
#
def ogr_sql_26():

    mem_ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
    mem_lyr = mem_ds.CreateLayer( "my_layer")

    feat = ogr.Feature(mem_lyr.GetLayerDefn() )
    mem_lyr.CreateFeature( feat )

    sql_lyr = mem_ds.ExecuteSQL("SELECT 'literal_value' AS my_column, 'literal_value2' my_column2 FROM my_layer")
    if sql_lyr.GetFeatureCount() != 1:
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat.GetFieldAsString('my_column') != 'literal_value':
        return 'fail'
    if feat.GetFieldAsString('my_column2') != 'literal_value2':
        return 'fail'
    mem_ds.ReleaseResultSet(sql_lyr)

    mem_ds = None

    return 'success'

###############################################################################

###############################################################################
# Test query on datetime columns
#
def ogr_sql_27():

    ds = ogr.Open('data/testdatetime.csv')

    sql_lyr = ds.ExecuteSQL("SELECT * FROM testdatetime WHERE " \
                            "timestamp < '2010/04/01 00:00:00' AND " \
                            "timestamp > '2009/11/15 11:59:59' AND " \
                            "timestamp != '2009/12/31 23:00:00' " \
                            "ORDER BY timestamp DESC")

    tr = ogrtest.check_features_against_list( sql_lyr, 'name', [ 'foo5', 'foo4'] )

    ds.ReleaseResultSet( sql_lyr )
    ds = None

    if tr:
        return 'success'
    else:
        return 'fail'


###############################################################################
# Test robustness against invalid SQL statements.
# With RFC 28 new implementation, most of them are directly caught by the generated
# code from the grammar

def ogr_sql_28():

    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
    lyr = ds.CreateLayer( "my_layer")
    lyr.GetLayerDefn().GetGeomFieldDefn(0).SetName('geom') # a bit border line but OK for Memory driver...
    field_defn = ogr.FieldDefn( "strfield", ogr.OFTString )
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn( "intfield", ogr.OFTInteger )
    lyr.CreateField(field_defn)

    lyr = ds.CreateLayer( "my_layer2")
    field_defn = ogr.FieldDefn( "strfield", ogr.OFTString )
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn( "strfield2", ogr.OFTString )
    lyr.CreateField(field_defn)

    try:
        sql_lyr = ds.ExecuteSQL(None)
        gdaltest.post_reason('expected error on NULL query')
        return 'fail'
    except:
        pass

    queries = [
    '',
    '1',
    '*',
    'SELECT',
    "SELECT ' FROM my_layer",
    'SELECT + FROM my_layer',
    'SELECT (1 FROM my_layer',
    'SELECT (1)) FROM my_layer',
    'SELECT (1,) FROM my_layer',
    'SELECT 1 + FROM my_layer',
    "SELECT 1 + 'a' FROM my_layer",
    'SELECT 1 - FROM my_layer',
    'SELECT 1 * FROM my_layer',
    'SELECT 1 % FROM my_layer',
    'SELECT *',
    'SELECT * FROM',
    'SELECT * FROM foo',
    'SELECT FROM my_layer',
    'SELECT FROM FROM my_layer',
    "SELECT ('strfield'",
    "SELECT 'strfield' +",
    "SELECT 'strfield' 'strfield'",
    "SELECT CONCAT('strfield')",
    'SELECT foo(strfield) FROM my_layer', # Undefined function 'foo' used.
    'SELECT strfield, FROM my_layer',
    'SELECT strfield, foo FROM my_layer',
    'SELECT strfield AS FROM my_layer',
    'SELECT strfield AS 1 FROM my_layer',
    'SELECT strfield AS strfield2 FROM',
    'SELECT strfield + intfield FROM my_layer',
    'SELECT CAST',
    'SELECT CAST(',
    'SELECT CAST(strfield',
    'SELECT CAST(strfield AS',
    'SELECT CAST(strfield AS foo',
    'SELECT CAST(strfield AS foo)',
    'SELECT CAST(strfield AS foo) FROM',
    'SELECT CAST(strfield AS foo) FROM my_layer',
    'SELECT CAST(strfield AS CHARACTER',
    'SELECT CAST(strfield AS CHARACTER)',
    'SELECT CAST(strfield AS CHARACTER) FROM',
    'SELECT CAST(strfield AS CHARACTER) FROM foo',
    'SELECT CAST(strfield AS CHARACTER(',
    'SELECT CAST(strfield AS CHARACTER(2',
    'SELECT CAST(strfield AS CHARACTER(2)',
    'SELECT CAST(strfield AS CHARACTER(2))',
    'SELECT CAST(strfield AS CHARACTER(2)) FROM',
    'SELECT CAST(strfield AS CHARACTER(2)) FROM foo',
    'SELECT CAST(strfield AS 1) FROM my_layer',
    'SELECT * FROM my_layer WHERE',
    #'SELECT * FROM my_layer WHERE strfield',
    'SELECT * FROM my_layer WHERE strfield = ',
    'SELECT * FROM my_layer WHERE strfield = foo',
    "SELECT * FROM my_layer WHERE foo = 'a'",
    "SELECT * FROM my_layer WHERE strfield = 'a"
    "SELECT * FROM my_layer WHERE strfield = 'a' ORDER ",
    "SELECT * FROM my_layer WHERE strfield = 'a' ORDER BY",
    "SELECT * FROM my_layer WHERE strfield = 'a' ORDER BY foo",
    "SELECT * FROM my_layer WHERE strfield = 'a' ORDER BY strfield UNK",
    "SELECT * FROM my_layer ORDER BY geom", # Cannot use geometry field 'geom' in a ORDER BY clause
    "SELECT FOO(*) FROM my_layer",
    "SELECT FOO(*) AS bar FROM my_layer",
    "SELECT COUNT",
    "SELECT COUNT(",
    "SELECT COUNT() FROM my_layer",
    "SELECT COUNT(*",
    "SELECT COUNT(*)",
    "SELECT COUNT(*) FROM",
    "SELECT COUNT(*) AS foo FROM",
    "SELECT COUNT(* FROM my_layer",
    "SELECT COUNT(FOO intfield) FROM my_layer",
    "SELECT COUNT(DISTINCT intfield FROM my_layer",
    "SELECT COUNT(DISTINCT *) FROM my_layer",
    "SELECT FOO(DISTINCT intfield) FROM my_layer",
    "SELECT FOO(DISTINCT intfield) as foo FROM my_layer",
    "SELECT DISTINCT foo FROM my_layer",
    "SELECT DISTINCT foo AS 'id' 'id2' FROM",
    "SELECT DISTINCT foo AS id id2 FROM",
    "SELECT DISTINCT FROM my_layer",
    "SELECT DISTINCT strfield, COUNT(DISTINCT intfield) FROM my_layer",
    "SELECT MIN(intfield*2) FROM my_layer",
    "SELECT MIN(intfield,2) FROM my_layer",
    "SELECT MIN(foo) FROM my_layer",
    "SELECT MAX(foo) FROM my_layer",
    "SELECT SUM(foo) FROM my_layer",
    "SELECT AVG(foo) FROM my_layer",
    "SELECT MIN(strfield) FROM my_layer",
    "SELECT MAX(strfield) FROM my_layer",
    "SELECT SUM(strfield) FROM my_layer",
    "SELECT AVG(strfield) FROM my_layer",
    "SELECT AVG(intfield, intfield) FROM my_layer",
    "SELECT * FROM my_layer WHERE AVG(intfield) = 1" ,
    "SELECT * FROM 'foo' foo" ,
    "SELECT * FROM my_layer WHERE strfield =" ,
    "SELECT * FROM my_layer WHERE strfield = foo" ,
    "SELECT * FROM my_layer WHERE strfield = intfield" ,
    "SELECT * FROM my_layer WHERE strfield = 1" ,
    "SELECT * FROM my_layer WHERE strfield = '1' AND" ,
    #"SELECT * FROM my_layer WHERE 1 AND 2" ,
    "SELECT * FROM my_layer WHERE strfield LIKE" ,
    "SELECT * FROM my_layer WHERE strfield LIKE 1" ,
    "SELECT * FROM my_layer WHERE strfield IS" ,
    "SELECT * FROM my_layer WHERE strfield IS NOT" ,
    "SELECT * FROM my_layer WHERE strfield IS foo" ,
    "SELECT * FROM my_layer WHERE strfield IS NOT foo" ,
    "SELECT * FROM my_layer WHERE (strfield IS NOT NULL" ,
    "SELECT * FROM my_layer WHERE strfield IN" ,
    "SELECT * FROM my_layer WHERE strfield IN(" ,
    "SELECT * FROM my_layer WHERE strfield IN()" ,
    "SELECT * FROM my_layer WHERE strfield IN('a'" ,
    "SELECT * FROM my_layer WHERE strfield IN('a'," ,
    "SELECT * FROM my_layer WHERE strfield IN('a','b'" ,
    "SELECT * FROM my_layer WHERE strfield IN('a','b'))" ,
    "SELECT * FROM my_layer LEFT" ,
    "SELECT * FROM my_layer LEFT JOIN" ,
    "SELECT * FROM my_layer LEFT JOIN foo",
    "SELECT * FROM my_layer LEFT JOIN foo ON my_layer.strfield = my_layer2.strfield",
    "SELECT * FROM my_layer LEFT JOIN my_layer2 ON my_layer.strfield = foo.strfield",
    "SELECT * FROM my_layer LEFT JOIN my_layer2 ON my_layer.strfield = my_layer2.foo",
    #"SELECT * FROM my_layer LEFT JOIN my_layer2 ON my_layer.strfield != my_layer2.strfield",
    "SELECT *, my_layer2. FROM my_layer LEFT JOIN my_layer2 ON my_layer.strfield = my_layer2.strfield",
    "SELECT *, my_layer2.foo FROM my_layer LEFT JOIN my_layer2 ON my_layer.strfield = my_layer2.strfield",
    "SELECT * FROM my_layer UNION" ,
    "SELECT * FROM my_layer UNION ALL" ,
    "SELECT * FROM my_layer UNION ALL SELECT" ,
    "SELECT * FROM my_layer UNION ALL SELECT *" ,
    "SELECT * FROM my_layer UNION ALL SELECT * FROM" ,
    ]

    for query in queries:
        gdal.ErrorReset()
        #print query
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        sql_lyr = ds.ExecuteSQL(query)
        gdal.PopErrorHandler()
        if sql_lyr is not None:
            gdaltest.post_reason('expected None result on "%s"' % query)
            ds.ReleaseResultSet(sql_lyr)
            return 'fail'
        if gdal.GetLastErrorType() == 0:
            gdaltest.post_reason('expected error on "%s"' % query)
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Verify that IS NULL and IS NOT NULL are working

def ogr_sql_29():

    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
    lyr = ds.CreateLayer( "my_layer")
    field_defn = ogr.FieldDefn( "strfield", ogr.OFTString )
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'a')
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'b')
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    sql_lyr = ds.ExecuteSQL( 'select * from my_layer where strfield is null'  )
    count_is_null = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet( sql_lyr )

    sql_lyr = ds.ExecuteSQL( 'select * from my_layer where strfield is not null'  )
    count_is_not_null = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet( sql_lyr )

    ds = None

    if count_is_null != 1:
        gdaltest.post_reason('IS NULL failed')
        print(count_is_null)
        return 'fail'

    if count_is_not_null != 2:
        gdaltest.post_reason('IS NOT NULL failed')
        print(count_is_not_null)
        return 'fail'

    return 'success'

###############################################################################
# Verify a select mixing a count(*) with something else works without errors

def ogr_sql_30():

    gdal.ErrorReset()

    sql_lyr = gdaltest.ds.ExecuteSQL( 'select min(eas_id), count(*) from poly' )

    feat = sql_lyr.GetNextFeature()
    val_count = feat.GetField(1)

    gdaltest.ds.ReleaseResultSet( sql_lyr )
    
    if gdal.GetLastErrorMsg() != '':
        return 'fail'
    
    if val_count == 10:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Regression test for #4022

def ogr_sql_31():

    gdal.ErrorReset()

    sql_lyr = gdaltest.ds.ExecuteSQL( 'select min(eas_id) from poly where area = 0' )

    feat = sql_lyr.GetNextFeature()
    val = feat.GetField(0)

    gdaltest.ds.ReleaseResultSet( sql_lyr )
    
    if gdal.GetLastErrorMsg() != '':
        return 'fail'
    
    if val is None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Regression test for #4022 (same as above, but with dialect = 'OGRSQL')

def ogr_sql_32():

    gdal.ErrorReset()

    sql_lyr = gdaltest.ds.ExecuteSQL( 'select min(eas_id) from poly where area = 0',
                                      dialect = 'OGRSQL' )

    feat = sql_lyr.GetNextFeature()
    val = feat.GetField(0)

    gdaltest.ds.ReleaseResultSet( sql_lyr )
    
    if gdal.GetLastErrorMsg() != '':
        return 'fail'
    
    if val is None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Check ALTER TABLE commands

def ogr_sql_33():

    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
    lyr = ds.CreateLayer( "my_layer")

    # We support with and without COLUMN keyword
    for extrakeyword in ('COLUMN ', ''):
        sql = 'ALTER TABLE my_layer ADD %smyfield NUMERIC(20, 8)' % extrakeyword
        ds.ExecuteSQL(sql)
        if lyr.GetLayerDefn().GetFieldIndex('myfield') == -1 or \
        lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('myfield')).GetType() != ogr.OFTReal or \
        lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('myfield')).GetWidth() != 20 or \
        lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('myfield')).GetPrecision() != 8:
            gdaltest.post_reason('%s failed' % sql)
            return 'fail'

        sql = 'ALTER TABLE my_layer RENAME %smyfield TO "myfield 2"' % extrakeyword
        ds.ExecuteSQL(sql)
        if lyr.GetLayerDefn().GetFieldIndex('myfield') != -1 or \
        lyr.GetLayerDefn().GetFieldIndex('myfield 2') == -1:
            gdaltest.post_reason('%s failed' % sql)
            return 'fail'

        sql = 'ALTER TABLE my_layer ALTER %s"myfield 2" TYPE CHARACTER' % extrakeyword
        ds.ExecuteSQL(sql)
        if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('myfield 2')).GetType() != ogr.OFTString:
            gdaltest.post_reason('%s failed' % sql)
            return 'fail'

        sql = 'ALTER TABLE my_layer ALTER %s"myfield 2" TYPE CHARACTER(15)' % extrakeyword
        ds.ExecuteSQL(sql)
        if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('myfield 2')).GetWidth() != 15:
            gdaltest.post_reason('%s failed' % sql)
            return 'fail'

        sql = 'ALTER TABLE my_layer DROP %s"myfield 2"' % extrakeyword
        ds.ExecuteSQL(sql)
        if lyr.GetLayerDefn().GetFieldIndex('myfield 2') != -1:
            gdaltest.post_reason('%s failed' % sql)
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test implicit conversion from string to numeric (#4259)

def ogr_sql_34():

    sql_lyr = gdaltest.ds.ExecuteSQL( "select count(*) from poly where eas_id in ('165')" )

    feat = sql_lyr.GetNextFeature()
    val = feat.GetField(0)

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if val != 1:
        print(val)
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = gdaltest.ds.ExecuteSQL( "select count(*) from poly where eas_id in ('a165')" )
    gdal.PopErrorHandler()
    if sql_lyr is not None:
        return 'fail'

    return 'success'

###############################################################################
# Test huge SQL queries (#4262)

def ogr_sql_35():

    cols = "area"
    for i in range(10):
        cols = cols + "," + cols
    sql_lyr = gdaltest.ds.ExecuteSQL( "select %s from poly" % cols )

    count_cols = sql_lyr.GetLayerDefn().GetFieldCount()

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if count_cols == 1024:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test select distinct on null values (#4353)

def ogr_sql_36():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('ogr_sql_36')
    lyr = ds.CreateLayer('layer')
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('floatfield', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('strfield', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('int64field', ogr.OFTInteger64))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1)
    feat.SetField(1, 2.3)
    feat.SetField(2, "456")
    feat.SetField(3, 1234567890123)
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None

    for fieldname in ['intfield', 'int64field', 'floatfield', 'strfield']:
        sql_lyr = ds.ExecuteSQL( "select distinct %s from layer order by %s asc" % (fieldname, fieldname))
        feat = sql_lyr.GetNextFeature()
        if feat.IsFieldSet(0) != 0:
            gdaltest.post_reason('fail')
            print('field %s' % fieldname)
            feat.DumpReadable()
            return 'fail'
        feat = sql_lyr.GetNextFeature()
        if feat.IsFieldSet(0) == 0:
            gdaltest.post_reason('fail')
            print('field %s' % fieldname)
            feat.DumpReadable()
            return 'fail'
        ds.ReleaseResultSet( sql_lyr )

    for fieldname in ['intfield', 'int64field', 'floatfield', 'strfield']:
        sql_lyr = ds.ExecuteSQL( "select distinct %s from layer order by %s desc" % (fieldname, fieldname))
        feat = sql_lyr.GetNextFeature()
        if feat.IsFieldSet(0) == 0:
            gdaltest.post_reason('fail')
            print('field %s' % fieldname)
            feat.DumpReadable()
            return 'fail'
        feat = sql_lyr.GetNextFeature()
        if feat.IsFieldSet(0) != 0:
            gdaltest.post_reason('fail')
            print('field %s' % fieldname)
            feat.DumpReadable()
            return 'fail'
        ds.ReleaseResultSet( sql_lyr )
        
    return 'success'

###############################################################################
# Test select count([distinct] column) with null values (#4354)

def ogr_sql_37():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('ogr_sql_37')
    lyr = ds.CreateLayer('layer')
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('floatfield', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('strfield', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('strfield_first_null', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('strfield_never_set', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('intfield_never_set', ogr.OFTInteger))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1)
    feat.SetField(2, "456")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1)
    feat.SetField(2, "456")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(1, 2.3)
    feat.SetField('strfield_first_null', "foo")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(1, 2.3)
    lyr.CreateFeature(feat)
    feat = None

    for fieldname in ['intfield', 'floatfield', 'strfield']:
        sql_lyr = ds.ExecuteSQL( "select count(%s), count(distinct %s), count(*) from layer" % (fieldname, fieldname))
        feat = sql_lyr.GetNextFeature()
        if feat.GetFieldAsInteger(0) != 2:
            gdaltest.post_reason('fail')
            print('field %s' % fieldname)
            feat.DumpReadable()
            return 'fail'

        if feat.GetFieldAsInteger(1) != 1:
            gdaltest.post_reason('fail')
            print('field %s' % fieldname)
            feat.DumpReadable()
            return 'fail'

        if feat.GetFieldAsInteger(2) != 4:
            gdaltest.post_reason('fail')
            print('field %s' % fieldname)
            feat.DumpReadable()
            return 'fail'

        ds.ReleaseResultSet( sql_lyr )

    sql_lyr = ds.ExecuteSQL( "select avg(intfield) from layer where intfield is null")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSet(0) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet( sql_lyr )

    # Fix crash when first values is null (#4509)
    sql_lyr = ds.ExecuteSQL( "select distinct strfield_first_null from layer")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSet('strfield_first_null'):
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat.GetFieldAsString('strfield_first_null') != 'foo':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet( sql_lyr )

    sql_lyr = ds.ExecuteSQL( "select distinct strfield_never_set from layer")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSet('strfield_never_set'):
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet( sql_lyr )

    sql_lyr = ds.ExecuteSQL( "select min(intfield_never_set), max(intfield_never_set), avg(intfield_never_set), sum(intfield_never_set), count(intfield_never_set) from layer")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSet(0) or feat.IsFieldSet(1) or feat.IsFieldSet(2) or feat.IsFieldSet(3) or feat.GetField(4) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet( sql_lyr )

    return 'success'

###############################################################################
# Test "SELECT MAX(OGR_GEOM_AREA) FROM XXXX" (#4633)

def ogr_sql_38():

    sql_lyr = gdaltest.ds.ExecuteSQL( "SELECT MAX(OGR_GEOM_AREA) FROM poly" )

    feat = sql_lyr.GetNextFeature()
    val = feat.GetFieldAsDouble(0)

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if abs(val - 1634833.39062) < 1e-5:
        return 'success'
    else:
        print(val)
        return 'fail'

###############################################################################
# Test ORDER BY on a float special field

def ogr_sql_39():

    sql_lyr = gdaltest.ds.ExecuteSQL( "SELECT * FROM poly ORDER BY OGR_GEOM_AREA" )

    feat = sql_lyr.GetNextFeature()
    val = feat.GetFieldAsDouble(0)

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if abs(val - 5268.813) < 1e-5:
        return 'success'
    else:
        print(val)
        return 'fail'

###############################################################################
# Test ORDER BY on a int special field

def ogr_sql_40():

    sql_lyr = gdaltest.ds.ExecuteSQL( "SELECT * FROM poly ORDER BY FID DESC" )

    feat = sql_lyr.GetNextFeature()

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if feat.GetFID() == 9:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test ORDER BY on a string special field

def ogr_sql_41():

    sql_lyr = gdaltest.ds.ExecuteSQL( "SELECT * FROM poly ORDER BY OGR_GEOMETRY" )

    feat = sql_lyr.GetNextFeature()

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if feat.GetFID() == 0:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test comparing to empty string

def ogr_sql_42():

    lyr = gdaltest.ds.GetLayerByName('poly')
    lyr.SetAttributeFilter("prfedea <> ''")
    feat = lyr.GetNextFeature()
    lyr.SetAttributeFilter(None)
    if feat is None:
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = gdaltest.ds.ExecuteSQL( "SELECT * FROM poly WHERE prfedea <> ''" )
    feat = sql_lyr.GetNextFeature()
    gdaltest.ds.ReleaseResultSet( sql_lyr )
    if feat is None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test escape sequences

def ogr_sql_43():

    ret = 'success'
    sql = "SELECT '\"' as a, '\\\'' as b, '\'\'' as c FROM poly"
    sql_lyr = gdaltest.ds.ExecuteSQL( sql )

    feat = sql_lyr.GetNextFeature()
    if feat['a'] != '"' or feat['b'] != '\'' or feat['c'] != '\'':
        ret = 'fail'

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    return ret

###############################################################################
# Test hstore_get_value()

def ogr_sql_44():

    # Invalid parameters
    for sql in ["SELECT hstore_get_value('a') FROM poly",
                "SELECT hstore_get_value(1, 1) FROM poly"]:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        sql_lyr = gdaltest.ds.ExecuteSQL( sql )
        gdal.PopErrorHandler()
        if sql_lyr is not None:
            gdaltest.post_reason('fail')
            print(sql)
            return 'fail'

    # Invalid hstore syntax or empty result
    for sql in [ "SELECT hstore_get_value('a', null) FROM poly",
                 "SELECT hstore_get_value(null, 'a') FROM poly",
                 "SELECT hstore_get_value('a', 'a') FROM poly",
                 "SELECT hstore_get_value('a=>b', 'c') FROM poly",
                 "SELECT hstore_get_value('a=>', 'a') FROM poly",
                 "SELECT hstore_get_value(' a => ', 'a') FROM poly",
                 "SELECT hstore_get_value('a=>b,z,c=>d', 'c') FROM poly",
                 "SELECT hstore_get_value('\"a', 'a') FROM poly",
                 "SELECT hstore_get_value('\"a\"', 'a') FROM poly",
                 "SELECT hstore_get_value('\"a\"=', 'a') FROM poly",
                 "SELECT hstore_get_value('\"a\" =>', 'a') FROM poly",
                 "SELECT hstore_get_value('\"a\" => ', 'a') FROM poly",
                 "SELECT hstore_get_value('\"a\" => \"', 'a') FROM poly",
                 "SELECT hstore_get_value('\"a\" => \"\" z', 'a') FROM poly" ]:
        sql_lyr = gdaltest.ds.ExecuteSQL( sql )
        f = sql_lyr.GetNextFeature()
        if f.IsFieldSet(0):
            gdaltest.post_reason('fail')
            print(sql)
            f.DumpReadable()
            return 'fail'
        gdaltest.ds.ReleaseResultSet( sql_lyr )

    # Valid hstore syntax
    for (sql, expected) in [ ("SELECT hstore_get_value('a=>b', 'a') FROM poly", 'b'),
                             ("SELECT hstore_get_value(' a => b ', 'a') FROM poly", 'b'),
                             ("SELECT hstore_get_value('\"a\"=>b', 'a') FROM poly", 'b'),
                             ("SELECT hstore_get_value(' \"a\" =>b', 'a') FROM poly", 'b'),
                             ("SELECT hstore_get_value('a=>\"b\"', 'a') FROM poly", 'b'),
                             ("SELECT hstore_get_value('a=> \"b\" ', 'a') FROM poly", 'b'),
                             ("SELECT hstore_get_value('\"a\"=>\"b\"', 'a') FROM poly", 'b'),
                             ("SELECT hstore_get_value(' \"a\" => \"b\" ', 'a') FROM poly", 'b'),
                             ("SELECT hstore_get_value(' \"a\\\"b\" => \"b\" ', 'a\"b') FROM poly", 'b')]:
        sql_lyr = gdaltest.ds.ExecuteSQL( sql )
        f = sql_lyr.GetNextFeature()
        if f.GetField(0) != expected:
            gdaltest.post_reason('fail')
            print(sql)
            f.DumpReadable()
            return 'fail'
        gdaltest.ds.ReleaseResultSet( sql_lyr )

    return 'success'

###############################################################################
# Test 64 bit GetFeatureCount()

def ogr_sql_45():

    ds = ogr.Open("""<OGRVRTDataSource>
  <OGRVRTLayer name="poly">
    <SrcDataSource relativeToVRT="0" shared="1">data/poly.shp</SrcDataSource>
    <SrcLayer>poly</SrcLayer>
    <GeometryType>wkbPolygon</GeometryType>
    <Field name="AREA" type="Real" src="AREA"/>
    <Field name="EAS_ID" type="Integer" src="EAS_ID"/>
    <Field name="PRFEDEA" type="Integer" src="PRFEDEA"/>
    <FeatureCount>1000000000000</FeatureCount>
  </OGRVRTLayer>
</OGRVRTDataSource>""")
    lyr = ds.GetLayer(0)

    if lyr.GetFeatureCount() != 1000000000000:
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = ds.ExecuteSQL('SELECT COUNT(*) FROM poly')
    f = sql_lyr.GetNextFeature()
    if f.GetField(0) != 1000000000000:
        gdaltest.post_reason('fail')
        return 'fail'
    f = None
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL('SELECT COUNT(AREA) FROM poly')
    if sql_lyr.GetLayerDefn().GetFieldDefn(0).GetType() != ogr.OFTInteger:
        gdaltest.post_reason('fail')
        return 'fail'
    f = sql_lyr.GetNextFeature()
    if f.GetField(0) != 10:
        gdaltest.post_reason('fail')
        return 'fail'
    f = None
    ds.ReleaseResultSet(sql_lyr)

    return 'success'

###############################################################################
# Test strict SQL quoting

def ogr_sql_46():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('test')
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('from', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1)
    feat.SetField(1, "not_from")
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 3)
    feat.SetField(1, "from")
    lyr.CreateFeature(feat)
    feat = None

    sql_lyr = ds.ExecuteSQL( "select id, 'id', \"id\" as id2, id as \"id3\", \"from\" from test where \"from\" = 'from'" )
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 3 or feat.GetField(1) != 'id' or feat.GetField(2) != 3 or feat.GetField(3) != 3 or feat.GetField(4) != 'from':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL( "select max(\"id\"), max(id), count(\"id\"), count(id) from \"test\"" )
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 3 or feat.GetField(1) != 3 or feat.GetField(2) != 2 or feat.GetField(3) != 2:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    # Not accepted
    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL( "select * from 'test'" )
    gdal.PopErrorHandler()
    if sql_lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Not accepted
    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL( "select distinct 'id' from 'test'" )
    gdal.PopErrorHandler()
    if sql_lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Not accepted
    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL( "select max('id') from 'test'" )
    gdal.PopErrorHandler()
    if sql_lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Not accepted
    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL( "select id as 'id2' from 'test'" )
    gdal.PopErrorHandler()
    if sql_lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'
        
    return 'success'

def ogr_sql_cleanup():
    gdaltest.lyr = None
    gdaltest.ds = None

    return 'success'


gdaltest_list = [ 
    ogr_sql_1,
    ogr_sql_2,
    ogr_sql_3,
    ogr_sql_3_desc,
    ogr_sql_4,
    ogr_sql_5,
    ogr_sql_6,
    ogr_sql_7,
    ogr_sql_8,
    ogr_sql_9,
    ogr_sql_10,
    ogr_sql_11,
    ogr_sql_12,
    ogr_sql_13,
    ogr_sql_14,
    ogr_sql_15,
    ogr_sql_16,
    ogr_sql_17,
    ogr_sql_18,
    ogr_sql_19,
    ogr_sql_20,
    ogr_sql_21,
    ogr_sql_22,
    ogr_sql_23,
    ogr_sql_24,
    ogr_sql_25,
    ogr_sql_26,
    ogr_sql_27,
    ogr_sql_28,
    ogr_sql_29,
    ogr_sql_30,
    ogr_sql_31,
    ogr_sql_32,
    ogr_sql_33,
    ogr_sql_34,
    ogr_sql_35,
    ogr_sql_36,
    ogr_sql_37,
    ogr_sql_38,
    ogr_sql_39,
    ogr_sql_40,
    ogr_sql_41,
    ogr_sql_42,
    ogr_sql_43,
    ogr_sql_44,
    ogr_sql_45,
    ogr_sql_46,
    ogr_sql_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_sql_test' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
