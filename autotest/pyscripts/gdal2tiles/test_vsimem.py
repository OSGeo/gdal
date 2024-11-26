#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal2tiles.py testing
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal
from osgeo_utils import gdal2tiles

pytestmark = pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)


def test_gdal2tiles_vsimem():

    if gdal.GetDriverByName("PNG") is None:
        pytest.skip("PNG driver is missing")

    gdal2tiles.main(
        argv=["gdal2tiles", "-q", "../../gcore/data/byte.tif", "/vsimem/gdal2tiles"]
    )

    assert set(gdal.ReadDirRecursive("/vsimem/gdal2tiles")) == set(
        [
            "14/",
            "14/2837/",
            "14/2837/9833.png",
            "14/2838/",
            "14/2838/9833.png",
            "googlemaps.html",
            "leaflet.html",
            "openlayers.html",
            "tilemapresource.xml",
        ]
    )
    gdal.RmdirRecursive("/vsimem/gdal2tiles")
