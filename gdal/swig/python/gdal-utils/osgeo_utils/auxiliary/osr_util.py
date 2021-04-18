# !/usr/bin/env python3
###############################################################################
# $Id$
#
#  Project:  GDAL utils.auxiliary
#  Purpose:  OSR utility functions
#  Author:   Idan Miara <idan@miara.com>
#
# ******************************************************************************
#  Copyright (c) 2021, Idan Miara <idan@miara.com>
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
from typing import Union, Tuple, Optional

import osgeo
from osgeo import osr, ogr, gdal

from osgeo_utils.auxiliary.numpy_util import NumpyCompatibleArray

AnySRS = Union[str, int, osr.SpatialReference, gdal.Dataset]


def get_srs(srs: AnySRS, gis_order: bool = False) -> osr.SpatialReference:
    """ returns an SRS object from epsg, pj_string or DataSet or SRS object """
    if isinstance(srs, osr.SpatialReference):
        pass
    elif isinstance(srs, gdal.Dataset):
        srs = get_srs_from_ds(srs)
    elif isinstance(srs, int):
        srs_ = osr.SpatialReference()
        if srs_.ImportFromEPSG(srs) != ogr.OGRERR_NONE:
            raise Exception(f'ogr error when parsing srs from epsg:{srs}')
        srs = srs_
    elif isinstance(srs, str):
        srs_ = osr.SpatialReference()
        if srs_.SetFromUserInput(srs) != ogr.OGRERR_NONE:  # accept PROJ string, WKT, PROJJSON, etc.
            raise Exception(f'ogr error when parsing srs from proj string: {srs}')
        srs = srs_
    else:
        raise Exception(f'Unknown SRS: {srs}')

    if gis_order and int(osgeo.__version__[0]) >= 3:
        # GDAL 3 changes axis order: https://github.com/OSGeo/gdal/issues/1546
        srs.SetAxisMappingStrategy(osgeo.osr.OAMS_TRADITIONAL_GIS_ORDER)
    return srs


def get_srs_from_ds(ds: gdal.Dataset) -> osr.SpatialReference:
    srs = osr.SpatialReference()
    srs.ImportFromWkt(ds.GetProjection())
    return srs


def get_transform(src_srs: AnySRS, tgt_srs: AnySRS) -> Optional[osr.CoordinateTransformation]:
    src_srs = get_srs(src_srs)
    tgt_srs = get_srs(tgt_srs)
    if src_srs.IsSame(tgt_srs):
        return None
    else:
        return osr.CoordinateTransformation(src_srs, tgt_srs)


def transform_points(ct: Optional[osr.CoordinateTransformation],
                     x: NumpyCompatibleArray, y: NumpyCompatibleArray, z: Optional[NumpyCompatibleArray] = None) -> \
                     Tuple[NumpyCompatibleArray, NumpyCompatibleArray, Optional[NumpyCompatibleArray]]:
    if ct is not None:
        if z is None:
            for idx, (x0, y0) in enumerate(zip(x, y)):
                x[idx], y[idx], _z = ct.TransformPoint(x0, y0)
        else:
            for idx, (x0, y0, z0) in enumerate(zip(x, y, z)):
                x[idx], y[idx], z[idx] = ct.TransformPoint(x0, y0, z0)
    return x, y, z
