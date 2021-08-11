#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Cloudant driver testing.
# Author:   Norman Barker, norman at cloudant com
#          Based on the CouchDB driver
#
###############################################################################
#  Copyright (c) 2014, Norman Barker <norman at cloudant com>
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


import gdaltest
import ogrtest
from osgeo import ogr
import pytest


pytestmark = [pytest.mark.require_driver('Cloudant'),
              pytest.mark.skipif('GDAL_ENABLE_DEPRECATED_DRIVER_CLOUDANT' not in os.environ,
                                 reason='GDAL_ENABLE_DEPRECATED_DRIVER_CLOUDANT not set')]


###############################################################################

@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    if 'CLOUDANT_TEST_SERVER' in os.environ:
        ogrtest.cloudant_test_server = os.environ['CLOUDANT_TEST_SERVER']
        ogrtest.cloudant_test_url = ogrtest.cloudant_test_server
    else:
        ogrtest.cloudant_test_server = 'https://yescandrephereddescamill:I1rhuWEIVDRvbpoQNOBW3pGV@normanb.cloudant.com'
        ogrtest.cloudant_test_url = 'https://normanb.cloudant.com'

    ogrtest.cloudant_test_layer = 'gdaltest'

    if gdaltest.gdalurlopen(ogrtest.cloudant_test_url) is None:
        pytest.skip('cannot open %s' % ogrtest.cloudant_test_url)

###############################################################################
# Test GetFeatureCount()


def test_ogr_cloudant_GetFeatureCount():

    ds = ogr.Open('cloudant:%s/%s' % (ogrtest.cloudant_test_server, ogrtest.cloudant_test_layer))
    assert ds is not None

    lyr = ds.GetLayer(0)
    assert lyr is not None

    count = lyr.GetFeatureCount()
    assert count == 52, 'did not get expected feature count'

###############################################################################
# Test GetNextFeature()


def test_ogr_cloudant_GetNextFeature():

    ds = ogr.Open('cloudant:%s/%s' % (ogrtest.cloudant_test_server, ogrtest.cloudant_test_layer))
    assert ds is not None

    lyr = ds.GetLayer(0)
    assert lyr is not None

    feat = lyr.GetNextFeature()
    assert feat is not None, 'did not get expected feature'
    if feat.GetField('_id') != '0400000US01':
        feat.DumpReadable()
        pytest.fail('did not get expected feature')


###############################################################################
# Test GetSpatialRef()


def test_ogr_cloudant_GetSpatialRef():

    ds = ogr.Open('cloudant:%s/%s' % (ogrtest.cloudant_test_server, ogrtest.cloudant_test_layer))
    assert ds is not None

    lyr = ds.GetLayer(0)
    assert lyr is not None

    sr = lyr.GetSpatialRef()

    if sr is None:
        return


###############################################################################
# Test GetExtent()


def test_ogr_cloudant_GetExtent():

    ds = ogr.Open('cloudant:%s/%s' % (ogrtest.cloudant_test_server, ogrtest.cloudant_test_layer))
    assert ds is not None

    lyr = ds.GetLayer(0)
    assert lyr is not None

    extent = lyr.GetExtent()
    assert extent is not None, 'did not get expected extent'

    assert extent == (-179.14734, 179.77847, 17.884813, 71.352561), \
        'did not get expected extent'

###############################################################################
# Test SetSpatialFilter()


def test_ogr_cloudant_SetSpatialFilter():

    if not ogrtest.have_geos():
        pytest.skip()

    ds = ogr.Open('cloudant:%s/%s' % (ogrtest.cloudant_test_server, ogrtest.cloudant_test_layer))
    assert ds is not None

    lyr = ds.GetLayer(0)
    assert lyr is not None

    lyr.SetSpatialFilterRect(-104.9847, 39.7392, -104.9847, 39.7392)

    feat = lyr.GetNextFeature()
    assert feat is not None, 'did not get expected feature'
    if feat.GetField('NAME') != 'Colorado':
        feat.DumpReadable()
        pytest.fail('did not get expected feature')




