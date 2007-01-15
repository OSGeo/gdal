#!/usr/bin/env python
###############################################################################
# $Id: ogr_join_test.py,v 1.3 2003/03/20 19:04:03 warmerda Exp $
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
# 
#  $Log: ogr_join_test.py,v $
#  Revision 1.3  2003/03/20 19:04:03  warmerda
#  added double join test
#
#  Revision 1.2  2003/03/19 20:39:23  warmerda
#  added test for external datasources
#
#  Revision 1.1  2003/03/19 06:19:57  warmerda
#  New
#
#

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

    sql_layer = gdaltest.ds.ExecuteSQL(
        'SELECT * FROM poly LEFT JOIN idlink ON poly.eas_id = idlink.eas_id' )

    count = sql_layer.GetFeatureCount()
    if count != 10:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 10' % count )
        return 'fail'

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
    ogr_join_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_join_test' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

