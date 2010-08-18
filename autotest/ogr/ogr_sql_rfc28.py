#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id: ogr_sql_test.py 19725 2010-05-15 13:39:49Z winkey $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR SQL capabilities added as part of RFC 28 implementation.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
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

from osgeo import gdal, ogr
import gdaltest
import ogrtest

###############################################################################
# Test an expression with a left side value and right side column and an \
# expression for the value.

def ogr_rfc28_1():
    gdaltest.ds = ogr.Open( 'data' )
    gdaltest.lyr = gdaltest.ds.GetLayerByName( 'poly' )

    gdaltest.lyr.SetAttributeFilter( '160+7 > eas_id' )

    count = gdaltest.lyr.GetFeatureCount()
    if count != 3:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 3' % count )
        return 'fail'

    return 'success'

###############################################################################
# Test CONCAT operator in the context of a WHERE clause.

def ogr_rfc28_2():
    gdaltest.lyr.SetAttributeFilter( "CONCAT('x',PRFEDEA) = 'x35043423'" )

    count = gdaltest.lyr.GetFeatureCount()
    if count != 1:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 1' % count )
        return 'fail'

    gdaltest.lyr.SetAttributeFilter( '' )
    return 'success'

###############################################################################
# Test '+' operator on strings.

def ogr_rfc28_3():
    gdaltest.lyr.SetAttributeFilter( "'x'+PRFEDEA = 'x35043423'" )

    count = gdaltest.lyr.GetFeatureCount()
    if count != 1:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 1' % count )
        return 'fail'

    gdaltest.lyr.SetAttributeFilter( '' )
    return 'success'

###############################################################################
# Test '%' operator.

def ogr_rfc28_4():
    gdaltest.lyr.SetAttributeFilter( "EAS_ID % 5 = 1" )

    count = gdaltest.lyr.GetFeatureCount()
    if count != 2:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 2' % count )
        return 'fail'

    gdaltest.lyr.SetAttributeFilter( '' )
    return 'success'

###############################################################################
# Test '%' operator.

def ogr_rfc28_5():
    gdaltest.lyr.SetAttributeFilter( "EAS_ID % 5 = 1" )

    count = gdaltest.lyr.GetFeatureCount()
    if count != 2:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 2' % count )
        return 'fail'

    gdaltest.lyr.SetAttributeFilter( '' )
    return 'success'

###############################################################################
# Test support for a quoted field name.

def ogr_rfc28_6():
    gdaltest.lyr.SetAttributeFilter( "'EAS_ID' = 166" )

    count = gdaltest.lyr.GetFeatureCount()
    if count != 1:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 1' % count )
        return 'fail'

    gdaltest.lyr.SetAttributeFilter( '' )
    return 'success'

###############################################################################
# test with distinguished name for field in where clause.

def ogr_rfc28_7():
    ql = gdaltest.ds.ExecuteSQL( "select eas_id from idlink where 'idlink.eas_id' = 166" )
    
    count = ql.GetFeatureCount()
    if count != 1:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 1' % count )
        return 'fail'

    gdaltest.ds.ReleaseResultSet( ql )
    return 'success'

###############################################################################
# test with distinguished name for field in target columns.

def ogr_rfc28_8():
    ql = gdaltest.ds.ExecuteSQL( "select 'idlink.eas_id' from idlink where 'idlink.eas_id' = 166" )
    
    count = ql.GetFeatureCount()
    if count != 1:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 1' % count )
        return 'fail'

    expect = [ 166 ]
    tr = ogrtest.check_features_against_list( ql, 'idlink.eas_id', expect )
    
    gdaltest.ds.ReleaseResultSet( ql )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test with quoted funky (non-identifier) name.

def ogr_rfc28_9():
    ds = ogr.Open( 'data/oddname.csv')
    lyr = ds.GetLayer(0)
    lyr.SetAttributeFilter( "'Funky @Name' = '32'" )

    count = lyr.GetFeatureCount()
    if count != 1:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 1' % count )
        return 'fail'

    expect = [ '8902' ]
    tr = ogrtest.check_features_against_list( lyr, 'PRIME_MERIDIAN_CODE', expect )
    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# test quoted names for funky columns in SELECT WHERE (confirm unparse quoting)

def ogr_rfc28_10():
    ds = ogr.Open( 'data/oddname.csv')
    lyr = ds.ExecuteSQL( "SELECT * from oddname where 'Funky @Name' = '32'" )

    count = lyr.GetFeatureCount()
    if count != 1:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 1' % count )
        return 'fail'

    expect = [ '8902' ]
    tr = ogrtest.check_features_against_list( lyr, 'PRIME_MERIDIAN_CODE', expect )
    ds.ReleaseResultSet( lyr )
    
    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# test quoted funky names in output columns list.

def ogr_rfc28_11():
    ds = ogr.Open( 'data/oddname.csv')
    lyr = ds.ExecuteSQL( "SELECT 'Funky @Name' from oddname where prime_meridian_code = '8902'" )

    count = lyr.GetFeatureCount()
    if count != 1:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 1' % count )
        return 'fail'

    expect = [ '32' ]
    tr = ogrtest.check_features_against_list( lyr, 'Funky @Name', expect )
    ds.ReleaseResultSet( lyr )
    
    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# test selecting fixed string fields.

def ogr_rfc28_12():
    lyr = gdaltest.ds.ExecuteSQL( "SELECT 'constant string', 'other' as abc, eas_id from idlink where eas_id = 165" )

    count = lyr.GetFeatureCount()
    if count != 1:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 1' % count )
        return 'fail'

    expect = [ 'other' ]
    tr = ogrtest.check_features_against_list( lyr, 'abc', expect )
    
    expect = [ 165 ]
    if tr:
        lyr.ResetReading()
        tr = ogrtest.check_features_against_list( lyr, 'eas_id', expect )
    
    expect = [ 'constant string' ]
    if tr:
        lyr.ResetReading()
        tr = ogrtest.check_features_against_list( lyr, 'field_1', expect )
    
    gdaltest.ds.ReleaseResultSet( lyr )
    
    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test SUBSTR operator in the context of a WHERE clause.

def ogr_rfc28_13():
    gdaltest.lyr.SetAttributeFilter( "SUBSTR(PRFEDEA,4,4) = '3423'" )

    count = gdaltest.lyr.GetFeatureCount()
    if count != 1:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 1' % count )
        return 'fail'

    gdaltest.lyr.SetAttributeFilter( '' )
    return 'success'

###############################################################################
# test selecting fixed string fields.

def ogr_rfc28_14():
    lyr = gdaltest.ds.ExecuteSQL( "SELECT SUBSTR(PRFEDEA,3,5) from poly where eas_id in (168,179)" )

    expect = [ '43411', '43423' ]
    tr = ogrtest.check_features_against_list( lyr, 'prfedea', expect )

    gdaltest.ds.ReleaseResultSet( lyr )
    
    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test CONCAT with more than two arguments.

def ogr_rfc28_15():
    lyr = gdaltest.ds.ExecuteSQL( "SELECT CONCAT(PRFEDEA,' ',CAST(EAS_ID AS CHARACTER(3))) from poly where eas_id in (168,179)" )

    expect = [ '35043411 168', '35043423 179' ]
    tr = ogrtest.check_features_against_list( lyr, 'prfedea', expect )

    gdaltest.ds.ReleaseResultSet( lyr )
    
    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
def ogr_rfc28_cleanup():
    gdaltest.lyr = None
    gdaltest.ds.Destroy()
    gdaltest.ds = None

    return 'success'


gdaltest_list = [
    ogr_rfc28_1,
    ogr_rfc28_2,
    ogr_rfc28_3,
    ogr_rfc28_4,
    ogr_rfc28_5,
    ogr_rfc28_6,
    ogr_rfc28_7,
    ogr_rfc28_8,
    ogr_rfc28_9,
    ogr_rfc28_10,
    ogr_rfc28_11,
    ogr_rfc28_12,
    ogr_rfc28_13,
    ogr_rfc28_14,
    ogr_rfc28_15,
    ogr_rfc28_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_sql_rfc28' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

