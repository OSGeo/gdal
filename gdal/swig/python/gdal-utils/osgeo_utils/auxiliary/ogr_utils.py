#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL utils.auxiliary
#  Purpose:  OGR utility functions
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
from typing import Sequence

from osgeo import ogr

from osgeo_utils.auxiliary.base import PathLikeOrStr
from osgeo_utils.auxiliary.osr_util import AnySRS, get_srs
from osgeo_utils.auxiliary.rectangle import GeoRectangle


def ogr_create_geometries_from_wkt(path: PathLikeOrStr, wkt_list: Sequence[str],
                                   of='ESRI Shapefile', srs: AnySRS = 4326):
    driver = ogr.GetDriverByName(of)
    ds = driver.CreateDataSource(os.fspath(path))
    srs = get_srs(srs)

    layer = ds.CreateLayer('', srs, ogr.wkbUnknown)
    for wkt in wkt_list:
        feature = ogr.Feature(layer.GetLayerDefn())
        geom = ogr.CreateGeometryFromWkt(wkt)
        feature.SetGeometry(geom)  # Set the feature geometry
        layer.CreateFeature(feature)  # Create the feature in the layer
        feature.Destroy()  # Destroy the feature to free resources
    # Destroy the data source to free resources
    ds.Destroy()


def ogr_get_layer_extent(lyr: ogr.Layer) -> GeoRectangle:
    result = None
    for feature in lyr:
        geom = feature.GetGeometryRef()
        envelope = geom.GetEnvelope()
        r = GeoRectangle.from_min_max(*envelope)
        if result is None:
            result = r
        else:
            result = result.union(r)
    return result
