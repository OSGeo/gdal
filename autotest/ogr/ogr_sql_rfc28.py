#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
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
    gdaltest.lyr.SetAttributeFilter( "SUBSTR(PRFEDEA,5,4) = '3423'" )

    count = gdaltest.lyr.GetFeatureCount()
    if count != 1:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 1' % count )
        return 'fail'

    gdaltest.lyr.SetAttributeFilter( '' )
    return 'success'

###############################################################################
# test selecting fixed string fields.

def ogr_rfc28_14():
    lyr = gdaltest.ds.ExecuteSQL( "SELECT SUBSTR(PRFEDEA,4,5) from poly where eas_id in (168,179)" )

    expect = [ '43411', '43423' ]
    tr = ogrtest.check_features_against_list( lyr, 'substr_prfedea', expect )

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
    tr = ogrtest.check_features_against_list( lyr, 'concat_prfedea', expect )

    gdaltest.ds.ReleaseResultSet( lyr )
    
    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test parse support for negative numbers (#3724)

def ogr_rfc28_16():
    lyr = gdaltest.ds.ExecuteSQL( "SELECT -1, 3--1,3*-1,2e-1,3-1 from poly where eas_id = 168" )

    expect = [ -1 ]
    tr = ogrtest.check_features_against_list( lyr, 'field_1', expect )

    expect = [ 4 ]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list( lyr, 'field_2', expect )

    expect = [ -3 ]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list( lyr, 'field_3', expect )

    expect = [ 0.2 ]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list( lyr, 'field_4', expect )

    expect = [ 2 ]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list( lyr, 'field_5', expect )

    gdaltest.ds.ReleaseResultSet( lyr )
    
    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test evaluation of division - had a problem with type conversion.

def ogr_rfc28_17():
    lyr = gdaltest.ds.ExecuteSQL( "SELECT 5/2, 5.0/2.0, 5/2.0, 5.0/2 from poly where eas_id = 168" )

    expect = [ 2 ]
    tr = ogrtest.check_features_against_list( lyr, 'field_1', expect )

    expect = [ 2.5 ]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list( lyr, 'field_2', expect )

    expect = [ 2.5 ]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list( lyr, 'field_3', expect )

    expect = [ 2.5 ]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list( lyr, 'field_4', expect )

    gdaltest.ds.ReleaseResultSet( lyr )
    
    if tr:
        return 'success'
    else:
        return 'fail'


###############################################################################
# Test some special distinct cases.

def ogr_rfc28_18():
    lyr = gdaltest.ds.ExecuteSQL( "SELECT COUNT(distinct id), COUNT(distinct id) as 'xx' from departs" )

    expect = [ 1 ]
    tr = ogrtest.check_features_against_list( lyr, 'COUNT_id', expect )

    expect = [ 1 ]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list( lyr, 'xx', expect )

    gdaltest.ds.ReleaseResultSet( lyr )
    
    if tr:
        return 'success'
    else:
        return 'fail'


###############################################################################
# Verify that NOT IN ( list ) works

def ogr_rfc28_19():

    sql_lyr = gdaltest.ds.ExecuteSQL( 'select * from poly where eas_id not in (158,165)' )

    count = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if count != 8:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 8' % count )
        return 'fail'

    return 'success'


###############################################################################
# Verify arithmetic operator precedence and unary minus

def ogr_rfc28_20():

    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
    lyr = ds.CreateLayer( "my_layer")
    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 2)
    lyr.CreateFeature(feat)

    sql_lyr = ds.ExecuteSQL( 'select -intfield + 1 + 2 * 3 + 5 - 3 * 2 from my_layer' )
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('FIELD_1') != 4:
        return 'fail'
    ds.ReleaseResultSet( sql_lyr )

    ds = None

    return 'success'

###############################################################################
# Verify that BETWEEN works

def ogr_rfc28_21():

    sql_lyr = gdaltest.ds.ExecuteSQL( 'select * from poly where eas_id between 165 and 169' )

    count_between = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    sql_lyr = gdaltest.ds.ExecuteSQL( 'select * from poly where eas_id >= 165 and eas_id <= 169' )

    count_ge_and_le = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if count_between != count_ge_and_le:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting %d' % (count_between, count_ge_and_le) )
        return 'fail'

    return 'success'

###############################################################################
# Verify that NOT BETWEEN works

def ogr_rfc28_22():

    sql_lyr = gdaltest.ds.ExecuteSQL( 'select * from poly where eas_id not between 165 and 169' )

    count_not_between = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    sql_lyr = gdaltest.ds.ExecuteSQL( 'select * from poly where not(eas_id >= 165 and eas_id <= 169)' )

    count_not_ge_and_le = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if count_not_between != count_not_ge_and_le:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting %d' % (count_not_between, count_not_ge_and_le) )
        return 'fail'

    return 'success'

###############################################################################
# Verify that NOT LIKE works

def ogr_rfc28_23():

    sql_lyr = gdaltest.ds.ExecuteSQL( "select * from poly where PRFEDEA NOT LIKE '35043413'" )

    count_not_like1 = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    sql_lyr = gdaltest.ds.ExecuteSQL( "select * from poly where NOT (PRFEDEA LIKE '35043413')" )

    count_not_like2 = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if count_not_like1 != count_not_like2:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting %d' % (count_not_like1, count_not_like2) )
        return 'fail'

    return 'success'

###############################################################################
# Verify that NULL works

def ogr_rfc28_24():

    sql_lyr = gdaltest.ds.ExecuteSQL( "select *, NULL, NULL as nullstrfield, CAST(null as integer) as nullintfield from poly where NULL IS NULL" )

    feat = sql_lyr.GetNextFeature()

    if feat.IsFieldSet('FIELD_4'):
        feat.DumpReadable()
        gdaltest.ds.ReleaseResultSet( sql_lyr )
        return 'fail'

    if feat.IsFieldSet('nullstrfield'):
        feat.DumpReadable()
        gdaltest.ds.ReleaseResultSet( sql_lyr )
        return 'fail'

    if feat.IsFieldSet('nullintfield'):
        feat.DumpReadable()
        gdaltest.ds.ReleaseResultSet( sql_lyr )
        return 'fail'

    count = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if count != 10:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting %d' % (count, 10) )
        return 'fail'

    return 'success'

###############################################################################
# Verify that LIKE pattern ESCAPE escape_char works

def ogr_rfc28_25():

    sql_lyr = gdaltest.ds.ExecuteSQL( "select * from poly where prfedea LIKE 'x35043408' ESCAPE 'x'" )

    count = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet( sql_lyr )

    if count != 1:
        gdaltest.post_reason( 'Got wrong count with GetFeatureCount() - %d, expecting 1' % count )
        return 'fail'

    return 'success'

###############################################################################
# Test SUBSTR with negative offsets

def ogr_rfc28_26():
    lyr = gdaltest.ds.ExecuteSQL( "SELECT SUBSTR(PRFEDEA,-2) from poly where eas_id in (168,179)" )

    expect = [ '11', '23' ]
    tr = ogrtest.check_features_against_list( lyr, 'substr_prfedea', expect )

    gdaltest.ds.ReleaseResultSet( lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test that we correctly let floating point values as floating point, and not as integer (#4634)"

def ogr_rfc28_27():

    lyr = gdaltest.ds.ExecuteSQL( "SELECT * FROM poly WHERE 4000000000. > 2000000000." )

    count = lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet( lyr )

    if count == 10:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Extensive test of the evaluation of arithmetic and logical operators

def ogr_rfc28_28_test_boolean(formula, expected_bool):
    lyr = gdaltest.ds.ExecuteSQL( "SELECT * from poly where fid = 0 and " + formula )
    count = lyr.GetFeatureCount()
    gdaltest.ds.ReleaseResultSet( lyr )

    if expected_bool:
        expected_count = 1
    else:
        expected_count = 0

    if count != expected_count:
        gdaltest.post_reason('bad result for %s : %d' % (formula, count))
        return 'fail'

    return 'success'

def ogr_rfc28_28():

    operators = [ '+', '-', '*', '/', '%' ]
    formulas = []
    for operator in operators:
        formulas.append( '6' + operator + '3' )
        formulas.append( '5.' + operator + '3.' )
        formulas.append( '5' + operator + '3.' )
        formulas.append( '5.' + operator + '3' )

    for formula in formulas:
        lyr = gdaltest.ds.ExecuteSQL( "SELECT " + formula + " from poly where fid = 0" )
        expect = [ eval(formula) ]
        tr = ogrtest.check_features_against_list( lyr, 'field_1', expect )
        gdaltest.ds.ReleaseResultSet( lyr )

        if tr == 0:
            gdaltest.post_reason('bad result for %s' % formula)
            return 'fail'

    operators = [ '<', '<=', '>', '>=', ' = ', '<>' ]
    formulas = []
    for operator in operators:
        formulas.append( '3' + operator + '3' )
        formulas.append( '3.' + operator + '3.' )
        formulas.append( '3' + operator + '6' )
        formulas.append( '3.' + operator + '6.' )
        formulas.append( '3' + operator + '6.' )
        formulas.append( '3.' + operator + '6' )
        formulas.append( '6' + operator + '3' )
        formulas.append( '6.' + operator + '3.' )
        formulas.append( '6' + operator + '3.' )
        formulas.append( '6.' + operator + '3' )
        formulas.append( "'a'" + operator + "'a'" )
        formulas.append( "'a'" + operator + "'b'" )
        formulas.append( "'b'" + operator + "'a'" )

    for formula in formulas:
        expected_bool = eval(formula.replace(' = ','==').replace('<>','!='))
        ret = ogr_rfc28_28_test_boolean(formula, expected_bool)
        if ret == 'fail':
            return ret

    formulas_and_expected_bool = [ [ '3 in (3,5)', True ],
                                   [ '4 in (3,5)', False ],
                                   [ '3. in (3.,4.)', True ],
                                   [ '4. in (3.,5.)', False ],
                                   [ "'c' in ('c','e')", True ],
                                   [ "'d' in ('c','e')", False ],
                                   [ '2 between 2 and 4', True ],
                                   [ '3 between 2 and 4', True ],
                                   [ '4 between 2 and 4', True ],
                                   [ '1 between 2 and 4', False ],
                                   [ '5 between 2 and 4', False ],
                                   [ '2. between 2. and 4.', True ],
                                   [ '3. between 2. and 4.', True ],
                                   [ '4. between 2. and 4.', True ],
                                   [ '1. between 2. and 4.', False ],
                                   [ '5. between 2. and 4.', False ],
                                   [ "'b' between 'b' and 'd'", True ],
                                   [ "'c' between 'b' and 'd'", True ],
                                   [ "'d' between 'b' and 'd'", True ],
                                   [ "'a' between 'b' and 'd'", False ],
                                   [ "'e' between 'b' and 'd'", False ] ]

    for [formula, expected_bool] in formulas_and_expected_bool:
        ret = ogr_rfc28_28_test_boolean(formula, expected_bool)
        if ret == 'fail':
            return ret

    return 'success'

###############################################################################
# Test behaviour of binary operations when one operand is a NULL value

def ogr_rfc28_29():

    lyr = gdaltest.ds.ExecuteSQL( "select * from idlink where (eas_id + cast(null as integer)) is not null or eas_id = 170 + cast(null as integer) or (eas_id + cast(null as float)) is not null or eas_id = 170.0 + cast(null as float)" )

    count = lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet( lyr )

    if count == 0:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test behaviour of binary operations on strings when one operand is a NULL value

def ogr_rfc28_30():

    lyr = gdaltest.ds.ExecuteSQL( "select * from idlink2 where F1 <> 'foo' or concat(F1,cast(null as character(32))) is not null" )

    count = lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet( lyr )

    if count == 0:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test UNION ALL

def ogr_rfc28_31():

    lyr = gdaltest.ds.ExecuteSQL( "select * from idlink union all select * from idlink2" )

    count = lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet( lyr )

    if count != 6 + 7:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test UNION ALL with parenthesis

def ogr_rfc28_32():

    lyr = gdaltest.ds.ExecuteSQL( "(select * from idlink) union all (select * from idlink2 order by eas_id)" )

    count = lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet( lyr )

    if count != 6 + 7:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test lack of end-of-string character

def ogr_rfc28_33():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL( "select * from idlink where name='foo" )
    gdal.PopErrorHandler()

    if lyr is None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test wildchar expension of an unknown table

def ogr_rfc28_34():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL( "select foo.* from idlink" )
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg().find('Table foo not recognised from foo.* definition') != 0:
        print(gdal.GetLastErrorMsg())
        return 'fail'

    if lyr is None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test selecting more than one distinct

def ogr_rfc28_35():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL( "select distinct eas_id, distinct name from idlink" )
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg().find('SELECTing more than one DISTINCT') != 0:
        print(gdal.GetLastErrorMsg())
        return 'fail'

    if lyr is None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test ORDER BY a DISTINCT list by more than one key

def ogr_rfc28_36():

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL( "select distinct eas_id from idlink order by eas_id, name" )
    if lyr is not None:
        lyr.GetNextFeature()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg().find("Can't ORDER BY a DISTINCT list by more than one key") != 0:
        print(gdal.GetLastErrorMsg())
        return 'fail'
    gdaltest.ds.ReleaseResultSet( lyr )
    return 'success'

###############################################################################
# Test different fields for ORDER BY and DISTINCT

def ogr_rfc28_37():

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL( "select distinct eas_id from idlink order by name" )
    if lyr is not None:
        lyr.GetNextFeature()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg().find("Only selected DISTINCT field can be used for ORDER BY") != 0:
        print(gdal.GetLastErrorMsg())
        return 'fail'
    gdaltest.ds.ReleaseResultSet( lyr )
    return 'success'

###############################################################################
# Test invalid SUBSTR 

def ogr_rfc28_38():

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL( "SELECT SUBSTR(PRFEDEA) from poly" )
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg().find("Expected 2 or 3 arguments to SUBSTR(), but got 1") != 0:
        print(gdal.GetLastErrorMsg())
        return 'fail'
    if lyr is not None:
        return 'fail'

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL( "SELECT SUBSTR(1,2) from poly" )
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg().find("Wrong argument type for SUBSTR()") != 0:
        print(gdal.GetLastErrorMsg())
        return 'fail'
    if lyr is not None:
        return 'fail'

    return 'success'

###############################################################################
# Test COUNT() on a 0-row result

def ogr_rfc28_39():

    lyr = gdaltest.ds.ExecuteSQL( "SELECT COUNT(*) from poly where 0 = 1" )

    tr = ogrtest.check_features_against_list( lyr, 'count_*', [0] )

    gdaltest.ds.ReleaseResultSet( lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test MIN(), MAX() and AVG() on a date (#5333)

def ogr_rfc28_40():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('DATE', ogr.OFTDateTime))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, '2013/12/31 23:59:59')
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, '2013/01/01 00:00:00')
    lyr.CreateFeature(feat)
    lyr = ds.ExecuteSQL( "SELECT MIN(DATE), MAX(DATE), AVG(DATE) from test" )

    tr = ogrtest.check_features_against_list( lyr, 'MIN_DATE', ['2013/01/01 00:00:00'] )
    lyr.ResetReading()
    tr2 = ogrtest.check_features_against_list( lyr, 'MAX_DATE', ['2013/12/31 23:59:59'] )
    lyr.ResetReading()
    tr3 = ogrtest.check_features_against_list( lyr, 'AVG_DATE', ['2013/07/02 11:59:59'] )

    gdaltest.ds.ReleaseResultSet( lyr )

    if not tr:
        return 'fail'

    if not tr2:
        return 'fail'

    if not tr3:
        return 'fail'
    return 'success'


###############################################################################
# Verify that SELECT * works on a layer with a field that has a dot character (#5379)

def ogr_rfc28_41():

    ds = ogr.GetDriverByName("Memory").CreateDataSource( "my_ds")
    lyr = ds.CreateLayer( "my_layer")
    field_defn = ogr.FieldDefn('a.b', ogr.OFTInteger)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 2)
    lyr.CreateFeature(feat)

    sql_lyr = ds.ExecuteSQL( 'select * from my_layer' )
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('a.b') != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet( sql_lyr )

    sql_lyr = ds.ExecuteSQL( 'select l.* from my_layer l' )
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('l.a.b') != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet( sql_lyr )

    ds = None

    return 'success'

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
    ogr_rfc28_16,
    ogr_rfc28_17,
    ogr_rfc28_18,
    ogr_rfc28_19,
    ogr_rfc28_20,
    ogr_rfc28_21,
    ogr_rfc28_22,
    ogr_rfc28_23,
    ogr_rfc28_24,
    ogr_rfc28_25,
    ogr_rfc28_26,
    ogr_rfc28_27,
    ogr_rfc28_28,
    ogr_rfc28_29,
    ogr_rfc28_30,
    ogr_rfc28_31,
    ogr_rfc28_32,
    ogr_rfc28_33,
    ogr_rfc28_34,
    ogr_rfc28_35,
    ogr_rfc28_36,
    ogr_rfc28_37,
    ogr_rfc28_38,
    ogr_rfc28_39,
    ogr_rfc28_40,
    ogr_rfc28_41,
    ogr_rfc28_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_sql_rfc28' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

