#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogrmerge.py testing
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2017, Even Rouault <even dot rouault at spatialys dot com>
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

sys.path.append( '../pymod' )

from osgeo import gdal
from osgeo import ogr
import gdaltest
import test_py_scripts

###############################################################################
# Test -single

def test_ogrmerge_1():
    script_path = test_py_scripts.get_py_script('ogrmerge')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-single -o /vsimem/out.shp ../ogr/data/poly.shp ../ogr/data/poly.shp')

    ds = ogr.Open('/vsimem/out.shp')
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 20:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/out.shp')

    return 'success'

###############################################################################
# Test -append and glob

def test_ogrmerge_2():
    script_path = test_py_scripts.get_py_script('ogrmerge')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-single -o /vsimem/out.shp ../ogr/data/poly.shp')
    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-append -single -o /vsimem/out.shp "../ogr/data/p*ly.shp"')

    ds = ogr.Open('/vsimem/out.shp')
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 20:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/out.shp')

    return 'success'

###############################################################################
# Test -overwrite_ds

def test_ogrmerge_3():
    script_path = test_py_scripts.get_py_script('ogrmerge')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-overwrite_ds -o /vsimem/out.shp ../ogr/data/poly.shp')
    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-overwrite_ds -single -o /vsimem/out.shp ../ogr/data/poly.shp')

    ds = ogr.Open('/vsimem/out.shp')
    lyr = ds.GetLayer(0)
    if lyr.GetFeatureCount() != 10:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/out.shp')

    return 'success'

###############################################################################
# Test -f VRT

def test_ogrmerge_4():
    script_path = test_py_scripts.get_py_script('ogrmerge')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-f VRT -o /vsimem/out.vrt ../ogr/data/poly.shp')

    ds = ogr.Open('/vsimem/out.vrt')
    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'poly':
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetFeatureCount() != 10:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.vrt')

    return 'success'

###############################################################################
# Test -nln

def test_ogrmerge_5():
    script_path = test_py_scripts.get_py_script('ogrmerge')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-f VRT -o /vsimem/out.vrt ../ogr/data/poly.shp ../ogr/data/testpoly.shp -nln '
        '"foo_{DS_NAME}_{DS_BASENAME}_{DS_INDEX}_{LAYER_NAME}_{LAYER_INDEX}"')

    ds = ogr.Open('/vsimem/out.vrt')
    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'foo_../ogr/data/poly.shp_poly_0_poly_0':
        gdaltest.post_reason('fail')
        print(lyr.GetName())
        return 'fail'
    if lyr.GetFeatureCount() != 10:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayer(1)
    if lyr.GetName() != 'foo_../ogr/data/testpoly.shp_testpoly_1_testpoly_0':
        gdaltest.post_reason('fail')
        print(lyr.GetName())
        return 'fail'
    if lyr.GetFeatureCount() != 14:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.vrt')

    return 'success'

###############################################################################
# Test -src_layer_field_name -src_layer_field_content

def test_ogrmerge_6():
    script_path = test_py_scripts.get_py_script('ogrmerge')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-single -f VRT -o /vsimem/out.vrt ../ogr/data/poly.shp '
        '-src_layer_field_name source -src_layer_field_content '
        '"foo_{DS_NAME}_{DS_BASENAME}_{DS_INDEX}_{LAYER_NAME}_{LAYER_INDEX}"')

    ds = ogr.Open('/vsimem/out.vrt')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['source'] != 'foo_../ogr/data/poly.shp_poly_0_poly_0':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.vrt')

    return 'success'

###############################################################################
# Test -src_geom_type

def test_ogrmerge_7():
    script_path = test_py_scripts.get_py_script('ogrmerge')
    if script_path is None:
        return 'skip'

    # No match in -single mode
    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-single -f VRT -o /vsimem/out.vrt ../ogr/data/poly.shp '
        '-src_geom_type POINT')

    ds = ogr.Open('/vsimem/out.vrt')
    if ds.GetLayerCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.vrt')

    # Match in single mode
    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-single -f VRT -o /vsimem/out.vrt ../ogr/data/poly.shp '
        '-src_geom_type POLYGON')

    ds = ogr.Open('/vsimem/out.vrt')
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.vrt')

    # No match in default mode
    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-f VRT -o /vsimem/out.vrt ../ogr/data/poly.shp '
        '-src_geom_type POINT')

    ds = ogr.Open('/vsimem/out.vrt')
    if ds.GetLayerCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.vrt')

    # Match in default mode
    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-f VRT -o /vsimem/out.vrt ../ogr/data/poly.shp '
        '-src_geom_type POLYGON')

    ds = ogr.Open('/vsimem/out.vrt')
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.vrt')

    return 'success'

###############################################################################
# Test -s_srs -t_srs in -single mode

def test_ogrmerge_8():
    script_path = test_py_scripts.get_py_script('ogrmerge')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-single -f VRT -o /vsimem/out.vrt ../ogr/data/poly.shp '
        '-s_srs EPSG:32630 -t_srs EPSG:4326')

    ds = ogr.Open('/vsimem/out.vrt')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    f = gdal.VSIFOpenL('/vsimem/out.vrt', 'rb')
    content = ''
    if f is not None:
        content = gdal.VSIFReadL(1, 10000, f).decode('UTF-8')
        gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/out.vrt')

    if content.find('<SrcSRS>EPSG:32630</SrcSRS>') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'

    if content.find('<TargetSRS>EPSG:4326</TargetSRS>') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'

    return 'success'

###############################################################################
# Test -s_srs -t_srs in default mode

def test_ogrmerge_9():
    script_path = test_py_scripts.get_py_script('ogrmerge')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-f VRT -o /vsimem/out.vrt ../ogr/data/poly.shp '
        '-s_srs EPSG:32630 -t_srs EPSG:4326')

    ds = ogr.Open('/vsimem/out.vrt')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    f = gdal.VSIFOpenL('/vsimem/out.vrt', 'rb')
    content = ''
    if f is not None:
        content = gdal.VSIFReadL(1, 10000, f).decode('UTF-8')
        gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/out.vrt')

    if content.find('<SrcSRS>EPSG:32630</SrcSRS>') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'

    if content.find('<TargetSRS>EPSG:4326</TargetSRS>') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'

    return 'success'

###############################################################################
# Test -a_srs in -single mode

def test_ogrmerge_10():
    script_path = test_py_scripts.get_py_script('ogrmerge')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-single -f VRT -o /vsimem/out.vrt ../ogr/data/poly.shp '
        '-a_srs EPSG:32630')

    ds = ogr.Open('/vsimem/out.vrt')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    f = gdal.VSIFOpenL('/vsimem/out.vrt', 'rb')
    content = ''
    if f is not None:
        content = gdal.VSIFReadL(1, 10000, f).decode('UTF-8')
        gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/out.vrt')

    if content.find('<LayerSRS>EPSG:32630</LayerSRS>') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'

    return 'success'

###############################################################################
# Test -a_srs in default mode

def test_ogrmerge_11():
    script_path = test_py_scripts.get_py_script('ogrmerge')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'ogrmerge',
        '-f VRT -o /vsimem/out.vrt ../ogr/data/poly.shp '
        '-a_srs EPSG:32630')

    ds = ogr.Open('/vsimem/out.vrt')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    f = gdal.VSIFOpenL('/vsimem/out.vrt', 'rb')
    content = ''
    if f is not None:
        content = gdal.VSIFReadL(1, 10000, f).decode('UTF-8')
        gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/out.vrt')

    if content.find('<LayerSRS>EPSG:32630</LayerSRS>') < 0:
        gdaltest.post_reason('fail')
        print(content)
        return 'fail'

    return 'success'

gdaltest_list = [
    test_ogrmerge_1,
    test_ogrmerge_2,
    test_ogrmerge_3,
    test_ogrmerge_4,
    test_ogrmerge_5,
    test_ogrmerge_6,
    test_ogrmerge_7,
    test_ogrmerge_8,
    test_ogrmerge_9,
    test_ogrmerge_10,
    test_ogrmerge_11
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_ogrmerge' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
