#!/usr/bin/env python
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
import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr

###############################################################################
# Test if driver is available

def ogr_db2_check_driver():

    ogrtest.db2_drv = None

    try:
        ogrtest.db2_drv = ogr.GetDriverByName('DB2ODBC')
    except:
        pass

    if ogrtest.db2_drv is None:
        return 'skip'

    return 'success'

###############################################################################
# Test if environment variable for DB2 connection is set and we can connect

def ogr_db2_init():

    if ogrtest.db2_drv is None:
        return 'skip'

    if 'DB2_TEST_SERVER' in os.environ:
        ogrtest.db2_test_server = "DB2ODBC:" + os.environ['DB2_TEST_SERVER']
    else:
        gdaltest.post_reason('Environment variable DB2_TEST_SERVER not found')
        ogrtest.db2_drv = None
        return 'skip'

    return 'success'
###############################################################################
# Test GetFeatureCount()

def ogr_db2_GetFeatureCount():

    if ogrtest.db2_drv is None:
        return 'skip'

    ds = ogr.Open(ogrtest.db2_test_server)

    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)

    if lyr is None:
        return 'fail'

    count = lyr.GetFeatureCount()
    if count != 5:
        gdaltest.post_reason('did not get expected feature count')
        return 'fail'

    return 'success'

###############################################################################
# Test GetSpatialRef()
def ogr_db2_GetSpatialRef():

    if ogrtest.db2_drv is None:
        return 'skip'

    ds = ogr.Open(ogrtest.db2_test_server)

    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)

    if lyr is None:
        return 'fail'

    sr = lyr.GetSpatialRef()

    if sr is None:
        gdaltest.post_reason('did not get expected srs')
        return 'fail'

    txt = sr.ExportToWkt()

    if txt.find('GEOGCS[\"GCS_WGS_1984') == -1:
        gdaltest.post_reason('did not get expected srs')
        print(txt)
        return 'fail'

    return 'success'


###############################################################################
# Test GetExtent()
def ogr_db2_GetExtent():

    if ogrtest.db2_drv is None:
        return 'skip'

    ds = ogr.Open(ogrtest.db2_test_server)

    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)

    if lyr is None:
        return 'fail'

    extent = lyr.GetExtent()
    if extent is None:
        gdaltest.post_reason('did not get extent')
        return 'fail'

    if extent != (-122.030745, -121.95672, 37.278665, 37.440885):
        gdaltest.post_reason('did not get expected extent')
        print(extent)
        return 'fail'

    return 'success'

###############################################################################
# Test GetFeature()
def ogr_db2_GetFeature():

    if ogrtest.db2_drv is None:
        return 'skip'

    ds = ogr.Open(ogrtest.db2_test_server)

    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)

    if lyr is None:
        return 'fail'

    feat = lyr.GetFeature(5)
    if feat is None:
        gdaltest.post_reason('did not get a feature')
        return 'fail'

    if feat.GetField('ZIP') != '95008':
        gdaltest.post_reason('did not get expected feature')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Test SetSpatialFilter()
def ogr_db2_SetSpatialFilter():

    if ogrtest.db2_drv is None:
        return 'skip'

    ds = ogr.Open(ogrtest.db2_test_server)

    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)

    if lyr is None:
        return 'fail'

# set a query envelope so we only get one feature
    lyr.SetSpatialFilterRect( -122.02, 37.42, -122.01, 37.43 )

    count = lyr.GetFeatureCount()

    if count != 1:
        gdaltest.post_reason('did not get expected feature count (1)')
        print(count)
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('did not get a feature')
        return 'fail'

    if feat.GetField('ZIP') != '94089':
        gdaltest.post_reason('did not get expected feature')
        feat.DumpReadable()
        return 'fail'

# start over with a larger envelope to get 3 out of 5 of the points
    lyr.ResetReading()
    lyr.SetSpatialFilterRect( -122.04, 37.30, -121.80, 37.43 )

    count = lyr.GetFeatureCount()

    if count != 3:
        gdaltest.post_reason('did not get expected feature count (3)')
        print(count)
        return 'fail'

# iterate through the features to make sure we get the same count
    count = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        count = count + 1
        feat = lyr.GetNextFeature()

    if count != 3:
        gdaltest.post_reason('did not get expected feature count (3)')
        print(count)
        return 'fail'

    return 'success'

#
# test what capabilities the DB2 driver provides
#
def ogr_db2_capabilities():

    if ogrtest.db2_drv is None:
        return 'skip'

    ds = ogr.Open(ogrtest.db2_test_server)

    if ds is None:
        return 'fail'

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
    return 'success'

def ogr_db2_listdrivers():
    cnt = ogr.GetDriverCount()
    print cnt
    formatsList = []  # Empty List

    for i in range(cnt):
        driver = ogr.GetDriver(i)
        driverName = driver.GetName()
#        print driverName
        if not driverName in formatsList:
            formatsList.append(driverName)

    formatsList.sort() # Sorting the messy list of ogr drivers

    for i in formatsList:
        print i

    return 'success'

gdaltest_list = [
    ogr_db2_check_driver,
    ogr_db2_init,
#    ogr_db2_listdrivers,
    ogr_db2_GetSpatialRef,
    ogr_db2_GetExtent,
    ogr_db2_GetFeature,
    ogr_db2_SetSpatialFilter,
    ogr_db2_capabilities,
    ogr_db2_GetFeatureCount
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_db2' )
    if os.name == 'nt':
        gdaltest.run_tests( gdaltest_list )
    else:
        print "These tests only run on Windows"
    gdaltest.summarize()
