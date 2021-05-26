#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id: ogr_mem.py 23065 2011-09-05 20:41:03Z rouault $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR Feature facilities, particularly SetFrom()
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
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
import pytest

###############################################################################
# Create a destination feature type with one field for each field in the source
# feature, with the same names, but all the field types of a specific type.


def mk_dst_feature(src_feature, field_type):

    dst_feat_defn = ogr.FeatureDefn('dst')

    src_feat_defn = src_feature.GetDefnRef()
    for i in range(src_feat_defn.GetFieldCount()):
        src_field_defn = src_feat_defn.GetFieldDefn(i)
        dst_field_defn = ogr.FieldDefn(src_field_defn.GetName(), field_type)
        dst_feat_defn.AddFieldDefn(dst_field_defn)

    return ogr.Feature(dst_feat_defn)

###############################################################################
# Create a source feature


def mk_src_feature():

    feat_def = ogr.FeatureDefn('src')

    field_def = ogr.FieldDefn('field_integer', ogr.OFTInteger)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_integer64', ogr.OFTInteger64)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_real', ogr.OFTReal)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_string', ogr.OFTString)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_binary', ogr.OFTBinary)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_date', ogr.OFTDate)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_time', ogr.OFTTime)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_datetime', ogr.OFTDateTime)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_integerlist', ogr.OFTIntegerList)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_integer64list', ogr.OFTInteger64List)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_reallist', ogr.OFTRealList)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_stringlist', ogr.OFTStringList)
    feat_def.AddFieldDefn(field_def)

    src_feature = ogr.Feature(feat_def)
    src_feature.SetField('field_integer', 17)
    src_feature.SetField('field_integer64', 9876543210)
    src_feature.SetField('field_real', 18.4)
    src_feature.SetField('field_string', 'abc def')
    src_feature.SetFieldBinaryFromHexString('field_binary', '0123465789ABCDEF')
    src_feature.SetField('field_date', '2011/11/11')
    src_feature.SetField('field_time', '14:10:35')
    src_feature.SetField('field_datetime', 2011, 11, 11, 14, 10, 35.123, 0)
    got_vals = src_feature.GetFieldAsDateTime(feat_def.GetFieldIndex('field_datetime'))
    expected_vals = [2011, 11, 11, 14, 10, 35.123, 0]
    for i, exp_val in enumerate(expected_vals):
        if got_vals[i] != pytest.approx(exp_val, abs=1e-4):
            print(got_vals)
            print(expected_vals)
    src_feature.field_integerlist = '(3:10,20,30)'
    src_feature.field_integer64list = [9876543210]
    src_feature.field_reallist = [123.5, 567.0]
    src_feature.field_stringlist = ['abc', 'def']

    return src_feature

###############################################################################
# Helper function to check a single field value


def check(feat, fieldname, value):
    if feat.GetField(fieldname) != value:
        gdaltest.post_reason('did not get value %s for field %s, got %s.'
                             % (str(value), fieldname,
                                str(feat.GetField(fieldname))),
                             frames=3)
        feat.DumpReadable()
        return 0
    return 1

###############################################################################
# Copy to Integer


def test_ogr_feature_cp_integer():
    src_feature = mk_src_feature()
    src_feature.field_integerlist = [15]
    src_feature.field_reallist = [17.5]

    dst_feature = mk_dst_feature(src_feature, ogr.OFTInteger)
    gdal.PushErrorHandler()
    dst_feature.SetFrom(src_feature)
    gdal.PopErrorHandler()

    assert check(dst_feature, 'field_integer', 17)

    assert check(dst_feature, 'field_integer64', 2147483647)

    assert check(dst_feature, 'field_real', 18)

    assert check(dst_feature, 'field_string', 0)

    assert check(dst_feature, 'field_binary', None)

    assert check(dst_feature, 'field_date', None)

    assert check(dst_feature, 'field_time', None)

    assert check(dst_feature, 'field_datetime', None)

    assert check(dst_feature, 'field_integerlist', 15)

    assert check(dst_feature, 'field_integer64list', 2147483647)

    assert check(dst_feature, 'field_reallist', 17)

    assert check(dst_feature, 'field_stringlist', None)

    vals = []
    for val in dst_feature:
        vals.append(val)
    assert (vals == [17, 2147483647, 18, 0, None, None, None, None, 15,
                2147483647, 17, None])

###############################################################################
# Copy to Integer64


def test_ogr_feature_cp_integer64():
    src_feature = mk_src_feature()
    src_feature.field_integerlist = [15]
    src_feature.field_reallist = [17.5]

    dst_feature = mk_dst_feature(src_feature, ogr.OFTInteger64)
    dst_feature.SetFrom(src_feature)

    assert check(dst_feature, 'field_integer', 17)

    assert check(dst_feature, 'field_integer64', 9876543210)

    gdal.PushErrorHandler()
    int32_ovflw = dst_feature.GetFieldAsInteger('field_integer64')
    gdal.PopErrorHandler()
    assert int32_ovflw == 2147483647

    assert check(dst_feature, 'field_real', 18)

    assert check(dst_feature, 'field_string', 0)

    assert check(dst_feature, 'field_binary', None)

    assert check(dst_feature, 'field_date', None)

    assert check(dst_feature, 'field_time', None)

    assert check(dst_feature, 'field_datetime', None)

    assert check(dst_feature, 'field_integerlist', 15)

    assert check(dst_feature, 'field_integer64list', 9876543210)

    assert check(dst_feature, 'field_reallist', 17)

    assert check(dst_feature, 'field_stringlist', None)

###############################################################################
# Copy to Real


def test_ogr_feature_cp_real():
    src_feature = mk_src_feature()
    src_feature.field_integerlist = [15]
    src_feature.field_reallist = [17.5]

    dst_feature = mk_dst_feature(src_feature, ogr.OFTReal)
    with gdaltest.error_handler():
        dst_feature.SetFrom(src_feature)

    assert check(dst_feature, 'field_integer', 17.0)

    assert check(dst_feature, 'field_real', 18.4)

    assert check(dst_feature, 'field_string', 0)

    assert check(dst_feature, 'field_binary', None)

    assert check(dst_feature, 'field_date', None)

    assert check(dst_feature, 'field_time', None)

    assert check(dst_feature, 'field_datetime', None)

    assert check(dst_feature, 'field_integerlist', 15.0)

    assert check(dst_feature, 'field_reallist', 17.5)

    assert check(dst_feature, 'field_stringlist', None)

###############################################################################
# Copy to String


def test_ogr_feature_cp_string():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature(src_feature, ogr.OFTString)
    dst_feature.SetFrom(src_feature)

    assert check(dst_feature, 'field_integer', '17')

    assert check(dst_feature, 'field_integer64', '9876543210')

    assert check(dst_feature, 'field_real', '18.4')

    assert check(dst_feature, 'field_string', 'abc def')

    assert check(dst_feature, 'field_binary', '0123465789ABCDEF')

    assert check(dst_feature, 'field_date', '2011/11/11')

    assert check(dst_feature, 'field_time', '14:10:35')

    assert check(dst_feature, 'field_datetime', '2011/11/11 14:10:35.123')

    assert check(dst_feature, 'field_integerlist', '(3:10,20,30)')

    assert check(dst_feature, 'field_integer64list', '(1:9876543210)')

    assert check(dst_feature, 'field_reallist', '(2:123.5,567)')

    assert check(dst_feature, 'field_stringlist', '(2:abc,def)')

###############################################################################
# Copy to Binary


def test_ogr_feature_cp_binary():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature(src_feature, ogr.OFTBinary)
    dst_feature.SetFrom(src_feature)

    assert check(dst_feature, 'field_integer', None)

    assert check(dst_feature, 'field_integer64', None)

    assert check(dst_feature, 'field_real', None)

    assert check(dst_feature, 'field_string', None)

    assert check(dst_feature, 'field_binary', '0123465789ABCDEF')

    expected = b'\x01\x23\x46\x57\x89\xAB\xCD\xEF'
    assert dst_feature.GetFieldAsBinary('field_binary') == expected
    assert dst_feature.GetFieldAsBinary(dst_feature.GetDefnRef().GetFieldIndex('field_binary')) == expected

    assert check(dst_feature, 'field_date', None)

    assert check(dst_feature, 'field_time', None)

    assert check(dst_feature, 'field_datetime', None)

    assert check(dst_feature, 'field_integerlist', None)

    assert check(dst_feature, 'field_integer64list', None)

    assert check(dst_feature, 'field_reallist', None)

    assert check(dst_feature, 'field_stringlist', None)

###############################################################################
# Copy to date


def test_ogr_feature_cp_date():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature(src_feature, ogr.OFTDate)
    dst_feature.SetFrom(src_feature)

    assert check(dst_feature, 'field_integer', None)

    assert check(dst_feature, 'field_integer64', None)

    assert check(dst_feature, 'field_real', None)

    assert check(dst_feature, 'field_string', None)

    assert check(dst_feature, 'field_binary', None)

    assert check(dst_feature, 'field_date', '2011/11/11')

    assert check(dst_feature, 'field_time', '0000/00/00')

    assert check(dst_feature, 'field_datetime', '2011/11/11')

    assert check(dst_feature, 'field_integerlist', None)

    assert check(dst_feature, 'field_integer64list', None)

    assert check(dst_feature, 'field_reallist', None)

    assert check(dst_feature, 'field_stringlist', None)

###############################################################################
# Copy to time


def test_ogr_feature_cp_time():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature(src_feature, ogr.OFTTime)
    dst_feature.SetFrom(src_feature)

    assert check(dst_feature, 'field_integer', None)

    assert check(dst_feature, 'field_integer64', None)

    assert check(dst_feature, 'field_real', None)

    assert check(dst_feature, 'field_string', None)

    assert check(dst_feature, 'field_binary', None)

    assert check(dst_feature, 'field_date', '00:00:00')

    assert check(dst_feature, 'field_time', '14:10:35')

    assert check(dst_feature, 'field_datetime', '14:10:35.123')

    assert check(dst_feature, 'field_integerlist', None)

    assert check(dst_feature, 'field_integer64list', None)

    assert check(dst_feature, 'field_reallist', None)

    assert check(dst_feature, 'field_stringlist', None)

###############################################################################
# Copy to datetime


def test_ogr_feature_cp_datetime():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature(src_feature, ogr.OFTDateTime)
    dst_feature.SetFrom(src_feature)

    assert check(dst_feature, 'field_integer', None)

    assert check(dst_feature, 'field_integer64', None)

    assert check(dst_feature, 'field_real', None)

    assert check(dst_feature, 'field_string', None)

    assert check(dst_feature, 'field_binary', None)

    assert check(dst_feature, 'field_date', '2011/11/11 00:00:00')

    assert check(dst_feature, 'field_time', '0000/00/00 14:10:35')

    assert check(dst_feature, 'field_datetime', '2011/11/11 14:10:35.123')

    assert check(dst_feature, 'field_integerlist', None)

    assert check(dst_feature, 'field_integer64list', None)

    assert check(dst_feature, 'field_reallist', None)

    assert check(dst_feature, 'field_stringlist', None)

###############################################################################
# Copy to integerlist


def test_ogr_feature_cp_integerlist():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature(src_feature, ogr.OFTIntegerList)
    gdal.PushErrorHandler()
    dst_feature.SetFrom(src_feature)
    gdal.PopErrorHandler()

    assert check(dst_feature, 'field_integer', [17])

    assert check(dst_feature, 'field_integer64', [2147483647])

    assert check(dst_feature, 'field_real', [18])

    assert check(dst_feature, 'field_string', None)

    assert check(dst_feature, 'field_binary', None)

    assert check(dst_feature, 'field_date', None)

    assert check(dst_feature, 'field_time', None)

    assert check(dst_feature, 'field_datetime', None)

    assert check(dst_feature, 'field_integerlist', [10, 20, 30])

    assert check(dst_feature, 'field_integer64list', [2147483647])

    assert check(dst_feature, 'field_reallist', [123, 567])

    assert check(dst_feature, 'field_stringlist', None)

###############################################################################
# Copy to integer64list


def test_ogr_feature_cp_integer64list():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature(src_feature, ogr.OFTInteger64List)
    dst_feature.SetFrom(src_feature)

    assert check(dst_feature, 'field_integer', [17])

    assert check(dst_feature, 'field_integer64', [9876543210])

    assert check(dst_feature, 'field_real', [18])

    assert check(dst_feature, 'field_string', None)

    assert check(dst_feature, 'field_binary', None)

    assert check(dst_feature, 'field_date', None)

    assert check(dst_feature, 'field_time', None)

    assert check(dst_feature, 'field_datetime', None)

    assert check(dst_feature, 'field_integerlist', [10, 20, 30])

    assert check(dst_feature, 'field_integer64list', [9876543210])

    assert check(dst_feature, 'field_reallist', [123, 567])

    assert check(dst_feature, 'field_stringlist', None)

###############################################################################
# Copy to reallist


def test_ogr_feature_cp_reallist():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature(src_feature, ogr.OFTRealList)
    dst_feature.SetFrom(src_feature)

    assert check(dst_feature, 'field_integer', [17.0])

    assert check(dst_feature, 'field_integer64', [9876543210.0])

    assert check(dst_feature, 'field_real', [18.4])

    assert check(dst_feature, 'field_string', None)

    assert check(dst_feature, 'field_binary', None)

    assert check(dst_feature, 'field_date', None)

    assert check(dst_feature, 'field_time', None)

    assert check(dst_feature, 'field_datetime', None)

    assert check(dst_feature, 'field_integerlist', [10.0, 20.0, 30.0])

    assert check(dst_feature, 'field_integer64list', [9876543210.0])

    assert check(dst_feature, 'field_reallist', [123.5, 567.0])

    assert check(dst_feature, 'field_stringlist', None)

###############################################################################
# Copy to stringlist


def test_ogr_feature_cp_stringlist():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature(src_feature, ogr.OFTStringList)
    dst_feature.SetFrom(src_feature)

    assert check(dst_feature, 'field_integer', ["17"])

    assert check(dst_feature, 'field_integer64', ["9876543210"])

    assert check(dst_feature, 'field_real', ["18.4"])

    assert check(dst_feature, 'field_string', ['abc def'])

    assert check(dst_feature, 'field_binary', ['0123465789ABCDEF'])

    assert check(dst_feature, 'field_date', ['2011/11/11'])

    assert check(dst_feature, 'field_time', ['14:10:35'])

    assert check(dst_feature, 'field_datetime', ['2011/11/11 14:10:35.123'])

    assert check(dst_feature, 'field_integerlist', ['10', '20', '30'])

    assert check(dst_feature, 'field_integer64list', ['9876543210'])

    assert check(dst_feature, 'field_reallist', ['123.5', '567'])

    assert check(dst_feature, 'field_stringlist', ['abc', 'def'])


###############################################################################
# Test SetField() / GetField() with unicode string

def test_ogr_feature_unicode():
    feat_def = ogr.FeatureDefn('test')

    field_def = ogr.FieldDefn('field_string', ogr.OFTString)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_integer64', ogr.OFTInteger64)
    feat_def.AddFieldDefn(field_def)

    src_feature = ogr.Feature(feat_def)
    src_feature.SetField('field_string', 'abc def')
    assert src_feature.GetField('field_string') == 'abc def'

    src_feature = ogr.Feature(feat_def)
    src_feature.SetField('field_string', 'abc def')
    assert src_feature.GetField('field_string') == 'abc def'

    src_feature = ogr.Feature(feat_def)
    src_feature.SetField('field_integer64', 1)
    assert src_feature.GetField('field_integer64') == 1

###############################################################################
# Test 64bit FID


def test_ogr_feature_64bit_fid():

    feat_def = ogr.FeatureDefn('test')
    f = ogr.Feature(feat_def)
    f.SetFID(123456789012345)
    assert f.GetFID() == 123456789012345

###############################################################################
# Test 64bit integer


def test_ogr_feature_overflow_64bit_integer():

    feat_def = ogr.FeatureDefn('test')
    feat_def.AddFieldDefn(ogr.FieldDefn('test', ogr.OFTInteger64))
    f = ogr.Feature(feat_def)
    gdal.PushErrorHandler()
    f.SetField(0, '9999999999999999999')
    gdal.PopErrorHandler()
    if f.GetField(0) != 9223372036854775807:
        f.DumpReadable()
        pytest.fail()
    gdal.PushErrorHandler()
    f.SetField(0, '-9999999999999999999')
    gdal.PopErrorHandler()
    if f.GetField(0) != -9223372036854775808:
        f.DumpReadable()
        pytest.fail()
    
###############################################################################
# Test SetNullable(), IsNullable() and Validate()


def test_ogr_feature_nullable_validate():
    # No fields
    feat_def = ogr.FeatureDefn('test')
    f = ogr.Feature(feat_def)
    assert f.Validate() == 1

    # Field with default parameter and empty feature
    feat_def = ogr.FeatureDefn('test')
    field_def = ogr.FieldDefn('field_string', ogr.OFTString)
    assert field_def.IsNullable() == 1
    assert feat_def.GetGeomFieldDefn(0).IsNullable() == 1
    feat_def.AddFieldDefn(field_def)
    f = ogr.Feature(feat_def)
    assert f.Validate() == 1

    # Field with not NULL constraint and empty feature
    feat_def = ogr.FeatureDefn('test')
    field_def = ogr.FieldDefn('field_string', ogr.OFTString)
    field_def.SetNullable(0)
    assert field_def.IsNullable() == 0
    feat_def.AddFieldDefn(field_def)
    f = ogr.Feature(feat_def)
    gdal.PushErrorHandler()
    ret = f.Validate()
    gdal.PopErrorHandler()
    assert ret == 0

    # Field with not NULL constraint and non-empty feature
    feat_def = ogr.FeatureDefn('test')
    field_def = ogr.FieldDefn('field_string', ogr.OFTString)
    field_def.SetNullable(0)
    feat_def.AddFieldDefn(field_def)
    f = ogr.Feature(feat_def)
    f.SetField(0, 'foo')
    assert f.Validate() == 1

    # Field with width constraint that is met
    feat_def = ogr.FeatureDefn('test')
    field_def = ogr.FieldDefn('field_string', ogr.OFTString)
    field_def.SetWidth(3)
    feat_def.AddFieldDefn(field_def)
    f = ogr.Feature(feat_def)
    f.SetField(0, 'foo')
    assert f.Validate() == 1

    # Field with width constraint that is not met
    feat_def = ogr.FeatureDefn('test')
    field_def = ogr.FieldDefn('field_string', ogr.OFTString)
    field_def.SetWidth(2)
    feat_def.AddFieldDefn(field_def)
    f = ogr.Feature(feat_def)
    f.SetField(0, 'foo')
    gdal.PushErrorHandler()
    ret = f.Validate()
    gdal.PopErrorHandler()
    assert ret == 0

    # Geometry field with not-null geometry constraint that is met
    feat_def = ogr.FeatureDefn('test')
    feat_def.SetGeomType(ogr.wkbNone)
    gfield_def = ogr.GeomFieldDefn('test', ogr.wkbUnknown)
    gfield_def.SetNullable(0)
    assert gfield_def.IsNullable() == 0
    feat_def.AddGeomFieldDefn(gfield_def)
    f = ogr.Feature(feat_def)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 1)'))
    assert f.Validate() == 1

    # Geometry field with not-null geometry constraint that is not met
    feat_def = ogr.FeatureDefn('test')
    feat_def.SetGeomType(ogr.wkbNone)
    gfield_def = ogr.GeomFieldDefn('test', ogr.wkbPoint)
    gfield_def.SetNullable(0)
    feat_def.AddGeomFieldDefn(gfield_def)
    f = ogr.Feature(feat_def)
    gdal.PushErrorHandler()
    ret = f.Validate()
    gdal.PopErrorHandler()
    assert ret == 0

    # Geometry field with Point geometry type that is met
    feat_def = ogr.FeatureDefn('test')
    feat_def.SetGeomType(ogr.wkbPoint)
    f = ogr.Feature(feat_def)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 1)'))
    assert f.Validate() == 1

    # Geometry field with LineString geometry type that is not met
    feat_def = ogr.FeatureDefn('test')
    feat_def.SetGeomType(ogr.wkbLineString)
    f = ogr.Feature(feat_def)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 1)'))
    gdal.PushErrorHandler()
    ret = f.Validate()
    gdal.PopErrorHandler()
    assert ret == 0

###############################################################################
# Test SetDefault(), GetDefault(), IsDefaultDriverSpecific() and FillUnsetWithDefault()


def test_ogr_feature_default():

    feat_def = ogr.FeatureDefn('test')
    field_def = ogr.FieldDefn('field_string', ogr.OFTString)

    assert field_def.GetDefault() is None
    assert not field_def.IsDefaultDriverSpecific()

    field_def.SetDefault("(some_expr)")
    assert field_def.GetDefault() == "(some_expr)"
    assert field_def.IsDefaultDriverSpecific()

    field_def.SetDefault("'a")
    assert field_def.GetDefault() == "'a"

    gdal.PushErrorHandler()
    field_def.SetDefault("'a''")
    gdal.PopErrorHandler()
    assert field_def.GetDefault() is None

    gdal.PushErrorHandler()
    field_def.SetDefault("'a'b'")
    gdal.PopErrorHandler()
    assert field_def.GetDefault() is None

    field_def.SetDefault("'a''b'''")
    assert field_def.GetDefault() == "'a''b'''"
    assert not field_def.IsDefaultDriverSpecific()
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_datetime', ogr.OFTDateTime)
    field_def.SetDefault("CURRENT_TIMESTAMP")
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_datetime2', ogr.OFTDateTime)
    field_def.SetDefault("'2015/06/30 12:34:56'")
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_int', ogr.OFTInteger)
    field_def.SetDefault('123')
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_nodefault', ogr.OFTInteger)
    feat_def.AddFieldDefn(field_def)

    f = ogr.Feature(feat_def)
    f.FillUnsetWithDefault()
    if f.GetField('field_string') != 'a\'b\'' or \
       not f.IsFieldSet('field_datetime') or \
       f.GetField('field_datetime2') != '2015/06/30 12:34:56+00' or \
       f.GetField('field_int') != 123 or \
       f.IsFieldSet('field_nodefault'):
        f.DumpReadable()
        pytest.fail()

    f = ogr.Feature(feat_def)
    f.SetField('field_string', 'b')
    f.FillUnsetWithDefault()
    if f.GetField('field_string') != 'b':
        f.DumpReadable()
        pytest.fail()

    
###############################################################################
# Test GetNativeData(), SetNativeData(), GetNativeMediaType(), SetNativeMediaType():


def test_ogr_feature_native_data():

    feat_def = ogr.FeatureDefn('test')
    f = ogr.Feature(feat_def)
    assert f.GetNativeData() is None
    assert f.GetNativeMediaType() is None

    f.SetNativeData('native_data')
    assert f.GetNativeData() == 'native_data'
    f.SetNativeMediaType('native_media_type')
    assert f.GetNativeMediaType() == 'native_media_type'

    f2 = ogr.Feature(feat_def)
    f2.SetFrom(f)
    assert f2.GetNativeData() == 'native_data'
    assert f2.GetNativeMediaType() == 'native_media_type'

    f_clone = f.Clone()
    assert f_clone.GetNativeData() == 'native_data'
    assert f_clone.GetNativeMediaType() == 'native_media_type'
    f_clone.SetNativeData(None)
    f_clone.SetNativeMediaType(None)
    assert f_clone.GetNativeData() is None
    assert f_clone.GetNativeMediaType() is None

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('test')
    lyr.SetMetadataItem('NATIVE_DATA', 'native_data', 'NATIVE_DATA')
    lyr.SetMetadataItem('NATIVE_MEDIA_TYPE', 'native_media_type', 'NATIVE_DATA')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetNativeData('native_data')
    f.SetNativeMediaType('native_media_type')
    lyr.CreateFeature(f)
    f = None

    dialects = ['OGR_SQL']
    if gdal.GetDriverByName('SQLITE') is not None:
        dialects += ['OGR_SQLITE']
    for dialect in dialects:
        sql_lyr = ds.ExecuteSQL('SELECT * FROM %s' % lyr.GetName(), dialect=dialect)
        native_data = sql_lyr.GetMetadataItem('NATIVE_DATA', 'NATIVE_DATA')
        assert native_data == 'native_data', dialect
        native_media_type = sql_lyr.GetMetadataItem('NATIVE_MEDIA_TYPE', 'NATIVE_DATA')
        assert native_media_type == 'native_media_type', dialect
        f = sql_lyr.GetNextFeature()
        assert f.GetNativeData() == 'native_data', dialect
        assert f.GetNativeMediaType() == 'native_media_type', dialect
        ds.ReleaseResultSet(sql_lyr)

    
###############################################################################
# Test assigning our geometry to ourselves


def test_ogr_feature_set_geometry_self():

    feat_def = ogr.FeatureDefn('test')
    f = ogr.Feature(feat_def)
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (2 49)'))
    f.SetGeometryDirectly(f.GetGeometryRef())
    f.SetGeometryDirectly(f.GetGeometryRef())
    f.SetGeometry(f.GetGeometryRef())
    f.SetGeometry(f.GetGeometryRef())
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (2 49)'

###############################################################################
# Test SetFieldNull(), IsFieldNull()


def test_ogr_feature_null_field():

    feat_def = ogr.FeatureDefn('test')
    field_def = ogr.FieldDefn('field_string', ogr.OFTString)
    feat_def.AddFieldDefn(field_def)
    f = ogr.Feature(feat_def)
    assert not f.IsFieldNull(feat_def.GetFieldIndex("field_string"))
    assert not f.IsFieldNull("field_string")
    f.SetFieldNull(feat_def.GetFieldIndex("field_string"))
    assert f.IsFieldNull(feat_def.GetFieldIndex("field_string")) != 0
    f.SetField("field_string", "foo")
    assert not f.IsFieldNull("field_string")
    f.SetFieldNull("field_string")
    assert f.IsFieldNull(feat_def.GetFieldIndex("field_string")) != 0
    f = None

    field_def = ogr.FieldDefn('field_binary', ogr.OFTBinary)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_integerlist', ogr.OFTIntegerList)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_integer64list', ogr.OFTInteger64List)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_reallist', ogr.OFTRealList)
    feat_def.AddFieldDefn(field_def)

    field_def = ogr.FieldDefn('field_stringlist', ogr.OFTStringList)
    feat_def.AddFieldDefn(field_def)

    f = ogr.Feature(feat_def)
    f.SetFieldBinaryFromHexString('field_binary', '0123465789ABCDEF')
    f.field_integerlist = '(3:10,20,30)'
    f.field_integer64list = [9876543210]
    f.field_reallist = [123.5, 567.0]
    f.field_stringlist = ['abc', 'def']
    assert f.IsFieldNull('field_binary') == 0
    assert f.IsFieldNull('field_integerlist') == 0
    assert f.IsFieldNull('field_integer64list') == 0
    assert f.IsFieldNull('field_reallist') == 0
    assert f.IsFieldNull('field_stringlist') == 0
    f.SetField('field_binary', None)
    f.SetFieldNull('field_integerlist')
    f.SetFieldNull('field_integer64list')
    f.SetFieldNull('field_reallist')
    f.SetFieldNull('field_stringlist')
    assert f.IsFieldNull('field_binary') != 0
    assert f.IsFieldNull('field_integerlist') != 0
    assert f.IsFieldNull('field_integer64list') != 0
    assert f.IsFieldNull('field_reallist') != 0
    assert f.IsFieldNull('field_stringlist') != 0

    f_clone = f.Clone()
    assert f_clone.IsFieldNull('field_binary') != 0
    assert f.Equal(f_clone)

    f = None
