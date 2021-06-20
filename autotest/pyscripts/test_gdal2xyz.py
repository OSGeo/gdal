#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal2xyz.py testing
# Author:   Idan Miara <idan@miara.com>
#
###############################################################################
# Copyright (c) 2021, Idan Miara <idan@miara.com>
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

import pytest

# test that osgeo_utils is available, if not skip all tests
pytest.importorskip('osgeo_utils')
pytest.importorskip('numpy')

from itertools import product
import numpy as np

from osgeo import gdal
from osgeo.gdal_array import flip_code

from osgeo_utils.auxiliary.raster_creation import create_flat_raster
from osgeo_utils.samples import gdallocationinfo
from osgeo_utils import gdal2xyz


def test_gdal2xyz_py_1():
    """ test get_ovr_idx, create_flat_raster """
    pytest.importorskip('numpy')

    size = (3, 3)
    origin = (500_000, 0)
    pixel_size = (10, -10)
    nodata_value = 255
    band_count = 2
    dt = gdal.GDT_Byte
    np_dt = flip_code(dt)
    ds = create_flat_raster(
        filename='', size=size, origin=origin, pixel_size=pixel_size,
        nodata_value=nodata_value, fill_value=nodata_value, band_count=band_count, dt=dt)
    src_nodata = nodata_value
    np.random.seed()
    for bnd_idx in range(band_count):
        bnd = ds.GetRasterBand(bnd_idx + 1)
        data = (np.random.random_sample(size) * 255).astype(np_dt)
        data[1, 1] = src_nodata
        bnd.WriteArray(data, 0, 0)
    dst_nodata = 254
    for pre_allocate_np_arrays, skip_nodata in product((True, False), (True, False)):
        geo_x, geo_y, data, nodata = \
            gdal2xyz.gdal2xyz(
                ds, None, return_np_arrays=True,
                src_nodata=src_nodata, dst_nodata=dst_nodata,
                skip_nodata=skip_nodata, pre_allocate_np_arrays=pre_allocate_np_arrays)
        _pixels, _lines, data2 = gdallocationinfo.gdallocationinfo(
            ds, x=geo_x, y=geo_y,
            resample_alg=gdal.GRIORA_NearestNeighbour,
            srs=gdallocationinfo.LocationInfoSRS.SameAsDS_SRS)
        data2[data2 == src_nodata] = dst_nodata
        assert np.all(np.equal(data, data2))


