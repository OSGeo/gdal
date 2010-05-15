#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR JOIN support.
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

import gdaltest
import ogr
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
    ogr_join_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_join_test' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

