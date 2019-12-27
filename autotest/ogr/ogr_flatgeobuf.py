#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  FlatGeobuf driver test suite.
# Author:   Björn Harrtell <bjorn@wololo.org>
#
###############################################################################
# Copyright (c) 2018-2019, Björn Harrtell <bjorn@wololo.org>
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

try:
    from BaseHTTPServer import BaseHTTPRequestHandler
except ImportError:
    from http.server import BaseHTTPRequestHandler

import os

from osgeo import ogr
from osgeo import osr
from osgeo import gdal

import gdaltest
import ogrtest
import pytest
import webserver

### utils

def verify_flatgeobuf_copy(name, fids, names):

    if gdaltest.features is None:
        print('Missing features collection')
        return False

    fname = os.path.join('tmp', name + '.fgb')
    ds = ogr.Open(fname)
    if ds is None:
        print('Can not open \'' + fname + '\'')
        return False

    lyr = ds.GetLayer(0)
    if lyr is None:
        print('Missing layer')
        return False

    ######################################################
    # Test attributes
    ret = ogrtest.check_features_against_list(lyr, 'FID', fids)
    if ret != 1:
        print('Wrong values in \'FID\' field')
        return False

    lyr.ResetReading()
    ret = ogrtest.check_features_against_list(lyr, 'NAME', names)
    if ret != 1:
        print('Wrong values in \'NAME\' field')
        return False

    ######################################################
    # Test geometries
    lyr.ResetReading()
    for i in range(len(gdaltest.features)):

        orig_feat = gdaltest.features[i]
        feat = lyr.GetNextFeature()

        if feat is None:
            print('Failed trying to read feature')
            return False

        if ogrtest.check_feature_geometry(feat, orig_feat.GetGeometryRef(),
                                          max_error=0.001) != 0:
            print('Geometry test failed')
            gdaltest.features = None
            return False

    gdaltest.features = None

    lyr = None

    return True


def copy_shape_to_flatgeobuf(name, wkbType, compress=None, options=[]):
    if gdaltest.flatgeobuf_drv is None:
        return False

    if compress is not None:
        if compress[0:5] == '/vsig':
            dst_name = os.path.join('/vsigzip/', 'tmp', name + '.fgb' + '.gz')
        elif compress[0:4] == '/vsiz':
            dst_name = os.path.join('/vsizip/', 'tmp', name + '.fgb' + '.zip')
        elif compress == '/vsistdout/':
            dst_name = compress
        else:
            return False
    else:
        dst_name = os.path.join('tmp', name + '.fgb')

    ds = gdaltest.flatgeobuf_drv.CreateDataSource(dst_name)
    if ds is None:
        return False

    ######################################################
    # Create layer
    lyr = ds.CreateLayer(name, None, wkbType, options)
    if lyr is None:
        return False

    ######################################################
    # Setup schema (all test shapefiles use common schmea)
    ogrtest.quick_create_layer_def(lyr,
                                   [('FID', ogr.OFTReal),
                                    ('NAME', ogr.OFTString)])

    ######################################################
    # Copy in shp

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())

    src_name = os.path.join('data', name + '.shp')
    shp_ds = ogr.Open(src_name)
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.features = []

    while feat is not None:
        gdaltest.features.append(feat)

        dst_feat.SetFrom(feat)
        lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    shp_lyr = None
    lyr = None

    ds = None

    return True

### tests

def test_ogr_flatgeobuf_1():

    gdaltest.flatgeobuf_drv = ogr.GetDriverByName('FlatGeobuf')

    if gdaltest.flatgeobuf_drv is not None:
        return
    pytest.fail()

def test_ogr_flatgeobuf_2():
    fgb_ds = ogr.Open('data/testfgb/poly.fgb')
    fgb_lyr = fgb_ds.GetLayer(0)

    assert fgb_lyr.TestCapability(ogr.OLCFastGetExtent)
    assert fgb_lyr.GetExtent() == (478315.53125, 481645.3125, 4762880.5, 4765610.5)

    # test expected spatial filter feature count consistency
    assert fgb_lyr.TestCapability(ogr.OLCFastFeatureCount)
    c = fgb_lyr.GetFeatureCount()
    assert c == 10
    c = fgb_lyr.SetSpatialFilterRect(478315.531250, 4762880.500000, 481645.312500, 4765610.500000)
    c = fgb_lyr.GetFeatureCount()
    assert c == 10
    c = fgb_lyr.SetSpatialFilterRect(878315.531250, 4762880.500000, 881645.312500, 4765610.500000)
    c = fgb_lyr.GetFeatureCount()
    assert c == 0
    c = fgb_lyr.SetSpatialFilterRect(479586.0,4764618.6,479808.2,4764797.8)
    c = fgb_lyr.GetFeatureCount()
    if ogrtest.have_geos():
        assert c == 4
    else:
        assert c == 5

    # check that ResetReading does not affect subsequent enumeration or filtering
    num = len(list([x for x in fgb_lyr]))
    if ogrtest.have_geos():
        assert num == 4
    else:
        assert num == 5
    fgb_lyr.ResetReading()
    c = fgb_lyr.GetFeatureCount()
    if ogrtest.have_geos():
        assert c == 4
    else:
        assert c == 5
    fgb_lyr.ResetReading()
    num = len(list([x for x in fgb_lyr]))
    if ogrtest.have_geos():
        assert num == 4
    else:
        assert num == 5

def test_ogr_flatgeobuf_2_1():
    fgb_ds = ogr.Open('data/testfgb/poly_no_index.fgb')
    fgb_lyr = fgb_ds.GetLayer(0)

    assert fgb_lyr.TestCapability(ogr.OLCFastGetExtent) is False
    assert fgb_lyr.GetExtent() == (478315.53125, 481645.3125, 4762880.5, 4765610.5)

    # test expected spatial filter feature count consistency
    assert fgb_lyr.TestCapability(ogr.OLCFastFeatureCount) is False
    c = fgb_lyr.GetFeatureCount()
    assert c == 10
    c = fgb_lyr.SetSpatialFilterRect(478315.531250, 4762880.500000, 481645.312500, 4765610.500000)
    c = fgb_lyr.GetFeatureCount()
    assert c == 10
    c = fgb_lyr.SetSpatialFilterRect(878315.531250, 4762880.500000, 881645.312500, 4765610.500000)
    c = fgb_lyr.GetFeatureCount()
    assert c == 0
    c = fgb_lyr.SetSpatialFilterRect(479586.0,4764618.6,479808.2,4764797.8)
    c = fgb_lyr.GetFeatureCount()
    if ogrtest.have_geos():
        assert c == 4
    else:
        assert c == 5

    # check that ResetReading does not affect subsequent enumeration or filtering
    num = len(list([x for x in fgb_lyr]))
    if ogrtest.have_geos():
        assert num == 4
    else:
        assert num == 5
    fgb_lyr.ResetReading()
    c = fgb_lyr.GetFeatureCount()
    if ogrtest.have_geos():
        assert c == 4
    else:
        assert c == 5
    fgb_lyr.ResetReading()
    num = len(list([x for x in fgb_lyr]))
    if ogrtest.have_geos():
        assert num == 4
    else:
        assert num == 5

def wktRoundtrip(expected):
    ds = ogr.GetDriverByName('FlatGeobuf').CreateDataSource('/vsimem/test.fgb')
    g = ogr.CreateGeometryFromWkt(expected)
    lyr = ds.CreateLayer('test', None, g.GetGeometryType(), [])
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(g)
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('/vsimem/test.fgb')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    actual = g.ExportToIsoWkt()
    ds = None

    ogr.GetDriverByName('FlatGeobuf').DeleteDataSource('/vsimem/test.fgb')
    assert not gdal.VSIStatL('/vsimem/test.fgb')

    assert actual == expected

def test_ogr_flatgeobuf_3():
    if gdaltest.flatgeobuf_drv is None:
        pytest.skip()
    wkts = ogrtest.get_wkt_data_series(with_z=True, with_m=True, with_gc=True, with_circular=True, with_surface=True)
    for wkt in wkts:
        wktRoundtrip(wkt)

# Run test_ogrsf
def test_ogr_flatgeobuf_8():
    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/testfgb/poly.fgb')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

def test_ogr_flatgeobuf_9():
    if gdaltest.flatgeobuf_drv is None:
        pytest.skip()

    gdaltest.tests = [
        ['gjpoint', [1], ['Point 1'], ogr.wkbPoint],
        ['gjline', [1], ['Line 1'], ogr.wkbLineString],
        ['gjpoly', [1], ['Polygon 1'], ogr.wkbPolygon],
        ['gjmultipoint', [1], ['MultiPoint 1'], ogr.wkbMultiPoint],
        ['gjmultiline', [2], ['MultiLine 1'], ogr.wkbMultiLineString],
        ['gjmultipoly', [2], ['MultiPoly 1'], ogr.wkbMultiPolygon]
    ]

    for i in range(len(gdaltest.tests)):
        test = gdaltest.tests[i]

        rc = copy_shape_to_flatgeobuf(test[0], test[3])
        assert rc, ('Failed making copy of ' + test[0] + '.shp')

        rc = verify_flatgeobuf_copy(test[0], test[1], test[2])
        assert rc, ('Verification of copy of ' + test[0] + '.shp failed')

    for i in range(len(gdaltest.tests)):
        test = gdaltest.tests[i]

        rc = copy_shape_to_flatgeobuf(test[0], test[3], None, ['SPATIAL_INDEX=NO'])
        assert rc, ('Failed making copy of ' + test[0] + '.shp')

        rc = verify_flatgeobuf_copy(test[0], test[1], test[2])
        assert rc, ('Verification of copy of ' + test[0] + '.shp failed')


# Test support for multiple layers in a directory


def test_ogr_flatgeobuf_directory():
    if gdaltest.flatgeobuf_drv is None:
        pytest.skip()

    ds = ogr.GetDriverByName('FlatGeobuf').CreateDataSource('/vsimem/multi_layer')
    with gdaltest.error_handler(): # name will be laundered
        ds.CreateLayer('foo<', geom_type = ogr.wkbPoint)
    ds.CreateLayer('bar', geom_type = ogr.wkbPoint)
    ds = None

    ds = gdal.OpenEx('/vsimem/multi_layer')
    assert set(ds.GetFileList()) == set(['/vsimem/multi_layer/bar.fgb', '/vsimem/multi_layer/foo_.fgb'])
    assert ds.GetLayer('foo<')
    assert ds.GetLayer('bar')
    ds = None

    ogr.GetDriverByName('FlatGeobuf').DeleteDataSource('/vsimem/multi_layer')
    assert not gdal.VSIStatL('/vsimem/multi_layer')


def test_ogr_flatgeobuf_srs_epsg():
    ds = ogr.GetDriverByName('FlatGeobuf').CreateDataSource('/vsimem/test.fgb')
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    ds.CreateLayer('test', srs = srs, geom_type = ogr.wkbPoint)
    ds = None

    ds = ogr.Open('/vsimem/test.fgb')
    lyr = ds.GetLayer(0)
    srs_got = lyr.GetSpatialRef()
    assert srs_got.IsSame(srs)
    assert srs_got.GetAuthorityName(None) == 'EPSG'
    assert srs_got.GetAuthorityCode(None) == '32631'
    ds = None

    ogr.GetDriverByName('FlatGeobuf').DeleteDataSource('/vsimem/test.fgb')
    assert not gdal.VSIStatL('/vsimem/test.fgb')


def test_ogr_flatgeobuf_srs_other_authority():
    ds = ogr.GetDriverByName('FlatGeobuf').CreateDataSource('/vsimem/test.fgb')
    srs = osr.SpatialReference()
    srs.SetFromUserInput("ESRI:104009")
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    ds.CreateLayer('test', srs = srs, geom_type = ogr.wkbPoint)
    ds = None

    ds = ogr.Open('/vsimem/test.fgb')
    lyr = ds.GetLayer(0)
    srs_got = lyr.GetSpatialRef()
    assert srs_got.IsSame(srs)
    assert srs_got.GetAuthorityName(None) == 'ESRI'
    assert srs_got.GetAuthorityCode(None) == '104009'
    ds = None

    ogr.GetDriverByName('FlatGeobuf').DeleteDataSource('/vsimem/test.fgb')
    assert not gdal.VSIStatL('/vsimem/test.fgb')


def test_ogr_flatgeobuf_srs_no_authority():
    ds = ogr.GetDriverByName('FlatGeobuf').CreateDataSource('/vsimem/test.fgb')
    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=longlat +ellps=clrk66")
    ds.CreateLayer('test', srs = srs, geom_type = ogr.wkbPoint)
    ds = None

    ds = ogr.Open('/vsimem/test.fgb')
    lyr = ds.GetLayer(0)
    srs_got = lyr.GetSpatialRef()
    assert srs_got.IsSame(srs)
    assert srs_got.GetAuthorityName(None) is None
    ds = None

    ogr.GetDriverByName('FlatGeobuf').DeleteDataSource('/vsimem/test.fgb')
    assert not gdal.VSIStatL('/vsimem/test.fgb')

def test_ogr_flatgeobuf_datatypes():
    ds = ogr.Open('data/testfgb/testdatatypes.fgb')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f['int'] == 1
    assert f['int64'] == 1234567890123
    assert f['double'] == 1.25
    assert f['string'] == 'my string'
    assert f['datetime'] == '2019/10/15 12:34:56.789+00'


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
                    f = open('data/testfgb/wfs/get_capabilities.xml', 'rb')
                    content = f.read()
                    f.close()
                    self.wfile.write(content)
                    return

                if self.path == '/fakewfs?SERVICE=WFS&VERSION=1.1.0&REQUEST=DescribeFeatureType&TYPENAME=topp:states':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    f = open('data/testfgb/wfs/describe_feature_type.xml', 'rb')
                    content = f.read()
                    f.close()
                    self.wfile.write(content)
                    return

                if self.path == '/fakewfs?SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=topp:states':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    f = open('data/testfgb/wfs/get_feature.xml', 'rb')
                    content = f.read()
                    f.close()
                    self.wfile.write(content)
                    return

                if self.path == '/fakewfs?OUTPUTFORMAT=application/flatgeobuf&SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=topp:states':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/flatgeobuf')
                    self.end_headers()
                    f = open('data/testfgb/wfs/get_feature.fgb', 'rb')
                    content = f.read()
                    f.close()
                    self.wfile.write(content)
                    return

            return
        except IOError:
            pass

        self.send_error(404, 'File Not Found: %s' % self.path)


@pytest.fixture(autouse=True, scope='module')
def ogr_wfs_init():
    gdaltest.wfs_drv = ogr.GetDriverByName('WFS')

def test_ogr_wfs_fake_wfs_server():
    if gdaltest.wfs_drv is None:
        pytest.skip()

    (process, port) = webserver.launch(handler=WFSHTTPHandler)
    if port == 0:
        pytest.skip()

    gdal.SetConfigOption('OGR_WFS_LOAD_MULTIPLE_LAYER_DEFN', 'NO')
    ds = ogr.Open("WFS:http://127.0.0.1:%d/fakewfs?OUTPUTFORMAT=application/flatgeobuf" % port)
    gdal.SetConfigOption('OGR_WFS_LOAD_MULTIPLE_LAYER_DEFN', None)
    if ds is None:
        webserver.server_stop(process, port)
        pytest.fail('did not managed to open WFS datastore')

    lyr = ds.GetLayerByName('topp:states')
    if lyr == None:
        webserver.server_stop(process, port)
        pytest.fail('did not get expected layer')
    name = lyr.GetName()
    if name != 'topp:states':
        print(name)
        webserver.server_stop(process, port)
        pytest.fail('did not get expected layer name (got %s)' % name)

    feat = lyr.GetNextFeature()
    if feat.GetField('STATE_NAME') != 'Illinois' or \
       ogrtest.check_feature_geometry(feat, 'MULTIPOLYGON (((-88.071564 37.51099,-88.087883 37.476273,-88.311707 37.442852,-88.359177 37.409309,-88.419853 37.420292,-88.467644 37.400757,-88.511322 37.296852,-88.501427 37.257782,-88.450699 37.205669,-88.422516 37.15691,-88.45047 37.098671,-88.476799 37.072144,-88.4907 37.06818,-88.517273 37.06477,-88.559273 37.072815,-88.61422 37.109047,-88.68837 37.13541,-88.739113 37.141182,-88.746506 37.152107,-88.863289 37.202194,-88.932503 37.218407,-88.993172 37.220036,-89.065033 37.18586,-89.116821 37.112137,-89.146347 37.093185,-89.169548 37.064236,-89.174332 37.025711,-89.150246 36.99844,-89.12986 36.988113,-89.193512 36.986771,-89.210052 37.028973,-89.237679 37.041733,-89.264053 37.087124,-89.284233 37.091244,-89.303291 37.085384,-89.3097 37.060909,-89.264244 37.027733,-89.262001 37.008686,-89.282768 36.999207,-89.310982 37.009682,-89.38295 37.049213,-89.37999 37.099083,-89.423798 37.137203,-89.440521 37.165318,-89.468216 37.224266,-89.465309 37.253731,-89.489594 37.256001,-89.513885 37.276402,-89.513885 37.304962,-89.50058 37.329441,-89.468742 37.339409,-89.435738 37.355717,-89.427574 37.411018,-89.453621 37.453186,-89.494781 37.491726,-89.524971 37.571957,-89.513367 37.615929,-89.51918 37.650375,-89.513374 37.67984,-89.521523 37.694798,-89.581436 37.706104,-89.666458 37.745453,-89.675858 37.78397,-89.691055 37.804794,-89.728447 37.840992,-89.851715 37.905064,-89.861046 37.905487,-89.866814 37.891876,-89.900551 37.875904,-89.937874 37.878044,-89.978912 37.911884,-89.958229 37.963634,-90.010811 37.969318,-90.041924 37.993206,-90.119339 38.032272,-90.134712 38.053951,-90.207527 38.088905,-90.254059 38.122169,-90.289635 38.166817,-90.336716 38.188713,-90.364769 38.234299,-90.369347 38.323559,-90.358688 38.36533,-90.339607 38.390846,-90.301842 38.427357,-90.265785 38.518688,-90.26123 38.532768,-90.240944 38.562805,-90.183708 38.610271,-90.183578 38.658772,-90.20224 38.700363,-90.196571 38.723965,-90.163399 38.773098,-90.135178 38.785484,-90.121727 38.80051,-90.113121 38.830467,-90.132812 38.853031,-90.243927 38.914509,-90.278931 38.924717,-90.31974 38.924908,-90.413071 38.96233,-90.469841 38.959179,-90.530426 38.891609,-90.570328 38.871326,-90.627213 38.880795,-90.668877 38.935253,-90.70607 39.037792,-90.707588 39.058178,-90.690399 39.0937,-90.716736 39.144211,-90.718193 39.195873,-90.732338 39.224747,-90.738083 39.24781,-90.779343 39.296803,-90.850494 39.350452,-90.947891 39.400585,-91.036339 39.444412,-91.064384 39.473984,-91.093613 39.528927,-91.156189 39.552593,-91.203247 39.600021,-91.317665 39.685917,-91.367088 39.72464,-91.373421 39.761272,-91.381714 39.803772,-91.449188 39.863049,-91.450989 39.885242,-91.434052 39.901829,-91.430389 39.921837,-91.447243 39.946064,-91.487289 40.005753,-91.504005 40.066711,-91.516129 40.134544,-91.506546 40.200459,-91.498932 40.251377,-91.486694 40.309624,-91.448593 40.371902,-91.418816 40.386875,-91.385757 40.392361,-91.372757 40.402988,-91.385399 40.44725,-91.374794 40.503654,-91.382103 40.528496,-91.412872 40.547993,-91.411118 40.572971,-91.37561 40.603439,-91.262062 40.639545,-91.214912 40.643818,-91.162498 40.656311,-91.129158 40.682148,-91.119987 40.705402,-91.092751 40.761547,-91.088905 40.833729,-91.04921 40.879585,-90.983276 40.923927,-90.960709 40.950504,-90.954651 41.070362,-90.957787 41.104359,-90.990341 41.144371,-91.018257 41.165825,-91.05632 41.176258,-91.101524 41.231522,-91.102348 41.267818,-91.07328 41.334896,-91.055786 41.401379,-91.027489 41.423508,-91.000694 41.431084,-90.949654 41.421234,-90.844139 41.444622,-90.7799 41.449821,-90.708214 41.450062,-90.658791 41.462318,-90.6007 41.509586,-90.54084 41.52597,-90.454994 41.527546,-90.434967 41.543579,-90.423004 41.567272,-90.348366 41.586849,-90.339348 41.602798,-90.341133 41.64909,-90.326027 41.722736,-90.304886 41.756466,-90.25531 41.781738,-90.195839 41.806137,-90.154518 41.930775,-90.14267 41.983963,-90.150536 42.033428,-90.168098 42.061043,-90.166649 42.103745,-90.176086 42.120502,-90.191574 42.122688,-90.230934 42.159721,-90.323601 42.197319,-90.367729 42.210209,-90.407173 42.242645,-90.417984 42.263924,-90.427681 42.340633,-90.441597 42.360073,-90.491043 42.388783,-90.563583 42.421837,-90.605827 42.46056,-90.648346 42.475643,-90.651772 42.494698,-90.638329 42.509361,-90.419975 42.508362,-89.923569 42.504108,-89.834618 42.50346,-89.400497 42.49749,-89.359444 42.497906,-88.939079 42.490864,-88.764954 42.490906,-88.70652 42.489655,-88.297897 42.49197,-88.194702 42.489613,-87.79731 42.489132,-87.836945 42.314213,-87.760239 42.156456,-87.670547 42.059822,-87.612625 41.847332,-87.529861 41.723591,-87.532646 41.469715,-87.532448 41.301304,-87.531731 41.173756,-87.532021 41.00993,-87.532669 40.745411,-87.53717 40.49461,-87.535675 40.483246,-87.535339 40.166195,-87.535774 39.887302,-87.535576 39.609341,-87.538567 39.477448,-87.540215 39.350525,-87.597664 39.338268,-87.625237 39.307404,-87.610619 39.297661,-87.615799 39.281418,-87.606895 39.258163,-87.584564 39.248753,-87.588593 39.208466,-87.594208 39.198128,-87.607925 39.196068,-87.644257 39.168507,-87.670326 39.146679,-87.659454 39.130653,-87.662262 39.113468,-87.631668 39.103943,-87.630867 39.088974,-87.612007 39.084606,-87.58532 39.062435,-87.581749 38.995743,-87.591858 38.994083,-87.547905 38.977077,-87.53347 38.963703,-87.530182 38.931919,-87.5392 38.904861,-87.559059 38.869812,-87.550507 38.857891,-87.507889 38.795559,-87.519028 38.776699,-87.508003 38.769722,-87.508316 38.736633,-87.543892 38.685974,-87.588478 38.672169,-87.625191 38.642811,-87.628647 38.622917,-87.619827 38.599209,-87.640594 38.593178,-87.652855 38.573872,-87.672943 38.547424,-87.65139 38.515369,-87.653534 38.500443,-87.679909 38.504005,-87.692818 38.481533,-87.756096 38.466125,-87.758659 38.457096,-87.738953 38.44548,-87.748428 38.417965,-87.784019 38.378124,-87.834503 38.352524,-87.850082 38.286098,-87.863007 38.285362,-87.874039 38.316788,-87.883446 38.315552,-87.888466 38.300659,-87.914108 38.281048,-87.913651 38.302345,-87.925919 38.304771,-87.980019 38.241085,-87.986008 38.234814,-87.977928 38.200714,-87.932289 38.171131,-87.931992 38.157528,-87.950569 38.136913,-87.973503 38.13176,-88.018547 38.103302,-88.012329 38.092346,-87.964867 38.096748,-87.975296 38.073307,-88.034729 38.054085,-88.043091 38.04512,-88.041473 38.038303,-88.021698 38.033531,-88.029213 38.008236,-88.021706 37.975056,-88.042511 37.956264,-88.041771 37.934498,-88.064621 37.929783,-88.078941 37.944,-88.084 37.92366,-88.030441 37.917591,-88.026588 37.905758,-88.044868 37.896004,-88.100082 37.90617,-88.101456 37.895306,-88.075737 37.867809,-88.034241 37.843746,-88.042137 37.827522,-88.089264 37.831249,-88.086029 37.817612,-88.035576 37.805683,-88.072472 37.735401,-88.133636 37.700745,-88.15937 37.660686,-88.157631 37.628479,-88.134171 37.583572,-88.071564 37.51099)))',
                                      max_error=0.00001) != 0:
        feat.DumpReadable()
        webserver.server_stop(process, port)
        pytest.fail('did not get expected feature')

    webserver.server_stop(process, port)
