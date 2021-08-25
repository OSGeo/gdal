#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test multidimensional support in netCDF driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2021, Even Rouault <even.rouault@spatialys.com>
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

from osgeo import gdal

# Must to be launched from netcdf_multidim.py::test_netcdf_multidim_cache_pamproxydb
if len(sys.argv) == 2 and sys.argv[1] == '-test_netcdf_multidim_cache_pamproxydb':

    gdal.SetConfigOption('GDAL_PAM_PROXY_DIR', 'tmp/tmppamproxydir')

    tmpfilename = 'tmp/tmpdirreadonly/test.nc'

    def get_transposed_and_cache():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        ar = ds.GetRootGroup().OpenMDArray('Band1')
        assert ar
        transpose = ar.Transpose([1, 0])
        assert transpose.Cache()
        with gdaltest.error_handler():
            # Cannot cache twice the same array
            assert transpose.Cache() is False

        ar2 = ds.GetRootGroup().OpenMDArray('Band1')
        assert ar2
        assert ar2.Cache()

        return transpose.Read()

    transposed_data = get_transposed_and_cache()

    def check_cache_exists():
        cache_ds = gdal.OpenEx('tmp/tmppamproxydir/000000_tmp_tmpdirreadonly_test.nc.gmac', gdal.OF_MULTIDIM_RASTER)
        assert cache_ds
        rg = cache_ds.GetRootGroup()
        cached_ar = rg.OpenMDArray('Transposed_view_of__Band1_along__1_0_')
        assert cached_ar
        assert cached_ar.Read() == transposed_data

        assert rg.OpenMDArray('_Band1') is not None

    check_cache_exists()

    def check_cache_working():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        ar = ds.GetRootGroup().OpenMDArray('Band1')
        transpose = ar.Transpose([1, 0])
        assert transpose.Read() == transposed_data
        # Again
        assert transpose.Read() == transposed_data

    check_cache_working()

    # Now alter the cache directly
    def alter_cache():
        cache_ds = gdal.OpenEx('tmp/tmppamproxydir/000000_tmp_tmpdirreadonly_test.nc.gmac',
                               gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert cache_ds
        rg = cache_ds.GetRootGroup()
        cached_ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        cached_ar.Write(b'\x00' * len(transposed_data))

    alter_cache()

    # And check we get the altered values
    def check_cache_really_working():
        ds = gdal.OpenEx(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        ar = ds.GetRootGroup().OpenMDArray('Band1')
        transpose = ar.Transpose([1, 0])
        assert transpose.Read() == b'\x00' * len(transposed_data)

    check_cache_really_working()

    gdal.Unlink(tmpfilename)
    gdal.Unlink('tmp/tmppamproxydir/000000_tmp_tmpdirreadonly_test.nc.gmac')

    print('success')
    sys.exit(0)
