#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR PGEO driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
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

import gdaltest
import ogrtest
import os
import pytest
import sys
from osgeo import gdal
from osgeo import ogr


@pytest.fixture(scope="module", autouse=True)
def setup_driver():
    driver = ogr.GetDriverByName('PGeo')
    if driver is None:
        pytest.skip("PGeo driver not available", allow_module_level=True)

    # remove mdb driver
    mdb_driver = ogr.GetDriverByName('MDB')
    if mdb_driver is not None:
        mdb_driver.Deregister()

    # we may have the PGeo GDAL driver, but be missing an ODBC driver for MS Access on the test environment
    # so open a test dataset and check to see if it's supported
    pgeo_ds = ogr.Open('data/pgeo/sample.mdb')
    if 'MDB_ODBC_DRIVER_INSTALLED' in os.environ:
        # if environment variable is set, then we know that the ODBC driver is installed and something
        # unexpected has happened (i.e. GDAL driver is broken!)
        assert pgeo_ds is not None
    elif pgeo_ds is None:
        pytest.skip('could not open DB. MDB ODBC driver probably missing or misconfigured', allow_module_level=True)

    yield

    if mdb_driver is not None:
        print('Reregistering MDB driver')
        mdb_driver.Register()


@pytest.fixture()
def download_test_data():
    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/pgeo/PGeoTest.zip', 'PGeoTest.zip'):
        pytest.skip("Test data could not be downloaded")

    try:
        os.stat('tmp/cache/Autodesk Test.mdb')
    except OSError:
        try:
            gdaltest.unzip('tmp/cache', 'tmp/cache/PGeoTest.zip')
            try:
                os.stat('tmp/cache/Autodesk Test.mdb')
            except OSError:
                pytest.skip()
        except:
            pytest.skip()

    pgeo_ds = ogr.Open('tmp/cache/Autodesk Test.mdb')
    if pgeo_ds is None:
        pytest.skip('could not open DB. Driver probably misconfigured')

    return pgeo_ds


@pytest.fixture()
def ogrsf_path():
    import test_cli_utilities
    path = test_cli_utilities.get_test_ogrsf_path()
    if path is None:
        pytest.skip('ogrsf test utility not found')

    return path


def recent_enough_mdb_odbc_driver():
    # At time of writing, mdbtools <= 0.9.4 has some deficiencies
    # See https://github.com/OSGeo/gdal/pull/4354#issuecomment-907455798 for details
    # So allow some tests only or Windows, or on a local machine that don't have the CI environment variable set
    return sys.platform == 'win32' or 'CI' not in os.environ

###############################################################################
# Basic testing - PGeo v10 format


def test_ogr_pgeo_basic():
    pgeo_ds = ogr.Open('data/pgeo/sample.mdb')
    assert pgeo_ds is not None

    assert pgeo_ds.GetLayerCount() == 4, 'did not get expected layer count'

    lyr = pgeo_ds.GetLayer(0)
    assert lyr is not None
    assert lyr.GetName() == 'lines', 'did not get expected layer name'
    assert lyr.GetGeomType() == ogr.wkbMultiLineString, 'did not get expected layer geometry type'
    assert lyr.GetFeatureCount() == 6, 'did not get expected feature count'

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       feat.GetField('Name') != 'Highway' or \
       feat.GetField('Value_') != 1:
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    if ogrtest.check_feature_geometry(feat, 'MULTILINESTRING ((-117.623198392192 35.1986535259395,-117.581273749384 35.1986535259395,-117.078178041084 35.3244274525649,-116.868554829742 35.5340506648064,-116.617006974692 35.8694478036733,-116.491233048067 35.9532970883899,-116.1558359092 36.2886942272568,-116.071986624483 36.3725435119736,-115.443116988658 36.7498652936484,-114.814247352833 37.0433377906066,-114.311151644533 37.169111717232,-113.38880951175 37.3787349294735,-113.095337014791 37.3787349294735,-112.592241306491 37.3787349294735,-111.753748459324 37.2948856447567,-111.502200604274 37.2529610028481,-111.082954180691 37.1271870753233,-110.747557041824 37.1271870753233,-110.160612048807 36.9175638630819,-110.034838121282 36.8337145783652,-109.741365624324 36.7498652936484,-109.573667054891 36.6660160089317,-109.238269916024 36.4983174394983,-109.07057134659 36.4144681547816,-108.81902349244 36.2886942272568,-108.693249564915 36.2467695853481,-108.483626353573 36.1629203006314,-107.645133505507 35.9113724464813,-106.597017446098 35.8694478036733,-106.05199709499 35.7017492342398,-105.80044924084 35.6178999495231,-105.590826028598 35.5759753076144,-105.297353532539 35.5759753076144,-104.961956393673 35.5759753076144,-104.710408538623 35.5340506648064,-104.458860684473 35.4921260219984,-103.871915691456 35.4921260219984,-103.788066406739 35.4921260219984,-103.326895340348 35.4082767372817,-102.949573558673 35.4082767372817,-102.488402493181 35.4502013800897,-102.069156068698 35.4502013800897,-101.482211075681 35.4502013800897,-100.937190724572 35.6598245923311,-100.308321088747 35.8694478036733,-100.056773234597 36.0371463731067,-99.0505818179965 36.0790710159147,-97.6670686188216 35.7436738770479,-97.1639729105214 35.6178999495231,-96.1158568511128 35.5340506648064,-95.6127611419132 35.5340506648064,-94.3969465130711 35.9113724464813,-93.6842275925293 36.2886942272568,-92.9295840300789 36.8337145783652,-92.2587897523451 37.169111717232,-91.6299201165201 37.5045088569982,-90.4141054867787 37.8818306377738,-90.4141054867787 37.8818306377738,-90.2464069173453 37.9237552805818,-89.4917633548949 37.8399059958651,-89.156366216028 37.6722074264316,-88.485571937395 37.5045088569982,-87.8147776596612 37.2529610028481,-87.5632298055111 37.169111717232,-87.1439833810282 37.0433377906066,-85.9700933949941 36.8756392211732,-85.8023948255607 36.8756392211732,-84.0834844875191 36.9594885058899,-84.0415598456103 37.0433377906066,-82.9515191433937 37.5464334989069,-82.6999712892436 37.6302827836236))', max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')

    lyr = pgeo_ds.GetLayer(1)
    assert lyr is not None
    assert lyr.GetName() == 'points', 'did not get expected layer name'
    assert lyr.GetGeomType() == ogr.wkbPoint, 'did not get expected layer geometry type'
    assert lyr.GetFeatureCount() == 17, 'did not get expected feature count'

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       feat.GetField('Class') != 'Jet' or \
        feat.GetField('Heading') != 90 or \
        feat.GetField('Importance') != 3 or \
        feat.GetField('Pilots') != 2 or \
        feat.GetField('Cabin_Crew') != 0 or \
        feat.GetField('Staff') != 2:
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    if ogrtest.check_feature_geometry(feat, 'POINT (-117.232574188648 37.2187715665546)', max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')

    lyr = pgeo_ds.GetLayer(2)
    assert lyr is not None
    assert lyr.GetName() == 'polys', 'did not get expected layer name'
    assert lyr.GetGeomType() == ogr.wkbMultiPolygon, 'did not get expected layer geometry type'
    assert lyr.GetFeatureCount() == 10, 'did not get expected feature count'

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       feat.GetField('Name') != 'Lake' or \
        feat.GetField('Value_') != 10:
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    if ogrtest.check_feature_geometry(feat, 'MULTIPOLYGON (((-109.189702908462 45.4535155983662,-108.967868942498 45.7308080551477,-108.635117994 46.0081005128284,-108.135991571254 46.2299344778939,-107.692323640224 46.2853929696099,-107.415031183442 46.3408514613259,-106.915904760696 46.3408514613259,-106.472236828767 46.3408514613259,-106.139485880269 46.1744759870772,-105.97311040602 46.0635590036451,-105.862193423488 45.8971835293964,-105.640359457523 45.7308080551477,-105.52944247499 45.3425986158335,-105.473983983274 45.2871401241175,-105.418525492457 45.009847667336,-105.252150018209 44.7880137013713,-104.863940577995 44.5107212445898,-104.697565103747 44.3998042611578,-104.586648121214 44.233428786909,-104.531189629498 43.9561363301276,-104.531189629498 43.6788438733461,-104.586648121214 43.3460929248487,-104.697565103747 43.1797174505999,-104.863940577995 42.9578834846351,-105.030316052244 42.7915080103865,-105.363067000741 42.5696740444217,-105.473983983274 42.514215553605,-106.361319846234 42.2923815876402,-106.583153812199 42.4032985701729,-107.026821743229 42.7360495186704,-107.137738725762 42.8469665021024,-107.193197217478 43.0688004671679,-107.193197217478 43.2351759414166,-107.193197217478 43.4570099073813,-107.248655709194 43.8452193475949,-107.248655709194 43.8452193475949,-107.359572691726 43.9006778384116,-107.581406657691 43.9006778384116,-107.858699114473 43.9006778384116,-107.969616097005 43.9006778384116,-108.080533080437 43.7897608558789,-108.302367045503 43.7897608558789,-108.635117994 43.9006778384116,-108.967868942498 44.233428786909,-109.189702908462 44.3443457703411,-109.300619890995 44.5107212445898,-109.300619890995 44.7880137013713,-109.300619890995 45.0653061581527,-109.300619890995 45.1762231415848,-109.189702908462 45.4535155983662),(-107.750165309282 45.3597163907988,-107.951114759139 45.0381972704877,-107.870734979736 44.8774377107818,-107.830545089584 44.7568680403278,-107.70997541913 44.6362983707733,-107.589405749576 44.6362983707733,-107.14731695971 44.5157287012186,-107.066937179407 44.435348920916,-106.946367509853 44.3951590307647,-106.86598772955 44.2343994710588,-106.745418059096 44.0736399104536,-106.58465849939 44.0334500212016,-106.464088829836 44.0334500212016,-106.263139379978 44.0334500212016,-106.222949489827 44.0736399104536,-106.102379819373 44.1540196907562,-106.062189929222 44.3549691406134,-106.102379819373 44.5157287012186,-106.142569709524 44.7166781510759,-106.263139379978 44.8372478206305,-106.343519159382 44.9176276009331,-106.464088829836 44.9980073803364,-106.58465849939 45.0381972704877,-106.745418059096 45.1989568310929,-106.785607949247 45.2391467203449,-106.906177619701 45.3597163907988,-106.986557399105 45.5204759505048,-107.227696739113 45.5606658406561,-107.468836079122 45.5204759505048,-107.750165309282 45.3597163907988)),((-103.490036969431 45.9625647403705,-103.409657189128 46.2037040803791,-103.289087519573 46.4448434203876,-103.007758289414 46.5654130908415,-102.525479609396 46.7261726505474,-102.284340269388 46.6859827603961,-102.163770599833 46.5654130908415,-102.083390819531 46.4448434203876,-102.003011039228 46.324273750833,-101.842251479522 46.1635141911271,-101.721681809967 46.0429445206731,-101.681491919816 45.9223748511185,-101.681491919816 45.7616152905133,-101.681491919816 45.6410456209587,-101.802061589371 45.4802860603535,-101.882441369673 45.3597163907988,-101.962821149976 45.3195265006476,-102.083390819531 45.1587669409416,-102.123580709682 45.1185770507903,-102.324530159539 45.1185770507903,-102.445099829993 45.1185770507903,-102.565669499548 45.1185770507903,-102.967568399262 45.1989568310929,-102.967568399262 45.2391467203449,-103.168517849119 45.3999062809502,-103.208707739271 45.4802860603535,-103.369467299876 45.5606658406561,-103.490036969431 45.9625647403705)))', max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')


###############################################################################
# Basic testing - PGeo v9 format


def test_ogr_pgeo_basic_v9(download_test_data):
    assert download_test_data.GetLayerCount() == 3, 'did not get expected layer count'

    lyr = download_test_data.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       feat.GetField('IDNUM') != 9424 or \
       feat.GetField('OWNER') != 'City':
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    if ogrtest.check_feature_geometry(feat, 'MULTILINESTRING ((1910941.703951031 445833.57942859828 0,1910947.927691862 445786.43811868131 0))', max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')

    feat_count = lyr.GetFeatureCount()
    assert feat_count == 9418, 'did not get expected feature count'


###############################################################################
# Test LIST_ALL_TABLES open option


def test_ogr_pgeo_list_all_tables():
    pgeo_ds = ogr.Open('data/pgeo/sample.mdb')
    assert pgeo_ds is not None

    assert pgeo_ds.GetLayerCount() == 4, 'did not get expected layer count'

    # Test LIST_ALL_TABLES=YES open option
    pgeo_ds_all_table = gdal.OpenEx('data/pgeo/sample.mdb', gdal.OF_VECTOR,
                                 open_options=['LIST_ALL_TABLES=YES'])

    # Depending on the actual ODBC driver used (i.e Microsoft driver vs mdbtools driver), a different
    # set of system tables are exposed (18 vs 27). Mdbtools includes the various MSys* tables, while the
    # Microsoft driver strips these out. Here we test only for the common subset of tables.
    assert pgeo_ds_all_table.GetLayerCount() >= 18, 'did not get expected layer count'
    layer_names = [pgeo_ds_all_table.GetLayer(i).GetName() for i in range(pgeo_ds_all_table.GetLayerCount())]

    for name in ['lines', 'points', 'polys',
                 'GDB_ColumnInfo', 'GDB_DatabaseLocks', 'GDB_GeomColumns', 'GDB_ItemRelationships',
                 'GDB_ItemRelationshipTypes', 'GDB_Items', 'GDB_Items_Shape_Index', 'GDB_ItemTypes',
                 'GDB_RasterColumns', 'GDB_ReplicaLog', 'GDB_SpatialRefs', 'lines_Shape_Index', 'non_spatial',
                 'points_Shape_Index', 'polys_Shape_Index']:
        assert name in layer_names

    private_layers = [pgeo_ds_all_table.GetLayer(i).GetName() for i in range(pgeo_ds_all_table.GetLayerCount()) if pgeo_ds_all_table.IsLayerPrivate(i)]
    for name in ['GDB_ColumnInfo', 'GDB_DatabaseLocks', 'GDB_GeomColumns', 'GDB_ItemRelationships',
                 'GDB_ItemRelationshipTypes', 'GDB_Items', 'GDB_Items_Shape_Index', 'GDB_ItemTypes',
                 'GDB_RasterColumns', 'GDB_ReplicaLog', 'GDB_SpatialRefs', 'lines_Shape_Index',
                 'points_Shape_Index', 'polys_Shape_Index']:
        assert name in private_layers
    for name in ['lines','points', 'polys', 'non_spatial']:
        assert name not in private_layers



###############################################################################
# Test spatial filter


def test_ogr_pgeo_spatial_filter():
    pgeo_ds = ogr.Open('data/pgeo/sample.mdb')
    assert pgeo_ds is not None

    lyr = pgeo_ds.GetLayer(1)

    lyr.SetSpatialFilterRect(-118.328, 34.501, -107.457, 40.820)

    feat_count = lyr.GetFeatureCount()
    assert feat_count == 4, 'did not get expected feature count'

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1:
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 3:
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 9:
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 15:
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    feat = lyr.GetNextFeature()
    assert feat is None

    # Check that geometry filter is well cleared
    lyr.SetSpatialFilter(None)
    feat_count = lyr.GetFeatureCount()
    assert feat_count == 17, 'did not get expected feature count'


###############################################################################
# Test spatial filter, v9 format


def test_ogr_pgeo_spatial_filter_v9(download_test_data):
    lyr = download_test_data.GetLayer(0)
    lyr.ResetReading()
    lyr.SetSpatialFilterRect(1909982.7, 445835.6, 1911646.3, 446627.5)

    feat_count = lyr.GetFeatureCount()
    assert feat_count == 18, 'did not get expected feature count'

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 6845 or \
       feat.GetField('IDNUM') != 2574 or \
       feat.GetField('OWNER') != 'City':
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    # Check that geometry filter is well cleared
    lyr.SetSpatialFilter(None)
    feat_count = lyr.GetFeatureCount()
    assert feat_count == 9418, 'did not get expected feature count'

###############################################################################
# Test attribute filter


def test_ogr_pgeo_attribute_filter():
    pgeo_ds = ogr.Open('data/pgeo/sample.mdb')
    assert pgeo_ds is not None

    lyr = pgeo_ds.GetLayer(1)

    lyr.SetAttributeFilter("Class='Biplane'")

    feat_count = lyr.GetFeatureCount()
    assert feat_count == 5, 'did not get expected feature count'

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 2 or \
       feat.GetField('Class') != 'Biplane' or \
        feat.GetField('Heading') != 0 or \
        feat.GetField('Importance') != 1 or \
        feat.GetField('Pilots') != 3 or \
        feat.GetField('Cabin_Crew') != 3 or \
        feat.GetField('Staff') != 6:
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    # Check that attribute filter is well cleared (#3706)
    lyr.SetAttributeFilter(None)
    feat_count = lyr.GetFeatureCount()
    assert feat_count == 17, 'did not get expected feature count'


###############################################################################
# Test attribute filter, v9 format data


def test_ogr_pgeo_attribute_filter_v9(download_test_data):
    lyr = download_test_data.GetLayer(0)
    lyr.SetAttributeFilter('OBJECTID=1')

    feat_count = lyr.GetFeatureCount()
    assert feat_count == 1, 'did not get expected feature count'

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       feat.GetField('IDNUM') != 9424 or \
       feat.GetField('OWNER') != 'City':
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    # Check that attribute filter is well cleared (#3706)
    lyr.SetAttributeFilter(None)
    feat_count = lyr.GetFeatureCount()
    assert feat_count == 9418, 'did not get expected feature count'


###############################################################################
# Test ExecuteSQL()


def test_ogr_pgeo_execute_sql():
    pgeo_ds = ogr.Open('data/pgeo/sample.mdb')
    assert pgeo_ds is not None

    sql_lyr = pgeo_ds.ExecuteSQL("SELECT * FROM points WHERE Class = 'B52'")

    feat_count = sql_lyr.GetFeatureCount()
    assert feat_count == 4, 'did not get expected feature count'

    feat = sql_lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 10 or \
       feat.GetField('Class') != 'B52' or \
        feat.GetField('Heading') != 0 or \
        feat.GetField('Importance') != 10 or \
        feat.GetField('Pilots') != 2 or \
        feat.GetField('Cabin_Crew') != 1 or \
        feat.GetField('Staff') != 3:
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    pgeo_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test ExecuteSQL(), v9 format data


def test_ogr_pgeo_execute_sql_v9(download_test_data):
    sql_lyr = download_test_data.ExecuteSQL('SELECT * FROM SDPipes WHERE OBJECTID = 1')

    feat_count = sql_lyr.GetFeatureCount()
    assert feat_count == 1, 'did not get expected feature count'

    feat = sql_lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       feat.GetField('IDNUM') != 9424 or \
       feat.GetField('OWNER') != 'City':
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    download_test_data.ReleaseResultSet(sql_lyr)


###############################################################################
# Test GetFeature()


def test_ogr_pgeo_get_feature():
    pgeo_ds = ogr.Open('data/pgeo/sample.mdb')
    assert pgeo_ds is not None

    lyr = pgeo_ds.GetLayer(1)
    feat = lyr.GetFeature(14)
    if feat.GetField('OBJECTID') != 14:
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')


###############################################################################
# Test GetFeature(), v9 format data


def test_ogr_pgeo_get_feature_v9(download_test_data):
    lyr = download_test_data.GetLayer(0)
    feat = lyr.GetFeature(9418)
    if feat.GetField('OBJECTID') != 9418:
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')


###############################################################################
# Run test_ogrsf


def test_ogr_pgeo_ogrsf(ogrsf_path):
    ret = gdaltest.runexternal(ogrsf_path + ' data/pgeo/sample.mdb')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1


###############################################################################
# Run test_ogrsf, v9 format data


def test_ogr_pgeo_ogrsf_v9(download_test_data, ogrsf_path):
    ret = gdaltest.runexternal(ogrsf_path + ' "tmp/cache/Autodesk Test.mdb"')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1


###############################################################################
# Run test_ogrsf with -sql


def test_ogr_pgeo_ogrsf_sql(ogrsf_path):
    ret = gdaltest.runexternal(ogrsf_path + ' "data/pgeo/sample.mdb" -sql "SELECT * FROM points"')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1


###############################################################################
# Run test_ogrsf with -sql, v9 format data


def test_ogr_pgeo_ogrsf_sql_v9(download_test_data, ogrsf_path):
    ret = gdaltest.runexternal(ogrsf_path + ' "tmp/cache/Autodesk Test.mdb" -sql "SELECT * FROM SDPipes"')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1


###############################################################################
# Open mdb with non-spatial tables


def test_ogr_pgeo_non_spatial():
    pgeo_ds = ogr.Open('data/pgeo/sample.mdb')
    assert pgeo_ds.GetLayerCount() == 4, 'did not get expected layer count'

    layer_names = [pgeo_ds.GetLayer(n).GetName() for n in range(4)]
    assert set(layer_names) == {'lines', 'polys', 'points', 'non_spatial'}, 'did not get expected layer names'

    non_spatial_layer = pgeo_ds.GetLayerByName('non_spatial')
    feat = non_spatial_layer.GetNextFeature()
    if feat.GetField('text_field') != 'Record 1' or \
       feat.GetField('int_field') != 13 or \
       feat.GetField('long_int_field') != 10001 or \
       feat.GetField('float_field') != 13.5 or \
       feat.GetField('double_field') != 14.5:
        feat.DumpReadable()
        pytest.fail('did not get expected attributes')

    if recent_enough_mdb_odbc_driver():
        assert feat.GetField('date_field') == '2020/01/30 00:00:00'

    feat_count = non_spatial_layer.GetFeatureCount()
    assert feat_count == 2, 'did not get expected feature count'

##################################################################################
# Open mdb with polygon layer containing a mix of single and multi-part geometries


def test_ogr_pgeo_mixed_single_multi_polygons():
    pgeo_ds = ogr.Open('data/pgeo/mixed_types.mdb')

    polygon_layer = pgeo_ds.GetLayerByName('polygons')
    assert polygon_layer.GetGeomType() == ogr.wkbMultiPolygon

    # The PGeo format has a similar approach to multi-part handling as Shapefiles,
    # where polygon and multipolygon geometries or line and multiline geometries will
    # co-exist in a layer reported as just polygon or line type respectively.
    # To handle this in a predictable way for clients we always promote the polygon/line
    # types to multitypes, and correspondingly ALWAYS return multi polygon/line geometry
    # objects for features (even if strictly speaking the original feature had a polygon/line
    # geometry object)
    feat = polygon_layer.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,
                                      'MULTIPOLYGON (((-11315979.9947 6171775.831,-10597634.808 6140025.7675,-11331855.0265 5477243.192,-11315979.9947 6171775.831)))',
                                      max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')

    feat = polygon_layer.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,
                                      'MULTIPOLYGON (((-9855477.0737 5596305.9301,-9581632.776 5258961.5054,-9863414.5896 5258961.5054,-9855477.0737 5596305.9301)),((-10101540.0658 6092400.6723,-9470507.5538 6112244.462,-9490351.3435 5350242.938,-10101540.0658 6092400.6723)))',
                                      max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')

##################################################################################
# Open mdb with lines layer containing a mix of single and multi-part geometries


def test_ogr_pgeo_mixed_single_multi_lines():
    pgeo_ds = ogr.Open('data/pgeo/mixed_types.mdb')

    polygon_layer = pgeo_ds.GetLayerByName('lines')
    assert polygon_layer.GetGeomType() == ogr.wkbMultiLineString

    # The PGeo format has a similar approach to multi-part handling as Shapefiles,
    # where polygon and multipolygon geometries or line and multiline geometries will
    # co-exist in a layer reported as just polygon or line type respectively.
    # To handle this in a predictable way for clients we always promote the polygon/line
    # types to multitypes, and correspondingly ALWAYS return multi polygon/line geometry
    # objects for features (even if strictly speaking the original feature had a polygon/line
    # geometry object)
    feat = polygon_layer.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,
                                      'MULTILINESTRING ((-10938947.9907 6608339.2042,-10244415.3516 6608339.2042))',
                                      max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')

    feat = polygon_layer.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,
                                      'MULTILINESTRING ((-10383321.8794 6457526.4025,-10391259.3953 5786806.3111),(-10252352.8675 6465463.9184,-9625289.1133 6469432.6764))',
                                      max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')


##################################################################################
# Open mdb with layers with z/m and check that they are handled correctly


def test_ogr_pgeo_z_m_handling():
    pgeo_ds = ogr.Open('data/pgeo/geometry_types.mdb')

    point_z_layer = pgeo_ds.GetLayerByName('point_z')
    if recent_enough_mdb_odbc_driver():
        assert point_z_layer.GetGeomType() == ogr.wkbPoint25D
    else:
        assert point_z_layer.GetGeomType() in (ogr.wkbPoint, ogr.wkbPoint25D)

    feat = point_z_layer.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,
                                      'POINT Z (-2 -1.0 4)',
                                      max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')

    feat = point_z_layer.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,
                                      'POINT Z (1 2 3)',
                                      max_error=0.0000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')

    point_m_layer = pgeo_ds.GetLayerByName('point_m')
    if recent_enough_mdb_odbc_driver():
        assert point_m_layer.GetGeomType() == ogr.wkbPointM
    else:
        assert point_m_layer.GetGeomType() in (ogr.wkbPoint, ogr.wkbPointM)

    feat = point_m_layer.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,
                                      'POINT M (1 2 11)',
                                      max_error=0.0001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')

    feat = point_m_layer.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,
                                      'POINT M (-2 -1 13)',
                                      max_error=0.0001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')

    point_zm_layer = pgeo_ds.GetLayerByName('point_zm')
    if recent_enough_mdb_odbc_driver():
        assert point_zm_layer.GetGeomType() == ogr.wkbPointZM
    else:
        assert point_zm_layer.GetGeomType() in (ogr.wkbPoint, ogr.wkbPointZM)

    feat = point_zm_layer.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,
                                      'POINT ZM (-2 -1.0 4 13)',
                                      max_error=0.0001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')

    feat = point_zm_layer.GetNextFeature()
    if ogrtest.check_feature_geometry(feat,
                                      'POINT ZM (1 2 3 11)',
                                      max_error=0.0001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected geometry')

###############################################################################


def test_ogr_pgeo_read_domains():
    ds = gdal.OpenEx('data/pgeo/domains.mdb', gdal.OF_VECTOR)

    with gdaltest.error_handler():
        assert ds.GetFieldDomain('i_dont_exist') is None
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'domains'
    lyr_defn = lyr.GetLayerDefn()

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('domain_1'))
    assert fld_defn.GetDomainName() == 'my coded domain'

    domain = ds.GetFieldDomain('my coded domain')
    assert domain is not None
    assert domain.GetName() == 'my coded domain'
    assert domain.GetDescription() == 'domain description'
    assert domain.GetDomainType() == ogr.OFDT_CODED
    assert domain.GetFieldType() == fld_defn.GetType()
    assert domain.GetFieldSubType() == fld_defn.GetSubType()
    assert domain.GetEnumeration() == {'code_a': 'Description A',
                                       'code_b': 'Description B',
                                       'code_c': 'Description C'}

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('domain_3'))
    assert fld_defn.GetDomainName() == 'range_domain'

    domain = ds.GetFieldDomain('range_domain')
    assert domain is not None
    assert domain.GetName() == 'range_domain'
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == fld_defn.GetType()
    assert domain.GetFieldSubType() == fld_defn.GetSubType()
    assert domain.GetMinAsDouble() == -5.0
    assert domain.GetMaxAsDouble() == 50.0


###############################################################################
# Test retrieving layer definition

def test_ogr_pgeo_read_definition():
    ds = gdal.OpenEx('data/pgeo/metadata.mdb', gdal.OF_VECTOR)

    sql_lyr = ds.ExecuteSQL('GetLayerDefinition not a table')
    assert sql_lyr is None

    sql_lyr = ds.ExecuteSQL('GetLayerDefinition metadata')
    feat_count = sql_lyr.GetFeatureCount()
    assert feat_count == 1, 'did not get expected feature count'

    feat = sql_lyr.GetNextFeature()
    assert feat is not None
    assert feat.GetField(0).startswith('<DEFeatureClassInfo')

    ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test retrieving layer metadata

def test_ogr_pgeo_read_metadata():
    ds = gdal.OpenEx('data/pgeo/metadata.mdb', gdal.OF_VECTOR)

    sql_lyr = ds.ExecuteSQL('GetLayerMetadata not a table')
    assert sql_lyr is None

    sql_lyr = ds.ExecuteSQL('GetLayerMetadata metadata')
    feat_count = sql_lyr.GetFeatureCount()
    assert feat_count == 1, 'did not get expected feature count'

    feat = sql_lyr.GetNextFeature()
    assert feat is not None
    assert feat.GetField(0).startswith('<metadata xml:lang="en"><Esri>')

    ds.ReleaseResultSet(sql_lyr)

