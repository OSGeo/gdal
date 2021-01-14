#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL utils.auxiliary
#  Purpose:  gdal utility functions
#  Author:   Even Rouault <even.rouault at spatialys.com>
#  Author:   Idan Miara <idan@miara.com>
#
# ******************************************************************************
#  Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
#  Copyright (c) 2020, Idan Miara <idan@miara.com>
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

from typing import Optional, Union

from osgeo import gdal
from osgeo.utils.auxiliary.base import get_extension, is_path_like, PathLike

path_or_ds = Union[PathLike, gdal.Dataset]


def DoesDriverHandleExtension(drv: gdal.Driver, ext: str):
    exts = drv.GetMetadataItem(gdal.DMD_EXTENSIONS)
    return exts is not None and exts.lower().find(ext.lower()) >= 0


def GetOutputDriversFor(filename: PathLike, is_raster=True):
    drv_list = []
    ext = get_extension(filename)
    if ext.lower() == 'vrt':
        return ['VRT']
    for i in range(gdal.GetDriverCount()):
        drv = gdal.GetDriver(i)
        if (drv.GetMetadataItem(gdal.DCAP_CREATE) is not None or
            drv.GetMetadataItem(gdal.DCAP_CREATECOPY) is not None) and \
            drv.GetMetadataItem(gdal.DCAP_RASTER if is_raster else gdal.DCAP_VECTOR) is not None:
            if ext and DoesDriverHandleExtension(drv, ext):
                drv_list.append(drv.ShortName)
            else:
                prefix = drv.GetMetadataItem(gdal.DMD_CONNECTION_PREFIX)
                if prefix is not None and filename.lower().startswith(prefix.lower()):
                    drv_list.append(drv.ShortName)

    # GMT is registered before netCDF for opening reasons, but we want
    # netCDF to be used by default for output.
    if ext.lower() == 'nc' and not drv_list and \
        drv_list[0].upper() == 'GMT' and drv_list[1].upper() == 'NETCDF':
        drv_list = ['NETCDF', 'GMT']

    return drv_list


def GetOutputDriverFor(filename: PathLike, is_raster=True, default_raster_format='GTiff',
                       default_vector_format='ESRI Shapefile'):
    if not filename:
        return 'MEM'
    drv_list = GetOutputDriversFor(filename, is_raster)
    ext = get_extension(filename)
    if not drv_list:
        if not ext:
            return default_raster_format if is_raster else default_vector_format
        else:
            raise Exception("Cannot guess driver for %s" % filename)
    elif len(drv_list) > 1:
        print("Several drivers matching %s extension. Using %s" % (ext if ext else '', drv_list[0]))
    return drv_list[0]


def open_ds(filename_or_ds: path_or_ds, *args, **kwargs):
    ods = OpenDS(filename_or_ds, *args, **kwargs)
    return ods.__enter__()


def get_ovr_count(filename_or_ds: path_or_ds):
    with OpenDS(filename_or_ds) as ds:
        bnd = ds.GetRasterBand(1)
        return bnd.GetOverviewCount()


def get_ovr_idx(filename_or_ds: path_or_ds, ovr_idx: Optional[int]):
    if ovr_idx is None:
        ovr_idx = 0
    if ovr_idx < 0:
        # -1 is the last overview; -2 is the one before the last
        overview_count = get_ovr_count(open_ds(filename_or_ds))
        ovr_idx = max(0, overview_count + ovr_idx + 1)
    return ovr_idx


class OpenDS:
    __slots__ = ['filename', 'ds', 'args', 'kwargs', 'own', 'silent_fail']

    def __init__(self, filename_or_ds: path_or_ds, silent_fail=False, *args, **kwargs):
        self.ds: Optional[gdal.Dataset] = None
        self.filename: Optional[PathLike] = None
        if is_path_like(filename_or_ds):
            self.filename = str(filename_or_ds)
        else:
            self.ds = filename_or_ds
        self.args = args
        self.kwargs = kwargs
        self.own = False
        self.silent_fail = silent_fail

    def __enter__(self) -> gdal.Dataset:

        if self.ds is None:
            self.ds = self._open_ds(self.filename, *self.args, **self.kwargs)
            if self.ds is None and not self.silent_fail:
                raise IOError('could not open file "{}"'.format(self.filename))
            self.own = True
        return self.ds

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.own:
            self.ds = False

    @staticmethod
    def _open_ds(
        filename: PathLike,
        access_mode=gdal.GA_ReadOnly,
        ovr_idx: int = None,
        open_options: dict = None,
        logger=None,
    ):
        open_options = dict(open_options or dict())
        ovr_idx = get_ovr_idx(filename, ovr_idx)
        if ovr_idx > 0:
            open_options["OVERVIEW_LEVEL"] = ovr_idx - 1  # gdal overview 0 is the first overview (after the base layer)
        if logger is not None:
            s = 'opening file: "{}"'.format(filename)
            if open_options:
                s = s + " with options: {}".format(str(open_options))
            logger.debug(s)
        open_options = ["{}={}".format(k, v) for k, v in open_options.items()]

        return gdal.OpenEx(str(filename), access_mode, open_options=open_options)
