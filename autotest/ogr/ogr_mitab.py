#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  MapInfo driver testing.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2012-2014, Even Rouault <even dot rouault at spatialys.com>
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

pytestmark = pytest.mark.require_driver('MapInfo File')


###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    gdaltest.mapinfo_drv = ogr.GetDriverByName('MapInfo File')
    gdaltest.mapinfo_ds = gdaltest.mapinfo_drv.CreateDataSource('tmp')

    assert gdaltest.mapinfo_ds is not None

    yield

    fl = gdal.ReadDir('/vsimem/')
    if fl is not None:
        print(fl)

    gdaltest.mapinfo_ds = None
    gdaltest.mapinfo_drv.DeleteDataSource('tmp')

###############################################################################
# Create table from data/poly.shp


def test_ogr_mitab_2():

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


def test_ogr_mitab_3():

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


def test_ogr_mitab_4():

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


def test_ogr_mitab_5():

    gdaltest.mapinfo_lyr.SetAttributeFilter(None)

    gdaltest.mapinfo_lyr.SetSpatialFilterRect(479505, 4763195,
                                              480526, 4762819)

    tr = ogrtest.check_features_against_list(gdaltest.mapinfo_lyr, 'eas_id',
                                             [158])

    gdaltest.mapinfo_lyr.SetSpatialFilter(None)

    assert tr

###############################################################################
# Verify that Non-WGS84 datums are populated correctly


def test_ogr_mitab_6():

    srs = gdaltest.mapinfo_lyr.GetSpatialRef()
    datum_name = srs.GetAttrValue('PROJCS|GEOGCS|DATUM')

    assert datum_name == "New_Zealand_GD49", \
        ("Datum name does not match (expected 'New_Zealand_GD49', got '%s')" % datum_name)

###############################################################################
# Create MIF file.


def test_ogr_mitab_7():

    gdaltest.mapinfo_ds = None
    gdaltest.mapinfo_drv.DeleteDataSource('tmp')

    gdaltest.mapinfo_ds = gdaltest.mapinfo_drv.CreateDataSource('tmp/wrk.mif')

    assert gdaltest.mapinfo_ds is not None

###############################################################################
# Create table from data/poly.shp


def test_ogr_mitab_8():

    #######################################################
    # Create memory Layer
    gdaltest.mapinfo_lyr = gdaltest.mapinfo_ds.CreateLayer('tpoly')

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


def test_ogr_mitab_9():

    gdaltest.mapinfo_ds = ogr.Open('tmp')
    gdaltest.mapinfo_lyr = gdaltest.mapinfo_ds.GetLayer(0)

    expect = [168, 169, 166, 158, 165]

    gdaltest.mapinfo_lyr.SetAttributeFilter('eas_id < 170')
    tr = ogrtest.check_features_against_list(gdaltest.mapinfo_lyr,
                                             'eas_id', expect)
    gdaltest.mapinfo_lyr.SetAttributeFilter(None)

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.mapinfo_lyr.GetNextFeature()

        assert (ogrtest.check_feature_geometry(read_feat, orig_feat.GetGeometryRef(),
                                          max_error=0.000000001) == 0)

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), \
                ('Attribute %d does not match' % fld)

    gdaltest.poly_feat = None
    gdaltest.shp_ds = None

    assert tr

###############################################################################
# Read mif file with 2 character .mid delimiter and verify operation.


def test_ogr_mitab_10():

    ds = ogr.Open('data/mitab/small.mif')
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()

    assert feat.NAME == " S. 11th St.", 'name attribute wrong.'

    assert feat.FLOODZONE == 10, 'FLOODZONE attribute wrong.'

    assert (ogrtest.check_feature_geometry(feat,
                                      'POLYGON ((407131.721 155322.441,407134.468 155329.616,407142.741 155327.242,407141.503 155322.467,407140.875 155320.049,407131.721 155322.441))',
                                      max_error=0.000000001) == 0)

    feat = lyr.GetNextFeature()

    assert feat.OWNER == 'Guarino "Chucky" Sandra', 'owner attribute wrong.'

    lyr = None
    ds = None

###############################################################################
# Verify support for NTF datum with non-greenwich datum per
#    http://trac.osgeo.org/gdal/ticket/1416
#
# This test also exercises SRS reference counting as described in issue:
#    http://trac.osgeo.org/gdal/ticket/1680


def test_ogr_mitab_11():

    ds = ogr.Open('data/mitab/small_ntf.mif')
    srs = ds.GetLayer(0).GetSpatialRef()
    ds = None

    pm_value = srs.GetAttrValue('PROJCS|GEOGCS|PRIMEM', 1)
    assert pm_value[:6] == '2.3372', \
        ('got unexpected prime meridian, not paris: ' + pm_value)

###############################################################################
# Verify that a newly created mif layer returns a non null layer definition


def test_ogr_mitab_12():

    ds = gdaltest.mapinfo_drv.CreateDataSource('tmp', options=['FORMAT=MIF'])
    lyr = ds.CreateLayer('testlyrdef')
    defn = lyr.GetLayerDefn()

    assert defn is not None

    ogrtest.quick_create_layer_def(lyr, [('AREA', ogr.OFTReal)])

    ds = None

###############################################################################
# Verify that field widths and precisions are propagated correctly in TAB.


def test_ogr_mitab_13():

    ds = ogr.Open('../ogr/data/mitab/testlyrdef.gml')
    if ds is None:
        pytest.skip()

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/testlyrdef.tab')
        ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/testlyrdef.tab')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f "MapInfo File" tmp/testlyrdef.tab ../ogr/data/mitab/testlyrdef.gml')

    ds = ogr.Open('tmp/testlyrdef.tab')

    # Check if the width and precision are as preserved.
    lyr = ds.GetLayer('testlyrdef')
    assert lyr is not None, 'Layer missing.'

    defn = lyr.GetLayerDefn()

    data = [['AREA', ogr.OFTReal, 7, 4],
            ['VOLUME', ogr.OFTReal, 0, 0],
            ['LENGTH', ogr.OFTInteger, 10, 0],
            ['WIDTH', ogr.OFTInteger, 4, 0]]

    for field in data:
        fld = defn.GetFieldDefn(defn.GetFieldIndex(field[0]))
        assert fld.GetType() == field[1] and fld.GetWidth() == field[2] and fld.GetPrecision() == field[3], \
            (field[0] + ' field definition wrong.')

    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/testlyrdef.tab')

###############################################################################
# Verify that field widths and precisions are propagated correctly in MIF.


def test_ogr_mitab_14():

    ds = ogr.Open('../ogr/data/mitab/testlyrdef.gml')
    if ds is None:
        pytest.skip()

    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        os.stat('tmp/testlyrdef.mif')
        ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/testlyrdef.mif')
    except (OSError, AttributeError):
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f "MapInfo File" -dsco FORMAT=MIF tmp/testlyrdef.mif ../ogr/data/mitab/testlyrdef.gml')

    ds = ogr.Open('tmp/testlyrdef.mif')

    # Check if the width and precision are as preserved.
    lyr = ds.GetLayer('testlyrdef')
    assert lyr is not None, 'Layer missing.'

    defn = lyr.GetLayerDefn()

    data = [['AREA', ogr.OFTReal, 7, 4],
            ['VOLUME', ogr.OFTReal, 0, 0],
            ['LENGTH', ogr.OFTInteger, 254, 0],
            ['WIDTH', ogr.OFTInteger, 254, 0]]

    for field in data:
        fld = defn.GetFieldDefn(defn.GetFieldIndex(field[0]))
        expected_with = field[2]
        if fld.GetType() == ogr.OFTInteger:
            expected_with = 0
        assert fld.GetType() == field[1] and fld.GetWidth() == expected_with and fld.GetPrecision() == field[3], \
            (field[0] + ' field definition wrong.')

    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/testlyrdef.mif')

###############################################################################
# Test .mif without .mid (#5141)


def test_ogr_mitab_15():

    ds = ogr.Open('data/mitab/nomid.mif')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat is not None
    ds = None

    # Test opening .mif without .mid even if there are declared attributes
    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource('/vsimem/nomid.mif')
    lyr = ds.CreateLayer('empty')
    lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 1)
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    lyr.CreateFeature(f)
    ds = None

    gdal.Unlink('/vsimem/nomid.mid')
    ds = ogr.Open('/vsimem/nomid.mif')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.IsFieldSet(0) or f.GetGeometryRef() is None:
        f.DumpReadable()
        pytest.fail()
    gdal.Unlink('/vsimem/nomid.mif')

###############################################################################
# Test empty .mif


def test_ogr_mitab_16():

    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource('tmp/empty.mif')
    lyr = ds.CreateLayer('empty')
    lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
    ds = None

    ds = ogr.Open('tmp/empty.mif')
    assert ds is not None
    ds = None

###############################################################################
# Run test_ogrsf


def test_ogr_mitab_17():

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp')
    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/wrk.mif')
    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Test EPSG:2154
# (https://github.com/mapgears/mitab/issues/1)


def test_ogr_mitab_18():

    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource('/vsimem/ogr_mitab_18.tab')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(2154)
    lyr = ds.CreateLayer('test', srs=sr)
    lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
    ds = None

    # Test with our generated file, and with one generated by MapInfo
    for filename in ['/vsimem/ogr_mitab_18.tab', 'data/mitab/lambert93_francais.TAB']:
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

    ogr.GetDriverByName('MapInfo File').DeleteDataSource('/vsimem/ogr_mitab_18.tab')

###############################################################################
# Check that we correctly round coordinate to the appropriate precision
# (https://github.com/mapgears/mitab/issues/2)


def test_ogr_mitab_19():

    ds = ogr.Open('data/mitab/utm31.TAB')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    # Strict text comparison to check precision
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (485248.12 2261.45)':
        feat.DumpReadable()
        pytest.fail()



###############################################################################
# Check that we take into account the user defined bound file
# (https://github.com/mapgears/mitab/issues/3)
# Also test BOUNDS layer creation option (http://trac.osgeo.org/gdal/ticket/5642)

def test_ogr_mitab_20():

    # Pass i==0: without MITAB_BOUNDS_FILE
    # Pass i==1: with MITAB_BOUNDS_FILE and French bounds : first load
    # Pass i==2: with MITAB_BOUNDS_FILE and French bounds : should use already loaded file
    # Pass i==3: without MITAB_BOUNDS_FILE : should unload the file
    # Pass i==4: use BOUNDS layer creation option
    # Pass i==5: with MITAB_BOUNDS_FILE and European bounds
    # Pass i==6: with MITAB_BOUNDS_FILE and generic EPSG:2154 (Europe bounds expected)
    for fmt in ['tab', 'mif']:
        for i in range(7):
            if i == 1 or i == 2 or i == 5 or i == 6:
                gdal.SetConfigOption('MITAB_BOUNDS_FILE', 'data/mitab/mitab_bounds.txt')
            ds = ogr.GetDriverByName('MapInfo File').CreateDataSource('/vsimem/ogr_mitab_20.' + fmt)
            sr = osr.SpatialReference()
            if i == 1 or i == 2:  # French bounds
                sr.SetFromUserInput("""PROJCS["RGF93 / Lambert-93",
        GEOGCS["RGF93",
            DATUM["Reseau_Geodesique_Francais_1993",
                SPHEROID["GRS 80",6378137,298.257222101],
                TOWGS84[0,0,0,0,0,0,0]],
            PRIMEM["Greenwich",0],
            UNIT["degree",0.0174532925199433]],
        PROJECTION["Lambert_Conformal_Conic_2SP"],
        PARAMETER["standard_parallel_1",49.00000000002],
        PARAMETER["standard_parallel_2",44],
        PARAMETER["latitude_of_origin",46.5],
        PARAMETER["central_meridian",3],
        PARAMETER["false_easting",700000],
        PARAMETER["false_northing",6600000],
        UNIT["Meter",1.0],
        AUTHORITY["EPSG","2154"]]""")
            elif i == 5:  # European bounds
                sr.SetFromUserInput("""PROJCS["RGF93 / Lambert-93",
        GEOGCS["RGF93",
            DATUM["Reseau_Geodesique_Francais_1993",
                SPHEROID["GRS 80",6378137,298.257222101],
                TOWGS84[0,0,0,0,0,0,0]],
            PRIMEM["Greenwich",0],
            UNIT["degree",0.0174532925199433]],
        PROJECTION["Lambert_Conformal_Conic_2SP"],
        PARAMETER["standard_parallel_1",49.00000000001],
        PARAMETER["standard_parallel_2",44],
        PARAMETER["latitude_of_origin",46.5],
        PARAMETER["central_meridian",3],
        PARAMETER["false_easting",700000],
        PARAMETER["false_northing",6600000],
        UNIT["Meter",1.0],
        AUTHORITY["EPSG","2154"]]""")
            else:
                sr.ImportFromEPSG(2154)
            if i == 4:
                lyr = ds.CreateLayer('test', srs=sr, options=['BOUNDS=75000,6000000,1275000,7200000'])
            else:
                lyr = ds.CreateLayer('test', srs=sr)
            lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (700000.001 6600000.001)"))
            lyr.CreateFeature(feat)
            ds = None
            gdal.SetConfigOption('MITAB_BOUNDS_FILE', None)

            ds = ogr.Open('/vsimem/ogr_mitab_20.' + fmt)
            lyr = ds.GetLayer(0)
            feat = lyr.GetNextFeature()
            assert not (i == 6 and lyr.GetSpatialRef().ExportToWkt().find('49.00000000001') < 0), \
                fmt
            # Strict text comparison to check precision
            if fmt == 'tab':
                if i == 1 or i == 2 or i == 4:
                    if feat.GetGeometryRef().ExportToWkt() != 'POINT (700000.001 6600000.001)':
                        print(i)
                        feat.DumpReadable()
                        pytest.fail(fmt)
                else:
                    if feat.GetGeometryRef().ExportToWkt() == 'POINT (700000.001 6600000.001)':
                        print(i)
                        feat.DumpReadable()
                        pytest.fail(fmt)

            ds = None

            ogr.GetDriverByName('MapInfo File').DeleteDataSource('/vsimem/ogr_mitab_20.' + fmt)

    gdal.SetConfigOption('MITAB_BOUNDS_FILE', 'tmp/mitab_bounds.txt')
    for i in range(2):
        if i == 1 and not sys.platform.startswith('linux'):
            time.sleep(1)

        f = open('tmp/mitab_bounds.txt', 'wb')
        if i == 0:
            f.write(
                """Source = CoordSys Earth Projection 3, 33, "m", 3, 46.5, 44, 49, 700000, 6600000
Destination=CoordSys Earth Projection 3, 33, "m", 3, 46.5, 44, 49.00000000001, 700000, 6600000 Bounds (-792421, 5278231) (3520778, 9741029)""".encode('ascii'))
        else:
            f.write(
                """Source = CoordSys Earth Projection 3, 33, "m", 3, 46.5, 44, 49, 700000, 6600000
Destination=CoordSys Earth Projection 3, 33, "m", 3, 46.5, 44, 49.00000000002, 700000, 6600000 Bounds (75000, 6000000) (1275000, 7200000)""".encode('ascii'))
        f.close()

        if i == 1 and sys.platform.startswith('linux'):
            os.system('touch -d "1 minute ago" tmp/mitab_bounds.txt')

        ds = ogr.GetDriverByName('MapInfo File').CreateDataSource('/vsimem/ogr_mitab_20.tab')
        sr = osr.SpatialReference()
        sr.ImportFromEPSG(2154)
        lyr = ds.CreateLayer('test', srs=sr)
        lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (700000.001 6600000.001)"))
        lyr.CreateFeature(feat)
        ds = None
        ds = ogr.Open('/vsimem/ogr_mitab_20.tab')
        lyr = ds.GetLayer(0)
        if i == 0:
            expected = '49.00000000001'
        else:
            expected = '49.00000000002'
        if lyr.GetSpatialRef().ExportToWkt().find(expected) < 0:
            print(i)
            gdal.SetConfigOption('MITAB_BOUNDS_FILE', None)
            os.unlink('tmp/mitab_bounds.txt')
            pytest.fail(lyr.GetSpatialRef().ExportToWkt())
        ds = None
        ogr.GetDriverByName('MapInfo File').DeleteDataSource('/vsimem/ogr_mitab_20.tab')

    gdal.SetConfigOption('MITAB_BOUNDS_FILE', None)
    os.unlink('tmp/mitab_bounds.txt')

###############################################################################
# Create .tab without explicit field


def test_ogr_mitab_21():

    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource('/vsimem/ogr_mitab_21.tab')
    lyr = ds.CreateLayer('test')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (0 0)"))
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    ds = None

    ds = ogr.Open('/vsimem/ogr_mitab_21.tab')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetField('FID') != 1:
        feat.DumpReadable()
        pytest.fail()
    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource('/vsimem/ogr_mitab_21.tab')

###############################################################################
# Test append in update mode


def test_ogr_mitab_22():

    filename = '/vsimem/ogr_mitab_22.tab'
    for nb_features in (2, 1000):
        if nb_features == 2:
            nb_runs = 2
        else:
            nb_runs = 1

        # When doing 2 runs, in the second one, we create an empty
        # .tab and then open it for update. This can trigger specific bugs
        for j in range(nb_runs):
            ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
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

    ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)

###############################################################################
# Test creating features then reading


def test_ogr_mitab_23():

    filename = '/vsimem/ogr_mitab_23.tab'

    for nb_features in (0, 1, 2, 100, 1000):
        ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
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

        ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)


###############################################################################
# Test creating features then reading then creating again then reading


def test_ogr_mitab_24():

    filename = '/vsimem/ogr_mitab_24.tab'

    for nb_features in (2, 100, 1000):
        ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
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

        ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)


###############################################################################
# Test that opening in update mode without doing any change does not alter
# file


def test_ogr_mitab_25():

    filename = 'tmp/ogr_mitab_25.tab'

    for nb_features in (2, 1000):
        ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
        lyr = ds.CreateLayer('test')
        lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
        for i in range(int(nb_features / 2)):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('ID', i + 1)
            feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (%d %d)" % (i, i)))
            lyr.CreateFeature(feat)
        ds = None

        if sys.platform.startswith('linux'):
            for ext in ('map', 'tab', 'dat', 'id'):
                os.system('touch -d "1 minute ago" %s' % filename[0:-3] + ext)

        mtime_dict = {}
        for ext in ('map', 'tab', 'dat', 'id'):
            mtime_dict[ext] = os.stat(filename[0:-3] + ext).st_mtime

        if not sys.platform.startswith('linux'):
            time.sleep(1)

        # Try without doing anything
        ds = ogr.Open(filename, update=1)
        ds = None
        for ext in ('map', 'tab', 'dat', 'id'):
            mtime = os.stat(filename[0:-3] + ext).st_mtime
            assert mtime_dict[ext] == mtime, ('mtime of .%s has changed !' % ext)

        # Try by reading all features
        ds = ogr.Open(filename, update=1)
        lyr = ds.GetLayer(0)
        lyr.GetFeatureCount(1)
        ds = None
        for ext in ('map', 'tab', 'dat', 'id'):
            mtime = os.stat(filename[0:-3] + ext).st_mtime
            assert mtime_dict[ext] == mtime, ('mtime of .%s has changed !' % ext)

        # Try by reading all features with a spatial index
        ds = ogr.Open(filename, update=1)
        lyr = ds.GetLayer(0)
        lyr.SetSpatialFilterRect(0.5, 0.5, 1.5, 1.5)
        lyr.GetFeatureCount(1)
        ds = None
        for ext in ('map', 'tab', 'dat', 'id'):
            mtime = os.stat(filename[0:-3] + ext).st_mtime
            assert mtime_dict[ext] == mtime, ('mtime of .%s has changed !' % ext)

        if test_cli_utilities.get_test_ogrsf_path() is not None:
            ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro -fsf ' + filename)
            assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

        ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)


###############################################################################
# Test DeleteFeature()


def test_ogr_mitab_26():

    filename = '/vsimem/ogr_mitab_26.tab'

    for nb_features in (2, 1000):
        if nb_features == 2:
            nb_runs = 2
        else:
            nb_runs = 1
        for j in range(nb_runs):
            ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
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

            # This used to trigger a bug in DAT record deletion during implementation...
            if nb_features == 1000:
                ds = ogr.Open(filename, update=1)
                lyr = ds.GetLayer(0)
                lyr.DeleteFeature(245)
                ds = None

                ds = ogr.Open(filename)
                lyr = ds.GetLayer(0)
                assert lyr.GetFeatureCount() == nb_features / 2 - 1
                ds = None

            ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)


###############################################################################
# Test SetFeature()


def test_ogr_mitab_27():

    filename = '/vsimem/ogr_mitab_27.tab'

    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('realfield', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('stringfield', ogr.OFTString))

    # Invalid call : feature without FID
    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    # Invalid call : feature with FID <= 0
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(0)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('intfield', 1)
    f.SetField('realfield', 2.34)
    f.SetField('stringfield', "foo")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    lyr.CreateFeature(f)
    fid = f.GetFID()

    # Invalid call : feature with FID > feature_count
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(2)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    # Update previously created object with blank feature
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(fid)
    lyr.SetFeature(f)

    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetField('intfield') != 0 or f.GetField('realfield') != 0 or f.GetField('stringfield') != '' or \
       f.GetGeometryRef() is not None:
        f.DumpReadable()
        pytest.fail()

    f.SetField('intfield', 1)
    f.SetField('realfield', 2.34)
    f.SetField('stringfield', "foo")
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (2 3)'))
    lyr.SetFeature(f)
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetField('intfield') != 1 or f.GetField('realfield') != 2.34 or f.GetField('stringfield') != 'foo' or \
       f.GetGeometryRef() is None:
        f.DumpReadable()
        pytest.fail()

    lyr.DeleteFeature(f.GetFID())
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    # SetFeature() on a deleted feature
    lyr.SetFeature(f)

    f = lyr.GetFeature(1)
    if f.GetField('intfield') != 1 or f.GetField('realfield') != 2.34 or f.GetField('stringfield') != 'foo' or \
       f.GetGeometryRef() is None:
        f.DumpReadable()
        pytest.fail()
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)

    f = lyr.GetFeature(1)
    # SetFeature() with identical feature : no-op
    assert lyr.SetFeature(f) == 0
    ds = None

    stat = gdal.VSIStatL(filename[0:-3] + "map")
    old_size = stat.size

    # This used to trigger a bug: when using SetFeature() repeatedly, we
    # can create object blocks in the .map that are made only of deleted
    # objects.
    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)

    f = lyr.GetFeature(1)
    for _ in range(100):
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (2 3)'))
        assert lyr.SetFeature(f) == 0
    ds = None

    stat = gdal.VSIStatL(filename[0:-3] + "map")
    assert stat.size == old_size

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)

    f = lyr.GetFeature(1)
    # SetFeature() with identical geometry : rewrite only attributes
    f.SetField('intfield', -1)
    assert lyr.SetFeature(f) == 0

    f = lyr.GetFeature(1)
    if f.GetField('intfield') != -1 or f.GetField('realfield') != 2.34 or f.GetField('stringfield') != 'foo' or \
       f.GetGeometryRef() is None:
        f.DumpReadable()
        pytest.fail()

    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)

###############################################################################


def generate_permutation(n):
    tab = [i for i in range(n)]
    for _ in range(10 * n):
        ind = random.randint(0, n - 1)
        tmp = tab[0]
        tab[0] = tab[ind]
        tab[ind] = tmp
    return tab

###############################################################################
# Test updating object blocks with deleted objects


def test_ogr_mitab_28():

    filename = '/vsimem/ogr_mitab_28.tab'

    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    # Generate 10x10 grid
    N2 = 10
    N = N2 * N2
    for n in generate_permutation(N):
        x = int(n / N2)
        y = n % N2
        f = ogr.Feature(lyr.GetLayerDefn())
        # f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(%d %d)' % (x,y)))
        f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(%d %d,%f %f,%f %f)' % (x, y, x + 0.1, y, x + 0.2, y)))
        lyr.CreateFeature(f)

    # Delete all features
    for i in range(N):
        lyr.DeleteFeature(i + 1)

    # Set deleted features
    i = 0
    permutation = generate_permutation(N)
    for n in permutation:
        x = int(n / N2)
        y = n % N2
        f = ogr.Feature(lyr.GetLayerDefn())
        # f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(%d %d)' % (x,y)))
        f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(%d %d,%f %f,%f %f)' % (x, y, x + 0.1, y, x + 0.2, y)))
        f.SetFID(i + 1)
        i = i + 1
        lyr.SetFeature(f)

    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    i = 0
    # Check sequential enumeration
    for f in lyr:
        g = f.GetGeometryRef()
        (x, y, _) = g.GetPoint(0)
        n = permutation[i]
        x_ref = int(n / N2)
        y_ref = n % N2
        assert abs(x - x_ref) + abs(y - y_ref) <= 0.1
        i = i + 1

    # Check spatial index integrity
    for n in range(N):
        x = int(n / N2)
        y = n % N2
        lyr.SetSpatialFilterRect(x - 0.5, y - 0.5, x + 0.5, y + 0.5)
        assert lyr.GetFeatureCount() == 1

    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)


###############################################################################
# Test updating a file with compressed geometries.

def test_ogr_mitab_29():
    try:
        os.stat('tmp/cache/compr_symb_deleted_records.tab')
    except OSError:
        try:
            gdaltest.unzip('tmp/cache', 'data/mitab/compr_symb_deleted_records.zip')
            try:
                os.stat('tmp/cache/compr_symb_deleted_records.tab')
            except OSError:
                pytest.skip()
        except OSError:
            pytest.skip()

    shutil.copy('tmp/cache/compr_symb_deleted_records.tab', 'tmp')
    shutil.copy('tmp/cache/compr_symb_deleted_records.dat', 'tmp')
    shutil.copy('tmp/cache/compr_symb_deleted_records.id', 'tmp')
    shutil.copy('tmp/cache/compr_symb_deleted_records.map', 'tmp')

    # Is a 100x100 point grid with only the 4 edge lines left (compressed points)
    ds = ogr.Open('tmp/compr_symb_deleted_records.tab', update=1)
    lyr = ds.GetLayer(0)
    # Re-add the 98x98 interior points
    N2 = 98
    N = N2 * N2
    permutation = generate_permutation(N)
    for n in permutation:
        x = 1 + int(n / N2)
        y = 1 + n % N2
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(%d %d)' % (x, y)))
        lyr.CreateFeature(f)
    ds = None

    # Check grid integrity that after reopening
    ds = ogr.Open('tmp/compr_symb_deleted_records.tab')
    lyr = ds.GetLayer(0)
    N2 = 100
    N = N2 * N2
    for n in range(N):
        x = int(n / N2)
        y = n % N2
        lyr.SetSpatialFilterRect(x - 0.01, y - 0.01, x + 0.01, y + 0.01)
        if lyr.GetFeatureCount() != 1:
            print(n)
            pytest.fail(x - 0.01, y - 0.01, x + 0.01, y + 0.01)
    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/compr_symb_deleted_records.tab')

###############################################################################
# Test SyncToDisk() in create mode


def test_ogr_mitab_30(update=0):

    filename = 'tmp/ogr_mitab_30.tab'

    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
    lyr = ds.CreateLayer('test', options=['BOUNDS=0,0,100,100'])
    lyr.CreateField(ogr.FieldDefn('ID', ogr.OFTInteger))
    assert lyr.SyncToDisk() == 0

    ds2 = ogr.Open(filename)
    lyr2 = ds2.GetLayer(0)
    assert lyr2.GetFeatureCount() == 0 and lyr2.GetLayerDefn().GetFieldCount() == 1
    ds2 = None

    # Check that the files are not updated in between
    if sys.platform.startswith('linux'):
        for ext in ('map', 'tab', 'dat', 'id'):
            os.system('touch -d "1 minute ago" %s' % filename[0:-3] + ext)

    stat = {}
    for ext in ('map', 'tab', 'dat', 'id'):
        stat[ext] = gdal.VSIStatL(filename[0:-3] + ext)

    if not sys.platform.startswith('linux'):
        time.sleep(1)

    assert lyr.SyncToDisk() == 0
    for ext in ('map', 'tab', 'dat', 'id'):
        stat2 = gdal.VSIStatL(filename[0:-3] + ext)
        assert stat[ext].size == stat2.size and stat[ext].mtime == stat2.mtime

    if update == 1:
        ds = None
        ds = ogr.Open(filename, update=1)
        lyr = ds.GetLayer(0)

    for j in range(100):
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField('ID', j + 1)
        feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (%d %d)' % (j, j)))
        lyr.CreateFeature(feat)
        feat = None

        if not (j <= 10 or (j % 5) == 0):
            continue

        for i in range(2):
            ret = lyr.SyncToDisk()
            assert ret == 0

            if i == 0:
                for ext in ('map', 'tab', 'dat', 'id'):
                    stat[ext] = gdal.VSIStatL(filename[0:-3] + ext)
            else:
                for ext in ('map', 'tab', 'dat', 'id'):
                    stat2 = gdal.VSIStatL(filename[0:-3] + ext)
                    if stat[ext].size != stat2.size:
                        print(j)
                        pytest.fail(i)

            ds2 = ogr.Open(filename)
            lyr2 = ds2.GetLayer(0)
            assert lyr2.GetFeatureCount() == j + 1, i
            feat2 = lyr2.GetFeature(j + 1)
            if feat2.GetField('ID') != j + 1 or feat2.GetGeometryRef().ExportToWkt() != 'POINT (%d %d)' % (j, j):
                print(i)
                feat2.DumpReadable()
                pytest.fail(j)
            lyr2.ResetReading()
            for _ in range(j + 1):
                feat2 = lyr2.GetNextFeature()
            if feat2.GetField('ID') != j + 1 or feat2.GetGeometryRef().ExportToWkt() != 'POINT (%d %d)' % (j, j):
                print(i)
                feat2.DumpReadable()
                pytest.fail(j)
            ds2 = None

    ds = None
    ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)

###############################################################################
# Test SyncToDisk() in update mode


def test_ogr_mitab_31():
    return test_ogr_mitab_30(update=1)

###############################################################################
# Check read support of non-spatial .tab/.data without .map or .id (#5718)
# We only check read-only behaviour though.


def test_ogr_mitab_32():

    for update in (0, 1):
        ds = ogr.Open('data/mitab/aspatial-table.tab', update=update)
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2, update
        f = lyr.GetNextFeature()
        assert f.GetField('a') == 1 and f.GetField('b') == 2 and f.GetField('d') == 'hello', \
            update
        f = lyr.GetFeature(2)
        assert f.GetField('a') == 4, update
    ds = None

###############################################################################
# Test opening and modifying a file created with MapInfo that consists of
# a single object block, without index block


def test_ogr_mitab_33():

    for update in (0, 1):
        ds = ogr.Open('data/mitab/single_point_mapinfo.tab', update=update)
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1, update
        f = lyr.GetNextFeature()
        assert f.GetField('toto') == '', update
    ds = None

    # Test adding a new object
    shutil.copy('data/mitab/single_point_mapinfo.tab', 'tmp')
    shutil.copy('data/mitab/single_point_mapinfo.dat', 'tmp')
    shutil.copy('data/mitab/single_point_mapinfo.id', 'tmp')
    shutil.copy('data/mitab/single_point_mapinfo.map', 'tmp')

    ds = ogr.Open('tmp/single_point_mapinfo.tab', update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(1363180 7509810)'))
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open('tmp/single_point_mapinfo.tab')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2
    f = lyr.GetNextFeature()
    assert f is not None
    f = lyr.GetNextFeature()
    assert f is not None
    ds = None

    # Test replacing the existing object
    shutil.copy('data/mitab/single_point_mapinfo.tab', 'tmp')
    shutil.copy('data/mitab/single_point_mapinfo.dat', 'tmp')
    shutil.copy('data/mitab/single_point_mapinfo.id', 'tmp')
    shutil.copy('data/mitab/single_point_mapinfo.map', 'tmp')

    ds = ogr.Open('tmp/single_point_mapinfo.tab', update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1)
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(1363180 7509810)'))
    lyr.SetFeature(f)
    f = None
    ds = None

    ds = ogr.Open('tmp/single_point_mapinfo.tab')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    f = lyr.GetNextFeature()
    assert f is not None
    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/single_point_mapinfo.tab')

###############################################################################
# Test updating a line that spans over several coordinate blocks


def test_ogr_mitab_34():

    filename = '/vsimem/ogr_mitab_34.tab'
    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
    lyr = ds.CreateLayer('ogr_mitab_34', options=['BOUNDS=-1000,0,1000,3000'])
    lyr.CreateField(ogr.FieldDefn('dummy', ogr.OFTString))
    geom = ogr.Geometry(ogr.wkbLineString)
    for i in range(1000):
        geom.AddPoint_2D(i, i)
    for _ in range(2):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(geom)
        lyr.CreateFeature(f)
        f = None
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    lyr.GetNextFeature()  # seek to another object
    geom = f.GetGeometryRef()
    geom.SetPoint_2D(0, -1000, 3000)
    lyr.SetFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    geom = f.GetGeometryRef()
    assert geom.GetX(0) == pytest.approx(-1000, abs=1e-2) and geom.GetY(0) == pytest.approx(3000, abs=1e-2)
    for i in range(999):
        assert geom.GetX(i + 1) == pytest.approx((i + 1), abs=1e-2) and geom.GetY(i + 1) == pytest.approx((i + 1), abs=1e-2)
    f = lyr.GetNextFeature()
    geom = f.GetGeometryRef()
    for i in range(1000):
        assert geom.GetX(i) == pytest.approx((i), abs=1e-2) and geom.GetY(i) == pytest.approx((i), abs=1e-2)
    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)

###############################################################################
# Test SRS support


def get_srs_from_coordsys(coordsys):
    mif_filename = '/vsimem/foo.mif'
    f = gdal.VSIFOpenL(mif_filename, "wb")
    content = """Version 300
Charset "Neutral"
Delimiter ","
%s
Columns 1
  foo Char(254)
Data

NONE
""" % coordsys
    content = content.encode('ascii')
    gdal.VSIFWriteL(content, 1, len(content), f)
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL(mif_filename[0:-3] + "mid", "wb")
    content = '""\n'
    content = content.encode('ascii')
    gdal.VSIFWriteL(content, 1, len(content), f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open(mif_filename)
    srs = ds.GetLayer(0).GetSpatialRef()
    if srs is not None:
        srs = srs.Clone()

    gdal.Unlink(mif_filename)
    gdal.Unlink(mif_filename[0:-3] + "mid")

    return srs


def get_coordsys_from_srs(srs):
    mif_filename = '/vsimem/foo.mif'
    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(mif_filename)
    lyr = ds.CreateLayer('foo', srs=srs)
    lyr.CreateField(ogr.FieldDefn('foo'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    ds = None
    f = gdal.VSIFOpenL(mif_filename, "rb")
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)
    gdal.Unlink(mif_filename)
    gdal.Unlink(mif_filename[0:-3] + "mid")
    data = data[data.find('CoordSys'):]
    data = data[0:data.find('\n')]
    return data


def test_ogr_mitab_35():

    # Local/non-earth
    srs = osr.SpatialReference()
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys NonEarth Units "m"'

    srs = osr.SpatialReference('LOCAL_CS["foo"]')
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys NonEarth Units "m"'
    srs = get_srs_from_coordsys(coordsys)
    wkt = srs.ExportToWkt()
    assert wkt in ('LOCAL_CS["Nonearth",UNIT["Meter",1]]', 'LOCAL_CS["Nonearth",UNIT["Meter",1],AXIS["Easting",EAST],AXIS["Northing",NORTH]]')

    # Test units
    for mif_unit in ['mi', 'km', 'in', 'ft', 'yd', 'mm', 'cm', 'm', 'survey ft', 'nmi', 'li', 'ch', 'rd']:
        coordsys = 'CoordSys NonEarth Units "%s"' % mif_unit
        srs = get_srs_from_coordsys(coordsys)
        # print(srs)
        got_coordsys = get_coordsys_from_srs(srs)
        assert coordsys == got_coordsys, srs

    # Geographic
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys Earth Projection 1, 104'
    srs = get_srs_from_coordsys(coordsys)
    wkt = srs.ExportToWkt()
    assert wkt == 'GEOGCS["unnamed",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST]]'
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys Earth Projection 1, 104'

    # Projected
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys Earth Projection 8, 104, "m", 3, 0, 0.9996, 500000, 0'
    srs = get_srs_from_coordsys(coordsys)
    wkt = srs.ExportToWkt()
    assert wkt == 'PROJCS["unnamed",GEOGCS["unnamed",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]'
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys Earth Projection 8, 104, "m", 3, 0, 0.9996, 500000, 0'

    # Test round-tripping of projection methods and a few units
    for coordsys in ['CoordSys Earth Projection 1, 104',
                     'CoordSys Earth Projection 2, 104, "survey ft", 1, 2',
                     'CoordSys Earth Projection 3, 104, "ft", 1, 2, 3, 4, 5, 6',
                     'CoordSys Earth Projection 4, 104, "m", 1, 90, 90',
                     'CoordSys Earth Projection 5, 104, "m", 1, 90, 90',
                     'CoordSys Earth Projection 6, 104, "m", 1, 2, 3, 4, 5, 6',
                     'CoordSys Earth Projection 7, 104, "m", 1, 2, 3, 4, 5, 6',
                     'CoordSys Earth Projection 8, 104, "m", 1, 2, 3, 4, 5',
                     'CoordSys Earth Projection 9, 104, "m", 1, 2, 3, 4, 5, 6',
                     'CoordSys Earth Projection 10, 104, "m", 1',
                     'CoordSys Earth Projection 11, 104, "m", 1',
                     'CoordSys Earth Projection 12, 104, "m", 1',
                     'CoordSys Earth Projection 13, 104, "m", 1',
                     'CoordSys Earth Projection 14, 104, "m", 1',
                     'CoordSys Earth Projection 15, 104, "m", 1',
                     'CoordSys Earth Projection 16, 104, "m", 1',
                     'CoordSys Earth Projection 17, 104, "m", 1',
                     'CoordSys Earth Projection 18, 104, "m", 1, 2, 3, 4',
                     'CoordSys Earth Projection 19, 104, "m", 1, 2, 3, 4, 5, 6',
                     'CoordSys Earth Projection 20, 104, "m", 1, 2, 3, 4, 5',
                     #'CoordSys Earth Projection 21, 104, "m", 1, 2, 3, 4, 5',
                     #'CoordSys Earth Projection 22, 104, "m", 1, 2, 3, 4, 5',
                     #'CoordSys Earth Projection 23, 104, "m", 1, 2, 3, 4, 5',
                     #'CoordSys Earth Projection 24, 104, "m", 1, 2, 3, 4, 5',
                     'CoordSys Earth Projection 25, 104, "m", 1, 2, 3, 4',
                     'CoordSys Earth Projection 26, 104, "m", 1, 2',
                     'CoordSys Earth Projection 27, 104, "m", 1, 2, 3, 4',
                     'CoordSys Earth Projection 28, 104, "m", 1, 2, 90',
                     # 'CoordSys Earth Projection 29, 104, "m", 1, 90, 90', # alias of 4
                     'CoordSys Earth Projection 30, 104, "m", 1, 2, 3, 4',
                     'CoordSys Earth Projection 31, 104, "m", 1, 2, 3, 4, 5',
                     'CoordSys Earth Projection 32, 104, "m", 1, 2, 3, 4, 5, 6',
                     'CoordSys Earth Projection 33, 104, "m", 1, 2, 3, 4',
                     ]:
        srs = get_srs_from_coordsys(coordsys)
        # print(srs)
        got_coordsys = get_coordsys_from_srs(srs)
        # if got_coordsys.find(' Bounds') >= 0:
        #    got_coordsys = got_coordsys[0:got_coordsys.find(' Bounds')]
        assert coordsys == got_coordsys, srs

    # Test TOWGS84
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4322)
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys Earth Projection 1, 103'
    srs = get_srs_from_coordsys(coordsys)
    wkt = srs.ExportToWkt()
    assert wkt in ('GEOGCS["unnamed",DATUM["WGS_1972",SPHEROID["WGS 72",6378135,298.26]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST]]', 'GEOGCS["unnamed",DATUM["World_Geodetic_System_1972",SPHEROID["WGS 72",6378135,298.26]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST]]')
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys Earth Projection 1, 103'

    # Test Lambert 93
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(2154)
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys Earth Projection 3, 33, "m", 3, 46.5, 44, 49, 700000, 6600000'
    srs = get_srs_from_coordsys(coordsys)
    assert srs.GetAuthorityCode(None) == '2154'
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys Earth Projection 3, 33, "m", 3, 46.5, 44, 49, 700000, 6600000'

    srs = osr.SpatialReference('PROJCS["RGF93 / Lambert-93",GEOGCS["RGF93",DATUM["Reseau_Geodesique_Francais_1993",SPHEROID["GRS 80",6378137,298.257222101]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",49.00000000002],PARAMETER["standard_parallel_2",44],PARAMETER["latitude_of_origin",46.5],PARAMETER["central_meridian",3],PARAMETER["false_easting",700000],PARAMETER["false_northing",6600000],UNIT["Meter",1.0],AUTHORITY["EPSG","2154"]]')
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys Earth Projection 3, 33, "m", 3, 46.5, 44, 49.00000000002, 700000, 6600000'
    gdal.SetConfigOption('MITAB_BOUNDS_FILE', 'data/mitab/mitab_bounds.txt')
    coordsys = get_coordsys_from_srs(srs)
    gdal.SetConfigOption('MITAB_BOUNDS_FILE', None)
    assert coordsys == 'CoordSys Earth Projection 3, 33, "m", 3, 46.5, 44, 49.00000000002, 700000, 6600000 Bounds (75000, 6000000) (1275000, 7200000)'

    # http://trac.osgeo.org/gdal/ticket/4115
    srs = get_srs_from_coordsys('CoordSys Earth Projection 10, 157, "m", 0')
    wkt = srs.ExportToWkt()
    assert wkt == 'PROJCS["WGS 84 / Pseudo-Mercator",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Mercator_1SP"],PARAMETER["central_meridian",0],PARAMETER["scale_factor",1],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],EXTENSION["PROJ4","+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +nadgrids=@null +wktext +no_defs"]]'
    # We don't round-trip currently

    # MIF 999
    srs = osr.SpatialReference("""GEOGCS["unnamed",
        DATUM["MIF 999,1,1,2,3",
            SPHEROID["WGS 72",6378135,298.26]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]]""")
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys Earth Projection 1, 999, 1, 1, 2, 3'
    srs = get_srs_from_coordsys(coordsys)
    wkt = srs.ExportToWkt()
    assert wkt == 'GEOGCS["unnamed",DATUM["MIF 999,1,1,2,3",SPHEROID["WGS 72",6378135,298.26],TOWGS84[1,2,3,0,0,0,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST]]'

    # MIF 9999
    srs = osr.SpatialReference("""GEOGCS["unnamed",
        DATUM["MIF 9999,1,1,2,3,4,5,6,7,3",
            SPHEROID["WGS 72",6378135,298.26]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]]""")
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys Earth Projection 1, 9999, 1, 1, 2, 3, 4, 5, 6, 7, 3'
    srs = get_srs_from_coordsys(coordsys)
    wkt = srs.ExportToWkt()
    assert wkt == 'GEOGCS["unnamed",DATUM["MIF 9999,1,1,2,3,4,5,6,7,3",SPHEROID["WGS 72",6378135,298.26],TOWGS84[1,2,3,-4,-5,-6,7]],PRIMEM["non-Greenwich",3],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST]]'

    # Test EPSG:2393 / KKJ
    srs = osr.SpatialReference("""PROJCS["KKJ / Finland Uniform Coordinate System",GEOGCS["KKJ",DATUM["Kartastokoordinaattijarjestelma_1966",SPHEROID["International 1924",6378388,297,AUTHORITY["EPSG","7022"]],AUTHORITY["EPSG","6123"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4123"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",27],PARAMETER["scale_factor",1],PARAMETER["false_easting",3500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Northing",NORTH],AXIS["Easting",EAST],AUTHORITY["EPSG","2393"]]""")
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys Earth Projection 24, 1016, "m", 27, 0, 1, 3500000, 0'
    srs = get_srs_from_coordsys(coordsys)
    wkt = srs.ExportToWkt()
    assert wkt == 'PROJCS["unnamed",GEOGCS["unnamed",DATUM["Kartastokoordinaattijarjestelma_1966",SPHEROID["International 1924",6378388,297]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",27],PARAMETER["scale_factor",1],PARAMETER["false_easting",3500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]'
    coordsys = get_coordsys_from_srs(srs)
    assert coordsys == 'CoordSys Earth Projection 24, 1016, "m", 27, 0, 1, 3500000, 0'

###############################################################################
# Test opening and modifying a file with polygons created with MapInfo that consists of
# a single object block, without index block


def test_ogr_mitab_36():

    # Test modifying a new object
    shutil.copy('data/mitab/polygon_without_index.tab', 'tmp')
    shutil.copy('data/mitab/polygon_without_index.dat', 'tmp')
    shutil.copy('data/mitab/polygon_without_index.id', 'tmp')
    shutil.copy('data/mitab/polygon_without_index.map', 'tmp')

    ds = ogr.Open('tmp/polygon_without_index.tab', update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    ring = g.GetGeometryRef(0)
    ring.SetPoint_2D(1, ring.GetX(1) + 100, ring.GetY())
    g = g.Clone()
    f.SetGeometry(g)
    lyr.SetFeature(f)
    f = None
    ds = None

    ds = ogr.Open('tmp/polygon_without_index.tab')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    got_g = f.GetGeometryRef()
    if ogrtest.check_feature_geometry(f, got_g, max_error=0.1):
        f.DumpReadable()
        pytest.fail(g)
    while True:
        f = lyr.GetNextFeature()
        if f is None:
            break
    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/polygon_without_index.tab')

###############################################################################
# Simple testing of Seamless tables


def test_ogr_mitab_37():

    ds = ogr.Open('data/mitab/seamless.tab')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 4

    f = lyr.GetNextFeature()
    assert f.GetFID() == 4294967297 and f.id == '1'

    f = lyr.GetNextFeature()
    assert f.GetFID() == 4294967298 and f.id == '2'

    f = lyr.GetNextFeature()
    assert f.GetFID() == 8589934593 and f.id == '3'

    f = lyr.GetNextFeature()
    assert f.GetFID() == 8589934594 and f.id == '4'

    f = lyr.GetFeature(4294967297)
    assert f.GetFID() == 4294967297 and f.id == '1'

    f = lyr.GetFeature(8589934594)
    assert f.GetFID() == 8589934594 and f.id == '4'

    f = lyr.GetFeature(8589934594 + 1)
    assert f is None

    f = lyr.GetFeature(4294967297 * 2 + 1)
    assert f is None

###############################################################################
# Open MIF with MID with TAB delimiter and empty first field (#5405)


def test_ogr_mitab_38():

    ds = ogr.Open('data/mitab/empty_first_field_with_tab_delimiter.mif')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['field1'] != '' or f['field2'] != 'foo':
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Read various geometry types from .mif


def test_ogr_mitab_39():

    ds = ogr.Open('data/mitab/all_geoms.mif')
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open('data/mitab/all_geoms.mif.golden.csv')
    lyr_ref = ds_ref.GetLayer(0)

    while True:
        f = lyr.GetNextFeature()
        f_ref = lyr_ref.GetNextFeature()
        if f is None:
            assert f_ref is None
            break
        if ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef()) != 0 or \
           f.GetStyleString() != f_ref.GetStyleString():
            f.DumpReadable()
            f_ref.DumpReadable()
            pytest.fail()


###############################################################################
# Read various geometry types from .mif but potentially truncated


def test_ogr_mitab_40():

    content = open('data/mitab/all_geoms.mif', 'rt').read()

    for i in range(len(content)):
        gdal.FileFromMemBuffer('/vsimem/ogr_mitab_40.mif', content[0:i])
        with gdaltest.error_handler():
            ds = ogr.Open('/vsimem/ogr_mitab_40.mif')
            if ds is not None:
                lyr = ds.GetLayer(0)
                for _ in lyr:
                    pass

    gdal.Unlink('/vsimem/ogr_mitab_40.mif')

###############################################################################
# Read various geometry types from .tab


def test_ogr_mitab_41():

    ds = ogr.Open('data/mitab/all_geoms.tab')
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open('data/mitab/all_geoms.mif.golden.csv')
    lyr_ref = ds_ref.GetLayer(0)

    while True:
        f = lyr.GetNextFeature()
        f_ref = lyr_ref.GetNextFeature()
        if f is None:
            assert f_ref is None
            break
        if ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef()) != 0 or \
           f.GetStyleString() != f_ref.GetStyleString():
            f.DumpReadable()
            f_ref.DumpReadable()
            pytest.fail()


###############################################################################
# Read various geometry types from .tab with block size = 32256


def test_ogr_mitab_42():

    ds = ogr.Open('/vsizip/data/mitab/all_geoms_block_32256.zip')
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open('data/mitab/all_geoms.mif.golden.csv')
    lyr_ref = ds_ref.GetLayer(0)

    while True:
        f = lyr.GetNextFeature()
        f_ref = lyr_ref.GetNextFeature()
        if f is None:
            assert f_ref is None
            break
        if ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef()) != 0 or \
           f.GetStyleString() != f_ref.GetStyleString():
            f.DumpReadable()
            f_ref.DumpReadable()
            pytest.fail()


###############################################################################
# Test creating tab with block size = 32256


def test_ogr_mitab_43():

    src_ds = gdal.OpenEx('/vsizip/data/mitab/all_geoms_block_32256.zip')
    gdal.VectorTranslate('/vsimem/all_geoms_block_512.tab', src_ds, format='MapInfo File')
    gdal.VectorTranslate('/vsimem/all_geoms_block_32256.tab', src_ds, format='MapInfo File', datasetCreationOptions=['BLOCKSIZE=32256'])
    with gdaltest.error_handler():
        out_ds = gdal.VectorTranslate('/vsimem/all_geoms_block_invalid.tab', src_ds, format='MapInfo File', datasetCreationOptions=['BLOCKSIZE=32768'])
    assert out_ds is None
    gdal.Unlink('/vsimem/all_geoms_block_invalid.dat')
    src_ds = None

    size = gdal.VSIStatL('/vsimem/all_geoms_block_512.map').size
    assert size == 6656

    size = gdal.VSIStatL('/vsimem/all_geoms_block_32256.map').size
    assert size == 161280

    ds = ogr.Open('/vsimem/all_geoms_block_32256.tab')
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open('/vsimem/all_geoms_block_512.tab')
    lyr_ref = ds_ref.GetLayer(0)

    while True:
        f = lyr.GetNextFeature()
        f_ref = lyr_ref.GetNextFeature()
        if f is None:
            assert f_ref is None
            break
        if ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef()) != 0 or \
           f.GetStyleString() != f_ref.GetStyleString():
            f.DumpReadable()
            f_ref.DumpReadable()
            pytest.fail()

    gdaltest.mapinfo_drv.DeleteDataSource('/vsimem/all_geoms_block_512.tab')
    gdaltest.mapinfo_drv.DeleteDataSource('/vsimem/all_geoms_block_32256.tab')
    gdal.Unlink('/vsimem/all_geoms_block_32768.dat')

###############################################################################
# Test limitation on width and precision of numeric fields in creation (#6392)


def test_ogr_mitab_44():

    ds = gdaltest.mapinfo_drv.CreateDataSource('/vsimem/ogr_mitab_44.mif')
    lyr = ds.CreateLayer('test')
    fld_defn = ogr.FieldDefn('test', ogr.OFTReal)
    fld_defn.SetWidth(30)
    fld_defn.SetPrecision(29)
    lyr.CreateField(fld_defn)
    ds = None

    ds = ogr.Open('/vsimem/ogr_mitab_44.mif')
    lyr = ds.GetLayer(0)
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fld_defn.GetWidth() == 20 and fld_defn.GetPrecision() == 16
    ds = None

    gdaltest.mapinfo_drv.DeleteDataSource('/vsimem/ogr_mitab_44.mif')

###############################################################################
# Test read/write MapInfo layers with encoding specified


def test_ogr_mitab_45():

    lyrNames = ['lyr1', 'lyr2']
    #                     0         1         2         3
    #                     012345678901234567890123456789012
    fldNames = ['field1', 'абвгдежзийклмнопрстуфхцчшщьъэюя']
    featNames = ['аз',
                 'буки',
                 'веди']
    formats = ['MIF', 'TAB', 'MIF', 'TAB']
    lyrNums = [1, 1, 2, 2]
    dsExts = ['.mif', '.tab', '', '']

    for formatN, frmt in enumerate(formats):
        lyrCount = lyrNums[formatN]
        ext = dsExts[formatN]
        dsName = '/vsimem/45/ogr_mitab_45_%s_%s%s' % (frmt, lyrCount, ext)

        ds = gdaltest.mapinfo_drv.CreateDataSource(dsName, options=['FORMAT=' + frmt])

        assert ds is not None, ('Can\'t create dataset: ' + dsName)

        for i in range(lyrCount):
            lyr = ds.CreateLayer(lyrNames[i], options=['ENCODING=CP1251'])
            assert lyr is not None, ('Can\'t create layer ' + lyrNames[i] +
                                     ' for ' + dsName)

            if lyr.TestCapability(ogr.OLCStringsAsUTF8) != 1:
                pytest.skip('skipping test: recode is not possible')

            for fldName in fldNames:
                fld_defn = ogr.FieldDefn(fldName, ogr.OFTString)
                fld_defn.SetWidth(254)
                lyr.CreateField(fld_defn)

            for featName in featNames:
                feat = ogr.Feature(lyr.GetLayerDefn())
                feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (25 72)"))
                for fldName in fldNames:
                    featValue = fldName + ' ' + featName
                    feat.SetField(fldName, featValue)
                lyr.CreateFeature(feat)
        ds = None

        # reopen and check
        ds = ogr.Open(dsName)
        assert ds is not None, ('Can\'t reopen dataset: ' + dsName)

        for i in range(lyrCount):
            lyr = ds.GetLayer(i)
            assert lyr is not None, ('Can\'t get layer ' + lyrNames[i] +
                                     ' from ' + dsName)

            for fldN, expectedName in enumerate(fldNames):
                fldName = lyr.GetLayerDefn().GetFieldDefn(fldN).GetName()
                assert fldName == expectedName, ('Can\'t get field name\n' +
                                         ' result name:   "' + fldName + '"\n'
                                         ' expected name: "' + expectedName + '"\n'
                                         ' from layer : ' + lyrNames[i] +
                                         ' from dataset :' + dsName)

            for featName in featNames:
                feat = lyr.GetNextFeature()
                for fldN, fldName in enumerate(fldNames):
                    expectedValue = fldName + ' ' + featName
                    # column value by number
                    value = feat.GetField(fldN)
                    assert value == expectedValue, \
                        ('Can\'t get field value by number\n' +
                                             ' result value:   "' + value + '"\n'
                                             ' expected value: "' + expectedValue + '"\n'
                                             ' from layer : ' + lyrNames[i] +
                                             ' from dataset :' + dsName)
                    # column value by name
                    value = feat.GetField(fldNames[fldN])
                    assert value == expectedValue, \
                        ('Can\'t get field value by name\n' +
                                             ' result value:   "' + value + '"\n'
                                             ' expected value: "' + expectedValue + '"\n'
                                             ' from layer : ' + lyrNames[i] +
                                             ' from dataset :' + dsName)

        gdaltest.mapinfo_drv.DeleteDataSource(dsName)


###############################################################################
# Test read MapInfo layers with encoding specified


def test_ogr_mitab_46():

    dsNames = ['data/mitab/tab-win1251.TAB',
               'data/mitab/win1251.mif']
    fldNames = ['Поле_А', 'Поле_Б', 'Поле_В', 'Поле_Г', 'Поле_Д']
    fldVal = [['Значение А', 'Значение Б', 'Значение В', 'Значение Г', 'Значение Д'],
              ['Значение 1', 'Значение 2', 'Значение 3', 'Значение 4', 'Значение 5'],
              ['Полигон', 'Синий', 'Заливка', 'А а Б б', 'ЪЫЁЩ']]

    for dsName in dsNames:

        ds = ogr.Open(dsName)
        assert ds is not None, ('Can\'t open dataset: ' + dsName)

        lyr = ds.GetLayer(0)
        assert lyr is not None, ('Can\'t get layer 0 from ' + dsName)

        if lyr.TestCapability(ogr.OLCStringsAsUTF8) != 1:
            pytest.skip('skipping test: recode is not possible')

        for fldN, expectedName in enumerate(fldNames):
            fldName = lyr.GetLayerDefn().GetFieldDefn(fldN).GetName()
            assert fldName == expectedName, ('Can\'t get field\n' +
                                     ' result name:   "' + fldName + '"\n'
                                     ' expected name: "' + expectedName + '"\n'
                                     ' from dataset :' + dsName)

        for featFldVal in fldVal:
            feat = lyr.GetNextFeature()
            for fldN, fldName in enumerate(fldNames):
                expectedValue = featFldVal[fldN]
                # column value by number
                value = feat.GetField(fldN)
                assert value == expectedValue, ('Can\'t get field value by number\n' +
                                         ' result value:   "' + value + '"\n'
                                         ' expected value: "' + expectedValue + '"\n'
                                         ' from dataset :' + dsName)
                # column value by name
                value = feat.GetField(fldName)
                assert value == expectedValue, ('Can\'t get field value by name\n' +
                                         ' result value:   "' + value + '"\n'
                                         ' expected value: "' + expectedValue + '"\n'
                                         ' from dataset :' + dsName)


###############################################################################
# Test opening a dataset with a .ind file


def test_ogr_mitab_47():

    ds = ogr.Open('data/mitab/poly_indexed.tab')
    lyr = ds.GetLayer(0)
    lyr.SetAttributeFilter("PRFEDEA = '35043413'")
    assert lyr.GetFeatureCount() == 1

    for ext in ('tab', 'dat', 'map', 'id'):
        gdal.FileFromMemBuffer('/vsimem/poly_indexed.' + ext,
                               open('data/mitab/poly_indexed.' + ext, 'rb').read())
    ds = ogr.Open('/vsimem/poly_indexed.tab')
    lyr = ds.GetLayer(0)
    lyr.SetAttributeFilter("PRFEDEA = '35043413'")
    assert lyr.GetFeatureCount() == 1
    ds = None
    for ext in ('tab', 'dat', 'map', 'id'):
        gdal.Unlink('/vsimem/poly_indexed.' + ext)


###############################################################################
# Test writing and reading LCC_1SP


def test_ogr_mitab_48():

    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource('/vsimem/test.mif')
    sr = osr.SpatialReference()
    sr.SetFromUserInput("""PROJCS["NTF (Paris) / France IV (deprecated)",
    GEOGCS["NTF (Paris)",
        DATUM["Nouvelle_Triangulation_Francaise_Paris",
            SPHEROID["Clarke 1880 (IGN)",6378249.2,293.4660212936269,
                AUTHORITY["EPSG","7011"]],
            TOWGS84[-168,-60,320,0,0,0,0],
            AUTHORITY["EPSG","6807"]],
        PRIMEM["Paris",2.33722917,
            AUTHORITY["EPSG","8903"]],
        UNIT["grad",0.01570796326794897,
            AUTHORITY["EPSG","9105"]],
        AUTHORITY["EPSG","4807"]],
    PROJECTION["Lambert_Conformal_Conic_1SP"],
    PARAMETER["latitude_of_origin",46.85],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",0.99994471],
    PARAMETER["false_easting",234.358],
    PARAMETER["false_northing",4185861.369],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AXIS["X",EAST],
    AXIS["Y",NORTH],
    AUTHORITY["EPSG","27584"]]""")
    lyr = ds.CreateLayer('foo', srs=sr)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    ds = None

    ds = ogr.Open('/vsimem/test.mif')
    lyr = ds.GetLayer(0)
    sr_got = lyr.GetSpatialRef()
    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource('/vsimem/test.mif')
    sr_expected = osr.SpatialReference()
    sr_expected.SetFromUserInput("""PROJCS["unnamed",
    GEOGCS["unnamed",
        DATUM["NTF_Paris_Meridian",
            SPHEROID["Clarke 1880 (modified for IGN)",6378249.2,293.4660213],
            TOWGS84[-168,-60,320,0,0,0,0]],
        PRIMEM["Paris",2.33722917],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_1SP"],
    PARAMETER["latitude_of_origin",42.165],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",0.99994471],
    PARAMETER["false_easting",234.358],
    PARAMETER["false_northing",4185861.369],
    UNIT["metre",1]]""")

    assert sr_got.IsSame(sr_expected) != 0, sr_got.ExportToPrettyWkt()

###############################################################################
# Test reading an aspatial TAB file.


def test_ogr_mitab_49_aspatial():

    ds = ogr.GetDriverByName('MapInfo File').Open('data/mitab/aspatial.tab')
    lyr = ds.GetLayer(0)

    geom_type = lyr.GetLayerDefn().GetGeomType()
    assert geom_type == ogr.wkbNone

    assert lyr.GetSpatialRef() is None

    assert lyr.GetExtent(can_return_null=True) is None

###############################################################################
# Test creating an indexed field


def test_ogr_mitab_tab_field_index_creation():

    layername = 'ogr_mitab_tab_field_index_creation'
    filename = '/vsimem/' + layername + '.tab'
    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
    lyr = ds.CreateLayer(layername)
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('other_field', ogr.OFTInteger))
    with gdaltest.error_handler():
        ds.ExecuteSQL('CREATE INDEX ON foo USING id')
        ds.ExecuteSQL('CREATE INDEX ON ' + layername + ' USING foo')
    ds.ExecuteSQL('CREATE INDEX ON ' + layername + ' USING id')
    ds.ExecuteSQL('CREATE INDEX ON ' + layername + ' USING id')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 100)
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 200)
    lyr.CreateFeature(f)
    ds = None

    assert gdal.VSIStatL('/vsimem/' + layername + '.ind') is not None, 'no ind file'

    ds = ogr.Open(filename)
    with gdaltest.error_handler():
        ds.ExecuteSQL('CREATE INDEX ON ' + layername + ' USING other_field')
    lyr = ds.GetLayer(0)
    lyr.SetAttributeFilter('id = 200')
    assert lyr.GetFeatureCount() == 1, 'bad feature count'
    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)

###############################################################################
# Test reading a tab_view file


def test_ogr_mitab_tab_view():

    ds = ogr.Open('data/mitab/view_first_table_second_table.tab')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2, 'bad field count'
    f = lyr.GetNextFeature()
    if f['ID'] != 100 or f['foo'] != 'foo':
        f.DumpReadable()
        pytest.fail('bad feature')
    ds = None

    ds = ogr.Open('data/mitab/view_select_all_first_table_second_table.tab')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 3, 'bad field count'
    f = lyr.GetNextFeature()
    if f['joint_field'] != 1 or f['ID'] != 100 or f['foo'] != 'foo':
        f.DumpReadable()
        pytest.fail('bad feature')
    ds = None


###############################################################################


def test_ogr_mitab_style():

    tmpfile = '/vsimem/ogr_mitab_style.tab'
    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(tmpfile)
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,0 0))'))
    f.SetStyleString("BRUSH(fc:#AABBCC,bc:#DDEEFF);PEN(c:#DDEEFF)")
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,0 0))'))
    f.SetStyleString('BRUSH(fc:#AABBCC,id:"mapinfo-brush-1")')
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,0 0))'))
    f.SetStyleString('BRUSH(fc:#AABBCC00,bc:#ddeeff00)')
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(tmpfile)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetStyleString() != 'BRUSH(fc:#aabbcc,bc:#ddeeff,id:"mapinfo-brush-2,ogr-brush-0");PEN(w:1px,c:#ddeeff,id:"mapinfo-pen-2,ogr-pen-0",cap:r,j:r)':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetStyleString() != 'BRUSH(fc:#aabbcc,id:"mapinfo-brush-1,ogr-brush-1");PEN(w:1px,c:#000000,id:"mapinfo-pen-2,ogr-pen-0",cap:r,j:r)':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetStyleString() != 'BRUSH(fc:#aabbcc,id:"mapinfo-brush-1,ogr-brush-1");PEN(w:1px,c:#000000,id:"mapinfo-pen-2,ogr-pen-0",cap:r,j:r)':
        f.DumpReadable()
        pytest.fail()
    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource(tmpfile)

###############################################################################


def test_ogr_mitab_tab_write_field_name_with_dot():

    tmpfile = '/vsimem/ogr_mitab_tab_write_field_name_with_dot.tab'
    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(tmpfile)
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('with.dot', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['with.dot'] = 1
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(2 3)'))
    lyr.CreateFeature(f)
    with gdaltest.error_handler():
        ds = None

    ds = ogr.Open(tmpfile)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f['with_dot'] == 1
    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource(tmpfile)


###############################################################################
# Test read text labels with local encoding from mif/mid file


def test_ogr_mitab_local_encoding_label():

    dsNames = ['data/mitab/win1251_text.mif',
               'data/mitab/tab-win1251_text.tab']
    expectedStyles = ['LABEL(t:"Поле",a:0.000000,s:2.070000g,c:#ff0000,p:2,f:"DejaVu Serif")',
                      'LABEL(t:"Поле",a:0.000000,s:0.015375g,c:#000000,p:1,f:"Times New Roman")']
    for (dsName, expectedStyle) in zip(dsNames, expectedStyles):

        ds = ogr.Open(dsName)
        assert ds is not None, ('Can\'t open dataset: ' + dsName)

        lyr = ds.GetLayer(0)
        assert lyr is not None, ('Can\'t get layer 0 from ' + dsName)

        if lyr.TestCapability(ogr.OLCStringsAsUTF8) != 1:
            pytest.skip('skipping test: recode is not possible')

        feat = lyr.GetNextFeature()
        assert lyr is not None, ('Can\'t find text feature in' + dsName)

        assert feat.GetStyleString() == expectedStyle, (feat.GetStyleString(), expectedStyle)


###############################################################################
# Check fix for https://github.com/OSGeo/gdal/issues/1232

def test_ogr_mitab_delete_feature_no_geometry():

    filename = '/vsimem/test.tab'
    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 1
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 2
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    assert lyr.DeleteFeature(1) == 0
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f['id'] == 2
    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)


###############################################################################
# Check fix for https://github.com/OSGeo/gdal/issues/1636

def test_ogr_mitab_too_large_value_for_decimal_field():

    filename = '/vsimem/test.tab'
    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)
    fld = ogr.FieldDefn('f', ogr.OFTReal)
    fld.SetWidth(20)
    fld.SetPrecision(12)
    lyr.CreateField(fld)

    f = ogr.Feature(lyr.GetLayerDefn())
    f['f'] = 1234567.012
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f['f'] = 123456789.012
    with gdaltest.error_handler():
        assert lyr.CreateFeature(f) != ogr.OGRERR_NONE
    f = None

    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)


###############################################################################
# Check custom datum/spheroid parameters export

def test_ogr_mitab_custom_datum_export():

    sr = osr.SpatialReference()
    sr.SetGeogCS('Custom', 'Custom', 'Sphere', 6370997.0, 0.0)
    sr.SetTOWGS84(1, 2, 3, 4, 5, 6, 7)
    proj =  sr.ExportToMICoordSys()
    assert proj == 'Earth Projection 1, 9999, 12, 1, 2, 3, -4, -5, -6, -7, 0'

    sr = osr.SpatialReference()
    sr.SetGeogCS('Custom', 'Custom', 'NWL-9D or WGS-66', 6378145.0, 298.25)
    sr.SetTOWGS84(1, 2, 3, 4, 5, 6, 7)
    sr.SetUTM(33)
    proj =  sr.ExportToMICoordSys()
    assert proj == 'Earth Projection 8, 9999, 42, 1, 2, 3, -4, -5, -6, -7, 0, "m", 15, 0, 0.9996, 500000, 0'

###############################################################################
# Check write/read description

def test_ogr_mitab_description():
    filename = '/vsimem/test_description.tab'

    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
    assert ds is not None, ('Can\'t create dataset: ' + filename)

    test_description = 'Состав данных: Топокарты (растр) 1:50К, 100К, 250К, 500К, Топокарты (вектор) 1:100К, 1:250К, ЦМР 10м, Реестр географических названий 1:100000, АТД 1:10000, лидарная съемка, ортофото. Лицензия: на геоданные - ограничительная, не соответствующая определению "открытых данных", так как запрещено распространение данных.'

    lyr = ds.CreateLayer('test_description', options=['ENCODING=CP1251', 'DESCRIPTION={}'.format(test_description)])
    assert lyr is not None, ('Can\'t create layer "test_description"')
    if lyr.TestCapability(ogr.OLCStringsAsUTF8) != 1:
        pytest.skip('skipping test: recode is not possible')

    lyr.CreateField(ogr.FieldDefn('feature_id', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('other_field', ogr.OFTInteger))

    # Check description truncate.
    check_text = 'Состав данных: Топокарты (растр) 1:50К, 100К, 250К, 500К, Топокарты (вектор) 1:100К, 1:250К, ЦМР 10м, Реестр географических названий 1:100000, АТД 1:10000, лидарная съемка, ортофото. Лицензия: на геоданные - ограничительная, не соответствующая определению "открытых данных", так как запрещено распростр'
    assert check_text == lyr.GetMetadataItem('DESCRIPTION')
    ds = None

    # Check storing description in tab file.
    ds = ogr.Open(filename, update=1)
    assert ds is not None, ('Can\'t open dataset: ' + filename)
    lyr = ds.GetLayer(0)
    assert lyr is not None, ('Can\'t get layer 0 from ' + filename)
    assert check_text == lyr.GetMetadataItem('DESCRIPTION')

    # Check update description in tab file.
    check_short_text = 'Состав данных: Топокарты (растр) 1:50К, 100К, 250К, 500К'
    lyr.SetMetadataItem('DESCRIPTION', check_short_text)
    ds = None

    ds = ogr.Open(filename)
    assert ds is not None, ('Can\'t open dataset: ' + filename)
    lyr = ds.GetLayer(0)
    assert lyr is not None, ('Can\'t get layer 0 from ' + filename)
    assert check_short_text == lyr.GetMetadataItem('DESCRIPTION')
    ds = None

    # Check line breaks and double quotes
    test_description = 'Состав данных: "Топокарты (растр)"\n1:50К,\n100К,\n250К,\n500К\r\n"new line"'
    check_description = 'Состав данных: "Топокарты (растр)" 1:50К, 100К, 250К, 500К  "new line"'

    ds = ogr.Open(filename, update=1)
    assert ds is not None, ('Can\'t open dataset: ' + filename)
    lyr = ds.GetLayer(0)
    assert lyr is not None, ('Can\'t get layer 0 from ' + filename)
    lyr.SetMetadataItem('DESCRIPTION', test_description)
    ds = None

    ds = ogr.Open(filename)
    assert ds is not None, ('Can\'t open dataset: ' + filename)
    lyr = ds.GetLayer(0)
    assert lyr is not None, ('Can\'t get layer 0 from ' + filename)
    assert check_description == lyr.GetMetadataItem('DESCRIPTION')
    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)

###############################################################################
# Test writing and reading back unset/null date, time, datetime


def test_ogr_mitab_nulldatetime():

    filename = '/vsimem/nulldatetime.tab'
    ds = ogr.GetDriverByName('MapInfo File').CreateDataSource(filename)
    lyr = ds.CreateLayer("nulldatetime")
    lyr.CreateField(ogr.FieldDefn("time", ogr.OFTTime))
    lyr.CreateField(ogr.FieldDefn("date", ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn("datetime", ogr.OFTDateTime))
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert not f.IsFieldSet("time")
    assert not f.IsFieldSet("date")
    assert not f.IsFieldSet("datetime")
    ds = None

    ogr.GetDriverByName('MapInfo File').DeleteDataSource(filename)
