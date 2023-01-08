#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsi7z/
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
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

import os

import pytest

from osgeo import gdal, ogr

###############################################################################
# Test /vsi7z/


def test_vsi7z_basic():

    content = gdal.ReadDir("/vsi7z/../ogr/data/poly.shp.7z")
    if content is None:
        pytest.skip()

    assert set(content) == set(["poly.PRJ", "poly.dbf", "poly.shp", "poly.shx"])

    assert gdal.ReadDir("/vsi7z/i_do/not/exist") is None
    assert gdal.VSIFOpenL("/vsi7z/../ogr/data/poly.shp.7z/i_do_not_exist", "rb") is None

    f = gdal.VSIFOpenL("/vsi7z/../ogr/data/poly.shp.7z/poly.PRJ", "rb")
    assert f is not None
    try:
        gdal.VSIFSeekL(f, 0, os.SEEK_END)
        assert gdal.VSIFTellL(f) == 415
        assert gdal.VSIFEofL(f) == 0
        assert len(gdal.VSIFReadL(1, 1, f)) == 0
        assert gdal.VSIFEofL(f) == 1
        assert gdal.VSIFTellL(f) == 415
    finally:
        gdal.VSIFCloseL(f)


###############################################################################
# Test /vsi7z/


def test_vsi7z_ogr():

    content = gdal.ReadDir("/vsi7z/../ogr/data/poly.shp.7z")
    if content is None:
        pytest.skip()

    ds = ogr.Open("/vsi7z/../ogr/data/poly.shp.7z")
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10
