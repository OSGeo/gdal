#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogrtindex testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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


from osgeo import ogr
from osgeo import osr
import ogrtest
import gdaltest
import test_cli_utilities
import pytest

###############################################################################
# Simple test


def test_ogrtindex_1(srs=None):
    if test_cli_utilities.get_ogrtindex_path() is None:
        pytest.skip()

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')

    for basename in ['tileindex', 'point1', 'point2', 'point3', 'point4']:
        for extension in ['shp', 'dbf', 'shx', 'prj']:
            try:
                os.remove('tmp/%s.%s' % (basename, extension))
            except OSError:
                pass

    shape_ds = shape_drv.CreateDataSource('tmp')

    shape_lyr = shape_ds.CreateLayer('point1', srs=srs)
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(49 2)'))
    shape_lyr.CreateFeature(dst_feat)
    dst_feat.Destroy()

    shape_lyr = shape_ds.CreateLayer('point2', srs=srs)
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(49 3)'))
    shape_lyr.CreateFeature(dst_feat)
    dst_feat.Destroy()

    shape_lyr = shape_ds.CreateLayer('point3', srs=srs)
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(48 2)'))
    shape_lyr.CreateFeature(dst_feat)
    dst_feat.Destroy()

    shape_lyr = shape_ds.CreateLayer('point4', srs=srs)
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(48 3)'))
    shape_lyr.CreateFeature(dst_feat)
    dst_feat.Destroy()

    shape_ds.Destroy()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_ogrtindex_path() + ' -skip_different_projection tmp/tileindex.shp tmp/point1.shp tmp/point2.shp tmp/point3.shp tmp/point4.shp')
    assert (err is None or err == ''), 'got error/warning'

    ds = ogr.Open('tmp/tileindex.shp')
    assert ds.GetLayer(0).GetFeatureCount() == 4, 'did not get expected feature count'

    if srs is not None:
        assert ds.GetLayer(0).GetSpatialRef() is not None and ds.GetLayer(0).GetSpatialRef().IsSame(srs, options = ['IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES']), \
            'did not get expected spatial ref'
    else:
        assert ds.GetLayer(0).GetSpatialRef() is None, 'did not get expected spatial ref'

    expected_wkts = ['POLYGON ((49 2,49 2,49 2,49 2,49 2))',
                     'POLYGON ((49 3,49 3,49 3,49 3,49 3))',
                     'POLYGON ((48 2,48 2,48 2,48 2,48 2))',
                     'POLYGON ((48 3,48 3,48 3,48 3,48 3))']
    i = 0
    feat = ds.GetLayer(0).GetNextFeature()
    while feat is not None:
        assert feat.GetGeometryRef().ExportToWkt() == expected_wkts[i], \
            ('i=%d, wkt=%s' % (i, feat.GetGeometryRef().ExportToWkt()))
        i = i + 1
        feat = ds.GetLayer(0).GetNextFeature()
    ds.Destroy()

###############################################################################
# Same test but with a SRS set on the different tiles to index


def test_ogrtindex_2():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    return test_ogrtindex_1(srs)

###############################################################################
# Test -src_srs_name, -src_srs_format and -t_srs


def test_ogrtindex_3():

    if test_cli_utilities.get_ogrtindex_path() is None:
        pytest.skip()

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')

    for basename in ['tileindex', 'point1', 'point2', 'point3', 'point4']:
        for extension in ['shp', 'dbf', 'shx', 'prj']:
            try:
                os.remove('tmp/%s.%s' % (basename, extension))
            except OSError:
                pass

    shape_ds = shape_drv.CreateDataSource('tmp')

    srs_4326 = osr.SpatialReference()
    srs_4326.ImportFromEPSG(4326)
    wkt_epsg_4326 = srs_4326.ExportToWkt()

    srs_32631 = osr.SpatialReference()
    srs_32631.ImportFromEPSG(32631)
    wkt_epsg_32631 = srs_32631.ExportToWkt()

    shape_lyr = shape_ds.CreateLayer('point1', srs=srs_4326)
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    shape_lyr.CreateFeature(dst_feat)

    shape_lyr = shape_ds.CreateLayer('point2', srs=srs_32631)
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(500000 5538630.70286887)'))
    shape_lyr.CreateFeature(dst_feat)
    shape_ds = None

    for (src_srs_format, expected_srss) in [
            ('', ['EPSG:4326', 'EPSG:32631']),
            ('-src_srs_format AUTO', ['EPSG:4326', 'EPSG:32631']),
            ('-src_srs_format EPSG', ['EPSG:4326', 'EPSG:32631']),
            ('-src_srs_format PROJ', ['+proj=longlat +datum=WGS84 +no_defs', '+proj=utm +zone=31 +datum=WGS84 +units=m +no_defs']),
            ('-src_srs_format WKT', [wkt_epsg_4326, wkt_epsg_32631])
    ]:

        if os.path.exists('tmp/tileindex.shp'):
            shape_drv.DeleteDataSource('tmp/tileindex.shp')
        if os.path.exists('tmp/tileindex.db'):
            os.unlink('tmp/tileindex.db')

        output_filename = 'tmp/tileindex.shp'
        output_format = ''
        if src_srs_format == '-src_srs_format WKT':
            if ogr.GetDriverByName('SQLite') is None:
                continue
            output_filename = 'tmp/tileindex.db'
            output_format = ' -f SQLite'

        (_, err) = gdaltest.runexternal_out_and_err(
            test_cli_utilities.get_ogrtindex_path() +
            ' -src_srs_name src_srs -t_srs EPSG:4326 ' + output_filename + ' tmp/point1.shp tmp/point2.shp ' + src_srs_format + output_format)

        assert src_srs_format == '-src_srs_format WKT' or (err is None or err == ''), \
            'got error/warning'

        ds = ogr.Open(output_filename)
        assert ds.GetLayer(0).GetFeatureCount() == 2, \
            'did not get expected feature count'

        assert ds.GetLayer(0).GetSpatialRef().GetAuthorityCode(None) == '4326', \
            'did not get expected spatial ref'

        expected_wkts = ['POLYGON ((2 49,2 49,2 49,2 49,2 49))',
                         'POLYGON ((3 50,3 50,3 50,3 50,3 50))']
        i = 0
        feat = ds.GetLayer(0).GetNextFeature()
        while feat is not None:
            if feat.GetField('src_srs') != expected_srss[i]:
                feat.DumpReadable()
                pytest.fail(i, src_srs_format)
            assert ogrtest.check_feature_geometry(feat, expected_wkts[i]) == 0, \
                ('i=%d, wkt=%s' % (i, feat.GetGeometryRef().ExportToWkt()))
            i = i + 1
            feat = ds.GetLayer(0).GetNextFeature()
        ds = None

    if os.path.exists('tmp/tileindex.shp'):
        shape_drv.DeleteDataSource('tmp/tileindex.shp')
    if os.path.exists('tmp/tileindex.db'):
        os.unlink('tmp/tileindex.db')

    
###############################################################################
# Cleanup


def test_ogrtindex_cleanup():
    if test_cli_utilities.get_ogrtindex_path() is None:
        pytest.skip()

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    shape_drv.DeleteDataSource('tmp/tileindex.shp')
    shape_drv.DeleteDataSource('tmp/point1.shp')
    shape_drv.DeleteDataSource('tmp/point2.shp')
    if os.path.exists('tmp/point3.shp'):
        shape_drv.DeleteDataSource('tmp/point3.shp')
    if os.path.exists('tmp/point4.shp'):
        shape_drv.DeleteDataSource('tmp/point4.shp')

    



