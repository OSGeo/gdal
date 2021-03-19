#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  WFS driver testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

from http.server import BaseHTTPRequestHandler
import os

import pytest

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal
import webserver

###############################################################################
# Test underlying OGR drivers
#


pytestmark = pytest.mark.require_driver('WFS')


@pytest.fixture(autouse=True, scope='module')
def ogr_wfs_init():
    gdaltest.wfs_drv = ogr.GetDriverByName('WFS')

    gdaltest.geoserver_wfs = None
    gdaltest.deegree_wfs = None
    gdaltest.ionic_wfs = None

    gml_ds = ogr.Open('data/gml/ionic_wfs.gml')
    if gml_ds is None:
        gdaltest.wfs_drv = None
        if gdal.GetLastErrorMsg().find('Xerces') != -1:
            pytest.skip()
        pytest.skip('failed to open test file.')


@pytest.fixture(
    params=['NO', None],
    scope='module',
    ids=['without-streaming', 'with-streaming']
)
def with_and_without_streaming(request):
    with gdaltest.config_option('OGR_WFS_USE_STREAMING', request.param):
        yield


###############################################################################
# Test reading a MapServer WFS server


@pytest.mark.skip()
def test_ogr_wfs_mapserver():
    if gdaltest.wfs_drv is None:
        pytest.skip()

    if gdaltest.gdalurlopen('http://www2.dmsolutions.ca/cgi-bin/mswfs_gmap') is None:
        pytest.skip('cannot open URL')

    ds = ogr.Open('WFS:http://www2.dmsolutions.ca/cgi-bin/mswfs_gmap')
    if ds is None:
        pytest.skip('did not managed to open WFS datastore')

    assert ds.GetLayerCount() == 2, 'did not get expected layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'park', 'did not get expected layer name'

    sr = lyr.GetSpatialRef()
    sr2 = osr.SpatialReference()
    sr2.ImportFromEPSG(42304)
    assert sr.IsSame(sr2), 'did not get expected SRS'

    feat_count = lyr.GetFeatureCount()
    assert feat_count == 46, 'did not get expected feature count'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    geom_wkt = geom.ExportToWkt()
    if geom_wkt.find("POLYGON ((389366.84375 3791519.75") == -1:
        feat.DumpReadable()
        pytest.fail('did not get expected feature')

    

###############################################################################
# Test reading a GeoServer WFS server

@pytest.mark.skip('FIXME: re-enable after adapting test')
def test_ogr_wfs_geoserver():
    if gdaltest.wfs_drv is None:
        pytest.skip()

    if gdaltest.gdalurlopen('http://demo.opengeo.org/geoserver/wfs?TYPENAME=za:za_points&SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType') is None:
        gdaltest.geoserver_wfs = False
        pytest.skip('cannot open URL')
    gdaltest.geoserver_wfs = True

    ds = ogr.Open('WFS:http://demo.opengeo.org/geoserver/wfs?TYPENAME=za:za_points')
    assert ds is not None, 'did not managed to open WFS datastore'

    assert ds.GetLayerCount() == 1, 'did not get expected layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'za:za_points', 'did not get expected layer name'

    sr = lyr.GetSpatialRef()
    sr2 = osr.SpatialReference()
    sr2.ImportFromEPSG(4326)
    assert sr.IsSame(sr2), 'did not get expected SRS'

    feat_count = lyr.GetFeatureCount()
    if feat_count < 14000:
        if gdal.GetLastErrorMsg().find('The connection attempt failed') != -1:
            gdaltest.geoserver_wfs = False
            pytest.skip('server probably in a broken state')
        print(feat_count)
        pytest.fail('did not get expected feature count')

    assert lyr.TestCapability(ogr.OLCFastFeatureCount), 'did not get OLCFastFeatureCount'

    ds = ogr.Open('WFS:http://demo.opengeo.org/geoserver/wfs?TYPENAME=tiger:poi&MAXFEATURES=10&VERSION=1.1.0')
    if ds is None:
        pytest.skip('server perhaps overloaded')
    lyr = ds.GetLayer(0)
    gdal.ErrorReset()
    feat = lyr.GetNextFeature()

    # This error message is generally the sign of a server in a broken state
    if feat is None and gdal.GetLastErrorMsg().find('<ows:ExceptionText>org.geoserver.platform.ServiceException') != -1:
        gdaltest.geoserver_wfs = False
        pytest.skip('server probably in a broken state')

    if feat.GetField('NAME') != 'museam' or \
       ogrtest.check_feature_geometry(feat, 'POINT (-74.0104611 40.70758763)',
                                      max_error=0.000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected feature (1)')

    # Same with VERSION=1.0.0
    ds = ogr.Open('WFS:http://demo.opengeo.org/geoserver/wfs?TYPENAME=tiger:poi&MAXFEATURES=10&VERSION=1.0.0')
    if ds is None:
        pytest.skip('server perhaps overloaded')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetField('NAME') != 'museam' or \
       ogrtest.check_feature_geometry(feat, 'POINT (-74.0104611 40.70758763)',
                                      max_error=0.000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected feature (2)')

    # Test attribute filter
    ds = ogr.Open("WFS:http://demo.opengeo.org/geoserver/wfs?TYPENAME=tiger:poi")
    if ds is None:
        pytest.skip('server perhaps overloaded')
    lyr = ds.GetLayer(0)
    lyr.SetAttributeFilter("MAINPAGE is not null and NAME >= 'a' and NAME LIKE 'mu%%eam'")
    feat_count = lyr.GetFeatureCount()
    assert feat_count == 1, \
        'did not get expected feature count after SetAttributeFilter (1)'
    feat = lyr.GetNextFeature()
    if feat.GetField('gml_id') != 'poi.1':
        feat.DumpReadable()
        pytest.fail('did not get expected feature (3)')

    if False:  # pylint: disable=using-constant-test
        # This GeoServer version doesn't understand <GmlObjectId>
        lyr.SetAttributeFilter("gml_id = 'poi.1'")
        feat_count = lyr.GetFeatureCount()
        assert feat_count == 1, \
            'did not get expected feature count after SetAttributeFilter (2)'
        feat = lyr.GetNextFeature()
        if feat.GetField('gml_id') != 'poi.1':
            feat.DumpReadable()
            pytest.fail('did not get expected feature (4)')

    
###############################################################################
# Test reading a GeoServer WFS server with OUTPUTFORMAT=json


@pytest.mark.skip('FIXME: re-enable after adapting test')
def test_ogr_wfs_geoserver_json():
    if gdaltest.wfs_drv is None:
        pytest.skip()

    if not gdaltest.geoserver_wfs:
        pytest.skip()

    ds = ogr.Open('WFS:http://demo.opengeo.org/geoserver/wfs?TYPENAME=za:za_points&MAXFEATURES=10&VERSION=1.1.0&OUTPUTFORMAT=json')
    assert ds is not None, 'did not managed to open WFS datastore'

    assert ds.GetLayerCount() == 1, 'did not get expected layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'za:za_points', 'did not get expected layer name'

    feat_count = lyr.GetFeatureCount()
    assert feat_count == 10, 'did not get expected feature count'

    assert lyr.TestCapability(ogr.OLCFastFeatureCount), \
        'did not get OLCFastFeatureCount'

    feat = lyr.GetNextFeature()
    # if feat.GetField('name') != 'Alexander Bay' or \
    if ogrtest.check_feature_geometry(feat, 'POINT (16.4827778 -28.5947222)',
                                      max_error=0.000000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected feature')

    

###############################################################################
# Test reading a GeoServer WFS server with OUTPUTFORMAT=SHAPE-ZIP

@pytest.mark.skip('FIXME: re-enable after adapting test')
def test_ogr_wfs_geoserver_shapezip():
    if gdaltest.wfs_drv is None:
        pytest.skip()

    if not gdaltest.geoserver_wfs:
        pytest.skip()

    ds = ogr.Open('WFS:http://demo.opengeo.org/geoserver/wfs?TYPENAME=za:za_points&MAXFEATURES=10&VERSION=1.1.0&OUTPUTFORMAT=SHAPE-ZIP')
    assert ds is not None, 'did not managed to open WFS datastore'

    assert ds.GetLayerCount() == 1, 'did not get expected layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'za:za_points', 'did not get expected layer name'

    feat_count = lyr.GetFeatureCount()
    assert feat_count == 10, 'did not get expected feature count'

    assert lyr.TestCapability(ogr.OLCFastFeatureCount), \
        'did not get OLCFastFeatureCount'

    feat = lyr.GetNextFeature()
    # if feat.GetField('name') != 'Alexander Bay' or \
    if ogrtest.check_feature_geometry(feat, 'POINT (16.4827778 -28.5947222)',
                                      max_error=0.000000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected feature')

    
###############################################################################
# Test WFS paging


@pytest.mark.skip('FIXME: re-enable after adapting test')
def test_ogr_wfs_geoserver_paging():
    if gdaltest.wfs_drv is None:
        pytest.skip()

    if not gdaltest.geoserver_wfs:
        pytest.skip()

    ds = ogr.Open('WFS:http://demo.opengeo.org/geoserver/wfs?TYPENAME=og:bugsites&VERSION=1.1.0')
    lyr = ds.GetLayer(0)
    feature_count_ref = lyr.GetFeatureCount()
    page_size = (int)(feature_count_ref / 3) + 1
    ds = None

    # Test with WFS 1.0.0
    gdal.SetConfigOption('OGR_WFS_PAGING_ALLOWED', 'ON')
    gdal.SetConfigOption('OGR_WFS_PAGE_SIZE', '%d' % page_size)
    ds = ogr.Open('WFS:http://demo.opengeo.org/geoserver/wfs?TYPENAME=og:bugsites&VERSION=1.0.0')
    gdal.SetConfigOption('OGR_WFS_PAGING_ALLOWED', None)
    gdal.SetConfigOption('OGR_WFS_PAGE_SIZE', None)
    assert ds is not None, 'did not managed to open WFS datastore'

    lyr = ds.GetLayer(0)
    feature_count_wfs100 = lyr.GetFeatureCount()
    ds = None

    assert feature_count_wfs100 == feature_count_ref

    # Test with WFS 1.1.0
    gdal.SetConfigOption('OGR_WFS_PAGING_ALLOWED', 'ON')
    gdal.SetConfigOption('OGR_WFS_PAGE_SIZE', '%d' % page_size)
    ds = ogr.Open('WFS:http://demo.opengeo.org/geoserver/wfs?TYPENAME=og:bugsites&VERSION=1.1.0')
    gdal.SetConfigOption('OGR_WFS_PAGING_ALLOWED', None)
    gdal.SetConfigOption('OGR_WFS_PAGE_SIZE', None)
    assert ds is not None, 'did not managed to open WFS datastore'

    lyr = ds.GetLayer(0)
    feature_count_wfs110 = lyr.GetFeatureCount()

    feature_count_wfs110_at_hand = 0
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    while feat is not None:
        feature_count_wfs110_at_hand = feature_count_wfs110_at_hand + 1
        feat = lyr.GetNextFeature()
    ds = None

    assert feature_count_wfs110 == feature_count_ref, feature_count_wfs100

    assert feature_count_wfs110_at_hand == feature_count_ref
###############################################################################
# Test reading a Deegree WFS server


@pytest.mark.skip()
def test_ogr_wfs_deegree():
    if gdaltest.wfs_drv is None:
        pytest.skip()

    if gdaltest.gdalurlopen('http://demo.deegree.org:80/utah-workspace') is None:
        gdaltest.deegree_wfs = False
        pytest.skip('cannot open URL')
    gdaltest.deegree_wfs = True

    ds = ogr.Open("WFS:http://demo.deegree.org:80/utah-workspace/services/wfs?ACCEPTVERSIONS=1.1.0&MAXFEATURES=10")
    if ds is None:
        if gdal.GetLastErrorMsg().find('Error returned by server') < 0:
            gdaltest.deegree_wfs = False
            pytest.skip()
        pytest.fail('did not managed to open WFS datastore')

    lyr = ds.GetLayerByName('app:SGID024_Springs')
    assert lyr.GetName() == 'app:SGID024_Springs', 'did not get expected layer name'

    sr = lyr.GetSpatialRef()
    sr2 = osr.SpatialReference()
    sr2.ImportFromEPSG(26912)
    assert sr.IsSame(sr2), 'did not get expected SRS'

    feat = lyr.GetNextFeature()
    if feat.GetField('OBJECTID') != 1 or \
       ogrtest.check_feature_geometry(feat, 'POINT (558750.703 4402882.05)',
                                      max_error=0.000000001) != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected feature')

    # Test attribute filter
    ds = ogr.Open("WFS:http://demo.deegree.org:80/utah-workspace/services/wfs?ACCEPTVERSIONS=1.1.0")
    lyr = ds.GetLayerByName('app:SGID024_Springs')
    lyr.SetAttributeFilter('OBJECTID = 9 or OBJECTID = 100 or (OBJECTID >= 20 and OBJECTID <= 30 and OBJECTID != 27)')
    feat_count = lyr.GetFeatureCount()
    if feat_count != 12:
        if gdal.GetLastErrorMsg().find('XML parsing of GML file failed') < 0 and \
           gdal.GetLastErrorMsg().find('No suitable driver found') < 0:
            print(feat_count)
            pytest.fail('did not get expected feature count after SetAttributeFilter')

    # Test attribute filter with gml_id
    # lyr.SetAttributeFilter("gml_id = 'SGID024_Springs30' or gml_id = 'SGID024_Springs100'")
    # feat_count = lyr.GetFeatureCount()
    # if feat_count != 2:
    #    gdaltest.post_reason('did not get expected feature count after SetAttributeFilter (2)')
    #    print(feat_count)
    #    return 'fail'
    
###############################################################################
# Run test_ogrsf


@pytest.mark.skip()
def test_ogr_wfs_test_ogrsf():
    if gdaltest.wfs_drv is None:
        pytest.skip()

    if not gdaltest.deegree_wfs:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro "WFS:http://demo.deegree.org:80/utah-workspace/services/wfs?ACCEPTVERSIONS=1.1.0&MAXFEATURES=10" app:SGID024_Springs')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1


###############################################################################
do_log = False


class WFSHTTPHandler(BaseHTTPRequestHandler):

    def log_request(self, code='-', size='-'):
        pass

    def do_GET(self):

        try:
            if do_log:
                f = open('/tmp/log.txt', 'a')
                f.write('GET %s\n' % self.path)
                f.close()

            if self.path.find('/fakewfs') != -1:

                if self.path == '/fakewfs?SERVICE=WFS&REQUEST=GetCapabilities' or \
                        self.path == '/fakewfs?SERVICE=WFS&REQUEST=GetCapabilities&ACCEPTVERSIONS=1.1.0,1.0.0':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    f = open('data/wfs/get_capabilities.xml', 'rb')
                    content = f.read()
                    f.close()
                    self.wfile.write(content)
                    return

                if self.path == '/fakewfs?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=rijkswegen':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    f = open('data/wfs/describe_feature_type.xml', 'rb')
                    content = f.read()
                    f.close()
                    self.wfile.write(content)
                    return

                if self.path == '/fakewfs?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=rijkswegen':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    f = open('data/wfs/get_feature.xml', 'rb')
                    content = f.read()
                    f.close()
                    self.wfile.write(content)
                    return

            return
        except IOError:
            pass

        self.send_error(404, 'File Not Found: %s' % self.path)

###############################################################################
# Test reading a local fake WFS server


def test_ogr_wfs_fake_wfs_server():
    if gdaltest.wfs_drv is None:
        pytest.skip()

    (process, port) = webserver.launch(handler=WFSHTTPHandler)
    if port == 0:
        pytest.skip()

    gdal.SetConfigOption('OGR_WFS_LOAD_MULTIPLE_LAYER_DEFN', 'NO')
    ds = ogr.Open("WFS:http://127.0.0.1:%d/fakewfs" % port)
    gdal.SetConfigOption('OGR_WFS_LOAD_MULTIPLE_LAYER_DEFN', None)
    if ds is None:
        webserver.server_stop(process, port)
        pytest.fail('did not managed to open WFS datastore')

    lyr = ds.GetLayerByName('rijkswegen')
    if lyr.GetName() != 'rijkswegen':
        print(lyr.GetName())
        webserver.server_stop(process, port)
        pytest.fail('did not get expected layer name')

    sr = lyr.GetSpatialRef()
    sr2 = osr.SpatialReference()
    sr2.ImportFromEPSG(28992)
    if not sr.IsSame(sr2):
        print(sr)
        webserver.server_stop(process, port)
        pytest.fail('did not get expected SRS')

    feat = lyr.GetNextFeature()
    if feat.GetField('MPLength') != '33513.' or \
       ogrtest.check_feature_geometry(feat, 'MULTICURVE ((154898.65286 568054.62753,160108.36082 566076.78094,164239.254332 563024.70188,170523.31535 561231.219583,172676.42256 559253.37299,175912.80562 557459.89069,180043.699132 553508.779495,183294.491306 552250.182732))',
                                      max_error=0.00001) != 0:
        feat.DumpReadable()
        webserver.server_stop(process, port)
        pytest.fail('did not get expected feature')

    webserver.server_stop(process, port)

###############################################################################
# Test CreateFeature() / UpdateFeature() / DeleteFeature() (WFS-T)


@pytest.mark.skip('FIXME: re-enable after adapting test')
def test_ogr_wfs_geoserver_wfst():
    if gdaltest.wfs_drv is None:
        pytest.skip()

    if not gdaltest.geoserver_wfs:
        pytest.skip()

    ds = ogr.Open('WFS:http://demo.opengeo.org/geoserver/wfs?VERSION=1.1.0', update=1)
    assert ds is not None

    lyr = ds.GetLayerByName('za:za_points')
    geom = ogr.CreateGeometryFromWkt('POINT(0 89.5)')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    # feat.SetField('name', 'name_set_by_ogr_wfs_8_test')
    feat.SetField('type', 'type_set_by_ogr_wfs_8_test')
    if lyr.CreateFeature(feat) != 0:
        # Likely a bug in the current GeoServer version ??
        if gdal.GetLastErrorMsg().find("No such property 'typeName'") >= 0:
            pytest.skip()

        pytest.fail('cannot create feature')

    print('Feature %d created !' % feat.GetFID())

    feat.SetField('type', 'type_modified_by_ogr_wfs_8_test')
    assert lyr.SetFeature(feat) == 0, 'cannot update feature'
    print('Feature %d updated !' % feat.GetFID())

    assert lyr.DeleteFeature(feat.GetFID()) == 0, 'could not delete feature'

    print('Feature %d deleted !' % feat.GetFID())

    # Test transactions
    assert lyr.StartTransaction() == 0, 'CommitTransaction() failed'

    geom = ogr.CreateGeometryFromWkt('POINT(0 89.5)')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    # feat.SetField('name', 'name_set_by_ogr_wfs_8_test')
    feat.SetField('type', 'type_set_by_ogr_wfs_8_test')
    assert lyr.CreateFeature(feat) == 0, 'cannot create feature'
    geom = ogr.CreateGeometryFromWkt('POINT(0 89.5)')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    # feat.SetField('name', 'name_set_by_ogr_wfs_8_test_2')
    feat.SetField('type', 'type_set_by_ogr_wfs_8_test_2')
    assert lyr.CreateFeature(feat) == 0, 'cannot create feature'

    assert lyr.CommitTransaction() == 0, 'CommitTransaction() failed'

    # Retrieve inserted features
    print('Retrieving created features gml:id')
    sql_lyr = ds.ExecuteSQL("SELECT _LAST_INSERTED_FIDS_ FROM za:za_points")
    feat = sql_lyr.GetNextFeature()
    while feat is not None:
        gml_id = feat.GetFieldAsString(0)
        print('Feature %s has been created in transaction !' % gml_id)
        feat = sql_lyr.GetNextFeature()
    feat = None
    count = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)

    assert count == 2, 'did not get expected feature count'

    # Delete a bunch of features
    print('Deleting created features')
    sql_lyr = ds.ExecuteSQL("DELETE FROM za:za_points WHERE type = 'type_set_by_ogr_wfs_8_test' OR type = 'type_set_by_ogr_wfs_8_test_2'")
    ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test CreateFeature() / UpdateFeature() / DeleteFeature() with expected
# failure due to server not allowing insert & delete

@pytest.mark.skip()
def test_ogr_wfs_deegree_wfst():

    if gdaltest.gdalurlopen('http://testing.deegree.org/deegree-wfs/services') is None:
        pytest.skip('cannot open URL')

    ds = ogr.Open('WFS:http://testing.deegree.org/deegree-wfs/services', update=1)
    assert ds is not None

    lyr = ds.GetLayerByName('app:CountyBoundaries_edited')
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    feat.SetField('name', 'nameSetByOGR')
    feat.SetField('fips', '10')
    feat.SetField('feature_id', '123456')
    feat.SetField('OBJECTID', '7890123')
    feat.SetField('shape_area', 12.34)
    feat.SetField('shape_len', 56.78)

    ret = lyr.CreateFeature(feat)
    if ret != 0:
        print('expected fail on CreateFeature')

    ret = lyr.DeleteFeature(1)
    if ret != 0:
        print('expected fail on DeleteFeature')

    feat = lyr.GetFeature(10)
    ret = lyr.SetFeature(feat)
    if ret != 0:
        print('expected fail on SetFeature')

    
###############################################################################
# Test CreateFeature() / UpdateFeature() / DeleteFeature() on a WFS 1.0.0 server


@pytest.mark.skip()
def test_ogr_wfs_ionic_wfst():

    if gdaltest.gdalurlopen('http://webservices.ionicsoft.com/ionicweb/wfs/BOSTON_ORA') is None:
        gdaltest.ionic_wfs = False
        pytest.skip('cannot open URL')
    gdaltest.ionic_wfs = True

    ds = ogr.Open('WFS:http://webservices.ionicsoft.com/ionicweb/wfs/BOSTON_ORA', update=1)
    if ds is None:
        if gdal.GetLastErrorMsg().find('HTTP error code : 403') != -1:
            gdaltest.ionic_wfs = False
            pytest.skip()
        pytest.fail()

    lyr = ds.GetLayerByName('wfs:BUSINESS')
    geom = ogr.CreateGeometryFromWkt('POINT(234000 890000)')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    feat.SetField('NAME', 'nameSetByOGR')
    feat.SetField('TOTAL_EMPLOYEES', '10')

    ret = lyr.CreateFeature(feat)
    assert ret == 0, 'fail on CreateFeature'

    gmlid = feat.GetField('gml_id')

    ret = lyr.SetFeature(feat)
    assert ret == 0, 'fail on SetFeature'

    ds.ExecuteSQL("DELETE FROM wfs:BUSINESS WHERE gml_id = '%s'" % gmlid)

###############################################################################
# Test ExecuteSQL() where SQL should be turned into PROPERTYNAME and FILTER parameters


@pytest.mark.skip()
def test_ogr_wfs_ionic_sql():

    if not gdaltest.ionic_wfs:
        pytest.skip()

    ds = ogr.Open('WFS:http://webservices.ionicsoft.com/ionicweb/wfs/BOSTON_ORA')
    assert ds is not None

    lyr = ds.ExecuteSQL("SELECT name FROM \"wfs:BUSINESS\" WHERE total_employees = 105")
    count = lyr.GetFeatureCount()

    ds.ReleaseResultSet(lyr)

    assert count == 1

###############################################################################
# Test opening a datasource from a XML description file
# The following test should issue 0 WFS http request


def test_ogr_wfs_xmldescriptionfile():

    ds = ogr.Open('data/wfs/testwfs.xml')
    lyr = ds.GetLayer(0)
    feature_defn = lyr.GetLayerDefn()
    index = feature_defn.GetFieldIndex('name')
    sr = lyr.GetSpatialRef()

    assert index == 1

    wkt = sr.ExportToWkt()
    assert wkt.find('WGS 84') != -1

    layermetadata = ds.GetLayerByName('WFSLayerMetadata')
    count_layers = layermetadata.GetFeatureCount()
    assert count_layers == ds.GetLayerCount(), 'count_layers != ds.GetLayerCount()'

    getcapabilitieslayer = ds.GetLayerByName('WFSGetCapabilities')
    getcapabilitieslayer_feat = getcapabilitieslayer.GetNextFeature()
    getcapabilitieslayer_content = getcapabilitieslayer_feat.GetFieldAsString(0)
    assert getcapabilitieslayer_content.startswith('<WFS_Capabilities'), \
        'did not get expected result'

    ds = None

###############################################################################
# Test opening a datasource from a XML description file that has just the URL


@pytest.mark.skip('FIXME: re-enable after adapting test')
def test_ogr_wfs_xmldescriptionfile_to_be_updated():

    if not gdaltest.geoserver_wfs:
        pytest.skip()

    f = open('tmp/ogr_wfs_xmldescriptionfile_to_be_updated.xml', 'wt')
    f.write('<OGRWFSDataSource>\n')
    f.write('<URL>http://demo.opengeo.org/geoserver/wfs</URL>\n')
    f.write('</OGRWFSDataSource>\n')
    f.close()

    # Should only emit GetCapabilities and serialize it
    ds = ogr.Open('tmp/ogr_wfs_xmldescriptionfile_to_be_updated.xml')
    assert ds is not None
    ds = None

    f = open('tmp/ogr_wfs_xmldescriptionfile_to_be_updated.xml', 'rt')
    content = f.read()
    assert content.find('WFS_Capabilities') != -1, \
        'XML description file was not filled as expected'
    assert content.find('<OGRWFSLayer') == -1, \
        'XML description file was not filled as expected'
    f.close()

    # Should emit DescribeFeatureType and serialize its result
    ds = ogr.Open('tmp/ogr_wfs_xmldescriptionfile_to_be_updated.xml')
    assert ds is not None
    ds.GetLayerByName('za:za_points').GetLayerDefn()
    ds = None

    f = open('tmp/ogr_wfs_xmldescriptionfile_to_be_updated.xml', 'rt')
    content = f.read()
    assert content.find('<OGRWFSLayer name="za:za_points">') != -1, \
        'XML description file was not filled as expected'
    f.close()

    os.unlink('tmp/ogr_wfs_xmldescriptionfile_to_be_updated.xml')

###############################################################################
# Test opening a datasource directly from a GetCapabilities answer XML file
# The following test should issue 0 WFS http request


def test_ogr_wfs_getcapabilitiesfile():

    ds = ogr.Open('data/wfs/getcapabilities_wfs.xml')

    if ds is None:
        gdal.Unlink('data/wfs/getcapabilities_wfs.gfs')
        pytest.fail()

    ds = None

    gdal.Unlink('data/wfs/getcapabilities_wfs.gfs')

###############################################################################
# Test opening a datastore which only support GML 3.2.1 output


@pytest.mark.skip()
def test_ogr_wfs_deegree_gml321():

    ds = ogr.Open('WFS:http://demo.deegree.org:80/inspire-workspace/services/wfs?ACCEPTVERSIONS=1.1.0&MAXFEATURES=10')
    if ds is None:
        if gdaltest.gdalurlopen('http://demo.deegree.org:80/inspire-workspace/services/wfs?ACCEPTVERSIONS=1.1.0') is None:
            pytest.skip('cannot open URL')
        if gdal.GetLastErrorMsg().find("Unable to determine the subcontroller for request type 'GetCapabilities' and service type 'WFS'") != -1:
            pytest.skip()
        pytest.fail()

    lyr = ds.GetLayerByName("ad:Address")
    gdal.ErrorReset()
    lyr.GetFeatureCount()
    assert gdal.GetLastErrorMsg() == ''

###############################################################################
# Test WFS 2.0.0 support


@pytest.mark.skip()
def test_ogr_wfs_deegree_wfs200():

    ds = ogr.Open('WFS:http://demo.deegree.org:80/utah-workspace/services/wfs?ACCEPTVERSIONS=2.0.0')
    if ds is None:
        if gdaltest.gdalurlopen('http://demo.deegree.org:80/utah-workspace/services/wfs?ACCEPTVERSIONS=2.0.0') is None:
            pytest.skip('cannot open URL')
        pytest.fail()

    lyr = ds.GetLayerByName("app:SGID024_Municipalities2004_edited")
    lyr.SetAttributeFilter('OBJECTID = 5')
    count = lyr.GetFeatureCount()
    if count != 1:
        if gdal.GetLastErrorMsg().find('HTTP error code : 500') < 0:
            print(count)
            pytest.fail("OBJECTID = 5 filter failed")
    else:
        feat = lyr.GetNextFeature()
        if feat.GetFieldAsInteger('OBJECTID') != 5:
            feat.DumpReadable()
            pytest.fail("OBJECTID = 5 filter failed")

    lyr.SetAttributeFilter("gml_id = 'SGID024_MUNICIPALITIES2004_EDITED_5'")
    count = lyr.GetFeatureCount()
    if count != 1:
        # FIXME ! Avoid failure on ogr_wfs_deegree_wfs200 (the server is likely buggy since it worked before, but no longer whereas the WFS client code hasn't changed)
        print("gml_id = 'SGID024_MUNICIPALITIES2004_EDITED_5' filter failed")
        # gdaltest.post_reason("gml_id = 'SGID024_MUNICIPALITIES2004_EDITED_5' filter failed")
        # print(count)
        # return 'fail'
    else:
        feat = lyr.GetNextFeature()
        if feat.GetFieldAsInteger('OBJECTID') != 6:
            feat.DumpReadable()
            pytest.fail("gml_id = 'SGID024_MUNICIPALITIES2004_EDITED_5' filter failed")

    lyr.SetAttributeFilter(None)
    lyr.SetSpatialFilterRect(-1e8, -1e8, 1e8, 1e8)
    spatialfiltercount = lyr.GetFeatureCount()
    lyr.SetSpatialFilter(None)
    allcount = lyr.GetFeatureCount()
    assert allcount == spatialfiltercount and allcount != 0, \
        'spatialfiltercount != allcount'

###############################################################################
# Test WFS SORTBY support


@pytest.mark.skip()
def test_ogr_wfs_deegree_sortby():

    ds = ogr.Open('WFS:http://demo.deegree.org:80/utah-workspace/services/wfs?MAXFEATURES=10&VERSION=1.1.0')
    if ds is None:
        if gdaltest.gdalurlopen('http://demo.deegree.org:80/utah-workspace/services/wfs') is None:
            pytest.skip('cannot open URL')
        pytest.fail()

    lyr = ds.ExecuteSQL("SELECT * FROM \"app:SGID024_Municipalities2004_edited\" ORDER BY OBJECTID DESC")

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsInteger('OBJECTID') != 240:
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsInteger('OBJECTID') != 239:
        feat.DumpReadable()
        pytest.fail()

    ds.ReleaseResultSet(lyr)

###############################################################################


def ogr_wfs_get_multiple_layer_defn(url):

    if not gdaltest.run_slow_tests():
        pytest.skip()

    ds = ogr.Open('WFS:' + url)
    if ds is None:
        if gdaltest.gdalurlopen(url) is None:
            pytest.skip('cannot open URL')
        pytest.fail()

    # This should be slow only for the first layer
    for i in range(0, ds.GetLayerCount()):
        lyr = ds.GetLayer(i)
        print('Layer %s has %d fields' % (lyr.GetName(), lyr.GetLayerDefn().GetFieldCount()))

    
###############################################################################
# Test a ESRI server


@pytest.mark.skip()
def test_ogr_wfs_esri():
    return ogr_wfs_get_multiple_layer_defn('http://map.ngdc.noaa.gov/wfsconnector/com.esri.wfs.Esrimap/dart_atlantic_f')

###############################################################################
# Test a ESRI server


def test_ogr_wfs_esri_2():
    return ogr_wfs_get_multiple_layer_defn('http://sentinel.ga.gov.au/wfsconnector/com.esri.wfs.Esrimap')

###############################################################################
# Test a CubeWerx server


def test_ogr_wfs_cubewerx():
    return ogr_wfs_get_multiple_layer_defn('http://portal.cubewerx.com/cubewerx/cubeserv/cubeserv.cgi?CONFIG=haiti_vgi&DATASTORE=vgi')

###############################################################################
# Test a TinyOWS server


def test_ogr_wfs_tinyows():
    return ogr_wfs_get_multiple_layer_defn('http://www.tinyows.org/cgi-bin/tinyows')

###############################################################################
# Test a ERDAS Apollo server


def test_ogr_wfs_erdas_apollo():
    return ogr_wfs_get_multiple_layer_defn('http://apollo.erdas.com/erdas-apollo/vector/Cherokee')

###############################################################################
# Test a Integraph server


def test_ogr_wfs_intergraph():
    return ogr_wfs_get_multiple_layer_defn('http://ideg.xunta.es/WFS_POL/request.aspx')

###############################################################################
# Test a MapInfo server


def test_ogr_wfs_mapinfo():
    return ogr_wfs_get_multiple_layer_defn('http://www.mapinfo.com/miwfs')


###############################################################################


def test_ogr_wfs_vsimem_fail_because_not_enabled(with_and_without_streaming):

    gdal.PushErrorHandler()
    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    gdal.PopErrorHandler()
    assert ds is None


###############################################################################
def test_ogr_wfs_vsimem_fail_because_no_get_capabilities(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.PushErrorHandler()
    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################


def test_ogr_wfs_vsimem_fail_because_empty_response(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&REQUEST=GetCapabilities',
                           '')
    gdal.PushErrorHandler()
    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    gdal.PopErrorHandler()
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Empty content returned by server') >= 0

###############################################################################


def test_ogr_wfs_vsimem_fail_because_no_WFS_Capabilities(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&REQUEST=GetCapabilities',
                           '<foo/>')
    gdal.PushErrorHandler()
    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    gdal.PopErrorHandler()
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Cannot find <WFS_Capabilities>') >= 0

###############################################################################


def test_ogr_wfs_vsimem_fail_because_exception(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&REQUEST=GetCapabilities',
                           '<ServiceExceptionReport/>')
    gdal.PushErrorHandler()
    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    gdal.PopErrorHandler()
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Error returned by server : <ServiceExceptionReport/>') >= 0

###############################################################################


def test_ogr_wfs_vsimem_fail_because_invalid_xml_capabilities(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&REQUEST=GetCapabilities',
                           '<invalid_xml')
    gdal.PushErrorHandler()
    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    gdal.PopErrorHandler()
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Invalid XML content : <invalid_xml') >= 0

###############################################################################


def test_ogr_wfs_vsimem_fail_because_missing_featuretypelist(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities>
</WFS_Capabilities>
""")
    gdal.PushErrorHandler()
    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    gdal.PopErrorHandler()
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Cannot find <FeatureTypeList>') >= 0

###############################################################################


def test_ogr_wfs_vsimem_wfs110_open_getcapabilities_file(with_and_without_streaming):

    gdal.FileFromMemBuffer('/vsimem/caps.xml',
                           """<WFS_Capabilities
""")

    gdal.PushErrorHandler()
    ds = ogr.Open('/vsimem/caps.xml')
    gdal.PopErrorHandler()
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Parse error at EOF, not all elements have been closed') >= 0

    gdal.FileFromMemBuffer('/vsimem/caps.xml',
                           """<foo><WFS_Capabilities/></foo>
""")

    gdal.PushErrorHandler()
    ds = ogr.Open('/vsimem/caps.xml')
    gdal.PopErrorHandler()
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Cannot find <WFS_Capabilities>') >= 0

    gdal.FileFromMemBuffer('/vsimem/caps.xml',
                           """<WFS_Capabilities version="1.1.0">
    <FeatureTypeList>
        <FeatureType/>
        <FeatureType>
            <Name>my_layer</Name>
        </FeatureType>
    </FeatureTypeList>
</WFS_Capabilities>
""")

    gdal.PushErrorHandler()
    ds = ogr.Open('/vsimem/caps.xml')
    gdal.PopErrorHandler()
    assert ds is None
    assert gdal.GetLastErrorMsg().find('Cannot find base URL') >= 0

    gdal.FileFromMemBuffer('/vsimem/caps.xml',
                           """<WFS_Capabilities version="1.1.0">
    <ows:OperationsMetadata>
        <ows:Operation name="GetCapabilities">
            <ows:DCP><ows:HTTP>
                <ows:Get xlink:href="/vsimem/foo"/>
                <ows:Post xlink:href="/vsimem/foo"/>
            </ows:HTTP></ows:DCP>
        </ows:Operation>
    </ows:OperationsMetadata>
    <FeatureTypeList>
        <FeatureType/>
        <FeatureType>
            <Name>my_layer</Name>
        </FeatureType>
    </FeatureTypeList>
</WFS_Capabilities>
""")

    ds = ogr.Open('/vsimem/caps.xml')
    assert ds is not None
    assert ds.GetLayerCount() == 1

###############################################################################


def test_ogr_wfs_vsimem_wfs110_minimal_instance(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    # Invalid response, but enough for use
    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&REQUEST=GetCapabilities',
                           """
<WFS_Capabilities version="1.1.0">
    <ows:ServiceIdentification>
      <ows:Title>LDS Testing</ows:Title>
    </ows:ServiceIdentification>
    <FeatureTypeList/>
</WFS_Capabilities>
""")
    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    assert ds is not None
    assert ds.GetLayerCount() == 0

    assert ds.GetMetadataDomainList() == ['', 'xml:capabilities']
    assert ds.GetMetadata() == {'TITLE': 'LDS Testing'}
    assert len(ds.GetMetadata_List("xml:capabilities")) == 1

    gdal.PushErrorHandler()
    ds = ogr.Open('WFS:/vsimem/wfs_endpoint', update=1)
    gdal.PopErrorHandler()
    assert ds is None

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_missing_describefeaturetype(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    # Invalid response, but enough for use
    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="1.1.0">
    <FeatureTypeList>
        <FeatureType/>
        <FeatureType>
            <Name>my_layer</Name>
        </FeatureType>
    </FeatureTypeList>
</WFS_Capabilities>
""")
    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    assert ds is not None
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'my_layer'

    # Missing DescribeFeatureType
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr_defn = lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    assert lyr_defn.GetFieldCount() == 0

    lyr_defn = lyr.GetLayerDefn()

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_invalid_describefeaturetype(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=my_layer',
                           """<invalid_xml
""")

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr_defn = lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    assert lyr_defn.GetFieldCount() == 0

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_describefeaturetype_missing_schema(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=my_layer',
                           """<missing_schema/>
""")

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr_defn = lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    assert lyr_defn.GetFieldCount() == 0

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_describefeaturetype(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=my_layer',
                           """<xsd:schema xmlns:foo="http://foo" xmlns:gml="http://www.opengis.net/gml" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
  <xsd:import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.1.1/base/gml.xsd"/>
  <xsd:complexType name="my_layerType">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="boolean" nillable="true" type="xsd:boolean"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="short" nillable="true" type="xsd:short"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="int" nillable="true" type="xsd:int"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="float" nillable="true" type="xsd:float"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="double" nillable="true" type="xsd:double"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="dt" nillable="true" type="xsd:dateTime"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="shape" nillable="true" type="gml:PointPropertyType"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="my_layer" substitutionGroup="gml:_Feature" type="foo:my_layerType"/>
</xsd:schema>
""")

    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 8
    assert lyr_defn.GetGeomFieldCount() == 1

    ds = gdal.OpenEx('WFS:/vsimem/wfs_endpoint', open_options=['EXPOSE_GML_ID=NO'])
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 7

    gdal.SetConfigOption('GML_EXPOSE_GML_ID', 'YES')
    ds = gdal.OpenEx('WFS:/vsimem/wfs_endpoint', open_options=['EXPOSE_GML_ID=NO'])
    gdal.SetConfigOption('GML_EXPOSE_GML_ID', None)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 7

    gdal.SetConfigOption('GML_EXPOSE_GML_ID', 'NO')
    ds = gdal.OpenEx('WFS:/vsimem/wfs_endpoint', open_options=['EXPOSE_GML_ID=YES'])
    gdal.SetConfigOption('GML_EXPOSE_GML_ID', None)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 8

    gdal.SetConfigOption('GML_EXPOSE_GML_ID', 'NO')
    ds = gdal.OpenEx('WFS:/vsimem/wfs_endpoint')
    gdal.SetConfigOption('GML_EXPOSE_GML_ID', None)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 7


###############################################################################
def test_ogr_wfs_vsimem_wfs110_one_layer_xmldescriptionfile_to_be_updated(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/ogr_wfs_xmldescriptionfile_to_be_updated.xml',
                           """<OGRWFSDataSource>
    <URL>/vsimem/wfs_endpoint</URL>
</OGRWFSDataSource>""")

    ds = ogr.Open('/vsimem/ogr_wfs_xmldescriptionfile_to_be_updated.xml')
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'my_layer'
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_wfs_xmldescriptionfile_to_be_updated.xml', 'rb')
    data = gdal.VSIFReadL(1, 100000, f).decode('ascii')
    gdal.VSIFCloseL(f)
    assert data == """<OGRWFSDataSource>
  <URL>/vsimem/wfs_endpoint</URL>
  <WFS_Capabilities version="1.1.0">
    <FeatureTypeList>
      <FeatureType />
      <FeatureType>
        <Name>my_layer</Name>
      </FeatureType>
    </FeatureTypeList>
  </WFS_Capabilities>
</OGRWFSDataSource>
"""

    ds = ogr.Open('/vsimem/ogr_wfs_xmldescriptionfile_to_be_updated.xml')
    lyr = ds.GetLayer(0)
    lyr.GetLayerDefn()
    ds = None

    f = gdal.VSIFOpenL('/vsimem/ogr_wfs_xmldescriptionfile_to_be_updated.xml', 'rb')
    data = gdal.VSIFReadL(1, 100000, f).decode('ascii')
    gdal.VSIFCloseL(f)
    assert data == """<OGRWFSDataSource>
  <URL>/vsimem/wfs_endpoint</URL>
  <WFS_Capabilities version="1.1.0">
    <FeatureTypeList>
      <FeatureType />
      <FeatureType>
        <Name>my_layer</Name>
      </FeatureType>
    </FeatureTypeList>
  </WFS_Capabilities>
  <OGRWFSLayer name="my_layer">
    <schema foo="http://foo" gml="http://www.opengis.net/gml" xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
      <import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.1.1/base/gml.xsd" />
      <complexType name="my_layerType">
        <complexContent>
          <extension base="gml:AbstractFeatureType">
            <sequence>
              <element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string" />
              <element maxOccurs="1" minOccurs="0" name="boolean" nillable="true" type="xsd:boolean" />
              <element maxOccurs="1" minOccurs="0" name="short" nillable="true" type="xsd:short" />
              <element maxOccurs="1" minOccurs="0" name="int" nillable="true" type="xsd:int" />
              <element maxOccurs="1" minOccurs="0" name="float" nillable="true" type="xsd:float" />
              <element maxOccurs="1" minOccurs="0" name="double" nillable="true" type="xsd:double" />
              <element maxOccurs="1" minOccurs="0" name="dt" nillable="true" type="xsd:dateTime" />
              <element maxOccurs="1" minOccurs="0" name="shape" nillable="true" type="gml:PointPropertyType" />
            </sequence>
          </extension>
        </complexContent>
      </complexType>
      <element name="my_layer" substitutionGroup="gml:_Feature" type="foo:my_layerType" />
    </schema>
  </OGRWFSLayer>
</OGRWFSDataSource>
"""

    gdal.FileFromMemBuffer('/vsimem/ogr_wfs_xmldescriptionfile_to_be_updated.xml',
                           """<OGRWFSDataSource>
  <URL>/vsimem/wfs_endpoint</URL>
  <WFS_Capabilities version="1.1.0">
    <FeatureTypeList>
      <FeatureType />
      <FeatureType>
        <Name>my_layer</Name>
      </FeatureType>
    </FeatureTypeList>
  </WFS_Capabilities>
  <OGRWFSLayer name="my_layer">
    <schema foo="http://foo" gml="http://www.opengis.net/gml" xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
      <import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.1.1/base/gml.xsd" />
      <complexType name="my_layerType">
        <complexContent>
          <extension base="gml:AbstractFeatureType">
            <sequence>
              <element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string" />
            </sequence>
          </extension>
        </complexContent>
      </complexType>
      <element name="my_layer" substitutionGroup="gml:_Feature" type="foo:my_layerType" />
    </schema>
  </OGRWFSLayer>
</OGRWFSDataSource>""")

    ds = ogr.Open('/vsimem/ogr_wfs_xmldescriptionfile_to_be_updated.xml')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    ds = None

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_missing_getfeaturecount_no_hits(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    count = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    assert count == 0

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_missing_getfeaturecount_with_hits(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="1.1.0">
    <OperationsMetadata>
        <ows:Operation name="GetFeature">
            <ows:Parameter name="resultType">
                <ows:Value>results</ows:Value>
                <ows:Value>hits</ows:Value>
            </ows:Parameter>
        </ows:Operation>
    </OperationsMetadata>
    <FeatureTypeList>
        <FeatureType>
            <Name>my_layer</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
    </FeatureTypeList>
</WFS_Capabilities>
""")

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    count = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    assert count == 0

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_invalid_getfeaturecount_with_hits(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&RESULTTYPE=hits',
                           """<invalid_xml""")

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    count = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    assert count == 0

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_getfeaturecount_with_hits_missing_FeatureCollection(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&RESULTTYPE=hits',
                           """<dummy_xml/>""")

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    count = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    assert count == 0

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_getfeaturecount_with_hits_invalid_xml(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&RESULTTYPE=hits',
                           """<invalid_xml""")

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    count = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    assert count == 0

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_getfeaturecount_with_hits_ServiceExceptionReport(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&RESULTTYPE=hits',
                           """<ServiceExceptionReport/>""")

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    count = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    assert count == 0


###############################################################################
def test_ogr_wfs_vsimem_wfs110_one_layer_getfeaturecount_with_hits_missing_numberOfFeatures(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&RESULTTYPE=hits',
                           """<FeatureCollection/>""")

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    count = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    assert count == 0

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_getfeaturecount_with_hits(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&RESULTTYPE=hits',
                           """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:foo="http://foo"
xmlns:wfs="http://www.opengis.net/wfs"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberOfFeatures="1"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer
                    http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd">
</wfs:FeatureCollection>""")

    count = lyr.GetFeatureCount()
    assert count == 1

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_missing_getfeature(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    assert f is None

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_invalid_getfeature(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer',
                           """<invalid_xml
""")

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    assert f is None

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_exception_getfeature(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer',
                           """<ServiceExceptionReport/>
""")

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg().find('Error returned by server') >= 0
    assert f is None

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_getfeature(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="1.1.0">
    <FeatureTypeList>
        <FeatureType>
            <Name>my_layer</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-170.0 -80.0</ows:LowerCorner>
                <ows:UpperCorner>170.0 80.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
    </FeatureTypeList>
</WFS_Capabilities>
""")
    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer',
                           """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:foo="http://foo"
xmlns:wfs="http://www.opengis.net/wfs"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberOfFeatures="1"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer
                    http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd">
    <gml:featureMembers>
        <foo:my_layer gml:id="my_layer.1">
            <foo:str>str</foo:str>
            <foo:boolean>true</foo:boolean>
            <foo:short>1</foo:short>
            <foo:int>123456789</foo:int>
            <foo:float>1.2</foo:float>
            <foo:double>1.23</foo:double>
            <foo:dt>2015-04-17T12:34:56Z</foo:dt>
            <foo:shape>
                <gml:Point srsDimension="2" srsName="urn:ogc:def:crs:EPSG::4326">
                    <gml:pos>49 2</gml:pos>
                </gml:Point>
            </foo:shape>
        </foo:my_layer>
    </gml:featureMembers>
</wfs:FeatureCollection>
""")

    f = lyr.GetNextFeature()
    if f.gml_id != 'my_layer.1' or f.boolean != 1 or f.str != 'str' or f.short != 1 or \
       f.int != 123456789 or f.float != 1.2 or f.double != 1.23 or f.dt != '2015/04/17 12:34:56+00' or \
       f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()

    sql_lyr = ds.ExecuteSQL('SELECT * FROM my_layer')
    f = sql_lyr.GetNextFeature()
    if f.gml_id != 'my_layer.1':
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_getextent(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)
    assert lyr.GetExtent() == (2, 2, 49, 49)

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_getextent_without_getfeature(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.Unlink('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler()
    extent = lyr.GetExtent()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    assert extent == (0, 0, 0, 0)

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_getextent_optimized(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="1.1.0">
    <FeatureTypeList>
        <FeatureType>
            <Name>my_layer</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
        <FeatureType>
            <Name>my_layer2</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-170.0 -80.0</ows:LowerCorner>
                <ows:UpperCorner>170.0 80.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
        <FeatureType>
            <Name>my_layer3</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::3857</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -85.0511287798065</ows:LowerCorner>
                <ows:UpperCorner>180.0 85.0511287798065</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
        <FeatureType>
            <Name>my_layer4</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::3857</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90</ows:LowerCorner>
                <ows:UpperCorner>180.0 90</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
    </FeatureTypeList>
  <ogc:Filter_Capabilities>
    <ogc:Scalar_Capabilities>
      <ogc:ArithmeticOperators>
        <ogc:SimpleArithmetic/>
        <ogc:Functions>
            <ogc:FunctionNames>
                <ogc:FunctionName nArgs="1">abs_4</ogc:FunctionName> <!-- geoserver "signature" -->
            </ogc:FunctionNames>
        </ogc:Functions>
      </ogc:ArithmeticOperators>
    </ogc:Scalar_Capabilities>
  </ogc:Filter_Capabilities>
</WFS_Capabilities>
""")

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)
    assert lyr.GetExtent() == (-180.0, 180.0, -90.0, 90.0)

    lyr = ds.GetLayer(1)
    gdal.PushErrorHandler()
    got_extent = lyr.GetExtent()
    gdal.PopErrorHandler()
    assert got_extent == (0.0, 0.0, 0.0, 0.0)

    ds = gdal.OpenEx('WFS:/vsimem/wfs_endpoint', open_options=['TRUST_CAPABILITIES_BOUNDS=YES'])
    lyr = ds.GetLayer(1)
    assert lyr.GetExtent() == (-170.0, 170.0, -80.0, 80.0)

    gdal.SetConfigOption('OGR_WFS_TRUST_CAPABILITIES_BOUNDS', 'YES')
    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    gdal.SetConfigOption('OGR_WFS_TRUST_CAPABILITIES_BOUNDS', None)

    lyr = ds.GetLayer(2)
    expected_extent = (-20037508.342789248, 20037508.342789248, -20037508.342789154, 20037508.342789147)
    got_extent = lyr.GetExtent()
    for i in range(4):
        assert expected_extent[i] == pytest.approx(got_extent[i], abs=1e-5)

    
###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_getfeature_ogr_getfeature(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="1.1.0">
    <FeatureTypeList>
        <FeatureType>
            <Name>my_layer</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
    </FeatureTypeList>
  <ogc:Filter_Capabilities>
    <ogc:Spatial_Capabilities>
      <ogc:GeometryOperands>
        <ogc:GeometryOperand>gml:Envelope</ogc:GeometryOperand>
        <ogc:GeometryOperand>gml:Point</ogc:GeometryOperand>
        <ogc:GeometryOperand>gml:LineString</ogc:GeometryOperand>
        <ogc:GeometryOperand>gml:Polygon</ogc:GeometryOperand>
      </ogc:GeometryOperands>
      <ogc:SpatialOperators>
        <ogc:SpatialOperator name="Disjoint"/>
        <ogc:SpatialOperator name="Equals"/>
        <ogc:SpatialOperator name="DWithin"/>
        <ogc:SpatialOperator name="Beyond"/>
        <ogc:SpatialOperator name="Intersects"/>
        <ogc:SpatialOperator name="Touches"/>
        <ogc:SpatialOperator name="Crosses"/>
        <ogc:SpatialOperator name="Within"/>
        <ogc:SpatialOperator name="Contains"/>
        <ogc:SpatialOperator name="Overlaps"/>
        <ogc:SpatialOperator name="BBOX"/>
      </ogc:SpatialOperators>
    </ogc:Spatial_Capabilities>
    <ogc:Scalar_Capabilities>
      <ogc:LogicalOperators/>
      <ogc:ComparisonOperators>
        <ogc:ComparisonOperator>LessThan</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>GreaterThan</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>LessThanEqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>GreaterThanEqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>EqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>NotEqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>Like</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>Between</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>NullCheck</ogc:ComparisonOperator>
      </ogc:ComparisonOperators>
      <ogc:ArithmeticOperators>
        <ogc:SimpleArithmetic/>
        <ogc:Functions/>
      </ogc:ArithmeticOperators>
    </ogc:Scalar_Capabilities>
    <ogc:Id_Capabilities>
      <ogc:FID/>
      <ogc:EID/>
    </ogc:Id_Capabilities>
  </ogc:Filter_Capabilities>
</WFS_Capabilities>
""")

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Fogc%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%22%3E%3CGmlObjectId%20id%3D%22my_layer.100%22%2F%3E%3C%2FFilter%3E',
                           """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:foo="http://foo"
xmlns:wfs="http://www.opengis.net/wfs"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberOfFeatures="1"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer
                    http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd">
    <gml:featureMembers>
        <foo:my_layer gml:id="my_layer.100">
        </foo:my_layer>
    </gml:featureMembers>
</wfs:FeatureCollection>
""")

    f = lyr.GetFeature(100)
    if f.gml_id != 'my_layer.100':
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_filter_gml_id_failed(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer',
                           """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:foo="http://foo"
xmlns:wfs="http://www.opengis.net/wfs"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberOfFeatures="0"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer
                    http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd">
</wfs:FeatureCollection>
""")

    lyr.SetAttributeFilter("gml_id = 'my_layer.1'")

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''
    assert f is None

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_filter_gml_id_success(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Fogc%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%22%3E%3CGmlObjectId%20id%3D%22my_layer.1%22%2F%3E%3CGmlObjectId%20id%3D%22my_layer.1%22%2F%3E%3C%2FFilter%3E',
                           """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:foo="http://foo"
xmlns:wfs="http://www.opengis.net/wfs"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberOfFeatures="1"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer
                    http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd">
    <gml:featureMembers>
        <foo:my_layer gml:id="my_layer.1">
            <foo:str>str</foo:str>
            <foo:boolean>true</foo:boolean>
            <foo:short>1</foo:short>
            <foo:int>123456789</foo:int>
            <foo:float>1.2</foo:float>
            <foo:double>1.23</foo:double>
            <foo:dt>2015-04-17T12:34:56Z</foo:dt>
            <foo:shape>
                <gml:Point srsDimension="2" srsName="urn:ogc:def:crs:EPSG::4326">
                    <gml:pos>49 2</gml:pos>
                </gml:Point>
            </foo:shape>
        </foo:my_layer>
    </gml:featureMembers>
</wfs:FeatureCollection>
""")

    lyr.SetAttributeFilter("gml_id = 'my_layer.1' OR gml_id = 'my_layer.1'")

    f = lyr.GetNextFeature()
    assert f is not None

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_filter(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Fogc%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%22%3E%3COr%3E%3COr%3E%3COr%3E%3CAnd%3E%3CPropertyIsEqualTo%3E%3CPropertyName%3Estr%3C%2FPropertyName%3E%3CLiteral%3Estr%3C%2FLiteral%3E%3C%2FPropertyIsEqualTo%3E%3CPropertyIsEqualTo%3E%3CPropertyName%3Eshort%3C%2FPropertyName%3E%3CLiteral%3E1%3C%2FLiteral%3E%3C%2FPropertyIsEqualTo%3E%3C%2FAnd%3E%3CPropertyIsLike%20wildCard%3D%27%2A%27%20singleChar%3D%27_%27%20escapeChar%3D%27%21%27%20matchCase%3D%27true%27%3E%3CPropertyName%3Estr%3C%2FPropertyName%3E%3CLiteral%3Est%2A%3C%2FLiteral%3E%3C%2FPropertyIsLike%3E%3C%2FOr%3E%3COr%3E%3CNot%3E%3CPropertyIsNull%3E%3CPropertyName%3Eboolean%3C%2FPropertyName%3E%3C%2FPropertyIsNull%3E%3C%2FNot%3E%3CPropertyIsGreaterThan%3E%3CPropertyName%3Eint%3C%2FPropertyName%3E%3CLiteral%3E1%3C%2FLiteral%3E%3C%2FPropertyIsGreaterThan%3E%3C%2FOr%3E%3C%2FOr%3E%3COr%3E%3COr%3E%3CPropertyIsGreaterThanOrEqualTo%3E%3CPropertyName%3Eint%3C%2FPropertyName%3E%3CLiteral%3E1%3C%2FLiteral%3E%3C%2FPropertyIsGreaterThanOrEqualTo%3E%3CPropertyIsNotEqualTo%3E%3CPropertyName%3Eint%3C%2FPropertyName%3E%3CLiteral%3E2%3C%2FLiteral%3E%3C%2FPropertyIsNotEqualTo%3E%3C%2FOr%3E%3COr%3E%3CPropertyIsLessThan%3E%3CPropertyName%3Eint%3C%2FPropertyName%3E%3CLiteral%3E2000000000%3C%2FLiteral%3E%3C%2FPropertyIsLessThan%3E%3CPropertyIsLessThanOrEqualTo%3E%3CPropertyName%3Eint%3C%2FPropertyName%3E%3CLiteral%3E2000000000%3C%2FLiteral%3E%3C%2FPropertyIsLessThanOrEqualTo%3E%3C%2FOr%3E%3C%2FOr%3E%3C%2FOr%3E%3C%2FFilter%3E',
                           """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:foo="http://foo"
xmlns:wfs="http://www.opengis.net/wfs"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberOfFeatures="1"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer
                    http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd">
    <gml:featureMembers>
        <foo:my_layer gml:id="my_layer.1">
            <foo:str>str</foo:str>
            <foo:boolean>true</foo:boolean>
            <foo:short>1</foo:short>
            <foo:int>123456789</foo:int>
            <foo:float>1.2</foo:float>
            <foo:double>1.23</foo:double>
            <foo:dt>2015-04-17T12:34:56Z</foo:dt>
            <foo:shape>
                <gml:Point srsDimension="2" srsName="urn:ogc:def:crs:EPSG::4326">
                    <gml:pos>49 2</gml:pos>
                </gml:Point>
            </foo:shape>
        </foo:my_layer>
    </gml:featureMembers>
</wfs:FeatureCollection>
""")

    lyr.SetAttributeFilter("(str = 'str' AND short = 1 AND float = 1.2) OR str LIKE 'st%' OR boolean IS NOT NULL OR int > 1 OR int >= 1 or int != 2 or int < 2000000000 or int <= 2000000000")

    f = lyr.GetNextFeature()
    assert f is not None

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_filter_spatial_ops(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    content = """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:foo="http://foo"
xmlns:wfs="http://www.opengis.net/wfs"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberOfFeatures="1"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer
                    http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd">
    <gml:featureMembers>
        <foo:my_layer gml:id="my_layer.1">
            <foo:str>str</foo:str>
            <foo:boolean>true</foo:boolean>
            <foo:short>1</foo:short>
            <foo:int>123456789</foo:int>
            <foo:float>1.2</foo:float>
            <foo:double>1.23</foo:double>
            <foo:dt>2015-04-17T12:34:56Z</foo:dt>
            <foo:shape>
                <gml:Point srsDimension="2" srsName="urn:ogc:def:crs:EPSG::4326">
                    <gml:pos>49 2</gml:pos>
                </gml:Point>
            </foo:shape>
        </foo:my_layer>
    </gml:featureMembers>
</wfs:FeatureCollection>
"""

    # Invalid syntax
    gdal.PushErrorHandler()
    ret = lyr.SetAttributeFilter("ST_Intersects(shape)")
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Wrong number of arguments for ST_Intersects') < 0)

    gdal.PushErrorHandler()
    ret = lyr.SetAttributeFilter("ST_Intersects(shape, 5)")
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Wrong field type for argument 2 of ST_Intersects') < 0)

    gdal.PushErrorHandler()
    ret = lyr.SetAttributeFilter("ST_Intersects(shape, ST_MakeEnvelope(1))")
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Wrong number of arguments for ST_MakeEnvelope') < 0)

    gdal.PushErrorHandler()
    ret = lyr.SetAttributeFilter("ST_Intersects(shape, ST_MakeEnvelope(1,1,1,'a'))")
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Wrong field type for argument 4 of ST_MakeEnvelope') < 0)

    gdal.PushErrorHandler()
    ret = lyr.SetAttributeFilter("ST_Intersects(shape, ST_MakeEnvelope(1,1,1,1,3.5))")
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Wrong field type for argument 5 of ST_MakeEnvelope') < 0)

    gdal.PushErrorHandler()
    ret = lyr.SetAttributeFilter("ST_Intersects(shape, ST_MakeEnvelope(1,1,1,1,'not_a_srs'))")
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Wrong value for argument 5 of ST_MakeEnvelope') < 0)

    gdal.PushErrorHandler()
    ret = lyr.SetAttributeFilter("ST_Intersects(shape, ST_MakeEnvelope(1,1,1,1,-5))")
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Wrong value for argument 5 of ST_MakeEnvelope') < 0)

    gdal.PushErrorHandler()
    ret = lyr.SetAttributeFilter("ST_Intersects(shape, ST_GeomFromText(1,2,3))")
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Wrong number of arguments for ST_GeomFromText') < 0)

    gdal.PushErrorHandler()
    ret = lyr.SetAttributeFilter("ST_Intersects(shape, ST_GeomFromText(1))")
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Wrong field type for argument 1 of ST_GeomFromText') < 0)

    gdal.PushErrorHandler()
    ret = lyr.SetAttributeFilter("ST_Intersects(shape, ST_GeomFromText('INVALID_GEOM'))")
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Wrong value for argument 1 of ST_GeomFromText') < 0)

    gdal.PushErrorHandler()
    ret = lyr.SetAttributeFilter("ST_Intersects(shape, ST_GeomFromText('POINT(0 0)', 'invalid_srs'))")
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Wrong value for argument 2 of ST_GeomFromText') < 0)

    gdal.PushErrorHandler()
    ret = lyr.SetAttributeFilter("ST_DWithin(shape)")
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Wrong number of arguments for ST_DWithin') < 0)

    gdal.PushErrorHandler()
    ret = lyr.SetAttributeFilter("ST_DWithin(shape,'a',5)")
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Wrong field type for argument 2 of ST_DWithin') < 0)

    gdal.PushErrorHandler()
    ret = lyr.SetAttributeFilter("ST_DWithin(shape,shape,'a')")
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Wrong field type for argument 3 of ST_DWithin') < 0)

    # Now valid requests
    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Fogc%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%22%3E%3COr%3E%3COr%3E%3CIntersects%3E%3CPropertyName%3Eshape%3C%2FPropertyName%3E%3Cgml:Envelope%20srsName%3D%22urn:ogc:def:crs:EPSG::4326%22%3E%3Cgml:lowerCorner%3E48.5%201.5%3C%2Fgml:lowerCorner%3E%3Cgml:upperCorner%3E49.5%202.5%3C%2Fgml:upperCorner%3E%3C%2Fgml:Envelope%3E%3C%2FIntersects%3E%3CIntersects%3E%3CPropertyName%3Eshape%3C%2FPropertyName%3E%3Cgml:Envelope%20srsName%3D%22urn:ogc:def:crs:EPSG::4326%22%3E%3Cgml:lowerCorner%3E48.5%201.5%3C%2Fgml:lowerCorner%3E%3Cgml:upperCorner%3E49.5%202.5%3C%2Fgml:upperCorner%3E%3C%2Fgml:Envelope%3E%3C%2FIntersects%3E%3C%2FOr%3E%3COr%3E%3CIntersects%3E%3CPropertyName%3Eshape%3C%2FPropertyName%3E%3Cgml:Envelope%20srsName%3D%22EPSG:4326%22%3E%3Cgml:lowerCorner%3E1.5%2048.5%3C%2Fgml:lowerCorner%3E%3Cgml:upperCorner%3E2.5%2049.5%3C%2Fgml:upperCorner%3E%3C%2Fgml:Envelope%3E%3C%2FIntersects%3E%3CIntersects%3E%3CPropertyName%3Eshape%3C%2FPropertyName%3E%3Cgml:Envelope%20srsName%3D%22urn:ogc:def:crs:EPSG::32630%22%3E%3Cgml:lowerCorner%3E380000%205370000%3C%2Fgml:lowerCorner%3E%3Cgml:upperCorner%3E470000%205490000%3C%2Fgml:upperCorner%3E%3C%2Fgml:Envelope%3E%3C%2FIntersects%3E%3C%2FOr%3E%3C%2FOr%3E%3C%2FFilter%3E',
                           content)

    lyr.SetAttributeFilter("ST_Intersects(shape, ST_MakeEnvelope(1.5,48.5,2.5,49.5)) OR " +
                           "ST_Intersects(shape, ST_MakeEnvelope(1.5,48.5,2.5,49.5, 4326)) OR " +
                           "ST_Intersects(shape, ST_MakeEnvelope(1.5,48.5,2.5,49.5, 'EPSG:4326')) OR " +
                           "ST_Intersects(shape, ST_MakeEnvelope(380000,5370000,470000,5490000,32630))")

    f = lyr.GetNextFeature()
    assert f is not None

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Fogc%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%22%3E%3COr%3E%3COr%3E%3CIntersects%3E%3CPropertyName%3Eshape%3C%2FPropertyName%3E%3Cgml:Polygon%20srsName%3D%22urn:ogc:def:crs:EPSG::4326%22%20gml:id%3D%22id1%22%3E%3Cgml:exterior%3E%3Cgml:LinearRing%3E%3Cgml:posList%3E48.5%201.5%2049.5%202.5%2049.5%202.5%2048.5%202.5%2048.5%201.5%3C%2Fgml:posList%3E%3C%2Fgml:LinearRing%3E%3C%2Fgml:exterior%3E%3C%2Fgml:Polygon%3E%3C%2FIntersects%3E%3CIntersects%3E%3CPropertyName%3Eshape%3C%2FPropertyName%3E%3Cgml:Polygon%20srsName%3D%22urn:ogc:def:crs:EPSG::4326%22%20gml:id%3D%22id2%22%3E%3Cgml:exterior%3E%3Cgml:LinearRing%3E%3Cgml:posList%3E48.5%201.5%2049.5%202.5%2049.5%202.5%2048.5%202.5%2048.5%201.5%3C%2Fgml:posList%3E%3C%2Fgml:LinearRing%3E%3C%2Fgml:exterior%3E%3C%2Fgml:Polygon%3E%3C%2FIntersects%3E%3C%2FOr%3E%3CIntersects%3E%3CPropertyName%3Eshape%3C%2FPropertyName%3E%3Cgml:Polygon%20srsName%3D%22EPSG:4326%22%20gml:id%3D%22id3%22%3E%3Cgml:exterior%3E%3Cgml:LinearRing%3E%3Cgml:posList%3E1.5%2048.5%202.5%2049.5%202.5%2049.5%202.5%2048.5%201.5%2048.5%3C%2Fgml:posList%3E%3C%2Fgml:LinearRing%3E%3C%2Fgml:exterior%3E%3C%2Fgml:Polygon%3E%3C%2FIntersects%3E%3C%2FOr%3E%3C%2FFilter%3E',
                           content)

    lyr.SetAttributeFilter("ST_Intersects(shape, ST_GeomFromText('POLYGON((1.5 48.5,2.5 49.5,2.5 49.5,2.5 48.5,1.5 48.5)))')) OR " +
                           "ST_Intersects(shape, ST_GeomFromText('POLYGON((1.5 48.5,2.5 49.5,2.5 49.5,2.5 48.5,1.5 48.5)))', 4326)) OR " +
                           "ST_Intersects(shape, ST_GeomFromText('POLYGON((1.5 48.5,2.5 49.5,2.5 49.5,2.5 48.5,1.5 48.5)))', 'EPSG:4326'))")

    f = lyr.GetNextFeature()
    assert f is not None

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Fogc%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%22%3E%3CDWithin%3E%3CPropertyName%3Eshape%3C%2FPropertyName%3E%3Cgml:Envelope%20srsName%3D%22urn:ogc:def:crs:EPSG::4326%22%3E%3Cgml:lowerCorner%3E48.5%201.5%3C%2Fgml:lowerCorner%3E%3Cgml:upperCorner%3E49.5%202.5%3C%2Fgml:upperCorner%3E%3C%2Fgml:Envelope%3E%3CDistance%20unit%3D%22m%22%3E5%3C%2FDistance%3E%3C%2FDWithin%3E%3C%2FFilter%3E',
                           content)

    lyr.SetAttributeFilter("ST_DWithin(shape,ST_MakeEnvelope(1.5,48.5,2.5,49.5),5)")

    f = lyr.GetNextFeature()
    assert f is not None

    sql_lyr = ds.ExecuteSQL("SELECT * FROM my_layer WHERE ST_Intersects(shape, ST_GeomFromText('POLYGON((1.5 48.5,2.5 49.5,2.5 49.5,2.5 48.5,1.5 48.5)))')) OR " +
                            "ST_Intersects(shape, ST_GeomFromText('POLYGON((1.5 48.5,2.5 49.5,2.5 49.5,2.5 48.5,1.5 48.5)))', 4326)) OR " +
                            "ST_Intersects(shape, ST_GeomFromText('POLYGON((1.5 48.5,2.5 49.5,2.5 49.5,2.5 48.5,1.5 48.5)))', 'EPSG:4326'))")
    f = sql_lyr.GetNextFeature()
    assert f is not None
    ds.ReleaseResultSet(sql_lyr)

    # Error case
    sql_lyr = ds.ExecuteSQL("SELECT ST_Intersects(shape, ST_GeomFromText('POLYGON((1.5 48.5,2.5 49.5,2.5 49.5,2.5 48.5,1.5 48.5))')) FROM my_layer")
    gdal.PushErrorHandler()
    f = sql_lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None
    ds.ReleaseResultSet(sql_lyr)

###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_spatial_filter(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Fogc%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%22%3E%3CBBOX%3E%3CPropertyName%3Eshape%3C%2FPropertyName%3E%3Cgml:Box%3E%3Cgml:coordinates%3E48.0000000000000000,1.0000000000000000%2050.0000000000000000,3.0000000000000000%3C%2Fgml:coordinates%3E%3C%2Fgml:Box%3E%3C%2FBBOX%3E%3C%2FFilter%3E',
                           """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:foo="http://foo"
xmlns:wfs="http://www.opengis.net/wfs"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberOfFeatures="1"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer
                    http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd">
    <gml:featureMembers>
        <foo:my_layer gml:id="my_layer.1">
            <foo:str>str</foo:str>
            <foo:boolean>true</foo:boolean>
            <foo:short>1</foo:short>
            <foo:int>123456789</foo:int>
            <foo:float>1.2</foo:float>
            <foo:double>1.23</foo:double>
            <foo:dt>2015-04-17T12:34:56Z</foo:dt>
            <foo:shape>
                <gml:Point srsDimension="2" srsName="urn:ogc:def:crs:EPSG::4326">
                    <gml:pos>49 2</gml:pos>
                </gml:Point>
            </foo:shape>
        </foo:my_layer>
    </gml:featureMembers>
</wfs:FeatureCollection>
""")

    lyr.SetSpatialFilterRect(1, 48, 3, 50)

    f = lyr.GetNextFeature()
    assert f is not None

    if gdal.GetConfigOption('OGR_WFS_USE_STREAMING') == 'NO':
        lyr.SetSpatialFilterRect(1.5, 48.5, 2.5, 49.5)
        f = lyr.GetNextFeature()
        assert f is not None

        lyr.SetSpatialFilter(None)
        lyr.ResetReading()

        lyr.ResetReading()
        lyr.SetSpatialFilterRect(1, 48, 3, 50)
        f = lyr.GetNextFeature()
        assert f is not None

    
###############################################################################


def test_ogr_wfs_vsimem_wfs110_one_layer_spatial_filter_and_attribute_filter(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Fogc%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%22%3E%3CAnd%3E%3CPropertyIsEqualTo%3E%3CPropertyName%3Estr%3C%2FPropertyName%3E%3CLiteral%3Estr%3C%2FLiteral%3E%3C%2FPropertyIsEqualTo%3E%3CBBOX%3E%3CPropertyName%3Eshape%3C%2FPropertyName%3E%3Cgml:Box%3E%3Cgml:coordinates%3E48.0000000000000000,1.0000000000000000%2050.0000000000000000,3.0000000000000000%3C%2Fgml:coordinates%3E%3C%2Fgml:Box%3E%3C%2FBBOX%3E%3C%2FAnd%3E%3C%2FFilter%3E',
                           """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:foo="http://foo"
xmlns:wfs="http://www.opengis.net/wfs"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberOfFeatures="1"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer
                    http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd">
    <gml:featureMembers>
        <foo:my_layer gml:id="my_layer.1">
            <foo:str>str</foo:str>
            <foo:boolean>true</foo:boolean>
            <foo:short>1</foo:short>
            <foo:int>123456789</foo:int>
            <foo:float>1.2</foo:float>
            <foo:double>1.23</foo:double>
            <foo:dt>2015-04-17T12:34:56Z</foo:dt>
            <foo:shape>
                <gml:Point srsDimension="2" srsName="urn:ogc:def:crs:EPSG::4326">
                    <gml:pos>49 2</gml:pos>
                </gml:Point>
            </foo:shape>
        </foo:my_layer>
    </gml:featureMembers>
</wfs:FeatureCollection>
""")

    lyr.SetSpatialFilterRect(1, 48, 3, 50)
    lyr.SetAttributeFilter("str = 'str'")

    f = lyr.GetNextFeature()
    assert f is not None

###############################################################################


def test_ogr_wfs_vsimem_wfs110_insertfeature(with_and_without_streaming):

    wfs_insert_url = None

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="1.1.0">
    <OperationsMetadata>
        <ows:Operation name="Transaction">
            <ows:DCP>
                <ows:HTTP>
                    <ows:Get xlink:href="/vsimem/wfs_endpoint"/>
                    <ows:Post xlink:href="/vsimem/wfs_endpoint"/>
                </ows:HTTP>
            </ows:DCP>
        </ows:Operation>
    </OperationsMetadata>
    <FeatureTypeList>
        <FeatureType>
            <Name>my_layer</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
    </FeatureTypeList>
  <ogc:Filter_Capabilities>
    <ogc:Spatial_Capabilities>
      <ogc:GeometryOperands>
        <ogc:GeometryOperand>gml:Envelope</ogc:GeometryOperand>
        <ogc:GeometryOperand>gml:Point</ogc:GeometryOperand>
        <ogc:GeometryOperand>gml:LineString</ogc:GeometryOperand>
        <ogc:GeometryOperand>gml:Polygon</ogc:GeometryOperand>
      </ogc:GeometryOperands>
      <ogc:SpatialOperators>
        <ogc:SpatialOperator name="Disjoint"/>
        <ogc:SpatialOperator name="Equals"/>
        <ogc:SpatialOperator name="DWithin"/>
        <ogc:SpatialOperator name="Beyond"/>
        <ogc:SpatialOperator name="Intersects"/>
        <ogc:SpatialOperator name="Touches"/>
        <ogc:SpatialOperator name="Crosses"/>
        <ogc:SpatialOperator name="Within"/>
        <ogc:SpatialOperator name="Contains"/>
        <ogc:SpatialOperator name="Overlaps"/>
        <ogc:SpatialOperator name="BBOX"/>
      </ogc:SpatialOperators>
    </ogc:Spatial_Capabilities>
    <ogc:Scalar_Capabilities>
      <ogc:LogicalOperators/>
      <ogc:ComparisonOperators>
        <ogc:ComparisonOperator>LessThan</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>GreaterThan</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>LessThanEqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>GreaterThanEqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>EqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>NotEqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>Like</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>Between</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>NullCheck</ogc:ComparisonOperator>
      </ogc:ComparisonOperators>
      <ogc:ArithmeticOperators>
        <ogc:SimpleArithmetic/>
        <ogc:Functions/>
      </ogc:ArithmeticOperators>
    </ogc:Scalar_Capabilities>
    <ogc:Id_Capabilities>
      <ogc:FID/>
      <ogc:EID/>
    </ogc:Id_Capabilities>
  </ogc:Filter_Capabilities>
</WFS_Capabilities>
""")

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint', update=1)
    lyr = ds.GetLayer(0)

    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    wfs_insert_url = """/vsimem/wfs_endpoint&POSTFIELDS=<?xml version="1.0"?>
<wfs:Transaction xmlns:wfs="http://www.opengis.net/wfs"
                 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                 service="WFS" version="1.1.0"
                 xmlns:gml="http://www.opengis.net/gml"
                 xmlns:ogc="http://www.opengis.net/ogc"
                 xsi:schemaLocation="http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer">
  <wfs:Insert>
    <feature:my_layer xmlns:feature="http://foo">
    </feature:my_layer>
  </wfs:Insert>
</wfs:Transaction>
"""
    gdal.FileFromMemBuffer(wfs_insert_url, "")

    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.FileFromMemBuffer(wfs_insert_url, "<invalid_xml")

    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.FileFromMemBuffer(wfs_insert_url, "<ServiceExceptionReport/>")

    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Error returned by server') < 0)

    gdal.FileFromMemBuffer(wfs_insert_url, "<dummy_xml/>")

    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Cannot find <TransactionResponse>') < 0)

    gdal.FileFromMemBuffer(wfs_insert_url,
                           """<TransactionResponse>
</TransactionResponse>
""")

    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.FileFromMemBuffer(wfs_insert_url,
                           """<TransactionResponse>
    <InsertResults>
        <Feature>
            <FeatureId/>
        </Feature>
    </InsertResults>
</TransactionResponse>
""")

    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.FileFromMemBuffer(wfs_insert_url,
                           """<TransactionResponse>
    <InsertResults>
        <Feature>
            <FeatureId fid="my_layer.100"/>
        </Feature>
    </InsertResults>
</TransactionResponse>
""")

    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL('SELECT _LAST_INSERTED_FIDS_ FROM not_existing_layer')
    gdal.PopErrorHandler()
    assert sql_lyr is None

    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    assert ret == 0
    assert f.GetFID() == 100

    sql_lyr = ds.ExecuteSQL('SELECT _LAST_INSERTED_FIDS_ FROM my_layer')
    got_f = sql_lyr.GetNextFeature()
    assert got_f is None
    ds.ReleaseResultSet(sql_lyr)

    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Cannot insert a feature when gml_id field is already set') < 0)

    # Empty StartTransaction + CommitTransaction
    ret = lyr.StartTransaction()
    assert ret == 0
    ret = lyr.CommitTransaction()
    assert ret == 0

    # Empty StartTransaction + RollbackTransaction
    ret = lyr.StartTransaction()
    assert ret == 0
    ret = lyr.RollbackTransaction()
    assert ret == 0

    # Isolated CommitTransaction
    gdal.PushErrorHandler()
    ret = lyr.CommitTransaction()
    gdal.PopErrorHandler()
    assert ret != 0

    # Isolated RollbackTransaction
    gdal.PushErrorHandler()
    ret = lyr.RollbackTransaction()
    gdal.PopErrorHandler()
    assert ret != 0

    # 2 StartTransaction in a row
    ret = lyr.StartTransaction()
    assert ret == 0
    gdal.PushErrorHandler()
    ret = lyr.StartTransaction()
    gdal.PopErrorHandler()
    assert ret != 0
    ret = lyr.RollbackTransaction()
    assert ret == 0

    # Missing TransactionSummary
    ret = lyr.StartTransaction()
    assert ret == 0
    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    assert ret == 0
    gdal.PushErrorHandler()
    ret = lyr.CommitTransaction()
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Only 0 features were inserted whereas 1 where expected') < 0)

    ret = lyr.StartTransaction()
    assert ret == 0
    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    assert ret == 0

    gdal.FileFromMemBuffer(wfs_insert_url, "<invalid_xml")
    gdal.PushErrorHandler()
    ret = lyr.CommitTransaction()
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Invalid XML content') < 0)

    ret = lyr.StartTransaction()
    assert ret == 0
    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    assert ret == 0

    gdal.FileFromMemBuffer(wfs_insert_url, "<dummy_xml/>")
    gdal.PushErrorHandler()
    ret = lyr.CommitTransaction()
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Cannot find <TransactionResponse>') < 0)

    ret = lyr.StartTransaction()
    assert ret == 0
    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    assert ret == 0

    gdal.FileFromMemBuffer(wfs_insert_url, "<ServiceExceptionReport/>")
    gdal.PushErrorHandler()
    ret = lyr.CommitTransaction()
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Error returned by server') < 0)

    ret = lyr.StartTransaction()
    assert ret == 0
    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    assert ret == 0

    gdal.FileFromMemBuffer(wfs_insert_url,
                           """<TransactionResponse>
    <TransactionSummary totalInserted="1"/>
</TransactionResponse>
""")
    gdal.PushErrorHandler()
    ret = lyr.CommitTransaction()
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Cannot find node InsertResults') < 0)

    ret = lyr.StartTransaction()
    assert ret == 0
    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    assert ret == 0

    gdal.FileFromMemBuffer(wfs_insert_url,
                           """<TransactionResponse>
    <TransactionSummary totalInserted="1"/>
    <InsertResults/>
</TransactionResponse>
""")
    gdal.PushErrorHandler()
    ret = lyr.CommitTransaction()
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Inconsistent InsertResults: did not get expected FID count') < 0)

    ret = lyr.StartTransaction()
    assert ret == 0
    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    assert ret == 0

    gdal.FileFromMemBuffer(wfs_insert_url,
                           """<TransactionResponse>
    <TransactionSummary totalInserted="1"/>
    <InsertResults>
        <Feature>
        </Feature>
    </InsertResults>
</TransactionResponse>
""")
    gdal.PushErrorHandler()
    ret = lyr.CommitTransaction()
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Cannot find fid') < 0)

    ret = lyr.StartTransaction()
    assert ret == 0
    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    assert ret == 0

    gdal.FileFromMemBuffer(wfs_insert_url,
                           """<TransactionResponse>
    <TransactionSummary totalInserted="1"/>
    <InsertResults>
        <Feature>
            <FeatureId fid="my_layer.100"/>
        </Feature>
    </InsertResults>
</TransactionResponse>
""")
    ret = lyr.CommitTransaction()
    assert ret == 0

    sql_lyr = ds.ExecuteSQL('SELECT _LAST_INSERTED_FIDS_ FROM my_layer')
    f = sql_lyr.GetNextFeature()
    assert f.gml_id == 'my_layer.100'
    sql_lyr.ResetReading()
    sql_lyr.SetNextByIndex(0)
    sql_lyr.GetFeature(0)
    sql_lyr.GetLayerDefn()
    sql_lyr.GetFeatureCount()
    sql_lyr.TestCapability('foo')
    ds.ReleaseResultSet(sql_lyr)

    gdal.Unlink(wfs_insert_url)
    wfs_insert_url = None

    wfs_insert_url = """/vsimem/wfs_endpoint&POSTFIELDS=<?xml version="1.0"?>
<wfs:Transaction xmlns:wfs="http://www.opengis.net/wfs"
                 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                 service="WFS" version="1.1.0"
                 xmlns:gml="http://www.opengis.net/gml"
                 xmlns:ogc="http://www.opengis.net/ogc"
                 xsi:schemaLocation="http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer">
  <wfs:Insert>
    <feature:my_layer xmlns:feature="http://foo">
      <feature:str>foo</feature:str>
      <feature:int>123456789</feature:int>
      <feature:double>2.34</feature:double>
      <feature:shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326"><gml:pos>49 2</gml:pos></gml:Point></feature:shape>
    </feature:my_layer>
  </wfs:Insert>
</wfs:Transaction>
"""
    gdal.FileFromMemBuffer(wfs_insert_url,
                           """<TransactionResponse>
    <TransactionSummary totalInserted="1"/>
    <InsertResults>
        <Feature>
            <FeatureId fid="my_layer.100"/>
        </Feature>
    </InsertResults>
</TransactionResponse>
""")

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('str', 'foo')
    f.SetField('int', 123456789)
    f.SetField('double', 2.34)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (2 49)'))
    ret = lyr.CreateFeature(f)
    assert ret == 0

    gdal.Unlink(wfs_insert_url)
    wfs_insert_url = None

###############################################################################


def test_ogr_wfs_vsimem_wfs110_updatefeature(with_and_without_streaming):

    wfs_update_url = None

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint', update=1)
    lyr = ds.GetLayer(0)

    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Cannot update a feature when gml_id field is not set') < 0)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('gml_id', 'my_layer.1')
    gdal.PushErrorHandler()
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0, gdal.GetLastErrorMsg()

    wfs_update_url = """/vsimem/wfs_endpoint&POSTFIELDS=<?xml version="1.0"?>
<wfs:Transaction xmlns:wfs="http://www.opengis.net/wfs"
                 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                 service="WFS" version="1.1.0"
                 xmlns:gml="http://www.opengis.net/gml"
                 xmlns:ogc="http://www.opengis.net/ogc"
                 xsi:schemaLocation="http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer">
  <wfs:Update typeName="feature:my_layer" xmlns:feature="http://foo">
    <wfs:Property>
      <wfs:Name>shape</wfs:Name>
    </wfs:Property>
    <wfs:Property>
      <wfs:Name>str</wfs:Name>
    </wfs:Property>
    <wfs:Property>
      <wfs:Name>boolean</wfs:Name>
    </wfs:Property>
    <wfs:Property>
      <wfs:Name>short</wfs:Name>
    </wfs:Property>
    <wfs:Property>
      <wfs:Name>int</wfs:Name>
    </wfs:Property>
    <wfs:Property>
      <wfs:Name>float</wfs:Name>
    </wfs:Property>
    <wfs:Property>
      <wfs:Name>double</wfs:Name>
    </wfs:Property>
    <wfs:Property>
      <wfs:Name>dt</wfs:Name>
    </wfs:Property>
    <ogc:Filter>
      <ogc:GmlObjectId gml:id="my_layer.1"/>
    </ogc:Filter>
  </wfs:Update>
</wfs:Transaction>
"""

    gdal.FileFromMemBuffer(wfs_update_url, "")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('gml_id', 'my_layer.1')
    gdal.PushErrorHandler()
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Empty content returned by server') < 0)

    gdal.FileFromMemBuffer(wfs_update_url, "<invalid_xmm")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('gml_id', 'my_layer.1')
    gdal.PushErrorHandler()
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Invalid XML content') < 0)

    gdal.FileFromMemBuffer(wfs_update_url, "<ServiceExceptionReport/>")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('gml_id', 'my_layer.1')
    gdal.PushErrorHandler()
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Error returned by server') < 0)

    gdal.FileFromMemBuffer(wfs_update_url, "<foo/>")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('gml_id', 'my_layer.1')
    gdal.PushErrorHandler()
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Cannot find <TransactionResponse>') < 0)

    gdal.FileFromMemBuffer(wfs_update_url, "<TransactionResponse/>")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('gml_id', 'my_layer.1')
    ret = lyr.SetFeature(f)
    assert ret == 0, gdal.GetLastErrorMsg()

    gdal.Unlink(wfs_update_url)
    wfs_update_url = """/vsimem/wfs_endpoint&POSTFIELDS=<?xml version="1.0"?>
<wfs:Transaction xmlns:wfs="http://www.opengis.net/wfs"
                 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                 service="WFS" version="1.1.0"
                 xmlns:gml="http://www.opengis.net/gml"
                 xmlns:ogc="http://www.opengis.net/ogc"
                 xsi:schemaLocation="http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer">
  <wfs:Update typeName="feature:my_layer" xmlns:feature="http://foo">
    <wfs:Property>
      <wfs:Name>shape</wfs:Name>
      <wfs:Value><gml:Point srsName="urn:ogc:def:crs:EPSG::4326"><gml:pos>49 2</gml:pos></gml:Point></wfs:Value>
    </wfs:Property>
    <wfs:Property>
      <wfs:Name>str</wfs:Name>
      <wfs:Value>foo</wfs:Value>
    </wfs:Property>
    <wfs:Property>
      <wfs:Name>boolean</wfs:Name>
    </wfs:Property>
    <wfs:Property>
      <wfs:Name>short</wfs:Name>
    </wfs:Property>
    <wfs:Property>
      <wfs:Name>int</wfs:Name>
      <wfs:Value>123456789</wfs:Value>
    </wfs:Property>
    <wfs:Property>
      <wfs:Name>float</wfs:Name>
    </wfs:Property>
    <wfs:Property>
      <wfs:Name>double</wfs:Name>
      <wfs:Value>2.34</wfs:Value>
    </wfs:Property>
    <wfs:Property>
      <wfs:Name>dt</wfs:Name>
    </wfs:Property>
    <ogc:Filter>
      <ogc:GmlObjectId gml:id="my_layer.1"/>
    </ogc:Filter>
  </wfs:Update>
</wfs:Transaction>
"""
    gdal.FileFromMemBuffer(wfs_update_url, "<TransactionResponse/>")

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('gml_id', 'my_layer.1')
    f.SetField('str', 'foo')
    f.SetField('int', 123456789)
    f.SetField('double', 2.34)
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (2 49)'))
    ret = lyr.SetFeature(f)
    assert ret == 0

###############################################################################


def test_ogr_wfs_vsimem_wfs110_deletefeature(with_and_without_streaming):

    wfs_delete_url = None

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint', update=1)
    lyr = ds.GetLayer(0)

    gdal.PushErrorHandler()
    ret = lyr.DeleteFeature(200)
    gdal.PopErrorHandler()
    assert ret != 0, gdal.GetLastErrorMsg()

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Fogc%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%22%3E%3CGmlObjectId%20id%3D%22my_layer.200%22%2F%3E%3C%2FFilter%3E',
                           """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:foo="http://foo"
xmlns:wfs="http://www.opengis.net/wfs"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberOfFeatures="1"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer
                    http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd">
    <gml:featureMembers>
        <foo:my_layer gml:id="my_layer.200">
        </foo:my_layer>
    </gml:featureMembers>
</wfs:FeatureCollection>
""")

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint', update=1)
    lyr = ds.GetLayer(0)

    gdal.PushErrorHandler()
    ret = lyr.DeleteFeature(200)
    gdal.PopErrorHandler()
    assert ret != 0, gdal.GetLastErrorMsg()

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint', update=1)
    lyr = ds.GetLayer(0)

    wfs_delete_url = """/vsimem/wfs_endpoint&POSTFIELDS=<?xml version="1.0"?>
<wfs:Transaction xmlns:wfs="http://www.opengis.net/wfs"
                 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                 service="WFS" version="1.1.0"
                 xmlns:gml="http://www.opengis.net/gml"
                 xmlns:ogc="http://www.opengis.net/ogc"
                 xsi:schemaLocation="http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer">
  <wfs:Delete xmlns:feature="http://foo" typeName="feature:my_layer">
    <ogc:Filter>
<ogc:FeatureId fid="my_layer.200"/>
    </ogc:Filter>
  </wfs:Delete>
</wfs:Transaction>
"""

    gdal.FileFromMemBuffer(wfs_delete_url, "")
    gdal.PushErrorHandler()
    ret = lyr.DeleteFeature(200)
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Empty content returned by server') < 0)

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint', update=1)
    lyr = ds.GetLayer(0)
    gdal.FileFromMemBuffer(wfs_delete_url, "<invalid_xml>")
    gdal.PushErrorHandler()
    ret = lyr.DeleteFeature(200)
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Invalid XML content') < 0)

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint', update=1)
    lyr = ds.GetLayer(0)
    gdal.FileFromMemBuffer(wfs_delete_url, "<foo/>")
    gdal.PushErrorHandler()
    ret = lyr.DeleteFeature(200)
    gdal.PopErrorHandler()
    assert not (ret == 0 or gdal.GetLastErrorMsg().find('Cannot find <TransactionResponse>') < 0)

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint', update=1)
    lyr = ds.GetLayer(0)
    gdal.FileFromMemBuffer(wfs_delete_url, "<TransactionResponse/>")
    ret = lyr.DeleteFeature(200)
    assert ret == 0, gdal.GetLastErrorMsg()

    gdal.Unlink(wfs_delete_url)
    wfs_delete_url = """/vsimem/wfs_endpoint&POSTFIELDS=<?xml version="1.0"?>
<wfs:Transaction xmlns:wfs="http://www.opengis.net/wfs"
                 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                 service="WFS" version="1.1.0"
                 xmlns:gml="http://www.opengis.net/gml"
                 xmlns:ogc="http://www.opengis.net/ogc"
                 xsi:schemaLocation="http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer">
  <wfs:Delete xmlns:feature="http://foo" typeName="feature:my_layer">
    <ogc:Filter>
<GmlObjectId id="my_layer.200"/>    </ogc:Filter>
  </wfs:Delete>
</wfs:Transaction>
"""

    gdal.FileFromMemBuffer(wfs_delete_url, "<TransactionResponse/>")

    gdal.ErrorReset()
    sql_lyr = ds.ExecuteSQL("DELETE FROM my_layer WHERE gml_id = 'my_layer.200'")
    assert gdal.GetLastErrorMsg() == ''

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL("DELETE FROM ")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL("DELETE FROM non_existing_layer WHERE truc")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg().find('Unknown layer') >= 0

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL("DELETE FROM my_layer BLA")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg().find('WHERE clause missing') >= 0

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL("DELETE FROM my_layer WHERE -")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg().find('SQL Expression Parsing Error') >= 0

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL("DELETE FROM my_layer WHERE ogr_geometry = 'POINT'")
    gdal.PopErrorHandler()
    assert sql_lyr is None and gdal.GetLastErrorMsg() != ''

###############################################################################


def test_ogr_wfs_vsimem_wfs110_schema_not_understood(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    # Invalid response, but enough for use
    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint_schema_not_understood?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="1.1.0">
    <FeatureTypeList>
        <FeatureType/>
        <FeatureType>
            <Name>my_layer</Name>
        </FeatureType>
    </FeatureTypeList>
</WFS_Capabilities>
""")
    ds = ogr.Open('WFS:/vsimem/wfs_endpoint_schema_not_understood')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint_schema_not_understood?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=my_layer',
                           """<xsd:schema xmlns:foo="http://foo" xmlns:gml="http://www.opengis.net/gml" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
  <xsd:import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.1.1/base/gml.xsd"/>
  <xsd:complexType name="my_layerType">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="SOME_TYPE_I_DONT_UNDERSTAND"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="boolean" nillable="true" type="xsd:boolean"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="short" nillable="true" type="xsd:short"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="int" nillable="true" type="xsd:int"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="float" nillable="true" type="xsd:float"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="double" nillable="true" type="xsd:double"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="dt" nillable="true" type="xsd:dateTime"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="shape" nillable="true" type="gml:PointPropertyType"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="my_layer" substitutionGroup="gml:_Feature" type="foo:my_layerType"/>
</xsd:schema>
""")

    gdal.PushErrorHandler()
    lyr_defn = lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    assert lyr_defn.GetFieldCount() == 0

    ds = ogr.Open('WFS:/vsimem/wfs_endpoint_schema_not_understood')
    lyr = ds.GetLayer(0)

    content = \
        """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:foo="http://foo"
xmlns:wfs="http://www.opengis.net/wfs"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberOfFeatures="1"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer
                    http://www.opengis.net/wfs http://schemas.opengis.net/wfs/1.1.0/wfs.xsd">
    <gml:featureMembers>
        <foo:my_layer gml:id="my_layer.1">
            <foo:str>str</foo:str>
            <foo:boolean>true</foo:boolean>
            <foo:short>1</foo:short>
            <foo:int>123456789</foo:int>
            <foo:float>1.2</foo:float>
            <foo:double>1.23</foo:double>
            <foo:dt>2015-04-17T12:34:56Z</foo:dt>
            <foo:shape>
                <gml:Point srsDimension="2" srsName="urn:ogc:def:crs:EPSG::4326">
                    <gml:pos>49 2</gml:pos>
                </gml:Point>
            </foo:shape>
        </foo:my_layer>
    </gml:featureMembers>
</wfs:FeatureCollection>
"""

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint_schema_not_understood?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer&MAXFEATURES=1', content)

    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 8

    gdal.FileFromMemBuffer('/vsimem/wfs_endpoint_schema_not_understood?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=my_layer', content)

    f = lyr.GetNextFeature()
    if f.gml_id != 'my_layer.1' or f.boolean != 1 or f.str != 'str' or f.short != 1 or \
       f.int != 123456789 or f.float != 1.2 or f.double != 1.23 or f.dt != '2015/04/17 12:34:56+00' or \
       f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_wfs_vsimem_wfs110_multiple_layers(with_and_without_streaming):

    gdal.FileFromMemBuffer('/vsimem/wfs110_multiple_layers?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="1.1.0">
    <FeatureTypeList>
        <FeatureType>
            <Name>my_layer</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
        <FeatureType>
            <Name>my_layer2</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
    </FeatureTypeList>
</WFS_Capabilities>
""")

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs110_multiple_layers')
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler()
    lyr_defn = lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    assert lyr_defn.GetFieldCount() == 0

    ds = ogr.Open('WFS:/vsimem/wfs110_multiple_layers')
    lyr = ds.GetLayer(0)
    gdal.FileFromMemBuffer('/vsimem/wfs110_multiple_layers?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=my_layer,my_layer2',
                           "<ServiceExceptionReport/>")
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler()
    lyr_defn = lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    assert lyr_defn.GetFieldCount() == 0

    ds = ogr.Open('WFS:/vsimem/wfs110_multiple_layers')
    lyr = ds.GetLayer(0)
    gdal.FileFromMemBuffer('/vsimem/wfs110_multiple_layers?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=my_layer,my_layer2',
                           "<invalid_xml")
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler()
    lyr_defn = lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    assert lyr_defn.GetFieldCount() == 0

    ds = ogr.Open('WFS:/vsimem/wfs110_multiple_layers')
    lyr = ds.GetLayer(0)
    gdal.FileFromMemBuffer('/vsimem/wfs110_multiple_layers?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=my_layer,my_layer2',
                           "<no_schema/>")
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler()
    lyr_defn = lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    assert lyr_defn.GetFieldCount() == 0

    ds = ogr.Open('WFS:/vsimem/wfs110_multiple_layers')
    lyr = ds.GetLayer(0)
    gdal.FileFromMemBuffer('/vsimem/wfs110_multiple_layers?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=my_layer,my_layer2',
                           """<xsd:schema xmlns:foo="http://foo" xmlns:gml="http://www.opengis.net/gml" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
  <xsd:import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.2.1/base/gml.xsd"/>
  <xsd:complexType name="my_layerType">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="my_layer" substitutionGroup="gml:_Feature" type="foo:my_layerType"/>
  <xsd:complexType name="my_layer2Type">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="my_layer2" substitutionGroup="gml:_Feature" type="foo:my_layer2Type"/>
</xsd:schema>
""")

    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 2

    lyr = ds.GetLayer(1)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 2

    ds = ogr.Open('WFS:/vsimem/wfs110_multiple_layers')
    lyr = ds.GetLayer(0)
    gdal.FileFromMemBuffer('/vsimem/wfs110_multiple_layers?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=my_layer,my_layer2',
                           """<xsd:schema xmlns:foo="http://foo" xmlns:gml="http://www.opengis.net/gml" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
  <xsd:import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.2.1/base/gml.xsd"/>
  <xsd:complexType name="my_layerType">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="my_layer" substitutionGroup="gml:_Feature" type="foo:my_layerType"/>
</xsd:schema>
""")

    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 2

    gdal.FileFromMemBuffer('/vsimem/wfs110_multiple_layers?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=my_layer2',
                           """<xsd:schema xmlns:foo="http://foo" xmlns:gml="http://www.opengis.net/gml" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
  <xsd:import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.2.1/base/gml.xsd"/>
 <xsd:complexType name="my_layer2Type">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="my_layer2" substitutionGroup="gml:_Feature" type="foo:my_layer2Type"/>
</xsd:schema>
""")
    lyr = ds.GetLayer(1)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 2

###############################################################################


def test_ogr_wfs_vsimem_wfs110_multiple_layers_same_name_different_ns(with_and_without_streaming):

    gdal.FileFromMemBuffer('/vsimem/wfs110_multiple_layers_different_ns?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="1.1.0">
    <FeatureTypeList>
        <FeatureType>
            <Name>ns1:my_layer</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
        <FeatureType>
            <Name>ns2:my_layer</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
    </FeatureTypeList>
</WFS_Capabilities>
""")

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs110_multiple_layers_different_ns')
    lyr = ds.GetLayer(0)
    gdal.FileFromMemBuffer('/vsimem/wfs110_multiple_layers_different_ns?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=ns1:my_layer',
                           """<xsd:schema xmlns:ns1="http://ns1" xmlns:ns2="http://ns2" xmlns:gml="http://www.opengis.net/gml" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
  <xsd:import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.2.1/base/gml.xsd"/>
  <xsd:complexType name="my_layerType">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="my_layer" substitutionGroup="gml:_Feature" type="my_layerType"/>
</xsd:schema>
""")

    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 2

    gdal.FileFromMemBuffer('/vsimem/wfs110_multiple_layers_different_ns?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=ns1:my_layer',
                           """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:ns1="http://ns1"
xmlns:wfs="http://www.opengis.net/wfs"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberMatched="unknown" numberReturned="2"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://ns1 /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=2.0.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer
                    http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd">
    <gml:featureMembers>
        <ns1:my_layer gml:id="my_layer.1">
        </ns1:my_layer>
    </gml:featureMembers>
</wfs:FeatureCollection>
""")

    f = lyr.GetNextFeature()
    assert f is not None

    gdal.FileFromMemBuffer('/vsimem/wfs110_multiple_layers_different_ns?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=ns2:my_layer',
                           """<xsd:schema xmlns:ns2="http://ns2" xmlns:ns2="http://ns2" xmlns:gml="http://www.opengis.net/gml" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
  <xsd:import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.2.1/base/gml.xsd"/>
  <xsd:complexType name="my_layerType">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="str2" nillable="true" type="xsd:string"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="my_layer" substitutionGroup="gml:_Feature" type="my_layerType"/>
</xsd:schema>
""")
    lyr = ds.GetLayer(1)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 3

###############################################################################


def test_ogr_wfs_vsimem_wfs200_paging(with_and_without_streaming):

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_paging?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="2.0.0">
    <OperationsMetadata>
        <ows:Operation name="GetFeature">
            <ows:Constraint name="CountDefault">
                <ows:NoValues/>
                <ows:DefaultValue>2</ows:DefaultValue>
            </ows:Constraint>
        </ows:Operation>
        <ows:Constraint name="ImplementsResultPaging">
            <ows:NoValues/><ows:DefaultValue>TRUE</ows:DefaultValue>
        </ows:Constraint>
    </OperationsMetadata>
    <FeatureTypeList>
        <FeatureType>
            <Name>my_layer</Name>
            <Title>title</Title>
            <Abstract>abstract</Abstract>
            <Keywords>
                <Keyword>keyword</Keyword>
            </Keywords>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
    </FeatureTypeList>
  <ogc:Filter_Capabilities>
    <ogc:Spatial_Capabilities>
      <ogc:GeometryOperands>
        <ogc:GeometryOperand>gml:Envelope</ogc:GeometryOperand>
        <ogc:GeometryOperand>gml:Point</ogc:GeometryOperand>
        <ogc:GeometryOperand>gml:LineString</ogc:GeometryOperand>
        <ogc:GeometryOperand>gml:Polygon</ogc:GeometryOperand>
      </ogc:GeometryOperands>
      <ogc:SpatialOperators>
        <ogc:SpatialOperator name="Disjoint"/>
        <ogc:SpatialOperator name="Equals"/>
        <ogc:SpatialOperator name="DWithin"/>
        <ogc:SpatialOperator name="Beyond"/>
        <ogc:SpatialOperator name="Intersects"/>
        <ogc:SpatialOperator name="Touches"/>
        <ogc:SpatialOperator name="Crosses"/>
        <ogc:SpatialOperator name="Within"/>
        <ogc:SpatialOperator name="Contains"/>
        <ogc:SpatialOperator name="Overlaps"/>
        <ogc:SpatialOperator name="BBOX"/>
      </ogc:SpatialOperators>
    </ogc:Spatial_Capabilities>
    <ogc:Scalar_Capabilities>
      <ogc:LogicalOperators/>
      <ogc:ComparisonOperators>
        <ogc:ComparisonOperator>LessThan</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>GreaterThan</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>LessThanEqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>GreaterThanEqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>EqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>NotEqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>Like</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>Between</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>NullCheck</ogc:ComparisonOperator>
      </ogc:ComparisonOperators>
      <ogc:ArithmeticOperators>
        <ogc:SimpleArithmetic/>
        <ogc:Functions/>
      </ogc:ArithmeticOperators>
    </ogc:Scalar_Capabilities>
    <ogc:Id_Capabilities>
      <ogc:FID/>
      <ogc:EID/>
    </ogc:Id_Capabilities>
  </ogc:Filter_Capabilities>
</WFS_Capabilities>
""")

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_paging')
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadata() == {'ABSTRACT': 'abstract', 'KEYWORD_1': 'keyword', 'TITLE': 'title'}

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_paging?SERVICE=WFS&VERSION=2.0.0&REQUEST=DescribeFeatureType&TYPENAME=my_layer',
                           """<xsd:schema xmlns:foo="http://foo" xmlns:gml="http://www.opengis.net/gml" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
  <xsd:import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.2.1/base/gml.xsd"/>
  <xsd:complexType name="my_layerType">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="boolean" nillable="true" type="xsd:boolean"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="short" nillable="true" type="xsd:short"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="int" nillable="true" type="xsd:int"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="float" nillable="true" type="xsd:float"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="double" nillable="true" type="xsd:double"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="dt" nillable="true" type="xsd:dateTime"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="shape" nillable="true" type="gml:PointPropertyType"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="my_layer" substitutionGroup="gml:_Feature" type="foo:my_layerType"/>
</xsd:schema>
""")

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_paging?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=my_layer&STARTINDEX=0&COUNT=2',
                           """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:foo="http://foo"
xmlns:wfs="http://www.opengis.net/wfs"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberMatched="unknown" numberReturned="2"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=2.0.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer
                    http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd">
    <gml:featureMembers>
        <foo:my_layer gml:id="my_layer.1">
            <foo:str>str</foo:str>
            <foo:boolean>true</foo:boolean>
            <foo:short>1</foo:short>
            <foo:int>123456789</foo:int>
            <foo:float>1.2</foo:float>
            <foo:double>1.23</foo:double>
            <foo:dt>2015-04-17T12:34:56Z</foo:dt>
            <foo:shape>
                <gml:Point srsDimension="2" srsName="urn:ogc:def:crs:EPSG::4326">
                    <gml:pos>49 2</gml:pos>
                </gml:Point>
            </foo:shape>
        </foo:my_layer>
    </gml:featureMembers>
    <gml:featureMembers>
        <foo:my_layer gml:id="my_layer.2">
        </foo:my_layer>
    </gml:featureMembers>
</wfs:FeatureCollection>
""")

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_paging?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=my_layer&STARTINDEX=2&COUNT=2',
                           """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:foo="http://foo"
xmlns:wfs="http://www.opengis.net/wfs"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberMatched="unknown" numberReturned="1"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://foo /vsimem/wfs_endpoint?SERVICE=WFS&amp;VERSION=1.1.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=my_layer
                    http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd">
    <gml:featureMembers>
        <foo:my_layer gml:id="my_layer.3">
        </foo:my_layer>
    </gml:featureMembers>
</wfs:FeatureCollection>
""")

    f = lyr.GetNextFeature()
    assert f is not None
    if f.gml_id != 'my_layer.1':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    assert f is not None
    if f.gml_id != 'my_layer.2':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    assert f is not None
    if f.gml_id != 'my_layer.3':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f is not None:
        f.DumpReadable()
        pytest.fail()

    # if lyr.GetFeatureCount() != 3:
    #    gdaltest.post_reason('fail')
    #    print(lyr.GetFeatureCount())
    #    return 'fail'

    

###############################################################################
def test_ogr_wfs_vsimem_wfs200_json(with_and_without_streaming):

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_json?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="2.0.0">
    <OperationsMetadata>
        <ows:Operation name="GetFeature">
            <ows:Parameter name="resultType">
                <ows:Value>results</ows:Value>
                <ows:Value>hits</ows:Value>
            </ows:Parameter>
            <ows:Parameter name="outputFormat">
                <ows:AllowedValues>
                    <ows:Value>application/json</ows:Value>
                </ows:AllowedValues>
            </ows:Parameter>
            <ows:Constraint name="CountDefault">
                <ows:NoValues/>
                <ows:DefaultValue>2</ows:DefaultValue>
            </ows:Constraint>
        </ows:Operation>
        <ows:Constraint name="ImplementsResultPaging">
            <ows:NoValues/><ows:DefaultValue>TRUE</ows:DefaultValue>
        </ows:Constraint>
    </OperationsMetadata>
    <FeatureTypeList>
        <FeatureType>
            <Name>my_layer</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
    </FeatureTypeList>
  <ogc:Filter_Capabilities>
    <ogc:Spatial_Capabilities>
      <ogc:GeometryOperands>
        <ogc:GeometryOperand>gml:Envelope</ogc:GeometryOperand>
        <ogc:GeometryOperand>gml:Point</ogc:GeometryOperand>
        <ogc:GeometryOperand>gml:LineString</ogc:GeometryOperand>
        <ogc:GeometryOperand>gml:Polygon</ogc:GeometryOperand>
      </ogc:GeometryOperands>
      <ogc:SpatialOperators>
        <ogc:SpatialOperator name="Disjoint"/>
        <ogc:SpatialOperator name="Equals"/>
        <ogc:SpatialOperator name="DWithin"/>
        <ogc:SpatialOperator name="Beyond"/>
        <ogc:SpatialOperator name="Intersects"/>
        <ogc:SpatialOperator name="Touches"/>
        <ogc:SpatialOperator name="Crosses"/>
        <ogc:SpatialOperator name="Within"/>
        <ogc:SpatialOperator name="Contains"/>
        <ogc:SpatialOperator name="Overlaps"/>
        <ogc:SpatialOperator name="BBOX"/>
      </ogc:SpatialOperators>
    </ogc:Spatial_Capabilities>
    <ogc:Scalar_Capabilities>
      <ogc:LogicalOperators/>
      <ogc:ComparisonOperators>
        <ogc:ComparisonOperator>LessThan</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>GreaterThan</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>LessThanEqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>GreaterThanEqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>EqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>NotEqualTo</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>Like</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>Between</ogc:ComparisonOperator>
        <ogc:ComparisonOperator>NullCheck</ogc:ComparisonOperator>
      </ogc:ComparisonOperators>
      <ogc:ArithmeticOperators>
        <ogc:SimpleArithmetic/>
        <ogc:Functions/>
      </ogc:ArithmeticOperators>
    </ogc:Scalar_Capabilities>
    <ogc:Id_Capabilities>
      <ogc:FID/>
      <ogc:EID/>
    </ogc:Id_Capabilities>
  </ogc:Filter_Capabilities>
</WFS_Capabilities>
""")

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_json?OUTPUTFORMAT=application/json')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_json?SERVICE=WFS&VERSION=2.0.0&REQUEST=DescribeFeatureType&TYPENAME=my_layer',
                           """<xsd:schema xmlns:foo="http://foo" xmlns:gml="http://www.opengis.net/gml" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
  <xsd:import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.2.1/base/gml.xsd"/>
  <xsd:complexType name="my_layerType">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="boolean" nillable="true" type="xsd:boolean"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="short" nillable="true" type="xsd:short"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="int" nillable="true" type="xsd:int"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="float" nillable="true" type="xsd:float"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="double" nillable="true" type="xsd:double"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="dt" nillable="true" type="xsd:dateTime"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="shape" nillable="true" type="gml:PointPropertyType"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="my_layer" substitutionGroup="gml:_Feature" type="foo:my_layerType"/>
</xsd:schema>
""")

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_json?OUTPUTFORMAT=application/json&SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=my_layer&STARTINDEX=0&COUNT=2',
                           """{"type":"FeatureCollection",
"totalFeatures":"unknown",
"features":[{"type":"Feature","id":"my_layer.1",
"geometry":{"type":"Point","coordinates":[2, 49]},
"properties":{"str":"str"}}]}
""")

    f = lyr.GetNextFeature()
    assert f is not None
    # We currently invert... A bit weird. See comment in code. Probably inappropriate
    if f.str != 'str' or f.GetGeometryRef().ExportToWkt() != 'POINT (49 2)':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f is not None:
        f.DumpReadable()
        pytest.fail()

    

###############################################################################
def test_ogr_wfs_vsimem_wfs200_multipart(with_and_without_streaming):

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_multipart?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="2.0.0">
    <FeatureTypeList>
        <FeatureType>
            <Name>my_layer</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
    </FeatureTypeList>
</WFS_Capabilities>
""")

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_multipart?OUTPUTFORMAT=multipart')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_multipart?SERVICE=WFS&VERSION=2.0.0&REQUEST=DescribeFeatureType&TYPENAME=my_layer',
                           """<xsd:schema xmlns:foo="http://foo" xmlns:gml="http://www.opengis.net/gml" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
  <xsd:import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.2.1/base/gml.xsd"/>
  <xsd:complexType name="my_layerType">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="shape" nillable="true" type="gml:PointPropertyType"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="my_layer" substitutionGroup="gml:_Feature" type="foo:my_layerType"/>
</xsd:schema>
""")

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_multipart?OUTPUTFORMAT=multipart&SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=my_layer',
                           """Content-Type: multipart/mixed; boundary="my_boundary"
\r
\r
--my_boundary
Content-Type: text/plain; charset=us-ascii
Content-Disposition: attachment; filename=my.json
\r
{
"type":"FeatureCollection",
"totalFeatures":"unknown",
"features":[
    {
        "type":"Feature",
        "id":"my_layer.1",
        "geometry":{"type":"Point","coordinates":[2, 49]},
        "properties":{"str":"str"}
    }
]
}
--my_boundary--
""")

    f = lyr.GetNextFeature()
    assert f is not None
    # We currently invert... A bit weird. See comment in code. Probably inappropriate
    if f.str != 'str' or f.GetGeometryRef().ExportToWkt() != 'POINT (49 2)':
        f.DumpReadable()
        pytest.fail()

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_multipart?OUTPUTFORMAT=multipart')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_multipart?OUTPUTFORMAT=multipart&SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=my_layer',
                           """Content-Type: multipart/mixed; boundary="my_boundary"
\r
\r
--my_boundary
\r
{
"type":"FeatureCollection",
"totalFeatures":"unknown",
"features":[
    {
        "type":"Feature",
        "id":"my_layer.1",
        "geometry":{"type":"Point","coordinates":[2, 49]},
        "properties":{"str":"str"}
    }
]
}
--my_boundary--
""")

    f = lyr.GetNextFeature()
    assert f is not None

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_multipart?OUTPUTFORMAT=multipart')
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_multipart?OUTPUTFORMAT=multipart&SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=my_layer',
                           """Content-Type: multipart/mixed; boundary="my_boundary"
\r
\r
--my_boundary
Content-Disposition: attachment; filename=my.csvt
\r
String,String
--my_boundary
Content-Disposition: attachment; filename=my.csv
\r
str,WKT
str,"POINT(2 49)"
--my_boundary--
""")
    f = lyr.GetNextFeature()
    assert f is not None
    # We currently invert... A bit weird. See comment in code. Probably inappropriate
    if f.str != 'str' or f.GetGeometryRef().ExportToWkt() != 'POINT (49 2)':
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_wfs_vsimem_wfs200_join(with_and_without_streaming):

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="2.0.0">
    <OperationsMetadata>
        <ows:Operation name="GetFeature">
            <ows:Constraint name="CountDefault">
                <ows:NoValues/>
                <ows:DefaultValue>1</ows:DefaultValue>
            </ows:Constraint>
        </ows:Operation>
        <ows:Constraint name="ImplementsResultPaging">
            <ows:NoValues/><ows:DefaultValue>TRUE</ows:DefaultValue>
        </ows:Constraint>
        <ows:Constraint name="ImplementsStandardJoins">
            <ows:NoValues/><ows:DefaultValue>TRUE</ows:DefaultValue>
        </ows:Constraint>
    </OperationsMetadata>
    <FeatureTypeList>
        <FeatureType>
            <Name>lyr1</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
        <FeatureType>
            <Name>lyr2</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
    </FeatureTypeList>
</WFS_Capabilities>
""")

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=DescribeFeatureType&TYPENAME=lyr1,lyr2',
                           """<xsd:schema xmlns:foo="http://foo" xmlns:gml="http://www.opengis.net/gml" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
  <xsd:import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.2.1/base/gml.xsd"/>
  <xsd:complexType name="lyr1Type">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="shape" nillable="true" type="gml:PointPropertyType"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="lyr1" substitutionGroup="gml:_Feature" type="foo:lyr1Type"/>
  <xsd:complexType name="lyr2Type">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str2" nillable="true" type="xsd:string"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="another_shape" nillable="true" type="gml:PointPropertyType"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="lyr2" substitutionGroup="gml:_Feature" type="foo:lyr2Type"/>
</xsd:schema>
""")

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_join')
    sql_lyr = ds.ExecuteSQL('SELECT * FROM lyr1 JOIN lyr2 ON lyr1.str = lyr2.str2')

    gdal.PushErrorHandler()
    f = sql_lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None

    ds.ReleaseResultSet(sql_lyr)

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_join')
    sql_lyr = ds.ExecuteSQL('SELECT * FROM lyr1 JOIN lyr2 ON lyr1.str = lyr2.str2')

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&STARTINDEX=0&COUNT=1&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E',
                           """""")

    gdal.PushErrorHandler()
    f = sql_lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None and gdal.GetLastErrorMsg().find('Empty content returned by server') >= 0

    ds.ReleaseResultSet(sql_lyr)

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_join')
    sql_lyr = ds.ExecuteSQL('SELECT * FROM lyr1 JOIN lyr2 ON lyr1.str = lyr2.str2')

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&STARTINDEX=0&COUNT=1&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E',
                           """<ServiceExceptionReport/>""")

    gdal.PushErrorHandler()
    f = sql_lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None and gdal.GetLastErrorMsg().find('Error returned by server') >= 0

    ds.ReleaseResultSet(sql_lyr)

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_join')
    sql_lyr = ds.ExecuteSQL('SELECT * FROM lyr1 JOIN lyr2 ON lyr1.str = lyr2.str2')

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&STARTINDEX=0&COUNT=1&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E',
                           """<invalid_xml""")

    gdal.PushErrorHandler()
    f = sql_lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None and gdal.GetLastErrorMsg().find('Error: cannot parse') >= 0

    ds.ReleaseResultSet(sql_lyr)

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_join')
    sql_lyr = ds.ExecuteSQL('SELECT * FROM lyr1 JOIN lyr2 ON lyr1.str = lyr2.str2')

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&STARTINDEX=0&COUNT=1&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E',
                           """<dummy_xml/>""")

    gdal.PushErrorHandler()
    f = sql_lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert f is None and gdal.GetLastErrorMsg().find('Error: cannot parse') >= 0

    ds.ReleaseResultSet(sql_lyr)

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_join')
    sql_lyr = ds.ExecuteSQL('SELECT * FROM lyr1 JOIN lyr2 ON lyr1.str = lyr2.str2')

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&STARTINDEX=0&COUNT=1&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E',
                           """<?xml version="1.0" encoding="UTF-8"?>
<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
    xmlns:foo="http://foo"
    xmlns:wfs="http://www.opengis.net/wfs/2.0"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    numberMatched="unknown" numberReturned="1" timeStamp="2015-01-01T00:00:00.000Z"
    xsi:schemaLocation="http://www.opengis.net/gml/3.2 http://schemas.opengis.net/gml/3.2.1/gml.xsd
                        http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd
                        http://foo /vsimem/wfs200_endpoint_join?SERVICE=WFS&amp;VERSION=2.0.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=lyr1,lyr2">
  <wfs:member>
    <wfs:Tuple>
      <wfs:member>
        <foo:lyr1 gml:id="lyr1-100">
          <foo:str>123.4</foo:str>
          <foo:shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="bla"><gml:pos>48.5 2.5</gml:pos></gml:Point></foo:shape>
        </foo:lyr1>
      </wfs:member>
      <wfs:member>
        <foo:lyr2 gml:id="lyr2-101">
          <foo:str2>123.4</foo:str2>
          <foo:another_shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="bla"><gml:pos>49 2</gml:pos></gml:Point></foo:another_shape>
        </foo:lyr2>
      </wfs:member>
    </wfs:Tuple>
  </wfs:member>
</wfs:FeatureCollection>
""")

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&STARTINDEX=1&COUNT=1&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E',
                           """<?xml version="1.0" encoding="UTF-8"?>
<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
    xmlns:foo="http://foo"
    xmlns:wfs="http://www.opengis.net/wfs/2.0"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    numberMatched="unknown" numberReturned="1" timeStamp="2015-01-01T00:00:00.000Z"
    xsi:schemaLocation="http://www.opengis.net/gml/3.2 http://schemas.opengis.net/gml/3.2.1/gml.xsd
                        http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd
                        http://foo /vsimem/wfs200_endpoint_join?SERVICE=WFS&amp;VERSION=2.0.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=lyr1,lyr2">
  <wfs:member>
    <wfs:Tuple>
      <wfs:member>
        <foo:lyr1 gml:id="lyr1-101">
          <foo:str>foo</foo:str>
          <foo:shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="bla"><gml:pos>48.5 2.5</gml:pos></gml:Point></foo:shape>
        </foo:lyr1>
      </wfs:member>
      <wfs:member>
        <foo:lyr2 gml:id="lyr2-102">
          <foo:str2>foo</foo:str2>
          <foo:another_shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="bla"><gml:pos>49 2</gml:pos></gml:Point></foo:another_shape>
        </foo:lyr2>
      </wfs:member>
    </wfs:Tuple>
  </wfs:member>
</wfs:FeatureCollection>
""")
    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&STARTINDEX=2&COUNT=1&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E',
                           """<?xml version="1.0" encoding="UTF-8"?>
<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
    xmlns:foo="http://foo"
    xmlns:wfs="http://www.opengis.net/wfs/2.0"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    numberMatched="unknown" numberReturned="0" timeStamp="2015-01-01T00:00:00.000Z"
    xsi:schemaLocation="http://www.opengis.net/gml/3.2 http://schemas.opengis.net/gml/3.2.1/gml.xsd
                        http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd
                        http://foo /vsimem/wfs200_endpoint_join?SERVICE=WFS&amp;VERSION=2.0.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=lyr1,lyr2">
</wfs:FeatureCollection>
""")

    f = sql_lyr.GetNextFeature()
    if f['lyr1.gml_id'] != 'lyr1-100' or f['lyr1.str'] != '123.4' or \
       f['lyr2.gml_id'] != 'lyr2-101' or f['lyr2.str2'] != '123.4' or \
       f['lyr1.shape'].ExportToWkt() != 'POINT (2.5 48.5)' or \
       f['lyr2.another_shape'].ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    f = sql_lyr.GetNextFeature()
    if f['lyr1.gml_id'] != 'lyr1-101' or f['lyr1.str'] != 'foo' or \
       f['lyr2.gml_id'] != 'lyr2-102' or f['lyr2.str2'] != 'foo' or \
       f['lyr1.shape'].ExportToWkt() != 'POINT (2.5 48.5)' or \
       f['lyr2.another_shape'].ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    f = sql_lyr.GetNextFeature()
    if f is not None:
        f.DumpReadable()
        pytest.fail()

    sql_lyr.ResetReading()
    sql_lyr.ResetReading()
    f = sql_lyr.GetNextFeature()
    if f['lyr1.gml_id'] != 'lyr1-100':
        f.DumpReadable()
        pytest.fail()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    fc = sql_lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 2, gdal.GetLastErrorMsg()

    # Empty content returned by server
    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E&RESULTTYPE=hits',
                           """""")
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    fc = sql_lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 2, gdal.GetLastErrorMsg()

    # Invalid XML
    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E&RESULTTYPE=hits',
                           """<invalid_xml""")
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    fc = sql_lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 2, gdal.GetLastErrorMsg()

    # Server exception
    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E&RESULTTYPE=hits',
                           """<ServiceExceptionReport/>""")
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    fc = sql_lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 2, gdal.GetLastErrorMsg()

    # Missing FeatureCollection
    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E&RESULTTYPE=hits',
                           """<dummy_xml/>""")
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    fc = sql_lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 2, gdal.GetLastErrorMsg()

    # Missing FeatureCollection.numberMatched
    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E&RESULTTYPE=hits',
                           """<FeatureCollection/>""")
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    fc = sql_lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 2, gdal.GetLastErrorMsg()

    # Valid
    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E&RESULTTYPE=hits',
                           """<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
xmlns:ogc="http://www.opengis.net/ogc"
xmlns:foo="http://foo"
xmlns:wfs="http://www.opengis.net/wfs/2.0"
xmlns:ows="http://www.opengis.net/ows"
xmlns:xlink="http://www.w3.org/1999/xlink"
xmlns:gml="http://www.opengis.net/gml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
numberMatched="3"
timeStamp="2015-04-17T14:14:24.859Z"
xsi:schemaLocation="http://foo blabla
                    http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd">
</wfs:FeatureCollection>""")
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    fc = sql_lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    assert fc == 3, gdal.GetLastErrorMsg()

    sql_lyr.TestCapability('foo')
    sql_lyr.GetLayerDefn()

    # Test filters (nt supported)
    sql_lyr.SetAttributeFilter(None)
    gdal.PushErrorHandler()
    sql_lyr.SetAttributeFilter('"lyr1.gml_id" IS NOT NULL')
    gdal.PopErrorHandler()

    sql_lyr.SetSpatialFilter(None)
    gdal.PushErrorHandler()
    sql_lyr.SetSpatialFilterRect(0, 0, 0, 0)
    gdal.PopErrorHandler()

    ds.ReleaseResultSet(sql_lyr)

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_join')
    sql_lyr = ds.ExecuteSQL('SELECT lyr1.*, lyr2.* FROM lyr1 JOIN lyr2 ON lyr1.str = lyr2.str2')
    f = sql_lyr.GetNextFeature()
    if f['lyr1.gml_id'] != 'lyr1-100' or f['lyr1.str'] != '123.4' or \
       f['lyr2.gml_id'] != 'lyr2-101' or f['lyr2.str2'] != '123.4' or \
       f['lyr1.shape'].ExportToWkt() != 'POINT (2.5 48.5)' or \
       f['lyr2.another_shape'].ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_join')
    sql_lyr = ds.ExecuteSQL('SELECT * FROM lyr1 my_alias1 JOIN lyr2 ON my_alias1.str = lyr2.str2')
    f = sql_lyr.GetNextFeature()
    if f['my_alias1.gml_id'] != 'lyr1-100' or f['my_alias1.str'] != '123.4' or \
       f['lyr2.gml_id'] != 'lyr2-101' or f['lyr2.str2'] != '123.4' or \
       f['my_alias1.shape'].ExportToWkt() != 'POINT (2.5 48.5)' or \
       f['lyr2.another_shape'].ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_join')
    sql_lyr = ds.ExecuteSQL('SELECT my_alias1.gml_id as gml_id1, ' +
                            'CAST(my_alias1.str AS integer) AS str_int, ' +
                            'CAST(my_alias1.str AS bigint) AS str_bigint, ' +
                            'CAST(my_alias1.str AS float) AS str_float, ' +
                            'my_alias1.shape AS myshape ' +
                            'FROM lyr1 my_alias1 JOIN lyr2 ON my_alias1.str = lyr2.str2')
    f = sql_lyr.GetNextFeature()
    if f['gml_id1'] != 'lyr1-100' or \
       f['str_int'] != 123 or \
       f['str_bigint'] != 123 or \
       f['str_float'] != 123.4 or \
       f['myshape'].ExportToWkt() != 'POINT (2.5 48.5)':
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_join')
    sql_lyr = ds.ExecuteSQL("SELECT * FROM lyr1 JOIN lyr2 ON lyr1.str = lyr2.str2 WHERE lyr2.str2 = '123.4'")

    content = """<?xml version="1.0" encoding="UTF-8"?>
<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
    xmlns:foo="http://foo"
    xmlns:wfs="http://www.opengis.net/wfs/2.0"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    numberMatched="unknown" numberReturned="1" timeStamp="2015-01-01T00:00:00.000Z"
    xsi:schemaLocation="http://www.opengis.net/gml/3.2 http://schemas.opengis.net/gml/3.2.1/gml.xsd
                        http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd
                        http://foo /vsimem/wfs200_endpoint_join?SERVICE=WFS&amp;VERSION=2.0.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=lyr1,lyr2">
  <wfs:member>
    <wfs:Tuple>
      <wfs:member>
        <foo:lyr1 gml:id="lyr1-100">
          <foo:str>123.4</foo:str>
          <foo:shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="bla"><gml:pos>48.5 2.5</gml:pos></gml:Point></foo:shape>
        </foo:lyr1>
      </wfs:member>
      <wfs:member>
        <foo:lyr2 gml:id="lyr2-101">
          <foo:str2>123.4</foo:str2>
          <foo:another_shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="bla"><gml:pos>49 2</gml:pos></gml:Point></foo:another_shape>
        </foo:lyr2>
      </wfs:member>
    </wfs:Tuple>
  </wfs:member>
</wfs:FeatureCollection>
"""

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&STARTINDEX=0&COUNT=1&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CAnd%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3CLiteral%3E123.4%3C%2FLiteral%3E%3C%2FPropertyIsEqualTo%3E%3C%2FAnd%3E%3C%2FFilter%3E',
                           content)

    f = sql_lyr.GetNextFeature()
    if f['lyr1.gml_id'] != 'lyr1-100' or f['lyr1.str'] != '123.4' or \
       f['lyr2.gml_id'] != 'lyr2-101' or f['lyr2.str2'] != '123.4' or \
       f['lyr1.shape'].ExportToWkt() != 'POINT (2.5 48.5)' or \
       f['lyr2.another_shape'].ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()

    ds.ReleaseResultSet(sql_lyr)

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&STARTINDEX=0&COUNT=1&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CAnd%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3CWithin%3E%3CValueReference%3Elyr2%2Fanother_shape%3C%2FValueReference%3E%3Cgml:Envelope%20srsName%3D%22urn:ogc:def:crs:EPSG::4326%22%3E%3Cgml:lowerCorner%3E%2D90%20%2D180%3C%2Fgml:lowerCorner%3E%3Cgml:upperCorner%3E90%20180%3C%2Fgml:upperCorner%3E%3C%2Fgml:Envelope%3E%3C%2FWithin%3E%3C%2FAnd%3E%3C%2FFilter%3E',
                           content)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM lyr1 JOIN lyr2 ON lyr1.str = lyr2.str2 WHERE ST_Within(lyr2.another_shape, ST_MakeEnvelope(-180,-90,180,90))")
    f = sql_lyr.GetNextFeature()
    if f['lyr1.gml_id'] != 'lyr1-100':
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28lyr1,lyr2%29&STARTINDEX=0&COUNT=1&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Elyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Elyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E&SORTBY=str%20DESC',
                           content)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM lyr1 JOIN lyr2 ON lyr1.str = lyr2.str2 ORDER BY lyr1.str DESC")
    f = sql_lyr.GetNextFeature()
    if f['lyr1.gml_id'] != 'lyr1-100':
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL("SELECT * FROM lyr1 JOIN lyr2 ON lyr1.str = lyr2.str2 WHERE lyr1.OGR_GEOMETRY IS NOT NULL")
    gdal.PopErrorHandler()
    assert sql_lyr is None and gdal.GetLastErrorMsg().find('Unsupported WHERE clause') >= 0

    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL("SELECT * FROM lyr1 JOIN lyr2 ON lyr1.OGR_GEOMETRY IS NOT NULL")
    gdal.PopErrorHandler()
    assert sql_lyr is None and gdal.GetLastErrorMsg().find('Unsupported JOIN clause') >= 0

    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL("SELECT 1 FROM lyr1 JOIN lyr2 ON lyr1.str = lyr2.str2")
    gdal.PopErrorHandler()
    assert sql_lyr is None and gdal.GetLastErrorMsg().find('Only column names supported in column selection') >= 0

    ds = None

###############################################################################


def test_ogr_wfs_vsimem_wfs200_join_layer_with_namespace_prefix(with_and_without_streaming):

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="2.0.0">
    <OperationsMetadata>
        <ows:Operation name="GetFeature">
            <ows:Constraint name="CountDefault">
                <ows:NoValues/>
                <ows:DefaultValue>1</ows:DefaultValue>
            </ows:Constraint>
        </ows:Operation>
        <ows:Constraint name="ImplementsResultPaging">
            <ows:NoValues/><ows:DefaultValue>TRUE</ows:DefaultValue>
        </ows:Constraint>
        <ows:Constraint name="ImplementsStandardJoins">
            <ows:NoValues/><ows:DefaultValue>TRUE</ows:DefaultValue>
        </ows:Constraint>
    </OperationsMetadata>
    <FeatureTypeList>
        <FeatureType xmlns:foo="http://foo">
            <Name>foo:lyr1</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
        <FeatureType xmlns:foo="http://foo">
            <Name>foo:lyr2</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
    </FeatureTypeList>
</WFS_Capabilities>
""")

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=DescribeFeatureType&TYPENAME=foo:lyr1,foo:lyr2',
                           """<xsd:schema xmlns:foo="http://foo" xmlns:gml="http://www.opengis.net/gml" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
  <xsd:import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.2.1/base/gml.xsd"/>
  <xsd:complexType name="lyr1Type">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="shape" nillable="true" type="gml:PointPropertyType"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="lyr1" substitutionGroup="gml:_Feature" type="foo:lyr1Type"/>
  <xsd:complexType name="lyr2Type">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str2" nillable="true" type="xsd:string"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="another_shape" nillable="true" type="gml:PointPropertyType"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="lyr2" substitutionGroup="gml:_Feature" type="foo:lyr2Type"/>
</xsd:schema>
""")

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_join')
    sql_lyr = ds.ExecuteSQL('SELECT * FROM lyr1 JOIN lyr2 ON lyr1.str = lyr2.str2')

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28foo:lyr1,foo:lyr2%29&STARTINDEX=0&COUNT=1&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:foo%3D%22http:%2F%2Ffoo%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Efoo:lyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Efoo:lyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E',
                           """<?xml version="1.0" encoding="UTF-8"?>
<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
    xmlns:foo="http://foo"
    xmlns:wfs="http://www.opengis.net/wfs/2.0"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    numberMatched="unknown" numberReturned="1" timeStamp="2015-01-01T00:00:00.000Z"
    xsi:schemaLocation="http://www.opengis.net/gml/3.2 http://schemas.opengis.net/gml/3.2.1/gml.xsd
                        http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd
                        http://foo /vsimem/wfs200_endpoint_join?SERVICE=WFS&amp;VERSION=2.0.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=lyr1,lyr2">
  <wfs:member>
    <wfs:Tuple>
      <wfs:member>
        <foo:lyr1 gml:id="lyr1-100">
          <foo:str>123.4</foo:str>
          <foo:shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="bla"><gml:pos>48.5 2.5</gml:pos></gml:Point></foo:shape>
        </foo:lyr1>
      </wfs:member>
      <wfs:member>
        <foo:lyr2 gml:id="lyr2-101">
          <foo:str2>123.4</foo:str2>
          <foo:another_shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="bla"><gml:pos>49 2</gml:pos></gml:Point></foo:another_shape>
        </foo:lyr2>
      </wfs:member>
    </wfs:Tuple>
  </wfs:member>
</wfs:FeatureCollection>
""")
    f = sql_lyr.GetNextFeature()
    if f['lyr1.gml_id'] != 'lyr1-100' or f['lyr1.str'] != '123.4' or \
       f['lyr2.gml_id'] != 'lyr2-101' or f['lyr2.str2'] != '123.4' or \
       f['lyr1.shape'].ExportToWkt() != 'POINT (2.5 48.5)' or \
       f['lyr2.another_shape'].ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()

    ds.ReleaseResultSet(sql_lyr)

###############################################################################


def test_ogr_wfs_vsimem_wfs200_join_distinct(with_and_without_streaming):

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&REQUEST=GetCapabilities',
                           """<WFS_Capabilities version="2.0.0">
    <OperationsMetadata>
        <ows:Operation name="GetFeature">
            <ows:Constraint name="CountDefault">
                <ows:NoValues/>
                <ows:DefaultValue>4</ows:DefaultValue>
            </ows:Constraint>
        </ows:Operation>
        <ows:Constraint name="ImplementsResultPaging">
            <ows:NoValues/><ows:DefaultValue>TRUE</ows:DefaultValue>
        </ows:Constraint>
        <ows:Constraint name="ImplementsStandardJoins">
            <ows:NoValues/><ows:DefaultValue>TRUE</ows:DefaultValue>
        </ows:Constraint>
    </OperationsMetadata>
    <FeatureTypeList>
        <FeatureType xmlns:foo="http://foo">
            <Name>foo:lyr1</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
        <FeatureType xmlns:foo="http://foo">
            <Name>foo:lyr2</Name>
            <DefaultSRS>urn:ogc:def:crs:EPSG::4326</DefaultSRS>
            <ows:WGS84BoundingBox>
                <ows:LowerCorner>-180.0 -90.0</ows:LowerCorner>
                <ows:UpperCorner>180.0 90.0</ows:UpperCorner>
            </ows:WGS84BoundingBox>
        </FeatureType>
    </FeatureTypeList>
</WFS_Capabilities>
""")

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=DescribeFeatureType&TYPENAME=foo:lyr1,foo:lyr2',
                           """<xsd:schema xmlns:foo="http://foo" xmlns:gml="http://www.opengis.net/gml" xmlns:xsd="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified" targetNamespace="http://foo">
  <xsd:import namespace="http://www.opengis.net/gml" schemaLocation="http://foo/schemas/gml/3.2.1/base/gml.xsd"/>
  <xsd:complexType name="lyr1Type">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str" nillable="true" type="xsd:string"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="int" nillable="true" type="xsd:int"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="int64" nillable="true" type="xsd:long"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="double" nillable="true" type="xsd:double"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="dt" nillable="true" type="xsd:dateTime"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="shape" nillable="true" type="gml:PointPropertyType"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="lyr1" substitutionGroup="gml:_Feature" type="foo:lyr1Type"/>
  <xsd:complexType name="lyr2Type">
    <xsd:complexContent>
      <xsd:extension base="gml:AbstractFeatureType">
        <xsd:sequence>
          <xsd:element maxOccurs="1" minOccurs="0" name="str2" nillable="true" type="xsd:string"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="another_str" nillable="true" type="xsd:string"/>
          <xsd:element maxOccurs="1" minOccurs="0" name="another_shape" nillable="true" type="gml:PointPropertyType"/>
        </xsd:sequence>
      </xsd:extension>
    </xsd:complexContent>
  </xsd:complexType>
  <xsd:element name="lyr2" substitutionGroup="gml:_Feature" type="foo:lyr2Type"/>
</xsd:schema>
""")

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    ds = ogr.Open('WFS:/vsimem/wfs200_endpoint_join')
    sql_lyr = ds.ExecuteSQL('SELECT DISTINCT lyr1.str, lyr1.int, lyr1.int64, lyr1.double, lyr1.dt, lyr2.another_shape FROM lyr1 JOIN lyr2 ON lyr1.str = lyr2.str2')

    gdal.FileFromMemBuffer('/vsimem/wfs200_endpoint_join?SERVICE=WFS&VERSION=2.0.0&REQUEST=GetFeature&TYPENAMES=%28foo:lyr1,foo:lyr2%29&STARTINDEX=0&COUNT=4&FILTER=%3CFilter%20xmlns%3D%22http:%2F%2Fwww.opengis.net%2Ffes%2F2.0%22%20xmlns:foo%3D%22http:%2F%2Ffoo%22%20xmlns:gml%3D%22http:%2F%2Fwww.opengis.net%2Fgml%2F3.2%22%3E%3CPropertyIsEqualTo%3E%3CValueReference%3Efoo:lyr1%2Fstr%3C%2FValueReference%3E%3CValueReference%3Efoo:lyr2%2Fstr2%3C%2FValueReference%3E%3C%2FPropertyIsEqualTo%3E%3C%2FFilter%3E',
                           """<?xml version="1.0" encoding="UTF-8"?>
<wfs:FeatureCollection xmlns:xs="http://www.w3.org/2001/XMLSchema"
    xmlns:foo="http://foo"
    xmlns:wfs="http://www.opengis.net/wfs/2.0"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    numberMatched="unknown" numberReturned="3" timeStamp="2015-01-01T00:00:00.000Z"
    xsi:schemaLocation="http://www.opengis.net/gml/3.2 http://schemas.opengis.net/gml/3.2.1/gml.xsd
                        http://www.opengis.net/wfs/2.0 http://schemas.opengis.net/wfs/2.0/wfs.xsd
                        http://foo /vsimem/wfs200_endpoint_join?SERVICE=WFS&amp;VERSION=2.0.0&amp;REQUEST=DescribeFeatureType&amp;TYPENAME=lyr1,lyr2">
  <wfs:member>
    <wfs:Tuple>
      <wfs:member>
        <foo:lyr1 gml:id="lyr1-1">
          <foo:str>foo</foo:str>
          <foo:int>1</foo:int>
          <foo:int64>9876543210</foo:int64>
          <foo:double>123.4</foo:double>
          <foo:dt>2015-04-17T12:34:56Z</foo:dt>
          <foo:shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="bla"><gml:pos>48.5 2.5</gml:pos></gml:Point></foo:shape>
        </foo:lyr1>
      </wfs:member>
      <wfs:member>
        <foo:lyr2 gml:id="lyr2-1">
          <foo:str2>foo</foo:str2>
          <foo:another_str>foo</foo:another_str>
          <foo:another_shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="bla"><gml:pos>49 2</gml:pos></gml:Point></foo:another_shape>
        </foo:lyr2>
      </wfs:member>
    </wfs:Tuple>
  </wfs:member>
  <wfs:member>
    <wfs:Tuple>
      <wfs:member>
        <foo:lyr1 gml:id="lyr1-1">
          <foo:str>foo</foo:str>
          <foo:int>1</foo:int>
          <foo:int64>9876543210</foo:int64>
          <foo:double>123.4</foo:double>
          <foo:dt>2015-04-17T12:34:56Z</foo:dt>
          <foo:shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="bla"><gml:pos>48.5 2.5</gml:pos></gml:Point></foo:shape>
        </foo:lyr1>
      </wfs:member>
      <wfs:member>
        <foo:lyr2 gml:id="lyr2-2">
          <foo:str2>foo</foo:str2>
          <foo:another_str>bar</foo:another_str>
          <foo:another_shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="bla"><gml:pos>49 2</gml:pos></gml:Point></foo:another_shape>
        </foo:lyr2>
      </wfs:member>
    </wfs:Tuple>
  </wfs:member>
  <wfs:member>
    <wfs:Tuple>
      <wfs:member>
        <foo:lyr1 gml:id="lyr1-2">
          <foo:str>bar</foo:str>
          <foo:int>1</foo:int>
          <foo:int64>9876543210</foo:int64>
          <foo:double>123.4</foo:double>
          <foo:dt>2015-04-17T12:34:56Z</foo:dt>
          <foo:shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="bla"><gml:pos>48.5 2.5</gml:pos></gml:Point></foo:shape>
        </foo:lyr1>
      </wfs:member>
      <wfs:member>
        <foo:lyr2 gml:id="lyr2-3">
          <foo:str2>bar</foo:str2>
          <foo:another_str>bar</foo:another_str>
          <foo:another_shape><gml:Point srsName="urn:ogc:def:crs:EPSG::4326" gml:id="bla"><gml:pos>49 2</gml:pos></gml:Point></foo:another_shape>
        </foo:lyr2>
      </wfs:member>
    </wfs:Tuple>
  </wfs:member>
</wfs:FeatureCollection>
""")
    assert sql_lyr.GetFeatureCount() == 2

    ds.ReleaseResultSet(sql_lyr)

###############################################################################


def test_ogr_wfs_vsimem_cleanup(with_and_without_streaming):

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', None)

    for f in gdal.ReadDir('/vsimem/'):
        gdal.Unlink('/vsimem/' + f)

    


