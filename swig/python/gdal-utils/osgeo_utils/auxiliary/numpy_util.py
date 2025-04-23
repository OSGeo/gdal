#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL utils.auxiliary
#  Purpose:  utility functions for gdal-numpy integration
#  Author:   Idan Miara <idan@miara.com>
#
# ******************************************************************************
#  Copyright (c) 2020, Idan Miara <idan@miara.com>
#
# SPDX-License-Identifier: MIT
# ******************************************************************************

import numpy as np

from osgeo import gdal, gdal_array
from osgeo_utils.auxiliary.array_util import ArrayOrScalarLike, ScalarLike


def GDALTypeCodeToNumericTypeCodeEx(buf_type, signed_byte, default=None):
    typecode = gdal_array.GDALTypeCodeToNumericTypeCode(buf_type)
    if typecode is None:
        typecode = default

    if buf_type == gdal.GDT_Byte and signed_byte:
        typecode = np.int8
    return typecode


def GDALTypeCodeAndNumericTypeCodeFromDataSet(ds):
    band = ds.GetRasterBand(1)
    buf_type = band.DataType
    signed_byte = False
    if buf_type == gdal.GDT_Byte:
        band._EnablePixelTypeSignedByteWarning(False)
        signed_byte = (
            band.GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE") == "SIGNEDBYTE"
        )
        band._EnablePixelTypeSignedByteWarning(True)
    np_typecode = GDALTypeCodeToNumericTypeCodeEx(
        buf_type, signed_byte=signed_byte, default=np.float32
    )
    return buf_type, np_typecode


def array_dist(
    x: ArrayOrScalarLike, y: ArrayOrScalarLike, is_max: bool = True
) -> ScalarLike:
    if isinstance(x, ScalarLike.__args__):
        return abs(x - y)
    if not isinstance(x, np.ndarray):
        x = np.array(x)
    if not isinstance(y, np.ndarray):
        y = np.array(y)
    diff = np.abs(x - y)
    return np.max(diff) if is_max else np.min(diff)
