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
from osgeo import osr

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
    if gdaltest.mssqlspatial_ds is None:
        return 'skip'

    # Fetch and store the major-version number of the SQL Server engine in use
    sql_lyr = gdaltest.mssqlspatial_ds.ExecuteSQL(
        'SELECT SERVERPROPERTY(\'ProductVersion\')')
    feat = sql_lyr.GetNextFeature()
    gdaltest.mssqlspatial_version = feat.GetFieldAsString(0)
    gdaltest.mssqlspatial_ds.ReleaseResultSet(sql_lyr)

    gdaltest.mssqlspatial_version_major = -1
    if '.' in gdaltest.mssqlspatial_version:
        version_major_str = gdaltest.mssqlspatial_version[
            0:gdaltest.mssqlspatial_version.find('.')]
        if version_major_str.isdigit():
            gdaltest.mssqlspatial_version_major = int(version_major_str)

    # Check whether the database server provides support for Z and M values,
    # available since SQL Server 2012
    gdaltest.mssqlspatial_has_z_m = (gdaltest.mssqlspatial_version_major >= 11)

    return 'success'

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
# Write more features with a bunch of different geometries, and verify the
# geometries are still OK.


def ogr_mssqlspatial_4():
    if gdaltest.mssqlspatial_ds is None:
        return 'skip'

    dst_feat = ogr.Feature(
        feature_def=gdaltest.mssqlspatial_lyr.GetLayerDefn())
    wkt_list = ['10', '2', '1', '4', '5', '6']

    # If the database engine supports 3D features, include one in the tests
    if gdaltest.mssqlspatial_has_z_m:
        wkt_list.append('3d_1')

    for item in wkt_list:
        wkt_filename = 'data/wkb_wkt/' + item + '.wkt'
        wkt = open(wkt_filename).read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Write geometry as a new feature.

        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField('PRFEDEA', item)
        dst_feat.SetFID(-1)
        if gdaltest.mssqlspatial_lyr.CreateFeature(dst_feat) \
           != ogr.OGRERR_NONE:
            gdaltest.post_reason('CreateFeature failed creating feature ' +
                                 'from file "' + wkt_filename + '"')
            return 'fail'

        ######################################################################
        # Before reading back the record, verify that the newly added feature 
        # is returned from the CreateFeature method with a newly assigned FID.
        
        if dst_feat.GetFID() == -1:
            gdaltest.post_reason('Assigned FID was not returned in the new feature')
            return 'fail'

        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.mssqlspatial_lyr.SetAttributeFilter("PRFEDEA = '%s'" % item)
        feat_read = gdaltest.mssqlspatial_lyr.GetNextFeature()
        geom_read = feat_read.GetGeometryRef()

        if ogrtest.check_feature_geometry(feat_read, geom) != 0:
            print(item)
            print(wkt)
            print(geom_read)
            return 'fail'

        feat_read.Destroy()

    dst_feat.Destroy()
    gdaltest.mssqlspatial_lyr.ResetReading()  # to close implicit transaction

    return 'success'

###############################################################################
# Run test_ogrsf


def ogr_mssqlspatial_test_ogrsf():

    if gdaltest.mssqlspatial_ds is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + " -ro '" + gdaltest.mssqlspatial_dsname + "' tpoly")

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Verify features can be created in an existing table that includes a geometry
# column but is not registered in the "geometry_columns" table.


def ogr_mssqlspatial_create_feature_in_unregistered_table():
    if gdaltest.mssqlspatial_ds is None:
        return 'skip'

    # Create a feature that specifies a spatial-reference system
    spatial_reference = osr.SpatialReference()
    spatial_reference.ImportFromEPSG(4326)

    feature = ogr.Feature(ogr.FeatureDefn('Unregistered'))
    feature.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (10 20)',
                                                          spatial_reference))

    # Create a table that includes a geometry column but is not registered in
    # the "geometry_columns" table
    gdaltest.mssqlspatial_ds.ExecuteSQL(
        'CREATE TABLE Unregistered'
        + '('
        +   'ObjectID int IDENTITY(1,1) NOT NULL PRIMARY KEY,'
        +   'Shape geometry NOT NULL'
        + ');')

    # Create a new MSSQLSpatial data source, one that will find the table just
    # created and make it available via GetLayerByName()
    use_geometry_columns = gdal.GetConfigOption(
        'MSSQLSPATIAL_USE_GEOMETRY_COLUMNS')
    gdal.SetConfigOption('MSSQLSPATIAL_USE_GEOMETRY_COLUMNS', 'NO')

    test_ds = ogr.Open(gdaltest.mssqlspatial_dsname, update=1)

    gdal.SetConfigOption('MSSQLSPATIAL_USE_GEOMETRY_COLUMNS',
                         use_geometry_columns)

    if test_ds is None:
        gdaltest.post_reason('cannot open data source')
        return 'fail'

    # Get a layer backed by the newly created table and verify that (as it is
    # unregistered) it has no associated spatial-reference system
    unregistered_layer = test_ds.GetLayerByName('Unregistered');
    if unregistered_layer is None:
        gdaltest.post_reason('did not get Unregistered layer')
        return 'fail'

    unregistered_spatial_reference = unregistered_layer.GetSpatialRef()
    if unregistered_spatial_reference is not None:
        gdaltest.post_reason('layer Unregistered unexpectedly has an SRS')
        return 'fail'

    # Verify creating the feature in the layer succeeds despite the lack of an
    # associated spatial-reference system
    if unregistered_layer.CreateFeature(feature) != ogr.OGRERR_NONE:
        gdaltest.post_reason('CreateFeature failed')
        return 'fail'

    # Verify the created feature received the spatial-reference system of the
    # original, as none was associated with the table
    unregistered_layer.ResetReading()
    created_feature = unregistered_layer.GetNextFeature()
    if created_feature is None:
        gdaltest.post_reason('did not get feature')
        return 'fail'

    created_feature_geometry = created_feature.GetGeometryRef()
    created_spatial_reference = created_feature_geometry.GetSpatialReference()
    if not ((created_spatial_reference == spatial_reference)
            or ((created_spatial_reference is not None)
                and created_spatial_reference.IsSame(spatial_reference))):
        gdaltest.post_reason('created-feature SRS does not match original')
        return 'fail'

    # Clean up
    test_ds.Destroy()
    feature.Destroy()

    return 'success'

###############################################################################
#


def ogr_mssqlspatial_cleanup():

    if gdaltest.mssqlspatial_ds is None:
        return 'skip'

    gdaltest.mssqlspatial_ds = None

    gdaltest.mssqlspatial_ds = ogr.Open(gdaltest.mssqlspatial_dsname, update=1)
    gdaltest.mssqlspatial_ds.ExecuteSQL('DROP TABLE Unregistered')
    gdaltest.mssqlspatial_ds.ExecuteSQL('DROP TABLE tpoly')

    gdaltest.mssqlspatial_ds = None

    return 'success'


gdaltest_list = [
    ogr_mssqlspatial_1,
    ogr_mssqlspatial_2,
    ogr_mssqlspatial_3,
    ogr_mssqlspatial_4,
    ogr_mssqlspatial_test_ogrsf,
    ogr_mssqlspatial_create_feature_in_unregistered_table,
    ogr_mssqlspatial_cleanup
]

if __name__ == '__main__':

    gdaltest.setup_run('ogr_mssqlspatial')

    gdaltest.run_tests(gdaltest_list)

    sys.exit(gdaltest.summarize())
