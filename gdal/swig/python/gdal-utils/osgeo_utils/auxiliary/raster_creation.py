#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL utils.auxiliary
#  Purpose:  raster creation utility functions
#  Author:   Idan Miara <idan@miara.com>
#
# ******************************************************************************
#  Copyright (c) 2021, Idan Miara <idan@miara.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
# ******************************************************************************
import os
import tempfile
from numbers import Real
from typing import Sequence, Optional

from osgeo import gdal

from osgeo_utils.auxiliary import osr_util, extent_util

from osgeo_utils.auxiliary.osr_util import get_srs, AnySRS
from osgeo_utils.auxiliary.base import PathLikeOrStr, MaybeSequence, is_true
from osgeo_utils.auxiliary.rectangle import GeoRectangle
from osgeo_utils.auxiliary.util import get_bigtiff_creation_option_value, get_data_type, DataTypeOrStr, CreationOptions


def create_flat_raster(filename: Optional[PathLikeOrStr],
                       driver_name: Optional[str] = None, dt: DataTypeOrStr = gdal.GDT_Byte,
                       size: MaybeSequence[int] = None, band_count: int = 1, creation_options: CreationOptions = None,
                       fill_value: Optional[Real] = None, nodata_value: Optional[Real] = None,
                       origin: Optional[Sequence[int]] = (500_000, 0), pixel_size: MaybeSequence[int] = 10,
                       extent: GeoRectangle = None, extent_srs: Optional[AnySRS] = None,
                       srs: Optional[AnySRS] = 32636,
                       overview_alg: Optional[str] = 'NEAR', overview_list: Optional[Sequence[int]] = None,
                       **kwargs) -> gdal.Dataset:
    if 'epsg' in kwargs:
        # backwards compatibility
        srs = kwargs['epsg']
        del kwargs['epsg']

    if filename is None:
        filename = tempfile.mktemp()
    elif not filename:
        filename = ''

    if driver_name is None:
        driver_name = 'GTiff' if filename else 'MEM'
    driver = gdal.GetDriverByName(driver_name)
    dt = get_data_type(dt)
    creation_options_list = get_creation_options(creation_options, driver_name=driver_name)

    geo_transform = None
    if pixel_size:
        if not isinstance(pixel_size, Sequence):
            pixel_size = (pixel_size, -pixel_size)
        if origin:
            geo_transform = (origin[0], pixel_size[0], 0, origin[1], 0, pixel_size[1])
        elif extent:
            if extent_srs is not None:
                transform = osr_util.get_transform(extent_srs, srs)
                if transform is not None:
                    extent = extent_util.transform_extent(extent, transform)
            size, geo_transform = extent.get_size_and_geotransform(pixel_size=pixel_size)

    if size is None:
        size = 128
    if not isinstance(size, Sequence):
        size = (size, size)
    ds = driver.Create(os.fspath(filename), *size, band_count, dt, creation_options_list)
    if geo_transform is not None:
        ds.SetGeoTransform(geo_transform)

    if srs is not None:
        srs = get_srs(srs)
        ds.SetSpatialRef(srs)
    for bnd_idx in range(band_count):
        bnd: gdal.Band = ds.GetRasterBand(bnd_idx+1)
        if fill_value is not None:
            bnd.Fill(fill_value)
        if nodata_value is not None:
            bnd.SetNoDataValue(nodata_value)
    if overview_alg and overview_list:
        ds.BuildOverviews(overview_alg, overviewlist=overview_list)
    return ds


def get_creation_options(creation_options: CreationOptions = None,
                         driver_name: str = 'GTiff',
                         sparse_ok: bool = None,
                         tiled: bool = None,
                         block_size: Optional[int] = None,
                         big_tiff: Optional[str] = None,
                         comp: str = None):
    creation_options = dict(creation_options or dict())

    driver_name = driver_name.lower()

    if comp is None:
        comp = creation_options.get("COMPRESS", "DEFLATE")
    creation_options["BIGTIFF"] = get_bigtiff_creation_option_value(big_tiff)
    creation_options["COMPRESS"] = comp

    if sparse_ok is None:
        sparse_ok = creation_options.get("SPARSE_OK", True)
    sparse_ok = is_true(sparse_ok)
    creation_options["SPARSE_OK"] = str(sparse_ok)

    if tiled is None:
        tiled = creation_options.get("TILED", True)
    tiled = is_true(tiled)
    creation_options["TILED"] = str(tiled)
    if tiled and block_size is not None:
        if driver_name == 'gtiff':
            creation_options["BLOCKXSIZE"] = block_size
            creation_options["BLOCKYSIZE"] = block_size
        elif driver_name == 'cog':
            creation_options["BLOCKSIZE"] = block_size

    creation_options_list = []
    for k, v in creation_options.items():
        creation_options_list.append("{}={}".format(k, v))

    return creation_options_list
