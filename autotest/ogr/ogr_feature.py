#!/usr/bin/env python
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

import sys

sys.path.append( '../pymod' )

from osgeo import gdal, ogr
import gdaltest

###############################################################################
# Create a destination feature type with one field for each field in the source
# feature, with the same names, but all the field types of a specific type.

def mk_dst_feature( src_feature, field_type ):
    
    dst_feat_defn = ogr.FeatureDefn( 'dst' )

    src_feat_defn = src_feature.GetDefnRef()
    for i in range(src_feat_defn.GetFieldCount()):
        src_field_defn = src_feat_defn.GetFieldDefn(i)
        dst_field_defn = ogr.FieldDefn( src_field_defn.GetName(), field_type )
        dst_feat_defn.AddFieldDefn( dst_field_defn )

    return ogr.Feature( dst_feat_defn )

###############################################################################
# Create a source feature 

def mk_src_feature():
    
    feat_def = ogr.FeatureDefn( 'src' )
    
    field_def = ogr.FieldDefn( 'field_integer', ogr.OFTInteger )
    feat_def.AddFieldDefn( field_def )
    
    field_def = ogr.FieldDefn( 'field_integer64', ogr.OFTInteger64 )
    feat_def.AddFieldDefn( field_def )
    
    field_def = ogr.FieldDefn( 'field_real', ogr.OFTReal )
    feat_def.AddFieldDefn( field_def )

    field_def = ogr.FieldDefn( 'field_string', ogr.OFTString )
    feat_def.AddFieldDefn( field_def )

    field_def = ogr.FieldDefn( 'field_binary', ogr.OFTBinary )
    feat_def.AddFieldDefn( field_def )

    field_def = ogr.FieldDefn( 'field_date', ogr.OFTDate )
    feat_def.AddFieldDefn( field_def )

    field_def = ogr.FieldDefn( 'field_time', ogr.OFTTime )
    feat_def.AddFieldDefn( field_def )

    field_def = ogr.FieldDefn( 'field_datetime', ogr.OFTDateTime )
    feat_def.AddFieldDefn( field_def )

    field_def = ogr.FieldDefn( 'field_integerlist', ogr.OFTIntegerList )
    feat_def.AddFieldDefn( field_def )

    field_def = ogr.FieldDefn( 'field_integer64list', ogr.OFTInteger64List )
    feat_def.AddFieldDefn( field_def )

    field_def = ogr.FieldDefn( 'field_reallist', ogr.OFTRealList )
    feat_def.AddFieldDefn( field_def )

    field_def = ogr.FieldDefn( 'field_stringlist', ogr.OFTStringList )
    feat_def.AddFieldDefn( field_def )

    src_feature = ogr.Feature( feat_def )
    src_feature.SetField( 'field_integer', 17 )
    src_feature.SetField( 'field_integer64', 9876543210 )
    src_feature.SetField( 'field_real', 18.4 )
    src_feature.SetField( 'field_string', 'abc def' )
    src_feature.SetFieldBinaryFromHexString( 'field_binary', '0123465789ABCDEF' )
    src_feature.SetField( 'field_date', '2011/11/11' )
    src_feature.SetField( 'field_time', '14:10:35' )
    src_feature.SetField( 'field_datetime', '2011/11/11 14:10:35')
    src_feature.field_integerlist = '(3:10,20,30)'
    src_feature.field_integer64list = [9876543210]
    src_feature.field_reallist = [123.5,567.0]
    src_feature.field_stringlist = ['abc','def']

    return src_feature

###############################################################################
# Helper function to check a single field value

def check( feat, fieldname, value ):
    if feat.GetField( fieldname ) != value:
        gdaltest.post_reason( 'did not get value %s for field %s, got %s.' \
                              % (str(value), fieldname,
                                 str(feat.GetField(fieldname))),
                              frames = 3 )
        feat.DumpReadable()
        return 0
    else:
        return 1
    
    
###############################################################################
# Copy to Integer

def ogr_feature_cp_integer():
    src_feature = mk_src_feature()
    src_feature.field_integerlist = [15]
    src_feature.field_reallist = [17.5]

    dst_feature = mk_dst_feature( src_feature, ogr.OFTInteger )
    gdal.PushErrorHandler()
    dst_feature.SetFrom( src_feature )
    gdal.PopErrorHandler()

    if not check( dst_feature, 'field_integer', 17 ):
        return 'failure'

    if not check( dst_feature, 'field_integer64', 2147483647 ):
        return 'failure'
    
    if not check( dst_feature, 'field_real', 18 ):
        return 'failure'

    if not check( dst_feature, 'field_string', 0 ):
        return 'failure'

    if not check( dst_feature, 'field_binary', None ):
        return 'failure'

    if not check( dst_feature, 'field_date', None ):
        return 'failure'

    if not check( dst_feature, 'field_time', None ):
        return 'failure'

    if not check( dst_feature, 'field_datetime', None ):
        return 'failure'

    if not check( dst_feature, 'field_integerlist', 15 ):
        return 'failure'

    if not check( dst_feature, 'field_integer64list', 2147483647 ):
        return 'failure'
    
    if not check( dst_feature, 'field_reallist', 17 ):
        return 'failure'
    
    if not check( dst_feature, 'field_stringlist', None ):
        return 'failure'
    
    return 'success'
    
###############################################################################
# Copy to Integer64

def ogr_feature_cp_integer64():
    src_feature = mk_src_feature()
    src_feature.field_integerlist = [15]
    src_feature.field_reallist = [17.5]

    dst_feature = mk_dst_feature( src_feature, ogr.OFTInteger64 )
    dst_feature.SetFrom( src_feature )

    if not check( dst_feature, 'field_integer', 17 ):
        return 'failure'

    if not check( dst_feature, 'field_integer64', 9876543210 ):
        return 'failure'
    
    gdal.PushErrorHandler()
    int32_ovflw = dst_feature.GetFieldAsInteger('field_integer64')
    gdal.PopErrorHandler()
    if int32_ovflw != 2147483647:
        gdaltest.post_reason('fail')
        return 'failure'
    
    if not check( dst_feature, 'field_real', 18 ):
        return 'failure'

    if not check( dst_feature, 'field_string', 0 ):
        return 'failure'

    if not check( dst_feature, 'field_binary', None ):
        return 'failure'

    if not check( dst_feature, 'field_date', None ):
        return 'failure'

    if not check( dst_feature, 'field_time', None ):
        return 'failure'

    if not check( dst_feature, 'field_datetime', None ):
        return 'failure'

    if not check( dst_feature, 'field_integerlist', 15 ):
        return 'failure'

    if not check( dst_feature, 'field_integer64list', 9876543210 ):
        return 'failure'
    
    if not check( dst_feature, 'field_reallist', 17 ):
        return 'failure'
    
    if not check( dst_feature, 'field_stringlist', None ):
        return 'failure'
    
    return 'success'

###############################################################################
# Copy to Real

def ogr_feature_cp_real():
    src_feature = mk_src_feature()
    src_feature.field_integerlist = [15]
    src_feature.field_reallist = [17.5]

    dst_feature = mk_dst_feature( src_feature, ogr.OFTReal )
    dst_feature.SetFrom( src_feature )

    if not check( dst_feature, 'field_integer', 17.0 ):
        return 'failure'

    if not check( dst_feature, 'field_real', 18.4 ):
        return 'failure'

    if not check( dst_feature, 'field_string', 0 ):
        return 'failure'

    if not check( dst_feature, 'field_binary', None ):
        return 'failure'

    if not check( dst_feature, 'field_date', None ):
        return 'failure'

    if not check( dst_feature, 'field_time', None ):
        return 'failure'

    if not check( dst_feature, 'field_datetime', None ):
        return 'failure'

    if not check( dst_feature, 'field_integerlist', 15.0 ):
        return 'failure'

    if not check( dst_feature, 'field_reallist', 17.5 ):
        return 'failure'
    
    if not check( dst_feature, 'field_stringlist', None ):
        return 'failure'
    
    return 'success'

###############################################################################
# Copy to String

def ogr_feature_cp_string():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature( src_feature, ogr.OFTString )
    dst_feature.SetFrom( src_feature )

    if not check( dst_feature, 'field_integer', '17' ):
        return 'failure'

    if not check( dst_feature, 'field_integer64', '9876543210' ):
        return 'failure'

    if not check( dst_feature, 'field_real', '18.4' ):
        return 'failure'

    if not check( dst_feature, 'field_string', 'abc def' ):
        return 'failure'

    if not check( dst_feature, 'field_binary', '0123465789ABCDEF' ):
        return 'failure'

    if not check( dst_feature, 'field_date', '2011/11/11' ):
        return 'failure'

    if not check( dst_feature, 'field_time', '14:10:35' ):
        return 'failure'

    if not check( dst_feature, 'field_datetime', '2011/11/11 14:10:35' ):
        return 'failure'

    if not check( dst_feature, 'field_integerlist', '(3:10,20,30)' ):
        return 'failure'

    if not check( dst_feature, 'field_integer64list', '(1:9876543210)' ):
        return 'failure'
    
    if not check( dst_feature, 'field_reallist', '(2:123.5,567)' ):
        return 'failure'
    
    if not check( dst_feature, 'field_stringlist', '(2:abc,def)' ):
        return 'failure'
    
    return 'success'

###############################################################################
# Copy to Binary

def ogr_feature_cp_binary():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature( src_feature, ogr.OFTBinary )
    dst_feature.SetFrom( src_feature )

    if not check( dst_feature, 'field_integer', None ):
        return 'failure'

    if not check( dst_feature, 'field_integer64', None ):
        return 'failure'
    
    if not check( dst_feature, 'field_real', None ):
        return 'failure'

    if not check( dst_feature, 'field_string', None ):
        return 'failure'

    if not check( dst_feature, 'field_binary', '0123465789ABCDEF' ):
        return 'failure'

    expected = '\x01\x23\x46\x57\x89\xAB\xCD\xEF'
    if sys.version_info >= (3,0,0):
        expected = expected.encode('LATIN1')
    if dst_feature.GetFieldAsBinary('field_binary') != expected:
        return 'failure'
    if dst_feature.GetFieldAsBinary(dst_feature.GetDefnRef().GetFieldIndex('field_binary')) != expected:
        return 'failure'

    if not check( dst_feature, 'field_date', None ):
        return 'failure'

    if not check( dst_feature, 'field_time', None ):
        return 'failure'

    if not check( dst_feature, 'field_datetime', None ):
        return 'failure'

    if not check( dst_feature, 'field_integerlist', None ):
        return 'failure'

    if not check( dst_feature, 'field_integer64list', None ):
        return 'failure'
    
    if not check( dst_feature, 'field_reallist', None ):
        return 'failure'
    
    if not check( dst_feature, 'field_stringlist', None ):
        return 'failure'
    
    return 'success'

###############################################################################
# Copy to date

def ogr_feature_cp_date():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature( src_feature, ogr.OFTDate )
    dst_feature.SetFrom( src_feature )

    if not check( dst_feature, 'field_integer', None ):
        return 'failure'

    if not check( dst_feature, 'field_integer64', None ):
        return 'failure'
    
    if not check( dst_feature, 'field_real', None ):
        return 'failure'

    if not check( dst_feature, 'field_string', None ):
        return 'failure'

    if not check( dst_feature, 'field_binary', None ):
        return 'failure'

    if not check( dst_feature, 'field_date', '2011/11/11' ):
        return 'failure'

    if not check( dst_feature, 'field_time', '0000/00/00' ):
        return 'failure'

    if not check( dst_feature, 'field_datetime', '2011/11/11' ):
        return 'failure'

    if not check( dst_feature, 'field_integerlist', None ):
        return 'failure'

    if not check( dst_feature, 'field_integer64list', None ):
        return 'failure'
    
    if not check( dst_feature, 'field_reallist', None ):
        return 'failure'
    
    if not check( dst_feature, 'field_stringlist', None ):
        return 'failure'
    
    return 'success'

###############################################################################
# Copy to time

def ogr_feature_cp_time():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature( src_feature, ogr.OFTTime )
    dst_feature.SetFrom( src_feature )

    if not check( dst_feature, 'field_integer', None ):
        return 'failure'

    if not check( dst_feature, 'field_integer64', None ):
        return 'failure'
    
    if not check( dst_feature, 'field_real', None ):
        return 'failure'

    if not check( dst_feature, 'field_string', None ):
        return 'failure'

    if not check( dst_feature, 'field_binary', None ):
        return 'failure'

    if not check( dst_feature, 'field_date', ' 0:00:00' ):
        return 'failure'

    if not check( dst_feature, 'field_time', '14:10:35' ):
        return 'failure'

    if not check( dst_feature, 'field_datetime', '14:10:35' ):
        return 'failure'

    if not check( dst_feature, 'field_integerlist', None ):
        return 'failure'

    if not check( dst_feature, 'field_integer64list', None ):
        return 'failure'
    
    if not check( dst_feature, 'field_reallist', None ):
        return 'failure'
    
    if not check( dst_feature, 'field_stringlist', None ):
        return 'failure'
    
    return 'success'

###############################################################################
# Copy to datetime

def ogr_feature_cp_datetime():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature( src_feature, ogr.OFTDateTime )
    dst_feature.SetFrom( src_feature )

    if not check( dst_feature, 'field_integer', None ):
        return 'failure'

    if not check( dst_feature, 'field_integer64', None ):
        return 'failure'
    
    if not check( dst_feature, 'field_real', None ):
        return 'failure'

    if not check( dst_feature, 'field_string', None ):
        return 'failure'

    if not check( dst_feature, 'field_binary', None ):
        return 'failure'

    if not check( dst_feature, 'field_date', '2011/11/11 00:00:00' ):
        return 'failure'

    if not check( dst_feature, 'field_time', '0000/00/00 14:10:35' ):
        return 'failure'

    if not check( dst_feature, 'field_datetime', '2011/11/11 14:10:35' ):
        return 'failure'

    if not check( dst_feature, 'field_integerlist', None ):
        return 'failure'

    if not check( dst_feature, 'field_integer64list', None ):
        return 'failure'
    
    if not check( dst_feature, 'field_reallist', None ):
        return 'failure'
    
    if not check( dst_feature, 'field_stringlist', None ):
        return 'failure'
    
    return 'success'

###############################################################################
# Copy to integerlist

def ogr_feature_cp_integerlist():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature( src_feature, ogr.OFTIntegerList )
    gdal.PushErrorHandler()
    dst_feature.SetFrom( src_feature )
    gdal.PopErrorHandler()

    if not check( dst_feature, 'field_integer', [17] ):
        return 'failure'

    if not check( dst_feature, 'field_integer64', [2147483647] ):
        return 'failure'
    
    if not check( dst_feature, 'field_real', [18] ):
        return 'failure'

    if not check( dst_feature, 'field_string', None ):
        return 'failure'

    if not check( dst_feature, 'field_binary', None ):
        return 'failure'

    if not check( dst_feature, 'field_date', None ):
        return 'failure'

    if not check( dst_feature, 'field_time', None ):
        return 'failure'

    if not check( dst_feature, 'field_datetime', None ):
        return 'failure'

    if not check( dst_feature, 'field_integerlist', [10, 20, 30] ):
        return 'failure'

    if not check( dst_feature, 'field_integer64list', [2147483647] ):
        return 'failure'
    
    if not check( dst_feature, 'field_reallist', [123,567] ):
        return 'failure'
    
    if not check( dst_feature, 'field_stringlist', None ):
        return 'failure'
    
    return 'success'

###############################################################################
# Copy to integer64list

def ogr_feature_cp_integer64list():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature( src_feature, ogr.OFTInteger64List )
    dst_feature.SetFrom( src_feature )

    if not check( dst_feature, 'field_integer', [17] ):
        return 'failure'

    if not check( dst_feature, 'field_integer64', [9876543210] ):
        return 'failure'
    
    if not check( dst_feature, 'field_real', [18] ):
        return 'failure'

    if not check( dst_feature, 'field_string', None ):
        return 'failure'

    if not check( dst_feature, 'field_binary', None ):
        return 'failure'

    if not check( dst_feature, 'field_date', None ):
        return 'failure'

    if not check( dst_feature, 'field_time', None ):
        return 'failure'

    if not check( dst_feature, 'field_datetime', None ):
        return 'failure'

    if not check( dst_feature, 'field_integerlist', [10, 20, 30] ):
        return 'failure'

    if not check( dst_feature, 'field_integer64list', [9876543210] ):
        return 'failure'
    
    if not check( dst_feature, 'field_reallist', [123,567] ):
        return 'failure'
    
    if not check( dst_feature, 'field_stringlist', None ):
        return 'failure'
    
    return 'success'

###############################################################################
# Copy to reallist

def ogr_feature_cp_reallist():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature( src_feature, ogr.OFTRealList )
    dst_feature.SetFrom( src_feature )

    if not check( dst_feature, 'field_integer', [17.0] ):
        return 'failure'

    if not check( dst_feature, 'field_integer64', [9876543210.0] ):
        return 'failure'
    
    if not check( dst_feature, 'field_real', [18.4] ):
        return 'failure'

    if not check( dst_feature, 'field_string', None ):
        return 'failure'

    if not check( dst_feature, 'field_binary', None ):
        return 'failure'

    if not check( dst_feature, 'field_date', None ):
        return 'failure'

    if not check( dst_feature, 'field_time', None ):
        return 'failure'

    if not check( dst_feature, 'field_datetime', None ):
        return 'failure'

    if not check( dst_feature, 'field_integerlist', [10.0,20.0,30.0] ):
        return 'failure'

    if not check( dst_feature, 'field_integer64list', [9876543210.0] ):
        return 'failure'
    
    if not check( dst_feature, 'field_reallist', [123.5, 567.0] ):
        return 'failure'
    
    if not check( dst_feature, 'field_stringlist', None ):
        return 'failure'
    
    return 'success'

###############################################################################
# Copy to stringlist

def ogr_feature_cp_stringlist():
    src_feature = mk_src_feature()

    dst_feature = mk_dst_feature( src_feature, ogr.OFTStringList )
    dst_feature.SetFrom( src_feature )

    if not check( dst_feature, 'field_integer', None ):
        return 'failure'

    if not check( dst_feature, 'field_integer64', None ):
        return 'failure'

    if not check( dst_feature, 'field_real', None ):
        return 'failure'

    if not check( dst_feature, 'field_string', ['abc def'] ):
        return 'failure'

    if not check( dst_feature, 'field_binary', None ):
        return 'failure'

    if not check( dst_feature, 'field_date', None ):
        return 'failure'

    if not check( dst_feature, 'field_time', None ):
        return 'failure'

    if not check( dst_feature, 'field_datetime', None ):
        return 'failure'

    if not check( dst_feature, 'field_integerlist', None ):
        return 'failure'

    if not check( dst_feature, 'field_integer64list', None ):
        return 'failure'

    if not check( dst_feature, 'field_reallist', None ):
        return 'failure'
    
    if not check( dst_feature, 'field_stringlist', ['abc', 'def'] ):
        return 'failure'
    
    return 'success'


###############################################################################
# Test SetField() with unicode string

def ogr_feature_unicode():
    if sys.version_info >= (3,0,0):
        return 'skip'

    feat_def = ogr.FeatureDefn( 'test' )

    field_def = ogr.FieldDefn( 'field_string', ogr.OFTString )
    feat_def.AddFieldDefn( field_def )

    src_feature = ogr.Feature( feat_def )
    src_feature.SetField( 'field_string', 'abc def'.decode('utf-8') )
    if src_feature.GetField('field_string') != 'abc def':
        return 'failure'

    return 'success'

###############################################################################
# Test 64bit FID

def ogr_feature_64bit_fid():
    
    feat_def = ogr.FeatureDefn( 'test' )
    f = ogr.Feature(feat_def)
    f.SetFID(123456789012345)
    if f.GetFID() != 123456789012345:
        return 'failure'
    return 'success'

###############################################################################
# Test 64bit integer

def ogr_feature_overflow_64bit_integer():
    
    feat_def = ogr.FeatureDefn( 'test' )
    feat_def.AddFieldDefn(ogr.FieldDefn('test', ogr.OFTInteger64))
    f = ogr.Feature(feat_def)
    gdal.PushErrorHandler()
    f.SetField(0, '9999999999999999999')
    gdal.PopErrorHandler()
    if f.GetField(0) != 9223372036854775807:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'failure'
    gdal.PushErrorHandler()
    f.SetField(0, '-9999999999999999999')
    gdal.PopErrorHandler()
    if f.GetField(0) != -9223372036854775808:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'failure'
    return 'success'

def ogr_feature_cleanup():

    gdaltest.src_feature = None

    return 'success'

gdaltest_list = [ 
    ogr_feature_cp_integer,
    ogr_feature_cp_integer64,
    ogr_feature_cp_real,
    ogr_feature_cp_string,
    ogr_feature_cp_binary,
    ogr_feature_cp_date,
    ogr_feature_cp_time,
    ogr_feature_cp_datetime,
    ogr_feature_cp_integerlist,
    ogr_feature_cp_integer64list,
    ogr_feature_cp_reallist,
    ogr_feature_cp_stringlist,
    ogr_feature_unicode,
    ogr_feature_64bit_fid,
    ogr_feature_overflow_64bit_integer,
    ogr_feature_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_feature' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

