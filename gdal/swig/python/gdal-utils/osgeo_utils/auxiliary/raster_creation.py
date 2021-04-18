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

import tempfile
from numbers import Real
from typing import Sequence, Optional

from osgeo import gdal, osr
from osgeo_utils.auxiliary.base import PathLike, MaybeSequence, is_true
from osgeo_utils.auxiliary.util import get_bigtiff_creation_option_value, get_data_type, DataTypeOrStr, CreationOptions


def create_flat_raster(filename: Optional[PathLike],
                       driver: Optional[str] = None, dt: DataTypeOrStr = gdal.GDT_Byte,
                       size: MaybeSequence[int] = 128, band_count: int = 1, creation_options: CreationOptions = None,
                       fill_value: Optional[Real] = None, nodata_value: Optional[Real] = None,
                       origin: Optional[Sequence[int]] = (500_000, 0), pixel_size: MaybeSequence[int] = 10,
                       epsg: Optional[int] = 32636,
                       overview_alg: Optional[str] = 'NEAR', overview_list: Optional[Sequence[int]] = None) -> gdal.Dataset:
    if filename is None:
        filename = tempfile.mktemp()
    elif not filename:
        filename = ''
    if driver is None:
        driver = 'GTiff' if filename else 'MEM'
    if not isinstance(size, Sequence):
        size = (size, size)

    drv = gdal.GetDriverByName(driver)
    dt = get_data_type(dt)
    creation_options_list = get_creation_options(creation_options, driver=driver)
    ds = drv.Create(str(filename), *size, band_count, dt, creation_options_list)

    if pixel_size and origin:
        if not isinstance(pixel_size, Sequence):
            pixel_size = (pixel_size, -pixel_size)
        ds.SetGeoTransform([origin[0], pixel_size[0], 0, origin[1], 0, pixel_size[1]])
    if epsg is not None:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(epsg)
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
                         driver: str = 'GTiff',
                         sparse_ok: bool = None,
                         tiled: bool = None,
                         block_size: Optional[int] = None,
                         big_tiff: Optional[str] = None,
                         comp: str = None):
    creation_options = dict(creation_options or dict())

    driver = driver.lower()

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
        if driver == 'gtiff':
            creation_options["BLOCKXSIZE"] = block_size
            creation_options["BLOCKYSIZE"] = block_size
        elif driver == 'cog':
            creation_options["BLOCKSIZE"] = block_size

    creation_options_list = []
    for k, v in creation_options.items():
        creation_options_list.append("{}={}".format(k, v))

    return creation_options_list
