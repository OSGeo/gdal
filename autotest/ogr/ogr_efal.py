#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  MapInfo EFAL driver testing.
# Author:   Pitney Bowes
#
###############################################################################
# Copyright (c) 2019, Frank Warmerdam <warmerdam@pobox.com>
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
import random
import sys
import shutil
import time


import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal
import test_cli_utilities
import pytest

def setup_mapinfo_test():
    try:
        gdaltest.mapinfo_drv
    except AttributeError:
        gdaltest.mapinfo_drv = None

    try:
        gdaltest.mapinfo_ds
    except AttributeError:
        gdaltest.mapinfo_ds = None

    if gdaltest.mapinfo_drv is None:
        gdaltest.mapinfo_drv = ogr.GetDriverByName('MapInfo EFAL')

    if gdaltest.mapinfo_drv is None:
        return

    if gdaltest.mapinfo_ds is None:
        gdaltest.mapinfo_ds = gdaltest.mapinfo_drv.CreateDataSource('tmp')

    return

###############################################################################
# Create table from data/poly.shp (TODO)
def test_ogr_efal_1():

    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    if gdaltest.mapinfo_ds is not None:
        return
    pytest.fail()


###############################################################################
# Create table from data/poly.shp


def test_ogr_efal_2():

    setup_mapinfo_test()
    if gdaltest.mapinfo_ds is None:
        pytest.skip()

    # This should convert to MapInfo datum name 'New_Zealand_GD49'
    WEIRD_SRS = 'PROJCS["NZGD49 / UTM zone 59S",GEOGCS["NZGD49",DATUM["NZGD49",SPHEROID["International 1924",6378388,297,AUTHORITY["EPSG","7022"]],TOWGS84[59.47,-5.04,187.44,0.47,-0.1,1.024,-4.5993],AUTHORITY["EPSG","6272"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4272"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",171],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",10000000],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","27259"]]'
    gdaltest.mapinfo_srs = osr.SpatialReference()
    gdaltest.mapinfo_srs.ImportFromWkt(WEIRD_SRS)

    #######################################################
    # Create memory Layer
    gdaltest.mapinfo_lyr = gdaltest.mapinfo_ds.CreateLayer('tpoly', gdaltest.mapinfo_srs)

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(gdaltest.mapinfo_lyr,
                                   [('AREA', ogr.OFTReal),
                                    ('EAS_ID', ogr.OFTInteger),
                                    ('PRFEDEA', ogr.OFTString)])

    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=gdaltest.mapinfo_lyr.GetLayerDefn())

    shp_ds = ogr.Open('data/poly.shp')
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    while feat is not None:

        gdaltest.poly_feat.append(feat)

        dst_feat.SetFrom(feat)
        gdaltest.mapinfo_lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    #######################################################
    # Close file.

    gdaltest.mapinfo_ds = None

###############################################################################
# Verify that stuff we just wrote is still OK.
#
# Note that we allow a fairly significant error since projected
# coordinates are not stored with much precision in Mapinfo format.


def test_ogr_efal_3():
    
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    gdaltest.mapinfo_ds = ogr.Open('tmp')
    gdaltest.mapinfo_lyr = gdaltest.mapinfo_ds.GetLayer(0)

    expect = [168, 169, 166, 158, 165]

    gdaltest.mapinfo_lyr.SetAttributeFilter('EAS_ID < 170')
    tr = ogrtest.check_features_against_list(gdaltest.mapinfo_lyr,
                                             'EAS_ID', expect)
    gdaltest.mapinfo_lyr.SetAttributeFilter(None)

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.mapinfo_lyr.GetNextFeature()

        assert (ogrtest.check_feature_geometry(read_feat,
                                          orig_feat.GetGeometryRef(),
                                          max_error=0.02) == 0), \
            ('Geometry check fail.  i=%d' % i)

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), \
                ('Attribute %d does not match' % fld)

    gdaltest.poly_feat = None
    gdaltest.shp_ds = None

    assert tr

###############################################################################
# Test ExecuteSQL() results layers with geometry.


def test_ogr_efal_4():

    setup_mapinfo_test()
    if gdaltest.mapinfo_ds is None:
        pytest.skip()

    sql_lyr = gdaltest.mapinfo_ds.ExecuteSQL(
        "select * from tpoly where prfedea = '35043413'")

    tr = ogrtest.check_features_against_list(sql_lyr, 'prfedea', ['35043413'])
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry(feat_read, 'POLYGON ((479750.688 4764702.000,479658.594 4764670.000,479640.094 4764721.000,479735.906 4764752.000,479750.688 4764702.000))', max_error=0.02) != 0:
            tr = 0

    gdaltest.mapinfo_ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Test spatial filtering.


def test_ogr_efal_5():

    setup_mapinfo_test()
    if gdaltest.mapinfo_ds is None:
        pytest.skip()

    gdaltest.mapinfo_lyr.SetAttributeFilter(None)

    gdaltest.mapinfo_lyr.SetSpatialFilterRect(479505, 4763195,
                                              480526, 4762819)

    tr = ogrtest.check_features_against_list(gdaltest.mapinfo_lyr, 'eas_id', [158])

    gdaltest.mapinfo_lyr.SetSpatialFilter(None)

    assert tr

###############################################################################
# Verify that Non-WGS84 datums are populated correctly


def test_ogr_efal_6():

    setup_mapinfo_test()
    if gdaltest.mapinfo_ds is None:
        pytest.skip()

    srs = gdaltest.mapinfo_lyr.GetSpatialRef()
    datum_name = srs.GetAttrValue('PROJCS|GEOGCS|DATUM')

    assert datum_name == "New_Zealand_GD49", \
        ("Datum name does not match (expected 'New_Zealand_GD49', got '%s')" % datum_name)

    #######################################################
    # Close file.

    gdaltest.mapinfo_ds = None
    gdaltest.mapinfo_drv.DeleteDataSource('tmp')

###############################################################################
# Create NativeX TAB file.


def test_ogr_efal_7():

    setup_mapinfo_test()
    if gdaltest.mapinfo_ds is None:
        pytest.skip()

    gdaltest.mapinfo_drv = ogr.GetDriverByName('MapInfo EFAL')
    
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    gdaltest.mapinfo_ds = gdaltest.mapinfo_drv.CreateDataSource('tmp', options=['FORMAT=NATIVEX'])

    if gdaltest.mapinfo_ds is not None:
        return
    pytest.fail()

###############################################################################
# Create table from data/poly.shp


def test_ogr_efal_8():

    setup_mapinfo_test()
    if gdaltest.mapinfo_ds is None:
        pytest.skip()

    # This should convert to MapInfo datum name 'New_Zealand_GD49'
    WEIRD_SRS = 'PROJCS["NZGD49 / UTM zone 59S",GEOGCS["NZGD49",DATUM["NZGD49",SPHEROID["International 1924",6378388,297,AUTHORITY["EPSG","7022"]],TOWGS84[59.47,-5.04,187.44,0.47,-0.1,1.024,-4.5993],AUTHORITY["EPSG","6272"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4272"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",171],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",10000000],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","27259"]]'
    gdaltest.mapinfo_srs = osr.SpatialReference()
    gdaltest.mapinfo_srs.ImportFromWkt(WEIRD_SRS)

    #######################################################
    # Create memory Layer
    gdaltest.mapinfo_lyr = gdaltest.mapinfo_ds.CreateLayer('tpoly', gdaltest.mapinfo_srs)

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(gdaltest.mapinfo_lyr,
                                   [('AREA', ogr.OFTReal),
                                    ('EAS_ID', ogr.OFTInteger),
                                    ('PRFEDEA', ogr.OFTString)])

    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=gdaltest.mapinfo_lyr.GetLayerDefn())

    shp_ds = ogr.Open('data/poly.shp')
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    while feat is not None:

        gdaltest.poly_feat.append(feat)

        dst_feat.SetFrom(feat)
        gdaltest.mapinfo_lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    #######################################################
    # Close file.

    gdaltest.mapinfo_ds = None

###############################################################################
# Verify that stuff we just wrote is still OK.
#
# Note that we allow a fairly significant error since projected
# coordinates are not stored with much precision in Mapinfo format.


def test_ogr_efal_9():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    gdaltest.mapinfo_ds = ogr.Open('tmp')
    gdaltest.mapinfo_lyr = gdaltest.mapinfo_ds.GetLayer(0)

    expect = [168, 169, 166, 158, 165]

    gdaltest.mapinfo_lyr.SetAttributeFilter('EAS_ID < 170')
    tr = ogrtest.check_features_against_list(gdaltest.mapinfo_lyr,
                                             'EAS_ID', expect)
    gdaltest.mapinfo_lyr.SetAttributeFilter(None)

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.mapinfo_lyr.GetNextFeature()

        assert (ogrtest.check_feature_geometry(read_feat,
                                          orig_feat.GetGeometryRef(),
                                          max_error=0.02) == 0), \
            ('Geometry check fail.  i=%d' % i)

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), \
                ('Attribute %d does not match' % fld)

    gdaltest.poly_feat = None
    gdaltest.shp_ds = None

    assert tr

###############################################################################
# Test ExecuteSQL() results layers with geometry.


def test_ogr_efal_10():

    setup_mapinfo_test()
    if gdaltest.mapinfo_ds is None:
        pytest.skip()

    sql_lyr = gdaltest.mapinfo_ds.ExecuteSQL(
        "select * from tpoly where prfedea = '35043413'")

    tr = ogrtest.check_features_against_list(sql_lyr, 'prfedea', ['35043413'])
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry(feat_read, 'POLYGON ((479750.688 4764702.000,479658.594 4764670.000,479640.094 4764721.000,479735.906 4764752.000,479750.688 4764702.000))', max_error=0.02) != 0:
            tr = 0

    gdaltest.mapinfo_ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Test spatial filtering.


def test_ogr_efal_11():
    setup_mapinfo_test()
    if gdaltest.mapinfo_ds is None:
        pytest.skip()

    gdaltest.mapinfo_lyr.SetAttributeFilter(None)

    gdaltest.mapinfo_lyr.SetSpatialFilterRect(479505, 4763195,
                                              480526, 4762819)

    tr = ogrtest.check_features_against_list(gdaltest.mapinfo_lyr, 'eas_id', [158])

    gdaltest.mapinfo_lyr.SetSpatialFilter(None)

    assert tr

###############################################################################
# Verify that Non-WGS84 datums are populated correctly


def test_ogr_efal_12():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    srs = gdaltest.mapinfo_lyr.GetSpatialRef()
    datum_name = srs.GetAttrValue('PROJCS|GEOGCS|DATUM')

    assert datum_name == "New_Zealand_GD49", \
        ("Datum name does not match (expected 'New_Zealand_GD49', got '%s')" % datum_name)

    #######################################################
    # Close file.

    gdaltest.mapinfo_ds = None
    gdaltest.mapinfo_drv.DeleteDataSource('tmp')

###############################################################################
# Verify that WindowsArabic Localized files open correctly.


def test_ogr_efal_13():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()


    arabic_ds = ogr.Open('data/efal/STATES_arabic.tab')
    assert arabic_ds is not None

    arabic_lr = arabic_ds.GetLayer(0)
    assert arabic_lr is not None

    arabic_lr_def = arabic_lr.GetLayerDefn()
    assert arabic_lr_def is not None

    data = [[u'سفشفث'.encode('utf-8'), ogr.OFTString, 20, 0],
            [u'غعنش'.encode('utf-8'), ogr.OFTString, 2, 0],
            [u'شقشلاهاب'.encode('utf-8'), ogr.OFTString, 2, 0]]
    
    for field in data:
        fld = arabic_lr_def.GetFieldDefn(arabic_lr_def.GetFieldIndex(field[0].decode('utf-8')))
        assert fld is not None
        expected_with = field[2]
        if fld.GetType() == ogr.OFTInteger:
            expected_with = 0
        assert fld.GetType() == field[1] and fld.GetWidth() == expected_with and fld.GetPrecision() == field[3], \
            (field[0].decode('utf-8') + ' field definition wrong.')
    
    #######################################################
    # Close file.
    arabic_ds = None

###############################################################################
# Verify that UTF-8 Localized files open correctly.


def test_ogr_efal_14():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()


    utf8_ds = ogr.Open('data/efal/STATES_utf8.tab')
    assert utf8_ds is not None

    utf8_lr = utf8_ds.GetLayer(0)
    assert utf8_lr is not None

    utf8_lr_def = utf8_lr.GetLayerDefn()
    assert utf8_lr_def is not None

    data = [[u'سفشفث'.encode('utf-8'), ogr.OFTString, 20, 0],
            [u'غعنش'.encode('utf-8'), ogr.OFTString, 2, 0],
            [u'شقشلاهاب'.encode('utf-8'), ogr.OFTString, 2, 0]]
    
    for field in data:
        fld = utf8_lr_def.GetFieldDefn(utf8_lr_def.GetFieldIndex(field[0].decode('utf-8')))
        assert fld is not None
        expected_with = field[2]
        if fld.GetType() == ogr.OFTInteger:
            expected_with = 0
        assert fld.GetType() == field[1] and fld.GetWidth() == expected_with and fld.GetPrecision() == field[3], \
            (field[0].decode('utf-8') + ' field definition wrong.')
    
    #######################################################
    # Close file.
    utf8_ds = None


###############################################################################
# Verify that UTF-16 Localized files open correctly.


def test_ogr_efal_15():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()


    utf16_ds = ogr.Open('data/efal/STATES_utf16.tab')
    assert utf16_ds is not None

    utf16_lr = utf16_ds.GetLayer(0)
    assert utf16_lr is not None

    utf16_lr_def = utf16_lr.GetLayerDefn()
    assert utf16_lr_def is not None

    data = [[u'سفشفث'.encode('utf-8'), ogr.OFTString, 20, 0],
            [u'غعنش'.encode('utf-8'), ogr.OFTString, 2, 0],
            [u'شقشلاهاب'.encode('utf-8'), ogr.OFTString, 2, 0]]
    
    for field in data:
        fld = utf16_lr_def.GetFieldDefn(utf16_lr_def.GetFieldIndex(field[0].decode('utf-8')))
        assert fld is not None
        expected_with = field[2]
        if fld.GetType() == ogr.OFTInteger:
            expected_with = 0
        assert fld.GetType() == field[1] and fld.GetWidth() == expected_with and fld.GetPrecision() == field[3], \
            (field[0].decode('utf-8') + ' field definition wrong.')
    
    #######################################################
    # Close file.
    utf16_ds = None

###############################################################################
# Verify creating a Native tab file without feature.


def test_ogr_efal_16():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    ds = ogr.GetDriverByName('MapInfo EFAL').CreateDataSource('/mem/ogr_efal_18.tab')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(2154)
    lyr = ds.CreateLayer('test', srs=sr)
    lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
    ds = None

    # Test with our generated file, and with one generated by MapInfo
    for filename in ['/mem/ogr_efal_18.tab', 'data/lambert93_francais.TAB']:
        ds = ogr.Open(filename, update=1)
        lyr = ds.GetLayer(0)
        sr_got = lyr.GetSpatialRef()
        wkt = sr_got.ExportToWkt()
        if '2154' not in wkt:
            print(filename)
            pytest.fail(sr_got)
        proj4 = sr_got.ExportToProj4()
        assert proj4.startswith('+proj=lcc +lat_0=46.5 +lon_0=3 +lat_1=49 +lat_2=44 +x_0=700000 +y_0=6600000 +ellps=GRS80 +towgs84=0,0,0,0,0,0,0 +units=m +no_defs')
        ds = None

    ogr.GetDriverByName('MapInfo EFAL').DeleteDataSource('/mem/ogr_efal_18.tab')


###############################################################################
# Verify creating a NativeX tab file without feature.


def test_ogr_efal_17():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    ds = ogr.GetDriverByName('MapInfo EFAL').CreateDataSource('/mem/ogr_efal_19.tab', options=['FORMAT=NATIVEX'])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(2154)
    lyr = ds.CreateLayer('test', srs=sr)
    lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
    ds = None

    # Test with our generated file, and with one generated by MapInfo
    for filename in ['/mem/ogr_efal_19.tab', 'data/lambert93_francais.TAB']:
        ds = ogr.Open(filename, update=1)
        lyr = ds.GetLayer(0)
        sr_got = lyr.GetSpatialRef()
        wkt = sr_got.ExportToWkt()
        if '2154' not in wkt:
            print(filename)
            pytest.fail(sr_got)
        proj4 = sr_got.ExportToProj4()
        assert proj4.startswith('+proj=lcc +lat_0=46.5 +lon_0=3 +lat_1=49 +lat_2=44 +x_0=700000 +y_0=6600000 +ellps=GRS80 +towgs84=0,0,0,0,0,0,0 +units=m +no_defs')
        ds = None

    ogr.GetDriverByName('MapInfo EFAL').DeleteDataSource('/mem/ogr_efal_19.tab')


# ###############################################################################
# Create .tab without explicit field


def test_ogr_efal_18():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    ds = ogr.GetDriverByName('MapInfo EFAL').CreateDataSource('/mem/ogr_efal_20.tab')
    lyr = ds.CreateLayer('test')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    ds = None

    ds = ogr.Open('/mem/ogr_efal_20.tab')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetField('FID') != 0:
        feat.DumpReadable()
        pytest.fail()
    ds = None

    ogr.GetDriverByName('MapInfo EFAL').DeleteDataSource('/mem/ogr_efal_20.tab')

###############################################################################
# Test append in update mode


def test_ogr_efal_19():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    filename = '/mem/ogr_efal_21.tab'
    for nb_features in (2, 100):
        if nb_features == 2:
            nb_runs = 2
        else:
            nb_runs = 1

        # When doing 2 runs, in the second one, we create an empty
        # .tab and then open it for update. This can trigger specific bugs
        for j in range(nb_runs):
            ds = ogr.GetDriverByName('MapInfo EFAL').CreateDataSource(filename)
            lyr = ds.CreateLayer('test')
            lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
            if j == 0:
                i = 0
                feat = ogr.Feature(lyr.GetLayerDefn())
                feat.SetField('ID', i + 1)
                feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (%d %d)" % (i, i)))
                if lyr.CreateFeature(feat) != 0:
                    print(i)
                    pytest.fail(nb_features)
            ds = None

            for i in range(nb_features - (1 - j)):
                ds = ogr.Open(filename, update=1)
                lyr = ds.GetLayer(0)
                feat = ogr.Feature(lyr.GetLayerDefn())
                feat.SetField('ID', i + 1 + (1 - j))
                feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (%d %d)" % (i + (1 - j), i + (1 - j))))
                if lyr.CreateFeature(feat) != 0:
                    print(i)
                    pytest.fail(nb_features)
                ds = None

            ds = ogr.Open(filename)
            lyr = ds.GetLayer(0)
            for i in range(nb_features):
                f = lyr.GetNextFeature()
                assert f is not None and f.GetField('ID') == i + 1, nb_features
            ds = None

    ogr.GetDriverByName('MapInfo EFAL').DeleteDataSource(filename)

###############################################################################
# Test creating features then reading


def test_ogr_efal_20():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    filename = '/mem/ogr_efal_22.tab'

    for nb_features in (0, 1, 2, 100, 1000):
        ds = ogr.GetDriverByName('MapInfo EFAL').CreateDataSource(filename)
        lyr = ds.CreateLayer('test')
        lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
        for i in range(nb_features):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('ID', i + 1)
            feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
            lyr.CreateFeature(feat)

        lyr.ResetReading()
        for i in range(nb_features):
            f = lyr.GetNextFeature()
            assert f is not None and f.GetField('ID') == i + 1, nb_features
        f = lyr.GetNextFeature()
        assert f is None
        ds = None

        ogr.GetDriverByName('MapInfo EFAL').DeleteDataSource(filename)

###############################################################################
# Test creating features then reading then creating again then reading


def test_ogr_efal_21():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    filename = '/mem/ogr_efal_22.tab'

    for nb_features in (2, 100, 1000):
        ds = ogr.GetDriverByName('MapInfo EFAL').CreateDataSource(filename)
        lyr = ds.CreateLayer('test')
        lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
        for i in range(int(nb_features / 2)):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('ID', i + 1)
            feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
            lyr.CreateFeature(feat)

        lyr.ResetReading()
        for i in range(int(nb_features / 2)):
            f = lyr.GetNextFeature()
            assert f is not None and f.GetField('ID') == i + 1, nb_features
        f = lyr.GetNextFeature()
        assert f is None

        for i in range(int(nb_features / 2)):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('ID', nb_features / 2 + i + 1)
            feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
            lyr.CreateFeature(feat)

        lyr.ResetReading()
        for i in range(nb_features):
            f = lyr.GetNextFeature()
            assert f is not None and f.GetField('ID') == i + 1, nb_features
        f = lyr.GetNextFeature()
        assert f is None

        ds = None

        ogr.GetDriverByName('MapInfo EFAL').DeleteDataSource(filename)


###############################################################################
# Test DeleteFeature()


def test_ogr_efal_22():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    filename = '/mem/ogr_efal_23.tab'

    for nb_features in (2, 1000):
        if nb_features == 2:
            nb_runs = 2
        else:
            nb_runs = 1
        for j in range(nb_runs):
            ds = ogr.GetDriverByName('MapInfo EFAL').CreateDataSource(filename)
            lyr = ds.CreateLayer('test')
            lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
            for i in range(nb_features):
                feat = ogr.Feature(lyr.GetLayerDefn())
                feat.SetField('ID', i + 1)
                feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (%d %d)" % (i, i)))
                lyr.CreateFeature(feat)

            if nb_features == 2:
                assert lyr.DeleteFeature(int(nb_features / 2)) == 0, j
            else:
                for k in range(int(nb_features / 2)):
                    assert lyr.DeleteFeature(int(nb_features / 4) + k) == 0, j

            if j == 1:
                # Expected failure : already deleted feature
                ret = lyr.DeleteFeature(int(nb_features / 2))
                if ret != ogr.OGRERR_NON_EXISTING_FEATURE:
                    print(j)
                    pytest.fail(nb_features)

                feat = lyr.GetFeature(int(nb_features / 2))
                if feat is not None:
                    print(j)
                    pytest.fail(nb_features)

                # Expected failure : illegal feature id
                ret = lyr.DeleteFeature(nb_features + 1)
                if ret != ogr.OGRERR_NON_EXISTING_FEATURE:
                    print(j)
                    pytest.fail(nb_features)

            ds = None

            ds = ogr.Open(filename)
            lyr = ds.GetLayer(0)
            assert lyr.GetFeatureCount() == nb_features / 2
            ds = None

            if nb_features == 1000:
                ds = ogr.Open(filename, update=1)
                lyr = ds.GetLayer(0)
                lyr.DeleteFeature(245)
                ds = None

                ds = ogr.Open(filename)
                lyr = ds.GetLayer(0)
                assert lyr.GetFeatureCount() == nb_features / 2 - 1
                ds = None

            ogr.GetDriverByName('MapInfo EFAL').DeleteDataSource(filename)

###############################################################################
# Test file without map


def test_ogr_efal_23():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    for update in (0, 1):
        ds = ogr.Open('data/aspatial-table.tab', update=update)
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2, update
        f = lyr.GetNextFeature()
        assert f.GetField('a') == 1 and f.GetField('b') == 2 and f.GetField('d') == 'hello', \
            update
        f = lyr.GetFeature(2)
        assert f.GetField('a') == 4, update
    ds = None


###############################################################################
# Simple testing of Seamless tables


def test_ogr_efal_24():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    ds = ogr.Open('data/efal/seamless.tab')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 4

    f = lyr.GetNextFeature()
    assert f.id == '1'

    f = lyr.GetNextFeature()
    assert f.id == '2'

    f = lyr.GetNextFeature()
    assert f.id == '3'

    f = lyr.GetNextFeature()
    assert f.id == '4'
    
    ds = None

#####################################################################################
# Testing the driver field alteration capability.


def test_ogr_efal_25():
    setup_mapinfo_test()
    if gdaltest.mapinfo_drv is None:
        pytest.skip()

    ds = ogr.GetDriverByName('MapInfo EFAL').CreateDataSource('tmp/rfcefal_test.tab')
    lyr = ds.CreateLayer('rfcefal_test')

    fd = ogr.FieldDefn('foo5', ogr.OFTString)
    fd.SetWidth(5)
    lyr.CreateField(fd)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'foo0')
    lyr.CreateFeature(feat)
    feat = None

    fd = ogr.FieldDefn('bar10', ogr.OFTString)
    fd.SetWidth(10)
    lyr.CreateField(fd)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'foo1')
    feat.SetField(1, 'bar1')
    lyr.CreateFeature(feat)
    feat = None

    fd = ogr.FieldDefn('baz15', ogr.OFTString)
    fd.SetWidth(15)
    lyr.CreateField(fd)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'foo2')
    feat.SetField(1, 'bar2_01234')
    feat.SetField(2, 'baz2_0123456789')
    lyr.CreateFeature(feat)
    feat = None

    fd = ogr.FieldDefn('baw20', ogr.OFTString)
    fd.SetWidth(20)
    lyr.CreateField(fd)
    
    assert lyr.TestCapability(ogr.OLCReorderFields) == 0
    assert lyr.TestCapability(ogr.OLCDeleteField) == 0
    assert lyr.TestCapability(ogr.OLCAlterFieldDefn) == 0
    
    lyr = None
    ds = None

def test_ogr_efal_cleanup():
    gdaltest.mapinfo_ds = None
    if gdaltest.mapinfo_drv is not None:
        gdaltest.mapinfo_drv.DeleteDataSource('tmp')



