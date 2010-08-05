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

import gdal
import gdaltest
import ogr
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

def ogr_sql_3():

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
# Test MAX() column function.

def ogr_sql_5():

    expect = [ 179 ]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 'select max(eas_id) from idlink' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'max_eas_id', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

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

    if fld_def.GetName() != 'block':
        gdaltest.post_reason( 'got wrong block field name' )
        print(fld_def.GetName())
        return 'fail'

    if fld_def.GetType() != ogr.OFTReal:
        gdaltest.post_reason( 'got wrong block field type' )
        print(fld_def.GetType())
        
    if fld_def.GetWidth() != 7:
        gdaltest.post_reason( 'got wrong block field width' )
        print(fld_def.GetWidth())
        
    if fld_def.GetPrecision() != 3:
        gdaltest.post_reason( 'got wrong block field precision' )
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

    name = 'data/departs.vrt'

    ds = ogr.Open( name )
    if ds is None:
        return 'fail'
    
    sql = 'select * from D\303\251parts'
    sql_lyr = ds.ExecuteSQL( sql )
    if sql_lyr is None:
        ds = None
        return 'fail'

    # XXX - Ticket #2221:
    # Field name returned consists of incorrectly encoding characters
    # so its not possible to compare it against fld_name value
    #fld_name = u'nomd\303\251part'
    #fld_def = sql_lyr.GetLayerDefn().GetFieldDefn(1)

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
    mem_lyr = mem_ds.CreateLayer( "my_layer")

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
#def ogr_sql_26():
#
#    mem_ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
#    mem_lyr = mem_ds.CreateLayer( "my_layer")
#
#    feat = ogr.Feature(mem_lyr.GetLayerDefn() )
#    mem_lyr.CreateFeature( feat )
#
#    sql_lyr = mem_ds.ExecuteSQL("SELECT 'literal_value' AS my_column FROM my_layer")
#    if sql_lyr.GetFeatureCount() != 1:
#        return 'fail'
#    feat = sql_lyr.GetNextFeature()
#    if feat.GetFieldAsString('my_column') != 'literal_value':
#        return 'fail'
#    mem_ds.ReleaseResultSet(sql_lyr)
#
#    mem_ds = None
#
#    return 'success'

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

def ogr_sql_cleanup():
    gdaltest.lyr = None
    gdaltest.ds.Destroy()
    gdaltest.ds = None

    return 'success'


gdaltest_list = [ 
    ogr_sql_1,
    ogr_sql_2,
    ogr_sql_3,
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
#    ogr_sql_26,
    ogr_sql_27,
    ogr_sql_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_sql_test' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

