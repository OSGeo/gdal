#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL sctipts
#  Purpose:  useful utility functions for this package
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

from osgeo import gdal
import os.path
# from pathlib import Path
# from typing import Sequence, Union


def is_path_like(s):
    # when GDAL drops Python2 support we can support from pathlib import Path, for now it's just str.
    # return isinstance(s, (str, Path))
    return isinstance(s, str)


def get_suffix(filename):
    # return Path(filename).suffix
    return os.path.splitext(filename)[1]


def is_sequence(f):
    # return isinstance(f, Sequence):
    return isinstance(f, (list, tuple))


def to_number(s):
    try:
        return int(s)
    except ValueError:
        return float(s)


def get_byte(number, i):
    return (number & (0xff << (i * 8))) >> (i * 8)


def path_join(*args):
    return os.path.join(*(str(arg) for arg in args))


def DoesDriverHandleExtension(drv, ext):
    exts = drv.GetMetadataItem(gdal.DMD_EXTENSIONS)
    return exts is not None and exts.lower().find(ext.lower()) >= 0


def GetExtension(filename):
    if filename.lower().endswith('.shp.zip'):
        return 'shp.zip'
    ext = os.path.splitext(filename)[1]
    if ext.startswith('.'):
        ext = ext[1:]
    return ext


def GetOutputDriversFor(filename, is_raster=True):
    drv_list = []
    ext = GetExtension(filename)
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


def GetOutputDriverFor(filename, is_raster=True, default_raster_format='GTiff', default_vector_format='ESRI Shapefile'):
    if not filename:
        return 'MEM'
    drv_list = GetOutputDriversFor(filename, is_raster)
    ext = GetExtension(filename)
    if not drv_list:
        if not ext:
            return default_raster_format if is_raster else default_vector_format
        else:
            raise Exception("Cannot guess driver for %s" % filename)
    elif len(drv_list) > 1:
        print("Several drivers matching %s extension. Using %s" % (ext if ext else '', drv_list[0]))
    return drv_list[0]
