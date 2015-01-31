#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR JOIN support.
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

import gdaltest
from osgeo import gdal
from osgeo import ogr
import ogrtest

###############################################################################
# Test a join.

def ogr_join_1():
    gdaltest.ds = ogr.Open( 'data' )

    sql_lyr = gdaltest.ds.ExecuteSQL(
        'SELECT * FROM poly LEFT JOIN idlink ON poly.eas_id = idlink.eas_id' )

    count = sql_lyr.GetFeatureCount()
    if count != 10:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 10' % count )
        return 'fail'

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    return 'success'

###############################################################################
# Check the values we are actually getting back (restricting the search a bit)

def ogr_join_2():

    expect = ['_166_', '_158_', '_165_' ]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 	\
        'SELECT * FROM poly ' \
        + 'LEFT JOIN idlink ON poly.eas_id = idlink.eas_id ' \
        + 'WHERE eas_id < 168' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'NAME', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Try various naming convesions for the selected fields. 

def ogr_join_3():

    expect = ['_166_', '_158_', '_165_' ]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 	\
        'SELECT poly.area, idlink.* FROM poly ' \
        + 'LEFT JOIN idlink ON poly.eas_id = idlink.eas_id ' \
        + 'WHERE eas_id < 168' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'idlink.NAME', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Verify that records for which a join can't be found work ok.

def ogr_join_4():

    expect = ['_179_', '_171_', None, None ]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 	\
        'SELECT poly.*, name FROM poly ' \
        + 'LEFT JOIN idlink ON poly.eas_id = idlink.eas_id ' \
        + 'WHERE eas_id > 170' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'NAME', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Verify that table aliases work

def ogr_join_5():

    expect = [ 179, 171, 173, 172 ]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 	\
        'SELECT p.*, il.name FROM poly p ' \
        + 'LEFT JOIN idlink il ON p.eas_id = il.eas_id ' \
        + 'WHERE eas_id > 170' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'p.eas_id', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Again, ordering by a primary field.

def ogr_join_6():

    expect = [ 171, 172, 173, 179 ]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 	\
        'SELECT p.*, il.name FROM poly p ' \
        + 'LEFT JOIN idlink il ON p.eas_id = il.eas_id ' \
        + 'WHERE eas_id > 170 ORDER BY p.eas_id' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'p.eas_id', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test joining to an external datasource.

def ogr_join_7():

    expect = [ 171, 172, 173, 179 ]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 	\
        'SELECT p.*, il.name FROM poly p ' \
        + 'LEFT JOIN "data/idlink.dbf".idlink il ON p.eas_id = il.eas_id ' \
        + 'WHERE eas_id > 170 ORDER BY p.eas_id' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'p.eas_id', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test doing two joins at once.

def ogr_join_8():

    expect = [ 171, None, None, 179 ]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 	\
        'SELECT p.*, il.name, il2.eas_id FROM poly p ' \
        + 'LEFT JOIN "data/idlink.dbf".idlink il ON p.eas_id = il.eas_id ' \
        + 'LEFT JOIN idlink il2 ON p.eas_id = il2.eas_id ' \
        + 'WHERE eas_id > 170 ORDER BY p.eas_id' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'il2.eas_id', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Verify fix for #2788 (memory corruption on wildcard expansion in SQL request
# with join clauses)

def ogr_join_9():

    expect = [ 179, 171, 173, 172 ]

    sql_lyr = gdaltest.ds.ExecuteSQL( 	\
        'SELECT poly.* FROM poly ' \
        + 'LEFT JOIN idlink ON poly.eas_id = idlink.eas_id ' \
        + 'WHERE eas_id > 170' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'poly.EAS_ID', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################

def ogr_join_10():

    expect = [None,None,None,None,None,None,None,None,None,None]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 	\
        'SELECT * FROM poly ' \
        + 'LEFT JOIN idlink2 ON poly.eas_id = idlink2.name ' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'F3', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test join on string field

def ogr_join_11():

    expect = ['_168_','_179_','_171_','_170_','_165_','_158_','_166_']
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 	\
        'SELECT il.*, il2.* FROM idlink il LEFT JOIN idlink2 il2 ON il.NAME = il2.NAME' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'il2.NAME', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test fix for #4112 (join between 2 datasources)

def ogr_join_12():
    ds = ogr.Open( 'data/poly.shp' )

    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM poly LEFT JOIN 'data/idlink.dbf'.idlink ON poly.eas_id = idlink.eas_id" )

    count = sql_lyr.GetFeatureCount()
    if count != 10:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 10' % count )
        return 'fail'

    ds.ReleaseResultSet( sql_lyr )

    return 'success'
    
###############################################################################
# Test joining a float column with a string column (#4321)

def ogr_join_13():

    expect = ['_168_','_179_','_171_',None, None,None,'_166_','_158_','_165_','_170_']
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 	\
        'SELECT * FROM poly ' \
        + 'LEFT JOIN idlink2 ON poly.eas_id = idlink2.eas_id' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'name', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test joining a string column with a float column (#4321, actually addressed by #4259)

def ogr_join_14():

    expect = [168,179,171,170,165,158,166]
    
    sql_lyr = gdaltest.ds.ExecuteSQL( 	\
        'SELECT * FROM idlink2 ' \
        + 'LEFT JOIN poly ON idlink2.eas_id = poly.eas_id' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'poly.EAS_ID', expect )

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test multiple joins with expressions (#4521)

def ogr_join_15():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_join_14')
    lyr = ds.CreateLayer('first')
    ogrtest.quick_create_layer_def(lyr, [['id']])
    ogrtest.quick_create_feature(lyr, [ 'key' ], None)

    lyr = ds.CreateLayer('second')
    ogrtest.quick_create_layer_def(lyr, [['col1_2'],['id'],['col3_2']])
    ogrtest.quick_create_feature(lyr, [ 'a2', 'key', 'c2' ], None)

    lyr = ds.CreateLayer('third')
    ogrtest.quick_create_layer_def(lyr, [['col1_3'],['id'],['col3_3']])
    ogrtest.quick_create_feature(lyr, [ 'a3', 'key', 'c3' ], None)

    sql_lyr = ds.ExecuteSQL("SELECT concat(col3_2, ''), col3_2 FROM first JOIN second ON first.id = second.id JOIN third ON first.id = third.id")
    feat = sql_lyr.GetNextFeature()
    val1 = feat.GetFieldAsString(0)
    val2 = feat.GetFieldAsString(1)
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    from osgeo import gdal
    gdal.Unlink('/vsimem/ogr_join_14/first.csv')
    gdal.Unlink('/vsimem/ogr_join_14/second.csv')
    gdal.Unlink('/vsimem/ogr_join_14/third.csv')
    gdal.Unlink('/vsimem/ogr_join_14')

    if val1 != 'c2':
        gdaltest.post_reason('fail')
        print(val1)
        return 'fail'

    if val2 != 'c2':
        gdaltest.post_reason('fail')
        print(val2)
        return 'fail'

    return 'success'

###############################################################################
# Test non-support of a secondarytable.fieldname in a where clause

def ogr_join_16():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = gdaltest.ds.ExecuteSQL(   \
        'SELECT * FROM poly ' \
        + 'LEFT JOIN idlink ON poly.eas_id = idlink.eas_id ' \
        + 'WHERE idlink.name = \'_165\'' )
    gdal.PopErrorHandler()

    if gdal.GetLastErrorMsg().find('Cannot use field') != 0:
        return 'fail'

    if sql_lyr is None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test non-support of a secondarytable.fieldname in a order by clause

def ogr_join_17():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = gdaltest.ds.ExecuteSQL(   \
        'SELECT * FROM poly ' \
        + 'LEFT JOIN idlink ON poly.eas_id = idlink.eas_id ' \
        + 'ORDER BY name' )
    gdal.PopErrorHandler()

    if gdal.GetLastErrorMsg().find('Cannot use field') != 0:
        print(gdal.GetLastErrorMsg())
        return 'fail'

    if sql_lyr is None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test wrong order of fiels in ON

def ogr_join_18():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = gdaltest.ds.ExecuteSQL(   \
        'SELECT * FROM poly LEFT JOIN idlink ON idlink.eas_id = poly.eas_id'  )
    gdal.PopErrorHandler()

    if gdal.GetLastErrorMsg().find('Currently the primary key must come from the primary table in') != 0:
        print(gdal.GetLastErrorMsg())
        return 'fail'

    if sql_lyr is None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test unrecognized primary field

def ogr_join_19():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = gdaltest.ds.ExecuteSQL(   \
        'SELECT * FROM poly LEFT JOIN idlink ON poly.foo = idlink.eas_id'  )
    gdal.PopErrorHandler()

    if gdal.GetLastErrorMsg().find('Unrecognised primary field poly.foo in JOIN clause') != 0:
        print(gdal.GetLastErrorMsg())
        return 'fail'

    if sql_lyr is None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test unrecognized secondary field

def ogr_join_20():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = gdaltest.ds.ExecuteSQL(   \
        'SELECT * FROM poly LEFT JOIN idlink ON poly.eas_id = idlink.foo'  )
    gdal.PopErrorHandler()

    if gdal.GetLastErrorMsg().find('Unrecognised secondary field idlink.foo in JOIN clause') != 0:
        print(gdal.GetLastErrorMsg())
        return 'fail'

    if sql_lyr is None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test unexpected secondary table

def ogr_join_21():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = gdaltest.ds.ExecuteSQL(   \
        'SELECT p.*, il.name, il2.eas_id FROM poly p ' \
        + 'LEFT JOIN "data/idlink.dbf".idlink il ON p.eas_id = il2.eas_id ' \
        + 'LEFT JOIN idlink il2 ON p.eas_id = il2.eas_id' )
    gdal.PopErrorHandler()

    if gdal.GetLastErrorMsg().find('Currently the secondary key must come from the secondary table') != 0:
        print(gdal.GetLastErrorMsg())
        return 'fail'

    if sql_lyr is None:
        return 'success'
    else:
        return 'fail'

###############################################################################

def ogr_join_cleanup():
    gdaltest.lyr = None
    gdaltest.ds.Destroy()
    gdaltest.ds = None

    return 'success'

gdaltest_list = [ 
    ogr_join_1,
    ogr_join_2,
    ogr_join_3,
    ogr_join_4,
    ogr_join_5,
    ogr_join_6,
    ogr_join_7,
    ogr_join_8,
    ogr_join_9,
    ogr_join_10,
    ogr_join_11,
    ogr_join_12,
    ogr_join_13,
    ogr_join_14,
    ogr_join_15,
    ogr_join_16,
    ogr_join_17,
    ogr_join_18,
    ogr_join_19,
    ogr_join_20,
    ogr_join_21,
    ogr_join_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_join_test' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

