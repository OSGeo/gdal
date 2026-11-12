#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for GTiff driver.
# Author:   Andrey Kiselev, dron@remotesensing.org
#
###############################################################################
# Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import gdaltest
import numpy as np
import pytest

from osgeo import gdal

init_list = [
    ("byte.tif", 4672),
    ("int16.tif", 4672),
    ("uint16.tif", 4672),
    ("int32.tif", 4672),
    ("uint32.tif", 4672),
    ("float32.tif", 4672),
    ("float64.tif", 4672),
    ("cint16.tif", 5028),
    ("cint32.tif", 5028),
    ("cfloat32.tif", 5028),
    ("cfloat64.tif", 5028),
]


# Some tests we don't need to do for each type.
@pytest.mark.parametrize(
    "testfunction",
    [
        "testSetGeoTransform",
        "testSetProjection",
        "testSetMetadata",
    ],
)
@pytest.mark.require_driver("GTiff")
def test_gtiff_set(testfunction, tmp_path):
    ut = gdaltest.GDALTest("GTiff", "byte.tif", 1, 4672, tmpdir=tmp_path)
    getattr(ut, testfunction)()


# Others we do for each pixel type.
@pytest.mark.parametrize(
    "filename,checksum",
    init_list,
    ids=[tup[0].split(".")[0] for tup in init_list],
)
@pytest.mark.parametrize(
    "testfunction",
    [
        "testCreateCopy",
        "testCreate",
        "testSetNoDataValue",
    ],
)
@pytest.mark.require_driver("GTiff")
def test_gtiff_create(filename, checksum, testfunction, tmp_path):
    ut = gdaltest.GDALTest("GTiff", filename, 1, checksum, tmpdir=tmp_path)
    getattr(ut, testfunction)()


@pytest.mark.require_driver("GTiff")
@pytest.mark.parametrize("num_threads", ["1", "2"])
@pytest.mark.parametrize("resample", [False])
@pytest.mark.parametrize(
    "x_off, y_off, x_size, y_size",
    [
        (300.4, 200.4, 500.4, 600.4),
        (300.4, 200, 500.4, 600),
    ],
)
def test_gtiff_multi_threaded_read(
    tmp_vsimem, num_threads, resample, x_off, y_off, x_size, y_size
):

    # 1024 x 1024 tiled GeoTIFF, 256 x 256 blocks, pixel values are the column index
    ds = gdal.GetDriverByName("MEM").Create("", 1024, 1024, 1, gdal.GDT_Float32)
    ds.GetRasterBand(1).WriteArray(
        np.tile(np.arange(1024, dtype=np.float32), (1024, 1))
    )
    gdal.Translate(tmp_vsimem / "src.tif", ds, creationOptions=["TILED=YES"])

    with gdal.config_option("GDAL_NUM_THREADS", num_threads):
        ds = gdal.Open(tmp_vsimem / "src.tif")
        if resample:
            a = ds.GetRasterBand(1).ReadAsArray(
                x_off, y_off, x_size, y_size, buf_xsize=250, buf_ysize=300
            )
        else:
            a = ds.GetRasterBand(1).ReadAsArray(
                x_off, y_off, x_size, y_size, buf_xsize=500, buf_ysize=600
            )

        assert (
            a[0, -1] == 800.0
        ), f"Did not read expected data 800 with num_threads={num_threads}"
