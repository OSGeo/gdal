#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_edit.py testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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


from osgeo import gdal
import test_py_scripts
import pytest

# Usage: gdal_edit [--help-general] [-a_srs srs_def] [-a_ullr ulx uly lrx lry]
#                 [-tr xres yres] [-a_nodata value]
#                 [-unsetgt] [-stats] [-approx_stats]
#                 [-gcp pixel line easting northing [elevation]]*
#                 [-mo "META-TAG=VALUE"]*  datasetname


###############################################################################
# Test -a_srs, -a_ullr, -a_nodata, -mo, -unit

def test_gdal_edit_py_1():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        pytest.skip()

    shutil.copy(test_py_scripts.get_data_path('gcore') + 'byte.tif', 'tmp/test_gdal_edit_py.tif')

    if sys.platform == 'win32':
        # Passing utf-8 characters doesn't at least please Wine...
        val = 'fake-utf8'
        val_encoded = val
    else:
        val = '\u00e9ven'
        val_encoded = val

    test_py_scripts.run_py_script(script_path, 'gdal_edit', 'tmp/test_gdal_edit_py.tif -a_srs EPSG:4326 -a_ullr 2 50 3 49 -a_nodata 123 -mo FOO=BAR -units metre -mo UTF8=' + val_encoded + ' -mo ' + val_encoded + '=UTF8')

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    nd = ds.GetRasterBand(1).GetNoDataValue()
    md = ds.GetMetadata()
    units = ds.GetRasterBand(1).GetUnitType()
    ds = None

    assert wkt.find('4326') != -1

    expected_gt = (2.0, 0.050000000000000003, 0.0, 50.0, 0.0, -0.050000000000000003)
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1e-10)

    assert nd == 123

    assert md['FOO'] == 'BAR'

    assert md['UTF8'] == val

    assert md[val] == 'UTF8'

    assert units == 'metre'

###############################################################################
# Test -a_ulurll

def test_gdal_edit_py_1b():

    script = 'gdal_edit'
    folder = test_py_scripts.get_py_script(script)
    if folder is None:
        pytest.skip()

    image = 'tmp/test_gdal_edit_py.tif'
    shutil.copy(test_py_scripts.get_data_path('gcore') + 'byte.tif', image)

    for points, expected in (
        ('2 50 3 50 2 49', (2, 0.05, 0, 50, 0, -0.05)),  # not rotated
        ('25 70 55 80 35 40', (25, 1.5, 0.5, 70, 0.5, -1.5)),  # rotated CCW
        ('25 70 55 65 20 40', (25, 1.5, -0.25, 70, -0.25, -1.5)),  # rotated CW
    ):
        arguments = image + ' -a_ulurll ' + points
        assert test_py_scripts.run_py_script(folder, script, arguments) == ''
        assert gdal.Open(image).GetGeoTransform() == pytest.approx(expected)

###############################################################################
# Test -unsetgt


def test_gdal_edit_py_2():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        pytest.skip()

    shutil.copy(test_py_scripts.get_data_path('gcore') + 'byte.tif', 'tmp/test_gdal_edit_py.tif')

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -unsetgt")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform(can_return_null=True)
    ds = None

    assert gt is None

    assert wkt != ''

###############################################################################
# Test -a_srs ''


def test_gdal_edit_py_3():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        pytest.skip()

    shutil.copy(test_py_scripts.get_data_path('gcore') + 'byte.tif', 'tmp/test_gdal_edit_py.tif')

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -a_srs ''")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    ds = None

    assert gt != (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)

    assert wkt == ''

###############################################################################
# Test -unsetstats


def test_gdal_edit_py_4():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        pytest.skip()

    shutil.copy(test_py_scripts.get_data_path('gcore') + 'byte.tif', 'tmp/test_gdal_edit_py.tif')
    ds = gdal.Open('tmp/test_gdal_edit_py.tif', gdal.GA_Update)
    band = ds.GetRasterBand(1)
    band.ComputeStatistics(False)
    band.SetMetadataItem('FOO', 'BAR')
    ds = band = None

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    band = ds.GetRasterBand(1)
    assert (not (band.GetMetadataItem('STATISTICS_MINIMUM') is None or
            band.GetMetadataItem('FOO') is None))
    ds = band = None

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -unsetstats")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    band = ds.GetRasterBand(1)
    assert (not (band.GetMetadataItem('STATISTICS_MINIMUM') is not None or
            band.GetMetadataItem('FOO') is None))
    ds = band = None

    with pytest.raises(OSError):
        os.stat('tmp/test_gdal_edit_py.tif.aux.xml')



###############################################################################
# Test -stats


def test_gdal_edit_py_5():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        pytest.skip()

    try:
        from osgeo import gdal_array
        gdal_array.BandRasterIONumPy
    except:
        pytest.skip()

    shutil.copy(test_py_scripts.get_data_path('gcore') + 'byte.tif', 'tmp/test_gdal_edit_py.tif')
    ds = gdal.Open('tmp/test_gdal_edit_py.tif', gdal.GA_Update)
    band = ds.GetRasterBand(1)
    array = band.ReadAsArray()
    # original minimum is 74; modify a pixel value from 99 to 22
    array[15, 12] = 22
    band.WriteArray(array)
    ds = band = None

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    assert ds.ReadAsArray()[15, 12] == 22
    ds = None

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -stats")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    stat_min = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIMUM')
    assert stat_min is not None and float(stat_min) == 22
    ds = None

    ds = gdal.Open('tmp/test_gdal_edit_py.tif', gdal.GA_Update)
    band = ds.GetRasterBand(1)
    array = band.ReadAsArray()
    array[15, 12] = 26
    band.WriteArray(array)
    ds = band = None

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    assert ds.ReadAsArray()[15, 12] == 26
    ds = None

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -stats")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    stat_min = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIMUM')
    assert stat_min is not None and float(stat_min) == 26
    ds = None

###############################################################################
# Test -setstats


def test_gdal_edit_py_6():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        pytest.skip()

    shutil.copy(test_py_scripts.get_data_path('gcore') + 'byte.tif', 'tmp/test_gdal_edit_py.tif')

    # original values should be min=74, max=255, mean=126.765 StdDev=22.928470838676
    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -setstats None None None None")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    stat_min = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIMUM')
    assert stat_min is not None and float(stat_min) == 74
    stat_max = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MAXIMUM')
    assert stat_max is not None and float(stat_max) == 255
    stat_mean = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MEAN')
    assert not (stat_mean is None or float(stat_mean) != pytest.approx(126.765, abs=0.001))
    stat_stddev = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_STDDEV')
    assert not (stat_stddev is None or float(stat_stddev) != pytest.approx(22.928, abs=0.001))

    ds = None

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -setstats 22 217 100 30")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    stat_min = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIMUM')
    assert stat_min is not None and float(stat_min) == 22
    stat_max = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MAXIMUM')
    assert stat_max is not None and float(stat_max) == 217
    stat_mean = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MEAN')
    assert stat_mean is not None and float(stat_mean) == 100
    stat_stddev = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_STDDEV')
    assert stat_stddev is not None and float(stat_stddev) == 30
    ds = None

###############################################################################
# Test -scale and -offset

def test_gdal_edit_py_7():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        pytest.skip()

    shutil.copy(test_py_scripts.get_data_path('gcore') + 'byte.tif', 'tmp/test_gdal_edit_py.tif')

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -scale 2 -offset 3")
    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    assert ds.GetRasterBand(1).GetScale() == 2
    assert ds.GetRasterBand(1).GetOffset() == 3
    ds = None

    shutil.copy(test_py_scripts.get_data_path('gcore') + '1bit_2bands.tif', 'tmp/test_gdal_edit_py.tif')
    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -scale 2 4 -offset 10 20")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    for i in [1, 2]:
        assert ds.GetRasterBand(i).GetScale() == i*2
        assert ds.GetRasterBand(i).GetOffset() == i*10

    ds = None

###############################################################################
# Test -colorinterp_X


def test_gdal_edit_py_8():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        pytest.skip()

    gdal.Translate('tmp/test_gdal_edit_py.tif',
                   test_py_scripts.get_data_path('gcore') + 'byte.tif',
                   options='-b 1 -b 1 -b 1 -b 1 -co PHOTOMETRIC=RGB -co ALPHA=NO')

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -colorinterp_4 alpha")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand

    test_py_scripts.run_py_script(script_path, 'gdal_edit', "tmp/test_gdal_edit_py.tif -colorinterp_4 undefined")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_Undefined

###############################################################################


def test_gdal_edit_py_unsetrpc():

    script_path = test_py_scripts.get_py_script('gdal_edit')
    if script_path is None:
        pytest.skip()

    gdal.Translate('tmp/test_gdal_edit_py.tif', test_py_scripts.get_data_path('gcore') + 'byte_rpc.tif')

    test_py_scripts.run_py_script(script_path, 'gdal_edit',
                                  "tmp/test_gdal_edit_py.tif -unsetrpc")

    ds = gdal.Open('tmp/test_gdal_edit_py.tif')
    assert not ds.GetMetadata('RPC')

###############################################################################
# Cleanup


def test_gdal_edit_py_cleanup():

    gdal.Unlink('tmp/test_gdal_edit_py.tif')




