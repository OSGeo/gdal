#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test non-driver specific multidimensional support
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

from osgeo import gdal


def test_multidim_asarray_epsg_4326():

    ds = gdal.Open('../gdrivers/data/small_world.tif')
    srs_ds = ds.GetSpatialRef()
    assert srs_ds.GetDataAxisToSRSAxisMapping() == [2, 1]
    band = ds.GetRasterBand(1)

    ar = band.AsMDArray()
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetSize() == ds.RasterYSize
    assert dims[1].GetSize() == ds.RasterXSize
    srs_ar = ar.GetSpatialRef()
    assert srs_ar.GetDataAxisToSRSAxisMapping() == [1, 2]

    assert ar.Read() == ds.GetRasterBand(1).ReadRaster()

    ixdim = 1
    iydim = 0
    ds2 = ar.AsClassicDataset(ixdim, iydim)
    assert ds2.RasterYSize == ds.RasterYSize
    assert ds2.RasterXSize == ds.RasterXSize
    srs_ds2 = ds2.GetSpatialRef()
    assert srs_ds2.GetDataAxisToSRSAxisMapping() == [2, 1]
    assert srs_ds2.IsSame(srs_ds)

    assert ds2.ReadRaster() == ds.GetRasterBand(1).ReadRaster()


def test_multidim_asarray_epsg_26711():

    ds = gdal.Open('data/byte.tif')
    srs_ds = ds.GetSpatialRef()
    assert srs_ds.GetDataAxisToSRSAxisMapping() == [1, 2]
    band = ds.GetRasterBand(1)

    ar = band.AsMDArray()
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetSize() == ds.RasterYSize
    assert dims[1].GetSize() == ds.RasterXSize
    srs_ar = ar.GetSpatialRef()
    assert srs_ar.GetDataAxisToSRSAxisMapping() == [2, 1]

    assert ar.Read() == ds.GetRasterBand(1).ReadRaster()

    ixdim = 1
    iydim = 0
    ds2 = ar.AsClassicDataset(ixdim, iydim)
    assert ds2.RasterYSize == ds.RasterYSize
    assert ds2.RasterXSize == ds.RasterXSize
    srs_ds2 = ds2.GetSpatialRef()
    assert srs_ds2.GetDataAxisToSRSAxisMapping() == [1, 2]
    assert srs_ds2.IsSame(srs_ds)

    assert ds2.ReadRaster() == ds.GetRasterBand(1).ReadRaster()

