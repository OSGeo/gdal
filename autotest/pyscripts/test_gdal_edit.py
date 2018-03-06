#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_edit.py testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import os
import shutil

sys.path.append( '../pymod' )

from osgeo import gdal
import gdaltest
import test_py_scripts

#Usage: gdal_edit [--help-general] [-a_srs srs_def] [-a_ullr ulx uly lrx lry]
#                 [-tr xres yres] [-a_nodata value]
#                 [-unsetgt] [-stats] [-approx_stats]
#                 [-gcp pixel line easting northing [elevation]]*
#                 [-mo "META-TAG=VALUE"]*  datasetname


###############################################################################
# Test -a_srs, -a_ullr, -a_nodata, -mo

def test_gdal_edit_py_1():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        return 'skip'

    shutil.copy('../gcore/data/byte.tif', 'tmp/test_gdal_edit_py.tif')

    if sys.platform == 'win32':
        # Passing utf-8 characters doesn't at least please Wine...
        val = 'fake-utf8'
        val_encoded = val
    elif sys.version_info >= (3,0,0):
        val = '\u00e9ven'
        val_encoded = val
    else:
        exec("val = u'\\u00e9ven'")
        val_encoded = val.encode( 'utf-8' )

    test_py_scripts.run_py_script(script_path, 'gdal_edit', 'tmp/test_gdal_edit_py.tif -a_srs EPSG:4326 -a_ullr 2 50 3 49 -a_nodata 123 -mo FOO=BAR -mo UTF8=' + val_encoded + ' -mo ' + val_encoded + '=UTF8')

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    nd = ds.GetRasterBand(1).GetNoDataValue()
    md = ds.GetMetadata()
    ds = None

    if wkt.find('4326') == -1:
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'

    expected_gt = (2.0, 0.050000000000000003, 0.0, 50.0, 0.0, -0.050000000000000003)
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-10:
            gdaltest.post_reason('fail')
            print(gt)
            return 'fail'

    if nd != 123:
        gdaltest.post_reason('fail')
        print(nd)
        return 'fail'

    if md['FOO'] != 'BAR':
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'

    if md['UTF8'] != val:
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'

    if md[val] != 'UTF8':
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'

    return 'success'

###############################################################################
# Test -unsetgt

def test_gdal_edit_py_2():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        return 'skip'

    shutil.copy('../gcore/data/byte.tif', 'tmp/test_gdal_edit_py.tif')

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -unsetgt")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform(can_return_null = True)
    ds = None

    if gt is not None:
        gdaltest.post_reason('fail')
        print(gt)
        return 'fail'

    if wkt == '':
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'

    return 'success'

###############################################################################
# Test -a_srs ''

def test_gdal_edit_py_3():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        return 'skip'

    shutil.copy('../gcore/data/byte.tif', 'tmp/test_gdal_edit_py.tif')

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -a_srs ''")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    ds = None

    if gt == (0.0, 1.0, 0.0, 0.0, 0.0, 1.0):
        gdaltest.post_reason('fail')
        print(gt)
        return 'fail'

    if wkt != '':
        gdaltest.post_reason('fail')
        print(wkt)
        return 'fail'

    return 'success'

###############################################################################
# Test -unsetstats

def test_gdal_edit_py_4():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        return 'skip'

    shutil.copy('../gcore/data/byte.tif', 'tmp/test_gdal_edit_py.tif')
    ds = gdal.Open( 'tmp/test_gdal_edit_py.tif', gdal.GA_Update )
    band = ds.GetRasterBand(1)
    band.ComputeStatistics(False)
    band.SetMetadataItem('FOO', 'BAR')
    ds = band = None

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    band = ds.GetRasterBand(1)
    if (band.GetMetadataItem('STATISTICS_MINIMUM') is None
            or band.GetMetadataItem('FOO') is None):
        gdaltest.post_reason('fail')
        return 'fail'
    ds = band = None

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -unsetstats")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    band = ds.GetRasterBand(1)
    if (band.GetMetadataItem('STATISTICS_MINIMUM') is not None
            or band.GetMetadataItem('FOO') is None):
        gdaltest.post_reason('fail')
        return 'fail'
    ds = band = None

    try:
        os.stat('tmp/test_gdal_edit_py.tif.aux.xml')
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Test -stats

def test_gdal_edit_py_5():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        return 'skip'

    try:
        from osgeo import gdalnumeric
        gdalnumeric.BandRasterIONumPy
    except:
        return 'skip'

    shutil.copy('../gcore/data/byte.tif', 'tmp/test_gdal_edit_py.tif')
    ds = gdal.Open( 'tmp/test_gdal_edit_py.tif', gdal.GA_Update )
    band = ds.GetRasterBand(1)
    array = band.ReadAsArray()
    # original minimum is 74; modify a pixel value from 99 to 22
    array[15, 12] = 22
    band.WriteArray(array)
    ds = band = None

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    if ds.ReadAsArray()[15, 12] != 22:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -stats")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    stat_min = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIMUM')
    if stat_min is None or float(stat_min) != 22:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = gdal.Open( 'tmp/test_gdal_edit_py.tif', gdal.GA_Update )
    band = ds.GetRasterBand(1)
    array = band.ReadAsArray()
    array[15, 12] = 26
    band.WriteArray(array)
    ds = band = None

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    if ds.ReadAsArray()[15, 12] != 26:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -stats")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    stat_min = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIMUM')
    if stat_min is None or float(stat_min) != 26:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test -scale and -offset

def test_gdal_edit_py_6():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        return 'skip'

    shutil.copy('../gcore/data/byte.tif', 'tmp/test_gdal_edit_py.tif')

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -scale 2 -offset 3")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    if ds.GetRasterBand(1).GetScale() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOffset() != 3:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test -colorinterp_X

def test_gdal_edit_py_7():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        return 'skip'

    gdal.Translate('tmp/test_gdal_edit_py.tif',
                   '../gcore/data/byte.tif',
                   options = '-b 1 -b 1 -b 1 -b 1 -co PHOTOMETRIC=RGB -co ALPHA=NO')

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -colorinterp_4 alpha")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    if ds.GetRasterBand(4).GetColorInterpretation() != gdal.GCI_AlphaBand:
        gdaltest.post_reason('fail')
        return 'fail'

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -colorinterp_4 undefined")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    if ds.GetRasterBand(4).GetColorInterpretation() != gdal.GCI_Undefined:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Cleanup

def test_gdal_edit_py_cleanup():

    try:
        os.unlink('tmp/test_gdal_edit_py.tif')
    except:
        pass

    return 'success'

gdaltest_list = [
    test_gdal_edit_py_1,
    test_gdal_edit_py_2,
    test_gdal_edit_py_3,
    test_gdal_edit_py_4,
    test_gdal_edit_py_5,
    test_gdal_edit_py_6,
    test_gdal_edit_py_7,
    test_gdal_edit_py_cleanup,
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_edit_py' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
