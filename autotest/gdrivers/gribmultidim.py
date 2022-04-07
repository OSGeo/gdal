#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test multidimensional support in GRIB driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even.rouault@spatialys.com>
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
import pytest
import struct

pytestmark = pytest.mark.require_driver('GRIB')

###############################################################################


def test_grib_multidim_grib2_3d_same_ref_time_different_forecast_time():

    ds = gdal.OpenEx('data/grib/ds.mint.bin', gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    assert not rg.GetGroupNames()
    assert not rg.OpenGroup('non_existing')
    assert rg.GetMDArrayNames() == ['Y', 'X', 'TIME', 'MinT_0-SFC']
    assert not rg.OpenMDArray('non_existing')
    dims = rg.GetDimensions()
    assert len(dims) == 3
    ar = rg.OpenMDArray('MinT_0-SFC')
    assert ar
    attrs = ar.GetAttributes()
    assert len(attrs) == 13
    assert ar.GetAttribute('name').Read() == 'MinT'
    assert ar.GetAttribute('long_name').Read() == 'Minimum temperature [C]'
    assert ar.GetAttribute('first_level').Read() == '0-SFC'
    assert ar.GetAttribute('discipline_code').Read() == 0
    assert ar.GetAttribute('discipline_name').Read() == 'Meteorological'
    assert ar.GetAttribute('center_code').Read() == 8
    assert ar.GetAttribute('center_name').Read() == 'US-NWSTG'
    assert ar.GetAttribute('signification_of_ref_time').Read() == 'Start of Forecast'
    assert ar.GetAttribute('reference_time_iso8601').Read() == '2008-02-21T17:00:00Z'
    assert ar.GetAttribute('production_status').Read() == 'Operational'
    assert ar.GetAttribute('type').Read() == 'Forecast'
    assert ar.GetAttribute('product_definition_template_number').Read() == 8
    assert ar.GetAttribute('product_definition_numbers').Read() == (
        0, 5, 2, 0, 0, 255, 255, 1, 19, 1, 0, 0, 255, 4294967295, 2147483649, 2008, 2, 22, 12, 0, 0, 1, 0, 3, 255, 1, 12, 1, 0)
    dims = ar.GetDimensions()
    assert len(dims) == 3
    assert dims[0].GetFullName() == '/TIME'
    assert dims[0].GetSize() == 2
    assert struct.unpack('d' * 2, dims[0].GetIndexingVariable().Read()) == pytest.approx((1203681600.0, 1203768000.0))
    assert dims[1].GetFullName() == '/Y'
    assert dims[1].GetSize() == 129
    assert struct.unpack('d' * 129, dims[1].GetIndexingVariable().Read())[0:2] == pytest.approx((1784311.461394906, 1786811.461394906))
    assert dims[2].GetFullName() == '/X'
    assert dims[2].GetSize() == 177
    assert struct.unpack('d' * 177, dims[2].GetIndexingVariable().Read())[0:2] == pytest.approx((-7125887.299303299, -7123387.299303299))
    assert ar.GetSpatialRef()
    assert ar.GetUnit() == 'C'
    assert ar.GetNoDataValueAsDouble() == 9999

    data = ar.Read()
    assert len(data) == 2 * 129 * 177 * 8
    data = struct.unpack('d' * 2 * 129 * 177, data)
    assert data[0] == 9999
    assert data[20 * 177 + 20] == 24.950006103515648

    data = ar.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Float32))
    assert len(data) == 2 * 129 * 177 * 4
    data = struct.unpack('f' * 2 * 129 * 177, data)
    assert data[0] == 9999
    assert data[20 * 177 + 20] == struct.unpack('f', struct.pack('f', 24.950006103515648))[0]

###############################################################################


def test_grib_multidim_grib1_2d():

    ds = gdal.OpenEx('data/grib/Sample_QuikSCAT.grb', gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    assert not rg.GetGroupNames()
    assert not rg.OpenGroup('non_existing')
    assert rg.GetMDArrayNames() == ['Y', 'X', 'CRAIN_0-SFC', 'USCT_0-SFC', 'VSCT_0-SFC', 'TSEC_0-SFC']
    dims = rg.GetDimensions()
    assert len(dims) == 2
    ar = rg.OpenMDArray('CRAIN_0-SFC')
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetFullName() == '/Y'
    assert dims[0].GetSize() == 74
    assert dims[1].GetFullName() == '/X'
    assert dims[1].GetSize() == 66

    data = ar.Read()
    assert len(data) == 74 * 66 * 8
    data = struct.unpack('d' * 74 * 66, data)
    assert data[0] == 0
    assert data[20] == 9999

    data = ar.Read(buffer_datatype = gdal.ExtendedDataType.Create(gdal.GDT_Float32))
    assert len(data) == 74 * 66 * 4
    data = struct.unpack('f' * 74 * 66, data)
    assert data[0] == 0
    assert data[20] == 9999


###############################################################################
# This file has different raster sizes for some of the products.


def test_grib_multidim_different_sizes_messages():

    ds = gdal.OpenEx('data/grib/bug3246.grb', gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    assert rg.GetMDArrayNames() == [
        'Y', 'X', 'UOGRD_1-SFC', 'VOGRD_1-SFC',
        'Y2', 'X2', 'PRMSL_0-MSL', 'UGRD_10-HTGL', 'VGRD_10-HTGL',
        'Y3', 'X3', 'HTSGW_1-SFC', 'WVPER_1-SFC', 'WVDIR_1-SFC', 'PERPW_1-SFC', 'DIRPW_1-SFC', 'PERSW_1-SFC', 'DIRSW_1-SFC']
    dims = rg.GetDimensions()
    assert len(dims) == 6

###############################################################################
# Test reading file with .idx sidecar file (that we don't use in the multidim API)


def test_grib_multidim_grib2_sidecar():

    ds = gdal.OpenEx('data/grib/gfs.t06z.pgrb2.10p0.f010.grib2', gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
