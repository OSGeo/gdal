#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test DB2 vector driver
#
# Author:   David Adler <dadler@adtechgeospatial.com>
#
###############################################################################
# Copyright (c) 2015, David Adler <dadler@adtechgeospatial.com>
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

# Before this test is run with a real database connection,
# set DB2_TEST_SERVER to point to the server and table to be used, like:
# DB2_TEST_SERVER=Database=SAMP105;DSN=SAMP105A;tables=TEST.ZIPPOINT
# or
# DB2_TEST_SERVER=Database=SAMP105;Driver={IBM DB2 CLIDRIVER};Hostname=<>;Port=<>;PROTOCOL=TCPIP;UID=<>;PWD=<>;tables=TEST.ZIPPOINT
#
# Also before running, the db2 setup script must be run to create the
# needed SRS and test tables
# In a DB2 command window, connect to a database and issue a command like
# db2 -tvf ogr\data\db2\db2_setup.sql
#
# These tests currently only run on Windows

import os


import ogrtest
from osgeo import ogr
import pytest

pytestmark = pytest.mark.require_driver('DB2ODBC')

###############################################################################
# Test if environment variable for DB2 connection is set and we can connect


@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    if 'DB2_TEST_SERVER' in os.environ:
        ogrtest.db2_test_server = "DB2ODBC:" + os.environ['DB2_TEST_SERVER']
    else:
        pytest.skip('Environment variable DB2_TEST_SERVER not found')

###############################################################################
# Test GetFeatureCount()


def test_ogr_db2_GetFeatureCount():

    ds = ogr.Open(ogrtest.db2_test_server)

    assert ds is not None

    lyr = ds.GetLayer(0)

    assert lyr is not None

    count = lyr.GetFeatureCount()
    assert count == 5, 'did not get expected feature count'

###############################################################################
# Test GetSpatialRef()


def test_ogr_db2_GetSpatialRef():

    ds = ogr.Open(ogrtest.db2_test_server)

    assert ds is not None

    lyr = ds.GetLayer(0)

    assert lyr is not None

    sr = lyr.GetSpatialRef()

    assert sr is not None, 'did not get expected srs'

    txt = sr.ExportToWkt()

    assert txt.find('GEOGCS[\"GCS_WGS_1984') != -1, 'did not get expected srs'


###############################################################################
# Test GetExtent()
def test_ogr_db2_GetExtent():

    ds = ogr.Open(ogrtest.db2_test_server)

    assert ds is not None

    lyr = ds.GetLayer(0)

    assert lyr is not None

    extent = lyr.GetExtent()
    assert extent is not None, 'did not get extent'

    assert extent == (-122.030745, -121.95672, 37.278665, 37.440885), \
        'did not get expected extent'

###############################################################################
# Test GetFeature()


def test_ogr_db2_GetFeature():

    ds = ogr.Open(ogrtest.db2_test_server)

    assert ds is not None

    lyr = ds.GetLayer(0)

    assert lyr is not None

    feat = lyr.GetFeature(5)
    assert feat is not None, 'did not get a feature'

    if feat.GetField('ZIP') != '95008':
        feat.DumpReadable()
        pytest.fail('did not get expected feature')

    
###############################################################################
# Test SetSpatialFilter()


def test_ogr_db2_SetSpatialFilter():

    ds = ogr.Open(ogrtest.db2_test_server)

    assert ds is not None

    lyr = ds.GetLayer(0)

    assert lyr is not None

# set a query envelope so we only get one feature
    lyr.SetSpatialFilterRect(-122.02, 37.42, -122.01, 37.43)

    count = lyr.GetFeatureCount()

    assert count == 1, 'did not get expected feature count (1)'

    feat = lyr.GetNextFeature()
    assert feat is not None, 'did not get a feature'

    if feat.GetField('ZIP') != '94089':
        feat.DumpReadable()
        pytest.fail('did not get expected feature')

# start over with a larger envelope to get 3 out of 5 of the points
    lyr.ResetReading()
    lyr.SetSpatialFilterRect(-122.04, 37.30, -121.80, 37.43)

    count = lyr.GetFeatureCount()

    assert count == 3, 'did not get expected feature count (3)'

# iterate through the features to make sure we get the same count
    count = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        count = count + 1
        feat = lyr.GetNextFeature()

    assert count == 3, 'did not get expected feature count (3)'

#
# test what capabilities the DB2 driver provides
#


def test_ogr_db2_capabilities():

    ds = ogr.Open(ogrtest.db2_test_server)

    assert ds is not None

    layer = ds.GetLayer()
    capabilities = [
        ogr.OLCRandomRead,
        ogr.OLCSequentialWrite,
        ogr.OLCRandomWrite,
        ogr.OLCFastSpatialFilter,
        ogr.OLCFastFeatureCount,
        ogr.OLCFastGetExtent,
        ogr.OLCCreateField,
        ogr.OLCDeleteField,
        ogr.OLCReorderFields,
        ogr.OLCAlterFieldDefn,
        ogr.OLCTransactions,
        ogr.OLCDeleteFeature,
        ogr.OLCFastSetNextByIndex,
        ogr.OLCStringsAsUTF8,
        ogr.OLCIgnoreFields
    ]

    print("Layer Capabilities:")
    for cap in capabilities:
        print("  %s = %s" % (cap, layer.TestCapability(cap)))

