#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_viewshed testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal
import gdaltest
import test_cli_utilities
import pytest

pytestmark = pytest.mark.skipif(test_cli_utilities.get_gdalwarp_path() is None or test_cli_utilities.get_gdal_viewshed_path() is None, reason="gdal_viewshed not available")

###############################################################################


def test_gdal_viewshed():

    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' -t_srs EPSG:32617 -overwrite ../gdrivers/data/n43.dt0 tmp/test_gdal_viewshed_in.tif')
    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_viewshed_path() + ' -oz 100 -ox 621528 -oy 4817617 tmp/test_gdal_viewshed_in.tif tmp/test_gdal_viewshed_out.tif')
    assert err is None or err == ''
    ds = gdal.Open('tmp/test_gdal_viewshed_out.tif')
    assert ds
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    gdal.Unlink('tmp/test_gdal_viewshed_in.tif')
    gdal.Unlink('tmp/test_gdal_viewshed_out.tif')
    assert cs == 42397
    assert nodata is None


###############################################################################


def test_gdal_viewshed_alternative_modes():

    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' -t_srs EPSG:32617 -overwrite ../gdrivers/data/n43.dt0 tmp/test_gdal_viewshed_in.tif')

    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_viewshed_path() + ' -om DEM -oz 100 -ox 621528 -oy 4817617 tmp/test_gdal_viewshed_in.tif tmp/test_gdal_viewshed_out.tif')
    assert err is None or err == ''
    ds = gdal.Open('tmp/test_gdal_viewshed_out.tif')
    assert ds
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    gdal.Unlink('tmp/test_gdal_viewshed_out.tif')
    assert cs == 48478
    assert nodata is None

    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_viewshed_path() + ' -om GROUND -oz 100 -ox 621528 -oy 4817617 tmp/test_gdal_viewshed_in.tif tmp/test_gdal_viewshed_out.tif')
    assert err is None or err == ''
    ds = gdal.Open('tmp/test_gdal_viewshed_out.tif')
    assert ds
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    gdal.Unlink('tmp/test_gdal_viewshed_in.tif')
    gdal.Unlink('tmp/test_gdal_viewshed_out.tif')
    assert cs == 49633
    assert nodata is None


###############################################################################


def test_gdal_viewshed_all_options():

    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' -t_srs EPSG:32617 -overwrite ../gdrivers/data/n43.dt0 tmp/test_gdal_viewshed_in.tif')
    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_viewshed_path() + ' -om NORMAL -f GTiff -oz 10 -ox 621528 -oy 4817617 -b 1 -a_nodata 0 -tz 5 -md 20000 -cc 0.85714 -iv 127 -vv 254 -ov 0 tmp/test_gdal_viewshed_in.tif tmp/test_gdal_viewshed_out.tif')
    assert err is None or err == ''
    ds = gdal.Open('tmp/test_gdal_viewshed_out.tif')
    assert ds
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    gdal.Unlink('tmp/test_gdal_viewshed_in.tif')
    gdal.Unlink('tmp/test_gdal_viewshed_out.tif')
    assert cs == 24412
    assert nodata == 0


###############################################################################


def test_gdal_viewshed_missing_source():

    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_viewshed_path())
    assert 'Missing source filename' in err


###############################################################################


def test_gdal_viewshed_missing_destination():

    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_viewshed_path() + ' /dev/null')
    assert 'Missing destination filename' in err


###############################################################################


def test_gdal_viewshed_missing_ox():

    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_viewshed_path() + ' /dev/null /dev/null')
    assert 'Missing -ox' in err


###############################################################################


def test_gdal_viewshed_missing_oy():

    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_viewshed_path() + ' -ox 0 /dev/null /dev/null')
    assert 'Missing -oy' in err


###############################################################################


def test_gdal_viewshed_invalid_input():

    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_viewshed_path() + ' -ox 0 -oy 0 /dev/null /dev/null')
    assert 'not recognized as a supported file format' in err


###############################################################################


def test_gdal_viewshed_invalid_band():

    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_viewshed_path() + ' -ox 0 -oy 0 -b 2 ../gdrivers/data/n43.dt0 tmp/tmp.tif')
    assert 'Illegal band' in err


###############################################################################


def test_gdal_viewshed_invalid_observer_point():

    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_viewshed_path() + ' -ox 0 -oy 0 ../gdrivers/data/n43.dt0 tmp/tmp.tif')
    gdal.Unlink('tmp/tmp.tif')
    assert 'The observer location falls outside of the DEM area' in err


###############################################################################


def test_gdal_viewshed_invalid_output_driver():

    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_viewshed_path() + ' -ox -79.5 -oy 43.5 -of FOOBAR ../gdrivers/data/n43.dt0 tmp/tmp.tif')
    assert 'Cannot get driver' in err


###############################################################################


def test_gdal_viewshed_invalid_output_filename():

    _, err = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdal_viewshed_path() + ' -ox -79.5 -oy 43.5 ../gdrivers/data/n43.dt0 i/do_not/exist.tif')
    assert 'Cannot create dataset' in err
