#!/usr/bin/env pytest
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
# Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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



from osgeo import gdal
from osgeo import ogr
import gdaltest
import ogrtest
import pytest

###############################################################################
# Test an expression with a left side value and right side column and an \
# expression for the value.


def test_ogr_rfc28_1():
    gdaltest.ds = ogr.Open('data')
    gdaltest.lyr = gdaltest.ds.GetLayerByName('poly')

    gdaltest.lyr.SetAttributeFilter('160+7 > eas_id')

    count = gdaltest.lyr.GetFeatureCount()
    assert count == 3, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 3' % count)

###############################################################################
# Test CONCAT operator in the context of a WHERE clause.


def test_ogr_rfc28_2():
    gdaltest.lyr.SetAttributeFilter("CONCAT('x',PRFEDEA) = 'x35043423'")

    count = gdaltest.lyr.GetFeatureCount()
    assert count == 1, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 1' % count)

    gdaltest.lyr.SetAttributeFilter('')

###############################################################################
# Test '+' operator on strings.


def test_ogr_rfc28_3():
    gdaltest.lyr.SetAttributeFilter("'x'+PRFEDEA = 'x35043423'")

    count = gdaltest.lyr.GetFeatureCount()
    assert count == 1, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 1' % count)

    gdaltest.lyr.SetAttributeFilter('')

###############################################################################
# Test '%' operator.


def test_ogr_rfc28_4():
    gdaltest.lyr.SetAttributeFilter("EAS_ID % 5 = 1")

    count = gdaltest.lyr.GetFeatureCount()
    assert count == 2, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 2' % count)

    gdaltest.lyr.SetAttributeFilter('')

###############################################################################
# Test '%' operator.


def test_ogr_rfc28_5():
    gdaltest.lyr.SetAttributeFilter("EAS_ID % 5 = 1")

    count = gdaltest.lyr.GetFeatureCount()
    assert count == 2, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 2' % count)

    gdaltest.lyr.SetAttributeFilter('')

###############################################################################
# Test support for a quoted field name.


def test_ogr_rfc28_6():
    gdaltest.lyr.SetAttributeFilter("\"EAS_ID\" = 166")

    count = gdaltest.lyr.GetFeatureCount()
    assert count == 1, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 1' % count)

    gdaltest.lyr.SetAttributeFilter('')

###############################################################################
# test with distinguished name for field in where clause.


def test_ogr_rfc28_7_wrong_quoting():
    with gdaltest.error_handler():
        ql = gdaltest.ds.ExecuteSQL("select eas_id from idlink where \"idlink.eas_id\" = 166")

    count = ql.GetFeatureCount()
    assert count == 1, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 1' % count)

    gdaltest.ds.ReleaseResultSet(ql)


def test_ogr_rfc28_7_good_quoting():
    ql = gdaltest.ds.ExecuteSQL("select eas_id from idlink where idlink.eas_id = 166")

    count = ql.GetFeatureCount()
    assert count == 1, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 1' % count)

    gdaltest.ds.ReleaseResultSet(ql)

###############################################################################
# test with distinguished name for field in target columns.


def test_ogr_rfc28_8_wrong_quoting():
    with gdaltest.error_handler():
        ql = gdaltest.ds.ExecuteSQL("select \"idlink.eas_id\" from idlink where \"idlink.eas_id\" = 166")

    count = ql.GetFeatureCount()
    assert count == 1, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 1' % count)

    expect = [166]
    tr = ogrtest.check_features_against_list(ql, 'idlink.eas_id', expect)

    gdaltest.ds.ReleaseResultSet(ql)

    assert tr


def test_ogr_rfc28_8_good_quoting():
    ql = gdaltest.ds.ExecuteSQL("select idlink.eas_id from idlink where idlink.eas_id = 166")

    count = ql.GetFeatureCount()
    assert count == 1, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 1' % count)

    expect = [166]
    tr = ogrtest.check_features_against_list(ql, 'idlink.eas_id', expect)

    gdaltest.ds.ReleaseResultSet(ql)

    assert tr

###############################################################################
# Test with quoted funky (non-identifier) name.


def test_ogr_rfc28_9():
    ds = ogr.Open('data/csv/oddname.csv')
    lyr = ds.GetLayer(0)
    lyr.SetAttributeFilter("\"Funky @Name\" = '32'")

    count = lyr.GetFeatureCount()
    assert count == 1, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 1' % count)

    expect = ['8902']
    tr = ogrtest.check_features_against_list(lyr, 'PRIME_MERIDIAN_CODE', expect)
    assert tr

# TODO: unparse quoting?
###############################################################################
# test quoted names for funky columns in SELECT WHERE (confirm unparse quoting)


def test_ogr_rfc28_10():
    ds = ogr.Open('data/csv/oddname.csv')
    lyr = ds.ExecuteSQL("SELECT * from oddname where \"Funky @Name\" = '32'")

    count = lyr.GetFeatureCount()
    assert count == 1, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 1' % count)

    expect = ['8902']
    tr = ogrtest.check_features_against_list(lyr, 'PRIME_MERIDIAN_CODE', expect)
    ds.ReleaseResultSet(lyr)

    assert tr

###############################################################################
# test quoted funky names in output columns list.


def test_ogr_rfc28_11():
    ds = ogr.Open('data/csv/oddname.csv')
    lyr = ds.ExecuteSQL("SELECT \"Funky @Name\" from oddname where prime_meridian_code = '8902'")

    count = lyr.GetFeatureCount()
    assert count == 1, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 1' % count)

    expect = ['32']
    tr = ogrtest.check_features_against_list(lyr, 'Funky @Name', expect)
    ds.ReleaseResultSet(lyr)

    assert tr

###############################################################################
# test selecting fixed string fields.


def test_ogr_rfc28_12():
    lyr = gdaltest.ds.ExecuteSQL("SELECT 'constant string', 'other' as abc, eas_id from idlink where eas_id = 165")

    count = lyr.GetFeatureCount()
    assert count == 1, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 1' % count)

    expect = ['other']
    tr = ogrtest.check_features_against_list(lyr, 'abc', expect)

    expect = [165]
    if tr:
        lyr.ResetReading()
        tr = ogrtest.check_features_against_list(lyr, 'eas_id', expect)

    expect = ['constant string']
    if tr:
        lyr.ResetReading()
        tr = ogrtest.check_features_against_list(lyr, 'field_1', expect)

    gdaltest.ds.ReleaseResultSet(lyr)

    assert tr

###############################################################################
# Test SUBSTR operator in the context of a WHERE clause.


def test_ogr_rfc28_13():
    gdaltest.lyr.SetAttributeFilter("SUBSTR(PRFEDEA,5,4) = '3423'")

    count = gdaltest.lyr.GetFeatureCount()
    assert count == 1, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 1' % count)

    gdaltest.lyr.SetAttributeFilter('')

###############################################################################
# test selecting fixed string fields.


def test_ogr_rfc28_14():
    lyr = gdaltest.ds.ExecuteSQL("SELECT SUBSTR(PRFEDEA,4,5) from poly where eas_id in (168,179)")

    expect = ['43411', '43423']
    tr = ogrtest.check_features_against_list(lyr, 'substr_prfedea', expect)

    gdaltest.ds.ReleaseResultSet(lyr)

    assert tr

###############################################################################
# Test CONCAT with more than two arguments.


def test_ogr_rfc28_15():
    lyr = gdaltest.ds.ExecuteSQL("SELECT CONCAT(PRFEDEA,' ',CAST(EAS_ID AS CHARACTER(3))) from poly where eas_id in (168,179)")

    expect = ['35043411 168', '35043423 179']
    tr = ogrtest.check_features_against_list(lyr, 'concat_prfedea', expect)

    gdaltest.ds.ReleaseResultSet(lyr)

    assert tr

###############################################################################
# Test parse support for negative numbers (#3724)


def test_ogr_rfc28_16():
    lyr = gdaltest.ds.ExecuteSQL("SELECT -1, 3--1,3*-1,2e-1,3-1 from poly where eas_id = 168")

    expect = [-1]
    tr = ogrtest.check_features_against_list(lyr, 'field_1', expect)

    expect = [4]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list(lyr, 'field_2', expect)

    expect = [-3]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list(lyr, 'field_3', expect)

    expect = [0.2]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list(lyr, 'field_4', expect)

    expect = [2]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list(lyr, 'field_5', expect)

    gdaltest.ds.ReleaseResultSet(lyr)

    assert tr

###############################################################################
# Test evaluation of division - had a problem with type conversion.


def test_ogr_rfc28_17():
    lyr = gdaltest.ds.ExecuteSQL("SELECT 5/2, 5.0/2.0, 5/2.0, 5.0/2 from poly where eas_id = 168")

    expect = [2]
    tr = ogrtest.check_features_against_list(lyr, 'field_1', expect)

    expect = [2.5]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list(lyr, 'field_2', expect)

    expect = [2.5]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list(lyr, 'field_3', expect)

    expect = [2.5]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list(lyr, 'field_4', expect)

    gdaltest.ds.ReleaseResultSet(lyr)

    assert tr


###############################################################################
# Test some special distinct cases.

def test_ogr_rfc28_18():
    ds = ogr.Open('data/shp/departs.shp')
    lyr = ds.ExecuteSQL("SELECT COUNT(distinct id), COUNT(distinct id) as \"xx\" from departs")

    expect = [1]
    tr = ogrtest.check_features_against_list(lyr, 'COUNT_id', expect)

    expect = [1]
    lyr.ResetReading()
    tr = ogrtest.check_features_against_list(lyr, 'xx', expect)

    ds.ReleaseResultSet(lyr)

    assert tr


###############################################################################
# Verify that NOT IN ( list ) works

def test_ogr_rfc28_19():

    sql_lyr = gdaltest.ds.ExecuteSQL('select * from poly where eas_id not in (158,165)')

    count = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet(sql_lyr)

    assert count == 8, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 8' % count)


###############################################################################
# Verify arithmetic operator precedence and unary minus

def test_ogr_rfc28_20():

    ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")
    lyr = ds.CreateLayer("my_layer")
    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 2)
    lyr.CreateFeature(feat)

    sql_lyr = ds.ExecuteSQL('select -intfield + 1 + 2 * 3 + 5 - 3 * 2 from my_layer')
    feat = sql_lyr.GetNextFeature()
    assert feat.GetField('FIELD_1') == 4
    ds.ReleaseResultSet(sql_lyr)

    ds = None

###############################################################################
# Verify that BETWEEN works


def test_ogr_rfc28_21():

    sql_lyr = gdaltest.ds.ExecuteSQL('select * from poly where eas_id between 165 and 169')

    count_between = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gdaltest.ds.ExecuteSQL('select * from poly where eas_id >= 165 and eas_id <= 169')

    count_ge_and_le = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet(sql_lyr)

    assert count_between == count_ge_and_le, \
        ('Got wrong count with GetFeatureCount() - %d, expecting %d' % (count_between, count_ge_and_le))

###############################################################################
# Verify that NOT BETWEEN works


def test_ogr_rfc28_22():

    sql_lyr = gdaltest.ds.ExecuteSQL('select * from poly where eas_id not between 165 and 169')

    count_not_between = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gdaltest.ds.ExecuteSQL('select * from poly where not(eas_id >= 165 and eas_id <= 169)')

    count_not_ge_and_le = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet(sql_lyr)

    assert count_not_between == count_not_ge_and_le, \
        ('Got wrong count with GetFeatureCount() - %d, expecting %d' % (count_not_between, count_not_ge_and_le))

###############################################################################
# Verify that NOT LIKE works


def test_ogr_rfc28_23():

    sql_lyr = gdaltest.ds.ExecuteSQL("select * from poly where PRFEDEA NOT LIKE '35043413'")

    count_not_like1 = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gdaltest.ds.ExecuteSQL("select * from poly where NOT (PRFEDEA LIKE '35043413')")

    count_not_like2 = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet(sql_lyr)

    assert count_not_like1 == count_not_like2, \
        ('Got wrong count with GetFeatureCount() - %d, expecting %d' % (count_not_like1, count_not_like2))

###############################################################################
# Verify that NULL works


def test_ogr_rfc28_24():

    sql_lyr = gdaltest.ds.ExecuteSQL("select *, NULL, NULL as nullstrfield, CAST(null as integer) as nullintfield from poly where NULL IS NULL")

    feat = sql_lyr.GetNextFeature()

    if feat.IsFieldSet('FIELD_4'):
        feat.DumpReadable()
        gdaltest.ds.ReleaseResultSet(sql_lyr)
        pytest.fail()

    if feat.IsFieldSet('nullstrfield'):
        feat.DumpReadable()
        gdaltest.ds.ReleaseResultSet(sql_lyr)
        pytest.fail()

    if feat.IsFieldSet('nullintfield'):
        feat.DumpReadable()
        gdaltest.ds.ReleaseResultSet(sql_lyr)
        pytest.fail()

    count = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet(sql_lyr)

    assert count == 10, \
        ('Got wrong count with GetFeatureCount() - %d, expecting %d' % (count, 10))

###############################################################################
# Verify that LIKE pattern ESCAPE escape_char works


def test_ogr_rfc28_25():

    sql_lyr = gdaltest.ds.ExecuteSQL("select * from poly where prfedea LIKE 'x35043408' ESCAPE 'x'")

    count = sql_lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet(sql_lyr)

    assert count == 1, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 1' % count)

###############################################################################
# Test SUBSTR with negative offsets


def test_ogr_rfc28_26():
    lyr = gdaltest.ds.ExecuteSQL("SELECT SUBSTR(PRFEDEA,-2) from poly where eas_id in (168,179)")

    expect = ['11', '23']
    tr = ogrtest.check_features_against_list(lyr, 'substr_prfedea', expect)

    gdaltest.ds.ReleaseResultSet(lyr)

    assert tr

###############################################################################
# Test that we correctly let floating point values as floating point, and not as integer (#4634)"


def test_ogr_rfc28_27():

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM poly WHERE 4000000000. > 2000000000.")

    count = lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet(lyr)

    assert count == 10

###############################################################################
# Extensive test of the evaluation of arithmetic and logical operators


def ogr_rfc28_28_test(formula, expected_val):
    sql_lyr = gdaltest.ds.ExecuteSQL("SELECT " + formula + " from poly where fid = 0")
    f = sql_lyr.GetNextFeature()
    got = f.GetField(0)
    gdaltest.ds.ReleaseResultSet(sql_lyr)

    assert got == expected_val, \
        ('bad result for %s : %s' % (formula, str(expected_val)))


def test_ogr_rfc28_28():

    operators = ['+', '-', '*', '/', '%']
    formulas = []
    for operator in operators:
        formulas.append('6' + operator + '3')
        formulas.append('5.1' + operator + '3.2')
        formulas.append('5' + operator + '3.2')
        formulas.append('5.1' + operator + '3')
        formulas.append('3000000000000' + operator + '3')
        if operator != '/':
            formulas.append('3' + operator + '3000000000000')
        formulas.append('3000000000000' + operator + '3.')
        if operator != '/':
            formulas.append('3.' + operator + '3000000000000')

    for formula in formulas:
        expected_val = eval(formula)
        ret = ogr_rfc28_28_test(formula, expected_val)
        if ret == 'fail':
            return ret

    operators = ['<', '<=', '>', '>=', ' = ', '<>']
    formulas = []
    for operator in operators:
        formulas.append('3' + operator + '3')
        formulas.append('3.' + operator + '3.')
        formulas.append('3' + operator + '6')
        formulas.append('3.' + operator + '6.')
        formulas.append('3' + operator + '6.')
        formulas.append('3.' + operator + '6')
        formulas.append('6' + operator + '3')
        formulas.append('6.' + operator + '3.')
        formulas.append('6' + operator + '3.')
        formulas.append('6.' + operator + '3')
        formulas.append("'a'" + operator + "'a'")
        formulas.append("'a'" + operator + "'b'")
        formulas.append("'b'" + operator + "'a'")
        formulas.append('3' + operator + '1000000000000')
        formulas.append('1000000000000' + operator + '3')
        formulas.append('1000000000000' + operator + '1000000000000')

    for formula in formulas:
        expected_bool = eval(formula.replace(' = ', '==').replace('<>', '!='))
        ret = ogr_rfc28_28_test(formula, expected_bool)
        if ret == 'fail':
            return ret

    formulas_and_expected_val = [['3 in (3,5)', True],
                                 ['1000000000000 in (1000000000000, 1000000000001)', True],
                                 ['4 in (3,5)', False],
                                 ['3. in (3.,4.)', True],
                                 ['4. in (3.,5.)', False],
                                 ["'c' in ('c','e')", True],
                                 ["'d' in ('c','e')", False],
                                 ['2 between 2 and 4', True],
                                 ['3 between 2 and 4', True],
                                 ['4 between 2 and 4', True],
                                 ['1 between 2 and 4', False],
                                 ['5 between 2 and 4', False],
                                 ['2. between 2. and 4.', True],
                                 ['3. between 2. and 4.', True],
                                 ['4. between 2. and 4.', True],
                                 ['1. between 2. and 4.', False],
                                 ['5. between 2. and 4.', False],
                                 ["'b' between 'b' and 'd'", True],
                                 ["'c' between 'b' and 'd'", True],
                                 ["'d' between 'b' and 'd'", True],
                                 ["'a' between 'b' and 'd'", False],
                                 ["'e' between 'b' and 'd'", False],
                                 ["null is null", True],
                                 ["1 is null", False],
                                 ["1.0 is null", False]]

    for [formula, expected_val] in formulas_and_expected_val:
        ret = ogr_rfc28_28_test(formula, expected_val)
        if ret == 'fail':
            return ret


###############################################################################
# Test behaviour of binary operations when one operand is a NULL value


def test_ogr_rfc28_29():

    lyr = gdaltest.ds.ExecuteSQL("select * from idlink where (eas_id + cast(null as integer)) is not null or eas_id = 170 + cast(null as integer) or (eas_id + cast(null as float)) is not null or eas_id = 170.0 + cast(null as float)")

    count = lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet(lyr)

    assert count == 0

###############################################################################
# Test behaviour of binary operations on strings when one operand is a NULL value


def test_ogr_rfc28_30():

    lyr = gdaltest.ds.ExecuteSQL("select * from idlink2 where F1 <> 'foo' or concat(F1,cast(null as character(32))) is not null")

    count = lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet(lyr)

    assert count == 0

###############################################################################
# Test UNION ALL


def test_ogr_rfc28_31():

    lyr = gdaltest.ds.ExecuteSQL("select * from idlink union all select * from idlink2")

    count = lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet(lyr)

    assert count != 6 + 7

###############################################################################
# Test UNION ALL with parenthesis


def test_ogr_rfc28_32():

    lyr = gdaltest.ds.ExecuteSQL("(select * from idlink) union all (select * from idlink2 order by eas_id)")

    count = lyr.GetFeatureCount()

    gdaltest.ds.ReleaseResultSet(lyr)

    assert count != 6 + 7

###############################################################################
# Test lack of end-of-string character


def test_ogr_rfc28_33():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL("select * from idlink where name='foo")
    gdal.PopErrorHandler()

    assert lyr is None

###############################################################################
# Test wildcard expansion of an unknown table.


def test_ogr_rfc28_34():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL("select foo.* from idlink")
    gdal.PopErrorHandler()
    assert (gdal.GetLastErrorMsg().find(
            'Table foo not recognised from foo.* definition') == 0)

    assert lyr is None

###############################################################################
# Test selecting more than one distinct


def test_ogr_rfc28_35():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL("select distinct eas_id, distinct name from idlink")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg().find('SQL Expression Parsing Error') == 0

    assert lyr is None

###############################################################################
# Test selecting more than one distinct


def test_ogr_rfc28_35_bis():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL("select distinct eas_id, name from idlink")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg().find('SELECT DISTINCT not supported on multiple columns') == 0

    assert lyr is None

###############################################################################
# Test selecting more than one distinct


def test_ogr_rfc28_35_ter():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL("select distinct * from idlink")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg().find('SELECT DISTINCT not supported on multiple columns') == 0

    assert lyr is None

###############################################################################
# Test ORDER BY a DISTINCT list by more than one key


def test_ogr_rfc28_36():

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL("select distinct eas_id from idlink order by eas_id, name")
    if lyr is not None:
        lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg().find("Can't ORDER BY a DISTINCT list by more than one key") == 0
    gdaltest.ds.ReleaseResultSet(lyr)

###############################################################################
# Test different fields for ORDER BY and DISTINCT


def test_ogr_rfc28_37():

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL("select distinct eas_id from idlink order by name")
    if lyr is not None:
        lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg().find("Only selected DISTINCT field can be used for ORDER BY") == 0
    gdaltest.ds.ReleaseResultSet(lyr)

###############################################################################
# Test invalid SUBSTR


def test_ogr_rfc28_38():

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL("SELECT SUBSTR(PRFEDEA) from poly")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg().find("Expected 2 or 3 arguments to SUBSTR(), but got 1") == 0
    assert lyr is None

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.ds.ExecuteSQL("SELECT SUBSTR(1,2) from poly")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg().find("Wrong argument type for SUBSTR()") == 0
    assert lyr is None

###############################################################################
# Test COUNT() on a 0-row result


def test_ogr_rfc28_39():

    lyr = gdaltest.ds.ExecuteSQL("SELECT COUNT(*) from poly where 0 = 1")

    tr = ogrtest.check_features_against_list(lyr, 'count_*', [0])

    gdaltest.ds.ReleaseResultSet(lyr)

    assert tr

###############################################################################
# Test MIN(), MAX() and AVG() on a date (#5333)


def test_ogr_rfc28_40():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('DATE', ogr.OFTDateTime))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, '2013/12/31 23:59:59')
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, '2013/01/01 00:00:00')
    lyr.CreateFeature(feat)
    lyr = ds.ExecuteSQL("SELECT MIN(DATE), MAX(DATE), AVG(DATE) from test")

    tr = ogrtest.check_features_against_list(lyr, 'MIN_DATE', ['2013/01/01 00:00:00'])
    lyr.ResetReading()
    tr2 = ogrtest.check_features_against_list(lyr, 'MAX_DATE', ['2013/12/31 23:59:59'])
    lyr.ResetReading()
    tr3 = ogrtest.check_features_against_list(lyr, 'AVG_DATE', ['2013/07/02 11:59:59.500'])

    gdaltest.ds.ReleaseResultSet(lyr)

    assert tr

    assert tr2

    assert tr3


###############################################################################
# Verify that SELECT * works on a layer with a field that has a dot character (#5379)

def test_ogr_rfc28_41():

    ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")
    lyr = ds.CreateLayer("my_layer")
    field_defn = ogr.FieldDefn('a.b', ogr.OFTInteger)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 2)
    lyr.CreateFeature(feat)

    sql_lyr = ds.ExecuteSQL('select * from my_layer')
    feat = sql_lyr.GetNextFeature()
    assert feat.GetField('a.b') == 2
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL('select l.* from my_layer l')
    feat = sql_lyr.GetNextFeature()
    assert feat.GetField('l.a.b') == 2
    ds.ReleaseResultSet(sql_lyr)

    ds = None

###############################################################################
# Test boolean and int16 support


def test_ogr_rfc28_42():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('test')
    fld_defn = ogr.FieldDefn('b', ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn('short', ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 0)
    feat.SetField(1, 32000)
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1)
    lyr.CreateFeature(feat)

    # To b OR NOT to b... that's the question
    lyr = ds.ExecuteSQL("SELECT b, NOT b, 1 + b, CAST(1 AS BOOLEAN), b IS NOT NULL, short, 1 + short, CAST(1 + short as SMALLINT) FROM test WHERE b OR NOT b")
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTBoolean
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetSubType() == ogr.OFSTBoolean
    assert lyr.GetLayerDefn().GetFieldDefn(2).GetSubType() == ogr.OFSTNone
    assert lyr.GetLayerDefn().GetFieldDefn(3).GetSubType() == ogr.OFSTBoolean
    assert lyr.GetLayerDefn().GetFieldDefn(4).GetSubType() == ogr.OFSTBoolean
    assert lyr.GetLayerDefn().GetFieldDefn(5).GetSubType() == ogr.OFSTInt16
    assert lyr.GetLayerDefn().GetFieldDefn(6).GetSubType() == ogr.OFSTNone
    assert lyr.GetLayerDefn().GetFieldDefn(7).GetSubType() == ogr.OFSTInt16
    f = lyr.GetNextFeature()
    if f.GetField('b') != 0 or f.GetField(1) != 1 or f.GetField(2) != 1 or \
       f.GetField(3) != 1 or f.GetField(4) != 1 or \
       f.GetField(5) != 32000 or f.GetField(6) != 32001 or f.GetField(7) != 32001:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetField('b') != 1 or f.GetField(1) != 0 or f.GetField(2) != 2 or f.GetField(3) != 1 or f.GetField(4) != 1:
        f.DumpReadable()
        pytest.fail()
    gdaltest.ds.ReleaseResultSet(lyr)

    lyr = ds.ExecuteSQL("SELECT MIN(b), MAX(b), SUM(b) FROM test")
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTBoolean
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetSubType() == ogr.OFSTBoolean
    assert lyr.GetLayerDefn().GetFieldDefn(2).GetSubType() == ogr.OFSTNone
    f = lyr.GetNextFeature()
    assert f.GetField('MIN_b') == 0
    assert f.GetField('MAX_b') == 1
    assert f.GetField('SUM_b') == 1
    gdaltest.ds.ReleaseResultSet(lyr)

###############################################################################
# Test integer64 support


def test_ogr_rfc28_43():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('test')
    fld_defn = ogr.FieldDefn('myint64', ogr.OFTInteger64)
    lyr.CreateField(fld_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, -1000000000000)
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 100000000000)
    lyr.CreateFeature(feat)

    lyr = ds.ExecuteSQL("SELECT 1000000000000, myint64, CAST(1 AS bigint), CAST(100000000000 AS bigint), CAST(1 AS numeric(15,0)) FROM test WHERE myint64 < -9999999999 or myint64 > 9999999999")
    f = lyr.GetNextFeature()
    assert lyr.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTInteger64
    assert lyr.GetLayerDefn().GetFieldDefn(3).GetType() == ogr.OFTInteger64
    if f.GetField(0) != 1000000000000 or f.GetField(1) != -1000000000000 or f.GetField(2) != 1 or f.GetField(3) != 100000000000 or f.GetField(4) != 1.0:
        f.DumpReadable()
        pytest.fail()
    gdaltest.ds.ReleaseResultSet(lyr)

    lyr = ds.ExecuteSQL("SELECT MIN(myint64), MAX(myint64), SUM(myint64) FROM test")
    f = lyr.GetNextFeature()
    if f.GetField('MIN_myint64') != -1000000000000:
        f.DumpReadable()
        pytest.fail()
    if f.GetField('MAX_myint64') != 100000000000:
        f.DumpReadable()
        pytest.fail()
    if f.GetField('SUM_myint64') != -1000000000000 + 100000000000:
        f.DumpReadable()
        pytest.fail()
    gdaltest.ds.ReleaseResultSet(lyr)

    lyr = ds.ExecuteSQL("SELECT DISTINCT myint64 FROM test ORDER BY myint64")
    f = lyr.GetNextFeature()
    if f.GetField('myint64') != -1000000000000:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetField('myint64') != 100000000000:
        f.DumpReadable()
        pytest.fail()
    gdaltest.ds.ReleaseResultSet(lyr)

###############################################################################
# Test crazy quoting of table and fields


def test_ogr_rfc28_44():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('lyr.withpoint')
    fld_defn = ogr.FieldDefn('field.withpoint', ogr.OFTInteger)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn('foo', ogr.OFTInteger)
    lyr.CreateField(fld_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, -1)
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1)
    feat.SetField(1, 2)
    lyr.CreateFeature(feat)

    gdal.ErrorReset()
    lyr = ds.ExecuteSQL("SELECT * FROM \"lyr.withpoint\" WHERE \"field.withpoint\" = 1")
    assert gdal.GetLastErrorMsg() == ''
    f = lyr.GetNextFeature()
    assert f is not None
    gdaltest.ds.ReleaseResultSet(lyr)

    gdal.ErrorReset()
    lyr = ds.ExecuteSQL("SELECT \"lyr.withpoint\".\"field.withpoint\", \"field.withpoint\" FROM \"lyr.withpoint\" WHERE \"lyr.withpoint\".\"field.withpoint\" = 1")
    assert gdal.GetLastErrorMsg() == ''
    f = lyr.GetNextFeature()
    assert f is not None
    gdaltest.ds.ReleaseResultSet(lyr)

    # Test our tolerance against lack of necessary quoting
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr = ds.ExecuteSQL("SELECT * FROM \"lyr.withpoint\" WHERE field.withpoint = 1")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() == 'Passed field name field.withpoint should have been surrounded by double quotes. Accepted since there is no ambiguity...'
    f = lyr.GetNextFeature()
    assert f is not None
    gdaltest.ds.ReleaseResultSet(lyr)

    # Againg, but in a situation where there IS ambiguity
    lyr = ds.CreateLayer('field')
    fld_defn = ogr.FieldDefn('id', ogr.OFTInteger)
    lyr.CreateField(fld_defn)

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr = ds.ExecuteSQL("SELECT * FROM \"lyr.withpoint\" JOIN field ON \"lyr.withpoint\".foo = field.id WHERE field.withpoint = 1")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() == '"field"."withpoint" not recognised as an available field.'
    assert lyr is None

    # Test our tolerance against unnecessary quoting
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr = ds.ExecuteSQL("SELECT * FROM \"lyr.withpoint\" f WHERE \"f.foo\" = 2")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() == 'Passed field name f.foo should NOT have been surrounded by double quotes. Accepted since there is no ambiguity...'
    f = lyr.GetNextFeature()
    assert f is not None
    gdaltest.ds.ReleaseResultSet(lyr)

###############################################################################
# Test 'FROM table_name AS alias'


def test_ogr_rfc28_45():

    ql = gdaltest.ds.ExecuteSQL("select eas_id from idlink as il where il.eas_id = 166")

    count = ql.GetFeatureCount()
    assert count == 1, \
        ('Got wrong count with GetFeatureCount() - %d, expecting 1' % count)

    gdaltest.ds.ReleaseResultSet(ql)

###############################################################################
# Test fid special column and 64 bit


def test_ogr_rfc28_46():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('lyr')
    fld_defn = ogr.FieldDefn('val', ogr.OFTInteger)
    lyr.CreateField(fld_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(3000000000)
    feat.SetField('val', 1)
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(2500000000)
    feat.SetField('val', 2)
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(2200000000)
    feat.SetField('val', 3)
    lyr.CreateFeature(feat)

    lyr.SetAttributeFilter('fid >= 2500000000')
    assert lyr.GetFeatureCount() == 2
    lyr.SetAttributeFilter(None)

    # Explicit cast of fid to bigint needed in SELECT columns
    sql_lyr = ds.ExecuteSQL('SELECT CAST(fid AS bigint) AS outfid, val FROM lyr WHERE fid >= 2500000000 ORDER BY fid')
    f = sql_lyr.GetNextFeature()
    if f.GetFID() != 2500000000 or f['outfid'] != 2500000000 or f['val'] != 2:
        f.DumpReadable()
        pytest.fail()
    f = sql_lyr.GetNextFeature()
    if f.GetFID() != 3000000000 or f['outfid'] != 3000000000 or f['val'] != 1:
        f.DumpReadable()
        pytest.fail()
    f = sql_lyr.GetNextFeature()
    if f is not None:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    # Explicit cast of fid to bigint no longer needed if the layer is declared OLMD_FID64=YES
    lyr.SetMetadataItem('OLMD_FID64', 'YES')
    sql_lyr = ds.ExecuteSQL('SELECT fid AS outfid, val FROM lyr WHERE fid >= 2500000000 ORDER BY fid')
    f = sql_lyr.GetNextFeature()
    if f.GetFID() != 2500000000 or f['outfid'] != 2500000000 or f['val'] != 2:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    ds = None

###############################################################################
# Test LIMIT and OFFSET


def test_ogr_rfc28_47():

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM POLY LIMIT 0")
    tr = ogrtest.check_features_against_list(lyr, 'EAS_ID', [])
    gdaltest.ds.ReleaseResultSet(lyr)
    assert tr

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM POLY LIMIT 1")
    assert lyr.GetFeatureCount() == 1
    tr = ogrtest.check_features_against_list(lyr, 'EAS_ID', [168])
    gdaltest.ds.ReleaseResultSet(lyr)
    assert tr

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM POLY ORDER BY EAS_ID LIMIT 1")
    assert lyr.GetFeatureCount() == 1
    tr = ogrtest.check_features_against_list(lyr, 'EAS_ID', [158])
    gdaltest.ds.ReleaseResultSet(lyr)
    assert tr

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM POLY ORDER BY PRFEDEA LIMIT 1")
    assert lyr.GetFeatureCount() == 1
    tr = ogrtest.check_features_against_list(lyr, 'PRFEDEA', ['35043369'])
    gdaltest.ds.ReleaseResultSet(lyr)
    assert tr

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM POLY WHERE 0 ORDER BY EAS_ID LIMIT 1")
    assert lyr.GetNextFeature() is None
    gdaltest.ds.ReleaseResultSet(lyr)

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM POLY WHERE EAS_ID = 168 LIMIT 11")
    assert lyr.GetFeatureCount() == 1
    tr = ogrtest.check_features_against_list(lyr, 'EAS_ID', [168])
    gdaltest.ds.ReleaseResultSet(lyr)
    assert tr

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM POLY WHERE EAS_ID = 168 OFFSET 0")
    tr = ogrtest.check_features_against_list(lyr, 'EAS_ID', [168])
    gdaltest.ds.ReleaseResultSet(lyr)
    assert tr

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM POLY WHERE EAS_ID = 168 OFFSET 1")
    assert lyr.GetFeatureCount() == 0
    tr = ogrtest.check_features_against_list(lyr, 'EAS_ID', [])
    gdaltest.ds.ReleaseResultSet(lyr)
    assert tr

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM POLY OFFSET 10")
    assert lyr.GetFeatureCount() == 0
    tr = ogrtest.check_features_against_list(lyr, 'EAS_ID', [])
    gdaltest.ds.ReleaseResultSet(lyr)
    assert tr

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM POLY OFFSET 8")
    assert lyr.GetFeatureCount() == 2
    tr = ogrtest.check_features_against_list(lyr, 'EAS_ID', [165, 170])
    gdaltest.ds.ReleaseResultSet(lyr)
    assert tr

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM POLY LIMIT 1 OFFSET 8")
    assert lyr.GetFeatureCount() == 1
    tr = ogrtest.check_features_against_list(lyr, 'EAS_ID', [165])
    gdaltest.ds.ReleaseResultSet(lyr)
    assert tr

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM POLY LIMIT 2 OFFSET 8")
    lyr.SetNextByIndex(1)
    f = lyr.GetNextFeature()
    assert f['EAS_ID'] == 170
    f = lyr.GetNextFeature()
    assert f is None
    gdaltest.ds.ReleaseResultSet(lyr)

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM POLY ORDER BY EAS_ID DESC LIMIT 2")
    tr = ogrtest.check_features_against_list(lyr, 'EAS_ID', [179, 173])
    gdaltest.ds.ReleaseResultSet(lyr)
    assert tr

    lyr = gdaltest.ds.ExecuteSQL("SELECT * FROM POLY ORDER BY EAS_ID DESC LIMIT 1 OFFSET 1")
    tr = ogrtest.check_features_against_list(lyr, 'EAS_ID', [173])
    gdaltest.ds.ReleaseResultSet(lyr)
    assert tr

    lyr = gdaltest.ds.ExecuteSQL("SELECT DISTINCT EAS_ID FROM POLY ORDER BY EAS_ID DESC LIMIT 2 OFFSET 3")
    tr = ogrtest.check_features_against_list(lyr, 'EAS_ID', [171, 170])
    gdaltest.ds.ReleaseResultSet(lyr)
    assert tr

###############################################################################
# Test date/datetime comparisons (#6810)


def test_ogr_rfc28_48():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('lyr')
    fld_defn = ogr.FieldDefn('dt', ogr.OFTDateTime)
    lyr.CreateField(fld_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('dt', '2017/02/17 11:06:34')
    lyr.CreateFeature(feat)

    with gdaltest.error_handler():
        assert lyr.SetAttributeFilter('dt >= 2500000000') != 0

    lyr.SetAttributeFilter("dt >= 'a'")
    with gdaltest.error_handler():
        assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("'a' <= dt")
    with gdaltest.error_handler():
        assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("dt BETWEEN dt AND 'a'")
    with gdaltest.error_handler():
        assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("dt >= '2017/02/17 11:06:34'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("dt >= '2017/02/17'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("dt >= '2017/02/17 11:06:35'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("dt > '2017/02/17 11:06:33'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("dt > '2017/02/17 11:06:34'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("dt <= '2017/02/17 11:06:34'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("dt <= '2017/02/17 11:06:33'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("dt < '2017/02/17 11:06:35'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("dt < '2017/02/17 11:06:34'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("dt = '2017/02/17 11:06:34'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("dt = '2017/02/17 11:06:34.001'")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("dt BETWEEN dt AND dt")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("dt BETWEEN '2017/02/17 11:06:33.999' AND '2017/02/17 11:06:34.001'")
    assert lyr.GetFeatureCount() == 1

    lyr.SetAttributeFilter("dt BETWEEN '2017/02/17 11:06:34.001' AND dt")
    assert lyr.GetFeatureCount() == 0

    lyr.SetAttributeFilter("dt BETWEEN dt AND '2017/02/17 11:06:33.999'")
    assert lyr.GetFeatureCount() == 0


###############################################################################
def test_ogr_rfc28_int_overflows():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('lyr')
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    tests = [('SELECT -9223372036854775808 FROM lyr', -9223372036854775808),
             ('SELECT -(-9223372036854775808) FROM lyr', -9223372036854775808), # we could argue about the expected result...
             ('SELECT -9223372036854775808/1 FROM lyr', -9223372036854775808),
             ('SELECT 9223372036854775807 FROM lyr', 9223372036854775807),
             ('SELECT 9223372036854775807*1 FROM lyr', 9223372036854775807),
             ('SELECT 9223372036854775807/1 FROM lyr', 9223372036854775807),
             ('SELECT 9223372036854775807/-1 FROM lyr', -9223372036854775807),
             ('SELECT 9223372036854775807*-1 FROM lyr', -9223372036854775807),
             ('SELECT -1*9223372036854775807 FROM lyr', -9223372036854775807),
             ('SELECT 1*(-9223372036854775808) FROM lyr', -9223372036854775808),
             ('SELECT 0*(-9223372036854775808) FROM lyr', 0),
             ('SELECT 9223372036854775806+1 FROM lyr', 9223372036854775807),
             ('SELECT -9223372036854775807-1 FROM lyr', -9223372036854775808),
             ('SELECT 9223372036854775808 FROM lyr', 9223372036854775808.0),
             ('SELECT -9223372036854775809 FROM lyr', -9223372036854775809.0),
             ('SELECT 9223372036854775807+1 FROM lyr', None),
             ('SELECT 9223372036854775807 - (-1) FROM lyr', None),
             ('SELECT -9223372036854775808-1 FROM lyr', None),
             ('SELECT -9223372036854775808 + (-1) FROM lyr', None),
             ('SELECT 9223372036854775807*2 FROM lyr', None),
             ('SELECT -9223372036854775807*2 FROM lyr', None),
             ('SELECT -1*(-9223372036854775808) FROM lyr', None),
             ('SELECT 2 * (-9223372036854775807) FROM lyr', None),
             ('SELECT 9223372036854775807*-2 FROM lyr', None),
             ('SELECT -9223372036854775807*-2 FROM lyr', None),
             ('SELECT -9223372036854775808*-1 FROM lyr', None),
             ('SELECT -9223372036854775808/-1 FROM lyr', None),
             ('SELECT 1/0 FROM lyr', 2147483647),
            ]
    for sql, res in tests:
        sql_lyr = ds.ExecuteSQL(sql)
        if res is None:
            with gdaltest.error_handler():
                f = sql_lyr.GetNextFeature()
        else:
            f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == res, (sql, res, f.GetField(0))
        ds.ReleaseResultSet(sql_lyr)


###############################################################################

def test_ogr_rfc28_many_or():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('lyr')
    fld_defn = ogr.FieldDefn('val', ogr.OFTInteger)
    lyr.CreateField(fld_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('val', -15)
    lyr.CreateFeature(feat)

    sql = '1 = 1 AND (' + ' OR '.join('val = %d' % i for i in range(1024)) + ')'
    assert lyr.SetAttributeFilter(sql) == 0
    f = lyr.GetNextFeature()
    assert f is None

    sql = '1 = 1 AND (' + ' OR '.join('val = %d' % (i - 100) for i in range(1024)) + ')'
    assert lyr.SetAttributeFilter(sql) == 0
    f = lyr.GetNextFeature()
    assert f is not None


###############################################################################

def test_ogr_rfc28_many_and():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('lyr')
    fld_defn = ogr.FieldDefn('val', ogr.OFTInteger)
    lyr.CreateField(fld_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('val', -15)
    lyr.CreateFeature(feat)

    sql = '1 = 1 AND (' + ' AND '.join('val = -1' for i in range(1024)) + ')'
    assert lyr.SetAttributeFilter(sql) == 0
    f = lyr.GetNextFeature()
    assert f is None

    sql = '1 = 1 AND (' + ' AND '.join('val = -15' for i in range(1024)) + ')'
    assert lyr.SetAttributeFilter(sql) == 0
    f = lyr.GetNextFeature()
    assert f is not None


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/3919

def test_ogr_rfc28_nested_or():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone)
    field = ogr.FieldDefn('fclass', ogr.OFTString)
    lyr.CreateField(field)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('fclass', 'x')
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

    tests_ok = [
        """(fclass = 'a' OR fclass = 'b') OR (fclass = 'c' OR fclass = 'd' OR fclass = 'x')""",
        """(fclass = 'c' OR fclass = 'd' OR fclass = 'x') OR (fclass = 'a' OR fclass = 'b')""",
        """fclass = 'c' OR fclass = 'd' OR fclass = 'x'""",
        """fclass = 'c' OR (fclass = 'd' OR fclass = 'x')""",
        """(fclass = 'c' OR fclass = 'd') OR fclass = 'x'""",
        """fclass = 'x' OR fclass = 'c' OR fclass = 'd'""",
        """fclass = 'x' OR (fclass = 'c' OR fclass = 'd')""",
        """(fclass = 'x' OR fclass = 'd') OR fclass = 'd'""",
        """(fclass = 'a' OR fclass = 'b' OR fclass = 'b2') OR (fclass = 'c' OR fclass = 'd' OR fclass = 'x')""",
        """(1 = 0) OR (fclass = 'c' OR fclass = 'd' OR fclass = 'x')""",
        """(fclass = 'c' OR fclass = 'd' OR fclass = 'x') OR (1 = 0)""",
        """(1 = 0 OR 1 = 0 OR 1 = 1) AND (fclass = 'c' OR fclass = 'd' OR fclass = 'x')""",
    ]

    for sql in tests_ok:
        lyr.SetAttributeFilter(sql)
        assert lyr.GetFeatureCount() == 1, sql

    tests_ko = [
        """(fclass = 'a' OR fclass = 'b') OR (fclass = 'c' OR fclass = 'd' OR fclass = 'COND_NOT_MET')""",
        """(fclass = 'c' OR fclass = 'd' OR fclass = 'COND_NOT_MET') OR (fclass = 'a' OR fclass = 'b')""",
        """fclass = 'c' OR fclass = 'd' OR fclass = 'COND_NOT_MET'""",
        """fclass = 'c' OR (fclass = 'd' OR fclass = 'COND_NOT_MET')""",
        """(fclass = 'c' OR fclass = 'd') OR fclass = 'COND_NOT_MET'""",
        """fclass = 'COND_NOT_MET' OR fclass = 'c' OR fclass = 'd'""",
        """fclass = 'COND_NOT_MET' OR (fclass = 'c' OR fclass = 'd')""",
        """(fclass = 'COND_NOT_MET' OR fclass = 'd') OR fclass = 'd'""",
        """(fclass = 'a' OR fclass = 'b' OR fclass = 'b2') OR (fclass = 'c' OR fclass = 'd' OR fclass = 'COND_NOT_MET')""",
        """(1 = 0) OR (fclass = 'c' OR fclass = 'd' OR fclass = 'COND_NOT_MET')""",
        """(fclass = 'c' OR fclass = 'd' OR fclass = 'COND_NOT_MET') OR (1 = 0)""",
        """(1 = 0 OR 1 = 0 OR 1 = 1) AND (fclass = 'c' OR fclass = 'd' OR fclass = 'COND_NOT_MET')""",
    ]

    for sql in tests_ko:
        lyr.SetAttributeFilter(sql)
        assert lyr.GetFeatureCount() == 0, sql


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/3249

def test_ogr_rfc28_order_by_two_columns():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('lyr')
    fld_defn = ogr.FieldDefn('int_val', ogr.OFTInteger)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn('str_val', ogr.OFTString)
    lyr.CreateField(fld_defn)
    for i in range(101):
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField('int_val', 100 - i)
        if i < 26:
            feat.SetField('str_val', chr(ord('a') + i))
        else:
            feat.SetField('str_val', chr(ord('a') + (i // 26)) + chr(ord('a') + (i % 26)))
        lyr.CreateFeature(feat)

    sql_lyr = ds.ExecuteSQL('SELECT * FROM lyr ORDER BY int_val, str_val')
    for i in range(101):
        f = sql_lyr.GetNextFeature()
        assert f['int_val'] == i
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL('SELECT * FROM lyr ORDER BY int_val, str_val LIMIT 1')
    f = sql_lyr.GetNextFeature()
    assert f['int_val'] == 0
    f = None
    ds.ReleaseResultSet(sql_lyr)

###############################################################################


def test_ogr_rfc28_cleanup():
    gdaltest.lyr = None
    gdaltest.ds = None



