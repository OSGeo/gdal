#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RFC 30 (UTF filename handling) support.
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

from osgeo import ogr

###############################################################################
# Try ogr.Open(), Driver.CreateDataSource(), Driver.DeleteDataSource()


def test_ogr_rfc30_1():

    filename = "/vsimem/\u00e9.shp"
    layer_name = "\u00e9"

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(filename)
    lyr = ds.CreateLayer("foo")
    ds = None

    ds = ogr.Open(filename)
    assert ds is not None, "cannot reopen datasource"
    lyr = ds.GetLayerByName(layer_name)
    assert lyr is not None, "cannot find layer"
    ds = None

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(filename)
