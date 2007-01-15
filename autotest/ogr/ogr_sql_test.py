#!/usr/bin/env python
###############################################################################
# $Id: ogr_sql_test.py,v 1.6 2006/12/04 01:54:55 fwarmerdam Exp $
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
# 
#  $Log: ogr_sql_test.py,v $
#  Revision 1.6  2006/12/04 01:54:55  fwarmerdam
#  Added brief tests for ogr_geometry, ogr_geom_wkt and ogr_style.
#
#  Revision 1.5  2006/09/22 05:51:10  fwarmerdam
#  added tests for distinct and max on empty dataset (bug 1298)
#
#  Revision 1.4  2005/08/03 13:38:55  fwarmerdam
#  added ilike test
#
#  Revision 1.3  2003/03/26 16:14:45  warmerda
#  added test for quoted table names
#
#  Revision 1.2  2003/03/19 06:20:15  warmerda
#  added an additional test of wildcard expansion
#
#  Revision 1.1  2003/03/05 05:05:10  warmerda
#  New
#

import os
import sys

sys.path.append( '../pymod' )

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
        'BRUSH(fc:#000000,bc:#ffffff,id:"mapinfo-brush-1.ogr-brush-1");PEN(w:1px,c:#000000,id:"mapinfo-pen-2.ogr-pen-0")',
        'BRUSH(fc:#000000,bc:#ffffff,id:"mapinfo-brush-1.ogr-brush-1");PEN(w:1px,c:#000000,id:"mapinfo-pen-2.ogr-pen-0")' ]

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
    ogr_sql_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_sql_test' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

