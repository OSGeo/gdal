#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test MSSQLSpatial driver functionality.
# Author:   Even Rouault <even dot rouault at mines dash paris dot ogr>
#
###############################################################################
# Copyright (c) 2018, Even Rouault <even dot rouault at spatialys.com>
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

import sys

sys.path.append('../pymod')

import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr

###############################################################################
# Open Database.


def ogr_mssqlspatial_1():

    gdaltest.mssqlspatial_ds = None

    if ogr.GetDriverByName('MSSQLSpatial') is None:
        return 'skip'

    gdaltest.mssqlspatial_dsname = gdal.GetConfigOption(
        'OGR_MSSQL_CONNECTION_STRING',
        # localhost doesn't work under chroot
        'MSSQL:server=127.0.0.1;database=TestDB;driver=ODBC Driver 17 for SQL Server;UID=SA;PWD=DummyPassw0rd')
    gdaltest.mssqlspatial_ds = ogr.Open(gdaltest.mssqlspatial_dsname, update=1)
    if gdaltest.mssqlspatial_ds is not None:
        return 'success'

    return 'skip'

###############################################################################
# Create table from data/poly.shp


def ogr_mssqlspatial_2():

    if gdaltest.mssqlspatial_ds is None:
        return 'skip'

    shp_ds = ogr.Open('data/poly.shp')
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)

    ######################################################
    # Create Layer
    gdaltest.mssqlspatial_lyr = gdaltest.mssqlspatial_ds.CreateLayer(
        'tpoly', srs=shp_lyr.GetSpatialRef())

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(gdaltest.mssqlspatial_lyr,
                                   [('AREA', ogr.OFTReal),
                                    ('EAS_ID', ogr.OFTInteger),
                                    ('PRFEDEA', ogr.OFTString),
                                    ('SHORTNAME', ogr.OFTString, 8),
                                    ('INT64', ogr.OFTInteger64)])

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(
        feature_def=gdaltest.mssqlspatial_lyr.GetLayerDefn())

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    while feat is not None:

        gdaltest.poly_feat.append(feat)

        dst_feat.SetFrom(feat)
        dst_feat.SetField('INT64', 1234567890123)
        gdaltest.mssqlspatial_lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    dst_feat = None

    if gdaltest.mssqlspatial_lyr.GetFeatureCount() != shp_lyr.GetFeatureCount():
        gdaltest.post_reason('not matching feature count')
        return 'fail'

    if not gdaltest.mssqlspatial_lyr.GetSpatialRef().IsSame(shp_lyr.GetSpatialRef()):
        gdaltest.post_reason('not matching spatial ref')
        return 'fail'

    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.


def ogr_mssqlspatial_3():
    if gdaltest.mssqlspatial_ds is None:
        return 'skip'

    if gdaltest.mssqlspatial_lyr.GetGeometryColumn() != 'ogr_geometry':
        gdaltest.post_reason('fail')
        print(gdaltest.mssqlspatial_lyr.GetGeometryColumn())
        return 'fail'

    if gdaltest.mssqlspatial_lyr.GetFeatureCount() != 10:
        gdaltest.post_reason('GetFeatureCount() returned %d instead of 10' %
                             gdaltest.mssqlspatial_lyr.GetFeatureCount())
        return 'fail'

    expect = [168, 169, 166, 158, 165]

    gdaltest.mssqlspatial_lyr.SetAttributeFilter('eas_id < 170')
    tr = ogrtest.check_features_against_list(gdaltest.mssqlspatial_lyr,
                                             'eas_id', expect)

    if gdaltest.mssqlspatial_lyr.GetFeatureCount() != 5:
        gdaltest.post_reason('GetFeatureCount() returned %d instead of 5' %
                             gdaltest.mssqlspatial_lyr.GetFeatureCount())
        return 'fail'

    gdaltest.mssqlspatial_lyr.SetAttributeFilter(None)

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.mssqlspatial_lyr.GetNextFeature()

        if ogrtest.check_feature_geometry(read_feat, orig_feat.GetGeometryRef(),
                                          max_error=0.001) != 0:
            return 'fail'

        for fld in range(3):
            if orig_feat.GetField(fld) != read_feat.GetField(fld):
                gdaltest.post_reason('Attribute %d does not match' % fld)
                return 'fail'
        if read_feat.GetField('INT64') != 1234567890123:
            gdaltest.post_reason('failure')
            return 'fail'

        read_feat = None
        orig_feat = None

    gdaltest.poly_feat = None
    gdaltest.shp_ds = None

    return 'success' if tr else 'fail'

###############################################################################
#


def ogr_mssqlspatial_cleanup():

    if gdaltest.mssqlspatial_ds is None:
        return 'skip'

    gdaltest.mssqlspatial_ds = None

    gdaltest.mssqlspatial_ds = ogr.Open(gdaltest.mssqlspatial_dsname, update=1)
    gdaltest.mssqlspatial_ds.ExecuteSQL('DROP TABLE tpoly')

    gdaltest.mssqlspatial_ds = None

    return 'success'


gdaltest_list = [
    ogr_mssqlspatial_1,
    #    ogr_mssqlspatial_cleanup,
    ogr_mssqlspatial_2,
    ogr_mssqlspatial_3,
    ogr_mssqlspatial_cleanup
]

if __name__ == '__main__':

    gdaltest.setup_run('ogr_mssqlspatial')

    gdaltest.run_tests(gdaltest_list)

    gdaltest.summarize()
