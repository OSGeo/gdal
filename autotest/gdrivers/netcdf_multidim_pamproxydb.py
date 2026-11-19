#!/usr/bin/env python
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test multidimensional support in netCDF driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2021, Even Rouault <even.rouault@spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import sys

sys.path.append("../pymod")

import gdaltest

from osgeo import gdal

# Must to be launched from netcdf_multidim.py::test_netcdf_multidim_cache_pamproxydb
if len(sys.argv) == 3 and sys.argv[1] == "-test_netcdf_multidim_cache_pamproxydb":

    tmp_dir = sys.argv[2]

    gdal.SetConfigOption("GDAL_PAM_PROXY_DIR", os.path.join(tmp_dir, "tmppamproxydir"))

    tmpfilename = os.path.join(tmp_dir, "tmpdirreadonly/test.nc")
    assert os.path.exists(tmpfilename)

    normalized_dir = str(tmp_dir).replace("/", "_").replace("-", "_").replace("\\", "_")

    def get_transposed_and_cache():
        ds = gdal.Open(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        ar = ds.GetRootGroup().OpenMDArray("Band1")
        assert ar
        transpose = ar.Transpose([1, 0])
        assert transpose.Cache()
        with gdaltest.disable_exceptions():
            # Cannot cache twice the same array
            assert transpose.Cache() is False

        ar2 = ds.GetRootGroup().OpenMDArray("Band1")
        assert ar2
        assert ar2.Cache()

        return transpose.Read()

    with gdaltest.disable_exceptions():
        transposed_data = get_transposed_and_cache()

    def check_cache_exists():

        cache_ds = gdal.Open(
            os.path.join(
                tmp_dir,
                "tmppamproxydir/000000_{}_tmpdirreadonly_test.nc.gmac".format(
                    normalized_dir
                ),
            ),
            gdal.OF_MULTIDIM_RASTER,
        )
        assert cache_ds
        rg = cache_ds.GetRootGroup()
        cached_ar = rg.OpenMDArray("Transposed_view_of__Band1_along__1_0_")
        assert cached_ar
        assert cached_ar.Read() == transposed_data

        assert rg.OpenMDArray("_Band1") is not None

    check_cache_exists()

    def check_cache_working():
        ds = gdal.Open(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        ar = ds.GetRootGroup().OpenMDArray("Band1")
        transpose = ar.Transpose([1, 0])
        assert transpose.Read() == transposed_data
        # Again
        assert transpose.Read() == transposed_data

    check_cache_working()

    # Now alter the cache directly
    def alter_cache():
        cache_ds = gdal.Open(
            os.path.join(
                tmp_dir,
                "tmppamproxydir/000000_{}_tmpdirreadonly_test.nc.gmac".format(
                    normalized_dir
                ),
            ),
            gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE,
        )
        assert cache_ds, "Failed to open cache dataset at {}".format(
            os.path.join(
                tmp_dir,
                "tmppamproxydir/000000_{}_tmpdirreadonly_test.nc.gmac".format(
                    normalized_dir
                ),
            )
        )
        rg = cache_ds.GetRootGroup()
        cached_ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        cached_ar.Write(b"\x00" * len(transposed_data))

    alter_cache()

    # And check we get the altered values
    def check_cache_really_working():
        ds = gdal.Open(tmpfilename, gdal.OF_MULTIDIM_RASTER)
        ar = ds.GetRootGroup().OpenMDArray("Band1")
        transpose = ar.Transpose([1, 0])
        assert transpose.Read() == b"\x00" * len(transposed_data)

    check_cache_really_working()

    print("success")
    sys.exit(0)
