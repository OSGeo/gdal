#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id: test_gdal_calc.py 25549 2013-01-26 11:17:10Z rouault $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_calc.py testing
# Author:   Etienne Tourigny <etourigny dot dev @ gmail dot com>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault @ spatialys.com>
# Copyright (c) 2014, Etienne Tourigny <etourigny dot dev @ gmail dot com>
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

# test that gdalnumeric is available, if not skip all tests
gdalnumeric_not_available = False
try:
    from osgeo import gdalnumeric
    gdalnumeric.BandRasterIONumPy
except (ImportError, AttributeError):
    gdalnumeric_not_available = True

# Usage: gdal_calc.py [-A <filename>] [--A_band] [-B...-Z filename] [other_options]


###############################################################################
# test basic copy

def test_gdal_calc_py_1():

    if gdalnumeric_not_available:
        pytest.skip('gdalnumeric is not available, skipping all tests')

    script_path = test_py_scripts.get_py_script('gdal_calc')
    if script_path is None:
        pytest.skip()

    shutil.copy('../gcore/data/stefan_full_rgba.tif', 'tmp/test_gdal_calc_py.tif')

    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif --calc=A --overwrite --outfile tmp/test_gdal_calc_py_1_1.tif')
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif --A_band=2 --calc=A --overwrite --outfile tmp/test_gdal_calc_py_1_2.tif')
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-Z tmp/test_gdal_calc_py.tif --Z_band=2 --calc=Z --overwrite --outfile tmp/test_gdal_calc_py_1_3.tif')

    ds1 = gdal.Open('tmp/test_gdal_calc_py_1_1.tif')
    ds2 = gdal.Open('tmp/test_gdal_calc_py_1_2.tif')
    ds3 = gdal.Open('tmp/test_gdal_calc_py_1_3.tif')

    assert ds1 is not None, 'ds1 not found'
    assert ds2 is not None, 'ds2 not found'
    assert ds3 is not None, 'ds3 not found'

    assert ds1.GetRasterBand(1).Checksum() == 12603, 'ds1 wrong checksum'
    assert ds2.GetRasterBand(1).Checksum() == 58561, 'ds2 wrong checksum'
    assert ds3.GetRasterBand(1).Checksum() == 58561, 'ds3 wrong checksum'

    ds1 = None
    ds2 = None
    ds3 = None

###############################################################################
# test simple formulas


def test_gdal_calc_py_2():

    if gdalnumeric_not_available:
        pytest.skip()

    script_path = test_py_scripts.get_py_script('gdal_calc')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif --A_band 1 -B tmp/test_gdal_calc_py.tif --B_band 2 --calc=A+B --overwrite --outfile tmp/test_gdal_calc_py_2_1.tif')
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif --A_band 1 -B tmp/test_gdal_calc_py.tif --B_band 2 --calc=A*B --overwrite --outfile tmp/test_gdal_calc_py_2_2.tif')
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif --A_band 1 --calc="sqrt(A)" --type=Float32 --overwrite --outfile tmp/test_gdal_calc_py_2_3.tif')

    ds1 = gdal.Open('tmp/test_gdal_calc_py_2_1.tif')
    ds2 = gdal.Open('tmp/test_gdal_calc_py_2_2.tif')
    ds3 = gdal.Open('tmp/test_gdal_calc_py_2_3.tif')

    assert ds1 is not None, 'ds1 not found'
    assert ds2 is not None, 'ds2 not found'
    assert ds3 is not None, 'ds3 not found'
    assert ds1.GetRasterBand(1).Checksum() == 12368, 'ds1 wrong checksum'
    assert ds2.GetRasterBand(1).Checksum() == 62785, 'ds2 wrong checksum'
    assert ds3.GetRasterBand(1).Checksum() == 47132, 'ds3 wrong checksum'
    ds1 = None
    ds2 = None
    ds3 = None


###############################################################################
# test --allBands option (simple copy)

def test_gdal_calc_py_3():

    if gdalnumeric_not_available:
        pytest.skip()

    script_path = test_py_scripts.get_py_script('gdal_calc')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif --allBands A --calc=A --overwrite --outfile tmp/test_gdal_calc_py_3.tif')

    ds = gdal.Open('tmp/test_gdal_calc_py_3.tif')

    assert ds is not None, 'ds not found'
    assert ds.GetRasterBand(1).Checksum() == 12603, 'band 1 wrong checksum'
    assert ds.GetRasterBand(2).Checksum() == 58561, 'band 2 wrong checksum'
    assert ds.GetRasterBand(3).Checksum() == 36064, 'band 3 wrong checksum'
    assert ds.GetRasterBand(4).Checksum() == 10807, 'band 4 wrong checksum'

    ds = None

###############################################################################
# test --allBands option (simple calc)


def test_gdal_calc_py_4():

    if gdalnumeric_not_available:
        pytest.skip()

    script_path = test_py_scripts.get_py_script('gdal_calc')
    if script_path is None:
        pytest.skip()

    # some values are clipped to 255, but this doesn't matter... small values were visually checked
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif --calc=1 --overwrite --outfile tmp/test_gdal_calc_py_4_1.tif')
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif -B tmp/test_gdal_calc_py_4_1.tif --B_band 1 --allBands A --calc=A+B --NoDataValue=999 --overwrite --outfile tmp/test_gdal_calc_py_4_2.tif')

    ds1 = gdal.Open('tmp/test_gdal_calc_py_4_2.tif')

    assert ds1 is not None, 'ds1 not found'
    assert ds1.GetRasterBand(1).Checksum() == 29935, 'ds1 band 1 wrong checksum'
    assert ds1.GetRasterBand(2).Checksum() == 13128, 'ds1 band 2 wrong checksum'
    assert ds1.GetRasterBand(3).Checksum() == 59092, 'ds1 band 3 wrong checksum'

    ds1 = None

    # these values were not tested
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '-A tmp/test_gdal_calc_py.tif -B tmp/test_gdal_calc_py.tif --B_band 1 --allBands A --calc=A*B --NoDataValue=999 --overwrite --outfile tmp/test_gdal_calc_py_4_3.tif')

    ds2 = gdal.Open('tmp/test_gdal_calc_py_4_3.tif')

    assert ds2 is not None, 'ds2 not found'
    assert ds2.GetRasterBand(1).Checksum() == 10025, 'ds2 band 1 wrong checksum'
    assert ds2.GetRasterBand(2).Checksum() == 62785, 'ds2 band 2 wrong checksum'
    assert ds2.GetRasterBand(3).Checksum() == 10621, 'ds2 band 3 wrong checksum'

    ds2 = None

###############################################################################
# test python interface, basic copy


def test_gdal_calc_py_5():

    if gdalnumeric_not_available:
        pytest.skip('gdalnumeric is not available, skipping all tests')

    script_path = test_py_scripts.get_py_script('gdal_calc')
    if script_path is None:
        pytest.skip()

    backup_sys_path = sys.path
    sys.path.insert(0, script_path)
    import gdal_calc

    shutil.copy('../gcore/data/stefan_full_rgba.tif', 'tmp/test_gdal_calc_py.tif')

    gdal_calc.Calc('A', A='tmp/test_gdal_calc_py.tif', overwrite=True, quiet=True, outfile='tmp/test_gdal_calc_py_5_1.tif')
    gdal_calc.Calc('A', A='tmp/test_gdal_calc_py.tif', A_band=2, overwrite=True, quiet=True, outfile='tmp/test_gdal_calc_py_5_2.tif')
    gdal_calc.Calc('Z', Z='tmp/test_gdal_calc_py.tif', Z_band=2, overwrite=True, quiet=True, outfile='tmp/test_gdal_calc_py_5_3.tif')

    sys.path = backup_sys_path

    ds1 = gdal.Open('tmp/test_gdal_calc_py_5_1.tif')
    ds2 = gdal.Open('tmp/test_gdal_calc_py_5_2.tif')
    ds3 = gdal.Open('tmp/test_gdal_calc_py_5_3.tif')

    assert ds1 is not None, 'ds1 not found'
    assert ds2 is not None, 'ds2 not found'
    assert ds3 is not None, 'ds3 not found'

    assert ds1.GetRasterBand(1).Checksum() == 12603, 'ds1 wrong checksum'
    assert ds2.GetRasterBand(1).Checksum() == 58561, 'ds2 wrong checksum'
    assert ds3.GetRasterBand(1).Checksum() == 58561, 'ds3 wrong checksum'

    ds1 = None
    ds2 = None
    ds3 = None

###############################################################################
# test nodata


def test_gdal_calc_py_6():

    if gdalnumeric_not_available:
        pytest.skip('gdalnumeric is not available, skipping all tests')

    script_path = test_py_scripts.get_py_script('gdal_calc')
    if script_path is None:
        pytest.skip()

    backup_sys_path = sys.path
    sys.path.insert(0, script_path)
    import gdal_calc

    gdal.Translate('tmp/test_gdal_calc_py.tif', '../gcore/data/byte.tif', options='-a_nodata 74')

    gdal_calc.Calc('A', A='tmp/test_gdal_calc_py.tif', overwrite=True, quiet=True, outfile='tmp/test_gdal_calc_py_6.tif', NoDataValue=1)

    sys.path = backup_sys_path

    ds = gdal.Open('tmp/test_gdal_calc_py_6.tif')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4673
    result = ds.GetRasterBand(1).ComputeRasterMinMax()
    assert result == (90, 255)

###############################################################################
# test --optfile

def test_gdal_calc_py_7():
    if gdalnumeric_not_available:
        pytest.skip('gdalnumeric is not available, skipping all tests')

    script_path = test_py_scripts.get_py_script('gdal_calc')
    if script_path is None:
        pytest.skip()

    shutil.copy('../gcore/data/stefan_full_rgba.tif', 'tmp/test_gdal_calc_py.tif')

    with open('tmp/opt1', 'w') as f:
        f.write('-A tmp/test_gdal_calc_py.tif --calc=A --overwrite --outfile tmp/test_gdal_calc_py_7_1.tif')

    # Lines in optfiles beginning with '#' should be ignored
    with open('tmp/opt2', 'w') as f:
        f.write('-A tmp/test_gdal_calc_py.tif --A_band=2 --calc=A --overwrite --outfile tmp/test_gdal_calc_py_7_2.tif')
        f.write('\n# -A_band=1')

    # options on separate lines should work, too
    opts = '-Z tmp/test_gdal_calc_py.tif', '--Z_band=2', '--calc=Z', '--overwrite', '--outfile tmp/test_gdal_calc_py_7_3.tif'
    with open('tmp/opt3', 'w') as f:
        for i in opts:
            f.write(i + '\n')

    # double-quoted options should be read as single arguments. Mixed numbers of arguments per line should work.
    opts = '-Z tmp/test_gdal_calc_py.tif --Z_band=2', '--calc "Z + 0"', '--overwrite --outfile tmp/test_gdal_calc_py_7_4.tif'
    with open('tmp/opt4', 'w') as f:
        for i in opts:
            f.write(i + '\n')

    test_py_scripts.run_py_script(script_path, 'gdal_calc', '--optfile tmp/opt1')
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '--optfile tmp/opt2')
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '--optfile tmp/opt3')
    test_py_scripts.run_py_script(script_path, 'gdal_calc', '--optfile tmp/opt4')

    ds1 = gdal.Open('tmp/test_gdal_calc_py_7_1.tif')
    ds2 = gdal.Open('tmp/test_gdal_calc_py_7_2.tif')
    ds3 = gdal.Open('tmp/test_gdal_calc_py_7_3.tif')
    ds4 = gdal.Open('tmp/test_gdal_calc_py_7_4.tif')

    assert ds1 is not None, 'ds1 not found'
    assert ds2 is not None, 'ds2 not found'
    assert ds3 is not None, 'ds3 not found'
    assert ds4 is not None, 'ds4 not found'

    assert ds1.GetRasterBand(1).Checksum() == 12603, 'ds1 wrong checksum'
    assert ds2.GetRasterBand(1).Checksum() == 58561, 'ds2 wrong checksum'
    assert ds3.GetRasterBand(1).Checksum() == 58561, 'ds3 wrong checksum'
    assert ds4.GetRasterBand(1).Checksum() == 58561, 'ds4 wrong checksum'

    ds1 = None
    ds2 = None
    ds3 = None
    ds4 = None

def test_gdal_calc_py_cleanup():

    lst = ['tmp/test_gdal_calc_py.tif',
           'tmp/test_gdal_calc_py_1_1.tif',
           'tmp/test_gdal_calc_py_1_2.tif',
           'tmp/test_gdal_calc_py_1_3.tif',
           'tmp/test_gdal_calc_py_2_1.tif',
           'tmp/test_gdal_calc_py_2_2.tif',
           'tmp/test_gdal_calc_py_2_3.tif',
           'tmp/test_gdal_calc_py_3.tif',
           'tmp/test_gdal_calc_py_4_1.tif',
           'tmp/test_gdal_calc_py_4_2.tif',
           'tmp/test_gdal_calc_py_4_3.tif',
           'tmp/test_gdal_calc_py_5_1.tif',
           'tmp/test_gdal_calc_py_5_2.tif',
           'tmp/test_gdal_calc_py_5_3.tif',
           'tmp/test_gdal_calc_py_6.tif',
           'tmp/test_gdal_calc_py_7_1.tif',
           'tmp/test_gdal_calc_py_7_2.tif',
           'tmp/test_gdal_calc_py_7_3.tif',
           'tmp/test_gdal_calc_py_7_4.tif',
           'tmp/opt1',
           'tmp/opt2',
           'tmp/opt3',
          ]
    for filename in lst:
        try:
            os.remove(filename)
        except OSError:
            pass

    



