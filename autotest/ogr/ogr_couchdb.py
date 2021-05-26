#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  CouchDB driver testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2014, Even Rouault <even dot rouault at spatialys.com>
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
import uuid


import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
import pytest


pytestmark = pytest.mark.require_driver('CouchDB')


###############################################################################

@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    if 'COUCHDB_TEST_SERVER' in os.environ:
        ogrtest.couchdb_test_server = os.environ['COUCHDB_TEST_SERVER']
    else:
        ogrtest.couchdb_test_server = 'http://127.0.0.1:5984'
    ogrtest.couchdb_temp_layer_name = 'layer_' + str(uuid.uuid1()).replace('-', '_')

    if gdaltest.gdalurlopen(ogrtest.couchdb_test_server) is None:
        pytest.skip('cannot open %s' % ogrtest.couchdb_test_server)

    yield

    ds = ogr.Open('couchdb:%s' % ogrtest.couchdb_test_server, update=1)
    assert ds is not None
    ds.ExecuteSQL('DELLAYER:' + ogrtest.couchdb_temp_layer_name)

###############################################################################
# Basic test


def test_ogr_couchdb_1():

    gdal.VectorTranslate('CouchDB:' + ogrtest.couchdb_test_server, 'data/poly.shp',
                         format='CouchDB',
                         layerName=ogrtest.couchdb_temp_layer_name,
                         layerCreationOptions=['UPDATE_PERMISSIONS=ALL'])
    ds = ogr.Open('couchdb:%s' % ogrtest.couchdb_test_server, update=1)
    assert ds is not None
    lyr = ds.GetLayerByName(ogrtest.couchdb_temp_layer_name)
    f = lyr.GetNextFeature()
    if f['AREA'] != 215229.266 or f['EAS_ID'] != '168' or f.GetGeometryRef() is None:
        f.DumpReadable()
        pytest.fail()
    ds.ExecuteSQL('DELLAYER:' + ogrtest.couchdb_temp_layer_name)

###############################################################################
# Test null / unset


def test_ogr_couchdb_2():

    ds = ogr.Open('couchdb:%s' % ogrtest.couchdb_test_server, update=1)
    assert ds is not None
    lyr = ds.CreateLayer(ogrtest.couchdb_temp_layer_name, geom_type=ogr.wkbNone, options=['UPDATE_PERMISSIONS=ALL'])
    lyr.CreateField(ogr.FieldDefn('str_field', ogr.OFTString))

    f = ogr.Feature(lyr.GetLayerDefn())
    f['str_field'] = 'foo'
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldNull('str_field')
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    f = lyr.GetNextFeature()
    if f['str_field'] != 'foo':
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f['str_field'] is not None:
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f.IsFieldSet('str_field'):
        f.DumpReadable()
        pytest.fail()


