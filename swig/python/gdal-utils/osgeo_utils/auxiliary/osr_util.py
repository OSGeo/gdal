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
from typing import Union, Optional

import osgeo
from osgeo import osr, ogr, gdal

from osgeo_utils.auxiliary.array_util import ArrayLike

AnySRS = Union[str, int, osr.SpatialReference, gdal.Dataset]
OAMS_AXIS_ORDER = int
_default_axis_order: Optional[OAMS_AXIS_ORDER] = None


def get_srs(srs_like: AnySRS,
            axis_order: Optional[OAMS_AXIS_ORDER] = None) -> osr.SpatialReference:
    """
    returns an SRS object from epsg, pj_string or DataSet or SRS object

    gis_order and axis_order are mutually exclusive
    if gis_order is not None then axis_order is set according to gis_order
    if axis_order == None -> set axis order to default_axis_order iff the input is not an osr.SpatialReference reference,
    otherwise set requested axis_order
    """
    if isinstance(srs_like, osr.SpatialReference):
        srs = srs_like
    elif isinstance(srs_like, gdal.Dataset):
        srs = osr.SpatialReference()
        srs.ImportFromWkt(srs_like.GetProjection())
    elif isinstance(srs_like, int):
        srs = osr.SpatialReference()
        if srs.ImportFromEPSG(srs_like) != ogr.OGRERR_NONE:
            raise Exception(f'ogr error when parsing srs from epsg:{srs_like}')
    elif isinstance(srs_like, str):
        srs = osr.SpatialReference()
        if srs.SetFromUserInput(srs_like) != ogr.OGRERR_NONE:  # accept PROJ string, WKT, PROJJSON, etc.
            raise Exception(f'ogr error when parsing srs from user input: {srs_like}')
    else:
        raise Exception(f'Unknown SRS: {srs_like}')

    if axis_order is None and srs != srs_like:
        axis_order = _default_axis_order
    if axis_order is not None and int(osgeo.__version__[0]) >= 3:
        # GDAL 3 changes axis order: https://github.com/OSGeo/gdal/issues/1546
        if not isinstance(axis_order, OAMS_AXIS_ORDER):
            raise Exception(f'Unexpected axis_order: {axis_order}')
        srs_axis_order = srs.GetAxisMappingStrategy()
        if axis_order != srs_axis_order:
            if srs == srs_like:
                # we don't want to change the input srs, thus create a copy
                srs = srs.Clone()
            srs.SetAxisMappingStrategy(axis_order)
    return srs


def get_axis_order_from_gis_order(gis_order: Optional[bool]):
    return None if gis_order is None \
        else osr.OAMS_TRADITIONAL_GIS_ORDER if gis_order \
        else osr.OAMS_AUTHORITY_COMPLIANT


def get_gis_order_from_axis_order(axis_order: Optional[OAMS_AXIS_ORDER]):
    return None if axis_order is None else axis_order == osr.OAMS_TRADITIONAL_GIS_ORDER


def set_default_axis_order(axis_order: Optional[OAMS_AXIS_ORDER] = None) -> Optional[OAMS_AXIS_ORDER]:
    global _default_axis_order
    _default_axis_order = axis_order
    return _default_axis_order


def get_default_axis_order() -> Optional[OAMS_AXIS_ORDER]:
    global _default_axis_order
    return _default_axis_order


def get_srs_pj(srs: AnySRS) -> str:
    srs = get_srs(srs)
    srs_pj4 = srs.ExportToProj4()
    return srs_pj4


def are_srs_equivalent(srs1: AnySRS, srs2: AnySRS) -> bool:
    if srs1 == srs2:
        return True
    srs1 = get_srs(srs1)
    srs2 = get_srs(srs2)
    return srs1.IsSame(srs2)


def get_transform(src_srs: AnySRS, tgt_srs: AnySRS) -> Optional[osr.CoordinateTransformation]:
    src_srs = get_srs(src_srs)
    tgt_srs = get_srs(tgt_srs)
    if src_srs.IsSame(tgt_srs):
        return None
    else:
        return osr.CoordinateTransformation(src_srs, tgt_srs)


def transform_points(ct: Optional[osr.CoordinateTransformation],
                     x: ArrayLike, y: ArrayLike, z: Optional[ArrayLike] = None) -> None:
    if ct is not None:
        if z is None:
            for idx, (x0, y0) in enumerate(zip(x, y)):
                x[idx], y[idx], _z = ct.TransformPoint(x0, y0)
        else:
            for idx, (x0, y0, z0) in enumerate(zip(x, y, z)):
                x[idx], y[idx], z[idx] = ct.TransformPoint(x0, y0, z0)
