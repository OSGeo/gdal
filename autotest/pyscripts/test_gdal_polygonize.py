#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test gdal_polygonize.py script
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
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


import ogrtest
import test_py_scripts

from osgeo import ogr
import pytest

###############################################################################
# Test a fairly simple case, with nodata masking.


def test_gdal_polygonize_1():

    script_path = test_py_scripts.get_py_script('gdal_polygonize')
    if script_path is None:
        pytest.skip()

    # Create a OGR datasource to put results in.
    shp_drv = ogr.GetDriverByName('ESRI Shapefile')
    try:
        os.stat('tmp/poly.shp')
        shp_drv.DeleteDataSource('tmp/poly.shp')
    except OSError:
        pass

    shp_ds = shp_drv.CreateDataSource('tmp/poly.shp')

    shp_layer = shp_ds.CreateLayer('poly', None, ogr.wkbPolygon)

    fd = ogr.FieldDefn('DN', ogr.OFTInteger)
    shp_layer.CreateField(fd)

    shp_ds.Destroy()

    # run the algorithm.
    test_py_scripts.run_py_script(script_path, 'gdal_polygonize', test_py_scripts.get_data_path('alg')+'polygonize_in.grd tmp poly DN')

    # Confirm we get the set of expected features in the output layer.

    shp_ds = ogr.Open('tmp')
    shp_lyr = shp_ds.GetLayerByName('poly')

    expected_feature_number = 13
    assert shp_lyr.GetFeatureCount() == expected_feature_number

    expect = [107, 123, 115, 115, 140, 148, 123, 140, 156,
              100, 101, 102, 103]

    tr = ogrtest.check_features_against_list(shp_lyr, 'DN', expect)

    # check at least one geometry.
    if tr:
        shp_lyr.SetAttributeFilter('dn = 156')
        feat_read = shp_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry(feat_read, 'POLYGON ((440720 3751200,440900 3751200,440900 3751020,440720 3751020,440720 3751200),(440780 3751140,440780 3751080,440840 3751080,440840 3751140,440780 3751140))') != 0:
            tr = 0
        feat_read.Destroy()

    shp_ds.Destroy()
    # Reload drv because of side effects of run_py_script()
    shp_drv = ogr.GetDriverByName('ESRI Shapefile')
    shp_drv.DeleteDataSource('tmp/poly.shp')

    assert tr

###############################################################################
# Test a simple case without masking.


def test_gdal_polygonize_2():

    script_path = test_py_scripts.get_py_script('gdal_polygonize')
    if script_path is None:
        pytest.skip()

    shp_drv = ogr.GetDriverByName('ESRI Shapefile')
    try:
        os.stat('tmp/out.shp')
        shp_drv.DeleteDataSource('tmp/out.shp')
    except OSError:
        pass

    # run the algorithm.
    test_py_scripts.run_py_script(script_path, 'gdal_polygonize', '-b 1 -f "ESRI Shapefile" -q -nomask '+test_py_scripts.get_data_path('alg')+'polygonize_in.grd tmp')

    # Confirm we get the set of expected features in the output layer.
    shp_ds = ogr.Open('tmp')
    shp_lyr = shp_ds.GetLayerByName('out')

    expected_feature_number = 17
    assert shp_lyr.GetFeatureCount() == expected_feature_number

    expect = [107, 123, 115, 132, 115, 132, 140, 132, 148, 123, 140,
              132, 156, 100, 101, 102, 103]

    tr = ogrtest.check_features_against_list(shp_lyr, 'DN', expect)

    shp_ds.Destroy()
    # Reload drv because of side effects of run_py_script()
    shp_drv = ogr.GetDriverByName('ESRI Shapefile')
    shp_drv.DeleteDataSource('tmp/out.shp')

    assert tr


def test_gdal_polygonize_3():

    script_path = test_py_scripts.get_py_script('gdal_polygonize')
    if script_path is None:
        pytest.skip()

    drv = ogr.GetDriverByName('GPKG')
    if drv is None:
        pytest.skip()
    try:
        os.stat('tmp/out.gpkg')
        drv.DeleteDataSource('tmp/out.gpkg')
    except OSError:
        pass

    # run the algorithm.
    test_py_scripts.run_py_script(script_path, 'gdal_polygonize', '-b 1 -f "GPKG" -q -nomask '+test_py_scripts.get_data_path('alg')+'polygonize_in.grd tmp/out.gpkg')

    # Confirm we get the set of expected features in the output layer.
    gpkg_ds = ogr.Open('tmp/out.gpkg')
    gpkg_lyr = gpkg_ds.GetLayerByName('out')
    geom_type = gpkg_lyr.GetGeomType()
    geom_is_polygon = geom_type in (ogr.wkbPolygon, ogr.wkbMultiPolygon)

    gpkg_ds.Destroy()
    # Reload drv because of side effects of run_py_script()
    drv = ogr.GetDriverByName('GPKG')
    drv.DeleteDataSource('tmp/out.gpkg')

    if geom_is_polygon:
        return
    pytest.fail('GetGeomType() returned %d instead of %d or %d (ogr.wkbPolygon or ogr.wkbMultiPolygon)' % (geom_type, ogr.wkbPolygon, ogr.wkbMultiPolygon))

###############################################################################
# Test -b mask


def test_gdal_polygonize_4():

    script_path = test_py_scripts.get_py_script('gdal_polygonize')
    if script_path is None:
        pytest.skip()

    # Test mask syntax
    test_py_scripts.run_py_script(script_path, 'gdal_polygonize', '-q -f GML -b mask ' + test_py_scripts.get_data_path('gcore') + 'byte.tif tmp/out.gml')

    content = open('tmp/out.gml', 'rt').read()

    os.unlink('tmp/out.gml')
    os.unlink('tmp/out.xsd')

    assert '<gml:Polygon srsName="urn:ogc:def:crs:EPSG::26711" gml:id="out.geom.0"><gml:exterior><gml:LinearRing><gml:posList>440720 3751320 440720 3750120 441920 3750120 441920 3751320 440720 3751320</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon>' in content

    # Test mask,1 syntax
    test_py_scripts.run_py_script(script_path, 'gdal_polygonize', '-q -f GML -b mask,1 ' + test_py_scripts.get_data_path('gcore') + 'byte.tif tmp/out.gml')

    content = open('tmp/out.gml', 'rt').read()

    os.unlink('tmp/out.gml')
    os.unlink('tmp/out.xsd')

    assert '<gml:Polygon srsName="urn:ogc:def:crs:EPSG::26711" gml:id="out.geom.0"><gml:exterior><gml:LinearRing><gml:posList>440720 3751320 440720 3750120 441920 3750120 441920 3751320 440720 3751320</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon>' in content



