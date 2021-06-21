#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR CSV driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal
import pytest

###############################################################################

@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    yield

    try:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ogr.GetDriverByName('CSV').DeleteDataSource('tmp/csvwrk')
        gdal.PopErrorHandler()
    except:
        pass

    try:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ogr.GetDriverByName('CSV').DeleteDataSource('tmp/ogr_csv_29')
        gdal.PopErrorHandler()
    except:
        pass


###############################################################################
# Check layer

def ogr_csv_check_layer(lyr, expect_code_as_numeric):

    if expect_code_as_numeric is True:
        expect = [8901, 8902, 8903, 8904]
    else:
        expect = ['8901', '8902', '8903', '8904']

    tr = ogrtest.check_features_against_list(lyr, 'PRIME_MERIDIAN_CODE', expect)
    assert tr

    lyr.ResetReading()

    expect = ['', 'Instituto Geografico e Cadastral; Lisbon',
              'Institut Geographique National (IGN), Paris',
              'Instituto Geografico "Augustin Cadazzi" (IGAC); Bogota']

    tr = ogrtest.check_features_against_list(lyr, 'INFORMATION_SOURCE', expect)
    assert tr

    lyr.ResetReading()

###############################################################################
# Verify the some attributes read properly.
#


def test_ogr_csv_2():
    csv_ds = ogr.Open('data/prime_meridian.csv')

    with gdaltest.error_handler():
        assert csv_ds.CreateLayer('foo') is None
        assert csv_ds.DeleteLayer(0) != 0

    lyr = csv_ds.GetLayerByName('prime_meridian')

    f = ogr.Feature(lyr.GetLayerDefn())
    with gdaltest.error_handler():
        assert lyr.CreateField(ogr.FieldDefn('foo')) != 0
        assert lyr.CreateFeature(f) != 0

    ogr_csv_check_layer(lyr, False)

###############################################################################
# Copy layer


def ogr_csv_copy_layer(csv_ds, csv_tmpds, layer_name, options):

    #######################################################
    # Create layer (.csv file)
    if options is None:
        new_lyr = csv_tmpds.CreateLayer(layer_name)
    else:
        new_lyr = csv_tmpds.CreateLayer(layer_name, options=options)

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(new_lyr,
                                   [('PRIME_MERIDIAN_CODE', ogr.OFTInteger),
                                    ('INFORMATION_SOURCE', ogr.OFTString)])

    #######################################################
    # Copy in matching prime meridian fields.

    dst_feat = ogr.Feature(feature_def=new_lyr.GetLayerDefn())

    srclyr = csv_ds.GetLayerByName('prime_meridian')
    srclyr.ResetReading()

    feat = srclyr.GetNextFeature()

    while feat is not None:

        dst_feat.SetFrom(feat)
        new_lyr.CreateFeature(dst_feat)

        feat = srclyr.GetNextFeature()

    return new_lyr

###############################################################################
# Copy prime_meridian.csv to a new subtree under the tmp directory.


def test_ogr_csv_3():
    csv_ds = ogr.Open('data/prime_meridian.csv')

    #######################################################
    # Ensure any old copy of our working datasource is cleaned up
    try:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ogr.GetDriverByName('CSV').DeleteDataSource('tmp/csvwrk')
        gdal.PopErrorHandler()
    except:
        pass

    #######################################################
    # Create CSV datasource (directory)
    csv_tmpds = \
        ogr.GetDriverByName('CSV').CreateDataSource('tmp/csvwrk')

    #######################################################
    # Create layer (.csv file)
    csv_lyr1 = ogr_csv_copy_layer(csv_ds, csv_tmpds, 'pm1', None)

    # No longer valid since we have editing capabilities
    if False:  # pylint: disable=using-constant-test
        # Check that we cannot add a new field now
        assert csv_lyr1.TestCapability(ogr.OLCCreateField) == 0
        field_defn = ogr.FieldDefn('dummy', ogr.OFTString)
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ret = csv_lyr1.CreateField(field_defn)
        gdal.PopErrorHandler()
        assert ret != 0

    # Verify the some attributes read properly.
    #
    # NOTE: one weird thing is that in this pass the prime_meridian_code field
    # is typed as integer instead of string since it is created literally.

    ogr_csv_check_layer(csv_lyr1, True)

###############################################################################
# Copy prime_meridian.csv again, in CRLF mode.


def test_ogr_csv_5():

    csv_ds = ogr.Open('data/prime_meridian.csv')
    csv_tmpds =  ogr.Open('tmp/csvwrk', update=1)

    #######################################################
    # Create layer (.csv file)
    csv_lyr2 = ogr_csv_copy_layer(csv_ds, csv_tmpds, 'pm2', ['LINEFORMAT=CRLF', ])

    ###############################################################################
    # Verify the some attributes read properly.
    #

    return ogr_csv_check_layer(csv_lyr2, True)

###############################################################################
# Delete a layer and verify it seems to have worked properly.
#


def test_ogr_csv_7():

    csv_tmpds = ogr.Open('tmp/csvwrk', update=1)

    idx = 0
    while idx < csv_tmpds.GetLayerCount():
        lyr = csv_tmpds.GetLayer(idx)
        if lyr.GetName() == 'pm1':
            break
        idx += 1
    assert lyr.GetName() == 'pm1', 'unexpected name for first layer'

    err = csv_tmpds.DeleteLayer(idx)

    assert err == 0, 'got error code from DeleteLayer'

    assert csv_tmpds.GetLayerCount() == 1 and csv_tmpds.GetLayer(0).GetName() == 'pm2', \
        'Layer not destroyed properly?'

    with gdaltest.error_handler():
        assert csv_tmpds.DeleteLayer(-1) != 0
        assert csv_tmpds.DeleteLayer(csv_tmpds.GetLayerCount()) != 0

###############################################################################
# Reopen and append a record then close.
#


def test_ogr_csv_8():

    ds = ogr.Open('tmp/csvwrk', update=1)

    lyr = ds.GetLayer(0)

    feat = ogr.Feature(feature_def=lyr.GetLayerDefn())

    feat.SetField('PRIME_MERIDIAN_CODE', '7000')
    feat.SetField('INFORMATION_SOURCE', 'This is a newline test\n')

    lyr.CreateFeature(feat)

###############################################################################
# Verify the some attributes read properly.
#


def test_ogr_csv_9():

    ds = ogr.Open('tmp/csvwrk', update=1)

    lyr = ds.GetLayer(0)

    expect = ['8901', '8902', '8903', '8904', '7000']

    tr = ogrtest.check_features_against_list(lyr, 'PRIME_MERIDIAN_CODE', expect)
    assert tr

    lyr.ResetReading()

    expect = ['', 'Instituto Geografico e Cadastral; Lisbon',
              'Institut Geographique National (IGN), Paris',
              'Instituto Geografico "Augustin Cadazzi" (IGAC); Bogota',
              'This is a newline test\n']

    tr = ogrtest.check_features_against_list(lyr, 'INFORMATION_SOURCE', expect)
    assert tr

###############################################################################
# Verify some capabilities and related stuff.
#


def test_ogr_csv_10():

    csv_ds = ogr.Open('data/prime_meridian.csv')
    lyr = csv_ds.GetLayerByName('prime_meridian')

    assert not lyr.TestCapability('SequentialWrite'), \
        'should not have write access to readonly layer'

    assert not lyr.TestCapability('RandomRead'), \
        ('CSV files do not efficiently support '
                             'random reading.')

    assert not lyr.TestCapability('FastGetExtent'), 'CSV files do not support getextent'

    assert not lyr.TestCapability('FastFeatureCount'), \
        'CSV files do not support fast feature count'

    assert ogr.GetDriverByName('CSV').TestCapability('DeleteDataSource'), \
        'CSV files do support DeleteDataSource'

    assert ogr.GetDriverByName('CSV').TestCapability('CreateDataSource'), \
        'CSV files do support CreateDataSource'

    assert not csv_ds.TestCapability('CreateLayer'), \
        'readonly datasource should not CreateLayer'

    assert not csv_ds.TestCapability('DeleteLayer'), \
        'should not have deletelayer on readonly ds.'

    csv_tmpds = ogr.Open('tmp/csvwrk', update=1)
    lyr = csv_tmpds.GetLayer(0)

    assert lyr.TestCapability('SequentialWrite'), \
        'should have write access to updatable layer'

    assert csv_tmpds.TestCapability('CreateLayer'), \
        'should have createlayer on updatable ds.'

    assert csv_tmpds.TestCapability('DeleteLayer'), \
        'should have deletelayer on updatable ds.'

###############################################################################


def ogr_csv_check_testcsvt(lyr):

    lyr.ResetReading()

    expect = [12, None]
    tr = ogrtest.check_features_against_list(lyr, 'INTCOL', expect)
    assert tr

    lyr.ResetReading()

    expect = [5.7, None]
    tr = ogrtest.check_features_against_list(lyr, 'REALCOL', expect)
    assert tr

    lyr.ResetReading()

    expect = ['foo', '']
    tr = ogrtest.check_features_against_list(lyr, 'STRINGCOL', expect)
    assert tr

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString('DATETIME') == '2008/12/25 11:22:33'

    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString('DATETIME') == ''

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString('DATE') == '2008/12/25'

    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString('DATE') == ''

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString('TIME') == '11:22:33'

    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString('TIME') == ''

    assert lyr.GetLayerDefn().GetFieldDefn(0).GetWidth() == 5, \
        'Field 0 : expecting width = 5'

    assert lyr.GetLayerDefn().GetFieldDefn(1).GetWidth() == 10, \
        'Field 1 : expecting width = 10'

    assert lyr.GetLayerDefn().GetFieldDefn(1).GetPrecision() == 7, \
        'Field 1 : expecting precision = 7'

    assert lyr.GetLayerDefn().GetFieldDefn(2).GetWidth() == 15, \
        'Field 2 : expecting width = 15'

    assert lyr.GetLayerDefn().GetFieldDefn(6).GetType() == ogr.OFTDateTime, \
        'Field DATETIME : wrong type'

    assert lyr.GetLayerDefn().GetFieldDefn(7).GetType() == ogr.OFTDate, \
        'Field DATETIME : wrong type'

    assert lyr.GetLayerDefn().GetFieldDefn(8).GetType() == ogr.OFTTime, \
        'Field DATETIME : wrong type'

    lyr.ResetReading()


###############################################################################
# Verify handling of csvt with width and precision specified
# Test NULL handling of non string columns too (#2756)

def test_ogr_csv_11():

    csv_ds = ogr.Open('data/csv/testcsvt.csv')

    assert csv_ds is not None

    lyr = csv_ds.GetLayerByName('testcsvt')

    return ogr_csv_check_testcsvt(lyr)

###############################################################################
# Verify CREATE_CSVT=YES option


def test_ogr_csv_12():

    csv_ds = ogr.Open('data/csv/testcsvt.csv')
    srclyr = csv_ds.GetLayerByName('testcsvt')

    #######################################################
    # Create layer (.csv file)
    options = ['CREATE_CSVT=YES', ]
    csv_tmpds = ogr.Open('tmp/csvwrk', update=1)
    csv_lyr2 = csv_tmpds.CreateLayer('testcsvt_copy', options=options)

    #######################################################
    # Setup Schema
    for i in range(srclyr.GetLayerDefn().GetFieldCount()):
        field_defn = srclyr.GetLayerDefn().GetFieldDefn(i)
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        csv_lyr2.CreateField(field_defn)
        gdal.PopErrorHandler()

    #######################################################
    # Recopy source layer into destination layer
    dst_feat = ogr.Feature(feature_def=csv_lyr2.GetLayerDefn())

    srclyr.ResetReading()

    feat = srclyr.GetNextFeature()

    while feat is not None:

        dst_feat.SetFrom(feat)
        csv_lyr2.CreateFeature(dst_feat)

        feat = srclyr.GetNextFeature()

    with gdaltest.error_handler():
        assert csv_tmpds.CreateLayer('testcsvt_copy') is None

    #######################################################
    # Closes everything and reopen
    csv_tmpds = None

    csv_ds = None
    csv_ds = ogr.Open('tmp/csvwrk/testcsvt_copy.csv')

    #######################################################
    # Checks copy
    assert csv_ds is not None

    lyr = csv_ds.GetLayerByName('testcsvt_copy')

    return ogr_csv_check_testcsvt(lyr)

###############################################################################
# Verify GEOMETRY=AS_WKT,AS_XY,AS_XYZ,AS_YX options


def test_ogr_csv_13():

    csv_tmpds = ogr.Open('tmp/csvwrk', update=1)

    # AS_WKT
    options = ['GEOMETRY=AS_WKT', 'CREATE_CSVT=YES']
    lyr = csv_tmpds.CreateLayer('as_wkt', options=options)

    field_defn = ogr.FieldDefn('ADATA', ogr.OFTString)
    lyr.CreateField(field_defn)

    # Some applications expect the WKT column not to be exposed. Check it
    assert lyr.GetLayerDefn().GetFieldCount() == 1

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature(dst_feat)

    # AS_WKT but no field
    options = ['GEOMETRY=AS_WKT', 'CREATE_CSVT=YES']
    lyr = csv_tmpds.CreateLayer('as_wkt_no_field', options=options)

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    lyr.CreateFeature(dst_feat)

    # AS_XY
    options = ['GEOMETRY=AS_XY', 'CREATE_CSVT=YES']
    lyr = csv_tmpds.CreateLayer('as_xy', options=options)

    field_defn = ogr.FieldDefn('ADATA', ogr.OFTString)
    lyr.CreateField(field_defn)

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature(dst_feat)

    # Nothing will be written in the x or y field
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(1 2,3 4)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature(dst_feat)

    # AS_YX
    options = ['GEOMETRY=AS_YX', 'CREATE_CSVT=YES']
    lyr = csv_tmpds.CreateLayer('as_yx', options=options)

    field_defn = ogr.FieldDefn('ADATA', ogr.OFTString)
    lyr.CreateField(field_defn)

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature(dst_feat)

    # AS_XYZ
    options = ['GEOMETRY=AS_XYZ', 'CREATE_CSVT=YES']
    lyr = csv_tmpds.CreateLayer('as_xyz', options=options)

    field_defn = ogr.FieldDefn('ADATA', ogr.OFTString)
    lyr.CreateField(field_defn)

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2 3)'))
    dst_feat.SetField('ADATA', 'avalue')
    lyr.CreateFeature(dst_feat)

    #######################################################
    # Closes everything and reopen
    csv_tmpds = None
    csv_tmpds = ogr.Open('tmp/csvwrk')

    # Test AS_WKT
    lyr = csv_tmpds.GetLayerByName('as_wkt')

    expect = ['POINT (1 2)']
    tr = ogrtest.check_features_against_list(lyr, 'WKT', expect)
    assert tr

    lyr.ResetReading()
    expect = ['avalue']
    tr = ogrtest.check_features_against_list(lyr, 'ADATA', expect)
    assert tr

    # Test as_wkt_no_field
    lyr = csv_tmpds.GetLayerByName('as_wkt_no_field')

    expect = ['POINT (1 2)']
    tr = ogrtest.check_features_against_list(lyr, 'WKT', expect)
    assert tr

    # Test AS_XY
    lyr = csv_tmpds.GetLayerByName('as_xy')

    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == 'X'

    expect = [1, None]
    tr = ogrtest.check_features_against_list(lyr, 'X', expect)
    assert tr

    lyr.ResetReading()
    expect = [2, None]
    tr = ogrtest.check_features_against_list(lyr, 'Y', expect)
    assert tr

    lyr.ResetReading()
    expect = ['avalue', 'avalue']
    tr = ogrtest.check_features_against_list(lyr, 'ADATA', expect)
    assert tr

    # Test AS_YX
    lyr = csv_tmpds.GetLayerByName('as_yx')

    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == 'Y'

    expect = [1]
    tr = ogrtest.check_features_against_list(lyr, 'X', expect)
    assert tr

    lyr.ResetReading()
    expect = [2]
    tr = ogrtest.check_features_against_list(lyr, 'Y', expect)
    assert tr

    # Test AS_XYZ
    lyr = csv_tmpds.GetLayerByName('as_xyz')

    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == 'X'

    expect = [1]
    tr = ogrtest.check_features_against_list(lyr, 'X', expect)
    assert tr

    lyr.ResetReading()
    expect = [2]
    tr = ogrtest.check_features_against_list(lyr, 'Y', expect)
    assert tr

    lyr.ResetReading()
    expect = [3]
    tr = ogrtest.check_features_against_list(lyr, 'Z', expect)
    assert tr

###############################################################################
# Copy prime_meridian.csv again, with SEMICOLON as separator


def test_ogr_csv_14():

    csv_tmpds = ogr.Open('tmp/csvwrk', update=1)
    csv_ds = ogr.Open('data/prime_meridian.csv')

    #######################################################
    # Create layer (.csv file)
    csv_lyr1 = ogr_csv_copy_layer(csv_ds, csv_tmpds, 'pm3', ['SEPARATOR=SEMICOLON', ])

    ogr_csv_check_layer(csv_lyr1, True)

###############################################################################
# Close the file and check again
#


def test_ogr_csv_16():

    ds = ogr.Open('tmp/csvwrk')
    csv_lyr1 = ds.GetLayerByName('pm3')

    return ogr_csv_check_layer(csv_lyr1, False)

###############################################################################
# Verify that WKT field treated as geometry.
#


def test_ogr_csv_17():

    csv_ds = ogr.Open('data/wkt.csv')
    csv_lyr = csv_ds.GetLayer(0)

    assert csv_lyr.GetLayerDefn().GetGeomType() == ogr.wkbUnknown, \
        'did not get wktUnknown for geometry type.'

    feat = csv_lyr.GetNextFeature()
    assert feat.GetField('WKT') == 'POLYGON((6.25 1.25,7.25 1.25,7.25 2.25,6.25 2.25,6.25 1.25))', \
        'feature 1: expected wkt value'

    assert not ogrtest.check_feature_geometry(feat, 'POLYGON((6.25 1.25,7.25 1.25,7.25 2.25,6.25 2.25,6.25 1.25))')

    feat = csv_lyr.GetNextFeature()

    feat = csv_lyr.GetNextFeature()
    assert not ogrtest.check_feature_geometry(feat, 'POLYGON((1.001 1.001,3.999 3.999,3.2 1.6,1.001 1.001))')


###############################################################################
# Write to /vsistdout/

def test_ogr_csv_18():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsistdout/')
    lyr = ds.CreateLayer('foo', options=['GEOMETRY=AS_WKT'])
    lyr.CreateField(ogr.FieldDefn('foo'))
    lyr.CreateField(ogr.FieldDefn('bar'))
    feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    feat.SetField('foo', 'bar')
    feat.SetField('bar', 'baz')
    geom = ogr.CreateGeometryFromWkt('POINT(0 1)')
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

###############################################################################
# Verify handling of non-numeric values in numeric columns


def test_ogr_csv_19():

    csv_ds = ogr.Open('data/csv/testnull.csv')

    assert csv_ds is not None

    lyr = csv_ds.GetLayerByName('testnull')

    lyr.ResetReading()
    with gdaltest.error_handler():
        assert ogrtest.check_features_against_list(lyr, 'INTCOL', [12])
    lyr.ResetReading()
    assert ogrtest.check_features_against_list(lyr, 'REALCOL', [5.7])
    lyr.ResetReading()
    assert ogrtest.check_features_against_list(lyr, 'INTCOL2', [None])
    lyr.ResetReading()
    assert ogrtest.check_features_against_list(lyr, 'REALCOL2', [None])
    lyr.ResetReading()
    assert ogrtest.check_features_against_list(lyr, 'STRINGCOL', ['foo'])


###############################################################################
# Verify handling of column names with numbers

def test_ogr_csv_20():

    csv_ds = ogr.Open('data/csv/testnumheader1.csv')
    assert csv_ds is not None

    lyr = csv_ds.GetLayerByName('testnumheader1')
    assert lyr is not None
    lyr.ResetReading()

    expect = ['1 - 2', '2-3']
    got = [lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef(),
           lyr.GetLayerDefn().GetFieldDefn(1).GetNameRef()]
    assert got[0] == expect[0], \
        ('column 0 got name %s expected %s' % (str(got[0]), str(expect[0])))
    assert got[1] == expect[1], \
        ('column 1 got name %s expected %s' % (str(got[1]), str(expect[1])))

    csv_ds = ogr.Open('data/csv/testnumheader2.csv')
    assert csv_ds is not None

    lyr = csv_ds.GetLayerByName('testnumheader2')
    assert lyr is not None
    lyr.ResetReading()

    expect = ['field_1', 'field_2']
    got = [lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef(),
           lyr.GetLayerDefn().GetFieldDefn(1).GetNameRef()]
    assert got[0] == expect[0], \
        ('column 0 got name %s expected %s' % (str(got[0]), str(expect[0])))
    assert got[1] == expect[1], \
        ('column 1 got name %s expected %s' % (str(got[1]), str(expect[1])))

###############################################################################
# Verify handling of numeric column names with quotes (bug #4361)


def test_ogr_csv_21():


    csv_ds = ogr.Open('data/csv/testquoteheader1.csv')
    assert csv_ds is not None

    lyr = csv_ds.GetLayerByName('testquoteheader1')
    assert lyr is not None
    lyr.ResetReading()

    expect = ['test', '2000', '2000.12']
    for i in range(0, 3):
        got = lyr.GetLayerDefn().GetFieldDefn(i).GetNameRef()
        assert got == expect[i], \
            ('column %d got name %s expected %s' % (i, str(got), str(expect[i])))

    csv_ds = ogr.Open('data/csv/testquoteheader2.csv')
    assert csv_ds is not None

    lyr = csv_ds.GetLayerByName('testquoteheader2')
    assert lyr is not None
    lyr.ResetReading()

    expect = ['field_1', 'field_2', 'field_3']
    for i in range(0, 3):
        got = lyr.GetLayerDefn().GetFieldDefn(i).GetNameRef()
        assert got == expect[i], \
            ('column %d got name %s expected %s' % (i, str(got), str(expect[i])))

###############################################################################
# Test handling of UTF8 BOM (bug #4623)

def test_ogr_csv_22():

    ds = ogr.Open('data/csv/csv_with_utf8_bom.csv')
    lyr = ds.GetLayer(0)
    fld0_name = lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef()

    assert fld0_name == 'id', 'bad field name'


def test_ogr_csv_23():
    # create a CSV file with UTF8 BOM
    ds = ogr.Open('tmp/csvwrk', update=1)
    lyr = ds.CreateLayer('utf8', options=['WRITE_BOM=YES', 'GEOMETRY=AS_WKT'])
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('bar', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', 123)
    feat.SetField('bar', 'baz')
    geom = ogr.CreateGeometryFromWkt('POINT(0 1)')
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = None
    lyr = None
    ds = None

    data = open('tmp/csvwrk/utf8.csv', 'rb').read()
    assert data[:6] == b'\xef\xbb\xbfWKT', "No UTF8 BOM header on output"

    # create a CSV file without UTF8 BOM
    ds = ogr.Open('tmp/csvwrk', update=1)
    lyr = ds.CreateLayer('utf8no', options=['WRITE_BOM=YES', 'GEOMETRY=AS_WKT'])
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('bar', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', 123)
    feat.SetField('bar', 'baz')
    geom = ogr.CreateGeometryFromWkt('POINT(0 1)')
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = None
    lyr = None
    ds = None

    data = open('tmp/csvwrk/utf8no.csv', 'rb').read()
    assert data[:3] != '\xef\xbb\xbfWKT', "Found UTF8 BOM header on output!"

###############################################################################
# Test single column CSV files


def test_ogr_csv_24():

    # Create an invalid CSV file
    f = gdal.VSIFOpenL('/vsimem/invalid.csv', 'wb')
    gdal.VSIFCloseL(f)

    # and check that it doesn't prevent from creating a new CSV file (#4824)
    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/single.csv')
    lyr = ds.CreateLayer('single')
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'bar')
    lyr.CreateFeature(feat)
    feat = None
    lyr = None
    ds = None

    ds = ogr.Open('/vsimem/single.csv')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    feat = lyr.GetNextFeature()
    assert feat.GetField(0) == ''
    feat = lyr.GetNextFeature()
    assert feat.GetField(0) == 'bar'
    ds = None

    gdal.Unlink('/vsimem/single.csv')
    gdal.Unlink('/vsimem/invalid.csv')


###############################################################################
# Test newline handling (#4452)

def test_ogr_csv_25():
    ds = ogr.Open('tmp/csvwrk', update=1)
    lyr = ds.CreateLayer('newlines', options=['LINEFORMAT=LF'])  # just in case tests are run on windows...
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', 'windows newline:\r\nlinux newline:\nend of string:')
    lyr.CreateFeature(feat)

    feat = None
    lyr = None
    ds = None

    EXPECTED = 'foo,\n"windows newline:\r\nlinux newline:\nend of string:"\n'

    data = open('tmp/csvwrk/newlines.csv', 'rb').read().decode('ascii')
    assert data == EXPECTED, \
        ("Newlines changed:\n\texpected=%s\n\tgot=     %s" % (repr(EXPECTED), repr(data)))


###############################################################################
# Test number padding behaviour (#4469)

def test_ogr_csv_26():
    ds = ogr.Open('tmp/csvwrk', update=1)
    lyr = ds.CreateLayer('num_padding', options=['LINEFORMAT=LF'])  # just in case tests are run on windows...

    field = ogr.FieldDefn('foo', ogr.OFTReal)
    field.SetWidth(50)
    field.SetPrecision(25)
    lyr.CreateField(field)

    feature = ogr.Feature(lyr.GetLayerDefn())
    feature.SetField('foo', 10.5)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', 10.5)
    lyr.CreateFeature(feat)

    feat = None
    lyr = None
    ds = None

    EXPECTED = 'foo,\n10.5000000000000000000000000\n'

    data = open('tmp/csvwrk/num_padding.csv', 'rb').read().decode('ascii')
    assert data == EXPECTED, ("expected=%s got= %s" % (repr(EXPECTED), repr(data)))

###############################################################################
# Test Eurostat .TSV files


def test_ogr_csv_27():

    ds = ogr.Open('data/csv/test_eurostat.tsv')
    lyr = ds.GetLayer(0)
    layer_defn = lyr.GetLayerDefn()
    assert layer_defn.GetFieldCount() == 8

    expected_fields = [('unit', ogr.OFTString),
                       ('geo', ogr.OFTString),
                       ('time_2010', ogr.OFTReal),
                       ('time_2010_flag', ogr.OFTString),
                       ('time_2011', ogr.OFTReal),
                       ('time_2011_flag', ogr.OFTString),
                       ('time_2012', ogr.OFTReal),
                       ('time_2012_flag', ogr.OFTString)]
    i = 0
    for expected_field in expected_fields:
        fld = layer_defn.GetFieldDefn(i)
        assert fld.GetName() == expected_field[0]
        assert fld.GetType() == expected_field[1]
        i = i + 1

    feat = lyr.GetNextFeature()
    if feat.GetField('unit') != 'NBR' or \
       feat.GetField('geo') != 'FOO' or \
       feat.IsFieldSet('time_2010') or \
       feat.IsFieldSet('time_2010_flag') or \
       feat.GetField('time_2011') != 1 or \
       feat.GetField('time_2011_flag') != 'u' or \
       feat.GetField('time_2012') != 2.34 or \
       feat.IsFieldSet('time_2012_flag'):
        feat.DumpReadable()
        pytest.fail()


###############################################################################
# Check that we don't rewrite erroneously a file that has no header (#5161).


def test_ogr_csv_28():

    f = open('tmp/ogr_csv_28.csv', 'wb')
    f.write('1,2\n'.encode('ascii'))
    f.close()

    ds = ogr.Open('tmp/ogr_csv_28.csv', update=1)
    del ds

    f = open('tmp/ogr_csv_28.csv', 'rb')
    data = f.read().decode('ascii')
    f.close()

    os.unlink('tmp/ogr_csv_28.csv')

    assert data == '1,2\n'

###############################################################################
# Check multi geometry field support


def test_ogr_csv_29():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('tmp/ogr_csv_29', options=['GEOMETRY=AS_WKT'])
    assert ds.TestCapability(ogr.ODsCCurveGeometries) == 1
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone)
    assert lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_lyr1_EPSG_4326", ogr.wkbPoint)) == 0
    assert lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_lyr2_EPSG_32632", ogr.wkbPolygon)) == 0
    with gdaltest.error_handler():
        assert lyr.CreateGeomField(ogr.GeomFieldDefn("geom__WKT_lyr2_EPSG_32632", ogr.wkbPolygon)) != 0
    ds = None

    ds = ogr.Open('tmp/ogr_csv_29', update=1)
    lyr = ds.GetLayerByName('test')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeomField(0, ogr.CreateGeometryFromWkt('POINT (1 2)'))
    feat.SetGeomField(1, ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,1 0,0 0))'))
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open('tmp/ogr_csv_29')
    lyr = ds.GetLayerByName('test')
    assert lyr.GetLayerDefn().GetGeomFieldCount() == 2
    srs = lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef()
    assert srs.GetAuthorityCode(None) == '4326'
    srs = lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef()
    assert srs.GetAuthorityCode(None) == '32632'
    feat = lyr.GetNextFeature()
    geom = feat.GetGeomFieldRef('geom__WKT_lyr1_EPSG_4326')
    if geom.ExportToWkt() != 'POINT (1 2)':
        feat.DumpReadable()
        pytest.fail()
    geom = feat.GetGeomFieldRef('geom__WKT_lyr2_EPSG_32632')
    if geom.ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,1 0,0 0))':
        feat.DumpReadable()
        pytest.fail()
    ds = None

###############################################################################
# Run test_ogrsf


def test_ogr_csv_30():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/ogr_csv_29')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Read geonames.org allCountries.txt


def test_ogr_csv_31():

    ds = ogr.Open('data/csv/allCountries.txt')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetField('GEONAMEID') != '3038814' or f.GetField('LATITUDE') != 42.5 or \
       f.GetGeometryRef().ExportToWkt() != 'POINT (1.48333 42.5)':
        f.DumpReadable()
        pytest.fail()

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetField('GEONAMEID') != '3038814':
        f.DumpReadable()
        pytest.fail()

    assert lyr.GetFeatureCount() == 10

###############################################################################
# Test AUTODETECT_TYPE=YES


def test_ogr_csv_32():

    # Without limit, everything will be detected as string
    ds = gdal.OpenEx('data/csv/testtypeautodetect.csv', gdal.OF_VECTOR,
                     open_options=['AUTODETECT_TYPE=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_values = ['', '1.5', '1', '1.5', '2', '', '2014-09-27 19:01:00', '2014-09-27', '2014-09-27 20:00:00',
                  '2014-09-27', '12:34:56', 'a', 'a', '1', '1', '1.5', '2014-09-27 19:01:00', '2014-09-27', '19:01:00', '2014-09-27T00:00:00Z']
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        assert (lyr.GetLayerDefn().GetFieldDefn(i).GetType() == ogr.OFTString and \
           lyr.GetLayerDefn().GetFieldDefn(i).GetWidth() == 0)
        if f.GetField(i) != col_values[i]:
            f.DumpReadable()
            pytest.fail(i)

    # Without limit, everything will be detected as string
    ds = gdal.OpenEx('data/csv/testtypeautodetect.csv', gdal.OF_VECTOR,
                     open_options=['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=0'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        assert (lyr.GetLayerDefn().GetFieldDefn(i).GetType() == ogr.OFTString and \
           lyr.GetLayerDefn().GetFieldDefn(i).GetWidth() == 0)
        if f.GetField(i) != col_values[i]:
            f.DumpReadable()
            pytest.fail(i)

    # We limit to the first "1.5" line
    ds = gdal.OpenEx('data/csv/testtypeautodetect.csv', gdal.OF_VECTOR,
                     open_options=['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=300', 'QUOTED_FIELDS_AS_STRING=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_type = [ogr.OFTString, ogr.OFTReal, ogr.OFTInteger, ogr.OFTReal, ogr.OFTInteger, ogr.OFTString,
                ogr.OFTDateTime, ogr.OFTDate, ogr.OFTDateTime, ogr.OFTDate, ogr.OFTTime,
                ogr.OFTString, ogr.OFTString, ogr.OFTString, ogr.OFTInteger, ogr.OFTReal, ogr.OFTDateTime, ogr.OFTDate, ogr.OFTTime, ogr.OFTDateTime]
    col_values = ['', 1.5, 1, 1.5, 2, '', '2014/09/27 19:01:00', '2014/09/27', '2014/09/27 20:00:00',
                  '2014/09/27', '12:34:56', 'a', 'a', '1', 1, 1.5, '2014/09/27 19:01:00', '2014/09/27', '19:01:00', '2014/09/27 00:00:00+00']
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        assert (lyr.GetLayerDefn().GetFieldDefn(i).GetType() == col_type[i] and \
           lyr.GetLayerDefn().GetFieldDefn(i).GetWidth() == 0)
        if f.GetField(i) != col_values[i]:
            f.DumpReadable()
            pytest.fail(i)

    # Without QUOTED_FIELDS_AS_STRING=YES, str3 will be detected as integer
    ds = gdal.OpenEx('data/csv/testtypeautodetect.csv', gdal.OF_VECTOR,
                     open_options=['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=300'])
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('str3')).GetType() == ogr.OFTInteger

    # We limit to the first 2 lines
    ds = gdal.OpenEx('data/csv/testtypeautodetect.csv', gdal.OF_VECTOR,
                     open_options=['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=350', 'QUOTED_FIELDS_AS_STRING=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_type = [ogr.OFTString, ogr.OFTReal, ogr.OFTReal, ogr.OFTReal, ogr.OFTInteger, ogr.OFTInteger,
                ogr.OFTDateTime, ogr.OFTDateTime, ogr.OFTDateTime, ogr.OFTDate, ogr.OFTTime,
                ogr.OFTString, ogr.OFTString, ogr.OFTString, ogr.OFTString, ogr.OFTString, ogr.OFTString, ogr.OFTString, ogr.OFTString, ogr.OFTDateTime]
    col_values = ['', 1.5, 1, 1.5, 2, None, '2014/09/27 19:01:00', '2014/09/27 00:00:00', '2014/09/27 20:00:00',
                  '2014/09/27', '12:34:56', 'a', 'a', '1', '1', '1.5', '2014-09-27 19:01:00', '2014-09-27', '19:01:00', '2014/09/27 00:00:00+00']
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        assert (lyr.GetLayerDefn().GetFieldDefn(i).GetType() == col_type[i] and \
           lyr.GetLayerDefn().GetFieldDefn(i).GetWidth() == 0)
        if f.GetField(i) != col_values[i]:
            f.DumpReadable()
            pytest.fail(i)

    # Test AUTODETECT_WIDTH=YES
    ds = gdal.OpenEx('data/csv/testtypeautodetect.csv', gdal.OF_VECTOR,
                     open_options=['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=350', 'AUTODETECT_WIDTH=YES', 'QUOTED_FIELDS_AS_STRING=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_width = [0, 3, 3, 3, 1, 1, 0, 0, 0, 0, 0, 1, 2, 1, 1, 3, 19, 10, 8, 0]
    col_precision = [0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]

    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        assert (lyr.GetLayerDefn().GetFieldDefn(i).GetType() == col_type[i] and \
           lyr.GetLayerDefn().GetFieldDefn(i).GetWidth() == col_width[i] and \
           lyr.GetLayerDefn().GetFieldDefn(i).GetPrecision() == col_precision[i])
        if f.GetField(i) != col_values[i]:
            f.DumpReadable()
            pytest.fail(i)

    # Test AUTODETECT_WIDTH=STRING_ONLY
    ds = gdal.OpenEx('data/csv/testtypeautodetect.csv', gdal.OF_VECTOR,
                     open_options=['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=350', 'AUTODETECT_WIDTH=STRING_ONLY', 'QUOTED_FIELDS_AS_STRING=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_width = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 1, 3, 19, 10, 8, 0]
    col_precision = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]

    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        assert (lyr.GetLayerDefn().GetFieldDefn(i).GetType() == col_type[i] and \
           lyr.GetLayerDefn().GetFieldDefn(i).GetWidth() == col_width[i] and \
           lyr.GetLayerDefn().GetFieldDefn(i).GetPrecision() == col_precision[i])
        if f.GetField(i) != col_values[i]:
            f.DumpReadable()
            pytest.fail(i)

    # Test KEEP_SOURCE_COLUMNS=YES
    ds = gdal.OpenEx('data/csv/testtypeautodetect.csv', gdal.OF_VECTOR,
                     open_options=['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=350', 'KEEP_SOURCE_COLUMNS=YES', 'QUOTED_FIELDS_AS_STRING=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_values = ['', 1.5, '1.5', 1, '1', 1.5, '1.5', 2, '2', None, None,
                  '2014/09/27 19:01:00', '2014-09-27 19:01:00', '2014/09/27 00:00:00', '2014-09-27', '2014/09/27 20:00:00', '2014-09-27 20:00:00',
                  '2014/09/27', '2014-09-27', '12:34:56', '12:34:56', 'a', 'a', '1', '1', '1.5', '2014-09-27 19:01:00', '2014-09-27', '19:01:00',
                  '2014/09/27 00:00:00+00', '2014-09-27T00:00:00Z']

    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        assert (lyr.GetLayerDefn().GetFieldDefn(i).GetType() == ogr.OFTString or \
           lyr.GetLayerDefn().GetFieldDefn(i + 1).GetNameRef() == lyr.GetLayerDefn().GetFieldDefn(i).GetNameRef() + '_original')
        if f.GetField(i) != col_values[i]:
            f.DumpReadable()
            pytest.fail(i)

    # Test warnings
    for fid in [3,  # string in real field
                4,  # string in int field
                5,  # real in int field
                6,  # string in datetime field
                7,  # Value with a width greater than field width found in record 7 for field int1
                8,  # Value with a width greater than field width found in record 8 for field str1
                9,  # Value with a precision greater than field precision found in record 9 for field real1
               ]:
        ds = gdal.OpenEx('data/csv/testtypeautodetect.csv', gdal.OF_VECTOR,
                         open_options=['AUTODETECT_TYPE=YES', 'AUTODETECT_SIZE_LIMIT=350', 'AUTODETECT_WIDTH=YES'])
        lyr = ds.GetLayer(0)
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        lyr.GetFeature(fid)
        gdal.PopErrorHandler()
        if gdal.GetLastErrorType() != gdal.CE_Warning:
            f.DumpReadable()
            pytest.fail(fid)

    # Test Real -> Integer64 (https://github.com/OSGeo/gdal/issues/343)
    gdal.FileFromMemBuffer('/vsimem/testtypeautodetect.csv',
                           """foo,bar
1.2,
1234567890123,""")
    ds = gdal.OpenEx('/vsimem/testtypeautodetect.csv', gdal.OF_VECTOR,
                     open_options=['AUTODETECT_TYPE=YES'])
    gdal.Unlink('/vsimem/testtypeautodetect.csv')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetField(0) != 1.2:
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test Boolean, Int16 and Float32 support


def test_ogr_csv_33():

    ds = gdal.OpenEx('data/csv/testtypeautodetectboolean.csv', gdal.OF_VECTOR,
                     open_options=['AUTODETECT_TYPE=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_values = [1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 'y']
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        assert (not (i < 10 and lyr.GetLayerDefn().GetFieldDefn(i).GetSubType() != ogr.OFSTBoolean) or \
           (i >= 10 and lyr.GetLayerDefn().GetFieldDefn(i).GetSubType() == ogr.OFSTBoolean))
        if f.GetField(i) != col_values[i]:
            f.DumpReadable()
            pytest.fail(i)
    ds = None

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/subtypes.csv')
    lyr = ds.CreateLayer('test', options=['CREATE_CSVT=YES'])
    fld = ogr.FieldDefn('b', ogr.OFTInteger)
    fld.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld)
    fld = ogr.FieldDefn('int16', ogr.OFTInteger)
    fld.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld)
    fld = ogr.FieldDefn('float32', ogr.OFTReal)
    fld.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 1)
    f.SetField(1, -32768)
    f.SetField(2, 1.23)
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/subtypes.csv')
    lyr = ds.GetLayer(0)
    assert (lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger and \
       lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTBoolean)
    assert (lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTInteger and \
       lyr.GetLayerDefn().GetFieldDefn(1).GetSubType() == ogr.OFSTInt16)
    assert (lyr.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTReal and \
       lyr.GetLayerDefn().GetFieldDefn(2).GetSubType() == ogr.OFSTFloat32)
    f = lyr.GetNextFeature()
    if f.GetField(0) != 1 or f.GetField(1) != -32768 or f.GetField(2) != 1.23:
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/subtypes.csv')
    gdal.Unlink('/vsimem/subtypes.csvt')

###############################################################################
# Test Integer64 support


def test_ogr_csv_34():

    ds = gdal.OpenEx('data/csv/testtypeautodetectinteger64.csv', gdal.OF_VECTOR,
                     open_options=['AUTODETECT_TYPE=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    col_values = [1, 10000000000, 10000000000, 10000000000.0]
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        if f.GetField(i) != col_values[i]:
            f.DumpReadable()
            pytest.fail(i)
    f = lyr.GetNextFeature()
    col_values = [10000000000, 1, 10000000000, 1.0]
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        if f.GetField(i) != col_values[i]:
            f.DumpReadable()
            pytest.fail(i)
    ds = None

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/int64.csv')
    lyr = ds.CreateLayer('test', options=['CREATE_CSVT=YES'])
    fld = ogr.FieldDefn('int64', ogr.OFTInteger64)
    lyr.CreateField(fld)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 10000000000)
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/int64.csv')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetField(0) != 10000000000:
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/int64.csv')
    gdal.Unlink('/vsimem/int64.csvt')

###############################################################################
# Test comma separator


def test_ogr_csv_35():

    gdal.FileFromMemBuffer('/vsimem/ogr_csv_35.csv',
                           """FIELD_1  "FIELD 2" FIELD_3
VAL1   "VAL 2"   "VAL 3"
""")

    ds = gdal.OpenEx('/vsimem/ogr_csv_35.csv', gdal.OF_VECTOR,
                     open_options=['MERGE_SEPARATOR=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['FIELD_1'] != 'VAL1' or f['FIELD 2'] != 'VAL 2' or f['FIELD_3'] != 'VAL 3':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_35.csv')

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_35.csv')
    lyr = ds.CreateLayer('ogr_csv_35', options=['SEPARATOR=SPACE'])
    lyr.CreateField(ogr.FieldDefn('FIELD_1', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('FIELD 2', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('FIELD_1', 'VAL1')
    f.SetField('FIELD 2', 'VAL 2')
    lyr.CreateFeature(f)
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_csv_35.csv', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert 'FIELD_1 "FIELD 2"' in data and 'VAL1 "VAL 2"' in data

    gdal.Unlink('/vsimem/ogr_csv_35.csv')

###############################################################################
# Test GEOM_POSSIBLE_NAMES open option


def test_ogr_csv_36():

    gdal.FileFromMemBuffer('/vsimem/ogr_csv_36.csv',
                           """id,mygeometry,format
1,"POINT(1 2)",wkt
2,"{""type"": ""Point"", ""coordinates"" : [2, 49]}",geojson
3,010100000000000000000008400000000000004940,wkb
4,0101000020e610000000000000000008400000000000004940,ewkb
5,something else,something else
""")

    ds = gdal.OpenEx('/vsimem/ogr_csv_36.csv', gdal.OF_VECTOR,
                     open_options=['GEOM_POSSIBLE_NAMES=mygeometry,another_field'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (1 2)' or f['id'] != '1' or f['mygeometry'] != 'POINT(1 2)' or f['format'] != 'wkt':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetGeometryRef() is not None:
        f.DumpReadable()
        pytest.fail()
    ds = None

    # Test prefix* pattern
    ds = gdal.OpenEx('/vsimem/ogr_csv_36.csv', gdal.OF_VECTOR,
                     open_options=['GEOM_POSSIBLE_NAMES=mygeom*'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (1 2)':
        f.DumpReadable()
        pytest.fail()
    ds = None

    # Test *suffix pattern
    ds = gdal.OpenEx('/vsimem/ogr_csv_36.csv', gdal.OF_VECTOR,
                     open_options=['GEOM_POSSIBLE_NAMES=*geometry'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (1 2)':
        f.DumpReadable()
        pytest.fail()
    ds = None

    # Test *middle* pattern
    ds = gdal.OpenEx('/vsimem/ogr_csv_36.csv', gdal.OF_VECTOR,
                     open_options=['GEOM_POSSIBLE_NAMES=*geom*'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (1 2)':
        f.DumpReadable()
        pytest.fail()
    ds = None

    # Test non matching pattern
    ds = gdal.OpenEx('/vsimem/ogr_csv_36.csv', gdal.OF_VECTOR,
                     open_options=['GEOM_POSSIBLE_NAMES=bla'])
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetGeomFieldCount() == 0
    ds = None

    # Check KEEP_GEOM_COLUMNS=NO
    ds = gdal.OpenEx('/vsimem/ogr_csv_36.csv', gdal.OF_VECTOR,
                     open_options=['GEOM_POSSIBLE_NAMES=mygeometry', 'KEEP_GEOM_COLUMNS=NO'])
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (1 2)' or f['id'] != '1' or f['format'] != 'wkt':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_36.csv')

###############################################################################
# Test X_POSSIBLE_NAMES, Y_POSSIBLE_NAMES and Z_POSSIBLE_NAMES open options


def test_ogr_csv_37():

    gdal.FileFromMemBuffer('/vsimem/ogr_csv_37.csv',
                           """id,y,other,x,z
1,49,a,2,100
2,50,b,3,
3,49,c,
""")

    ds = gdal.OpenEx('/vsimem/ogr_csv_37.csv', gdal.OF_VECTOR,
                     open_options=['X_POSSIBLE_NAMES=long,x', 'Y_POSSIBLE_NAMES=lat,y'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)' or f['id'] != '1' or f['x'] != 2 or f['y'] != 49 or f['other'] != 'a' or f['z'] != '100':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetGeometryRef() is not None:
        f.DumpReadable()
        pytest.fail()
    ds = None

    # Check Z_POSSIBLE_NAMES
    ds = gdal.OpenEx('/vsimem/ogr_csv_37.csv', gdal.OF_VECTOR,
                     open_options=['X_POSSIBLE_NAMES=long,x', 'Y_POSSIBLE_NAMES=lat,y', 'Z_POSSIBLE_NAMES=z'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49 100)' or f['id'] != '1' or f['x'] != 2 or f['y'] != 49 or f['other'] != 'a' or f['z'] != 100:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetGeometryRef() is not None:
        f.DumpReadable()
        pytest.fail()
    ds = None

    # Check KEEP_GEOM_COLUMNS=NO
    ds = gdal.OpenEx('/vsimem/ogr_csv_37.csv', gdal.OF_VECTOR,
                     open_options=['X_POSSIBLE_NAMES=long,x', 'Y_POSSIBLE_NAMES=lat,y', 'KEEP_GEOM_COLUMNS=NO'])
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 3
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)' or f['id'] != '1' or f['other'] != 'a':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_37.csv')

###############################################################################
# Test GeoCSV WKT type


def test_ogr_csv_38():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_38.csv')
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:4326')
    lyr = ds.CreateLayer('ogr_csv_38', srs=srs, options=['GEOMETRY=AS_WKT', 'CREATE_CSVT=YES', 'GEOMETRY_NAME=mygeom'])
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 1)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_38.csv')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetName() == 'mygeom'
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef().ExportToWkt().find('4326') >= 0
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_38.csv')
    gdal.Unlink('/vsimem/ogr_csv_38.csvt')
    gdal.Unlink('/vsimem/ogr_csv_38.prj')

###############################################################################
# Test GeoCSV CoordX and CoordY types


def test_ogr_csv_39():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_39.csv')
    lyr = ds.CreateLayer('ogr_csv_38', options=['GEOMETRY=AS_XY', 'CREATE_CSVT=YES'])
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 1)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_39.csv')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_39.csv')
    gdal.Unlink('/vsimem/ogr_csv_39.csvt')

###############################################################################
# Test X_POSSIBLE_NAMES, Y_POSSIBLE_NAMES, GEOM_POSSIBLE_NAMES and KEEP_GEOM_COLUMNS=NO together (#6137)


def test_ogr_csv_40():

    gdal.FileFromMemBuffer('/vsimem/ogr_csv_40.csv',
                           """latitude,longitude,the_geom,id
49,2,0101000020E61000004486E281C5C257C068B89DDA998F4640,1
""")

    ds = gdal.OpenEx('/vsimem/ogr_csv_40.csv', gdal.OF_VECTOR,
                     open_options=['X_POSSIBLE_NAMES=longitude', 'Y_POSSIBLE_NAMES=latitude', 'GEOM_POSSIBLE_NAMES=the_geom', 'KEEP_GEOM_COLUMNS=NO'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)' or f['id'] != '1' or f['the_geom'] != '0101000020E61000004486E281C5C257C068B89DDA998F4640':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_40.csv')

    gdal.FileFromMemBuffer('/vsimem/ogr_csv_40.csv',
                           """the_geom,latitude,longitude,id
0101000020E61000004486E281C5C257C068B89DDA998F4640,49,2,1
""")

    ds = gdal.OpenEx('/vsimem/ogr_csv_40.csv', gdal.OF_VECTOR,
                     open_options=['X_POSSIBLE_NAMES=longitude', 'Y_POSSIBLE_NAMES=latitude', 'GEOM_POSSIBLE_NAMES=the_geom', 'KEEP_GEOM_COLUMNS=NO'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt().find('POINT (-95.04') < 0 or f['id'] != '1' or f['longitude'] != '2' or f['latitude'] != '49':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_40.csv')

###############################################################################
# Test GEOM_POSSIBLE_NAMES and KEEP_GEOM_COLUMNS=NO together with empty content in geom column (#6152)


def test_ogr_csv_41():

    gdal.FileFromMemBuffer('/vsimem/ogr_csv_41.csv',
                           """id,the_geom,foo
1,,bar
""")

    ds = gdal.OpenEx('/vsimem/ogr_csv_41.csv', gdal.OF_VECTOR,
                     open_options=['GEOM_POSSIBLE_NAMES=the_geom', 'KEEP_GEOM_COLUMNS=NO'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef() is not None or f['id'] != '1' or f['foo'] != 'bar':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_41.csv')

###############################################################################
# Test writing field with empty content


def test_ogr_csv_42():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_42.csv')
    lyr = ds.CreateLayer('ogr_csv_42')
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 1)
    assert lyr.CreateFeature(f) == 0
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_42.csv')

###############################################################################
# Test editing capabilities


def test_ogr_csv_43():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_43.csv')
    lyr = ds.CreateLayer('ogr_csv_43', options=['GEOMETRY=AS_WKT', 'CREATE_CSVT=YES'])
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 1)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    assert lyr.CreateFeature(f) == 0
    f = None
    lyr.SetNextByIndex(0)
    f = lyr.GetNextFeature()
    if f['id'] != 1:
        f.DumpReadable()
        pytest.fail()
    f = None

    assert lyr.TestCapability(ogr.OLCCreateField) == 1
    assert lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString)) == 0
    with gdaltest.error_handler():
        assert lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString)) != 0
    f = lyr.GetFeature(1)
    f.SetField('foo', 'bar')
    assert lyr.TestCapability(ogr.OLCRandomWrite) == 1
    assert lyr.SetFeature(f) == 0
    f = lyr.GetFeature(1)
    if f['id'] != 1 or f['foo'] != 'bar' or f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f['id'] != 1 or f['foo'] != 'bar' or f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    assert f is None
    assert lyr.GetFeatureCount() == 1
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 2)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(3 50)'))
    f.SetField('foo', 'baz')
    assert lyr.CreateFeature(f) == 0
    assert f.GetFID() == 2
    f = lyr.GetFeature(2)
    if f['id'] != 2 or f['foo'] != 'baz' or f.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
        f.DumpReadable()
        pytest.fail()
    assert lyr.GetFeatureCount() == 2
    lyr.SetNextByIndex(1)
    f = lyr.GetNextFeature()
    if f['id'] != 2:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f is not None:
        f.DumpReadable()
        pytest.fail()
    f = None

    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_43.csv', update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(2)
    if f['id'] != 2 or f['foo'] != 'baz' or f.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
        f.DumpReadable()
        pytest.fail()
    f = None
    assert lyr.TestCapability(ogr.OLCDeleteField) == 1
    with gdaltest.error_handler():
        assert lyr.DeleteField(-1) != 0
    assert lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex('foo')) == 0
    assert lyr.TestCapability(ogr.OLCDeleteFeature) == 1
    assert lyr.DeleteFeature(2) == 0
    assert lyr.DeleteFeature(2) == ogr.OGRERR_NON_EXISTING_FEATURE
    assert lyr.DeleteFeature(3) == ogr.OGRERR_NON_EXISTING_FEATURE
    assert lyr.GetFeature(2) is None
    assert lyr.GetFeature(3) is None
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f is not None
    f = lyr.GetNextFeature()
    assert f is None
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 3
    assert lyr.CreateFeature(f) == 0
    assert f.GetFID() == 3
    f = None
    assert lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString)) == 0
    f = lyr.GetFeature(1)
    assert f['foo'] != 'bar'
    f = lyr.GetFeature(3)
    assert f.GetFID() == 3
    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_43.csv', update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(2)
    if f['id'] != 3:
        f.DumpReadable()
        pytest.fail()
    f = None
    assert lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex('foo')) == 0
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(4 51)'))
    assert lyr.CreateFeature(f) == 0
    if f.GetFID() != 3:
        f.DumpReadable()
        pytest.fail()
    assert lyr.GetExtent() == (2.0, 4.0, 49.0, 51.0)
    assert lyr.DeleteFeature(f.GetFID()) == 0
    assert lyr.GetFeatureCount() == 2

    with gdaltest.error_handler():
        lyr.SetSpatialFilter(-1, None)
    with gdaltest.error_handler():
        lyr.SetSpatialFilter(1, None)
    lyr.SetSpatialFilterRect(0, 0, 100, 100)
    lyr.SetSpatialFilterRect(0, 0, 0, 100, 100)
    lyr.SetSpatialFilter(0, lyr.GetSpatialFilter())
    lyr.SetSpatialFilter(lyr.GetSpatialFilter())
    assert lyr.GetFeatureCount() == 1
    assert lyr.GetExtent() == (2.0, 2.0, 49.0, 49.0)
    assert lyr.GetExtent(geom_field=0) == (2.0, 2.0, 49.0, 49.0)
    with gdaltest.error_handler():
        lyr.GetExtent(geom_field=-1)
    lyr.SetAttributeFilter(None)

    assert lyr.TestCapability(ogr.OLCCurveGeometries) == 1
    assert lyr.TestCapability(ogr.OLCTransactions) == 0
    lyr.StartTransaction()
    lyr.RollbackTransaction()
    lyr.CommitTransaction()

    assert lyr.GetGeometryColumn() == ''
    assert lyr.GetFIDColumn() == ''

    assert lyr.TestCapability(ogr.OLCReorderFields) == 1
    assert lyr.ReorderFields([0, 1]) == 0
    with gdaltest.error_handler():
        assert lyr.ReorderFields([0, -1]) != 0

    assert lyr.TestCapability(ogr.OLCAlterFieldDefn) == 1
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert lyr.AlterFieldDefn(0, fld_defn, 0) == 0
    with gdaltest.error_handler():
        assert lyr.AlterFieldDefn(-1, fld_defn, 0) != 0

    f = lyr.GetFeature(2)
    f.SetGeomField(0, ogr.CreateGeometryFromWkt('POINT (1 2)'))
    assert lyr.SetFeature(f) == 0
    f = None
    assert lyr.TestCapability(ogr.OLCCreateGeomField) == 1
    assert lyr.CreateGeomField(ogr.GeomFieldDefn('geom__WKT_2')) == 0
    f = lyr.GetFeature(2)
    f.SetGeomField(1, ogr.CreateGeometryFromWkt('POINT (3 4)'))
    assert lyr.SetFeature(f) == 0

    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_43.csv', update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(2)
    if f['WKT'] != 'POINT (1 2)' or f['_WKT_2'] != 'POINT (3 4)':
        f.DumpReadable()
        pytest.fail()
    assert lyr.SetFeature(f) == 0
    f = None
    assert lyr.DeleteFeature(2) == 0
    assert lyr.GetFeature(2) is None
    assert lyr.DeleteFeature(2) == ogr.OGRERR_NON_EXISTING_FEATURE
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_43.csv')
    gdal.Unlink('/vsimem/ogr_csv_43.csvt')

###############################################################################
# Test seeking back while creating


def test_ogr_csv_44():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_44.csv')
    lyr = ds.CreateLayer('ogr_csv_44', options=['GEOMETRY=AS_WKT'])
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 1)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    assert lyr.CreateFeature(f) == 0
    f = None
    f = lyr.GetFeature(1)
    if f['id'] != 1 or f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 2)
    assert lyr.CreateFeature(f) == 0
    f = lyr.GetFeature(2)
    if f['id'] != 2 or f.GetGeometryRef() is not None:
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_44.csv')

###############################################################################
# Test QGIS use case that consists in reopening a file just after calling
# CreateField() on the main dataset and assuming that file is already serialized.


def test_ogr_csv_45():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_45.csv')
    lyr = ds.CreateLayer('ogr_csv_45', options=['GEOMETRY=AS_WKT'])
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_45.csv', update=1)
    lyr = ds.GetLayer(0)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTInteger))

    ds2 = ogr.Open('/vsimem/ogr_csv_45.csv')
    lyr2 = ds2.GetLayer(0)
    assert lyr2.GetLayerDefn().GetFieldCount() == 3
    ds2 = None

    ds = None

    gdal.Unlink('/vsimem/ogr_csv_45.csv')

###############################################################################
# Test edition of CSV files with X_POSSIBLE_NAMES, Y_POSSIBLE_NAMES open options


def test_ogr_csv_46():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_46.csv')
    lyr = ds.CreateLayer('ogr_csv_46')
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('X', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('Y', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('Z', ogr.OFTReal))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 1
    f['X'] = 1
    f['Y'] = 2
    f['Z'] = 3
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = gdal.OpenEx('/vsimem/ogr_csv_46.csv', gdal.OF_VECTOR | gdal.OF_UPDATE, open_options=['X_POSSIBLE_NAMES=X', 'Y_POSSIBLE_NAMES=Y'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (10 20 30)'))
    lyr.SetFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_46.csv')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 4
    f = lyr.GetNextFeature()
    if f['id'] != '1' or f['X'] != '10' or f['Y'] != '20' or f['Z'] != '3':
        f.DumpReadable()
        pytest.fail()
    ds = None

    ds = gdal.OpenEx('/vsimem/ogr_csv_46.csv', gdal.OF_VECTOR | gdal.OF_UPDATE, open_options=['KEEP_GEOM_COLUMNS=NO', 'X_POSSIBLE_NAMES=X', 'Y_POSSIBLE_NAMES=Y'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (-10 -20 30)'))
    lyr.SetFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_46.csv')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 4
    f = lyr.GetNextFeature()
    if f['id'] != '1' or f['X'] != '-10' or f['Y'] != '-20' or f['Z'] != '3':
        f.DumpReadable()
        pytest.fail()
    ds = None

    ds = gdal.OpenEx('/vsimem/ogr_csv_46.csv', gdal.OF_VECTOR | gdal.OF_UPDATE, open_options=['KEEP_GEOM_COLUMNS=NO', 'X_POSSIBLE_NAMES=X', 'Y_POSSIBLE_NAMES=Y', 'Z_POSSIBLE_NAMES=Z'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (-1 -2 -3)'))
    lyr.SetFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_46.csv')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 4
    f = lyr.GetNextFeature()
    if f['id'] != '1' or f['X'] != '-1' or f['Y'] != '-2' or f['Z'] != '-3':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_46.csv')

###############################################################################
# Test writing XYZM


def test_ogr_csv_47():

    ds = ogr.GetDriverByName('CSV').CreateDataSource('/vsimem/ogr_csv_47.csv')
    assert ds.TestCapability(ogr.ODsCMeasuredGeometries) == 1
    lyr = ds.CreateLayer('ogr_csv_47', options=['GEOMETRY=AS_WKT'])
    assert lyr.TestCapability(ogr.OLCMeasuredGeometries) == 1
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 1
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT ZM (1 2 3 4)'))
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_csv_47.csv')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != 'POINT ZM (1 2 3 4)':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_47.csv')

###############################################################################
# Test reading/writing StringList, etc..


def test_ogr_csv_48():

    gdal.FileFromMemBuffer('/vsimem/ogr_csv_48.csvt', 'JsonStringList,JsonStringList,JsonIntegerList,JsonInteger64List,JsonRealList\n')
    gdal.FileFromMemBuffer('/vsimem/ogr_csv_48.csv',
                           """stringlist,emptystringlist,intlist,int64list,reallist
"[""a"",null]",[],"[1]","[1234567890123]","[0.125]"
""")

    gdal.VectorTranslate('/vsimem/ogr_csv_48_out.csv', '/vsimem/ogr_csv_48.csv', format='CSV', layerCreationOptions=['CREATE_CSVT=YES', 'LINEFORMAT=LF'])

    f = gdal.VSIFOpenL('/vsimem/ogr_csv_48_out.csv', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert data.startswith('stringlist,emptystringlist,intlist,int64list,reallist\n"[ ""a"", """" ]",[],[ 1 ],[ 1234567890123 ],[ 0.125')

    f = gdal.VSIFOpenL('/vsimem/ogr_csv_48_out.csvt', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert data.startswith('JSonStringList,JSonStringList,JSonIntegerList,JSonInteger64List,JSonRealList')

    gdal.Unlink('/vsimem/ogr_csv_48.csv')
    gdal.Unlink('/vsimem/ogr_csv_48.csvt')
    gdal.Unlink('/vsimem/ogr_csv_48_out.csv')
    gdal.Unlink('/vsimem/ogr_csv_48_out.csvt')

###############################################################################
# Test EMPTY_STRING_AS_NULL=ES


def test_ogr_csv_49():
    gdal.FileFromMemBuffer('/vsimem/ogr_csv_49.csv',
                           """id,str
1,
""")

    ds = gdal.OpenEx('/vsimem/ogr_csv_49.csv', open_options=['EMPTY_STRING_AS_NULL=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if not f.IsFieldNull('str'):
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_49.csv')

###############################################################################


def test_ogr_csv_more_than_100_geom_fields():

    with gdaltest.error_handler():
        ds = ogr.Open('data/csv/more_than_100_geom_fields.csv')
    lyr = ds.GetLayer(0)
    lyr.GetNextFeature()

###############################################################################


def test_ogr_csv_string_quoting_always():

    gdal.VectorTranslate('/vsimem/ogr_csv_string_quoting_always.csv',
                         'data/poly.shp', format='CSV',
                         where='FID = 0',
                         layerCreationOptions=['CREATE_CSVT=YES', 'STRING_QUOTING=ALWAYS', 'LINEFORMAT=LF'])

    f = gdal.VSIFOpenL('/vsimem/ogr_csv_string_quoting_always.csv', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert data.startswith('"AREA","EAS_ID","PRFEDEA"\n215229.266,"168","35043411"')

    ds = gdal.OpenEx('/vsimem/ogr_csv_string_quoting_always.csv', gdal.OF_UPDATE | gdal.OF_VECTOR)
    gdal.VectorTranslate(ds, 'data/poly.shp',
                         layerName='ogr_csv_string_quoting_always',
                         where='FID = 1', accessMode='append')
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_csv_string_quoting_always.csv', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert data.startswith('"AREA","EAS_ID","PRFEDEA"\n215229.266,"168","35043411"\n247328.172,"179","35043423"')

    gdal.Unlink('/vsimem/ogr_csv_string_quoting_always.csv')
    gdal.Unlink('/vsimem/ogr_csv_string_quoting_always.csvt')
    gdal.Unlink('/vsimem/ogr_csv_string_quoting_always.prj')

###############################################################################


def test_ogr_csv_string_quoting_if_ambiguous():

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, gdal.GDT_Unknown)
    lyr = src_ds.CreateLayer('layer')
    lyr.CreateField(ogr.FieldDefn('foo'))
    lyr.CreateField(ogr.FieldDefn('bar'))
    lyr.CreateField(ogr.FieldDefn('baz'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['foo'] = '00123'
    f['bar'] = 'x'
    f['baz'] = '1.25'
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/ogr_csv_string_quoting_if_ambiguous.csv',
                         src_ds, format='CSV')

    f = gdal.VSIFOpenL('/vsimem/ogr_csv_string_quoting_if_ambiguous.csv', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert '"00123",x,"1.25"' in data

    gdal.Unlink('/vsimem/ogr_csv_string_quoting_if_ambiguous.csv')

###############################################################################


def test_ogr_csv_string_quoting_if_needed():

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, gdal.GDT_Unknown)
    lyr = src_ds.CreateLayer('layer')
    lyr.CreateField(ogr.FieldDefn('foo'))
    lyr.CreateField(ogr.FieldDefn('bar'))
    lyr.CreateField(ogr.FieldDefn('baz'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['foo'] = '00123'
    f['bar'] = 'x'
    f['baz'] = '1.25'
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/ogr_csv_string_quoting_if_needed.csv',
                         src_ds, format='CSV',
                         layerCreationOptions=['STRING_QUOTING=IF_NEEDED'])

    f = gdal.VSIFOpenL('/vsimem/ogr_csv_string_quoting_if_needed.csv', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert '00123,x,1.25' in data

    gdal.Unlink('/vsimem/ogr_csv_string_quoting_if_needed.csv')

###############################################################################


def test_ogr_csv_iter_and_set_feature():
    gdal.FileFromMemBuffer('/vsimem/ogr_csv_iter_and_set_feature.csv',
                           """id,str
1,
2,
""")

    ds = gdal.OpenEx('/vsimem/ogr_csv_iter_and_set_feature.csv',
                     gdal.OF_UPDATE)
    lyr = ds.GetLayer(0)
    count = 0
    for f in lyr:
        lyr.SetFeature(f)
        count += 1
    ds = None

    gdal.Unlink('/vsimem/ogr_csv_iter_and_set_feature.csv')

    assert count == 2
